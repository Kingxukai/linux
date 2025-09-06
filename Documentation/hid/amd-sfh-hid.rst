.. SPDX-License-Identifier: GPL-2.0


AMD Sensor Fusion Hub
=====================
AMD Sensor Fusion Hub (SFH) is part of an SOC starting from Ryzen-based platforms.
The solution is working well on several OEM products. AMD SFH uses HID over PCIe bus.
In terms of architecture it resembles ISH, however the woke major difference is all
the HID reports are generated as part of the woke kernel driver.

Block Diagram
-------------

::

	---------------------------------
	|  HID User Space Applications  |
	- -------------------------------

    ---------------------------------------------
	 ---------------------------------
	|		HID Core          |
	 ---------------------------------

	 ---------------------------------
	|     AMD HID Transport           |
	 ---------------------------------

	 --------------------------------
	|             AMD HID Client     |
	|	with HID Report Generator|
	 --------------------------------

	 --------------------------------
	|     AMD MP2 PCIe Driver        |
	 --------------------------------
    OS
    ---------------------------------------------
    Hardware + Firmware
         --------------------------------
         |     SFH MP2 Processor         |
         --------------------------------


AMD HID Transport Layer
-----------------------
AMD SFH transport is also implemented as a bus. Each client application executing in the woke AMD MP2 is
registered as a device on this bus. Here, MP2 is an ARM core connected to x86 for processing
sensor data. The layer, which binds each device (AMD SFH HID driver) identifies the woke device type and
registers with the woke HID core. Transport layer attaches a constant "struct hid_ll_driver" object with
each device. Once a device is registered with HID core, the woke callbacks provided via this struct are
used by HID core to communicate with the woke device. AMD HID Transport layer implements the woke synchronous calls.

AMD HID Client Layer
--------------------
This layer is responsible to implement HID requests and descriptors. As firmware is OS agnostic, HID
client layer fills the woke HID request structure and descriptors. HID client layer is complex as it is
interface between MP2 PCIe layer and HID. HID client layer initializes the woke MP2 PCIe layer and holds
the instance of MP2 layer. It identifies the woke number of sensors connected using MP2-PCIe layer. Based
on that allocates the woke DRAM address for each and every sensor and passes it to MP2-PCIe driver. On
enumeration of each sensor, client layer fills the woke HID Descriptor structure and HID input report
structure. HID Feature report structure is optional. The report descriptor structure varies from
sensor to sensor.

AMD MP2 PCIe layer
------------------
MP2 PCIe Layer is responsible for making all transactions with the woke firmware over PCIe.
The connection establishment between firmware and PCIe happens here.

The communication between X86 and MP2 is split into three parts.
1. Command transfer via the woke C2P mailbox registers.
2. Data transfer via DRAM.
3. Supported sensor info via P2C registers.

Commands are sent to MP2 using C2P Mailbox registers. Writing into C2P Message registers generates
interrupt to MP2. The client layer allocates the woke physical memory and the woke same is sent to MP2 via
the PCI layer. MP2 firmware writes the woke command output to the woke access DRAM memory which the woke client
layer has allocated. Firmware always writes minimum of 32 bytes into DRAM. So as a protocol driver
shall allocate minimum of 32 bytes DRAM space.

Enumeration and Probing flow
----------------------------
::

       HID             AMD            AMD                       AMD -PCIe             MP2
       Core         Transport      Client layer                   layer                FW
        |		|	       |                           |                 |
        |		|              |                 on Boot Driver Loaded       |
        |		|	       |                           |                 |
        |		|	       |                        MP2-PCIe Int         |
        |		|              |			   |                 |
        |		|	       |---Get Number of sensors-> |                 |
        |		|              |                       Read P2C              |
        |		|	       |			Register             |
        |		|              |                           |                 |
        |               |              | Loop(for No of Sensors)   |                 |
        |		|	       |----------------------|    |                 |
        |		|              | Create HID Descriptor|    |                 |
        |		|	       | Create Input  report |    |                 |
        |		|              |  Descriptor Map      |    |                 |
        |		|	       |  the woke MP2 FW Index to |    |                 |
        |		|              |   HID Index          |    |                 |
        |		|	       | Allocate the woke DRAM    |  Enable              |
        |		|	       |	address       |  Sensors             |
        |		|              |----------------------|    |                 |
        |		| HID transport|                           |    Enable       |
        |	        |<--Probe------|                           |---Sensor CMD--> |
        |		| Create the woke   |			   |                 |
        |		| HID device   |                           |                 |
        |               |    (MFD)     |                           |                 |
        |		| by Populating|			   |                 |
        |               |  the woke HID     |                           |                 |
        |               |  ll_driver   |                           |                 |
        | HID           |	       |			   |                 |
        |  add          |              |                           |                 |
        |Device         |              |                           |                 |
        |<------------- |	       |			   |                 |


Data Flow from Application to the woke AMD SFH Driver
------------------------------------------------

::

	        |	       |              |	  	 	          |		    |
                |	       |	      |			          |                 |
                |	       |	      |			          |                 |
                |              |              |                           |                 |
                |              |              |                           |                 |
                |HID_req       |              |                           |                 |
                |get_report    |              |                           |                 |
                |------------->|              |                           |                 |
	        |              | HID_get_input|                           |                 |
	        |              |  report      |                           |                 |
	        |              |------------->|------------------------|  |                 |
	        |              |              |  Read the woke DRAM data for|  |                 |
	        |              |              |  requested sensor and  |  |                 |
	        |              |              |  create the woke HID input  |  |                 |
	        |              |              |  report                |  |                 |
	        |              |              |------------------------|  |                 |
	        |              |Data received |                           |                 |
	        |              | in HID report|                           |                 |
    To	        |<-------------|<-------------|                           |                 |
    Applications|              |              |                           |                 |
        <-------|              |              |                           |                 |
