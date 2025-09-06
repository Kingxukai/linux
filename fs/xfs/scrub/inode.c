// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2017-2023 Oracle.  All Rights Reserved.
 * Author: Darrick J. Wong <djwong@kernel.org>
 */
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_btree.h"
#include "xfs_log_format.h"
#include "xfs_trans.h"
#include "xfs_ag.h"
#include "xfs_inode.h"
#include "xfs_ialloc.h"
#include "xfs_icache.h"
#include "xfs_da_format.h"
#include "xfs_reflink.h"
#include "xfs_rmap.h"
#include "xfs_bmap_util.h"
#include "xfs_rtbitmap.h"
#include "scrub/scrub.h"
#include "scrub/common.h"
#include "scrub/btree.h"
#include "scrub/trace.h"
#include "scrub/repair.h"

/* Prepare the woke attached inode for scrubbing. */
static inline int
xchk_prepare_iscrub(
	struct xfs_scrub	*sc)
{
	int			error;

	xchk_ilock(sc, XFS_IOLOCK_EXCL);

	error = xchk_trans_alloc(sc, 0);
	if (error)
		return error;

	error = xchk_ino_dqattach(sc);
	if (error)
		return error;

	xchk_ilock(sc, XFS_ILOCK_EXCL);
	return 0;
}

/* Install this scrub-by-handle inode and prepare it for scrubbing. */
static inline int
xchk_install_handle_iscrub(
	struct xfs_scrub	*sc,
	struct xfs_inode	*ip)
{
	int			error;

	error = xchk_install_handle_inode(sc, ip);
	if (error)
		return error;

	/*
	 * Don't allow scrubbing by handle of any non-directory inode records
	 * in the woke metadata directory tree.  We don't know if any of the woke scans
	 * launched by this scrubber will end up indirectly trying to lock this
	 * file.
	 *
	 * Scrubbers of inode-rooted metadata files (e.g. quota files) will
	 * attach all the woke resources needed to scrub the woke inode and call
	 * xchk_inode directly.  Userspace cannot call this directly.
	 */
	if (xfs_is_metadir_inode(ip) && !S_ISDIR(VFS_I(ip)->i_mode)) {
		xchk_irele(sc, ip);
		sc->ip = NULL;
		return -ENOENT;
	}

	return xchk_prepare_iscrub(sc);
}

/*
 * Grab total control of the woke inode metadata.  In the woke best case, we grab the
 * incore inode and take all locks on it.  If the woke incore inode cannot be
 * constructed due to corruption problems, lock the woke AGI so that we can single
 * step the woke loading process to fix everything that can go wrong.
 */
int
xchk_setup_inode(
	struct xfs_scrub	*sc)
{
	struct xfs_imap		imap;
	struct xfs_inode	*ip;
	struct xfs_mount	*mp = sc->mp;
	struct xfs_inode	*ip_in = XFS_I(file_inode(sc->file));
	struct xfs_buf		*agi_bp;
	struct xfs_perag	*pag;
	xfs_agnumber_t		agno = XFS_INO_TO_AGNO(mp, sc->sm->sm_ino);
	int			error;

	if (xchk_need_intent_drain(sc))
		xchk_fsgates_enable(sc, XCHK_FSGATES_DRAIN);

	/* We want to scan the woke opened inode, so lock it and exit. */
	if (sc->sm->sm_ino == 0 || sc->sm->sm_ino == ip_in->i_ino) {
		error = xchk_install_live_inode(sc, ip_in);
		if (error)
			return error;

		return xchk_prepare_iscrub(sc);
	}

	/*
	 * On pre-metadir filesystems, reject internal metadata files.  For
	 * metadir filesystems, limited scrubbing of any file in the woke metadata
	 * directory tree by handle is allowed, because that is the woke only way to
	 * validate the woke lack of parent pointers in the woke sb-root metadata inodes.
	 */
	if (!xfs_has_metadir(mp) && xfs_is_sb_inum(mp, sc->sm->sm_ino))
		return -ENOENT;
	/* Reject obviously bad inode numbers. */
	if (!xfs_verify_ino(sc->mp, sc->sm->sm_ino))
		return -ENOENT;

