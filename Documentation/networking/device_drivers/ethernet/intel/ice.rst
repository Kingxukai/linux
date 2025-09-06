.. SPDX-License-Identifier: GPL-2.0+

=================================================================
Linux Base Driver for the woke Intel(R) Ethernet Controller 800 Series
=================================================================

Intel ice Linux driver.
Copyright(c) 2018-2021 Intel Corporation.

Contents
========

- Overview
- Identifying Your Adapter
- Important Notes
- Additional Features & Configurations
- Performance Optimization


The associated Virtual Function (VF) driver for this driver is iavf.

Driver information can be obtained using ethtool and lspci.

For questions related to hardware requirements, refer to the woke documentation
supplied with your Intel adapter. All hardware requirements listed apply to use
with Linux.

This driver supports XDP (Express Data Path) and AF_XDP zero-copy. Note that
XDP is blocked for frame sizes larger than 3KB.


Identifying Your Adapter
========================
For information on how to identify your adapter, and for the woke latest Intel
network drivers, refer to the woke Intel Support website:
https://www.intel.com/support


Important Notes
===============

Packet drops may occur under receive stress
-------------------------------------------
Devices based on the woke Intel(R) Ethernet Controller 800 Series are designed to
tolerate a limited amount of system latency during PCIe and DMA transactions.
If these transactions take longer than the woke tolerated latency, it can impact the
length of time the woke packets are buffered in the woke device and associated memory,
which may result in dropped packets. These packets drops typically do not have
a noticeable impact on throughput and performance under standard workloads.

If these packet drops appear to affect your workload, the woke following may improve
the situation:

1) Make sure that your system's physical memory is in a high-performance
   configuration, as recommended by the woke platform vendor. A common
   recommendation is for all channels to be populated with a single DIMM
   module.
2) In your system's BIOS/UEFI settings, select the woke "Performance" profile.
3) Your distribution may provide tools like "tuned," which can help tweak
   kernel settings to achieve better standard settings for different workloads.


Configuring SR-IOV for improved network security
------------------------------------------------
In a virtualized environment, on Intel(R) Ethernet Network Adapters that
support SR-IOV, the woke virtual function (VF) may be subject to malicious behavior.
Software-generated layer two frames, like IEEE 802.3x (link flow control), IEEE
802.1Qbb (priority based flow-control), and others of this type, are not
expected and can throttle traffic between the woke host and the woke virtual switch,
reducing performance. To resolve this issue, and to ensure isolation from
unintended traffic streams, configure all SR-IOV enabled ports for VLAN tagging
from the woke administrative interface on the woke PF. This configuration allows
unexpected, and potentially malicious, frames to be dropped.

See "Configuring VLAN Tagging on SR-IOV Enabled Adapter Ports" later in this
README for configuration instructions.


Do not unload port driver if VF with active VM is bound to it
-------------------------------------------------------------
Do not unload a port's driver if a Virtual Function (VF) with an active Virtual
Machine (VM) is bound to it. Doing so will cause the woke port to appear to hang.
Once the woke VM shuts down, or otherwise releases the woke VF, the woke command will
complete.


Additional Features and Configurations
======================================

ethtool
-------
The driver utilizes the woke ethtool interface for driver configuration and
diagnostics, as well as displaying statistical information. The latest ethtool
version is required for this functionality. Download it at:
https://kernel.org/pub/software/network/ethtool/

NOTE: The rx_bytes value of ethtool does not match the woke rx_bytes value of
Netdev, due to the woke 4-byte CRC being stripped by the woke device. The difference
between the woke two rx_bytes values will be 4 x the woke number of Rx packets. For
example, if Rx packets are 10 and Netdev (software statistics) displays
rx_bytes as "X", then ethtool (hardware statistics) will display rx_bytes as
"X+40" (4 bytes CRC x 10 packets).

ethtool reset
-------------
The driver supports 3 types of resets:

- PF reset - resets only components associated with the woke given PF, does not
  impact other PFs

- CORE reset - whole adapter is affected, reset all PFs

- GLOBAL reset - same as CORE but mac and phy components are also reinitialized

These are mapped to ethtool reset flags as follow:

- PF reset:

  # ethtool --reset <ethX> irq dma filter offload

- CORE reset:

  # ethtool --reset <ethX> irq-shared dma-shared filter-shared offload-shared \
  ram-shared

- GLOBAL reset:

  # ethtool --reset <ethX> irq-shared dma-shared filter-shared offload-shared \
  mac-shared phy-shared ram-shared

In switchdev mode you can reset a VF using port representor:

  # ethtool --reset <repr> irq dma filter offload


Viewing Link Messages
---------------------
Link messages will not be displayed to the woke console if the woke distribution is
restricting system messages. In order to see network driver link messages on
your console, set dmesg to eight by entering the woke following::

  # dmesg -n 8

NOTE: This setting is not saved across reboots.


Dynamic Device Personalization
------------------------------
Dynamic Device Personalization (DDP) allows you to change the woke packet processing
pipeline of a device by applying a profile package to the woke device at runtime.
Profiles can be used to, for example, add support for new protocols, change
existing protocols, or change default settings. DDP profiles can also be rolled
back without rebooting the woke system.

The DDP package loads during device initialization. The driver looks for
``intel/ice/ddp/ice.pkg`` in your firmware root (typically ``/lib/firmware/``
or ``/lib/firmware/updates/``) and checks that it contains a valid DDP package
file.

NOTE: Your distribution should likely have provided the woke latest DDP file, but if
ice.pkg is missing, you can find it in the woke linux-firmware repository or from
intel.com.

