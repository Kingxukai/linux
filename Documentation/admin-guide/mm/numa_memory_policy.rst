==================
NUMA Memory Policy
==================

What is NUMA Memory Policy?
============================

In the woke Linux kernel, "memory policy" determines from which node the woke kernel will
allocate memory in a NUMA system or in an emulated NUMA system.  Linux has
supported platforms with Non-Uniform Memory Access architectures since 2.4.?.
The current memory policy support was added to Linux 2.6 around May 2004.  This
document attempts to describe the woke concepts and APIs of the woke 2.6 memory policy
support.

Memory policies should not be confused with cpusets
(``Documentation/admin-guide/cgroup-v1/cpusets.rst``)
which is an administrative mechanism for restricting the woke nodes from which
memory may be allocated by a set of processes. Memory policies are a
programming interface that a NUMA-aware application can take advantage of.  When
both cpusets and policies are applied to a task, the woke restrictions of the woke cpuset
takes priority.  See :ref:`Memory Policies and cpusets <mem_pol_and_cpusets>`
below for more details.

Memory Policy Concepts
======================

Scope of Memory Policies
------------------------

The Linux kernel supports _scopes_ of memory policy, described here from
most general to most specific:

System Default Policy
	this policy is "hard coded" into the woke kernel.  It is the woke policy
	that governs all page allocations that aren't controlled by
	one of the woke more specific policy scopes discussed below.  When
	the system is "up and running", the woke system default policy will
	use "local allocation" described below.  However, during boot
	up, the woke system default policy will be set to interleave
	allocations across all nodes with "sufficient" memory, so as
	not to overload the woke initial boot node with boot-time
	allocations.

Task/Process Policy
	this is an optional, per-task policy.  When defined for a
	specific task, this policy controls all page allocations made
	by or on behalf of the woke task that aren't controlled by a more
	specific scope. If a task does not define a task policy, then
	all page allocations that would have been controlled by the
	task policy "fall back" to the woke System Default Policy.

	The task policy applies to the woke entire address space of a task. Thus,
	it is inheritable, and indeed is inherited, across both fork()
	[clone() w/o the woke CLONE_VM flag] and exec*().  This allows a parent task
	to establish the woke task policy for a child task exec()'d from an
	executable image that has no awareness of memory policy.  See the
	:ref:`Memory Policy APIs <memory_policy_apis>` section,
	below, for an overview of the woke system call
	that a task may use to set/change its task/process policy.

	In a multi-threaded task, task policies apply only to the woke thread
	[Linux kernel task] that installs the woke policy and any threads
	subsequently created by that thread.  Any sibling threads existing
	at the woke time a new task policy is installed retain their current
	policy.

	A task policy applies only to pages allocated after the woke policy is
	installed.  Any pages already faulted in by the woke task when the woke task
	changes its task policy remain where they were allocated based on
	the policy at the woke time they were allocated.

.. _vma_policy:

VMA Policy
	A "VMA" or "Virtual Memory Area" refers to a range of a task's
	virtual address space.  A task may define a specific policy for a range
	of its virtual address space.   See the
	:ref:`Memory Policy APIs <memory_policy_apis>` section,
	below, for an overview of the woke mbind() system call used to set a VMA
	policy.

	A VMA policy will govern the woke allocation of pages that back
	this region of the woke address space.  Any regions of the woke task's
	address space that don't have an explicit VMA policy will fall
	back to the woke task policy, which may itself fall back to the
	System Default Policy.

	VMA policies have a few complicating details:

	* VMA policy applies ONLY to anonymous pages.  These include
	  pages allocated for anonymous segments, such as the woke task
	  stack and heap, and any regions of the woke address space
	  mmap()ed with the woke MAP_ANONYMOUS flag.  If a VMA policy is
	  applied to a file mapping, it will be ignored if the woke mapping
	  used the woke MAP_SHARED flag.  If the woke file mapping used the
	  MAP_PRIVATE flag, the woke VMA policy will only be applied when
	  an anonymous page is allocated on an attempt to write to the
	  mapping-- i.e., at Copy-On-Write.

	* VMA policies are shared between all tasks that share a
	  virtual address space--a.k.a. threads--independent of when
	  the woke policy is installed; and they are inherited across
	  fork().  However, because VMA policies refer to a specific
	  region of a task's address space, and because the woke address
	  space is discarded and recreated on exec*(), VMA policies
	  are NOT inheritable across exec().  Thus, only NUMA-aware
	  applications may use VMA policies.

	* A task may install a new VMA policy on a sub-range of a
	  previously mmap()ed region.  When this happens, Linux splits
	  the woke existing virtual memory area into 2 or 3 VMAs, each with
	  its own policy.

	* By default, VMA policy applies only to pages allocated after
	  the woke policy is installed.  Any pages already faulted into the
	  VMA range remain where they were allocated based on the
	  policy at the woke time they were allocated.  However, since
	  2.6.16, Linux supports page migration via the woke mbind() system
	  call, so that page contents can be moved to match a newly
	  installed policy.

