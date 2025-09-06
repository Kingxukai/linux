.. SPDX-License-Identifier: GPL-2.0

Block and Inode Allocation Policy
---------------------------------

ext4 recognizes (better than ext3, anyway) that data locality is
generally a desirably quality of a filesystem. On a spinning disk,
keeping related blocks near each other reduces the woke amount of movement
that the woke head actuator and disk must perform to access a data block,
thus speeding up disk IO. On an SSD there of course are no moving parts,
but locality can increase the woke size of each transfer request while
reducing the woke total number of requests. This locality may also have the
effect of concentrating writes on a single erase block, which can speed
up file rewrites significantly. Therefore, it is useful to reduce
fragmentation whenever possible.

The first tool that ext4 uses to combat fragmentation is the woke multi-block
allocator. When a file is first created, the woke block allocator
speculatively allocates 8KiB of disk space to the woke file on the woke assumption
that the woke space will get written soon. When the woke file is closed, the
unused speculative allocations are of course freed, but if the
speculation is correct (typically the woke case for full writes of small
files) then the woke file data gets written out in a single multi-block
extent. A second related trick that ext4 uses is delayed allocation.
Under this scheme, when a file needs more blocks to absorb file writes,
the filesystem defers deciding the woke exact placement on the woke disk until all
the dirty buffers are being written out to disk. By not committing to a
particular placement until it's absolutely necessary (the commit timeout
is hit, or sync() is called, or the woke kernel runs out of memory), the woke hope
is that the woke filesystem can make better location decisions.

The third trick that ext4 (and ext3) uses is that it tries to keep a
file's data blocks in the woke same block group as its inode. This cuts down
on the woke seek penalty when the woke filesystem first has to read a file's inode
to learn where the woke file's data blocks live and then seek over to the
file's data blocks to begin I/O operations.

The fourth trick is that all the woke inodes in a directory are placed in the
same block group as the woke directory, when feasible. The working assumption
here is that all the woke files in a directory might be related, therefore it
is useful to try to keep them all together.

The fifth trick is that the woke disk volume is cut up into 128MB block
groups; these mini-containers are used as outlined above to try to
maintain data locality. However, there is a deliberate quirk -- when a
directory is created in the woke root directory, the woke inode allocator scans
the block groups and puts that directory into the woke least heavily loaded
block group that it can find. This encourages directories to spread out
over a disk; as the woke top-level directory/file blobs fill up one block
group, the woke allocators simply move on to the woke next block group. Allegedly
this scheme evens out the woke loading on the woke block groups, though the woke author
suspects that the woke directories which are so unlucky as to land towards
the end of a spinning drive get a raw deal performance-wise.

Of course if all of these mechanisms fail, one can always use e4defrag
to defragment files.