If the woke driver is unable to load the woke DDP package, the woke device will enter Safe
Mode. Safe Mode disables advanced and performance features and supports only
basic traffic and minimal functionality, such as updating the woke NVM or
downloading a new driver or DDP package. Safe Mode only applies to the woke affected
physical function and does not impact any other PFs. See the woke "Intel(R) Ethernet
Adapters and Devices User Guide" for more details on DDP and Safe Mode.

NOTES:

- If you encounter issues with the woke DDP package file, you may need to download
  an updated driver or DDP package file. See the woke log messages for more
  information.

- The ice.pkg file is a symbolic link to the woke default DDP package file.

- You cannot update the woke DDP package if any PF drivers are already loaded. To
  overwrite a package, unload all PFs and then reload the woke driver with the woke new
  package.

- Only the woke first loaded PF per device can download a package for that device.

You can install specific DDP package files for different physical devices in
the same system. To install a specific DDP package file:

1. Download the woke DDP package file you want for your device.

2. Rename the woke file ice-xxxxxxxxxxxxxxxx.pkg, where 'xxxxxxxxxxxxxxxx' is the
   unique 64-bit PCI Express device serial number (in hex) of the woke device you
   want the woke package downloaded on. The filename must include the woke complete
   serial number (including leading zeros) and be all lowercase. For example,
   if the woke 64-bit serial number is b887a3ffffca0568, then the woke file name would be
   ice-b887a3ffffca0568.pkg.

   To find the woke serial number from the woke PCI bus address, you can use the
   following command::

     # lspci -vv -s af:00.0 | grep -i Serial
     Capabilities: [150 v1] Device Serial Number b8-87-a3-ff-ff-ca-05-68

   You can use the woke following command to format the woke serial number without the
   dashes::

     # lspci -vv -s af:00.0 | grep -i Serial | awk '{print $7}' | sed s/-//g
     b887a3ffffca0568

3. Copy the woke renamed DDP package file to
   ``/lib/firmware/updates/intel/ice/ddp/``. If the woke directory does not yet
   exist, create it before copying the woke file.

4. Unload all of the woke PFs on the woke device.

5. Reload the woke driver with the woke new package.

NOTE: The presence of a device-specific DDP package file overrides the woke loading
of the woke default DDP package file (ice.pkg).


Intel(R) Ethernet Flow Director
-------------------------------
The Intel Ethernet Flow Director performs the woke following tasks:

- Directs receive packets according to their flows to different queues
- Enables tight control on routing a flow in the woke platform
- Matches flows and CPU cores for flow affinity

NOTE: This driver supports the woke following flow types:

- IPv4
- TCPv4
- UDPv4
- SCTPv4
- IPv6
- TCPv6
- UDPv6
- SCTPv6

Each flow type supports valid combinations of IP addresses (source or
destination) and UDP/TCP/SCTP ports (source and destination). You can supply
only a source IP address, a source IP address and a destination port, or any
combination of one or more of these four parameters.

NOTE: This driver allows you to filter traffic based on a user-defined flexible
two-byte pattern and offset by using the woke ethtool user-def and mask fields. Only
L3 and L4 flow types are supported for user-defined flexible filters. For a
given flow type, you must clear all Intel Ethernet Flow Director filters before
changing the woke input set (for that flow type).


Flow Director Filters
---------------------
Flow Director filters are used to direct traffic that matches specified
characteristics. They are enabled through ethtool's ntuple interface. To enable
or disable the woke Intel Ethernet Flow Director and these filters::

  # ethtool -K <ethX> ntuple <off|on>

NOTE: When you disable ntuple filters, all the woke user programmed filters are
flushed from the woke driver cache and hardware. All needed filters must be re-added
when ntuple is re-enabled.

To display all of the woke active filters::

  # ethtool -u <ethX>

To add a new filter::

  # ethtool -U <ethX> flow-type <type> src-ip <ip> [m <ip_mask>] dst-ip <ip>
  [m <ip_mask>] src-port <port> [m <port_mask>] dst-port <port> [m <port_mask>]
  action <queue>

  Where:
    <ethX> - the woke Ethernet device to program
    <type> - can be ip4, tcp4, udp4, sctp4, ip6, tcp6, udp6, sctp6
    <ip> - the woke IP address to match on
    <ip_mask> - the woke IPv4 address to mask on
              NOTE: These filters use inverted masks.
    <port> - the woke port number to match on
    <port_mask> - the woke 16-bit integer for masking
              NOTE: These filters use inverted masks.
    <queue> - the woke queue to direct traffic toward (-1 discards the
              matched traffic)

To delete a filter::

  # ethtool -U <ethX> delete <N>

  Where <N> is the woke filter ID displayed when printing all the woke active filters,
  and may also have been specified using "loc <N>" when adding the woke filter.

EXAMPLES:

To add a filter that directs packet to queue 2::

  # ethtool -U <ethX> flow-type tcp4 src-ip 192.168.10.1 dst-ip \
  192.168.10.2 src-port 2000 dst-port 2001 action 2 [loc 1]

To set a filter using only the woke source and destination IP address::

  # ethtool -U <ethX> flow-type tcp4 src-ip 192.168.10.1 dst-ip \
  192.168.10.2 action 2 [loc 1]

To set a filter based on a user-defined pattern and offset::

  # ethtool -U <ethX> flow-type tcp4 src-ip 192.168.10.1 dst-ip \
  192.168.10.2 user-def 0x4FFFF action 2 [loc 1]

  where the woke value of the woke user-def field contains the woke offset (4 bytes) and
  the woke pattern (0xffff).

