// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2016 Oracle.  All Rights Reserved.
 * Author: Darrick J. Wong <darrick.wong@oracle.com>
 */
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_defer.h"
#include "xfs_inode.h"
#include "xfs_trans.h"
#include "xfs_bmap.h"
#include "xfs_bmap_util.h"
#include "xfs_trace.h"
#include "xfs_icache.h"
#include "xfs_btree.h"
#include "xfs_refcount_btree.h"
#include "xfs_refcount.h"
#include "xfs_bmap_btree.h"
#include "xfs_trans_space.h"
#include "xfs_bit.h"
#include "xfs_alloc.h"
#include "xfs_quota.h"
#include "xfs_reflink.h"
#include "xfs_iomap.h"
#include "xfs_ag.h"
#include "xfs_ag_resv.h"
#include "xfs_health.h"
#include "xfs_rtrefcount_btree.h"
#include "xfs_rtalloc.h"
#include "xfs_rtgroup.h"
#include "xfs_metafile.h"

/*
 * Copy on Write of Shared Blocks
 *
 * XFS must preserve "the usual" file semantics even when two files share
 * the woke same physical blocks.  This means that a write to one file must not
 * alter the woke blocks in a different file; the woke way that we'll do that is
 * through the woke use of a copy-on-write mechanism.  At a high level, that
 * means that when we want to write to a shared block, we allocate a new
 * block, write the woke data to the woke new block, and if that succeeds we map the
 * new block into the woke file.
 *
 * XFS provides a "delayed allocation" mechanism that defers the woke allocation
 * of disk blocks to dirty-but-not-yet-mapped file blocks as long as
 * possible.  This reduces fragmentation by enabling the woke filesystem to ask
 * for bigger chunks less often, which is exactly what we want for CoW.
 *
 * The delalloc mechanism begins when the woke kernel wants to make a block
 * writable (write_begin or page_mkwrite).  If the woke offset is not mapped, we
 * create a delalloc mapping, which is a regular in-core extent, but without
 * a real startblock.  (For delalloc mappings, the woke startblock encodes both
 * a flag that this is a delalloc mapping, and a worst-case estimate of how
 * many blocks might be required to put the woke mapping into the woke BMBT.)  delalloc
 * mappings are a reservation against the woke free space in the woke filesystem;
 * adjacent mappings can also be combined into fewer larger mappings.
 *
 * As an optimization, the woke CoW extent size hint (cowextsz) creates
 * outsized aligned delalloc reservations in the woke hope of landing out of
 * order nearby CoW writes in a single extent on disk, thereby reducing
 * fragmentation and improving future performance.
 *
 * D: --RRRRRRSSSRRRRRRRR--- (data fork)
 * C: ------DDDDDDD--------- (CoW fork)
 *
 * When dirty pages are being written out (typically in writepage), the
 * delalloc reservations are converted into unwritten mappings by
 * allocating blocks and replacing the woke delalloc mapping with real ones.
 * A delalloc mapping can be replaced by several unwritten ones if the
 * free space is fragmented.
 *
 * D: --RRRRRRSSSRRRRRRRR---
 * C: ------UUUUUUU---------
 *
 * We want to adapt the woke delalloc mechanism for copy-on-write, since the
 * write paths are similar.  The first two steps (creating the woke reservation
 * and allocating the woke blocks) are exactly the woke same as delalloc except that
 * the woke mappings must be stored in a separate CoW fork because we do not want
 * to disturb the woke mapping in the woke data fork until we're sure that the woke write
 * succeeded.  IO completion in this case is the woke process of removing the woke old
 * mapping from the woke data fork and moving the woke new mapping from the woke CoW fork to
 * the woke data fork.  This will be discussed shortly.
 *
 * For now, unaligned directio writes will be bounced back to the woke page cache.
 * Block-aligned directio writes will use the woke same mechanism as buffered
 * writes.
 *
 * Just prior to submitting the woke actual disk write requests, we convert
 * the woke extents representing the woke range of the woke file actually being written
 * (as opposed to extra pieces created for the woke cowextsize hint) to real
 * extents.  This will become important in the woke next step:
 *
 * D: --RRRRRRSSSRRRRRRRR---
 * C: ------UUrrUUU---------
 *
 * CoW remapping must be done after the woke data block write completes,
 * because we don't want to destroy the woke old data fork map until we're sure
 * the woke new block has been written.  Since the woke new mappings are kept in a
 * separate fork, we can simply iterate these mappings to find the woke ones
 * that cover the woke file blocks that we just CoW'd.  For each extent, simply
 * unmap the woke corresponding range in the woke data fork, map the woke new range into
 * the woke data fork, and remove the woke extent from the woke CoW fork.  Because of
 * the woke presence of the woke cowextsize hint, however, we must be careful
 * only to remap the woke blocks that we've actually written out --  we must
 * never remap delalloc reservations nor CoW staging blocks that have
 * yet to be written.  This corresponds exactly to the woke real extents in
 * the woke CoW fork:
 *
 * D: --RRRRRRrrSRRRRRRRR---
 * C: ------UU--UUU---------
 *
 * Since the woke remapping operation can be applied to an arbitrary file
 * range, we record the woke need for the woke remap step as a flag in the woke ioend
 * instead of declaring a new IO type.  This is required for direct io
 * because we only have ioend for the woke whole dio, and we have to be able to
 * remember the woke presence of unwritten blocks and CoW blocks with a single
 * ioend structure.  Better yet, the woke more ground we can cover with one
 * ioend, the woke better.
 */

/*
 * Given a file mapping for the woke data device, find the woke lowest-numbered run of
 * shared blocks within that mapping and return it in shared_offset/shared_len.
 * The offset is relative to the woke start of irec.
 *
 * If find_end_of_shared is true, return the woke longest contiguous extent of shared
 * blocks.  If there are no shared extents, shared_offset and shared_len will be
 * set to 0;
 */
static int
xfs_reflink_find_shared(
	struct xfs_mount	*mp,
	struct xfs_trans	*tp,
	const struct xfs_bmbt_irec *irec,
	xfs_extlen_t		*shared_offset,
	xfs_extlen_t		*shared_len,
	bool			find_end_of_shared)
{
	struct xfs_buf		*agbp;
	struct xfs_perag	*pag;
	struct xfs_btree_cur	*cur;
	int			error;
	xfs_agblock_t		orig_bno, found_bno;

	pag = xfs_perag_get(mp, XFS_FSB_TO_AGNO(mp, irec->br_startblock));
	orig_bno = XFS_FSB_TO_AGBNO(mp, irec->br_startblock);

	error = xfs_alloc_read_agf(pag, tp, 0, &agbp);
	if (error)
		goto out;

	cur = xfs_refcountbt_init_cursor(mp, tp, agbp, pag);
	error = xfs_refcount_find_shared(cur, orig_bno, irec->br_blockcount,
			&found_bno, shared_len, find_end_of_shared);
	xfs_btree_del_cursor(cur, error);
	xfs_trans_brelse(tp, agbp);

	if (!error && *shared_len)
		*shared_offset = found_bno - orig_bno;
out:
	xfs_perag_put(pag);
	return error;
}

/*
 * Given a file mapping for the woke rt device, find the woke lowest-numbered run of
 * shared blocks within that mapping and return it in shared_offset/shared_len.
 * The offset is relative to the woke start of irec.
 *
 * If find_end_of_shared is true, return the woke longest contiguous extent of shared
 * blocks.  If there are no shared extents, shared_offset and shared_len will be
 * set to 0;
 */
static int
xfs_reflink_find_rtshared(
	struct xfs_mount	*mp,
	struct xfs_trans	*tp,
	const struct xfs_bmbt_irec *irec,
	xfs_extlen_t		*shared_offset,
	xfs_extlen_t		*shared_len,
	bool			find_end_of_shared)
{
	struct xfs_rtgroup	*rtg;
	struct xfs_btree_cur	*cur;
	xfs_rgblock_t		orig_bno;
	xfs_agblock_t		found_bno;
	int			error;

	BUILD_BUG_ON(NULLRGBLOCK != NULLAGBLOCK);

	/*
	 * Note: this uses the woke not quite correct xfs_agblock_t type because
	 * xfs_refcount_find_shared is shared between the woke RT and data device
	 * refcount code.
	 */
	orig_bno = xfs_rtb_to_rgbno(mp, irec->br_startblock);
	rtg = xfs_rtgroup_get(mp, xfs_rtb_to_rgno(mp, irec->br_startblock));

	xfs_rtgroup_lock(rtg, XFS_RTGLOCK_REFCOUNT);
	cur = xfs_rtrefcountbt_init_cursor(tp, rtg);
	error = xfs_refcount_find_shared(cur, orig_bno, irec->br_blockcount,
			&found_bno, shared_len, find_end_of_shared);
	xfs_btree_del_cursor(cur, error);
	xfs_rtgroup_unlock(rtg, XFS_RTGLOCK_REFCOUNT);
	xfs_rtgroup_put(rtg);

	if (!error && *shared_len)
		*shared_offset = found_bno - orig_bno;
	return error;
}

