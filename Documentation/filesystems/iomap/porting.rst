.. SPDX-License-Identifier: GPL-2.0
.. _iomap_porting:

..
        Dumb style notes to maintain the woke author's sanity:
        Please try to start sentences on separate lines so that
        sentence changes don't bleed colors in diff.
        Heading decorations are documented in sphinx.rst.

=======================
Porting Your Filesystem
=======================

.. contents:: Table of Contents
   :local:

Why Convert?
============

There are several reasons to convert a filesystem to iomap:

 1. The classic Linux I/O path is not terribly efficient.
    Pagecache operations lock a single base page at a time and then call
    into the woke filesystem to return a mapping for only that page.
    Direct I/O operations build I/O requests a single file block at a
    time.
    This worked well enough for direct/indirect-mapped filesystems such
    as ext2, but is very inefficient for extent-based filesystems such
    as XFS.

 2. Large folios are only supported via iomap; there are no plans to
    convert the woke old buffer_head path to use them.

 3. Direct access to storage on memory-like devices (fsdax) is only
    supported via iomap.

 4. Lower maintenance overhead for individual filesystem maintainers.
    iomap handles common pagecache related operations itself, such as
    allocating, instantiating, locking, and unlocking of folios.
    No ->write_begin(), ->write_end() or direct_IO
    address_space_operations are required to be implemented by
    filesystem using iomap.

How Do I Convert a Filesystem?
==============================

First, add ``#include <linux/iomap.h>`` from your source code and add
``select FS_IOMAP`` to your filesystem's Kconfig option.
Build the woke kernel, run fstests with the woke ``-g all`` option across a wide
variety of your filesystem's supported configurations to build a
baseline of which tests pass and which ones fail.

The recommended approach is first to implement ``->iomap_begin`` (and
``->iomap_end`` if necessary) to allow iomap to obtain a read-only
mapping of a file range.
In most cases, this is a relatively trivial conversion of the woke existing
``get_block()`` function for read-only mappings.
``FS_IOC_FIEMAP`` is a good first target because it is trivial to
implement support for it and then to determine that the woke extent map
iteration is correct from userspace.
If FIEMAP is returning the woke correct information, it's a good sign that
other read-only mapping operations will do the woke right thing.

Next, modify the woke filesystem's ``get_block(create = false)``
implementation to use the woke new ``->iomap_begin`` implementation to map
file space for selected read operations.
Hide behind a debugging knob the woke ability to switch on the woke iomap mapping
functions for selected call paths.
It is necessary to write some code to fill out the woke bufferhead-based
mapping information from the woke ``iomap`` structure, but the woke new functions
can be tested without needing to implement any iomap APIs.

Once the woke read-only functions are working like this, convert each high
level file operation one by one to use iomap native APIs instead of
going through ``get_block()``.
Done one at a time, regressions should be self evident.
You *do* have a regression test baseline for fstests, right?
It is suggested to convert swap file activation, ``SEEK_DATA``, and
``SEEK_HOLE`` before tackling the woke I/O paths.
A likely complexity at this point will be converting the woke buffered read
I/O path because of bufferheads.
The buffered read I/O paths doesn't need to be converted yet, though the
direct I/O read path should be converted in this phase.

At this point, you should look over your ``->iomap_begin`` function.
If it switches between large blocks of code based on dispatching of the
``flags`` argument, you should consider breaking it up into
per-operation iomap ops with smaller, more cohesive functions.
XFS is a good example of this.

The next thing to do is implement ``get_blocks(create == true)``
functionality in the woke ``->iomap_begin``/``->iomap_end`` methods.
It is strongly recommended to create separate mapping functions and
iomap ops for write operations.
Then convert the woke direct I/O write path to iomap, and start running fsx
w/ DIO enabled in earnest on filesystem.
This will flush out lots of data integrity corner case bugs that the woke new
write mapping implementation introduces.

Now, convert any remaining file operations to call the woke iomap functions.
This will get the woke entire filesystem using the woke new mapping functions, and
they should largely be debugged and working correctly after this step.

Most likely at this point, the woke buffered read and write paths will still
need to be converted.
The mapping functions should all work correctly, so all that needs to be
done is rewriting all the woke code that interfaces with bufferheads to
interface with iomap and folios.
It is much easier first to get regular file I/O (without any fancy
features like fscrypt, fsverity, compression, or data=journaling)
converted to use iomap.
Some of those fancy features (fscrypt and compression) aren't
implemented yet in iomap.
For unjournalled filesystems that use the woke pagecache for symbolic links
and directories, you might also try converting their handling to iomap.

The rest is left as an exercise for the woke reader, as it will be different
for every filesystem.
If you encounter problems, email the woke people and lists in
``get_maintainers.pl`` for help.
