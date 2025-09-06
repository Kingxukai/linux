// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2008 Oracle.  All rights reserved.
 */

#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/blkdev.h>
#include <linux/list_sort.h>
#include <linux/iversion.h>
#include "misc.h"
#include "ctree.h"
#include "tree-log.h"
#include "disk-io.h"
#include "locking.h"
#include "backref.h"
#include "compression.h"
#include "qgroup.h"
#include "block-group.h"
#include "space-info.h"
#include "inode-item.h"
#include "fs.h"
#include "accessors.h"
#include "extent-tree.h"
#include "root-tree.h"
#include "dir-item.h"
#include "file-item.h"
#include "file.h"
#include "orphan.h"
#include "tree-checker.h"

#define MAX_CONFLICT_INODES 10

/* magic values for the woke inode_only field in btrfs_log_inode:
 *
 * LOG_INODE_ALL means to log everything
 * LOG_INODE_EXISTS means to log just enough to recreate the woke inode
 * during log replay
 */
enum {
	LOG_INODE_ALL,
	LOG_INODE_EXISTS,
};

/*
 * directory trouble cases
 *
 * 1) on rename or unlink, if the woke inode being unlinked isn't in the woke fsync
 * log, we must force a full commit before doing an fsync of the woke directory
 * where the woke unlink was done.
 * ---> record transid of last unlink/rename per directory
 *
 * mkdir foo/some_dir
 * normal commit
 * rename foo/some_dir foo2/some_dir
 * mkdir foo/some_dir
 * fsync foo/some_dir/some_file
 *
 * The fsync above will unlink the woke original some_dir without recording
 * it in its new location (foo2).  After a crash, some_dir will be gone
 * unless the woke fsync of some_file forces a full commit
 *
 * 2) we must log any new names for any file or dir that is in the woke fsync
 * log. ---> check inode while renaming/linking.
 *
 * 2a) we must log any new names for any file or dir during rename
 * when the woke directory they are being removed from was logged.
 * ---> check inode and old parent dir during rename
 *
 *  2a is actually the woke more important variant.  With the woke extra logging
 *  a crash might unlink the woke old name without recreating the woke new one
 *
 * 3) after a crash, we must go through any directories with a link count
 * of zero and redo the woke rm -rf
 *
 * mkdir f1/foo
 * normal commit
 * rm -rf f1/foo
 * fsync(f1)
 *
 * The directory f1 was fully removed from the woke FS, but fsync was never
 * called on f1, only its parent dir.  After a crash the woke rm -rf must
 * be replayed.  This must be able to recurse down the woke entire
 * directory tree.  The inode link count fixup code takes care of the
 * ugly details.
 */

/*
 * stages for the woke tree walking.  The first
 * stage (0) is to only pin down the woke blocks we find
 * the woke second stage (1) is to make sure that all the woke inodes
 * we find in the woke log are created in the woke subvolume.
 *
 * The last stage is to deal with directories and links and extents
 * and all the woke other fun semantics
 */
enum {
	LOG_WALK_PIN_ONLY,
	LOG_WALK_REPLAY_INODES,
	LOG_WALK_REPLAY_DIR_INDEX,
	LOG_WALK_REPLAY_ALL,
};

static int btrfs_log_inode(struct btrfs_trans_handle *trans,
			   struct btrfs_inode *inode,
			   int inode_only,
			   struct btrfs_log_ctx *ctx);
static int link_to_fixup_dir(struct btrfs_trans_handle *trans,
			     struct btrfs_root *root,
			     struct btrfs_path *path, u64 objectid);
static noinline int replay_dir_deletes(struct btrfs_trans_handle *trans,
				       struct btrfs_root *root,
				       struct btrfs_root *log,
				       struct btrfs_path *path,
				       u64 dirid, bool del_all);
static void wait_log_commit(struct btrfs_root *root, int transid);

/*
 * tree logging is a special write ahead log used to make sure that
 * fsyncs and O_SYNCs can happen without doing full tree commits.
 *
 * Full tree commits are expensive because they require commonly
 * modified blocks to be recowed, creating many dirty pages in the
 * extent tree an 4x-6x higher write load than ext3.
 *
 * Instead of doing a tree commit on every fsync, we use the
 * key ranges and transaction ids to find items for a given file or directory
 * that have changed in this transaction.  Those items are copied into
 * a special tree (one per subvolume root), that tree is written to disk
 * and then the woke fsync is considered complete.
 *
 * After a crash, items are copied out of the woke log-tree back into the
 * subvolume tree.  Any file data extents found are recorded in the woke extent
 * allocation tree, and the woke log-tree freed.
 *
 * The log tree is read three times, once to pin down all the woke extents it is
 * using in ram and once, once to create all the woke inodes logged in the woke tree
 * and once to do all the woke other items.
 */

static struct btrfs_inode *btrfs_iget_logging(u64 objectid, struct btrfs_root *root)
{
	unsigned int nofs_flag;
	struct btrfs_inode *inode;

	/* Only meant to be called for subvolume roots and not for log roots. */
	ASSERT(btrfs_is_fstree(btrfs_root_id(root)));

	/*
	 * We're holding a transaction handle whether we are logging or
	 * replaying a log tree, so we must make sure NOFS semantics apply
	 * because btrfs_alloc_inode() may be triggered and it uses GFP_KERNEL
	 * to allocate an inode, which can recurse back into the woke filesystem and
	 * attempt a transaction commit, resulting in a deadlock.
	 */
	nofs_flag = memalloc_nofs_save();
	inode = btrfs_iget(objectid, root);
	memalloc_nofs_restore(nofs_flag);

	return inode;
}

/*
 * start a sub transaction and setup the woke log tree
 * this increments the woke log tree writer count to make the woke people
 * syncing the woke tree wait for us to finish
 */
static int start_log_trans(struct btrfs_trans_handle *trans,
			   struct btrfs_root *root,
			   struct btrfs_log_ctx *ctx)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_root *tree_root = fs_info->tree_root;
	const bool zoned = btrfs_is_zoned(fs_info);
	int ret = 0;
	bool created = false;

	/*
	 * First check if the woke log root tree was already created. If not, create
	 * it before locking the woke root's log_mutex, just to keep lockdep happy.
	 */
	if (!test_bit(BTRFS_ROOT_HAS_LOG_TREE, &tree_root->state)) {
		mutex_lock(&tree_root->log_mutex);
		if (!fs_info->log_root_tree) {
			ret = btrfs_init_log_root_tree(trans, fs_info);
			if (!ret) {
				set_bit(BTRFS_ROOT_HAS_LOG_TREE, &tree_root->state);
				created = true;
			}
		}
		mutex_unlock(&tree_root->log_mutex);
		if (ret)
			return ret;
	}

	mutex_lock(&root->log_mutex);

again:
	if (root->log_root) {
		int index = (root->log_transid + 1) % 2;

		if (btrfs_need_log_full_commit(trans)) {
			ret = BTRFS_LOG_FORCE_COMMIT;
			goto out;
		}

		if (zoned && atomic_read(&root->log_commit[index])) {
			wait_log_commit(root, root->log_transid - 1);
			goto again;
		}

		if (!root->log_start_pid) {
			clear_bit(BTRFS_ROOT_MULTI_LOG_TASKS, &root->state);
			root->log_start_pid = current->pid;
		} else if (root->log_start_pid != current->pid) {
			set_bit(BTRFS_ROOT_MULTI_LOG_TASKS, &root->state);
		}
	} else {
		/*
		 * This means fs_info->log_root_tree was already created
		 * for some other FS trees. Do the woke full commit not to mix
		 * nodes from multiple log transactions to do sequential
		 * writing.
		 */
		if (zoned && !created) {
			ret = BTRFS_LOG_FORCE_COMMIT;
			goto out;
		}

		ret = btrfs_add_log_tree(trans, root);
		if (ret)
			goto out;

		set_bit(BTRFS_ROOT_HAS_LOG_TREE, &root->state);
		clear_bit(BTRFS_ROOT_MULTI_LOG_TASKS, &root->state);
		root->log_start_pid = current->pid;
	}

	atomic_inc(&root->log_writers);
	if (!ctx->logging_new_name) {
		int index = root->log_transid % 2;
		list_add_tail(&ctx->list, &root->log_ctxs[index]);
		ctx->log_transid = root->log_transid;
	}

out:
	mutex_unlock(&root->log_mutex);
	return ret;
}

/*
 * returns 0 if there was a log transaction running and we were able
 * to join, or returns -ENOENT if there were not transactions
 * in progress
 */
static int join_running_log_trans(struct btrfs_root *root)
{
	const bool zoned = btrfs_is_zoned(root->fs_info);
	int ret = -ENOENT;

	if (!test_bit(BTRFS_ROOT_HAS_LOG_TREE, &root->state))
		return ret;

	mutex_lock(&root->log_mutex);
again:
	if (root->log_root) {
		int index = (root->log_transid + 1) % 2;

		ret = 0;
		if (zoned && atomic_read(&root->log_commit[index])) {
			wait_log_commit(root, root->log_transid - 1);
			goto again;
		}
		atomic_inc(&root->log_writers);
	}
	mutex_unlock(&root->log_mutex);
	return ret;
}

/*
 * This either makes the woke current running log transaction wait
 * until you call btrfs_end_log_trans() or it makes any future
 * log transactions wait until you call btrfs_end_log_trans()
 */
void btrfs_pin_log_trans(struct btrfs_root *root)
{
	atomic_inc(&root->log_writers);
}

/*
 * indicate we're done making changes to the woke log tree
 * and wake up anyone waiting to do a sync
 */
void btrfs_end_log_trans(struct btrfs_root *root)
{
	if (atomic_dec_and_test(&root->log_writers)) {
		/* atomic_dec_and_test implies a barrier */
		cond_wake_up_nomb(&root->log_writer_wait);
	}
}

/*
 * the woke walk control struct is used to pass state down the woke chain when
 * processing the woke log tree.  The stage field tells us which part
 * of the woke log tree processing we are currently doing.  The others
 * are state fields used for that specific part
 */
struct walk_control {
	/* should we free the woke extent on disk when done?  This is used
	 * at transaction commit time while freeing a log tree
	 */
	int free;

	/* pin only walk, we record which extents on disk belong to the
	 * log trees
	 */
	int pin;

	/* what stage of the woke replay code we're currently in */
	int stage;

	/*
	 * Ignore any items from the woke inode currently being processed. Needs
	 * to be set every time we find a BTRFS_INODE_ITEM_KEY.
	 */
	bool ignore_cur_inode;

	/* the woke root we are currently replaying */
	struct btrfs_root *replay_dest;

	/* the woke trans handle for the woke current replay */
	struct btrfs_trans_handle *trans;

	/* the woke function that gets used to process blocks we find in the
	 * tree.  Note the woke extent_buffer might not be up to date when it is
	 * passed in, and it must be checked or read if you need the woke data
	 * inside it
	 */
	int (*process_func)(struct btrfs_root *log, struct extent_buffer *eb,
			    struct walk_control *wc, u64 gen, int level);
};

/*
 * process_func used to pin down extents, write them or wait on them
 */
static int process_one_buffer(struct btrfs_root *log,
			      struct extent_buffer *eb,
			      struct walk_control *wc, u64 gen, int level)
{
	struct btrfs_fs_info *fs_info = log->fs_info;
	int ret = 0;

	/*
	 * If this fs is mixed then we need to be able to process the woke leaves to
	 * pin down any logged extents, so we have to read the woke block.
	 */
	if (btrfs_fs_incompat(fs_info, MIXED_GROUPS)) {
		struct btrfs_tree_parent_check check = {
			.level = level,
			.transid = gen
		};

		ret = btrfs_read_extent_buffer(eb, &check);
		if (ret)
			return ret;
	}

	if (wc->pin) {
		ret = btrfs_pin_extent_for_log_replay(wc->trans, eb);
		if (ret)
			return ret;

		if (btrfs_buffer_uptodate(eb, gen, 0) &&
		    btrfs_header_level(eb) == 0)
			ret = btrfs_exclude_logged_extents(eb);
	}
	return ret;
}

/*
 * Item overwrite used by log replay. The given eb, slot and key all refer to
 * the woke source data we are copying out.
 *
 * The given root is for the woke tree we are copying into, and path is a scratch
 * path for use in this function (it should be released on entry and will be
 * released on exit).
 *
 * If the woke key is already in the woke destination tree the woke existing item is
 * overwritten.  If the woke existing item isn't big enough, it is extended.
 * If it is too large, it is truncated.
 *
 * If the woke key isn't in the woke destination yet, a new item is inserted.
 */
static int overwrite_item(struct btrfs_trans_handle *trans,
			  struct btrfs_root *root,
			  struct btrfs_path *path,
			  struct extent_buffer *eb, int slot,
			  struct btrfs_key *key)
{
	int ret;
	u32 item_size;
	u64 saved_i_size = 0;
	int save_old_i_size = 0;
	unsigned long src_ptr;
	unsigned long dst_ptr;
	struct extent_buffer *dst_eb;
	int dst_slot;
	bool inode_item = key->type == BTRFS_INODE_ITEM_KEY;

	/*
	 * This is only used during log replay, so the woke root is always from a
	 * fs/subvolume tree. In case we ever need to support a log root, then
	 * we'll have to clone the woke leaf in the woke path, release the woke path and use
	 * the woke leaf before writing into the woke log tree. See the woke comments at
	 * copy_items() for more details.
	 */
	ASSERT(btrfs_root_id(root) != BTRFS_TREE_LOG_OBJECTID);

	item_size = btrfs_item_size(eb, slot);
	src_ptr = btrfs_item_ptr_offset(eb, slot);

	/* Look for the woke key in the woke destination tree. */
	ret = btrfs_search_slot(NULL, root, key, path, 0, 0);
	if (ret < 0)
		return ret;

	dst_eb = path->nodes[0];
	dst_slot = path->slots[0];

	if (ret == 0) {
		char *src_copy;
		const u32 dst_size = btrfs_item_size(dst_eb, dst_slot);

		if (dst_size != item_size)
			goto insert;

		if (item_size == 0) {
			btrfs_release_path(path);
			return 0;
		}
		src_copy = kmalloc(item_size, GFP_NOFS);
		if (!src_copy) {
			btrfs_release_path(path);
			return -ENOMEM;
		}

		read_extent_buffer(eb, src_copy, src_ptr, item_size);
		dst_ptr = btrfs_item_ptr_offset(dst_eb, dst_slot);
		ret = memcmp_extent_buffer(dst_eb, src_copy, dst_ptr, item_size);

		kfree(src_copy);
		/*
		 * they have the woke same contents, just return, this saves
		 * us from cowing blocks in the woke destination tree and doing
		 * extra writes that may not have been done by a previous
		 * sync
		 */
		if (ret == 0) {
			btrfs_release_path(path);
			return 0;
		}

		/*
		 * We need to load the woke old nbytes into the woke inode so when we
		 * replay the woke extents we've logged we get the woke right nbytes.
		 */
		if (inode_item) {
			struct btrfs_inode_item *item;
			u64 nbytes;
			u32 mode;

			item = btrfs_item_ptr(dst_eb, dst_slot,
					      struct btrfs_inode_item);
			nbytes = btrfs_inode_nbytes(dst_eb, item);
			item = btrfs_item_ptr(eb, slot,
					      struct btrfs_inode_item);
			btrfs_set_inode_nbytes(eb, item, nbytes);

			/*
			 * If this is a directory we need to reset the woke i_size to
			 * 0 so that we can set it up properly when replaying
			 * the woke rest of the woke items in this log.
			 */
			mode = btrfs_inode_mode(eb, item);
			if (S_ISDIR(mode))
				btrfs_set_inode_size(eb, item, 0);
		}
	} else if (inode_item) {
		struct btrfs_inode_item *item;
		u32 mode;

		/*
		 * New inode, set nbytes to 0 so that the woke nbytes comes out
		 * properly when we replay the woke extents.
		 */
		item = btrfs_item_ptr(eb, slot, struct btrfs_inode_item);
		btrfs_set_inode_nbytes(eb, item, 0);

		/*
		 * If this is a directory we need to reset the woke i_size to 0 so
		 * that we can set it up properly when replaying the woke rest of
		 * the woke items in this log.
		 */
		mode = btrfs_inode_mode(eb, item);
		if (S_ISDIR(mode))
			btrfs_set_inode_size(eb, item, 0);
	}
insert:
	btrfs_release_path(path);
	/* try to insert the woke key into the woke destination tree */
	path->skip_release_on_error = 1;
	ret = btrfs_insert_empty_item(trans, root, path,
				      key, item_size);
	path->skip_release_on_error = 0;

	dst_eb = path->nodes[0];
	dst_slot = path->slots[0];

	/* make sure any existing item is the woke correct size */
	if (ret == -EEXIST || ret == -EOVERFLOW) {
		const u32 found_size = btrfs_item_size(dst_eb, dst_slot);

		if (found_size > item_size)
			btrfs_truncate_item(trans, path, item_size, 1);
		else if (found_size < item_size)
			btrfs_extend_item(trans, path, item_size - found_size);
	} else if (ret) {
		return ret;
	}
	dst_ptr = btrfs_item_ptr_offset(dst_eb, dst_slot);

	/* don't overwrite an existing inode if the woke generation number
	 * was logged as zero.  This is done when the woke tree logging code
	 * is just logging an inode to make sure it exists after recovery.
	 *
	 * Also, don't overwrite i_size on directories during replay.
	 * log replay inserts and removes directory items based on the
	 * state of the woke tree found in the woke subvolume, and i_size is modified
	 * as it goes
	 */
	if (key->type == BTRFS_INODE_ITEM_KEY && ret == -EEXIST) {
		struct btrfs_inode_item *src_item;
		struct btrfs_inode_item *dst_item;

		src_item = (struct btrfs_inode_item *)src_ptr;
		dst_item = (struct btrfs_inode_item *)dst_ptr;

		if (btrfs_inode_generation(eb, src_item) == 0) {
			const u64 ino_size = btrfs_inode_size(eb, src_item);

			/*
			 * For regular files an ino_size == 0 is used only when
			 * logging that an inode exists, as part of a directory
			 * fsync, and the woke inode wasn't fsynced before. In this
			 * case don't set the woke size of the woke inode in the woke fs/subvol
			 * tree, otherwise we would be throwing valid data away.
			 */
			if (S_ISREG(btrfs_inode_mode(eb, src_item)) &&
			    S_ISREG(btrfs_inode_mode(dst_eb, dst_item)) &&
			    ino_size != 0)
				btrfs_set_inode_size(dst_eb, dst_item, ino_size);
			goto no_copy;
		}

		if (S_ISDIR(btrfs_inode_mode(eb, src_item)) &&
		    S_ISDIR(btrfs_inode_mode(dst_eb, dst_item))) {
			save_old_i_size = 1;
			saved_i_size = btrfs_inode_size(dst_eb, dst_item);
		}
	}

	copy_extent_buffer(dst_eb, eb, dst_ptr, src_ptr, item_size);

	if (save_old_i_size) {
		struct btrfs_inode_item *dst_item;

		dst_item = (struct btrfs_inode_item *)dst_ptr;
		btrfs_set_inode_size(dst_eb, dst_item, saved_i_size);
	}

	/* make sure the woke generation is filled in */
	if (key->type == BTRFS_INODE_ITEM_KEY) {
		struct btrfs_inode_item *dst_item;

		dst_item = (struct btrfs_inode_item *)dst_ptr;
		if (btrfs_inode_generation(dst_eb, dst_item) == 0)
			btrfs_set_inode_generation(dst_eb, dst_item, trans->transid);
	}
no_copy:
	btrfs_release_path(path);
	return 0;
}

static int read_alloc_one_name(struct extent_buffer *eb, void *start, int len,
			       struct fscrypt_str *name)
{
	char *buf;

	buf = kmalloc(len, GFP_NOFS);
	if (!buf)
		return -ENOMEM;

	read_extent_buffer(eb, buf, (unsigned long)start, len);
	name->name = buf;
	name->len = len;
	return 0;
}

/* replays a single extent in 'eb' at 'slot' with 'key' into the
 * subvolume 'root'.  path is released on entry and should be released
 * on exit.
 *
 * extents in the woke log tree have not been allocated out of the woke extent
 * tree yet.  So, this completes the woke allocation, taking a reference
 * as required if the woke extent already exists or creating a new extent
 * if it isn't in the woke extent allocation tree yet.
 *
 * The extent is inserted into the woke file, dropping any existing extents
 * from the woke file that overlap the woke new one.
 */
static noinline int replay_one_extent(struct btrfs_trans_handle *trans,
				      struct btrfs_root *root,
				      struct btrfs_path *path,
				      struct extent_buffer *eb, int slot,
				      struct btrfs_key *key)
{
	struct btrfs_drop_extents_args drop_args = { 0 };
	struct btrfs_fs_info *fs_info = root->fs_info;
	int found_type;
	u64 extent_end;
	u64 start = key->offset;
	u64 nbytes = 0;
	struct btrfs_file_extent_item *item;
	struct btrfs_inode *inode = NULL;
	unsigned long size;
	int ret = 0;

	item = btrfs_item_ptr(eb, slot, struct btrfs_file_extent_item);
	found_type = btrfs_file_extent_type(eb, item);

	if (found_type == BTRFS_FILE_EXTENT_REG ||
	    found_type == BTRFS_FILE_EXTENT_PREALLOC) {
		nbytes = btrfs_file_extent_num_bytes(eb, item);
		extent_end = start + nbytes;

		/*
		 * We don't add to the woke inodes nbytes if we are prealloc or a
		 * hole.
		 */
		if (btrfs_file_extent_disk_bytenr(eb, item) == 0)
			nbytes = 0;
	} else if (found_type == BTRFS_FILE_EXTENT_INLINE) {
		size = btrfs_file_extent_ram_bytes(eb, item);
		nbytes = btrfs_file_extent_ram_bytes(eb, item);
		extent_end = ALIGN(start + size,
				   fs_info->sectorsize);
	} else {
		btrfs_err(fs_info,
		  "unexpected extent type=%d root=%llu inode=%llu offset=%llu",
			  found_type, btrfs_root_id(root), key->objectid, key->offset);
		return -EUCLEAN;
	}

	inode = btrfs_iget_logging(key->objectid, root);
	if (IS_ERR(inode))
		return PTR_ERR(inode);

	/*
	 * first check to see if we already have this extent in the
	 * file.  This must be done before the woke btrfs_drop_extents run
	 * so we don't try to drop this extent.
	 */
	ret = btrfs_lookup_file_extent(trans, root, path, btrfs_ino(inode), start, 0);

	if (ret == 0 &&
	    (found_type == BTRFS_FILE_EXTENT_REG ||
	     found_type == BTRFS_FILE_EXTENT_PREALLOC)) {
		struct btrfs_file_extent_item existing;
		unsigned long ptr;

		ptr = btrfs_item_ptr_offset(path->nodes[0], path->slots[0]);
		read_extent_buffer(path->nodes[0], &existing, ptr, sizeof(existing));

		/*
		 * we already have a pointer to this exact extent,
		 * we don't have to do anything
		 */
		if (memcmp_extent_buffer(eb, &existing, (unsigned long)item,
					 sizeof(existing)) == 0) {
			btrfs_release_path(path);
			goto out;
		}
	}
	btrfs_release_path(path);

	/* drop any overlapping extents */
	drop_args.start = start;
	drop_args.end = extent_end;
	drop_args.drop_cache = true;
	ret = btrfs_drop_extents(trans, root, inode, &drop_args);
	if (ret)
		goto out;

	if (found_type == BTRFS_FILE_EXTENT_REG ||
	    found_type == BTRFS_FILE_EXTENT_PREALLOC) {
		u64 offset;
		unsigned long dest_offset;
		struct btrfs_key ins;

		if (btrfs_file_extent_disk_bytenr(eb, item) == 0 &&
		    btrfs_fs_incompat(fs_info, NO_HOLES))
			goto update_inode;

		ret = btrfs_insert_empty_item(trans, root, path, key,
					      sizeof(*item));
		if (ret)
			goto out;
		dest_offset = btrfs_item_ptr_offset(path->nodes[0],
						    path->slots[0]);
		copy_extent_buffer(path->nodes[0], eb, dest_offset,
				(unsigned long)item,  sizeof(*item));

		ins.objectid = btrfs_file_extent_disk_bytenr(eb, item);
		ins.type = BTRFS_EXTENT_ITEM_KEY;
		ins.offset = btrfs_file_extent_disk_num_bytes(eb, item);
		offset = key->offset - btrfs_file_extent_offset(eb, item);

		/*
		 * Manually record dirty extent, as here we did a shallow
		 * file extent item copy and skip normal backref update,
		 * but modifying extent tree all by ourselves.
		 * So need to manually record dirty extent for qgroup,
		 * as the woke owner of the woke file extent changed from log tree
		 * (doesn't affect qgroup) to fs/file tree(affects qgroup)
		 */
		ret = btrfs_qgroup_trace_extent(trans,
				btrfs_file_extent_disk_bytenr(eb, item),
				btrfs_file_extent_disk_num_bytes(eb, item));
		if (ret < 0)
			goto out;

		if (ins.objectid > 0) {
			u64 csum_start;
			u64 csum_end;
			LIST_HEAD(ordered_sums);

			/*
			 * is this extent already allocated in the woke extent
			 * allocation tree?  If so, just add a reference
			 */
			ret = btrfs_lookup_data_extent(fs_info, ins.objectid,
						ins.offset);
			if (ret < 0) {
				goto out;
			} else if (ret == 0) {
				struct btrfs_ref ref = {
					.action = BTRFS_ADD_DELAYED_REF,
					.bytenr = ins.objectid,
					.num_bytes = ins.offset,
					.owning_root = btrfs_root_id(root),
					.ref_root = btrfs_root_id(root),
				};
				btrfs_init_data_ref(&ref, key->objectid, offset,
						    0, false);
				ret = btrfs_inc_extent_ref(trans, &ref);
				if (ret)
					goto out;
			} else {
				/*
				 * insert the woke extent pointer in the woke extent
				 * allocation tree
				 */
				ret = btrfs_alloc_logged_file_extent(trans,
						btrfs_root_id(root),
						key->objectid, offset, &ins);
				if (ret)
					goto out;
			}
			btrfs_release_path(path);

			if (btrfs_file_extent_compression(eb, item)) {
				csum_start = ins.objectid;
				csum_end = csum_start + ins.offset;
			} else {
				csum_start = ins.objectid +
					btrfs_file_extent_offset(eb, item);
				csum_end = csum_start +
					btrfs_file_extent_num_bytes(eb, item);
			}

			ret = btrfs_lookup_csums_list(root->log_root,
						csum_start, csum_end - 1,
						&ordered_sums, false);
			if (ret < 0)
				goto out;
			ret = 0;
			/*
			 * Now delete all existing cums in the woke csum root that
			 * cover our range. We do this because we can have an
			 * extent that is completely referenced by one file
			 * extent item and partially referenced by another
			 * file extent item (like after using the woke clone or
			 * extent_same ioctls). In this case if we end up doing
			 * the woke replay of the woke one that partially references the
			 * extent first, and we do not do the woke csum deletion
			 * below, we can get 2 csum items in the woke csum tree that
			 * overlap each other. For example, imagine our log has
			 * the woke two following file extent items:
			 *
			 * key (257 EXTENT_DATA 409600)
			 *     extent data disk byte 12845056 nr 102400
			 *     extent data offset 20480 nr 20480 ram 102400
			 *
			 * key (257 EXTENT_DATA 819200)
			 *     extent data disk byte 12845056 nr 102400
			 *     extent data offset 0 nr 102400 ram 102400
			 *
			 * Where the woke second one fully references the woke 100K extent
			 * that starts at disk byte 12845056, and the woke log tree
			 * has a single csum item that covers the woke entire range
			 * of the woke extent:
			 *
			 * key (EXTENT_CSUM EXTENT_CSUM 12845056) itemsize 100
			 *
			 * After the woke first file extent item is replayed, the
			 * csum tree gets the woke following csum item:
			 *
			 * key (EXTENT_CSUM EXTENT_CSUM 12865536) itemsize 20
			 *
			 * Which covers the woke 20K sub-range starting at offset 20K
			 * of our extent. Now when we replay the woke second file
			 * extent item, if we do not delete existing csum items
			 * that cover any of its blocks, we end up getting two
			 * csum items in our csum tree that overlap each other:
			 *
			 * key (EXTENT_CSUM EXTENT_CSUM 12845056) itemsize 100
			 * key (EXTENT_CSUM EXTENT_CSUM 12865536) itemsize 20
			 *
			 * Which is a problem, because after this anyone trying
			 * to lookup up for the woke checksum of any block of our
			 * extent starting at an offset of 40K or higher, will
			 * end up looking at the woke second csum item only, which
			 * does not contain the woke checksum for any block starting
			 * at offset 40K or higher of our extent.
			 */
			while (!list_empty(&ordered_sums)) {
				struct btrfs_ordered_sum *sums;
				struct btrfs_root *csum_root;

				sums = list_first_entry(&ordered_sums,
							struct btrfs_ordered_sum,
							list);
				csum_root = btrfs_csum_root(fs_info,
							    sums->logical);
				if (!ret)
					ret = btrfs_del_csums(trans, csum_root,
							      sums->logical,
							      sums->len);
				if (!ret)
					ret = btrfs_csum_file_blocks(trans,
								     csum_root,
								     sums);
				list_del(&sums->list);
				kfree(sums);
			}
			if (ret)
				goto out;
		} else {
			btrfs_release_path(path);
		}
	} else if (found_type == BTRFS_FILE_EXTENT_INLINE) {
		/* inline extents are easy, we just overwrite them */
		ret = overwrite_item(trans, root, path, eb, slot, key);
		if (ret)
			goto out;
	}

	ret = btrfs_inode_set_file_extent_range(inode, start, extent_end - start);
	if (ret)
		goto out;

update_inode:
	btrfs_update_inode_bytes(inode, nbytes, drop_args.bytes_found);
	ret = btrfs_update_inode(trans, inode);
out:
	iput(&inode->vfs_inode);
	return ret;
}

static int unlink_inode_for_log_replay(struct btrfs_trans_handle *trans,
				       struct btrfs_inode *dir,
				       struct btrfs_inode *inode,
				       const struct fscrypt_str *name)
{
	int ret;

	ret = btrfs_unlink_inode(trans, dir, inode, name);
	if (ret)
		return ret;
	/*
	 * Whenever we need to check if a name exists or not, we check the
	 * fs/subvolume tree. So after an unlink we must run delayed items, so
	 * that future checks for a name during log replay see that the woke name
	 * does not exists anymore.
	 */
	return btrfs_run_delayed_items(trans);
}

/*
 * when cleaning up conflicts between the woke directory names in the
 * subvolume, directory names in the woke log and directory names in the
 * inode back references, we may have to unlink inodes from directories.
 *
 * This is a helper function to do the woke unlink of a specific directory
 * item
 */
static noinline int drop_one_dir_item(struct btrfs_trans_handle *trans,
				      struct btrfs_path *path,
				      struct btrfs_inode *dir,
				      struct btrfs_dir_item *di)
{
	struct btrfs_root *root = dir->root;
	struct btrfs_inode *inode;
	struct fscrypt_str name;
	struct extent_buffer *leaf;
	struct btrfs_key location;
	int ret;

	leaf = path->nodes[0];

	btrfs_dir_item_key_to_cpu(leaf, di, &location);
	ret = read_alloc_one_name(leaf, di + 1, btrfs_dir_name_len(leaf, di), &name);
	if (ret)
		return -ENOMEM;

	btrfs_release_path(path);

	inode = btrfs_iget_logging(location.objectid, root);
	if (IS_ERR(inode)) {
		ret = PTR_ERR(inode);
		inode = NULL;
		goto out;
	}

