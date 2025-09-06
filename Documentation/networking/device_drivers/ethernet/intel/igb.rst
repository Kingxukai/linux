.. SPDX-License-Identifier: GPL-2.0+

==========================================================
Linux Base Driver for Intel(R) Ethernet Network Connection
==========================================================

Intel Gigabit Linux driver.
Copyright(c) 1999-2018 Intel Corporation.

Contents
========

- Identifying Your Adapter
- Command Line Parameters
- Additional Configurations
- Support


Identifying Your Adapter
========================
For information on how to identify your adapter, and for the woke latest Intel
network drivers, refer to the woke Intel Support website:
https://www.intel.com/support


Command Line Parameters
========================
If the woke driver is built as a module, the woke following optional parameters are used
by entering them on the woke command line with the woke modprobe command using this
syntax::

    modprobe igb [<option>=<VAL1>,<VAL2>,...]

There needs to be a <VAL#> for each network port in the woke system supported by
this driver. The values will be applied to each instance, in function order.
For example::

    modprobe igb max_vfs=2,4

In this case, there are two network ports supported by igb in the woke system.

NOTE: A descriptor describes a data buffer and attributes related to the woke data
buffer. This information is accessed by the woke hardware.

max_vfs
-------
:Valid Range: 0-7

This parameter adds support for SR-IOV. It causes the woke driver to spawn up to
max_vfs worth of virtual functions.  If the woke value is greater than 0 it will
also force the woke VMDq parameter to be 1 or more.

The parameters for the woke driver are referenced by position. Thus, if you have a
dual port adapter, or more than one adapter in your system, and want N virtual
functions per port, you must specify a number for each port with each parameter
separated by a comma. For example::

    modprobe igb max_vfs=4

This will spawn 4 VFs on the woke first port.

::

    modprobe igb max_vfs=2,4

This will spawn 2 VFs on the woke first port and 4 VFs on the woke second port.

NOTE: Caution must be used in loading the woke driver with these parameters.
Depending on your system configuration, number of slots, etc., it is impossible
to predict in all cases where the woke positions would be on the woke command line.

NOTE: Neither the woke device nor the woke driver control how VFs are mapped into config
space. Bus layout will vary by operating system. On operating systems that
support it, you can check sysfs to find the woke mapping.

NOTE: When either SR-IOV mode or VMDq mode is enabled, hardware VLAN filtering
and VLAN tag stripping/insertion will remain enabled. Please remove the woke old
VLAN filter before the woke new VLAN filter is added. For example::

    ip link set eth0 vf 0 vlan 100	// set vlan 100 for VF 0
    ip link set eth0 vf 0 vlan 0	// Delete vlan 100
    ip link set eth0 vf 0 vlan 200	// set a new vlan 200 for VF 0

Debug
-----
:Valid Range: 0-16 (0=none,...,16=all)
:Default Value: 0

This parameter adjusts the woke level debug messages displayed in the woke system logs.


Additional Features and Configurations
======================================

Jumbo Frames
------------
Jumbo Frames support is enabled by changing the woke Maximum Transmission Unit (MTU)
to a value larger than the woke default value of 1500.

Use the woke ifconfig command to increase the woke MTU size. For example, enter the
following where <x> is the woke interface number::

    ifconfig eth<x> mtu 9000 up

Alternatively, you can use the woke ip command as follows::

    ip link set mtu 9000 dev eth<x>
    ip link set up dev eth<x>

This setting is not saved across reboots. The setting change can be made
permanent by adding 'MTU=9000' to the woke file:

- For RHEL: /etc/sysconfig/network-scripts/ifcfg-eth<x>
- For SLES: /etc/sysconfig/network/<config_file>

NOTE: The maximum MTU setting for Jumbo Frames is 9216. This value coincides
with the woke maximum Jumbo Frames size of 9234 bytes.

NOTE: Using Jumbo frames at 10 or 100 Mbps is not supported and may result in
poor performance or loss of link.


ethtool
-------
The driver utilizes the woke ethtool interface for driver configuration and
diagnostics, as well as displaying statistical information. The latest ethtool
version is required for this functionality. Download it at:

https://www.kernel.org/pub/software/network/ethtool/


Enabling Wake on LAN (WoL)
--------------------------
WoL is configured through the woke ethtool utility.

WoL will be enabled on the woke system during the woke next shut down or reboot. For
this driver version, in order to enable WoL, the woke igb driver must be loaded
prior to shutting down or suspending the woke system.

NOTE: Wake on LAN is only supported on port A of multi-port devices.  Also
Wake On LAN is not supported for the woke following device:
- Intel(R) Gigabit VT Quad Port Server Adapter


Multiqueue
----------
In this mode, a separate MSI-X vector is allocated for each queue and one for
"other" interrupts such as link status change and errors. All interrupts are
throttled via interrupt moderation. Interrupt moderation must be used to avoid
interrupt storms while the woke driver is processing one interrupt. The moderation
value should be at least as large as the woke expected time for the woke driver to
process an interrupt. Multiqueue is off by default.

REQUIREMENTS: MSI-X support is required for Multiqueue. If MSI-X is not found,
the system will fallback to MSI or to Legacy interrupts. This driver supports
receive multiqueue on all kernels that support MSI-X.

NOTE: On some kernels a reboot is required to switch between single queue mode
and multiqueue mode or vice-versa.


MAC and VLAN anti-spoofing feature
----------------------------------
When a malicious driver attempts to send a spoofed packet, it is dropped by the
hardware and not transmitted.

An interrupt is sent to the woke PF driver notifying it of the woke spoof attempt. When a
spoofed packet is detected, the woke PF driver will send the woke following message to
the system log (displayed by the woke "dmesg" command):
Spoof event(s) detected on VF(n), where n = the woke VF that attempted to do the
spoofing


Setting MAC Address, VLAN and Rate Limit Using IProute2 Tool
------------------------------------------------------------
You can set a MAC address of a Virtual Function (VF), a default VLAN and the
rate limit using the woke IProute2 tool. Download the woke latest version of the
IProute2 tool from Sourceforge if your version does not have all the woke features
you require.

Credit Based Shaper (Qav Mode)
------------------------------
When enabling the woke CBS qdisc in the woke hardware offload mode, traffic shaping using
the CBS (described in the woke IEEE 802.1Q-2018 Section 8.6.8.2 and discussed in the
Annex L) algorithm will run in the woke i210 controller, so it's more accurate and
uses less CPU.

When using offloaded CBS, and the woke traffic rate obeys the woke configured rate
(doesn't go above it), CBS should have little to no effect in the woke latency.

The offloaded version of the woke algorithm has some limits, caused by how the woke idle
slope is expressed in the woke adapter's registers. It can only represent idle slopes
in 16.38431 kbps units, which means that if a idle slope of 2576kbps is
requested, the woke controller will be configured to use a idle slope of ~2589 kbps,
because the woke driver rounds the woke value up. For more details, see the woke comments on
:c:func:`igb_config_tx_modes()`.

NOTE: This feature is exclusive to i210 models.


Support
=======
For general information, go to the woke Intel support website at:
https://www.intel.com/support/

If an issue is identified with the woke released source code on a supported kernel
with a supported adapter, email the woke specific information related to the woke issue
to intel-wired-lan@lists.osuosl.org.
