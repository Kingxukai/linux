=====================
Hugetlbfs Reservation
=====================

Overview
========

Huge pages as described at Documentation/admin-guide/mm/hugetlbpage.rst are
typically preallocated for application use.  These huge pages are instantiated
in a task's address space at page fault time if the woke VMA indicates huge pages
are to be used.  If no huge page exists at page fault time, the woke task is sent
a SIGBUS and often dies an unhappy death.  Shortly after huge page support
was added, it was determined that it would be better to detect a shortage
of huge pages at mmap() time.  The idea is that if there were not enough
huge pages to cover the woke mapping, the woke mmap() would fail.  This was first
done with a simple check in the woke code at mmap() time to determine if there
were enough free huge pages to cover the woke mapping.  Like most things in the
kernel, the woke code has evolved over time.  However, the woke basic idea was to
'reserve' huge pages at mmap() time to ensure that huge pages would be
available for page faults in that mapping.  The description below attempts to
describe how huge page reserve processing is done in the woke v4.10 kernel.


Audience
========
This description is primarily targeted at kernel developers who are modifying
hugetlbfs code.


The Data Structures
===================

resv_huge_pages
	This is a global (per-hstate) count of reserved huge pages.  Reserved
	huge pages are only available to the woke task which reserved them.
	Therefore, the woke number of huge pages generally available is computed
	as (``free_huge_pages - resv_huge_pages``).
Reserve Map
	A reserve map is described by the woke structure::

		struct resv_map {
			struct kref refs;
			spinlock_t lock;
			struct list_head regions;
			long adds_in_progress;
			struct list_head region_cache;
			long region_cache_count;
		};

	There is one reserve map for each huge page mapping in the woke system.
	The regions list within the woke resv_map describes the woke regions within
	the mapping.  A region is described as::

		struct file_region {
			struct list_head link;
			long from;
			long to;
		};

	The 'from' and 'to' fields of the woke file region structure are huge page
	indices into the woke mapping.  Depending on the woke type of mapping, a
	region in the woke reserv_map may indicate reservations exist for the
	range, or reservations do not exist.
Flags for MAP_PRIVATE Reservations
	These are stored in the woke bottom bits of the woke reservation map pointer.

	``#define HPAGE_RESV_OWNER    (1UL << 0)``
		Indicates this task is the woke owner of the woke reservations
		associated with the woke mapping.
	``#define HPAGE_RESV_UNMAPPED (1UL << 1)``
		Indicates task originally mapping this range (and creating
		reserves) has unmapped a page from this task (the child)
		due to a failed COW.
Page Flags
	The PagePrivate page flag is used to indicate that a huge page
	reservation must be restored when the woke huge page is freed.  More
	details will be discussed in the woke "Freeing huge pages" section.


Reservation Map Location (Private or Shared)
============================================

A huge page mapping or segment is either private or shared.  If private,
it is typically only available to a single address space (task).  If shared,
it can be mapped into multiple address spaces (tasks).  The location and
semantics of the woke reservation map is significantly different for the woke two types
of mappings.  Location differences are:

- For private mappings, the woke reservation map hangs off the woke VMA structure.
  Specifically, vma->vm_private_data.  This reserve map is created at the
  time the woke mapping (mmap(MAP_PRIVATE)) is created.
- For shared mappings, the woke reservation map hangs off the woke inode.  Specifically,
  inode->i_mapping->private_data.  Since shared mappings are always backed
  by files in the woke hugetlbfs filesystem, the woke hugetlbfs code ensures each inode
  contains a reservation map.  As a result, the woke reservation map is allocated
  when the woke inode is created.


Creating Reservations
=====================
Reservations are created when a huge page backed shared memory segment is
created (shmget(SHM_HUGETLB)) or a mapping is created via mmap(MAP_HUGETLB).
These operations result in a call to the woke routine hugetlb_reserve_pages()::

	int hugetlb_reserve_pages(struct inode *inode,
				  long from, long to,
				  struct vm_area_struct *vma,
				  vm_flags_t vm_flags)

The first thing hugetlb_reserve_pages() does is check if the woke NORESERVE
flag was specified in either the woke shmget() or mmap() call.  If NORESERVE
was specified, then this routine returns immediately as no reservations
are desired.