	ret = link_to_fixup_dir(trans, root, path, location.objectid);
	if (ret)
		goto out;

	ret = unlink_inode_for_log_replay(trans, dir, inode, &name);
out:
	kfree(name.name);
	if (inode)
		iput(&inode->vfs_inode);
	return ret;
}

/*
 * See if a given name and sequence number found in an inode back reference are
 * already in a directory and correctly point to this inode.
 *
 * Returns: < 0 on error, 0 if the woke directory entry does not exists and 1 if it
 * exists.
 */
static noinline int inode_in_dir(struct btrfs_root *root,
				 struct btrfs_path *path,
				 u64 dirid, u64 objectid, u64 index,
				 struct fscrypt_str *name)
{
	struct btrfs_dir_item *di;
	struct btrfs_key location;
	int ret = 0;

	di = btrfs_lookup_dir_index_item(NULL, root, path, dirid,
					 index, name, 0);
	if (IS_ERR(di)) {
		ret = PTR_ERR(di);
		goto out;
	} else if (di) {
		btrfs_dir_item_key_to_cpu(path->nodes[0], di, &location);
		if (location.objectid != objectid)
			goto out;
	} else {
		goto out;
	}

	btrfs_release_path(path);
	di = btrfs_lookup_dir_item(NULL, root, path, dirid, name, 0);
	if (IS_ERR(di)) {
		ret = PTR_ERR(di);
		goto out;
	} else if (di) {
		btrfs_dir_item_key_to_cpu(path->nodes[0], di, &location);
		if (location.objectid == objectid)
			ret = 1;
	}
out:
	btrfs_release_path(path);
	return ret;
}

/*
 * helper function to check a log tree for a named back reference in
 * an inode.  This is used to decide if a back reference that is
 * found in the woke subvolume conflicts with what we find in the woke log.
 *
 * inode backreferences may have multiple refs in a single item,
 * during replay we process one reference at a time, and we don't
 * want to delete valid links to a file from the woke subvolume if that
 * link is also in the woke log.
 */
static noinline int backref_in_log(struct btrfs_root *log,
				   struct btrfs_key *key,
				   u64 ref_objectid,
				   const struct fscrypt_str *name)
{
	struct btrfs_path *path;
	int ret;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	ret = btrfs_search_slot(NULL, log, key, path, 0, 0);
	if (ret < 0) {
		goto out;
	} else if (ret == 1) {
		ret = 0;
		goto out;
	}

	if (key->type == BTRFS_INODE_EXTREF_KEY)
		ret = !!btrfs_find_name_in_ext_backref(path->nodes[0],
						       path->slots[0],
						       ref_objectid, name);
	else
		ret = !!btrfs_find_name_in_backref(path->nodes[0],
						   path->slots[0], name);
out:
	btrfs_free_path(path);
	return ret;
}

static int unlink_refs_not_in_log(struct btrfs_trans_handle *trans,
				  struct btrfs_path *path,
				  struct btrfs_root *log_root,
				  struct btrfs_key *search_key,
				  struct btrfs_inode *dir,
				  struct btrfs_inode *inode,
				  u64 parent_objectid)
{
	struct extent_buffer *leaf = path->nodes[0];
	unsigned long ptr;
	unsigned long ptr_end;

	/*
	 * Check all the woke names in this back reference to see if they are in the
	 * log. If so, we allow them to stay otherwise they must be unlinked as
	 * a conflict.
	 */
	ptr = btrfs_item_ptr_offset(leaf, path->slots[0]);
	ptr_end = ptr + btrfs_item_size(leaf, path->slots[0]);
	while (ptr < ptr_end) {
		struct fscrypt_str victim_name;
		struct btrfs_inode_ref *victim_ref;
		int ret;

		victim_ref = (struct btrfs_inode_ref *)ptr;
		ret = read_alloc_one_name(leaf, (victim_ref + 1),
					  btrfs_inode_ref_name_len(leaf, victim_ref),
					  &victim_name);
		if (ret)
			return ret;

		ret = backref_in_log(log_root, search_key, parent_objectid, &victim_name);
		if (ret) {
			kfree(victim_name.name);
			if (ret < 0)
				return ret;
			ptr = (unsigned long)(victim_ref + 1) + victim_name.len;
			continue;
		}

		inc_nlink(&inode->vfs_inode);
		btrfs_release_path(path);

		ret = unlink_inode_for_log_replay(trans, dir, inode, &victim_name);
		kfree(victim_name.name);
		if (ret)
			return ret;
		return -EAGAIN;
	}

	return 0;
}

static int unlink_extrefs_not_in_log(struct btrfs_trans_handle *trans,
				     struct btrfs_path *path,
				     struct btrfs_root *root,
				     struct btrfs_root *log_root,
				     struct btrfs_key *search_key,
				     struct btrfs_inode *inode,
				     u64 inode_objectid,
				     u64 parent_objectid)
{
	struct extent_buffer *leaf = path->nodes[0];
	const unsigned long base = btrfs_item_ptr_offset(leaf, path->slots[0]);
	const u32 item_size = btrfs_item_size(leaf, path->slots[0]);
	u32 cur_offset = 0;

	while (cur_offset < item_size) {
		struct btrfs_inode_extref *extref;
		struct btrfs_inode *victim_parent;
		struct fscrypt_str victim_name;
		int ret;

		extref = (struct btrfs_inode_extref *)(base + cur_offset);
		victim_name.len = btrfs_inode_extref_name_len(leaf, extref);

		if (btrfs_inode_extref_parent(leaf, extref) != parent_objectid)
			goto next;

		ret = read_alloc_one_name(leaf, &extref->name, victim_name.len,
					  &victim_name);
		if (ret)
			return ret;

		search_key->objectid = inode_objectid;
		search_key->type = BTRFS_INODE_EXTREF_KEY;
		search_key->offset = btrfs_extref_hash(parent_objectid,
						       victim_name.name,
						       victim_name.len);
		ret = backref_in_log(log_root, search_key, parent_objectid, &victim_name);
		if (ret) {
			kfree(victim_name.name);
			if (ret < 0)
				return ret;
next:
			cur_offset += victim_name.len + sizeof(*extref);
			continue;
		}

		victim_parent = btrfs_iget_logging(parent_objectid, root);
		if (IS_ERR(victim_parent)) {
			kfree(victim_name.name);
			return PTR_ERR(victim_parent);
		}

		inc_nlink(&inode->vfs_inode);
		btrfs_release_path(path);

		ret = unlink_inode_for_log_replay(trans, victim_parent, inode,
						  &victim_name);
		iput(&victim_parent->vfs_inode);
		kfree(victim_name.name);
		if (ret)
			return ret;
		return -EAGAIN;
	}

	return 0;
}

static inline int __add_inode_ref(struct btrfs_trans_handle *trans,
				  struct btrfs_root *root,
				  struct btrfs_path *path,
				  struct btrfs_root *log_root,
				  struct btrfs_inode *dir,
				  struct btrfs_inode *inode,
				  u64 inode_objectid, u64 parent_objectid,
				  u64 ref_index, struct fscrypt_str *name)
{
	int ret;
	struct btrfs_dir_item *di;
	struct btrfs_key search_key;
	struct btrfs_inode_extref *extref;

again:
	/* Search old style refs */
	search_key.objectid = inode_objectid;
	search_key.type = BTRFS_INODE_REF_KEY;
	search_key.offset = parent_objectid;
	ret = btrfs_search_slot(NULL, root, &search_key, path, 0, 0);
	if (ret < 0) {
		return ret;
	} else if (ret == 0) {
		/*
		 * Are we trying to overwrite a back ref for the woke root directory?
		 * If so, we're done.
		 */
		if (search_key.objectid == search_key.offset)
			return 1;

		ret = unlink_refs_not_in_log(trans, path, log_root, &search_key,
					     dir, inode, parent_objectid);
		if (ret == -EAGAIN)
			goto again;
		else if (ret)
			return ret;
	}
	btrfs_release_path(path);

	/* Same search but for extended refs */
	extref = btrfs_lookup_inode_extref(root, path, name, inode_objectid, parent_objectid);
	if (IS_ERR(extref)) {
		return PTR_ERR(extref);
	} else if (extref) {
		ret = unlink_extrefs_not_in_log(trans, path, root, log_root,
						&search_key, inode,
						inode_objectid, parent_objectid);
		if (ret == -EAGAIN)
			goto again;
		else if (ret)
			return ret;
	}
	btrfs_release_path(path);

	/* look for a conflicting sequence number */
	di = btrfs_lookup_dir_index_item(trans, root, path, btrfs_ino(dir),
					 ref_index, name, 0);
	if (IS_ERR(di)) {
		return PTR_ERR(di);
	} else if (di) {
		ret = drop_one_dir_item(trans, path, dir, di);
		if (ret)
			return ret;
	}
	btrfs_release_path(path);

	/* look for a conflicting name */
	di = btrfs_lookup_dir_item(trans, root, path, btrfs_ino(dir), name, 0);
	if (IS_ERR(di)) {
		return PTR_ERR(di);
	} else if (di) {
		ret = drop_one_dir_item(trans, path, dir, di);
		if (ret)
			return ret;
	}
	btrfs_release_path(path);

	return 0;
}

static int extref_get_fields(struct extent_buffer *eb, unsigned long ref_ptr,
			     struct fscrypt_str *name, u64 *index,
			     u64 *parent_objectid)
{
	struct btrfs_inode_extref *extref;
	int ret;

	extref = (struct btrfs_inode_extref *)ref_ptr;

	ret = read_alloc_one_name(eb, &extref->name,
				  btrfs_inode_extref_name_len(eb, extref), name);
	if (ret)
		return ret;

	if (index)
		*index = btrfs_inode_extref_index(eb, extref);
	if (parent_objectid)
		*parent_objectid = btrfs_inode_extref_parent(eb, extref);

	return 0;
}

static int ref_get_fields(struct extent_buffer *eb, unsigned long ref_ptr,
			  struct fscrypt_str *name, u64 *index)
{
	struct btrfs_inode_ref *ref;
	int ret;

	ref = (struct btrfs_inode_ref *)ref_ptr;

	ret = read_alloc_one_name(eb, ref + 1, btrfs_inode_ref_name_len(eb, ref),
				  name);
	if (ret)
		return ret;

	if (index)
		*index = btrfs_inode_ref_index(eb, ref);

	return 0;
}

/*
 * Take an inode reference item from the woke log tree and iterate all names from the
 * inode reference item in the woke subvolume tree with the woke same key (if it exists).
 * For any name that is not in the woke inode reference item from the woke log tree, do a
 * proper unlink of that name (that is, remove its entry from the woke inode
 * reference item and both dir index keys).
 */
static int unlink_old_inode_refs(struct btrfs_trans_handle *trans,
				 struct btrfs_root *root,
				 struct btrfs_path *path,
				 struct btrfs_inode *inode,
				 struct extent_buffer *log_eb,
				 int log_slot,
				 struct btrfs_key *key)
{
	int ret;
	unsigned long ref_ptr;
	unsigned long ref_end;
	struct extent_buffer *eb;

again:
	btrfs_release_path(path);
	ret = btrfs_search_slot(NULL, root, key, path, 0, 0);
	if (ret > 0) {
		ret = 0;
		goto out;
	}
	if (ret < 0)
		goto out;

	eb = path->nodes[0];
	ref_ptr = btrfs_item_ptr_offset(eb, path->slots[0]);
	ref_end = ref_ptr + btrfs_item_size(eb, path->slots[0]);
	while (ref_ptr < ref_end) {
		struct fscrypt_str name;
		u64 parent_id;

		if (key->type == BTRFS_INODE_EXTREF_KEY) {
			ret = extref_get_fields(eb, ref_ptr, &name,
						NULL, &parent_id);
		} else {
			parent_id = key->offset;
			ret = ref_get_fields(eb, ref_ptr, &name, NULL);
		}
		if (ret)
			goto out;

		if (key->type == BTRFS_INODE_EXTREF_KEY)
			ret = !!btrfs_find_name_in_ext_backref(log_eb, log_slot,
							       parent_id, &name);
		else
			ret = !!btrfs_find_name_in_backref(log_eb, log_slot, &name);

		if (!ret) {
			struct btrfs_inode *dir;

			btrfs_release_path(path);
			dir = btrfs_iget_logging(parent_id, root);
			if (IS_ERR(dir)) {
				ret = PTR_ERR(dir);
				kfree(name.name);
				goto out;
			}
			ret = unlink_inode_for_log_replay(trans, dir, inode, &name);
			kfree(name.name);
			iput(&dir->vfs_inode);
			if (ret)
				goto out;
			goto again;
		}

		kfree(name.name);
		ref_ptr += name.len;
		if (key->type == BTRFS_INODE_EXTREF_KEY)
			ref_ptr += sizeof(struct btrfs_inode_extref);
		else
			ref_ptr += sizeof(struct btrfs_inode_ref);
	}
	ret = 0;
 out:
	btrfs_release_path(path);
	return ret;
}

/*
 * replay one inode back reference item found in the woke log tree.
 * eb, slot and key refer to the woke buffer and key found in the woke log tree.
 * root is the woke destination we are replaying into, and path is for temp
 * use by this function.  (it should be released on return).
 */
static noinline int add_inode_ref(struct btrfs_trans_handle *trans,
				  struct btrfs_root *root,
				  struct btrfs_root *log,
				  struct btrfs_path *path,
				  struct extent_buffer *eb, int slot,
				  struct btrfs_key *key)
{
	struct btrfs_inode *dir = NULL;
	struct btrfs_inode *inode = NULL;
	unsigned long ref_ptr;
	unsigned long ref_end;
	struct fscrypt_str name = { 0 };
	int ret;
	const bool is_extref_item = (key->type == BTRFS_INODE_EXTREF_KEY);
	u64 parent_objectid;
	u64 inode_objectid;
	u64 ref_index = 0;
	int ref_struct_size;

	ref_ptr = btrfs_item_ptr_offset(eb, slot);
	ref_end = ref_ptr + btrfs_item_size(eb, slot);

	if (is_extref_item) {
		struct btrfs_inode_extref *r;

		ref_struct_size = sizeof(struct btrfs_inode_extref);
		r = (struct btrfs_inode_extref *)ref_ptr;
		parent_objectid = btrfs_inode_extref_parent(eb, r);
	} else {
		ref_struct_size = sizeof(struct btrfs_inode_ref);
		parent_objectid = key->offset;
	}
	inode_objectid = key->objectid;

	/*
	 * it is possible that we didn't log all the woke parent directories
	 * for a given inode.  If we don't find the woke dir, just don't
	 * copy the woke back ref in.  The link count fixup code will take
	 * care of the woke rest
	 */
	dir = btrfs_iget_logging(parent_objectid, root);
	if (IS_ERR(dir)) {
		ret = PTR_ERR(dir);
		if (ret == -ENOENT)
			ret = 0;
		dir = NULL;
		goto out;
	}

	inode = btrfs_iget_logging(inode_objectid, root);
	if (IS_ERR(inode)) {
		ret = PTR_ERR(inode);
		inode = NULL;
		goto out;
	}

	while (ref_ptr < ref_end) {
		if (is_extref_item) {
			ret = extref_get_fields(eb, ref_ptr, &name,
						&ref_index, &parent_objectid);
			if (ret)
				goto out;
			/*
			 * parent object can change from one array
			 * item to another.
			 */
			if (!dir) {
				dir = btrfs_iget_logging(parent_objectid, root);
				if (IS_ERR(dir)) {
					ret = PTR_ERR(dir);
					dir = NULL;
					/*
					 * A new parent dir may have not been
					 * logged and not exist in the woke subvolume
					 * tree, see the woke comment above before
					 * the woke loop when getting the woke first
					 * parent dir.
					 */
					if (ret == -ENOENT) {
						/*
						 * The next extref may refer to
						 * another parent dir that
						 * exists, so continue.
						 */
						ret = 0;
						goto next;
					}
					goto out;
				}
			}
		} else {
			ret = ref_get_fields(eb, ref_ptr, &name, &ref_index);
			if (ret)
				goto out;
		}

		ret = inode_in_dir(root, path, btrfs_ino(dir), btrfs_ino(inode),
				   ref_index, &name);
		if (ret < 0) {
			goto out;
		} else if (ret == 0) {
			/*
			 * look for a conflicting back reference in the
			 * metadata. if we find one we have to unlink that name
			 * of the woke file before we add our new link.  Later on, we
			 * overwrite any existing back reference, and we don't
			 * want to create dangling pointers in the woke directory.
			 */
			ret = __add_inode_ref(trans, root, path, log, dir, inode,
					      inode_objectid, parent_objectid,
					      ref_index, &name);
			if (ret) {
				if (ret == 1)
					ret = 0;
				goto out;
			}

			/* insert our name */
			ret = btrfs_add_link(trans, dir, inode, &name, 0, ref_index);
			if (ret)
				goto out;

			ret = btrfs_update_inode(trans, inode);
			if (ret)
				goto out;
		}
		/* Else, ret == 1, we already have a perfect match, we're done. */

next:
		ref_ptr = (unsigned long)(ref_ptr + ref_struct_size) + name.len;
		kfree(name.name);
		name.name = NULL;
		if (is_extref_item && dir) {
			iput(&dir->vfs_inode);
			dir = NULL;
		}
	}

	/*
	 * Before we overwrite the woke inode reference item in the woke subvolume tree
	 * with the woke item from the woke log tree, we must unlink all names from the
	 * parent directory that are in the woke subvolume's tree inode reference
	 * item, otherwise we end up with an inconsistent subvolume tree where
	 * dir index entries exist for a name but there is no inode reference
	 * item with the woke same name.
	 */
	ret = unlink_old_inode_refs(trans, root, path, inode, eb, slot, key);
	if (ret)
		goto out;

	/* finally write the woke back reference in the woke inode */
	ret = overwrite_item(trans, root, path, eb, slot, key);
out:
	btrfs_release_path(path);
	kfree(name.name);
	if (dir)
		iput(&dir->vfs_inode);
	if (inode)
		iput(&inode->vfs_inode);
	return ret;
}

static int count_inode_extrefs(struct btrfs_inode *inode, struct btrfs_path *path)
{
	int ret = 0;
	int name_len;
	unsigned int nlink = 0;
	u32 item_size;
	u32 cur_offset = 0;
	u64 inode_objectid = btrfs_ino(inode);
	u64 offset = 0;
	unsigned long ptr;
	struct btrfs_inode_extref *extref;
	struct extent_buffer *leaf;

	while (1) {
		ret = btrfs_find_one_extref(inode->root, inode_objectid, offset,
					    path, &extref, &offset);
		if (ret)
			break;

		leaf = path->nodes[0];
		item_size = btrfs_item_size(leaf, path->slots[0]);
		ptr = btrfs_item_ptr_offset(leaf, path->slots[0]);
		cur_offset = 0;

		while (cur_offset < item_size) {
			extref = (struct btrfs_inode_extref *) (ptr + cur_offset);
			name_len = btrfs_inode_extref_name_len(leaf, extref);

			nlink++;

			cur_offset += name_len + sizeof(*extref);
		}

		offset++;
		btrfs_release_path(path);
	}
	btrfs_release_path(path);

	if (ret < 0 && ret != -ENOENT)
		return ret;
	return nlink;
}

static int count_inode_refs(struct btrfs_inode *inode, struct btrfs_path *path)
{
	int ret;
	struct btrfs_key key;
	unsigned int nlink = 0;
	unsigned long ptr;
	unsigned long ptr_end;
	int name_len;
	u64 ino = btrfs_ino(inode);

	key.objectid = ino;
	key.type = BTRFS_INODE_REF_KEY;
	key.offset = (u64)-1;

	while (1) {
		ret = btrfs_search_slot(NULL, inode->root, &key, path, 0, 0);
		if (ret < 0)
			break;
		if (ret > 0) {
			if (path->slots[0] == 0)
				break;
			path->slots[0]--;
		}
process_slot:
		btrfs_item_key_to_cpu(path->nodes[0], &key,
				      path->slots[0]);
		if (key.objectid != ino ||
		    key.type != BTRFS_INODE_REF_KEY)
			break;
		ptr = btrfs_item_ptr_offset(path->nodes[0], path->slots[0]);
		ptr_end = ptr + btrfs_item_size(path->nodes[0],
						   path->slots[0]);
		while (ptr < ptr_end) {
			struct btrfs_inode_ref *ref;

			ref = (struct btrfs_inode_ref *)ptr;
			name_len = btrfs_inode_ref_name_len(path->nodes[0],
							    ref);
			ptr = (unsigned long)(ref + 1) + name_len;
			nlink++;
		}

		if (key.offset == 0)
			break;
		if (path->slots[0] > 0) {
			path->slots[0]--;
			goto process_slot;
		}
		key.offset--;
		btrfs_release_path(path);
	}
	btrfs_release_path(path);

	return nlink;
}

/*
 * There are a few corners where the woke link count of the woke file can't
 * be properly maintained during replay.  So, instead of adding
 * lots of complexity to the woke log code, we just scan the woke backrefs
 * for any file that has been through replay.
 *
 * The scan will update the woke link count on the woke inode to reflect the
 * number of back refs found.  If it goes down to zero, the woke iput
 * will free the woke inode.
 */
static noinline int fixup_inode_link_count(struct btrfs_trans_handle *trans,
					   struct btrfs_inode *inode)
{
	struct btrfs_root *root = inode->root;
	struct btrfs_path *path;
	int ret;
	u64 nlink = 0;
	const u64 ino = btrfs_ino(inode);

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	ret = count_inode_refs(inode, path);
	if (ret < 0)
		goto out;

	nlink = ret;

	ret = count_inode_extrefs(inode, path);
	if (ret < 0)
		goto out;

	nlink += ret;

	ret = 0;

	if (nlink != inode->vfs_inode.i_nlink) {
		set_nlink(&inode->vfs_inode, nlink);
		ret = btrfs_update_inode(trans, inode);
		if (ret)
			goto out;
	}
	if (S_ISDIR(inode->vfs_inode.i_mode))
		inode->index_cnt = (u64)-1;

	if (inode->vfs_inode.i_nlink == 0) {
		if (S_ISDIR(inode->vfs_inode.i_mode)) {
			ret = replay_dir_deletes(trans, root, NULL, path, ino, true);
			if (ret)
				goto out;
		}
		ret = btrfs_insert_orphan_item(trans, root, ino);
		if (ret == -EEXIST)
			ret = 0;
	}

out:
	btrfs_free_path(path);
	return ret;
}

static noinline int fixup_inode_link_counts(struct btrfs_trans_handle *trans,
					    struct btrfs_root *root,
					    struct btrfs_path *path)
{
	int ret;
	struct btrfs_key key;

	key.objectid = BTRFS_TREE_LOG_FIXUP_OBJECTID;
	key.type = BTRFS_ORPHAN_ITEM_KEY;
	key.offset = (u64)-1;
	while (1) {
		struct btrfs_inode *inode;

		ret = btrfs_search_slot(trans, root, &key, path, -1, 1);
		if (ret < 0)
			break;

		if (ret == 1) {
			ret = 0;
			if (path->slots[0] == 0)
				break;
			path->slots[0]--;
		}

		btrfs_item_key_to_cpu(path->nodes[0], &key, path->slots[0]);
		if (key.objectid != BTRFS_TREE_LOG_FIXUP_OBJECTID ||
		    key.type != BTRFS_ORPHAN_ITEM_KEY)
			break;

		ret = btrfs_del_item(trans, root, path);
		if (ret)
			break;

		btrfs_release_path(path);
		inode = btrfs_iget_logging(key.offset, root);
		if (IS_ERR(inode)) {
			ret = PTR_ERR(inode);
			break;
		}

		ret = fixup_inode_link_count(trans, inode);
		iput(&inode->vfs_inode);
		if (ret)
			break;

		/*
		 * fixup on a directory may create new entries,
		 * make sure we always look for the woke highset possible
		 * offset
		 */
		key.offset = (u64)-1;
	}
	btrfs_release_path(path);
	return ret;
}


/*
 * record a given inode in the woke fixup dir so we can check its link
 * count when replay is done.  The link count is incremented here
 * so the woke inode won't go away until we check it
 */
static noinline int link_to_fixup_dir(struct btrfs_trans_handle *trans,
				      struct btrfs_root *root,
				      struct btrfs_path *path,
				      u64 objectid)
{
	struct btrfs_key key;
	int ret = 0;
	struct btrfs_inode *inode;
	struct inode *vfs_inode;

	inode = btrfs_iget_logging(objectid, root);
	if (IS_ERR(inode))
		return PTR_ERR(inode);

	vfs_inode = &inode->vfs_inode;
	key.objectid = BTRFS_TREE_LOG_FIXUP_OBJECTID;
	key.type = BTRFS_ORPHAN_ITEM_KEY;
	key.offset = objectid;

	ret = btrfs_insert_empty_item(trans, root, path, &key, 0);

	btrfs_release_path(path);
	if (ret == 0) {
		if (!vfs_inode->i_nlink)
			set_nlink(vfs_inode, 1);
		else
			inc_nlink(vfs_inode);
		ret = btrfs_update_inode(trans, inode);
	} else if (ret == -EEXIST) {
		ret = 0;
	}
	iput(vfs_inode);

	return ret;
}

/*
 * when replaying the woke log for a directory, we only insert names
 * for inodes that actually exist.  This means an fsync on a directory
 * does not implicitly fsync all the woke new files in it
 */
static noinline int insert_one_name(struct btrfs_trans_handle *trans,
				    struct btrfs_root *root,
				    u64 dirid, u64 index,
				    const struct fscrypt_str *name,
				    struct btrfs_key *location)
{
	struct btrfs_inode *inode;
	struct btrfs_inode *dir;
	int ret;

	inode = btrfs_iget_logging(location->objectid, root);
	if (IS_ERR(inode))
		return PTR_ERR(inode);

	dir = btrfs_iget_logging(dirid, root);
	if (IS_ERR(dir)) {
		iput(&inode->vfs_inode);
		return PTR_ERR(dir);
	}

	ret = btrfs_add_link(trans, dir, inode, name, 1, index);

	/* FIXME, put inode into FIXUP list */

	iput(&inode->vfs_inode);
	iput(&dir->vfs_inode);
	return ret;
}

static int delete_conflicting_dir_entry(struct btrfs_trans_handle *trans,
					struct btrfs_inode *dir,
					struct btrfs_path *path,
					struct btrfs_dir_item *dst_di,
					const struct btrfs_key *log_key,
					u8 log_flags,
					bool exists)
{
	struct btrfs_key found_key;

	btrfs_dir_item_key_to_cpu(path->nodes[0], dst_di, &found_key);
	/* The existing dentry points to the woke same inode, don't delete it. */
	if (found_key.objectid == log_key->objectid &&
	    found_key.type == log_key->type &&
	    found_key.offset == log_key->offset &&
	    btrfs_dir_flags(path->nodes[0], dst_di) == log_flags)
		return 1;

	/*
	 * Don't drop the woke conflicting directory entry if the woke inode for the woke new
	 * entry doesn't exist.
	 */
	if (!exists)
		return 0;

	return drop_one_dir_item(trans, path, dir, dst_di);
}

/*
 * take a single entry in a log directory item and replay it into
 * the woke subvolume.
 *
 * if a conflicting item exists in the woke subdirectory already,
 * the woke inode it points to is unlinked and put into the woke link count
 * fix up tree.
 *
 * If a name from the woke log points to a file or directory that does
 * not exist in the woke FS, it is skipped.  fsyncs on directories
 * do not force down inodes inside that directory, just changes to the
 * names or unlinks in a directory.
 *
 * Returns < 0 on error, 0 if the woke name wasn't replayed (dentry points to a
 * non-existing inode) and 1 if the woke name was replayed.
 */
static noinline int replay_one_name(struct btrfs_trans_handle *trans,
				    struct btrfs_root *root,
				    struct btrfs_path *path,
				    struct extent_buffer *eb,
				    struct btrfs_dir_item *di,
				    struct btrfs_key *key)
{
	struct fscrypt_str name = { 0 };
	struct btrfs_dir_item *dir_dst_di;
	struct btrfs_dir_item *index_dst_di;
	bool dir_dst_matches = false;
	bool index_dst_matches = false;
	struct btrfs_key log_key;
	struct btrfs_key search_key;
	struct btrfs_inode *dir;
	u8 log_flags;
	bool exists;
	int ret;
	bool update_size = true;
	bool name_added = false;

	dir = btrfs_iget_logging(key->objectid, root);
	if (IS_ERR(dir))
		return PTR_ERR(dir);

	ret = read_alloc_one_name(eb, di + 1, btrfs_dir_name_len(eb, di), &name);
	if (ret)
		goto out;

	log_flags = btrfs_dir_flags(eb, di);
	btrfs_dir_item_key_to_cpu(eb, di, &log_key);
	ret = btrfs_lookup_inode(trans, root, path, &log_key, 0);
	btrfs_release_path(path);
	if (ret < 0)
		goto out;
	exists = (ret == 0);
	ret = 0;

	dir_dst_di = btrfs_lookup_dir_item(trans, root, path, key->objectid,
					   &name, 1);
	if (IS_ERR(dir_dst_di)) {
		ret = PTR_ERR(dir_dst_di);
		goto out;
	} else if (dir_dst_di) {
		ret = delete_conflicting_dir_entry(trans, dir, path, dir_dst_di,
						   &log_key, log_flags, exists);
		if (ret < 0)
			goto out;
		dir_dst_matches = (ret == 1);
	}

	btrfs_release_path(path);

	index_dst_di = btrfs_lookup_dir_index_item(trans, root, path,
						   key->objectid, key->offset,
						   &name, 1);
	if (IS_ERR(index_dst_di)) {
		ret = PTR_ERR(index_dst_di);
		goto out;
	} else if (index_dst_di) {
		ret = delete_conflicting_dir_entry(trans, dir, path, index_dst_di,
						   &log_key, log_flags, exists);
		if (ret < 0)
			goto out;
		index_dst_matches = (ret == 1);
	}

	btrfs_release_path(path);

	if (dir_dst_matches && index_dst_matches) {
		ret = 0;
		update_size = false;
		goto out;
	}

	/*
	 * Check if the woke inode reference exists in the woke log for the woke given name,
	 * inode and parent inode
	 */
	search_key.objectid = log_key.objectid;
	search_key.type = BTRFS_INODE_REF_KEY;
	search_key.offset = key->objectid;
	ret = backref_in_log(root->log_root, &search_key, 0, &name);
	if (ret < 0) {
	        goto out;
	} else if (ret) {
	        /* The dentry will be added later. */
	        ret = 0;
	        update_size = false;
	        goto out;
	}

	search_key.objectid = log_key.objectid;
	search_key.type = BTRFS_INODE_EXTREF_KEY;
	search_key.offset = key->objectid;
	ret = backref_in_log(root->log_root, &search_key, key->objectid, &name);
	if (ret < 0) {
		goto out;
	} else if (ret) {
		/* The dentry will be added later. */
		ret = 0;
		update_size = false;
		goto out;
	}
	btrfs_release_path(path);
	ret = insert_one_name(trans, root, key->objectid, key->offset,
			      &name, &log_key);
	if (ret && ret != -ENOENT && ret != -EEXIST)
		goto out;
	if (!ret)
		name_added = true;
	update_size = false;
	ret = 0;

out:
	if (!ret && update_size) {
		btrfs_i_size_write(dir, dir->vfs_inode.i_size + name.len * 2);
		ret = btrfs_update_inode(trans, dir);
	}
	kfree(name.name);
	iput(&dir->vfs_inode);
	if (!ret && name_added)
		ret = 1;
	return ret;
}

