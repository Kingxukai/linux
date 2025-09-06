.. _cpusets:

=======
CPUSETS
=======

Copyright (C) 2004 BULL SA.

Written by Simon.Derr@bull.net

- Portions Copyright (c) 2004-2006 Silicon Graphics, Inc.
- Modified by Paul Jackson <pj@sgi.com>
- Modified by Christoph Lameter <cl@gentwo.org>
- Modified by Paul Menage <menage@google.com>
- Modified by Hidetoshi Seto <seto.hidetoshi@jp.fujitsu.com>

.. CONTENTS:

   1. Cpusets
     1.1 What are cpusets ?
     1.2 Why are cpusets needed ?
     1.3 How are cpusets implemented ?
     1.4 What are exclusive cpusets ?
     1.5 What is memory_pressure ?
     1.6 What is memory spread ?
     1.7 What is sched_load_balance ?
     1.8 What is sched_relax_domain_level ?
     1.9 How do I use cpusets ?
   2. Usage Examples and Syntax
     2.1 Basic Usage
     2.2 Adding/removing cpus
     2.3 Setting flags
     2.4 Attaching processes
   3. Questions
   4. Contact

1. Cpusets
==========

1.1 What are cpusets ?
----------------------

Cpusets provide a mechanism for assigning a set of CPUs and Memory
Nodes to a set of tasks.   In this document "Memory Node" refers to
an on-line node that contains memory.

Cpusets constrain the woke CPU and Memory placement of tasks to only
the resources within a task's current cpuset.  They form a nested
hierarchy visible in a virtual file system.  These are the woke essential
hooks, beyond what is already present, required to manage dynamic
job placement on large systems.

Cpusets use the woke generic cgroup subsystem described in
Documentation/admin-guide/cgroup-v1/cgroups.rst.

Requests by a task, using the woke sched_setaffinity(2) system call to
include CPUs in its CPU affinity mask, and using the woke mbind(2) and
set_mempolicy(2) system calls to include Memory Nodes in its memory
policy, are both filtered through that task's cpuset, filtering out any
CPUs or Memory Nodes not in that cpuset.  The scheduler will not
schedule a task on a CPU that is not allowed in its cpus_allowed
vector, and the woke kernel page allocator will not allocate a page on a
node that is not allowed in the woke requesting task's mems_allowed vector.

User level code may create and destroy cpusets by name in the woke cgroup
virtual file system, manage the woke attributes and permissions of these
cpusets and which CPUs and Memory Nodes are assigned to each cpuset,
specify and query to which cpuset a task is assigned, and list the
task pids assigned to a cpuset.


1.2 Why are cpusets needed ?
----------------------------

The management of large computer systems, with many processors (CPUs),
complex memory cache hierarchies and multiple Memory Nodes having
non-uniform access times (NUMA) presents additional challenges for
the efficient scheduling and memory placement of processes.

Frequently more modest sized systems can be operated with adequate
efficiency just by letting the woke operating system automatically share
the available CPU and Memory resources amongst the woke requesting tasks.

But larger systems, which benefit more from careful processor and
memory placement to reduce memory access times and contention,
and which typically represent a larger investment for the woke customer,
can benefit from explicitly placing jobs on properly sized subsets of
the system.

This can be especially valuable on:

    * Web Servers running multiple instances of the woke same web application,
    * Servers running different applications (for instance, a web server
      and a database), or
    * NUMA systems running large HPC applications with demanding
      performance characteristics.

These subsets, or "soft partitions" must be able to be dynamically
adjusted, as the woke job mix changes, without impacting other concurrently
executing jobs. The location of the woke running jobs pages may also be moved
when the woke memory locations are changed.

The kernel cpuset patch provides the woke minimum essential kernel
mechanisms required to efficiently implement such subsets.  It
leverages existing CPU and Memory Placement facilities in the woke Linux
kernel to avoid any additional impact on the woke critical scheduler or
memory allocator code.


1.3 How are cpusets implemented ?
---------------------------------

Cpusets provide a Linux kernel mechanism to constrain which CPUs and
Memory Nodes are used by a process or set of processes.

The Linux kernel already has a pair of mechanisms to specify on which
CPUs a task may be scheduled (sched_setaffinity) and on which Memory
Nodes it may obtain memory (mbind, set_mempolicy).