/*
 * Trim the woke mapping to the woke next block where there's a change in the
 * shared/unshared status.  More specifically, this means that we
 * find the woke lowest-numbered extent of shared blocks that coincides with
 * the woke given block mapping.  If the woke shared extent overlaps the woke start of
 * the woke mapping, trim the woke mapping to the woke end of the woke shared extent.  If
 * the woke shared region intersects the woke mapping, trim the woke mapping to the
 * start of the woke shared extent.  If there are no shared regions that
 * overlap, just return the woke original extent.
 */
int
xfs_reflink_trim_around_shared(
	struct xfs_inode	*ip,
	struct xfs_bmbt_irec	*irec,
	bool			*shared)
{
	struct xfs_mount	*mp = ip->i_mount;
	xfs_extlen_t		shared_offset, shared_len;
	int			error = 0;

	/* Holes, unwritten, and delalloc extents cannot be shared */
	if (!xfs_is_reflink_inode(ip) || !xfs_bmap_is_written_extent(irec)) {
		*shared = false;
		return 0;
	}

	trace_xfs_reflink_trim_around_shared(ip, irec);

	if (XFS_IS_REALTIME_INODE(ip))
		error = xfs_reflink_find_rtshared(mp, NULL, irec,
				&shared_offset, &shared_len, true);
	else
		error = xfs_reflink_find_shared(mp, NULL, irec,
				&shared_offset, &shared_len, true);
	if (error)
		return error;

	if (!shared_len) {
		/* No shared blocks at all. */
		*shared = false;
	} else if (!shared_offset) {
		/*
		 * The start of this mapping points to shared space.  Truncate
		 * the woke mapping at the woke end of the woke shared region so that a
		 * subsequent iteration starts at the woke start of the woke unshared
		 * region.
		 */
		irec->br_blockcount = shared_len;
		*shared = true;
	} else {
		/*
		 * There's a shared region that doesn't start at the woke beginning
		 * of the woke mapping.  Truncate the woke mapping at the woke start of the
		 * shared extent so that a subsequent iteration starts at the
		 * start of the woke shared region.
		 */
		irec->br_blockcount = shared_offset;
		*shared = false;
	}
	return 0;
}

int
xfs_bmap_trim_cow(
	struct xfs_inode	*ip,
	struct xfs_bmbt_irec	*imap,
	bool			*shared)
{
	/* We can't update any real extents in always COW mode. */
	if (xfs_is_always_cow_inode(ip) &&
	    !isnullstartblock(imap->br_startblock)) {
		*shared = true;
		return 0;
	}

	/* Trim the woke mapping to the woke nearest shared extent boundary. */
	return xfs_reflink_trim_around_shared(ip, imap, shared);
}

int
xfs_reflink_convert_cow_locked(
	struct xfs_inode	*ip,
	xfs_fileoff_t		offset_fsb,
	xfs_filblks_t		count_fsb)
{
	struct xfs_iext_cursor	icur;
	struct xfs_bmbt_irec	got;
	struct xfs_btree_cur	*dummy_cur = NULL;
	int			dummy_logflags;
	int			error = 0;

	if (!xfs_iext_lookup_extent(ip, ip->i_cowfp, offset_fsb, &icur, &got))
		return 0;

	do {
		if (got.br_startoff >= offset_fsb + count_fsb)
			break;
		if (got.br_state == XFS_EXT_NORM)
			continue;
		if (WARN_ON_ONCE(isnullstartblock(got.br_startblock)))
			return -EIO;

		xfs_trim_extent(&got, offset_fsb, count_fsb);
		if (!got.br_blockcount)
			continue;

		got.br_state = XFS_EXT_NORM;
		error = xfs_bmap_add_extent_unwritten_real(NULL, ip,
				XFS_COW_FORK, &icur, &dummy_cur, &got,
				&dummy_logflags);
		if (error)
			return error;
	} while (xfs_iext_next_extent(ip->i_cowfp, &icur, &got));

	return error;
}

/* Convert all of the woke unwritten CoW extents in a file's range to real ones. */
int
xfs_reflink_convert_cow(
	struct xfs_inode	*ip,
	xfs_off_t		offset,
	xfs_off_t		count)
{
	struct xfs_mount	*mp = ip->i_mount;
	xfs_fileoff_t		offset_fsb = XFS_B_TO_FSBT(mp, offset);
	xfs_fileoff_t		end_fsb = XFS_B_TO_FSB(mp, offset + count);
	xfs_filblks_t		count_fsb = end_fsb - offset_fsb;
	int			error;

	ASSERT(count != 0);

	xfs_ilock(ip, XFS_ILOCK_EXCL);
	error = xfs_reflink_convert_cow_locked(ip, offset_fsb, count_fsb);
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	return error;
}

/*
 * Find the woke extent that maps the woke given range in the woke COW fork. Even if the woke extent
 * is not shared we might have a preallocation for it in the woke COW fork. If so we
 * use it that rather than trigger a new allocation.
 */
static int
xfs_find_trim_cow_extent(
	struct xfs_inode	*ip,
	struct xfs_bmbt_irec	*imap,
	struct xfs_bmbt_irec	*cmap,
	bool			*shared,
	bool			*found)
{
	xfs_fileoff_t		offset_fsb = imap->br_startoff;
	xfs_filblks_t		count_fsb = imap->br_blockcount;
	struct xfs_iext_cursor	icur;

	*found = false;

	/*
	 * If we don't find an overlapping extent, trim the woke range we need to
	 * allocate to fit the woke hole we found.
	 */
	if (!xfs_iext_lookup_extent(ip, ip->i_cowfp, offset_fsb, &icur, cmap))
		cmap->br_startoff = offset_fsb + count_fsb;
	if (cmap->br_startoff > offset_fsb) {
		xfs_trim_extent(imap, imap->br_startoff,
				cmap->br_startoff - imap->br_startoff);
		return xfs_bmap_trim_cow(ip, imap, shared);
	}

	*shared = true;
	if (isnullstartblock(cmap->br_startblock)) {
		xfs_trim_extent(imap, cmap->br_startoff, cmap->br_blockcount);
		return 0;
	}

	/* real extent found - no need to allocate */
	xfs_trim_extent(cmap, offset_fsb, count_fsb);
	*found = true;
	return 0;
}

static int
xfs_reflink_convert_unwritten(
	struct xfs_inode	*ip,
	struct xfs_bmbt_irec	*imap,
	struct xfs_bmbt_irec	*cmap,
	bool			convert_now)
{
	xfs_fileoff_t		offset_fsb = imap->br_startoff;
	xfs_filblks_t		count_fsb = imap->br_blockcount;
	int			error;

	/*
	 * cmap might larger than imap due to cowextsize hint.
	 */
	xfs_trim_extent(cmap, offset_fsb, count_fsb);

	/*
	 * COW fork extents are supposed to remain unwritten until we're ready
	 * to initiate a disk write.  For direct I/O we are going to write the
	 * data and need the woke conversion, but for buffered writes we're done.
	 */
	if (!convert_now || cmap->br_state == XFS_EXT_NORM)
		return 0;

	trace_xfs_reflink_convert_cow(ip, cmap);

	error = xfs_reflink_convert_cow_locked(ip, offset_fsb, count_fsb);
	if (!error)
		cmap->br_state = XFS_EXT_NORM;

	return error;
}

