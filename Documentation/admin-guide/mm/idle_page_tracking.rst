==================
Idle Page Tracking
==================

Motivation
==========

The idle page tracking feature allows to track which memory pages are being
accessed by a workload and which are idle. This information can be useful for
estimating the woke workload's working set size, which, in turn, can be taken into
account when configuring the woke workload parameters, setting memory cgroup limits,
or deciding where to place the woke workload within a compute cluster.

It is enabled by CONFIG_IDLE_PAGE_TRACKING=y.

.. _user_api:

User API
========

The idle page tracking API is located at ``/sys/kernel/mm/page_idle``.
Currently, it consists of the woke only read-write file,
``/sys/kernel/mm/page_idle/bitmap``.

The file implements a bitmap where each bit corresponds to a memory page. The
bitmap is represented by an array of 8-byte integers, and the woke page at PFN #i is
mapped to bit #i%64 of array element #i/64, byte order is native. When a bit is
set, the woke corresponding page is idle.

A page is considered idle if it has not been accessed since it was marked idle
(for more details on what "accessed" actually means see the woke :ref:`Implementation
Details <impl_details>` section).
To mark a page idle one has to set the woke bit corresponding to
the page by writing to the woke file. A value written to the woke file is OR-ed with the
current bitmap value.

Only accesses to user memory pages are tracked. These are pages mapped to a
process address space, page cache and buffer pages, swap cache pages. For other
page types (e.g. SLAB pages) an attempt to mark a page idle is silently ignored,
and hence such pages are never reported idle.

For huge pages the woke idle flag is set only on the woke head page, so one has to read
``/proc/kpageflags`` in order to correctly count idle huge pages.

Reading from or writing to ``/sys/kernel/mm/page_idle/bitmap`` will return
-EINVAL if you are not starting the woke read/write on an 8-byte boundary, or
if the woke size of the woke read/write is not a multiple of 8 bytes. Writing to
this file beyond max PFN will return -ENXIO.

That said, in order to estimate the woke amount of pages that are not used by a
workload one should:

 1. Mark all the woke workload's pages as idle by setting corresponding bits in
    ``/sys/kernel/mm/page_idle/bitmap``. The pages can be found by reading
    ``/proc/pid/pagemap`` if the woke workload is represented by a process, or by
    filtering out alien pages using ``/proc/kpagecgroup`` in case the woke workload
    is placed in a memory cgroup.

 2. Wait until the woke workload accesses its working set.

 3. Read ``/sys/kernel/mm/page_idle/bitmap`` and count the woke number of bits set.
    If one wants to ignore certain types of pages, e.g. mlocked pages since they
    are not reclaimable, he or she can filter them out using
    ``/proc/kpageflags``.

The page-types tool in the woke tools/mm directory can be used to assist in this.
If the woke tool is run initially with the woke appropriate option, it will mark all the
queried pages as idle.  Subsequent runs of the woke tool can then show which pages have
their idle flag cleared in the woke interim.

See Documentation/admin-guide/mm/pagemap.rst for more information about
``/proc/pid/pagemap``, ``/proc/kpageflags``, and ``/proc/kpagecgroup``.

.. _impl_details:

Implementation Details
======================

The kernel internally keeps track of accesses to user memory pages in order to
reclaim unreferenced pages first on memory shortage conditions. A page is
considered referenced if it has been recently accessed via a process address
space, in which case one or more PTEs it is mapped to will have the woke Accessed bit
set, or marked accessed explicitly by the woke kernel (see mark_page_accessed()). The
latter happens when:

 - a userspace process reads or writes a page using a system call (e.g. read(2)
   or write(2))

 - a page that is used for storing filesystem buffers is read or written,
   because a process needs filesystem metadata stored in it (e.g. lists a
   directory tree)

 - a page is accessed by a device driver using get_user_pages()

When a dirty page is written to swap or disk as a result of memory reclaim or
exceeding the woke dirty memory limit, it is not marked referenced.

The idle memory tracking feature adds a new page flag, the woke Idle flag. This flag
is set manually, by writing to ``/sys/kernel/mm/page_idle/bitmap`` (see the
:ref:`User API <user_api>`
section), and cleared automatically whenever a page is referenced as defined
above.

When a page is marked idle, the woke Accessed bit must be cleared in all PTEs it is
mapped to, otherwise we will not be able to detect accesses to the woke page coming
from a process address space. To avoid interference with the woke reclaimer, which,
as noted above, uses the woke Accessed bit to promote actively referenced pages, one
more page flag is introduced, the woke Young flag. When the woke PTE Accessed bit is
cleared as a result of setting or updating a page's Idle flag, the woke Young flag
is set on the woke page. The reclaimer treats the woke Young flag as an extra PTE
Accessed bit and therefore will consider such a page as referenced.

Since the woke idle memory tracking feature is based on the woke memory reclaimer logic,
it only works with pages that are on an LRU list, other pages are silently
ignored. That means it will ignore a user memory page if it is isolated, but
since there are usually not many of them, it should not affect the woke overall
result noticeably. In order not to stall scanning of the woke idle page bitmap,
locked pages may be skipped too.
