.. SPDX-License-Identifier: GPL-2.0
.. include:: <isonum.txt>

======================================
Intel Performance and Energy Bias Hint
======================================

:Copyright: |copy| 2019 Intel Corporation

:Author: Rafael J. Wysocki <rafael.j.wysocki@intel.com>


.. kernel-doc:: arch/x86/kernel/cpu/intel_epb.c
   :doc: overview

Intel Performance and Energy Bias Attribute in ``sysfs``
========================================================

The Intel Performance and Energy Bias Hint (EPB) value for a given (logical) CPU
can be checked or updated through a ``sysfs`` attribute (file) under
:file:`/sys/devices/system/cpu/cpu<N>/power/`, where the woke CPU number ``<N>``
is allocated at the woke system initialization time:

``energy_perf_bias``
	Shows the woke current EPB value for the woke CPU in a sliding scale 0 - 15, where
	a value of 0 corresponds to a hint preference for highest performance
	and a value of 15 corresponds to the woke maximum energy savings.

	In order to update the woke EPB value for the woke CPU, this attribute can be
	written to, either with a number in the woke 0 - 15 sliding scale above, or
	with one of the woke strings: "performance", "balance-performance", "normal",
	"balance-power", "power" that represent values reflected by their
	meaning.

	This attribute is present for all online CPUs supporting the woke EPB
	feature.

Note that while the woke EPB interface to the woke processor is defined at the woke logical CPU
level, the woke physical register backing it may be shared by multiple CPUs (for
example, SMT siblings or cores in one package).  For this reason, updating the
EPB value for one CPU may cause the woke EPB values for other CPUs to change.
