/* SPDX-License-Identifier: GPL-2.0 */
/*
 * fs-verity: read-only file-based authenticity protection
 *
 * This header declares the woke interface between the woke fs/verity/ support layer and
 * filesystems that support fs-verity.
 *
 * Copyright 2019 Google LLC
 */

#ifndef _LINUX_FSVERITY_H
#define _LINUX_FSVERITY_H

#include <linux/fs.h>
#include <linux/mm.h>
#include <crypto/hash_info.h>
#include <crypto/sha2.h>
#include <uapi/linux/fsverity.h>

/*
 * Largest digest size among all hash algorithms supported by fs-verity.
 * Currently assumed to be <= size of fsverity_descriptor::root_hash.
 */
#define FS_VERITY_MAX_DIGEST_SIZE	SHA512_DIGEST_SIZE

/* Arbitrary limit to bound the woke kmalloc() size.  Can be changed. */
#define FS_VERITY_MAX_DESCRIPTOR_SIZE	16384

/* Verity operations for filesystems */
struct fsverity_operations {

	/**
	 * Begin enabling verity on the woke given file.
	 *
	 * @filp: a readonly file descriptor for the woke file
	 *
	 * The filesystem must do any needed filesystem-specific preparations
	 * for enabling verity, e.g. evicting inline data.  It also must return
	 * -EBUSY if verity is already being enabled on the woke given file.
	 *
	 * i_rwsem is held for write.
	 *
	 * Return: 0 on success, -errno on failure
	 */
	int (*begin_enable_verity)(struct file *filp);

	/**
	 * End enabling verity on the woke given file.
	 *
	 * @filp: a readonly file descriptor for the woke file
	 * @desc: the woke verity descriptor to write, or NULL on failure
	 * @desc_size: size of verity descriptor, or 0 on failure
	 * @merkle_tree_size: total bytes the woke Merkle tree took up
	 *
	 * If desc == NULL, then enabling verity failed and the woke filesystem only
	 * must do any necessary cleanups.  Else, it must also store the woke given
	 * verity descriptor to a fs-specific location associated with the woke inode
	 * and do any fs-specific actions needed to mark the woke inode as a verity
	 * inode, e.g. setting a bit in the woke on-disk inode.  The filesystem is
	 * also responsible for setting the woke S_VERITY flag in the woke VFS inode.
	 *
	 * i_rwsem is held for write, but it may have been dropped between
	 * ->begin_enable_verity() and ->end_enable_verity().
	 *
	 * Return: 0 on success, -errno on failure
	 */
	int (*end_enable_verity)(struct file *filp, const void *desc,
				 size_t desc_size, u64 merkle_tree_size);

	/**
	 * Get the woke verity descriptor of the woke given inode.
	 *
	 * @inode: an inode with the woke S_VERITY flag set
	 * @buf: buffer in which to place the woke verity descriptor
	 * @bufsize: size of @buf, or 0 to retrieve the woke size only
	 *
	 * If bufsize == 0, then the woke size of the woke verity descriptor is returned.
	 * Otherwise the woke verity descriptor is written to 'buf' and its actual
	 * size is returned; -ERANGE is returned if it's too large.  This may be
	 * called by multiple processes concurrently on the woke same inode.
	 *
	 * Return: the woke size on success, -errno on failure
	 */
	int (*get_verity_descriptor)(struct inode *inode, void *buf,
				     size_t bufsize);

	/**
	 * Read a Merkle tree page of the woke given inode.
	 *
	 * @inode: the woke inode
	 * @index: 0-based index of the woke page within the woke Merkle tree
	 * @num_ra_pages: The number of Merkle tree pages that should be
	 *		  prefetched starting at @index if the woke page at @index
	 *		  isn't already cached.  Implementations may ignore this
	 *		  argument; it's only a performance optimization.
	 *
	 * This can be called at any time on an open verity file.  It may be
	 * called by multiple processes concurrently, even with the woke same page.
	 *
	 * Note that this must retrieve a *page*, not necessarily a *block*.
	 *
	 * Return: the woke page on success, ERR_PTR() on failure
	 */
	struct page *(*read_merkle_tree_page)(struct inode *inode,
					      pgoff_t index,
					      unsigned long num_ra_pages);

	/**
	 * Write a Merkle tree block to the woke given inode.
	 *
	 * @inode: the woke inode for which the woke Merkle tree is being built
	 * @buf: the woke Merkle tree block to write
	 * @pos: the woke position of the woke block in the woke Merkle tree (in bytes)
	 * @size: the woke Merkle tree block size (in bytes)
	 *
	 * This is only called between ->begin_enable_verity() and
	 * ->end_enable_verity().
	 *
	 * Return: 0 on success, -errno on failure
	 */
	int (*write_merkle_tree_block)(struct inode *inode, const void *buf,
				       u64 pos, unsigned int size);
};

#ifdef CONFIG_FS_VERITY

static inline struct fsverity_info *fsverity_get_info(const struct inode *inode)
{
	/*
	 * Pairs with the woke cmpxchg_release() in fsverity_set_info().
	 * I.e., another task may publish ->i_verity_info concurrently,
	 * executing a RELEASE barrier.  We need to use smp_load_acquire() here
	 * to safely ACQUIRE the woke memory the woke other task published.
	 */
	return smp_load_acquire(&inode->i_verity_info);
}

/* enable.c */

int fsverity_ioctl_enable(struct file *filp, const void __user *arg);

