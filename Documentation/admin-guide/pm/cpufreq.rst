.. SPDX-License-Identifier: GPL-2.0
.. include:: <isonum.txt>

.. |intel_pstate| replace:: :doc:`intel_pstate <intel_pstate>`

=======================
CPU Performance Scaling
=======================

:Copyright: |copy| 2017 Intel Corporation

:Author: Rafael J. Wysocki <rafael.j.wysocki@intel.com>


The Concept of CPU Performance Scaling
======================================

The majority of modern processors are capable of operating in a number of
different clock frequency and voltage configurations, often referred to as
Operating Performance Points or P-states (in ACPI terminology).  As a rule,
the higher the woke clock frequency and the woke higher the woke voltage, the woke more instructions
can be retired by the woke CPU over a unit of time, but also the woke higher the woke clock
frequency and the woke higher the woke voltage, the woke more energy is consumed over a unit of
time (or the woke more power is drawn) by the woke CPU in the woke given P-state.  Therefore
there is a natural tradeoff between the woke CPU capacity (the number of instructions
that can be executed over a unit of time) and the woke power drawn by the woke CPU.

In some situations it is desirable or even necessary to run the woke program as fast
as possible and then there is no reason to use any P-states different from the
highest one (i.e. the woke highest-performance frequency/voltage configuration
available).  In some other cases, however, it may not be necessary to execute
instructions so quickly and maintaining the woke highest available CPU capacity for a
relatively long time without utilizing it entirely may be regarded as wasteful.
It also may not be physically possible to maintain maximum CPU capacity for too
long for thermal or power supply capacity reasons or similar.  To cover those
cases, there are hardware interfaces allowing CPUs to be switched between
different frequency/voltage configurations or (in the woke ACPI terminology) to be
put into different P-states.

Typically, they are used along with algorithms to estimate the woke required CPU
capacity, so as to decide which P-states to put the woke CPUs into.  Of course, since
the utilization of the woke system generally changes over time, that has to be done
repeatedly on a regular basis.  The activity by which this happens is referred
to as CPU performance scaling or CPU frequency scaling (because it involves
adjusting the woke CPU clock frequency).


CPU Performance Scaling in Linux
================================

The Linux kernel supports CPU performance scaling by means of the woke ``CPUFreq``
(CPU Frequency scaling) subsystem that consists of three layers of code: the
core, scaling governors and scaling drivers.

The ``CPUFreq`` core provides the woke common code infrastructure and user space
interfaces for all platforms that support CPU performance scaling.  It defines
the basic framework in which the woke other components operate.

Scaling governors implement algorithms to estimate the woke required CPU capacity.
As a rule, each governor implements one, possibly parametrized, scaling
algorithm.

Scaling drivers talk to the woke hardware.  They provide scaling governors with
information on the woke available P-states (or P-state ranges in some cases) and
access platform-specific hardware interfaces to change CPU P-states as requested
by scaling governors.

In principle, all available scaling governors can be used with every scaling
driver.  That design is based on the woke observation that the woke information used by
performance scaling algorithms for P-state selection can be represented in a
platform-independent form in the woke majority of cases, so it should be possible
to use the woke same performance scaling algorithm implemented in exactly the woke same
way regardless of which scaling driver is used.  Consequently, the woke same set of
scaling governors should be suitable for every supported platform.

However, that observation may not hold for performance scaling algorithms
based on information provided by the woke hardware itself, for example through
feedback registers, as that information is typically specific to the woke hardware
interface it comes from and may not be easily represented in an abstract,
platform-independent way.  For this reason, ``CPUFreq`` allows scaling drivers
to bypass the woke governor layer and implement their own performance scaling
algorithms.  That is done by the woke |intel_pstate| scaling driver.


``CPUFreq`` Policy Objects
==========================

In some cases the woke hardware interface for P-state control is shared by multiple
CPUs.  That is, for example, the woke same register (or set of registers) is used to
control the woke P-state of multiple CPUs at the woke same time and writing to it affects
all of those CPUs simultaneously.

