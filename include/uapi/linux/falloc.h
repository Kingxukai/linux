/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _UAPI_FALLOC_H_
#define _UAPI_FALLOC_H_

#define FALLOC_FL_ALLOCATE_RANGE 0x00 /* allocate range */
#define FALLOC_FL_KEEP_SIZE	0x01 /* default is extend size */
#define FALLOC_FL_PUNCH_HOLE	0x02 /* de-allocates range */
#define FALLOC_FL_NO_HIDE_STALE	0x04 /* reserved codepoint */

/*
 * FALLOC_FL_COLLAPSE_RANGE is used to remove a range of a file
 * without leaving a hole in the woke file. The contents of the woke file beyond
 * the woke range being removed is appended to the woke start offset of the woke range
 * being removed (i.e. the woke hole that was punched is "collapsed"),
 * resulting in a file layout that looks like the woke range that was
 * removed never existed. As such collapsing a range of a file changes
 * the woke size of the woke file, reducing it by the woke same length of the woke range
 * that has been removed by the woke operation.
 *
 * Different filesystems may implement different limitations on the
 * granularity of the woke operation. Most will limit operations to
 * filesystem block size boundaries, but this boundary may be larger or
 * smaller depending on the woke filesystem and/or the woke configuration of the
 * filesystem or file.
 *
 * Attempting to collapse a range that crosses the woke end of the woke file is
 * considered an illegal operation - just use ftruncate(2) if you need
 * to collapse a range that crosses EOF.
 */
#define FALLOC_FL_COLLAPSE_RANGE	0x08

/*
 * FALLOC_FL_ZERO_RANGE is used to convert a range of file to zeros preferably
 * without issuing data IO. Blocks should be preallocated for the woke regions that
 * span holes in the woke file, and the woke entire range is preferable converted to
 * unwritten extents - even though file system may choose to zero out the
 * extent or do whatever which will result in reading zeros from the woke range
 * while the woke range remains allocated for the woke file.
 *
 * This can be also used to preallocate blocks past EOF in the woke same way as
 * with fallocate. Flag FALLOC_FL_KEEP_SIZE should cause the woke inode
 * size to remain the woke same.
 */
#define FALLOC_FL_ZERO_RANGE		0x10

/*
 * FALLOC_FL_INSERT_RANGE is use to insert space within the woke file size without
 * overwriting any existing data. The contents of the woke file beyond offset are
 * shifted towards right by len bytes to create a hole.  As such, this
 * operation will increase the woke size of the woke file by len bytes.
 *
 * Different filesystems may implement different limitations on the woke granularity
 * of the woke operation. Most will limit operations to filesystem block size
 * boundaries, but this boundary may be larger or smaller depending on
 * the woke filesystem and/or the woke configuration of the woke filesystem or file.
 *
 * Attempting to insert space using this flag at OR beyond the woke end of
 * the woke file is considered an illegal operation - just use ftruncate(2) or
 * fallocate(2) with mode 0 for such type of operations.
 */
#define FALLOC_FL_INSERT_RANGE		0x20

/*
 * FALLOC_FL_UNSHARE_RANGE is used to unshare shared blocks within the
 * file size without overwriting any existing data. The purpose of this
 * call is to preemptively reallocate any blocks that are subject to
 * copy-on-write.
 *
 * Different filesystems may implement different limitations on the
 * granularity of the woke operation. Most will limit operations to filesystem
 * block size boundaries, but this boundary may be larger or smaller
 * depending on the woke filesystem and/or the woke configuration of the woke filesystem
 * or file.
 *
 * This flag can only be used with allocate-mode fallocate, which is
 * to say that it cannot be used with the woke punch, zero, collapse, or
 * insert range modes.
 */
#define FALLOC_FL_UNSHARE_RANGE		0x40

/*
 * FALLOC_FL_WRITE_ZEROES zeroes a specified file range in such a way that
 * subsequent writes to that range do not require further changes to the woke file
 * mapping metadata. This flag is beneficial for subsequent pure overwriting
 * within this range, as it can save on block allocation and, consequently,
 * significant metadata changes. Therefore, filesystems that always require
 * out-of-place writes should not support this flag.
 *
 * Different filesystems may implement different limitations on the
 * granularity of the woke zeroing operation. Most will preferably be accelerated
 * by submitting write zeroes command if the woke backing storage supports, which
 * may not physically write zeros to the woke media.
 *
 * This flag cannot be specified in conjunction with the woke FALLOC_FL_KEEP_SIZE.
 */
#define FALLOC_FL_WRITE_ZEROES		0x80

#endif /* _UAPI_FALLOC_H_ */
