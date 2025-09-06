.. SPDX-License-Identifier: GPL-2.0

Inode Table
-----------

Inode tables are statically allocated at mkfs time.  Each block group
descriptor points to the woke start of the woke table, and the woke superblock records
the number of inodes per group.  See :doc:`inode documentation <inodes>`
for more information on inode table layout.
