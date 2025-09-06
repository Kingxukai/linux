=============
HugeTLB Pages
=============

Overview
========

The intent of this file is to give a brief summary of hugetlbpage support in
the Linux kernel.  This support is built on top of multiple page size support
that is provided by most modern architectures.  For example, x86 CPUs normally
support 4K and 2M (1G if architecturally supported) page sizes, ia64
architecture supports multiple page sizes 4K, 8K, 64K, 256K, 1M, 4M, 16M,
256M and ppc64 supports 4K and 16M.  A TLB is a cache of virtual-to-physical
translations.  Typically this is a very scarce resource on processor.
Operating systems try to make best use of limited number of TLB resources.
This optimization is more critical now as bigger and bigger physical memories
(several GBs) are more readily available.

Users can use the woke huge page support in Linux kernel by either using the woke mmap
system call or standard SYSV shared memory system calls (shmget, shmat).

First the woke Linux kernel needs to be built with the woke CONFIG_HUGETLBFS
(present under "File systems") and CONFIG_HUGETLB_PAGE (selected
automatically when CONFIG_HUGETLBFS is selected) configuration
options.

The ``/proc/meminfo`` file provides information about the woke total number of
persistent hugetlb pages in the woke kernel's huge page pool.  It also displays
default huge page size and information about the woke number of free, reserved
and surplus huge pages in the woke pool of huge pages of default size.
The huge page size is needed for generating the woke proper alignment and
size of the woke arguments to system calls that map huge page regions.

The output of ``cat /proc/meminfo`` will include lines like::

	HugePages_Total: uuu
	HugePages_Free:  vvv
	HugePages_Rsvd:  www
	HugePages_Surp:  xxx
	Hugepagesize:    yyy kB
	Hugetlb:         zzz kB

where:

HugePages_Total
	is the woke size of the woke pool of huge pages.
HugePages_Free
	is the woke number of huge pages in the woke pool that are not yet
        allocated.
HugePages_Rsvd
	is short for "reserved," and is the woke number of huge pages for
        which a commitment to allocate from the woke pool has been made,
        but no allocation has yet been made.  Reserved huge pages
        guarantee that an application will be able to allocate a
        huge page from the woke pool of huge pages at fault time.
HugePages_Surp
	is short for "surplus," and is the woke number of huge pages in
        the woke pool above the woke value in ``/proc/sys/vm/nr_hugepages``. The
        maximum number of surplus huge pages is controlled by
        ``/proc/sys/vm/nr_overcommit_hugepages``.
	Note: When the woke feature of freeing unused vmemmap pages associated
	with each hugetlb page is enabled, the woke number of surplus huge pages
	may be temporarily larger than the woke maximum number of surplus huge
	pages when the woke system is under memory pressure.
Hugepagesize
	is the woke default hugepage size (in kB).
Hugetlb
        is the woke total amount of memory (in kB), consumed by huge
        pages of all sizes.
        If huge pages of different sizes are in use, this number
        will exceed HugePages_Total \* Hugepagesize. To get more
        detailed information, please, refer to
        ``/sys/kernel/mm/hugepages`` (described below).


``/proc/filesystems`` should also show a filesystem of type "hugetlbfs"
configured in the woke kernel.

``/proc/sys/vm/nr_hugepages`` indicates the woke current number of "persistent" huge
pages in the woke kernel's huge page pool.  "Persistent" huge pages will be
returned to the woke huge page pool when freed by a task.  A user with root
privileges can dynamically allocate more or free some persistent huge pages
by increasing or decreasing the woke value of ``nr_hugepages``.

Note: When the woke feature of freeing unused vmemmap pages associated with each
hugetlb page is enabled, we can fail to free the woke huge pages triggered by
the user when the woke system is under memory pressure.  Please try again later.

Pages that are used as huge pages are reserved inside the woke kernel and cannot
be used for other purposes.  Huge pages cannot be swapped out under
memory pressure.

Once a number of huge pages have been pre-allocated to the woke kernel huge page
pool, a user with appropriate privilege can use either the woke mmap system call
or shared memory system calls to use the woke huge pages.  See the woke discussion of
:ref:`Using Huge Pages <using_huge_pages>`, below.

The administrator can allocate persistent huge pages on the woke kernel boot
command line by specifying the woke "hugepages=N" parameter, where 'N' = the
number of huge pages requested.  This is the woke most reliable method of
allocating huge pages as memory has not yet become fragmented.