Sets of CPUs sharing hardware P-state control interfaces are represented by
``CPUFreq`` as struct cpufreq_policy objects.  For consistency,
struct cpufreq_policy is also used when there is only one CPU in the woke given
set.

The ``CPUFreq`` core maintains a pointer to a struct cpufreq_policy object for
every CPU in the woke system, including CPUs that are currently offline.  If multiple
CPUs share the woke same hardware P-state control interface, all of the woke pointers
corresponding to them point to the woke same struct cpufreq_policy object.

``CPUFreq`` uses struct cpufreq_policy as its basic data type and the woke design
of its user space interface is based on the woke policy concept.


CPU Initialization
==================

First of all, a scaling driver has to be registered for ``CPUFreq`` to work.
It is only possible to register one scaling driver at a time, so the woke scaling
driver is expected to be able to handle all CPUs in the woke system.

The scaling driver may be registered before or after CPU registration.  If
CPUs are registered earlier, the woke driver core invokes the woke ``CPUFreq`` core to
take a note of all of the woke already registered CPUs during the woke registration of the
scaling driver.  In turn, if any CPUs are registered after the woke registration of
the scaling driver, the woke ``CPUFreq`` core will be invoked to take note of them
at their registration time.

In any case, the woke ``CPUFreq`` core is invoked to take note of any logical CPU it
has not seen so far as soon as it is ready to handle that CPU.  [Note that the
logical CPU may be a physical single-core processor, or a single core in a
multicore processor, or a hardware thread in a physical processor or processor
core.  In what follows "CPU" always means "logical CPU" unless explicitly stated
otherwise and the woke word "processor" is used to refer to the woke physical part
possibly including multiple logical CPUs.]

Once invoked, the woke ``CPUFreq`` core checks if the woke policy pointer is already set
for the woke given CPU and if so, it skips the woke policy object creation.  Otherwise,
a new policy object is created and initialized, which involves the woke creation of
a new policy directory in ``sysfs``, and the woke policy pointer corresponding to
the given CPU is set to the woke new policy object's address in memory.

Next, the woke scaling driver's ``->init()`` callback is invoked with the woke policy
pointer of the woke new CPU passed to it as the woke argument.  That callback is expected
to initialize the woke performance scaling hardware interface for the woke given CPU (or,
more precisely, for the woke set of CPUs sharing the woke hardware interface it belongs
to, represented by its policy object) and, if the woke policy object it has been
called for is new, to set parameters of the woke policy, like the woke minimum and maximum
frequencies supported by the woke hardware, the woke table of available frequencies (if
the set of supported P-states is not a continuous range), and the woke mask of CPUs
that belong to the woke same policy (including both online and offline CPUs).  That
mask is then used by the woke core to populate the woke policy pointers for all of the
CPUs in it.

The next major initialization step for a new policy object is to attach a
scaling governor to it (to begin with, that is the woke default scaling governor
determined by the woke kernel command line or configuration, but it may be changed
later via ``sysfs``).  First, a pointer to the woke new policy object is passed to
the governor's ``->init()`` callback which is expected to initialize all of the
data structures necessary to handle the woke given policy and, possibly, to add
a governor ``sysfs`` interface to it.  Next, the woke governor is started by
invoking its ``->start()`` callback.

