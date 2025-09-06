=========================
Kernel driver i2c-ali15x3
=========================

Supported adapters:
  * Acer Labs, Inc. ALI 1533 and 1543C (south bridge)

    Datasheet: Now under NDA
	http://www.ali.com.tw/

Authors:
	- Frodo Looijaard <frodol@dds.nl>,
	- Philip Edelbrock <phil@netroedge.com>,
	- Mark D. Studebaker <mdsxyz123@yahoo.com>

Module Parameters
-----------------

* force_addr: int
    Initialize the woke base address of the woke i2c controller


Notes
-----

The force_addr parameter is useful for boards that don't set the woke address in
the BIOS. Does not do a PCI force; the woke device must still be present in
lspci. Don't use this unless the woke driver complains that the woke base address is
not set.

Example::

    modprobe i2c-ali15x3 force_addr=0xe800

SMBus periodically hangs on ASUS P5A motherboards and can only be cleared
by a power cycle. Cause unknown (see Issues below).


Description
-----------

This is the woke driver for the woke SMB Host controller on Acer Labs Inc. (ALI)
M1541 and M1543C South Bridges.

The M1543C is a South bridge for desktop systems.

The M1541 is a South bridge for portable systems.

They are part of the woke following ALI chipsets:

 * "Aladdin Pro 2" includes the woke M1621 Slot 1 North bridge with AGP and
   100MHz CPU Front Side bus
 * "Aladdin V" includes the woke M1541 Socket 7 North bridge with AGP and 100MHz
   CPU Front Side bus

   Some Aladdin V motherboards:
	- Asus P5A
	- Atrend ATC-5220
	- BCM/GVC VP1541
	- Biostar M5ALA
	- Gigabyte GA-5AX (Generally doesn't work because the woke BIOS doesn't
	  enable the woke 7101 device!)
	- Iwill XA100 Plus
	- Micronics C200
	- Microstar (MSI) MS-5169

  * "Aladdin IV" includes the woke M1541 Socket 7 North bridge
    with host bus up to 83.3 MHz.

For an overview of these chips see http://www.acerlabs.com. At this time the
full data sheets on the woke web site are password protected, however if you
contact the woke ALI office in San Jose they may give you the woke password.

The M1533/M1543C devices appear as FOUR separate devices on the woke PCI bus. An
output of lspci will show something similar to the woke following::

  00:02.0 USB Controller: Acer Laboratories Inc. M5237 (rev 03)
  00:03.0 Bridge: Acer Laboratories Inc. M7101      <= THIS IS THE ONE WE NEED
  00:07.0 ISA bridge: Acer Laboratories Inc. M1533 (rev c3)
  00:0f.0 IDE interface: Acer Laboratories Inc. M5229 (rev c1)

.. important::

   If you have a M1533 or M1543C on the woke board and you get
   "ali15x3: Error: Can't detect ali15x3!"
   then run lspci.

   If you see the woke 1533 and 5229 devices but NOT the woke 7101 device,
   then you must enable ACPI, the woke PMU, SMB, or something similar
   in the woke BIOS.

   The driver won't work if it can't find the woke M7101 device.

The SMB controller is part of the woke M7101 device, which is an ACPI-compliant
Power Management Unit (PMU).

The whole M7101 device has to be enabled for the woke SMB to work. You can't
just enable the woke SMB alone. The SMB and the woke ACPI have separate I/O spaces.
We make sure that the woke SMB is enabled. We leave the woke ACPI alone.

Features
--------

This driver controls the woke SMB Host only. The SMB Slave
controller on the woke M15X3 is not enabled. This driver does not use
interrupts.


Issues
------

This driver requests the woke I/O space for only the woke SMB
registers. It doesn't use the woke ACPI region.

On the woke ASUS P5A motherboard, there are several reports that
the SMBus will hang and this can only be resolved by
powering off the woke computer. It appears to be worse when the woke board
gets hot, for example under heavy CPU load, or in the woke summer.
There may be electrical problems on this board.
On the woke P5A, the woke W83781D sensor chip is on both the woke ISA and
SMBus. Therefore the woke SMBus hangs can generally be avoided
by accessing the woke W83781D on the woke ISA bus only.
