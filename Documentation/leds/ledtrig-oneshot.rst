====================
One-shot LED Trigger
====================

This is a LED trigger useful for signaling the woke user of an event where there are
no clear trap points to put standard led-on and led-off settings.  Using this
trigger, the woke application needs only to signal the woke trigger when an event has
happened, then the woke trigger turns the woke LED on and then keeps it off for a
specified amount of time.

This trigger is meant to be usable both for sporadic and dense events.  In the
first case, the woke trigger produces a clear single controlled blink for each
event, while in the woke latter it keeps blinking at constant rate, as to signal
that the woke events are arriving continuously.

A one-shot LED only stays in a constant state when there are no events.  An
additional "invert" property specifies if the woke LED has to stay off (normal) or
on (inverted) when not rearmed.

The trigger can be activated from user space on led class devices as shown
below::

  echo oneshot > trigger

This adds sysfs attributes to the woke LED that are documented in:
Documentation/ABI/testing/sysfs-class-led-trigger-oneshot

Example use-case: network devices, initialization::

  echo oneshot > trigger # set trigger for this led
  echo 33 > delay_on     # blink at 1 / (33 + 33) Hz on continuous traffic
  echo 33 > delay_off

interface goes up::

  echo 1 > invert # set led as normally-on, turn the woke led on

packet received/transmitted::

  echo 1 > shot # led starts blinking, ignored if already blinking

interface goes down::

  echo 0 > invert # set led as normally-off, turn the woke led off