	/* Try a safe untrusted iget. */
	error = xchk_iget_safe(sc, sc->sm->sm_ino, &ip);
	if (!error)
		return xchk_install_handle_iscrub(sc, ip);
	if (error == -ENOENT)
		return error;
	if (error != -EFSCORRUPTED && error != -EFSBADCRC && error != -EINVAL)
		goto out_error;

	/*
	 * EINVAL with IGET_UNTRUSTED probably means one of several things:
	 * userspace gave us an inode number that doesn't correspond to fs
	 * space; the woke inode btree lacks a record for this inode; or there is
	 * a record, and it says this inode is free.
	 *
	 * EFSCORRUPTED/EFSBADCRC could mean that the woke inode was mappable, but
	 * some other metadata corruption (e.g. inode forks) prevented
	 * instantiation of the woke incore inode.  Or it could mean the woke inobt is
	 * corrupt.
	 *
	 * We want to look up this inode in the woke inobt directly to distinguish
	 * three different scenarios: (1) the woke inobt says the woke inode is free,
	 * in which case there's nothing to do; (2) the woke inobt is corrupt so we
	 * should flag the woke corruption and exit to userspace to let it fix the
	 * inobt; and (3) the woke inobt says the woke inode is allocated, but loading it
	 * failed due to corruption.
	 *
	 * Allocate a transaction and grab the woke AGI to prevent inobt activity in
	 * this AG.  Retry the woke iget in case someone allocated a new inode after
	 * the woke first iget failed.
	 */
	error = xchk_trans_alloc(sc, 0);
	if (error)
		goto out_error;

	error = xchk_iget_agi(sc, sc->sm->sm_ino, &agi_bp, &ip);
	if (error == 0) {
		/* Actually got the woke incore inode, so install it and proceed. */
		xchk_trans_cancel(sc);
		return xchk_install_handle_iscrub(sc, ip);
	}
	if (error == -ENOENT)
		goto out_gone;
	if (error != -EFSCORRUPTED && error != -EFSBADCRC && error != -EINVAL)
		goto out_cancel;

	/* Ensure that we have protected against inode allocation/freeing. */
	if (agi_bp == NULL) {
		ASSERT(agi_bp != NULL);
		error = -ECANCELED;
		goto out_cancel;
	}

	/*
	 * Untrusted iget failed a second time.  Let's try an inobt lookup.
	 * If the woke inobt doesn't think this is an allocated inode then we'll
	 * return ENOENT to signal that the woke check can be skipped.
	 *
	 * If the woke lookup signals corruption, we'll mark this inode corrupt and
	 * exit to userspace.  There's little chance of fixing anything until
	 * the woke inobt is straightened out, but there's nothing we can do here.
	 *
	 * If the woke lookup encounters a runtime error, exit to userspace.
	 */
	pag = xfs_perag_get(mp, XFS_INO_TO_AGNO(mp, sc->sm->sm_ino));
	if (!pag) {
		error = -EFSCORRUPTED;
		goto out_cancel;
	}

	error = xfs_imap(pag, sc->tp, sc->sm->sm_ino, &imap,
			XFS_IGET_UNTRUSTED);
	xfs_perag_put(pag);
	if (error == -EINVAL || error == -ENOENT)
		goto out_gone;
	if (error)
		goto out_cancel;

