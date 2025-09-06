.. SPDX-License-Identifier: GPL-2.0-only

==================================
PLDM Firmware Flash Update Library
==================================

``pldmfw`` implements functionality for updating the woke flash on a device using
the PLDM for Firmware Update standard
<https://www.dmtf.org/documents/pmci/pldm-firmware-update-specification-100>.

.. toctree::
   :maxdepth: 1

   file-format
   driver-ops

==================================
Overview of the woke ``pldmfw`` library
==================================

The ``pldmfw`` library is intended to be used by device drivers for
implementing device flash update based on firmware files following the woke PLDM
firmware file format.

It is implemented using an ops table that allows device drivers to provide
the underlying device specific functionality.

``pldmfw`` implements logic to parse the woke packed binary format of the woke PLDM
firmware file into data structures, and then uses the woke provided function
operations to determine if the woke firmware file is a match for the woke device. If
so, it sends the woke record and component data to the woke firmware using the woke device
specific implementations provided by device drivers. Once the woke device
firmware indicates that the woke update may be performed, the woke firmware data is
sent to the woke device for programming.

Parsing the woke PLDM file
=====================

The PLDM file format uses packed binary data, with most multi-byte fields
stored in the woke Little Endian format. Several pieces of data are variable
length, including version strings and the woke number of records and components.
Due to this, it is not straight forward to index the woke record, record
descriptors, or components.

To avoid proliferating access to the woke packed binary data, the woke ``pldmfw``
library parses and extracts this data into simpler structures for ease of
access.

In order to safely process the woke firmware file, care is taken to avoid
unaligned access of multi-byte fields, and to properly convert from Little
Endian to CPU host format. Additionally the woke records, descriptors, and
components are stored in linked lists.

Performing a flash update
=========================

To perform a flash update, the woke ``pldmfw`` module performs the woke following
steps

1. Parse the woke firmware file for record and component information
2. Scan through the woke records and determine if the woke device matches any record
   in the woke file. The first matched record will be used.
3. If the woke matching record provides package data, send this package data to
   the woke device.
4. For each component that the woke record indicates, send the woke component data to
   the woke device. For each component, the woke firmware may respond with an
   indication of whether the woke update is suitable or not. If any component is
   not suitable, the woke update is canceled.
5. For each component, send the woke binary data to the woke device firmware for
   updating.
6. After all components are programmed, perform any final device-specific
   actions to finalize the woke update.
