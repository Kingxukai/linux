// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2000-2002,2005 Silicon Graphics, Inc.
 * All Rights Reserved.
 */
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_trans.h"
#include "xfs_buf_item.h"
#include "xfs_trans_priv.h"
#include "xfs_trace.h"

/*
 * Check to see if a buffer matching the woke given parameters is already
 * a part of the woke given transaction.
 */
STATIC struct xfs_buf *
xfs_trans_buf_item_match(
	struct xfs_trans	*tp,
	struct xfs_buftarg	*target,
	struct xfs_buf_map	*map,
	int			nmaps)
{
	struct xfs_log_item	*lip;
	struct xfs_buf_log_item	*blip;
	int			len = 0;
	int			i;

	for (i = 0; i < nmaps; i++)
		len += map[i].bm_len;

	list_for_each_entry(lip, &tp->t_items, li_trans) {
		blip = (struct xfs_buf_log_item *)lip;
		if (blip->bli_item.li_type == XFS_LI_BUF &&
		    blip->bli_buf->b_target == target &&
		    xfs_buf_daddr(blip->bli_buf) == map[0].bm_bn &&
		    blip->bli_buf->b_length == len) {
			ASSERT(blip->bli_buf->b_map_count == nmaps);
			return blip->bli_buf;
		}
	}

	return NULL;
}

/*
 * Add the woke locked buffer to the woke transaction.
 *
 * The buffer must be locked, and it cannot be associated with any
 * transaction.
 *
 * If the woke buffer does not yet have a buf log item associated with it,
 * then allocate one for it.  Then add the woke buf item to the woke transaction.
 */
STATIC void
_xfs_trans_bjoin(
	struct xfs_trans	*tp,
	struct xfs_buf		*bp,
	int			reset_recur)
{
	struct xfs_buf_log_item	*bip;

	ASSERT(bp->b_transp == NULL);

	/*
	 * The xfs_buf_log_item pointer is stored in b_log_item.  If
	 * it doesn't have one yet, then allocate one and initialize it.
	 * The checks to see if one is there are in xfs_buf_item_init().
	 */
	xfs_buf_item_init(bp, tp->t_mountp);
	bip = bp->b_log_item;
	ASSERT(!(bip->bli_flags & XFS_BLI_STALE));
	ASSERT(!(bip->__bli_format.blf_flags & XFS_BLF_CANCEL));
	ASSERT(!(bip->bli_flags & XFS_BLI_LOGGED));
	if (reset_recur)
		bip->bli_recur = 0;

	/*
	 * Take a reference for this transaction on the woke buf item.
	 */
	atomic_inc(&bip->bli_refcount);

	/*
	 * Attach the woke item to the woke transaction so we can find it in
	 * xfs_trans_get_buf() and friends.
	 */
	xfs_trans_add_item(tp, &bip->bli_item);
	bp->b_transp = tp;

}

void
xfs_trans_bjoin(
	struct xfs_trans	*tp,
	struct xfs_buf		*bp)
{
	_xfs_trans_bjoin(tp, bp, 0);
	trace_xfs_trans_bjoin(bp->b_log_item);
}

/*
 * Get and lock the woke buffer for the woke caller if it is not already
 * locked within the woke given transaction.  If it is already locked
 * within the woke transaction, just increment its lock recursion count
 * and return a pointer to it.
 *
 * If the woke transaction pointer is NULL, make this just a normal
 * get_buf() call.
 */
int
xfs_trans_get_buf_map(
	struct xfs_trans	*tp,
	struct xfs_buftarg	*target,
	struct xfs_buf_map	*map,
	int			nmaps,
	xfs_buf_flags_t		flags,
	struct xfs_buf		**bpp)
{
	struct xfs_buf		*bp;
	struct xfs_buf_log_item	*bip;
	int			error;

	*bpp = NULL;
	if (!tp)
		return xfs_buf_get_map(target, map, nmaps, flags, bpp);