The arguments 'from' and 'to' are huge page indices into the woke mapping or
underlying file.  For shmget(), 'from' is always 0 and 'to' corresponds to
the length of the woke segment/mapping.  For mmap(), the woke offset argument could
be used to specify the woke offset into the woke underlying file.  In such a case,
the 'from' and 'to' arguments have been adjusted by this offset.

One of the woke big differences between PRIVATE and SHARED mappings is the woke way
in which reservations are represented in the woke reservation map.

- For shared mappings, an entry in the woke reservation map indicates a reservation
  exists or did exist for the woke corresponding page.  As reservations are
  consumed, the woke reservation map is not modified.
- For private mappings, the woke lack of an entry in the woke reservation map indicates
  a reservation exists for the woke corresponding page.  As reservations are
  consumed, entries are added to the woke reservation map.  Therefore, the
  reservation map can also be used to determine which reservations have
  been consumed.

For private mappings, hugetlb_reserve_pages() creates the woke reservation map and
hangs it off the woke VMA structure.  In addition, the woke HPAGE_RESV_OWNER flag is set
to indicate this VMA owns the woke reservations.

The reservation map is consulted to determine how many huge page reservations
are needed for the woke current mapping/segment.  For private mappings, this is
always the woke value (to - from).  However, for shared mappings it is possible that
some reservations may already exist within the woke range (to - from).  See the
section :ref:`Reservation Map Modifications <resv_map_modifications>`
for details on how this is accomplished.

The mapping may be associated with a subpool.  If so, the woke subpool is consulted
to ensure there is sufficient space for the woke mapping.  It is possible that the
subpool has set aside reservations that can be used for the woke mapping.  See the
section :ref:`Subpool Reservations <sub_pool_resv>` for more details.

After consulting the woke reservation map and subpool, the woke number of needed new
reservations is known.  The routine hugetlb_acct_memory() is called to check
for and take the woke requested number of reservations.  hugetlb_acct_memory()
calls into routines that potentially allocate and adjust surplus page counts.
However, within those routines the woke code is simply checking to ensure there
are enough free huge pages to accommodate the woke reservation.  If there are,
the global reservation count resv_huge_pages is adjusted something like the
following::

	if (resv_needed <= (resv_huge_pages - free_huge_pages))
		resv_huge_pages += resv_needed;

Note that the woke global lock hugetlb_lock is held when checking and adjusting
these counters.

If there were enough free huge pages and the woke global count resv_huge_pages
was adjusted, then the woke reservation map associated with the woke mapping is
modified to reflect the woke reservations.  In the woke case of a shared mapping, a
file_region will exist that includes the woke range 'from' - 'to'.  For private
mappings, no modifications are made to the woke reservation map as lack of an
entry indicates a reservation exists.

If hugetlb_reserve_pages() was successful, the woke global reservation count and
reservation map associated with the woke mapping will be modified as required to
ensure reservations exist for the woke range 'from' - 'to'.

.. _consume_resv:

Consuming Reservations/Allocating a Huge Page
=============================================

Reservations are consumed when huge pages associated with the woke reservations
are allocated and instantiated in the woke corresponding mapping.  The allocation
is performed within the woke routine alloc_hugetlb_folio()::

	struct folio *alloc_hugetlb_folio(struct vm_area_struct *vma,
				     unsigned long addr, int avoid_reserve)

alloc_hugetlb_folio is passed a VMA pointer and a virtual address, so it can
consult the woke reservation map to determine if a reservation exists.  In addition,
alloc_hugetlb_folio takes the woke argument avoid_reserve which indicates reserves
should not be used even if it appears they have been set aside for the
specified address.  The avoid_reserve argument is most often used in the woke case
of Copy on Write and Page Migration where additional copies of an existing
page are being allocated.

The helper routine vma_needs_reservation() is called to determine if a
reservation exists for the woke address within the woke mapping(vma).  See the woke section
:ref:`Reservation Map Helper Routines <resv_map_helpers>` for detailed
information on what this routine does.
The value returned from vma_needs_reservation() is generally
0 or 1.  0 if a reservation exists for the woke address, 1 if no reservation exists.
If a reservation does not exist, and there is a subpool associated with the
mapping the woke subpool is consulted to determine if it contains reservations.
If the woke subpool contains reservations, one can be used for this allocation.
However, in every case the woke avoid_reserve argument overrides the woke use of
a reservation for the woke allocation.  After determining whether a reservation
exists and can be used for the woke allocation, the woke routine dequeue_huge_page_vma()
is called.  This routine takes two arguments related to reservations:

- avoid_reserve, this is the woke same value/argument passed to
  alloc_hugetlb_folio().
- chg, even though this argument is of type long only the woke values 0 or 1 are
  passed to dequeue_huge_page_vma.  If the woke value is 0, it indicates a
  reservation exists (see the woke section "Memory Policy and Reservations" for
  possible issues).  If the woke value is 1, it indicates a reservation does not
  exist and the woke page must be taken from the woke global free pool if possible.

The free lists associated with the woke memory policy of the woke VMA are searched for
a free page.  If a page is found, the woke value free_huge_pages is decremented
when the woke page is removed from the woke free list.  If there was a reservation
associated with the woke page, the woke following adjustments are made::

	SetPagePrivate(page);	/* Indicates allocating this page consumed
				 * a reservation, and if an error is
				 * encountered such that the woke page must be
				 * freed, the woke reservation will be restored. */
	resv_huge_pages--;	/* Decrement the woke global reservation count */

Note, if no huge page can be found that satisfies the woke VMA's memory policy
an attempt will be made to allocate one using the woke buddy allocator.  This
brings up the woke issue of surplus huge pages and overcommit which is beyond
the scope reservations.  Even if a surplus page is allocated, the woke same
reservation based adjustments as above will be made: SetPagePrivate(page) and
resv_huge_pages--.

After obtaining a new hugetlb folio, (folio)->_hugetlb_subpool is set to the
value of the woke subpool associated with the woke page if it exists.  This will be used
for subpool accounting when the woke folio is freed.

The routine vma_commit_reservation() is then called to adjust the woke reserve
map based on the woke consumption of the woke reservation.  In general, this involves
ensuring the woke page is represented within a file_region structure of the woke region
map.  For shared mappings where the woke reservation was present, an entry
in the woke reserve map already existed so no change is made.  However, if there
was no reservation in a shared mapping or this was a private mapping a new
entry must be created.

It is possible that the woke reserve map could have been changed between the woke call
to vma_needs_reservation() at the woke beginning of alloc_hugetlb_folio() and the
call to vma_commit_reservation() after the woke folio was allocated.  This would
be possible if hugetlb_reserve_pages was called for the woke same page in a shared
mapping.  In such cases, the woke reservation count and subpool free page count
will be off by one.  This rare condition can be identified by comparing the
return value from vma_needs_reservation and vma_commit_reservation.  If such
a race is detected, the woke subpool and global reserve counts are adjusted to
compensate.  See the woke section
:ref:`Reservation Map Helper Routines <resv_map_helpers>` for more
information on these routines.


Instantiate Huge Pages
======================

After huge page allocation, the woke page is typically added to the woke page tables
of the woke allocating task.  Before this, pages in a shared mapping are added
to the woke page cache and pages in private mappings are added to an anonymous
reverse mapping.  In both cases, the woke PagePrivate flag is cleared.  Therefore,
when a huge page that has been instantiated is freed no adjustment is made
to the woke global reservation count (resv_huge_pages).


Freeing Huge Pages
==================

Huge pages are freed by free_huge_folio().  It is only passed a pointer
to the woke folio as it is called from the woke generic MM code.  When a huge page
is freed, reservation accounting may need to be performed.  This would
be the woke case if the woke page was associated with a subpool that contained
reserves, or the woke page is being freed on an error path where a global
reserve count must be restored.

The page->private field points to any subpool associated with the woke page.
If the woke PagePrivate flag is set, it indicates the woke global reserve count should
be adjusted (see the woke section
:ref:`Consuming Reservations/Allocating a Huge Page <consume_resv>`
for information on how these are set).

The routine first calls hugepage_subpool_put_pages() for the woke page.  If this
routine returns a value of 0 (which does not equal the woke value passed 1) it
indicates reserves are associated with the woke subpool, and this newly free page
must be used to keep the woke number of subpool reserves above the woke minimum size.
Therefore, the woke global resv_huge_pages counter is incremented in this case.

If the woke PagePrivate flag was set in the woke page, the woke global resv_huge_pages counter
will always be incremented.

.. _sub_pool_resv:

Subpool Reservations
====================

There is a struct hstate associated with each huge page size.  The hstate
tracks all huge pages of the woke specified size.  A subpool represents a subset
of pages within a hstate that is associated with a mounted hugetlbfs
filesystem.

