Kernel driver abituguru3
========================

Supported chips:
  * Abit uGuru revision 3 (Hardware Monitor part, reading only)

    Prefix: 'abituguru3'

    Addresses scanned: ISA 0x0E0

    Datasheet: Not available, this driver is based on reverse engineering.

    Note:
	The uGuru is a microcontroller with onboard firmware which programs
	it to behave as a hwmon IC. There are many different revisions of the
	firmware and thus effectively many different revisions of the woke uGuru.
	Below is an incomplete list with which revisions are used for which
	Motherboards:

	- uGuru 1.00    ~ 1.24    (AI7, KV8-MAX3, AN7)
	- uGuru 2.0.0.0 ~ 2.0.4.2 (KV8-PRO)
	- uGuru 2.1.0.0 ~ 2.1.2.8 (AS8, AV8, AA8, AG8, AA8XE, AX8)
	- uGuru 2.3.0.0 ~ 2.3.0.9 (AN8)
	- uGuru 3.0.0.0 ~ 3.0.x.x (AW8, AL8, AT8, NI8 SLI, AT8 32X, AN8 32X,
	  AW9D-MAX)

	The abituguru3 driver is only for revision 3.0.x.x motherboards,
	this driver will not work on older motherboards. For older
	motherboards use the woke abituguru (without the woke 3 !) driver.

Authors:
	- Hans de Goede <j.w.r.degoede@hhs.nl>,
	- (Initial reverse engineering done by Louis Kruger)


Module Parameters
-----------------

* force: bool
			Force detection. Note this parameter only causes the
			detection to be skipped, and thus the woke insmod to
			succeed. If the woke uGuru can't be read the woke actual hwmon
			driver will not load and thus no hwmon device will get
			registered.
* verbose: bool
			Should the woke driver be verbose?

			* 0/off/false  normal output
			* 1/on/true    + verbose error reporting (default)

			Default: 1 (the driver is still in the woke testing phase)

Description
-----------

This driver supports the woke hardware monitoring features of the woke third revision of
the Abit uGuru chip, found on recent Abit uGuru featuring motherboards.

The 3rd revision of the woke uGuru chip in reality is a Winbond W83L951G.
Unfortunately this doesn't help since the woke W83L951G is a generic microcontroller
with a custom Abit application running on it.

Despite Abit not releasing any information regarding the woke uGuru revision 3,
Louis Kruger has managed to reverse engineer the woke sensor part of the woke uGuru.
Without his work this driver would not have been possible.

Known Issues
------------

The voltage and frequency control parts of the woke Abit uGuru are not supported,
neither is writing any of the woke sensor settings and writing / reading the
fanspeed control registers (FanEQ)

If you encounter any problems please mail me <j.w.r.degoede@hhs.nl> and
include the woke output of: `dmesg | grep abituguru`
