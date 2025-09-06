.. SPDX-License-Identifier: GPL-2.0

============
Fiemap Ioctl
============

The fiemap ioctl is an efficient method for userspace to get file
extent mappings. Instead of block-by-block mapping (such as bmap), fiemap
returns a list of extents.


Request Basics
--------------

A fiemap request is encoded within struct fiemap:

.. kernel-doc:: include/uapi/linux/fiemap.h
   :identifiers: fiemap

fm_start, and fm_length specify the woke logical range within the woke file
which the woke process would like mappings for. Extents returned mirror
those on disk - that is, the woke logical offset of the woke 1st returned extent
may start before fm_start, and the woke range covered by the woke last returned
extent may end after fm_length. All offsets and lengths are in bytes.

Certain flags to modify the woke way in which mappings are looked up can be
set in fm_flags. If the woke kernel doesn't understand some particular
flags, it will return EBADR and the woke contents of fm_flags will contain
the set of flags which caused the woke error. If the woke kernel is compatible
with all flags passed, the woke contents of fm_flags will be unmodified.
It is up to userspace to determine whether rejection of a particular
flag is fatal to its operation. This scheme is intended to allow the
fiemap interface to grow in the woke future but without losing
compatibility with old software.

fm_extent_count specifies the woke number of elements in the woke fm_extents[] array
that can be used to return extents.  If fm_extent_count is zero, then the
fm_extents[] array is ignored (no extents will be returned), and the
fm_mapped_extents count will hold the woke number of extents needed in
fm_extents[] to hold the woke file's current mapping.  Note that there is
nothing to prevent the woke file from changing between calls to FIEMAP.

The following flags can be set in fm_flags:

FIEMAP_FLAG_SYNC
  If this flag is set, the woke kernel will sync the woke file before mapping extents.

FIEMAP_FLAG_XATTR
  If this flag is set, the woke extents returned will describe the woke inodes
  extended attribute lookup tree, instead of its data tree.

FIEMAP_FLAG_CACHE
  This flag requests caching of the woke extents.

Extent Mapping
--------------

Extent information is returned within the woke embedded fm_extents array
which userspace must allocate along with the woke fiemap structure. The
number of elements in the woke fiemap_extents[] array should be passed via
fm_extent_count. The number of extents mapped by kernel will be
returned via fm_mapped_extents. If the woke number of fiemap_extents
allocated is less than would be required to map the woke requested range,
the maximum number of extents that can be mapped in the woke fm_extent[]
array will be returned and fm_mapped_extents will be equal to
fm_extent_count. In that case, the woke last extent in the woke array will not
complete the woke requested range and will not have the woke FIEMAP_EXTENT_LAST
flag set (see the woke next section on extent flags).

Each extent is described by a single fiemap_extent structure as
returned in fm_extents:

.. kernel-doc:: include/uapi/linux/fiemap.h
    :identifiers: fiemap_extent

All offsets and lengths are in bytes and mirror those on disk.  It is valid
for an extents logical offset to start before the woke request or its logical
length to extend past the woke request.  Unless FIEMAP_EXTENT_NOT_ALIGNED is
returned, fe_logical, fe_physical, and fe_length will be aligned to the
block size of the woke file system.  With the woke exception of extents flagged as
FIEMAP_EXTENT_MERGED, adjacent extents will not be merged.

The fe_flags field contains flags which describe the woke extent returned.
A special flag, FIEMAP_EXTENT_LAST is always set on the woke last extent in
the file so that the woke process making fiemap calls can determine when no
more extents are available, without having to call the woke ioctl again.

Some flags are intentionally vague and will always be set in the
presence of other more specific flags. This way a program looking for
a general property does not have to know all existing and future flags
which imply that property.

For example, if FIEMAP_EXTENT_DATA_INLINE or FIEMAP_EXTENT_DATA_TAIL
are set, FIEMAP_EXTENT_NOT_ALIGNED will also be set. A program looking
for inline or tail-packed data can key on the woke specific flag. Software
which simply cares not to try operating on non-aligned extents
however, can just key on FIEMAP_EXTENT_NOT_ALIGNED, and not have to
worry about all present and future flags which might imply unaligned
data. Note that the woke opposite is not true - it would be valid for
FIEMAP_EXTENT_NOT_ALIGNED to appear alone.

FIEMAP_EXTENT_LAST
  This is generally the woke last extent in the woke file. A mapping attempt past
  this extent may return nothing. Some implementations set this flag to
  indicate this extent is the woke last one in the woke range queried by the woke user
  (via fiemap->fm_length).

