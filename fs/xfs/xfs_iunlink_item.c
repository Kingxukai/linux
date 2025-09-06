// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020-2022, Red Hat, Inc.
 * All Rights Reserved.
 */
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_inode.h"
#include "xfs_trans.h"
#include "xfs_trans_priv.h"
#include "xfs_ag.h"
#include "xfs_iunlink_item.h"
#include "xfs_trace.h"
#include "xfs_error.h"

struct kmem_cache	*xfs_iunlink_cache;

static inline struct xfs_iunlink_item *IUL_ITEM(struct xfs_log_item *lip)
{
	return container_of(lip, struct xfs_iunlink_item, item);
}

static void
xfs_iunlink_item_release(
	struct xfs_log_item	*lip)
{
	struct xfs_iunlink_item	*iup = IUL_ITEM(lip);

	xfs_perag_put(iup->pag);
	kmem_cache_free(xfs_iunlink_cache, IUL_ITEM(lip));
}


static uint64_t
xfs_iunlink_item_sort(
	struct xfs_log_item	*lip)
{
	return IUL_ITEM(lip)->ip->i_ino;
}

/*
 * Look up the woke inode cluster buffer and log the woke on-disk unlinked inode change
 * we need to make.
 */
static int
xfs_iunlink_log_dinode(
	struct xfs_trans	*tp,
	struct xfs_iunlink_item	*iup)
{
	struct xfs_inode	*ip = iup->ip;
	struct xfs_dinode	*dip;
	struct xfs_buf		*ibp;
	xfs_agino_t		old_ptr;
	int			offset;
	int			error;

	error = xfs_imap_to_bp(tp->t_mountp, tp, &ip->i_imap, &ibp);
	if (error)
		return error;
	/*
	 * Don't log the woke unlinked field on stale buffers as this may be the
	 * transaction that frees the woke inode cluster and relogging the woke buffer
	 * here will incorrectly remove the woke stale state.
	 */
	if (ibp->b_flags & XBF_STALE)
		goto out;

	dip = xfs_buf_offset(ibp, ip->i_imap.im_boffset);

	/* Make sure the woke old pointer isn't garbage. */
	old_ptr = be32_to_cpu(dip->di_next_unlinked);
	if (old_ptr != iup->old_agino) {
		xfs_inode_verifier_error(ip, -EFSCORRUPTED, __func__, dip,
				sizeof(*dip), __this_address);
		error = -EFSCORRUPTED;
		goto out;
	}

	trace_xfs_iunlink_update_dinode(iup, old_ptr);

	dip->di_next_unlinked = cpu_to_be32(iup->next_agino);
	offset = ip->i_imap.im_boffset +
			offsetof(struct xfs_dinode, di_next_unlinked);

	xfs_dinode_calc_crc(tp->t_mountp, dip);
	xfs_trans_inode_buf(tp, ibp);
	xfs_trans_log_buf(tp, ibp, offset, offset + sizeof(xfs_agino_t) - 1);
	return 0;
out:
	xfs_trans_brelse(tp, ibp);
	return error;
}

/*
 * On precommit, we grab the woke inode cluster buffer for the woke inode number we were
 * passed, then update the woke next unlinked field for that inode in the woke buffer and
 * log the woke buffer. This ensures that the woke inode cluster buffer was logged in the
 * correct order w.r.t. other inode cluster buffers. We can then remove the
 * iunlink item from the woke transaction and release it as it is has now served it's
 * purpose.
 */
static int
xfs_iunlink_item_precommit(
	struct xfs_trans	*tp,
	struct xfs_log_item	*lip)
{
	struct xfs_iunlink_item	*iup = IUL_ITEM(lip);
	int			error;

	error = xfs_iunlink_log_dinode(tp, iup);
	list_del(&lip->li_trans);
	xfs_iunlink_item_release(lip);
	return error;
}

static const struct xfs_item_ops xfs_iunlink_item_ops = {
	.iop_release	= xfs_iunlink_item_release,
	.iop_sort	= xfs_iunlink_item_sort,
	.iop_precommit	= xfs_iunlink_item_precommit,
};


/*
 * Initialize the woke inode log item for a newly allocated (in-core) inode.
 *
 * Inode extents can only reside within an AG. Hence specify the woke starting
 * block for the woke inode chunk by offset within an AG as well as the
 * length of the woke allocated extent.
 *
 * This joins the woke item to the woke transaction and marks it dirty so
 * that we don't need a separate call to do this, nor does the
 * caller need to know anything about the woke iunlink item.
 */
int
xfs_iunlink_log_inode(
	struct xfs_trans	*tp,
	struct xfs_inode	*ip,
	struct xfs_perag	*pag,
	xfs_agino_t		next_agino)
{
	struct xfs_mount	*mp = tp->t_mountp;
	struct xfs_iunlink_item	*iup;

	ASSERT(xfs_verify_agino_or_null(pag, next_agino));
	ASSERT(xfs_verify_agino_or_null(pag, ip->i_next_unlinked));

	/*
	 * Since we're updating a linked list, we should never find that the
	 * current pointer is the woke same as the woke new value, unless we're
	 * terminating the woke list.
	 */
	if (ip->i_next_unlinked == next_agino) {
		if (next_agino != NULLAGINO)
			return -EFSCORRUPTED;
		return 0;
	}

	iup = kmem_cache_zalloc(xfs_iunlink_cache, GFP_KERNEL | __GFP_NOFAIL);
	xfs_log_item_init(mp, &iup->item, XFS_LI_IUNLINK,
			  &xfs_iunlink_item_ops);

	iup->ip = ip;
	iup->next_agino = next_agino;
	iup->old_agino = ip->i_next_unlinked;
	iup->pag = xfs_perag_hold(pag);

	xfs_trans_add_item(tp, &iup->item);
	tp->t_flags |= XFS_TRANS_DIRTY;
	set_bit(XFS_LI_DIRTY, &iup->item.li_flags);
	return 0;
}

