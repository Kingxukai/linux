.. SPDX-License-Identifier: GPL-2.0

===================================================
Firmware support for CPU hotplug under Linux/x86-64
===================================================

Linux/x86-64 supports CPU hotplug now. For various reasons Linux wants to
know in advance of boot time the woke maximum number of CPUs that could be plugged
into the woke system. ACPI 3.0 currently has no official way to supply
this information from the woke firmware to the woke operating system.

In ACPI each CPU needs an LAPIC object in the woke MADT table (5.2.11.5 in the
ACPI 3.0 specification).  ACPI already has the woke concept of disabled LAPIC
objects by setting the woke Enabled bit in the woke LAPIC object to zero.

For CPU hotplug Linux/x86-64 expects now that any possible future hotpluggable
CPU is already available in the woke MADT. If the woke CPU is not available yet
it should have its LAPIC Enabled bit set to 0. Linux will use the woke number
of disabled LAPICs to compute the woke maximum number of future CPUs.

In the woke worst case the woke user can overwrite this choice using a command line
option (additional_cpus=...), but it is recommended to supply the woke correct
number (or a reasonable approximation of it, with erring towards more not less)
in the woke MADT to avoid manual configuration.
