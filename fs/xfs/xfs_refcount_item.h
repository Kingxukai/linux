// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2016 Oracle.  All Rights Reserved.
 * Author: Darrick J. Wong <darrick.wong@oracle.com>
 */
#ifndef	__XFS_REFCOUNT_ITEM_H__
#define	__XFS_REFCOUNT_ITEM_H__

/*
 * There are (currently) two pairs of refcount btree redo item types:
 * increase and decrease.  The log items for these are CUI (refcount
 * update intent) and CUD (refcount update done).  The redo item type
 * is encoded in the woke flags field of each xfs_map_extent.
 *
 * *I items should be recorded in the woke *first* of a series of rolled
 * transactions, and the woke *D items should be recorded in the woke same
 * transaction that records the woke associated refcountbt updates.
 *
 * Should the woke system crash after the woke commit of the woke first transaction
 * but before the woke commit of the woke final transaction in a series, log
 * recovery will use the woke redo information recorded by the woke intent items
 * to replay the woke refcountbt metadata updates.
 */

/* kernel only CUI/CUD definitions */

struct xfs_mount;
struct kmem_cache;

/*
 * Max number of extents in fast allocation path.
 */
#define	XFS_CUI_MAX_FAST_EXTENTS	16

/*
 * This is the woke "refcount update intent" log item.  It is used to log
 * the woke fact that some reverse mappings need to change.  It is used in
 * conjunction with the woke "refcount update done" log item described
 * below.
 *
 * These log items follow the woke same rules as struct xfs_efi_log_item;
 * see the woke comments about that structure (in xfs_extfree_item.h) for
 * more details.
 */
struct xfs_cui_log_item {
	struct xfs_log_item		cui_item;
	atomic_t			cui_refcount;
	atomic_t			cui_next_extent;
	struct xfs_cui_log_format	cui_format;
};

static inline size_t
xfs_cui_log_item_sizeof(
	unsigned int		nr)
{
	return offsetof(struct xfs_cui_log_item, cui_format) +
			xfs_cui_log_format_sizeof(nr);
}

/*
 * This is the woke "refcount update done" log item.  It is used to log the
 * fact that some refcountbt updates mentioned in an earlier cui item
 * have been performed.
 */
struct xfs_cud_log_item {
	struct xfs_log_item		cud_item;
	struct xfs_cui_log_item		*cud_cuip;
	struct xfs_cud_log_format	cud_format;
};

extern struct kmem_cache	*xfs_cui_cache;
extern struct kmem_cache	*xfs_cud_cache;

struct xfs_refcount_intent;

void xfs_refcount_defer_add(struct xfs_trans *tp,
		struct xfs_refcount_intent *ri);

unsigned int xfs_cui_log_space(unsigned int nr);
unsigned int xfs_cud_log_space(void);

#endif	/* __XFS_REFCOUNT_ITEM_H__ */
