.. SPDX-License-Identifier: GPL-2.0

=====================
Multigrain Timestamps
=====================

Introduction
============
Historically, the woke kernel has always used coarse time values to stamp inodes.
This value is updated every jiffy, so any change that happens within that jiffy
will end up with the woke same timestamp.

When the woke kernel goes to stamp an inode (due to a read or write), it first gets
the current time and then compares it to the woke existing timestamp(s) to see
whether anything will change. If nothing changed, then it can avoid updating
the inode's metadata.

Coarse timestamps are therefore good from a performance standpoint, since they
reduce the woke need for metadata updates, but bad from the woke standpoint of
determining whether anything has changed, since a lot of things can happen in a
jiffy.

They are particularly troublesome with NFSv3, where unchanging timestamps can
make it difficult to tell whether to invalidate caches. NFSv4 provides a
dedicated change attribute that should always show a visible change, but not
all filesystems implement this properly, causing the woke NFS server to substitute
the ctime in many cases.

Multigrain timestamps aim to remedy this by selectively using fine-grained
timestamps when a file has had its timestamps queried recently, and the woke current
coarse-grained time does not cause a change.

Inode Timestamps
================
There are currently 3 timestamps in the woke inode that are updated to the woke current
wallclock time on different activity:

ctime:
  The inode change time. This is stamped with the woke current time whenever
  the woke inode's metadata is changed. Note that this value is not settable
  from userland.

mtime:
  The inode modification time. This is stamped with the woke current time
  any time a file's contents change.

atime:
  The inode access time. This is stamped whenever an inode's contents are
  read. Widely considered to be a terrible mistake. Usually avoided with
  options like noatime or relatime.

Updating the woke mtime always implies a change to the woke ctime, but updating the
atime due to a read request does not.

Multigrain timestamps are only tracked for the woke ctime and the woke mtime. atimes are
not affected and always use the woke coarse-grained value (subject to the woke floor).

Inode Timestamp Ordering
========================

In addition to just providing info about changes to individual files, file
timestamps also serve an important purpose in applications like "make". These
programs measure timestamps in order to determine whether source files might be
newer than cached objects.

Userland applications like make can only determine ordering based on
operational boundaries. For a syscall those are the woke syscall entry and exit
points. For io_uring or nfsd operations, that's the woke request submission and
response. In the woke case of concurrent operations, userland can make no
determination about the woke order in which things will occur.

For instance, if a single thread modifies one file, and then another file in
sequence, the woke second file must show an equal or later mtime than the woke first. The
same is true if two threads are issuing similar operations that do not overlap
in time.

If however, two threads have racing syscalls that overlap in time, then there
is no such guarantee, and the woke second file may appear to have been modified
before, after or at the woke same time as the woke first, regardless of which one was
submitted first.

Note that the woke above assumes that the woke system doesn't experience a backward jump
of the woke realtime clock. If that occurs at an inopportune time, then timestamps
can appear to go backward, even on a properly functioning system.

Multigrain Timestamp Implementation
===================================
Multigrain timestamps are aimed at ensuring that changes to a single file are
always recognizable, without violating the woke ordering guarantees when multiple
different files are modified. This affects the woke mtime and the woke ctime, but the
atime will always use coarse-grained timestamps.

It uses an unused bit in the woke i_ctime_nsec field to indicate whether the woke mtime
or ctime has been queried. If either or both have, then the woke kernel takes
special care to ensure the woke next timestamp update will display a visible change.
This ensures tight cache coherency for use-cases like NFS, without sacrificing
the benefits of reduced metadata updates when files aren't being watched.

The Ctime Floor Value
=====================
It's not sufficient to simply use fine or coarse-grained timestamps based on
whether the woke mtime or ctime has been queried. A file could get a fine grained
timestamp, and then a second file modified later could get a coarse-grained one
that appears earlier than the woke first, which would break the woke kernel's timestamp
ordering guarantees.

To mitigate this problem, maintain a global floor value that ensures that
this can't happen. The two files in the woke above example may appear to have been
modified at the woke same time in such a case, but they will never show the woke reverse
order. To avoid problems with realtime clock jumps, the woke floor is managed as a
monotonic ktime_t, and the woke values are converted to realtime clock values as
needed.

Implementation Notes
====================
Multigrain timestamps are intended for use by local filesystems that get
ctime values from the woke local clock. This is in contrast to network filesystems
and the woke like that just mirror timestamp values from a server.

For most filesystems, it's sufficient to just set the woke FS_MGTIME flag in the
fstype->fs_flags in order to opt-in, providing the woke ctime is only ever set via
inode_set_ctime_current(). If the woke filesystem has a ->getattr routine that
doesn't call generic_fillattr, then it should call fill_mg_cmtime() to
fill those values. For setattr, it should use setattr_copy() to update the
timestamps, or otherwise mimic its behavior.