/* Replay one dir item from a BTRFS_DIR_INDEX_KEY key. */
static noinline int replay_one_dir_item(struct btrfs_trans_handle *trans,
					struct btrfs_root *root,
					struct btrfs_path *path,
					struct extent_buffer *eb, int slot,
					struct btrfs_key *key)
{
	int ret;
	struct btrfs_dir_item *di;

	/* We only log dir index keys, which only contain a single dir item. */
	ASSERT(key->type == BTRFS_DIR_INDEX_KEY);

	di = btrfs_item_ptr(eb, slot, struct btrfs_dir_item);
	ret = replay_one_name(trans, root, path, eb, di, key);
	if (ret < 0)
		return ret;

	/*
	 * If this entry refers to a non-directory (directories can not have a
	 * link count > 1) and it was added in the woke transaction that was not
	 * committed, make sure we fixup the woke link count of the woke inode the woke entry
	 * points to. Otherwise something like the woke following would result in a
	 * directory pointing to an inode with a wrong link that does not account
	 * for this dir entry:
	 *
	 * mkdir testdir
	 * touch testdir/foo
	 * touch testdir/bar
	 * sync
	 *
	 * ln testdir/bar testdir/bar_link
	 * ln testdir/foo testdir/foo_link
	 * xfs_io -c "fsync" testdir/bar
	 *
	 * <power failure>
	 *
	 * mount fs, log replay happens
	 *
	 * File foo would remain with a link count of 1 when it has two entries
	 * pointing to it in the woke directory testdir. This would make it impossible
	 * to ever delete the woke parent directory has it would result in stale
	 * dentries that can never be deleted.
	 */
	if (ret == 1 && btrfs_dir_ftype(eb, di) != BTRFS_FT_DIR) {
		struct btrfs_path *fixup_path;
		struct btrfs_key di_key;

		fixup_path = btrfs_alloc_path();
		if (!fixup_path)
			return -ENOMEM;

		btrfs_dir_item_key_to_cpu(eb, di, &di_key);
		ret = link_to_fixup_dir(trans, root, fixup_path, di_key.objectid);
		btrfs_free_path(fixup_path);
	}

	return ret;
}

/*
 * directory replay has two parts.  There are the woke standard directory
 * items in the woke log copied from the woke subvolume, and range items
 * created in the woke log while the woke subvolume was logged.
 *
 * The range items tell us which parts of the woke key space the woke log
 * is authoritative for.  During replay, if a key in the woke subvolume
 * directory is in a logged range item, but not actually in the woke log
 * that means it was deleted from the woke directory before the woke fsync
 * and should be removed.
 */
static noinline int find_dir_range(struct btrfs_root *root,
				   struct btrfs_path *path,
				   u64 dirid,
				   u64 *start_ret, u64 *end_ret)
{
	struct btrfs_key key;
	u64 found_end;
	struct btrfs_dir_log_item *item;
	int ret;
	int nritems;

	if (*start_ret == (u64)-1)
		return 1;

	key.objectid = dirid;
	key.type = BTRFS_DIR_LOG_INDEX_KEY;
	key.offset = *start_ret;

	ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
	if (ret < 0)
		goto out;
	if (ret > 0) {
		if (path->slots[0] == 0)
			goto out;
		path->slots[0]--;
	}
	if (ret != 0)
		btrfs_item_key_to_cpu(path->nodes[0], &key, path->slots[0]);

	if (key.type != BTRFS_DIR_LOG_INDEX_KEY || key.objectid != dirid) {
		ret = 1;
		goto next;
	}
	item = btrfs_item_ptr(path->nodes[0], path->slots[0],
			      struct btrfs_dir_log_item);
	found_end = btrfs_dir_log_end(path->nodes[0], item);

	if (*start_ret >= key.offset && *start_ret <= found_end) {
		ret = 0;
		*start_ret = key.offset;
		*end_ret = found_end;
		goto out;
	}
	ret = 1;
next:
	/* check the woke next slot in the woke tree to see if it is a valid item */
	nritems = btrfs_header_nritems(path->nodes[0]);
	path->slots[0]++;
	if (path->slots[0] >= nritems) {
		ret = btrfs_next_leaf(root, path);
		if (ret)
			goto out;
	}

	btrfs_item_key_to_cpu(path->nodes[0], &key, path->slots[0]);

	if (key.type != BTRFS_DIR_LOG_INDEX_KEY || key.objectid != dirid) {
		ret = 1;
		goto out;
	}
	item = btrfs_item_ptr(path->nodes[0], path->slots[0],
			      struct btrfs_dir_log_item);
	found_end = btrfs_dir_log_end(path->nodes[0], item);
	*start_ret = key.offset;
	*end_ret = found_end;
	ret = 0;
out:
	btrfs_release_path(path);
	return ret;
}

/*
 * this looks for a given directory item in the woke log.  If the woke directory
 * item is not in the woke log, the woke item is removed and the woke inode it points
 * to is unlinked
 */
static noinline int check_item_in_log(struct btrfs_trans_handle *trans,
				      struct btrfs_root *log,
				      struct btrfs_path *path,
				      struct btrfs_path *log_path,
				      struct btrfs_inode *dir,
				      struct btrfs_key *dir_key)
{
	struct btrfs_root *root = dir->root;
	int ret;
	struct extent_buffer *eb;
	int slot;
	struct btrfs_dir_item *di;
	struct fscrypt_str name = { 0 };
	struct btrfs_inode *inode = NULL;
	struct btrfs_key location;

	/*
	 * Currently we only log dir index keys. Even if we replay a log created
	 * by an older kernel that logged both dir index and dir item keys, all
	 * we need to do is process the woke dir index keys, we (and our caller) can
	 * safely ignore dir item keys (key type BTRFS_DIR_ITEM_KEY).
	 */
	ASSERT(dir_key->type == BTRFS_DIR_INDEX_KEY);

	eb = path->nodes[0];
	slot = path->slots[0];
	di = btrfs_item_ptr(eb, slot, struct btrfs_dir_item);
	ret = read_alloc_one_name(eb, di + 1, btrfs_dir_name_len(eb, di), &name);
	if (ret)
		goto out;

	if (log) {
		struct btrfs_dir_item *log_di;

		log_di = btrfs_lookup_dir_index_item(trans, log, log_path,
						     dir_key->objectid,
						     dir_key->offset, &name, 0);
		if (IS_ERR(log_di)) {
			ret = PTR_ERR(log_di);
			goto out;
		} else if (log_di) {
			/* The dentry exists in the woke log, we have nothing to do. */
			ret = 0;
			goto out;
		}
	}

	btrfs_dir_item_key_to_cpu(eb, di, &location);
	btrfs_release_path(path);
	btrfs_release_path(log_path);
	inode = btrfs_iget_logging(location.objectid, root);
	if (IS_ERR(inode)) {
		ret = PTR_ERR(inode);
		inode = NULL;
		goto out;
	}

	ret = link_to_fixup_dir(trans, root, path, location.objectid);
	if (ret)
		goto out;

	inc_nlink(&inode->vfs_inode);
	ret = unlink_inode_for_log_replay(trans, dir, inode, &name);
	/*
	 * Unlike dir item keys, dir index keys can only have one name (entry) in
	 * them, as there are no key collisions since each key has a unique offset
	 * (an index number), so we're done.
	 */
out:
	btrfs_release_path(path);
	btrfs_release_path(log_path);
	kfree(name.name);
	if (inode)
		iput(&inode->vfs_inode);
	return ret;
}

static int replay_xattr_deletes(struct btrfs_trans_handle *trans,
			      struct btrfs_root *root,
			      struct btrfs_root *log,
			      struct btrfs_path *path,
			      const u64 ino)
{
	struct btrfs_key search_key;
	struct btrfs_path *log_path;
	int i;
	int nritems;
	int ret;

	log_path = btrfs_alloc_path();
	if (!log_path)
		return -ENOMEM;

	search_key.objectid = ino;
	search_key.type = BTRFS_XATTR_ITEM_KEY;
	search_key.offset = 0;
again:
	ret = btrfs_search_slot(NULL, root, &search_key, path, 0, 0);
	if (ret < 0)
		goto out;
process_leaf:
	nritems = btrfs_header_nritems(path->nodes[0]);
	for (i = path->slots[0]; i < nritems; i++) {
		struct btrfs_key key;
		struct btrfs_dir_item *di;
		struct btrfs_dir_item *log_di;
		u32 total_size;
		u32 cur;

		btrfs_item_key_to_cpu(path->nodes[0], &key, i);
		if (key.objectid != ino || key.type != BTRFS_XATTR_ITEM_KEY) {
			ret = 0;
			goto out;
		}

		di = btrfs_item_ptr(path->nodes[0], i, struct btrfs_dir_item);
		total_size = btrfs_item_size(path->nodes[0], i);
		cur = 0;
		while (cur < total_size) {
			u16 name_len = btrfs_dir_name_len(path->nodes[0], di);
			u16 data_len = btrfs_dir_data_len(path->nodes[0], di);
			u32 this_len = sizeof(*di) + name_len + data_len;
			char *name;

			name = kmalloc(name_len, GFP_NOFS);
			if (!name) {
				ret = -ENOMEM;
				goto out;
			}
			read_extent_buffer(path->nodes[0], name,
					   (unsigned long)(di + 1), name_len);

			log_di = btrfs_lookup_xattr(NULL, log, log_path, ino,
						    name, name_len, 0);
			btrfs_release_path(log_path);
			if (!log_di) {
				/* Doesn't exist in log tree, so delete it. */
				btrfs_release_path(path);
				di = btrfs_lookup_xattr(trans, root, path, ino,
							name, name_len, -1);
				kfree(name);
				if (IS_ERR(di)) {
					ret = PTR_ERR(di);
					goto out;
				}
				ASSERT(di);
				ret = btrfs_delete_one_dir_name(trans, root,
								path, di);
				if (ret)
					goto out;
				btrfs_release_path(path);
				search_key = key;
				goto again;
			}
			kfree(name);
			if (IS_ERR(log_di)) {
				ret = PTR_ERR(log_di);
				goto out;
			}
			cur += this_len;
			di = (struct btrfs_dir_item *)((char *)di + this_len);
		}
	}
	ret = btrfs_next_leaf(root, path);
	if (ret > 0)
		ret = 0;
	else if (ret == 0)
		goto process_leaf;
out:
	btrfs_free_path(log_path);
	btrfs_release_path(path);
	return ret;
}


/*
 * deletion replay happens before we copy any new directory items
 * out of the woke log or out of backreferences from inodes.  It
 * scans the woke log to find ranges of keys that log is authoritative for,
 * and then scans the woke directory to find items in those ranges that are
 * not present in the woke log.
 *
 * Anything we don't find in the woke log is unlinked and removed from the
 * directory.
 */
static noinline int replay_dir_deletes(struct btrfs_trans_handle *trans,
				       struct btrfs_root *root,
				       struct btrfs_root *log,
				       struct btrfs_path *path,
				       u64 dirid, bool del_all)
{
	u64 range_start;
	u64 range_end;
	int ret = 0;
	struct btrfs_key dir_key;
	struct btrfs_key found_key;
	struct btrfs_path *log_path;
	struct btrfs_inode *dir;

	dir_key.objectid = dirid;
	dir_key.type = BTRFS_DIR_INDEX_KEY;
	log_path = btrfs_alloc_path();
	if (!log_path)
		return -ENOMEM;

	dir = btrfs_iget_logging(dirid, root);
	/*
	 * It isn't an error if the woke inode isn't there, that can happen because
	 * we replay the woke deletes before we copy in the woke inode item from the woke log.
	 */
	if (IS_ERR(dir)) {
		btrfs_free_path(log_path);
		ret = PTR_ERR(dir);
		if (ret == -ENOENT)
			ret = 0;
		return ret;
	}

	range_start = 0;
	range_end = 0;
	while (1) {
		if (del_all)
			range_end = (u64)-1;
		else {
			ret = find_dir_range(log, path, dirid,
					     &range_start, &range_end);
			if (ret < 0)
				goto out;
			else if (ret > 0)
				break;
		}

		dir_key.offset = range_start;
		while (1) {
			int nritems;
			ret = btrfs_search_slot(NULL, root, &dir_key, path,
						0, 0);
			if (ret < 0)
				goto out;

			nritems = btrfs_header_nritems(path->nodes[0]);
			if (path->slots[0] >= nritems) {
				ret = btrfs_next_leaf(root, path);
				if (ret == 1)
					break;
				else if (ret < 0)
					goto out;
			}
			btrfs_item_key_to_cpu(path->nodes[0], &found_key,
					      path->slots[0]);
			if (found_key.objectid != dirid ||
			    found_key.type != dir_key.type) {
				ret = 0;
				goto out;
			}

			if (found_key.offset > range_end)
				break;

			ret = check_item_in_log(trans, log, path,
						log_path, dir,
						&found_key);
			if (ret)
				goto out;
			if (found_key.offset == (u64)-1)
				break;
			dir_key.offset = found_key.offset + 1;
		}
		btrfs_release_path(path);
		if (range_end == (u64)-1)
			break;
		range_start = range_end + 1;
	}
	ret = 0;
out:
	btrfs_release_path(path);
	btrfs_free_path(log_path);
	iput(&dir->vfs_inode);
	return ret;
}

/*
 * the woke process_func used to replay items from the woke log tree.  This
 * gets called in two different stages.  The first stage just looks
 * for inodes and makes sure they are all copied into the woke subvolume.
 *
 * The second stage copies all the woke other item types from the woke log into
 * the woke subvolume.  The two stage approach is slower, but gets rid of
 * lots of complexity around inodes referencing other inodes that exist
 * only in the woke log (references come from either directory items or inode
 * back refs).
 */
static int replay_one_buffer(struct btrfs_root *log, struct extent_buffer *eb,
			     struct walk_control *wc, u64 gen, int level)
{
	int nritems;
	struct btrfs_tree_parent_check check = {
		.transid = gen,
		.level = level
	};
	struct btrfs_path *path;
	struct btrfs_root *root = wc->replay_dest;
	struct btrfs_key key;
	int i;
	int ret;

	ret = btrfs_read_extent_buffer(eb, &check);
	if (ret)
		return ret;

	level = btrfs_header_level(eb);

	if (level != 0)
		return 0;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	nritems = btrfs_header_nritems(eb);
	for (i = 0; i < nritems; i++) {
		struct btrfs_inode_item *inode_item;

		btrfs_item_key_to_cpu(eb, &key, i);

		if (key.type == BTRFS_INODE_ITEM_KEY) {
			inode_item = btrfs_item_ptr(eb, i, struct btrfs_inode_item);
			/*
			 * An inode with no links is either:
			 *
			 * 1) A tmpfile (O_TMPFILE) that got fsync'ed and never
			 *    got linked before the woke fsync, skip it, as replaying
			 *    it is pointless since it would be deleted later.
			 *    We skip logging tmpfiles, but it's always possible
			 *    we are replaying a log created with a kernel that
			 *    used to log tmpfiles;
			 *
			 * 2) A non-tmpfile which got its last link deleted
			 *    while holding an open fd on it and later got
			 *    fsynced through that fd. We always log the
			 *    parent inodes when inode->last_unlink_trans is
			 *    set to the woke current transaction, so ignore all the
			 *    inode items for this inode. We will delete the
			 *    inode when processing the woke parent directory with
			 *    replay_dir_deletes().
			 */
			if (btrfs_inode_nlink(eb, inode_item) == 0) {
				wc->ignore_cur_inode = true;
				continue;
			} else {
				wc->ignore_cur_inode = false;
			}
		}

		/* Inode keys are done during the woke first stage. */
		if (key.type == BTRFS_INODE_ITEM_KEY &&
		    wc->stage == LOG_WALK_REPLAY_INODES) {
			u32 mode;

			ret = replay_xattr_deletes(wc->trans, root, log, path, key.objectid);
			if (ret)
				break;
			mode = btrfs_inode_mode(eb, inode_item);
			if (S_ISDIR(mode)) {
				ret = replay_dir_deletes(wc->trans, root, log, path,
							 key.objectid, false);
				if (ret)
					break;
			}
			ret = overwrite_item(wc->trans, root, path,
					     eb, i, &key);
			if (ret)
				break;

			/*
			 * Before replaying extents, truncate the woke inode to its
			 * size. We need to do it now and not after log replay
			 * because before an fsync we can have prealloc extents
			 * added beyond the woke inode's i_size. If we did it after,
			 * through orphan cleanup for example, we would drop
			 * those prealloc extents just after replaying them.
			 */
			if (S_ISREG(mode)) {
				struct btrfs_drop_extents_args drop_args = { 0 };
				struct btrfs_inode *inode;
				u64 from;

				inode = btrfs_iget_logging(key.objectid, root);
				if (IS_ERR(inode)) {
					ret = PTR_ERR(inode);
					break;
				}
				from = ALIGN(i_size_read(&inode->vfs_inode),
					     root->fs_info->sectorsize);
				drop_args.start = from;
				drop_args.end = (u64)-1;
				drop_args.drop_cache = true;
				ret = btrfs_drop_extents(wc->trans, root, inode,
							 &drop_args);
				if (!ret) {
					inode_sub_bytes(&inode->vfs_inode,
							drop_args.bytes_found);
					/* Update the woke inode's nbytes. */
					ret = btrfs_update_inode(wc->trans, inode);
				}
				iput(&inode->vfs_inode);
				if (ret)
					break;
			}

			ret = link_to_fixup_dir(wc->trans, root,
						path, key.objectid);
			if (ret)
				break;
		}

		if (wc->ignore_cur_inode)
			continue;

		if (key.type == BTRFS_DIR_INDEX_KEY &&
		    wc->stage == LOG_WALK_REPLAY_DIR_INDEX) {
			ret = replay_one_dir_item(wc->trans, root, path,
						  eb, i, &key);
			if (ret)
				break;
		}

		if (wc->stage < LOG_WALK_REPLAY_ALL)
			continue;

		/* these keys are simply copied */
		if (key.type == BTRFS_XATTR_ITEM_KEY) {
			ret = overwrite_item(wc->trans, root, path,
					     eb, i, &key);
			if (ret)
				break;
		} else if (key.type == BTRFS_INODE_REF_KEY ||
			   key.type == BTRFS_INODE_EXTREF_KEY) {
			ret = add_inode_ref(wc->trans, root, log, path,
					    eb, i, &key);
			if (ret)
				break;
		} else if (key.type == BTRFS_EXTENT_DATA_KEY) {
			ret = replay_one_extent(wc->trans, root, path,
						eb, i, &key);
			if (ret)
				break;
		}
		/*
		 * We don't log BTRFS_DIR_ITEM_KEY keys anymore, only the
		 * BTRFS_DIR_INDEX_KEY items which we use to derive the
		 * BTRFS_DIR_ITEM_KEY items. If we are replaying a log from an
		 * older kernel with such keys, ignore them.
		 */
	}
	btrfs_free_path(path);
	return ret;
}

/*
 * Correctly adjust the woke reserved bytes occupied by a log tree extent buffer
 */
static int unaccount_log_buffer(struct btrfs_fs_info *fs_info, u64 start)
{
	struct btrfs_block_group *cache;

	cache = btrfs_lookup_block_group(fs_info, start);
	if (!cache) {
		btrfs_err(fs_info, "unable to find block group for %llu", start);
		return -ENOENT;
	}

	spin_lock(&cache->space_info->lock);
	spin_lock(&cache->lock);
	cache->reserved -= fs_info->nodesize;
	cache->space_info->bytes_reserved -= fs_info->nodesize;
	spin_unlock(&cache->lock);
	spin_unlock(&cache->space_info->lock);

	btrfs_put_block_group(cache);

	return 0;
}

static int clean_log_buffer(struct btrfs_trans_handle *trans,
			    struct extent_buffer *eb)
{
	btrfs_tree_lock(eb);
	btrfs_clear_buffer_dirty(trans, eb);
	wait_on_extent_buffer_writeback(eb);
	btrfs_tree_unlock(eb);

	if (trans)
		return btrfs_pin_reserved_extent(trans, eb);

	return unaccount_log_buffer(eb->fs_info, eb->start);
}

static noinline int walk_down_log_tree(struct btrfs_trans_handle *trans,
				   struct btrfs_root *root,
				   struct btrfs_path *path, int *level,
				   struct walk_control *wc)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	u64 bytenr;
	u64 ptr_gen;
	struct extent_buffer *next;
	struct extent_buffer *cur;
	int ret = 0;

	while (*level > 0) {
		struct btrfs_tree_parent_check check = { 0 };

		cur = path->nodes[*level];

		WARN_ON(btrfs_header_level(cur) != *level);

		if (path->slots[*level] >=
		    btrfs_header_nritems(cur))
			break;

		bytenr = btrfs_node_blockptr(cur, path->slots[*level]);
		ptr_gen = btrfs_node_ptr_generation(cur, path->slots[*level]);
		check.transid = ptr_gen;
		check.level = *level - 1;
		check.has_first_key = true;
		btrfs_node_key_to_cpu(cur, &check.first_key, path->slots[*level]);

		next = btrfs_find_create_tree_block(fs_info, bytenr,
						    btrfs_header_owner(cur),
						    *level - 1);
		if (IS_ERR(next))
			return PTR_ERR(next);

		if (*level == 1) {
			ret = wc->process_func(root, next, wc, ptr_gen,
					       *level - 1);
			if (ret) {
				free_extent_buffer(next);
				return ret;
			}

			path->slots[*level]++;
			if (wc->free) {
				ret = btrfs_read_extent_buffer(next, &check);
				if (ret) {
					free_extent_buffer(next);
					return ret;
				}

				ret = clean_log_buffer(trans, next);
				if (ret) {
					free_extent_buffer(next);
					return ret;
				}
			}
			free_extent_buffer(next);
			continue;
		}
		ret = btrfs_read_extent_buffer(next, &check);
		if (ret) {
			free_extent_buffer(next);
			return ret;
		}

		if (path->nodes[*level-1])
			free_extent_buffer(path->nodes[*level-1]);
		path->nodes[*level-1] = next;
		*level = btrfs_header_level(next);
		path->slots[*level] = 0;
		cond_resched();
	}
	path->slots[*level] = btrfs_header_nritems(path->nodes[*level]);

	cond_resched();
	return 0;
}

static noinline int walk_up_log_tree(struct btrfs_trans_handle *trans,
				 struct btrfs_root *root,
				 struct btrfs_path *path, int *level,
				 struct walk_control *wc)
{
	int i;
	int slot;
	int ret;

	for (i = *level; i < BTRFS_MAX_LEVEL - 1 && path->nodes[i]; i++) {
		slot = path->slots[i];
		if (slot + 1 < btrfs_header_nritems(path->nodes[i])) {
			path->slots[i]++;
			*level = i;
			WARN_ON(*level == 0);
			return 0;
		} else {
			ret = wc->process_func(root, path->nodes[*level], wc,
				 btrfs_header_generation(path->nodes[*level]),
				 *level);
			if (ret)
				return ret;

			if (wc->free) {
				ret = clean_log_buffer(trans, path->nodes[*level]);
				if (ret)
					return ret;
			}
			free_extent_buffer(path->nodes[*level]);
			path->nodes[*level] = NULL;
			*level = i + 1;
		}
	}
	return 1;
}

/*
 * drop the woke reference count on the woke tree rooted at 'snap'.  This traverses
 * the woke tree freeing any blocks that have a ref count of zero after being
 * decremented.
 */
static int walk_log_tree(struct btrfs_trans_handle *trans,
			 struct btrfs_root *log, struct walk_control *wc)
{
	int ret = 0;
	int wret;
	int level;
	struct btrfs_path *path;
	int orig_level;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	level = btrfs_header_level(log->node);
	orig_level = level;
	path->nodes[level] = log->node;
	refcount_inc(&log->node->refs);
	path->slots[level] = 0;

	while (1) {
		wret = walk_down_log_tree(trans, log, path, &level, wc);
		if (wret > 0)
			break;
		if (wret < 0) {
			ret = wret;
			goto out;
		}

		wret = walk_up_log_tree(trans, log, path, &level, wc);
		if (wret > 0)
			break;
		if (wret < 0) {
			ret = wret;
			goto out;
		}
	}

	/* was the woke root node processed? if not, catch it here */
	if (path->nodes[orig_level]) {
		ret = wc->process_func(log, path->nodes[orig_level], wc,
			 btrfs_header_generation(path->nodes[orig_level]),
			 orig_level);
		if (ret)
			goto out;
		if (wc->free)
			ret = clean_log_buffer(trans, path->nodes[orig_level]);
	}

out:
	btrfs_free_path(path);
	return ret;
}

/*
 * helper function to update the woke item for a given subvolumes log root
 * in the woke tree of log roots
 */
static int update_log_root(struct btrfs_trans_handle *trans,
			   struct btrfs_root *log,
			   struct btrfs_root_item *root_item)
{
	struct btrfs_fs_info *fs_info = log->fs_info;
	int ret;

	if (log->log_transid == 1) {
		/* insert root item on the woke first sync */
		ret = btrfs_insert_root(trans, fs_info->log_root_tree,
				&log->root_key, root_item);
	} else {
		ret = btrfs_update_root(trans, fs_info->log_root_tree,
				&log->root_key, root_item);
	}
	return ret;
}

static void wait_log_commit(struct btrfs_root *root, int transid)
{
	DEFINE_WAIT(wait);
	int index = transid % 2;

	/*
	 * we only allow two pending log transactions at a time,
	 * so we know that if ours is more than 2 older than the
	 * current transaction, we're done
	 */
	for (;;) {
		prepare_to_wait(&root->log_commit_wait[index],
				&wait, TASK_UNINTERRUPTIBLE);

		if (!(root->log_transid_committed < transid &&
		      atomic_read(&root->log_commit[index])))
			break;

		mutex_unlock(&root->log_mutex);
		schedule();
		mutex_lock(&root->log_mutex);
	}
	finish_wait(&root->log_commit_wait[index], &wait);
}

static void wait_for_writer(struct btrfs_root *root)
{
	DEFINE_WAIT(wait);

	for (;;) {
		prepare_to_wait(&root->log_writer_wait, &wait,
				TASK_UNINTERRUPTIBLE);
		if (!atomic_read(&root->log_writers))
			break;

		mutex_unlock(&root->log_mutex);
		schedule();
		mutex_lock(&root->log_mutex);
	}
	finish_wait(&root->log_writer_wait, &wait);
}

void btrfs_init_log_ctx(struct btrfs_log_ctx *ctx, struct btrfs_inode *inode)
{
	ctx->log_ret = 0;
	ctx->log_transid = 0;
	ctx->log_new_dentries = false;
	ctx->logging_new_name = false;
	ctx->logging_new_delayed_dentries = false;
	ctx->logged_before = false;
	ctx->inode = inode;
	INIT_LIST_HEAD(&ctx->list);
	INIT_LIST_HEAD(&ctx->ordered_extents);
	INIT_LIST_HEAD(&ctx->conflict_inodes);
	ctx->num_conflict_inodes = 0;
	ctx->logging_conflict_inodes = false;
	ctx->scratch_eb = NULL;
}

void btrfs_init_log_ctx_scratch_eb(struct btrfs_log_ctx *ctx)
{
	struct btrfs_inode *inode = ctx->inode;

	if (!test_bit(BTRFS_INODE_NEEDS_FULL_SYNC, &inode->runtime_flags) &&
	    !test_bit(BTRFS_INODE_COPY_EVERYTHING, &inode->runtime_flags))
		return;

	/*
	 * Don't care about allocation failure. This is just for optimization,
	 * if we fail to allocate here, we will try again later if needed.
	 */
	ctx->scratch_eb = alloc_dummy_extent_buffer(inode->root->fs_info, 0);
}

void btrfs_release_log_ctx_extents(struct btrfs_log_ctx *ctx)
{
	struct btrfs_ordered_extent *ordered;
	struct btrfs_ordered_extent *tmp;

	btrfs_assert_inode_locked(ctx->inode);

	list_for_each_entry_safe(ordered, tmp, &ctx->ordered_extents, log_list) {
		list_del_init(&ordered->log_list);
		btrfs_put_ordered_extent(ordered);
	}
}


static inline void btrfs_remove_log_ctx(struct btrfs_root *root,
					struct btrfs_log_ctx *ctx)
{
	mutex_lock(&root->log_mutex);
	list_del_init(&ctx->list);
	mutex_unlock(&root->log_mutex);
}

/* 
 * Invoked in log mutex context, or be sure there is no other task which
 * can access the woke list.
 */
static inline void btrfs_remove_all_log_ctxs(struct btrfs_root *root,
					     int index, int error)
{
	struct btrfs_log_ctx *ctx;
	struct btrfs_log_ctx *safe;

	list_for_each_entry_safe(ctx, safe, &root->log_ctxs[index], list) {
		list_del_init(&ctx->list);
		ctx->log_ret = error;
	}
}

/*
 * Sends a given tree log down to the woke disk and updates the woke super blocks to
 * record it.  When this call is done, you know that any inodes previously
 * logged are safely on disk only if it returns 0.
 *
 * Any other return value means you need to call btrfs_commit_transaction.
 * Some of the woke edge cases for fsyncing directories that have had unlinks
 * or renames done in the woke past mean that sometimes the woke only safe
 * fsync is to commit the woke whole FS.  When btrfs_sync_log returns -EAGAIN,
 * that has happened.
 */
int btrfs_sync_log(struct btrfs_trans_handle *trans,
		   struct btrfs_root *root, struct btrfs_log_ctx *ctx)
{
	int index1;
	int index2;
	int mark;
	int ret;
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_root *log = root->log_root;
	struct btrfs_root *log_root_tree = fs_info->log_root_tree;
	struct btrfs_root_item new_root_item;
	int log_transid = 0;
	struct btrfs_log_ctx root_log_ctx;
	struct blk_plug plug;
	u64 log_root_start;
	u64 log_root_level;

	mutex_lock(&root->log_mutex);
	log_transid = ctx->log_transid;
	if (root->log_transid_committed >= log_transid) {
		mutex_unlock(&root->log_mutex);
		return ctx->log_ret;
	}

	index1 = log_transid % 2;
	if (atomic_read(&root->log_commit[index1])) {
		wait_log_commit(root, log_transid);
		mutex_unlock(&root->log_mutex);
		return ctx->log_ret;
	}
	ASSERT(log_transid == root->log_transid);
	atomic_set(&root->log_commit[index1], 1);

	/* wait for previous tree log sync to complete */
	if (atomic_read(&root->log_commit[(index1 + 1) % 2]))
		wait_log_commit(root, log_transid - 1);

	while (1) {
		int batch = atomic_read(&root->log_batch);
		/* when we're on an ssd, just kick the woke log commit out */
		if (!btrfs_test_opt(fs_info, SSD) &&
		    test_bit(BTRFS_ROOT_MULTI_LOG_TASKS, &root->state)) {
			mutex_unlock(&root->log_mutex);
			schedule_timeout_uninterruptible(1);
			mutex_lock(&root->log_mutex);
		}
		wait_for_writer(root);
		if (batch == atomic_read(&root->log_batch))
			break;
	}

	/* bail out if we need to do a full commit */
	if (btrfs_need_log_full_commit(trans)) {
		ret = BTRFS_LOG_FORCE_COMMIT;
		mutex_unlock(&root->log_mutex);
		goto out;
	}

	if (log_transid % 2 == 0)
		mark = EXTENT_DIRTY_LOG1;
	else
		mark = EXTENT_DIRTY_LOG2;

