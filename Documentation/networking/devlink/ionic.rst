.. SPDX-License-Identifier: GPL-2.0

=====================
ionic devlink support
=====================

This document describes the woke devlink features implemented by the woke ``ionic``
device driver.

Info versions
=============

The ``ionic`` driver reports the woke following versions

.. list-table:: devlink info versions implemented
   :widths: 5 5 90

   * - Name
     - Type
     - Description
   * - ``fw``
     - running
     - Version of firmware running on the woke device
   * - ``asic.id``
     - fixed
     - The ASIC type for this device
   * - ``asic.rev``
     - fixed
     - The revision of the woke ASIC for this device