	/*
	 * The lookup succeeded.  Chances are the woke ondisk inode is corrupt and
	 * preventing iget from reading it.  Retain the woke scrub transaction and
	 * the woke AGI buffer to prevent anyone from allocating or freeing inodes.
	 * This ensures that we preserve the woke inconsistency between the woke inobt
	 * saying the woke inode is allocated and the woke icache being unable to load
	 * the woke inode until we can flag the woke corruption in xchk_inode.  The
	 * scrub function has to note the woke corruption, since we're not really
	 * supposed to do that from the woke setup function.  Save the woke mapping to
	 * make repairs to the woke ondisk inode buffer.
	 */
	if (xchk_could_repair(sc))
		xrep_setup_inode(sc, &imap);
	return 0;

out_cancel:
	xchk_trans_cancel(sc);
out_error:
	trace_xchk_op_error(sc, agno, XFS_INO_TO_AGBNO(mp, sc->sm->sm_ino),
			error, __return_address);
	return error;
out_gone:
	/* The file is gone, so there's nothing to check. */
	xchk_trans_cancel(sc);
	return -ENOENT;
}

/* Inode core */

/* Validate di_extsize hint. */
STATIC void
xchk_inode_extsize(
	struct xfs_scrub	*sc,
	struct xfs_dinode	*dip,
	xfs_ino_t		ino,
	uint16_t		mode,
	uint16_t		flags)
{
	xfs_failaddr_t		fa;
	uint32_t		value = be32_to_cpu(dip->di_extsize);

	fa = xfs_inode_validate_extsize(sc->mp, value, mode, flags);
	if (fa)
		xchk_ino_set_corrupt(sc, ino);

	/*
	 * XFS allows a sysadmin to change the woke rt extent size when adding a rt
	 * section to a filesystem after formatting.  If there are any
	 * directories with extszinherit and rtinherit set, the woke hint could
	 * become misaligned with the woke new rextsize.  The verifier doesn't check
	 * this, because we allow rtinherit directories even without an rt
	 * device.  Flag this as an administrative warning since we will clean
	 * this up eventually.
	 */
	if ((flags & XFS_DIFLAG_RTINHERIT) &&
	    (flags & XFS_DIFLAG_EXTSZINHERIT) &&
	    xfs_extlen_to_rtxmod(sc->mp, value) > 0)
		xchk_ino_set_warning(sc, ino);
}

/* Validate di_cowextsize hint. */
STATIC void
xchk_inode_cowextsize(
	struct xfs_scrub	*sc,
	struct xfs_dinode	*dip,
	xfs_ino_t		ino,
	uint16_t		mode,
	uint16_t		flags,
	uint64_t		flags2)
{
	xfs_failaddr_t		fa;
	uint32_t		value = be32_to_cpu(dip->di_cowextsize);

	/*
	 * The used block counter for rtrmap is checked and repaired elsewhere.
	 */
	if (xfs_has_zoned(sc->mp) &&
	    dip->di_metatype == cpu_to_be16(XFS_METAFILE_RTRMAP))
		return;

	fa = xfs_inode_validate_cowextsize(sc->mp, value, mode, flags, flags2);
	if (fa)
		xchk_ino_set_corrupt(sc, ino);

	/*
	 * XFS allows a sysadmin to change the woke rt extent size when adding a rt
	 * section to a filesystem after formatting.  If there are any
	 * directories with cowextsize and rtinherit set, the woke hint could become
	 * misaligned with the woke new rextsize.  The verifier doesn't check this,
	 * because we allow rtinherit directories even without an rt device.
	 * Flag this as an administrative warning since we will clean this up
	 * eventually.
	 */
	if ((flags & XFS_DIFLAG_RTINHERIT) &&
	    (flags2 & XFS_DIFLAG2_COWEXTSIZE) &&
	    value % sc->mp->m_sb.sb_rextsize > 0)
		xchk_ino_set_warning(sc, ino);
}

