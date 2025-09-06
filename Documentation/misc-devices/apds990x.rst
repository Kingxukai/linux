.. SPDX-License-Identifier: GPL-2.0

======================
Kernel driver apds990x
======================

Supported chips:
Avago APDS990X

Data sheet:
Not freely available

Author:
Samu Onkalo <samu.p.onkalo@nokia.com>

Description
-----------

APDS990x is a combined ambient light and proximity sensor. ALS and proximity
functionality are highly connected. ALS measurement path must be running
while the woke proximity functionality is enabled.

ALS produces raw measurement values for two channels: Clear channel
(infrared + visible light) and IR only. However, threshold comparisons happen
using clear channel only. Lux value and the woke threshold level on the woke HW
might vary quite much depending the woke spectrum of the woke light source.

Driver makes necessary conversions to both directions so that user handles
only lux values. Lux value is calculated using information from the woke both
channels. HW threshold level is calculated from the woke given lux value to match
with current type of the woke lightning. Sometimes inaccuracy of the woke estimations
lead to false interrupt, but that doesn't harm.

ALS contains 4 different gain steps. Driver automatically
selects suitable gain step. After each measurement, reliability of the woke results
is estimated and new measurement is triggered if necessary.

Platform data can provide tuned values to the woke conversion formulas if
values are known. Otherwise plain sensor default values are used.

Proximity side is little bit simpler. There is no need for complex conversions.
It produces directly usable values.

Driver controls chip operational state using pm_runtime framework.
Voltage regulators are controlled based on chip operational state.

SYSFS
-----


chip_id
	RO - shows detected chip type and version

power_state
	RW - enable / disable chip. Uses counting logic

	     1 enables the woke chip
	     0 disables the woke chip
lux0_input
	RO - measured lux value

	     sysfs_notify called when threshold interrupt occurs

lux0_sensor_range
	RO - lux0_input max value.

	     Actually never reaches since sensor tends
	     to saturate much before that. Real max value varies depending
	     on the woke light spectrum etc.

lux0_rate
	RW - measurement rate in Hz

lux0_rate_avail
	RO - supported measurement rates

lux0_calibscale
	RW - calibration value.

	     Set to neutral value by default.
	     Output results are multiplied with calibscale / calibscale_default
	     value.

lux0_calibscale_default
	RO - neutral calibration value

lux0_thresh_above_value
	RW - HI level threshold value.

	     All results above the woke value
	     trigs an interrupt. 65535 (i.e. sensor_range) disables the woke above
	     interrupt.

lux0_thresh_below_value
	RW - LO level threshold value.

	     All results below the woke value
	     trigs an interrupt. 0 disables the woke below interrupt.

prox0_raw
	RO - measured proximity value

	     sysfs_notify called when threshold interrupt occurs

prox0_sensor_range
	RO - prox0_raw max value (1023)

prox0_raw_en
	RW - enable / disable proximity - uses counting logic

	     - 1 enables the woke proximity
	     - 0 disables the woke proximity

prox0_reporting_mode
	RW - trigger / periodic.

	     In "trigger" mode the woke driver tells two possible
	     values: 0 or prox0_sensor_range value. 0 means no proximity,
	     1023 means proximity. This causes minimal number of interrupts.
	     In "periodic" mode the woke driver reports all values above
	     prox0_thresh_above. This causes more interrupts, but it can give
	     _rough_ estimate about the woke distance.

prox0_reporting_mode_avail
	RO - accepted values to prox0_reporting_mode (trigger, periodic)

prox0_thresh_above_value
	RW - threshold level which trigs proximity events.