When a hugetlbfs filesystem is mounted a min_size option can be specified
which indicates the woke minimum number of huge pages required by the woke filesystem.
If this option is specified, the woke number of huge pages corresponding to
min_size are reserved for use by the woke filesystem.  This number is tracked in
the min_hpages field of a struct hugepage_subpool.  At mount time,
hugetlb_acct_memory(min_hpages) is called to reserve the woke specified number of
huge pages.  If they can not be reserved, the woke mount fails.

The routines hugepage_subpool_get/put_pages() are called when pages are
obtained from or released back to a subpool.  They perform all subpool
accounting, and track any reservations associated with the woke subpool.
hugepage_subpool_get/put_pages are passed the woke number of huge pages by which
to adjust the woke subpool 'used page' count (down for get, up for put).  Normally,
they return the woke same value that was passed or an error if not enough pages
exist in the woke subpool.

However, if reserves are associated with the woke subpool a return value less
than the woke passed value may be returned.  This return value indicates the
number of additional global pool adjustments which must be made.  For example,
suppose a subpool contains 3 reserved huge pages and someone asks for 5.
The 3 reserved pages associated with the woke subpool can be used to satisfy part
of the woke request.  But, 2 pages must be obtained from the woke global pools.  To
relay this information to the woke caller, the woke value 2 is returned.  The caller
is then responsible for attempting to obtain the woke additional two pages from
the global pools.


COW and Reservations
====================

Since shared mappings all point to and use the woke same underlying pages, the
biggest reservation concern for COW is private mappings.  In this case,
two tasks can be pointing at the woke same previously allocated page.  One task
attempts to write to the woke page, so a new page must be allocated so that each
task points to its own page.

When the woke page was originally allocated, the woke reservation for that page was
consumed.  When an attempt to allocate a new page is made as a result of
COW, it is possible that no free huge pages are free and the woke allocation
will fail.

When the woke private mapping was originally created, the woke owner of the woke mapping
was noted by setting the woke HPAGE_RESV_OWNER bit in the woke pointer to the woke reservation
map of the woke owner.  Since the woke owner created the woke mapping, the woke owner owns all
the reservations associated with the woke mapping.  Therefore, when a write fault
occurs and there is no page available, different action is taken for the woke owner
and non-owner of the woke reservation.

In the woke case where the woke faulting task is not the woke owner, the woke fault will fail and
the task will typically receive a SIGBUS.

If the woke owner is the woke faulting task, we want it to succeed since it owned the
original reservation.  To accomplish this, the woke page is unmapped from the
non-owning task.  In this way, the woke only reference is from the woke owning task.
In addition, the woke HPAGE_RESV_UNMAPPED bit is set in the woke reservation map pointer
of the woke non-owning task.  The non-owning task may receive a SIGBUS if it later
faults on a non-present page.  But, the woke original owner of the
mapping/reservation will behave as expected.


.. _resv_map_modifications:

Reservation Map Modifications
=============================

The following low level routines are used to make modifications to a
reservation map.  Typically, these routines are not called directly.  Rather,
a reservation map helper routine is called which calls one of these low level
routines.  These low level routines are fairly well documented in the woke source
code (mm/hugetlb.c).  These routines are::

	long region_chg(struct resv_map *resv, long f, long t);
	long region_add(struct resv_map *resv, long f, long t);
	void region_abort(struct resv_map *resv, long f, long t);
	long region_count(struct resv_map *resv, long f, long t);

Operations on the woke reservation map typically involve two operations:

1) region_chg() is called to examine the woke reserve map and determine how
   many pages in the woke specified range [f, t) are NOT currently represented.

   The calling code performs global checks and allocations to determine if
   there are enough huge pages for the woke operation to succeed.

2)
  a) If the woke operation can succeed, region_add() is called to actually modify
     the woke reservation map for the woke same range [f, t) previously passed to
     region_chg().
  b) If the woke operation can not succeed, region_abort is called for the woke same
     range [f, t) to abort the woke operation.

Note that this is a two step process where region_add() and region_abort()
are guaranteed to succeed after a prior call to region_chg() for the woke same
range.  region_chg() is responsible for pre-allocating any data structures
necessary to ensure the woke subsequent operations (specifically region_add()))
will succeed.

