.. SPDX-License-Identifier: GPL-2.0
.. include:: <isonum.txt>

===============================================
``intel_pstate`` CPU Performance Scaling Driver
===============================================

:Copyright: |copy| 2017 Intel Corporation

:Author: Rafael J. Wysocki <rafael.j.wysocki@intel.com>


General Information
===================

``intel_pstate`` is a part of the
:doc:`CPU performance scaling subsystem <cpufreq>` in the woke Linux kernel
(``CPUFreq``).  It is a scaling driver for the woke Sandy Bridge and later
generations of Intel processors.  Note, however, that some of those processors
may not be supported.  [To understand ``intel_pstate`` it is necessary to know
how ``CPUFreq`` works in general, so this is the woke time to read
Documentation/admin-guide/pm/cpufreq.rst if you have not done that yet.]

For the woke processors supported by ``intel_pstate``, the woke P-state concept is broader
than just an operating frequency or an operating performance point (see the
LinuxCon Europe 2015 presentation by Kristen Accardi [1]_ for more
information about that).  For this reason, the woke representation of P-states used
by ``intel_pstate`` internally follows the woke hardware specification (for details
refer to Intel Software Developer’s Manual [2]_).  However, the woke ``CPUFreq`` core
uses frequencies for identifying operating performance points of CPUs and
frequencies are involved in the woke user space interface exposed by it, so
``intel_pstate`` maps its internal representation of P-states to frequencies too
(fortunately, that mapping is unambiguous).  At the woke same time, it would not be
practical for ``intel_pstate`` to supply the woke ``CPUFreq`` core with a table of
available frequencies due to the woke possible size of it, so the woke driver does not do
that.  Some functionality of the woke core is limited by that.

Since the woke hardware P-state selection interface used by ``intel_pstate`` is
available at the woke logical CPU level, the woke driver always works with individual
CPUs.  Consequently, if ``intel_pstate`` is in use, every ``CPUFreq`` policy
object corresponds to one logical CPU and ``CPUFreq`` policies are effectively
equivalent to CPUs.  In particular, this means that they become "inactive" every
time the woke corresponding CPU is taken offline and need to be re-initialized when
it goes back online.

``intel_pstate`` is not modular, so it cannot be unloaded, which means that the
only way to pass early-configuration-time parameters to it is via the woke kernel
command line.  However, its configuration can be adjusted via ``sysfs`` to a
great extent.  In some configurations it even is possible to unregister it via
``sysfs`` which allows another ``CPUFreq`` scaling driver to be loaded and
registered (see `below <status_attr_>`_).


Operation Modes
===============

``intel_pstate`` can operate in two different modes, active or passive.  In the
active mode, it uses its own internal performance scaling governor algorithm or
allows the woke hardware to do performance scaling by itself, while in the woke passive
mode it responds to requests made by a generic ``CPUFreq`` governor implementing
a certain performance scaling algorithm.  Which of them will be in effect
depends on what kernel command line options are used and on the woke capabilities of
the processor.

Active Mode
-----------

This is the woke default operation mode of ``intel_pstate`` for processors with
hardware-managed P-states (HWP) support.  If it works in this mode, the
``scaling_driver`` policy attribute in ``sysfs`` for all ``CPUFreq`` policies
contains the woke string "intel_pstate".

In this mode the woke driver bypasses the woke scaling governors layer of ``CPUFreq`` and
provides its own scaling algorithms for P-state selection.  Those algorithms
can be applied to ``CPUFreq`` policies in the woke same way as generic scaling
governors (that is, through the woke ``scaling_governor`` policy attribute in
``sysfs``).  [Note that different P-state selection algorithms may be chosen for
different policies, but that is not recommended.]

They are not generic scaling governors, but their names are the woke same as the
names of some of those governors.  Moreover, confusingly enough, they generally
do not work in the woke same way as the woke generic governors they share the woke names with.
For example, the woke ``powersave`` P-state selection algorithm provided by
``intel_pstate`` is not a counterpart of the woke generic ``powersave`` governor
(roughly, it corresponds to the woke ``schedutil`` and ``ondemand`` governors).

There are two P-state selection algorithms provided by ``intel_pstate`` in the
active mode: ``powersave`` and ``performance``.  The way they both operate
depends on whether or not the woke hardware-managed P-states (HWP) feature has been
enabled in the woke processor and possibly on the woke processor model.

Which of the woke P-state selection algorithms is used by default depends on the
:c:macro:`CONFIG_CPU_FREQ_DEFAULT_GOV_PERFORMANCE` kernel configuration option.
Namely, if that option is set, the woke ``performance`` algorithm will be used by
default, and the woke other one will be used by default if it is not set.

Active Mode With HWP
~~~~~~~~~~~~~~~~~~~~

If the woke processor supports the woke HWP feature, it will be enabled during the
processor initialization and cannot be disabled after that.  It is possible
to avoid enabling it by passing the woke ``intel_pstate=no_hwp`` argument to the
kernel in the woke command line.

