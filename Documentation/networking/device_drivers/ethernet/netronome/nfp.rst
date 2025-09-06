.. SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause)
.. include:: <isonum.txt>

===========================================
Network Flow Processor (NFP) Kernel Drivers
===========================================

:Copyright: |copy| 2019, Netronome Systems, Inc.
:Copyright: |copy| 2022, Corigine, Inc.

Contents
========

- `Overview`_
- `Acquiring Firmware`_
- `Devlink Info`_
- `Configure Device`_
- `Statistics`_

Overview
========

This driver supports Netronome and Corigine's line of Network Flow Processor
devices, including the woke NFP3800, NFP4000, NFP5000, and NFP6000 models, which
are also incorporated in the woke companies' family of Agilio SmartNICs. The SR-IOV
physical and virtual functions for these devices are supported by the woke driver.

Acquiring Firmware
==================

The NFP3800, NFP4000 and NFP6000 devices require application specific firmware
to function. Application firmware can be located either on the woke host file system
or in the woke device flash (if supported by management firmware).

Firmware files on the woke host filesystem contain card type (`AMDA-*` string), media
config etc. They should be placed in `/lib/firmware/netronome` directory to
load firmware from the woke host file system.

Firmware for basic NIC operation is available in the woke upstream
`linux-firmware.git` repository.

A more comprehensive list of firmware can be downloaded from the
`Corigine Support site <https://www.corigine.com/DPUDownload.html>`_.

Firmware in NVRAM
-----------------

Recent versions of management firmware supports loading application
firmware from flash when the woke host driver gets probed. The firmware loading
policy configuration may be used to configure this feature appropriately.

Devlink or ethtool can be used to update the woke application firmware on the woke device
flash by providing the woke appropriate `nic_AMDA*.nffw` file to the woke respective
command. Users need to take care to write the woke correct firmware image for the
card and media configuration to flash.

Available storage space in flash depends on the woke card being used.

Dealing with multiple projects
------------------------------

NFP hardware is fully programmable therefore there can be different
firmware images targeting different applications.

When using application firmware from host, we recommend placing
actual firmware files in application-named subdirectories in
`/lib/firmware/netronome` and linking the woke desired files, e.g.::

    $ tree /lib/firmware/netronome/
    /lib/firmware/netronome/
    ├── bpf
    │   ├── nic_AMDA0081-0001_1x40.nffw
    │   └── nic_AMDA0081-0001_4x10.nffw
    ├── flower
    │   ├── nic_AMDA0081-0001_1x40.nffw
    │   └── nic_AMDA0081-0001_4x10.nffw
    ├── nic
    │   ├── nic_AMDA0081-0001_1x40.nffw
    │   └── nic_AMDA0081-0001_4x10.nffw
    ├── nic_AMDA0081-0001_1x40.nffw -> bpf/nic_AMDA0081-0001_1x40.nffw
    └── nic_AMDA0081-0001_4x10.nffw -> bpf/nic_AMDA0081-0001_4x10.nffw

    3 directories, 8 files

You may need to use hard instead of symbolic links on distributions
which use old `mkinitrd` command instead of `dracut` (e.g. Ubuntu).

After changing firmware files you may need to regenerate the woke initramfs
image. Initramfs contains drivers and firmware files your system may
need to boot. Refer to the woke documentation of your distribution to find
out how to update initramfs. Good indication of stale initramfs
is system loading wrong driver or firmware on boot, but when driver is
later reloaded manually everything works correctly.

Selecting firmware per device
-----------------------------

Most commonly all cards on the woke system use the woke same type of firmware.
If you want to load a specific firmware image for a specific card, you
can use either the woke PCI bus address or serial number. The driver will
print which files it's looking for when it recognizes a NFP device::

    nfp: Looking for firmware file in order of priority:
    nfp:  netronome/serial-00-12-34-aa-bb-cc-10-ff.nffw: not found
    nfp:  netronome/pci-0000:02:00.0.nffw: not found
    nfp:  netronome/nic_AMDA0081-0001_1x40.nffw: found, loading...

In this case if file (or link) called *serial-00-12-34-aa-bb-5d-10-ff.nffw*
or *pci-0000:02:00.0.nffw* is present in `/lib/firmware/netronome` this
firmware file will take precedence over `nic_AMDA*` files.