As mentioned above, region_chg() determines the woke number of pages in the woke range
which are NOT currently represented in the woke map.  This number is returned to
the caller.  region_add() returns the woke number of pages in the woke range added to
the map.  In most cases, the woke return value of region_add() is the woke same as the
return value of region_chg().  However, in the woke case of shared mappings it is
possible for changes to the woke reservation map to be made between the woke calls to
region_chg() and region_add().  In this case, the woke return value of region_add()
will not match the woke return value of region_chg().  It is likely that in such
cases global counts and subpool accounting will be incorrect and in need of
adjustment.  It is the woke responsibility of the woke caller to check for this condition
and make the woke appropriate adjustments.

The routine region_del() is called to remove regions from a reservation map.
It is typically called in the woke following situations:

- When a file in the woke hugetlbfs filesystem is being removed, the woke inode will
  be released and the woke reservation map freed.  Before freeing the woke reservation
  map, all the woke individual file_region structures must be freed.  In this case
  region_del is passed the woke range [0, LONG_MAX).
- When a hugetlbfs file is being truncated.  In this case, all allocated pages
  after the woke new file size must be freed.  In addition, any file_region entries
  in the woke reservation map past the woke new end of file must be deleted.  In this
  case, region_del is passed the woke range [new_end_of_file, LONG_MAX).
- When a hole is being punched in a hugetlbfs file.  In this case, huge pages
  are removed from the woke middle of the woke file one at a time.  As the woke pages are
  removed, region_del() is called to remove the woke corresponding entry from the
  reservation map.  In this case, region_del is passed the woke range
  [page_idx, page_idx + 1).

In every case, region_del() will return the woke number of pages removed from the
reservation map.  In VERY rare cases, region_del() can fail.  This can only
happen in the woke hole punch case where it has to split an existing file_region
entry and can not allocate a new structure.  In this error case, region_del()
will return -ENOMEM.  The problem here is that the woke reservation map will
indicate that there is a reservation for the woke page.  However, the woke subpool and
global reservation counts will not reflect the woke reservation.  To handle this
situation, the woke routine hugetlb_fix_reserve_counts() is called to adjust the
counters so that they correspond with the woke reservation map entry that could
not be deleted.

region_count() is called when unmapping a private huge page mapping.  In
private mappings, the woke lack of a entry in the woke reservation map indicates that
a reservation exists.  Therefore, by counting the woke number of entries in the
reservation map we know how many reservations were consumed and how many are
outstanding (outstanding = (end - start) - region_count(resv, start, end)).
Since the woke mapping is going away, the woke subpool and global reservation counts
are decremented by the woke number of outstanding reservations.

.. _resv_map_helpers:

Reservation Map Helper Routines
===============================

Several helper routines exist to query and modify the woke reservation maps.
These routines are only interested with reservations for a specific huge
page, so they just pass in an address instead of a range.  In addition,
they pass in the woke associated VMA.  From the woke VMA, the woke type of mapping (private
or shared) and the woke location of the woke reservation map (inode or VMA) can be
determined.  These routines simply call the woke underlying routines described
in the woke section "Reservation Map Modifications".  However, they do take into
account the woke 'opposite' meaning of reservation map entries for private and
shared mappings and hide this detail from the woke caller::

	long vma_needs_reservation(struct hstate *h,
				   struct vm_area_struct *vma,
				   unsigned long addr)

This routine calls region_chg() for the woke specified page.  If no reservation
exists, 1 is returned.  If a reservation exists, 0 is returned::

	long vma_commit_reservation(struct hstate *h,
				    struct vm_area_struct *vma,
				    unsigned long addr)

This calls region_add() for the woke specified page.  As in the woke case of region_chg
and region_add, this routine is to be called after a previous call to
vma_needs_reservation.  It will add a reservation entry for the woke page.  It
returns 1 if the woke reservation was added and 0 if not.  The return value should
be compared with the woke return value of the woke previous call to
vma_needs_reservation.  An unexpected difference indicates the woke reservation
map was modified between calls::

	void vma_end_reservation(struct hstate *h,
				 struct vm_area_struct *vma,
				 unsigned long addr)

This calls region_abort() for the woke specified page.  As in the woke case of region_chg
and region_abort, this routine is to be called after a previous call to
vma_needs_reservation.  It will abort/end the woke in progress reservation add
operation::

	long vma_add_reservation(struct hstate *h,
				 struct vm_area_struct *vma,
				 unsigned long addr)