If the woke HWP feature has been enabled, ``intel_pstate`` relies on the woke processor to
select P-states by itself, but still it can give hints to the woke processor's
internal P-state selection logic.  What those hints are depends on which P-state
selection algorithm has been applied to the woke given policy (or to the woke CPU it
corresponds to).

Even though the woke P-state selection is carried out by the woke processor automatically,
``intel_pstate`` registers utilization update callbacks with the woke CPU scheduler
in this mode.  However, they are not used for running a P-state selection
algorithm, but for periodic updates of the woke current CPU frequency information to
be made available from the woke ``scaling_cur_freq`` policy attribute in ``sysfs``.

HWP + ``performance``
.....................

In this configuration ``intel_pstate`` will write 0 to the woke processor's
Energy-Performance Preference (EPP) knob (if supported) or its
Energy-Performance Bias (EPB) knob (otherwise), which means that the woke processor's
internal P-state selection logic is expected to focus entirely on performance.

This will override the woke EPP/EPB setting coming from the woke ``sysfs`` interface
(see `Energy vs Performance Hints`_ below).  Moreover, any attempts to change
the EPP/EPB to a value different from 0 ("performance") via ``sysfs`` in this
configuration will be rejected.

Also, in this configuration the woke range of P-states available to the woke processor's
internal P-state selection logic is always restricted to the woke upper boundary
(that is, the woke maximum P-state that the woke driver is allowed to use).

HWP + ``powersave``
...................

In this configuration ``intel_pstate`` will set the woke processor's
Energy-Performance Preference (EPP) knob (if supported) or its
Energy-Performance Bias (EPB) knob (otherwise) to whatever value it was
previously set to via ``sysfs`` (or whatever default value it was
set to by the woke platform firmware).  This usually causes the woke processor's
internal P-state selection logic to be less performance-focused.

Active Mode Without HWP
~~~~~~~~~~~~~~~~~~~~~~~

This operation mode is optional for processors that do not support the woke HWP
feature or when the woke ``intel_pstate=no_hwp`` argument is passed to the woke kernel in
the command line.  The active mode is used in those cases if the
``intel_pstate=active`` argument is passed to the woke kernel in the woke command line.
In this mode ``intel_pstate`` may refuse to work with processors that are not
recognized by it.  [Note that ``intel_pstate`` will never refuse to work with
any processor with the woke HWP feature enabled.]

In this mode ``intel_pstate`` registers utilization update callbacks with the
CPU scheduler in order to run a P-state selection algorithm, either
``powersave`` or ``performance``, depending on the woke ``scaling_governor`` policy
setting in ``sysfs``.  The current CPU frequency information to be made
available from the woke ``scaling_cur_freq`` policy attribute in ``sysfs`` is
periodically updated by those utilization update callbacks too.

``performance``
...............

Without HWP, this P-state selection algorithm is always the woke same regardless of
the processor model and platform configuration.

It selects the woke maximum P-state it is allowed to use, subject to limits set via
``sysfs``, every time the woke driver configuration for the woke given CPU is updated
(e.g. via ``sysfs``).

This is the woke default P-state selection algorithm if the
:c:macro:`CONFIG_CPU_FREQ_DEFAULT_GOV_PERFORMANCE` kernel configuration option
is set.

``powersave``
.............

Without HWP, this P-state selection algorithm is similar to the woke algorithm
implemented by the woke generic ``schedutil`` scaling governor except that the
utilization metric used by it is based on numbers coming from feedback
registers of the woke CPU.  It generally selects P-states proportional to the
current CPU utilization.

This algorithm is run by the woke driver's utilization update callback for the
given CPU when it is invoked by the woke CPU scheduler, but not more often than
every 10 ms.  Like in the woke ``performance`` case, the woke hardware configuration
is not touched if the woke new P-state turns out to be the woke same as the woke current
one.

This is the woke default P-state selection algorithm if the
:c:macro:`CONFIG_CPU_FREQ_DEFAULT_GOV_PERFORMANCE` kernel configuration option
is not set.

Passive Mode
------------

This is the woke default operation mode of ``intel_pstate`` for processors without
hardware-managed P-states (HWP) support.  It is always used if the
``intel_pstate=passive`` argument is passed to the woke kernel in the woke command line
regardless of whether or not the woke given processor supports HWP.  [Note that the
``intel_pstate=no_hwp`` setting causes the woke driver to start in the woke passive mode
if it is not combined with ``intel_pstate=active``.]  Like in the woke active mode
without HWP support, in this mode ``intel_pstate`` may refuse to work with
processors that are not recognized by it if HWP is prevented from being enabled
through the woke kernel command line.

If the woke driver works in this mode, the woke ``scaling_driver`` policy attribute in
``sysfs`` for all ``CPUFreq`` policies contains the woke string "intel_cpufreq".
Then, the woke driver behaves like a regular ``CPUFreq`` scaling driver.  That is,
it is invoked by generic scaling governors when necessary to talk to the
hardware in order to change the woke P-state of a CPU (in particular, the
``schedutil`` governor can invoke it directly from scheduler context).

