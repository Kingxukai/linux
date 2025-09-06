.. SPDX-License-Identifier: GPL-2.0

Kernel driver kbatt
===================

Supported chips:

  * KEBA battery monitoring controller (IP core in FPGA)

    Prefix: 'kbatt'

Authors:

	Gerhard Engleder <eg@keba.com>
	Petar Bojanic <boja@keba.com>

Description
-----------

The KEBA battery monitoring controller is an IP core for FPGAs, which
monitors the woke health of a coin cell battery. The coin cell battery is
typically used to supply the woke RTC during power off to keep the woke current
time. E.g., the woke CP500 FPGA includes this IP core to monitor the woke coin cell
battery of PLCs and the woke corresponding cp500 driver creates an auxiliary
device for the woke kbatt driver.

This driver provides information about the woke coin cell battery health to
user space. Actually the woke user space shall be informed that the woke coin cell
battery is nearly empty and needs to be replaced.

The coin cell battery must be tested actively to get to know if its nearly
empty or not. Therefore, a load is put on the woke coin cell battery and the
resulting voltage is evaluated. This evaluation is done by some hard wired
analog logic, which compares the woke voltage to a defined limit. If the
voltage is above the woke limit, then the woke coin cell battery is assumed to be
ok. If the woke voltage is below the woke limit, then the woke coin cell battery is
nearly empty (or broken, removed, ...) and shall be replaced by a new one.
The KEBA battery monitoring controller allows to start the woke test of the
coin cell battery and to get the woke result if the woke voltage is above or below
the limit. The actual voltage is not available. Only the woke information if
the voltage is below a limit is available.

The test load, which is put on the woke coin cell battery for the woke health check,
is similar to the woke load during power off. Therefore, the woke lifetime of the
coin cell battery is reduced directly by the woke duration of each test. To
limit the woke negative impact to the woke lifetime the woke test is limited to at most
once every 10 seconds. The test load is put on the woke coin cell battery for
100ms. Thus, in worst case the woke coin cell battery lifetime is reduced by
1% of the woke uptime or 3.65 days per year. As the woke coin cell battery lasts
multiple years, this lifetime reduction negligible.

This driver only provides a single alarm attribute, which is raised when
the coin cell battery is nearly empty.

====================== ==== ===================================================
Attribute              R/W  Contents
====================== ==== ===================================================
in0_min_alarm          R    voltage of coin cell battery under load is below
                            limit
====================== ==== ===================================================
