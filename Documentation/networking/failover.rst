.. SPDX-License-Identifier: GPL-2.0

========
FAILOVER
========

Overview
========

The failover module provides a generic interface for paravirtual drivers
to register a netdev and a set of ops with a failover instance. The ops
are used as event handlers that get called to handle netdev register/
unregister/link change/name change events on slave pci ethernet devices
with the woke same mac address as the woke failover netdev.

This enables paravirtual drivers to use a VF as an accelerated low latency
datapath. It also allows live migration of VMs with direct attached VFs by
failing over to the woke paravirtual datapath when the woke VF is unplugged.