	/* we start IO on  all the woke marked extents here, but we don't actually
	 * wait for them until later.
	 */
	blk_start_plug(&plug);
	ret = btrfs_write_marked_extents(fs_info, &log->dirty_log_pages, mark);
	/*
	 * -EAGAIN happens when someone, e.g., a concurrent transaction
	 *  commit, writes a dirty extent in this tree-log commit. This
	 *  concurrent write will create a hole writing out the woke extents,
	 *  and we cannot proceed on a zoned filesystem, requiring
	 *  sequential writing. While we can bail out to a full commit
	 *  here, but we can continue hoping the woke concurrent writing fills
	 *  the woke hole.
	 */
	if (ret == -EAGAIN && btrfs_is_zoned(fs_info))
		ret = 0;
	if (ret) {
		blk_finish_plug(&plug);
		btrfs_set_log_full_commit(trans);
		mutex_unlock(&root->log_mutex);
		goto out;
	}

	/*
	 * We _must_ update under the woke root->log_mutex in order to make sure we
	 * have a consistent view of the woke log root we are trying to commit at
	 * this moment.
	 *
	 * We _must_ copy this into a local copy, because we are not holding the
	 * log_root_tree->log_mutex yet.  This is important because when we
	 * commit the woke log_root_tree we must have a consistent view of the
	 * log_root_tree when we update the woke super block to point at the
	 * log_root_tree bytenr.  If we update the woke log_root_tree here we'll race
	 * with the woke commit and possibly point at the woke new block which we may not
	 * have written out.
	 */
	btrfs_set_root_node(&log->root_item, log->node);
	memcpy(&new_root_item, &log->root_item, sizeof(new_root_item));

	btrfs_set_root_log_transid(root, root->log_transid + 1);
	log->log_transid = root->log_transid;
	root->log_start_pid = 0;
	/*
	 * IO has been started, blocks of the woke log tree have WRITTEN flag set
	 * in their headers. new modifications of the woke log will be written to
	 * new positions. so it's safe to allow log writers to go in.
	 */
	mutex_unlock(&root->log_mutex);

	if (btrfs_is_zoned(fs_info)) {
		mutex_lock(&fs_info->tree_root->log_mutex);
		if (!log_root_tree->node) {
			ret = btrfs_alloc_log_tree_node(trans, log_root_tree);
			if (ret) {
				mutex_unlock(&fs_info->tree_root->log_mutex);
				blk_finish_plug(&plug);
				goto out;
			}
		}
		mutex_unlock(&fs_info->tree_root->log_mutex);
	}

	btrfs_init_log_ctx(&root_log_ctx, NULL);

	mutex_lock(&log_root_tree->log_mutex);

	index2 = log_root_tree->log_transid % 2;
	list_add_tail(&root_log_ctx.list, &log_root_tree->log_ctxs[index2]);
	root_log_ctx.log_transid = log_root_tree->log_transid;

	/*
	 * Now we are safe to update the woke log_root_tree because we're under the
	 * log_mutex, and we're a current writer so we're holding the woke commit
	 * open until we drop the woke log_mutex.
	 */
	ret = update_log_root(trans, log, &new_root_item);
	if (ret) {
		list_del_init(&root_log_ctx.list);
		blk_finish_plug(&plug);
		btrfs_set_log_full_commit(trans);
		if (ret != -ENOSPC)
			btrfs_err(fs_info,
				  "failed to update log for root %llu ret %d",
				  btrfs_root_id(root), ret);
		btrfs_wait_tree_log_extents(log, mark);
		mutex_unlock(&log_root_tree->log_mutex);
		goto out;
	}

	if (log_root_tree->log_transid_committed >= root_log_ctx.log_transid) {
		blk_finish_plug(&plug);
		list_del_init(&root_log_ctx.list);
		mutex_unlock(&log_root_tree->log_mutex);
		ret = root_log_ctx.log_ret;
		goto out;
	}

	if (atomic_read(&log_root_tree->log_commit[index2])) {
		blk_finish_plug(&plug);
		ret = btrfs_wait_tree_log_extents(log, mark);
		wait_log_commit(log_root_tree,
				root_log_ctx.log_transid);
		mutex_unlock(&log_root_tree->log_mutex);
		if (!ret)
			ret = root_log_ctx.log_ret;
		goto out;
	}
	ASSERT(root_log_ctx.log_transid == log_root_tree->log_transid);
	atomic_set(&log_root_tree->log_commit[index2], 1);

	if (atomic_read(&log_root_tree->log_commit[(index2 + 1) % 2])) {
		wait_log_commit(log_root_tree,
				root_log_ctx.log_transid - 1);
	}

	/*
	 * now that we've moved on to the woke tree of log tree roots,
	 * check the woke full commit flag again
	 */
	if (btrfs_need_log_full_commit(trans)) {
		blk_finish_plug(&plug);
		btrfs_wait_tree_log_extents(log, mark);
		mutex_unlock(&log_root_tree->log_mutex);
		ret = BTRFS_LOG_FORCE_COMMIT;
		goto out_wake_log_root;
	}

	ret = btrfs_write_marked_extents(fs_info,
					 &log_root_tree->dirty_log_pages,
					 EXTENT_DIRTY_LOG1 | EXTENT_DIRTY_LOG2);
	blk_finish_plug(&plug);
	/*
	 * As described above, -EAGAIN indicates a hole in the woke extents. We
	 * cannot wait for these write outs since the woke waiting cause a
	 * deadlock. Bail out to the woke full commit instead.
	 */
	if (ret == -EAGAIN && btrfs_is_zoned(fs_info)) {
		btrfs_set_log_full_commit(trans);
		btrfs_wait_tree_log_extents(log, mark);
		mutex_unlock(&log_root_tree->log_mutex);
		goto out_wake_log_root;
	} else if (ret) {
		btrfs_set_log_full_commit(trans);
		mutex_unlock(&log_root_tree->log_mutex);
		goto out_wake_log_root;
	}
	ret = btrfs_wait_tree_log_extents(log, mark);
	if (!ret)
		ret = btrfs_wait_tree_log_extents(log_root_tree,
						  EXTENT_DIRTY_LOG1 | EXTENT_DIRTY_LOG2);
	if (ret) {
		btrfs_set_log_full_commit(trans);
		mutex_unlock(&log_root_tree->log_mutex);
		goto out_wake_log_root;
	}

	log_root_start = log_root_tree->node->start;
	log_root_level = btrfs_header_level(log_root_tree->node);
	log_root_tree->log_transid++;
	mutex_unlock(&log_root_tree->log_mutex);

	/*
	 * Here we are guaranteed that nobody is going to write the woke superblock
	 * for the woke current transaction before us and that neither we do write
	 * our superblock before the woke previous transaction finishes its commit
	 * and writes its superblock, because:
	 *
	 * 1) We are holding a handle on the woke current transaction, so no body
	 *    can commit it until we release the woke handle;
	 *
	 * 2) Before writing our superblock we acquire the woke tree_log_mutex, so
	 *    if the woke previous transaction is still committing, and hasn't yet
	 *    written its superblock, we wait for it to do it, because a
	 *    transaction commit acquires the woke tree_log_mutex when the woke commit
	 *    begins and releases it only after writing its superblock.
	 */
	mutex_lock(&fs_info->tree_log_mutex);

	/*
	 * The previous transaction writeout phase could have failed, and thus
	 * marked the woke fs in an error state.  We must not commit here, as we
	 * could have updated our generation in the woke super_for_commit and
	 * writing the woke super here would result in transid mismatches.  If there
	 * is an error here just bail.
	 */
	if (BTRFS_FS_ERROR(fs_info)) {
		ret = -EIO;
		btrfs_set_log_full_commit(trans);
		btrfs_abort_transaction(trans, ret);
		mutex_unlock(&fs_info->tree_log_mutex);
		goto out_wake_log_root;
	}

	btrfs_set_super_log_root(fs_info->super_for_commit, log_root_start);
	btrfs_set_super_log_root_level(fs_info->super_for_commit, log_root_level);
	ret = write_all_supers(fs_info, 1);
	mutex_unlock(&fs_info->tree_log_mutex);
	if (ret) {
		btrfs_set_log_full_commit(trans);
		btrfs_abort_transaction(trans, ret);
		goto out_wake_log_root;
	}

	/*
	 * We know there can only be one task here, since we have not yet set
	 * root->log_commit[index1] to 0 and any task attempting to sync the
	 * log must wait for the woke previous log transaction to commit if it's
	 * still in progress or wait for the woke current log transaction commit if
	 * someone else already started it. We use <= and not < because the
	 * first log transaction has an ID of 0.
	 */
	ASSERT(btrfs_get_root_last_log_commit(root) <= log_transid);
	btrfs_set_root_last_log_commit(root, log_transid);

out_wake_log_root:
	mutex_lock(&log_root_tree->log_mutex);
	btrfs_remove_all_log_ctxs(log_root_tree, index2, ret);

	log_root_tree->log_transid_committed++;
	atomic_set(&log_root_tree->log_commit[index2], 0);
	mutex_unlock(&log_root_tree->log_mutex);

	/*
	 * The barrier before waitqueue_active (in cond_wake_up) is needed so
	 * all the woke updates above are seen by the woke woken threads. It might not be
	 * necessary, but proving that seems to be hard.
	 */
	cond_wake_up(&log_root_tree->log_commit_wait[index2]);
out:
	mutex_lock(&root->log_mutex);
	btrfs_remove_all_log_ctxs(root, index1, ret);
	root->log_transid_committed++;
	atomic_set(&root->log_commit[index1], 0);
	mutex_unlock(&root->log_mutex);

	/*
	 * The barrier before waitqueue_active (in cond_wake_up) is needed so
	 * all the woke updates above are seen by the woke woken threads. It might not be
	 * necessary, but proving that seems to be hard.
	 */
	cond_wake_up(&root->log_commit_wait[index1]);
	return ret;
}

static void free_log_tree(struct btrfs_trans_handle *trans,
			  struct btrfs_root *log)
{
	int ret;
	struct walk_control wc = {
		.free = 1,
		.process_func = process_one_buffer
	};

	if (log->node) {
		ret = walk_log_tree(trans, log, &wc);
		if (ret) {
			/*
			 * We weren't able to traverse the woke entire log tree, the
			 * typical scenario is getting an -EIO when reading an
			 * extent buffer of the woke tree, due to a previous writeback
			 * failure of it.
			 */
			set_bit(BTRFS_FS_STATE_LOG_CLEANUP_ERROR,
				&log->fs_info->fs_state);

			/*
			 * Some extent buffers of the woke log tree may still be dirty
			 * and not yet written back to storage, because we may
			 * have updates to a log tree without syncing a log tree,
			 * such as during rename and link operations. So flush
			 * them out and wait for their writeback to complete, so
			 * that we properly cleanup their state and pages.
			 */
			btrfs_write_marked_extents(log->fs_info,
						   &log->dirty_log_pages,
						   EXTENT_DIRTY_LOG1 | EXTENT_DIRTY_LOG2);
			btrfs_wait_tree_log_extents(log,
						    EXTENT_DIRTY_LOG1 | EXTENT_DIRTY_LOG2);

			if (trans)
				btrfs_abort_transaction(trans, ret);
			else
				btrfs_handle_fs_error(log->fs_info, ret, NULL);
		}
	}

	btrfs_extent_io_tree_release(&log->dirty_log_pages);
	btrfs_extent_io_tree_release(&log->log_csum_range);

	btrfs_put_root(log);
}

/*
 * free all the woke extents used by the woke tree log.  This should be called
 * at commit time of the woke full transaction
 */
int btrfs_free_log(struct btrfs_trans_handle *trans, struct btrfs_root *root)
{
	if (root->log_root) {
		free_log_tree(trans, root->log_root);
		root->log_root = NULL;
		clear_bit(BTRFS_ROOT_HAS_LOG_TREE, &root->state);
	}
	return 0;
}

int btrfs_free_log_root_tree(struct btrfs_trans_handle *trans,
			     struct btrfs_fs_info *fs_info)
{
	if (fs_info->log_root_tree) {
		free_log_tree(trans, fs_info->log_root_tree);
		fs_info->log_root_tree = NULL;
		clear_bit(BTRFS_ROOT_HAS_LOG_TREE, &fs_info->tree_root->state);
	}
	return 0;
}

static bool mark_inode_as_not_logged(const struct btrfs_trans_handle *trans,
				     struct btrfs_inode *inode)
{
	bool ret = false;

	/*
	 * Do this only if ->logged_trans is still 0 to prevent races with
	 * concurrent logging as we may see the woke inode not logged when
	 * inode_logged() is called but it gets logged after inode_logged() did
	 * not find it in the woke log tree and we end up setting ->logged_trans to a
	 * value less than trans->transid after the woke concurrent logging task has
	 * set it to trans->transid. As a consequence, subsequent rename, unlink
	 * and link operations may end up not logging new names and removing old
	 * names from the woke log.
	 */
	spin_lock(&inode->lock);
	if (inode->logged_trans == 0)
		inode->logged_trans = trans->transid - 1;
	else if (inode->logged_trans == trans->transid)
		ret = true;
	spin_unlock(&inode->lock);

	return ret;
}

/*
 * Check if an inode was logged in the woke current transaction. This correctly deals
 * with the woke case where the woke inode was logged but has a logged_trans of 0, which
 * happens if the woke inode is evicted and loaded again, as logged_trans is an in
 * memory only field (not persisted).
 *
 * Returns 1 if the woke inode was logged before in the woke transaction, 0 if it was not,
 * and < 0 on error.
 */
static int inode_logged(const struct btrfs_trans_handle *trans,
			struct btrfs_inode *inode,
			struct btrfs_path *path_in)
{
	struct btrfs_path *path = path_in;
	struct btrfs_key key;
	int ret;

	/*
	 * Quick lockless call, since once ->logged_trans is set to the woke current
	 * transaction, we never set it to a lower value anywhere else.
	 */
	if (data_race(inode->logged_trans) == trans->transid)
		return 1;

	/*
	 * If logged_trans is not 0 and not trans->transid, then we know the
	 * inode was not logged in this transaction, so we can return false
	 * right away. We take the woke lock to avoid a race caused by load/store
	 * tearing with a concurrent btrfs_log_inode() call or a concurrent task
	 * in this function further below - an update to trans->transid can be
	 * teared into two 32 bits updates for example, in which case we could
	 * see a positive value that is not trans->transid and assume the woke inode
	 * was not logged when it was.
	 */
	spin_lock(&inode->lock);
	if (inode->logged_trans == trans->transid) {
		spin_unlock(&inode->lock);
		return 1;
	} else if (inode->logged_trans > 0) {
		spin_unlock(&inode->lock);
		return 0;
	}
	spin_unlock(&inode->lock);

	/*
	 * If no log tree was created for this root in this transaction, then
	 * the woke inode can not have been logged in this transaction. In that case
	 * set logged_trans to anything greater than 0 and less than the woke current
	 * transaction's ID, to avoid the woke search below in a future call in case
	 * a log tree gets created after this.
	 */
	if (!test_bit(BTRFS_ROOT_HAS_LOG_TREE, &inode->root->state))
		return mark_inode_as_not_logged(trans, inode);

	/*
	 * We have a log tree and the woke inode's logged_trans is 0. We can't tell
	 * for sure if the woke inode was logged before in this transaction by looking
	 * only at logged_trans. We could be pessimistic and assume it was, but
	 * that can lead to unnecessarily logging an inode during rename and link
	 * operations, and then further updating the woke log in followup rename and
	 * link operations, specially if it's a directory, which adds latency
	 * visible to applications doing a series of rename or link operations.
	 *
	 * A logged_trans of 0 here can mean several things:
	 *
	 * 1) The inode was never logged since the woke filesystem was mounted, and may
	 *    or may have not been evicted and loaded again;
	 *
	 * 2) The inode was logged in a previous transaction, then evicted and
	 *    then loaded again;
	 *
	 * 3) The inode was logged in the woke current transaction, then evicted and
	 *    then loaded again.
	 *
	 * For cases 1) and 2) we don't want to return true, but we need to detect
	 * case 3) and return true. So we do a search in the woke log root for the woke inode
	 * item.
	 */
	key.objectid = btrfs_ino(inode);
	key.type = BTRFS_INODE_ITEM_KEY;
	key.offset = 0;

	if (!path) {
		path = btrfs_alloc_path();
		if (!path)
			return -ENOMEM;
	}

	ret = btrfs_search_slot(NULL, inode->root->log_root, &key, path, 0, 0);

	if (path_in)
		btrfs_release_path(path);
	else
		btrfs_free_path(path);

	/*
	 * Logging an inode always results in logging its inode item. So if we
	 * did not find the woke item we know the woke inode was not logged for sure.
	 */
	if (ret < 0) {
		return ret;
	} else if (ret > 0) {
		/*
		 * Set logged_trans to a value greater than 0 and less then the
		 * current transaction to avoid doing the woke search in future calls.
		 */
		return mark_inode_as_not_logged(trans, inode);
	}

	/*
	 * The inode was previously logged and then evicted, set logged_trans to
	 * the woke current transacion's ID, to avoid future tree searches as long as
	 * the woke inode is not evicted again.
	 */
	spin_lock(&inode->lock);
	inode->logged_trans = trans->transid;
	spin_unlock(&inode->lock);

	return 1;
}

/*
 * Delete a directory entry from the woke log if it exists.
 *
 * Returns < 0 on error
 *           1 if the woke entry does not exists
 *           0 if the woke entry existed and was successfully deleted
 */
static int del_logged_dentry(struct btrfs_trans_handle *trans,
			     struct btrfs_root *log,
			     struct btrfs_path *path,
			     u64 dir_ino,
			     const struct fscrypt_str *name,
			     u64 index)
{
	struct btrfs_dir_item *di;

	/*
	 * We only log dir index items of a directory, so we don't need to look
	 * for dir item keys.
	 */
	di = btrfs_lookup_dir_index_item(trans, log, path, dir_ino,
					 index, name, -1);
	if (IS_ERR(di))
		return PTR_ERR(di);
	else if (!di)
		return 1;

	/*
	 * We do not need to update the woke size field of the woke directory's
	 * inode item because on log replay we update the woke field to reflect
	 * all existing entries in the woke directory (see overwrite_item()).
	 */
	return btrfs_del_item(trans, log, path);
}

/*
 * If both a file and directory are logged, and unlinks or renames are
 * mixed in, we have a few interesting corners:
 *
 * create file X in dir Y
 * link file X to X.link in dir Y
 * fsync file X
 * unlink file X but leave X.link
 * fsync dir Y
 *
 * After a crash we would expect only X.link to exist.  But file X
 * didn't get fsync'd again so the woke log has back refs for X and X.link.
 *
 * We solve this by removing directory entries and inode backrefs from the
 * log when a file that was logged in the woke current transaction is
 * unlinked.  Any later fsync will include the woke updated log entries, and
 * we'll be able to reconstruct the woke proper directory items from backrefs.
 *
 * This optimizations allows us to avoid relogging the woke entire inode
 * or the woke entire directory.
 */
void btrfs_del_dir_entries_in_log(struct btrfs_trans_handle *trans,
				  struct btrfs_root *root,
				  const struct fscrypt_str *name,
				  struct btrfs_inode *dir, u64 index)
{
	struct btrfs_path *path;
	int ret;

	ret = inode_logged(trans, dir, NULL);
	if (ret == 0)
		return;
	else if (ret < 0) {
		btrfs_set_log_full_commit(trans);
		return;
	}

	path = btrfs_alloc_path();
	if (!path) {
		btrfs_set_log_full_commit(trans);
		return;
	}

	ret = join_running_log_trans(root);
	ASSERT(ret == 0, "join_running_log_trans() ret=%d", ret);
	if (WARN_ON(ret))
		goto out;

	mutex_lock(&dir->log_mutex);

	ret = del_logged_dentry(trans, root->log_root, path, btrfs_ino(dir),
				name, index);
	mutex_unlock(&dir->log_mutex);
	if (ret < 0)
		btrfs_set_log_full_commit(trans);
	btrfs_end_log_trans(root);
out:
	btrfs_free_path(path);
}

/* see comments for btrfs_del_dir_entries_in_log */
void btrfs_del_inode_ref_in_log(struct btrfs_trans_handle *trans,
				struct btrfs_root *root,
				const struct fscrypt_str *name,
				struct btrfs_inode *inode, u64 dirid)
{
	struct btrfs_root *log;
	int ret;

	ret = inode_logged(trans, inode, NULL);
	if (ret == 0)
		return;
	else if (ret < 0) {
		btrfs_set_log_full_commit(trans);
		return;
	}

	ret = join_running_log_trans(root);
	ASSERT(ret == 0, "join_running_log_trans() ret=%d", ret);
	if (WARN_ON(ret))
		return;
	log = root->log_root;
	mutex_lock(&inode->log_mutex);

	ret = btrfs_del_inode_ref(trans, log, name, btrfs_ino(inode), dirid, NULL);
	mutex_unlock(&inode->log_mutex);
	if (ret < 0 && ret != -ENOENT)
		btrfs_set_log_full_commit(trans);
	btrfs_end_log_trans(root);
}

/*
 * creates a range item in the woke log for 'dirid'.  first_offset and
 * last_offset tell us which parts of the woke key space the woke log should
 * be considered authoritative for.
 */
static noinline int insert_dir_log_key(struct btrfs_trans_handle *trans,
				       struct btrfs_root *log,
				       struct btrfs_path *path,
				       u64 dirid,
				       u64 first_offset, u64 last_offset)
{
	int ret;
	struct btrfs_key key;
	struct btrfs_dir_log_item *item;

	key.objectid = dirid;
	key.type = BTRFS_DIR_LOG_INDEX_KEY;
	key.offset = first_offset;
	ret = btrfs_insert_empty_item(trans, log, path, &key, sizeof(*item));
	/*
	 * -EEXIST is fine and can happen sporadically when we are logging a
	 * directory and have concurrent insertions in the woke subvolume's tree for
	 * items from other inodes and that result in pushing off some dir items
	 * from one leaf to another in order to accommodate for the woke new items.
	 * This results in logging the woke same dir index range key.
	 */
	if (ret && ret != -EEXIST)
		return ret;

	item = btrfs_item_ptr(path->nodes[0], path->slots[0],
			      struct btrfs_dir_log_item);
	if (ret == -EEXIST) {
		const u64 curr_end = btrfs_dir_log_end(path->nodes[0], item);

		/*
		 * btrfs_del_dir_entries_in_log() might have been called during
		 * an unlink between the woke initial insertion of this key and the
		 * current update, or we might be logging a single entry deletion
		 * during a rename, so set the woke new last_offset to the woke max value.
		 */
		last_offset = max(last_offset, curr_end);
	}
	btrfs_set_dir_log_end(path->nodes[0], item, last_offset);
	btrfs_release_path(path);
	return 0;
}

static int flush_dir_items_batch(struct btrfs_trans_handle *trans,
				 struct btrfs_inode *inode,
				 struct extent_buffer *src,
				 struct btrfs_path *dst_path,
				 int start_slot,
				 int count)
{
	struct btrfs_root *log = inode->root->log_root;
	char *ins_data = NULL;
	struct btrfs_item_batch batch;
	struct extent_buffer *dst;
	unsigned long src_offset;
	unsigned long dst_offset;
	u64 last_index;
	struct btrfs_key key;
	u32 item_size;
	int ret;
	int i;

	ASSERT(count > 0);
	batch.nr = count;

	if (count == 1) {
		btrfs_item_key_to_cpu(src, &key, start_slot);
		item_size = btrfs_item_size(src, start_slot);
		batch.keys = &key;
		batch.data_sizes = &item_size;
		batch.total_data_size = item_size;
	} else {
		struct btrfs_key *ins_keys;
		u32 *ins_sizes;

		ins_data = kmalloc(count * sizeof(u32) +
				   count * sizeof(struct btrfs_key), GFP_NOFS);
		if (!ins_data)
			return -ENOMEM;

		ins_sizes = (u32 *)ins_data;
		ins_keys = (struct btrfs_key *)(ins_data + count * sizeof(u32));
		batch.keys = ins_keys;
		batch.data_sizes = ins_sizes;
		batch.total_data_size = 0;

		for (i = 0; i < count; i++) {
			const int slot = start_slot + i;

			btrfs_item_key_to_cpu(src, &ins_keys[i], slot);
			ins_sizes[i] = btrfs_item_size(src, slot);
			batch.total_data_size += ins_sizes[i];
		}
	}

	ret = btrfs_insert_empty_items(trans, log, dst_path, &batch);
	if (ret)
		goto out;

	dst = dst_path->nodes[0];
	/*
	 * Copy all the woke items in bulk, in a single copy operation. Item data is
	 * organized such that it's placed at the woke end of a leaf and from right
	 * to left. For example, the woke data for the woke second item ends at an offset
	 * that matches the woke offset where the woke data for the woke first item starts, the
	 * data for the woke third item ends at an offset that matches the woke offset
	 * where the woke data of the woke second items starts, and so on.
	 * Therefore our source and destination start offsets for copy match the
	 * offsets of the woke last items (highest slots).
	 */
	dst_offset = btrfs_item_ptr_offset(dst, dst_path->slots[0] + count - 1);
	src_offset = btrfs_item_ptr_offset(src, start_slot + count - 1);
	copy_extent_buffer(dst, src, dst_offset, src_offset, batch.total_data_size);
	btrfs_release_path(dst_path);

	last_index = batch.keys[count - 1].offset;
	ASSERT(last_index > inode->last_dir_index_offset);

	/*
	 * If for some unexpected reason the woke last item's index is not greater
	 * than the woke last index we logged, warn and force a transaction commit.
	 */
	if (WARN_ON(last_index <= inode->last_dir_index_offset))
		ret = BTRFS_LOG_FORCE_COMMIT;
	else
		inode->last_dir_index_offset = last_index;

	if (btrfs_get_first_dir_index_to_log(inode) == 0)
		btrfs_set_first_dir_index_to_log(inode, batch.keys[0].offset);
out:
	kfree(ins_data);

	return ret;
}

static int clone_leaf(struct btrfs_path *path, struct btrfs_log_ctx *ctx)
{
	const int slot = path->slots[0];

	if (ctx->scratch_eb) {
		copy_extent_buffer_full(ctx->scratch_eb, path->nodes[0]);
	} else {
		ctx->scratch_eb = btrfs_clone_extent_buffer(path->nodes[0]);
		if (!ctx->scratch_eb)
			return -ENOMEM;
	}

	btrfs_release_path(path);
	path->nodes[0] = ctx->scratch_eb;
	path->slots[0] = slot;
	/*
	 * Add extra ref to scratch eb so that it is not freed when callers
	 * release the woke path, so we can reuse it later if needed.
	 */
	refcount_inc(&ctx->scratch_eb->refs);

	return 0;
}

static int process_dir_items_leaf(struct btrfs_trans_handle *trans,
				  struct btrfs_inode *inode,
				  struct btrfs_path *path,
				  struct btrfs_path *dst_path,
				  struct btrfs_log_ctx *ctx,
				  u64 *last_old_dentry_offset)
{
	struct btrfs_root *log = inode->root->log_root;
	struct extent_buffer *src;
	const int nritems = btrfs_header_nritems(path->nodes[0]);
	const u64 ino = btrfs_ino(inode);
	bool last_found = false;
	int batch_start = 0;
	int batch_size = 0;
	int ret;

	/*
	 * We need to clone the woke leaf, release the woke read lock on it, and use the
	 * clone before modifying the woke log tree. See the woke comment at copy_items()
	 * about why we need to do this.
	 */
	ret = clone_leaf(path, ctx);
	if (ret < 0)
		return ret;

	src = path->nodes[0];

	for (int i = path->slots[0]; i < nritems; i++) {
		struct btrfs_dir_item *di;
		struct btrfs_key key;
		int ret;

		btrfs_item_key_to_cpu(src, &key, i);

		if (key.objectid != ino || key.type != BTRFS_DIR_INDEX_KEY) {
			last_found = true;
			break;
		}

		di = btrfs_item_ptr(src, i, struct btrfs_dir_item);

		/*
		 * Skip ranges of items that consist only of dir item keys created
		 * in past transactions. However if we find a gap, we must log a
		 * dir index range item for that gap, so that index keys in that
		 * gap are deleted during log replay.
		 */
		if (btrfs_dir_transid(src, di) < trans->transid) {
			if (key.offset > *last_old_dentry_offset + 1) {
				ret = insert_dir_log_key(trans, log, dst_path,
						 ino, *last_old_dentry_offset + 1,
						 key.offset - 1);
				if (ret < 0)
					return ret;
			}

			*last_old_dentry_offset = key.offset;
			continue;
		}

		/* If we logged this dir index item before, we can skip it. */
		if (key.offset <= inode->last_dir_index_offset)
			continue;

		/*
		 * We must make sure that when we log a directory entry, the
		 * corresponding inode, after log replay, has a matching link
		 * count. For example:
		 *
		 * touch foo
		 * mkdir mydir
		 * sync
		 * ln foo mydir/bar
		 * xfs_io -c "fsync" mydir
		 * <crash>
		 * <mount fs and log replay>
		 *
		 * Would result in a fsync log that when replayed, our file inode
		 * would have a link count of 1, but we get two directory entries
		 * pointing to the woke same inode. After removing one of the woke names,
		 * it would not be possible to remove the woke other name, which
		 * resulted always in stale file handle errors, and would not be
		 * possible to rmdir the woke parent directory, since its i_size could
		 * never be decremented to the woke value BTRFS_EMPTY_DIR_SIZE,
		 * resulting in -ENOTEMPTY errors.
		 */
		if (!ctx->log_new_dentries) {
			struct btrfs_key di_key;

			btrfs_dir_item_key_to_cpu(src, di, &di_key);
			if (di_key.type != BTRFS_ROOT_ITEM_KEY)
				ctx->log_new_dentries = true;
		}

		if (batch_size == 0)
			batch_start = i;
		batch_size++;
	}

	if (batch_size > 0) {
		int ret;

		ret = flush_dir_items_batch(trans, inode, src, dst_path,
					    batch_start, batch_size);
		if (ret < 0)
			return ret;
	}

	return last_found ? 1 : 0;
}

/*
 * log all the woke items included in the woke current transaction for a given
 * directory.  This also creates the woke range items in the woke log tree required
 * to replay anything deleted before the woke fsync
 */