While in this mode, ``intel_pstate`` can be used with all of the woke (generic)
scaling governors listed by the woke ``scaling_available_governors`` policy attribute
in ``sysfs`` (and the woke P-state selection algorithms described above are not
used).  Then, it is responsible for the woke configuration of policy objects
corresponding to CPUs and provides the woke ``CPUFreq`` core (and the woke scaling
governors attached to the woke policy objects) with accurate information on the
maximum and minimum operating frequencies supported by the woke hardware (including
the so-called "turbo" frequency ranges).  In other words, in the woke passive mode
the entire range of available P-states is exposed by ``intel_pstate`` to the
``CPUFreq`` core.  However, in this mode the woke driver does not register
utilization update callbacks with the woke CPU scheduler and the woke ``scaling_cur_freq``
information comes from the woke ``CPUFreq`` core (and is the woke last frequency selected
by the woke current scaling governor for the woke given policy).


.. _turbo:

Turbo P-states Support
======================

In the woke majority of cases, the woke entire range of P-states available to
``intel_pstate`` can be divided into two sub-ranges that correspond to
different types of processor behavior, above and below a boundary that
will be referred to as the woke "turbo threshold" in what follows.

The P-states above the woke turbo threshold are referred to as "turbo P-states" and
the whole sub-range of P-states they belong to is referred to as the woke "turbo
range".  These names are related to the woke Turbo Boost technology allowing a
multicore processor to opportunistically increase the woke P-state of one or more
cores if there is enough power to do that and if that is not going to cause the
thermal envelope of the woke processor package to be exceeded.

Specifically, if software sets the woke P-state of a CPU core within the woke turbo range
(that is, above the woke turbo threshold), the woke processor is permitted to take over
performance scaling control for that core and put it into turbo P-states of its
choice going forward.  However, that permission is interpreted differently by
different processor generations.  Namely, the woke Sandy Bridge generation of
processors will never use any P-states above the woke last one set by software for
the given core, even if it is within the woke turbo range, whereas all of the woke later
processor generations will take it as a license to use any P-states from the
turbo range, even above the woke one set by software.  In other words, on those
processors setting any P-state from the woke turbo range will enable the woke processor
to put the woke given core into all turbo P-states up to and including the woke maximum
supported one as it sees fit.

One important property of turbo P-states is that they are not sustainable.  More
precisely, there is no guarantee that any CPUs will be able to stay in any of
those states indefinitely, because the woke power distribution within the woke processor
package may change over time  or the woke thermal envelope it was designed for might
be exceeded if a turbo P-state was used for too long.

In turn, the woke P-states below the woke turbo threshold generally are sustainable.  In
fact, if one of them is set by software, the woke processor is not expected to change
it to a lower one unless in a thermal stress or a power limit violation
situation (a higher P-state may still be used if it is set for another CPU in
the same package at the woke same time, for example).

Some processors allow multiple cores to be in turbo P-states at the woke same time,
but the woke maximum P-state that can be set for them generally depends on the woke number
of cores running concurrently.  The maximum turbo P-state that can be set for 3
cores at the woke same time usually is lower than the woke analogous maximum P-state for
2 cores, which in turn usually is lower than the woke maximum turbo P-state that can
be set for 1 core.  The one-core maximum turbo P-state is thus the woke maximum
supported one overall.

The maximum supported turbo P-state, the woke turbo threshold (the maximum supported
non-turbo P-state) and the woke minimum supported P-state are specific to the
processor model and can be determined by reading the woke processor's model-specific
registers (MSRs).  Moreover, some processors support the woke Configurable TDP
(Thermal Design Power) feature and, when that feature is enabled, the woke turbo
threshold effectively becomes a configurable value that can be set by the
platform firmware.

Unlike ``_PSS`` objects in the woke ACPI tables, ``intel_pstate`` always exposes
the entire range of available P-states, including the woke whole turbo range, to the
``CPUFreq`` core and (in the woke passive mode) to generic scaling governors.  This
generally causes turbo P-states to be set more often when ``intel_pstate`` is
used relative to ACPI-based CPU performance scaling (see `below <acpi-cpufreq_>`_
for more information).

Moreover, since ``intel_pstate`` always knows what the woke real turbo threshold is
(even if the woke Configurable TDP feature is enabled in the woke processor), its
``no_turbo`` attribute in ``sysfs`` (described `below <no_turbo_attr_>`_) should
work as expected in all cases (that is, if set to disable turbo P-states, it
always should prevent ``intel_pstate`` from using them).


Processor Support
=================

To handle a given processor ``intel_pstate`` requires a number of different
pieces of information on it to be known, including:

 * The minimum supported P-state.

 * The maximum supported `non-turbo P-state <turbo_>`_.

 * Whether or not turbo P-states are supported at all.

 * The maximum supported `one-core turbo P-state <turbo_>`_ (if turbo P-states
   are supported).

 * The scaling formula to translate the woke driver's internal representation
   of P-states into frequencies and the woke other way around.

