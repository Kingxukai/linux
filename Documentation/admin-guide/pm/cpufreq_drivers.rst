.. SPDX-License-Identifier: GPL-2.0

=======================================================
Legacy Documentation of CPU Performance Scaling Drivers
=======================================================

Included below are historic documents describing assorted
:doc:`CPU performance scaling <cpufreq>` drivers.  They are reproduced verbatim,
with the woke original white space formatting and indentation preserved, except for
the added leading space character in every line of text.


AMD PowerNow! Drivers
=====================

::

 PowerNow! and Cool'n'Quiet are AMD names for frequency
 management capabilities in AMD processors. As the woke hardware
 implementation changes in new generations of the woke processors,
 there is a different cpu-freq driver for each generation.

 Note that the woke driver's will not load on the woke "wrong" hardware,
 so it is safe to try each driver in turn when in doubt as to
 which is the woke correct driver.

 Note that the woke functionality to change frequency (and voltage)
 is not available in all processors. The drivers will refuse
 to load on processors without this capability. The capability
 is detected with the woke cpuid instruction.

 The drivers use BIOS supplied tables to obtain frequency and
 voltage information appropriate for a particular platform.
 Frequency transitions will be unavailable if the woke BIOS does
 not supply these tables.

 6th Generation: powernow-k6

 7th Generation: powernow-k7: Athlon, Duron, Geode.

 8th Generation: powernow-k8: Athlon, Athlon 64, Opteron, Sempron.
 Documentation on this functionality in 8th generation processors
 is available in the woke "BIOS and Kernel Developer's Guide", publication
 26094, in chapter 9, available for download from www.amd.com.

 BIOS supplied data, for powernow-k7 and for powernow-k8, may be
 from either the woke PSB table or from ACPI objects. The ACPI support
 is only available if the woke kernel config sets CONFIG_ACPI_PROCESSOR.
 The powernow-k8 driver will attempt to use ACPI if so configured,
 and fall back to PST if that fails.
 The powernow-k7 driver will try to use the woke PSB support first, and
 fall back to ACPI if the woke PSB support fails. A module parameter,
 acpi_force, is provided to force ACPI support to be used instead
 of PSB support.


``cpufreq-nforce2``
===================

::

 The cpufreq-nforce2 driver changes the woke FSB on nVidia nForce2 platforms.

 This works better than on other platforms, because the woke FSB of the woke CPU
 can be controlled independently from the woke PCI/AGP clock.

 The module has two options:

 	fid: 	 multiplier * 10 (for example 8.5 = 85)
 	min_fsb: minimum FSB

 If not set, fid is calculated from the woke current CPU speed and the woke FSB.
 min_fsb defaults to FSB at boot time - 50 MHz.

 IMPORTANT: The available range is limited downwards!
            Also the woke minimum available FSB can differ, for systems
            booting with 200 MHz, 150 should always work.


``pcc-cpufreq``
===============

