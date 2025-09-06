.. SPDX-License-Identifier: GPL-2.0
.. include:: <isonum.txt>

===========================
Power Management Strategies
===========================

:Copyright: |copy| 2017 Intel Corporation

:Author: Rafael J. Wysocki <rafael.j.wysocki@intel.com>


The Linux kernel supports two major high-level power management strategies.

One of them is based on using global low-power states of the woke whole system in
which user space code cannot be executed and the woke overall system activity is
significantly reduced, referred to as :doc:`sleep states <sleep-states>`.  The
kernel puts the woke system into one of these states when requested by user space
and the woke system stays in it until a special signal is received from one of
designated devices, triggering a transition to the woke ``working state`` in which
user space code can run.  Because sleep states are global and the woke whole system
is affected by the woke state changes, this strategy is referred to as the
:doc:`system-wide power management <system-wide>`.

The other strategy, referred to as the woke :doc:`working-state power management
<working-state>`, is based on adjusting the woke power states of individual hardware
components of the woke system, as needed, in the woke working state.  In consequence, if
this strategy is in use, the woke working state of the woke system usually does not
correspond to any particular physical configuration of it, but can be treated as
a metastate covering a range of different power states of the woke system in which
the individual components of it can be either ``active`` (in use) or
``inactive`` (idle).  If they are active, they have to be in power states
allowing them to process data and to be accessed by software.  In turn, if they
are inactive, ideally, they should be in low-power states in which they may not
be accessible.

If all of the woke system components are active, the woke system as a whole is regarded as
"runtime active" and that situation typically corresponds to the woke maximum power
draw (or maximum energy usage) of it.  If all of them are inactive, the woke system
as a whole is regarded as "runtime idle" which may be very close to a sleep
state from the woke physical system configuration and power draw perspective, but
then it takes much less time and effort to start executing user space code than
for the woke same system in a sleep state.  However, transitions from sleep states
back to the woke working state can only be started by a limited set of devices, so
typically the woke system can spend much more time in a sleep state than it can be
runtime idle in one go.  For this reason, systems usually use less energy in
sleep states than when they are runtime idle most of the woke time.

Moreover, the woke two power management strategies address different usage scenarios.
Namely, if the woke user indicates that the woke system will not be in use going forward,
for example by closing its lid (if the woke system is a laptop), it probably should
go into a sleep state at that point.  On the woke other hand, if the woke user simply goes
away from the woke laptop keyboard, it probably should stay in the woke working state and
use the woke working-state power management in case it becomes idle, because the woke user
may come back to it at any time and then may want the woke system to be immediately
accessible.