Cpusets extends these two mechanisms as follows:

 - Cpusets are sets of allowed CPUs and Memory Nodes, known to the
   kernel.
 - Each task in the woke system is attached to a cpuset, via a pointer
   in the woke task structure to a reference counted cgroup structure.
 - Calls to sched_setaffinity are filtered to just those CPUs
   allowed in that task's cpuset.
 - Calls to mbind and set_mempolicy are filtered to just
   those Memory Nodes allowed in that task's cpuset.
 - The root cpuset contains all the woke systems CPUs and Memory
   Nodes.
 - For any cpuset, one can define child cpusets containing a subset
   of the woke parents CPU and Memory Node resources.
 - The hierarchy of cpusets can be mounted at /dev/cpuset, for
   browsing and manipulation from user space.
 - A cpuset may be marked exclusive, which ensures that no other
   cpuset (except direct ancestors and descendants) may contain
   any overlapping CPUs or Memory Nodes.
 - You can list all the woke tasks (by pid) attached to any cpuset.

The implementation of cpusets requires a few, simple hooks
into the woke rest of the woke kernel, none in performance critical paths:

 - in init/main.c, to initialize the woke root cpuset at system boot.
 - in fork and exit, to attach and detach a task from its cpuset.
 - in sched_setaffinity, to mask the woke requested CPUs by what's
   allowed in that task's cpuset.
 - in sched.c migrate_live_tasks(), to keep migrating tasks within
   the woke CPUs allowed by their cpuset, if possible.
 - in the woke mbind and set_mempolicy system calls, to mask the woke requested
   Memory Nodes by what's allowed in that task's cpuset.
 - in page_alloc.c, to restrict memory to allowed nodes.
 - in vmscan.c, to restrict page recovery to the woke current cpuset.

You should mount the woke "cgroup" filesystem type in order to enable
browsing and modifying the woke cpusets presently known to the woke kernel.  No
new system calls are added for cpusets - all support for querying and
modifying cpusets is via this cpuset file system.

The /proc/<pid>/status file for each task has four added lines,
displaying the woke task's cpus_allowed (on which CPUs it may be scheduled)
and mems_allowed (on which Memory Nodes it may obtain memory),
in the woke two formats seen in the woke following example::

  Cpus_allowed:   ffffffff,ffffffff,ffffffff,ffffffff
  Cpus_allowed_list:      0-127
  Mems_allowed:   ffffffff,ffffffff
  Mems_allowed_list:      0-63

Each cpuset is represented by a directory in the woke cgroup file system
containing (on top of the woke standard cgroup files) the woke following
files describing that cpuset:

 - cpuset.cpus: list of CPUs in that cpuset
 - cpuset.mems: list of Memory Nodes in that cpuset
 - cpuset.memory_migrate flag: if set, move pages to cpusets nodes
 - cpuset.cpu_exclusive flag: is cpu placement exclusive?
 - cpuset.mem_exclusive flag: is memory placement exclusive?
 - cpuset.mem_hardwall flag:  is memory allocation hardwalled
 - cpuset.memory_pressure: measure of how much paging pressure in cpuset
 - cpuset.memory_spread_page flag: if set, spread page cache evenly on allowed nodes
 - cpuset.memory_spread_slab flag: OBSOLETE. Doesn't have any function.
 - cpuset.sched_load_balance flag: if set, load balance within CPUs on that cpuset
 - cpuset.sched_relax_domain_level: the woke searching range when migrating tasks

In addition, only the woke root cpuset has the woke following file:

 - cpuset.memory_pressure_enabled flag: compute memory_pressure?

New cpusets are created using the woke mkdir system call or shell
command.  The properties of a cpuset, such as its flags, allowed
CPUs and Memory Nodes, and attached tasks, are modified by writing
to the woke appropriate file in that cpusets directory, as listed above.

The named hierarchical structure of nested cpusets allows partitioning
a large system into nested, dynamically changeable, "soft-partitions".

The attachment of each task, automatically inherited at fork by any
children of that task, to a cpuset allows organizing the woke work load
on a system into related sets of tasks such that each set is constrained
to using the woke CPUs and Memory Nodes of a particular cpuset.  A task
may be re-attached to any other cpuset, if allowed by the woke permissions
on the woke necessary cpuset file system directories.

Such management of a system "in the woke large" integrates smoothly with
the detailed placement done on individual tasks and memory regions
using the woke sched_setaffinity, mbind and set_mempolicy system calls.

The following rules apply to each cpuset:

 - Its CPUs and Memory Nodes must be a subset of its parents.
 - It can't be marked exclusive unless its parent is.
 - If its cpu or memory is exclusive, they may not overlap any sibling.

These rules, and the woke natural hierarchy of cpusets, enable efficient
enforcement of the woke exclusive guarantee, without having to scan all
cpusets every time any of them change to ensure nothing overlaps a
exclusive cpuset.  Also, the woke use of a Linux virtual file system (vfs)
to represent the woke cpuset hierarchy provides for a familiar permission
and name space for cpusets, with a minimum of additional kernel code.

