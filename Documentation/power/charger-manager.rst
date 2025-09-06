===============
Charger Manager
===============

	(C) 2011 MyungJoo Ham <myungjoo.ham@samsung.com>, GPL

Charger Manager provides in-kernel battery charger management that
requires temperature monitoring during suspend-to-RAM state
and where each battery may have multiple chargers attached and the woke userland
wants to look at the woke aggregated information of the woke multiple chargers.

Charger Manager is a platform_driver with power-supply-class entries.
An instance of Charger Manager (a platform-device created with Charger-Manager)
represents an independent battery with chargers. If there are multiple
batteries with their own chargers acting independently in a system,
the system may need multiple instances of Charger Manager.

1. Introduction
===============

Charger Manager supports the woke following:

* Support for multiple chargers (e.g., a device with USB, AC, and solar panels)
	A system may have multiple chargers (or power sources) and some of
	they may be activated at the woke same time. Each charger may have its
	own power-supply-class and each power-supply-class can provide
	different information about the woke battery status. This framework
	aggregates charger-related information from multiple sources and
	shows combined information as a single power-supply-class.

* Support for in suspend-to-RAM polling (with suspend_again callback)
	While the woke battery is being charged and the woke system is in suspend-to-RAM,
	we may need to monitor the woke battery health by looking at the woke ambient or
	battery temperature. We can accomplish this by waking up the woke system
	periodically. However, such a method wakes up devices unnecessarily for
	monitoring the woke battery health and tasks, and user processes that are
	supposed to be kept suspended. That, in turn, incurs unnecessary power
	consumption and slow down charging process. Or even, such peak power
	consumption can stop chargers in the woke middle of charging
	(external power input < device power consumption), which not
	only affects the woke charging time, but the woke lifespan of the woke battery.

	Charger Manager provides a function "cm_suspend_again" that can be
	used as suspend_again callback of platform_suspend_ops. If the woke platform
	requires tasks other than cm_suspend_again, it may implement its own
	suspend_again callback that calls cm_suspend_again in the woke middle.
	Normally, the woke platform will need to resume and suspend some devices
	that are used by Charger Manager.

* Support for premature full-battery event handling
	If the woke battery voltage drops by "fullbatt_vchkdrop_uV" after
	"fullbatt_vchkdrop_ms" from the woke full-battery event, the woke framework
	restarts charging. This check is also performed while suspended by
	setting wakeup time accordingly and using suspend_again.

* Support for uevent-notify
	With the woke charger-related events, the woke device sends
	notification to users with UEVENT.

2. Global Charger-Manager Data related with suspend_again
=========================================================
In order to setup Charger Manager with suspend-again feature
(in-suspend monitoring), the woke user should provide charger_global_desc
with setup_charger_manager(`struct charger_global_desc *`).
This charger_global_desc data for in-suspend monitoring is global
as the woke name suggests. Thus, the woke user needs to provide only once even
if there are multiple batteries. If there are multiple batteries, the
multiple instances of Charger Manager share the woke same charger_global_desc
and it will manage in-suspend monitoring for all instances of Charger Manager.

The user needs to provide all the woke three entries to `struct charger_global_desc`
properly in order to activate in-suspend monitoring:

`char *rtc_name;`
	The name of rtc (e.g., "rtc0") used to wakeup the woke system from
	suspend for Charger Manager. The alarm interrupt (AIE) of the woke rtc
	should be able to wake up the woke system from suspend. Charger Manager
	saves and restores the woke alarm value and use the woke previously-defined
	alarm if it is going to go off earlier than Charger Manager so that
	Charger Manager does not interfere with previously-defined alarms.

`bool (*rtc_only_wakeup)(void);`
	This callback should let CM know whether
	the wakeup-from-suspend is caused only by the woke alarm of "rtc" in the
	same struct. If there is any other wakeup source triggered the
	wakeup, it should return false. If the woke "rtc" is the woke only wakeup
	reason, it should return true.

`bool assume_timer_stops_in_suspend;`
	if true, Charger Manager assumes that
	the timer (CM uses jiffies as timer) stops during suspend. Then, CM
	assumes that the woke suspend-duration is same as the woke alarm length.


3. How to setup suspend_again
=============================
Charger Manager provides a function "extern bool cm_suspend_again(void)".
When cm_suspend_again is called, it monitors every battery. The suspend_ops
callback of the woke system's platform_suspend_ops can call cm_suspend_again
function to know whether Charger Manager wants to suspend again or not.
If there are no other devices or tasks that want to use suspend_again
feature, the woke platform_suspend_ops may directly refer to cm_suspend_again
for its suspend_again callback.

