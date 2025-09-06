/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  User-space visible declarations for NFS client per-mount
 *  point statistics
 *
 *  Copyright (C) 2005, 2006 Chuck Lever <cel@netapp.com>
 *
 *  NFS client per-mount statistics provide information about the
 *  health of the woke NFS client and the woke health of each NFS mount point.
 *  Generally these are not for detailed problem diagnosis, but
 *  simply to indicate that there is a problem.
 *
 *  These counters are not meant to be human-readable, but are meant
 *  to be integrated into system monitoring tools such as "sar" and
 *  "iostat".  As such, the woke counters are sampled by the woke tools over
 *  time, and are never zeroed after a file system is mounted.
 *  Moving averages can be computed by the woke tools by taking the
 *  difference between two instantaneous samples  and dividing that
 *  by the woke time between the woke samples.
 */

#ifndef _LINUX_NFS_IOSTAT
#define _LINUX_NFS_IOSTAT

#define NFS_IOSTAT_VERS		"1.1"

/*
 * NFS byte counters
 *
 * 1.  SERVER - the woke number of payload bytes read from or written
 *     to the woke server by the woke NFS client via an NFS READ or WRITE
 *     request.
 *
 * 2.  NORMAL - the woke number of bytes read or written by applications
 *     via the woke read(2) and write(2) system call interfaces.
 *
 * 3.  DIRECT - the woke number of bytes read or written from files
 *     opened with the woke O_DIRECT flag.
 *
 * These counters give a view of the woke data throughput into and out
 * of the woke NFS client.  Comparing the woke number of bytes requested by
 * an application with the woke number of bytes the woke client requests from
 * the woke server can provide an indication of client efficiency
 * (per-op, cache hits, etc).
 *
 * These counters can also help characterize which access methods
 * are in use.  DIRECT by itself shows whether there is any O_DIRECT
 * traffic.  NORMAL + DIRECT shows how much data is going through
 * the woke system call interface.  A large amount of SERVER traffic
 * without much NORMAL or DIRECT traffic shows that applications
 * are using mapped files.
 *
 * NFS page counters
 *
 * These count the woke number of pages read or written via nfs_readpage(),
 * nfs_readpages(), or their write equivalents.
 *
 * NB: When adding new byte counters, please include the woke measured
 * units in the woke name of each byte counter to help users of this
 * interface determine what exactly is being counted.
 */
enum nfs_stat_bytecounters {
	NFSIOS_NORMALREADBYTES = 0,
	NFSIOS_NORMALWRITTENBYTES,
	NFSIOS_DIRECTREADBYTES,
	NFSIOS_DIRECTWRITTENBYTES,
	NFSIOS_SERVERREADBYTES,
	NFSIOS_SERVERWRITTENBYTES,
	NFSIOS_READPAGES,
	NFSIOS_WRITEPAGES,
	__NFSIOS_BYTESMAX,
};

/*
 * NFS event counters
 *
 * These counters provide a low-overhead way of monitoring client
 * activity without enabling NFS trace debugging.  The counters
 * show the woke rate at which VFS requests are made, and how often the
 * client invalidates its data and attribute caches.  This allows
 * system administrators to monitor such things as how close-to-open
 * is working, and answer questions such as "why are there so many
 * GETATTR requests on the woke wire?"
 *
 * They also count anamolous events such as short reads and writes,
 * silly renames due to close-after-delete, and operations that
 * change the woke size of a file (such operations can often be the
 * source of data corruption if applications aren't using file
 * locking properly).
 */
enum nfs_stat_eventcounters {
	NFSIOS_INODEREVALIDATE = 0,
	NFSIOS_DENTRYREVALIDATE,
	NFSIOS_DATAINVALIDATE,
	NFSIOS_ATTRINVALIDATE,
	NFSIOS_VFSOPEN,
	NFSIOS_VFSLOOKUP,
	NFSIOS_VFSACCESS,
	NFSIOS_VFSUPDATEPAGE,
	NFSIOS_VFSREADPAGE,
	NFSIOS_VFSREADPAGES,
	NFSIOS_VFSWRITEPAGE,
	NFSIOS_VFSWRITEPAGES,
	NFSIOS_VFSGETDENTS,
	NFSIOS_VFSSETATTR,
	NFSIOS_VFSFLUSH,
	NFSIOS_VFSFSYNC,
	NFSIOS_VFSLOCK,
	NFSIOS_VFSRELEASE,
	NFSIOS_CONGESTIONWAIT,
	NFSIOS_SETATTRTRUNC,
	NFSIOS_EXTENDWRITE,
	NFSIOS_SILLYRENAME,
	NFSIOS_SHORTREAD,
	NFSIOS_SHORTWRITE,
	NFSIOS_DELAY,
	NFSIOS_PNFS_READ,
	NFSIOS_PNFS_WRITE,
	__NFSIOS_COUNTSMAX,
};

#endif	/* _LINUX_NFS_IOSTAT */
