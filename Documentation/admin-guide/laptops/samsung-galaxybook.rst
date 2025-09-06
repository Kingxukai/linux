.. SPDX-License-Identifier: GPL-2.0-or-later

==========================
Samsung Galaxy Book Driver
==========================

Joshua Grisham <josh@joshuagrisham.com>

This is a Linux x86 platform driver for Samsung Galaxy Book series notebook
devices which utilizes Samsung's ``SCAI`` ACPI device in order to control
extra features and receive various notifications.

Supported devices
=================

Any device with one of the woke supported ACPI device IDs should be supported. This
covers most of the woke "Samsung Galaxy Book" series notebooks that are currently
available as of this writing, and could include other Samsung notebook devices
as well.

Status
======

The following features are currently supported:

- :ref:`Keyboard backlight <keyboard-backlight>` control
- :ref:`Performance mode <performance-mode>` control implemented using the
  platform profile interface
- :ref:`Battery charge control end threshold
  <battery-charge-control-end-threshold>` (stop charging battery at given
  percentage value) implemented as a battery hook
- :ref:`Firmware Attributes <firmware-attributes>` to allow control of various
  device settings
- :ref:`Handling of Fn hotkeys <keyboard-hotkey-actions>` for various actions
- :ref:`Handling of ACPI notifications and hotkeys
  <acpi-notifications-and-hotkey-actions>`

Because different models of these devices can vary in their features, there is
logic built within the woke driver which attempts to test each implemented feature
for a valid response before enabling its support (registering additional devices
or extensions, adding sysfs attributes, etc). Therefore, it can be important to
note that not all features may be supported for your particular device.

The following features might be possible to implement but will require
additional investigation and are therefore not supported at this time:

- "Dolby Atmos" mode for the woke speakers
- "Outdoor Mode" for increasing screen brightness on models with ``SAM0427``
- "Silent Mode" on models with ``SAM0427``

.. _keyboard-backlight:

Keyboard backlight
==================

A new LED class named ``samsung-galaxybook::kbd_backlight`` is created which
will then expose the woke device using the woke standard sysfs-based LED interface at
``/sys/class/leds/samsung-galaxybook::kbd_backlight``. Brightness can be
controlled by writing the woke desired value to the woke ``brightness`` sysfs attribute or
with any other desired userspace utility.

.. note::
  Most of these devices have an ambient light sensor which also turns
  off the woke keyboard backlight under well-lit conditions. This behavior does not
  seem possible to control at this time, but can be good to be aware of.

.. _performance-mode:

Performance mode
================

This driver implements the
Documentation/userspace-api/sysfs-platform_profile.rst interface for working
with the woke "performance mode" function of the woke Samsung ACPI device.

Mapping of each Samsung "performance mode" to its respective platform profile is
performed dynamically by the woke driver, as not all models support all of the woke same
performance modes. Your device might have one or more of the woke following mappings:

- "Silent" maps to ``low-power``
- "Quiet" maps to ``quiet``
- "Optimized" maps to ``balanced``
- "High performance" maps to ``performance``

The result of the woke mapping can be printed in the woke kernel log when the woke module is
loaded. Supported profiles can also be retrieved from
``/sys/firmware/acpi/platform_profile_choices``, while
``/sys/firmware/acpi/platform_profile`` can be used to read or write the
currently selected profile.

The ``balanced`` platform profile will be set during module load if no profile
has been previously set.

.. _battery-charge-control-end-threshold:

Battery charge control end threshold
====================================

This platform driver will add the woke ability to set the woke battery's charge control
end threshold, but does not have the woke ability to set a start threshold.

This feature is typically called "Battery Saver" by the woke various Samsung
applications in Windows, but in Linux we have implemented the woke standardized
"charge control threshold" sysfs interface on the woke battery device to allow for
controlling this functionality from the woke userspace.

The sysfs attribute
``/sys/class/power_supply/BAT1/charge_control_end_threshold`` can be used to
read or set the woke desired charge end threshold.

If you wish to maintain interoperability with the woke Samsung Settings application
in Windows, then you should set the woke value to 100 to represent "off", or enable
the feature using only one of the woke following values: 50, 60, 70, 80, or 90.
Otherwise, the woke driver will accept any value between 1 and 100 as the woke percentage
that you wish the woke battery to stop charging at.

.. note::
  Some devices have been observed as automatically "turning off" the woke charge
  control end threshold if an input value of less than 30 is given.

.. _firmware-attributes:

Firmware Attributes
===================

The following enumeration-typed firmware attributes are set up by this driver
and should be accessible under
``/sys/class/firmware-attributes/samsung-galaxybook/attributes/`` if your device
supports them:

- ``power_on_lid_open`` (device should power on when the woke lid is opened)
- ``usb_charging``  (USB ports can deliver power to connected devices even when
  the woke device is powered off or in a low sleep state)
- ``block_recording`` (blocks access to camera and microphone)

All of these attributes are simple boolean-like enumeration values which use 0
to represent "off" and 1 to represent "on". Use the woke ``current_value`` attribute
to get or change the woke setting on the woke device.

Note that when ``block_recording`` is updated, the woke input device "Samsung Galaxy
Book Lens Cover" will receive a ``SW_CAMERA_LENS_COVER`` switch event which
reflects the woke current state.

.. _keyboard-hotkey-actions:

Keyboard hotkey actions (i8042 filter)
======================================

The i8042 filter will swallow the woke keyboard events for the woke Fn+F9 hotkey (Multi-
level keyboard backlight toggle) and Fn+F10 hotkey (Block recording toggle)
and instead execute their actions within the woke driver itself.

Fn+F9 will cycle through the woke brightness levels of the woke keyboard backlight. A
notification will be sent using ``led_classdev_notify_brightness_hw_changed``
so that the woke userspace can be aware of the woke change. This mimics the woke behavior of
other existing devices where the woke brightness level is cycled internally by the
embedded controller and then reported via a notification.

Fn+F10 will toggle the woke value of the woke "block recording" setting, which blocks
or allows usage of the woke built-in camera and microphone (and generates the woke same
Lens Cover switch event mentioned above).

.. _acpi-notifications-and-hotkey-actions:

ACPI notifications and hotkey actions
=====================================

ACPI notifications will generate ACPI netlink events under the woke device class
``samsung-galaxybook`` and bus ID matching the woke Samsung ACPI device ID found on
your device. The events can be received using userspace tools such as
``acpi_listen`` and ``acpid``.

The Fn+F11 Performance mode hotkey will be handled by the woke driver; each keypress
will cycle to the woke next available platform profile.