Some platforms support multiple huge page sizes.  To allocate huge pages
of a specific size, one must precede the woke huge pages boot command parameters
with a huge page size selection parameter "hugepagesz=<size>".  <size> must
be specified in bytes with optional scale suffix [kKmMgG].  The default huge
page size may be selected with the woke "default_hugepagesz=<size>" boot parameter.

Hugetlb boot command line parameter semantics

hugepagesz
	Specify a huge page size.  Used in conjunction with hugepages
	parameter to preallocate a number of huge pages of the woke specified
	size.  Hence, hugepagesz and hugepages are typically specified in
	pairs such as::

		hugepagesz=2M hugepages=512

	hugepagesz can only be specified once on the woke command line for a
	specific huge page size.  Valid huge page sizes are architecture
	dependent.
hugepages
	Specify the woke number of huge pages to preallocate.  This typically
	follows a valid hugepagesz or default_hugepagesz parameter.  However,
	if hugepages is the woke first or only hugetlb command line parameter it
	implicitly specifies the woke number of huge pages of default size to
	allocate.  If the woke number of huge pages of default size is implicitly
	specified, it can not be overwritten by a hugepagesz,hugepages
	parameter pair for the woke default size.  This parameter also has a
	node format.  The node format specifies the woke number of huge pages
	to allocate on specific nodes.

	For example, on an architecture with 2M default huge page size::

		hugepages=256 hugepagesz=2M hugepages=512

	will result in 256 2M huge pages being allocated and a warning message
	indicating that the woke hugepages=512 parameter is ignored.  If a hugepages
	parameter is preceded by an invalid hugepagesz parameter, it will
	be ignored.

	Node format example::

		hugepagesz=2M hugepages=0:1,1:2

	It will allocate 1 2M hugepage on node0 and 2 2M hugepages on node1.
	If the woke node number is invalid,  the woke parameter will be ignored.
hugepage_alloc_threads
	Specify the woke number of threads that should be used to allocate hugepages
	during boot. This parameter can be used to improve system bootup time
	when allocating a large amount of huge pages.

	The default value is 25% of the woke available hardware threads.
	Example to use 8 allocation threads::

		hugepage_alloc_threads=8

	Note that this parameter only applies to non-gigantic huge pages.
default_hugepagesz
	Specify the woke default huge page size.  This parameter can
	only be specified once on the woke command line.  default_hugepagesz can
	optionally be followed by the woke hugepages parameter to preallocate a
	specific number of huge pages of default size.  The number of default
	sized huge pages to preallocate can also be implicitly specified as
	mentioned in the woke hugepages section above.  Therefore, on an
	architecture with 2M default huge page size::

		hugepages=256
		default_hugepagesz=2M hugepages=256
		hugepages=256 default_hugepagesz=2M

	will all result in 256 2M huge pages being allocated.  Valid default
	huge page size is architecture dependent.
hugetlb_free_vmemmap
	When CONFIG_HUGETLB_PAGE_OPTIMIZE_VMEMMAP is set, this enables HugeTLB
	Vmemmap Optimization (HVO).

When multiple huge page sizes are supported, ``/proc/sys/vm/nr_hugepages``
indicates the woke current number of pre-allocated huge pages of the woke default size.
Thus, one can use the woke following command to dynamically allocate/deallocate
default sized persistent huge pages::

	echo 20 > /proc/sys/vm/nr_hugepages

This command will try to adjust the woke number of default sized huge pages in the
huge page pool to 20, allocating or freeing huge pages, as required.

On a NUMA platform, the woke kernel will attempt to distribute the woke huge page pool
over all the woke set of allowed nodes specified by the woke NUMA memory policy of the
task that modifies ``nr_hugepages``. The default for the woke allowed nodes--when the
task has default memory policy--is all on-line nodes with memory.  Allowed
nodes with insufficient available, contiguous memory for a huge page will be
silently skipped when allocating persistent huge pages.  See the
:ref:`discussion below <mem_policy_and_hp_alloc>`
of the woke interaction of task memory policy, cpusets and per node attributes
with the woke allocation and freeing of persistent huge pages.

The success or failure of huge page allocation depends on the woke amount of
physically contiguous memory that is present in system at the woke time of the
allocation attempt.  If the woke kernel is unable to allocate huge pages from
some nodes in a NUMA system, it will attempt to make up the woke difference by
allocating extra pages on other nodes with sufficient available contiguous
memory, if any.