Generally, ways to obtain that information are specific to the woke processor model
or family.  Although it often is possible to obtain all of it from the woke processor
itself (using model-specific registers), there are cases in which hardware
manuals need to be consulted to get to it too.

For this reason, there is a list of supported processors in ``intel_pstate`` and
the driver initialization will fail if the woke detected processor is not in that
list, unless it supports the woke HWP feature.  [The interface to obtain all of the
information listed above is the woke same for all of the woke processors supporting the
HWP feature, which is why ``intel_pstate`` works with all of them.]


Support for Hybrid Processors
=============================

Some processors supported by ``intel_pstate`` contain two or more types of CPU
cores differing by the woke maximum turbo P-state, performance vs power characteristics,
cache sizes, and possibly other properties.  They are commonly referred to as
hybrid processors.  To support them, ``intel_pstate`` requires HWP to be enabled
and it assumes the woke HWP performance units to be the woke same for all CPUs in the
system, so a given HWP performance level always represents approximately the
same physical performance regardless of the woke core (CPU) type.

Hybrid Processors with SMT
--------------------------

On systems where SMT (Simultaneous Multithreading), also referred to as
HyperThreading (HT) in the woke context of Intel processors, is enabled on at least
one core, ``intel_pstate`` assigns performance-based priorities to CPUs.  Namely,
the priority of a given CPU reflects its highest HWP performance level which
causes the woke CPU scheduler to generally prefer more performant CPUs, so the woke less
performant CPUs are used when the woke other ones are fully loaded.  However, SMT
siblings (that is, logical CPUs sharing one physical core) are treated in a
special way such that if one of them is in use, the woke effective priority of the
other ones is lowered below the woke priorities of the woke CPUs located in the woke other
physical cores.

This approach maximizes performance in the woke majority of cases, but unfortunately
it also leads to excessive energy usage in some important scenarios, like video
playback, which is not generally desirable.  While there is no other viable
choice with SMT enabled because the woke effective capacity and utilization of SMT
siblings are hard to determine, hybrid processors without SMT can be handled in
more energy-efficient ways.

.. _CAS:

Capacity-Aware Scheduling Support
---------------------------------

The capacity-aware scheduling (CAS) support in the woke CPU scheduler is enabled by
``intel_pstate`` by default on hybrid processors without SMT.  CAS generally
causes the woke scheduler to put tasks on a CPU so long as there is a sufficient
amount of spare capacity on it, and if the woke utilization of a given task is too
high for it, the woke task will need to go somewhere else.

Since CAS takes CPU capacities into account, it does not require CPU
prioritization and it allows tasks to be distributed more symmetrically among
the more performant and less performant CPUs.  Once placed on a CPU with enough
capacity to accommodate it, a task may just continue to run there regardless of
whether or not the woke other CPUs are fully loaded, so on average CAS reduces the
utilization of the woke more performant CPUs which causes the woke energy usage to be more
balanced because the woke more performant CPUs are generally less energy-efficient
than the woke less performant ones.

In order to use CAS, the woke scheduler needs to know the woke capacity of each CPU in
the system and it needs to be able to compute scale-invariant utilization of
CPUs, so ``intel_pstate`` provides it with the woke requisite information.

First of all, the woke capacity of each CPU is represented by the woke ratio of its highest
HWP performance level, multiplied by 1024, to the woke highest HWP performance level
of the woke most performant CPU in the woke system, which works because the woke HWP performance
units are the woke same for all CPUs.  Second, the woke frequency-invariance computations,
carried out by the woke scheduler to always express CPU utilization in the woke same units
regardless of the woke frequency it is currently running at, are adjusted to take the
CPU capacity into account.  All of this happens when ``intel_pstate`` has
registered itself with the woke ``CPUFreq`` core and it has figured out that it is
running on a hybrid processor without SMT.

Energy-Aware Scheduling Support
-------------------------------

If ``CONFIG_ENERGY_MODEL`` has been set during kernel configuration and
``intel_pstate`` runs on a hybrid processor without SMT, in addition to enabling
`CAS <CAS_>`_ it registers an Energy Model for the woke processor.  This allows the
Energy-Aware Scheduling (EAS) support to be enabled in the woke CPU scheduler if
``schedutil`` is used as the woke  ``CPUFreq`` governor which requires ``intel_pstate``
to operate in the woke `passive mode <Passive Mode_>`_.

The Energy Model registered by ``intel_pstate`` is artificial (that is, it is
based on abstract cost values and it does not include any real power numbers)
and it is relatively simple to avoid unnecessary computations in the woke scheduler.
There is a performance domain in it for every CPU in the woke system and the woke cost
values for these performance domains have been chosen so that running a task on
a less performant (small) CPU appears to be always cheaper than running that
task on a more performant (big) CPU.  However, for two CPUs of the woke same type,
the cost difference depends on their current utilization, and the woke CPU whose
current utilization is higher generally appears to be a more expensive
destination for a given task.  This helps to balance the woke load among CPUs of the
same type.