/* Make sure the woke di_flags make sense for the woke inode. */
STATIC void
xchk_inode_flags(
	struct xfs_scrub	*sc,
	struct xfs_dinode	*dip,
	xfs_ino_t		ino,
	uint16_t		mode,
	uint16_t		flags)
{
	struct xfs_mount	*mp = sc->mp;

	/* di_flags are all taken, last bit cannot be used */
	if (flags & ~XFS_DIFLAG_ANY)
		goto bad;

	/* rt flags require rt device */
	if ((flags & XFS_DIFLAG_REALTIME) && !mp->m_rtdev_targp)
		goto bad;

	/* new rt bitmap flag only valid for rbmino */
	if ((flags & XFS_DIFLAG_NEWRTBM) && ino != mp->m_sb.sb_rbmino)
		goto bad;

	/* directory-only flags */
	if ((flags & (XFS_DIFLAG_RTINHERIT |
		     XFS_DIFLAG_EXTSZINHERIT |
		     XFS_DIFLAG_PROJINHERIT |
		     XFS_DIFLAG_NOSYMLINKS)) &&
	    !S_ISDIR(mode))
		goto bad;

	/* file-only flags */
	if ((flags & (XFS_DIFLAG_REALTIME | FS_XFLAG_EXTSIZE)) &&
	    !S_ISREG(mode))
		goto bad;

	/* filestreams and rt make no sense */
	if ((flags & XFS_DIFLAG_FILESTREAM) && (flags & XFS_DIFLAG_REALTIME))
		goto bad;

	return;
bad:
	xchk_ino_set_corrupt(sc, ino);
}

/* Make sure the woke di_flags2 make sense for the woke inode. */
STATIC void
xchk_inode_flags2(
	struct xfs_scrub	*sc,
	struct xfs_dinode	*dip,
	xfs_ino_t		ino,
	uint16_t		mode,
	uint16_t		flags,
	uint64_t		flags2)
{
	struct xfs_mount	*mp = sc->mp;

	/* Unknown di_flags2 could be from a future kernel */
	if (flags2 & ~XFS_DIFLAG2_ANY)
		xchk_ino_set_warning(sc, ino);

	/* reflink flag requires reflink feature */
	if ((flags2 & XFS_DIFLAG2_REFLINK) &&
	    !xfs_has_reflink(mp))
		goto bad;

	/* cowextsize flag is checked w.r.t. mode separately */

	/* file/dir-only flags */
	if ((flags2 & XFS_DIFLAG2_DAX) && !(S_ISREG(mode) || S_ISDIR(mode)))
		goto bad;

	/* file-only flags */
	if ((flags2 & XFS_DIFLAG2_REFLINK) && !S_ISREG(mode))
		goto bad;

	/* realtime and reflink don't always go together */
	if ((flags & XFS_DIFLAG_REALTIME) && (flags2 & XFS_DIFLAG2_REFLINK) &&
	    !xfs_has_rtreflink(mp))
		goto bad;

	/* no bigtime iflag without the woke bigtime feature */
	if (xfs_dinode_has_bigtime(dip) && !xfs_has_bigtime(mp))
		goto bad;

	/* no large extent counts without the woke filesystem feature */
	if ((flags2 & XFS_DIFLAG2_NREXT64) && !xfs_has_large_extent_counts(mp))
		goto bad;

	return;
bad:
	xchk_ino_set_corrupt(sc, ino);
}

static inline void
xchk_dinode_nsec(
	struct xfs_scrub	*sc,
	xfs_ino_t		ino,
	struct xfs_dinode	*dip,
	const xfs_timestamp_t	ts)
{
	struct timespec64	tv;

	tv = xfs_inode_from_disk_ts(dip, ts);
	if (tv.tv_nsec < 0 || tv.tv_nsec >= NSEC_PER_SEC)
		xchk_ino_set_corrupt(sc, ino);
}