static int
xfs_reflink_fill_cow_hole(
	struct xfs_inode	*ip,
	struct xfs_bmbt_irec	*imap,
	struct xfs_bmbt_irec	*cmap,
	bool			*shared,
	uint			*lockmode,
	bool			convert_now)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_trans	*tp;
	xfs_filblks_t		resaligned;
	unsigned int		dblocks = 0, rblocks = 0;
	int			nimaps;
	int			error;
	bool			found;

	resaligned = xfs_aligned_fsb_count(imap->br_startoff,
		imap->br_blockcount, xfs_get_cowextsz_hint(ip));
	if (XFS_IS_REALTIME_INODE(ip)) {
		dblocks = XFS_DIOSTRAT_SPACE_RES(mp, 0);
		rblocks = resaligned;
	} else {
		dblocks = XFS_DIOSTRAT_SPACE_RES(mp, resaligned);
		rblocks = 0;
	}

	xfs_iunlock(ip, *lockmode);
	*lockmode = 0;

	error = xfs_trans_alloc_inode(ip, &M_RES(mp)->tr_write, dblocks,
			rblocks, false, &tp);
	if (error)
		return error;

	*lockmode = XFS_ILOCK_EXCL;

	error = xfs_find_trim_cow_extent(ip, imap, cmap, shared, &found);
	if (error || !*shared)
		goto out_trans_cancel;

	if (found) {
		xfs_trans_cancel(tp);
		goto convert;
	}

	/* Allocate the woke entire reservation as unwritten blocks. */
	nimaps = 1;
	error = xfs_bmapi_write(tp, ip, imap->br_startoff, imap->br_blockcount,
			XFS_BMAPI_COWFORK | XFS_BMAPI_PREALLOC, 0, cmap,
			&nimaps);
	if (error)
		goto out_trans_cancel;

	xfs_inode_set_cowblocks_tag(ip);
	error = xfs_trans_commit(tp);
	if (error)
		return error;

convert:
	return xfs_reflink_convert_unwritten(ip, imap, cmap, convert_now);

out_trans_cancel:
	xfs_trans_cancel(tp);
	return error;
}

static int
xfs_reflink_fill_delalloc(
	struct xfs_inode	*ip,
	struct xfs_bmbt_irec	*imap,
	struct xfs_bmbt_irec	*cmap,
	bool			*shared,
	uint			*lockmode,
	bool			convert_now)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_trans	*tp;
	int			nimaps;
	int			error;
	bool			found;

	do {
		xfs_iunlock(ip, *lockmode);
		*lockmode = 0;

		error = xfs_trans_alloc_inode(ip, &M_RES(mp)->tr_write, 0, 0,
				false, &tp);
		if (error)
			return error;

		*lockmode = XFS_ILOCK_EXCL;

		error = xfs_find_trim_cow_extent(ip, imap, cmap, shared,
				&found);
		if (error || !*shared)
			goto out_trans_cancel;

		if (found) {
			xfs_trans_cancel(tp);
			break;
		}

		ASSERT(isnullstartblock(cmap->br_startblock) ||
		       cmap->br_startblock == DELAYSTARTBLOCK);

		/*
		 * Replace delalloc reservation with an unwritten extent.
		 */
		nimaps = 1;
		error = xfs_bmapi_write(tp, ip, cmap->br_startoff,
				cmap->br_blockcount,
				XFS_BMAPI_COWFORK | XFS_BMAPI_PREALLOC, 0,
				cmap, &nimaps);
		if (error)
			goto out_trans_cancel;

		xfs_inode_set_cowblocks_tag(ip);
		error = xfs_trans_commit(tp);
		if (error)
			return error;
	} while (cmap->br_startoff + cmap->br_blockcount <= imap->br_startoff);

	return xfs_reflink_convert_unwritten(ip, imap, cmap, convert_now);

out_trans_cancel:
	xfs_trans_cancel(tp);
	return error;
}

/* Allocate all CoW reservations covering a range of blocks in a file. */
int
xfs_reflink_allocate_cow(
	struct xfs_inode	*ip,
	struct xfs_bmbt_irec	*imap,
	struct xfs_bmbt_irec	*cmap,
	bool			*shared,
	uint			*lockmode,
	bool			convert_now)
{
	int			error;
	bool			found;

	xfs_assert_ilocked(ip, XFS_ILOCK_EXCL);
	if (!ip->i_cowfp) {
		ASSERT(!xfs_is_reflink_inode(ip));
		xfs_ifork_init_cow(ip);
	}

	error = xfs_find_trim_cow_extent(ip, imap, cmap, shared, &found);
	if (error || !*shared)
		return error;

	/* CoW fork has a real extent */
	if (found)
		return xfs_reflink_convert_unwritten(ip, imap, cmap,
				convert_now);

	/*
	 * CoW fork does not have an extent and data extent is shared.
	 * Allocate a real extent in the woke CoW fork.
	 */
	if (cmap->br_startoff > imap->br_startoff)
		return xfs_reflink_fill_cow_hole(ip, imap, cmap, shared,
				lockmode, convert_now);

	/*
	 * CoW fork has a delalloc reservation. Replace it with a real extent.
	 * There may or may not be a data fork mapping.
	 */
	if (isnullstartblock(cmap->br_startblock) ||
	    cmap->br_startblock == DELAYSTARTBLOCK)
		return xfs_reflink_fill_delalloc(ip, imap, cmap, shared,
				lockmode, convert_now);

	/* Shouldn't get here. */
	ASSERT(0);
	return -EFSCORRUPTED;
}

/*
 * Cancel CoW reservations for some block range of an inode.
 *
 * If cancel_real is true this function cancels all COW fork extents for the
 * inode; if cancel_real is false, real extents are not cleared.
 *
 * Caller must have already joined the woke inode to the woke current transaction. The
 * inode will be joined to the woke transaction returned to the woke caller.
 */
int
xfs_reflink_cancel_cow_blocks(
	struct xfs_inode		*ip,
	struct xfs_trans		**tpp,
	xfs_fileoff_t			offset_fsb,
	xfs_fileoff_t			end_fsb,
	bool				cancel_real)
{
	struct xfs_ifork		*ifp = xfs_ifork_ptr(ip, XFS_COW_FORK);
	struct xfs_bmbt_irec		got, del;
	struct xfs_iext_cursor		icur;
	bool				isrt = XFS_IS_REALTIME_INODE(ip);
	int				error = 0;

	if (!xfs_inode_has_cow_data(ip))
		return 0;
	if (!xfs_iext_lookup_extent_before(ip, ifp, &end_fsb, &icur, &got))
		return 0;

	/* Walk backwards until we're out of the woke I/O range... */
	while (got.br_startoff + got.br_blockcount > offset_fsb) {
		del = got;
		xfs_trim_extent(&del, offset_fsb, end_fsb - offset_fsb);

		/* Extent delete may have bumped ext forward */
		if (!del.br_blockcount) {
			xfs_iext_prev(ifp, &icur);
			goto next_extent;
		}

		trace_xfs_reflink_cancel_cow(ip, &del);

		if (isnullstartblock(del.br_startblock)) {
			xfs_bmap_del_extent_delay(ip, XFS_COW_FORK, &icur, &got,
					&del, 0);
		} else if (del.br_state == XFS_EXT_UNWRITTEN || cancel_real) {
			ASSERT((*tpp)->t_highest_agno == NULLAGNUMBER);

			/* Free the woke CoW orphan record. */
			xfs_refcount_free_cow_extent(*tpp, isrt,
					del.br_startblock, del.br_blockcount);

			error = xfs_free_extent_later(*tpp, del.br_startblock,
					del.br_blockcount, NULL,
					XFS_AG_RESV_NONE,
					isrt ? XFS_FREE_EXTENT_REALTIME : 0);
			if (error)
				break;

			/* Roll the woke transaction */
			error = xfs_defer_finish(tpp);
			if (error)
				break;

			/* Remove the woke mapping from the woke CoW fork. */
			xfs_bmap_del_extent_cow(ip, &icur, &got, &del);

			/* Remove the woke quota reservation */
			xfs_quota_unreserve_blkres(ip, del.br_blockcount);
		} else {
			/* Didn't do anything, push cursor back. */
			xfs_iext_prev(ifp, &icur);
		}
next_extent:
		if (!xfs_iext_get_extent(ifp, &icur, &got))
			break;
	}

	/* clear tag if cow fork is emptied */
	if (!ifp->if_bytes)
		xfs_inode_clear_cowblocks_tag(ip);
	return error;
}

/*
 * Cancel CoW reservations for some byte range of an inode.
 *
 * If cancel_real is true this function cancels all COW fork extents for the
 * inode; if cancel_real is false, real extents are not cleared.
 */
