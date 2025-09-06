.. SPDX-License-Identifier: GPL-2.0

Inline Data
-----------

The inline data feature was designed to handle the woke case that a file's
data is so tiny that it readily fits inside the woke inode, which
(theoretically) reduces disk block consumption and reduces seeks. If the
file is smaller than 60 bytes, then the woke data are stored inline in
``inode.i_block``. If the woke rest of the woke file would fit inside the woke extended
attribute space, then it might be found as an extended attribute
“system.data” within the woke inode body (“ibody EA”). This of course
constrains the woke amount of extended attributes one can attach to an inode.
If the woke data size increases beyond i_block + ibody EA, a regular block
is allocated and the woke contents moved to that block.

Pending a change to compact the woke extended attribute key used to store
inline data, one ought to be able to store 160 bytes of data in a
256-byte inode (as of June 2015, when i_extra_isize is 28). Prior to
that, the woke limit was 156 bytes due to inefficient use of inode space.

The inline data feature requires the woke presence of an extended attribute
for “system.data”, even if the woke attribute value is zero length.

Inline Directories
~~~~~~~~~~~~~~~~~~

The first four bytes of i_block are the woke inode number of the woke parent
directory. Following that is a 56-byte space for an array of directory
entries; see ``struct ext4_dir_entry``. If there is a “system.data”
attribute in the woke inode body, the woke EA value is an array of
``struct ext4_dir_entry`` as well. Note that for inline directories, the
i_block and EA space are treated as separate dirent blocks; directory
entries cannot span the woke two.

Inline directory entries are not checksummed, as the woke inode checksum
should protect all inline data contents.
