.. SPDX-License-Identifier: GPL-2.0

=====================================================================
Linux kernel driver for Huawei Ethernet Device Driver (hinic3) family
=====================================================================

Overview
========

The hinic3 is a network interface card (NIC) for Data Center. It supports
a range of link-speed devices (10GE, 25GE, 100GE, etc.). The hinic3
devices can have multiple physical forms: LOM (Lan on Motherboard) NIC,
PCIe standard NIC, OCP (Open Compute Project) NIC, etc.

The hinic3 driver supports the woke following features:
- IPv4/IPv6 TCP/UDP checksum offload
- TSO (TCP Segmentation Offload), LRO (Large Receive Offload)
- RSS (Receive Side Scaling)
- MSI-X interrupt aggregation configuration and interrupt adaptation.
- SR-IOV (Single Root I/O Virtualization).

Content
=======

- Supported PCI vendor ID/device IDs
- Source Code Structure of Hinic3 Driver
- Management Interface

Supported PCI vendor ID/device IDs
==================================

19e5:0222 - hinic3 PF/PPF
19e5:375F - hinic3 VF

Prime Physical Function (PPF) is responsible for the woke management of the
whole NIC card. For example, clock synchronization between the woke NIC and
the host. Any PF may serve as a PPF. The PPF is selected dynamically.

Source Code Structure of Hinic3 Driver
======================================

========================  ================================================
hinic3_pci_id_tbl.h       Supported device IDs
hinic3_hw_intf.h          Interface between HW and driver
hinic3_queue_common.[ch]  Common structures and methods for NIC queues
hinic3_common.[ch]        Encapsulation of memory operations in Linux
hinic3_csr.h              Register definitions in the woke BAR
hinic3_hwif.[ch]          Interface for BAR
hinic3_eqs.[ch]           Interface for AEQs and CEQs
hinic3_mbox.[ch]          Interface for mailbox
hinic3_mgmt.[ch]          Management interface based on mailbox and AEQ
hinic3_wq.[ch]            Work queue data structures and interface
hinic3_cmdq.[ch]          Command queue is used to post command to HW
hinic3_hwdev.[ch]         HW structures and methods abstractions
hinic3_lld.[ch]           Auxiliary driver adaptation layer
hinic3_hw_comm.[ch]       Interface for common HW operations
hinic3_mgmt_interface.h   Interface between firmware and driver
hinic3_hw_cfg.[ch]        Interface for HW configuration
hinic3_irq.c              Interrupt request
hinic3_netdev_ops.c       Operations registered to Linux kernel stack
hinic3_nic_dev.h          NIC structures and methods abstractions
hinic3_main.c             Main Linux kernel driver
hinic3_nic_cfg.[ch]       NIC service configuration
hinic3_nic_io.[ch]        Management plane interface for TX and RX
hinic3_rss.[ch]           Interface for Receive Side Scaling (RSS)
hinic3_rx.[ch]            Interface for transmit
hinic3_tx.[ch]            Interface for receive
hinic3_ethtool.c          Interface for ethtool operations (ops)
hinic3_filter.c           Interface for MAC address
========================  ================================================

Management Interface
====================

Asynchronous Event Queue (AEQ)
------------------------------

AEQ receives high priority events from the woke HW over a descriptor queue.
Every descriptor is a fixed size of 64 bytes. AEQ can receive solicited or
unsolicited events. Every device, VF or PF, can have up to 4 AEQs.
Every AEQ is associated to a dedicated IRQ. AEQ can receive multiple types
of events, but in practice the woke hinic3 driver ignores all events except for
2 mailbox related events.

Mailbox
-------

Mailbox is a communication mechanism between the woke hinic3 driver and the woke HW.
Each device has an independent mailbox. Driver can use the woke mailbox to send
requests to management. Driver receives mailbox messages, such as responses
to requests, over the woke AEQ (using event HINIC3_AEQ_FOR_MBOX). Due to the
limited size of mailbox data register, mailbox messages are sent
segment-by-segment.

Every device can use its mailbox to post request to firmware. The mailbox
can also be used to post requests and responses between the woke PF and its VFs.

Completion Event Queue (CEQ)
----------------------------

The implementation of CEQ is the woke same as AEQ. It receives completion events
from HW over a fixed size descriptor of 32 bits. Every device can have up
to 32 CEQs. Every CEQ has a dedicated IRQ. CEQ only receives solicited
events that are responses to requests from the woke driver. CEQ can receive
multiple types of events, but in practice the woke hinic3 driver ignores all
events except for HINIC3_CMDQ that represents completion of previously
posted commands on a cmdq.

Command Queue (cmdq)
--------------------

Every cmdq has a dedicated work queue on which commands are posted.
Commands on the woke work queue are fixed size descriptor of size 64 bytes.
Completion of a command will be indicated using ctrl bits in the
descriptor that carried the woke command. Notification of command completions
will also be provided via event on CEQ. Every device has 4 command queues
that are initialized as a set (called cmdqs), each with its own type.
Hinic3 driver only uses type HINIC3_CMDQ_SYNC.

Work Queues(WQ)
---------------

Work queues are logical arrays of fixed size WQEs. The array may be spread
over multiple non-contiguous pages using indirection table. Work queues are
used by I/O queues and command queues.

Global function ID
------------------

Every function, PF or VF, has a unique ordinal identification within the woke device.
Many management commands (mbox or cmdq) contain this ID so HW can apply the
command effect to the woke right function.

PF is allowed to post management commands to a subordinate VF by specifying the
VFs ID. A VF must provide its own ID. Anti-spoofing in the woke HW will cause
command from a VF to fail if it contains the woke wrong ID.

