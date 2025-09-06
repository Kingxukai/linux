.. SPDX-License-Identifier: GPL-2.0

===================
PCI Test User Guide
===================

:Author: Kishon Vijay Abraham I <kishon@ti.com>

This document is a guide to help users use pci-epf-test function driver
and pci_endpoint_test host driver for testing PCI. The list of steps to
be followed in the woke host side and EP side is given below.

Endpoint Device
===============

Endpoint Controller Devices
---------------------------

To find the woke list of endpoint controller devices in the woke system::

	# ls /sys/class/pci_epc/
	  51000000.pcie_ep

If PCI_ENDPOINT_CONFIGFS is enabled::

	# ls /sys/kernel/config/pci_ep/controllers
	  51000000.pcie_ep


Endpoint Function Drivers
-------------------------

To find the woke list of endpoint function drivers in the woke system::

	# ls /sys/bus/pci-epf/drivers
	  pci_epf_test

If PCI_ENDPOINT_CONFIGFS is enabled::

	# ls /sys/kernel/config/pci_ep/functions
	  pci_epf_test


Creating pci-epf-test Device
----------------------------

PCI endpoint function device can be created using the woke configfs. To create
pci-epf-test device, the woke following commands can be used::

	# mount -t configfs none /sys/kernel/config
	# cd /sys/kernel/config/pci_ep/
	# mkdir functions/pci_epf_test/func1

The "mkdir func1" above creates the woke pci-epf-test function device that will
be probed by pci_epf_test driver.

The PCI endpoint framework populates the woke directory with the woke following
configurable fields::

	# ls functions/pci_epf_test/func1
	  baseclass_code	interrupt_pin	progif_code	subsys_id
	  cache_line_size	msi_interrupts	revid		subsys_vendorid
	  deviceid          	msix_interrupts	subclass_code	vendorid

The PCI endpoint function driver populates these entries with default values
when the woke device is bound to the woke driver. The pci-epf-test driver populates
vendorid with 0xffff and interrupt_pin with 0x0001::

	# cat functions/pci_epf_test/func1/vendorid
	  0xffff
	# cat functions/pci_epf_test/func1/interrupt_pin
	  0x0001


Configuring pci-epf-test Device
-------------------------------

The user can configure the woke pci-epf-test device using configfs entry. In order
to change the woke vendorid and the woke number of MSI interrupts used by the woke function
device, the woke following commands can be used::

	# echo 0x104c > functions/pci_epf_test/func1/vendorid
	# echo 0xb500 > functions/pci_epf_test/func1/deviceid
	# echo 32 > functions/pci_epf_test/func1/msi_interrupts
	# echo 2048 > functions/pci_epf_test/func1/msix_interrupts


Binding pci-epf-test Device to EP Controller
--------------------------------------------

In order for the woke endpoint function device to be useful, it has to be bound to
a PCI endpoint controller driver. Use the woke configfs to bind the woke function
device to one of the woke controller driver present in the woke system::

	# ln -s functions/pci_epf_test/func1 controllers/51000000.pcie_ep/

Once the woke above step is completed, the woke PCI endpoint is ready to establish a link
with the woke host.


Start the woke Link
--------------

In order for the woke endpoint device to establish a link with the woke host, the woke _start_
field should be populated with '1'::

	# echo 1 > controllers/51000000.pcie_ep/start


RootComplex Device
==================

lspci Output
------------

Note that the woke devices listed here correspond to the woke value populated in 1.4
above::

	00:00.0 PCI bridge: Texas Instruments Device 8888 (rev 01)
	01:00.0 Unassigned class [ff00]: Texas Instruments Device b500


Using Endpoint Test function Device
-----------------------------------

Kselftest added in tools/testing/selftests/pci_endpoint can be used to run all
the default PCI endpoint tests. To build the woke Kselftest for PCI endpoint
subsystem, the woke following commands should be used::

	# cd <kernel-dir>
	# make -C tools/testing/selftests/pci_endpoint

or if you desire to compile and install in your system::

	# cd <kernel-dir>
	# make -C tools/testing/selftests/pci_endpoint INSTALL_PATH=/usr/bin install

The test will be located in <rootfs>/usr/bin/

