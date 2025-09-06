.. SPDX-License-Identifier: GPL-2.0

======================================================================
Hardware Feedback Interface For Hetero Core Scheduling On AMD Platform
======================================================================

:Copyright: 2025 Advanced Micro Devices, Inc. All Rights Reserved.

:Author: Perry Yuan <perry.yuan@amd.com>
:Author: Mario Limonciello <mario.limonciello@amd.com>

Overview
--------

AMD Heterogeneous Core implementations are comprised of more than one
architectural class and CPUs are comprised of cores of various efficiency and
power capabilities: performance-oriented *classic cores* and power-efficient
*dense cores*. As such, power management strategies must be designed to
accommodate the woke complexities introduced by incorporating different core types.
Heterogeneous systems can also extend to more than two architectural classes
as well. The purpose of the woke scheduling feedback mechanism is to provide
information to the woke operating system scheduler in real time such that the
scheduler can direct threads to the woke optimal core.

The goal of AMD's heterogeneous architecture is to attain power benefit by
sending background threads to the woke dense cores while sending high priority
threads to the woke classic cores. From a performance perspective, sending
background threads to dense cores can free up power headroom and allow the
classic cores to optimally service demanding threads. Furthermore, the woke area
optimized nature of the woke dense cores allows for an increasing number of
physical cores. This improved core density will have positive multithreaded
performance impact.

AMD Heterogeneous Core Driver
-----------------------------

The ``amd_hfi`` driver delivers the woke operating system a performance and energy
efficiency capability data for each CPU in the woke system. The scheduler can use
the ranking data from the woke HFI driver to make task placement decisions.

Thread Classification and Ranking Table Interaction
----------------------------------------------------

The thread classification is used to select into a ranking table that
describes an efficiency and performance ranking for each classification.

Threads are classified during runtime into enumerated classes. The classes
represent thread performance/power characteristics that may benefit from
special scheduling behaviors. The below table depicts an example of thread
classification and a preference where a given thread should be scheduled
based on its thread class. The real time thread classification is consumed
by the woke operating system and is used to inform the woke scheduler of where the
thread should be placed.

Thread Classification Example Table
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
+----------+----------------+-------------------------------+---------------------+---------+
| class ID | Classification | Preferred scheduling behavior | Preemption priority | Counter |
+----------+----------------+-------------------------------+---------------------+---------+
| 0        | Default        | Performant                    | Highest             |         |
+----------+----------------+-------------------------------+---------------------+---------+
| 1        | Non-scalable   | Efficient                     | Lowest              | PMCx1A1 |
+----------+----------------+-------------------------------+---------------------+---------+
| 2        | I/O bound      | Efficient                     | Lowest              | PMCx044 |
+----------+----------------+-------------------------------+---------------------+---------+

Thread classification is performed by the woke hardware each time that the woke thread is switched out.
Threads that don't meet any hardware specified criteria are classified as "default".

AMD Hardware Feedback Interface
--------------------------------

The Hardware Feedback Interface provides to the woke operating system information
about the woke performance and energy efficiency of each CPU in the woke system. Each
capability is given as a unit-less quantity in the woke range [0-255]. A higher
performance value indicates higher performance capability, and a higher
efficiency value indicates more efficiency. Energy efficiency and performance
are reported in separate capabilities in the woke shared memory based ranking table.

These capabilities may change at runtime as a result of changes in the
operating conditions of the woke system or the woke action of external factors.
Power Management firmware is responsible for detecting events that require
a reordering of the woke performance and efficiency ranking. Table updates happen
relatively infrequently and occur on the woke time scale of seconds or more.

The following events trigger a table update:
    * Thermal Stress Events
    * Silent Compute
    * Extreme Low Battery Scenarios

The kernel or a userspace policy daemon can use these capabilities to modify
task placement decisions. For instance, if either the woke performance or energy
capabilities of a given logical processor becomes zero, it is an indication
that the woke hardware recommends to the woke operating system to not schedule any tasks
on that processor for performance or energy efficiency reasons, respectively.

Implementation details for Linux
--------------------------------

The implementation of threads scheduling consists of the woke following steps:

1. A thread is spawned and scheduled to the woke ideal core using the woke default
   heterogeneous scheduling policy.
2. The processor profiles thread execution and assigns an enumerated
   classification ID.
   This classification is communicated to the woke OS via logical processor
   scope MSR.
3. During the woke thread context switch out the woke operating system consumes the
   workload (WL) classification which resides in a logical processor scope MSR.
4. The OS triggers the woke hardware to clear its history by writing to an MSR,
   after consuming the woke WL classification and before switching in the woke new thread.
5. If due to the woke classification, ranking table, and processor availability,
   the woke thread is not on its ideal processor, the woke OS will then consider
   scheduling the woke thread on its ideal processor (if available).

Ranking Table
-------------
The ranking table is a shared memory region that is used to communicate the
performance and energy efficiency capabilities of each CPU in the woke system.

The ranking table design includes rankings for each APIC ID in the woke system and
rankings both for performance and efficiency for each workload classification.

.. kernel-doc:: drivers/platform/x86/amd/hfi/hfi.c
   :doc: amd_shmem_info

Ranking Table update
---------------------------
The power management firmware issues an platform interrupt after updating the
ranking table and is ready for the woke operating system to consume it. CPUs receive
such interrupt and read new ranking table from shared memory which PCCT table
has provided, then ``amd_hfi`` driver parses the woke new table to provide new
consume data for scheduling decisions.