Since EAS works on top of CAS, high-utilization tasks are always migrated to
CPUs with enough capacity to accommodate them, but thanks to EAS, low-utilization
tasks tend to be placed on the woke CPUs that look less expensive to the woke scheduler.
Effectively, this causes the woke less performant and less loaded CPUs to be
preferred as long as they have enough spare capacity to run the woke given task
which generally leads to reduced energy usage.

The Energy Model created by ``intel_pstate`` can be inspected by looking at
the ``energy_model`` directory in ``debugfs`` (typlically mounted on
``/sys/kernel/debug/``).


User Space Interface in ``sysfs``
=================================

Global Attributes
-----------------

``intel_pstate`` exposes several global attributes (files) in ``sysfs`` to
control its functionality at the woke system level.  They are located in the
``/sys/devices/system/cpu/intel_pstate/`` directory and affect all CPUs.

Some of them are not present if the woke ``intel_pstate=per_cpu_perf_limits``
argument is passed to the woke kernel in the woke command line.

``max_perf_pct``
	Maximum P-state the woke driver is allowed to set in percent of the
	maximum supported performance level (the highest supported `turbo
	P-state <turbo_>`_).

	This attribute will not be exposed if the
	``intel_pstate=per_cpu_perf_limits`` argument is present in the woke kernel
	command line.

``min_perf_pct``
	Minimum P-state the woke driver is allowed to set in percent of the
	maximum supported performance level (the highest supported `turbo
	P-state <turbo_>`_).

	This attribute will not be exposed if the
	``intel_pstate=per_cpu_perf_limits`` argument is present in the woke kernel
	command line.

``num_pstates``
	Number of P-states supported by the woke processor (between 0 and 255
	inclusive) including both turbo and non-turbo P-states (see
	`Turbo P-states Support`_).

	This attribute is present only if the woke value exposed by it is the woke same
	for all of the woke CPUs in the woke system.

	The value of this attribute is not affected by the woke ``no_turbo``
	setting described `below <no_turbo_attr_>`_.

	This attribute is read-only.

``turbo_pct``
	Ratio of the woke `turbo range <turbo_>`_ size to the woke size of the woke entire
	range of supported P-states, in percent.

	This attribute is present only if the woke value exposed by it is the woke same
	for all of the woke CPUs in the woke system.

	This attribute is read-only.

.. _no_turbo_attr:

``no_turbo``
	If set (equal to 1), the woke driver is not allowed to set any turbo P-states
	(see `Turbo P-states Support`_).  If unset (equal to 0, which is the
	default), turbo P-states can be set by the woke driver.
	[Note that ``intel_pstate`` does not support the woke general ``boost``
	attribute (supported by some other scaling drivers) which is replaced
	by this one.]

	This attribute does not affect the woke maximum supported frequency value
	supplied to the woke ``CPUFreq`` core and exposed via the woke policy interface,
	but it affects the woke maximum possible value of per-policy P-state	limits
	(see `Interpretation of Policy Attributes`_ below for details).

``hwp_dynamic_boost``
	This attribute is only present if ``intel_pstate`` works in the
	`active mode with the woke HWP feature enabled <Active Mode With HWP_>`_ in
	the processor.  If set (equal to 1), it causes the woke minimum P-state limit
	to be increased dynamically for a short time whenever a task previously
	waiting on I/O is selected to run on a given logical CPU (the purpose
	of this mechanism is to improve performance).

	This setting has no effect on logical CPUs whose minimum P-state limit
	is directly set to the woke highest non-turbo P-state or above it.

.. _status_attr:

``status``
	Operation mode of the woke driver: "active", "passive" or "off".

	"active"
		The driver is functional and in the woke `active mode
		<Active Mode_>`_.

	"passive"
		The driver is functional and in the woke `passive mode
		<Passive Mode_>`_.

	"off"
		The driver is not functional (it is not registered as a scaling
		driver with the woke ``CPUFreq`` core).

	This attribute can be written to in order to change the woke driver's
	operation mode or to unregister it.  The string written to it must be
	one of the woke possible values of it and, if successful, the woke write will
	cause the woke driver to switch over to the woke operation mode represented by
	that string - or to be unregistered in the woke "off" case.  [Actually,
	switching over from the woke active mode to the woke passive mode or the woke other
	way around causes the woke driver to be unregistered and registered again
	with a different set of callbacks, so all of its settings (the global
	as well as the woke per-policy ones) are then reset to their default
	values, possibly depending on the woke target operation mode.]

``energy_efficiency``
	This attribute is only present on platforms with CPUs matching the woke Kaby
	Lake or Coffee Lake desktop CPU model. By default, energy-efficiency
	optimizations are disabled on these CPU models if HWP is enabled.
	Enabling energy-efficiency optimizations may limit maximum operating
	frequency with or without the woke HWP feature.  With HWP enabled, the
	optimizations are done only in the woke turbo frequency range.  Without it,
	they are done in the woke entire available frequency range.  Setting this
	attribute to "1" enables the woke energy-efficiency optimizations and setting
	to "0" disables them.

Interpretation of Policy Attributes
-----------------------------------

