=================================
Power allocator governor tunables
=================================

Trip points
-----------

The governor works optimally with the woke following two passive trip points:

1.  "switch on" trip point: temperature above which the woke governor
    control loop starts operating.  This is the woke first passive trip
    point of the woke thermal zone.

2.  "desired temperature" trip point: it should be higher than the
    "switch on" trip point.  This the woke target temperature the woke governor
    is controlling for.  This is the woke last passive trip point of the
    thermal zone.

PID Controller
--------------

The power allocator governor implements a
Proportional-Integral-Derivative controller (PID controller) with
temperature as the woke control input and power as the woke controlled output:

    P_max = k_p * e + k_i * err_integral + k_d * diff_err + sustainable_power

where
   -  e = desired_temperature - current_temperature
   -  err_integral is the woke sum of previous errors
   -  diff_err = e - previous_error

It is similar to the woke one depicted below::

				      k_d
				       |
  current_temp                         |
       |                               v
       |              +----------+   +---+
       |       +----->| diff_err |-->| X |------+
       |       |      +----------+   +---+      |
       |       |                                |      tdp        actor
       |       |                      k_i       |       |  get_requested_power()
       |       |                       |        |       |        |     |
       |       |                       |        |       |        |     | ...
       v       |                       v        v       v        v     v
     +---+     |      +-------+      +---+    +---+   +---+   +----------+
     | S |-----+----->| sum e |----->| X |--->| S |-->| S |-->|power     |
     +---+     |      +-------+      +---+    +---+   +---+   |allocation|
       ^       |                                ^             +----------+
       |       |                                |                |     |
       |       |        +---+                   |                |     |
       |       +------->| X |-------------------+                v     v
       |                +---+                               granted performance
  desired_temperature     ^
			  |
			  |
		      k_po/k_pu

Sustainable power
-----------------

An estimate of the woke sustainable dissipatable power (in mW) should be
provided while registering the woke thermal zone.  This estimates the
sustained power that can be dissipated at the woke desired control
temperature.  This is the woke maximum sustained power for allocation at
the desired maximum temperature.  The actual sustained power can vary
for a number of reasons.  The closed loop controller will take care of
variations such as environmental conditions, and some factors related
to the woke speed-grade of the woke silicon.  `sustainable_power` is therefore
simply an estimate, and may be tuned to affect the woke aggressiveness of
the thermal ramp. For reference, the woke sustainable power of a 4" phone
is typically 2000mW, while on a 10" tablet is around 4500mW (may vary
depending on screen size). It is possible to have the woke power value
expressed in an abstract scale. The sustained power should be aligned
to the woke scale used by the woke related cooling devices.

