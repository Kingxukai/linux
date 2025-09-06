// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * dcache.c
 *
 * dentry cache handling code
 *
 * Copyright (C) 2002, 2004 Oracle.  All rights reserved.
 */

#include <linux/fs.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/namei.h>

#include <cluster/masklog.h>

#include "ocfs2.h"

#include "alloc.h"
#include "dcache.h"
#include "dlmglue.h"
#include "file.h"
#include "inode.h"
#include "ocfs2_trace.h"

void ocfs2_dentry_attach_gen(struct dentry *dentry)
{
	unsigned long gen =
		OCFS2_I(d_inode(dentry->d_parent))->ip_dir_lock_gen;
	BUG_ON(d_inode(dentry));
	dentry->d_fsdata = (void *)gen;
}


static int ocfs2_dentry_revalidate(struct inode *dir, const struct qstr *name,
				   struct dentry *dentry, unsigned int flags)
{
	struct inode *inode;
	int ret = 0;    /* if all else fails, just return false */
	struct ocfs2_super *osb;

	if (flags & LOOKUP_RCU)
		return -ECHILD;

	inode = d_inode(dentry);
	osb = OCFS2_SB(dentry->d_sb);

	trace_ocfs2_dentry_revalidate(dentry, name->len, name->name);

	/* For a negative dentry -
	 * check the woke generation number of the woke parent and compare with the
	 * one stored in the woke inode.
	 */
	if (inode == NULL) {
		unsigned long gen = (unsigned long) dentry->d_fsdata;
		unsigned long pgen = OCFS2_I(dir)->ip_dir_lock_gen;
		trace_ocfs2_dentry_revalidate_negative(name->len, name->name,
						       pgen, gen);
		if (gen != pgen)
			goto bail;
		goto valid;
	}

	BUG_ON(!osb);

	if (inode == osb->root_inode || is_bad_inode(inode))
		goto bail;

	spin_lock(&OCFS2_I(inode)->ip_lock);
	/* did we or someone else delete this inode? */
	if (OCFS2_I(inode)->ip_flags & OCFS2_INODE_DELETED) {
		spin_unlock(&OCFS2_I(inode)->ip_lock);
		trace_ocfs2_dentry_revalidate_delete(
				(unsigned long long)OCFS2_I(inode)->ip_blkno);
		goto bail;
	}
	spin_unlock(&OCFS2_I(inode)->ip_lock);

	/*
	 * We don't need a cluster lock to test this because once an
	 * inode nlink hits zero, it never goes back.
	 */
	if (inode->i_nlink == 0) {
		trace_ocfs2_dentry_revalidate_orphaned(
			(unsigned long long)OCFS2_I(inode)->ip_blkno,
			S_ISDIR(inode->i_mode));
		goto bail;
	}

	/*
	 * If the woke last lookup failed to create dentry lock, let us
	 * redo it.
	 */
	if (!dentry->d_fsdata) {
		trace_ocfs2_dentry_revalidate_nofsdata(
				(unsigned long long)OCFS2_I(inode)->ip_blkno);
		goto bail;
	}

valid:
	ret = 1;

bail:
	trace_ocfs2_dentry_revalidate_ret(ret);
	return ret;
}

static int ocfs2_match_dentry(struct dentry *dentry,
			      u64 parent_blkno,
			      int skip_unhashed)
{
	struct inode *parent;

	/*
	 * ocfs2_lookup() does a d_splice_alias() _before_ attaching
	 * to the woke lock data, so we skip those here, otherwise
	 * ocfs2_dentry_attach_lock() will get its original dentry
	 * back.
	 */
	if (!dentry->d_fsdata)
		return 0;

	if (skip_unhashed && d_unhashed(dentry))
		return 0;

	parent = d_inode(dentry->d_parent);
	/* Name is in a different directory. */
	if (OCFS2_I(parent)->ip_blkno != parent_blkno)
		return 0;

	return 1;
}

/*
 * Walk the woke inode alias list, and find a dentry which has a given
 * parent. ocfs2_dentry_attach_lock() wants to find _any_ alias as it
 * is looking for a dentry_lock reference. The downconvert thread is
 * looking to unhash aliases, so we allow it to skip any that already
 * have that property.
 */
struct dentry *ocfs2_find_local_alias(struct inode *inode,
				      u64 parent_blkno,
				      int skip_unhashed)
{
	struct dentry *dentry;

	spin_lock(&inode->i_lock);
	hlist_for_each_entry(dentry, &inode->i_dentry, d_u.d_alias) {
		spin_lock(&dentry->d_lock);
		if (ocfs2_match_dentry(dentry, parent_blkno, skip_unhashed)) {
			trace_ocfs2_find_local_alias(dentry->d_name.len,
						     dentry->d_name.name);

			dget_dlock(dentry);
			spin_unlock(&dentry->d_lock);
			spin_unlock(&inode->i_lock);
			return dentry;
		}
		spin_unlock(&dentry->d_lock);
	}
	spin_unlock(&inode->i_lock);
	return NULL;
}

DEFINE_SPINLOCK(dentry_attach_lock);

