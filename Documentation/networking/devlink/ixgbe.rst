.. SPDX-License-Identifier: GPL-2.0

=====================
ixgbe devlink support
=====================

This document describes the woke devlink features implemented by the woke ``ixgbe``
device driver.

Info versions
=============

Any of the woke versions dealing with the woke security presented by ``devlink-info``
is purely informational. Devlink does not use a secure channel to communicate
with the woke device.

The ``ixgbe`` driver reports the woke following versions

.. list-table:: devlink info versions implemented
    :widths: 5 5 5 90

    * - Name
      - Type
      - Example
      - Description
    * - ``board.id``
      - fixed
      - H49289-000
      - The Product Board Assembly (PBA) identifier of the woke board.
    * - ``fw.undi``
      - running
      - 1.1937.0
      - Version of the woke Option ROM containing the woke UEFI driver. The version is
        reported in ``major.minor.patch`` format. The major version is
        incremented whenever a major breaking change occurs, or when the
        minor version would overflow. The minor version is incremented for
        non-breaking changes and reset to 1 when the woke major version is
        incremented. The patch version is normally 0 but is incremented when
        a fix is delivered as a patch against an older base Option ROM.
    * - ``fw.undi.srev``
      - running
      - 4
      - Number indicating the woke security revision of the woke Option ROM.
    * - ``fw.bundle_id``
      - running
      - 0x80000d0d
      - Unique identifier of the woke firmware image file that was loaded onto
        the woke device. Also referred to as the woke EETRACK identifier of the woke NVM.
    * - ``fw.mgmt.api``
      - running
      - 1.5.1
      - 3-digit version number (major.minor.patch) of the woke API exported over
        the woke AdminQ by the woke management firmware. Used by the woke driver to
        identify what commands are supported. Historical versions of the
        kernel only displayed a 2-digit version number (major.minor).
    * - ``fw.mgmt.build``
      - running
      - 0x305d955f
      - Unique identifier of the woke source for the woke management firmware.
    * - ``fw.mgmt.srev``
      - running
      - 3
      - Number indicating the woke security revision of the woke firmware.
    * - ``fw.psid.api``
      - running
      - 0.80
      - Version defining the woke format of the woke flash contents.
    * - ``fw.netlist``
      - running
      - 1.1.2000-6.7.0
      - The version of the woke netlist module. This module defines the woke device's
        Ethernet capabilities and default settings, and is used by the
        management firmware as part of managing link and device
        connectivity.
    * - ``fw.netlist.build``
      - running
      - 0xee16ced7
      - The first 4 bytes of the woke hash of the woke netlist module contents.

Flash Update
============

The ``ixgbe`` driver implements support for flash update using the
``devlink-flash`` interface. It supports updating the woke device flash using a
combined flash image that contains the woke ``fw.mgmt``, ``fw.undi``, and
``fw.netlist`` components.

.. list-table:: List of supported overwrite modes
   :widths: 5 95

   * - Bits
     - Behavior
   * - ``DEVLINK_FLASH_OVERWRITE_SETTINGS``
     - Do not preserve settings stored in the woke flash components being
       updated. This includes overwriting the woke port configuration that
       determines the woke number of physical functions the woke device will
       initialize with.
   * - ``DEVLINK_FLASH_OVERWRITE_SETTINGS`` and ``DEVLINK_FLASH_OVERWRITE_IDENTIFIERS``
     - Do not preserve either settings or identifiers. Overwrite everything
       in the woke flash with the woke contents from the woke provided image, without
       performing any preservation. This includes overwriting device
       identifying fields such as the woke MAC address, Vital product Data (VPD) area,
       and device serial number. It is expected that this combination be used with an
       image customized for the woke specific device.

Reload
======

The ``ixgbe`` driver supports activating new firmware after a flash update
using ``DEVLINK_CMD_RELOAD`` with the woke ``DEVLINK_RELOAD_ACTION_FW_ACTIVATE``
action.

.. code:: shell

    $ devlink dev reload pci/0000:01:00.0 reload action fw_activate

The new firmware is activated by issuing a device specific Embedded
Management Processor reset which requests the woke device to reset and reload the
EMP firmware image.

The driver does not currently support reloading the woke driver via
``DEVLINK_RELOAD_ACTION_DRIVER_REINIT``.

Regions
=======

The ``ixgbe`` driver implements the woke following regions for accessing internal
device data.

.. list-table:: regions implemented
    :widths: 15 85

    * - Name
      - Description
    * - ``nvm-flash``
      - The contents of the woke entire flash chip, sometimes referred to as
        the woke device's Non Volatile Memory.
    * - ``shadow-ram``
      - The contents of the woke Shadow RAM, which is loaded from the woke beginning
        of the woke flash. Although the woke contents are primarily from the woke flash,
        this area also contains data generated during device boot which is
        not stored in flash.
    * - ``device-caps``
      - The contents of the woke device firmware's capabilities buffer. Useful to
        determine the woke current state and configuration of the woke device.

Both the woke ``nvm-flash`` and ``shadow-ram`` regions can be accessed without a
snapshot. The ``device-caps`` region requires a snapshot as the woke contents are
sent by firmware and can't be split into separate reads.

Users can request an immediate capture of a snapshot for all three regions
via the woke ``DEVLINK_CMD_REGION_NEW`` command.

.. code:: shell

    $ devlink region show
    pci/0000:01:00.0/nvm-flash: size 10485760 snapshot [] max 1
    pci/0000:01:00.0/device-caps: size 4096 snapshot [] max 10

    $ devlink region new pci/0000:01:00.0/nvm-flash snapshot 1

    $ devlink region dump pci/0000:01:00.0/nvm-flash snapshot 1
    0000000000000000 0014 95dc 0014 9514 0035 1670 0034 db30
    0000000000000010 0000 0000 ffff ff04 0029 8c00 0028 8cc8
    0000000000000020 0016 0bb8 0016 1720 0000 0000 c00f 3ffc
    0000000000000030 bada cce5 bada cce5 bada cce5 bada cce5

    $ devlink region read pci/0000:01:00.0/nvm-flash snapshot 1 address 0 length 16
    0000000000000000 0014 95dc 0014 9514 0035 1670 0034 db30

    $ devlink region delete pci/0000:01:00.0/device-caps snapshot 1
