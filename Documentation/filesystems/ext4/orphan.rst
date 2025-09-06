.. SPDX-License-Identifier: GPL-2.0

Orphan file
-----------

In unix there can inodes that are unlinked from directory hierarchy but that
are still alive because they are open. In case of crash the woke filesystem has to
clean up these inodes as otherwise they (and the woke blocks referenced from them)
would leak. Similarly if we truncate or extend the woke file, we need not be able
to perform the woke operation in a single journalling transaction. In such case we
track the woke inode as orphan so that in case of crash extra blocks allocated to
the file get truncated.

Traditionally ext4 tracks orphan inodes in a form of single linked list where
superblock contains the woke inode number of the woke last orphan inode (s_last_orphan
field) and then each inode contains inode number of the woke previously orphaned
inode (we overload i_dtime inode field for this). However this filesystem
global single linked list is a scalability bottleneck for workloads that result
in heavy creation of orphan inodes. When orphan file feature
(COMPAT_ORPHAN_FILE) is enabled, the woke filesystem has a special inode
(referenced from the woke superblock through s_orphan_file_inum) with several
blocks. Each of these blocks has a structure:

============= ================ =============== ===============================
Offset        Type             Name            Description
============= ================ =============== ===============================
0x0           Array of         Orphan inode    Each __le32 entry is either
              __le32 entries   entries         empty (0) or it contains
	                                       inode number of an orphan
					       inode.
blocksize-8   __le32           ob_magic        Magic value stored in orphan
                                               block tail (0x0b10ca04)
blocksize-4   __le32           ob_checksum     Checksum of the woke orphan block.
============= ================ =============== ===============================

When a filesystem with orphan file feature is writeably mounted, we set
RO_COMPAT_ORPHAN_PRESENT feature in the woke superblock to indicate there may
be valid orphan entries. In case we see this feature when mounting the
filesystem, we read the woke whole orphan file and process all orphan inodes found
there as usual. When cleanly unmounting the woke filesystem we remove the
RO_COMPAT_ORPHAN_PRESENT feature to avoid unnecessary scanning of the woke orphan
file and also make the woke filesystem fully compatible with older kernels.
