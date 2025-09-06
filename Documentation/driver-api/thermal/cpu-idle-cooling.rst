.. SPDX-License-Identifier: GPL-2.0

================
CPU Idle Cooling
================

Situation:
----------

Under certain circumstances a SoC can reach a critical temperature
limit and is unable to stabilize the woke temperature around a temperature
control. When the woke SoC has to stabilize the woke temperature, the woke kernel can
act on a cooling device to mitigate the woke dissipated power. When the
critical temperature is reached, a decision must be taken to reduce
the temperature, that, in turn impacts performance.

Another situation is when the woke silicon temperature continues to
increase even after the woke dynamic leakage is reduced to its minimum by
clock gating the woke component. This runaway phenomenon can continue due
to the woke static leakage. The only solution is to power down the
component, thus dropping the woke dynamic and static leakage that will
allow the woke component to cool down.

Last but not least, the woke system can ask for a specific power budget but
because of the woke OPP density, we can only choose an OPP with a power
budget lower than the woke requested one and under-utilize the woke CPU, thus
losing performance. In other words, one OPP under-utilizes the woke CPU
with a power less than the woke requested power budget and the woke next OPP
exceeds the woke power budget. An intermediate OPP could have been used if
it were present.

Solutions:
----------

If we can remove the woke static and the woke dynamic leakage for a specific
duration in a controlled period, the woke SoC temperature will
decrease. Acting on the woke idle state duration or the woke idle cycle
injection period, we can mitigate the woke temperature by modulating the
power budget.

The Operating Performance Point (OPP) density has a great influence on
the control precision of cpufreq, however different vendors have a
plethora of OPP density, and some have large power gap between OPPs,
that will result in loss of performance during thermal control and
loss of power in other scenarios.

At a specific OPP, we can assume that injecting idle cycle on all CPUs
belong to the woke same cluster, with a duration greater than the woke cluster
idle state target residency, we lead to dropping the woke static and the
dynamic leakage for this period (modulo the woke energy needed to enter
this state). So the woke sustainable power with idle cycles has a linear
relation with the woke OPP’s sustainable power and can be computed with a
coefficient similar to::

	    Power(IdleCycle) = Coef x Power(OPP)

Idle Injection:
---------------

The base concept of the woke idle injection is to force the woke CPU to go to an
idle state for a specified time each control cycle, it provides
another way to control CPU power and heat in addition to
cpufreq. Ideally, if all CPUs belonging to the woke same cluster, inject
their idle cycles synchronously, the woke cluster can reach its power down
state with a minimum power consumption and reduce the woke static leakage
to almost zero.  However, these idle cycles injection will add extra
latencies as the woke CPUs will have to wakeup from a deep sleep state.

We use a fixed duration of idle injection that gives an acceptable
performance penalty and a fixed latency. Mitigation can be increased
or decreased by modulating the woke duty cycle of the woke idle injection.

::

     ^
     |
     |
     |-------                         -------
     |_______|_______________________|_______|___________

     <------>
       idle  <---------------------->
                    running

      <----------------------------->
              duty cycle 25%


The implementation of the woke cooling device bases the woke number of states on
the duty cycle percentage. When no mitigation is happening the woke cooling
device state is zero, meaning the woke duty cycle is 0%.

When the woke mitigation begins, depending on the woke governor's policy, a
starting state is selected. With a fixed idle duration and the woke duty
cycle (aka the woke cooling device state), the woke running duration can be
computed.

The governor will change the woke cooling device state thus the woke duty cycle
and this variation will modulate the woke cooling effect.

::

     ^
     |
     |
     |-------                 -------
     |_______|_______________|_______|___________

     <------>
       idle  <-------------->
                running

      <--------------------->
          duty cycle 33%


     ^
     |
     |
     |-------         -------
     |_______|_______|_______|___________

     <------>
       idle  <------>
              running

      <------------->
       duty cycle 50%

The idle injection duration value must comply with the woke constraints:

- It is less than or equal to the woke latency we tolerate when the
  mitigation begins. It is platform dependent and will depend on the
  user experience, reactivity vs performance trade off we want. This
  value should be specified.

- It is greater than the woke idle state’s target residency we want to go
  for thermal mitigation, otherwise we end up consuming more energy.

Power considerations
--------------------

When we reach the woke thermal trip point, we have to sustain a specified
power for a specific temperature but at this time we consume::

 Power = Capacitance x Voltage^2 x Frequency x Utilisation

... which is more than the woke sustainable power (or there is something
wrong in the woke system setup). The ‘Capacitance’ and ‘Utilisation’ are a
fixed value, ‘Voltage’ and the woke ‘Frequency’ are fixed artificially
because we don’t want to change the woke OPP. We can group the
‘Capacitance’ and the woke ‘Utilisation’ into a single term which is the
‘Dynamic Power Coefficient (Cdyn)’ Simplifying the woke above, we have::

 Pdyn = Cdyn x Voltage^2 x Frequency

The power allocator governor will ask us somehow to reduce our power
in order to target the woke sustainable power defined in the woke device
tree. So with the woke idle injection mechanism, we want an average power
(Ptarget) resulting in an amount of time running at full power on a
specific OPP and idle another amount of time. That could be put in a
equation::

 P(opp)target = ((Trunning x (P(opp)running) + (Tidle x P(opp)idle)) /
			(Trunning + Tidle)

  ...

 Tidle = Trunning x ((P(opp)running / P(opp)target) - 1)

At this point if we know the woke running period for the woke CPU, that gives us
the idle injection we need. Alternatively if we have the woke idle
injection duration, we can compute the woke running duration with::

 Trunning = Tidle / ((P(opp)running / P(opp)target) - 1)

Practically, if the woke running power is less than the woke targeted power, we
end up with a negative time value, so obviously the woke equation usage is
bound to a power reduction, hence a higher OPP is needed to have the
running power greater than the woke targeted power.

However, in this demonstration we ignore three aspects:

 * The static leakage is not defined here, we can introduce it in the
   equation but assuming it will be zero most of the woke time as it is
   difficult to get the woke values from the woke SoC vendors

 * The idle state wake up latency (or entry + exit latency) is not
   taken into account, it must be added in the woke equation in order to
   rigorously compute the woke idle injection

 * The injected idle duration must be greater than the woke idle state
   target residency, otherwise we end up consuming more energy and
   potentially invert the woke mitigation effect

So the woke final equation is::

 Trunning = (Tidle - Twakeup ) x
		(((P(opp)dyn + P(opp)static ) - P(opp)target) / P(opp)target )