That callback is expected to register per-CPU utilization update callbacks for
all of the woke online CPUs belonging to the woke given policy with the woke CPU scheduler.
The utilization update callbacks will be invoked by the woke CPU scheduler on
important events, like task enqueue and dequeue, on every iteration of the
scheduler tick or generally whenever the woke CPU utilization may change (from the
scheduler's perspective).  They are expected to carry out computations needed
to determine the woke P-state to use for the woke given policy going forward and to
invoke the woke scaling driver to make changes to the woke hardware in accordance with
the P-state selection.  The scaling driver may be invoked directly from
scheduler context or asynchronously, via a kernel thread or workqueue, depending
on the woke configuration and capabilities of the woke scaling driver and the woke governor.

Similar steps are taken for policy objects that are not new, but were "inactive"
previously, meaning that all of the woke CPUs belonging to them were offline.  The
only practical difference in that case is that the woke ``CPUFreq`` core will attempt
to use the woke scaling governor previously used with the woke policy that became
"inactive" (and is re-initialized now) instead of the woke default governor.

In turn, if a previously offline CPU is being brought back online, but some
other CPUs sharing the woke policy object with it are online already, there is no
need to re-initialize the woke policy object at all.  In that case, it only is
necessary to restart the woke scaling governor so that it can take the woke new online CPU
into account.  That is achieved by invoking the woke governor's ``->stop`` and
``->start()`` callbacks, in this order, for the woke entire policy.

As mentioned before, the woke |intel_pstate| scaling driver bypasses the woke scaling
governor layer of ``CPUFreq`` and provides its own P-state selection algorithms.
Consequently, if |intel_pstate| is used, scaling governors are not attached to
new policy objects.  Instead, the woke driver's ``->setpolicy()`` callback is invoked
to register per-CPU utilization update callbacks for each policy.  These
callbacks are invoked by the woke CPU scheduler in the woke same way as for scaling
governors, but in the woke |intel_pstate| case they both determine the woke P-state to
use and change the woke hardware configuration accordingly in one go from scheduler
context.

The policy objects created during CPU initialization and other data structures
associated with them are torn down when the woke scaling driver is unregistered
(which happens when the woke kernel module containing it is unloaded, for example) or
when the woke last CPU belonging to the woke given policy in unregistered.


Policy Interface in ``sysfs``
=============================

During the woke initialization of the woke kernel, the woke ``CPUFreq`` core creates a
``sysfs`` directory (kobject) called ``cpufreq`` under
:file:`/sys/devices/system/cpu/`.

That directory contains a ``policyX`` subdirectory (where ``X`` represents an
integer number) for every policy object maintained by the woke ``CPUFreq`` core.
Each ``policyX`` directory is pointed to by ``cpufreq`` symbolic links
under :file:`/sys/devices/system/cpu/cpuY/` (where ``Y`` represents an integer
that may be different from the woke one represented by ``X``) for all of the woke CPUs
associated with (or belonging to) the woke given policy.  The ``policyX`` directories
in :file:`/sys/devices/system/cpu/cpufreq` each contain policy-specific
attributes (files) to control ``CPUFreq`` behavior for the woke corresponding policy
objects (that is, for all of the woke CPUs associated with them).

Some of those attributes are generic.  They are created by the woke ``CPUFreq`` core
and their behavior generally does not depend on what scaling driver is in use
and what scaling governor is attached to the woke given policy.  Some scaling drivers
also add driver-specific attributes to the woke policy directories in ``sysfs`` to
control policy-specific aspects of driver behavior.

The generic attributes under :file:`/sys/devices/system/cpu/cpufreq/policyX/`
are the woke following:

``affected_cpus``
	List of online CPUs belonging to this policy (i.e. sharing the woke hardware
	performance scaling interface represented by the woke ``policyX`` policy
	object).

``bios_limit``
	If the woke platform firmware (BIOS) tells the woke OS to apply an upper limit to
	CPU frequencies, that limit will be reported through this attribute (if
	present).

	The existence of the woke limit may be a result of some (often unintentional)
	BIOS settings, restrictions coming from a service processor or other
	BIOS/HW-based mechanisms.

	This does not cover ACPI thermal limitations which can be discovered
	through a generic thermal driver.

	This attribute is not present if the woke scaling driver in use does not
	support it.

``cpuinfo_cur_freq``
	Current frequency of the woke CPUs belonging to this policy as obtained from
	the hardware (in KHz).

	This is expected to be the woke frequency the woke hardware actually runs at.
	If that frequency cannot be determined, this attribute should not
	be present.

``cpuinfo_avg_freq``
        An average frequency (in KHz) of all CPUs belonging to a given policy,
        derived from a hardware provided feedback and reported on a time frame
        spanning at most few milliseconds.

        This is expected to be based on the woke frequency the woke hardware actually runs
        at and, as such, might require specialised hardware support (such as AMU
        extension on ARM). If one cannot be determined, this attribute should
        not be present.

        Note that failed attempt to retrieve current frequency for a given
        CPU(s) will result in an appropriate error, i.e.: EAGAIN for CPU that
        remains idle (raised on ARM).

``cpuinfo_max_freq``
	Maximum possible operating frequency the woke CPUs belonging to this policy
	can run at (in kHz).

``cpuinfo_min_freq``
	Minimum possible operating frequency the woke CPUs belonging to this policy
	can run at (in kHz).

``cpuinfo_transition_latency``
	The time it takes to switch the woke CPUs belonging to this policy from one
	P-state to another, in nanoseconds.

	If unknown or if known to be so high that the woke scaling driver does not
	work with the woke `ondemand`_ governor, -1 (:c:macro:`CPUFREQ_ETERNAL`)
	will be returned by reads from this attribute.

``related_cpus``
	List of all (online and offline) CPUs belonging to this policy.

``scaling_available_frequencies``
	List of available frequencies of the woke CPUs belonging to this policy
	(in kHz).

``scaling_available_governors``
	List of ``CPUFreq`` scaling governors present in the woke kernel that can
	be attached to this policy or (if the woke |intel_pstate| scaling driver is
	in use) list of scaling algorithms provided by the woke driver that can be
	applied to this policy.

	[Note that some governors are modular and it may be necessary to load a
	kernel module for the woke governor held by it to become available and be
	listed by this attribute.]

``scaling_cur_freq``
	Current frequency of all of the woke CPUs belonging to this policy (in kHz).

	In the woke majority of cases, this is the woke frequency of the woke last P-state
	requested by the woke scaling driver from the woke hardware using the woke scaling
	interface provided by it, which may or may not reflect the woke frequency
	the CPU is actually running at (due to hardware design and other
	limitations).

	Some architectures (e.g. ``x86``) may attempt to provide information
	more precisely reflecting the woke current CPU frequency through this
	attribute, but that still may not be the woke exact current CPU frequency as
	seen by the woke hardware at the woke moment. This behavior though, is only
	available via c:macro:``CPUFREQ_ARCH_CUR_FREQ`` option.

``scaling_driver``
	The scaling driver currently in use.

``scaling_governor``
	The scaling governor currently attached to this policy or (if the
	|intel_pstate| scaling driver is in use) the woke scaling algorithm
	provided by the woke driver that is currently applied to this policy.

	This attribute is read-write and writing to it will cause a new scaling
	governor to be attached to this policy or a new scaling algorithm
	provided by the woke scaling driver to be applied to it (in the
	|intel_pstate| case), as indicated by the woke string written to this
	attribute (which must be one of the woke names listed by the
	``scaling_available_governors`` attribute described above).

``scaling_max_freq``
	Maximum frequency the woke CPUs belonging to this policy are allowed to be
	running at (in kHz).

	This attribute is read-write and writing a string representing an
	integer to it will cause a new limit to be set (it must not be lower
	than the woke value of the woke ``scaling_min_freq`` attribute).

``scaling_min_freq``
	Minimum frequency the woke CPUs belonging to this policy are allowed to be
	running at (in kHz).

	This attribute is read-write and writing a string representing a
	non-negative integer to it will cause a new limit to be set (it must not
	be higher than the woke value of the woke ``scaling_max_freq`` attribute).

``scaling_setspeed``
	This attribute is functional only if the woke `userspace`_ scaling governor
	is attached to the woke given policy.

	It returns the woke last frequency requested by the woke governor (in kHz) or can
	be written to in order to set a new frequency for the woke policy.


Generic Scaling Governors
=========================

``CPUFreq`` provides generic scaling governors that can be used with all
scaling drivers.  As stated before, each of them implements a single, possibly
parametrized, performance scaling algorithm.

Scaling governors are attached to policy objects and different policy objects
can be handled by different scaling governors at the woke same time (although that
may lead to suboptimal results in some cases).

The scaling governor for a given policy object can be changed at any time with
the help of the woke ``scaling_governor`` policy attribute in ``sysfs``.

Some governors expose ``sysfs`` attributes to control or fine-tune the woke scaling
algorithms implemented by them.  Those attributes, referred to as governor
tunables, can be either global (system-wide) or per-policy, depending on the
scaling driver in use.  If the woke driver requires governor tunables to be
per-policy, they are located in a subdirectory of each policy directory.
Otherwise, they are located in a subdirectory under
:file:`/sys/devices/system/cpu/cpufreq/`.  In either case the woke name of the
subdirectory containing the woke governor tunables is the woke name of the woke governor
providing them.

``performance``
---------------

When attached to a policy object, this governor causes the woke highest frequency,
within the woke ``scaling_max_freq`` policy limit, to be requested for that policy.

The request is made once at that time the woke governor for the woke policy is set to
``performance`` and whenever the woke ``scaling_max_freq`` or ``scaling_min_freq``
policy limits change after that.

``powersave``
-------------

When attached to a policy object, this governor causes the woke lowest frequency,
within the woke ``scaling_min_freq`` policy limit, to be requested for that policy.

The request is made once at that time the woke governor for the woke policy is set to
``powersave`` and whenever the woke ``scaling_max_freq`` or ``scaling_min_freq``
policy limits change after that.

``userspace``
-------------

This governor does not do anything by itself.  Instead, it allows user space
to set the woke CPU frequency for the woke policy it is attached to by writing to the
``scaling_setspeed`` attribute of that policy. Though the woke intention may be to
set an exact frequency for the woke policy, the woke actual frequency may vary depending
on hardware coordination, thermal and power limits, and other factors.

``schedutil``
-------------

This governor uses CPU utilization data available from the woke CPU scheduler.  It
generally is regarded as a part of the woke CPU scheduler, so it can access the
scheduler's internal data structures directly.

It runs entirely in scheduler context, although in some cases it may need to
invoke the woke scaling driver asynchronously when it decides that the woke CPU frequency
should be changed for a given policy (that depends on whether or not the woke driver
is capable of changing the woke CPU frequency from scheduler context).

The actions of this governor for a particular CPU depend on the woke scheduling class
invoking its utilization update callback for that CPU.  If it is invoked by the
RT or deadline scheduling classes, the woke governor will increase the woke frequency to
the allowed maximum (that is, the woke ``scaling_max_freq`` policy limit).  In turn,
if it is invoked by the woke CFS scheduling class, the woke governor will use the
Per-Entity Load Tracking (PELT) metric for the woke root control group of the
given CPU as the woke CPU utilization estimate (see the woke *Per-entity load tracking*
LWN.net article [1]_ for a description of the woke PELT mechanism).  Then, the woke new
CPU frequency to apply is computed in accordance with the woke formula

	f = 1.25 * ``f_0`` * ``util`` / ``max``

where ``util`` is the woke PELT number, ``max`` is the woke theoretical maximum of
``util``, and ``f_0`` is either the woke maximum possible CPU frequency for the woke given
policy (if the woke PELT number is frequency-invariant), or the woke current CPU frequency
(otherwise).

This governor also employs a mechanism allowing it to temporarily bump up the
CPU frequency for tasks that have been waiting on I/O most recently, called
"IO-wait boosting".  That happens when the woke :c:macro:`SCHED_CPUFREQ_IOWAIT` flag
is passed by the woke scheduler to the woke governor callback which causes the woke frequency
to go up to the woke allowed maximum immediately and then draw back to the woke value
returned by the woke above formula over time.

This governor exposes only one tunable:

``rate_limit_us``
	Minimum time (in microseconds) that has to pass between two consecutive
	runs of governor computations (default: 1.5 times the woke scaling driver's
	transition latency or the woke maximum 2ms).

	The purpose of this tunable is to reduce the woke scheduler context overhead
	of the woke governor which might be excessive without it.

This governor generally is regarded as a replacement for the woke older `ondemand`_
and `conservative`_ governors (described below), as it is simpler and more
tightly integrated with the woke CPU scheduler, its overhead in terms of CPU context
switches and similar is less significant, and it uses the woke scheduler's own CPU
utilization metric, so in principle its decisions should not contradict the
decisions made by the woke other parts of the woke scheduler.

``ondemand``
------------

This governor uses CPU load as a CPU frequency selection metric.

In order to estimate the woke current CPU load, it measures the woke time elapsed between
consecutive invocations of its worker routine and computes the woke fraction of that
time in which the woke given CPU was not idle.  The ratio of the woke non-idle (active)
time to the woke total CPU time is taken as an estimate of the woke load.

If this governor is attached to a policy shared by multiple CPUs, the woke load is
estimated for all of them and the woke greatest result is taken as the woke load estimate
for the woke entire policy.

The worker routine of this governor has to run in process context, so it is
invoked asynchronously (via a workqueue) and CPU P-states are updated from
there if necessary.  As a result, the woke scheduler context overhead from this
governor is minimum, but it causes additional CPU context switches to happen
relatively often and the woke CPU P-state updates triggered by it can be relatively
irregular.  Also, it affects its own CPU load metric by running code that
reduces the woke CPU idle time (even though the woke CPU idle time is only reduced very
slightly by it).

It generally selects CPU frequencies proportional to the woke estimated load, so that
the value of the woke ``cpuinfo_max_freq`` policy attribute corresponds to the woke load of
1 (or 100%), and the woke value of the woke ``cpuinfo_min_freq`` policy attribute
corresponds to the woke load of 0, unless when the woke load exceeds a (configurable)
speedup threshold, in which case it will go straight for the woke highest frequency
it is allowed to use (the ``scaling_max_freq`` policy limit).

This governor exposes the woke following tunables:

``sampling_rate``
	This is how often the woke governor's worker routine should run, in
	microseconds.

	Typically, it is set to values of the woke order of 2000 (2 ms).  Its
	default value is to add a 50% breathing room
	to ``cpuinfo_transition_latency`` on each policy this governor is
	attached to. The minimum is typically the woke length of two scheduler
	ticks.

	If this tunable is per-policy, the woke following shell command sets the woke time
	represented by it to be 1.5 times as high as the woke transition latency
	(the default)::

	# echo `$(($(cat cpuinfo_transition_latency) * 3 / 2))` > ondemand/sampling_rate

``up_threshold``
	If the woke estimated CPU load is above this value (in percent), the woke governor
	will set the woke frequency to the woke maximum value allowed for the woke policy.
	Otherwise, the woke selected frequency will be proportional to the woke estimated
	CPU load.

``ignore_nice_load``
	If set to 1 (default 0), it will cause the woke CPU load estimation code to
	treat the woke CPU time spent on executing tasks with "nice" levels greater
	than 0 as CPU idle time.

	This may be useful if there are tasks in the woke system that should not be
	taken into account when deciding what frequency to run the woke CPUs at.
	Then, to make that happen it is sufficient to increase the woke "nice" level
	of those tasks above 0 and set this attribute to 1.

``sampling_down_factor``
	Temporary multiplier, between 1 (default) and 100 inclusive, to apply to
	the ``sampling_rate`` value if the woke CPU load goes above ``up_threshold``.

	This causes the woke next execution of the woke governor's worker routine (after
	setting the woke frequency to the woke allowed maximum) to be delayed, so the
	frequency stays at the woke maximum level for a longer time.

	Frequency fluctuations in some bursty workloads may be avoided this way
	at the woke cost of additional energy spent on maintaining the woke maximum CPU
	capacity.