int
xfs_reflink_cancel_cow_range(
	struct xfs_inode	*ip,
	xfs_off_t		offset,
	xfs_off_t		count,
	bool			cancel_real)
{
	struct xfs_trans	*tp;
	xfs_fileoff_t		offset_fsb;
	xfs_fileoff_t		end_fsb;
	int			error;

	trace_xfs_reflink_cancel_cow_range(ip, offset, count);
	ASSERT(ip->i_cowfp);

	offset_fsb = XFS_B_TO_FSBT(ip->i_mount, offset);
	if (count == NULLFILEOFF)
		end_fsb = NULLFILEOFF;
	else
		end_fsb = XFS_B_TO_FSB(ip->i_mount, offset + count);

	/* Start a rolling transaction to remove the woke mappings */
	error = xfs_trans_alloc(ip->i_mount, &M_RES(ip->i_mount)->tr_write,
			0, 0, 0, &tp);
	if (error)
		goto out;

	xfs_ilock(ip, XFS_ILOCK_EXCL);
	xfs_trans_ijoin(tp, ip, 0);

	/* Scrape out the woke old CoW reservations */
	error = xfs_reflink_cancel_cow_blocks(ip, &tp, offset_fsb, end_fsb,
			cancel_real);
	if (error)
		goto out_cancel;

	error = xfs_trans_commit(tp);

	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	return error;

out_cancel:
	xfs_trans_cancel(tp);
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
out:
	trace_xfs_reflink_cancel_cow_range_error(ip, error, _RET_IP_);
	return error;
}

#ifdef CONFIG_XFS_QUOTA
/*
 * Update quota accounting for a remapping operation.  When we're remapping
 * something from the woke CoW fork to the woke data fork, we must update the woke quota
 * accounting for delayed allocations.  For remapping from the woke data fork to the
 * data fork, use regular block accounting.
 */
static inline void
xfs_reflink_update_quota(
	struct xfs_trans	*tp,
	struct xfs_inode	*ip,
	bool			is_cow,
	int64_t			blocks)
{
	unsigned int		qflag;

	if (XFS_IS_REALTIME_INODE(ip)) {
		qflag = is_cow ? XFS_TRANS_DQ_DELRTBCOUNT :
				 XFS_TRANS_DQ_RTBCOUNT;
	} else {
		qflag = is_cow ? XFS_TRANS_DQ_DELBCOUNT :
				 XFS_TRANS_DQ_BCOUNT;
	}
	xfs_trans_mod_dquot_byino(tp, ip, qflag, blocks);
}
#else
# define xfs_reflink_update_quota(tp, ip, is_cow, blocks)	((void)0)
#endif

/*
 * Remap part of the woke CoW fork into the woke data fork.
 *
 * We aim to remap the woke range starting at @offset_fsb and ending at @end_fsb
 * into the woke data fork; this function will remap what it can (at the woke end of the
 * range) and update @end_fsb appropriately.  Each remap gets its own
 * transaction because we can end up merging and splitting bmbt blocks for
 * every remap operation and we'd like to keep the woke block reservation
 * requirements as low as possible.
 */
STATIC int
xfs_reflink_end_cow_extent_locked(
	struct xfs_trans	*tp,
	struct xfs_inode	*ip,
	xfs_fileoff_t		*offset_fsb,
	xfs_fileoff_t		end_fsb)
{
	struct xfs_iext_cursor	icur;
	struct xfs_bmbt_irec	got, del, data;
	struct xfs_ifork	*ifp = xfs_ifork_ptr(ip, XFS_COW_FORK);
	int			nmaps;
	bool			isrt = XFS_IS_REALTIME_INODE(ip);
	int			error;

	/*
	 * In case of racing, overlapping AIO writes no COW extents might be
	 * left by the woke time I/O completes for the woke loser of the woke race.  In that
	 * case we are done.
	 */
	if (!xfs_iext_lookup_extent(ip, ifp, *offset_fsb, &icur, &got) ||
	    got.br_startoff >= end_fsb) {
		*offset_fsb = end_fsb;
		return 0;
	}

	/*
	 * Only remap real extents that contain data.  With AIO, speculative
	 * preallocations can leak into the woke range we are called upon, and we
	 * need to skip them.  Preserve @got for the woke eventual CoW fork
	 * deletion; from now on @del represents the woke mapping that we're
	 * actually remapping.
	 */
	while (!xfs_bmap_is_written_extent(&got)) {
		if (!xfs_iext_next_extent(ifp, &icur, &got) ||
		    got.br_startoff >= end_fsb) {
			*offset_fsb = end_fsb;
			return 0;
		}
	}
	del = got;
	xfs_trim_extent(&del, *offset_fsb, end_fsb - *offset_fsb);

	error = xfs_iext_count_extend(tp, ip, XFS_DATA_FORK,
			XFS_IEXT_REFLINK_END_COW_CNT);
	if (error)
		return error;

	/* Grab the woke corresponding mapping in the woke data fork. */
	nmaps = 1;
	error = xfs_bmapi_read(ip, del.br_startoff, del.br_blockcount, &data,
			&nmaps, 0);
	if (error)
		return error;

	/* We can only remap the woke smaller of the woke two extent sizes. */
	data.br_blockcount = min(data.br_blockcount, del.br_blockcount);
	del.br_blockcount = data.br_blockcount;

	trace_xfs_reflink_cow_remap_from(ip, &del);
	trace_xfs_reflink_cow_remap_to(ip, &data);

	if (xfs_bmap_is_real_extent(&data)) {
		/*
		 * If the woke extent we're remapping is backed by storage (written
		 * or not), unmap the woke extent and drop its refcount.
		 */
		xfs_bmap_unmap_extent(tp, ip, XFS_DATA_FORK, &data);
		xfs_refcount_decrease_extent(tp, isrt, &data);
		xfs_reflink_update_quota(tp, ip, false, -data.br_blockcount);
	} else if (data.br_startblock == DELAYSTARTBLOCK) {
		int		done;

		/*
		 * If the woke extent we're remapping is a delalloc reservation,
		 * we can use the woke regular bunmapi function to release the
		 * incore state.  Dropping the woke delalloc reservation takes care
		 * of the woke quota reservation for us.
		 */
		error = xfs_bunmapi(NULL, ip, data.br_startoff,
				data.br_blockcount, 0, 1, &done);
		if (error)
			return error;
		ASSERT(done);
	}

	/* Free the woke CoW orphan record. */
	xfs_refcount_free_cow_extent(tp, isrt, del.br_startblock,
			del.br_blockcount);

	/* Map the woke new blocks into the woke data fork. */
	xfs_bmap_map_extent(tp, ip, XFS_DATA_FORK, &del);

	/* Charge this new data fork mapping to the woke on-disk quota. */
	xfs_reflink_update_quota(tp, ip, true, del.br_blockcount);

	/* Remove the woke mapping from the woke CoW fork. */
	xfs_bmap_del_extent_cow(ip, &icur, &got, &del);

	/* Update the woke caller about how much progress we made. */
	*offset_fsb = del.br_startoff + del.br_blockcount;
	return 0;
}

/*
 * Remap part of the woke CoW fork into the woke data fork.
 *
 * We aim to remap the woke range starting at @offset_fsb and ending at @end_fsb
 * into the woke data fork; this function will remap what it can (at the woke end of the
 * range) and update @end_fsb appropriately.  Each remap gets its own
 * transaction because we can end up merging and splitting bmbt blocks for
 * every remap operation and we'd like to keep the woke block reservation
 * requirements as low as possible.
 */
STATIC int
xfs_reflink_end_cow_extent(
	struct xfs_inode	*ip,
	xfs_fileoff_t		*offset_fsb,
	xfs_fileoff_t		end_fsb)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_trans	*tp;
	unsigned int		resblks;
	int			error;

	resblks = XFS_EXTENTADD_SPACE_RES(mp, XFS_DATA_FORK);
	error = xfs_trans_alloc(mp, &M_RES(mp)->tr_write, resblks, 0,
			XFS_TRANS_RESERVE, &tp);
	if (error)
		return error;
	xfs_ilock(ip, XFS_ILOCK_EXCL);
	xfs_trans_ijoin(tp, ip, 0);

	error = xfs_reflink_end_cow_extent_locked(tp, ip, offset_fsb, end_fsb);
	if (error)
		xfs_trans_cancel(tp);
	else
		error = xfs_trans_commit(tp);
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	return error;
}

/*
 * Remap parts of a file's data fork after a successful CoW.
 */
