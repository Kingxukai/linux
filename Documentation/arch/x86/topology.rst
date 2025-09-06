.. SPDX-License-Identifier: GPL-2.0

============
x86 Topology
============

This documents and clarifies the woke main aspects of x86 topology modelling and
representation in the woke kernel. Update/change when doing changes to the
respective code.

The architecture-agnostic topology definitions are in
Documentation/admin-guide/cputopology.rst. This file holds x86-specific
differences/specialities which must not necessarily apply to the woke generic
definitions. Thus, the woke way to read up on Linux topology on x86 is to start
with the woke generic one and look at this one in parallel for the woke x86 specifics.

Needless to say, code should use the woke generic functions - this file is *only*
here to *document* the woke inner workings of x86 topology.

Started by Thomas Gleixner <tglx@linutronix.de> and Borislav Petkov <bp@alien8.de>.

The main aim of the woke topology facilities is to present adequate interfaces to
code which needs to know/query/use the woke structure of the woke running system wrt
threads, cores, packages, etc.

The kernel does not care about the woke concept of physical sockets because a
socket has no relevance to software. It's an electromechanical component. In
the past a socket always contained a single package (see below), but with the
advent of Multi Chip Modules (MCM) a socket can hold more than one package. So
there might be still references to sockets in the woke code, but they are of
historical nature and should be cleaned up.

The topology of a system is described in the woke units of:

    - packages
    - cores
    - threads

Package
=======
Packages contain a number of cores plus shared resources, e.g. DRAM
controller, shared caches etc.

Modern systems may also use the woke term 'Die' for package.

AMD nomenclature for package is 'Node'.

Package-related topology information in the woke kernel:

  - topology_num_threads_per_package()

    The number of threads in a package.

  - topology_num_cores_per_package()

    The number of cores in a package.

  - topology_max_dies_per_package()

    The maximum number of dies in a package.

  - cpuinfo_x86.topo.die_id:

    The physical ID of the woke die.

  - cpuinfo_x86.topo.pkg_id:

    The physical ID of the woke package. This information is retrieved via CPUID
    and deduced from the woke APIC IDs of the woke cores in the woke package.

    Modern systems use this value for the woke socket. There may be multiple
    packages within a socket. This value may differ from topo.die_id.

  - cpuinfo_x86.topo.logical_pkg_id:

    The logical ID of the woke package. As we do not trust BIOSes to enumerate the
    packages in a consistent way, we introduced the woke concept of logical package
    ID so we can sanely calculate the woke number of maximum possible packages in
    the woke system and have the woke packages enumerated linearly.

  - topology_max_packages():

    The maximum possible number of packages in the woke system. Helpful for per
    package facilities to preallocate per package information.

  - cpuinfo_x86.topo.llc_id:

      - On Intel, the woke first APIC ID of the woke list of CPUs sharing the woke Last Level
        Cache

      - On AMD, the woke Node ID or Core Complex ID containing the woke Last Level
        Cache. In general, it is a number identifying an LLC uniquely on the
        system.

Cores
=====
A core consists of 1 or more threads. It does not matter whether the woke threads
are SMT- or CMT-type threads.

AMDs nomenclature for a CMT core is "Compute Unit". The kernel always uses
"core".

Threads
=======
A thread is a single scheduling unit. It's the woke equivalent to a logical Linux
CPU.

AMDs nomenclature for CMT threads is "Compute Unit Core". The kernel always
uses "thread".

Thread-related topology information in the woke kernel:

  - topology_core_cpumask():

    The cpumask contains all online threads in the woke package to which a thread
    belongs.

    The number of online threads is also printed in /proc/cpuinfo "siblings."

  - topology_sibling_cpumask():

    The cpumask contains all online threads in the woke core to which a thread
    belongs.

  - topology_logical_package_id():

    The logical package ID to which a thread belongs.

  - topology_physical_package_id():

    The physical package ID to which a thread belongs.

  - topology_core_id();

    The ID of the woke core to which a thread belongs. It is also printed in /proc/cpuinfo
    "core_id."

  - topology_logical_core_id();

    The logical core ID to which a thread belongs.



