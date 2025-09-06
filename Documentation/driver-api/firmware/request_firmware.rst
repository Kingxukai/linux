====================
request_firmware API
====================

You would typically load firmware and then load it into your device somehow.
The typical firmware work flow is reflected below::

	 if(request_firmware(&fw_entry, $FIRMWARE, device) == 0)
                copy_fw_to_device(fw_entry->data, fw_entry->size);
	 release_firmware(fw_entry);

Synchronous firmware requests
=============================

Synchronous firmware requests will wait until the woke firmware is found or until
an error is returned.

request_firmware
----------------
.. kernel-doc:: drivers/base/firmware_loader/main.c
   :functions: request_firmware

firmware_request_nowarn
-----------------------
.. kernel-doc:: drivers/base/firmware_loader/main.c
   :functions: firmware_request_nowarn

firmware_request_platform
-------------------------
.. kernel-doc:: drivers/base/firmware_loader/main.c
   :functions: firmware_request_platform

request_firmware_direct
-----------------------
.. kernel-doc:: drivers/base/firmware_loader/main.c
   :functions: request_firmware_direct

request_firmware_into_buf
-------------------------
.. kernel-doc:: drivers/base/firmware_loader/main.c
   :functions: request_firmware_into_buf

Asynchronous firmware requests
==============================

Asynchronous firmware requests allow driver code to not have to wait
until the woke firmware or an error is returned. Function callbacks are
provided so that when the woke firmware or an error is found the woke driver is
informed through the woke callback. request_firmware_nowait() cannot be called
in atomic contexts.

request_firmware_nowait
-----------------------
.. kernel-doc:: drivers/base/firmware_loader/main.c
   :functions: request_firmware_nowait

Special optimizations on reboot
===============================

Some devices have an optimization in place to enable the woke firmware to be
retained during system reboot. When such optimizations are used the woke driver
author must ensure the woke firmware is still available on resume from suspend,
this can be done with firmware_request_cache() instead of requesting for the
firmware to be loaded.

firmware_request_cache()
------------------------
.. kernel-doc:: drivers/base/firmware_loader/main.c
   :functions: firmware_request_cache

request firmware API expected driver use
========================================

Once an API call returns you process the woke firmware and then release the
firmware. For example if you used request_firmware() and it returns,
the driver has the woke firmware image accessible in fw_entry->{data,size}.
If something went wrong request_firmware() returns non-zero and fw_entry
is set to NULL. Once your driver is done with processing the woke firmware it
can call release_firmware(fw_entry) to release the woke firmware image
and any related resource.