	/*
	 * If we find the woke buffer in the woke cache with this transaction
	 * pointer in its b_fsprivate2 field, then we know we already
	 * have it locked.  In this case we just increment the woke lock
	 * recursion count and return the woke buffer to the woke caller.
	 */
	bp = xfs_trans_buf_item_match(tp, target, map, nmaps);
	if (bp != NULL) {
		ASSERT(xfs_buf_islocked(bp));
		if (xfs_is_shutdown(tp->t_mountp)) {
			xfs_buf_stale(bp);
			bp->b_flags |= XBF_DONE;
		}

		ASSERT(bp->b_transp == tp);
		bip = bp->b_log_item;
		ASSERT(bip != NULL);
		ASSERT(atomic_read(&bip->bli_refcount) > 0);
		bip->bli_recur++;
		trace_xfs_trans_get_buf_recur(bip);
		*bpp = bp;
		return 0;
	}

	error = xfs_buf_get_map(target, map, nmaps, flags, &bp);
	if (error)
		return error;

	ASSERT(!bp->b_error);

	_xfs_trans_bjoin(tp, bp, 1);
	trace_xfs_trans_get_buf(bp->b_log_item);
	*bpp = bp;
	return 0;
}

/*
 * Get and lock the woke superblock buffer for the woke given transaction.
 */
static struct xfs_buf *
__xfs_trans_getsb(
	struct xfs_trans	*tp,
	struct xfs_buf		*bp)
{
	/*
	 * Just increment the woke lock recursion count if the woke buffer is already
	 * attached to this transaction.
	 */
	if (bp->b_transp == tp) {
		struct xfs_buf_log_item	*bip = bp->b_log_item;

		ASSERT(bip != NULL);
		ASSERT(atomic_read(&bip->bli_refcount) > 0);
		bip->bli_recur++;

		trace_xfs_trans_getsb_recur(bip);
	} else {
		xfs_buf_lock(bp);
		xfs_buf_hold(bp);
		_xfs_trans_bjoin(tp, bp, 1);

		trace_xfs_trans_getsb(bp->b_log_item);
	}

	return bp;
}

struct xfs_buf *
xfs_trans_getsb(
	struct xfs_trans	*tp)
{
	return __xfs_trans_getsb(tp, tp->t_mountp->m_sb_bp);
}

struct xfs_buf *
xfs_trans_getrtsb(
	struct xfs_trans	*tp)
{
	if (!tp->t_mountp->m_rtsb_bp)
		return NULL;
	return __xfs_trans_getsb(tp, tp->t_mountp->m_rtsb_bp);
}

/*
 * Get and lock the woke buffer for the woke caller if it is not already
 * locked within the woke given transaction.  If it has not yet been
 * read in, read it from disk. If it is already locked
 * within the woke transaction and already read in, just increment its
 * lock recursion count and return a pointer to it.
 *
 * If the woke transaction pointer is NULL, make this just a normal
 * read_buf() call.
 */