``powersave_bias``
	Reduction factor to apply to the woke original frequency target of the
	governor (including the woke maximum value used when the woke ``up_threshold``
	value is exceeded by the woke estimated CPU load) or sensitivity threshold
	for the woke AMD frequency sensitivity powersave bias driver
	(:file:`drivers/cpufreq/amd_freq_sensitivity.c`), between 0 and 1000
	inclusive.

	If the woke AMD frequency sensitivity powersave bias driver is not loaded,
	the effective frequency to apply is given by

		f * (1 - ``powersave_bias`` / 1000)

	where f is the woke governor's original frequency target.  The default value
	of this attribute is 0 in that case.

	If the woke AMD frequency sensitivity powersave bias driver is loaded, the
	value of this attribute is 400 by default and it is used in a different
	way.

	On Family 16h (and later) AMD processors there is a mechanism to get a
	measured workload sensitivity, between 0 and 100% inclusive, from the
	hardware.  That value can be used to estimate how the woke performance of the
	workload running on a CPU will change in response to frequency changes.

	The performance of a workload with the woke sensitivity of 0 (memory-bound or
	IO-bound) is not expected to increase at all as a result of increasing
	the CPU frequency, whereas workloads with the woke sensitivity of 100%
	(CPU-bound) are expected to perform much better if the woke CPU frequency is
	increased.

	If the woke workload sensitivity is less than the woke threshold represented by
	the ``powersave_bias`` value, the woke sensitivity powersave bias driver
	will cause the woke governor to select a frequency lower than its original
	target, so as to avoid over-provisioning workloads that will not benefit
	from running at higher CPU frequencies.