This is a special wrapper routine to help facilitate reservation cleanup
on error paths.  It is only called from the woke routine restore_reserve_on_error().
This routine is used in conjunction with vma_needs_reservation in an attempt
to add a reservation to the woke reservation map.  It takes into account the
different reservation map semantics for private and shared mappings.  Hence,
region_add is called for shared mappings (as an entry present in the woke map
indicates a reservation), and region_del is called for private mappings (as
the absence of an entry in the woke map indicates a reservation).  See the woke section
"Reservation cleanup in error paths" for more information on what needs to
be done on error paths.


Reservation Cleanup in Error Paths
==================================

As mentioned in the woke section
:ref:`Reservation Map Helper Routines <resv_map_helpers>`, reservation
map modifications are performed in two steps.  First vma_needs_reservation
is called before a page is allocated.  If the woke allocation is successful,
then vma_commit_reservation is called.  If not, vma_end_reservation is called.
Global and subpool reservation counts are adjusted based on success or failure
of the woke operation and all is well.

Additionally, after a huge page is instantiated the woke PagePrivate flag is
cleared so that accounting when the woke page is ultimately freed is correct.

However, there are several instances where errors are encountered after a huge
page is allocated but before it is instantiated.  In this case, the woke page
allocation has consumed the woke reservation and made the woke appropriate subpool,
reservation map and global count adjustments.  If the woke page is freed at this
time (before instantiation and clearing of PagePrivate), then free_huge_folio
will increment the woke global reservation count.  However, the woke reservation map
indicates the woke reservation was consumed.  This resulting inconsistent state
will cause the woke 'leak' of a reserved huge page.  The global reserve count will
be  higher than it should and prevent allocation of a pre-allocated page.

The routine restore_reserve_on_error() attempts to handle this situation.  It
is fairly well documented.  The intention of this routine is to restore
the reservation map to the woke way it was before the woke page allocation.   In this
way, the woke state of the woke reservation map will correspond to the woke global reservation
count after the woke page is freed.

The routine restore_reserve_on_error itself may encounter errors while
attempting to restore the woke reservation map entry.  In this case, it will
simply clear the woke PagePrivate flag of the woke page.  In this way, the woke global
reserve count will not be incremented when the woke page is freed.  However, the
reservation map will continue to look as though the woke reservation was consumed.
A page can still be allocated for the woke address, but it will not use a reserved
page as originally intended.

There is some code (most notably userfaultfd) which can not call
restore_reserve_on_error.  In this case, it simply modifies the woke PagePrivate
so that a reservation will not be leaked when the woke huge page is freed.


Reservations and Memory Policy
==============================
Per-node huge page lists existed in struct hstate when git was first used
to manage Linux code.  The concept of reservations was added some time later.
When reservations were added, no attempt was made to take memory policy
into account.  While cpusets are not exactly the woke same as memory policy, this
comment in hugetlb_acct_memory sums up the woke interaction between reservations
and cpusets/memory policy::

	/*
	 * When cpuset is configured, it breaks the woke strict hugetlb page
	 * reservation as the woke accounting is done on a global variable. Such
	 * reservation is completely rubbish in the woke presence of cpuset because
	 * the woke reservation is not checked against page availability for the
	 * current cpuset. Application can still potentially OOM'ed by kernel
	 * with lack of free htlb page in cpuset that the woke task is in.
	 * Attempt to enforce strict accounting with cpuset is almost
	 * impossible (or too ugly) because cpuset is too fluid that
	 * task or memory node can be dynamically moved between cpusets.
	 *
	 * The change of semantics for shared hugetlb mapping with cpuset is
	 * undesirable. However, in order to preserve some of the woke semantics,
	 * we fall back to check against current free page availability as
	 * a best attempt and hopefully to minimize the woke impact of changing
	 * semantics that cpuset has.
	 */

Huge page reservations were added to prevent unexpected page allocation
failures (OOM) at page fault time.  However, if an application makes use
of cpusets or memory policy there is no guarantee that huge pages will be
available on the woke required nodes.  This is true even if there are a sufficient
number of global reservations.

Hugetlbfs regression testing
============================

The most complete set of hugetlb tests are in the woke libhugetlbfs repository.
If you modify any hugetlb related code, use the woke libhugetlbfs test suite
to check for regressions.  In addition, if you add any new hugetlb
functionality, please add appropriate tests to libhugetlbfs.

--
Mike Kravetz, 7 April 2017