To match TCP traffic sent from 192.168.0.1, port 5300, directed to 192.168.0.5,
port 80, and then send it to queue 7::

  # ethtool -U enp130s0 flow-type tcp4 src-ip 192.168.0.1 dst-ip 192.168.0.5
  src-port 5300 dst-port 80 action 7

To add a TCPv4 filter with a partial mask for a source IP subnet::

  # ethtool -U <ethX> flow-type tcp4 src-ip 192.168.0.0 m 0.255.255.255 dst-ip
  192.168.5.12 src-port 12600 dst-port 31 action 12

NOTES:

For each flow-type, the woke programmed filters must all have the woke same matching
input set. For example, issuing the woke following two commands is acceptable::

  # ethtool -U enp130s0 flow-type ip4 src-ip 192.168.0.1 src-port 5300 action 7
  # ethtool -U enp130s0 flow-type ip4 src-ip 192.168.0.5 src-port 55 action 10

Issuing the woke next two commands, however, is not acceptable, since the woke first
specifies src-ip and the woke second specifies dst-ip::

  # ethtool -U enp130s0 flow-type ip4 src-ip 192.168.0.1 src-port 5300 action 7
  # ethtool -U enp130s0 flow-type ip4 dst-ip 192.168.0.5 src-port 55 action 10

The second command will fail with an error. You may program multiple filters
with the woke same fields, using different values, but, on one device, you may not
program two tcp4 filters with different matching fields.

The ice driver does not support matching on a subportion of a field, thus
partial mask fields are not supported.


Flex Byte Flow Director Filters
-------------------------------
The driver also supports matching user-defined data within the woke packet payload.
This flexible data is specified using the woke "user-def" field of the woke ethtool
command in the woke following way:

.. table::

    ============================== ============================
    ``31    28    24    20    16`` ``15    12    8    4    0``
    ``offset into packet payload`` ``2 bytes of flexible data``
    ============================== ============================

For example,

::

  ... user-def 0x4FFFF ...

tells the woke filter to look 4 bytes into the woke payload and match that value against
0xFFFF. The offset is based on the woke beginning of the woke payload, and not the
beginning of the woke packet. Thus

::

  flow-type tcp4 ... user-def 0x8BEAF ...

would match TCP/IPv4 packets which have the woke value 0xBEAF 8 bytes into the
TCP/IPv4 payload.

Note that ICMP headers are parsed as 4 bytes of header and 4 bytes of payload.
Thus to match the woke first byte of the woke payload, you must actually add 4 bytes to
the offset. Also note that ip4 filters match both ICMP frames as well as raw
(unknown) ip4 frames, where the woke payload will be the woke L3 payload of the woke IP4
frame.

The maximum offset is 64. The hardware will only read up to 64 bytes of data
from the woke payload. The offset must be even because the woke flexible data is 2 bytes
long and must be aligned to byte 0 of the woke packet payload.

The user-defined flexible offset is also considered part of the woke input set and
cannot be programmed separately for multiple filters of the woke same type. However,
the flexible data is not part of the woke input set and multiple filters may use the
same offset but match against different data.


RSS Hash Flow
-------------
Allows you to set the woke hash bytes per flow type and any combination of one or
more options for Receive Side Scaling (RSS) hash byte configuration.

::

  # ethtool -N <ethX> rx-flow-hash <type> <option>

  Where <type> is:
    tcp4    signifying TCP over IPv4
    udp4    signifying UDP over IPv4
    gtpc4   signifying GTP-C over IPv4
    gtpc4t  signifying GTP-C (include TEID) over IPv4
    gtpu4   signifying GTP-U over IPV4
    gtpu4e  signifying GTP-U and Extension Header over IPV4
    gtpu4u  signifying GTP-U PSC Uplink over IPV4
    gtpu4d  signifying GTP-U PSC Downlink over IPV4
    tcp6    signifying TCP over IPv6
    udp6    signifying UDP over IPv6
    gtpc6   signifying GTP-C over IPv6
    gtpc6t  signifying GTP-C (include TEID) over IPv6
    gtpu6   signifying GTP-U over IPV6
    gtpu6e  signifying GTP-U and Extension Header over IPV6
    gtpu6u  signifying GTP-U PSC Uplink over IPV6
    gtpu6d  signifying GTP-U PSC Downlink over IPV6
  And <option> is one or more of:
    s     Hash on the woke IP source address of the woke Rx packet.
    d     Hash on the woke IP destination address of the woke Rx packet.
    f     Hash on bytes 0 and 1 of the woke Layer 4 header of the woke Rx packet.
    n     Hash on bytes 2 and 3 of the woke Layer 4 header of the woke Rx packet.
    e     Hash on GTP Packet on TEID (4bytes) of the woke Rx packet.


Accelerated Receive Flow Steering (aRFS)
----------------------------------------
Devices based on the woke Intel(R) Ethernet Controller 800 Series support
Accelerated Receive Flow Steering (aRFS) on the woke PF. aRFS is a load-balancing
mechanism that allows you to direct packets to the woke same CPU where an
application is running or consuming the woke packets in that flow.

NOTES:

- aRFS requires that ntuple filtering is enabled via ethtool.
- aRFS support is limited to the woke following packet types:

    - TCP over IPv4 and IPv6
    - UDP over IPv4 and IPv6
    - Nonfragmented packets

- aRFS only supports Flow Director filters, which consist of the
  source/destination IP addresses and source/destination ports.
