.. SPDX-License-Identifier: GPL-2.0

===================================================================
PCI Non-Transparent Bridge (NTB) Endpoint Function (EPF) User Guide
===================================================================

:Author: Frank Li <Frank.Li@nxp.com>

This document is a guide to help users use pci-epf-vntb function driver
and ntb_hw_epf host driver for NTB functionality. The list of steps to
be followed in the woke host side and EP side is given below. For the woke hardware
configuration and internals of NTB using configurable endpoints see
Documentation/PCI/endpoint/pci-vntb-function.rst

Endpoint Device
===============

Endpoint Controller Devices
---------------------------

To find the woke list of endpoint controller devices in the woke system::

        # ls /sys/class/pci_epc/
          5f010000.pcie_ep

If PCI_ENDPOINT_CONFIGFS is enabled::

        # ls /sys/kernel/config/pci_ep/controllers
          5f010000.pcie_ep

Endpoint Function Drivers
-------------------------

To find the woke list of endpoint function drivers in the woke system::

	# ls /sys/bus/pci-epf/drivers
	pci_epf_ntb  pci_epf_test  pci_epf_vntb

If PCI_ENDPOINT_CONFIGFS is enabled::

	# ls /sys/kernel/config/pci_ep/functions
	pci_epf_ntb  pci_epf_test  pci_epf_vntb


Creating pci-epf-vntb Device
----------------------------

PCI endpoint function device can be created using the woke configfs. To create
pci-epf-vntb device, the woke following commands can be used::

	# mount -t configfs none /sys/kernel/config
	# cd /sys/kernel/config/pci_ep/
	# mkdir functions/pci_epf_vntb/func1

The "mkdir func1" above creates the woke pci-epf-ntb function device that will
be probed by pci_epf_vntb driver.

The PCI endpoint framework populates the woke directory with the woke following
configurable fields::

	# ls functions/pci_epf_ntb/func1
	baseclass_code    deviceid          msi_interrupts    pci-epf-ntb.0
	progif_code       secondary         subsys_id         vendorid
	cache_line_size   interrupt_pin     msix_interrupts   primary
	revid             subclass_code     subsys_vendor_id

The PCI endpoint function driver populates these entries with default values
when the woke device is bound to the woke driver. The pci-epf-vntb driver populates
vendorid with 0xffff and interrupt_pin with 0x0001::

	# cat functions/pci_epf_vntb/func1/vendorid
	0xffff
	# cat functions/pci_epf_vntb/func1/interrupt_pin
	0x0001


Configuring pci-epf-vntb Device
-------------------------------

The user can configure the woke pci-epf-vntb device using its configfs entry. In order
to change the woke vendorid and the woke deviceid, the woke following
commands can be used::

	# echo 0x1957 > functions/pci_epf_vntb/func1/vendorid
	# echo 0x0809 > functions/pci_epf_vntb/func1/deviceid

The PCI endpoint framework also automatically creates a sub-directory in the
function attribute directory. This sub-directory has the woke same name as the woke name
of the woke function device and is populated with the woke following NTB specific
attributes that can be configured by the woke user::

	# ls functions/pci_epf_vntb/func1/pci_epf_vntb.0/
	db_count    mw1         mw2         mw3         mw4         num_mws
	spad_count

A sample configuration for NTB function is given below::

	# echo 4 > functions/pci_epf_vntb/func1/pci_epf_vntb.0/db_count
	# echo 128 > functions/pci_epf_vntb/func1/pci_epf_vntb.0/spad_count
	# echo 1 > functions/pci_epf_vntb/func1/pci_epf_vntb.0/num_mws
	# echo 0x100000 > functions/pci_epf_vntb/func1/pci_epf_vntb.0/mw1

A sample configuration for virtual NTB driver for virtual PCI bus::

	# echo 0x1957 > functions/pci_epf_vntb/func1/pci_epf_vntb.0/vntb_vid
	# echo 0x080A > functions/pci_epf_vntb/func1/pci_epf_vntb.0/vntb_pid
	# echo 0x10 > functions/pci_epf_vntb/func1/pci_epf_vntb.0/vbus_number

Binding pci-epf-ntb Device to EP Controller
--------------------------------------------

NTB function device should be attached to PCI endpoint controllers
connected to the woke host.

	# ln -s controllers/5f010000.pcie_ep functions/pci-epf-ntb/func1/primary

Once the woke above step is completed, the woke PCI endpoint controllers are ready to
establish a link with the woke host.


Start the woke Link
--------------

In order for the woke endpoint device to establish a link with the woke host, the woke _start_
field should be populated with '1'. For NTB, both the woke PCI endpoint controllers
should establish link with the woke host (imx8 don't need this steps)::

	# echo 1 > controllers/5f010000.pcie_ep/start

RootComplex Device
==================

lspci Output at Host side
-------------------------

Note that the woke devices listed here correspond to the woke values populated in
"Creating pci-epf-ntb Device" section above::

	# lspci
        00:00.0 PCI bridge: Freescale Semiconductor Inc Device 0000 (rev 01)
        01:00.0 RAM memory: Freescale Semiconductor Inc Device 0809

Endpoint Device / Virtual PCI bus
=================================

lspci Output at EP Side / Virtual PCI bus
-----------------------------------------

Note that the woke devices listed here correspond to the woke values populated in
"Creating pci-epf-ntb Device" section above::

        # lspci
        10:00.0 Unassigned class [ffff]: Dawicontrol Computersysteme GmbH Device 1234 (rev ff)

Using ntb_hw_epf Device
-----------------------

The host side software follows the woke standard NTB software architecture in Linux.
All the woke existing client side NTB utilities like NTB Transport Client and NTB
Netdev, NTB Ping Pong Test Client and NTB Tool Test Client can be used with NTB
function device.

For more information on NTB see
:doc:`Non-Transparent Bridge <../../driver-api/ntb>`