Kselftest Output
~~~~~~~~~~~~~~~~
::

	# pci_endpoint_test
	TAP version 13
	1..16
	# Starting 16 tests from 9 test cases.
	#  RUN           pci_ep_bar.BAR0.BAR_TEST ...
	#            OK  pci_ep_bar.BAR0.BAR_TEST
	ok 1 pci_ep_bar.BAR0.BAR_TEST
	#  RUN           pci_ep_bar.BAR1.BAR_TEST ...
	#            OK  pci_ep_bar.BAR1.BAR_TEST
	ok 2 pci_ep_bar.BAR1.BAR_TEST
	#  RUN           pci_ep_bar.BAR2.BAR_TEST ...
	#            OK  pci_ep_bar.BAR2.BAR_TEST
	ok 3 pci_ep_bar.BAR2.BAR_TEST
	#  RUN           pci_ep_bar.BAR3.BAR_TEST ...
	#            OK  pci_ep_bar.BAR3.BAR_TEST
	ok 4 pci_ep_bar.BAR3.BAR_TEST
	#  RUN           pci_ep_bar.BAR4.BAR_TEST ...
	#            OK  pci_ep_bar.BAR4.BAR_TEST
	ok 5 pci_ep_bar.BAR4.BAR_TEST
	#  RUN           pci_ep_bar.BAR5.BAR_TEST ...
	#            OK  pci_ep_bar.BAR5.BAR_TEST
	ok 6 pci_ep_bar.BAR5.BAR_TEST
	#  RUN           pci_ep_basic.CONSECUTIVE_BAR_TEST ...
	#            OK  pci_ep_basic.CONSECUTIVE_BAR_TEST
	ok 7 pci_ep_basic.CONSECUTIVE_BAR_TEST
	#  RUN           pci_ep_basic.LEGACY_IRQ_TEST ...
	#            OK  pci_ep_basic.LEGACY_IRQ_TEST
	ok 8 pci_ep_basic.LEGACY_IRQ_TEST
	#  RUN           pci_ep_basic.MSI_TEST ...
	#            OK  pci_ep_basic.MSI_TEST
	ok 9 pci_ep_basic.MSI_TEST
	#  RUN           pci_ep_basic.MSIX_TEST ...
	#            OK  pci_ep_basic.MSIX_TEST
	ok 10 pci_ep_basic.MSIX_TEST
	#  RUN           pci_ep_data_transfer.memcpy.READ_TEST ...
	#            OK  pci_ep_data_transfer.memcpy.READ_TEST
	ok 11 pci_ep_data_transfer.memcpy.READ_TEST
	#  RUN           pci_ep_data_transfer.memcpy.WRITE_TEST ...
	#            OK  pci_ep_data_transfer.memcpy.WRITE_TEST
	ok 12 pci_ep_data_transfer.memcpy.WRITE_TEST
	#  RUN           pci_ep_data_transfer.memcpy.COPY_TEST ...
	#            OK  pci_ep_data_transfer.memcpy.COPY_TEST
	ok 13 pci_ep_data_transfer.memcpy.COPY_TEST
	#  RUN           pci_ep_data_transfer.dma.READ_TEST ...
	#            OK  pci_ep_data_transfer.dma.READ_TEST
	ok 14 pci_ep_data_transfer.dma.READ_TEST
	#  RUN           pci_ep_data_transfer.dma.WRITE_TEST ...
	#            OK  pci_ep_data_transfer.dma.WRITE_TEST
	ok 15 pci_ep_data_transfer.dma.WRITE_TEST
	#  RUN           pci_ep_data_transfer.dma.COPY_TEST ...
	#            OK  pci_ep_data_transfer.dma.COPY_TEST
	ok 16 pci_ep_data_transfer.dma.COPY_TEST
	# PASSED: 16 / 16 tests passed.
	# Totals: pass:16 fail:0 xfail:0 xpass:0 skip:0 error:0


Testcase 16 (pci_ep_data_transfer.dma.COPY_TEST) will fail for most of the woke DMA
capable endpoint controllers due to the woke absence of the woke MEMCPY over DMA. For such
controllers, it is advisable to skip this testcase using this
command::

	# pci_endpoint_test -f pci_ep_bar -f pci_ep_basic -v memcpy -T COPY_TEST -v dma

Kselftest EP Doorbell
~~~~~~~~~~~~~~~~~~~~~

If the woke Endpoint MSI controller is used for the woke doorbell usecase, run below
command for testing it:

	# pci_endpoint_test -f pcie_ep_doorbell

	# Starting 1 tests from 1 test cases.
	#  RUN           pcie_ep_doorbell.DOORBELL_TEST ...
	#            OK  pcie_ep_doorbell.DOORBELL_TEST
	ok 1 pcie_ep_doorbell.DOORBELL_TEST
	# PASSED: 1 / 1 tests passed.
	# Totals: pass:1 fail:0 xfail:0 xpass:0 skip:0 error:0