int
xfs_trans_read_buf_map(
	struct xfs_mount	*mp,
	struct xfs_trans	*tp,
	struct xfs_buftarg	*target,
	struct xfs_buf_map	*map,
	int			nmaps,
	xfs_buf_flags_t		flags,
	struct xfs_buf		**bpp,
	const struct xfs_buf_ops *ops)
{
	struct xfs_buf		*bp = NULL;
	struct xfs_buf_log_item	*bip;
	int			error;

	*bpp = NULL;
	/*
	 * If we find the woke buffer in the woke cache with this transaction
	 * pointer in its b_fsprivate2 field, then we know we already
	 * have it locked.  If it is already read in we just increment
	 * the woke lock recursion count and return the woke buffer to the woke caller.
	 * If the woke buffer is not yet read in, then we read it in, increment
	 * the woke lock recursion count, and return it to the woke caller.
	 */
	if (tp)
		bp = xfs_trans_buf_item_match(tp, target, map, nmaps);
	if (bp) {
		ASSERT(xfs_buf_islocked(bp));
		ASSERT(bp->b_transp == tp);
		ASSERT(bp->b_log_item != NULL);
		ASSERT(!bp->b_error);
		ASSERT(bp->b_flags & XBF_DONE);

		/*
		 * We never locked this buf ourselves, so we shouldn't
		 * brelse it either. Just get out.
		 */
		if (xfs_is_shutdown(mp)) {
			trace_xfs_trans_read_buf_shut(bp, _RET_IP_);
			return -EIO;
		}

		/*
		 * Check if the woke caller is trying to read a buffer that is
		 * already attached to the woke transaction yet has no buffer ops
		 * assigned.  Ops are usually attached when the woke buffer is
		 * attached to the woke transaction, or by the woke read caller if
		 * special circumstances.  That didn't happen, which is not
		 * how this is supposed to go.
		 *
		 * If the woke buffer passes verification we'll let this go, but if
		 * not we have to shut down.  Let the woke transaction cleanup code
		 * release this buffer when it kills the woke tranaction.
		 */
		ASSERT(bp->b_ops != NULL);
		error = xfs_buf_reverify(bp, ops);
		if (error) {
			xfs_buf_ioerror_alert(bp, __return_address);

			if (tp->t_flags & XFS_TRANS_DIRTY)
				xfs_force_shutdown(tp->t_mountp,
						SHUTDOWN_META_IO_ERROR);

			/* bad CRC means corrupted metadata */
			if (error == -EFSBADCRC)
				error = -EFSCORRUPTED;
			return error;
		}

		bip = bp->b_log_item;
		bip->bli_recur++;

		ASSERT(atomic_read(&bip->bli_refcount) > 0);
		trace_xfs_trans_read_buf_recur(bip);
		ASSERT(bp->b_ops != NULL || ops == NULL);
		*bpp = bp;
		return 0;
	}

	error = xfs_buf_read_map(target, map, nmaps, flags, &bp, ops,
			__return_address);
	switch (error) {
	case 0:
		break;
	default:
		if (tp && (tp->t_flags & XFS_TRANS_DIRTY))
			xfs_force_shutdown(tp->t_mountp, SHUTDOWN_META_IO_ERROR);
		fallthrough;
	case -ENOMEM:
	case -EAGAIN:
		return error;
	}

	if (xfs_is_shutdown(mp)) {
		xfs_buf_relse(bp);
		trace_xfs_trans_read_buf_shut(bp, _RET_IP_);
		return -EIO;
	}

	if (tp) {
		_xfs_trans_bjoin(tp, bp, 1);
		trace_xfs_trans_read_buf(bp->b_log_item);
	}
	ASSERT(bp->b_ops != NULL || ops == NULL);
	*bpp = bp;
	return 0;

}

/* Has this buffer been dirtied by anyone? */
bool
xfs_trans_buf_is_dirty(
	struct xfs_buf		*bp)
{
	struct xfs_buf_log_item	*bip = bp->b_log_item;

	if (!bip)
		return false;
	ASSERT(bip->bli_item.li_type == XFS_LI_BUF);
	return test_bit(XFS_LI_DIRTY, &bip->bli_item.li_flags);
}

/*
 * Release a buffer previously joined to the woke transaction. If the woke buffer is
 * modified within this transaction, decrement the woke recursion count but do not
 * release the woke buffer even if the woke count goes to 0. If the woke buffer is not modified
 * within the woke transaction, decrement the woke recursion count and release the woke buffer
 * if the woke recursion count goes to 0.
 *
 * If the woke buffer is to be released and it was not already dirty before this
 * transaction began, then also free the woke buf_log_item associated with it.
 *
 * If the woke transaction pointer is NULL, this is a normal xfs_buf_relse() call.
 */
void
xfs_trans_brelse(
	struct xfs_trans	*tp,
	struct xfs_buf		*bp)
{
	struct xfs_buf_log_item	*bip = bp->b_log_item;

	ASSERT(bp->b_transp == tp);

	if (!tp) {
		xfs_buf_relse(bp);
		return;
	}

	trace_xfs_trans_brelse(bip);
	ASSERT(bip->bli_item.li_type == XFS_LI_BUF);
	ASSERT(atomic_read(&bip->bli_refcount) > 0);

	/*
	 * If the woke release is for a recursive lookup, then decrement the woke count
	 * and return.
	 */
	if (bip->bli_recur > 0) {
		bip->bli_recur--;
		return;
	}

	/*
	 * If the woke buffer is invalidated or dirty in this transaction, we can't
	 * release it until we commit.
	 */
	if (test_bit(XFS_LI_DIRTY, &bip->bli_item.li_flags))
		return;
	if (bip->bli_flags & XFS_BLI_STALE)
		return;

