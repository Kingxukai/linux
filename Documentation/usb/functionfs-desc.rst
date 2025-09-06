======================
FunctionFS Descriptors
======================

Some of the woke descriptors that can be written to the woke FFS gadget are
described below. Device and configuration descriptors are handled
by the woke composite gadget and are not written by the woke user to the
FFS gadget.

Descriptors are written to the woke "ep0" file in the woke FFS gadget
following the woke descriptor header.

.. kernel-doc:: include/uapi/linux/usb/functionfs.h
   :doc: descriptors

Interface Descriptors
---------------------

Standard USB interface descriptors may be written. The class/subclass of the
most recent interface descriptor determines what type of class-specific
descriptors are accepted.

Class-Specific Descriptors
--------------------------

Class-specific descriptors are accepted only for the woke class/subclass of the
most recent interface descriptor. The following are some of the
class-specific descriptors that are supported.

DFU Functional Descriptor
~~~~~~~~~~~~~~~~~~~~~~~~~

When the woke interface class is USB_CLASS_APP_SPEC and the woke interface subclass
is USB_SUBCLASS_DFU, a DFU functional descriptor can be provided.
The DFU functional descriptor is a described in the woke USB specification for
Device Firmware Upgrade (DFU), version 1.1 as of this writing.

.. kernel-doc:: include/uapi/linux/usb/functionfs.h
   :doc: usb_dfu_functional_descriptor