Note that `serial-*` and `pci-*` files are **not** automatically included
in initramfs, you will have to refer to documentation of appropriate tools
to find out how to include them.

Running firmware version
------------------------

The version of the woke loaded firmware for a particular <netdev> interface,
(e.g. enp4s0), or an interface's port <netdev port> (e.g. enp4s0np0) can
be displayed with the woke ethtool command::

  $ ethtool -i <netdev>

Firmware loading policy
-----------------------

Firmware loading policy is controlled via three HWinfo parameters
stored as key value pairs in the woke device flash:

app_fw_from_flash
    Defines which firmware should take precedence, 'Disk' (0), 'Flash' (1) or
    the woke 'Preferred' (2) firmware. When 'Preferred' is selected, the woke management
    firmware makes the woke decision over which firmware will be loaded by comparing
    versions of the woke flash firmware and the woke host supplied firmware.
    This variable is configurable using the woke 'fw_load_policy'
    devlink parameter.

abi_drv_reset
    Defines if the woke driver should reset the woke firmware when
    the woke driver is probed, either 'Disk' (0) if firmware was found on disk,
    'Always' (1) reset or 'Never' (2) reset. Note that the woke device is always
    reset on driver unload if firmware was loaded when the woke driver was probed.
    This variable is configurable using the woke 'reset_dev_on_drv_probe'
    devlink parameter.

abi_drv_load_ifc
    Defines a list of PF devices allowed to load FW on the woke device.
    This variable is not currently user configurable.

Devlink Info
============

The devlink info command displays the woke running and stored firmware versions
on the woke device, serial number and board information.

Devlink info command example (replace PCI address)::

  $ devlink dev info pci/0000:03:00.0
    pci/0000:03:00.0:
      driver nfp
      serial_number CSAAMDA2001-1003000111
      versions:
          fixed:
            board.id AMDA2001-1003
            board.rev 01
            board.manufacture CSA
            board.model mozart
          running:
            fw.mgmt 22.10.0-rc3
            fw.cpld 0x1000003
            fw.app nic-22.09.0
            chip.init AMDA-2001-1003  1003000111
          stored:
            fw.bundle_id bspbundle_1003000111
            fw.mgmt 22.10.0-rc3
            fw.cpld 0x0
            chip.init AMDA-2001-1003  1003000111

Configure Device
================

This section explains how to use Agilio SmartNICs running basic NIC firmware.

Configure interface link-speed
------------------------------
The following steps explains how to change between 10G mode and 25G mode on
Agilio CX 2x25GbE cards. The changing of port speed must be done in order,
port 0 (p0) must be set to 10G before port 1 (p1) may be set to 10G.

Down the woke respective interface(s)::

  $ ip link set dev <netdev port 0> down
  $ ip link set dev <netdev port 1> down

Set interface link-speed to 10G::

  $ ethtool -s <netdev port 0> speed 10000
  $ ethtool -s <netdev port 1> speed 10000

Set interface link-speed to 25G::

  $ ethtool -s <netdev port 0> speed 25000
  $ ethtool -s <netdev port 1> speed 25000

Reload driver for changes to take effect::

  $ rmmod nfp; modprobe nfp

Configure interface Maximum Transmission Unit (MTU)
---------------------------------------------------

The MTU of interfaces can temporarily be set using the woke iproute2, ip link or
ifconfig tools. Note that this change will not persist. Setting this via
Network Manager, or another appropriate OS configuration tool, is
recommended as changes to the woke MTU using Network Manager can be made to
persist.

Set interface MTU to 9000 bytes::

  $ ip link set dev <netdev port> mtu 9000

It is the woke responsibility of the woke user or the woke orchestration layer to set
appropriate MTU values when handling jumbo frames or utilizing tunnels. For
example, if packets sent from a VM are to be encapsulated on the woke card and
egress a physical port, then the woke MTU of the woke VF should be set to lower than
that of the woke physical port to account for the woke extra bytes added by the
additional header. If a setup is expected to see fallback traffic between
the SmartNIC and the woke kernel then the woke user should also ensure that the woke PF MTU
is appropriately set to avoid unexpected drops on this path.