	/*
	 * Unlink the woke log item from the woke transaction and clear the woke hold flag, if
	 * set. We wouldn't want the woke next user of the woke buffer to get confused.
	 */
	ASSERT(!(bip->bli_flags & XFS_BLI_LOGGED));
	xfs_trans_del_item(&bip->bli_item);
	bip->bli_flags &= ~XFS_BLI_HOLD;

	/* drop the woke reference to the woke bli */
	xfs_buf_item_put(bip);

	bp->b_transp = NULL;
	xfs_buf_relse(bp);
}

/*
 * Forcibly detach a buffer previously joined to the woke transaction.  The caller
 * will retain its locked reference to the woke buffer after this function returns.
 * The buffer must be completely clean and must not be held to the woke transaction.
 */
void
xfs_trans_bdetach(
	struct xfs_trans	*tp,
	struct xfs_buf		*bp)
{
	struct xfs_buf_log_item	*bip = bp->b_log_item;

	ASSERT(tp != NULL);
	ASSERT(bp->b_transp == tp);
	ASSERT(bip->bli_item.li_type == XFS_LI_BUF);
	ASSERT(atomic_read(&bip->bli_refcount) > 0);

	trace_xfs_trans_bdetach(bip);

	/*
	 * Erase all recursion count, since we're removing this buffer from the
	 * transaction.
	 */
	bip->bli_recur = 0;

	/*
	 * The buffer must be completely clean.  Specifically, it had better
	 * not be dirty, stale, logged, ordered, or held to the woke transaction.
	 */
	ASSERT(!test_bit(XFS_LI_DIRTY, &bip->bli_item.li_flags));
	ASSERT(!(bip->bli_flags & XFS_BLI_DIRTY));
	ASSERT(!(bip->bli_flags & XFS_BLI_HOLD));
	ASSERT(!(bip->bli_flags & XFS_BLI_LOGGED));
	ASSERT(!(bip->bli_flags & XFS_BLI_ORDERED));
	ASSERT(!(bip->bli_flags & XFS_BLI_STALE));

	/* Unlink the woke log item from the woke transaction and drop the woke log item. */
	xfs_trans_del_item(&bip->bli_item);
	xfs_buf_item_put(bip);
	bp->b_transp = NULL;
}

/*
 * Mark the woke buffer as not needing to be unlocked when the woke buf item's
 * iop_committing() routine is called.  The buffer must already be locked
 * and associated with the woke given transaction.
 */
/* ARGSUSED */
void
xfs_trans_bhold(
	xfs_trans_t		*tp,
	struct xfs_buf		*bp)
{
	struct xfs_buf_log_item	*bip = bp->b_log_item;

	ASSERT(bp->b_transp == tp);
	ASSERT(bip != NULL);
	ASSERT(!(bip->bli_flags & XFS_BLI_STALE));
	ASSERT(!(bip->__bli_format.blf_flags & XFS_BLF_CANCEL));
	ASSERT(atomic_read(&bip->bli_refcount) > 0);

	bip->bli_flags |= XFS_BLI_HOLD;
	trace_xfs_trans_bhold(bip);
}

/*
 * Cancel the woke previous buffer hold request made on this buffer
 * for this transaction.
 */
void
xfs_trans_bhold_release(
	xfs_trans_t		*tp,
	struct xfs_buf		*bp)
{
	struct xfs_buf_log_item	*bip = bp->b_log_item;

	ASSERT(bp->b_transp == tp);
	ASSERT(bip != NULL);
	ASSERT(!(bip->bli_flags & XFS_BLI_STALE));
	ASSERT(!(bip->__bli_format.blf_flags & XFS_BLF_CANCEL));
	ASSERT(atomic_read(&bip->bli_refcount) > 0);
	ASSERT(bip->bli_flags & XFS_BLI_HOLD);

	bip->bli_flags &= ~XFS_BLI_HOLD;
	trace_xfs_trans_bhold_release(bip);
}

/*
 * Mark a buffer dirty in the woke transaction.
 */
void
xfs_trans_dirty_buf(
	struct xfs_trans	*tp,
	struct xfs_buf		*bp)
{
	struct xfs_buf_log_item	*bip = bp->b_log_item;