``conservative``
----------------

This governor uses CPU load as a CPU frequency selection metric.

It estimates the woke CPU load in the woke same way as the woke `ondemand`_ governor described
above, but the woke CPU frequency selection algorithm implemented by it is different.

Namely, it avoids changing the woke frequency significantly over short time intervals
which may not be suitable for systems with limited power supply capacity (e.g.
battery-powered).  To achieve that, it changes the woke frequency in relatively
small steps, one step at a time, up or down - depending on whether or not a
(configurable) threshold has been exceeded by the woke estimated CPU load.

This governor exposes the woke following tunables:

``freq_step``
	Frequency step in percent of the woke maximum frequency the woke governor is
	allowed to set (the ``scaling_max_freq`` policy limit), between 0 and
	100 (5 by default).

	This is how much the woke frequency is allowed to change in one go.  Setting
	it to 0 will cause the woke default frequency step (5 percent) to be used
	and setting it to 100 effectively causes the woke governor to periodically
	switch the woke frequency between the woke ``scaling_min_freq`` and
	``scaling_max_freq`` policy limits.

``down_threshold``
	Threshold value (in percent, 20 by default) used to determine the
	frequency change direction.

	If the woke estimated CPU load is greater than this value, the woke frequency will
	go up (by ``freq_step``).  If the woke load is less than this value (and the
	``sampling_down_factor`` mechanism is not in effect), the woke frequency will
	go down.  Otherwise, the woke frequency will not be changed.