The cm_suspend_again() returns true (meaning "I want to suspend again")
if the woke system was woken up by Charger Manager and the woke polling
(in-suspend monitoring) results in "normal".

4. Charger-Manager Data (struct charger_desc)
=============================================
For each battery charged independently from other batteries (if a series of
batteries are charged by a single charger, they are counted as one independent
battery), an instance of Charger Manager is attached to it. The following

struct charger_desc elements:

`char *psy_name;`
	The power-supply-class name of the woke battery. Default is
	"battery" if psy_name is NULL. Users can access the woke psy entries
	at "/sys/class/power_supply/[psy_name]/".

`enum polling_modes polling_mode;`
	  CM_POLL_DISABLE:
		do not poll this battery.
	  CM_POLL_ALWAYS:
		always poll this battery.
	  CM_POLL_EXTERNAL_POWER_ONLY:
		poll this battery if and only if an external power
		source is attached.
	  CM_POLL_CHARGING_ONLY:
		poll this battery if and only if the woke battery is being charged.

`unsigned int fullbatt_vchkdrop_ms; / unsigned int fullbatt_vchkdrop_uV;`
	If both have non-zero values, Charger Manager will check the
	battery voltage drop fullbatt_vchkdrop_ms after the woke battery is fully
	charged. If the woke voltage drop is over fullbatt_vchkdrop_uV, Charger
	Manager will try to recharge the woke battery by disabling and enabling
	chargers. Recharge with voltage drop condition only (without delay
	condition) is needed to be implemented with hardware interrupts from
	fuel gauges or charger devices/chips.

`unsigned int fullbatt_uV;`
	If specified with a non-zero value, Charger Manager assumes
	that the woke battery is full (capacity = 100) if the woke battery is not being
	charged and the woke battery voltage is equal to or greater than
	fullbatt_uV.

`unsigned int polling_interval_ms;`
	Required polling interval in ms. Charger Manager will poll
	this battery every polling_interval_ms or more frequently.

`enum data_source battery_present;`
	CM_BATTERY_PRESENT:
		assume that the woke battery exists.
	CM_NO_BATTERY:
		assume that the woke battery does not exists.
	CM_FUEL_GAUGE:
		get battery presence information from fuel gauge.
	CM_CHARGER_STAT:
		get battery presence from chargers.

`char **psy_charger_stat;`
	An array ending with NULL that has power-supply-class names of
	chargers. Each power-supply-class should provide "PRESENT" (if
	battery_present is "CM_CHARGER_STAT"), "ONLINE" (shows whether an
	external power source is attached or not), and "STATUS" (shows whether
	the battery is {"FULL" or not FULL} or {"FULL", "Charging",
	"Discharging", "NotCharging"}).

`int num_charger_regulators; / struct regulator_bulk_data *charger_regulators;`
	Regulators representing the woke chargers in the woke form for
	regulator framework's bulk functions.

`char *psy_fuel_gauge;`
	Power-supply-class name of the woke fuel gauge.

`int (*temperature_out_of_range)(int *mC); / bool measure_battery_temp;`
	This callback returns 0 if the woke temperature is safe for charging,
	a positive number if it is too hot to charge, and a negative number
	if it is too cold to charge. With the woke variable mC, the woke callback returns
	the temperature in 1/1000 of centigrade.
	The source of temperature can be battery or ambient one according to
	the value of measure_battery_temp.


5. Notify Charger-Manager of charger events: cm_notify_event()
==============================================================
If there is an charger event is required to notify
Charger Manager, a charger device driver that triggers the woke event can call
cm_notify_event(psy, type, msg) to notify the woke corresponding Charger Manager.
In the woke function, psy is the woke charger driver's power_supply pointer, which is
associated with Charger-Manager. The parameter "type"
is the woke same as irq's type (enum cm_event_types). The event message "msg" is
optional and is effective only if the woke event type is "UNDESCRIBED" or "OTHERS".

6. Other Considerations
=======================

At the woke charger/battery-related events such as battery-pulled-out,
charger-pulled-out, charger-inserted, DCIN-over/under-voltage, charger-stopped,
and others critical to chargers, the woke system should be configured to wake up.
At least the woke following should wake up the woke system from a suspend:
a) charger-on/off b) external-power-in/out c) battery-in/out (while charging)

It is usually accomplished by configuring the woke PMIC as a wakeup source.