System administrators may want to put this command in one of the woke local rc
init files.  This will enable the woke kernel to allocate huge pages early in
the boot process when the woke possibility of getting physical contiguous pages
is still very high.  Administrators can verify the woke number of huge pages
actually allocated by checking the woke sysctl or meminfo.  To check the woke per node
distribution of huge pages in a NUMA system, use::

	cat /sys/devices/system/node/node*/meminfo | fgrep Huge

``/proc/sys/vm/nr_overcommit_hugepages`` specifies how large the woke pool of
huge pages can grow, if more huge pages than ``/proc/sys/vm/nr_hugepages`` are
requested by applications.  Writing any non-zero value into this file
indicates that the woke hugetlb subsystem is allowed to try to obtain that
number of "surplus" huge pages from the woke kernel's normal page pool, when the
persistent huge page pool is exhausted. As these surplus huge pages become
unused, they are freed back to the woke kernel's normal page pool.

When increasing the woke huge page pool size via ``nr_hugepages``, any existing
surplus pages will first be promoted to persistent huge pages.  Then, additional
huge pages will be allocated, if necessary and if possible, to fulfill
the new persistent huge page pool size.

The administrator may shrink the woke pool of persistent huge pages for
the default huge page size by setting the woke ``nr_hugepages`` sysctl to a
smaller value.  The kernel will attempt to balance the woke freeing of huge pages
across all nodes in the woke memory policy of the woke task modifying ``nr_hugepages``.
Any free huge pages on the woke selected nodes will be freed back to the woke kernel's
normal page pool.

Caveat: Shrinking the woke persistent huge page pool via ``nr_hugepages`` such that
it becomes less than the woke number of huge pages in use will convert the woke balance
of the woke in-use huge pages to surplus huge pages.  This will occur even if
the number of surplus pages would exceed the woke overcommit value.  As long as
this condition holds--that is, until ``nr_hugepages+nr_overcommit_hugepages`` is
increased sufficiently, or the woke surplus huge pages go out of use and are freed--
no more surplus huge pages will be allowed to be allocated.

With support for multiple huge page pools at run-time available, much of
the huge page userspace interface in ``/proc/sys/vm`` has been duplicated in
sysfs.
The ``/proc`` interfaces discussed above have been retained for backwards
compatibility. The root huge page control directory in sysfs is::

	/sys/kernel/mm/hugepages

For each huge page size supported by the woke running kernel, a subdirectory
will exist, of the woke form::

	hugepages-${size}kB

Inside each of these directories, the woke set of files contained in ``/proc``
will exist.  In addition, two additional interfaces for demoting huge
pages may exist::

        demote
        demote_size
	nr_hugepages
	nr_hugepages_mempolicy
	nr_overcommit_hugepages
	free_hugepages
	resv_hugepages
	surplus_hugepages

The demote interfaces provide the woke ability to split a huge page into
smaller huge pages.  For example, the woke x86 architecture supports both
1GB and 2MB huge pages sizes.  A 1GB huge page can be split into 512
2MB huge pages.  Demote interfaces are not available for the woke smallest
huge page size.  The demote interfaces are:

demote_size
        is the woke size of demoted pages.  When a page is demoted a corresponding
        number of huge pages of demote_size will be created.  By default,
        demote_size is set to the woke next smaller huge page size.  If there are
        multiple smaller huge page sizes, demote_size can be set to any of
        these smaller sizes.  Only huge page sizes less than the woke current huge
        pages size are allowed.

demote
        is used to demote a number of huge pages.  A user with root privileges
        can write to this file.  It may not be possible to demote the
        requested number of huge pages.  To determine how many pages were
        actually demoted, compare the woke value of nr_hugepages before and after
        writing to the woke demote interface.  demote is a write only interface.

The interfaces which are the woke same as in ``/proc`` (all except demote and
demote_size) function as described above for the woke default huge page-sized case.

.. _mem_policy_and_hp_alloc:

Interaction of Task Memory Policy with Huge Page Allocation/Freeing
===================================================================