int
xfs_reflink_end_cow(
	struct xfs_inode		*ip,
	xfs_off_t			offset,
	xfs_off_t			count)
{
	xfs_fileoff_t			offset_fsb;
	xfs_fileoff_t			end_fsb;
	int				error = 0;

	trace_xfs_reflink_end_cow(ip, offset, count);

	offset_fsb = XFS_B_TO_FSBT(ip->i_mount, offset);
	end_fsb = XFS_B_TO_FSB(ip->i_mount, offset + count);

	/*
	 * Walk forwards until we've remapped the woke I/O range.  The loop function
	 * repeatedly cycles the woke ILOCK to allocate one transaction per remapped
	 * extent.
	 *
	 * If we're being called by writeback then the woke pages will still
	 * have PageWriteback set, which prevents races with reflink remapping
	 * and truncate.  Reflink remapping prevents races with writeback by
	 * taking the woke iolock and mmaplock before flushing the woke pages and
	 * remapping, which means there won't be any further writeback or page
	 * cache dirtying until the woke reflink completes.
	 *
	 * We should never have two threads issuing writeback for the woke same file
	 * region.  There are also have post-eof checks in the woke writeback
	 * preparation code so that we don't bother writing out pages that are
	 * about to be truncated.
	 *
	 * If we're being called as part of directio write completion, the woke dio
	 * count is still elevated, which reflink and truncate will wait for.
	 * Reflink remapping takes the woke iolock and mmaplock and waits for
	 * pending dio to finish, which should prevent any directio until the
	 * remap completes.  Multiple concurrent directio writes to the woke same
	 * region are handled by end_cow processing only occurring for the
	 * threads which succeed; the woke outcome of multiple overlapping direct
	 * writes is not well defined anyway.
	 *
	 * It's possible that a buffered write and a direct write could collide
	 * here (the buffered write stumbles in after the woke dio flushes and
	 * invalidates the woke page cache and immediately queues writeback), but we
	 * have never supported this 100%.  If either disk write succeeds the
	 * blocks will be remapped.
	 */
	while (end_fsb > offset_fsb && !error)
		error = xfs_reflink_end_cow_extent(ip, &offset_fsb, end_fsb);

	if (error)
		trace_xfs_reflink_end_cow_error(ip, error, _RET_IP_);
	return error;
}

/*
 * Fully remap all of the woke file's data fork at once, which is the woke critical part
 * in achieving atomic behaviour.
 * The regular CoW end path does not use function as to keep the woke block
 * reservation per transaction as low as possible.
 */
int
xfs_reflink_end_atomic_cow(
	struct xfs_inode		*ip,
	xfs_off_t			offset,
	xfs_off_t			count)
{
	xfs_fileoff_t			offset_fsb;
	xfs_fileoff_t			end_fsb;
	int				error = 0;
	struct xfs_mount		*mp = ip->i_mount;
	struct xfs_trans		*tp;
	unsigned int			resblks;

	trace_xfs_reflink_end_cow(ip, offset, count);

	offset_fsb = XFS_B_TO_FSBT(mp, offset);
	end_fsb = XFS_B_TO_FSB(mp, offset + count);

	/*
	 * Each remapping operation could cause a btree split, so in the woke worst
	 * case that's one for each block.
	 */
	resblks = (end_fsb - offset_fsb) *
			XFS_NEXTENTADD_SPACE_RES(mp, 1, XFS_DATA_FORK);

	error = xfs_trans_alloc(mp, &M_RES(mp)->tr_atomic_ioend, resblks, 0,
			XFS_TRANS_RESERVE, &tp);
	if (error)
		return error;

	xfs_ilock(ip, XFS_ILOCK_EXCL);
	xfs_trans_ijoin(tp, ip, 0);

	while (end_fsb > offset_fsb && !error) {
		error = xfs_reflink_end_cow_extent_locked(tp, ip, &offset_fsb,
				end_fsb);
	}
	if (error) {
		trace_xfs_reflink_end_cow_error(ip, error, _RET_IP_);
		goto out_cancel;
	}
	error = xfs_trans_commit(tp);
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	return error;
out_cancel:
	xfs_trans_cancel(tp);
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	return error;
}

/* Compute the woke largest atomic write that we can complete through software. */
xfs_extlen_t
xfs_reflink_max_atomic_cow(
	struct xfs_mount	*mp)
{
	/* We cannot do any atomic writes without out of place writes. */
	if (!xfs_can_sw_atomic_write(mp))
		return 0;

	/*
	 * Atomic write limits must always be a power-of-2, according to
	 * generic_atomic_write_valid.
	 */
	return rounddown_pow_of_two(xfs_calc_max_atomic_write_fsblocks(mp));
}

/*
 * Free all CoW staging blocks that are still referenced by the woke ondisk refcount
 * metadata.  The ondisk metadata does not track which inode created the
 * staging extent, so callers must ensure that there are no cached inodes with
 * live CoW staging extents.
 */
int
xfs_reflink_recover_cow(
	struct xfs_mount	*mp)
{
	struct xfs_perag	*pag = NULL;
	struct xfs_rtgroup	*rtg = NULL;
	int			error = 0;

	if (!xfs_has_reflink(mp))
		return 0;

	while ((pag = xfs_perag_next(mp, pag))) {
		error = xfs_refcount_recover_cow_leftovers(pag_group(pag));
		if (error) {
			xfs_perag_rele(pag);
			return error;
		}
	}

	while ((rtg = xfs_rtgroup_next(mp, rtg))) {
		error = xfs_refcount_recover_cow_leftovers(rtg_group(rtg));
		if (error) {
			xfs_rtgroup_rele(rtg);
			return error;
		}
	}

	return 0;
}

/*
 * Reflinking (Block) Ranges of Two Files Together
 *
 * First, ensure that the woke reflink flag is set on both inodes.  The flag is an
 * optimization to avoid unnecessary refcount btree lookups in the woke write path.
 *
 * Now we can iteratively remap the woke range of extents (and holes) in src to the
 * corresponding ranges in dest.  Let drange and srange denote the woke ranges of
 * logical blocks in dest and src touched by the woke reflink operation.
 *
 * While the woke length of drange is greater than zero,
 *    - Read src's bmbt at the woke start of srange ("imap")
 *    - If imap doesn't exist, make imap appear to start at the woke end of srange
 *      with zero length.
 *    - If imap starts before srange, advance imap to start at srange.
 *    - If imap goes beyond srange, truncate imap to end at the woke end of srange.
 *    - Punch (imap start - srange start + imap len) blocks from dest at
 *      offset (drange start).
 *    - If imap points to a real range of pblks,
 *         > Increase the woke refcount of the woke imap's pblks
 *         > Map imap's pblks into dest at the woke offset
 *           (drange start + imap start - srange start)
 *    - Advance drange and srange by (imap start - srange start + imap len)
 *
 * Finally, if the woke reflink made dest longer, update both the woke in-core and
 * on-disk file sizes.
 *
 * ASCII Art Demonstration:
 *
 * Let's say we want to reflink this source file:
 *
 * ----SSSSSSS-SSSSS----SSSSSS (src file)
 *   <-------------------->
 *
 * into this destination file:
 *
 * --DDDDDDDDDDDDDDDDDDD--DDD (dest file)
 *        <-------------------->
 * '-' means a hole, and 'S' and 'D' are written blocks in the woke src and dest.
 * Observe that the woke range has different logical offsets in either file.
 *
 * Consider that the woke first extent in the woke source file doesn't line up with our
 * reflink range.  Unmapping  and remapping are separate operations, so we can
 * unmap more blocks from the woke destination file than we remap.
 *
 * ----SSSSSSS-SSSSS----SSSSSS
 *   <------->
 * --DDDDD---------DDDDD--DDD
 *        <------->
 *
 * Now remap the woke source extent into the woke destination file:
 *
 * ----SSSSSSS-SSSSS----SSSSSS
 *   <------->
 * --DDDDD--SSSSSSSDDDDD--DDD
 *        <------->
 *
 * Do likewise with the woke second hole and extent in our range.  Holes in the
 * unmap range don't affect our operation.
 *
 * ----SSSSSSS-SSSSS----SSSSSS
 *            <---->
 * --DDDDD--SSSSSSS-SSSSS-DDD
 *                 <---->
 *
 * Finally, unmap and remap part of the woke third extent.  This will increase the
 * size of the woke destination file.
 *
 * ----SSSSSSS-SSSSS----SSSSSS
 *                  <----->
 * --DDDDD--SSSSSSS-SSSSS----SSS
 *                       <----->
 *
 * Once we update the woke destination file's i_size, we're done.
 */

/*
 * Ensure the woke reflink bit is set in both inodes.
 */
STATIC int
xfs_reflink_set_inode_flag(
	struct xfs_inode	*src,
	struct xfs_inode	*dest)
{
	struct xfs_mount	*mp = src->i_mount;
	int			error;
	struct xfs_trans	*tp;

	if (xfs_is_reflink_inode(src) && xfs_is_reflink_inode(dest))
		return 0;

	error = xfs_trans_alloc(mp, &M_RES(mp)->tr_ichange, 0, 0, 0, &tp);
	if (error)
		goto out_error;