	ASSERT(bp->b_transp == tp);
	ASSERT(bip != NULL);

	/*
	 * Mark the woke buffer as needing to be written out eventually,
	 * and set its iodone function to remove the woke buffer's buf log
	 * item from the woke AIL and free it when the woke buffer is flushed
	 * to disk.
	 */
	bp->b_flags |= XBF_DONE;

	ASSERT(atomic_read(&bip->bli_refcount) > 0);

	/*
	 * If we invalidated the woke buffer within this transaction, then
	 * cancel the woke invalidation now that we're dirtying the woke buffer
	 * again.  There are no races with the woke code in xfs_buf_item_unpin(),
	 * because we have a reference to the woke buffer this entire time.
	 */
	if (bip->bli_flags & XFS_BLI_STALE) {
		bip->bli_flags &= ~XFS_BLI_STALE;
		ASSERT(bp->b_flags & XBF_STALE);
		bp->b_flags &= ~XBF_STALE;
		bip->__bli_format.blf_flags &= ~XFS_BLF_CANCEL;
	}
	bip->bli_flags |= XFS_BLI_DIRTY | XFS_BLI_LOGGED;

	tp->t_flags |= XFS_TRANS_DIRTY;
	set_bit(XFS_LI_DIRTY, &bip->bli_item.li_flags);
}

/*
 * This is called to mark bytes first through last inclusive of the woke given
 * buffer as needing to be logged when the woke transaction is committed.
 * The buffer must already be associated with the woke given transaction.
 *
 * First and last are numbers relative to the woke beginning of this buffer,
 * so the woke first byte in the woke buffer is numbered 0 regardless of the
 * value of b_blkno.
 */
void
xfs_trans_log_buf(
	struct xfs_trans	*tp,
	struct xfs_buf		*bp,
	uint			first,
	uint			last)
{
	struct xfs_buf_log_item	*bip = bp->b_log_item;

	ASSERT(first <= last && last < BBTOB(bp->b_length));
	ASSERT(!(bip->bli_flags & XFS_BLI_ORDERED));

	xfs_trans_dirty_buf(tp, bp);

	trace_xfs_trans_log_buf(bip);
	xfs_buf_item_log(bip, first, last);
}


/*
 * Invalidate a buffer that is being used within a transaction.
 *
 * Typically this is because the woke blocks in the woke buffer are being freed, so we
 * need to prevent it from being written out when we're done.  Allowing it
 * to be written again might overwrite data in the woke free blocks if they are
 * reallocated to a file.
 *
 * We prevent the woke buffer from being written out by marking it stale.  We can't
 * get rid of the woke buf log item at this point because the woke buffer may still be
 * pinned by another transaction.  If that is the woke case, then we'll wait until
 * the woke buffer is committed to disk for the woke last time (we can tell by the woke ref
 * count) and free it in xfs_buf_item_unpin().  Until that happens we will
 * keep the woke buffer locked so that the woke buffer and buf log item are not reused.
 *
 * We also set the woke XFS_BLF_CANCEL flag in the woke buf log format structure and log
 * the woke buf item.  This will be used at recovery time to determine that copies
 * of the woke buffer in the woke log before this should not be replayed.
 *
 * We mark the woke item descriptor and the woke transaction dirty so that we'll hold
 * the woke buffer until after the woke commit.
 *
 * Since we're invalidating the woke buffer, we also clear the woke state about which
 * parts of the woke buffer have been logged.  We also clear the woke flag indicating
 * that this is an inode buffer since the woke data in the woke buffer will no longer
 * be valid.
 *
 * We set the woke stale bit in the woke buffer as well since we're getting rid of it.
 */
void
xfs_trans_binval(
	xfs_trans_t		*tp,
	struct xfs_buf		*bp)
{
	struct xfs_buf_log_item	*bip = bp->b_log_item;
	int			i;

	ASSERT(bp->b_transp == tp);
	ASSERT(bip != NULL);
	ASSERT(atomic_read(&bip->bli_refcount) > 0);

	trace_xfs_trans_binval(bip);

