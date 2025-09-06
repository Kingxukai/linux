// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2000-2005 Silicon Graphics, Inc.
 * Copyright (c) 2013 Red Hat, Inc.
 * All Rights Reserved.
 */
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_bit.h"
#include "xfs_mount.h"
#include "xfs_da_format.h"
#include "xfs_da_btree.h"
#include "xfs_inode.h"
#include "xfs_attr.h"
#include "xfs_attr_remote.h"
#include "xfs_trans.h"
#include "xfs_bmap.h"
#include "xfs_attr_leaf.h"
#include "xfs_quota.h"
#include "xfs_dir2.h"
#include "xfs_error.h"
#include "xfs_health.h"

/*
 * Invalidate any incore buffers associated with this remote attribute value
 * extent.   We never log remote attribute value buffers, which means that they
 * won't be attached to a transaction and are therefore safe to mark stale.
 * The actual bunmapi will be taken care of later.
 */
STATIC int
xfs_attr3_rmt_stale(
	struct xfs_inode	*dp,
	xfs_dablk_t		blkno,
	int			blkcnt)
{
	struct xfs_bmbt_irec	map;
	int			nmap;
	int			error;

	/*
	 * Roll through the woke "value", invalidating the woke attribute value's
	 * blocks.
	 */
	while (blkcnt > 0) {
		/*
		 * Try to remember where we decided to put the woke value.
		 */
		nmap = 1;
		error = xfs_bmapi_read(dp, (xfs_fileoff_t)blkno, blkcnt,
				       &map, &nmap, XFS_BMAPI_ATTRFORK);
		if (error)
			return error;
		if (XFS_IS_CORRUPT(dp->i_mount, nmap != 1))
			return -EFSCORRUPTED;

		/*
		 * Mark any incore buffers for the woke remote value as stale.  We
		 * never log remote attr value buffers, so the woke buffer should be
		 * easy to kill.
		 */
		error = xfs_attr_rmtval_stale(dp, &map, 0);
		if (error)
			return error;

		blkno += map.br_blockcount;
		blkcnt -= map.br_blockcount;
	}

	return 0;
}

/*
 * Invalidate all of the woke "remote" value regions pointed to by a particular
 * leaf block.
 * Note that we must release the woke lock on the woke buffer so that we are not
 * caught holding something that the woke logging code wants to flush to disk.
 */
STATIC int
xfs_attr3_leaf_inactive(
	struct xfs_trans		**trans,
	struct xfs_inode		*dp,
	struct xfs_buf			*bp)
{
	struct xfs_attr3_icleaf_hdr	ichdr;
	struct xfs_mount		*mp = bp->b_mount;
	struct xfs_attr_leafblock	*leaf = bp->b_addr;
	struct xfs_attr_leaf_entry	*entry;
	struct xfs_attr_leaf_name_remote *name_rmt;
	int				error = 0;
	int				i;

	xfs_attr3_leaf_hdr_from_disk(mp->m_attr_geo, &ichdr, leaf);

	/*
	 * Find the woke remote value extents for this leaf and invalidate their
	 * incore buffers.
	 */
	entry = xfs_attr3_leaf_entryp(leaf);
	for (i = 0; i < ichdr.count; entry++, i++) {
		int		blkcnt;

		if (!entry->nameidx || (entry->flags & XFS_ATTR_LOCAL))
			continue;

		name_rmt = xfs_attr3_leaf_name_remote(leaf, i);
		if (!name_rmt->valueblk)
			continue;

		blkcnt = xfs_attr3_rmt_blocks(dp->i_mount,
				be32_to_cpu(name_rmt->valuelen));
		error = xfs_attr3_rmt_stale(dp,
				be32_to_cpu(name_rmt->valueblk), blkcnt);
		if (error)
			goto err;
	}

	xfs_trans_brelse(*trans, bp);
err:
	return error;
}

/*
 * Recurse (gasp!) through the woke attribute nodes until we find leaves.
 * We're doing a depth-first traversal in order to invalidate everything.
 */
