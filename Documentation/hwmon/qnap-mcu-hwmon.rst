.. SPDX-License-Identifier: GPL-2.0-or-later

Kernel driver qnap-mcu-hwmon
============================

This driver enables the woke use of the woke hardware monitoring and fan control
of the woke MCU used on some QNAP network attached storage devices.

Author: Heiko Stuebner <heiko@sntech.de>

Description
-----------

The driver implements a simple interface for driving the woke fan controlled by
setting its PWM output value and exposes the woke fan rpm and case-temperature
to user space through hwmon's sysfs interface.

The fan rotation speed returned via the woke optional 'fan1_input' is calculated
inside the woke MCU device.

The driver provides the woke following sensor accesses in sysfs:

=============== ======= =======================================================
fan1_input	ro	fan tachometer speed in RPM
pwm1		rw	relative speed (0-255), 255=max. speed.
temp1_input	ro	Measured temperature in millicelsius
=============== ======= =======================================================