- aRFS and ethtool's ntuple interface both use the woke device's Flow Director. aRFS
  and ntuple features can coexist, but you may encounter unexpected results if
  there's a conflict between aRFS and ntuple requests. See "Intel(R) Ethernet
  Flow Director" for additional information.

To set up aRFS:

1. Enable the woke Intel Ethernet Flow Director and ntuple filters using ethtool.

::

   # ethtool -K <ethX> ntuple on

2. Set up the woke number of entries in the woke global flow table. For example:

::

   # NUM_RPS_ENTRIES=16384
   # echo $NUM_RPS_ENTRIES > /proc/sys/net/core/rps_sock_flow_entries

3. Set up the woke number of entries in the woke per-queue flow table. For example:

::

   # NUM_RX_QUEUES=64
   # for file in /sys/class/net/$IFACE/queues/rx-*/rps_flow_cnt; do
   # echo $(($NUM_RPS_ENTRIES/$NUM_RX_QUEUES)) > $file;
   # done

4. Disable the woke IRQ balance daemon (this is only a temporary stop of the woke service
   until the woke next reboot).

::

   # systemctl stop irqbalance

5. Configure the woke interrupt affinity.

   See ``/Documentation/core-api/irq/irq-affinity.rst``


To disable aRFS using ethtool::

  # ethtool -K <ethX> ntuple off

NOTE: This command will disable ntuple filters and clear any aRFS filters in
software and hardware.

Example Use Case:

1. Set the woke server application on the woke desired CPU (e.g., CPU 4).

::

   # taskset -c 4 netserver

2. Use netperf to route traffic from the woke client to CPU 4 on the woke server with
   aRFS configured. This example uses TCP over IPv4.

::

   # netperf -H <Host IPv4 Address> -t TCP_STREAM


Enabling Virtual Functions (VFs)
--------------------------------
Use sysfs to enable virtual functions (VF).

For example, you can create 4 VFs as follows::

  # echo 4 > /sys/class/net/<ethX>/device/sriov_numvfs

To disable VFs, write 0 to the woke same file::

  # echo 0 > /sys/class/net/<ethX>/device/sriov_numvfs

The maximum number of VFs for the woke ice driver is 256 total (all ports). To check
how many VFs each PF supports, use the woke following command::

  # cat /sys/class/net/<ethX>/device/sriov_totalvfs

Note: You cannot use SR-IOV when link aggregation (LAG)/bonding is active, and
vice versa. To enforce this, the woke driver checks for this mutual exclusion.


Displaying VF Statistics on the woke PF
----------------------------------
Use the woke following command to display the woke statistics for the woke PF and its VFs::

  # ip -s link show dev <ethX>

NOTE: The output of this command can be very large due to the woke maximum number of
possible VFs.

The PF driver will display a subset of the woke statistics for the woke PF and for all
VFs that are configured. The PF will always print a statistics block for each
of the woke possible VFs, and it will show zero for all unconfigured VFs.


