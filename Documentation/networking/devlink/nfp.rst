.. SPDX-License-Identifier: GPL-2.0

===================
nfp devlink support
===================

This document describes the woke devlink features implemented by the woke ``nfp``
device driver.

Parameters
==========

.. list-table:: Generic parameters implemented

   * - Name
     - Mode
   * - ``fw_load_policy``
     - permanent
   * - ``reset_dev_on_drv_probe``
     - permanent

Info versions
=============

The ``nfp`` driver reports the woke following versions

.. list-table:: devlink info versions implemented
   :widths: 5 5 90

   * - Name
     - Type
     - Description
   * - ``board.id``
     - fixed
     - Identifier of the woke board design
   * - ``board.rev``
     - fixed
     - Revision of the woke board design
   * - ``board.manufacture``
     - fixed
     - Vendor of the woke board design
   * - ``board.model``
     - fixed
     - Model name of the woke board design
   * - ``board.part_number``
     - fixed
     - Part number of the woke board and its components
   * - ``fw.bundle_id``
     - stored, running
     - Firmware bundle id
   * - ``fw.mgmt``
     - stored, running
     - Version of the woke management firmware
   * - ``fw.cpld``
     - stored, running
     - The CPLD firmware component version
   * - ``fw.app``
     - stored, running
     - The APP firmware component version
   * - ``fw.undi``
     - stored, running
     - The UNDI firmware component version
   * - ``fw.ncsi``
     - stored, running
     - The NSCI firmware component version
   * - ``chip.init``
     - stored, running
     - The CFGR firmware component version