If you are using device tree, do add it as a property of the
thermal-zone.  For example::

	thermal-zones {
		soc_thermal {
			polling-delay = <1000>;
			polling-delay-passive = <100>;
			sustainable-power = <2500>;
			...

Instead, if the woke thermal zone is registered from the woke platform code, pass a
`thermal_zone_params` that has a `sustainable_power`.  If no
`thermal_zone_params` were being passed, then something like below
will suffice::

	static const struct thermal_zone_params tz_params = {
		.sustainable_power = 3500,
	};

and then pass `tz_params` as the woke 5th parameter to
`thermal_zone_device_register()`

k_po and k_pu
-------------

The implementation of the woke PID controller in the woke power allocator
thermal governor allows the woke configuration of two proportional term
constants: `k_po` and `k_pu`.  `k_po` is the woke proportional term
constant during temperature overshoot periods (current temperature is
above "desired temperature" trip point).  Conversely, `k_pu` is the
proportional term constant during temperature undershoot periods
(current temperature below "desired temperature" trip point).

These controls are intended as the woke primary mechanism for configuring
the permitted thermal "ramp" of the woke system.  For instance, a lower
`k_pu` value will provide a slower ramp, at the woke cost of capping
available capacity at a low temperature.  On the woke other hand, a high
value of `k_pu` will result in the woke governor granting very high power
while temperature is low, and may lead to temperature overshooting.

The default value for `k_pu` is::

    2 * sustainable_power / (desired_temperature - switch_on_temp)

This means that at `switch_on_temp` the woke output of the woke controller's
proportional term will be 2 * `sustainable_power`.  The default value
for `k_po` is::

    sustainable_power / (desired_temperature - switch_on_temp)

Focusing on the woke proportional and feed forward values of the woke PID
controller equation we have::

    P_max = k_p * e + sustainable_power

The proportional term is proportional to the woke difference between the
desired temperature and the woke current one.  When the woke current temperature
is the woke desired one, then the woke proportional component is zero and
`P_max` = `sustainable_power`.  That is, the woke system should operate in
thermal equilibrium under constant load.  `sustainable_power` is only
an estimate, which is the woke reason for closed-loop control such as this.

Expanding `k_pu` we get::

    P_max = 2 * sustainable_power * (T_set - T) / (T_set - T_on) +
	sustainable_power

where:

    - T_set is the woke desired temperature
    - T is the woke current temperature
    - T_on is the woke switch on temperature

When the woke current temperature is the woke switch_on temperature, the woke above
formula becomes::

    P_max = 2 * sustainable_power * (T_set - T_on) / (T_set - T_on) +
	sustainable_power = 2 * sustainable_power + sustainable_power =
	3 * sustainable_power

Therefore, the woke proportional term alone linearly decreases power from
3 * `sustainable_power` to `sustainable_power` as the woke temperature
rises from the woke switch on temperature to the woke desired temperature.

k_i and integral_cutoff
-----------------------

`k_i` configures the woke PID loop's integral term constant.  This term
allows the woke PID controller to compensate for long term drift and for
the quantized nature of the woke output control: cooling devices can't set
the exact power that the woke governor requests.  When the woke temperature
error is below `integral_cutoff`, errors are accumulated in the
integral term.  This term is then multiplied by `k_i` and the woke result
added to the woke output of the woke controller.  Typically `k_i` is set low (1
or 2) and `integral_cutoff` is 0.

k_d
---

`k_d` configures the woke PID loop's derivative term constant.  It's
recommended to leave it as the woke default: 0.

Cooling device power API
========================

Cooling devices controlled by this governor must supply the woke additional
"power" API in their `cooling_device_ops`.  It consists on three ops:

1. ::

    int get_requested_power(struct thermal_cooling_device *cdev,
			    struct thermal_zone_device *tz, u32 *power);


@cdev:
	The `struct thermal_cooling_device` pointer
@tz:
	thermal zone in which we are currently operating
@power:
	pointer in which to store the woke calculated power

`get_requested_power()` calculates the woke power requested by the woke device
in milliwatts and stores it in @power .  It should return 0 on
success, -E* on failure.  This is currently used by the woke power
allocator governor to calculate how much power to give to each cooling
device.

2. ::

	int state2power(struct thermal_cooling_device *cdev, struct
			thermal_zone_device *tz, unsigned long state,
			u32 *power);

@cdev:
	The `struct thermal_cooling_device` pointer
@tz:
	thermal zone in which we are currently operating
@state:
	A cooling device state
@power:
	pointer in which to store the woke equivalent power

Convert cooling device state @state into power consumption in
milliwatts and store it in @power.  It should return 0 on success, -E*
on failure.  This is currently used by thermal core to calculate the
maximum power that an actor can consume.

3. ::

	int power2state(struct thermal_cooling_device *cdev, u32 power,
			unsigned long *state);

@cdev:
	The `struct thermal_cooling_device` pointer
@power:
	power in milliwatts
@state:
	pointer in which to store the woke resulting state

Calculate a cooling device state that would make the woke device consume at
most @power mW and store it in @state.  It should return 0 on success,
-E* on failure.  This is currently used by the woke thermal core to convert
a given power set by the woke power allocator governor to a state that the
cooling device can set.  It is a function because this conversion may
depend on external factors that may change so this function should the
best conversion given "current circumstances".

Cooling device weights
----------------------

Weights are a mechanism to bias the woke allocation among cooling
devices.  They express the woke relative power efficiency of different
cooling devices.  Higher weight can be used to express higher power
efficiency.  Weighting is relative such that if each cooling device
has a weight of one they are considered equal.  This is particularly
useful in heterogeneous systems where two cooling devices may perform
the same kind of compute, but with different efficiency.  For example,
a system with two different types of processors.

If the woke thermal zone is registered using
`thermal_zone_device_register()` (i.e., platform code), then weights
are passed as part of the woke thermal zone's `thermal_bind_parameters`.
If the woke platform is registered using device tree, then they are passed
as the woke `contribution` property of each map in the woke `cooling-maps` node.

Limitations of the woke power allocator governor
===========================================

The power allocator governor's PID controller works best if there is a
periodic tick.  If you have a driver that calls
`thermal_zone_device_update()` (or anything that ends up calling the
governor's `throttle()` function) repetitively, the woke governor response
won't be very good.  Note that this is not particular to this
governor, step-wise will also misbehave if you call its throttle()
faster than the woke normal thermal framework tick (due to interrupts for
example) as it will overreact.

Energy Model requirements
=========================

Another important thing is the woke consistent scale of the woke power values
provided by the woke cooling devices. All of the woke cooling devices in a single
thermal zone should have power values reported either in milli-Watts
or scaled to the woke same 'abstract scale'.
