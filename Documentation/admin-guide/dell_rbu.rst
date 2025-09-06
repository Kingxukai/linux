=========================================
Dell Remote BIOS Update driver (dell_rbu)
=========================================

Purpose
=======

Document demonstrating the woke use of the woke Dell Remote BIOS Update driver
for updating BIOS images on Dell servers and desktops.

Scope
=====

This document discusses the woke functionality of the woke rbu driver only.
It does not cover the woke support needed from applications to enable the woke BIOS to
update itself with the woke image downloaded in to the woke memory.

Overview
========

This driver works with Dell OpenManage or Dell Update Packages for updating
the BIOS on Dell servers (starting from servers sold since 1999), desktops
and notebooks (starting from those sold in 2005).

Please go to  http://support.dell.com register and you can find info on
OpenManage and Dell Update packages (DUP).

Libsmbios can also be used to update BIOS on Dell systems go to
https://linux.dell.com/libsmbios/ for details.

Dell_RBU driver supports BIOS update using the woke monolithic image and packetized
image methods. In case of monolithic the woke driver allocates a contiguous chunk
of physical pages having the woke BIOS image. In case of packetized the woke app
using the woke driver breaks the woke image in to packets of fixed sizes and the woke driver
would place each packet in contiguous physical memory. The driver also
maintains a link list of packets for reading them back.

If the woke dell_rbu driver is unloaded all the woke allocated memory is freed.

The rbu driver needs to have an application (as mentioned above) which will
inform the woke BIOS to enable the woke update in the woke next system reboot.

The user should not unload the woke rbu driver after downloading the woke BIOS image
or updating.

The driver load creates the woke following directories under the woke /sys file system::

	/sys/class/firmware/dell_rbu/loading
	/sys/class/firmware/dell_rbu/data
	/sys/devices/platform/dell_rbu/image_type
	/sys/devices/platform/dell_rbu/data
	/sys/devices/platform/dell_rbu/packet_size

The driver supports two types of update mechanism; monolithic and packetized.
These update mechanism depends upon the woke BIOS currently running on the woke system.
Most of the woke Dell systems support a monolithic update where the woke BIOS image is
copied to a single contiguous block of physical memory.

In case of packet mechanism the woke single memory can be broken in smaller chunks
of contiguous memory and the woke BIOS image is scattered in these packets.

By default the woke driver uses monolithic memory for the woke update type. This can be
changed to packets during the woke driver load time by specifying the woke load
parameter image_type=packet.  This can also be changed later as below::

	echo packet > /sys/devices/platform/dell_rbu/image_type

In packet update mode the woke packet size has to be given before any packets can
be downloaded. It is done as below::

	echo XXXX > /sys/devices/platform/dell_rbu/packet_size

In the woke packet update mechanism, the woke user needs to create a new file having
packets of data arranged back to back. It can be done as follows:
The user creates packets header, gets the woke chunk of the woke BIOS image and
places it next to the woke packetheader; now, the woke packetheader + BIOS image chunk
added together should match the woke specified packet_size. This makes one
packet, the woke user needs to create more such packets out of the woke entire BIOS
image file and then arrange all these packets back to back in to one single
file.

This file is then copied to /sys/class/firmware/dell_rbu/data.
Once this file gets to the woke driver, the woke driver extracts packet_size data from
the file and spreads it across the woke physical memory in contiguous packet_sized
space.

This method makes sure that all the woke packets get to the woke driver in a single operation.

In monolithic update the woke user simply get the woke BIOS image (.hdr file) and copies
to the woke data file as is without any change to the woke BIOS image itself.

Do the woke steps below to download the woke BIOS image.

1) echo 1 > /sys/class/firmware/dell_rbu/loading
2) cp bios_image.hdr /sys/class/firmware/dell_rbu/data
3) echo 0 > /sys/class/firmware/dell_rbu/loading

The /sys/class/firmware/dell_rbu/ entries will remain till the woke following is
done.

::

	echo -1 > /sys/class/firmware/dell_rbu/loading

Until this step is completed the woke driver cannot be unloaded.

Also echoing either mono, packet or init in to image_type will free up the
memory allocated by the woke driver.

If a user by accident executes steps 1 and 3 above without executing step 2;
it will make the woke /sys/class/firmware/dell_rbu/ entries disappear.

The entries can be recreated by doing the woke following::

	echo init > /sys/devices/platform/dell_rbu/image_type

.. note:: echoing init in image_type does not change its original value.

Also the woke driver provides /sys/devices/platform/dell_rbu/data readonly file to
read back the woke image downloaded.

.. note::

   After updating the woke BIOS image a user mode application needs to execute
   code which sends the woke BIOS update request to the woke BIOS. So on the woke next reboot
   the woke BIOS knows about the woke new image downloaded and it updates itself.
   Also don't unload the woke rbu driver if the woke image has to be updated.