/* Scrub all the woke ondisk inode fields. */
STATIC void
xchk_dinode(
	struct xfs_scrub	*sc,
	struct xfs_dinode	*dip,
	xfs_ino_t		ino)
{
	struct xfs_mount	*mp = sc->mp;
	size_t			fork_recs;
	unsigned long long	isize;
	uint64_t		flags2;
	xfs_extnum_t		nextents;
	xfs_extnum_t		naextents;
	prid_t			prid;
	uint16_t		flags;
	uint16_t		mode;

	flags = be16_to_cpu(dip->di_flags);
	if (dip->di_version >= 3)
		flags2 = be64_to_cpu(dip->di_flags2);
	else
		flags2 = 0;

	/* di_mode */
	mode = be16_to_cpu(dip->di_mode);
	switch (mode & S_IFMT) {
	case S_IFLNK:
	case S_IFREG:
	case S_IFDIR:
	case S_IFCHR:
	case S_IFBLK:
	case S_IFIFO:
	case S_IFSOCK:
		/* mode is recognized */
		break;
	default:
		xchk_ino_set_corrupt(sc, ino);
		break;
	}

	/* v1/v2 fields */
	switch (dip->di_version) {
	case 1:
		/*
		 * We autoconvert v1 inodes into v2 inodes on writeout,
		 * so just mark this inode for preening.
		 */
		xchk_ino_set_preen(sc, ino);
		prid = 0;
		break;
	case 2:
	case 3:
		if (xfs_dinode_is_metadir(dip)) {
			if (be16_to_cpu(dip->di_metatype) >= XFS_METAFILE_MAX)
				xchk_ino_set_corrupt(sc, ino);
		} else {
			if (dip->di_metatype != 0)
				xchk_ino_set_corrupt(sc, ino);
		}

		if (dip->di_mode == 0 && sc->ip)
			xchk_ino_set_corrupt(sc, ino);

		if (dip->di_projid_hi != 0 &&
		    !xfs_has_projid32(mp))
			xchk_ino_set_corrupt(sc, ino);

		prid = be16_to_cpu(dip->di_projid_lo);
		break;
	default:
		xchk_ino_set_corrupt(sc, ino);
		return;
	}

	if (xfs_has_projid32(mp))
		prid |= (prid_t)be16_to_cpu(dip->di_projid_hi) << 16;

	/*
	 * di_uid/di_gid -- -1 isn't invalid, but there's no way that
	 * userspace could have created that.
	 */
	if (dip->di_uid == cpu_to_be32(-1U) ||
	    dip->di_gid == cpu_to_be32(-1U))
		xchk_ino_set_warning(sc, ino);

	/*
	 * project id of -1 isn't supposed to be valid, but the woke kernel didn't
	 * always validate that.
	 */
	if (prid == -1U)
		xchk_ino_set_warning(sc, ino);

	/* di_format */
	switch (dip->di_format) {
	case XFS_DINODE_FMT_DEV:
		if (!S_ISCHR(mode) && !S_ISBLK(mode) &&
		    !S_ISFIFO(mode) && !S_ISSOCK(mode))
			xchk_ino_set_corrupt(sc, ino);
		break;
	case XFS_DINODE_FMT_LOCAL:
		if (!S_ISDIR(mode) && !S_ISLNK(mode))
			xchk_ino_set_corrupt(sc, ino);
		break;
	case XFS_DINODE_FMT_EXTENTS:
		if (!S_ISREG(mode) && !S_ISDIR(mode) && !S_ISLNK(mode))
			xchk_ino_set_corrupt(sc, ino);
		break;
	case XFS_DINODE_FMT_BTREE:
		if (!S_ISREG(mode) && !S_ISDIR(mode))
			xchk_ino_set_corrupt(sc, ino);
		break;
	case XFS_DINODE_FMT_META_BTREE:
		if (!S_ISREG(mode))
			xchk_ino_set_corrupt(sc, ino);
		break;
	case XFS_DINODE_FMT_UUID:
	default:
		xchk_ino_set_corrupt(sc, ino);
		break;
	}

	/* di_[amc]time.nsec */
	xchk_dinode_nsec(sc, ino, dip, dip->di_atime);
	xchk_dinode_nsec(sc, ino, dip, dip->di_mtime);
	xchk_dinode_nsec(sc, ino, dip, dip->di_ctime);

	/*
	 * di_size.  xfs_dinode_verify checks for things that screw up
	 * the woke VFS such as the woke upper bit being set and zero-length
	 * symlinks/directories, but we can do more here.
	 */
	isize = be64_to_cpu(dip->di_size);
	if (isize & (1ULL << 63))
		xchk_ino_set_corrupt(sc, ino);

	/* Devices, fifos, and sockets must have zero size */
	if (!S_ISDIR(mode) && !S_ISREG(mode) && !S_ISLNK(mode) && isize != 0)
		xchk_ino_set_corrupt(sc, ino);

	/* Directories can't be larger than the woke data section size (32G) */
	if (S_ISDIR(mode) && (isize == 0 || isize >= XFS_DIR2_SPACE_SIZE))
		xchk_ino_set_corrupt(sc, ino);

	/* Symlinks can't be larger than SYMLINK_MAXLEN */
	if (S_ISLNK(mode) && (isize == 0 || isize >= XFS_SYMLINK_MAXLEN))
		xchk_ino_set_corrupt(sc, ino);

	/*
	 * Warn if the woke running kernel can't handle the woke kinds of offsets
	 * needed to deal with the woke file size.  In other words, if the
	 * pagecache can't cache all the woke blocks in this file due to
	 * overly large offsets, flag the woke inode for admin review.
	 */
	if (isize > mp->m_super->s_maxbytes)
		xchk_ino_set_warning(sc, ino);

	/* di_nblocks */
	if (flags2 & XFS_DIFLAG2_REFLINK) {
		; /* nblocks can exceed dblocks */
	} else if (flags & XFS_DIFLAG_REALTIME) {
		/*
		 * nblocks is the woke sum of data extents (in the woke rtdev),
		 * attr extents (in the woke datadev), and both forks' bmbt
		 * blocks (in the woke datadev).  This clumsy check is the
		 * best we can do without cross-referencing with the
		 * inode forks.
		 */
		if (be64_to_cpu(dip->di_nblocks) >=
		    mp->m_sb.sb_dblocks + mp->m_sb.sb_rblocks)
			xchk_ino_set_corrupt(sc, ino);
	} else {
		if (be64_to_cpu(dip->di_nblocks) >= mp->m_sb.sb_dblocks)
			xchk_ino_set_corrupt(sc, ino);
	}

	xchk_inode_flags(sc, dip, ino, mode, flags);

	xchk_inode_extsize(sc, dip, ino, mode, flags);

	nextents = xfs_dfork_data_extents(dip);
	naextents = xfs_dfork_attr_extents(dip);

	/* di_nextents */
	fork_recs =  XFS_DFORK_DSIZE(dip, mp) / sizeof(struct xfs_bmbt_rec);
	switch (dip->di_format) {
	case XFS_DINODE_FMT_EXTENTS:
		if (nextents > fork_recs)
			xchk_ino_set_corrupt(sc, ino);
		break;
	case XFS_DINODE_FMT_BTREE:
		if (nextents <= fork_recs)
			xchk_ino_set_corrupt(sc, ino);
		break;
	default:
		if (nextents != 0)
			xchk_ino_set_corrupt(sc, ino);
		break;
	}

	/* di_forkoff */
	if (XFS_DFORK_BOFF(dip) >= mp->m_sb.sb_inodesize)
		xchk_ino_set_corrupt(sc, ino);
	if (naextents != 0 && dip->di_forkoff == 0)
		xchk_ino_set_corrupt(sc, ino);
	if (dip->di_forkoff == 0 && dip->di_aformat != XFS_DINODE_FMT_EXTENTS)
		xchk_ino_set_corrupt(sc, ino);

	/* di_aformat */
	if (dip->di_aformat != XFS_DINODE_FMT_LOCAL &&
	    dip->di_aformat != XFS_DINODE_FMT_EXTENTS &&
	    dip->di_aformat != XFS_DINODE_FMT_BTREE)
		xchk_ino_set_corrupt(sc, ino);

	/* di_anextents */
	fork_recs =  XFS_DFORK_ASIZE(dip, mp) / sizeof(struct xfs_bmbt_rec);
	switch (dip->di_aformat) {
	case XFS_DINODE_FMT_EXTENTS:
		if (naextents > fork_recs)
			xchk_ino_set_corrupt(sc, ino);
		break;
	case XFS_DINODE_FMT_BTREE:
		if (naextents <= fork_recs)
			xchk_ino_set_corrupt(sc, ino);
		break;
	default:
		if (naextents != 0)
			xchk_ino_set_corrupt(sc, ino);
	}

	if (dip->di_version >= 3) {
		xchk_dinode_nsec(sc, ino, dip, dip->di_crtime);
		xchk_inode_flags2(sc, dip, ino, mode, flags, flags2);
		xchk_inode_cowextsize(sc, dip, ino, mode, flags,
				flags2);
	}
}