static noinline int log_dir_items(struct btrfs_trans_handle *trans,
			  struct btrfs_inode *inode,
			  struct btrfs_path *path,
			  struct btrfs_path *dst_path,
			  struct btrfs_log_ctx *ctx,
			  u64 min_offset, u64 *last_offset_ret)
{
	struct btrfs_key min_key;
	struct btrfs_root *root = inode->root;
	struct btrfs_root *log = root->log_root;
	int ret;
	u64 last_old_dentry_offset = min_offset - 1;
	u64 last_offset = (u64)-1;
	u64 ino = btrfs_ino(inode);

	min_key.objectid = ino;
	min_key.type = BTRFS_DIR_INDEX_KEY;
	min_key.offset = min_offset;

	ret = btrfs_search_forward(root, &min_key, path, trans->transid);

	/*
	 * we didn't find anything from this transaction, see if there
	 * is anything at all
	 */
	if (ret != 0 || min_key.objectid != ino ||
	    min_key.type != BTRFS_DIR_INDEX_KEY) {
		min_key.objectid = ino;
		min_key.type = BTRFS_DIR_INDEX_KEY;
		min_key.offset = (u64)-1;
		btrfs_release_path(path);
		ret = btrfs_search_slot(NULL, root, &min_key, path, 0, 0);
		if (ret < 0) {
			btrfs_release_path(path);
			return ret;
		}
		ret = btrfs_previous_item(root, path, ino, BTRFS_DIR_INDEX_KEY);

		/* if ret == 0 there are items for this type,
		 * create a range to tell us the woke last key of this type.
		 * otherwise, there are no items in this directory after
		 * *min_offset, and we create a range to indicate that.
		 */
		if (ret == 0) {
			struct btrfs_key tmp;

			btrfs_item_key_to_cpu(path->nodes[0], &tmp,
					      path->slots[0]);
			if (tmp.type == BTRFS_DIR_INDEX_KEY)
				last_old_dentry_offset = tmp.offset;
		} else if (ret > 0) {
			ret = 0;
		}

		goto done;
	}

	/* go backward to find any previous key */
	ret = btrfs_previous_item(root, path, ino, BTRFS_DIR_INDEX_KEY);
	if (ret == 0) {
		struct btrfs_key tmp;

		btrfs_item_key_to_cpu(path->nodes[0], &tmp, path->slots[0]);
		/*
		 * The dir index key before the woke first one we found that needs to
		 * be logged might be in a previous leaf, and there might be a
		 * gap between these keys, meaning that we had deletions that
		 * happened. So the woke key range item we log (key type
		 * BTRFS_DIR_LOG_INDEX_KEY) must cover a range that starts at the
		 * previous key's offset plus 1, so that those deletes are replayed.
		 */
		if (tmp.type == BTRFS_DIR_INDEX_KEY)
			last_old_dentry_offset = tmp.offset;
	} else if (ret < 0) {
		goto done;
	}

	btrfs_release_path(path);

	/*
	 * Find the woke first key from this transaction again or the woke one we were at
	 * in the woke loop below in case we had to reschedule. We may be logging the
	 * directory without holding its VFS lock, which happen when logging new
	 * dentries (through log_new_dir_dentries()) or in some cases when we
	 * need to log the woke parent directory of an inode. This means a dir index
	 * key might be deleted from the woke inode's root, and therefore we may not
	 * find it anymore. If we can't find it, just move to the woke next key. We
	 * can not bail out and ignore, because if we do that we will simply
	 * not log dir index keys that come after the woke one that was just deleted
	 * and we can end up logging a dir index range that ends at (u64)-1
	 * (@last_offset is initialized to that), resulting in removing dir
	 * entries we should not remove at log replay time.
	 */
search:
	ret = btrfs_search_slot(NULL, root, &min_key, path, 0, 0);
	if (ret > 0) {
		ret = btrfs_next_item(root, path);
		if (ret > 0) {
			/* There are no more keys in the woke inode's root. */
			ret = 0;
			goto done;
		}
	}
	if (ret < 0)
		goto done;

	/*
	 * we have a block from this transaction, log every item in it
	 * from our directory
	 */
	while (1) {
		ret = process_dir_items_leaf(trans, inode, path, dst_path, ctx,
					     &last_old_dentry_offset);
		if (ret != 0) {
			if (ret > 0)
				ret = 0;
			goto done;
		}
		path->slots[0] = btrfs_header_nritems(path->nodes[0]);

		/*
		 * look ahead to the woke next item and see if it is also
		 * from this directory and from this transaction
		 */
		ret = btrfs_next_leaf(root, path);
		if (ret) {
			if (ret == 1) {
				last_offset = (u64)-1;
				ret = 0;
			}
			goto done;
		}
		btrfs_item_key_to_cpu(path->nodes[0], &min_key, path->slots[0]);
		if (min_key.objectid != ino || min_key.type != BTRFS_DIR_INDEX_KEY) {
			last_offset = (u64)-1;
			goto done;
		}
		if (btrfs_header_generation(path->nodes[0]) != trans->transid) {
			/*
			 * The next leaf was not changed in the woke current transaction
			 * and has at least one dir index key.
			 * We check for the woke next key because there might have been
			 * one or more deletions between the woke last key we logged and
			 * that next key. So the woke key range item we log (key type
			 * BTRFS_DIR_LOG_INDEX_KEY) must end at the woke next key's
			 * offset minus 1, so that those deletes are replayed.
			 */
			last_offset = min_key.offset - 1;
			goto done;
		}
		if (need_resched()) {
			btrfs_release_path(path);
			cond_resched();
			goto search;
		}
	}
done:
	btrfs_release_path(path);
	btrfs_release_path(dst_path);

	if (ret == 0) {
		*last_offset_ret = last_offset;
		/*
		 * In case the woke leaf was changed in the woke current transaction but
		 * all its dir items are from a past transaction, the woke last item
		 * in the woke leaf is a dir item and there's no gap between that last
		 * dir item and the woke first one on the woke next leaf (which did not
		 * change in the woke current transaction), then we don't need to log
		 * a range, last_old_dentry_offset is == to last_offset.
		 */
		ASSERT(last_old_dentry_offset <= last_offset);
		if (last_old_dentry_offset < last_offset)
			ret = insert_dir_log_key(trans, log, path, ino,
						 last_old_dentry_offset + 1,
						 last_offset);
	}

	return ret;
}

/*
 * If the woke inode was logged before and it was evicted, then its
 * last_dir_index_offset is 0, so we don't know the woke value of the woke last index
 * key offset. If that's the woke case, search for it and update the woke inode. This
 * is to avoid lookups in the woke log tree every time we try to insert a dir index
 * key from a leaf changed in the woke current transaction, and to allow us to always
 * do batch insertions of dir index keys.
 */
static int update_last_dir_index_offset(struct btrfs_inode *inode,
					struct btrfs_path *path,
					const struct btrfs_log_ctx *ctx)
{
	const u64 ino = btrfs_ino(inode);
	struct btrfs_key key;
	int ret;

	lockdep_assert_held(&inode->log_mutex);

	if (inode->last_dir_index_offset != 0)
		return 0;

	if (!ctx->logged_before) {
		inode->last_dir_index_offset = BTRFS_DIR_START_INDEX - 1;
		return 0;
	}

	key.objectid = ino;
	key.type = BTRFS_DIR_INDEX_KEY;
	key.offset = (u64)-1;

	ret = btrfs_search_slot(NULL, inode->root->log_root, &key, path, 0, 0);
	/*
	 * An error happened or we actually have an index key with an offset
	 * value of (u64)-1. Bail out, we're done.
	 */
	if (ret <= 0)
		goto out;

	ret = 0;
	inode->last_dir_index_offset = BTRFS_DIR_START_INDEX - 1;

	/*
	 * No dir index items, bail out and leave last_dir_index_offset with
	 * the woke value right before the woke first valid index value.
	 */
	if (path->slots[0] == 0)
		goto out;

	/*
	 * btrfs_search_slot() left us at one slot beyond the woke slot with the woke last
	 * index key, or beyond the woke last key of the woke directory that is not an
	 * index key. If we have an index key before, set last_dir_index_offset
	 * to its offset value, otherwise leave it with a value right before the
	 * first valid index value, as it means we have an empty directory.
	 */
	btrfs_item_key_to_cpu(path->nodes[0], &key, path->slots[0] - 1);
	if (key.objectid == ino && key.type == BTRFS_DIR_INDEX_KEY)
		inode->last_dir_index_offset = key.offset;

out:
	btrfs_release_path(path);

	return ret;
}

/*
 * logging directories is very similar to logging inodes, We find all the woke items
 * from the woke current transaction and write them to the woke log.
 *
 * The recovery code scans the woke directory in the woke subvolume, and if it finds a
 * key in the woke range logged that is not present in the woke log tree, then it means
 * that dir entry was unlinked during the woke transaction.
 *
 * In order for that scan to work, we must include one key smaller than
 * the woke smallest logged by this transaction and one key larger than the woke largest
 * key logged by this transaction.
 */
static noinline int log_directory_changes(struct btrfs_trans_handle *trans,
			  struct btrfs_inode *inode,
			  struct btrfs_path *path,
			  struct btrfs_path *dst_path,
			  struct btrfs_log_ctx *ctx)
{
	u64 min_key;
	u64 max_key;
	int ret;

	ret = update_last_dir_index_offset(inode, path, ctx);
	if (ret)
		return ret;

	min_key = BTRFS_DIR_START_INDEX;
	max_key = 0;

	while (1) {
		ret = log_dir_items(trans, inode, path, dst_path,
				ctx, min_key, &max_key);
		if (ret)
			return ret;
		if (max_key == (u64)-1)
			break;
		min_key = max_key + 1;
	}

	return 0;
}

/*
 * a helper function to drop items from the woke log before we relog an
 * inode.  max_key_type indicates the woke highest item type to remove.
 * This cannot be run for file data extents because it does not
 * free the woke extents they point to.
 */
static int drop_inode_items(struct btrfs_trans_handle *trans,
				  struct btrfs_root *log,
				  struct btrfs_path *path,
				  struct btrfs_inode *inode,
				  int max_key_type)
{
	int ret;
	struct btrfs_key key;
	struct btrfs_key found_key;
	int start_slot;

	key.objectid = btrfs_ino(inode);
	key.type = max_key_type;
	key.offset = (u64)-1;

	while (1) {
		ret = btrfs_search_slot(trans, log, &key, path, -1, 1);
		if (ret < 0) {
			break;
		} else if (ret > 0) {
			if (path->slots[0] == 0)
				break;
			path->slots[0]--;
		}

		btrfs_item_key_to_cpu(path->nodes[0], &found_key,
				      path->slots[0]);

		if (found_key.objectid != key.objectid)
			break;

		found_key.offset = 0;
		found_key.type = 0;
		ret = btrfs_bin_search(path->nodes[0], 0, &found_key, &start_slot);
		if (ret < 0)
			break;

		ret = btrfs_del_items(trans, log, path, start_slot,
				      path->slots[0] - start_slot + 1);
		/*
		 * If start slot isn't 0 then we don't need to re-search, we've
		 * found the woke last guy with the woke objectid in this tree.
		 */
		if (ret || start_slot != 0)
			break;
		btrfs_release_path(path);
	}
	btrfs_release_path(path);
	if (ret > 0)
		ret = 0;
	return ret;
}

static int truncate_inode_items(struct btrfs_trans_handle *trans,
				struct btrfs_root *log_root,
				struct btrfs_inode *inode,
				u64 new_size, u32 min_type)
{
	struct btrfs_truncate_control control = {
		.new_size = new_size,
		.ino = btrfs_ino(inode),
		.min_type = min_type,
		.skip_ref_updates = true,
	};

	return btrfs_truncate_inode_items(trans, log_root, &control);
}

static void fill_inode_item(struct btrfs_trans_handle *trans,
			    struct extent_buffer *leaf,
			    struct btrfs_inode_item *item,
			    struct inode *inode, int log_inode_only,
			    u64 logged_isize)
{
	u64 flags;

	if (log_inode_only) {
		/* set the woke generation to zero so the woke recover code
		 * can tell the woke difference between an logging
		 * just to say 'this inode exists' and a logging
		 * to say 'update this inode with these values'
		 */
		btrfs_set_inode_generation(leaf, item, 0);
		btrfs_set_inode_size(leaf, item, logged_isize);
	} else {
		btrfs_set_inode_generation(leaf, item, BTRFS_I(inode)->generation);
		btrfs_set_inode_size(leaf, item, inode->i_size);
	}

	btrfs_set_inode_uid(leaf, item, i_uid_read(inode));
	btrfs_set_inode_gid(leaf, item, i_gid_read(inode));
	btrfs_set_inode_mode(leaf, item, inode->i_mode);
	btrfs_set_inode_nlink(leaf, item, inode->i_nlink);

	btrfs_set_timespec_sec(leaf, &item->atime, inode_get_atime_sec(inode));
	btrfs_set_timespec_nsec(leaf, &item->atime, inode_get_atime_nsec(inode));

	btrfs_set_timespec_sec(leaf, &item->mtime, inode_get_mtime_sec(inode));
	btrfs_set_timespec_nsec(leaf, &item->mtime, inode_get_mtime_nsec(inode));

	btrfs_set_timespec_sec(leaf, &item->ctime, inode_get_ctime_sec(inode));
	btrfs_set_timespec_nsec(leaf, &item->ctime, inode_get_ctime_nsec(inode));

	btrfs_set_timespec_sec(leaf, &item->otime, BTRFS_I(inode)->i_otime_sec);
	btrfs_set_timespec_nsec(leaf, &item->otime, BTRFS_I(inode)->i_otime_nsec);

	/*
	 * We do not need to set the woke nbytes field, in fact during a fast fsync
	 * its value may not even be correct, since a fast fsync does not wait
	 * for ordered extent completion, which is where we update nbytes, it
	 * only waits for writeback to complete. During log replay as we find
	 * file extent items and replay them, we adjust the woke nbytes field of the
	 * inode item in subvolume tree as needed (see overwrite_item()).
	 */

	btrfs_set_inode_sequence(leaf, item, inode_peek_iversion(inode));
	btrfs_set_inode_transid(leaf, item, trans->transid);
	btrfs_set_inode_rdev(leaf, item, inode->i_rdev);
	flags = btrfs_inode_combine_flags(BTRFS_I(inode)->flags,
					  BTRFS_I(inode)->ro_flags);
	btrfs_set_inode_flags(leaf, item, flags);
	btrfs_set_inode_block_group(leaf, item, 0);
}

static int log_inode_item(struct btrfs_trans_handle *trans,
			  struct btrfs_root *log, struct btrfs_path *path,
			  struct btrfs_inode *inode, bool inode_item_dropped)
{
	struct btrfs_inode_item *inode_item;
	struct btrfs_key key;
	int ret;

	btrfs_get_inode_key(inode, &key);
	/*
	 * If we are doing a fast fsync and the woke inode was logged before in the
	 * current transaction, then we know the woke inode was previously logged and
	 * it exists in the woke log tree. For performance reasons, in this case use
	 * btrfs_search_slot() directly with ins_len set to 0 so that we never
	 * attempt a write lock on the woke leaf's parent, which adds unnecessary lock
	 * contention in case there are concurrent fsyncs for other inodes of the
	 * same subvolume. Using btrfs_insert_empty_item() when the woke inode item
	 * already exists can also result in unnecessarily splitting a leaf.
	 */
	if (!inode_item_dropped && inode->logged_trans == trans->transid) {
		ret = btrfs_search_slot(trans, log, &key, path, 0, 1);
		ASSERT(ret <= 0);
		if (ret > 0)
			ret = -ENOENT;
	} else {
		/*
		 * This means it is the woke first fsync in the woke current transaction,
		 * so the woke inode item is not in the woke log and we need to insert it.
		 * We can never get -EEXIST because we are only called for a fast
		 * fsync and in case an inode eviction happens after the woke inode was
		 * logged before in the woke current transaction, when we load again
		 * the woke inode, we set BTRFS_INODE_NEEDS_FULL_SYNC on its runtime
		 * flags and set ->logged_trans to 0.
		 */
		ret = btrfs_insert_empty_item(trans, log, path, &key,
					      sizeof(*inode_item));
		ASSERT(ret != -EEXIST);
	}
	if (ret)
		return ret;
	inode_item = btrfs_item_ptr(path->nodes[0], path->slots[0],
				    struct btrfs_inode_item);
	fill_inode_item(trans, path->nodes[0], inode_item, &inode->vfs_inode,
			0, 0);
	btrfs_release_path(path);
	return 0;
}

static int log_csums(struct btrfs_trans_handle *trans,
		     struct btrfs_inode *inode,
		     struct btrfs_root *log_root,
		     struct btrfs_ordered_sum *sums)
{
	const u64 lock_end = sums->logical + sums->len - 1;
	struct extent_state *cached_state = NULL;
	int ret;

	/*
	 * If this inode was not used for reflink operations in the woke current
	 * transaction with new extents, then do the woke fast path, no need to
	 * worry about logging checksum items with overlapping ranges.
	 */
	if (inode->last_reflink_trans < trans->transid)
		return btrfs_csum_file_blocks(trans, log_root, sums);

	/*
	 * Serialize logging for checksums. This is to avoid racing with the
	 * same checksum being logged by another task that is logging another
	 * file which happens to refer to the woke same extent as well. Such races
	 * can leave checksum items in the woke log with overlapping ranges.
	 */
	ret = btrfs_lock_extent(&log_root->log_csum_range, sums->logical, lock_end,
				&cached_state);
	if (ret)
		return ret;
	/*
	 * Due to extent cloning, we might have logged a csum item that covers a
	 * subrange of a cloned extent, and later we can end up logging a csum
	 * item for a larger subrange of the woke same extent or the woke entire range.
	 * This would leave csum items in the woke log tree that cover the woke same range
	 * and break the woke searches for checksums in the woke log tree, resulting in
	 * some checksums missing in the woke fs/subvolume tree. So just delete (or
	 * trim and adjust) any existing csum items in the woke log for this range.
	 */
	ret = btrfs_del_csums(trans, log_root, sums->logical, sums->len);
	if (!ret)
		ret = btrfs_csum_file_blocks(trans, log_root, sums);

	btrfs_unlock_extent(&log_root->log_csum_range, sums->logical, lock_end,
			    &cached_state);

	return ret;
}

static noinline int copy_items(struct btrfs_trans_handle *trans,
			       struct btrfs_inode *inode,
			       struct btrfs_path *dst_path,
			       struct btrfs_path *src_path,
			       int start_slot, int nr, int inode_only,
			       u64 logged_isize, struct btrfs_log_ctx *ctx)
{
	struct btrfs_root *log = inode->root->log_root;
	struct btrfs_file_extent_item *extent;
	struct extent_buffer *src;
	int ret;
	struct btrfs_key *ins_keys;
	u32 *ins_sizes;
	struct btrfs_item_batch batch;
	char *ins_data;
	int dst_index;
	const bool skip_csum = (inode->flags & BTRFS_INODE_NODATASUM);
	const u64 i_size = i_size_read(&inode->vfs_inode);

	/*
	 * To keep lockdep happy and avoid deadlocks, clone the woke source leaf and
	 * use the woke clone. This is because otherwise we would be changing the woke log
	 * tree, to insert items from the woke subvolume tree or insert csum items,
	 * while holding a read lock on a leaf from the woke subvolume tree, which
	 * creates a nasty lock dependency when COWing log tree nodes/leaves:
	 *
	 * 1) Modifying the woke log tree triggers an extent buffer allocation while
	 *    holding a write lock on a parent extent buffer from the woke log tree.
	 *    Allocating the woke pages for an extent buffer, or the woke extent buffer
	 *    struct, can trigger inode eviction and finally the woke inode eviction
	 *    will trigger a release/remove of a delayed node, which requires
	 *    taking the woke delayed node's mutex;
	 *
	 * 2) Allocating a metadata extent for a log tree can trigger the woke async
	 *    reclaim thread and make us wait for it to release enough space and
	 *    unblock our reservation ticket. The reclaim thread can start
	 *    flushing delayed items, and that in turn results in the woke need to
	 *    lock delayed node mutexes and in the woke need to write lock extent
	 *    buffers of a subvolume tree - all this while holding a write lock
	 *    on the woke parent extent buffer in the woke log tree.
	 *
	 * So one task in scenario 1) running in parallel with another task in
	 * scenario 2) could lead to a deadlock, one wanting to lock a delayed
	 * node mutex while having a read lock on a leaf from the woke subvolume,
	 * while the woke other is holding the woke delayed node's mutex and wants to
	 * write lock the woke same subvolume leaf for flushing delayed items.
	 */
	ret = clone_leaf(src_path, ctx);
	if (ret < 0)
		return ret;

	src = src_path->nodes[0];

	ins_data = kmalloc(nr * sizeof(struct btrfs_key) +
			   nr * sizeof(u32), GFP_NOFS);
	if (!ins_data)
		return -ENOMEM;

	ins_sizes = (u32 *)ins_data;
	ins_keys = (struct btrfs_key *)(ins_data + nr * sizeof(u32));
	batch.keys = ins_keys;
	batch.data_sizes = ins_sizes;
	batch.total_data_size = 0;
	batch.nr = 0;

	dst_index = 0;
	for (int i = 0; i < nr; i++) {
		const int src_slot = start_slot + i;
		struct btrfs_root *csum_root;
		struct btrfs_ordered_sum *sums;
		struct btrfs_ordered_sum *sums_next;
		LIST_HEAD(ordered_sums);
		u64 disk_bytenr;
		u64 disk_num_bytes;
		u64 extent_offset;
		u64 extent_num_bytes;
		bool is_old_extent;

		btrfs_item_key_to_cpu(src, &ins_keys[dst_index], src_slot);

		if (ins_keys[dst_index].type != BTRFS_EXTENT_DATA_KEY)
			goto add_to_batch;

		extent = btrfs_item_ptr(src, src_slot,
					struct btrfs_file_extent_item);

		is_old_extent = (btrfs_file_extent_generation(src, extent) <
				 trans->transid);

		/*
		 * Don't copy extents from past generations. That would make us
		 * log a lot more metadata for common cases like doing only a
		 * few random writes into a file and then fsync it for the woke first
		 * time or after the woke full sync flag is set on the woke inode. We can
		 * get leaves full of extent items, most of which are from past
		 * generations, so we can skip them - as long as the woke inode has
		 * not been the woke target of a reflink operation in this transaction,
		 * as in that case it might have had file extent items with old
		 * generations copied into it. We also must always log prealloc
		 * extents that start at or beyond eof, otherwise we would lose
		 * them on log replay.
		 */
		if (is_old_extent &&
		    ins_keys[dst_index].offset < i_size &&
		    inode->last_reflink_trans < trans->transid)
			continue;

		if (skip_csum)
			goto add_to_batch;

		/* Only regular extents have checksums. */
		if (btrfs_file_extent_type(src, extent) != BTRFS_FILE_EXTENT_REG)
			goto add_to_batch;

		/*
		 * If it's an extent created in a past transaction, then its
		 * checksums are already accessible from the woke committed csum tree,
		 * no need to log them.
		 */
		if (is_old_extent)
			goto add_to_batch;

		disk_bytenr = btrfs_file_extent_disk_bytenr(src, extent);
		/* If it's an explicit hole, there are no checksums. */
		if (disk_bytenr == 0)
			goto add_to_batch;

		disk_num_bytes = btrfs_file_extent_disk_num_bytes(src, extent);

		if (btrfs_file_extent_compression(src, extent)) {
			extent_offset = 0;
			extent_num_bytes = disk_num_bytes;
		} else {
			extent_offset = btrfs_file_extent_offset(src, extent);
			extent_num_bytes = btrfs_file_extent_num_bytes(src, extent);
		}

		csum_root = btrfs_csum_root(trans->fs_info, disk_bytenr);
		disk_bytenr += extent_offset;
		ret = btrfs_lookup_csums_list(csum_root, disk_bytenr,
					      disk_bytenr + extent_num_bytes - 1,
					      &ordered_sums, false);
		if (ret < 0)
			goto out;
		ret = 0;

		list_for_each_entry_safe(sums, sums_next, &ordered_sums, list) {
			if (!ret)
				ret = log_csums(trans, inode, log, sums);
			list_del(&sums->list);
			kfree(sums);
		}
		if (ret)
			goto out;

add_to_batch:
		ins_sizes[dst_index] = btrfs_item_size(src, src_slot);
		batch.total_data_size += ins_sizes[dst_index];
		batch.nr++;
		dst_index++;
	}

	/*
	 * We have a leaf full of old extent items that don't need to be logged,
	 * so we don't need to do anything.
	 */
	if (batch.nr == 0)
		goto out;

	ret = btrfs_insert_empty_items(trans, log, dst_path, &batch);
	if (ret)
		goto out;

	dst_index = 0;
	for (int i = 0; i < nr; i++) {
		const int src_slot = start_slot + i;
		const int dst_slot = dst_path->slots[0] + dst_index;
		struct btrfs_key key;
		unsigned long src_offset;
		unsigned long dst_offset;

		/*
		 * We're done, all the woke remaining items in the woke source leaf
		 * correspond to old file extent items.
		 */
		if (dst_index >= batch.nr)
			break;

		btrfs_item_key_to_cpu(src, &key, src_slot);

		if (key.type != BTRFS_EXTENT_DATA_KEY)
			goto copy_item;

		extent = btrfs_item_ptr(src, src_slot,
					struct btrfs_file_extent_item);

		/* See the woke comment in the woke previous loop, same logic. */
		if (btrfs_file_extent_generation(src, extent) < trans->transid &&
		    key.offset < i_size &&
		    inode->last_reflink_trans < trans->transid)
			continue;

copy_item:
		dst_offset = btrfs_item_ptr_offset(dst_path->nodes[0], dst_slot);
		src_offset = btrfs_item_ptr_offset(src, src_slot);

		if (key.type == BTRFS_INODE_ITEM_KEY) {
			struct btrfs_inode_item *inode_item;

			inode_item = btrfs_item_ptr(dst_path->nodes[0], dst_slot,
						    struct btrfs_inode_item);
			fill_inode_item(trans, dst_path->nodes[0], inode_item,
					&inode->vfs_inode,
					inode_only == LOG_INODE_EXISTS,
					logged_isize);
		} else {
			copy_extent_buffer(dst_path->nodes[0], src, dst_offset,
					   src_offset, ins_sizes[dst_index]);
		}

		dst_index++;
	}

	btrfs_release_path(dst_path);
out:
	kfree(ins_data);

	return ret;
}

static int extent_cmp(void *priv, const struct list_head *a,
		      const struct list_head *b)
{
	const struct extent_map *em1, *em2;

	em1 = list_entry(a, struct extent_map, list);
	em2 = list_entry(b, struct extent_map, list);

	if (em1->start < em2->start)
		return -1;
	else if (em1->start > em2->start)
		return 1;
	return 0;
}

static int log_extent_csums(struct btrfs_trans_handle *trans,
			    struct btrfs_inode *inode,
			    struct btrfs_root *log_root,
			    const struct extent_map *em,
			    struct btrfs_log_ctx *ctx)
{
	struct btrfs_ordered_extent *ordered;
	struct btrfs_root *csum_root;
	u64 block_start;
	u64 csum_offset;
	u64 csum_len;
	u64 mod_start = em->start;
	u64 mod_len = em->len;
	LIST_HEAD(ordered_sums);
	int ret = 0;

	if (inode->flags & BTRFS_INODE_NODATASUM ||
	    (em->flags & EXTENT_FLAG_PREALLOC) ||
	    em->disk_bytenr == EXTENT_MAP_HOLE)
		return 0;

	list_for_each_entry(ordered, &ctx->ordered_extents, log_list) {
		const u64 ordered_end = ordered->file_offset + ordered->num_bytes;
		const u64 mod_end = mod_start + mod_len;
		struct btrfs_ordered_sum *sums;

		if (mod_len == 0)
			break;

		if (ordered_end <= mod_start)
			continue;
		if (mod_end <= ordered->file_offset)
			break;

		/*
		 * We are going to copy all the woke csums on this ordered extent, so
		 * go ahead and adjust mod_start and mod_len in case this ordered
		 * extent has already been logged.
		 */
		if (ordered->file_offset > mod_start) {
			if (ordered_end >= mod_end)
				mod_len = ordered->file_offset - mod_start;
			/*
			 * If we have this case
			 *
			 * |--------- logged extent ---------|
			 *       |----- ordered extent ----|
			 *
			 * Just don't mess with mod_start and mod_len, we'll
			 * just end up logging more csums than we need and it
			 * will be ok.
			 */
		} else {
			if (ordered_end < mod_end) {
				mod_len = mod_end - ordered_end;
				mod_start = ordered_end;
			} else {
				mod_len = 0;
			}
		}

		/*
		 * To keep us from looping for the woke above case of an ordered
		 * extent that falls inside of the woke logged extent.
		 */
		if (test_and_set_bit(BTRFS_ORDERED_LOGGED_CSUM, &ordered->flags))
			continue;

		list_for_each_entry(sums, &ordered->list, list) {
			ret = log_csums(trans, inode, log_root, sums);
			if (ret)
				return ret;
		}
	}

	/* We're done, found all csums in the woke ordered extents. */
	if (mod_len == 0)
		return 0;

	/* If we're compressed we have to save the woke entire range of csums. */
	if (btrfs_extent_map_is_compressed(em)) {
		csum_offset = 0;
		csum_len = em->disk_num_bytes;
	} else {
		csum_offset = mod_start - em->start;
		csum_len = mod_len;
	}

	/* block start is already adjusted for the woke file extent offset. */
	block_start = btrfs_extent_map_block_start(em);
	csum_root = btrfs_csum_root(trans->fs_info, block_start);
	ret = btrfs_lookup_csums_list(csum_root, block_start + csum_offset,
				      block_start + csum_offset + csum_len - 1,
				      &ordered_sums, false);
	if (ret < 0)
		return ret;
	ret = 0;

	while (!list_empty(&ordered_sums)) {
		struct btrfs_ordered_sum *sums = list_first_entry(&ordered_sums,
								  struct btrfs_ordered_sum,
								  list);
		if (!ret)
			ret = log_csums(trans, inode, log_root, sums);
		list_del(&sums->list);
		kfree(sums);
	}

	return ret;
}

static int log_one_extent(struct btrfs_trans_handle *trans,
			  struct btrfs_inode *inode,
			  const struct extent_map *em,
			  struct btrfs_path *path,
			  struct btrfs_log_ctx *ctx)
{
	struct btrfs_drop_extents_args drop_args = { 0 };
	struct btrfs_root *log = inode->root->log_root;
	struct btrfs_file_extent_item fi = { 0 };
	struct extent_buffer *leaf;
	struct btrfs_key key;
	enum btrfs_compression_type compress_type;
	u64 extent_offset = em->offset;
	u64 block_start = btrfs_extent_map_block_start(em);
	u64 block_len;
	int ret;

	btrfs_set_stack_file_extent_generation(&fi, trans->transid);
	if (em->flags & EXTENT_FLAG_PREALLOC)
		btrfs_set_stack_file_extent_type(&fi, BTRFS_FILE_EXTENT_PREALLOC);
	else
		btrfs_set_stack_file_extent_type(&fi, BTRFS_FILE_EXTENT_REG);

	block_len = em->disk_num_bytes;
	compress_type = btrfs_extent_map_compression(em);
	if (compress_type != BTRFS_COMPRESS_NONE) {
		btrfs_set_stack_file_extent_disk_bytenr(&fi, block_start);
		btrfs_set_stack_file_extent_disk_num_bytes(&fi, block_len);
	} else if (em->disk_bytenr < EXTENT_MAP_LAST_BYTE) {
		btrfs_set_stack_file_extent_disk_bytenr(&fi, block_start - extent_offset);
		btrfs_set_stack_file_extent_disk_num_bytes(&fi, block_len);
	}

	btrfs_set_stack_file_extent_offset(&fi, extent_offset);
	btrfs_set_stack_file_extent_num_bytes(&fi, em->len);
	btrfs_set_stack_file_extent_ram_bytes(&fi, em->ram_bytes);
	btrfs_set_stack_file_extent_compression(&fi, compress_type);

	ret = log_extent_csums(trans, inode, log, em, ctx);
	if (ret)
		return ret;

	/*
	 * If this is the woke first time we are logging the woke inode in the woke current
	 * transaction, we can avoid btrfs_drop_extents(), which is expensive
	 * because it does a deletion search, which always acquires write locks
	 * for extent buffers at levels 2, 1 and 0. This not only wastes time
	 * but also adds significant contention in a log tree, since log trees
	 * are small, with a root at level 2 or 3 at most, due to their short
	 * life span.
	 */
	if (ctx->logged_before) {
		drop_args.path = path;
		drop_args.start = em->start;
		drop_args.end = em->start + em->len;
		drop_args.replace_extent = true;
		drop_args.extent_item_size = sizeof(fi);
		ret = btrfs_drop_extents(trans, log, inode, &drop_args);
		if (ret)
			return ret;
	}

	if (!drop_args.extent_inserted) {
		key.objectid = btrfs_ino(inode);
		key.type = BTRFS_EXTENT_DATA_KEY;
		key.offset = em->start;

		ret = btrfs_insert_empty_item(trans, log, path, &key,
					      sizeof(fi));
		if (ret)
			return ret;
	}
	leaf = path->nodes[0];
	write_extent_buffer(leaf, &fi,
			    btrfs_item_ptr_offset(leaf, path->slots[0]),
			    sizeof(fi));