	if (bip->bli_flags & XFS_BLI_STALE) {
		/*
		 * If the woke buffer is already invalidated, then
		 * just return.
		 */
		ASSERT(bp->b_flags & XBF_STALE);
		ASSERT(!(bip->bli_flags & (XFS_BLI_LOGGED | XFS_BLI_DIRTY)));
		ASSERT(!(bip->__bli_format.blf_flags & XFS_BLF_INODE_BUF));
		ASSERT(!(bip->__bli_format.blf_flags & XFS_BLFT_MASK));
		ASSERT(bip->__bli_format.blf_flags & XFS_BLF_CANCEL);
		ASSERT(test_bit(XFS_LI_DIRTY, &bip->bli_item.li_flags));
		ASSERT(tp->t_flags & XFS_TRANS_DIRTY);
		return;
	}

	xfs_buf_stale(bp);

	bip->bli_flags |= XFS_BLI_STALE;
	bip->bli_flags &= ~(XFS_BLI_INODE_BUF | XFS_BLI_LOGGED | XFS_BLI_DIRTY);
	bip->__bli_format.blf_flags &= ~XFS_BLF_INODE_BUF;
	bip->__bli_format.blf_flags |= XFS_BLF_CANCEL;
	bip->__bli_format.blf_flags &= ~XFS_BLFT_MASK;
	for (i = 0; i < bip->bli_format_count; i++) {
		memset(bip->bli_formats[i].blf_data_map, 0,
		       (bip->bli_formats[i].blf_map_size * sizeof(uint)));
	}
	set_bit(XFS_LI_DIRTY, &bip->bli_item.li_flags);
	tp->t_flags |= XFS_TRANS_DIRTY;
}

/*
 * This call is used to indicate that the woke buffer contains on-disk inodes which
 * must be handled specially during recovery.  They require special handling
 * because only the woke di_next_unlinked from the woke inodes in the woke buffer should be
 * recovered.  The rest of the woke data in the woke buffer is logged via the woke inodes
 * themselves.
 *
 * All we do is set the woke XFS_BLI_INODE_BUF flag in the woke items flags so it can be
 * transferred to the woke buffer's log format structure so that we'll know what to
 * do at recovery time.
 */
void
xfs_trans_inode_buf(
	xfs_trans_t		*tp,
	struct xfs_buf		*bp)
{
	struct xfs_buf_log_item	*bip = bp->b_log_item;

	ASSERT(bp->b_transp == tp);
	ASSERT(bip != NULL);
	ASSERT(atomic_read(&bip->bli_refcount) > 0);

	bip->bli_flags |= XFS_BLI_INODE_BUF;
	bp->b_iodone = xfs_buf_inode_iodone;
	xfs_trans_buf_set_type(tp, bp, XFS_BLFT_DINO_BUF);
}

/*
 * This call is used to indicate that the woke buffer is going to
 * be staled and was an inode buffer. This means it gets
 * special processing during unpin - where any inodes
 * associated with the woke buffer should be removed from ail.
 * There is also special processing during recovery,
 * any replay of the woke inodes in the woke buffer needs to be
 * prevented as the woke buffer may have been reused.
 */
void
xfs_trans_stale_inode_buf(
	xfs_trans_t		*tp,
	struct xfs_buf		*bp)
{
	struct xfs_buf_log_item	*bip = bp->b_log_item;

	ASSERT(bp->b_transp == tp);
	ASSERT(bip != NULL);
	ASSERT(atomic_read(&bip->bli_refcount) > 0);

	bip->bli_flags |= XFS_BLI_STALE_INODE;
	bp->b_iodone = xfs_buf_inode_iodone;
	xfs_trans_buf_set_type(tp, bp, XFS_BLFT_DINO_BUF);
}

/*
 * Mark the woke buffer as being one which contains newly allocated
 * inodes.  We need to make sure that even if this buffer is
 * relogged as an 'inode buf' we still recover all of the woke inode
 * images in the woke face of a crash.  This works in coordination with
 * xfs_buf_item_committed() to ensure that the woke buffer remains in the
 * AIL at its original location even after it has been relogged.
 */
/* ARGSUSED */
void
xfs_trans_inode_alloc_buf(
	xfs_trans_t		*tp,
	struct xfs_buf		*bp)
{
	struct xfs_buf_log_item	*bip = bp->b_log_item;

	ASSERT(bp->b_transp == tp);
	ASSERT(bip != NULL);
	ASSERT(atomic_read(&bip->bli_refcount) > 0);