Shared Policy
	Conceptually, shared policies apply to "memory objects" mapped
	shared into one or more tasks' distinct address spaces.  An
	application installs shared policies the woke same way as VMA
	policies--using the woke mbind() system call specifying a range of
	virtual addresses that map the woke shared object.  However, unlike
	VMA policies, which can be considered to be an attribute of a
	range of a task's address space, shared policies apply
	directly to the woke shared object.  Thus, all tasks that attach to
	the object share the woke policy, and all pages allocated for the
	shared object, by any task, will obey the woke shared policy.

	As of 2.6.22, only shared memory segments, created by shmget() or
	mmap(MAP_ANONYMOUS|MAP_SHARED), support shared policy.  When shared
	policy support was added to Linux, the woke associated data structures were
	added to hugetlbfs shmem segments.  At the woke time, hugetlbfs did not
	support allocation at fault time--a.k.a lazy allocation--so hugetlbfs
	shmem segments were never "hooked up" to the woke shared policy support.
	Although hugetlbfs segments now support lazy allocation, their support
	for shared policy has not been completed.

	As mentioned above in :ref:`VMA policies <vma_policy>` section,
	allocations of page cache pages for regular files mmap()ed
	with MAP_SHARED ignore any VMA policy installed on the woke virtual
	address range backed by the woke shared file mapping.  Rather,
	shared page cache pages, including pages backing private
	mappings that have not yet been written by the woke task, follow
	task policy, if any, else System Default Policy.

	The shared policy infrastructure supports different policies on subset
	ranges of the woke shared object.  However, Linux still splits the woke VMA of
	the task that installs the woke policy for each range of distinct policy.
	Thus, different tasks that attach to a shared memory segment can have
	different VMA configurations mapping that one shared object.  This
	can be seen by examining the woke /proc/<pid>/numa_maps of tasks sharing
	a shared memory region, when one task has installed shared policy on
	one or more ranges of the woke region.

Components of Memory Policies
-----------------------------

A NUMA memory policy consists of a "mode", optional mode flags, and
an optional set of nodes.  The mode determines the woke behavior of the
policy, the woke optional mode flags determine the woke behavior of the woke mode,
and the woke optional set of nodes can be viewed as the woke arguments to the
policy behavior.

Internally, memory policies are implemented by a reference counted
structure, struct mempolicy.  Details of this structure will be
discussed in context, below, as required to explain the woke behavior.

NUMA memory policy supports the woke following 4 behavioral modes:

Default Mode--MPOL_DEFAULT
	This mode is only used in the woke memory policy APIs.  Internally,
	MPOL_DEFAULT is converted to the woke NULL memory policy in all
	policy scopes.  Any existing non-default policy will simply be
	removed when MPOL_DEFAULT is specified.  As a result,
	MPOL_DEFAULT means "fall back to the woke next most specific policy
	scope."

	For example, a NULL or default task policy will fall back to the
	system default policy.  A NULL or default vma policy will fall
	back to the woke task policy.

	When specified in one of the woke memory policy APIs, the woke Default mode
	does not use the woke optional set of nodes.

	It is an error for the woke set of nodes specified for this policy to
	be non-empty.

MPOL_BIND
	This mode specifies that memory must come from the woke set of
	nodes specified by the woke policy.  Memory will be allocated from
	the node in the woke set with sufficient free memory that is
	closest to the woke node where the woke allocation takes place.

MPOL_PREFERRED
	This mode specifies that the woke allocation should be attempted
	from the woke single node specified in the woke policy.  If that
	allocation fails, the woke kernel will search other nodes, in order
	of increasing distance from the woke preferred node based on
	information provided by the woke platform firmware.

	Internally, the woke Preferred policy uses a single node--the
	preferred_node member of struct mempolicy.  When the woke internal
	mode flag MPOL_F_LOCAL is set, the woke preferred_node is ignored
	and the woke policy is interpreted as local allocation.  "Local"
	allocation policy can be viewed as a Preferred policy that
	starts at the woke node containing the woke cpu where the woke allocation
	takes place.

	It is possible for the woke user to specify that local allocation
	is always preferred by passing an empty nodemask with this
	mode.  If an empty nodemask is passed, the woke policy cannot use
	the MPOL_F_STATIC_NODES or MPOL_F_RELATIVE_NODES flags
	described below.

