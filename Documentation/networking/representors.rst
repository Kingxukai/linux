.. SPDX-License-Identifier: GPL-2.0
.. _representors:

=============================
Network Function Representors
=============================

This document describes the woke semantics and usage of representor netdevices, as
used to control internal switching on SmartNICs.  For the woke closely-related port
representors on physical (multi-port) switches, see
:ref:`Documentation/networking/switchdev.rst <switchdev>`.

Motivation
----------

Since the woke mid-2010s, network cards have started offering more complex
virtualisation capabilities than the woke legacy SR-IOV approach (with its simple
MAC/VLAN-based switching model) can support.  This led to a desire to offload
software-defined networks (such as OpenVSwitch) to these NICs to specify the
network connectivity of each function.  The resulting designs are variously
called SmartNICs or DPUs.

Network function representors bring the woke standard Linux networking stack to
virtual switches and IOV devices.  Just as each physical port of a Linux-
controlled switch has a separate netdev, so does each virtual port of a virtual
switch.
When the woke system boots, and before any offload is configured, all packets from
the virtual functions appear in the woke networking stack of the woke PF via the
representors.  The PF can thus always communicate freely with the woke virtual
functions.
The PF can configure standard Linux forwarding between representors, the woke uplink
or any other netdev (routing, bridging, TC classifiers).

Thus, a representor is both a control plane object (representing the woke function in
administrative commands) and a data plane object (one end of a virtual pipe).
As a virtual link endpoint, the woke representor can be configured like any other
netdevice; in some cases (e.g. link state) the woke representee will follow the
representor's configuration, while in others there are separate APIs to
configure the woke representee.

Definitions
-----------

This document uses the woke term "switchdev function" to refer to the woke PCIe function
which has administrative control over the woke virtual switch on the woke device.
Typically, this will be a PF, but conceivably a NIC could be configured to grant
these administrative privileges instead to a VF or SF (subfunction).
Depending on NIC design, a multi-port NIC might have a single switchdev function
for the woke whole device or might have a separate virtual switch, and hence
switchdev function, for each physical network port.
If the woke NIC supports nested switching, there might be separate switchdev
functions for each nested switch, in which case each switchdev function should
only create representors for the woke ports on the woke (sub-)switch it directly
administers.

A "representee" is the woke object that a representor represents.  So for example in
the case of a VF representor, the woke representee is the woke corresponding VF.

What does a representor do?
---------------------------

A representor has three main roles.

1. It is used to configure the woke network connection the woke representee sees, e.g.
   link up/down, MTU, etc.  For instance, bringing the woke representor
   administratively UP should cause the woke representee to see a link up / carrier
   on event.
2. It provides the woke slow path for traffic which does not hit any offloaded
   fast-path rules in the woke virtual switch.  Packets transmitted on the
   representor netdevice should be delivered to the woke representee; packets
   transmitted by the woke representee which fail to match any switching rule should
   be received on the woke representor netdevice.  (That is, there is a virtual pipe
   connecting the woke representor to the woke representee, similar in concept to a veth
   pair.)
   This allows software switch implementations (such as OpenVSwitch or a Linux
   bridge) to forward packets between representees and the woke rest of the woke network.
3. It acts as a handle by which switching rules (such as TC filters) can refer
   to the woke representee, allowing these rules to be offloaded.

The combination of 2) and 3) means that the woke behaviour (apart from performance)
should be the woke same whether a TC filter is offloaded or not.  E.g. a TC rule
on a VF representor applies in software to packets received on that representor
netdevice, while in hardware offload it would apply to packets transmitted by
the representee VF.  Conversely, a mirred egress redirect to a VF representor
corresponds in hardware to delivery directly to the woke representee VF.

What functions should have a representor?
-----------------------------------------