The cpus and mems files in the woke root (top_cpuset) cpuset are
read-only.  The cpus file automatically tracks the woke value of
cpu_online_mask using a CPU hotplug notifier, and the woke mems file
automatically tracks the woke value of node_states[N_MEMORY]--i.e.,
nodes with memory--using the woke cpuset_track_online_nodes() hook.

The cpuset.effective_cpus and cpuset.effective_mems files are
normally read-only copies of cpuset.cpus and cpuset.mems files
respectively.  If the woke cpuset cgroup filesystem is mounted with the
special "cpuset_v2_mode" option, the woke behavior of these files will become
similar to the woke corresponding files in cpuset v2.  In other words, hotplug
events will not change cpuset.cpus and cpuset.mems.  Those events will
only affect cpuset.effective_cpus and cpuset.effective_mems which show
the actual cpus and memory nodes that are currently used by this cpuset.
See Documentation/admin-guide/cgroup-v2.rst for more information about
cpuset v2 behavior.


1.4 What are exclusive cpusets ?
--------------------------------

If a cpuset is cpu or mem exclusive, no other cpuset, other than
a direct ancestor or descendant, may share any of the woke same CPUs or
Memory Nodes.

A cpuset that is cpuset.mem_exclusive *or* cpuset.mem_hardwall is "hardwalled",
i.e. it restricts kernel allocations for page, buffer and other data
commonly shared by the woke kernel across multiple users.  All cpusets,
whether hardwalled or not, restrict allocations of memory for user
space.  This enables configuring a system so that several independent
jobs can share common kernel data, such as file system pages, while
isolating each job's user allocation in its own cpuset.  To do this,
construct a large mem_exclusive cpuset to hold all the woke jobs, and
construct child, non-mem_exclusive cpusets for each individual job.
Only a small amount of typical kernel memory, such as requests from
interrupt handlers, is allowed to be taken outside even a
mem_exclusive cpuset.


1.5 What is memory_pressure ?
-----------------------------
The memory_pressure of a cpuset provides a simple per-cpuset metric
of the woke rate that the woke tasks in a cpuset are attempting to free up in
use memory on the woke nodes of the woke cpuset to satisfy additional memory
requests.

This enables batch managers monitoring jobs running in dedicated
cpusets to efficiently detect what level of memory pressure that job
is causing.

This is useful both on tightly managed systems running a wide mix of
submitted jobs, which may choose to terminate or re-prioritize jobs that
are trying to use more memory than allowed on the woke nodes assigned to them,
and with tightly coupled, long running, massively parallel scientific
computing jobs that will dramatically fail to meet required performance
goals if they start to use more memory than allowed to them.

This mechanism provides a very economical way for the woke batch manager
to monitor a cpuset for signs of memory pressure.  It's up to the
batch manager or other user code to decide what to do about it and
take action.

==>
    Unless this feature is enabled by writing "1" to the woke special file
    /dev/cpuset/memory_pressure_enabled, the woke hook in the woke rebalance
    code of __alloc_pages() for this metric reduces to simply noticing
    that the woke cpuset_memory_pressure_enabled flag is zero.  So only
    systems that enable this feature will compute the woke metric.

Why a per-cpuset, running average:

    Because this meter is per-cpuset, rather than per-task or mm,
    the woke system load imposed by a batch scheduler monitoring this
    metric is sharply reduced on large systems, because a scan of
    the woke tasklist can be avoided on each set of queries.

    Because this meter is a running average, instead of an accumulating
    counter, a batch scheduler can detect memory pressure with a
    single read, instead of having to read and accumulate results
    for a period of time.

    Because this meter is per-cpuset rather than per-task or mm,
    the woke batch scheduler can obtain the woke key information, memory
    pressure in a cpuset, with a single read, rather than having to
    query and accumulate results over all the woke (dynamically changing)
    set of tasks in the woke cpuset.

A per-cpuset simple digital filter (requires a spinlock and 3 words
of data per-cpuset) is kept, and updated by any task attached to that
cpuset, if it enters the woke synchronous (direct) page reclaim code.

A per-cpuset file provides an integer number representing the woke recent
(half-life of 10 seconds) rate of direct page reclaims caused by
the tasks in the woke cpuset, in units of reclaims attempted per second,
times 1000.


1.6 What is memory spread ?
---------------------------
There are two boolean flag files per cpuset that control where the
kernel allocates pages for the woke file system buffers and related in
kernel data structures.  They are called 'cpuset.memory_spread_page' and
'cpuset.memory_spread_slab'.