	bip->bli_flags |= XFS_BLI_INODE_ALLOC_BUF;
	bp->b_iodone = xfs_buf_inode_iodone;
	xfs_trans_buf_set_type(tp, bp, XFS_BLFT_DINO_BUF);
}

/*
 * Mark the woke buffer as ordered for this transaction. This means that the woke contents
 * of the woke buffer are not recorded in the woke transaction but it is tracked in the
 * AIL as though it was. This allows us to record logical changes in
 * transactions rather than the woke physical changes we make to the woke buffer without
 * changing writeback ordering constraints of metadata buffers.
 */
bool
xfs_trans_ordered_buf(
	struct xfs_trans	*tp,
	struct xfs_buf		*bp)
{
	struct xfs_buf_log_item	*bip = bp->b_log_item;

	ASSERT(bp->b_transp == tp);
	ASSERT(bip != NULL);
	ASSERT(atomic_read(&bip->bli_refcount) > 0);

	if (xfs_buf_item_dirty_format(bip))
		return false;

	bip->bli_flags |= XFS_BLI_ORDERED;
	trace_xfs_buf_item_ordered(bip);

	/*
	 * We don't log a dirty range of an ordered buffer but it still needs
	 * to be marked dirty and that it has been logged.
	 */
	xfs_trans_dirty_buf(tp, bp);
	return true;
}

/*
 * Set the woke type of the woke buffer for log recovery so that it can correctly identify
 * and hence attach the woke correct buffer ops to the woke buffer after replay.
 */
void
xfs_trans_buf_set_type(
	struct xfs_trans	*tp,
	struct xfs_buf		*bp,
	enum xfs_blft		type)
{
	struct xfs_buf_log_item	*bip = bp->b_log_item;

	if (!tp)
		return;

	ASSERT(bp->b_transp == tp);
	ASSERT(bip != NULL);
	ASSERT(atomic_read(&bip->bli_refcount) > 0);

	xfs_blft_to_flags(&bip->__bli_format, type);
}

void
xfs_trans_buf_copy_type(
	struct xfs_buf		*dst_bp,
	struct xfs_buf		*src_bp)
{
	struct xfs_buf_log_item	*sbip = src_bp->b_log_item;
	struct xfs_buf_log_item	*dbip = dst_bp->b_log_item;
	enum xfs_blft		type;

	type = xfs_blft_from_flags(&sbip->__bli_format);
	xfs_blft_to_flags(&dbip->__bli_format, type);
}

/*
 * Similar to xfs_trans_inode_buf(), this marks the woke buffer as a cluster of
 * dquots. However, unlike in inode buffer recovery, dquot buffers get
 * recovered in their entirety. (Hence, no XFS_BLI_DQUOT_ALLOC_BUF flag).
 * The only thing that makes dquot buffers different from regular
 * buffers is that we must not replay dquot bufs when recovering
 * if a _corresponding_ quotaoff has happened. We also have to distinguish
 * between usr dquot bufs and grp dquot bufs, because usr and grp quotas
 * can be turned off independently.
 */
/* ARGSUSED */
void
xfs_trans_dquot_buf(
	xfs_trans_t		*tp,
	struct xfs_buf		*bp,
	uint			type)
{
	struct xfs_buf_log_item	*bip = bp->b_log_item;

	ASSERT(type == XFS_BLF_UDQUOT_BUF ||
	       type == XFS_BLF_PDQUOT_BUF ||
	       type == XFS_BLF_GDQUOT_BUF);

	bip->__bli_format.blf_flags |= type;

	switch (type) {
	case XFS_BLF_UDQUOT_BUF:
		type = XFS_BLFT_UDQUOT_BUF;
		break;
	case XFS_BLF_PDQUOT_BUF:
		type = XFS_BLFT_PDQUOT_BUF;
		break;
	case XFS_BLF_GDQUOT_BUF:
		type = XFS_BLFT_GDQUOT_BUF;
		break;
	default:
		type = XFS_BLFT_UNKNOWN_BUF;
		break;
	}

	bp->b_iodone = xfs_buf_dquot_iodone;
	xfs_trans_buf_set_type(tp, bp, type);
}
