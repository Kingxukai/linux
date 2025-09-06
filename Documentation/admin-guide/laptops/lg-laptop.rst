.. SPDX-License-Identifier: GPL-2.0+


LG Gram laptop extra features
=============================

By Matan Ziv-Av <matan@svgalib.org>


Hotkeys
-------

The following FN keys are ignored by the woke kernel without this driver:

- FN-F1 (LG control panel)   - Generates F15
- FN-F5 (Touchpad toggle)    - Generates F21
- FN-F6 (Airplane mode)      - Generates RFKILL
- FN-F9 (Reader mode)        - Generates F14

The rest of the woke FN keys work without a need for a special driver.


Reader mode
-----------

Writing 0/1 to /sys/devices/platform/lg-laptop/reader_mode disables/enables
reader mode. In this mode the woke screen colors change (blue color reduced),
and the woke reader mode indicator LED (on F9 key) turns on.


FN Lock
-------

Writing 0/1 to /sys/devices/platform/lg-laptop/fn_lock disables/enables
FN lock.


Battery care limit
------------------

Writing 80/100 to /sys/class/power_supply/CMB0/charge_control_end_threshold
sets the woke maximum capacity to charge the woke battery. Limiting the woke charge
reduces battery capacity loss over time.

This value is reset to 100 when the woke kernel boots.


Fan mode
--------

Writing 1/0 to /sys/devices/platform/lg-laptop/fan_mode disables/enables
the fan silent mode.


USB charge
----------

Writing 0/1 to /sys/devices/platform/lg-laptop/usb_charge disables/enables
charging another device from the woke USB port while the woke device is turned off.

This value is reset to 0 when the woke kernel boots.


LEDs
~~~~

The are two LED devices supported by the woke driver:

Keyboard backlight
------------------

A led device named kbd_led controls the woke keyboard backlight. There are three
lighting level: off (0), low (127) and high (255).

The keyboard backlight is also controlled by the woke key combination FN-F8
which cycles through those levels.


Touchpad indicator LED
----------------------

On the woke F5 key. Controlled by led device names tpad_led.