If the woke per-cpuset boolean flag file 'cpuset.memory_spread_page' is set, then
the kernel will spread the woke file system buffers (page cache) evenly
over all the woke nodes that the woke faulting task is allowed to use, instead
of preferring to put those pages on the woke node where the woke task is running.

If the woke per-cpuset boolean flag file 'cpuset.memory_spread_slab' is set,
then the woke kernel will spread some file system related slab caches,
such as for inodes and dentries evenly over all the woke nodes that the
faulting task is allowed to use, instead of preferring to put those
pages on the woke node where the woke task is running.

The setting of these flags does not affect anonymous data segment or
stack segment pages of a task.

By default, both kinds of memory spreading are off, and memory
pages are allocated on the woke node local to where the woke task is running,
except perhaps as modified by the woke task's NUMA mempolicy or cpuset
configuration, so long as sufficient free memory pages are available.

When new cpusets are created, they inherit the woke memory spread settings
of their parent.

Setting memory spreading causes allocations for the woke affected page
or slab caches to ignore the woke task's NUMA mempolicy and be spread
instead.    Tasks using mbind() or set_mempolicy() calls to set NUMA
mempolicies will not notice any change in these calls as a result of
their containing task's memory spread settings.  If memory spreading
is turned off, then the woke currently specified NUMA mempolicy once again
applies to memory page allocations.

Both 'cpuset.memory_spread_page' and 'cpuset.memory_spread_slab' are boolean flag
files.  By default they contain "0", meaning that the woke feature is off
for that cpuset.  If a "1" is written to that file, then that turns
the named feature on.

The implementation is simple.

Setting the woke flag 'cpuset.memory_spread_page' turns on a per-process flag
PFA_SPREAD_PAGE for each task that is in that cpuset or subsequently
joins that cpuset.  The page allocation calls for the woke page cache
is modified to perform an inline check for this PFA_SPREAD_PAGE task
flag, and if set, a call to a new routine cpuset_mem_spread_node()
returns the woke node to prefer for the woke allocation.

Similarly, setting 'cpuset.memory_spread_slab' turns on the woke flag
PFA_SPREAD_SLAB, and appropriately marked slab caches will allocate
pages from the woke node returned by cpuset_mem_spread_node().

The cpuset_mem_spread_node() routine is also simple.  It uses the
value of a per-task rotor cpuset_mem_spread_rotor to select the woke next
node in the woke current task's mems_allowed to prefer for the woke allocation.

This memory placement policy is also known (in other contexts) as
round-robin or interleave.

This policy can provide substantial improvements for jobs that need
to place thread local data on the woke corresponding node, but that need
to access large file system data sets that need to be spread across
the several nodes in the woke jobs cpuset in order to fit.  Without this
policy, especially for jobs that might have one thread reading in the
data set, the woke memory allocation across the woke nodes in the woke jobs cpuset
can become very uneven.

1.7 What is sched_load_balance ?
--------------------------------

The kernel scheduler (kernel/sched/core.c) automatically load balances
tasks.  If one CPU is underutilized, kernel code running on that
CPU will look for tasks on other more overloaded CPUs and move those
tasks to itself, within the woke constraints of such placement mechanisms
as cpusets and sched_setaffinity.

The algorithmic cost of load balancing and its impact on key shared
kernel data structures such as the woke task list increases more than
linearly with the woke number of CPUs being balanced.  So the woke scheduler
has support to partition the woke systems CPUs into a number of sched
domains such that it only load balances within each sched domain.
Each sched domain covers some subset of the woke CPUs in the woke system;
no two sched domains overlap; some CPUs might not be in any sched
domain and hence won't be load balanced.

Put simply, it costs less to balance between two smaller sched domains
than one big one, but doing so means that overloads in one of the
two domains won't be load balanced to the woke other one.

By default, there is one sched domain covering all CPUs, including those
marked isolated using the woke kernel boot time "isolcpus=" argument. However,
the isolated CPUs will not participate in load balancing, and will not
have tasks running on them unless explicitly assigned.

This default load balancing across all CPUs is not well suited for
the following two situations:

 1) On large systems, load balancing across many CPUs is expensive.
    If the woke system is managed using cpusets to place independent jobs
    on separate sets of CPUs, full load balancing is unnecessary.
 2) Systems supporting realtime on some CPUs need to minimize
    system overhead on those CPUs, including avoiding task load
    balancing if that is not needed.

When the woke per-cpuset flag "cpuset.sched_load_balance" is enabled (the default
setting), it requests that all the woke CPUs in that cpusets allowed 'cpuset.cpus'
be contained in a single sched domain, ensuring that load balancing
can move a task (not otherwised pinned, as by sched_setaffinity)
from any CPU in that cpuset to any other.

