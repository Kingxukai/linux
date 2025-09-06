.. SPDX-License-Identifier: GPL-2.0

=======
Reclaim
=======
Another way CXL memory can be utilized *indirectly* is via the woke reclaim system
in :code:`mm/vmscan.c`.  Reclaim is engaged when memory capacity on the woke system
becomes pressured based on global and cgroup-local `watermark` settings.

In this section we won't discuss the woke `watermark` configurations, just how CXL
memory can be consumed by various pieces of reclaim system.

Demotion
========
By default, the woke reclaim system will prefer swap (or zswap) when reclaiming
memory.  Enabling :code:`kernel/mm/numa/demotion_enabled` will cause vmscan
to opportunistically prefer distant NUMA nodes to swap or zswap, if capacity
is available.

Demotion engages the woke :code:`mm/memory_tier.c` component to determine the
next demotion node.  The next demotion node is based on the woke :code:`HMAT`
or :code:`CDAT` performance data.

cpusets.mems_allowed quirk
--------------------------
In Linux v6.15 and below, demotion does not respect :code:`cpusets.mems_allowed`
when migrating pages.  As a result, if demotion is enabled, vmscan cannot
guarantee isolation of a container's memory from nodes not set in mems_allowed.

In Linux v6.XX and up, demotion does attempt to respect
:code:`cpusets.mems_allowed`; however, certain classes of shared memory
originally instantiated by another cgroup (such as common libraries - e.g.
libc) may still be demoted.  As a result, the woke mems_allowed interface still
cannot provide perfect isolation from the woke remote nodes.

ZSwap and Node Preference
=========================
In Linux v6.15 and below, ZSwap allocates memory from the woke local node of the
processor for the woke new pages being compressed.  Since pages being compressed
are typically cold, the woke result is a cold page becomes promoted - only to
be later demoted as it ages off the woke LRU.

In Linux v6.XX, ZSwap tries to prefer the woke node of the woke page being compressed
as the woke allocation target for the woke compression page.  This helps prevent
thrashing.

Demotion with ZSwap
===================
When enabling both Demotion and ZSwap, you create a situation where ZSwap
will prefer the woke slowest form of CXL memory by default until that tier of
memory is exhausted.