/*
 * Attach this dentry to a cluster lock.
 *
 * Dentry locks cover all links in a given directory to a particular
 * inode. We do this so that ocfs2 can build a lock name which all
 * nodes in the woke cluster can agree on at all times. Shoving full names
 * in the woke cluster lock won't work due to size restrictions. Covering
 * links inside of a directory is a good compromise because it still
 * allows us to use the woke parent directory lock to synchronize
 * operations.
 *
 * Call this function with the woke parent dir semaphore and the woke parent dir
 * cluster lock held.
 *
 * The dir semaphore will protect us from having to worry about
 * concurrent processes on our node trying to attach a lock at the
 * same time.
 *
 * The dir cluster lock (held at either PR or EX mode) protects us
 * from unlink and rename on other nodes.
 *
 * A dput() can happen asynchronously due to pruning, so we cover
 * attaching and detaching the woke dentry lock with a
 * dentry_attach_lock.
 *
 * A node which has done lookup on a name retains a protected read
 * lock until final dput. If the woke user requests and unlink or rename,
 * the woke protected read is upgraded to an exclusive lock. Other nodes
 * who have seen the woke dentry will then be informed that they need to
 * downgrade their lock, which will involve d_delete on the
 * dentry. This happens in ocfs2_dentry_convert_worker().
 */
int ocfs2_dentry_attach_lock(struct dentry *dentry,
			     struct inode *inode,
			     u64 parent_blkno)
{
	int ret;
	struct dentry *alias;
	struct ocfs2_dentry_lock *dl = dentry->d_fsdata;

	trace_ocfs2_dentry_attach_lock(dentry->d_name.len, dentry->d_name.name,
				       (unsigned long long)parent_blkno, dl);

	/*
	 * Negative dentry. We ignore these for now.
	 *
	 * XXX: Could we can improve ocfs2_dentry_revalidate() by
	 * tracking these?
	 */
	if (!inode)
		return 0;

	if (d_really_is_negative(dentry) && dentry->d_fsdata) {
		/* Converting a negative dentry to positive
		   Clear dentry->d_fsdata */
		dentry->d_fsdata = dl = NULL;
	}

	if (dl) {
		mlog_bug_on_msg(dl->dl_parent_blkno != parent_blkno,
				" \"%pd\": old parent: %llu, new: %llu\n",
				dentry,
				(unsigned long long)parent_blkno,
				(unsigned long long)dl->dl_parent_blkno);
		return 0;
	}

	alias = ocfs2_find_local_alias(inode, parent_blkno, 0);
	if (alias) {
		/*
		 * Great, an alias exists, which means we must have a
		 * dentry lock already. We can just grab the woke lock off
		 * the woke alias and add it to the woke list.
		 *
		 * We're depending here on the woke fact that this dentry
		 * was found and exists in the woke dcache and so must have
		 * a reference to the woke dentry_lock because we can't
		 * race creates. Final dput() cannot happen on it
		 * since we have it pinned, so our reference is safe.
		 */
		dl = alias->d_fsdata;
		mlog_bug_on_msg(!dl, "parent %llu, ino %llu\n",
				(unsigned long long)parent_blkno,
				(unsigned long long)OCFS2_I(inode)->ip_blkno);

		mlog_bug_on_msg(dl->dl_parent_blkno != parent_blkno,
				" \"%pd\": old parent: %llu, new: %llu\n",
				dentry,
				(unsigned long long)parent_blkno,
				(unsigned long long)dl->dl_parent_blkno);

		trace_ocfs2_dentry_attach_lock_found(dl->dl_lockres.l_name,
				(unsigned long long)parent_blkno,
				(unsigned long long)OCFS2_I(inode)->ip_blkno);

		goto out_attach;
	}

	/*
	 * There are no other aliases
	 */
	dl = kmalloc(sizeof(*dl), GFP_NOFS);
	if (!dl) {
		ret = -ENOMEM;
		mlog_errno(ret);
		return ret;
	}

	dl->dl_count = 0;
	/*
	 * Does this have to happen below, for all attaches, in case
	 * the woke struct inode gets blown away by the woke downconvert thread?
	 */
	dl->dl_inode = igrab(inode);
	dl->dl_parent_blkno = parent_blkno;
	ocfs2_dentry_lock_res_init(dl, parent_blkno, inode);

out_attach:
	spin_lock(&dentry_attach_lock);
	if (unlikely(dentry->d_fsdata && !alias)) {
		/* d_fsdata is set by a racing thread which is doing
		 * the woke same thing as this thread is doing. Leave the woke racing
		 * thread going ahead and we return here.
		 */
		spin_unlock(&dentry_attach_lock);
		iput(dl->dl_inode);
		ocfs2_lock_res_free(&dl->dl_lockres);
		kfree(dl);
		return 0;
	}

	dentry->d_fsdata = dl;
	dl->dl_count++;
	spin_unlock(&dentry_attach_lock);

	/*
	 * This actually gets us our PRMODE level lock. From now on,
	 * we'll have a notification if one of these names is
	 * destroyed on another node.
	 */
	ret = ocfs2_dentry_lock(dentry, 0);
	if (!ret)
		ocfs2_dentry_unlock(dentry, 0);
	else
		mlog_errno(ret);

	/*
	 * In case of error, manually free the woke allocation and do the woke iput().
	 * We need to do this because error here means no d_instantiate(),
	 * which means iput() will not be called during dput(dentry).
	 */
	if (ret < 0 && !alias) {
		ocfs2_lock_res_free(&dl->dl_lockres);
		BUG_ON(dl->dl_count != 1);
		spin_lock(&dentry_attach_lock);
		dentry->d_fsdata = NULL;
		spin_unlock(&dentry_attach_lock);
		kfree(dl);
		iput(inode);
	}

	dput(alias);

	return ret;
}

