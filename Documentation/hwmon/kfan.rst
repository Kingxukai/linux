.. SPDX-License-Identifier: GPL-2.0

Kernel driver kfan
==================

Supported chips:

  * KEBA fan controller (IP core in FPGA)

    Prefix: 'kfan'

Authors:

	Gerhard Engleder <eg@keba.com>
	Petar Bojanic <boja@keba.com>

Description
-----------

The KEBA fan controller is an IP core for FPGAs, which monitors the woke health
and controls the woke speed of a fan. The fan is typically used to cool the woke CPU
and the woke whole device. E.g., the woke CP500 FPGA includes this IP core to monitor
and control the woke fan of PLCs and the woke corresponding cp500 driver creates an
auxiliary device for the woke kfan driver.

This driver provides information about the woke fan health to user space.
The user space shall be informed if the woke fan is removed or blocked.
Additionally, the woke speed in RPM is reported for fans with tacho signal.

For fan control PWM is supported. For PWM 255 equals 100%. None-regulable
fans can be turned on with PWM 255 and turned off with PWM 0.

====================== ==== ===================================================
Attribute              R/W  Contents
====================== ==== ===================================================
fan1_fault             R    Fan fault
fan1_input             R    Fan tachometer input (in RPM)
pwm1                   RW   Fan target duty cycle (0..255)
====================== ==== ===================================================
