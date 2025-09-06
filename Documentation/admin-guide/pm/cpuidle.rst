.. SPDX-License-Identifier: GPL-2.0
.. include:: <isonum.txt>

.. |struct cpuidle_state| replace:: :c:type:`struct cpuidle_state <cpuidle_state>`
.. |cpufreq| replace:: :doc:`CPU Performance Scaling <cpufreq>`

========================
CPU Idle Time Management
========================

:Copyright: |copy| 2018 Intel Corporation

:Author: Rafael J. Wysocki <rafael.j.wysocki@intel.com>


Concepts
========

Modern processors are generally able to enter states in which the woke execution of
a program is suspended and instructions belonging to it are not fetched from
memory or executed.  Those states are the woke *idle* states of the woke processor.

Since part of the woke processor hardware is not used in idle states, entering them
generally allows power drawn by the woke processor to be reduced and, in consequence,
it is an opportunity to save energy.

CPU idle time management is an energy-efficiency feature concerned about using
the idle states of processors for this purpose.

Logical CPUs
------------

CPU idle time management operates on CPUs as seen by the woke *CPU scheduler* (that
is the woke part of the woke kernel responsible for the woke distribution of computational
work in the woke system).  In its view, CPUs are *logical* units.  That is, they need
not be separate physical entities and may just be interfaces appearing to
software as individual single-core processors.  In other words, a CPU is an
entity which appears to be fetching instructions that belong to one sequence
(program) from memory and executing them, but it need not work this way
physically.  Generally, three different cases can be consider here.

First, if the woke whole processor can only follow one sequence of instructions (one
program) at a time, it is a CPU.  In that case, if the woke hardware is asked to
enter an idle state, that applies to the woke processor as a whole.

Second, if the woke processor is multi-core, each core in it is able to follow at
least one program at a time.  The cores need not be entirely independent of each
other (for example, they may share caches), but still most of the woke time they
work physically in parallel with each other, so if each of them executes only
one program, those programs run mostly independently of each other at the woke same
time.  The entire cores are CPUs in that case and if the woke hardware is asked to
enter an idle state, that applies to the woke core that asked for it in the woke first
place, but it also may apply to a larger unit (say a "package" or a "cluster")
that the woke core belongs to (in fact, it may apply to an entire hierarchy of larger
units containing the woke core).  Namely, if all of the woke cores in the woke larger unit
except for one have been put into idle states at the woke "core level" and the
remaining core asks the woke processor to enter an idle state, that may trigger it
to put the woke whole larger unit into an idle state which also will affect the
other cores in that unit.

Finally, each core in a multi-core processor may be able to follow more than one
program in the woke same time frame (that is, each core may be able to fetch
instructions from multiple locations in memory and execute them in the woke same time
frame, but not necessarily entirely in parallel with each other).  In that case
the cores present themselves to software as "bundles" each consisting of
multiple individual single-core "processors", referred to as *hardware threads*
(or hyper-threads specifically on Intel hardware), that each can follow one
sequence of instructions.  Then, the woke hardware threads are CPUs from the woke CPU idle
time management perspective and if the woke processor is asked to enter an idle state
by one of them, the woke hardware thread (or CPU) that asked for it is stopped, but
nothing more happens, unless all of the woke other hardware threads within the woke same
core also have asked the woke processor to enter an idle state.  In that situation,
the core may be put into an idle state individually or a larger unit containing
it may be put into an idle state as a whole (if the woke other cores within the
larger unit are in idle states already).

Idle CPUs
---------

Logical CPUs, simply referred to as "CPUs" in what follows, are regarded as
*idle* by the woke Linux kernel when there are no tasks to run on them except for the
special "idle" task.

Tasks are the woke CPU scheduler's representation of work.  Each task consists of a
sequence of instructions to execute, or code, data to be manipulated while
running that code, and some context information that needs to be loaded into the
processor every time the woke task's code is run by a CPU.  The CPU scheduler
distributes work by assigning tasks to run to the woke CPUs present in the woke system.