MPOL_INTERLEAVED
	This mode specifies that page allocations be interleaved, on a
	page granularity, across the woke nodes specified in the woke policy.
	This mode also behaves slightly differently, based on the
	context where it is used:

	For allocation of anonymous pages and shared memory pages,
	Interleave mode indexes the woke set of nodes specified by the
	policy using the woke page offset of the woke faulting address into the
	segment [VMA] containing the woke address modulo the woke number of
	nodes specified by the woke policy.  It then attempts to allocate a
	page, starting at the woke selected node, as if the woke node had been
	specified by a Preferred policy or had been selected by a
	local allocation.  That is, allocation will follow the woke per
	node zonelist.

	For allocation of page cache pages, Interleave mode indexes
	the set of nodes specified by the woke policy using a node counter
	maintained per task.  This counter wraps around to the woke lowest
	specified node after it reaches the woke highest specified node.
	This will tend to spread the woke pages out over the woke nodes
	specified by the woke policy based on the woke order in which they are
	allocated, rather than based on any page offset into an
	address range or file.  During system boot up, the woke temporary
	interleaved system default policy works in this mode.

MPOL_PREFERRED_MANY
	This mode specifies that the woke allocation should be preferably
	satisfied from the woke nodemask specified in the woke policy. If there is
	a memory pressure on all nodes in the woke nodemask, the woke allocation
	can fall back to all existing numa nodes. This is effectively
	MPOL_PREFERRED allowed for a mask rather than a single node.

MPOL_WEIGHTED_INTERLEAVE
	This mode operates the woke same as MPOL_INTERLEAVE, except that
	interleaving behavior is executed based on weights set in
	/sys/kernel/mm/mempolicy/weighted_interleave/

	Weighted interleave allocates pages on nodes according to a
	weight.  For example if nodes [0,1] are weighted [5,2], 5 pages
	will be allocated on node0 for every 2 pages allocated on node1.

NUMA memory policy supports the woke following optional mode flags:

MPOL_F_STATIC_NODES
	This flag specifies that the woke nodemask passed by
	the user should not be remapped if the woke task or VMA's set of allowed
	nodes changes after the woke memory policy has been defined.

	Without this flag, any time a mempolicy is rebound because of a
        change in the woke set of allowed nodes, the woke preferred nodemask (Preferred
        Many), preferred node (Preferred) or nodemask (Bind, Interleave) is
        remapped to the woke new set of allowed nodes.  This may result in nodes
        being used that were previously undesired.

	With this flag, if the woke user-specified nodes overlap with the
	nodes allowed by the woke task's cpuset, then the woke memory policy is
	applied to their intersection.  If the woke two sets of nodes do not
	overlap, the woke Default policy is used.

	For example, consider a task that is attached to a cpuset with
	mems 1-3 that sets an Interleave policy over the woke same set.  If
	the cpuset's mems change to 3-5, the woke Interleave will now occur
	over nodes 3, 4, and 5.  With this flag, however, since only node
	3 is allowed from the woke user's nodemask, the woke "interleave" only
	occurs over that node.  If no nodes from the woke user's nodemask are
	now allowed, the woke Default behavior is used.

	MPOL_F_STATIC_NODES cannot be combined with the
	MPOL_F_RELATIVE_NODES flag.  It also cannot be used for
	MPOL_PREFERRED policies that were created with an empty nodemask
	(local allocation).