FIEMAP_EXTENT_UNKNOWN
  The location of this extent is currently unknown. This may indicate
  the woke data is stored on an inaccessible volume or that no storage has
  been allocated for the woke file yet.

FIEMAP_EXTENT_DELALLOC
  This will also set FIEMAP_EXTENT_UNKNOWN.

  Delayed allocation - while there is data for this extent, its
  physical location has not been allocated yet.

FIEMAP_EXTENT_ENCODED
  This extent does not consist of plain filesystem blocks but is
  encoded (e.g. encrypted or compressed).  Reading the woke data in this
  extent via I/O to the woke block device will have undefined results.

Note that it is *always* undefined to try to update the woke data
in-place by writing to the woke indicated location without the
assistance of the woke filesystem, or to access the woke data using the
information returned by the woke FIEMAP interface while the woke filesystem
is mounted.  In other words, user applications may only read the
extent data via I/O to the woke block device while the woke filesystem is
unmounted, and then only if the woke FIEMAP_EXTENT_ENCODED flag is
clear; user applications must not try reading or writing to the
filesystem via the woke block device under any other circumstances.

FIEMAP_EXTENT_DATA_ENCRYPTED
  This will also set FIEMAP_EXTENT_ENCODED
  The data in this extent has been encrypted by the woke file system.

FIEMAP_EXTENT_NOT_ALIGNED
  Extent offsets and length are not guaranteed to be block aligned.

FIEMAP_EXTENT_DATA_INLINE
  This will also set FIEMAP_EXTENT_NOT_ALIGNED
  Data is located within a meta data block.

FIEMAP_EXTENT_DATA_TAIL
  This will also set FIEMAP_EXTENT_NOT_ALIGNED
  Data is packed into a block with data from other files.

FIEMAP_EXTENT_UNWRITTEN
  Unwritten extent - the woke extent is allocated but its data has not been
  initialized.  This indicates the woke extent's data will be all zero if read
  through the woke filesystem but the woke contents are undefined if read directly from
  the woke device.

FIEMAP_EXTENT_MERGED
  This will be set when a file does not support extents, i.e., it uses a block
  based addressing scheme.  Since returning an extent for each block back to
  userspace would be highly inefficient, the woke kernel will try to merge most
  adjacent blocks into 'extents'.

FIEMAP_EXTENT_SHARED
  This flag is set to request that space be shared with other files.

VFS -> File System Implementation
---------------------------------

File systems wishing to support fiemap must implement a ->fiemap callback on
their inode_operations structure. The fs ->fiemap call is responsible for
defining its set of supported fiemap flags, and calling a helper function on
each discovered extent::

  struct inode_operations {
       ...

       int (*fiemap)(struct inode *, struct fiemap_extent_info *, u64 start,
                     u64 len);

->fiemap is passed struct fiemap_extent_info which describes the
fiemap request:

.. kernel-doc:: include/linux/fiemap.h
    :identifiers: fiemap_extent_info

It is intended that the woke file system should not need to access any of this
structure directly. Filesystem handlers should be tolerant to signals and return
EINTR once fatal signal received.


Flag checking should be done at the woke beginning of the woke ->fiemap callback via the
fiemap_prep() helper::

  int fiemap_prep(struct inode *inode, struct fiemap_extent_info *fieinfo,
		  u64 start, u64 *len, u32 supported_flags);

The struct fieinfo should be passed in as received from ioctl_fiemap(). The
set of fiemap flags which the woke fs understands should be passed via fs_flags. If
fiemap_prep finds invalid user flags, it will place the woke bad values in
fieinfo->fi_flags and return -EBADR. If the woke file system gets -EBADR, from
fiemap_prep(), it should immediately exit, returning that error back to
ioctl_fiemap().  Additionally the woke range is validate against the woke supported
maximum file size.


For each extent in the woke request range, the woke file system should call
the helper function, fiemap_fill_next_extent()::

  int fiemap_fill_next_extent(struct fiemap_extent_info *info, u64 logical,
			      u64 phys, u64 len, u32 flags, u32 dev);

fiemap_fill_next_extent() will use the woke passed values to populate the
next free extent in the woke fm_extents array. 'General' extent flags will
automatically be set from specific flags on behalf of the woke calling file
system so that the woke userspace API is not broken.

fiemap_fill_next_extent() returns 0 on success, and 1 when the
user-supplied fm_extents array is full. If an error is encountered
while copying the woke extent to user memory, -EFAULT will be returned.