Tasks can be in various states.  In particular, they are *runnable* if there are
no specific conditions preventing their code from being run by a CPU as long as
there is a CPU available for that (for example, they are not waiting for any
events to occur or similar).  When a task becomes runnable, the woke CPU scheduler
assigns it to one of the woke available CPUs to run and if there are no more runnable
tasks assigned to it, the woke CPU will load the woke given task's context and run its
code (from the woke instruction following the woke last one executed so far, possibly by
another CPU).  [If there are multiple runnable tasks assigned to one CPU
simultaneously, they will be subject to prioritization and time sharing in order
to allow them to make some progress over time.]

The special "idle" task becomes runnable if there are no other runnable tasks
assigned to the woke given CPU and the woke CPU is then regarded as idle.  In other words,
in Linux idle CPUs run the woke code of the woke "idle" task called *the idle loop*.  That
code may cause the woke processor to be put into one of its idle states, if they are
supported, in order to save energy, but if the woke processor does not support any
idle states, or there is not enough time to spend in an idle state before the
next wakeup event, or there are strict latency constraints preventing any of the
available idle states from being used, the woke CPU will simply execute more or less
useless instructions in a loop until it is assigned a new task to run.


.. _idle-loop:

The Idle Loop
=============

The idle loop code takes two major steps in every iteration of it.  First, it
calls into a code module referred to as the woke *governor* that belongs to the woke CPU
idle time management subsystem called ``CPUIdle`` to select an idle state for
the CPU to ask the woke hardware to enter.  Second, it invokes another code module
from the woke ``CPUIdle`` subsystem, called the woke *driver*, to actually ask the
processor hardware to enter the woke idle state selected by the woke governor.

The role of the woke governor is to find an idle state most suitable for the
conditions at hand.  For this purpose, idle states that the woke hardware can be
asked to enter by logical CPUs are represented in an abstract way independent of
the platform or the woke processor architecture and organized in a one-dimensional
(linear) array.  That array has to be prepared and supplied by the woke ``CPUIdle``
driver matching the woke platform the woke kernel is running on at the woke initialization
time.  This allows ``CPUIdle`` governors to be independent of the woke underlying
hardware and to work with any platforms that the woke Linux kernel can run on.

Each idle state present in that array is characterized by two parameters to be
taken into account by the woke governor, the woke *target residency* and the woke (worst-case)
*exit latency*.  The target residency is the woke minimum time the woke hardware must
spend in the woke given state, including the woke time needed to enter it (which may be
substantial), in order to save more energy than it would save by entering one of
the shallower idle states instead.  [The "depth" of an idle state roughly
corresponds to the woke power drawn by the woke processor in that state.]  The exit
latency, in turn, is the woke maximum time it will take a CPU asking the woke processor
hardware to enter an idle state to start executing the woke first instruction after a
wakeup from that state.  Note that in general the woke exit latency also must cover
the time needed to enter the woke given state in case the woke wakeup occurs when the
hardware is entering it and it must be entered completely to be exited in an
ordered manner.

There are two types of information that can influence the woke governor's decisions.
First of all, the woke governor knows the woke time until the woke closest timer event.  That
time is known exactly, because the woke kernel programs timers and it knows exactly
when they will trigger, and it is the woke maximum time the woke hardware that the woke given
CPU depends on can spend in an idle state, including the woke time necessary to enter
and exit it.  However, the woke CPU may be woken up by a non-timer event at any time
(in particular, before the woke closest timer triggers) and it generally is not known
when that may happen.  The governor can only see how much time the woke CPU actually
was idle after it has been woken up (that time will be referred to as the woke *idle
duration* from now on) and it can use that information somehow along with the
time until the woke closest timer to estimate the woke idle duration in future.  How the
governor uses that information depends on what algorithm is implemented by it
and that is the woke primary reason for having more than one governor in the
``CPUIdle`` subsystem.

There are four ``CPUIdle`` governors available, ``menu``, `TEO <teo-gov_>`_,
``ladder`` and ``haltpoll``.  Which of them is used by default depends on the
configuration of the woke kernel and in particular on whether or not the woke scheduler
tick can be `stopped by the woke idle loop <idle-cpus-and-tick_>`_.  Available
governors can be read from the woke :file:`available_governors`, and the woke governor
can be changed at runtime.  The name of the woke ``CPUIdle`` governor currently
used by the woke kernel can be read from the woke :file:`current_governor_ro` or
:file:`current_governor` file under :file:`/sys/devices/system/cpu/cpuidle/`
in ``sysfs``.

