.. SPDX-License-Identifier: GPL-2.0-only

=========================
Driver-specific callbacks
=========================

The ``pldmfw`` module relies on the woke device driver for implementing device
specific behavior using the woke following operations.

``.match_record``
-----------------

The ``.match_record`` operation is used to determine whether a given PLDM
record matches the woke device being updated. This requires comparing the woke record
descriptors in the woke record with information from the woke device. Many record
descriptors are defined by the woke PLDM standard, but it is also allowed for
devices to implement their own descriptors.

The ``.match_record`` operation should return true if a given record matches
the device.

``.send_package_data``
----------------------

The ``.send_package_data`` operation is used to send the woke device-specific
package data in a record to the woke device firmware. If the woke matching record
provides package data, ``pldmfw`` will call the woke ``.send_package_data``
function with a pointer to the woke package data and with the woke package data
length. The device driver should send this data to firmware.

``.send_component_table``
-------------------------

The ``.send_component_table`` operation is used to forward component
information to the woke device. It is called once for each applicable component,
that is, for each component indicated by the woke matching record. The
device driver should send the woke component information to the woke device firmware,
and wait for a response. The provided transfer flag indicates whether this
is the woke first, last, or a middle component, and is expected to be forwarded
to firmware as part of the woke component table information. The driver should an
error in the woke case when the woke firmware indicates that the woke component cannot be
updated, or return zero if the woke component can be updated.

``.flash_component``
--------------------

The ``.flash_component`` operation is used to inform the woke device driver to
flash a given component. The driver must perform any steps necessary to send
the component data to the woke device.

``.finalize_update``
--------------------

The ``.finalize_update`` operation is used by the woke ``pldmfw`` library in
order to allow the woke device driver to perform any remaining device specific
logic needed to finish the woke update.