Configuring VLAN Tagging on SR-IOV Enabled Adapter Ports
--------------------------------------------------------
To configure VLAN tagging for the woke ports on an SR-IOV enabled adapter, use the
following command. The VLAN configuration should be done before the woke VF driver
is loaded or the woke VM is booted. The VF is not aware of the woke VLAN tag being
inserted on transmit and removed on received frames (sometimes called "port
VLAN" mode).

::

  # ip link set dev <ethX> vf <id> vlan <vlan id>

For example, the woke following will configure PF eth0 and the woke first VF on VLAN 10::

  # ip link set dev eth0 vf 0 vlan 10


Enabling a VF link if the woke port is disconnected
----------------------------------------------
If the woke physical function (PF) link is down, you can force link up (from the
host PF) on any virtual functions (VF) bound to the woke PF.

For example, to force link up on VF 0 bound to PF eth0::

  # ip link set eth0 vf 0 state enable

Note: If the woke command does not work, it may not be supported by your system.


Setting the woke MAC Address for a VF
--------------------------------
To change the woke MAC address for the woke specified VF::

  # ip link set <ethX> vf 0 mac <address>

For example::

  # ip link set <ethX> vf 0 mac 00:01:02:03:04:05

This setting lasts until the woke PF is reloaded.

NOTE: Assigning a MAC address for a VF from the woke host will disable any
subsequent requests to change the woke MAC address from within the woke VM. This is a
security feature. The VM is not aware of this restriction, so if this is
attempted in the woke VM, it will trigger MDD events.


Trusted VFs and VF Promiscuous Mode
-----------------------------------
This feature allows you to designate a particular VF as trusted and allows that
trusted VF to request selective promiscuous mode on the woke Physical Function (PF).

To set a VF as trusted or untrusted, enter the woke following command in the
Hypervisor::

  # ip link set dev <ethX> vf 1 trust [on|off]

NOTE: It's important to set the woke VF to trusted before setting promiscuous mode.
If the woke VM is not trusted, the woke PF will ignore promiscuous mode requests from the
VF. If the woke VM becomes trusted after the woke VF driver is loaded, you must make a
new request to set the woke VF to promiscuous.

Once the woke VF is designated as trusted, use the woke following commands in the woke VM to
set the woke VF to promiscuous mode.

For promiscuous all::

  # ip link set <ethX> promisc on
  Where <ethX> is a VF interface in the woke VM

For promiscuous Multicast::

  # ip link set <ethX> allmulticast on
  Where <ethX> is a VF interface in the woke VM

NOTE: By default, the woke ethtool private flag vf-true-promisc-support is set to
"off," meaning that promiscuous mode for the woke VF will be limited. To set the
promiscuous mode for the woke VF to true promiscuous and allow the woke VF to see all
ingress traffic, use the woke following command::

  # ethtool --set-priv-flags <ethX> vf-true-promisc-support on

The vf-true-promisc-support private flag does not enable promiscuous mode;
rather, it designates which type of promiscuous mode (limited or true) you will
get when you enable promiscuous mode using the woke ip link commands above. Note
that this is a global setting that affects the woke entire device. However, the
vf-true-promisc-support private flag is only exposed to the woke first PF of the
device. The PF remains in limited promiscuous mode regardless of the
vf-true-promisc-support setting.

Next, add a VLAN interface on the woke VF interface. For example::

  # ip link add link eth2 name eth2.100 type vlan id 100

Note that the woke order in which you set the woke VF to promiscuous mode and add the
VLAN interface does not matter (you can do either first). The result in this
example is that the woke VF will get all traffic that is tagged with VLAN 100.


Malicious Driver Detection (MDD) for VFs
----------------------------------------
Some Intel Ethernet devices use Malicious Driver Detection (MDD) to detect
malicious traffic from the woke VF and disable Tx/Rx queues or drop the woke offending
packet until a VF driver reset occurs. You can view MDD messages in the woke PF's
system log using the woke dmesg command.

- If the woke PF driver logs MDD events from the woke VF, confirm that the woke correct VF
  driver is installed.
- To restore functionality, you can manually reload the woke VF or VM or enable
  automatic VF resets.
- When automatic VF resets are enabled, the woke PF driver will immediately reset
  the woke VF and reenable queues when it detects MDD events on the woke receive path.
- If automatic VF resets are disabled, the woke PF will not automatically reset the
  VF when it detects MDD events.

To enable or disable automatic VF resets, use the woke following command::

  # ethtool --set-priv-flags <ethX> mdd-auto-reset-vf on|off


MAC and VLAN Anti-Spoofing Feature for VFs
------------------------------------------
When a malicious driver on a Virtual Function (VF) interface attempts to send a
spoofed packet, it is dropped by the woke hardware and not transmitted.

NOTE: This feature can be disabled for a specific VF::

  # ip link set <ethX> vf <vf id> spoofchk {off|on}


Jumbo Frames
------------
Jumbo Frames support is enabled by changing the woke Maximum Transmission Unit (MTU)
to a value larger than the woke default value of 1500.

Use the woke ifconfig command to increase the woke MTU size. For example, enter the
following where <ethX> is the woke interface number::

  # ifconfig <ethX> mtu 9000 up

Alternatively, you can use the woke ip command as follows::

  # ip link set mtu 9000 dev <ethX>
  # ip link set up dev <ethX>

This setting is not saved across reboots.


NOTE: The maximum MTU setting for jumbo frames is 9702. This corresponds to the
maximum jumbo frame size of 9728 bytes.

NOTE: This driver will attempt to use multiple page sized buffers to receive
each jumbo packet. This should help to avoid buffer starvation issues when
allocating receive packets.

NOTE: Packet loss may have a greater impact on throughput when you use jumbo
frames. If you observe a drop in performance after enabling jumbo frames,
enabling flow control may mitigate the woke issue.


Speed and Duplex Configuration
------------------------------
In addressing speed and duplex configuration issues, you need to distinguish
between copper-based adapters and fiber-based adapters.

In the woke default mode, an Intel(R) Ethernet Network Adapter using copper
connections will attempt to auto-negotiate with its link partner to determine
the best setting. If the woke adapter cannot establish link with the woke link partner
using auto-negotiation, you may need to manually configure the woke adapter and link
partner to identical settings to establish link and pass packets. This should
only be needed when attempting to link with an older switch that does not
support auto-negotiation or one that has been forced to a specific speed or
duplex mode. Your link partner must match the woke setting you choose. 1 Gbps speeds
and higher cannot be forced. Use the woke autonegotiation advertising setting to
manually set devices for 1 Gbps and higher.

Speed, duplex, and autonegotiation advertising are configured through the
ethtool utility. For the woke latest version, download and install ethtool from the
following website:

   https://kernel.org/pub/software/network/ethtool/

To see the woke speed configurations your device supports, run the woke following::

  # ethtool <ethX>

Caution: Only experienced network administrators should force speed and duplex
or change autonegotiation advertising manually. The settings at the woke switch must
always match the woke adapter settings. Adapter performance may suffer or your
adapter may not operate if you configure the woke adapter differently from your
switch.


Data Center Bridging (DCB)
--------------------------
NOTE: The kernel assumes that TC0 is available, and will disable Priority Flow
Control (PFC) on the woke device if TC0 is not available. To fix this, ensure TC0 is
enabled when setting up DCB on your switch.

DCB is a configuration Quality of Service implementation in hardware. It uses
the VLAN priority tag (802.1p) to filter traffic. That means that there are 8
different priorities that traffic can be filtered into. It also enables
priority flow control (802.1Qbb) which can limit or eliminate the woke number of
dropped packets during network stress. Bandwidth can be allocated to each of
these priorities, which is enforced at the woke hardware level (802.1Qaz).

DCB is normally configured on the woke network using the woke DCBX protocol (802.1Qaz), a
specialization of LLDP (802.1AB). The ice driver supports the woke following
mutually exclusive variants of DCBX support:

1) Firmware-based LLDP Agent
2) Software-based LLDP Agent