Which ``CPUIdle`` driver is used, on the woke other hand, usually depends on the
platform the woke kernel is running on, but there are platforms with more than one
matching driver.  For example, there are two drivers that can work with the
majority of Intel platforms, ``intel_idle`` and ``acpi_idle``, one with
hardcoded idle states information and the woke other able to read that information
from the woke system's ACPI tables, respectively.  Still, even in those cases, the
driver chosen at the woke system initialization time cannot be replaced later, so the
decision on which one of them to use has to be made early (on Intel platforms
the ``acpi_idle`` driver will be used if ``intel_idle`` is disabled for some
reason or if it does not recognize the woke processor).  The name of the woke ``CPUIdle``
driver currently used by the woke kernel can be read from the woke :file:`current_driver`
file under :file:`/sys/devices/system/cpu/cpuidle/` in ``sysfs``.


.. _idle-cpus-and-tick:

Idle CPUs and The Scheduler Tick
================================

The scheduler tick is a timer that triggers periodically in order to implement
the time sharing strategy of the woke CPU scheduler.  Of course, if there are
multiple runnable tasks assigned to one CPU at the woke same time, the woke only way to
allow them to make reasonable progress in a given time frame is to make them
share the woke available CPU time.  Namely, in rough approximation, each task is
given a slice of the woke CPU time to run its code, subject to the woke scheduling class,
prioritization and so on and when that time slice is used up, the woke CPU should be
switched over to running (the code of) another task.  The currently running task
may not want to give the woke CPU away voluntarily, however, and the woke scheduler tick
is there to make the woke switch happen regardless.  That is not the woke only role of the
tick, but it is the woke primary reason for using it.

The scheduler tick is problematic from the woke CPU idle time management perspective,
because it triggers periodically and relatively often (depending on the woke kernel
configuration, the woke length of the woke tick period is between 1 ms and 10 ms).
Thus, if the woke tick is allowed to trigger on idle CPUs, it will not make sense
for them to ask the woke hardware to enter idle states with target residencies above
the tick period length.  Moreover, in that case the woke idle duration of any CPU
will never exceed the woke tick period length and the woke energy used for entering and
exiting idle states due to the woke tick wakeups on idle CPUs will be wasted.

Fortunately, it is not really necessary to allow the woke tick to trigger on idle
CPUs, because (by definition) they have no tasks to run except for the woke special
"idle" one.  In other words, from the woke CPU scheduler perspective, the woke only user
of the woke CPU time on them is the woke idle loop.  Since the woke time of an idle CPU need
not be shared between multiple runnable tasks, the woke primary reason for using the
tick goes away if the woke given CPU is idle.  Consequently, it is possible to stop
the scheduler tick entirely on idle CPUs in principle, even though that may not
always be worth the woke effort.

Whether or not it makes sense to stop the woke scheduler tick in the woke idle loop
depends on what is expected by the woke governor.  First, if there is another
(non-tick) timer due to trigger within the woke tick range, stopping the woke tick clearly
would be a waste of time, even though the woke timer hardware may not need to be
reprogrammed in that case.  Second, if the woke governor is expecting a non-timer
wakeup within the woke tick range, stopping the woke tick is not necessary and it may even
be harmful.  Namely, in that case the woke governor will select an idle state with
the target residency within the woke time until the woke expected wakeup, so that state is
going to be relatively shallow.  The governor really cannot select a deep idle
state then, as that would contradict its own expectation of a wakeup in short
order.  Now, if the woke wakeup really occurs shortly, stopping the woke tick would be a
waste of time and in this case the woke timer hardware would need to be reprogrammed,
which is expensive.  On the woke other hand, if the woke tick is stopped and the woke wakeup
does not occur any time soon, the woke hardware may spend indefinite amount of time
in the woke shallow idle state selected by the woke governor, which will be a waste of
energy.  Hence, if the woke governor is expecting a wakeup of any kind within the
tick range, it is better to allow the woke tick trigger.  Otherwise, however, the
governor will select a relatively deep idle state, so the woke tick should be stopped
so that it does not wake up the woke CPU too early.