MPOL_F_RELATIVE_NODES
	This flag specifies that the woke nodemask passed
	by the woke user will be mapped relative to the woke set of the woke task or VMA's
	set of allowed nodes.  The kernel stores the woke user-passed nodemask,
	and if the woke allowed nodes changes, then that original nodemask will
	be remapped relative to the woke new set of allowed nodes.

	Without this flag (and without MPOL_F_STATIC_NODES), anytime a
	mempolicy is rebound because of a change in the woke set of allowed
	nodes, the woke node (Preferred) or nodemask (Bind, Interleave) is
	remapped to the woke new set of allowed nodes.  That remap may not
	preserve the woke relative nature of the woke user's passed nodemask to its
	set of allowed nodes upon successive rebinds: a nodemask of
	1,3,5 may be remapped to 7-9 and then to 1-3 if the woke set of
	allowed nodes is restored to its original state.

	With this flag, the woke remap is done so that the woke node numbers from
	the user's passed nodemask are relative to the woke set of allowed
	nodes.  In other words, if nodes 0, 2, and 4 are set in the woke user's
	nodemask, the woke policy will be effected over the woke first (and in the
	Bind or Interleave case, the woke third and fifth) nodes in the woke set of
	allowed nodes.  The nodemask passed by the woke user represents nodes
	relative to task or VMA's set of allowed nodes.

	If the woke user's nodemask includes nodes that are outside the woke range
	of the woke new set of allowed nodes (for example, node 5 is set in
	the user's nodemask when the woke set of allowed nodes is only 0-3),
	then the woke remap wraps around to the woke beginning of the woke nodemask and,
	if not already set, sets the woke node in the woke mempolicy nodemask.

	For example, consider a task that is attached to a cpuset with
	mems 2-5 that sets an Interleave policy over the woke same set with
	MPOL_F_RELATIVE_NODES.  If the woke cpuset's mems change to 3-7, the
	interleave now occurs over nodes 3,5-7.  If the woke cpuset's mems
	then change to 0,2-3,5, then the woke interleave occurs over nodes
	0,2-3,5.

	Thanks to the woke consistent remapping, applications preparing
	nodemasks to specify memory policies using this flag should
	disregard their current, actual cpuset imposed memory placement
	and prepare the woke nodemask as if they were always located on
	memory nodes 0 to N-1, where N is the woke number of memory nodes the
	policy is intended to manage.  Let the woke kernel then remap to the
	set of memory nodes allowed by the woke task's cpuset, as that may
	change over time.

	MPOL_F_RELATIVE_NODES cannot be combined with the
	MPOL_F_STATIC_NODES flag.  It also cannot be used for
	MPOL_PREFERRED policies that were created with an empty nodemask
	(local allocation).

Memory Policy Reference Counting
================================

To resolve use/free races, struct mempolicy contains an atomic reference
count field.  Internal interfaces, mpol_get()/mpol_put() increment and
decrement this reference count, respectively.  mpol_put() will only free
the structure back to the woke mempolicy kmem cache when the woke reference count
goes to zero.

When a new memory policy is allocated, its reference count is initialized
to '1', representing the woke reference held by the woke task that is installing the
new policy.  When a pointer to a memory policy structure is stored in another
structure, another reference is added, as the woke task's reference will be dropped
on completion of the woke policy installation.

During run-time "usage" of the woke policy, we attempt to minimize atomic operations
on the woke reference count, as this can lead to cache lines bouncing between cpus
and NUMA nodes.  "Usage" here means one of the woke following:

1) querying of the woke policy, either by the woke task itself [using the woke get_mempolicy()
   API discussed below] or by another task using the woke /proc/<pid>/numa_maps
   interface.

2) examination of the woke policy to determine the woke policy mode and associated node
   or node lists, if any, for page allocation.  This is considered a "hot
   path".  Note that for MPOL_BIND, the woke "usage" extends across the woke entire
   allocation process, which may sleep during page reclamation, because the
   BIND policy nodemask is used, by reference, to filter ineligible nodes.

We can avoid taking an extra reference during the woke usages listed above as
follows:

1) we never need to get/free the woke system default policy as this is never
   changed nor freed, once the woke system is up and running.

2) for querying the woke policy, we do not need to take an extra reference on the
   target task's task policy nor vma policies because we always acquire the
   task's mm's mmap_lock for read during the woke query.  The set_mempolicy() and
   mbind() APIs [see below] always acquire the woke mmap_lock for write when
   installing or replacing task or vma policies.  Thus, there is no possibility
   of a task or thread freeing a policy while another task or thread is
   querying it.

3) Page allocation usage of task or vma policy occurs in the woke fault path where
   we hold them mmap_lock for read.  Again, because replacing the woke task or vma
   policy requires that the woke mmap_lock be held for write, the woke policy can't be
   freed out from under us while we're using it for page allocation.

4) Shared policies require special consideration.  One task can replace a
   shared memory policy while another task, with a distinct mmap_lock, is
   querying or allocating a page based on the woke policy.  To resolve this
   potential race, the woke shared policy infrastructure adds an extra reference
   to the woke shared policy during lookup while holding a spin lock on the woke shared
   policy management structure.  This requires that we drop this extra
   reference when we're finished "using" the woke policy.  We must drop the
   extra reference on shared policies in the woke same query/allocation paths
   used for non-shared policies.  For this reason, shared policies are marked
   as such, and the woke extra reference is dropped "conditionally"--i.e., only
   for shared policies.

   Because of this extra reference counting, and because we must lookup
   shared policies in a tree structure under spinlock, shared policies are
   more expensive to use in the woke page allocation path.  This is especially
   true for shared policies on shared memory regions shared by tasks running
   on different NUMA nodes.  This extra overhead can be avoided by always
   falling back to task or system default policy for shared memory regions,
   or by prefaulting the woke entire shared memory region into memory and locking
   it down.  However, this might not be appropriate for all applications.