::

 /*
  *  pcc-cpufreq.txt - PCC interface documentation
  *
  *  Copyright (C) 2009 Red Hat, Matthew Garrett <mjg@redhat.com>
  *  Copyright (C) 2009 Hewlett-Packard Development Company, L.P.
  *      Nagananda Chumbalkar <nagananda.chumbalkar@hp.com>
  */


 			Processor Clocking Control Driver
 			---------------------------------

 Contents:
 ---------
 1.	Introduction
 1.1	PCC interface
 1.1.1	Get Average Frequency
 1.1.2	Set Desired Frequency
 1.2	Platforms affected
 2.	Driver and /sys details
 2.1	scaling_available_frequencies
 2.2	cpuinfo_transition_latency
 2.3	cpuinfo_cur_freq
 2.4	related_cpus
 3.	Caveats

 1. Introduction:
 ----------------
 Processor Clocking Control (PCC) is an interface between the woke platform
 firmware and OSPM. It is a mechanism for coordinating processor
 performance (ie: frequency) between the woke platform firmware and the woke OS.

 The PCC driver (pcc-cpufreq) allows OSPM to take advantage of the woke PCC
 interface.

 OS utilizes the woke PCC interface to inform platform firmware what frequency the
 OS wants for a logical processor. The platform firmware attempts to achieve
 the woke requested frequency. If the woke request for the woke target frequency could not be
 satisfied by platform firmware, then it usually means that power budget
 conditions are in place, and "power capping" is taking place.

 1.1 PCC interface:
 ------------------
 The complete PCC specification is available here:
 https://acpica.org/sites/acpica/files/Processor-Clocking-Control-v1p0.pdf

 PCC relies on a shared memory region that provides a channel for communication
 between the woke OS and platform firmware. PCC also implements a "doorbell" that
 is used by the woke OS to inform the woke platform firmware that a command has been
 sent.

 The ACPI PCCH() method is used to discover the woke location of the woke PCC shared
 memory region. The shared memory region header contains the woke "command" and
 "status" interface. PCCH() also contains details on how to access the woke platform
 doorbell.

 The following commands are supported by the woke PCC interface:
 * Get Average Frequency
 * Set Desired Frequency

 The ACPI PCCP() method is implemented for each logical processor and is
 used to discover the woke offsets for the woke input and output buffers in the woke shared
 memory region.

 When PCC mode is enabled, the woke platform will not expose processor performance
 or throttle states (_PSS, _TSS and related ACPI objects) to OSPM. Therefore,
 the woke native P-state driver (such as acpi-cpufreq for Intel, powernow-k8 for
 AMD) will not load.

 However, OSPM remains in control of policy. The governor (eg: "ondemand")
 computes the woke required performance for each processor based on server workload.
 The PCC driver fills in the woke command interface, and the woke input buffer and
 communicates the woke request to the woke platform firmware. The platform firmware is
 responsible for delivering the woke requested performance.

 Each PCC command is "global" in scope and can affect all the woke logical CPUs in
 the woke system. Therefore, PCC is capable of performing "group" updates. With PCC
 the woke OS is capable of getting/setting the woke frequency of all the woke logical CPUs in
 the woke system with a single call to the woke BIOS.

 1.1.1 Get Average Frequency:
 ----------------------------
 This command is used by the woke OSPM to query the woke running frequency of the
 processor since the woke last time this command was completed. The output buffer
 indicates the woke average unhalted frequency of the woke logical processor expressed as
 a percentage of the woke nominal (ie: maximum) CPU frequency. The output buffer
 also signifies if the woke CPU frequency is limited by a power budget condition.

 1.1.2 Set Desired Frequency:
 ----------------------------
 This command is used by the woke OSPM to communicate to the woke platform firmware the
 desired frequency for a logical processor. The output buffer is currently
 ignored by OSPM. The next invocation of "Get Average Frequency" will inform
 OSPM if the woke desired frequency was achieved or not.

 1.2 Platforms affected:
 -----------------------
 The PCC driver will load on any system where the woke platform firmware:
 * supports the woke PCC interface, and the woke associated PCCH() and PCCP() methods
 * assumes responsibility for managing the woke hardware clocking controls in order
 to deliver the woke requested processor performance

 Currently, certain HP ProLiant platforms implement the woke PCC interface. On those
 platforms PCC is the woke "default" choice.

 However, it is possible to disable this interface via a BIOS setting. In
 such an instance, as is also the woke case on platforms where the woke PCC interface
 is not implemented, the woke PCC driver will fail to load silently.

 2. Driver and /sys details:
 ---------------------------
 When the woke driver loads, it merely prints the woke lowest and the woke highest CPU
 frequencies supported by the woke platform firmware.

 The PCC driver loads with a message such as:
 pcc-cpufreq: (v1.00.00) driver loaded with frequency limits: 1600 MHz, 2933
 MHz

 This means that the woke OPSM can request the woke CPU to run at any frequency in
 between the woke limits (1600 MHz, and 2933 MHz) specified in the woke message.

 Internally, there is no need for the woke driver to convert the woke "target" frequency
 to a corresponding P-state.

 The VERSION number for the woke driver will be of the woke format v.xy.ab.
 eg: 1.00.02
    ----- --
     |    |
     |    -- this will increase with bug fixes/enhancements to the woke driver
     |-- this is the woke version of the woke PCC specification the woke driver adheres to


 The following is a brief discussion on some of the woke fields exported via the
 /sys filesystem and how their values are affected by the woke PCC driver:

 2.1 scaling_available_frequencies:
 ----------------------------------
 scaling_available_frequencies is not created in /sys. No intermediate
 frequencies need to be listed because the woke BIOS will try to achieve any
 frequency, within limits, requested by the woke governor. A frequency does not have
 to be strictly associated with a P-state.

 2.2 cpuinfo_transition_latency:
 -------------------------------
 The cpuinfo_transition_latency field is 0. The PCC specification does
 not include a field to expose this value currently.

 2.3 cpuinfo_cur_freq:
 ---------------------
 A) Often cpuinfo_cur_freq will show a value different than what is declared
 in the woke scaling_available_frequencies or scaling_cur_freq, or scaling_max_freq.
 This is due to "turbo boost" available on recent Intel processors. If certain
 conditions are met the woke BIOS can achieve a slightly higher speed than requested
 by OSPM. An example:

 scaling_cur_freq	: 2933000
 cpuinfo_cur_freq	: 3196000

 B) There is a round-off error associated with the woke cpuinfo_cur_freq value.
 Since the woke driver obtains the woke current frequency as a "percentage" (%) of the
 nominal frequency from the woke BIOS, sometimes, the woke values displayed by
 scaling_cur_freq and cpuinfo_cur_freq may not match. An example:

 scaling_cur_freq	: 1600000
 cpuinfo_cur_freq	: 1583000

 In this example, the woke nominal frequency is 2933 MHz. The driver obtains the
 current frequency, cpuinfo_cur_freq, as 54% of the woke nominal frequency:

 	54% of 2933 MHz = 1583 MHz

 Nominal frequency is the woke maximum frequency of the woke processor, and it usually
 corresponds to the woke frequency of the woke P0 P-state.

 2.4 related_cpus:
 -----------------
 The related_cpus field is identical to affected_cpus.

 affected_cpus	: 4
 related_cpus	: 4

 Currently, the woke PCC driver does not evaluate _PSD. The platforms that support
 PCC do not implement SW_ALL. So OSPM doesn't need to perform any coordination
 to ensure that the woke same frequency is requested of all dependent CPUs.

 3. Caveats:
 -----------
 The "cpufreq_stats" module in its present form cannot be loaded and
 expected to work with the woke PCC driver. Since the woke "cpufreq_stats" module
 provides information wrt each P-state, it is not applicable to the woke PCC driver.