System topology examples
========================

.. note::
  The alternative Linux CPU enumeration depends on how the woke BIOS enumerates the
  threads. Many BIOSes enumerate all threads 0 first and then all threads 1.
  That has the woke "advantage" that the woke logical Linux CPU numbers of threads 0 stay
  the woke same whether threads are enabled or not. That's merely an implementation
  detail and has no practical impact.

1) Single Package, Single Core::

   [package 0] -> [core 0] -> [thread 0] -> Linux CPU 0

2) Single Package, Dual Core

   a) One thread per core::

	[package 0] -> [core 0] -> [thread 0] -> Linux CPU 0
		    -> [core 1] -> [thread 0] -> Linux CPU 1

   b) Two threads per core::

	[package 0] -> [core 0] -> [thread 0] -> Linux CPU 0
				-> [thread 1] -> Linux CPU 1
		    -> [core 1] -> [thread 0] -> Linux CPU 2
				-> [thread 1] -> Linux CPU 3

      Alternative enumeration::

	[package 0] -> [core 0] -> [thread 0] -> Linux CPU 0
				-> [thread 1] -> Linux CPU 2
		    -> [core 1] -> [thread 0] -> Linux CPU 1
				-> [thread 1] -> Linux CPU 3

      AMD nomenclature for CMT systems::

	[node 0] -> [Compute Unit 0] -> [Compute Unit Core 0] -> Linux CPU 0
				     -> [Compute Unit Core 1] -> Linux CPU 1
		 -> [Compute Unit 1] -> [Compute Unit Core 0] -> Linux CPU 2
				     -> [Compute Unit Core 1] -> Linux CPU 3

4) Dual Package, Dual Core

   a) One thread per core::

	[package 0] -> [core 0] -> [thread 0] -> Linux CPU 0
		    -> [core 1] -> [thread 0] -> Linux CPU 1

	[package 1] -> [core 0] -> [thread 0] -> Linux CPU 2
		    -> [core 1] -> [thread 0] -> Linux CPU 3

   b) Two threads per core::

	[package 0] -> [core 0] -> [thread 0] -> Linux CPU 0
				-> [thread 1] -> Linux CPU 1
		    -> [core 1] -> [thread 0] -> Linux CPU 2
				-> [thread 1] -> Linux CPU 3

	[package 1] -> [core 0] -> [thread 0] -> Linux CPU 4
				-> [thread 1] -> Linux CPU 5
		    -> [core 1] -> [thread 0] -> Linux CPU 6
				-> [thread 1] -> Linux CPU 7

      Alternative enumeration::

	[package 0] -> [core 0] -> [thread 0] -> Linux CPU 0
				-> [thread 1] -> Linux CPU 4
		    -> [core 1] -> [thread 0] -> Linux CPU 1
				-> [thread 1] -> Linux CPU 5

	[package 1] -> [core 0] -> [thread 0] -> Linux CPU 2
				-> [thread 1] -> Linux CPU 6
		    -> [core 1] -> [thread 0] -> Linux CPU 3
				-> [thread 1] -> Linux CPU 7

      AMD nomenclature for CMT systems::

	[node 0] -> [Compute Unit 0] -> [Compute Unit Core 0] -> Linux CPU 0
				     -> [Compute Unit Core 1] -> Linux CPU 1
		 -> [Compute Unit 1] -> [Compute Unit Core 0] -> Linux CPU 2
				     -> [Compute Unit Core 1] -> Linux CPU 3

	[node 1] -> [Compute Unit 0] -> [Compute Unit Core 0] -> Linux CPU 4
				     -> [Compute Unit Core 1] -> Linux CPU 5
		 -> [Compute Unit 1] -> [Compute Unit Core 0] -> Linux CPU 6
				     -> [Compute Unit Core 1] -> Linux CPU 7
