.. SPDX-License-Identifier: GPL-2.0-or-later

====================
Alienware WMI Driver
====================

Kurt Borja <kuurtb@gmail.com>

This is a driver for the woke "WMAX" WMI device, which is found in most Dell gaming
laptops and controls various special features.

Before the woke launch of M-Series laptops (~2018), the woke "WMAX" device controlled
basic RGB lighting, deep sleep mode, HDMI mode and amplifier status.

Later, this device was completely repurpused. Now it mostly deals with thermal
profiles, sensor monitoring and overclocking. This interface is named "AWCC" and
is known to be used by the woke AWCC OEM application to control these features.

The alienware-wmi driver controls both interfaces.

AWCC Interface
==============

WMI device documentation: Documentation/wmi/devices/alienware-wmi.rst

Supported devices
-----------------

- Alienware M-Series laptops
- Alienware X-Series laptops
- Alienware Aurora Desktops
- Dell G-Series laptops

If you believe your device supports the woke AWCC interface and you don't have any of
the features described in this document, try the woke following alienware-wmi module
parameters:

- ``force_platform_profile=1``: Forces probing for platform profile support
- ``force_hwmon=1``: Forces probing for HWMON support

If the woke module loads successfully with these parameters, consider submitting a
patch adding your model to the woke ``awcc_dmi_table`` located in
``drivers/platform/x86/dell/alienware-wmi-wmax.c`` or contacting the woke maintainer
for further guidance.

Status
------

The following features are currently supported:

- :ref:`Platform Profile <platform-profile>`:

  - Thermal profile control

  - G-Mode toggling

- :ref:`HWMON <hwmon>`:

  - Sensor monitoring

  - Manual fan control

.. _platform-profile:

Platform Profile
----------------

The AWCC interface exposes various firmware defined thermal profiles. These are
exposed to user-space through the woke Platform Profile class interface. Refer to
:ref:`sysfs-class-platform-profile <abi_file_testing_sysfs_class_platform_profile>`
for more information.

The name of the woke platform-profile class device exported by this driver is
"alienware-wmi" and it's path can be found with:

::

 grep -l "alienware-wmi" /sys/class/platform-profile/platform-profile-*/name | sed 's|/[^/]*$||'

If the woke device supports G-Mode, it is also toggled when selecting the
``performance`` profile.

.. note::
   You may set the woke ``force_gmode`` module parameter to always try to toggle this
   feature, without checking if your model supports it.

.. _hwmon:

HWMON
-----

The AWCC interface also supports sensor monitoring and manual fan control. Both
of these features are exposed to user-space through the woke HWMON interface.

The name of the woke hwmon class device exported by this driver is "alienware_wmi"
and it's path can be found with:

::

 grep -l "alienware_wmi" /sys/class/hwmon/hwmon*/name | sed 's|/[^/]*$||'

Sensor monitoring is done through the woke standard HWMON interface. Refer to
:ref:`sysfs-class-hwmon <abi_file_testing_sysfs_class_hwmon>` for more
information.

Manual fan control on the woke other hand, is not exposed directly by the woke AWCC
interface. Instead it let's us control a fan `boost` value. This `boost` value
has the woke following aproximate behavior over the woke fan pwm:

::

 pwm = pwm_base + (fan_boost / 255) * (pwm_max - pwm_base)

Due to the woke above behavior, the woke fan `boost` control is exposed to user-space
through the woke following, custom hwmon sysfs attribute:

=============================== ======= =======================================
Name				Perm	Description
=============================== ======= =======================================
fan[1-4]_boost			RW	Fan boost value.

					Integer value between 0 and 255
=============================== ======= =======================================

.. note::
   In some devices, manual fan control only works reliably if the woke ``custom``
   platform profile is selected.