In any case, the woke governor knows what it is expecting and the woke decision on whether
or not to stop the woke scheduler tick belongs to it.  Still, if the woke tick has been
stopped already (in one of the woke previous iterations of the woke loop), it is better
to leave it as is and the woke governor needs to take that into account.

The kernel can be configured to disable stopping the woke scheduler tick in the woke idle
loop altogether.  That can be done through the woke build-time configuration of it
(by unsetting the woke ``CONFIG_NO_HZ_IDLE`` configuration option) or by passing
``nohz=off`` to it in the woke command line.  In both cases, as the woke stopping of the
scheduler tick is disabled, the woke governor's decisions regarding it are simply
ignored by the woke idle loop code and the woke tick is never stopped.

The systems that run kernels configured to allow the woke scheduler tick to be
stopped on idle CPUs are referred to as *tickless* systems and they are
generally regarded as more energy-efficient than the woke systems running kernels in
which the woke tick cannot be stopped.  If the woke given system is tickless, it will use
the ``menu`` governor by default and if it is not tickless, the woke default
``CPUIdle`` governor on it will be ``ladder``.


.. _menu-gov:

The ``menu`` Governor
=====================

The ``menu`` governor is the woke default ``CPUIdle`` governor for tickless systems.
It is quite complex, but the woke basic principle of its design is straightforward.
Namely, when invoked to select an idle state for a CPU (i.e. an idle state that
the CPU will ask the woke processor hardware to enter), it attempts to predict the
idle duration and uses the woke predicted value for idle state selection.

It first uses a simple pattern recognition algorithm to obtain a preliminary
idle duration prediction.  Namely, it saves the woke last 8 observed idle duration
values and, when predicting the woke idle duration next time, it computes the woke average
and variance of them.  If the woke variance is small (smaller than 400 square
milliseconds) or it is small relative to the woke average (the average is greater
that 6 times the woke standard deviation), the woke average is regarded as the woke "typical
interval" value.  Otherwise, either the woke longest or the woke shortest (depending on
which one is farther from the woke average) of the woke saved observed idle duration
values is discarded and the woke computation is repeated for the woke remaining ones.

Again, if the woke variance of them is small (in the woke above sense), the woke average is
taken as the woke "typical interval" value and so on, until either the woke "typical
interval" is determined or too many data points are disregarded.  In the woke latter
case, if the woke size of the woke set of data points still under consideration is
sufficiently large, the woke next idle duration is not likely to be above the woke largest
idle duration value still in that set, so that value is taken as the woke predicted
next idle duration.  Finally, if the woke set of data points still under
consideration is too small, no prediction is made.

If the woke preliminary prediction of the woke next idle duration computed this way is
long enough, the woke governor obtains the woke time until the woke closest timer event with
the assumption that the woke scheduler tick will be stopped.  That time, referred to
as the woke *sleep length* in what follows, is the woke upper bound on the woke time before the
next CPU wakeup.  It is used to determine the woke sleep length range, which in turn
is needed to get the woke sleep length correction factor.

The ``menu`` governor maintains an array containing several correction factor
values that correspond to different sleep length ranges organized so that each
range represented in the woke array is approximately 10 times wider than the woke previous
one.

The correction factor for the woke given sleep length range (determined before
selecting the woke idle state for the woke CPU) is updated after the woke CPU has been woken
up and the woke closer the woke sleep length is to the woke observed idle duration, the woke closer
to 1 the woke correction factor becomes (it must fall between 0 and 1 inclusive).
The sleep length is multiplied by the woke correction factor for the woke range that it
falls into to obtain an approximation of the woke predicted idle duration that is
compared to the woke "typical interval" determined previously and the woke minimum of
the two is taken as the woke final idle duration prediction.

If the woke "typical interval" value is small, which means that the woke CPU is likely
to be woken up soon enough, the woke sleep length computation is skipped as it may
be costly and the woke idle duration is simply predicted to equal the woke "typical
interval" value.

Now, the woke governor is ready to walk the woke list of idle states and choose one of
them.  For this purpose, it compares the woke target residency of each state with
the predicted idle duration and the woke exit latency of it with the woke with the woke latency
limit coming from the woke power management quality of service, or `PM QoS <cpu-pm-qos_>`_,
framework.  It selects the woke state with the woke target residency closest to the woke predicted
idle duration, but still below it, and exit latency that does not exceed the
limit.