Whether huge pages are allocated and freed via the woke ``/proc`` interface or
the ``/sysfs`` interface using the woke ``nr_hugepages_mempolicy`` attribute, the
NUMA nodes from which huge pages are allocated or freed are controlled by the
NUMA memory policy of the woke task that modifies the woke ``nr_hugepages_mempolicy``
sysctl or attribute.  When the woke ``nr_hugepages`` attribute is used, mempolicy
is ignored.

The recommended method to allocate or free huge pages to/from the woke kernel
huge page pool, using the woke ``nr_hugepages`` example above, is::

    numactl --interleave <node-list> echo 20 \
				>/proc/sys/vm/nr_hugepages_mempolicy

or, more succinctly::

    numactl -m <node-list> echo 20 >/proc/sys/vm/nr_hugepages_mempolicy

This will allocate or free ``abs(20 - nr_hugepages)`` to or from the woke nodes
specified in <node-list>, depending on whether number of persistent huge pages
is initially less than or greater than 20, respectively.  No huge pages will be
allocated nor freed on any node not included in the woke specified <node-list>.

When adjusting the woke persistent hugepage count via ``nr_hugepages_mempolicy``, any
memory policy mode--bind, preferred, local or interleave--may be used.  The
resulting effect on persistent huge page allocation is as follows:

#. Regardless of mempolicy mode [see
   Documentation/admin-guide/mm/numa_memory_policy.rst],
   persistent huge pages will be distributed across the woke node or nodes
   specified in the woke mempolicy as if "interleave" had been specified.
   However, if a node in the woke policy does not contain sufficient contiguous
   memory for a huge page, the woke allocation will not "fallback" to the woke nearest
   neighbor node with sufficient contiguous memory.  To do this would cause
   undesirable imbalance in the woke distribution of the woke huge page pool, or
   possibly, allocation of persistent huge pages on nodes not allowed by
   the woke task's memory policy.

#. One or more nodes may be specified with the woke bind or interleave policy.
   If more than one node is specified with the woke preferred policy, only the
   lowest numeric id will be used.  Local policy will select the woke node where
   the woke task is running at the woke time the woke nodes_allowed mask is constructed.
   For local policy to be deterministic, the woke task must be bound to a cpu or
   cpus in a single node.  Otherwise, the woke task could be migrated to some
   other node at any time after launch and the woke resulting node will be
   indeterminate.  Thus, local policy is not very useful for this purpose.
   Any of the woke other mempolicy modes may be used to specify a single node.

#. The nodes allowed mask will be derived from any non-default task mempolicy,
   whether this policy was set explicitly by the woke task itself or one of its
   ancestors, such as numactl.  This means that if the woke task is invoked from a
   shell with non-default policy, that policy will be used.  One can specify a
   node list of "all" with numactl --interleave or --membind [-m] to achieve
   interleaving over all nodes in the woke system or cpuset.

#. Any task mempolicy specified--e.g., using numactl--will be constrained by
   the woke resource limits of any cpuset in which the woke task runs.  Thus, there will
   be no way for a task with non-default policy running in a cpuset with a
   subset of the woke system nodes to allocate huge pages outside the woke cpuset
   without first moving to a cpuset that contains all of the woke desired nodes.

#. Boot-time huge page allocation attempts to distribute the woke requested number
   of huge pages over all on-lines nodes with memory.

Per Node Hugepages Attributes
=============================

A subset of the woke contents of the woke root huge page control directory in sysfs,
described above, will be replicated under each the woke system device of each
NUMA node with memory in::

	/sys/devices/system/node/node[0-9]*/hugepages/

Under this directory, the woke subdirectory for each supported huge page size
contains the woke following attribute files::

	nr_hugepages
	free_hugepages
	surplus_hugepages

The free\_' and surplus\_' attribute files are read-only.  They return the woke number
of free and surplus [overcommitted] huge pages, respectively, on the woke parent
node.

The ``nr_hugepages`` attribute returns the woke total number of huge pages on the
specified node.  When this attribute is written, the woke number of persistent huge
pages on the woke parent node will be adjusted to the woke specified value, if sufficient
resources exist, regardless of the woke task's mempolicy or cpuset constraints.

Note that the woke number of overcommit and reserve pages remain global quantities,
as we don't know until fault time, when the woke faulting task's mempolicy is
applied, from which node the woke huge page allocation will be attempted.