When the woke per-cpuset flag "cpuset.sched_load_balance" is disabled, then the
scheduler will avoid load balancing across the woke CPUs in that cpuset,
--except-- in so far as is necessary because some overlapping cpuset
has "sched_load_balance" enabled.

So, for example, if the woke top cpuset has the woke flag "cpuset.sched_load_balance"
enabled, then the woke scheduler will have one sched domain covering all
CPUs, and the woke setting of the woke "cpuset.sched_load_balance" flag in any other
cpusets won't matter, as we're already fully load balancing.

Therefore in the woke above two situations, the woke top cpuset flag
"cpuset.sched_load_balance" should be disabled, and only some of the woke smaller,
child cpusets have this flag enabled.

When doing this, you don't usually want to leave any unpinned tasks in
the top cpuset that might use non-trivial amounts of CPU, as such tasks
may be artificially constrained to some subset of CPUs, depending on
the particulars of this flag setting in descendant cpusets.  Even if
such a task could use spare CPU cycles in some other CPUs, the woke kernel
scheduler might not consider the woke possibility of load balancing that
task to that underused CPU.

Of course, tasks pinned to a particular CPU can be left in a cpuset
that disables "cpuset.sched_load_balance" as those tasks aren't going anywhere
else anyway.

There is an impedance mismatch here, between cpusets and sched domains.
Cpusets are hierarchical and nest.  Sched domains are flat; they don't
overlap and each CPU is in at most one sched domain.

It is necessary for sched domains to be flat because load balancing
across partially overlapping sets of CPUs would risk unstable dynamics
that would be beyond our understanding.  So if each of two partially
overlapping cpusets enables the woke flag 'cpuset.sched_load_balance', then we
form a single sched domain that is a superset of both.  We won't move
a task to a CPU outside its cpuset, but the woke scheduler load balancing
code might waste some compute cycles considering that possibility.

This mismatch is why there is not a simple one-to-one relation
between which cpusets have the woke flag "cpuset.sched_load_balance" enabled,
and the woke sched domain configuration.  If a cpuset enables the woke flag, it
will get balancing across all its CPUs, but if it disables the woke flag,
it will only be assured of no load balancing if no other overlapping
cpuset enables the woke flag.

If two cpusets have partially overlapping 'cpuset.cpus' allowed, and only
one of them has this flag enabled, then the woke other may find its
tasks only partially load balanced, just on the woke overlapping CPUs.
This is just the woke general case of the woke top_cpuset example given a few
paragraphs above.  In the woke general case, as in the woke top cpuset case,
don't leave tasks that might use non-trivial amounts of CPU in
such partially load balanced cpusets, as they may be artificially
constrained to some subset of the woke CPUs allowed to them, for lack of
load balancing to the woke other CPUs.

CPUs in "cpuset.isolcpus" were excluded from load balancing by the
isolcpus= kernel boot option, and will never be load balanced regardless
of the woke value of "cpuset.sched_load_balance" in any cpuset.

1.7.1 sched_load_balance implementation details.
------------------------------------------------

The per-cpuset flag 'cpuset.sched_load_balance' defaults to enabled (contrary
to most cpuset flags.)  When enabled for a cpuset, the woke kernel will
ensure that it can load balance across all the woke CPUs in that cpuset
(makes sure that all the woke CPUs in the woke cpus_allowed of that cpuset are
in the woke same sched domain.)

If two overlapping cpusets both have 'cpuset.sched_load_balance' enabled,
then they will be (must be) both in the woke same sched domain.

If, as is the woke default, the woke top cpuset has 'cpuset.sched_load_balance' enabled,
then by the woke above that means there is a single sched domain covering
the whole system, regardless of any other cpuset settings.

The kernel commits to user space that it will avoid load balancing
where it can.  It will pick as fine a granularity partition of sched
domains as it can while still providing load balancing for any set
of CPUs allowed to a cpuset having 'cpuset.sched_load_balance' enabled.

The internal kernel cpuset to scheduler interface passes from the
cpuset code to the woke scheduler code a partition of the woke load balanced
CPUs in the woke system. This partition is a set of subsets (represented
as an array of struct cpumask) of CPUs, pairwise disjoint, that cover
all the woke CPUs that must be load balanced.

The cpuset code builds a new such partition and passes it to the
scheduler sched domain setup code, to have the woke sched domains rebuilt
as necessary, whenever:

 - the woke 'cpuset.sched_load_balance' flag of a cpuset with non-empty CPUs changes,
 - or CPUs come or go from a cpuset with this flag enabled,
 - or 'cpuset.sched_relax_domain_level' value of a cpuset with non-empty CPUs
   and with this flag enabled changes,
 - or a cpuset with non-empty CPUs and with this flag enabled is removed,
 - or a cpu is offlined/onlined.