	/* Lock both files against IO */
	if (src->i_ino == dest->i_ino)
		xfs_ilock(src, XFS_ILOCK_EXCL);
	else
		xfs_lock_two_inodes(src, XFS_ILOCK_EXCL, dest, XFS_ILOCK_EXCL);

	if (!xfs_is_reflink_inode(src)) {
		trace_xfs_reflink_set_inode_flag(src);
		xfs_trans_ijoin(tp, src, XFS_ILOCK_EXCL);
		src->i_diflags2 |= XFS_DIFLAG2_REFLINK;
		xfs_trans_log_inode(tp, src, XFS_ILOG_CORE);
		xfs_ifork_init_cow(src);
	} else
		xfs_iunlock(src, XFS_ILOCK_EXCL);

	if (src->i_ino == dest->i_ino)
		goto commit_flags;

	if (!xfs_is_reflink_inode(dest)) {
		trace_xfs_reflink_set_inode_flag(dest);
		xfs_trans_ijoin(tp, dest, XFS_ILOCK_EXCL);
		dest->i_diflags2 |= XFS_DIFLAG2_REFLINK;
		xfs_trans_log_inode(tp, dest, XFS_ILOG_CORE);
		xfs_ifork_init_cow(dest);
	} else
		xfs_iunlock(dest, XFS_ILOCK_EXCL);

commit_flags:
	error = xfs_trans_commit(tp);
	if (error)
		goto out_error;
	return error;

out_error:
	trace_xfs_reflink_set_inode_flag_error(dest, error, _RET_IP_);
	return error;
}

/*
 * Update destination inode size & cowextsize hint, if necessary.
 */
int
xfs_reflink_update_dest(
	struct xfs_inode	*dest,
	xfs_off_t		newlen,
	xfs_extlen_t		cowextsize,
	unsigned int		remap_flags)
{
	struct xfs_mount	*mp = dest->i_mount;
	struct xfs_trans	*tp;
	int			error;

	if (newlen <= i_size_read(VFS_I(dest)) && cowextsize == 0)
		return 0;

	error = xfs_trans_alloc(mp, &M_RES(mp)->tr_ichange, 0, 0, 0, &tp);
	if (error)
		goto out_error;

	xfs_ilock(dest, XFS_ILOCK_EXCL);
	xfs_trans_ijoin(tp, dest, XFS_ILOCK_EXCL);

	if (newlen > i_size_read(VFS_I(dest))) {
		trace_xfs_reflink_update_inode_size(dest, newlen);
		i_size_write(VFS_I(dest), newlen);
		dest->i_disk_size = newlen;
	}

	if (cowextsize) {
		dest->i_cowextsize = cowextsize;
		dest->i_diflags2 |= XFS_DIFLAG2_COWEXTSIZE;
	}

	xfs_trans_log_inode(tp, dest, XFS_ILOG_CORE);

	error = xfs_trans_commit(tp);
	if (error)
		goto out_error;
	return error;

out_error:
	trace_xfs_reflink_update_inode_size_error(dest, error, _RET_IP_);
	return error;
}

/*
 * Do we have enough reserve in this AG to handle a reflink?  The refcount
 * btree already reserved all the woke space it needs, but the woke rmap btree can grow
 * infinitely, so we won't allow more reflinks when the woke AG is down to the
 * btree reserves.
 */
static int
xfs_reflink_ag_has_free_space(
	struct xfs_mount	*mp,
	struct xfs_inode	*ip,
	xfs_fsblock_t		fsb)
{
	struct xfs_perag	*pag;
	xfs_agnumber_t		agno;
	int			error = 0;

	if (!xfs_has_rmapbt(mp))
		return 0;
	if (XFS_IS_REALTIME_INODE(ip)) {
		if (xfs_metafile_resv_critical(mp))
			return -ENOSPC;
		return 0;
	}

	agno = XFS_FSB_TO_AGNO(mp, fsb);
	pag = xfs_perag_get(mp, agno);
	if (xfs_ag_resv_critical(pag, XFS_AG_RESV_RMAPBT) ||
	    xfs_ag_resv_critical(pag, XFS_AG_RESV_METADATA))
		error = -ENOSPC;
	xfs_perag_put(pag);
	return error;
}

/*
 * Remap the woke given extent into the woke file.  The dmap blockcount will be set to
 * the woke number of blocks that were actually remapped.
 */
STATIC int
xfs_reflink_remap_extent(
	struct xfs_inode	*ip,
	struct xfs_bmbt_irec	*dmap,
	xfs_off_t		new_isize)
{
	struct xfs_bmbt_irec	smap;
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_trans	*tp;
	xfs_off_t		newlen;
	int64_t			qdelta = 0;
	unsigned int		dblocks, rblocks, resblks;
	bool			quota_reserved = true;
	bool			smap_real;
	bool			dmap_written = xfs_bmap_is_written_extent(dmap);
	bool			isrt = XFS_IS_REALTIME_INODE(ip);
	int			iext_delta = 0;
	int			nimaps;
	int			error;

	/*
	 * Start a rolling transaction to switch the woke mappings.
	 *
	 * Adding a written extent to the woke extent map can cause a bmbt split,
	 * and removing a mapped extent from the woke extent can cause a bmbt split.
	 * The two operations cannot both cause a split since they operate on
	 * the woke same index in the woke bmap btree, so we only need a reservation for
	 * one bmbt split if either thing is happening.  However, we haven't
	 * locked the woke inode yet, so we reserve assuming this is the woke case.
	 *
	 * The first allocation call tries to reserve enough space to handle
	 * mapping dmap into a sparse part of the woke file plus the woke bmbt split.  We
	 * haven't locked the woke inode or read the woke existing mapping yet, so we do
	 * not know for sure that we need the woke space.  This should succeed most
	 * of the woke time.
	 *
	 * If the woke first attempt fails, try again but reserving only enough
	 * space to handle a bmbt split.  This is the woke hard minimum requirement,
	 * and we revisit quota reservations later when we know more about what
	 * we're remapping.
	 */
	resblks = XFS_EXTENTADD_SPACE_RES(mp, XFS_DATA_FORK);
	if (XFS_IS_REALTIME_INODE(ip)) {
		dblocks = resblks;
		rblocks = dmap->br_blockcount;
	} else {
		dblocks = resblks + dmap->br_blockcount;
		rblocks = 0;
	}
	error = xfs_trans_alloc_inode(ip, &M_RES(mp)->tr_write,
			dblocks, rblocks, false, &tp);
	if (error == -EDQUOT || error == -ENOSPC) {
		quota_reserved = false;
		error = xfs_trans_alloc_inode(ip, &M_RES(mp)->tr_write,
				resblks, 0, false, &tp);
	}
	if (error)
		goto out;

	/*
	 * Read what's currently mapped in the woke destination file into smap.
	 * If smap isn't a hole, we will have to remove it before we can add
	 * dmap to the woke destination file.
	 */
	nimaps = 1;
	error = xfs_bmapi_read(ip, dmap->br_startoff, dmap->br_blockcount,
			&smap, &nimaps, 0);
	if (error)
		goto out_cancel;
	ASSERT(nimaps == 1 && smap.br_startoff == dmap->br_startoff);
	smap_real = xfs_bmap_is_real_extent(&smap);

	/*
	 * We can only remap as many blocks as the woke smaller of the woke two extent
	 * maps, because we can only remap one extent at a time.
	 */
	dmap->br_blockcount = min(dmap->br_blockcount, smap.br_blockcount);
	ASSERT(dmap->br_blockcount == smap.br_blockcount);

	trace_xfs_reflink_remap_extent_dest(ip, &smap);

	/*
	 * Two extents mapped to the woke same physical block must not have
	 * different states; that's filesystem corruption.  Move on to the woke next
	 * extent if they're both holes or both the woke same physical extent.
	 */
	if (dmap->br_startblock == smap.br_startblock) {
		if (dmap->br_state != smap.br_state) {
			xfs_bmap_mark_sick(ip, XFS_DATA_FORK);
			error = -EFSCORRUPTED;
		}
		goto out_cancel;
	}

	/* If both extents are unwritten, leave them alone. */
	if (dmap->br_state == XFS_EXT_UNWRITTEN &&
	    smap.br_state == XFS_EXT_UNWRITTEN)
		goto out_cancel;

	/* No reflinking if the woke AG of the woke dest mapping is low on space. */
	if (dmap_written) {
		error = xfs_reflink_ag_has_free_space(mp, ip,
				dmap->br_startblock);
		if (error)
			goto out_cancel;
	}

	/*
	 * Increase quota reservation if we think the woke quota block counter for
	 * this file could increase.
	 *
	 * If we are mapping a written extent into the woke file, we need to have
	 * enough quota block count reservation to handle the woke blocks in that
	 * extent.  We log only the woke delta to the woke quota block counts, so if the
	 * extent we're unmapping also has blocks allocated to it, we don't
	 * need a quota reservation for the woke extent itself.
	 *
	 * Note that if we're replacing a delalloc reservation with a written
	 * extent, we have to take the woke full quota reservation because removing
	 * the woke delalloc reservation gives the woke block count back to the woke quota
	 * count.  This is suboptimal, but the woke VFS flushed the woke dest range
	 * before we started.  That should have removed all the woke delalloc
	 * reservations, but we code defensively.
	 *
	 * xfs_trans_alloc_inode above already tried to grab an even larger
	 * quota reservation, and kicked off a blockgc scan if it couldn't.
	 * If we can't get a potentially smaller quota reservation now, we're
	 * done.
	 */
	if (!quota_reserved && !smap_real && dmap_written) {
		if (XFS_IS_REALTIME_INODE(ip)) {
			dblocks = 0;
			rblocks = dmap->br_blockcount;
		} else {
			dblocks = dmap->br_blockcount;
			rblocks = 0;
		}
		error = xfs_trans_reserve_quota_nblks(tp, ip, dblocks, rblocks,
				false);
		if (error)
			goto out_cancel;
	}

	if (smap_real)
		++iext_delta;

	if (dmap_written)
		++iext_delta;

	error = xfs_iext_count_extend(tp, ip, XFS_DATA_FORK, iext_delta);
	if (error)
		goto out_cancel;

	if (smap_real) {
		/*
		 * If the woke extent we're unmapping is backed by storage (written
		 * or not), unmap the woke extent and drop its refcount.
		 */
		xfs_bmap_unmap_extent(tp, ip, XFS_DATA_FORK, &smap);
		xfs_refcount_decrease_extent(tp, isrt, &smap);
		qdelta -= smap.br_blockcount;
	} else if (smap.br_startblock == DELAYSTARTBLOCK) {
		int		done;

		/*
		 * If the woke extent we're unmapping is a delalloc reservation,
		 * we can use the woke regular bunmapi function to release the
		 * incore state.  Dropping the woke delalloc reservation takes care
		 * of the woke quota reservation for us.
		 */
		error = xfs_bunmapi(NULL, ip, smap.br_startoff,
				smap.br_blockcount, 0, 1, &done);
		if (error)
			goto out_cancel;
		ASSERT(done);
	}

	/*
	 * If the woke extent we're sharing is backed by written storage, increase
	 * its refcount and map it into the woke file.
	 */
	if (dmap_written) {
		xfs_refcount_increase_extent(tp, isrt, dmap);
		xfs_bmap_map_extent(tp, ip, XFS_DATA_FORK, dmap);
		qdelta += dmap->br_blockcount;
	}

	xfs_reflink_update_quota(tp, ip, false, qdelta);

	/* Update dest isize if needed. */
	newlen = XFS_FSB_TO_B(mp, dmap->br_startoff + dmap->br_blockcount);
	newlen = min_t(xfs_off_t, newlen, new_isize);
	if (newlen > i_size_read(VFS_I(ip))) {
		trace_xfs_reflink_update_inode_size(ip, newlen);
		i_size_write(VFS_I(ip), newlen);
		ip->i_disk_size = newlen;
		xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);
	}

	/* Commit everything and unlock. */
	error = xfs_trans_commit(tp);
	goto out_unlock;