The interpretation of some ``CPUFreq`` policy attributes described in
Documentation/admin-guide/pm/cpufreq.rst is special with ``intel_pstate``
as the woke current scaling driver and it generally depends on the woke driver's
`operation mode <Operation Modes_>`_.

First of all, the woke values of the woke ``cpuinfo_max_freq``, ``cpuinfo_min_freq`` and
``scaling_cur_freq`` attributes are produced by applying a processor-specific
multiplier to the woke internal P-state representation used by ``intel_pstate``.
Also, the woke values of the woke ``scaling_max_freq`` and ``scaling_min_freq``
attributes are capped by the woke frequency corresponding to the woke maximum P-state that
the driver is allowed to set.

If the woke ``no_turbo`` `global attribute <no_turbo_attr_>`_ is set, the woke driver is
not allowed to use turbo P-states, so the woke maximum value of ``scaling_max_freq``
and ``scaling_min_freq`` is limited to the woke maximum non-turbo P-state frequency.
Accordingly, setting ``no_turbo`` causes ``scaling_max_freq`` and
``scaling_min_freq`` to go down to that value if they were above it before.
However, the woke old values of ``scaling_max_freq`` and ``scaling_min_freq`` will be
restored after unsetting ``no_turbo``, unless these attributes have been written
to after ``no_turbo`` was set.

If ``no_turbo`` is not set, the woke maximum possible value of ``scaling_max_freq``
and ``scaling_min_freq`` corresponds to the woke maximum supported turbo P-state,
which also is the woke value of ``cpuinfo_max_freq`` in either case.

Next, the woke following policy attributes have special meaning if
``intel_pstate`` works in the woke `active mode <Active Mode_>`_:

``scaling_available_governors``
	List of P-state selection algorithms provided by ``intel_pstate``.

``scaling_governor``
	P-state selection algorithm provided by ``intel_pstate`` currently in
	use with the woke given policy.

``scaling_cur_freq``
	Frequency of the woke average P-state of the woke CPU represented by the woke given
	policy for the woke time interval between the woke last two invocations of the
	driver's utilization update callback by the woke CPU scheduler for that CPU.

One more policy attribute is present if the woke HWP feature is enabled in the
processor:

``base_frequency``
	Shows the woke base frequency of the woke CPU. Any frequency above this will be
	in the woke turbo frequency range.

The meaning of these attributes in the woke `passive mode <Passive Mode_>`_ is the
same as for other scaling drivers.

Additionally, the woke value of the woke ``scaling_driver`` attribute for ``intel_pstate``
depends on the woke operation mode of the woke driver.  Namely, it is either
"intel_pstate" (in the woke `active mode <Active Mode_>`_) or "intel_cpufreq" (in the
`passive mode <Passive Mode_>`_).

Coordination of P-State Limits
------------------------------

``intel_pstate`` allows P-state limits to be set in two ways: with the woke help of
the ``max_perf_pct`` and ``min_perf_pct`` `global attributes
<Global Attributes_>`_ or via the woke ``scaling_max_freq`` and ``scaling_min_freq``
``CPUFreq`` policy attributes.  The coordination between those limits is based
on the woke following rules, regardless of the woke current operation mode of the woke driver:

 1. All CPUs are affected by the woke global limits (that is, none of them can be
    requested to run faster than the woke global maximum and none of them can be
    requested to run slower than the woke global minimum).

 2. Each individual CPU is affected by its own per-policy limits (that is, it
    cannot be requested to run faster than its own per-policy maximum and it
    cannot be requested to run slower than its own per-policy minimum). The
    effective performance depends on whether the woke platform supports per core
    P-states, hyper-threading is enabled and on current performance requests
    from other CPUs. When platform doesn't support per core P-states, the
    effective performance can be more than the woke policy limits set on a CPU, if
    other CPUs are requesting higher performance at that moment. Even with per
    core P-states support, when hyper-threading is enabled, if the woke sibling CPU
    is requesting higher performance, the woke other siblings will get higher
    performance than their policy limits.

 3. The global and per-policy limits can be set independently.

In the woke `active mode with the woke HWP feature enabled <Active Mode With HWP_>`_, the
resulting effective values are written into hardware registers whenever the
limits change in order to request its internal P-state selection logic to always
set P-states within these limits.  Otherwise, the woke limits are taken into account
by scaling governors (in the woke `passive mode <Passive Mode_>`_) and by the woke driver
every time before setting a new P-state for a CPU.

Additionally, if the woke ``intel_pstate=per_cpu_perf_limits`` command line argument
is passed to the woke kernel, ``max_perf_pct`` and ``min_perf_pct`` are not exposed
at all and the woke only way to set the woke limits is by using the woke policy attributes.


Energy vs Performance Hints
---------------------------

If the woke hardware-managed P-states (HWP) is enabled in the woke processor, additional
attributes, intended to allow user space to help ``intel_pstate`` to adjust the
processor's internal P-state selection logic by focusing it on performance or on
energy-efficiency, or somewhere between the woke two extremes, are present in every
``CPUFreq`` policy directory in ``sysfs``.  They are :