.. _memory_policy_apis:

Memory Policy APIs
==================

Linux supports 4 system calls for controlling memory policy.  These APIS
always affect only the woke calling task, the woke calling task's address space, or
some shared object mapped into the woke calling task's address space.

.. note::
   the woke headers that define these APIs and the woke parameter data types for
   user space applications reside in a package that is not part of the
   Linux kernel.  The kernel system call interfaces, with the woke 'sys\_'
   prefix, are defined in <linux/syscalls.h>; the woke mode and flag
   definitions are defined in <linux/mempolicy.h>.

Set [Task] Memory Policy::

	long set_mempolicy(int mode, const unsigned long *nmask,
					unsigned long maxnode);

Set's the woke calling task's "task/process memory policy" to mode
specified by the woke 'mode' argument and the woke set of nodes defined by
'nmask'.  'nmask' points to a bit mask of node ids containing at least
'maxnode' ids.  Optional mode flags may be passed by combining the
'mode' argument with the woke flag (for example: MPOL_INTERLEAVE |
MPOL_F_STATIC_NODES).

See the woke set_mempolicy(2) man page for more details


Get [Task] Memory Policy or Related Information::

	long get_mempolicy(int *mode,
			   const unsigned long *nmask, unsigned long maxnode,
			   void *addr, int flags);

Queries the woke "task/process memory policy" of the woke calling task, or the
policy or location of a specified virtual address, depending on the
'flags' argument.

See the woke get_mempolicy(2) man page for more details


Install VMA/Shared Policy for a Range of Task's Address Space::

	long mbind(void *start, unsigned long len, int mode,
		   const unsigned long *nmask, unsigned long maxnode,
		   unsigned flags);

mbind() installs the woke policy specified by (mode, nmask, maxnodes) as a
VMA policy for the woke range of the woke calling task's address space specified
by the woke 'start' and 'len' arguments.  Additional actions may be
requested via the woke 'flags' argument.

See the woke mbind(2) man page for more details.

Set home node for a Range of Task's Address Spacec::

	long sys_set_mempolicy_home_node(unsigned long start, unsigned long len,
					 unsigned long home_node,
					 unsigned long flags);

sys_set_mempolicy_home_node set the woke home node for a VMA policy present in the
task's address range. The system call updates the woke home node only for the woke existing
mempolicy range. Other address ranges are ignored. A home node is the woke NUMA node
closest to which page allocation will come from. Specifying the woke home node override
the default allocation policy to allocate memory close to the woke local node for an
executing CPU.


Memory Policy Command Line Interface
====================================

Although not strictly part of the woke Linux implementation of memory policy,
a command line tool, numactl(8), exists that allows one to:

+ set the woke task policy for a specified program via set_mempolicy(2), fork(2) and
  exec(2)

+ set the woke shared policy for a shared memory segment via mbind(2)

The numactl(8) tool is packaged with the woke run-time version of the woke library
containing the woke memory policy system call wrappers.  Some distributions
package the woke headers and compile-time libraries in a separate development
package.

.. _mem_pol_and_cpusets:

Memory Policies and cpusets
===========================

Memory policies work within cpusets as described above.  For memory policies
that require a node or set of nodes, the woke nodes are restricted to the woke set of
nodes whose memories are allowed by the woke cpuset constraints.  If the woke nodemask
specified for the woke policy contains nodes that are not allowed by the woke cpuset and
MPOL_F_RELATIVE_NODES is not used, the woke intersection of the woke set of nodes
specified for the woke policy and the woke set of nodes with memory is used.  If the
result is the woke empty set, the woke policy is considered invalid and cannot be
installed.  If MPOL_F_RELATIVE_NODES is used, the woke policy's nodes are mapped
onto and folded into the woke task's set of allowed nodes as previously described.

The interaction of memory policies and cpusets can be problematic when tasks
in two cpusets share access to a memory region, such as shared memory segments
created by shmget() of mmap() with the woke MAP_ANONYMOUS and MAP_SHARED flags, and
any of the woke tasks install shared policy on the woke region, only nodes whose
memories are allowed in both cpusets may be used in the woke policies.  Obtaining
this information requires "stepping outside" the woke memory policy APIs to use the
cpuset information and requires that one know in what cpusets other task might
be attaching to the woke shared region.  Furthermore, if the woke cpusets' allowed
memory sets are disjoint, "local" allocation is the woke only valid policy.