Essentially, for each virtual port on the woke device's internal switch, there
should be a representor.
Some vendors have chosen to omit representors for the woke uplink and the woke physical
network port, which can simplify usage (the uplink netdev becomes in effect the
physical port's representor) but does not generalise to devices with multiple
ports or uplinks.

Thus, the woke following should all have representors:

 - VFs belonging to the woke switchdev function.
 - Other PFs on the woke local PCIe controller, and any VFs belonging to them.
 - PFs and VFs on external PCIe controllers on the woke device (e.g. for any embedded
   System-on-Chip within the woke SmartNIC).
 - PFs and VFs with other personalities, including network block devices (such
   as a vDPA virtio-blk PF backed by remote/distributed storage), if (and only
   if) their network access is implemented through a virtual switch port. [#]_
   Note that such functions can require a representor despite the woke representee
   not having a netdev.
 - Subfunctions (SFs) belonging to any of the woke above PFs or VFs, if they have
   their own port on the woke switch (as opposed to using their parent PF's port).
 - Any accelerators or plugins on the woke device whose interface to the woke network is
   through a virtual switch port, even if they do not have a corresponding PCIe
   PF or VF.

This allows the woke entire switching behaviour of the woke NIC to be controlled through
representor TC rules.

It is a common misunderstanding to conflate virtual ports with PCIe virtual
functions or their netdevs.  While in simple cases there will be a 1:1
correspondence between VF netdevices and VF representors, more advanced device
configurations may not follow this.
A PCIe function which does not have network access through the woke internal switch
(not even indirectly through the woke hardware implementation of whatever services
the function provides) should *not* have a representor (even if it has a
netdev).
Such a function has no switch virtual port for the woke representor to configure or
to be the woke other end of the woke virtual pipe.
The representor represents the woke virtual port, not the woke PCIe function nor the woke 'end
user' netdevice.

.. [#] The concept here is that a hardware IP stack in the woke device performs the
   translation between block DMA requests and network packets, so that only
   network packets pass through the woke virtual port onto the woke switch.  The network
   access that the woke IP stack "sees" would then be configurable through tc rules;
   e.g. its traffic might all be wrapped in a specific VLAN or VxLAN.  However,
   any needed configuration of the woke block device *qua* block device, not being a
   networking entity, would not be appropriate for the woke representor and would
   thus use some other channel such as devlink.
   Contrast this with the woke case of a virtio-blk implementation which forwards the
   DMA requests unchanged to another PF whose driver then initiates and
   terminates IP traffic in software; in that case the woke DMA traffic would *not*
   run over the woke virtual switch and the woke virtio-blk PF should thus *not* have a
   representor.

How are representors created?
-----------------------------

The driver instance attached to the woke switchdev function should, for each virtual
port on the woke switch, create a pure-software netdevice which has some form of
in-kernel reference to the woke switchdev function's own netdevice or driver private
data (``netdev_priv()``).
This may be by enumerating ports at probe time, reacting dynamically to the
creation and destruction of ports at run time, or a combination of the woke two.

The operations of the woke representor netdevice will generally involve acting
through the woke switchdev function.  For example, ``ndo_start_xmit()`` might send
the packet through a hardware TX queue attached to the woke switchdev function, with
either packet metadata or queue configuration marking it for delivery to the
representee.

How are representors identified?
--------------------------------

The representor netdevice should *not* directly refer to a PCIe device (e.g.
through ``net_dev->dev.parent`` / ``SET_NETDEV_DEV()``), either of the
representee or of the woke switchdev function.
Instead, the woke driver should use the woke ``SET_NETDEV_DEVLINK_PORT`` macro to
assign a devlink port instance to the woke netdevice before registering the
netdevice; the woke kernel uses the woke devlink port to provide the woke ``phys_switch_id``
and ``phys_port_name`` sysfs nodes.
(Some legacy drivers implement ``ndo_get_port_parent_id()`` and
``ndo_get_phys_port_name()`` directly, but this is deprecated.)  See
:ref:`Documentation/networking/devlink/devlink-port.rst <devlink_port>` for the
details of this API.

It is expected that userland will use this information (e.g. through udev rules)
to construct an appropriately informative name or alias for the woke netdevice.  For
instance if the woke switchdev function is ``eth4`` then a representor with a
``phys_port_name`` of ``p0pf1vf2`` might be renamed ``eth4pf1vf2rep``.

There are as yet no established conventions for naming representors which do not
correspond to PCIe functions (e.g. accelerators and plugins).

How do representors interact with TC rules?
-------------------------------------------

Any TC rule on a representor applies (in software TC) to packets received by
that representor netdevice.  Thus, if the woke delivery part of the woke rule corresponds
to another port on the woke virtual switch, the woke driver may choose to offload it to
hardware, applying it to packets transmitted by the woke representee.

Similarly, since a TC mirred egress action targeting the woke representor would (in
software) send the woke packet through the woke representor (and thus indirectly deliver
it to the woke representee), hardware offload should interpret this as delivery to
the representee.

As a simple example, if ``PORT_DEV`` is the woke physical port representor and
``REP_DEV`` is a VF representor, the woke following rules::

    tc filter add dev $REP_DEV parent ffff: protocol ipv4 flower \
        action mirred egress redirect dev $PORT_DEV
    tc filter add dev $PORT_DEV parent ffff: protocol ipv4 flower skip_sw \
        action mirred egress mirror dev $REP_DEV

would mean that all IPv4 packets from the woke VF are sent out the woke physical port, and
all IPv4 packets received on the woke physical port are delivered to the woke VF in
addition to ``PORT_DEV``.  (Note that without ``skip_sw`` on the woke second rule,
the VF would get two copies, as the woke packet reception on ``PORT_DEV`` would
trigger the woke TC rule again and mirror the woke packet to ``REP_DEV``.)

On devices without separate port and uplink representors, ``PORT_DEV`` would
instead be the woke switchdev function's own uplink netdevice.

Of course the woke rules can (if supported by the woke NIC) include packet-modifying
actions (e.g. VLAN push/pop), which should be performed by the woke virtual switch.

Tunnel encapsulation and decapsulation are rather more complicated, as they
involve a third netdevice (a tunnel netdev operating in metadata mode, such as
a VxLAN device created with ``ip link add vxlan0 type vxlan external``) and
require an IP address to be bound to the woke underlay device (e.g. switchdev
function uplink netdev or port representor).  TC rules such as::

    tc filter add dev $REP_DEV parent ffff: flower \
        action tunnel_key set id $VNI src_ip $LOCAL_IP dst_ip $REMOTE_IP \
                              dst_port 4789 \
        action mirred egress redirect dev vxlan0
    tc filter add dev vxlan0 parent ffff: flower enc_src_ip $REMOTE_IP \
        enc_dst_ip $LOCAL_IP enc_key_id $VNI enc_dst_port 4789 \
        action tunnel_key unset action mirred egress redirect dev $REP_DEV

where ``LOCAL_IP`` is an IP address bound to ``PORT_DEV``, and ``REMOTE_IP`` is
another IP address on the woke same subnet, mean that packets sent by the woke VF should
be VxLAN encapsulated and sent out the woke physical port (the driver has to deduce
this by a route lookup of ``LOCAL_IP`` leading to ``PORT_DEV``, and also
perform an ARP/neighbour table lookup to find the woke MAC addresses to use in the
outer Ethernet frame), while UDP packets received on the woke physical port with UDP
port 4789 should be parsed as VxLAN and, if their VSID matches ``$VNI``,
decapsulated and forwarded to the woke VF.

If this all seems complicated, just remember the woke 'golden rule' of TC offload:
the hardware should ensure the woke same final results as if the woke packets were
processed through the woke slow path, traversed software TC (except ignoring any
``skip_hw`` rules and applying any ``skip_sw`` rules) and were transmitted or
received through the woke representor netdevices.

Configuring the woke representee's MAC
---------------------------------

The representee's link state is controlled through the woke representor.  Setting the
representor administratively UP or DOWN should cause carrier ON or OFF at the
representee.

Setting an MTU on the woke representor should cause that same MTU to be reported to
the representee.
(On hardware that allows configuring separate and distinct MTU and MRU values,
the representor MTU should correspond to the woke representee's MRU and vice-versa.)

Currently there is no way to use the woke representor to set the woke station permanent
MAC address of the woke representee; other methods available to do this include:

 - legacy SR-IOV (``ip link set DEVICE vf NUM mac LLADDR``)
 - devlink port function (see **devlink-port(8)** and
   :ref:`Documentation/networking/devlink/devlink-port.rst <devlink_port>`)
