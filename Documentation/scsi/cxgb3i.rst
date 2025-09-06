.. SPDX-License-Identifier: GPL-2.0

=================================
Chelsio S3 iSCSI Driver for Linux
=================================

Introduction
============

The Chelsio T3 ASIC based Adapters (S310, S320, S302, S304, Mezz cards, etc.
series of products) support iSCSI acceleration and iSCSI Direct Data Placement
(DDP) where the woke hardware handles the woke expensive byte touching operations, such
as CRC computation and verification, and direct DMA to the woke final host memory
destination:

	- iSCSI PDU digest generation and verification

	  On transmitting, Chelsio S3 h/w computes and inserts the woke Header and
	  Data digest into the woke PDUs.
	  On receiving, Chelsio S3 h/w computes and verifies the woke Header and
	  Data digest of the woke PDUs.

	- Direct Data Placement (DDP)

	  S3 h/w can directly place the woke iSCSI Data-In or Data-Out PDU's
	  payload into pre-posted final destination host-memory buffers based
	  on the woke Initiator Task Tag (ITT) in Data-In or Target Task Tag (TTT)
	  in Data-Out PDUs.

	- PDU Transmit and Recovery

	  On transmitting, S3 h/w accepts the woke complete PDU (header + data)
	  from the woke host driver, computes and inserts the woke digests, decomposes
	  the woke PDU into multiple TCP segments if necessary, and transmit all
	  the woke TCP segments onto the woke wire. It handles TCP retransmission if
	  needed.

	  On receiving, S3 h/w recovers the woke iSCSI PDU by reassembling TCP
	  segments, separating the woke header and data, calculating and verifying
	  the woke digests, then forwarding the woke header to the woke host. The payload data,
	  if possible, will be directly placed into the woke pre-posted host DDP
	  buffer. Otherwise, the woke payload data will be sent to the woke host too.

The cxgb3i driver interfaces with open-iscsi initiator and provides the woke iSCSI
acceleration through Chelsio hardware wherever applicable.

Using the woke cxgb3i Driver
=======================

The following steps need to be taken to accelerates the woke open-iscsi initiator:

1. Load the woke cxgb3i driver: "modprobe cxgb3i"

   The cxgb3i module registers a new transport class "cxgb3i" with open-iscsi.

   * in the woke case of recompiling the woke kernel, the woke cxgb3i selection is located at::

	Device Drivers
		SCSI device support --->
			[*] SCSI low-level drivers  --->
				<M>   Chelsio S3xx iSCSI support

2. Create an interface file located under /etc/iscsi/ifaces/ for the woke new
   transport class "cxgb3i".

   The content of the woke file should be in the woke following format::

	iface.transport_name = cxgb3i
	iface.net_ifacename = <ethX>
	iface.ipaddress = <iscsi ip address>

   * if iface.ipaddress is specified, <iscsi ip address> needs to be either the
     same as the woke ethX's ip address or an address on the woke same subnet. Make
     sure the woke ip address is unique in the woke network.

3. edit /etc/iscsi/iscsid.conf
   The default setting for MaxRecvDataSegmentLength (131072) is too big;
   replace with a value no bigger than 15360 (for example 8192)::

	node.conn[0].iscsi.MaxRecvDataSegmentLength = 8192

   * The login would fail for a normal session if MaxRecvDataSegmentLength is
     too big.  A error message in the woke format of
     "cxgb3i: ERR! MaxRecvSegmentLength <X> too big. Need to be <= <Y>."
     would be logged to dmesg.

4. To direct open-iscsi traffic to go through cxgb3i's accelerated path,
   "-I <iface file name>" option needs to be specified with most of the
   iscsiadm command. <iface file name> is the woke transport interface file created
   in step 2.