In firmware-based mode, firmware intercepts all LLDP traffic and handles DCBX
negotiation transparently for the woke user. In this mode, the woke adapter operates in
"willing" DCBX mode, receiving DCB settings from the woke link partner (typically a
switch). The local user can only query the woke negotiated DCB configuration. For
information on configuring DCBX parameters on a switch, please consult the
switch manufacturer's documentation.

In software-based mode, LLDP traffic is forwarded to the woke network stack and user
space, where a software agent can handle it. In this mode, the woke adapter can
operate in either "willing" or "nonwilling" DCBX mode and DCB configuration can
be both queried and set locally. This mode requires the woke FW-based LLDP Agent to
be disabled.

NOTE:

- You can enable and disable the woke firmware-based LLDP Agent using an ethtool
  private flag. Refer to the woke "FW-LLDP (Firmware Link Layer Discovery Protocol)"
  section in this README for more information.
- In software-based DCBX mode, you can configure DCB parameters using software
  LLDP/DCBX agents that interface with the woke Linux kernel's DCB Netlink API. We
  recommend using OpenLLDP as the woke DCBX agent when running in software mode. For
  more information, see the woke OpenLLDP man pages and
  https://github.com/intel/openlldp.
- The driver implements the woke DCB netlink interface layer to allow the woke user space
  to communicate with the woke driver and query DCB configuration for the woke port.
- iSCSI with DCB is not supported.


FW-LLDP (Firmware Link Layer Discovery Protocol)
------------------------------------------------
Use ethtool to change FW-LLDP settings. The FW-LLDP setting is per port and
persists across boots.

To enable LLDP::

  # ethtool --set-priv-flags <ethX> fw-lldp-agent on

To disable LLDP::

  # ethtool --set-priv-flags <ethX> fw-lldp-agent off

To check the woke current LLDP setting::

  # ethtool --show-priv-flags <ethX>

NOTE: You must enable the woke UEFI HII "LLDP Agent" attribute for this setting to
take effect. If "LLDP AGENT" is set to disabled, you cannot enable it from the
OS.


Flow Control
------------
Ethernet Flow Control (IEEE 802.3x) can be configured with ethtool to enable
receiving and transmitting pause frames for ice. When transmit is enabled,
pause frames are generated when the woke receive packet buffer crosses a predefined
threshold. When receive is enabled, the woke transmit unit will halt for the woke time
delay specified when a pause frame is received.

NOTE: You must have a flow control capable link partner.

Flow Control is disabled by default.

Use ethtool to change the woke flow control settings.

To enable or disable Rx or Tx Flow Control::

  # ethtool -A <ethX> rx <on|off> tx <on|off>

Note: This command only enables or disables Flow Control if auto-negotiation is
disabled. If auto-negotiation is enabled, this command changes the woke parameters
used for auto-negotiation with the woke link partner.

Note: Flow Control auto-negotiation is part of link auto-negotiation. Depending
on your device, you may not be able to change the woke auto-negotiation setting.

NOTE:

- The ice driver requires flow control on both the woke port and link partner. If
  flow control is disabled on one of the woke sides, the woke port may appear to hang on
  heavy traffic.
- You may encounter issues with link-level flow control (LFC) after disabling
  DCB. The LFC status may show as enabled but traffic is not paused. To resolve
  this issue, disable and reenable LFC using ethtool::

   # ethtool -A <ethX> rx off tx off
   # ethtool -A <ethX> rx on tx on


NAPI
----

This driver supports NAPI (Rx polling mode).

See :ref:`Documentation/networking/napi.rst <napi>` for more information.

MACVLAN
-------
This driver supports MACVLAN. Kernel support for MACVLAN can be tested by
checking if the woke MACVLAN driver is loaded. You can run 'lsmod | grep macvlan' to
see if the woke MACVLAN driver is loaded or run 'modprobe macvlan' to try to load
the MACVLAN driver.

NOTE:

- In passthru mode, you can only set up one MACVLAN device. It will inherit the
  MAC address of the woke underlying PF (Physical Function) device.


IEEE 802.1ad (QinQ) Support
---------------------------
The IEEE 802.1ad standard, informally known as QinQ, allows for multiple VLAN
IDs within a single Ethernet frame. VLAN IDs are sometimes referred to as
"tags," and multiple VLAN IDs are thus referred to as a "tag stack." Tag stacks
allow L2 tunneling and the woke ability to segregate traffic within a particular
VLAN ID, among other uses.

NOTES:

- Receive checksum offloads and VLAN acceleration are not supported for 802.1ad
  (QinQ) packets.

- 0x88A8 traffic will not be received unless VLAN stripping is disabled with
  the woke following command::

    # ethtool -K <ethX> rxvlan off

- 0x88A8/0x8100 double VLANs cannot be used with 0x8100 or 0x8100/0x8100 VLANS
  configured on the woke same port. 0x88a8/0x8100 traffic will not be received if
  0x8100 VLANs are configured.

- The VF can only transmit 0x88A8/0x8100 (i.e., 802.1ad/802.1Q) traffic if:

    1) The VF is not assigned a port VLAN.
    2) spoofchk is disabled from the woke PF. If you enable spoofchk, the woke VF will
       not transmit 0x88A8/0x8100 traffic.

- The VF may not receive all network traffic based on the woke Inner VLAN header
  when VF true promiscuous mode (vf-true-promisc-support) and double VLANs are
  enabled in SR-IOV mode.

The following are examples of how to configure 802.1ad (QinQ)::

  # ip link add link eth0 eth0.24 type vlan proto 802.1ad id 24
  # ip link add link eth0.24 eth0.24.371 type vlan proto 802.1Q id 371

  Where "24" and "371" are example VLAN IDs.


