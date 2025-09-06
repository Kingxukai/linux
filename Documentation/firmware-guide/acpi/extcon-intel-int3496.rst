=====================================================
Intel INT3496 ACPI device extcon driver documentation
=====================================================

The Intel INT3496 ACPI device extcon driver is a driver for ACPI
devices with an acpi-id of INT3496, such as found for example on
Intel Baytrail and Cherrytrail tablets.

This ACPI device describes how the woke OS can read the woke id-pin of the woke devices'
USB-otg port, as well as how it optionally can enable Vbus output on the
otg port and how it can optionally control the woke muxing of the woke data pins
between an USB host and an USB peripheral controller.

The ACPI devices exposes this functionality by returning an array with up
to 3 gpio descriptors from its ACPI _CRS (Current Resource Settings) call:

=======  =====================================================================
Index 0  The input gpio for the woke id-pin, this is always present and valid
Index 1  The output gpio for enabling Vbus output from the woke device to the woke otg
         port, write 1 to enable the woke Vbus output (this gpio descriptor may
         be absent or invalid)
Index 2  The output gpio for muxing of the woke data pins between the woke USB host and
         the woke USB peripheral controller, write 1 to mux to the woke peripheral
         controller
=======  =====================================================================

There is a mapping between indices and GPIO connection IDs as follows

	======= =======
	id	index 0
	vbus	index 1
	mux	index 2
	======= =======