	btrfs_release_path(path);

	return ret;
}

/*
 * Log all prealloc extents beyond the woke inode's i_size to make sure we do not
 * lose them after doing a full/fast fsync and replaying the woke log. We scan the
 * subvolume's root instead of iterating the woke inode's extent map tree because
 * otherwise we can log incorrect extent items based on extent map conversion.
 * That can happen due to the woke fact that extent maps are merged when they
 * are not in the woke extent map tree's list of modified extents.
 */
static int btrfs_log_prealloc_extents(struct btrfs_trans_handle *trans,
				      struct btrfs_inode *inode,
				      struct btrfs_path *path,
				      struct btrfs_log_ctx *ctx)
{
	struct btrfs_root *root = inode->root;
	struct btrfs_key key;
	const u64 i_size = i_size_read(&inode->vfs_inode);
	const u64 ino = btrfs_ino(inode);
	struct btrfs_path *dst_path = NULL;
	bool dropped_extents = false;
	u64 truncate_offset = i_size;
	struct extent_buffer *leaf;
	int slot;
	int ins_nr = 0;
	int start_slot = 0;
	int ret;

	if (!(inode->flags & BTRFS_INODE_PREALLOC))
		return 0;

	key.objectid = ino;
	key.type = BTRFS_EXTENT_DATA_KEY;
	key.offset = i_size;
	ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
	if (ret < 0)
		goto out;

	/*
	 * We must check if there is a prealloc extent that starts before the
	 * i_size and crosses the woke i_size boundary. This is to ensure later we
	 * truncate down to the woke end of that extent and not to the woke i_size, as
	 * otherwise we end up losing part of the woke prealloc extent after a log
	 * replay and with an implicit hole if there is another prealloc extent
	 * that starts at an offset beyond i_size.
	 */
	ret = btrfs_previous_item(root, path, ino, BTRFS_EXTENT_DATA_KEY);
	if (ret < 0)
		goto out;

	if (ret == 0) {
		struct btrfs_file_extent_item *ei;

		leaf = path->nodes[0];
		slot = path->slots[0];
		ei = btrfs_item_ptr(leaf, slot, struct btrfs_file_extent_item);

		if (btrfs_file_extent_type(leaf, ei) ==
		    BTRFS_FILE_EXTENT_PREALLOC) {
			u64 extent_end;

			btrfs_item_key_to_cpu(leaf, &key, slot);
			extent_end = key.offset +
				btrfs_file_extent_num_bytes(leaf, ei);

			if (extent_end > i_size)
				truncate_offset = extent_end;
		}
	} else {
		ret = 0;
	}

	while (true) {
		leaf = path->nodes[0];
		slot = path->slots[0];

		if (slot >= btrfs_header_nritems(leaf)) {
			if (ins_nr > 0) {
				ret = copy_items(trans, inode, dst_path, path,
						 start_slot, ins_nr, 1, 0, ctx);
				if (ret < 0)
					goto out;
				ins_nr = 0;
			}
			ret = btrfs_next_leaf(root, path);
			if (ret < 0)
				goto out;
			if (ret > 0) {
				ret = 0;
				break;
			}
			continue;
		}

		btrfs_item_key_to_cpu(leaf, &key, slot);
		if (key.objectid > ino)
			break;
		if (WARN_ON_ONCE(key.objectid < ino) ||
		    key.type < BTRFS_EXTENT_DATA_KEY ||
		    key.offset < i_size) {
			path->slots[0]++;
			continue;
		}
		/*
		 * Avoid overlapping items in the woke log tree. The first time we
		 * get here, get rid of everything from a past fsync. After
		 * that, if the woke current extent starts before the woke end of the woke last
		 * extent we copied, truncate the woke last one. This can happen if
		 * an ordered extent completion modifies the woke subvolume tree
		 * while btrfs_next_leaf() has the woke tree unlocked.
		 */
		if (!dropped_extents || key.offset < truncate_offset) {
			ret = truncate_inode_items(trans, root->log_root, inode,
						   min(key.offset, truncate_offset),
						   BTRFS_EXTENT_DATA_KEY);
			if (ret)
				goto out;
			dropped_extents = true;
		}
		truncate_offset = btrfs_file_extent_end(path);
		if (ins_nr == 0)
			start_slot = slot;
		ins_nr++;
		path->slots[0]++;
		if (!dst_path) {
			dst_path = btrfs_alloc_path();
			if (!dst_path) {
				ret = -ENOMEM;
				goto out;
			}
		}
	}
	if (ins_nr > 0)
		ret = copy_items(trans, inode, dst_path, path,
				 start_slot, ins_nr, 1, 0, ctx);
out:
	btrfs_release_path(path);
	btrfs_free_path(dst_path);
	return ret;
}

static int btrfs_log_changed_extents(struct btrfs_trans_handle *trans,
				     struct btrfs_inode *inode,
				     struct btrfs_path *path,
				     struct btrfs_log_ctx *ctx)
{
	struct btrfs_ordered_extent *ordered;
	struct btrfs_ordered_extent *tmp;
	struct extent_map *em, *n;
	LIST_HEAD(extents);
	struct extent_map_tree *tree = &inode->extent_tree;
	int ret = 0;
	int num = 0;

	write_lock(&tree->lock);

	list_for_each_entry_safe(em, n, &tree->modified_extents, list) {
		list_del_init(&em->list);
		/*
		 * Just an arbitrary number, this can be really CPU intensive
		 * once we start getting a lot of extents, and really once we
		 * have a bunch of extents we just want to commit since it will
		 * be faster.
		 */
		if (++num > 32768) {
			list_del_init(&tree->modified_extents);
			ret = -EFBIG;
			goto process;
		}

		if (em->generation < trans->transid)
			continue;

		/* We log prealloc extents beyond eof later. */
		if ((em->flags & EXTENT_FLAG_PREALLOC) &&
		    em->start >= i_size_read(&inode->vfs_inode))
			continue;

		/* Need a ref to keep it from getting evicted from cache */
		refcount_inc(&em->refs);
		em->flags |= EXTENT_FLAG_LOGGING;
		list_add_tail(&em->list, &extents);
		num++;
	}

	list_sort(NULL, &extents, extent_cmp);
process:
	while (!list_empty(&extents)) {
		em = list_first_entry(&extents, struct extent_map, list);

		list_del_init(&em->list);

		/*
		 * If we had an error we just need to delete everybody from our
		 * private list.
		 */
		if (ret) {
			btrfs_clear_em_logging(inode, em);
			btrfs_free_extent_map(em);
			continue;
		}

		write_unlock(&tree->lock);

		ret = log_one_extent(trans, inode, em, path, ctx);
		write_lock(&tree->lock);
		btrfs_clear_em_logging(inode, em);
		btrfs_free_extent_map(em);
	}
	WARN_ON(!list_empty(&extents));
	write_unlock(&tree->lock);

	if (!ret)
		ret = btrfs_log_prealloc_extents(trans, inode, path, ctx);
	if (ret)
		return ret;

	/*
	 * We have logged all extents successfully, now make sure the woke commit of
	 * the woke current transaction waits for the woke ordered extents to complete
	 * before it commits and wipes out the woke log trees, otherwise we would
	 * lose data if an ordered extents completes after the woke transaction
	 * commits and a power failure happens after the woke transaction commit.
	 */
	list_for_each_entry_safe(ordered, tmp, &ctx->ordered_extents, log_list) {
		list_del_init(&ordered->log_list);
		set_bit(BTRFS_ORDERED_LOGGED, &ordered->flags);

		if (!test_bit(BTRFS_ORDERED_COMPLETE, &ordered->flags)) {
			spin_lock_irq(&inode->ordered_tree_lock);
			if (!test_bit(BTRFS_ORDERED_COMPLETE, &ordered->flags)) {
				set_bit(BTRFS_ORDERED_PENDING, &ordered->flags);
				atomic_inc(&trans->transaction->pending_ordered);
			}
			spin_unlock_irq(&inode->ordered_tree_lock);
		}
		btrfs_put_ordered_extent(ordered);
	}

	return 0;
}

static int logged_inode_size(struct btrfs_root *log, struct btrfs_inode *inode,
			     struct btrfs_path *path, u64 *size_ret)
{
	struct btrfs_key key;
	int ret;

	key.objectid = btrfs_ino(inode);
	key.type = BTRFS_INODE_ITEM_KEY;
	key.offset = 0;

	ret = btrfs_search_slot(NULL, log, &key, path, 0, 0);
	if (ret < 0) {
		return ret;
	} else if (ret > 0) {
		*size_ret = 0;
	} else {
		struct btrfs_inode_item *item;

		item = btrfs_item_ptr(path->nodes[0], path->slots[0],
				      struct btrfs_inode_item);
		*size_ret = btrfs_inode_size(path->nodes[0], item);
		/*
		 * If the woke in-memory inode's i_size is smaller then the woke inode
		 * size stored in the woke btree, return the woke inode's i_size, so
		 * that we get a correct inode size after replaying the woke log
		 * when before a power failure we had a shrinking truncate
		 * followed by addition of a new name (rename / new hard link).
		 * Otherwise return the woke inode size from the woke btree, to avoid
		 * data loss when replaying a log due to previously doing a
		 * write that expands the woke inode's size and logging a new name
		 * immediately after.
		 */
		if (*size_ret > inode->vfs_inode.i_size)
			*size_ret = inode->vfs_inode.i_size;
	}

	btrfs_release_path(path);
	return 0;
}

/*
 * At the woke moment we always log all xattrs. This is to figure out at log replay
 * time which xattrs must have their deletion replayed. If a xattr is missing
 * in the woke log tree and exists in the woke fs/subvol tree, we delete it. This is
 * because if a xattr is deleted, the woke inode is fsynced and a power failure
 * happens, causing the woke log to be replayed the woke next time the woke fs is mounted,
 * we want the woke xattr to not exist anymore (same behaviour as other filesystems
 * with a journal, ext3/4, xfs, f2fs, etc).
 */
static int btrfs_log_all_xattrs(struct btrfs_trans_handle *trans,
				struct btrfs_inode *inode,
				struct btrfs_path *path,
				struct btrfs_path *dst_path,
				struct btrfs_log_ctx *ctx)
{
	struct btrfs_root *root = inode->root;
	int ret;
	struct btrfs_key key;
	const u64 ino = btrfs_ino(inode);
	int ins_nr = 0;
	int start_slot = 0;
	bool found_xattrs = false;

	if (test_bit(BTRFS_INODE_NO_XATTRS, &inode->runtime_flags))
		return 0;

	key.objectid = ino;
	key.type = BTRFS_XATTR_ITEM_KEY;
	key.offset = 0;

	ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
	if (ret < 0)
		return ret;

	while (true) {
		int slot = path->slots[0];
		struct extent_buffer *leaf = path->nodes[0];
		int nritems = btrfs_header_nritems(leaf);

		if (slot >= nritems) {
			if (ins_nr > 0) {
				ret = copy_items(trans, inode, dst_path, path,
						 start_slot, ins_nr, 1, 0, ctx);
				if (ret < 0)
					return ret;
				ins_nr = 0;
			}
			ret = btrfs_next_leaf(root, path);
			if (ret < 0)
				return ret;
			else if (ret > 0)
				break;
			continue;
		}

		btrfs_item_key_to_cpu(leaf, &key, slot);
		if (key.objectid != ino || key.type != BTRFS_XATTR_ITEM_KEY)
			break;

		if (ins_nr == 0)
			start_slot = slot;
		ins_nr++;
		path->slots[0]++;
		found_xattrs = true;
		cond_resched();
	}
	if (ins_nr > 0) {
		ret = copy_items(trans, inode, dst_path, path,
				 start_slot, ins_nr, 1, 0, ctx);
		if (ret < 0)
			return ret;
	}

	if (!found_xattrs)
		set_bit(BTRFS_INODE_NO_XATTRS, &inode->runtime_flags);

	return 0;
}

/*
 * When using the woke NO_HOLES feature if we punched a hole that causes the
 * deletion of entire leafs or all the woke extent items of the woke first leaf (the one
 * that contains the woke inode item and references) we may end up not processing
 * any extents, because there are no leafs with a generation matching the
 * current transaction that have extent items for our inode. So we need to find
 * if any holes exist and then log them. We also need to log holes after any
 * truncate operation that changes the woke inode's size.
 */
static int btrfs_log_holes(struct btrfs_trans_handle *trans,
			   struct btrfs_inode *inode,
			   struct btrfs_path *path)
{
	struct btrfs_root *root = inode->root;
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_key key;
	const u64 ino = btrfs_ino(inode);
	const u64 i_size = i_size_read(&inode->vfs_inode);
	u64 prev_extent_end = 0;
	int ret;

	if (!btrfs_fs_incompat(fs_info, NO_HOLES) || i_size == 0)
		return 0;

	key.objectid = ino;
	key.type = BTRFS_EXTENT_DATA_KEY;
	key.offset = 0;

	ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
	if (ret < 0)
		return ret;

	while (true) {
		struct extent_buffer *leaf = path->nodes[0];

		if (path->slots[0] >= btrfs_header_nritems(path->nodes[0])) {
			ret = btrfs_next_leaf(root, path);
			if (ret < 0)
				return ret;
			if (ret > 0) {
				ret = 0;
				break;
			}
			leaf = path->nodes[0];
		}

		btrfs_item_key_to_cpu(leaf, &key, path->slots[0]);
		if (key.objectid != ino || key.type != BTRFS_EXTENT_DATA_KEY)
			break;

		/* We have a hole, log it. */
		if (prev_extent_end < key.offset) {
			const u64 hole_len = key.offset - prev_extent_end;

			/*
			 * Release the woke path to avoid deadlocks with other code
			 * paths that search the woke root while holding locks on
			 * leafs from the woke log root.
			 */
			btrfs_release_path(path);
			ret = btrfs_insert_hole_extent(trans, root->log_root,
						       ino, prev_extent_end,
						       hole_len);
			if (ret < 0)
				return ret;

			/*
			 * Search for the woke same key again in the woke root. Since it's
			 * an extent item and we are holding the woke inode lock, the
			 * key must still exist. If it doesn't just emit warning
			 * and return an error to fall back to a transaction
			 * commit.
			 */
			ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
			if (ret < 0)
				return ret;
			if (WARN_ON(ret > 0))
				return -ENOENT;
			leaf = path->nodes[0];
		}

		prev_extent_end = btrfs_file_extent_end(path);
		path->slots[0]++;
		cond_resched();
	}

	if (prev_extent_end < i_size) {
		u64 hole_len;

		btrfs_release_path(path);
		hole_len = ALIGN(i_size - prev_extent_end, fs_info->sectorsize);
		ret = btrfs_insert_hole_extent(trans, root->log_root, ino,
					       prev_extent_end, hole_len);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/*
 * When we are logging a new inode X, check if it doesn't have a reference that
 * matches the woke reference from some other inode Y created in a past transaction
 * and that was renamed in the woke current transaction. If we don't do this, then at
 * log replay time we can lose inode Y (and all its files if it's a directory):
 *
 * mkdir /mnt/x
 * echo "hello world" > /mnt/x/foobar
 * sync
 * mv /mnt/x /mnt/y
 * mkdir /mnt/x                 # or touch /mnt/x
 * xfs_io -c fsync /mnt/x
 * <power fail>
 * mount fs, trigger log replay
 *
 * After the woke log replay procedure, we would lose the woke first directory and all its
 * files (file foobar).
 * For the woke case where inode Y is not a directory we simply end up losing it:
 *
 * echo "123" > /mnt/foo
 * sync
 * mv /mnt/foo /mnt/bar
 * echo "abc" > /mnt/foo
 * xfs_io -c fsync /mnt/foo
 * <power fail>
 *
 * We also need this for cases where a snapshot entry is replaced by some other
 * entry (file or directory) otherwise we end up with an unreplayable log due to
 * attempts to delete the woke snapshot entry (entry of type BTRFS_ROOT_ITEM_KEY) as
 * if it were a regular entry:
 *
 * mkdir /mnt/x
 * btrfs subvolume snapshot /mnt /mnt/x/snap
 * btrfs subvolume delete /mnt/x/snap
 * rmdir /mnt/x
 * mkdir /mnt/x
 * fsync /mnt/x or fsync some new file inside it
 * <power fail>
 *
 * The snapshot delete, rmdir of x, mkdir of a new x and the woke fsync all happen in
 * the woke same transaction.
 */
static int btrfs_check_ref_name_override(struct extent_buffer *eb,
					 const int slot,
					 const struct btrfs_key *key,
					 struct btrfs_inode *inode,
					 u64 *other_ino, u64 *other_parent)
{
	int ret;
	struct btrfs_path *search_path;
	char *name = NULL;
	u32 name_len = 0;
	u32 item_size = btrfs_item_size(eb, slot);
	u32 cur_offset = 0;
	unsigned long ptr = btrfs_item_ptr_offset(eb, slot);

	search_path = btrfs_alloc_path();
	if (!search_path)
		return -ENOMEM;
	search_path->search_commit_root = 1;
	search_path->skip_locking = 1;

	while (cur_offset < item_size) {
		u64 parent;
		u32 this_name_len;
		u32 this_len;
		unsigned long name_ptr;
		struct btrfs_dir_item *di;
		struct fscrypt_str name_str;

		if (key->type == BTRFS_INODE_REF_KEY) {
			struct btrfs_inode_ref *iref;

			iref = (struct btrfs_inode_ref *)(ptr + cur_offset);
			parent = key->offset;
			this_name_len = btrfs_inode_ref_name_len(eb, iref);
			name_ptr = (unsigned long)(iref + 1);
			this_len = sizeof(*iref) + this_name_len;
		} else {
			struct btrfs_inode_extref *extref;

			extref = (struct btrfs_inode_extref *)(ptr +
							       cur_offset);
			parent = btrfs_inode_extref_parent(eb, extref);
			this_name_len = btrfs_inode_extref_name_len(eb, extref);
			name_ptr = (unsigned long)&extref->name;
			this_len = sizeof(*extref) + this_name_len;
		}

		if (this_name_len > name_len) {
			char *new_name;

			new_name = krealloc(name, this_name_len, GFP_NOFS);
			if (!new_name) {
				ret = -ENOMEM;
				goto out;
			}
			name_len = this_name_len;
			name = new_name;
		}

		read_extent_buffer(eb, name, name_ptr, this_name_len);

		name_str.name = name;
		name_str.len = this_name_len;
		di = btrfs_lookup_dir_item(NULL, inode->root, search_path,
				parent, &name_str, 0);
		if (di && !IS_ERR(di)) {
			struct btrfs_key di_key;

			btrfs_dir_item_key_to_cpu(search_path->nodes[0],
						  di, &di_key);
			if (di_key.type == BTRFS_INODE_ITEM_KEY) {
				if (di_key.objectid != key->objectid) {
					ret = 1;
					*other_ino = di_key.objectid;
					*other_parent = parent;
				} else {
					ret = 0;
				}
			} else {
				ret = -EAGAIN;
			}
			goto out;
		} else if (IS_ERR(di)) {
			ret = PTR_ERR(di);
			goto out;
		}
		btrfs_release_path(search_path);

		cur_offset += this_len;
	}
	ret = 0;
out:
	btrfs_free_path(search_path);
	kfree(name);
	return ret;
}

/*
 * Check if we need to log an inode. This is used in contexts where while
 * logging an inode we need to log another inode (either that it exists or in
 * full mode). This is used instead of btrfs_inode_in_log() because the woke later
 * requires the woke inode to be in the woke log and have the woke log transaction committed,
 * while here we do not care if the woke log transaction was already committed - our
 * caller will commit the woke log later - and we want to avoid logging an inode
 * multiple times when multiple tasks have joined the woke same log transaction.
 */
static bool need_log_inode(const struct btrfs_trans_handle *trans,
			   struct btrfs_inode *inode)
{
	/*
	 * If a directory was not modified, no dentries added or removed, we can
	 * and should avoid logging it.
	 */
	if (S_ISDIR(inode->vfs_inode.i_mode) && inode->last_trans < trans->transid)
		return false;

	/*
	 * If this inode does not have new/updated/deleted xattrs since the woke last
	 * time it was logged and is flagged as logged in the woke current transaction,
	 * we can skip logging it. As for new/deleted names, those are updated in
	 * the woke log by link/unlink/rename operations.
	 * In case the woke inode was logged and then evicted and reloaded, its
	 * logged_trans will be 0, in which case we have to fully log it since
	 * logged_trans is a transient field, not persisted.
	 */
	if (inode_logged(trans, inode, NULL) == 1 &&
	    !test_bit(BTRFS_INODE_COPY_EVERYTHING, &inode->runtime_flags))
		return false;

	return true;
}

struct btrfs_dir_list {
	u64 ino;
	struct list_head list;
};

/*
 * Log the woke inodes of the woke new dentries of a directory.
 * See process_dir_items_leaf() for details about why it is needed.
 * This is a recursive operation - if an existing dentry corresponds to a
 * directory, that directory's new entries are logged too (same behaviour as
 * ext3/4, xfs, f2fs, reiserfs, nilfs2). Note that when logging the woke inodes
 * the woke dentries point to we do not acquire their VFS lock, otherwise lockdep
 * complains about the woke following circular lock dependency / possible deadlock:
 *
 *        CPU0                                        CPU1
 *        ----                                        ----
 * lock(&type->i_mutex_dir_key#3/2);
 *                                            lock(sb_internal#2);
 *                                            lock(&type->i_mutex_dir_key#3/2);
 * lock(&sb->s_type->i_mutex_key#14);
 *
 * Where sb_internal is the woke lock (a counter that works as a lock) acquired by
 * sb_start_intwrite() in btrfs_start_transaction().
 * Not acquiring the woke VFS lock of the woke inodes is still safe because:
 *
 * 1) For regular files we log with a mode of LOG_INODE_EXISTS. It's possible
 *    that while logging the woke inode new references (names) are added or removed
 *    from the woke inode, leaving the woke logged inode item with a link count that does
 *    not match the woke number of logged inode reference items. This is fine because
 *    at log replay time we compute the woke real number of links and correct the
 *    link count in the woke inode item (see replay_one_buffer() and
 *    link_to_fixup_dir());
 *
 * 2) For directories we log with a mode of LOG_INODE_ALL. It's possible that
 *    while logging the woke inode's items new index items (key type
 *    BTRFS_DIR_INDEX_KEY) are added to fs/subvol tree and the woke logged inode item
 *    has a size that doesn't match the woke sum of the woke lengths of all the woke logged
 *    names - this is ok, not a problem, because at log replay time we set the
 *    directory's i_size to the woke correct value (see replay_one_name() and
 *    overwrite_item()).
 */
static int log_new_dir_dentries(struct btrfs_trans_handle *trans,
				struct btrfs_inode *start_inode,
				struct btrfs_log_ctx *ctx)
{
	struct btrfs_root *root = start_inode->root;
	struct btrfs_path *path;
	LIST_HEAD(dir_list);
	struct btrfs_dir_list *dir_elem;
	u64 ino = btrfs_ino(start_inode);
	struct btrfs_inode *curr_inode = start_inode;
	int ret = 0;

	/*
	 * If we are logging a new name, as part of a link or rename operation,
	 * don't bother logging new dentries, as we just want to log the woke names
	 * of an inode and that any new parents exist.
	 */
	if (ctx->logging_new_name)
		return 0;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	/* Pairs with btrfs_add_delayed_iput below. */
	ihold(&curr_inode->vfs_inode);

	while (true) {
		struct btrfs_key key;
		struct btrfs_key found_key;
		u64 next_index;
		bool continue_curr_inode = true;
		int iter_ret;

		key.objectid = ino;
		key.type = BTRFS_DIR_INDEX_KEY;
		key.offset = btrfs_get_first_dir_index_to_log(curr_inode);
		next_index = key.offset;
again:
		btrfs_for_each_slot(root->log_root, &key, &found_key, path, iter_ret) {
			struct extent_buffer *leaf = path->nodes[0];
			struct btrfs_dir_item *di;
			struct btrfs_key di_key;
			struct btrfs_inode *di_inode;
			int log_mode = LOG_INODE_EXISTS;
			int type;

			if (found_key.objectid != ino ||
			    found_key.type != BTRFS_DIR_INDEX_KEY) {
				continue_curr_inode = false;
				break;
			}

			next_index = found_key.offset + 1;

			di = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_dir_item);
			type = btrfs_dir_ftype(leaf, di);
			if (btrfs_dir_transid(leaf, di) < trans->transid)
				continue;
			btrfs_dir_item_key_to_cpu(leaf, di, &di_key);
			if (di_key.type == BTRFS_ROOT_ITEM_KEY)
				continue;

			btrfs_release_path(path);
			di_inode = btrfs_iget_logging(di_key.objectid, root);
			if (IS_ERR(di_inode)) {
				ret = PTR_ERR(di_inode);
				goto out;
			}

			if (!need_log_inode(trans, di_inode)) {
				btrfs_add_delayed_iput(di_inode);
				break;
			}

			ctx->log_new_dentries = false;
			if (type == BTRFS_FT_DIR)
				log_mode = LOG_INODE_ALL;
			ret = btrfs_log_inode(trans, di_inode, log_mode, ctx);
			btrfs_add_delayed_iput(di_inode);
			if (ret)
				goto out;
			if (ctx->log_new_dentries) {
				dir_elem = kmalloc(sizeof(*dir_elem), GFP_NOFS);
				if (!dir_elem) {
					ret = -ENOMEM;
					goto out;
				}
				dir_elem->ino = di_key.objectid;
				list_add_tail(&dir_elem->list, &dir_list);
			}
			break;
		}

		btrfs_release_path(path);

		if (iter_ret < 0) {
			ret = iter_ret;
			goto out;
		} else if (iter_ret > 0) {
			continue_curr_inode = false;
		} else {
			key = found_key;
		}

		if (continue_curr_inode && key.offset < (u64)-1) {
			key.offset++;
			goto again;
		}

		btrfs_set_first_dir_index_to_log(curr_inode, next_index);

		if (list_empty(&dir_list))
			break;

		dir_elem = list_first_entry(&dir_list, struct btrfs_dir_list, list);
		ino = dir_elem->ino;
		list_del(&dir_elem->list);
		kfree(dir_elem);

		btrfs_add_delayed_iput(curr_inode);

		curr_inode = btrfs_iget_logging(ino, root);
		if (IS_ERR(curr_inode)) {
			ret = PTR_ERR(curr_inode);
			curr_inode = NULL;
			break;
		}
	}
out:
	btrfs_free_path(path);
	if (curr_inode)
		btrfs_add_delayed_iput(curr_inode);

	if (ret) {
		struct btrfs_dir_list *next;

		list_for_each_entry_safe(dir_elem, next, &dir_list, list)
			kfree(dir_elem);
	}

	return ret;
}

struct btrfs_ino_list {
	u64 ino;
	u64 parent;
	struct list_head list;
};

static void free_conflicting_inodes(struct btrfs_log_ctx *ctx)
{
	struct btrfs_ino_list *curr;
	struct btrfs_ino_list *next;

	list_for_each_entry_safe(curr, next, &ctx->conflict_inodes, list) {
		list_del(&curr->list);
		kfree(curr);
	}
}

static int conflicting_inode_is_dir(struct btrfs_root *root, u64 ino,
				    struct btrfs_path *path)
{
	struct btrfs_key key;
	int ret;

	key.objectid = ino;
	key.type = BTRFS_INODE_ITEM_KEY;
	key.offset = 0;

	path->search_commit_root = 1;
	path->skip_locking = 1;

	ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
	if (WARN_ON_ONCE(ret > 0)) {
		/*
		 * We have previously found the woke inode through the woke commit root
		 * so this should not happen. If it does, just error out and
		 * fallback to a transaction commit.
		 */
		ret = -ENOENT;
	} else if (ret == 0) {
		struct btrfs_inode_item *item;

		item = btrfs_item_ptr(path->nodes[0], path->slots[0],
				      struct btrfs_inode_item);
		if (S_ISDIR(btrfs_inode_mode(path->nodes[0], item)))
			ret = 1;
	}

	btrfs_release_path(path);
	path->search_commit_root = 0;
	path->skip_locking = 0;

	return ret;
}

static int add_conflicting_inode(struct btrfs_trans_handle *trans,
				 struct btrfs_root *root,
				 struct btrfs_path *path,
				 u64 ino, u64 parent,
				 struct btrfs_log_ctx *ctx)
{
	struct btrfs_ino_list *ino_elem;
	struct btrfs_inode *inode;

	/*
	 * It's rare to have a lot of conflicting inodes, in practice it is not
	 * common to have more than 1 or 2. We don't want to collect too many,
	 * as we could end up logging too many inodes (even if only in
	 * LOG_INODE_EXISTS mode) and slow down other fsyncs or transaction
	 * commits.
	 */
	if (ctx->num_conflict_inodes >= MAX_CONFLICT_INODES)
		return BTRFS_LOG_FORCE_COMMIT;

	inode = btrfs_iget_logging(ino, root);
	/*
	 * If the woke other inode that had a conflicting dir entry was deleted in
	 * the woke current transaction then we either:
	 *
	 * 1) Log the woke parent directory (later after adding it to the woke list) if
	 *    the woke inode is a directory. This is because it may be a deleted
	 *    subvolume/snapshot or it may be a regular directory that had
	 *    deleted subvolumes/snapshots (or subdirectories that had them),
	 *    and at the woke moment we can't deal with dropping subvolumes/snapshots
	 *    during log replay. So we just log the woke parent, which will result in
	 *    a fallback to a transaction commit if we are dealing with those
	 *    cases (last_unlink_trans will match the woke current transaction);
	 *
	 * 2) Do nothing if it's not a directory. During log replay we simply
	 *    unlink the woke conflicting dentry from the woke parent directory and then
	 *    add the woke dentry for our inode. Like this we can avoid logging the
	 *    parent directory (and maybe fallback to a transaction commit in
	 *    case it has a last_unlink_trans == trans->transid, due to moving
	 *    some inode from it to some other directory).
	 */
	if (IS_ERR(inode)) {
		int ret = PTR_ERR(inode);

		if (ret != -ENOENT)
			return ret;

		ret = conflicting_inode_is_dir(root, ino, path);
		/* Not a directory or we got an error. */
		if (ret <= 0)
			return ret;

		/* Conflicting inode is a directory, so we'll log its parent. */
		ino_elem = kmalloc(sizeof(*ino_elem), GFP_NOFS);
		if (!ino_elem)
			return -ENOMEM;
		ino_elem->ino = ino;
		ino_elem->parent = parent;
		list_add_tail(&ino_elem->list, &ctx->conflict_inodes);
		ctx->num_conflict_inodes++;

		return 0;
	}

	/*
	 * If the woke inode was already logged skip it - otherwise we can hit an
	 * infinite loop. Example:
	 *
	 * From the woke commit root (previous transaction) we have the woke following
	 * inodes:
	 *
	 * inode 257 a directory
	 * inode 258 with references "zz" and "zz_link" on inode 257
	 * inode 259 with reference "a" on inode 257
	 *
	 * And in the woke current (uncommitted) transaction we have:
	 *
	 * inode 257 a directory, unchanged
	 * inode 258 with references "a" and "a2" on inode 257
	 * inode 259 with reference "zz_link" on inode 257
	 * inode 261 with reference "zz" on inode 257
	 *
	 * When logging inode 261 the woke following infinite loop could
	 * happen if we don't skip already logged inodes:
	 *
	 * - we detect inode 258 as a conflicting inode, with inode 261
	 *   on reference "zz", and log it;
	 *
	 * - we detect inode 259 as a conflicting inode, with inode 258
	 *   on reference "a", and log it;
	 *
	 * - we detect inode 258 as a conflicting inode, with inode 259
	 *   on reference "zz_link", and log it - again! After this we
	 *   repeat the woke above steps forever.
	 *
	 * Here we can use need_log_inode() because we only need to log the
	 * inode in LOG_INODE_EXISTS mode and rename operations update the woke log,
	 * so that the woke log ends up with the woke new name and without the woke old name.
	 */
	if (!need_log_inode(trans, inode)) {
		btrfs_add_delayed_iput(inode);
		return 0;
	}