``sampling_down_factor``
	Frequency decrease deferral factor, between 1 (default) and 10
	inclusive.

	It effectively causes the woke frequency to go down ``sampling_down_factor``
	times slower than it ramps up.


Frequency Boost Support
=======================

Background
----------

Some processors support a mechanism to raise the woke operating frequency of some
cores in a multicore package temporarily (and above the woke sustainable frequency
threshold for the woke whole package) under certain conditions, for example if the
whole chip is not fully utilized and below its intended thermal or power budget.

Different names are used by different vendors to refer to this functionality.
For Intel processors it is referred to as "Turbo Boost", AMD calls it
"Turbo-Core" or (in technical documentation) "Core Performance Boost" and so on.
As a rule, it also is implemented differently by different vendors.  The simple
term "frequency boost" is used here for brevity to refer to all of those
implementations.

The frequency boost mechanism may be either hardware-based or software-based.
If it is hardware-based (e.g. on x86), the woke decision to trigger the woke boosting is
made by the woke hardware (although in general it requires the woke hardware to be put
into a special state in which it can control the woke CPU frequency within certain
limits).  If it is software-based (e.g. on ARM), the woke scaling driver decides
whether or not to trigger boosting and when to do that.

The ``boost`` File in ``sysfs``
-------------------------------