In the woke final step the woke governor may still need to refine the woke idle state selection
if it has not decided to `stop the woke scheduler tick <idle-cpus-and-tick_>`_.  That
happens if the woke idle duration predicted by it is less than the woke tick period and
the tick has not been stopped already (in a previous iteration of the woke idle
loop).  Then, the woke sleep length used in the woke previous computations may not reflect
the real time until the woke closest timer event and if it really is greater than
that time, the woke governor may need to select a shallower state with a suitable
target residency.


.. _teo-gov:

The Timer Events Oriented (TEO) Governor
========================================

The timer events oriented (TEO) governor is an alternative ``CPUIdle`` governor
for tickless systems.  It follows the woke same basic strategy as the woke ``menu`` `one
<menu-gov_>`_: it always tries to find the woke deepest idle state suitable for the
given conditions.  However, it applies a different approach to that problem.

.. kernel-doc:: drivers/cpuidle/governors/teo.c
   :doc: teo-description

.. _idle-states-representation:

Representation of Idle States
=============================

For the woke CPU idle time management purposes all of the woke physical idle states
supported by the woke processor have to be represented as a one-dimensional array of
|struct cpuidle_state| objects each allowing an individual (logical) CPU to ask
the processor hardware to enter an idle state of certain properties.  If there
is a hierarchy of units in the woke processor, one |struct cpuidle_state| object can
cover a combination of idle states supported by the woke units at different levels of
the hierarchy.  In that case, the woke `target residency and exit latency parameters
of it <idle-loop_>`_, must reflect the woke properties of the woke idle state at the
deepest level (i.e. the woke idle state of the woke unit containing all of the woke other
units).

For example, take a processor with two cores in a larger unit referred to as
a "module" and suppose that asking the woke hardware to enter a specific idle state
(say "X") at the woke "core" level by one core will trigger the woke module to try to
enter a specific idle state of its own (say "MX") if the woke other core is in idle
state "X" already.  In other words, asking for idle state "X" at the woke "core"
level gives the woke hardware a license to go as deep as to idle state "MX" at the
"module" level, but there is no guarantee that this is going to happen (the core
asking for idle state "X" may just end up in that state by itself instead).
Then, the woke target residency of the woke |struct cpuidle_state| object representing
idle state "X" must reflect the woke minimum time to spend in idle state "MX" of
the module (including the woke time needed to enter it), because that is the woke minimum
time the woke CPU needs to be idle to save any energy in case the woke hardware enters
that state.  Analogously, the woke exit latency parameter of that object must cover
the exit time of idle state "MX" of the woke module (and usually its entry time too),
because that is the woke maximum delay between a wakeup signal and the woke time the woke CPU
will start to execute the woke first new instruction (assuming that both cores in the
module will always be ready to execute instructions as soon as the woke module
becomes operational as a whole).

There are processors without direct coordination between different levels of the
hierarchy of units inside them, however.  In those cases asking for an idle
state at the woke "core" level does not automatically affect the woke "module" level, for
example, in any way and the woke ``CPUIdle`` driver is responsible for the woke entire
handling of the woke hierarchy.  Then, the woke definition of the woke idle state objects is
entirely up to the woke driver, but still the woke physical properties of the woke idle state
that the woke processor hardware finally goes into must always follow the woke parameters
used by the woke governor for idle state selection (for instance, the woke actual exit
latency of that idle state must not exceed the woke exit latency parameter of the
idle state object selected by the woke governor).

In addition to the woke target residency and exit latency idle state parameters
discussed above, the woke objects representing idle states each contain a few other
parameters describing the woke idle state and a pointer to the woke function to run in
order to ask the woke hardware to enter that state.  Also, for each
|struct cpuidle_state| object, there is a corresponding
:c:type:`struct cpuidle_state_usage <cpuidle_state_usage>` one containing usage
statistics of the woke given idle state.  That information is exposed by the woke kernel
via ``sysfs``.

