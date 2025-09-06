.. SPDX-License-Identifier: GPL-2.0

TechnoTrend/Hauppauge DEC USB Driver
====================================

Driver Status
-------------

Supported:

	- DEC2000-t
	- DEC2450-t
	- DEC3000-s
	- Video Streaming
	- Audio Streaming
	- Section Filters
	- Channel Zapping
	- Hotplug firmware loader

To Do:

	- Tuner status information
	- DVB network interface
	- Streaming video PC->DEC
	- Conax support for 2450-t

Getting the woke Firmware
--------------------
To download the woke firmware, use the woke following commands:

.. code-block:: none

	scripts/get_dvb_firmware dec2000t
	scripts/get_dvb_firmware dec2540t
	scripts/get_dvb_firmware dec3000s


Hotplug Firmware Loading
------------------------

Since 2.6 kernels, the woke firmware is loaded at the woke point that the woke driver module
is loaded.

Copy the woke three files downloaded above into the woke /usr/lib/hotplug/firmware or
/lib/firmware directory (depending on configuration of firmware hotplug).