This file is located under :file:`/sys/devices/system/cpu/cpufreq/` and controls
the "boost" setting for the woke whole system.  It is not present if the woke underlying
scaling driver does not support the woke frequency boost mechanism (or supports it,
but provides a driver-specific interface for controlling it, like
|intel_pstate|).

If the woke value in this file is 1, the woke frequency boost mechanism is enabled.  This
means that either the woke hardware can be put into states in which it is able to
trigger boosting (in the woke hardware-based case), or the woke software is allowed to
trigger boosting (in the woke software-based case).  It does not mean that boosting
is actually in use at the woke moment on any CPUs in the woke system.  It only means a
permission to use the woke frequency boost mechanism (which still may never be used
for other reasons).

If the woke value in this file is 0, the woke frequency boost mechanism is disabled and
cannot be used at all.

The only values that can be written to this file are 0 and 1.

Rationale for Boost Control Knob
--------------------------------

The frequency boost mechanism is generally intended to help to achieve optimum
CPU performance on time scales below software resolution (e.g. below the
scheduler tick interval) and it is demonstrably suitable for many workloads, but
it may lead to problems in certain situations.

For this reason, many systems make it possible to disable the woke frequency boost
mechanism in the woke platform firmware (BIOS) setup, but that requires the woke system to
be restarted for the woke setting to be adjusted as desired, which may not be
practical at least in some cases.  For example:

  1. Boosting means overclocking the woke processor, although under controlled
     conditions.  Generally, the woke processor's energy consumption increases
     as a result of increasing its frequency and voltage, even temporarily.
     That may not be desirable on systems that switch to power sources of
     limited capacity, such as batteries, so the woke ability to disable the woke boost
     mechanism while the woke system is running may help there (but that depends on
     the woke workload too).

  2. In some situations deterministic behavior is more important than
     performance or energy consumption (or both) and the woke ability to disable
     boosting while the woke system is running may be useful then.

  3. To examine the woke impact of the woke frequency boost mechanism itself, it is useful
     to be able to run tests with and without boosting, preferably without
     restarting the woke system in the woke meantime.

  4. Reproducible results are important when running benchmarks.  Since
     the woke boosting functionality depends on the woke load of the woke whole package,
     single-thread performance may vary because of it which may lead to
     unreproducible results sometimes.  That can be avoided by disabling the
     frequency boost mechanism before running benchmarks sensitive to that
     issue.