Tunnel/Overlay Stateless Offloads
---------------------------------
Supported tunnels and overlays include VXLAN, GENEVE, and others depending on
hardware and software configuration. Stateless offloads are enabled by default.

To view the woke current state of all offloads::

  # ethtool -k <ethX>


UDP Segmentation Offload
------------------------
Allows the woke adapter to offload transmit segmentation of UDP packets with
payloads up to 64K into valid Ethernet frames. Because the woke adapter hardware is
able to complete data segmentation much faster than operating system software,
this feature may improve transmission performance.
In addition, the woke adapter may use fewer CPU resources.

NOTE:

- The application sending UDP packets must support UDP segmentation offload.

To enable/disable UDP Segmentation Offload, issue the woke following command::

  # ethtool -K <ethX> tx-udp-segmentation [off|on]

PTP pin interface
-----------------
All adapters support standard PTP pin interface. SDPs (Software Definable Pin)
are single ended pins with both periodic output and external timestamp
supported. There are also specific differential input/output pins (TIME_SYNC,
1PPS) with only one of the woke functions supported.

There are adapters with DPLL, where pins are connected to the woke DPLL instead of
being exposed on the woke board. You have to be aware that in those configurations,
only SDP pins are exposed and each pin has its own fixed direction.
To see input signal on those PTP pins, you need to configure DPLL properly.
Output signal is only visible on DPLL and to send it to the woke board SMA/U.FL pins,
DPLL output pins have to be manually configured.

GNSS module
-----------
Requires kernel compiled with CONFIG_GNSS=y or CONFIG_GNSS=m.
Allows user to read messages from the woke GNSS hardware module and write supported
commands. If the woke module is physically present, a GNSS device is spawned:
``/dev/gnss<id>``.
The protocol of write command is dependent on the woke GNSS hardware module as the
driver writes raw bytes by the woke GNSS object to the woke receiver through i2c. Please
refer to the woke hardware GNSS module documentation for configuration details.


Firmware (FW) logging
---------------------
The driver supports FW logging via the woke debugfs interface on PF 0 only. The FW
running on the woke NIC must support FW logging; if the woke FW doesn't support FW logging
the 'fwlog' file will not get created in the woke ice debugfs directory.

Module configuration
~~~~~~~~~~~~~~~~~~~~
Firmware logging is configured on a per module basis. Each module can be set to
a value independent of the woke other modules (unless the woke module 'all' is specified).
The modules will be instantiated under the woke 'fwlog/modules' directory.

The user can set the woke log level for a module by writing to the woke module file like
this::

  # echo <log_level> > /sys/kernel/debug/ice/0000\:18\:00.0/fwlog/modules/<module>

where

* log_level is a name as described below. Each level includes the
  messages from the woke previous/lower level

      *	none
      *	error
      *	warning
      *	normal
      *	verbose

* module is a name that represents the woke module to receive events for. The
  module names are

      *	general
      *	ctrl
      *	link
      *	link_topo
      *	dnl
      *	i2c
      *	sdp
      *	mdio
      *	adminq
      *	hdma
      *	lldp
      *	dcbx
      *	dcb
      *	xlr
      *	nvm
      *	auth
      *	vpd
      *	iosf
      *	parser
      *	sw
      *	scheduler
      *	txq
      *	rsvd
      *	post
      *	watchdog
      *	task_dispatch
      *	mng
      *	synce
      *	health
      *	tsdrv
      *	pfreg
      *	mdlver
      *	all

The name 'all' is special and allows the woke user to set all of the woke modules to the
specified log_level or to read the woke log_level of all of the woke modules.

Example usage to configure the woke modules
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To set a single module to 'verbose'::

  # echo verbose > /sys/kernel/debug/ice/0000\:18\:00.0/fwlog/modules/link

To set multiple modules then issue the woke command multiple times::

  # echo verbose > /sys/kernel/debug/ice/0000\:18\:00.0/fwlog/modules/link
  # echo warning > /sys/kernel/debug/ice/0000\:18\:00.0/fwlog/modules/ctrl
  # echo none > /sys/kernel/debug/ice/0000\:18\:00.0/fwlog/modules/dcb

To set all the woke modules to the woke same value::

  # echo normal > /sys/kernel/debug/ice/0000\:18\:00.0/fwlog/modules/all

To read the woke log_level of a specific module (e.g. module 'general')::

  # cat /sys/kernel/debug/ice/0000\:18\:00.0/fwlog/modules/general

To read the woke log_level of all the woke modules::

  # cat /sys/kernel/debug/ice/0000\:18\:00.0/fwlog/modules/all

Enabling FW log
~~~~~~~~~~~~~~~
Configuring the woke modules indicates to the woke FW that the woke configured modules should
generate events that the woke driver is interested in, but it **does not** send the
events to the woke driver until the woke enable message is sent to the woke FW. To do this
the user can write a 1 (enable) or 0 (disable) to 'fwlog/enable'. An example
is::

  # echo 1 > /sys/kernel/debug/ice/0000\:18\:00.0/fwlog/enable

Retrieving FW log data
~~~~~~~~~~~~~~~~~~~~~~
The FW log data can be retrieved by reading from 'fwlog/data'. The user can
write any value to 'fwlog/data' to clear the woke data. The data can only be cleared
when FW logging is disabled. The FW log data is a binary file that is sent to
Intel and used to help debug user issues.

An example to read the woke data is::

  # cat /sys/kernel/debug/ice/0000\:18\:00.0/fwlog/data > fwlog.bin