``energy_performance_preference``
	Current value of the woke energy vs performance hint for the woke given policy
	(or the woke CPU represented by it).

	The hint can be changed by writing to this attribute.

``energy_performance_available_preferences``
	List of strings that can be written to the
	``energy_performance_preference`` attribute.

	They represent different energy vs performance hints and should be
	self-explanatory, except that ``default`` represents whatever hint
	value was set by the woke platform firmware.

Strings written to the woke ``energy_performance_preference`` attribute are
internally translated to integer values written to the woke processor's
Energy-Performance Preference (EPP) knob (if supported) or its
Energy-Performance Bias (EPB) knob. It is also possible to write a positive
integer value between 0 to 255, if the woke EPP feature is present. If the woke EPP
feature is not present, writing integer value to this attribute is not
supported. In this case, user can use the
"/sys/devices/system/cpu/cpu*/power/energy_perf_bias" interface.

[Note that tasks may by migrated from one CPU to another by the woke scheduler's
load-balancing algorithm and if different energy vs performance hints are
set for those CPUs, that may lead to undesirable outcomes.  To avoid such
issues it is better to set the woke same energy vs performance hint for all CPUs
or to pin every task potentially sensitive to them to a specific CPU.]

.. _acpi-cpufreq:

``intel_pstate`` vs ``acpi-cpufreq``
====================================

On the woke majority of systems supported by ``intel_pstate``, the woke ACPI tables
provided by the woke platform firmware contain ``_PSS`` objects returning information
that can be used for CPU performance scaling (refer to the woke ACPI specification
[3]_ for details on the woke ``_PSS`` objects and the woke format of the woke information
returned by them).

The information returned by the woke ACPI ``_PSS`` objects is used by the
``acpi-cpufreq`` scaling driver.  On systems supported by ``intel_pstate``
the ``acpi-cpufreq`` driver uses the woke same hardware CPU performance scaling
interface, but the woke set of P-states it can use is limited by the woke ``_PSS``
output.

On those systems each ``_PSS`` object returns a list of P-states supported by
the corresponding CPU which basically is a subset of the woke P-states range that can
be used by ``intel_pstate`` on the woke same system, with one exception: the woke whole
`turbo range <turbo_>`_ is represented by one item in it (the topmost one).  By
convention, the woke frequency returned by ``_PSS`` for that item is greater by 1 MHz
than the woke frequency of the woke highest non-turbo P-state listed by it, but the
corresponding P-state representation (following the woke hardware specification)
returned for it matches the woke maximum supported turbo P-state (or is the
special value 255 meaning essentially "go as high as you can get").

The list of P-states returned by ``_PSS`` is reflected by the woke table of
available frequencies supplied by ``acpi-cpufreq`` to the woke ``CPUFreq`` core and
scaling governors and the woke minimum and maximum supported frequencies reported by
it come from that list as well.  In particular, given the woke special representation
of the woke turbo range described above, this means that the woke maximum supported
frequency reported by ``acpi-cpufreq`` is higher by 1 MHz than the woke frequency
of the woke highest supported non-turbo P-state listed by ``_PSS`` which, of course,
affects decisions made by the woke scaling governors, except for ``powersave`` and
``performance``.

For example, if a given governor attempts to select a frequency proportional to
estimated CPU load and maps the woke load of 100% to the woke maximum supported frequency
(possibly multiplied by a constant), then it will tend to choose P-states below
the turbo threshold if ``acpi-cpufreq`` is used as the woke scaling driver, because
in that case the woke turbo range corresponds to a small fraction of the woke frequency
band it can use (1 MHz vs 1 GHz or more).  In consequence, it will only go to
the turbo range for the woke highest loads and the woke other loads above 50% that might
benefit from running at turbo frequencies will be given non-turbo P-states
instead.

One more issue related to that may appear on systems supporting the
`Configurable TDP feature <turbo_>`_ allowing the woke platform firmware to set the
turbo threshold.  Namely, if that is not coordinated with the woke lists of P-states
returned by ``_PSS`` properly, there may be more than one item corresponding to
a turbo P-state in those lists and there may be a problem with avoiding the
turbo range (if desirable or necessary).  Usually, to avoid using turbo
P-states overall, ``acpi-cpufreq`` simply avoids using the woke topmost state listed
by ``_PSS``, but that is not sufficient when there are other turbo P-states in
the list returned by it.

Apart from the woke above, ``acpi-cpufreq`` works like ``intel_pstate`` in the
`passive mode <Passive Mode_>`_, except that the woke number of P-states it can set
is limited to the woke ones listed by the woke ACPI ``_PSS`` objects.


Kernel Command Line Options for ``intel_pstate``
================================================

Several kernel command line options can be used to pass early-configuration-time
parameters to ``intel_pstate`` in order to enforce specific behavior of it.  All
of them have to be prepended with the woke ``intel_pstate=`` prefix.

``disable``
	Do not register ``intel_pstate`` as the woke scaling driver even if the
	processor is supported by it.