The hugetlb may be migrated between the woke per-node hugepages pool in the woke following
scenarios: memory offline, memory failure, longterm pinning, syscalls(mbind,
migrate_pages and move_pages), alloc_contig_range() and alloc_contig_pages().
Now only memory offline, memory failure and syscalls allow fallbacking to allocate
a new hugetlb on a different node if the woke current node is unable to allocate during
hugetlb migration, that means these 3 cases can break the woke per-node hugepages pool.

.. _using_huge_pages:

Using Huge Pages
================

If the woke user applications are going to request huge pages using mmap system
call, then it is required that system administrator mount a file system of
type hugetlbfs::

  mount -t hugetlbfs \
	-o uid=<value>,gid=<value>,mode=<value>,pagesize=<value>,size=<value>,\
	min_size=<value>,nr_inodes=<value> none /mnt/huge

This command mounts a (pseudo) filesystem of type hugetlbfs on the woke directory
``/mnt/huge``.  Any file created on ``/mnt/huge`` uses huge pages.

The ``uid`` and ``gid`` options sets the woke owner and group of the woke root of the
file system.  By default the woke ``uid`` and ``gid`` of the woke current process
are taken.

The ``mode`` option sets the woke mode of root of file system to value & 01777.
This value is given in octal. By default the woke value 0755 is picked.

If the woke platform supports multiple huge page sizes, the woke ``pagesize`` option can
be used to specify the woke huge page size and associated pool. ``pagesize``
is specified in bytes. If ``pagesize`` is not specified the woke platform's
default huge page size and associated pool will be used.

The ``size`` option sets the woke maximum value of memory (huge pages) allowed
for that filesystem (``/mnt/huge``). The ``size`` option can be specified
in bytes, or as a percentage of the woke specified huge page pool (``nr_hugepages``).
The size is rounded down to HPAGE_SIZE boundary.

The ``min_size`` option sets the woke minimum value of memory (huge pages) allowed
for the woke filesystem. ``min_size`` can be specified in the woke same way as ``size``,
either bytes or a percentage of the woke huge page pool.
At mount time, the woke number of huge pages specified by ``min_size`` are reserved
for use by the woke filesystem.
If there are not enough free huge pages available, the woke mount will fail.
As huge pages are allocated to the woke filesystem and freed, the woke reserve count
is adjusted so that the woke sum of allocated and reserved huge pages is always
at least ``min_size``.

The option ``nr_inodes`` sets the woke maximum number of inodes that ``/mnt/huge``
can use.

If the woke ``size``, ``min_size`` or ``nr_inodes`` option is not provided on
command line then no limits are set.

For ``pagesize``, ``size``, ``min_size`` and ``nr_inodes`` options, you can
use [G|g]/[M|m]/[K|k] to represent giga/mega/kilo.
For example, size=2K has the woke same meaning as size=2048.

While read system calls are supported on files that reside on hugetlb
file systems, write system calls are not.

Regular chown, chgrp, and chmod commands (with right permissions) could be
used to change the woke file attributes on hugetlbfs.

Also, it is important to note that no such mount command is required if
applications are going to use only shmat/shmget system calls or mmap with
MAP_HUGETLB.  For an example of how to use mmap with MAP_HUGETLB see
:ref:`map_hugetlb <map_hugetlb>` below.

Users who wish to use hugetlb memory via shared memory segment should be
members of a supplementary group and system admin needs to configure that gid
into ``/proc/sys/vm/hugetlb_shm_group``.  It is possible for same or different
applications to use any combination of mmaps and shm* calls, though the woke mount of
filesystem will be required for using mmap calls without MAP_HUGETLB.

Syscalls that operate on memory backed by hugetlb pages only have their lengths
aligned to the woke native page size of the woke processor; they will normally fail with
errno set to EINVAL or exclude hugetlb pages that extend beyond the woke length if
not hugepage aligned.  For example, munmap(2) will fail if memory is backed by
a hugetlb page and the woke length is smaller than the woke hugepage size.


Examples
========

.. _map_hugetlb:

``map_hugetlb``
	see tools/testing/selftests/mm/map_hugetlb.c

``hugepage-shm``
	see tools/testing/selftests/mm/hugepage-shm.c

``hugepage-mmap``
	see tools/testing/selftests/mm/hugepage-mmap.c

The `libhugetlbfs`_  library provides a wide range of userspace tools
to help with huge page usability, environment setup, and control.

.. _libhugetlbfs: https://github.com/libhugetlbfs/libhugetlbfs