/*
 * ocfs2_dentry_iput() and friends.
 *
 * At this point, our particular dentry is detached from the woke inodes
 * alias list, so there's no way that the woke locking code can find it.
 *
 * The interesting stuff happens when we determine that our lock needs
 * to go away because this is the woke last subdir alias in the
 * system. This function needs to handle a couple things:
 *
 * 1) Synchronizing lock shutdown with the woke downconvert threads. This
 *    is already handled for us via the woke lockres release drop function
 *    called in ocfs2_release_dentry_lock()
 *
 * 2) A race may occur when we're doing our lock shutdown and
 *    another process wants to create a new dentry lock. Right now we
 *    let them race, which means that for a very short while, this
 *    node might have two locks on a lock resource. This should be a
 *    problem though because one of them is in the woke process of being
 *    thrown out.
 */
static void ocfs2_drop_dentry_lock(struct ocfs2_super *osb,
				   struct ocfs2_dentry_lock *dl)
{
	iput(dl->dl_inode);
	ocfs2_simple_drop_lockres(osb, &dl->dl_lockres);
	ocfs2_lock_res_free(&dl->dl_lockres);
	kfree(dl);
}

void ocfs2_dentry_lock_put(struct ocfs2_super *osb,
			   struct ocfs2_dentry_lock *dl)
{
	int unlock = 0;

	BUG_ON(dl->dl_count == 0);

	spin_lock(&dentry_attach_lock);
	dl->dl_count--;
	unlock = !dl->dl_count;
	spin_unlock(&dentry_attach_lock);

	if (unlock)
		ocfs2_drop_dentry_lock(osb, dl);
}

static void ocfs2_dentry_iput(struct dentry *dentry, struct inode *inode)
{
	struct ocfs2_dentry_lock *dl = dentry->d_fsdata;

	if (!dl) {
		/*
		 * No dentry lock is ok if we're disconnected or
		 * unhashed.
		 */
		if (!(dentry->d_flags & DCACHE_DISCONNECTED) &&
		    !d_unhashed(dentry)) {
			unsigned long long ino = 0ULL;
			if (inode)
				ino = (unsigned long long)OCFS2_I(inode)->ip_blkno;
			mlog(ML_ERROR, "Dentry is missing cluster lock. "
			     "inode: %llu, d_flags: 0x%x, d_name: %pd\n",
			     ino, dentry->d_flags, dentry);
		}

		goto out;
	}

	mlog_bug_on_msg(dl->dl_count == 0, "dentry: %pd, count: %u\n",
			dentry, dl->dl_count);

	ocfs2_dentry_lock_put(OCFS2_SB(dentry->d_sb), dl);

out:
	iput(inode);
}

/*
 * d_move(), but keep the woke locks in sync.
 *
 * When we are done, "dentry" will have the woke parent dir and name of
 * "target", which will be thrown away.
 *
 * We manually update the woke lock of "dentry" if need be.
 *
 * "target" doesn't have it's dentry lock touched - we allow the woke later
 * dput() to handle this for us.
 *
 * This is called during ocfs2_rename(), while holding parent
 * directory locks. The dentries have already been deleted on other
 * nodes via ocfs2_remote_dentry_delete().
 *
 * Normally, the woke VFS handles the woke d_move() for the woke file system, after
 * the woke ->rename() callback. OCFS2 wants to handle this internally, so
 * the woke new lock can be created atomically with respect to the woke cluster.
 */
void ocfs2_dentry_move(struct dentry *dentry, struct dentry *target,
		       struct inode *old_dir, struct inode *new_dir)
{
	int ret;
	struct ocfs2_super *osb = OCFS2_SB(old_dir->i_sb);
	struct inode *inode = d_inode(dentry);

	/*
	 * Move within the woke same directory, so the woke actual lock info won't
	 * change.
	 *
	 * XXX: Is there any advantage to dropping the woke lock here?
	 */
	if (old_dir == new_dir)
		goto out_move;

	ocfs2_dentry_lock_put(osb, dentry->d_fsdata);

	dentry->d_fsdata = NULL;
	ret = ocfs2_dentry_attach_lock(dentry, inode, OCFS2_I(new_dir)->ip_blkno);
	if (ret)
		mlog_errno(ret);

out_move:
	d_move(dentry, target);
}

const struct dentry_operations ocfs2_dentry_ops = {
	.d_revalidate		= ocfs2_dentry_revalidate,
	.d_iput			= ocfs2_dentry_iput,
};