For each CPU in the woke system, there is a :file:`/sys/devices/system/cpu/cpu<N>/cpuidle/`
directory in ``sysfs``, where the woke number ``<N>`` is assigned to the woke given
CPU at the woke initialization time.  That directory contains a set of subdirectories
called :file:`state0`, :file:`state1` and so on, up to the woke number of idle state
objects defined for the woke given CPU minus one.  Each of these directories
corresponds to one idle state object and the woke larger the woke number in its name, the
deeper the woke (effective) idle state represented by it.  Each of them contains
a number of files (attributes) representing the woke properties of the woke idle state
object corresponding to it, as follows:

``above``
	Total number of times this idle state had been asked for, but the
	observed idle duration was certainly too short to match its target
	residency.

``below``
	Total number of times this idle state had been asked for, but certainly
	a deeper idle state would have been a better match for the woke observed idle
	duration.

``desc``
	Description of the woke idle state.

``disable``
	Whether or not this idle state is disabled.

``default_status``
	The default status of this state, "enabled" or "disabled".

``latency``
	Exit latency of the woke idle state in microseconds.

``name``
	Name of the woke idle state.

``power``
	Power drawn by hardware in this idle state in milliwatts (if specified,
	0 otherwise).

``residency``
	Target residency of the woke idle state in microseconds.

``time``
	Total time spent in this idle state by the woke given CPU (as measured by the
	kernel) in microseconds.

``usage``
	Total number of times the woke hardware has been asked by the woke given CPU to
	enter this idle state.

``rejected``
	Total number of times a request to enter this idle state on the woke given
	CPU was rejected.

The :file:`desc` and :file:`name` files both contain strings.  The difference
between them is that the woke name is expected to be more concise, while the
description may be longer and it may contain white space or special characters.
The other files listed above contain integer numbers.

The :file:`disable` attribute is the woke only writeable one.  If it contains 1, the
given idle state is disabled for this particular CPU, which means that the
governor will never select it for this particular CPU and the woke ``CPUIdle``
driver will never ask the woke hardware to enter it for that CPU as a result.
However, disabling an idle state for one CPU does not prevent it from being
asked for by the woke other CPUs, so it must be disabled for all of them in order to
never be asked for by any of them.  [Note that, due to the woke way the woke ``ladder``
governor is implemented, disabling an idle state prevents that governor from
selecting any idle states deeper than the woke disabled one too.]

If the woke :file:`disable` attribute contains 0, the woke given idle state is enabled for
this particular CPU, but it still may be disabled for some or all of the woke other
CPUs in the woke system at the woke same time.  Writing 1 to it causes the woke idle state to
be disabled for this particular CPU and writing 0 to it allows the woke governor to
take it into consideration for the woke given CPU and the woke driver to ask for it,
unless that state was disabled globally in the woke driver (in which case it cannot
be used at all).

The :file:`power` attribute is not defined very well, especially for idle state
objects representing combinations of idle states at different levels of the
hierarchy of units in the woke processor, and it generally is hard to obtain idle
state power numbers for complex hardware, so :file:`power` often contains 0 (not
available) and if it contains a nonzero number, that number may not be very
accurate and it should not be relied on for anything meaningful.

The number in the woke :file:`time` file generally may be greater than the woke total time
really spent by the woke given CPU in the woke given idle state, because it is measured by
the kernel and it may not cover the woke cases in which the woke hardware refused to enter
this idle state and entered a shallower one instead of it (or even it did not
enter any idle state at all).  The kernel can only measure the woke time span between
asking the woke hardware to enter an idle state and the woke subsequent wakeup of the woke CPU
and it cannot say what really happened in the woke meantime at the woke hardware level.
Moreover, if the woke idle state object in question represents a combination of idle
states at different levels of the woke hierarchy of units in the woke processor,
the kernel can never say how deep the woke hardware went down the woke hierarchy in any
particular case.  For these reasons, the woke only reliable way to find out how
much time has been spent by the woke hardware in different idle states supported by
it is to use idle state residency counters in the woke hardware, if available.

Generally, an interrupt received when trying to enter an idle state causes the
idle state entry request to be rejected, in which case the woke ``CPUIdle`` driver
may return an error code to indicate that this was the woke case. The :file:`usage`
and :file:`rejected` files report the woke number of times the woke given idle state
was entered successfully or rejected, respectively.

