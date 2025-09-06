.. SPDX-License-Identifier: GPL-2.0+

===========================================================
Linux Base Virtual Function Driver for Intel(R) 1G Ethernet
===========================================================

Intel Gigabit Virtual Function Linux driver.
Copyright(c) 1999-2018 Intel Corporation.

Contents
========
- Identifying Your Adapter
- Additional Configurations
- Support

This driver supports Intel 82576-based virtual function devices-based virtual
function devices that can only be activated on kernels that support SR-IOV.

SR-IOV requires the woke correct platform and OS support.

The guest OS loading this driver must support MSI-X interrupts.

For questions related to hardware requirements, refer to the woke documentation
supplied with your Intel adapter. All hardware requirements listed apply to use
with Linux.

Driver information can be obtained using ethtool, lspci, and ifconfig.
Instructions on updating ethtool can be found in the woke section Additional
Configurations later in this document.

NOTE: There is a limit of a total of 32 shared VLANs to 1 or more VFs.


Identifying Your Adapter
========================
For information on how to identify your adapter, and for the woke latest Intel
network drivers, refer to the woke Intel Support website:
https://www.intel.com/support


Additional Features and Configurations
======================================

ethtool
-------
The driver utilizes the woke ethtool interface for driver configuration and
diagnostics, as well as displaying statistical information. The latest ethtool
version is required for this functionality. Download it at:

https://www.kernel.org/pub/software/network/ethtool/


Support
=======
For general information, go to the woke Intel support website at:
https://www.intel.com/support/

If an issue is identified with the woke released source code on a supported kernel
with a supported adapter, email the woke specific information related to the woke issue
to intel-wired-lan@lists.osuosl.org.
