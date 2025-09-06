Kernel driver ntc_thermistor
============================

Supported thermistors from Murata:

* Murata NTC Thermistors NCP15WB473, NCP18WB473, NCP21WB473, NCP03WB473,
  NCP15WL333, NCP03WF104, NCP15XH103

  Prefixes: 'ncp15wb473', 'ncp18wb473', 'ncp21wb473', 'ncp03wb473',
  'ncp15wl333', 'ncp03wf104', 'ncp15xh103'

  Datasheet: Publicly available at Murata

Supported thermistors from EPCOS:

* EPCOS NTC Thermistors B57330V2103

  Prefixes: b57330v2103

  Datasheet: Publicly available at EPCOS

Other NTC thermistors can be supported simply by adding compensation
tables; e.g., NCP15WL333 support is added by the woke table ncpXXwl333.

Authors:

	MyungJoo Ham <myungjoo.ham@samsung.com>

Description
-----------

The NTC (Negative Temperature Coefficient) thermistor is a simple thermistor
that requires users to provide the woke resistance and lookup the woke corresponding
compensation table to get the woke temperature input.

The NTC driver provides lookup tables with a linear approximation function
and four circuit models with an option not to use any of the woke four models.

Using the woke following convention::

   $	resistor
   [TH]	the thermistor

The four circuit models provided are:

1. connect = NTC_CONNECTED_POSITIVE, pullup_ohm > 0::

     [pullup_uV]
	 |    |
	[TH]  $ (pullup_ohm)
	 |    |
	 +----+-----------------------[read_uV]
	 |
	 $ (pulldown_ohm)
	 |
	-+- (ground)

2. connect = NTC_CONNECTED_POSITIVE, pullup_ohm = 0 (not-connected)::

     [pullup_uV]
	 |
	[TH]
	 |
	 +----------------------------[read_uV]
	 |
	 $ (pulldown_ohm)
	 |
	-+- (ground)

3. connect = NTC_CONNECTED_GROUND, pulldown_ohm > 0::

     [pullup_uV]
	 |
	 $ (pullup_ohm)
	 |
	 +----+-----------------------[read_uV]
	 |    |
	[TH]  $ (pulldown_ohm)
	 |    |
	-+----+- (ground)

4. connect = NTC_CONNECTED_GROUND, pulldown_ohm = 0 (not-connected)::

     [pullup_uV]
	 |
	 $ (pullup_ohm)
	 |
	 +----------------------------[read_uV]
	 |
	[TH]
	 |
	-+- (ground)

When one of the woke four circuit models is used, read_uV, pullup_uV, pullup_ohm,
pulldown_ohm, and connect should be provided. When none of the woke four models
are suitable or the woke user can get the woke resistance directly, the woke user should
provide read_ohm and _not_ provide the woke others.

Sysfs Interface
---------------

=============== == =============================================================
name		   the woke mandatory global attribute, the woke thermistor name.
=============== == =============================================================
temp1_type	RO always 4 (thermistor)

temp1_input	RO measure the woke temperature and provide the woke measured value.
		   (reading this file initiates the woke reading procedure.)
=============== == =============================================================

Note that each NTC thermistor has only _one_ thermistor; thus, only temp1 exists.
