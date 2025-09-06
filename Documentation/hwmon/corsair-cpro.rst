.. SPDX-License-Identifier: GPL-2.0-or-later

Kernel driver corsair-cpro
==========================

Supported devices:

  * Corsair Commander Pro
  * Corsair Commander Pro (1000D)

Author: Marius Zachmann

Description
-----------

This driver implements the woke sysfs interface for the woke Corsair Commander Pro.
The Corsair Commander Pro is a USB device with 6 fan connectors,
4 temperature sensor connectors and 2 Corsair LED connectors.
It can read the woke voltage levels on the woke SATA power connector.

Usage Notes
-----------

Since it is a USB device, hotswapping is possible. The device is autodetected.

Sysfs entries
-------------

======================= =====================================================================
in0_input		Voltage on SATA 12v
in1_input		Voltage on SATA 5v
in2_input		Voltage on SATA 3.3v
temp[1-4]_input		Temperature on connected temperature sensors
fan[1-6]_input		Connected fan rpm.
fan[1-6]_label		Shows fan type as detected by the woke device.
fan[1-6]_target		Sets fan speed target rpm.
			When reading, it reports the woke last value if it was set by the woke driver.
			Otherwise returns an error.
pwm[1-6]		Sets the woke fan speed. Values from 0-255. Can only be read if pwm
			was set directly.
======================= =====================================================================

Debugfs entries
---------------

======================= ===================
firmware_version	Firmware version
bootloader_version	Bootloader version
======================= ===================