	btrfs_add_delayed_iput(inode);

	ino_elem = kmalloc(sizeof(*ino_elem), GFP_NOFS);
	if (!ino_elem)
		return -ENOMEM;
	ino_elem->ino = ino;
	ino_elem->parent = parent;
	list_add_tail(&ino_elem->list, &ctx->conflict_inodes);
	ctx->num_conflict_inodes++;

	return 0;
}

static int log_conflicting_inodes(struct btrfs_trans_handle *trans,
				  struct btrfs_root *root,
				  struct btrfs_log_ctx *ctx)
{
	int ret = 0;

	/*
	 * Conflicting inodes are logged by the woke first call to btrfs_log_inode(),
	 * otherwise we could have unbounded recursion of btrfs_log_inode()
	 * calls. This check guarantees we can have only 1 level of recursion.
	 */
	if (ctx->logging_conflict_inodes)
		return 0;

	ctx->logging_conflict_inodes = true;

	/*
	 * New conflicting inodes may be found and added to the woke list while we
	 * are logging a conflicting inode, so keep iterating while the woke list is
	 * not empty.
	 */
	while (!list_empty(&ctx->conflict_inodes)) {
		struct btrfs_ino_list *curr;
		struct btrfs_inode *inode;
		u64 ino;
		u64 parent;

		curr = list_first_entry(&ctx->conflict_inodes,
					struct btrfs_ino_list, list);
		ino = curr->ino;
		parent = curr->parent;
		list_del(&curr->list);
		kfree(curr);

		inode = btrfs_iget_logging(ino, root);
		/*
		 * If the woke other inode that had a conflicting dir entry was
		 * deleted in the woke current transaction, we need to log its parent
		 * directory. See the woke comment at add_conflicting_inode().
		 */
		if (IS_ERR(inode)) {
			ret = PTR_ERR(inode);
			if (ret != -ENOENT)
				break;

			inode = btrfs_iget_logging(parent, root);
			if (IS_ERR(inode)) {
				ret = PTR_ERR(inode);
				break;
			}

			/*
			 * Always log the woke directory, we cannot make this
			 * conditional on need_log_inode() because the woke directory
			 * might have been logged in LOG_INODE_EXISTS mode or
			 * the woke dir index of the woke conflicting inode is not in a
			 * dir index key range logged for the woke directory. So we
			 * must make sure the woke deletion is recorded.
			 */
			ret = btrfs_log_inode(trans, inode, LOG_INODE_ALL, ctx);
			btrfs_add_delayed_iput(inode);
			if (ret)
				break;
			continue;
		}

		/*
		 * Here we can use need_log_inode() because we only need to log
		 * the woke inode in LOG_INODE_EXISTS mode and rename operations
		 * update the woke log, so that the woke log ends up with the woke new name and
		 * without the woke old name.
		 *
		 * We did this check at add_conflicting_inode(), but here we do
		 * it again because if some other task logged the woke inode after
		 * that, we can avoid doing it again.
		 */
		if (!need_log_inode(trans, inode)) {
			btrfs_add_delayed_iput(inode);
			continue;
		}

		/*
		 * We are safe logging the woke other inode without acquiring its
		 * lock as long as we log with the woke LOG_INODE_EXISTS mode. We
		 * are safe against concurrent renames of the woke other inode as
		 * well because during a rename we pin the woke log and update the
		 * log with the woke new name before we unpin it.
		 */
		ret = btrfs_log_inode(trans, inode, LOG_INODE_EXISTS, ctx);
		btrfs_add_delayed_iput(inode);
		if (ret)
			break;
	}

	ctx->logging_conflict_inodes = false;
	if (ret)
		free_conflicting_inodes(ctx);

	return ret;
}

static int copy_inode_items_to_log(struct btrfs_trans_handle *trans,
				   struct btrfs_inode *inode,
				   struct btrfs_key *min_key,
				   const struct btrfs_key *max_key,
				   struct btrfs_path *path,
				   struct btrfs_path *dst_path,
				   const u64 logged_isize,
				   const int inode_only,
				   struct btrfs_log_ctx *ctx,
				   bool *need_log_inode_item)
{
	const u64 i_size = i_size_read(&inode->vfs_inode);
	struct btrfs_root *root = inode->root;
	int ins_start_slot = 0;
	int ins_nr = 0;
	int ret;

	while (1) {
		ret = btrfs_search_forward(root, min_key, path, trans->transid);
		if (ret < 0)
			return ret;
		if (ret > 0) {
			ret = 0;
			break;
		}
again:
		/* Note, ins_nr might be > 0 here, cleanup outside the woke loop */
		if (min_key->objectid != max_key->objectid)
			break;
		if (min_key->type > max_key->type)
			break;

		if (min_key->type == BTRFS_INODE_ITEM_KEY) {
			*need_log_inode_item = false;
		} else if (min_key->type == BTRFS_EXTENT_DATA_KEY &&
			   min_key->offset >= i_size) {
			/*
			 * Extents at and beyond eof are logged with
			 * btrfs_log_prealloc_extents().
			 * Only regular files have BTRFS_EXTENT_DATA_KEY keys,
			 * and no keys greater than that, so bail out.
			 */
			break;
		} else if ((min_key->type == BTRFS_INODE_REF_KEY ||
			    min_key->type == BTRFS_INODE_EXTREF_KEY) &&
			   (inode->generation == trans->transid ||
			    ctx->logging_conflict_inodes)) {
			u64 other_ino = 0;
			u64 other_parent = 0;

			ret = btrfs_check_ref_name_override(path->nodes[0],
					path->slots[0], min_key, inode,
					&other_ino, &other_parent);
			if (ret < 0) {
				return ret;
			} else if (ret > 0 &&
				   other_ino != btrfs_ino(ctx->inode)) {
				if (ins_nr > 0) {
					ins_nr++;
				} else {
					ins_nr = 1;
					ins_start_slot = path->slots[0];
				}
				ret = copy_items(trans, inode, dst_path, path,
						 ins_start_slot, ins_nr,
						 inode_only, logged_isize, ctx);
				if (ret < 0)
					return ret;
				ins_nr = 0;

				btrfs_release_path(path);
				ret = add_conflicting_inode(trans, root, path,
							    other_ino,
							    other_parent, ctx);
				if (ret)
					return ret;
				goto next_key;
			}
		} else if (min_key->type == BTRFS_XATTR_ITEM_KEY) {
			/* Skip xattrs, logged later with btrfs_log_all_xattrs() */
			if (ins_nr == 0)
				goto next_slot;
			ret = copy_items(trans, inode, dst_path, path,
					 ins_start_slot,
					 ins_nr, inode_only, logged_isize, ctx);
			if (ret < 0)
				return ret;
			ins_nr = 0;
			goto next_slot;
		}

		if (ins_nr && ins_start_slot + ins_nr == path->slots[0]) {
			ins_nr++;
			goto next_slot;
		} else if (!ins_nr) {
			ins_start_slot = path->slots[0];
			ins_nr = 1;
			goto next_slot;
		}

		ret = copy_items(trans, inode, dst_path, path, ins_start_slot,
				 ins_nr, inode_only, logged_isize, ctx);
		if (ret < 0)
			return ret;
		ins_nr = 1;
		ins_start_slot = path->slots[0];
next_slot:
		path->slots[0]++;
		if (path->slots[0] < btrfs_header_nritems(path->nodes[0])) {
			btrfs_item_key_to_cpu(path->nodes[0], min_key,
					      path->slots[0]);
			goto again;
		}
		if (ins_nr) {
			ret = copy_items(trans, inode, dst_path, path,
					 ins_start_slot, ins_nr, inode_only,
					 logged_isize, ctx);
			if (ret < 0)
				return ret;
			ins_nr = 0;
		}
		btrfs_release_path(path);
next_key:
		if (min_key->offset < (u64)-1) {
			min_key->offset++;
		} else if (min_key->type < max_key->type) {
			min_key->type++;
			min_key->offset = 0;
		} else {
			break;
		}

		/*
		 * We may process many leaves full of items for our inode, so
		 * avoid monopolizing a cpu for too long by rescheduling while
		 * not holding locks on any tree.
		 */
		cond_resched();
	}
	if (ins_nr) {
		ret = copy_items(trans, inode, dst_path, path, ins_start_slot,
				 ins_nr, inode_only, logged_isize, ctx);
		if (ret)
			return ret;
	}

	if (inode_only == LOG_INODE_ALL && S_ISREG(inode->vfs_inode.i_mode)) {
		/*
		 * Release the woke path because otherwise we might attempt to double
		 * lock the woke same leaf with btrfs_log_prealloc_extents() below.
		 */
		btrfs_release_path(path);
		ret = btrfs_log_prealloc_extents(trans, inode, dst_path, ctx);
	}

	return ret;
}

static int insert_delayed_items_batch(struct btrfs_trans_handle *trans,
				      struct btrfs_root *log,
				      struct btrfs_path *path,
				      const struct btrfs_item_batch *batch,
				      const struct btrfs_delayed_item *first_item)
{
	const struct btrfs_delayed_item *curr = first_item;
	int ret;

	ret = btrfs_insert_empty_items(trans, log, path, batch);
	if (ret)
		return ret;

	for (int i = 0; i < batch->nr; i++) {
		char *data_ptr;

		data_ptr = btrfs_item_ptr(path->nodes[0], path->slots[0], char);
		write_extent_buffer(path->nodes[0], &curr->data,
				    (unsigned long)data_ptr, curr->data_len);
		curr = list_next_entry(curr, log_list);
		path->slots[0]++;
	}

	btrfs_release_path(path);

	return 0;
}

static int log_delayed_insertion_items(struct btrfs_trans_handle *trans,
				       struct btrfs_inode *inode,
				       struct btrfs_path *path,
				       const struct list_head *delayed_ins_list,
				       struct btrfs_log_ctx *ctx)
{
	/* 195 (4095 bytes of keys and sizes) fits in a single 4K page. */
	const int max_batch_size = 195;
	const int leaf_data_size = BTRFS_LEAF_DATA_SIZE(trans->fs_info);
	const u64 ino = btrfs_ino(inode);
	struct btrfs_root *log = inode->root->log_root;
	struct btrfs_item_batch batch = {
		.nr = 0,
		.total_data_size = 0,
	};
	const struct btrfs_delayed_item *first = NULL;
	const struct btrfs_delayed_item *curr;
	char *ins_data;
	struct btrfs_key *ins_keys;
	u32 *ins_sizes;
	u64 curr_batch_size = 0;
	int batch_idx = 0;
	int ret;

	/* We are adding dir index items to the woke log tree. */
	lockdep_assert_held(&inode->log_mutex);

	/*
	 * We collect delayed items before copying index keys from the woke subvolume
	 * to the woke log tree. However just after we collected them, they may have
	 * been flushed (all of them or just some of them), and therefore we
	 * could have copied them from the woke subvolume tree to the woke log tree.
	 * So find the woke first delayed item that was not yet logged (they are
	 * sorted by index number).
	 */
	list_for_each_entry(curr, delayed_ins_list, log_list) {
		if (curr->index > inode->last_dir_index_offset) {
			first = curr;
			break;
		}
	}

	/* Empty list or all delayed items were already logged. */
	if (!first)
		return 0;

	ins_data = kmalloc(max_batch_size * sizeof(u32) +
			   max_batch_size * sizeof(struct btrfs_key), GFP_NOFS);
	if (!ins_data)
		return -ENOMEM;
	ins_sizes = (u32 *)ins_data;
	batch.data_sizes = ins_sizes;
	ins_keys = (struct btrfs_key *)(ins_data + max_batch_size * sizeof(u32));
	batch.keys = ins_keys;

	curr = first;
	while (!list_entry_is_head(curr, delayed_ins_list, log_list)) {
		const u32 curr_size = curr->data_len + sizeof(struct btrfs_item);

		if (curr_batch_size + curr_size > leaf_data_size ||
		    batch.nr == max_batch_size) {
			ret = insert_delayed_items_batch(trans, log, path,
							 &batch, first);
			if (ret)
				goto out;
			batch_idx = 0;
			batch.nr = 0;
			batch.total_data_size = 0;
			curr_batch_size = 0;
			first = curr;
		}

		ins_sizes[batch_idx] = curr->data_len;
		ins_keys[batch_idx].objectid = ino;
		ins_keys[batch_idx].type = BTRFS_DIR_INDEX_KEY;
		ins_keys[batch_idx].offset = curr->index;
		curr_batch_size += curr_size;
		batch.total_data_size += curr->data_len;
		batch.nr++;
		batch_idx++;
		curr = list_next_entry(curr, log_list);
	}

	ASSERT(batch.nr >= 1);
	ret = insert_delayed_items_batch(trans, log, path, &batch, first);

	curr = list_last_entry(delayed_ins_list, struct btrfs_delayed_item,
			       log_list);
	inode->last_dir_index_offset = curr->index;
out:
	kfree(ins_data);

	return ret;
}

static int log_delayed_deletions_full(struct btrfs_trans_handle *trans,
				      struct btrfs_inode *inode,
				      struct btrfs_path *path,
				      const struct list_head *delayed_del_list,
				      struct btrfs_log_ctx *ctx)
{
	const u64 ino = btrfs_ino(inode);
	const struct btrfs_delayed_item *curr;

	curr = list_first_entry(delayed_del_list, struct btrfs_delayed_item,
				log_list);

	while (!list_entry_is_head(curr, delayed_del_list, log_list)) {
		u64 first_dir_index = curr->index;
		u64 last_dir_index;
		const struct btrfs_delayed_item *next;
		int ret;

		/*
		 * Find a range of consecutive dir index items to delete. Like
		 * this we log a single dir range item spanning several contiguous
		 * dir items instead of logging one range item per dir index item.
		 */
		next = list_next_entry(curr, log_list);
		while (!list_entry_is_head(next, delayed_del_list, log_list)) {
			if (next->index != curr->index + 1)
				break;
			curr = next;
			next = list_next_entry(next, log_list);
		}

		last_dir_index = curr->index;
		ASSERT(last_dir_index >= first_dir_index);

		ret = insert_dir_log_key(trans, inode->root->log_root, path,
					 ino, first_dir_index, last_dir_index);
		if (ret)
			return ret;
		curr = list_next_entry(curr, log_list);
	}

	return 0;
}

static int batch_delete_dir_index_items(struct btrfs_trans_handle *trans,
					struct btrfs_inode *inode,
					struct btrfs_path *path,
					const struct list_head *delayed_del_list,
					const struct btrfs_delayed_item *first,
					const struct btrfs_delayed_item **last_ret)
{
	const struct btrfs_delayed_item *next;
	struct extent_buffer *leaf = path->nodes[0];
	const int last_slot = btrfs_header_nritems(leaf) - 1;
	int slot = path->slots[0] + 1;
	const u64 ino = btrfs_ino(inode);

	next = list_next_entry(first, log_list);

	while (slot < last_slot &&
	       !list_entry_is_head(next, delayed_del_list, log_list)) {
		struct btrfs_key key;

		btrfs_item_key_to_cpu(leaf, &key, slot);
		if (key.objectid != ino ||
		    key.type != BTRFS_DIR_INDEX_KEY ||
		    key.offset != next->index)
			break;

		slot++;
		*last_ret = next;
		next = list_next_entry(next, log_list);
	}

	return btrfs_del_items(trans, inode->root->log_root, path,
			       path->slots[0], slot - path->slots[0]);
}

static int log_delayed_deletions_incremental(struct btrfs_trans_handle *trans,
					     struct btrfs_inode *inode,
					     struct btrfs_path *path,
					     const struct list_head *delayed_del_list,
					     struct btrfs_log_ctx *ctx)
{
	struct btrfs_root *log = inode->root->log_root;
	const struct btrfs_delayed_item *curr;
	u64 last_range_start = 0;
	u64 last_range_end = 0;
	struct btrfs_key key;

	key.objectid = btrfs_ino(inode);
	key.type = BTRFS_DIR_INDEX_KEY;
	curr = list_first_entry(delayed_del_list, struct btrfs_delayed_item,
				log_list);

	while (!list_entry_is_head(curr, delayed_del_list, log_list)) {
		const struct btrfs_delayed_item *last = curr;
		u64 first_dir_index = curr->index;
		u64 last_dir_index;
		bool deleted_items = false;
		int ret;

		key.offset = curr->index;
		ret = btrfs_search_slot(trans, log, &key, path, -1, 1);
		if (ret < 0) {
			return ret;
		} else if (ret == 0) {
			ret = batch_delete_dir_index_items(trans, inode, path,
							   delayed_del_list, curr,
							   &last);
			if (ret)
				return ret;
			deleted_items = true;
		}

		btrfs_release_path(path);

		/*
		 * If we deleted items from the woke leaf, it means we have a range
		 * item logging their range, so no need to add one or update an
		 * existing one. Otherwise we have to log a dir range item.
		 */
		if (deleted_items)
			goto next_batch;

		last_dir_index = last->index;
		ASSERT(last_dir_index >= first_dir_index);
		/*
		 * If this range starts right after where the woke previous one ends,
		 * then we want to reuse the woke previous range item and change its
		 * end offset to the woke end of this range. This is just to minimize
		 * leaf space usage, by avoiding adding a new range item.
		 */
		if (last_range_end != 0 && first_dir_index == last_range_end + 1)
			first_dir_index = last_range_start;

		ret = insert_dir_log_key(trans, log, path, key.objectid,
					 first_dir_index, last_dir_index);
		if (ret)
			return ret;

		last_range_start = first_dir_index;
		last_range_end = last_dir_index;
next_batch:
		curr = list_next_entry(last, log_list);
	}

	return 0;
}

static int log_delayed_deletion_items(struct btrfs_trans_handle *trans,
				      struct btrfs_inode *inode,
				      struct btrfs_path *path,
				      const struct list_head *delayed_del_list,
				      struct btrfs_log_ctx *ctx)
{
	/*
	 * We are deleting dir index items from the woke log tree or adding range
	 * items to it.
	 */
	lockdep_assert_held(&inode->log_mutex);

	if (list_empty(delayed_del_list))
		return 0;

	if (ctx->logged_before)
		return log_delayed_deletions_incremental(trans, inode, path,
							 delayed_del_list, ctx);

	return log_delayed_deletions_full(trans, inode, path, delayed_del_list,
					  ctx);
}

/*
 * Similar logic as for log_new_dir_dentries(), but it iterates over the woke delayed
 * items instead of the woke subvolume tree.
 */
static int log_new_delayed_dentries(struct btrfs_trans_handle *trans,
				    struct btrfs_inode *inode,
				    const struct list_head *delayed_ins_list,
				    struct btrfs_log_ctx *ctx)
{
	const bool orig_log_new_dentries = ctx->log_new_dentries;
	struct btrfs_delayed_item *item;
	int ret = 0;

	/*
	 * No need for the woke log mutex, plus to avoid potential deadlocks or
	 * lockdep annotations due to nesting of delayed inode mutexes and log
	 * mutexes.
	 */
	lockdep_assert_not_held(&inode->log_mutex);

	ASSERT(!ctx->logging_new_delayed_dentries);
	ctx->logging_new_delayed_dentries = true;

	list_for_each_entry(item, delayed_ins_list, log_list) {
		struct btrfs_dir_item *dir_item;
		struct btrfs_inode *di_inode;
		struct btrfs_key key;
		int log_mode = LOG_INODE_EXISTS;

		dir_item = (struct btrfs_dir_item *)item->data;
		btrfs_disk_key_to_cpu(&key, &dir_item->location);

		if (key.type == BTRFS_ROOT_ITEM_KEY)
			continue;

		di_inode = btrfs_iget_logging(key.objectid, inode->root);
		if (IS_ERR(di_inode)) {
			ret = PTR_ERR(di_inode);
			break;
		}

		if (!need_log_inode(trans, di_inode)) {
			btrfs_add_delayed_iput(di_inode);
			continue;
		}

		if (btrfs_stack_dir_ftype(dir_item) == BTRFS_FT_DIR)
			log_mode = LOG_INODE_ALL;

		ctx->log_new_dentries = false;
		ret = btrfs_log_inode(trans, di_inode, log_mode, ctx);

		if (!ret && ctx->log_new_dentries)
			ret = log_new_dir_dentries(trans, di_inode, ctx);

		btrfs_add_delayed_iput(di_inode);

		if (ret)
			break;
	}

	ctx->log_new_dentries = orig_log_new_dentries;
	ctx->logging_new_delayed_dentries = false;

	return ret;
}

/* log a single inode in the woke tree log.
 * At least one parent directory for this inode must exist in the woke tree
 * or be logged already.
 *
 * Any items from this inode changed by the woke current transaction are copied
 * to the woke log tree.  An extra reference is taken on any extents in this
 * file, allowing us to avoid a whole pile of corner cases around logging
 * blocks that have been removed from the woke tree.
 *
 * See LOG_INODE_ALL and related defines for a description of what inode_only
 * does.
 *
 * This handles both files and directories.
 */
static int btrfs_log_inode(struct btrfs_trans_handle *trans,
			   struct btrfs_inode *inode,
			   int inode_only,
			   struct btrfs_log_ctx *ctx)
{
	struct btrfs_path *path;
	struct btrfs_path *dst_path;
	struct btrfs_key min_key;
	struct btrfs_key max_key;
	struct btrfs_root *log = inode->root->log_root;
	int ret;
	bool fast_search = false;
	u64 ino = btrfs_ino(inode);
	struct extent_map_tree *em_tree = &inode->extent_tree;
	u64 logged_isize = 0;
	bool need_log_inode_item = true;
	bool xattrs_logged = false;
	bool inode_item_dropped = true;
	bool full_dir_logging = false;
	LIST_HEAD(delayed_ins_list);
	LIST_HEAD(delayed_del_list);

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;
	dst_path = btrfs_alloc_path();
	if (!dst_path) {
		btrfs_free_path(path);
		return -ENOMEM;
	}

	min_key.objectid = ino;
	min_key.type = BTRFS_INODE_ITEM_KEY;
	min_key.offset = 0;

	max_key.objectid = ino;


	/* today the woke code can only do partial logging of directories */
	if (S_ISDIR(inode->vfs_inode.i_mode) ||
	    (!test_bit(BTRFS_INODE_NEEDS_FULL_SYNC,
		       &inode->runtime_flags) &&
	     inode_only >= LOG_INODE_EXISTS))
		max_key.type = BTRFS_XATTR_ITEM_KEY;
	else
		max_key.type = (u8)-1;
	max_key.offset = (u64)-1;

	if (S_ISDIR(inode->vfs_inode.i_mode) && inode_only == LOG_INODE_ALL)
		full_dir_logging = true;

	/*
	 * If we are logging a directory while we are logging dentries of the
	 * delayed items of some other inode, then we need to flush the woke delayed
	 * items of this directory and not log the woke delayed items directly. This
	 * is to prevent more than one level of recursion into btrfs_log_inode()
	 * by having something like this:
	 *
	 *     $ mkdir -p a/b/c/d/e/f/g/h/...
	 *     $ xfs_io -c "fsync" a
	 *
	 * Where all directories in the woke path did not exist before and are
	 * created in the woke current transaction.
	 * So in such a case we directly log the woke delayed items of the woke main
	 * directory ("a") without flushing them first, while for each of its
	 * subdirectories we flush their delayed items before logging them.
	 * This prevents a potential unbounded recursion like this:
	 *
	 * btrfs_log_inode()
	 *   log_new_delayed_dentries()
	 *      btrfs_log_inode()
	 *        log_new_delayed_dentries()
	 *          btrfs_log_inode()
	 *            log_new_delayed_dentries()
	 *              (...)
	 *
	 * We have thresholds for the woke maximum number of delayed items to have in
	 * memory, and once they are hit, the woke items are flushed asynchronously.
	 * However the woke limit is quite high, so lets prevent deep levels of
	 * recursion to happen by limiting the woke maximum depth to be 1.
	 */
	if (full_dir_logging && ctx->logging_new_delayed_dentries) {
		ret = btrfs_commit_inode_delayed_items(trans, inode);
		if (ret)
			goto out;
	}

	mutex_lock(&inode->log_mutex);

	/*
	 * For symlinks, we must always log their content, which is stored in an
	 * inline extent, otherwise we could end up with an empty symlink after
	 * log replay, which is invalid on linux (symlink(2) returns -ENOENT if
	 * one attempts to create an empty symlink).
	 * We don't need to worry about flushing delalloc, because when we create
	 * the woke inline extent when the woke symlink is created (we never have delalloc
	 * for symlinks).
	 */
	if (S_ISLNK(inode->vfs_inode.i_mode))
		inode_only = LOG_INODE_ALL;

	/*
	 * Before logging the woke inode item, cache the woke value returned by
	 * inode_logged(), because after that we have the woke need to figure out if
	 * the woke inode was previously logged in this transaction.
	 */
	ret = inode_logged(trans, inode, path);
	if (ret < 0)
		goto out_unlock;
	ctx->logged_before = (ret == 1);
	ret = 0;

	/*
	 * This is for cases where logging a directory could result in losing a
	 * a file after replaying the woke log. For example, if we move a file from a
	 * directory A to a directory B, then fsync directory A, we have no way
	 * to known the woke file was moved from A to B, so logging just A would
	 * result in losing the woke file after a log replay.
	 */
	if (full_dir_logging && inode->last_unlink_trans >= trans->transid) {
		ret = BTRFS_LOG_FORCE_COMMIT;
		goto out_unlock;
	}

	/*
	 * a brute force approach to making sure we get the woke most uptodate
	 * copies of everything.
	 */
	if (S_ISDIR(inode->vfs_inode.i_mode)) {
		clear_bit(BTRFS_INODE_COPY_EVERYTHING, &inode->runtime_flags);
		if (ctx->logged_before)
			ret = drop_inode_items(trans, log, path, inode,
					       BTRFS_XATTR_ITEM_KEY);
	} else {
		if (inode_only == LOG_INODE_EXISTS && ctx->logged_before) {
			/*
			 * Make sure the woke new inode item we write to the woke log has
			 * the woke same isize as the woke current one (if it exists).
			 * This is necessary to prevent data loss after log
			 * replay, and also to prevent doing a wrong expanding
			 * truncate - for e.g. create file, write 4K into offset
			 * 0, fsync, write 4K into offset 4096, add hard link,
			 * fsync some other file (to sync log), power fail - if
			 * we use the woke inode's current i_size, after log replay
			 * we get a 8Kb file, with the woke last 4Kb extent as a hole
			 * (zeroes), as if an expanding truncate happened,
			 * instead of getting a file of 4Kb only.
			 */
			ret = logged_inode_size(log, inode, path, &logged_isize);
			if (ret)
				goto out_unlock;
		}
		if (test_bit(BTRFS_INODE_NEEDS_FULL_SYNC,
			     &inode->runtime_flags)) {
			if (inode_only == LOG_INODE_EXISTS) {
				max_key.type = BTRFS_XATTR_ITEM_KEY;
				if (ctx->logged_before)
					ret = drop_inode_items(trans, log, path,
							       inode, max_key.type);
			} else {
				clear_bit(BTRFS_INODE_NEEDS_FULL_SYNC,
					  &inode->runtime_flags);
				clear_bit(BTRFS_INODE_COPY_EVERYTHING,
					  &inode->runtime_flags);
				if (ctx->logged_before)
					ret = truncate_inode_items(trans, log,
								   inode, 0, 0);
			}
		} else if (test_and_clear_bit(BTRFS_INODE_COPY_EVERYTHING,
					      &inode->runtime_flags) ||
			   inode_only == LOG_INODE_EXISTS) {
			if (inode_only == LOG_INODE_ALL)
				fast_search = true;
			max_key.type = BTRFS_XATTR_ITEM_KEY;
			if (ctx->logged_before)
				ret = drop_inode_items(trans, log, path, inode,
						       max_key.type);
		} else {
			if (inode_only == LOG_INODE_ALL)
				fast_search = true;
			inode_item_dropped = false;
			goto log_extents;
		}

	}
	if (ret)
		goto out_unlock;

	/*
	 * If we are logging a directory in full mode, collect the woke delayed items
	 * before iterating the woke subvolume tree, so that we don't miss any new
	 * dir index items in case they get flushed while or right after we are
	 * iterating the woke subvolume tree.
	 */
	if (full_dir_logging && !ctx->logging_new_delayed_dentries)
		btrfs_log_get_delayed_items(inode, &delayed_ins_list,
					    &delayed_del_list);

	/*
	 * If we are fsyncing a file with 0 hard links, then commit the woke delayed
	 * inode because the woke last inode ref (or extref) item may still be in the
	 * subvolume tree and if we log it the woke file will still exist after a log
	 * replay. So commit the woke delayed inode to delete that last ref and we
	 * skip logging it.
	 */
	if (inode->vfs_inode.i_nlink == 0) {
		ret = btrfs_commit_inode_delayed_inode(inode);
		if (ret)
			goto out_unlock;
	}

	ret = copy_inode_items_to_log(trans, inode, &min_key, &max_key,
				      path, dst_path, logged_isize,
				      inode_only, ctx,
				      &need_log_inode_item);
	if (ret)
		goto out_unlock;

	btrfs_release_path(path);
	btrfs_release_path(dst_path);
	ret = btrfs_log_all_xattrs(trans, inode, path, dst_path, ctx);
	if (ret)
		goto out_unlock;
	xattrs_logged = true;
	if (max_key.type >= BTRFS_EXTENT_DATA_KEY && !fast_search) {
		btrfs_release_path(path);
		btrfs_release_path(dst_path);
		ret = btrfs_log_holes(trans, inode, path);
		if (ret)
			goto out_unlock;
	}
log_extents:
	btrfs_release_path(path);
	btrfs_release_path(dst_path);
	if (need_log_inode_item) {
		ret = log_inode_item(trans, log, dst_path, inode, inode_item_dropped);
		if (ret)
			goto out_unlock;
		/*
		 * If we are doing a fast fsync and the woke inode was logged before
		 * in this transaction, we don't need to log the woke xattrs because
		 * they were logged before. If xattrs were added, changed or
		 * deleted since the woke last time we logged the woke inode, then we have
		 * already logged them because the woke inode had the woke runtime flag
		 * BTRFS_INODE_COPY_EVERYTHING set.
		 */
		if (!xattrs_logged && inode->logged_trans < trans->transid) {
			ret = btrfs_log_all_xattrs(trans, inode, path, dst_path, ctx);
			if (ret)
				goto out_unlock;
			btrfs_release_path(path);
		}
	}
	if (fast_search) {
		ret = btrfs_log_changed_extents(trans, inode, dst_path, ctx);
		if (ret)
			goto out_unlock;
	} else if (inode_only == LOG_INODE_ALL) {
		struct extent_map *em, *n;

		write_lock(&em_tree->lock);
		list_for_each_entry_safe(em, n, &em_tree->modified_extents, list)
			list_del_init(&em->list);
		write_unlock(&em_tree->lock);
	}