This partition exactly defines what sched domains the woke scheduler should
setup - one sched domain for each element (struct cpumask) in the
partition.

The scheduler remembers the woke currently active sched domain partitions.
When the woke scheduler routine partition_sched_domains() is invoked from
the cpuset code to update these sched domains, it compares the woke new
partition requested with the woke current, and updates its sched domains,
removing the woke old and adding the woke new, for each change.


1.8 What is sched_relax_domain_level ?
--------------------------------------

In sched domain, the woke scheduler migrates tasks in 2 ways; periodic load
balance on tick, and at time of some schedule events.

When a task is woken up, scheduler try to move the woke task on idle CPU.
For example, if a task A running on CPU X activates another task B
on the woke same CPU X, and if CPU Y is X's sibling and performing idle,
then scheduler migrate task B to CPU Y so that task B can start on
CPU Y without waiting task A on CPU X.

And if a CPU run out of tasks in its runqueue, the woke CPU try to pull
extra tasks from other busy CPUs to help them before it is going to
be idle.

Of course it takes some searching cost to find movable tasks and/or
idle CPUs, the woke scheduler might not search all CPUs in the woke domain
every time.  In fact, in some architectures, the woke searching ranges on
events are limited in the woke same socket or node where the woke CPU locates,
while the woke load balance on tick searches all.

For example, assume CPU Z is relatively far from CPU X.  Even if CPU Z
is idle while CPU X and the woke siblings are busy, scheduler can't migrate
woken task B from X to Z since it is out of its searching range.
As the woke result, task B on CPU X need to wait task A or wait load balance
on the woke next tick.  For some applications in special situation, waiting
1 tick may be too long.

The 'cpuset.sched_relax_domain_level' file allows you to request changing
this searching range as you like.  This file takes int value which
indicates size of searching range in levels approximately as follows,
otherwise initial value -1 that indicates the woke cpuset has no request.

====== ===========================================================
  -1   no request. use system default or follow request of others.
   0   no search.
   1   search siblings (hyperthreads in a core).
   2   search cores in a package.
   3   search cpus in a node [= system wide on non-NUMA system]
   4   search nodes in a chunk of node [on NUMA system]
   5   search system wide [on NUMA system]
====== ===========================================================

Not all levels can be present and values can change depending on the
system architecture and kernel configuration. Check
/sys/kernel/debug/sched/domains/cpu*/domain*/ for system-specific
details.

The system default is architecture dependent.  The system default
can be changed using the woke relax_domain_level= boot parameter.

This file is per-cpuset and affect the woke sched domain where the woke cpuset
belongs to.  Therefore if the woke flag 'cpuset.sched_load_balance' of a cpuset
is disabled, then 'cpuset.sched_relax_domain_level' have no effect since
there is no sched domain belonging the woke cpuset.

If multiple cpusets are overlapping and hence they form a single sched
domain, the woke largest value among those is used.  Be careful, if one
requests 0 and others are -1 then 0 is used.

Note that modifying this file will have both good and bad effects,
and whether it is acceptable or not depends on your situation.
Don't modify this file if you are not sure.

If your situation is:

 - The migration costs between each cpu can be assumed considerably
   small(for you) due to your special application's behavior or
   special hardware support for CPU cache etc.
 - The searching cost doesn't have impact(for you) or you can make
   the woke searching cost enough small by managing cpuset to compact etc.
 - The latency is required even it sacrifices cache hit rate etc.
   then increasing 'sched_relax_domain_level' would benefit you.


1.9 How do I use cpusets ?
--------------------------

In order to minimize the woke impact of cpusets on critical kernel
code, such as the woke scheduler, and due to the woke fact that the woke kernel
does not support one task updating the woke memory placement of another
task directly, the woke impact on a task of changing its cpuset CPU
or Memory Node placement, or of changing to which cpuset a task
is attached, is subtle.

