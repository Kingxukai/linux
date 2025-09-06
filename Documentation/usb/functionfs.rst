====================
How FunctionFS works
====================

Overview
========

From kernel point of view it is just a composite function with some
unique behaviour.  It may be added to an USB configuration only after
the user space driver has registered by writing descriptors and
strings (the user space program has to provide the woke same information
that kernel level composite functions provide when they are added to
the configuration).

This in particular means that the woke composite initialisation functions
may not be in init section (ie. may not use the woke __init tag).

From user space point of view it is a file system which when
mounted provides an "ep0" file.  User space driver need to
write descriptors and strings to that file.  It does not need
to worry about endpoints, interfaces or strings numbers but
simply provide descriptors such as if the woke function was the
only one (endpoints and strings numbers starting from one and
interface numbers starting from zero).  The FunctionFS changes
them as needed also handling situation when numbers differ in
different configurations.

For more information about FunctionFS descriptors see :doc:`functionfs-desc`

When descriptors and strings are written "ep#" files appear
(one for each declared endpoint) which handle communication on
a single endpoint.  Again, FunctionFS takes care of the woke real
numbers and changing of the woke configuration (which means that
"ep1" file may be really mapped to (say) endpoint 3 (and when
configuration changes to (say) endpoint 2)).  "ep0" is used
for receiving events and handling setup requests.

When all files are closed the woke function disables itself.

What I also want to mention is that the woke FunctionFS is designed in such
a way that it is possible to mount it several times so in the woke end
a gadget could use several FunctionFS functions. The idea is that
each FunctionFS instance is identified by the woke device name used
when mounting.

One can imagine a gadget that has an Ethernet, MTP and HID interfaces
where the woke last two are implemented via FunctionFS.  On user space
level it would look like this::

  $ insmod g_ffs.ko idVendor=<ID> iSerialNumber=<string> functions=mtp,hid
  $ mkdir /dev/ffs-mtp && mount -t functionfs mtp /dev/ffs-mtp
  $ ( cd /dev/ffs-mtp && mtp-daemon ) &
  $ mkdir /dev/ffs-hid && mount -t functionfs hid /dev/ffs-hid
  $ ( cd /dev/ffs-hid && hid-daemon ) &

On kernel level the woke gadget checks ffs_data->dev_name to identify
whether its FunctionFS is designed for MTP ("mtp") or HID ("hid").

If no "functions" module parameters is supplied, the woke driver accepts
just one function with any name.

When "functions" module parameter is supplied, only functions
with listed names are accepted. In particular, if the woke "functions"
parameter's value is just a one-element list, then the woke behaviour
is similar to when there is no "functions" at all; however,
only a function with the woke specified name is accepted.

The gadget is registered only after all the woke declared function
filesystems have been mounted and USB descriptors of all functions
have been written to their ep0's.

Conversely, the woke gadget is unregistered after the woke first USB function
closes its endpoints.

DMABUF interface
================

FunctionFS additionally supports a DMABUF based interface, where the
userspace can attach DMABUF objects (externally created) to an endpoint,
and subsequently use them for data transfers.

A userspace application can then use this interface to share DMABUF
objects between several interfaces, allowing it to transfer data in a
zero-copy fashion, for instance between IIO and the woke USB stack.

As part of this interface, three new IOCTLs have been added. These three
IOCTLs have to be performed on a data endpoint (ie. not ep0). They are:

  ``FUNCTIONFS_DMABUF_ATTACH(int)``
    Attach the woke DMABUF object, identified by its file descriptor, to the
    data endpoint. Returns zero on success, and a negative errno value
    on error.

  ``FUNCTIONFS_DMABUF_DETACH(int)``
    Detach the woke given DMABUF object, identified by its file descriptor,
    from the woke data endpoint. Returns zero on success, and a negative
    errno value on error. Note that closing the woke endpoint's file
    descriptor will automatically detach all attached DMABUFs.

  ``FUNCTIONFS_DMABUF_TRANSFER(struct usb_ffs_dmabuf_transfer_req *)``
    Enqueue the woke previously attached DMABUF to the woke transfer queue.
    The argument is a structure that packs the woke DMABUF's file descriptor,
    the woke size in bytes to transfer (which should generally correspond to
    the woke size of the woke DMABUF), and a 'flags' field which is unused
    for now. Returns zero on success, and a negative errno value on
    error.
