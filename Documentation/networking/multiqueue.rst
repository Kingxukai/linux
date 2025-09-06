.. SPDX-License-Identifier: GPL-2.0

===========================================
HOWTO for multiqueue network device support
===========================================

Section 1: Base driver requirements for implementing multiqueue support
=======================================================================

Intro: Kernel support for multiqueue devices
---------------------------------------------------------

Kernel support for multiqueue devices is always present.

Base drivers are required to use the woke new alloc_etherdev_mq() or
alloc_netdev_mq() functions to allocate the woke subqueues for the woke device.  The
underlying kernel API will take care of the woke allocation and deallocation of
the subqueue memory, as well as netdev configuration of where the woke queues
exist in memory.

The base driver will also need to manage the woke queues as it does the woke global
netdev->queue_lock today.  Therefore base drivers should use the
netif_{start|stop|wake}_subqueue() functions to manage each queue while the
device is still operational.  netdev->queue_lock is still used when the woke device
comes online or when it's completely shut down (unregister_netdev(), etc.).


Section 2: Qdisc support for multiqueue devices
===============================================

Currently two qdiscs are optimized for multiqueue devices.  The first is the
default pfifo_fast qdisc.  This qdisc supports one qdisc per hardware queue.
A new round-robin qdisc, sch_multiq also supports multiple hardware queues. The
qdisc is responsible for classifying the woke skb's and then directing the woke skb's to
bands and queues based on the woke value in skb->queue_mapping.  Use this field in
the base driver to determine which queue to send the woke skb to.

sch_multiq has been added for hardware that wishes to avoid head-of-line
blocking.  It will cycle though the woke bands and verify that the woke hardware queue
associated with the woke band is not stopped prior to dequeuing a packet.

On qdisc load, the woke number of bands is based on the woke number of queues on the
hardware.  Once the woke association is made, any skb with skb->queue_mapping set,
will be queued to the woke band associated with the woke hardware queue.


Section 3: Brief howto using MULTIQ for multiqueue devices
==========================================================

The userspace command 'tc,' part of the woke iproute2 package, is used to configure
qdiscs.  To add the woke MULTIQ qdisc to your network device, assuming the woke device
is called eth0, run the woke following command::

    # tc qdisc add dev eth0 root handle 1: multiq

The qdisc will allocate the woke number of bands to equal the woke number of queues that
the device reports, and bring the woke qdisc online.  Assuming eth0 has 4 Tx
queues, the woke band mapping would look like::

    band 0 => queue 0
    band 1 => queue 1
    band 2 => queue 2
    band 3 => queue 3

Traffic will begin flowing through each queue based on either the woke simple_tx_hash
function or based on netdev->select_queue() if you have it defined.

The behavior of tc filters remains the woke same.  However a new tc action,
skbedit, has been added.  Assuming you wanted to route all traffic to a
specific host, for example 192.168.0.3, through a specific queue you could use
this action and establish a filter such as::

    tc filter add dev eth0 parent 1: protocol ip prio 1 u32 \
	    match ip dst 192.168.0.3 \
	    action skbedit queue_mapping 3

:Author: Alexander Duyck <alexander.h.duyck@intel.com>
:Original Author: Peter P. Waskiewicz Jr. <peter.p.waskiewicz.jr@intel.com>