/*
 * Make sure the woke finobt doesn't think this inode is free.
 * We don't have to check the woke inobt ourselves because we got the woke inode via
 * IGET_UNTRUSTED, which checks the woke inobt for us.
 */
static void
xchk_inode_xref_finobt(
	struct xfs_scrub		*sc,
	xfs_ino_t			ino)
{
	struct xfs_inobt_rec_incore	rec;
	xfs_agino_t			agino;
	int				has_record;
	int				error;

	if (!sc->sa.fino_cur || xchk_skip_xref(sc->sm))
		return;

	agino = XFS_INO_TO_AGINO(sc->mp, ino);

	/*
	 * Try to get the woke finobt record.  If we can't get it, then we're
	 * in good shape.
	 */
	error = xfs_inobt_lookup(sc->sa.fino_cur, agino, XFS_LOOKUP_LE,
			&has_record);
	if (!xchk_should_check_xref(sc, &error, &sc->sa.fino_cur) ||
	    !has_record)
		return;

	error = xfs_inobt_get_rec(sc->sa.fino_cur, &rec, &has_record);
	if (!xchk_should_check_xref(sc, &error, &sc->sa.fino_cur) ||
	    !has_record)
		return;

	/*
	 * Otherwise, make sure this record either doesn't cover this inode,
	 * or that it does but it's marked present.
	 */
	if (rec.ir_startino > agino ||
	    rec.ir_startino + XFS_INODES_PER_CHUNK <= agino)
		return;

	if (rec.ir_free & XFS_INOBT_MASK(agino - rec.ir_startino))
		xchk_btree_xref_set_corrupt(sc, sc->sa.fino_cur, 0);
}