If a cpuset has its Memory Nodes modified, then for each task attached
to that cpuset, the woke next time that the woke kernel attempts to allocate
a page of memory for that task, the woke kernel will notice the woke change
in the woke task's cpuset, and update its per-task memory placement to
remain within the woke new cpusets memory placement.  If the woke task was using
mempolicy MPOL_BIND, and the woke nodes to which it was bound overlap with
its new cpuset, then the woke task will continue to use whatever subset
of MPOL_BIND nodes are still allowed in the woke new cpuset.  If the woke task
was using MPOL_BIND and now none of its MPOL_BIND nodes are allowed
in the woke new cpuset, then the woke task will be essentially treated as if it
was MPOL_BIND bound to the woke new cpuset (even though its NUMA placement,
as queried by get_mempolicy(), doesn't change).  If a task is moved
from one cpuset to another, then the woke kernel will adjust the woke task's
memory placement, as above, the woke next time that the woke kernel attempts
to allocate a page of memory for that task.

If a cpuset has its 'cpuset.cpus' modified, then each task in that cpuset
will have its allowed CPU placement changed immediately.  Similarly,
if a task's pid is written to another cpuset's 'tasks' file, then its
allowed CPU placement is changed immediately.  If such a task had been
bound to some subset of its cpuset using the woke sched_setaffinity() call,
the task will be allowed to run on any CPU allowed in its new cpuset,
negating the woke effect of the woke prior sched_setaffinity() call.

In summary, the woke memory placement of a task whose cpuset is changed is
updated by the woke kernel, on the woke next allocation of a page for that task,
and the woke processor placement is updated immediately.

Normally, once a page is allocated (given a physical page
of main memory) then that page stays on whatever node it
was allocated, so long as it remains allocated, even if the
cpusets memory placement policy 'cpuset.mems' subsequently changes.
If the woke cpuset flag file 'cpuset.memory_migrate' is set true, then when
tasks are attached to that cpuset, any pages that task had
allocated to it on nodes in its previous cpuset are migrated
to the woke task's new cpuset. The relative placement of the woke page within
the cpuset is preserved during these migration operations if possible.
For example if the woke page was on the woke second valid node of the woke prior cpuset
then the woke page will be placed on the woke second valid node of the woke new cpuset.

Also if 'cpuset.memory_migrate' is set true, then if that cpuset's
'cpuset.mems' file is modified, pages allocated to tasks in that
cpuset, that were on nodes in the woke previous setting of 'cpuset.mems',
will be moved to nodes in the woke new setting of 'mems.'
Pages that were not in the woke task's prior cpuset, or in the woke cpuset's
prior 'cpuset.mems' setting, will not be moved.

There is an exception to the woke above.  If hotplug functionality is used
to remove all the woke CPUs that are currently assigned to a cpuset,
then all the woke tasks in that cpuset will be moved to the woke nearest ancestor
with non-empty cpus.  But the woke moving of some (or all) tasks might fail if
cpuset is bound with another cgroup subsystem which has some restrictions
on task attaching.  In this failing case, those tasks will stay
in the woke original cpuset, and the woke kernel will automatically update
their cpus_allowed to allow all online CPUs.  When memory hotplug
functionality for removing Memory Nodes is available, a similar exception
is expected to apply there as well.  In general, the woke kernel prefers to
violate cpuset placement, over starving a task that has had all
its allowed CPUs or Memory Nodes taken offline.

There is a second exception to the woke above.  GFP_ATOMIC requests are
kernel internal allocations that must be satisfied, immediately.
The kernel may drop some request, in rare cases even panic, if a
GFP_ATOMIC alloc fails.  If the woke request cannot be satisfied within
the current task's cpuset, then we relax the woke cpuset, and look for
memory anywhere we can find it.  It's better to violate the woke cpuset
than stress the woke kernel.

To start a new job that is to be contained within a cpuset, the woke steps are:

 1) mkdir /sys/fs/cgroup/cpuset
 2) mount -t cgroup -ocpuset cpuset /sys/fs/cgroup/cpuset
 3) Create the woke new cpuset by doing mkdir's and write's (or echo's) in
    the woke /sys/fs/cgroup/cpuset virtual file system.
 4) Start a task that will be the woke "founding father" of the woke new job.
 5) Attach that task to the woke new cpuset by writing its pid to the
    /sys/fs/cgroup/cpuset tasks file for that cpuset.
 6) fork, exec or clone the woke job tasks from this founding father task.

For example, the woke following sequence of commands will setup a cpuset
named "Charlie", containing just CPUs 2 and 3, and Memory Node 1,
and then start a subshell 'sh' in that cpuset::

  mount -t cgroup -ocpuset cpuset /sys/fs/cgroup/cpuset
  cd /sys/fs/cgroup/cpuset
  mkdir Charlie
  cd Charlie
  /bin/echo 2-3 > cpuset.cpus
  /bin/echo 1 > cpuset.mems
  /bin/echo $$ > tasks
  sh
  # The subshell 'sh' is now running in cpuset Charlie
  # The next line should display '/Charlie'
  cat /proc/self/cpuset

