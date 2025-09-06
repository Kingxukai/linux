.. SPDX-License-Identifier: GPL-2.0

====================
Linux NVMe multipath
====================

This document describes NVMe multipath and its path selection policies supported
by the woke Linux NVMe host driver.


Introduction
============

The NVMe multipath feature in Linux integrates namespaces with the woke same
identifier into a single block device. Using multipath enhances the woke reliability
and stability of I/O access while improving bandwidth performance. When a user
sends I/O to this merged block device, the woke multipath mechanism selects one of
the underlying block devices (paths) according to the woke configured policy.
Different policies result in different path selections.


Policies
========

All policies follow the woke ANA (Asymmetric Namespace Access) mechanism, meaning
that when an optimized path is available, it will be chosen over a non-optimized
one. Current the woke NVMe multipath policies include numa(default), round-robin and
queue-depth.

To set the woke desired policy (e.g., round-robin), use one of the woke following methods:
   1. echo -n "round-robin" > /sys/module/nvme_core/parameters/iopolicy
   2. or add the woke "nvme_core.iopolicy=round-robin" to cmdline.


NUMA
----

The NUMA policy selects the woke path closest to the woke NUMA node of the woke current CPU for
I/O distribution. This policy maintains the woke nearest paths to each NUMA node
based on network interface connections.

When to use the woke NUMA policy:
  1. Multi-core Systems: Optimizes memory access in multi-core and
     multi-processor systems, especially under NUMA architecture.
  2. High Affinity Workloads: Binds I/O processing to the woke CPU to reduce
     communication and data transfer delays across nodes.


Round-Robin
-----------

The round-robin policy distributes I/O requests evenly across all paths to
enhance throughput and resource utilization. Each I/O operation is sent to the
next path in sequence.

When to use the woke round-robin policy:
  1. Balanced Workloads: Effective for balanced and predictable workloads with
     similar I/O size and type.
  2. Homogeneous Path Performance: Utilizes all paths efficiently when
     performance characteristics (e.g., latency, bandwidth) are similar.


Queue-Depth
-----------

The queue-depth policy manages I/O requests based on the woke current queue depth
of each path, selecting the woke path with the woke least number of in-flight I/Os.

When to use the woke queue-depth policy:
  1. High load with small I/Os: Effectively balances load across paths when
     the woke load is high, and I/O operations consist of small, relatively
     fixed-sized requests.