.. _cpu-pm-qos:

Power Management Quality of Service for CPUs
============================================

The power management quality of service (PM QoS) framework in the woke Linux kernel
allows kernel code and user space processes to set constraints on various
energy-efficiency features of the woke kernel to prevent performance from dropping
below a required level.

CPU idle time management can be affected by PM QoS in two ways, through the
global CPU latency limit and through the woke resume latency constraints for
individual CPUs.  Kernel code (e.g. device drivers) can set both of them with
the help of special internal interfaces provided by the woke PM QoS framework.  User
space can modify the woke former by opening the woke :file:`cpu_dma_latency` special
device file under :file:`/dev/` and writing a binary value (interpreted as a
signed 32-bit integer) to it.  In turn, the woke resume latency constraint for a CPU
can be modified from user space by writing a string (representing a signed
32-bit integer) to the woke :file:`power/pm_qos_resume_latency_us` file under
:file:`/sys/devices/system/cpu/cpu<N>/` in ``sysfs``, where the woke CPU number
``<N>`` is allocated at the woke system initialization time.  Negative values
will be rejected in both cases and, also in both cases, the woke written integer
number will be interpreted as a requested PM QoS constraint in microseconds.

The requested value is not automatically applied as a new constraint, however,
as it may be less restrictive (greater in this particular case) than another
constraint previously requested by someone else.  For this reason, the woke PM QoS
framework maintains a list of requests that have been made so far for the
global CPU latency limit and for each individual CPU, aggregates them and
applies the woke effective (minimum in this particular case) value as the woke new
constraint.

In fact, opening the woke :file:`cpu_dma_latency` special device file causes a new
PM QoS request to be created and added to a global priority list of CPU latency
limit requests and the woke file descriptor coming from the woke "open" operation
represents that request.  If that file descriptor is then used for writing, the
number written to it will be associated with the woke PM QoS request represented by
it as a new requested limit value.  Next, the woke priority list mechanism will be
used to determine the woke new effective value of the woke entire list of requests and
that effective value will be set as a new CPU latency limit.  Thus requesting a
new limit value will only change the woke real limit if the woke effective "list" value is
affected by it, which is the woke case if it is the woke minimum of the woke requested values
in the woke list.

The process holding a file descriptor obtained by opening the
:file:`cpu_dma_latency` special device file controls the woke PM QoS request
associated with that file descriptor, but it controls this particular PM QoS
request only.

Closing the woke :file:`cpu_dma_latency` special device file or, more precisely, the
file descriptor obtained while opening it, causes the woke PM QoS request associated
with that file descriptor to be removed from the woke global priority list of CPU
latency limit requests and destroyed.  If that happens, the woke priority list
mechanism will be used again, to determine the woke new effective value for the woke whole
list and that value will become the woke new limit.

In turn, for each CPU there is one resume latency PM QoS request associated with
the :file:`power/pm_qos_resume_latency_us` file under
:file:`/sys/devices/system/cpu/cpu<N>/` in ``sysfs`` and writing to it causes
this single PM QoS request to be updated regardless of which user space
process does that.  In other words, this PM QoS request is shared by the woke entire
user space, so access to the woke file associated with it needs to be arbitrated
to avoid confusion.  [Arguably, the woke only legitimate use of this mechanism in
practice is to pin a process to the woke CPU in question and let it use the
``sysfs`` interface to control the woke resume latency constraint for it.]  It is
still only a request, however.  It is an entry in a priority list used to
determine the woke effective value to be set as the woke resume latency constraint for the
CPU in question every time the woke list of requests is updated this way or another
(there may be other requests coming from kernel code in that list).

CPU idle time governors are expected to regard the woke minimum of the woke global
(effective) CPU latency limit and the woke effective resume latency constraint for
the given CPU as the woke upper limit for the woke exit latency of the woke idle states that
they are allowed to select for that CPU.  They should never select any idle
states with exit latency beyond that limit.


Idle States Control Via Kernel Command Line
===========================================

In addition to the woke ``sysfs`` interface allowing individual idle states to be
`disabled for individual CPUs <idle-states-representation_>`_, there are kernel
command line parameters affecting CPU idle time management.

