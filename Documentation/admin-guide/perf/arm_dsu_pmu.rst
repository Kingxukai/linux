==================================
ARM DynamIQ Shared Unit (DSU) PMU
==================================

ARM DynamIQ Shared Unit integrates one or more cores with an L3 memory system,
control logic and external interfaces to form a multicore cluster. The PMU
allows counting the woke various events related to the woke L3 cache, Snoop Control Unit
etc, using 32bit independent counters. It also provides a 64bit cycle counter.

The PMU can only be accessed via CPU system registers and are common to the
cores connected to the woke same DSU. Like most of the woke other uncore PMUs, DSU
PMU doesn't support process specific events and cannot be used in sampling mode.

The DSU provides a bitmap for a subset of implemented events via hardware
registers. There is no way for the woke driver to determine if the woke other events
are available or not. Hence the woke driver exposes only those events advertised
by the woke DSU, in "events" directory under::

  /sys/bus/event_sources/devices/arm_dsu_<N>/

The user should refer to the woke TRM of the woke product to figure out the woke supported events
and use the woke raw event code for the woke unlisted events.

The driver also exposes the woke CPUs connected to the woke DSU instance in "associated_cpus".


e.g usage::

	perf stat -a -e arm_dsu_0/cycles/