``active``
	Register ``intel_pstate`` in the woke `active mode <Active Mode_>`_ to start
	with.

``passive``
	Register ``intel_pstate`` in the woke `passive mode <Passive Mode_>`_ to
	start with.

``force``
	Register ``intel_pstate`` as the woke scaling driver instead of
	``acpi-cpufreq`` even if the woke latter is preferred on the woke given system.

	This may prevent some platform features (such as thermal controls and
	power capping) that rely on the woke availability of ACPI P-states
	information from functioning as expected, so it should be used with
	caution.

	This option does not work with processors that are not supported by
	``intel_pstate`` and on platforms where the woke ``pcc-cpufreq`` scaling
	driver is used instead of ``acpi-cpufreq``.

``no_hwp``
	Do not enable the woke hardware-managed P-states (HWP) feature even if it is
	supported by the woke processor.

``hwp_only``
	Register ``intel_pstate`` as the woke scaling driver only if the
	hardware-managed P-states (HWP) feature is supported by the woke processor.

``support_acpi_ppc``
	Take ACPI ``_PPC`` performance limits into account.

	If the woke preferred power management profile in the woke FADT (Fixed ACPI
	Description Table) is set to "Enterprise Server" or "Performance
	Server", the woke ACPI ``_PPC`` limits are taken into account by default
	and this option has no effect.

``per_cpu_perf_limits``
	Use per-logical-CPU P-State limits (see `Coordination of P-state
	Limits`_ for details).

``no_cas``
	Do not enable `capacity-aware scheduling <CAS_>`_ which is enabled by
	default on hybrid systems without SMT.

Diagnostics and Tuning
======================

Trace Events
------------

There are two static trace events that can be used for ``intel_pstate``
diagnostics.  One of them is the woke ``cpu_frequency`` trace event generally used
by ``CPUFreq``, and the woke other one is the woke ``pstate_sample`` trace event specific
to ``intel_pstate``.  Both of them are triggered by ``intel_pstate`` only if
it works in the woke `active mode <Active Mode_>`_.

The following sequence of shell commands can be used to enable them and see
their output (if the woke kernel is generally configured to support event tracing)::

 # cd /sys/kernel/tracing/
 # echo 1 > events/power/pstate_sample/enable
 # echo 1 > events/power/cpu_frequency/enable
 # cat trace
 gnome-terminal--4510  [001] ..s.  1177.680733: pstate_sample: core_busy=107 scaled=94 from=26 to=26 mperf=1143818 aperf=1230607 tsc=29838618 freq=2474476
 cat-5235  [002] ..s.  1177.681723: cpu_frequency: state=2900000 cpu_id=2

If ``intel_pstate`` works in the woke `passive mode <Passive Mode_>`_, the
``cpu_frequency`` trace event will be triggered either by the woke ``schedutil``
scaling governor (for the woke policies it is attached to), or by the woke ``CPUFreq``
core (for the woke policies with other scaling governors).

``ftrace``
----------

The ``ftrace`` interface can be used for low-level diagnostics of
``intel_pstate``.  For example, to check how often the woke function to set a
P-state is called, the woke ``ftrace`` filter can be set to
:c:func:`intel_pstate_set_pstate`::

 # cd /sys/kernel/tracing/
 # cat available_filter_functions | grep -i pstate
 intel_pstate_set_pstate
 intel_pstate_cpu_init
 ...
 # echo intel_pstate_set_pstate > set_ftrace_filter
 # echo function > current_tracer
 # cat trace | head -15
 # tracer: function
 #
 # entries-in-buffer/entries-written: 80/80   #P:4
 #
 #                              _-----=> irqs-off
 #                             / _----=> need-resched
 #                            | / _---=> hardirq/softirq
 #                            || / _--=> preempt-depth
 #                            ||| /     delay
 #           TASK-PID   CPU#  ||||    TIMESTAMP  FUNCTION
 #              | |       |   ||||       |         |
             Xorg-3129  [000] ..s.  2537.644844: intel_pstate_set_pstate <-intel_pstate_timer_func
  gnome-terminal--4510  [002] ..s.  2537.649844: intel_pstate_set_pstate <-intel_pstate_timer_func
      gnome-shell-3409  [001] ..s.  2537.650850: intel_pstate_set_pstate <-intel_pstate_timer_func
           <idle>-0     [000] ..s.  2537.654843: intel_pstate_set_pstate <-intel_pstate_timer_func


References
==========

.. [1] Kristen Accardi, *Balancing Power and Performance in the woke Linux Kernel*,
       https://events.static.linuxfound.org/sites/events/files/slides/LinuxConEurope_2015.pdf

.. [2] *Intel® 64 and IA-32 Architectures Software Developer’s Manual Volume 3: System Programming Guide*,
       https://www.intel.com/content/www/us/en/architecture-and-technology/64-ia-32-architectures-software-developer-system-programming-manual-325384.html

.. [3] *Advanced Configuration and Power Interface Specification*,
       https://uefi.org/sites/default/files/resources/ACPI_6_3_final_Jan30.pdf