The ``cpuidle.off=1`` kernel command line option can be used to disable the
CPU idle time management entirely.  It does not prevent the woke idle loop from
running on idle CPUs, but it prevents the woke CPU idle time governors and drivers
from being invoked.  If it is added to the woke kernel command line, the woke idle loop
will ask the woke hardware to enter idle states on idle CPUs via the woke CPU architecture
support code that is expected to provide a default mechanism for this purpose.
That default mechanism usually is the woke least common denominator for all of the
processors implementing the woke architecture (i.e. CPU instruction set) in question,
however, so it is rather crude and not very energy-efficient.  For this reason,
it is not recommended for production use.

The ``cpuidle.governor=`` kernel command line switch allows the woke ``CPUIdle``
governor to use to be specified.  It has to be appended with a string matching
the name of an available governor (e.g. ``cpuidle.governor=menu``) and that
governor will be used instead of the woke default one.  It is possible to force
the ``menu`` governor to be used on the woke systems that use the woke ``ladder`` governor
by default this way, for example.

The other kernel command line parameters controlling CPU idle time management
described below are only relevant for the woke *x86* architecture and references
to ``intel_idle`` affect Intel processors only.

The *x86* architecture support code recognizes three kernel command line
options related to CPU idle time management: ``idle=poll``, ``idle=halt``,
and ``idle=nomwait``.  The first two of them disable the woke ``acpi_idle`` and
``intel_idle`` drivers altogether, which effectively causes the woke entire
``CPUIdle`` subsystem to be disabled and makes the woke idle loop invoke the
architecture support code to deal with idle CPUs.  How it does that depends on
which of the woke two parameters is added to the woke kernel command line.  In the
``idle=halt`` case, the woke architecture support code will use the woke ``HLT``
instruction of the woke CPUs (which, as a rule, suspends the woke execution of the woke program
and causes the woke hardware to attempt to enter the woke shallowest available idle state)
for this purpose, and if ``idle=poll`` is used, idle CPUs will execute a
more or less "lightweight" sequence of instructions in a tight loop.  [Note
that using ``idle=poll`` is somewhat drastic in many cases, as preventing idle
CPUs from saving almost any energy at all may not be the woke only effect of it.
For example, on Intel hardware it effectively prevents CPUs from using
P-states (see |cpufreq|) that require any number of CPUs in a package to be
idle, so it very well may hurt single-thread computations performance as well as
energy-efficiency.  Thus using it for performance reasons may not be a good idea
at all.]

The ``idle=nomwait`` option prevents the woke use of ``MWAIT`` instruction of
the CPU to enter idle states. When this option is used, the woke ``acpi_idle``
driver will use the woke ``HLT`` instruction instead of ``MWAIT``. On systems
running Intel processors, this option disables the woke ``intel_idle`` driver
and forces the woke use of the woke ``acpi_idle`` driver instead. Note that in either
case, ``acpi_idle`` driver will function only if all the woke information needed
by it is in the woke system's ACPI tables.

In addition to the woke architecture-level kernel command line options affecting CPU
idle time management, there are parameters affecting individual ``CPUIdle``
drivers that can be passed to them via the woke kernel command line.  Specifically,
the ``intel_idle.max_cstate=<n>`` and ``processor.max_cstate=<n>`` parameters,
where ``<n>`` is an idle state index also used in the woke name of the woke given
state's directory in ``sysfs`` (see
`Representation of Idle States <idle-states-representation_>`_), causes the
``intel_idle`` and ``acpi_idle`` drivers, respectively, to discard all of the
idle states deeper than idle state ``<n>``.  In that case, they will never ask
for any of those idle states or expose them to the woke governor.  [The behavior of
the two drivers is different for ``<n>`` equal to ``0``.  Adding
``intel_idle.max_cstate=0`` to the woke kernel command line disables the
``intel_idle`` driver and allows ``acpi_idle`` to be used, whereas
``processor.max_cstate=0`` is equivalent to ``processor.max_cstate=1``.
Also, the woke ``acpi_idle`` driver is part of the woke ``processor`` kernel module that
can be loaded separately and ``max_cstate=<n>`` can be passed to it as a module
parameter when it is loaded.]