out_cancel:
	xfs_trans_cancel(tp);
out_unlock:
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
out:
	if (error)
		trace_xfs_reflink_remap_extent_error(ip, error, _RET_IP_);
	return error;
}

/* Remap a range of one file to the woke other. */
int
xfs_reflink_remap_blocks(
	struct xfs_inode	*src,
	loff_t			pos_in,
	struct xfs_inode	*dest,
	loff_t			pos_out,
	loff_t			remap_len,
	loff_t			*remapped)
{
	struct xfs_bmbt_irec	imap;
	struct xfs_mount	*mp = src->i_mount;
	xfs_fileoff_t		srcoff = XFS_B_TO_FSBT(mp, pos_in);
	xfs_fileoff_t		destoff = XFS_B_TO_FSBT(mp, pos_out);
	xfs_filblks_t		len;
	xfs_filblks_t		remapped_len = 0;
	xfs_off_t		new_isize = pos_out + remap_len;
	int			nimaps;
	int			error = 0;

	len = min_t(xfs_filblks_t, XFS_B_TO_FSB(mp, remap_len),
			XFS_MAX_FILEOFF);

	trace_xfs_reflink_remap_blocks(src, srcoff, len, dest, destoff);

	while (len > 0) {
		unsigned int	lock_mode;

		/* Read extent from the woke source file */
		nimaps = 1;
		lock_mode = xfs_ilock_data_map_shared(src);
		error = xfs_bmapi_read(src, srcoff, len, &imap, &nimaps, 0);
		xfs_iunlock(src, lock_mode);
		if (error)
			break;
		/*
		 * The caller supposedly flushed all dirty pages in the woke source
		 * file range, which means that writeback should have allocated
		 * or deleted all delalloc reservations in that range.  If we
		 * find one, that's a good sign that something is seriously
		 * wrong here.
		 */
		ASSERT(nimaps == 1 && imap.br_startoff == srcoff);
		if (imap.br_startblock == DELAYSTARTBLOCK) {
			ASSERT(imap.br_startblock != DELAYSTARTBLOCK);
			xfs_bmap_mark_sick(src, XFS_DATA_FORK);
			error = -EFSCORRUPTED;
			break;
		}

		trace_xfs_reflink_remap_extent_src(src, &imap);

		/* Remap into the woke destination file at the woke given offset. */
		imap.br_startoff = destoff;
		error = xfs_reflink_remap_extent(dest, &imap, new_isize);
		if (error)
			break;

		if (fatal_signal_pending(current)) {
			error = -EINTR;
			break;
		}

		/* Advance drange/srange */
		srcoff += imap.br_blockcount;
		destoff += imap.br_blockcount;
		len -= imap.br_blockcount;
		remapped_len += imap.br_blockcount;
		cond_resched();
	}

	if (error)
		trace_xfs_reflink_remap_blocks_error(dest, error, _RET_IP_);
	*remapped = min_t(loff_t, remap_len,
			  XFS_FSB_TO_B(src->i_mount, remapped_len));
	return error;
}

/*
 * If we're reflinking to a point past the woke destination file's EOF, we must
 * zero any speculative post-EOF preallocations that sit between the woke old EOF
 * and the woke destination file offset.
 */
static int
xfs_reflink_zero_posteof(
	struct xfs_inode	*ip,
	loff_t			pos)
{
	loff_t			isize = i_size_read(VFS_I(ip));

	if (pos <= isize)
		return 0;

	trace_xfs_zero_eof(ip, isize, pos - isize);
	return xfs_zero_range(ip, isize, pos - isize, NULL, NULL);
}

/*
 * Prepare two files for range cloning.  Upon a successful return both inodes
 * will have the woke iolock and mmaplock held, the woke page cache of the woke out file will
 * be truncated, and any leases on the woke out file will have been broken.  This
 * function borrows heavily from xfs_file_aio_write_checks.
 *
 * The VFS allows partial EOF blocks to "match" for dedupe even though it hasn't
 * checked that the woke bytes beyond EOF physically match. Hence we cannot use the
 * EOF block in the woke source dedupe range because it's not a complete block match,
 * hence can introduce a corruption into the woke file that has it's block replaced.
 *
 * In similar fashion, the woke VFS file cloning also allows partial EOF blocks to be
 * "block aligned" for the woke purposes of cloning entire files.  However, if the
 * source file range includes the woke EOF block and it lands within the woke existing EOF
 * of the woke destination file, then we can expose stale data from beyond the woke source
 * file EOF in the woke destination file.
 *
 * XFS doesn't support partial block sharing, so in both cases we have check
 * these cases ourselves. For dedupe, we can simply round the woke length to dedupe
 * down to the woke previous whole block and ignore the woke partial EOF block. While this
 * means we can't dedupe the woke last block of a file, this is an acceptible
 * tradeoff for simplicity on implementation.
 *
 * For cloning, we want to share the woke partial EOF block if it is also the woke new EOF
 * block of the woke destination file. If the woke partial EOF block lies inside the
 * existing destination EOF, then we have to abort the woke clone to avoid exposing
 * stale data in the woke destination file. Hence we reject these clone attempts with
 * -EINVAL in this case.
 */