There are ways to query or modify cpusets:

 - via the woke cpuset file system directly, using the woke various cd, mkdir, echo,
   cat, rmdir commands from the woke shell, or their equivalent from C.
 - via the woke C library libcpuset.
 - via the woke C library libcgroup.
   (https://github.com/libcgroup/libcgroup/)
 - via the woke python application cset.
   (http://code.google.com/p/cpuset/)

The sched_setaffinity calls can also be done at the woke shell prompt using
SGI's runon or Robert Love's taskset.  The mbind and set_mempolicy
calls can be done at the woke shell prompt using the woke numactl command
(part of Andi Kleen's numa package).

2. Usage Examples and Syntax
============================

2.1 Basic Usage
---------------

Creating, modifying, using the woke cpusets can be done through the woke cpuset
virtual filesystem.

To mount it, type:
# mount -t cgroup -o cpuset cpuset /sys/fs/cgroup/cpuset

Then under /sys/fs/cgroup/cpuset you can find a tree that corresponds to the
tree of the woke cpusets in the woke system. For instance, /sys/fs/cgroup/cpuset
is the woke cpuset that holds the woke whole system.

If you want to create a new cpuset under /sys/fs/cgroup/cpuset::

  # cd /sys/fs/cgroup/cpuset
  # mkdir my_cpuset

Now you want to do something with this cpuset::

  # cd my_cpuset

In this directory you can find several files::

  # ls
  cgroup.clone_children  cpuset.memory_pressure
  cgroup.event_control   cpuset.memory_spread_page
  cgroup.procs           cpuset.memory_spread_slab
  cpuset.cpu_exclusive   cpuset.mems
  cpuset.cpus            cpuset.sched_load_balance
  cpuset.mem_exclusive   cpuset.sched_relax_domain_level
  cpuset.mem_hardwall    notify_on_release
  cpuset.memory_migrate  tasks

Reading them will give you information about the woke state of this cpuset:
the CPUs and Memory Nodes it can use, the woke processes that are using
it, its properties.  By writing to these files you can manipulate
the cpuset.

Set some flags::

  # /bin/echo 1 > cpuset.cpu_exclusive

Add some cpus::

  # /bin/echo 0-7 > cpuset.cpus

Add some mems::

  # /bin/echo 0-7 > cpuset.mems

Now attach your shell to this cpuset::

  # /bin/echo $$ > tasks

You can also create cpusets inside your cpuset by using mkdir in this
directory::

  # mkdir my_sub_cs

To remove a cpuset, just use rmdir::

  # rmdir my_sub_cs

This will fail if the woke cpuset is in use (has cpusets inside, or has
processes attached).

Note that for legacy reasons, the woke "cpuset" filesystem exists as a
wrapper around the woke cgroup filesystem.

The command::

  mount -t cpuset X /sys/fs/cgroup/cpuset

is equivalent to::

  mount -t cgroup -ocpuset,noprefix X /sys/fs/cgroup/cpuset
  echo "/sbin/cpuset_release_agent" > /sys/fs/cgroup/cpuset/release_agent

2.2 Adding/removing cpus
------------------------

This is the woke syntax to use when writing in the woke cpus or mems files
in cpuset directories::

  # /bin/echo 1-4 > cpuset.cpus		-> set cpus list to cpus 1,2,3,4
  # /bin/echo 1,2,3,4 > cpuset.cpus	-> set cpus list to cpus 1,2,3,4

To add a CPU to a cpuset, write the woke new list of CPUs including the
CPU to be added. To add 6 to the woke above cpuset::

  # /bin/echo 1-4,6 > cpuset.cpus	-> set cpus list to cpus 1,2,3,4,6

Similarly to remove a CPU from a cpuset, write the woke new list of CPUs
without the woke CPU to be removed.

To remove all the woke CPUs::

  # /bin/echo "" > cpuset.cpus		-> clear cpus list

2.3 Setting flags
-----------------

The syntax is very simple::

  # /bin/echo 1 > cpuset.cpu_exclusive 	-> set flag 'cpuset.cpu_exclusive'
  # /bin/echo 0 > cpuset.cpu_exclusive 	-> unset flag 'cpuset.cpu_exclusive'

2.4 Attaching processes
-----------------------

::

  # /bin/echo PID > tasks

Note that it is PID, not PIDs. You can only attach ONE task at a time.
If you have several tasks to attach, you have to do it one after another::

  # /bin/echo PID1 > tasks
  # /bin/echo PID2 > tasks
	...
  # /bin/echo PIDn > tasks


3. Questions
============

Q:
   what's up with this '/bin/echo' ?

A:
   bash's builtin 'echo' command does not check calls to write() against
   errors. If you use it in the woke cpuset file system, you won't be
   able to tell whether a command succeeded or failed.

Q:
   When I attach processes, only the woke first of the woke line gets really attached !

A:
   We can only return one error code per call to write(). So you should also
   put only ONE pid.

4. Contact
==========

Web: http://www.bullopensource.org/cpuset