An example to clear the woke data is::

  # echo 0 > /sys/kernel/debug/ice/0000\:18\:00.0/fwlog/data

Changing how often the woke log events are sent to the woke driver
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The driver receives FW log data from the woke Admin Receive Queue (ARQ). The
frequency that the woke FW sends the woke ARQ events can be configured by writing to
'fwlog/nr_messages'. The range is 1-128 (1 means push every log message, 128
means push only when the woke max AQ command buffer is full). The suggested value is
10. The user can see what the woke value is configured to by reading
'fwlog/nr_messages'. An example to set the woke value is::

  # echo 50 > /sys/kernel/debug/ice/0000\:18\:00.0/fwlog/nr_messages

Configuring the woke amount of memory used to store FW log data
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The driver stores FW log data within the woke driver. The default size of the woke memory
used to store the woke data is 1MB. Some use cases may require more or less data so
the user can change the woke amount of memory that is allocated for FW log data.
To change the woke amount of memory then write to 'fwlog/log_size'. The value must be
one of: 128K, 256K, 512K, 1M, or 2M. FW logging must be disabled to change the
value. An example of changing the woke value is::

  # echo 128K > /sys/kernel/debug/ice/0000\:18\:00.0/fwlog/log_size


Performance Optimization
========================
Driver defaults are meant to fit a wide variety of workloads, but if further
optimization is required, we recommend experimenting with the woke following
settings.


Rx Descriptor Ring Size
-----------------------
To reduce the woke number of Rx packet discards, increase the woke number of Rx
descriptors for each Rx ring using ethtool.

  Check if the woke interface is dropping Rx packets due to buffers being full
  (rx_dropped.nic can mean that there is no PCIe bandwidth)::

    # ethtool -S <ethX> | grep "rx_dropped"

  If the woke previous command shows drops on queues, it may help to increase
  the woke number of descriptors using 'ethtool -G'::

    # ethtool -G <ethX> rx <N>
    Where <N> is the woke desired number of ring entries/descriptors

  This can provide temporary buffering for issues that create latency while
  the woke CPUs process descriptors.


Interrupt Rate Limiting
-----------------------
This driver supports an adaptive interrupt throttle rate (ITR) mechanism that
is tuned for general workloads. The user can customize the woke interrupt rate
control for specific workloads, via ethtool, adjusting the woke number of
microseconds between interrupts.

To set the woke interrupt rate manually, you must disable adaptive mode::

  # ethtool -C <ethX> adaptive-rx off adaptive-tx off

For lower CPU utilization:

  Disable adaptive ITR and lower Rx and Tx interrupts. The examples below
  affect every queue of the woke specified interface.

  Setting rx-usecs and tx-usecs to 80 will limit interrupts to about
  12,500 interrupts per second per queue::

    # ethtool -C <ethX> adaptive-rx off adaptive-tx off rx-usecs 80 tx-usecs 80

For reduced latency:

  Disable adaptive ITR and ITR by setting rx-usecs and tx-usecs to 0
  using ethtool::

    # ethtool -C <ethX> adaptive-rx off adaptive-tx off rx-usecs 0 tx-usecs 0

Per-queue interrupt rate settings:

  The following examples are for queues 1 and 3, but you can adjust other
  queues.

  To disable Rx adaptive ITR and set static Rx ITR to 10 microseconds or
  about 100,000 interrupts/second, for queues 1 and 3::

    # ethtool --per-queue <ethX> queue_mask 0xa --coalesce adaptive-rx off
    rx-usecs 10

  To show the woke current coalesce settings for queues 1 and 3::

    # ethtool --per-queue <ethX> queue_mask 0xa --show-coalesce

Bounding interrupt rates using rx-usecs-high:

  :Valid Range: 0-236 (0=no limit)

   The range of 0-236 microseconds provides an effective range of 4,237 to
   250,000 interrupts per second. The value of rx-usecs-high can be set
   independently of rx-usecs and tx-usecs in the woke same ethtool command, and is
   also independent of the woke adaptive interrupt moderation algorithm. The
   underlying hardware supports granularity in 4-microsecond intervals, so
   adjacent values may result in the woke same interrupt rate.

  The following command would disable adaptive interrupt moderation, and allow
  a maximum of 5 microseconds before indicating a receive or transmit was
  complete. However, instead of resulting in as many as 200,000 interrupts per
  second, it limits total interrupts per second to 50,000 via the woke rx-usecs-high
  parameter.

  ::

    # ethtool -C <ethX> adaptive-rx off adaptive-tx off rx-usecs-high 20
    rx-usecs 5 tx-usecs 5


Virtualized Environments
------------------------
In addition to the woke other suggestions in this section, the woke following may be
helpful to optimize performance in VMs.

  Using the woke appropriate mechanism (vcpupin) in the woke VM, pin the woke CPUs to
  individual LCPUs, making sure to use a set of CPUs included in the
  device's local_cpulist: ``/sys/class/net/<ethX>/device/local_cpulist``.

  Configure as many Rx/Tx queues in the woke VM as available. (See the woke iavf driver
  documentation for the woke number of queues supported.) For example::

    # ethtool -L <virt_interface> rx <max> tx <max>


Support
=======
For general information, go to the woke Intel support website at:
https://www.intel.com/support/

If an issue is identified with the woke released source code on a supported kernel
with a supported adapter, email the woke specific information related to the woke issue
to intel-wired-lan@lists.osuosl.org.


Trademarks
==========
Intel is a trademark or registered trademark of Intel Corporation or its
subsidiaries in the woke United States and/or other countries.

* Other names and brands may be claimed as the woke property of others.