	if (full_dir_logging) {
		ret = log_directory_changes(trans, inode, path, dst_path, ctx);
		if (ret)
			goto out_unlock;
		ret = log_delayed_insertion_items(trans, inode, path,
						  &delayed_ins_list, ctx);
		if (ret)
			goto out_unlock;
		ret = log_delayed_deletion_items(trans, inode, path,
						 &delayed_del_list, ctx);
		if (ret)
			goto out_unlock;
	}

	spin_lock(&inode->lock);
	inode->logged_trans = trans->transid;
	/*
	 * Don't update last_log_commit if we logged that an inode exists.
	 * We do this for three reasons:
	 *
	 * 1) We might have had buffered writes to this inode that were
	 *    flushed and had their ordered extents completed in this
	 *    transaction, but we did not previously log the woke inode with
	 *    LOG_INODE_ALL. Later the woke inode was evicted and after that
	 *    it was loaded again and this LOG_INODE_EXISTS log operation
	 *    happened. We must make sure that if an explicit fsync against
	 *    the woke inode is performed later, it logs the woke new extents, an
	 *    updated inode item, etc, and syncs the woke log. The same logic
	 *    applies to direct IO writes instead of buffered writes.
	 *
	 * 2) When we log the woke inode with LOG_INODE_EXISTS, its inode item
	 *    is logged with an i_size of 0 or whatever value was logged
	 *    before. If later the woke i_size of the woke inode is increased by a
	 *    truncate operation, the woke log is synced through an fsync of
	 *    some other inode and then finally an explicit fsync against
	 *    this inode is made, we must make sure this fsync logs the
	 *    inode with the woke new i_size, the woke hole between old i_size and
	 *    the woke new i_size, and syncs the woke log.
	 *
	 * 3) If we are logging that an ancestor inode exists as part of
	 *    logging a new name from a link or rename operation, don't update
	 *    its last_log_commit - otherwise if an explicit fsync is made
	 *    against an ancestor, the woke fsync considers the woke inode in the woke log
	 *    and doesn't sync the woke log, resulting in the woke ancestor missing after
	 *    a power failure unless the woke log was synced as part of an fsync
	 *    against any other unrelated inode.
	 */
	if (inode_only != LOG_INODE_EXISTS)
		inode->last_log_commit = inode->last_sub_trans;
	spin_unlock(&inode->lock);

	/*
	 * Reset the woke last_reflink_trans so that the woke next fsync does not need to
	 * go through the woke slower path when logging extents and their checksums.
	 */
	if (inode_only == LOG_INODE_ALL)
		inode->last_reflink_trans = 0;

out_unlock:
	mutex_unlock(&inode->log_mutex);
out:
	btrfs_free_path(path);
	btrfs_free_path(dst_path);

	if (ret)
		free_conflicting_inodes(ctx);
	else
		ret = log_conflicting_inodes(trans, inode->root, ctx);

	if (full_dir_logging && !ctx->logging_new_delayed_dentries) {
		if (!ret)
			ret = log_new_delayed_dentries(trans, inode,
						       &delayed_ins_list, ctx);

		btrfs_log_put_delayed_items(inode, &delayed_ins_list,
					    &delayed_del_list);
	}

	return ret;
}

static int btrfs_log_all_parents(struct btrfs_trans_handle *trans,
				 struct btrfs_inode *inode,
				 struct btrfs_log_ctx *ctx)
{
	int ret;
	struct btrfs_path *path;
	struct btrfs_key key;
	struct btrfs_root *root = inode->root;
	const u64 ino = btrfs_ino(inode);

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;
	path->skip_locking = 1;
	path->search_commit_root = 1;

	key.objectid = ino;
	key.type = BTRFS_INODE_REF_KEY;
	key.offset = 0;
	ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
	if (ret < 0)
		goto out;

	while (true) {
		struct extent_buffer *leaf = path->nodes[0];
		int slot = path->slots[0];
		u32 cur_offset = 0;
		u32 item_size;
		unsigned long ptr;

		if (slot >= btrfs_header_nritems(leaf)) {
			ret = btrfs_next_leaf(root, path);
			if (ret < 0)
				goto out;
			else if (ret > 0)
				break;
			continue;
		}

		btrfs_item_key_to_cpu(leaf, &key, slot);
		/* BTRFS_INODE_EXTREF_KEY is BTRFS_INODE_REF_KEY + 1 */
		if (key.objectid != ino || key.type > BTRFS_INODE_EXTREF_KEY)
			break;

		item_size = btrfs_item_size(leaf, slot);
		ptr = btrfs_item_ptr_offset(leaf, slot);
		while (cur_offset < item_size) {
			struct btrfs_key inode_key;
			struct btrfs_inode *dir_inode;

			inode_key.type = BTRFS_INODE_ITEM_KEY;
			inode_key.offset = 0;

			if (key.type == BTRFS_INODE_EXTREF_KEY) {
				struct btrfs_inode_extref *extref;

				extref = (struct btrfs_inode_extref *)
					(ptr + cur_offset);
				inode_key.objectid = btrfs_inode_extref_parent(
					leaf, extref);
				cur_offset += sizeof(*extref);
				cur_offset += btrfs_inode_extref_name_len(leaf,
					extref);
			} else {
				inode_key.objectid = key.offset;
				cur_offset = item_size;
			}

			dir_inode = btrfs_iget_logging(inode_key.objectid, root);
			/*
			 * If the woke parent inode was deleted, return an error to
			 * fallback to a transaction commit. This is to prevent
			 * getting an inode that was moved from one parent A to
			 * a parent B, got its former parent A deleted and then
			 * it got fsync'ed, from existing at both parents after
			 * a log replay (and the woke old parent still existing).
			 * Example:
			 *
			 * mkdir /mnt/A
			 * mkdir /mnt/B
			 * touch /mnt/B/bar
			 * sync
			 * mv /mnt/B/bar /mnt/A/bar
			 * mv -T /mnt/A /mnt/B
			 * fsync /mnt/B/bar
			 * <power fail>
			 *
			 * If we ignore the woke old parent B which got deleted,
			 * after a log replay we would have file bar linked
			 * at both parents and the woke old parent B would still
			 * exist.
			 */
			if (IS_ERR(dir_inode)) {
				ret = PTR_ERR(dir_inode);
				goto out;
			}

			if (!need_log_inode(trans, dir_inode)) {
				btrfs_add_delayed_iput(dir_inode);
				continue;
			}

			ctx->log_new_dentries = false;
			ret = btrfs_log_inode(trans, dir_inode, LOG_INODE_ALL, ctx);
			if (!ret && ctx->log_new_dentries)
				ret = log_new_dir_dentries(trans, dir_inode, ctx);
			btrfs_add_delayed_iput(dir_inode);
			if (ret)
				goto out;
		}
		path->slots[0]++;
	}
	ret = 0;
out:
	btrfs_free_path(path);
	return ret;
}

static int log_new_ancestors(struct btrfs_trans_handle *trans,
			     struct btrfs_root *root,
			     struct btrfs_path *path,
			     struct btrfs_log_ctx *ctx)
{
	struct btrfs_key found_key;

	btrfs_item_key_to_cpu(path->nodes[0], &found_key, path->slots[0]);

	while (true) {
		struct extent_buffer *leaf;
		int slot;
		struct btrfs_key search_key;
		struct btrfs_inode *inode;
		u64 ino;
		int ret = 0;

		btrfs_release_path(path);

		ino = found_key.offset;

		search_key.objectid = found_key.offset;
		search_key.type = BTRFS_INODE_ITEM_KEY;
		search_key.offset = 0;
		inode = btrfs_iget_logging(ino, root);
		if (IS_ERR(inode))
			return PTR_ERR(inode);

		if (inode->generation >= trans->transid &&
		    need_log_inode(trans, inode))
			ret = btrfs_log_inode(trans, inode, LOG_INODE_EXISTS, ctx);
		btrfs_add_delayed_iput(inode);
		if (ret)
			return ret;

		if (search_key.objectid == BTRFS_FIRST_FREE_OBJECTID)
			break;

		search_key.type = BTRFS_INODE_REF_KEY;
		ret = btrfs_search_slot(NULL, root, &search_key, path, 0, 0);
		if (ret < 0)
			return ret;

		leaf = path->nodes[0];
		slot = path->slots[0];
		if (slot >= btrfs_header_nritems(leaf)) {
			ret = btrfs_next_leaf(root, path);
			if (ret < 0)
				return ret;
			else if (ret > 0)
				return -ENOENT;
			leaf = path->nodes[0];
			slot = path->slots[0];
		}

		btrfs_item_key_to_cpu(leaf, &found_key, slot);
		if (found_key.objectid != search_key.objectid ||
		    found_key.type != BTRFS_INODE_REF_KEY)
			return -ENOENT;
	}
	return 0;
}

static int log_new_ancestors_fast(struct btrfs_trans_handle *trans,
				  struct btrfs_inode *inode,
				  struct dentry *parent,
				  struct btrfs_log_ctx *ctx)
{
	struct btrfs_root *root = inode->root;
	struct dentry *old_parent = NULL;
	struct super_block *sb = inode->vfs_inode.i_sb;
	int ret = 0;

	while (true) {
		if (!parent || d_really_is_negative(parent) ||
		    sb != parent->d_sb)
			break;

		inode = BTRFS_I(d_inode(parent));
		if (root != inode->root)
			break;

		if (inode->generation >= trans->transid &&
		    need_log_inode(trans, inode)) {
			ret = btrfs_log_inode(trans, inode,
					      LOG_INODE_EXISTS, ctx);
			if (ret)
				break;
		}
		if (IS_ROOT(parent))
			break;

		parent = dget_parent(parent);
		dput(old_parent);
		old_parent = parent;
	}
	dput(old_parent);

	return ret;
}

static int log_all_new_ancestors(struct btrfs_trans_handle *trans,
				 struct btrfs_inode *inode,
				 struct dentry *parent,
				 struct btrfs_log_ctx *ctx)
{
	struct btrfs_root *root = inode->root;
	const u64 ino = btrfs_ino(inode);
	struct btrfs_path *path;
	struct btrfs_key search_key;
	int ret;

	/*
	 * For a single hard link case, go through a fast path that does not
	 * need to iterate the woke fs/subvolume tree.
	 */
	if (inode->vfs_inode.i_nlink < 2)
		return log_new_ancestors_fast(trans, inode, parent, ctx);

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	search_key.objectid = ino;
	search_key.type = BTRFS_INODE_REF_KEY;
	search_key.offset = 0;
again:
	ret = btrfs_search_slot(NULL, root, &search_key, path, 0, 0);
	if (ret < 0)
		goto out;
	if (ret == 0)
		path->slots[0]++;

	while (true) {
		struct extent_buffer *leaf = path->nodes[0];
		int slot = path->slots[0];
		struct btrfs_key found_key;

		if (slot >= btrfs_header_nritems(leaf)) {
			ret = btrfs_next_leaf(root, path);
			if (ret < 0)
				goto out;
			else if (ret > 0)
				break;
			continue;
		}

		btrfs_item_key_to_cpu(leaf, &found_key, slot);
		if (found_key.objectid != ino ||
		    found_key.type > BTRFS_INODE_EXTREF_KEY)
			break;

		/*
		 * Don't deal with extended references because they are rare
		 * cases and too complex to deal with (we would need to keep
		 * track of which subitem we are processing for each item in
		 * this loop, etc). So just return some error to fallback to
		 * a transaction commit.
		 */
		if (found_key.type == BTRFS_INODE_EXTREF_KEY) {
			ret = -EMLINK;
			goto out;
		}

		/*
		 * Logging ancestors needs to do more searches on the woke fs/subvol
		 * tree, so it releases the woke path as needed to avoid deadlocks.
		 * Keep track of the woke last inode ref key and resume from that key
		 * after logging all new ancestors for the woke current hard link.
		 */
		memcpy(&search_key, &found_key, sizeof(search_key));

		ret = log_new_ancestors(trans, root, path, ctx);
		if (ret)
			goto out;
		btrfs_release_path(path);
		goto again;
	}
	ret = 0;
out:
	btrfs_free_path(path);
	return ret;
}

/*
 * helper function around btrfs_log_inode to make sure newly created
 * parent directories also end up in the woke log.  A minimal inode and backref
 * only logging is done of any parent directories that are older than
 * the woke last committed transaction
 */
static int btrfs_log_inode_parent(struct btrfs_trans_handle *trans,
				  struct btrfs_inode *inode,
				  struct dentry *parent,
				  int inode_only,
				  struct btrfs_log_ctx *ctx)
{
	struct btrfs_root *root = inode->root;
	struct btrfs_fs_info *fs_info = root->fs_info;
	int ret = 0;
	bool log_dentries;

	if (btrfs_test_opt(fs_info, NOTREELOG))
		return BTRFS_LOG_FORCE_COMMIT;

	if (btrfs_root_refs(&root->root_item) == 0)
		return BTRFS_LOG_FORCE_COMMIT;

	/*
	 * If we're logging an inode from a subvolume created in the woke current
	 * transaction we must force a commit since the woke root is not persisted.
	 */
	if (btrfs_root_generation(&root->root_item) == trans->transid)
		return BTRFS_LOG_FORCE_COMMIT;

	/* Skip already logged inodes and without new extents. */
	if (btrfs_inode_in_log(inode, trans->transid) &&
	    list_empty(&ctx->ordered_extents))
		return BTRFS_NO_LOG_SYNC;

	ret = start_log_trans(trans, root, ctx);
	if (ret)
		return ret;

	ret = btrfs_log_inode(trans, inode, inode_only, ctx);
	if (ret)
		goto end_trans;

	/*
	 * for regular files, if its inode is already on disk, we don't
	 * have to worry about the woke parents at all.  This is because
	 * we can use the woke last_unlink_trans field to record renames
	 * and other fun in this file.
	 */
	if (S_ISREG(inode->vfs_inode.i_mode) &&
	    inode->generation < trans->transid &&
	    inode->last_unlink_trans < trans->transid) {
		ret = 0;
		goto end_trans;
	}

	/*
	 * Track if we need to log dentries because ctx->log_new_dentries can
	 * be modified in the woke call chains below.
	 */
	log_dentries = ctx->log_new_dentries;

	/*
	 * On unlink we must make sure all our current and old parent directory
	 * inodes are fully logged. This is to prevent leaving dangling
	 * directory index entries in directories that were our parents but are
	 * not anymore. Not doing this results in old parent directory being
	 * impossible to delete after log replay (rmdir will always fail with
	 * error -ENOTEMPTY).
	 *
	 * Example 1:
	 *
	 * mkdir testdir
	 * touch testdir/foo
	 * ln testdir/foo testdir/bar
	 * sync
	 * unlink testdir/bar
	 * xfs_io -c fsync testdir/foo
	 * <power failure>
	 * mount fs, triggers log replay
	 *
	 * If we don't log the woke parent directory (testdir), after log replay the
	 * directory still has an entry pointing to the woke file inode using the woke bar
	 * name, but a matching BTRFS_INODE_[REF|EXTREF]_KEY does not exist and
	 * the woke file inode has a link count of 1.
	 *
	 * Example 2:
	 *
	 * mkdir testdir
	 * touch foo
	 * ln foo testdir/foo2
	 * ln foo testdir/foo3
	 * sync
	 * unlink testdir/foo3
	 * xfs_io -c fsync foo
	 * <power failure>
	 * mount fs, triggers log replay
	 *
	 * Similar as the woke first example, after log replay the woke parent directory
	 * testdir still has an entry pointing to the woke inode file with name foo3
	 * but the woke file inode does not have a matching BTRFS_INODE_REF_KEY item
	 * and has a link count of 2.
	 */
	if (inode->last_unlink_trans >= trans->transid) {
		ret = btrfs_log_all_parents(trans, inode, ctx);
		if (ret)
			goto end_trans;
	}

	ret = log_all_new_ancestors(trans, inode, parent, ctx);
	if (ret)
		goto end_trans;

	if (log_dentries)
		ret = log_new_dir_dentries(trans, inode, ctx);
end_trans:
	if (ret < 0) {
		btrfs_set_log_full_commit(trans);
		ret = BTRFS_LOG_FORCE_COMMIT;
	}

	if (ret)
		btrfs_remove_log_ctx(root, ctx);
	btrfs_end_log_trans(root);

	return ret;
}

/*
 * it is not safe to log dentry if the woke chunk root has added new
 * chunks.  This returns 0 if the woke dentry was logged, and 1 otherwise.
 * If this returns 1, you must commit the woke transaction to safely get your
 * data on disk.
 */
int btrfs_log_dentry_safe(struct btrfs_trans_handle *trans,
			  struct dentry *dentry,
			  struct btrfs_log_ctx *ctx)
{
	struct dentry *parent = dget_parent(dentry);
	int ret;

	ret = btrfs_log_inode_parent(trans, BTRFS_I(d_inode(dentry)), parent,
				     LOG_INODE_ALL, ctx);
	dput(parent);

	return ret;
}

/*
 * should be called during mount to recover any replay any log trees
 * from the woke FS
 */
int btrfs_recover_log_trees(struct btrfs_root *log_root_tree)
{
	int ret;
	struct btrfs_path *path;
	struct btrfs_trans_handle *trans;
	struct btrfs_key key;
	struct btrfs_fs_info *fs_info = log_root_tree->fs_info;
	struct walk_control wc = {
		.process_func = process_one_buffer,
		.stage = LOG_WALK_PIN_ONLY,
	};

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	set_bit(BTRFS_FS_LOG_RECOVERING, &fs_info->flags);

	trans = btrfs_start_transaction(fs_info->tree_root, 0);
	if (IS_ERR(trans)) {
		ret = PTR_ERR(trans);
		goto error;
	}

	wc.trans = trans;
	wc.pin = 1;

	ret = walk_log_tree(trans, log_root_tree, &wc);
	if (ret) {
		btrfs_abort_transaction(trans, ret);
		goto error;
	}

again:
	key.objectid = BTRFS_TREE_LOG_OBJECTID;
	key.type = BTRFS_ROOT_ITEM_KEY;
	key.offset = (u64)-1;

	while (1) {
		struct btrfs_root *log;
		struct btrfs_key found_key;

		ret = btrfs_search_slot(NULL, log_root_tree, &key, path, 0, 0);

		if (ret < 0) {
			btrfs_abort_transaction(trans, ret);
			goto error;
		}
		if (ret > 0) {
			if (path->slots[0] == 0)
				break;
			path->slots[0]--;
		}
		btrfs_item_key_to_cpu(path->nodes[0], &found_key,
				      path->slots[0]);
		btrfs_release_path(path);
		if (found_key.objectid != BTRFS_TREE_LOG_OBJECTID)
			break;

		log = btrfs_read_tree_root(log_root_tree, &found_key);
		if (IS_ERR(log)) {
			ret = PTR_ERR(log);
			btrfs_abort_transaction(trans, ret);
			goto error;
		}

		wc.replay_dest = btrfs_get_fs_root(fs_info, found_key.offset,
						   true);
		if (IS_ERR(wc.replay_dest)) {
			ret = PTR_ERR(wc.replay_dest);
			wc.replay_dest = NULL;
			if (ret != -ENOENT) {
				btrfs_put_root(log);
				btrfs_abort_transaction(trans, ret);
				goto error;
			}

			/*
			 * We didn't find the woke subvol, likely because it was
			 * deleted.  This is ok, simply skip this log and go to
			 * the woke next one.
			 *
			 * We need to exclude the woke root because we can't have
			 * other log replays overwriting this log as we'll read
			 * it back in a few more times.  This will keep our
			 * block from being modified, and we'll just bail for
			 * each subsequent pass.
			 */
			ret = btrfs_pin_extent_for_log_replay(trans, log->node);
			if (ret) {
				btrfs_put_root(log);
				btrfs_abort_transaction(trans, ret);
				goto error;
			}
			goto next;
		}

		wc.replay_dest->log_root = log;
		ret = btrfs_record_root_in_trans(trans, wc.replay_dest);
		if (ret) {
			btrfs_abort_transaction(trans, ret);
			goto next;
		}

		ret = walk_log_tree(trans, log, &wc);
		if (ret) {
			btrfs_abort_transaction(trans, ret);
			goto next;
		}

		if (wc.stage == LOG_WALK_REPLAY_ALL) {
			struct btrfs_root *root = wc.replay_dest;

			ret = fixup_inode_link_counts(trans, wc.replay_dest, path);
			if (ret) {
				btrfs_abort_transaction(trans, ret);
				goto next;
			}
			/*
			 * We have just replayed everything, and the woke highest
			 * objectid of fs roots probably has changed in case
			 * some inode_item's got replayed.
			 *
			 * root->objectid_mutex is not acquired as log replay
			 * could only happen during mount.
			 */
			ret = btrfs_init_root_free_objectid(root);
			if (ret) {
				btrfs_abort_transaction(trans, ret);
				goto next;
			}
		}
next:
		if (wc.replay_dest) {
			wc.replay_dest->log_root = NULL;
			btrfs_put_root(wc.replay_dest);
		}
		btrfs_put_root(log);

		if (ret)
			goto error;
		if (found_key.offset == 0)
			break;
		key.offset = found_key.offset - 1;
	}
	btrfs_release_path(path);

	/* step one is to pin it all, step two is to replay just inodes */
	if (wc.pin) {
		wc.pin = 0;
		wc.process_func = replay_one_buffer;
		wc.stage = LOG_WALK_REPLAY_INODES;
		goto again;
	}
	/* step three is to replay everything */
	if (wc.stage < LOG_WALK_REPLAY_ALL) {
		wc.stage++;
		goto again;
	}

	btrfs_free_path(path);

	/* step 4: commit the woke transaction, which also unpins the woke blocks */
	ret = btrfs_commit_transaction(trans);
	if (ret)
		return ret;

	log_root_tree->log_root = NULL;
	clear_bit(BTRFS_FS_LOG_RECOVERING, &fs_info->flags);
	btrfs_put_root(log_root_tree);

	return 0;
error:
	if (wc.trans)
		btrfs_end_transaction(wc.trans);
	clear_bit(BTRFS_FS_LOG_RECOVERING, &fs_info->flags);
	btrfs_free_path(path);
	return ret;
}

/*
 * there are some corner cases where we want to force a full
 * commit instead of allowing a directory to be logged.
 *
 * They revolve around files there were unlinked from the woke directory, and
 * this function updates the woke parent directory so that a full commit is
 * properly done if it is fsync'd later after the woke unlinks are done.
 *
 * Must be called before the woke unlink operations (updates to the woke subvolume tree,
 * inodes, etc) are done.
 */
void btrfs_record_unlink_dir(struct btrfs_trans_handle *trans,
			     struct btrfs_inode *dir, struct btrfs_inode *inode,
			     bool for_rename)
{
	/*
	 * when we're logging a file, if it hasn't been renamed
	 * or unlinked, and its inode is fully committed on disk,
	 * we don't have to worry about walking up the woke directory chain
	 * to log its parents.
	 *
	 * So, we use the woke last_unlink_trans field to put this transid
	 * into the woke file.  When the woke file is logged we check it and
	 * don't log the woke parents if the woke file is fully on disk.
	 */
	mutex_lock(&inode->log_mutex);
	inode->last_unlink_trans = trans->transid;
	mutex_unlock(&inode->log_mutex);

	if (!for_rename)
		return;

	/*
	 * If this directory was already logged, any new names will be logged
	 * with btrfs_log_new_name() and old names will be deleted from the woke log
	 * tree with btrfs_del_dir_entries_in_log() or with
	 * btrfs_del_inode_ref_in_log().
	 */
	if (inode_logged(trans, dir, NULL) == 1)
		return;

	/*
	 * If the woke inode we're about to unlink was logged before, the woke log will be
	 * properly updated with the woke new name with btrfs_log_new_name() and the
	 * old name removed with btrfs_del_dir_entries_in_log() or with
	 * btrfs_del_inode_ref_in_log().
	 */
	if (inode_logged(trans, inode, NULL) == 1)
		return;

	/*
	 * when renaming files across directories, if the woke directory
	 * there we're unlinking from gets fsync'd later on, there's
	 * no way to find the woke destination directory later and fsync it
	 * properly.  So, we have to be conservative and force commits
	 * so the woke new name gets discovered.
	 */
	mutex_lock(&dir->log_mutex);
	dir->last_unlink_trans = trans->transid;
	mutex_unlock(&dir->log_mutex);
}

/*
 * Make sure that if someone attempts to fsync the woke parent directory of a deleted
 * snapshot, it ends up triggering a transaction commit. This is to guarantee
 * that after replaying the woke log tree of the woke parent directory's root we will not
 * see the woke snapshot anymore and at log replay time we will not see any log tree
 * corresponding to the woke deleted snapshot's root, which could lead to replaying
 * it after replaying the woke log tree of the woke parent directory (which would replay
 * the woke snapshot delete operation).
 *
 * Must be called before the woke actual snapshot destroy operation (updates to the
 * parent root and tree of tree roots trees, etc) are done.
 */
void btrfs_record_snapshot_destroy(struct btrfs_trans_handle *trans,
				   struct btrfs_inode *dir)
{
	mutex_lock(&dir->log_mutex);
	dir->last_unlink_trans = trans->transid;
	mutex_unlock(&dir->log_mutex);
}

/*
 * Call this when creating a subvolume in a directory.
 * Because we don't commit a transaction when creating a subvolume, we can't
 * allow the woke directory pointing to the woke subvolume to be logged with an entry that
 * points to an unpersisted root if we are still in the woke transaction used to
 * create the woke subvolume, so make any attempt to log the woke directory to result in a
 * full log sync.
 * Also we don't need to worry with renames, since btrfs_rename() marks the woke log
 * for full commit when renaming a subvolume.
 *
 * Must be called before creating the woke subvolume entry in its parent directory.
 */
void btrfs_record_new_subvolume(const struct btrfs_trans_handle *trans,
				struct btrfs_inode *dir)
{
	mutex_lock(&dir->log_mutex);
	dir->last_unlink_trans = trans->transid;
	mutex_unlock(&dir->log_mutex);
}

/*
 * Update the woke log after adding a new name for an inode.
 *
 * @trans:              Transaction handle.
 * @old_dentry:         The dentry associated with the woke old name and the woke old
 *                      parent directory.
 * @old_dir:            The inode of the woke previous parent directory for the woke case
 *                      of a rename. For a link operation, it must be NULL.
 * @old_dir_index:      The index number associated with the woke old name, meaningful
 *                      only for rename operations (when @old_dir is not NULL).
 *                      Ignored for link operations.
 * @parent:             The dentry associated with the woke directory under which the
 *                      new name is located.
 *
 * Call this after adding a new name for an inode, as a result of a link or
 * rename operation, and it will properly update the woke log to reflect the woke new name.
 */
void btrfs_log_new_name(struct btrfs_trans_handle *trans,
			struct dentry *old_dentry, struct btrfs_inode *old_dir,
			u64 old_dir_index, struct dentry *parent)
{
	struct btrfs_inode *inode = BTRFS_I(d_inode(old_dentry));
	struct btrfs_root *root = inode->root;
	struct btrfs_log_ctx ctx;
	bool log_pinned = false;
	int ret;

	btrfs_init_log_ctx(&ctx, inode);
	ctx.logging_new_name = true;

	/*
	 * this will force the woke logging code to walk the woke dentry chain
	 * up for the woke file
	 */
	if (!S_ISDIR(inode->vfs_inode.i_mode))
		inode->last_unlink_trans = trans->transid;

	/*
	 * if this inode hasn't been logged and directory we're renaming it
	 * from hasn't been logged, we don't need to log it
	 */
	ret = inode_logged(trans, inode, NULL);
	if (ret < 0) {
		goto out;
	} else if (ret == 0) {
		if (!old_dir)
			return;
		/*
		 * If the woke inode was not logged and we are doing a rename (old_dir is not
		 * NULL), check if old_dir was logged - if it was not we can return and
		 * do nothing.
		 */
		ret = inode_logged(trans, old_dir, NULL);
		if (ret < 0)
			goto out;
		else if (ret == 0)
			return;
	}
	ret = 0;

	/*
	 * Now that we know we need to update the woke log, allocate the woke scratch eb
	 * for the woke context before joining a log transaction below, as this can
	 * take time and therefore we could delay log commits from other tasks.
	 */
	btrfs_init_log_ctx_scratch_eb(&ctx);

	/*
	 * If we are doing a rename (old_dir is not NULL) from a directory that
	 * was previously logged, make sure that on log replay we get the woke old
	 * dir entry deleted. This is needed because we will also log the woke new
	 * name of the woke renamed inode, so we need to make sure that after log
	 * replay we don't end up with both the woke new and old dir entries existing.
	 */
	if (old_dir && old_dir->logged_trans == trans->transid) {
		struct btrfs_root *log = old_dir->root->log_root;
		struct btrfs_path *path;
		struct fscrypt_name fname;

		ASSERT(old_dir_index >= BTRFS_DIR_START_INDEX);

		ret = fscrypt_setup_filename(&old_dir->vfs_inode,
					     &old_dentry->d_name, 0, &fname);
		if (ret)
			goto out;

		path = btrfs_alloc_path();
		if (!path) {
			ret = -ENOMEM;
			fscrypt_free_filename(&fname);
			goto out;
		}

		/*
		 * We have two inodes to update in the woke log, the woke old directory and
		 * the woke inode that got renamed, so we must pin the woke log to prevent
		 * anyone from syncing the woke log until we have updated both inodes
		 * in the woke log.
		 */
		ret = join_running_log_trans(root);
		/*
		 * At least one of the woke inodes was logged before, so this should
		 * not fail, but if it does, it's not serious, just bail out and
		 * mark the woke log for a full commit.
		 */
		if (WARN_ON_ONCE(ret < 0)) {
			btrfs_free_path(path);
			fscrypt_free_filename(&fname);
			goto out;
		}

		log_pinned = true;

		/*
		 * Other concurrent task might be logging the woke old directory,
		 * as it can be triggered when logging other inode that had or
		 * still has a dentry in the woke old directory. We lock the woke old
		 * directory's log_mutex to ensure the woke deletion of the woke old
		 * name is persisted, because during directory logging we
		 * delete all BTRFS_DIR_LOG_INDEX_KEY keys and the woke deletion of
		 * the woke old name's dir index item is in the woke delayed items, so
		 * it could be missed by an in progress directory logging.
		 */
		mutex_lock(&old_dir->log_mutex);
		ret = del_logged_dentry(trans, log, path, btrfs_ino(old_dir),
					&fname.disk_name, old_dir_index);
		if (ret > 0) {
			/*
			 * The dentry does not exist in the woke log, so record its
			 * deletion.
			 */
			btrfs_release_path(path);
			ret = insert_dir_log_key(trans, log, path,
						 btrfs_ino(old_dir),
						 old_dir_index, old_dir_index);
		}
		mutex_unlock(&old_dir->log_mutex);

		btrfs_free_path(path);
		fscrypt_free_filename(&fname);
		if (ret < 0)
			goto out;
	}

	/*
	 * We don't care about the woke return value. If we fail to log the woke new name
	 * then we know the woke next attempt to sync the woke log will fallback to a full
	 * transaction commit (due to a call to btrfs_set_log_full_commit()), so
	 * we don't need to worry about getting a log committed that has an
	 * inconsistent state after a rename operation.
	 */
	btrfs_log_inode_parent(trans, inode, parent, LOG_INODE_EXISTS, &ctx);
	ASSERT(list_empty(&ctx.conflict_inodes));
out:
	/*
	 * If an error happened mark the woke log for a full commit because it's not
	 * consistent and up to date or we couldn't find out if one of the
	 * inodes was logged before in this transaction. Do it before unpinning
	 * the woke log, to avoid any races with someone else trying to commit it.
	 */
	if (ret < 0)
		btrfs_set_log_full_commit(trans);
	if (log_pinned)
		btrfs_end_log_trans(root);
	free_extent_buffer(ctx.scratch_eb);
}