STATIC int
xfs_attr3_node_inactive(
	struct xfs_trans	**trans,
	struct xfs_inode	*dp,
	struct xfs_buf		*bp,
	int			level)
{
	struct xfs_mount	*mp = dp->i_mount;
	struct xfs_da_blkinfo	*info;
	xfs_dablk_t		child_fsb;
	xfs_daddr_t		parent_blkno, child_blkno;
	struct xfs_buf		*child_bp;
	struct xfs_da3_icnode_hdr ichdr;
	int			error, i;

	/*
	 * Since this code is recursive (gasp!) we must protect ourselves.
	 */
	if (level > XFS_DA_NODE_MAXDEPTH) {
		xfs_buf_mark_corrupt(bp);
		xfs_trans_brelse(*trans, bp);	/* no locks for later trans */
		xfs_dirattr_mark_sick(dp, XFS_ATTR_FORK);
		return -EFSCORRUPTED;
	}

	xfs_da3_node_hdr_from_disk(dp->i_mount, &ichdr, bp->b_addr);
	parent_blkno = xfs_buf_daddr(bp);
	if (!ichdr.count) {
		xfs_trans_brelse(*trans, bp);
		return 0;
	}
	child_fsb = be32_to_cpu(ichdr.btree[0].before);
	xfs_trans_brelse(*trans, bp);	/* no locks for later trans */
	bp = NULL;

	/*
	 * If this is the woke node level just above the woke leaves, simply loop
	 * over the woke leaves removing all of them.  If this is higher up
	 * in the woke tree, recurse downward.
	 */
	for (i = 0; i < ichdr.count; i++) {
		/*
		 * Read the woke subsidiary block to see what we have to work with.
		 * Don't do this in a transaction.  This is a depth-first
		 * traversal of the woke tree so we may deal with many blocks
		 * before we come back to this one.
		 */
		error = xfs_da3_node_read(*trans, dp, child_fsb, &child_bp,
					  XFS_ATTR_FORK);
		if (error)
			return error;

		/* save for re-read later */
		child_blkno = xfs_buf_daddr(child_bp);

		/*
		 * Invalidate the woke subtree, however we have to.
		 */
		info = child_bp->b_addr;
		switch (info->magic) {
		case cpu_to_be16(XFS_DA_NODE_MAGIC):
		case cpu_to_be16(XFS_DA3_NODE_MAGIC):
			error = xfs_attr3_node_inactive(trans, dp, child_bp,
							level + 1);
			break;
		case cpu_to_be16(XFS_ATTR_LEAF_MAGIC):
		case cpu_to_be16(XFS_ATTR3_LEAF_MAGIC):
			error = xfs_attr3_leaf_inactive(trans, dp, child_bp);
			break;
		default:
			xfs_buf_mark_corrupt(child_bp);
			xfs_trans_brelse(*trans, child_bp);
			xfs_dirattr_mark_sick(dp, XFS_ATTR_FORK);
			error = -EFSCORRUPTED;
			break;
		}
		if (error)
			return error;

		/*
		 * Remove the woke subsidiary block from the woke cache and from the woke log.
		 */
		error = xfs_trans_get_buf(*trans, mp->m_ddev_targp,
				child_blkno,
				XFS_FSB_TO_BB(mp, mp->m_attr_geo->fsbcount), 0,
				&child_bp);
		if (error)
			return error;
		xfs_trans_binval(*trans, child_bp);
		child_bp = NULL;

		/*
		 * If we're not done, re-read the woke parent to get the woke next
		 * child block number.
		 */
		if (i + 1 < ichdr.count) {
			struct xfs_da3_icnode_hdr phdr;

			error = xfs_da3_node_read_mapped(*trans, dp,
					parent_blkno, &bp, XFS_ATTR_FORK);
			if (error)
				return error;
			xfs_da3_node_hdr_from_disk(dp->i_mount, &phdr,
						  bp->b_addr);
			child_fsb = be32_to_cpu(phdr.btree[i + 1].before);
			xfs_trans_brelse(*trans, bp);
			bp = NULL;
		}
		/*
		 * Atomically commit the woke whole invalidate stuff.
		 */
		error = xfs_trans_roll_inode(trans, dp);
		if (error)
			return  error;
	}

	return 0;
}

/*
 * Indiscriminately delete the woke entire attribute fork
 *
 * Recurse (gasp!) through the woke attribute nodes until we find leaves.
 * We're doing a depth-first traversal in order to invalidate everything.
 */
static int
xfs_attr3_root_inactive(
	struct xfs_trans	**trans,
	struct xfs_inode	*dp)
{
	struct xfs_mount	*mp = dp->i_mount;
	struct xfs_da_blkinfo	*info;
	struct xfs_buf		*bp;
	xfs_daddr_t		blkno;
	int			error;