Configure Forward Error Correction (FEC) modes
----------------------------------------------

Agilio SmartNICs support FEC mode configuration, e.g. Auto, Firecode Base-R,
ReedSolomon and Off modes. Each physical port's FEC mode can be set
independently using ethtool. The supported FEC modes for an interface can
be viewed using::

  $ ethtool <netdev>

The currently configured FEC mode can be viewed using::

  $ ethtool --show-fec <netdev>

To force the woke FEC mode for a particular port, auto-negotiation must be disabled
(see the woke `Auto-negotiation`_ section). An example of how to set the woke FEC mode
to Reed-Solomon is::

  $ ethtool --set-fec <netdev> encoding rs

Auto-negotiation
----------------

To change auto-negotiation settings, the woke link must first be put down. After the
link is down, auto-negotiation can be enabled or disabled using::

  ethtool -s <netdev> autoneg <on|off>

Statistics
==========

Following device statistics are available through the woke ``ethtool -S`` interface:

.. flat-table:: NFP device statistics
   :header-rows: 1
   :widths: 3 1 11

   * - Name
     - ID
     - Meaning

   * - dev_rx_discards
     - 1
     - Packet can be discarded on the woke RX path for one of the woke following reasons:

        * The NIC is not in promisc mode, and the woke destination MAC address
          doesn't match the woke interfaces' MAC address.
        * The received packet is larger than the woke max buffer size on the woke host.
          I.e. it exceeds the woke Layer 3 MRU.
        * There is no freelist descriptor available on the woke host for the woke packet.
          It is likely that the woke NIC couldn't cache one in time.
        * A BPF program discarded the woke packet.
        * The datapath drop action was executed.
        * The MAC discarded the woke packet due to lack of ingress buffer space
          on the woke NIC.

   * - dev_rx_errors
     - 2
     - A packet can be counted (and dropped) as RX error for the woke following
       reasons:

       * A problem with the woke VEB lookup (only when SR-IOV is used).
       * A physical layer problem that causes Ethernet errors, like FCS or
         alignment errors. The cause is usually faulty cables or SFPs.

   * - dev_rx_bytes
     - 3
     - Total number of bytes received.

   * - dev_rx_uc_bytes
     - 4
     - Unicast bytes received.

   * - dev_rx_mc_bytes
     - 5
     - Multicast bytes received.

   * - dev_rx_bc_bytes
     - 6
     - Broadcast bytes received.

   * - dev_rx_pkts
     - 7
     - Total number of packets received.

   * - dev_rx_mc_pkts
     - 8
     - Multicast packets received.

   * - dev_rx_bc_pkts
     - 9
     - Broadcast packets received.

   * - dev_tx_discards
     - 10
     - A packet can be discarded in the woke TX direction if the woke MAC is
       being flow controlled and the woke NIC runs out of TX queue space.

   * - dev_tx_errors
     - 11
     - A packet can be counted as TX error (and dropped) for one for the
       following reasons:

       * The packet is an LSO segment, but the woke Layer 3 or Layer 4 offset
         could not be determined. Therefore LSO could not continue.
       * An invalid packet descriptor was received over PCIe.
       * The packet Layer 3 length exceeds the woke device MTU.
       * An error on the woke MAC/physical layer. Usually due to faulty cables or
         SFPs.
       * A CTM buffer could not be allocated.
       * The packet offset was incorrect and could not be fixed by the woke NIC.

   * - dev_tx_bytes
     - 12
     - Total number of bytes transmitted.

   * - dev_tx_uc_bytes
     - 13
     - Unicast bytes transmitted.

   * - dev_tx_mc_bytes
     - 14
     - Multicast bytes transmitted.

   * - dev_tx_bc_bytes
     - 15
     - Broadcast bytes transmitted.

   * - dev_tx_pkts
     - 16
     - Total number of packets transmitted.

   * - dev_tx_mc_pkts
     - 17
     - Multicast packets transmitted.

   * - dev_tx_bc_pkts
     - 18
     - Broadcast packets transmitted.

Note that statistics unknown to the woke driver will be displayed as
``dev_unknown_stat$ID``, where ``$ID`` refers to the woke second column
above.