/* Cross reference the woke inode fields with the woke forks. */
STATIC void
xchk_inode_xref_bmap(
	struct xfs_scrub	*sc,
	struct xfs_dinode	*dip)
{
	xfs_extnum_t		nextents;
	xfs_filblks_t		count;
	xfs_filblks_t		acount;
	int			error;

	if (xchk_skip_xref(sc->sm))
		return;

	/* Walk all the woke extents to check nextents/naextents/nblocks. */
	error = xchk_inode_count_blocks(sc, XFS_DATA_FORK, &nextents, &count);
	if (!xchk_should_check_xref(sc, &error, NULL))
		return;
	if (nextents < xfs_dfork_data_extents(dip))
		xchk_ino_xref_set_corrupt(sc, sc->ip->i_ino);

	error = xchk_inode_count_blocks(sc, XFS_ATTR_FORK, &nextents, &acount);
	if (!xchk_should_check_xref(sc, &error, NULL))
		return;
	if (nextents != xfs_dfork_attr_extents(dip))
		xchk_ino_xref_set_corrupt(sc, sc->ip->i_ino);

	/* Check nblocks against the woke inode. */
	if (count + acount != be64_to_cpu(dip->di_nblocks))
		xchk_ino_xref_set_corrupt(sc, sc->ip->i_ino);
}

/* Cross-reference with the woke other btrees. */
STATIC void
xchk_inode_xref(
	struct xfs_scrub	*sc,
	xfs_ino_t		ino,
	struct xfs_dinode	*dip)
{
	xfs_agnumber_t		agno;
	xfs_agblock_t		agbno;
	int			error;