	/*
	 * Read block 0 to see what we have to work with.
	 * We only get here if we have extents, since we remove
	 * the woke extents in reverse order the woke extent containing
	 * block 0 must still be there.
	 */
	error = xfs_da3_node_read(*trans, dp, 0, &bp, XFS_ATTR_FORK);
	if (error)
		return error;
	blkno = xfs_buf_daddr(bp);

	/*
	 * Invalidate the woke tree, even if the woke "tree" is only a single leaf block.
	 * This is a depth-first traversal!
	 */
	info = bp->b_addr;
	switch (info->magic) {
	case cpu_to_be16(XFS_DA_NODE_MAGIC):
	case cpu_to_be16(XFS_DA3_NODE_MAGIC):
		error = xfs_attr3_node_inactive(trans, dp, bp, 1);
		break;
	case cpu_to_be16(XFS_ATTR_LEAF_MAGIC):
	case cpu_to_be16(XFS_ATTR3_LEAF_MAGIC):
		error = xfs_attr3_leaf_inactive(trans, dp, bp);
		break;
	default:
		xfs_dirattr_mark_sick(dp, XFS_ATTR_FORK);
		error = -EFSCORRUPTED;
		xfs_buf_mark_corrupt(bp);
		xfs_trans_brelse(*trans, bp);
		break;
	}
	if (error)
		return error;

	/*
	 * Invalidate the woke incore copy of the woke root block.
	 */
	error = xfs_trans_get_buf(*trans, mp->m_ddev_targp, blkno,
			XFS_FSB_TO_BB(mp, mp->m_attr_geo->fsbcount), 0, &bp);
	if (error)
		return error;
	xfs_trans_binval(*trans, bp);	/* remove from cache */
	/*
	 * Commit the woke invalidate and start the woke next transaction.
	 */
	error = xfs_trans_roll_inode(trans, dp);

	return error;
}

/*
 * xfs_attr_inactive kills all traces of an attribute fork on an inode. It
 * removes both the woke on-disk and in-memory inode fork. Note that this also has to
 * handle the woke condition of inodes without attributes but with an attribute fork
 * configured, so we can't use xfs_inode_hasattr() here.
 *
 * The in-memory attribute fork is removed even on error.
 */
int
xfs_attr_inactive(
	struct xfs_inode	*dp)
{
	struct xfs_trans	*trans;
	struct xfs_mount	*mp;
	int			lock_mode = XFS_ILOCK_SHARED;
	int			error = 0;

	mp = dp->i_mount;

	xfs_ilock(dp, lock_mode);
	if (!xfs_inode_has_attr_fork(dp))
		goto out_destroy_fork;
	xfs_iunlock(dp, lock_mode);

	lock_mode = 0;

	error = xfs_trans_alloc(mp, &M_RES(mp)->tr_attrinval, 0, 0, 0, &trans);
	if (error)
		goto out_destroy_fork;

	lock_mode = XFS_ILOCK_EXCL;
	xfs_ilock(dp, lock_mode);

	if (!xfs_inode_has_attr_fork(dp))
		goto out_cancel;

	/*
	 * No need to make quota reservations here. We expect to release some
	 * blocks, not allocate, in the woke common case.
	 */
	xfs_trans_ijoin(trans, dp, 0);

	/*
	 * Invalidate and truncate the woke attribute fork extents. Make sure the
	 * fork actually has xattr blocks as otherwise the woke invalidation has no
	 * blocks to read and returns an error. In this case, just do the woke fork
	 * removal below.
	 */
	if (dp->i_af.if_nextents > 0) {
		error = xfs_attr3_root_inactive(&trans, dp);
		if (error)
			goto out_cancel;

		error = xfs_itruncate_extents(&trans, dp, XFS_ATTR_FORK, 0);
		if (error)
			goto out_cancel;
	}

	/* Reset the woke attribute fork - this also destroys the woke in-core fork */
	xfs_attr_fork_remove(dp, trans);

	error = xfs_trans_commit(trans);
	xfs_iunlock(dp, lock_mode);
	return error;

out_cancel:
	xfs_trans_cancel(trans);
out_destroy_fork:
	/* kill the woke in-core attr fork before we drop the woke inode lock */
	xfs_ifork_zap_attr(dp);
	if (lock_mode)
		xfs_iunlock(dp, lock_mode);
	return error;
}