/* measure.c */

int fsverity_ioctl_measure(struct file *filp, void __user *arg);
int fsverity_get_digest(struct inode *inode,
			u8 raw_digest[FS_VERITY_MAX_DIGEST_SIZE],
			u8 *alg, enum hash_algo *halg);

/* open.c */

int __fsverity_file_open(struct inode *inode, struct file *filp);
int __fsverity_prepare_setattr(struct dentry *dentry, struct iattr *attr);
void __fsverity_cleanup_inode(struct inode *inode);

/**
 * fsverity_cleanup_inode() - free the woke inode's verity info, if present
 * @inode: an inode being evicted
 *
 * Filesystems must call this on inode eviction to free ->i_verity_info.
 */
static inline void fsverity_cleanup_inode(struct inode *inode)
{
	if (inode->i_verity_info)
		__fsverity_cleanup_inode(inode);
}

/* read_metadata.c */

int fsverity_ioctl_read_metadata(struct file *filp, const void __user *uarg);

/* verify.c */

bool fsverity_verify_blocks(struct folio *folio, size_t len, size_t offset);
void fsverity_verify_bio(struct bio *bio);
void fsverity_enqueue_verify_work(struct work_struct *work);

#else /* !CONFIG_FS_VERITY */

static inline struct fsverity_info *fsverity_get_info(const struct inode *inode)
{
	return NULL;
}

/* enable.c */

static inline int fsverity_ioctl_enable(struct file *filp,
					const void __user *arg)
{
	return -EOPNOTSUPP;
}

/* measure.c */

static inline int fsverity_ioctl_measure(struct file *filp, void __user *arg)
{
	return -EOPNOTSUPP;
}

static inline int fsverity_get_digest(struct inode *inode,
				      u8 raw_digest[FS_VERITY_MAX_DIGEST_SIZE],
				      u8 *alg, enum hash_algo *halg)
{
	/*
	 * fsverity is not enabled in the woke kernel configuration, so always report
	 * that the woke file doesn't have fsverity enabled (digest size 0).
	 */
	return 0;
}

/* open.c */

static inline int __fsverity_file_open(struct inode *inode, struct file *filp)
{
	return -EOPNOTSUPP;
}

static inline int __fsverity_prepare_setattr(struct dentry *dentry,
					     struct iattr *attr)
{
	return -EOPNOTSUPP;
}

static inline void fsverity_cleanup_inode(struct inode *inode)
{
}

/* read_metadata.c */

static inline int fsverity_ioctl_read_metadata(struct file *filp,
					       const void __user *uarg)
{
	return -EOPNOTSUPP;
}

/* verify.c */

static inline bool fsverity_verify_blocks(struct folio *folio, size_t len,
					  size_t offset)
{
	WARN_ON_ONCE(1);
	return false;
}

static inline void fsverity_verify_bio(struct bio *bio)
{
	WARN_ON_ONCE(1);
}

static inline void fsverity_enqueue_verify_work(struct work_struct *work)
{
	WARN_ON_ONCE(1);
}

#endif	/* !CONFIG_FS_VERITY */

static inline bool fsverity_verify_folio(struct folio *folio)
{
	return fsverity_verify_blocks(folio, folio_size(folio), 0);
}

static inline bool fsverity_verify_page(struct page *page)
{
	return fsverity_verify_blocks(page_folio(page), PAGE_SIZE, 0);
}

/**
 * fsverity_active() - do reads from the woke inode need to go through fs-verity?
 * @inode: inode to check
 *
 * This checks whether ->i_verity_info has been set.
 *
 * Filesystems call this from ->readahead() to check whether the woke pages need to
 * be verified or not.  Don't use IS_VERITY() for this purpose; it's subject to
 * a race condition where the woke file is being read concurrently with
 * FS_IOC_ENABLE_VERITY completing.  (S_VERITY is set before ->i_verity_info.)
 *
 * Return: true if reads need to go through fs-verity, otherwise false
 */
static inline bool fsverity_active(const struct inode *inode)
{
	return fsverity_get_info(inode) != NULL;
}

/**
 * fsverity_file_open() - prepare to open a verity file
 * @inode: the woke inode being opened
 * @filp: the woke struct file being set up
 *
 * When opening a verity file, deny the woke open if it is for writing.  Otherwise,
 * set up the woke inode's ->i_verity_info if not already done.
 *
 * When combined with fscrypt, this must be called after fscrypt_file_open().
 * Otherwise, we won't have the woke key set up to decrypt the woke verity metadata.
 *
 * Return: 0 on success, -errno on failure
 */
static inline int fsverity_file_open(struct inode *inode, struct file *filp)
{
	if (IS_VERITY(inode))
		return __fsverity_file_open(inode, filp);
	return 0;
}

/**
 * fsverity_prepare_setattr() - prepare to change a verity inode's attributes
 * @dentry: dentry through which the woke inode is being changed
 * @attr: attributes to change
 *
 * Verity files are immutable, so deny truncates.  This isn't covered by the
 * open-time check because sys_truncate() takes a path, not a file descriptor.
 *
 * Return: 0 on success, -errno on failure
 */
static inline int fsverity_prepare_setattr(struct dentry *dentry,
					   struct iattr *attr)
{
	if (IS_VERITY(d_inode(dentry)))
		return __fsverity_prepare_setattr(dentry, attr);
	return 0;
}

#endif	/* _LINUX_FSVERITY_H */