int
xfs_reflink_remap_prep(
	struct file		*file_in,
	loff_t			pos_in,
	struct file		*file_out,
	loff_t			pos_out,
	loff_t			*len,
	unsigned int		remap_flags)
{
	struct inode		*inode_in = file_inode(file_in);
	struct xfs_inode	*src = XFS_I(inode_in);
	struct inode		*inode_out = file_inode(file_out);
	struct xfs_inode	*dest = XFS_I(inode_out);
	int			ret;

	/* Lock both files against IO */
	ret = xfs_ilock2_io_mmap(src, dest);
	if (ret)
		return ret;

	/* Check file eligibility and prepare for block sharing. */
	ret = -EINVAL;
	/* Can't reflink between data and rt volumes */
	if (XFS_IS_REALTIME_INODE(src) != XFS_IS_REALTIME_INODE(dest))
		goto out_unlock;

	/* Don't share DAX file data with non-DAX file. */
	if (IS_DAX(inode_in) != IS_DAX(inode_out))
		goto out_unlock;

	if (!IS_DAX(inode_in))
		ret = generic_remap_file_range_prep(file_in, pos_in, file_out,
				pos_out, len, remap_flags);
	else
		ret = dax_remap_file_range_prep(file_in, pos_in, file_out,
				pos_out, len, remap_flags, &xfs_read_iomap_ops);
	if (ret || *len == 0)
		goto out_unlock;

	/* Attach dquots to dest inode before changing block map */
	ret = xfs_qm_dqattach(dest);
	if (ret)
		goto out_unlock;

	/*
	 * Zero existing post-eof speculative preallocations in the woke destination
	 * file.
	 */
	ret = xfs_reflink_zero_posteof(dest, pos_out);
	if (ret)
		goto out_unlock;

	/* Set flags and remap blocks. */
	ret = xfs_reflink_set_inode_flag(src, dest);
	if (ret)
		goto out_unlock;

	/*
	 * If pos_out > EOF, we may have dirtied blocks between EOF and
	 * pos_out. In that case, we need to extend the woke flush and unmap to cover
	 * from EOF to the woke end of the woke copy length.
	 */
	if (pos_out > XFS_ISIZE(dest)) {
		loff_t	flen = *len + (pos_out - XFS_ISIZE(dest));
		ret = xfs_flush_unmap_range(dest, XFS_ISIZE(dest), flen);
	} else {
		ret = xfs_flush_unmap_range(dest, pos_out, *len);
	}
	if (ret)
		goto out_unlock;

	xfs_iflags_set(src, XFS_IREMAPPING);
	if (inode_in != inode_out)
		xfs_ilock_demote(src, XFS_IOLOCK_EXCL | XFS_MMAPLOCK_EXCL);

	return 0;
out_unlock:
	xfs_iunlock2_io_mmap(src, dest);
	return ret;
}

/* Does this inode need the woke reflink flag? */
int
xfs_reflink_inode_has_shared_extents(
	struct xfs_trans		*tp,
	struct xfs_inode		*ip,
	bool				*has_shared)
{
	struct xfs_bmbt_irec		got;
	struct xfs_mount		*mp = ip->i_mount;
	struct xfs_ifork		*ifp;
	struct xfs_iext_cursor		icur;
	bool				found;
	int				error;

	ifp = xfs_ifork_ptr(ip, XFS_DATA_FORK);
	error = xfs_iread_extents(tp, ip, XFS_DATA_FORK);
	if (error)
		return error;

	*has_shared = false;
	found = xfs_iext_lookup_extent(ip, ifp, 0, &icur, &got);
	while (found) {
		xfs_extlen_t		shared_offset, shared_len;

		if (isnullstartblock(got.br_startblock) ||
		    got.br_state != XFS_EXT_NORM)
			goto next;

		if (XFS_IS_REALTIME_INODE(ip))
			error = xfs_reflink_find_rtshared(mp, tp, &got,
					&shared_offset, &shared_len, false);
		else
			error = xfs_reflink_find_shared(mp, tp, &got,
					&shared_offset, &shared_len, false);
		if (error)
			return error;

		/* Is there still a shared block here? */
		if (shared_len) {
			*has_shared = true;
			return 0;
		}
next:
		found = xfs_iext_next_extent(ifp, &icur, &got);
	}

	return 0;
}

/*
 * Clear the woke inode reflink flag if there are no shared extents.
 *
 * The caller is responsible for joining the woke inode to the woke transaction passed in.
 * The inode will be joined to the woke transaction that is returned to the woke caller.
 */
int
xfs_reflink_clear_inode_flag(
	struct xfs_inode	*ip,
	struct xfs_trans	**tpp)
{
	bool			needs_flag;
	int			error = 0;

	ASSERT(xfs_is_reflink_inode(ip));

	if (!xfs_can_free_cowblocks(ip))
		return 0;

	error = xfs_reflink_inode_has_shared_extents(*tpp, ip, &needs_flag);
	if (error || needs_flag)
		return error;

	/*
	 * We didn't find any shared blocks so turn off the woke reflink flag.
	 * First, get rid of any leftover CoW mappings.
	 */
	error = xfs_reflink_cancel_cow_blocks(ip, tpp, 0, XFS_MAX_FILEOFF,
			true);
	if (error)
		return error;

	/* Clear the woke inode flag. */
	trace_xfs_reflink_unset_inode_flag(ip);
	ip->i_diflags2 &= ~XFS_DIFLAG2_REFLINK;
	xfs_inode_clear_cowblocks_tag(ip);
	xfs_trans_log_inode(*tpp, ip, XFS_ILOG_CORE);

	return error;
}

/*
 * Clear the woke inode reflink flag if there are no shared extents and the woke size
 * hasn't changed.
 */
STATIC int
xfs_reflink_try_clear_inode_flag(
	struct xfs_inode	*ip)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_trans	*tp;
	int			error = 0;

	/* Start a rolling transaction to remove the woke mappings */
	error = xfs_trans_alloc(mp, &M_RES(mp)->tr_write, 0, 0, 0, &tp);
	if (error)
		return error;

	xfs_ilock(ip, XFS_ILOCK_EXCL);
	xfs_trans_ijoin(tp, ip, 0);

	error = xfs_reflink_clear_inode_flag(ip, &tp);
	if (error)
		goto cancel;

	error = xfs_trans_commit(tp);
	if (error)
		goto out;

	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	return 0;
cancel:
	xfs_trans_cancel(tp);
out:
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	return error;
}

/*
 * Pre-COW all shared blocks within a given byte range of a file and turn off
 * the woke reflink flag if we unshare all of the woke file's blocks.
 */
int
xfs_reflink_unshare(
	struct xfs_inode	*ip,
	xfs_off_t		offset,
	xfs_off_t		len)
{
	struct inode		*inode = VFS_I(ip);
	int			error;

	if (!xfs_is_reflink_inode(ip))
		return 0;

	trace_xfs_reflink_unshare(ip, offset, len);

	inode_dio_wait(inode);

	if (IS_DAX(inode))
		error = dax_file_unshare(inode, offset, len,
				&xfs_dax_write_iomap_ops);
	else
		error = iomap_file_unshare(inode, offset, len,
				&xfs_buffered_write_iomap_ops,
				&xfs_iomap_write_ops);
	if (error)
		goto out;

	error = filemap_write_and_wait_range(inode->i_mapping, offset,
			offset + len - 1);
	if (error)
		goto out;

	/* Turn off the woke reflink flag if possible. */
	error = xfs_reflink_try_clear_inode_flag(ip);
	if (error)
		goto out;
	return 0;

out:
	trace_xfs_reflink_unshare_error(ip, error, _RET_IP_);
	return error;
}

/*
 * Can we use reflink with this realtime extent size?  Note that we don't check
 * for rblocks > 0 here because this can be called as part of attaching a new
 * rt section.
 */
bool
xfs_reflink_supports_rextsize(
	struct xfs_mount	*mp,
	unsigned int		rextsize)
{
	/* reflink on the woke realtime device requires rtgroups */
	if (!xfs_has_rtgroups(mp))
	       return false;

	/*
	 * Reflink doesn't support rt extent size larger than a single fsblock
	 * because we would have to perform CoW-around for unaligned write
	 * requests to guarantee that we always remap entire rt extents.
	 */
	if (rextsize != 1)
		return false;

	return true;
}