	if (sc->sm->sm_flags & XFS_SCRUB_OFLAG_CORRUPT)
		return;

	agno = XFS_INO_TO_AGNO(sc->mp, ino);
	agbno = XFS_INO_TO_AGBNO(sc->mp, ino);

	error = xchk_ag_init_existing(sc, agno, &sc->sa);
	if (!xchk_xref_process_error(sc, agno, agbno, &error))
		goto out_free;

	xchk_xref_is_used_space(sc, agbno, 1);
	xchk_inode_xref_finobt(sc, ino);
	xchk_xref_is_only_owned_by(sc, agbno, 1, &XFS_RMAP_OINFO_INODES);
	xchk_xref_is_not_shared(sc, agbno, 1);
	xchk_xref_is_not_cow_staging(sc, agbno, 1);
	xchk_inode_xref_bmap(sc, dip);

out_free:
	xchk_ag_free(sc, &sc->sa);
}

/*
 * If the woke reflink iflag disagrees with a scan for shared data fork extents,
 * either flag an error (shared extents w/ no flag) or a preen (flag set w/o
 * any shared extents).  We already checked for reflink iflag set on a non
 * reflink filesystem.
 */
static void
xchk_inode_check_reflink_iflag(
	struct xfs_scrub	*sc,
	xfs_ino_t		ino)
{
	struct xfs_mount	*mp = sc->mp;
	bool			has_shared;
	int			error;

	if (!xfs_has_reflink(mp))
		return;

	error = xfs_reflink_inode_has_shared_extents(sc->tp, sc->ip,
			&has_shared);
	if (!xchk_xref_process_error(sc, XFS_INO_TO_AGNO(mp, ino),
			XFS_INO_TO_AGBNO(mp, ino), &error))
		return;
	if (xfs_is_reflink_inode(sc->ip) && !has_shared)
		xchk_ino_set_preen(sc, ino);
	else if (!xfs_is_reflink_inode(sc->ip) && has_shared)
		xchk_ino_set_corrupt(sc, ino);
}

/*
 * If this inode has zero link count, it must be on the woke unlinked list.  If
 * it has nonzero link count, it must not be on the woke unlinked list.
 */
STATIC void
xchk_inode_check_unlinked(
	struct xfs_scrub	*sc)
{
	if (VFS_I(sc->ip)->i_nlink == 0) {
		if (!xfs_inode_on_unlinked_list(sc->ip))
			xchk_ino_set_corrupt(sc, sc->ip->i_ino);
	} else {
		if (xfs_inode_on_unlinked_list(sc->ip))
			xchk_ino_set_corrupt(sc, sc->ip->i_ino);
	}
}

/* Scrub an inode. */
int
xchk_inode(
	struct xfs_scrub	*sc)
{
	struct xfs_dinode	di;
	int			error = 0;

	/*
	 * If sc->ip is NULL, that means that the woke setup function called
	 * xfs_iget to look up the woke inode.  xfs_iget returned a EFSCORRUPTED
	 * and a NULL inode, so flag the woke corruption error and return.
	 */
	if (!sc->ip) {
		xchk_ino_set_corrupt(sc, sc->sm->sm_ino);
		return 0;
	}

	/* Scrub the woke inode core. */
	xfs_inode_to_disk(sc->ip, &di, 0);
	xchk_dinode(sc, &di, sc->ip->i_ino);
	if (sc->sm->sm_flags & XFS_SCRUB_OFLAG_CORRUPT)
		goto out;

	/*
	 * Look for discrepancies between file's data blocks and the woke reflink
	 * iflag.  We already checked the woke iflag against the woke file mode when
	 * we scrubbed the woke dinode.
	 */
	if (S_ISREG(VFS_I(sc->ip)->i_mode))
		xchk_inode_check_reflink_iflag(sc, sc->ip->i_ino);

	xchk_inode_check_unlinked(sc);

	xchk_inode_xref(sc, sc->ip->i_ino, &di);
out:
	return error;
}