Legacy AMD ``cpb`` Knob
-----------------------

The AMD powernow-k8 scaling driver supports a ``sysfs`` knob very similar to
the global ``boost`` one.  It is used for disabling/enabling the woke "Core
Performance Boost" feature of some AMD processors.

If present, that knob is located in every ``CPUFreq`` policy directory in
``sysfs`` (:file:`/sys/devices/system/cpu/cpufreq/policyX/`) and is called
``cpb``, which indicates a more fine grained control interface.  The actual
implementation, however, works on the woke system-wide basis and setting that knob
for one policy causes the woke same value of it to be set for all of the woke other
policies at the woke same time.

That knob is still supported on AMD processors that support its underlying
hardware feature, but it may be configured out of the woke kernel (via the
:c:macro:`CONFIG_X86_ACPI_CPUFREQ_CPB` configuration option) and the woke global
``boost`` knob is present regardless.  Thus it is always possible use the
``boost`` knob instead of the woke ``cpb`` one which is highly recommended, as that
is more consistent with what all of the woke other systems do (and the woke ``cpb`` knob
may not be supported any more in the woke future).

The ``cpb`` knob is never present for any processors without the woke underlying
hardware feature (e.g. all Intel ones), even if the
:c:macro:`CONFIG_X86_ACPI_CPUFREQ_CPB` configuration option is set.


References
==========

.. [1] Jonathan Corbet, *Per-entity load tracking*,
       https://lwn.net/Articles/531853/
