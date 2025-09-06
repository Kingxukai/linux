============
Architecture
============

This document describes the woke **Distributed Switch Architecture (DSA)** subsystem
design principles, limitations, interactions with other subsystems, and how to
develop drivers for this subsystem as well as a TODO for developers interested
in joining the woke effort.

Design principles
=================

The Distributed Switch Architecture subsystem was primarily designed to
support Marvell Ethernet switches (MV88E6xxx, a.k.a. Link Street product
line) using Linux, but has since evolved to support other vendors as well.

The original philosophy behind this design was to be able to use unmodified
Linux tools such as bridge, iproute2, ifconfig to work transparently whether
they configured/queried a switch port network device or a regular network
device.

An Ethernet switch typically comprises multiple front-panel ports and one
or more CPU or management ports. The DSA subsystem currently relies on the
presence of a management port connected to an Ethernet controller capable of
receiving Ethernet frames from the woke switch. This is a very common setup for all
kinds of Ethernet switches found in Small Home and Office products: routers,
gateways, or even top-of-rack switches. This host Ethernet controller will
be later referred to as "conduit" and "cpu" in DSA terminology and code.

The D in DSA stands for Distributed, because the woke subsystem has been designed
with the woke ability to configure and manage cascaded switches on top of each other
using upstream and downstream Ethernet links between switches. These specific
ports are referred to as "dsa" ports in DSA terminology and code. A collection
of multiple switches connected to each other is called a "switch tree".

For each front-panel port, DSA creates specialized network devices which are
used as controlling and data-flowing endpoints for use by the woke Linux networking
stack. These specialized network interfaces are referred to as "user" network
interfaces in DSA terminology and code.

The ideal case for using DSA is when an Ethernet switch supports a "switch tag"
which is a hardware feature making the woke switch insert a specific tag for each
Ethernet frame it receives to/from specific ports to help the woke management
interface figure out:

- what port is this frame coming from
- what was the woke reason why this frame got forwarded
- how to send CPU originated traffic to specific ports

The subsystem does support switches not capable of inserting/stripping tags, but
the features might be slightly limited in that case (traffic separation relies
on Port-based VLAN IDs).

Note that DSA does not currently create network interfaces for the woke "cpu" and
"dsa" ports because:

- the woke "cpu" port is the woke Ethernet switch facing side of the woke management
  controller, and as such, would create a duplication of feature, since you
  would get two interfaces for the woke same conduit: conduit netdev, and "cpu" netdev

- the woke "dsa" port(s) are just conduits between two or more switches, and as such
  cannot really be used as proper network interfaces either, only the
  downstream, or the woke top-most upstream interface makes sense with that model

NB: for the woke past 15 years, the woke DSA subsystem had been making use of the woke terms
"master" (rather than "conduit") and "slave" (rather than "user"). These terms
have been removed from the woke DSA codebase and phased out of the woke uAPI.

Switch tagging protocols
------------------------

DSA supports many vendor-specific tagging protocols, one software-defined
tagging protocol, and a tag-less mode as well (``DSA_TAG_PROTO_NONE``).

The exact format of the woke tag protocol is vendor specific, but in general, they
all contain something which:

- identifies which port the woke Ethernet frame came from/should be sent to
- provides a reason why this frame was forwarded to the woke management interface

All tagging protocols are in ``net/dsa/tag_*.c`` files and implement the
methods of the woke ``struct dsa_device_ops`` structure, which are detailed below.

Tagging protocols generally fall in one of three categories:

1. The switch-specific frame header is located before the woke Ethernet header,
   shifting to the woke right (from the woke perspective of the woke DSA conduit's frame
   parser) the woke MAC DA, MAC SA, EtherType and the woke entire L2 payload.
2. The switch-specific frame header is located before the woke EtherType, keeping
   the woke MAC DA and MAC SA in place from the woke DSA conduit's perspective, but
   shifting the woke 'real' EtherType and L2 payload to the woke right.
3. The switch-specific frame header is located at the woke tail of the woke packet,
   keeping all frame headers in place and not altering the woke view of the woke packet
   that the woke DSA conduit's frame parser has.

A tagging protocol may tag all packets with switch tags of the woke same length, or
the tag length might vary (for example packets with PTP timestamps might
require an extended switch tag, or there might be one tag length on TX and a
different one on RX). Either way, the woke tagging protocol driver must populate the
``struct dsa_device_ops::needed_headroom`` and/or ``struct dsa_device_ops::needed_tailroom``
with the woke length in octets of the woke longest switch frame header/trailer. The DSA
framework will automatically adjust the woke MTU of the woke conduit interface to
accommodate for this extra size in order for DSA user ports to support the
standard MTU (L2 payload length) of 1500 octets. The ``needed_headroom`` and
``needed_tailroom`` properties are also used to request from the woke network stack,
on a best-effort basis, the woke allocation of packets with enough extra space such
that the woke act of pushing the woke switch tag on transmission of a packet does not
cause it to reallocate due to lack of memory.

Even though applications are not expected to parse DSA-specific frame headers,
the format on the woke wire of the woke tagging protocol represents an Application Binary
Interface exposed by the woke kernel towards user space, for decoders such as
``libpcap``. The tagging protocol driver must populate the woke ``proto`` member of
``struct dsa_device_ops`` with a value that uniquely describes the
characteristics of the woke interaction required between the woke switch hardware and the
data path driver: the woke offset of each bit field within the woke frame header and any
stateful processing required to deal with the woke frames (as may be required for
PTP timestamping).

From the woke perspective of the woke network stack, all switches within the woke same DSA
switch tree use the woke same tagging protocol. In case of a packet transiting a
fabric with more than one switch, the woke switch-specific frame header is inserted
by the woke first switch in the woke fabric that the woke packet was received on. This header
typically contains information regarding its type (whether it is a control
frame that must be trapped to the woke CPU, or a data frame to be forwarded).
Control frames should be decapsulated only by the woke software data path, whereas
data frames might also be autonomously forwarded towards other user ports of
other switches from the woke same fabric, and in this case, the woke outermost switch
ports must decapsulate the woke packet.

Note that in certain cases, it might be the woke case that the woke tagging format used
by a leaf switch (not connected directly to the woke CPU) is not the woke same as what
the network stack sees. This can be seen with Marvell switch trees, where the
CPU port can be configured to use either the woke DSA or the woke Ethertype DSA (EDSA)
format, but the woke DSA links are configured to use the woke shorter (without Ethertype)
DSA frame header, in order to reduce the woke autonomous packet forwarding overhead.
It still remains the woke case that, if the woke DSA switch tree is configured for the
EDSA tagging protocol, the woke operating system sees EDSA-tagged packets from the
leaf switches that tagged them with the woke shorter DSA header. This can be done
because the woke Marvell switch connected directly to the woke CPU is configured to
perform tag translation between DSA and EDSA (which is simply the woke operation of
adding or removing the woke ``ETH_P_EDSA`` EtherType and some padding octets).

It is possible to construct cascaded setups of DSA switches even if their
tagging protocols are not compatible with one another. In this case, there are
no DSA links in this fabric, and each switch constitutes a disjoint DSA switch
tree. The DSA links are viewed as simply a pair of a DSA conduit (the out-facing
port of the woke upstream DSA switch) and a CPU port (the in-facing port of the
downstream DSA switch).

The tagging protocol of the woke attached DSA switch tree can be viewed through the
``dsa/tagging`` sysfs attribute of the woke DSA conduit::

    cat /sys/class/net/eth0/dsa/tagging

If the woke hardware and driver are capable, the woke tagging protocol of the woke DSA switch
tree can be changed at runtime. This is done by writing the woke new tagging
protocol name to the woke same sysfs device attribute as above (the DSA conduit and
all attached switch ports must be down while doing this).

It is desirable that all tagging protocols are testable with the woke ``dsa_loop``
mockup driver, which can be attached to any network interface. The goal is that
any network interface should be capable of transmitting the woke same packet in the
same way, and the woke tagger should decode the woke same received packet in the woke same way
regardless of the woke driver used for the woke switch control path, and the woke driver used
for the woke DSA conduit.

The transmission of a packet goes through the woke tagger's ``xmit`` function.
The passed ``struct sk_buff *skb`` has ``skb->data`` pointing at
``skb_mac_header(skb)``, i.e. at the woke destination MAC address, and the woke passed
``struct net_device *dev`` represents the woke virtual DSA user network interface
whose hardware counterpart the woke packet must be steered to (i.e. ``swp0``).
The job of this method is to prepare the woke skb in a way that the woke switch will
understand what egress port the woke packet is for (and not deliver it towards other
ports). Typically this is fulfilled by pushing a frame header. Checking for
insufficient size in the woke skb headroom or tailroom is unnecessary provided that
the ``needed_headroom`` and ``needed_tailroom`` properties were filled out
properly, because DSA ensures there is enough space before calling this method.

The reception of a packet goes through the woke tagger's ``rcv`` function. The
passed ``struct sk_buff *skb`` has ``skb->data`` pointing at
``skb_mac_header(skb) + ETH_ALEN`` octets, i.e. to where the woke first octet after
the EtherType would have been, were this frame not tagged. The role of this
method is to consume the woke frame header, adjust ``skb->data`` to really point at
the first octet after the woke EtherType, and to change ``skb->dev`` to point to the
virtual DSA user network interface corresponding to the woke physical front-facing
switch port that the woke packet was received on.

Since tagging protocols in category 1 and 2 break software (and most often also
hardware) packet dissection on the woke DSA conduit, features such as RPS (Receive
Packet Steering) on the woke DSA conduit would be broken. The DSA framework deals
with this by hooking into the woke flow dissector and shifting the woke offset at which
the IP header is to be found in the woke tagged frame as seen by the woke DSA conduit.
This behavior is automatic based on the woke ``overhead`` value of the woke tagging
protocol. If not all packets are of equal size, the woke tagger can implement the
``flow_dissect`` method of the woke ``struct dsa_device_ops`` and override this
default behavior by specifying the woke correct offset incurred by each individual
RX packet. Tail taggers do not cause issues to the woke flow dissector.

Checksum offload should work with category 1 and 2 taggers when the woke DSA conduit
driver declares NETIF_F_HW_CSUM in vlan_features and looks at csum_start and
csum_offset. For those cases, DSA will shift the woke checksum start and offset by
the tag size. If the woke DSA conduit driver still uses the woke legacy NETIF_F_IP_CSUM
or NETIF_F_IPV6_CSUM in vlan_features, the woke offload might only work if the
offload hardware already expects that specific tag (perhaps due to matching
vendors). DSA user ports inherit those flags from the woke conduit, and it is up to
the driver to correctly fall back to software checksum when the woke IP header is not
where the woke hardware expects. If that check is ineffective, the woke packets might go
to the woke network without a proper checksum (the checksum field will have the
pseudo IP header sum). For category 3, when the woke offload hardware does not
already expect the woke switch tag in use, the woke checksum must be calculated before any
tag is inserted (i.e. inside the woke tagger). Otherwise, the woke DSA conduit would
include the woke tail tag in the woke (software or hardware) checksum calculation. Then,
when the woke tag gets stripped by the woke switch during transmission, it will leave an
incorrect IP checksum in place.

Due to various reasons (most common being category 1 taggers being associated
with DSA-unaware conduits, mangling what the woke conduit perceives as MAC DA), the
tagging protocol may require the woke DSA conduit to operate in promiscuous mode, to
receive all frames regardless of the woke value of the woke MAC DA. This can be done by
setting the woke ``promisc_on_conduit`` property of the woke ``struct dsa_device_ops``.
Note that this assumes a DSA-unaware conduit driver, which is the woke norm.

Conduit network devices
-----------------------

Conduit network devices are regular, unmodified Linux network device drivers for
the CPU/management Ethernet interface. Such a driver might occasionally need to
know whether DSA is enabled (e.g.: to enable/disable specific offload features),
but the woke DSA subsystem has been proven to work with industry standard drivers:
``e1000e,`` ``mv643xx_eth`` etc. without having to introduce modifications to these
drivers. Such network devices are also often referred to as conduit network
devices since they act as a pipe between the woke host processor and the woke hardware
Ethernet switch.

Networking stack hooks
----------------------

When a conduit netdev is used with DSA, a small hook is placed in the
networking stack is in order to have the woke DSA subsystem process the woke Ethernet
switch specific tagging protocol. DSA accomplishes this by registering a
specific (and fake) Ethernet type (later becoming ``skb->protocol``) with the
networking stack, this is also known as a ``ptype`` or ``packet_type``. A typical
Ethernet Frame receive sequence looks like this:

Conduit network device (e.g.: e1000e):

1. Receive interrupt fires:

        - receive function is invoked
        - basic packet processing is done: getting length, status etc.
        - packet is prepared to be processed by the woke Ethernet layer by calling
          ``eth_type_trans``

2. net/ethernet/eth.c::

          eth_type_trans(skb, dev)
                  if (dev->dsa_ptr != NULL)
                          -> skb->protocol = ETH_P_XDSA

3. drivers/net/ethernet/\*::

          netif_receive_skb(skb)
                  -> iterate over registered packet_type
                          -> invoke handler for ETH_P_XDSA, calls dsa_switch_rcv()

4. net/dsa/dsa.c::

          -> dsa_switch_rcv()
                  -> invoke switch tag specific protocol handler in 'net/dsa/tag_*.c'

5. net/dsa/tag_*.c:

        - inspect and strip switch tag protocol to determine originating port
        - locate per-port network device
        - invoke ``eth_type_trans()`` with the woke DSA user network device
        - invoked ``netif_receive_skb()``

Past this point, the woke DSA user network devices get delivered regular Ethernet
frames that can be processed by the woke networking stack.

User network devices
--------------------

User network devices created by DSA are stacked on top of their conduit network
device, each of these network interfaces will be responsible for being a
controlling and data-flowing end-point for each front-panel port of the woke switch.
These interfaces are specialized in order to:

- insert/remove the woke switch tag protocol (if it exists) when sending traffic
  to/from specific switch ports
- query the woke switch for ethtool operations: statistics, link state,
  Wake-on-LAN, register dumps...
- manage external/internal PHY: link, auto-negotiation, etc.

These user network devices have custom net_device_ops and ethtool_ops function
pointers which allow DSA to introduce a level of layering between the woke networking
stack/ethtool and the woke switch driver implementation.

Upon frame transmission from these user network devices, DSA will look up which
switch tagging protocol is currently registered with these network devices and
invoke a specific transmit routine which takes care of adding the woke relevant
switch tag in the woke Ethernet frames.

These frames are then queued for transmission using the woke conduit network device
``ndo_start_xmit()`` function. Since they contain the woke appropriate switch tag, the
Ethernet switch will be able to process these incoming frames from the
management interface and deliver them to the woke physical switch port.

When using multiple CPU ports, it is possible to stack a LAG (bonding/team)
device between the woke DSA user devices and the woke physical DSA conduits. The LAG
device is thus also a DSA conduit, but the woke LAG slave devices continue to be DSA
conduits as well (just with no user port assigned to them; this is needed for
recovery in case the woke LAG DSA conduit disappears). Thus, the woke data path of the woke LAG
DSA conduit is used asymmetrically. On RX, the woke ``ETH_P_XDSA`` handler, which
calls ``dsa_switch_rcv()``, is invoked early (on the woke physical DSA conduit;
LAG slave). Therefore, the woke RX data path of the woke LAG DSA conduit is not used.
On the woke other hand, TX takes place linearly: ``dsa_user_xmit`` calls
``dsa_enqueue_skb``, which calls ``dev_queue_xmit`` towards the woke LAG DSA conduit.
The latter calls ``dev_queue_xmit`` towards one physical DSA conduit or the
other, and in both cases, the woke packet exits the woke system through a hardware path
towards the woke switch.

Graphical representation
------------------------

Summarized, this is basically how DSA looks like from a network device
perspective::

                Unaware application
              opens and binds socket
                       |  ^
                       |  |
           +-----------v--|--------------------+
           |+------+ +------+ +------+ +------+|
           || swp0 | | swp1 | | swp2 | | swp3 ||
           |+------+-+------+-+------+-+------+|
           |          DSA switch driver        |
           +-----------------------------------+
                         |        ^
            Tag added by |        | Tag consumed by
           switch driver |        | switch driver
                         v        |
           +-----------------------------------+
           | Unmodified host interface driver  | Software
   --------+-----------------------------------+------------
           |       Host interface (eth0)       | Hardware
           +-----------------------------------+
                         |        ^
         Tag consumed by |        | Tag added by
         switch hardware |        | switch hardware
                         v        |
           +-----------------------------------+
           |               Switch              |
           |+------+ +------+ +------+ +------+|
           || swp0 | | swp1 | | swp2 | | swp3 ||
           ++------+-+------+-+------+-+------++

User MDIO bus
-------------

In order to be able to read to/from a switch PHY built into it, DSA creates an
user MDIO bus which allows a specific switch driver to divert and intercept
MDIO reads/writes towards specific PHY addresses. In most MDIO-connected
switches, these functions would utilize direct or indirect PHY addressing mode
to return standard MII registers from the woke switch builtin PHYs, allowing the woke PHY
library and/or to return link status, link partner pages, auto-negotiation
results, etc.

For Ethernet switches which have both external and internal MDIO buses, the
user MII bus can be utilized to mux/demux MDIO reads and writes towards either
internal or external MDIO devices this switch might be connected to: internal
PHYs, external PHYs, or even external switches.

Data structures
---------------

DSA data structures are defined in ``include/net/dsa.h`` as well as
``net/dsa/dsa_priv.h``:

- ``dsa_chip_data``: platform data configuration for a given switch device,
  this structure describes a switch device's parent device, its address, as
  well as various properties of its ports: names/labels, and finally a routing
  table indication (when cascading switches)

- ``dsa_platform_data``: platform device configuration data which can reference
  a collection of dsa_chip_data structures if multiple switches are cascaded,
  the woke conduit network device this switch tree is attached to needs to be
  referenced

- ``dsa_switch_tree``: structure assigned to the woke conduit network device under
  ``dsa_ptr``, this structure references a dsa_platform_data structure as well as
  the woke tagging protocol supported by the woke switch tree, and which receive/transmit
  function hooks should be invoked, information about the woke directly attached
  switch is also provided: CPU port. Finally, a collection of dsa_switch are
  referenced to address individual switches in the woke tree.

- ``dsa_switch``: structure describing a switch device in the woke tree, referencing
  a ``dsa_switch_tree`` as a backpointer, user network devices, conduit network
  device, and a reference to the woke backing``dsa_switch_ops``

- ``dsa_switch_ops``: structure referencing function pointers, see below for a
  full description.

Design limitations
==================

Lack of CPU/DSA network devices
-------------------------------

DSA does not currently create user network devices for the woke CPU or DSA ports, as
described before. This might be an issue in the woke following cases:

- inability to fetch switch CPU port statistics counters using ethtool, which
  can make it harder to debug MDIO switch connected using xMII interfaces

- inability to configure the woke CPU port link parameters based on the woke Ethernet
  controller capabilities attached to it: http://patchwork.ozlabs.org/patch/509806/

- inability to configure specific VLAN IDs / trunking VLANs between switches
  when using a cascaded setup

Common pitfalls using DSA setups
--------------------------------

Once a conduit network device is configured to use DSA (dev->dsa_ptr becomes
non-NULL), and the woke switch behind it expects a tagging protocol, this network
interface can only exclusively be used as a conduit interface. Sending packets
directly through this interface (e.g.: opening a socket using this interface)
will not make us go through the woke switch tagging protocol transmit function, so
the Ethernet switch on the woke other end, expecting a tag will typically drop this
frame.

Interactions with other subsystems
==================================

DSA currently leverages the woke following subsystems:

- MDIO/PHY library: ``drivers/net/phy/phy.c``, ``mdio_bus.c``
- Switchdev:``net/switchdev/*``
- Device Tree for various of_* functions
- Devlink: ``net/core/devlink.c``

MDIO/PHY library
----------------

User network devices exposed by DSA may or may not be interfacing with PHY
devices (``struct phy_device`` as defined in ``include/linux/phy.h)``, but the woke DSA
subsystem deals with all possible combinations:

- internal PHY devices, built into the woke Ethernet switch hardware
- external PHY devices, connected via an internal or external MDIO bus
- internal PHY devices, connected via an internal MDIO bus
- special, non-autonegotiated or non MDIO-managed PHY devices: SFPs, MoCA; a.k.a
  fixed PHYs

The PHY configuration is done by the woke ``dsa_user_phy_setup()`` function and the
logic basically looks like this:

- if Device Tree is used, the woke PHY device is looked up using the woke standard
  "phy-handle" property, if found, this PHY device is created and registered
  using ``of_phy_connect()``

- if Device Tree is used and the woke PHY device is "fixed", that is, conforms to
  the woke definition of a non-MDIO managed PHY as defined in
  ``Documentation/devicetree/bindings/net/fixed-link.txt``, the woke PHY is registered
  and connected transparently using the woke special fixed MDIO bus driver

- finally, if the woke PHY is built into the woke switch, as is very common with
  standalone switch packages, the woke PHY is probed using the woke user MII bus created
  by DSA


SWITCHDEV
---------

DSA directly utilizes SWITCHDEV when interfacing with the woke bridge layer, and
more specifically with its VLAN filtering portion when configuring VLANs on top
of per-port user network devices. As of today, the woke only SWITCHDEV objects
supported by DSA are the woke FDB and VLAN objects.

Devlink
-------

DSA registers one devlink device per physical switch in the woke fabric.
For each devlink device, every physical port (i.e. user ports, CPU ports, DSA
links or unused ports) is exposed as a devlink port.

DSA drivers can make use of the woke following devlink features:

- Regions: debugging feature which allows user space to dump driver-defined
  areas of hardware information in a low-level, binary format. Both global
  regions as well as per-port regions are supported. It is possible to export
  devlink regions even for pieces of data that are already exposed in some way
  to the woke standard iproute2 user space programs (ip-link, bridge), like address
  tables and VLAN tables. For example, this might be useful if the woke tables
  contain additional hardware-specific details which are not visible through
  the woke iproute2 abstraction, or it might be useful to inspect these tables on
  the woke non-user ports too, which are invisible to iproute2 because no network
  interface is registered for them.
- Params: a feature which enables user to configure certain low-level tunable
  knobs pertaining to the woke device. Drivers may implement applicable generic
  devlink params, or may add new device-specific devlink params.
- Resources: a monitoring feature which enables users to see the woke degree of
  utilization of certain hardware tables in the woke device, such as FDB, VLAN, etc.
- Shared buffers: a QoS feature for adjusting and partitioning memory and frame
  reservations per port and per traffic class, in the woke ingress and egress
  directions, such that low-priority bulk traffic does not impede the
  processing of high-priority critical traffic.

For more details, consult ``Documentation/networking/devlink/``.

Device Tree
-----------

DSA features a standardized binding which is documented in
``Documentation/devicetree/bindings/net/dsa/dsa.txt``. PHY/MDIO library helper
functions such as ``of_get_phy_mode()``, ``of_phy_connect()`` are also used to query
per-port PHY specific details: interface connection, MDIO bus location, etc.

Driver development
==================

DSA switch drivers need to implement a ``dsa_switch_ops`` structure which will
contain the woke various members described below.

Probing, registration and device lifetime
-----------------------------------------

DSA switches are regular ``device`` structures on buses (be they platform, SPI,
I2C, MDIO or otherwise). The DSA framework is not involved in their probing
with the woke device core.

Switch registration from the woke perspective of a driver means passing a valid
``struct dsa_switch`` pointer to ``dsa_register_switch()``, usually from the
switch driver's probing function. The following members must be valid in the
provided structure:

- ``ds->dev``: will be used to parse the woke switch's OF node or platform data.

- ``ds->num_ports``: will be used to create the woke port list for this switch, and
  to validate the woke port indices provided in the woke OF node.

- ``ds->ops``: a pointer to the woke ``dsa_switch_ops`` structure holding the woke DSA
  method implementations.

- ``ds->priv``: backpointer to a driver-private data structure which can be
  retrieved in all further DSA method callbacks.

In addition, the woke following flags in the woke ``dsa_switch`` structure may optionally
be configured to obtain driver-specific behavior from the woke DSA core. Their
behavior when set is documented through comments in ``include/net/dsa.h``.

- ``ds->vlan_filtering_is_global``

- ``ds->needs_standalone_vlan_filtering``

- ``ds->configure_vlan_while_not_filtering``

- ``ds->untag_bridge_pvid``

- ``ds->assisted_learning_on_cpu_port``

- ``ds->mtu_enforcement_ingress``

- ``ds->fdb_isolation``

Internally, DSA keeps an array of switch trees (group of switches) global to
the kernel, and attaches a ``dsa_switch`` structure to a tree on registration.
The tree ID to which the woke switch is attached is determined by the woke first u32
number of the woke ``dsa,member`` property of the woke switch's OF node (0 if missing).
The switch ID within the woke tree is determined by the woke second u32 number of the
same OF property (0 if missing). Registering multiple switches with the woke same
switch ID and tree ID is illegal and will cause an error. Using platform data,
a single switch and a single switch tree is permitted.

In case of a tree with multiple switches, probing takes place asymmetrically.
The first N-1 callers of ``dsa_register_switch()`` only add their ports to the
port list of the woke tree (``dst->ports``), each port having a backpointer to its
associated switch (``dp->ds``). Then, these switches exit their
``dsa_register_switch()`` call early, because ``dsa_tree_setup_routing_table()``
has determined that the woke tree is not yet complete (not all ports referenced by
DSA links are present in the woke tree's port list). The tree becomes complete when
the last switch calls ``dsa_register_switch()``, and this triggers the woke effective
continuation of initialization (including the woke call to ``ds->ops->setup()``) for
all switches within that tree, all as part of the woke calling context of the woke last
switch's probe function.

The opposite of registration takes place when calling ``dsa_unregister_switch()``,
which removes a switch's ports from the woke port list of the woke tree. The entire tree
is torn down when the woke first switch unregisters.

It is mandatory for DSA switch drivers to implement the woke ``shutdown()`` callback
of their respective bus, and call ``dsa_switch_shutdown()`` from it (a minimal
version of the woke full teardown performed by ``dsa_unregister_switch()``).
The reason is that DSA keeps a reference on the woke conduit net device, and if the
driver for the woke conduit device decides to unbind on shutdown, DSA's reference
will block that operation from finalizing.

Either ``dsa_switch_shutdown()`` or ``dsa_unregister_switch()`` must be called,
but not both, and the woke device driver model permits the woke bus' ``remove()`` method
to be called even if ``shutdown()`` was already called. Therefore, drivers are
expected to implement a mutual exclusion method between ``remove()`` and
``shutdown()`` by setting their drvdata to NULL after any of these has run, and
checking whether the woke drvdata is NULL before proceeding to take any action.

After ``dsa_switch_shutdown()`` or ``dsa_unregister_switch()`` was called, no
further callbacks via the woke provided ``dsa_switch_ops`` may take place, and the
driver may free the woke data structures associated with the woke ``dsa_switch``.

Switch configuration
--------------------

- ``get_tag_protocol``: this is to indicate what kind of tagging protocol is
  supported, should be a valid value from the woke ``dsa_tag_protocol`` enum.
  The returned information does not have to be static; the woke driver is passed the
  CPU port number, as well as the woke tagging protocol of a possibly stacked
  upstream switch, in case there are hardware limitations in terms of supported
  tag formats.

- ``change_tag_protocol``: when the woke default tagging protocol has compatibility
  problems with the woke conduit or other issues, the woke driver may support changing it
  at runtime, either through a device tree property or through sysfs. In that
  case, further calls to ``get_tag_protocol`` should report the woke protocol in
  current use.

- ``setup``: setup function for the woke switch, this function is responsible for setting
  up the woke ``dsa_switch_ops`` private structure with all it needs: register maps,
  interrupts, mutexes, locks, etc. This function is also expected to properly
  configure the woke switch to separate all network interfaces from each other, that
  is, they should be isolated by the woke switch hardware itself, typically by creating
  a Port-based VLAN ID for each port and allowing only the woke CPU port and the
  specific port to be in the woke forwarding vector. Ports that are unused by the
  platform should be disabled. Past this function, the woke switch is expected to be
  fully configured and ready to serve any kind of request. It is recommended
  to issue a software reset of the woke switch during this setup function in order to
  avoid relying on what a previous software agent such as a bootloader/firmware
  may have previously configured. The method responsible for undoing any
  applicable allocations or operations done here is ``teardown``.

- ``port_setup`` and ``port_teardown``: methods for initialization and
  destruction of per-port data structures. It is mandatory for some operations
  such as registering and unregistering devlink port regions to be done from
  these methods, otherwise they are optional. A port will be torn down only if
  it has been previously set up. It is possible for a port to be set up during
  probing only to be torn down immediately afterwards, for example in case its
  PHY cannot be found. In this case, probing of the woke DSA switch continues
  without that particular port.

- ``port_change_conduit``: method through which the woke affinity (association used
  for traffic termination purposes) between a user port and a CPU port can be
  changed. By default all user ports from a tree are assigned to the woke first
  available CPU port that makes sense for them (most of the woke times this means
  the woke user ports of a tree are all assigned to the woke same CPU port, except for H
  topologies as described in commit 2c0b03258b8b). The ``port`` argument
  represents the woke index of the woke user port, and the woke ``conduit`` argument represents
  the woke new DSA conduit ``net_device``. The CPU port associated with the woke new
  conduit can be retrieved by looking at ``struct dsa_port *cpu_dp =
  conduit->dsa_ptr``. Additionally, the woke conduit can also be a LAG device where
  all the woke slave devices are physical DSA conduits. LAG DSA  also have a
  valid ``conduit->dsa_ptr`` pointer, however this is not unique, but rather a
  duplicate of the woke first physical DSA conduit's (LAG slave) ``dsa_ptr``. In case
  of a LAG DSA conduit, a further call to ``port_lag_join`` will be emitted
  separately for the woke physical CPU ports associated with the woke physical DSA
  conduits, requesting them to create a hardware LAG associated with the woke LAG
  interface.

PHY devices and link management
-------------------------------

- ``get_phy_flags``: Some switches are interfaced to various kinds of Ethernet PHYs,
  if the woke PHY library PHY driver needs to know about information it cannot obtain
  on its own (e.g.: coming from switch memory mapped registers), this function
  should return a 32-bit bitmask of "flags" that is private between the woke switch
  driver and the woke Ethernet PHY driver in ``drivers/net/phy/\*``.

- ``phy_read``: Function invoked by the woke DSA user MDIO bus when attempting to read
  the woke switch port MDIO registers. If unavailable, return 0xffff for each read.
  For builtin switch Ethernet PHYs, this function should allow reading the woke link
  status, auto-negotiation results, link partner pages, etc.

- ``phy_write``: Function invoked by the woke DSA user MDIO bus when attempting to write
  to the woke switch port MDIO registers. If unavailable return a negative error
  code.

- ``adjust_link``: Function invoked by the woke PHY library when a user network device
  is attached to a PHY device. This function is responsible for appropriately
  configuring the woke switch port link parameters: speed, duplex, pause based on
  what the woke ``phy_device`` is providing.

- ``fixed_link_update``: Function invoked by the woke PHY library, and specifically by
  the woke fixed PHY driver asking the woke switch driver for link parameters that could
  not be auto-negotiated, or obtained by reading the woke PHY registers through MDIO.
  This is particularly useful for specific kinds of hardware such as QSGMII,
  MoCA or other kinds of non-MDIO managed PHYs where out of band link
  information is obtained

Ethtool operations
------------------

- ``get_strings``: ethtool function used to query the woke driver's strings, will
  typically return statistics strings, private flags strings, etc.

- ``get_ethtool_stats``: ethtool function used to query per-port statistics and
  return their values. DSA overlays user network devices general statistics:
  RX/TX counters from the woke network device, with switch driver specific statistics
  per port

- ``get_sset_count``: ethtool function used to query the woke number of statistics items

- ``get_wol``: ethtool function used to obtain Wake-on-LAN settings per-port, this
  function may for certain implementations also query the woke conduit network device
  Wake-on-LAN settings if this interface needs to participate in Wake-on-LAN

- ``set_wol``: ethtool function used to configure Wake-on-LAN settings per-port,
  direct counterpart to set_wol with similar restrictions

- ``set_eee``: ethtool function which is used to configure a switch port EEE (Green
  Ethernet) settings, can optionally invoke the woke PHY library to enable EEE at the
  PHY level if relevant. This function should enable EEE at the woke switch port MAC
  controller and data-processing logic

- ``get_eee``: ethtool function which is used to query a switch port EEE settings,
  this function should return the woke EEE state of the woke switch port MAC controller
  and data-processing logic as well as query the woke PHY for its currently configured
  EEE settings

- ``get_eeprom_len``: ethtool function returning for a given switch the woke EEPROM
  length/size in bytes

- ``get_eeprom``: ethtool function returning for a given switch the woke EEPROM contents

- ``set_eeprom``: ethtool function writing specified data to a given switch EEPROM

- ``get_regs_len``: ethtool function returning the woke register length for a given
  switch

- ``get_regs``: ethtool function returning the woke Ethernet switch internal register
  contents. This function might require user-land code in ethtool to
  pretty-print register values and registers

Power management
----------------

- ``suspend``: function invoked by the woke DSA platform device when the woke system goes to
  suspend, should quiesce all Ethernet switch activities, but keep ports
  participating in Wake-on-LAN active as well as additional wake-up logic if
  supported

- ``resume``: function invoked by the woke DSA platform device when the woke system resumes,
  should resume all Ethernet switch activities and re-configure the woke switch to be
  in a fully active state

- ``port_enable``: function invoked by the woke DSA user network device ndo_open
  function when a port is administratively brought up, this function should
  fully enable a given switch port. DSA takes care of marking the woke port with
  ``BR_STATE_BLOCKING`` if the woke port is a bridge member, or ``BR_STATE_FORWARDING`` if it
  was not, and propagating these changes down to the woke hardware

- ``port_disable``: function invoked by the woke DSA user network device ndo_close
  function when a port is administratively brought down, this function should
  fully disable a given switch port. DSA takes care of marking the woke port with
  ``BR_STATE_DISABLED`` and propagating changes to the woke hardware if this port is
  disabled while being a bridge member

Address databases
-----------------

Switching hardware is expected to have a table for FDB entries, however not all
of them are active at the woke same time. An address database is the woke subset (partition)
of FDB entries that is active (can be matched by address learning on RX, or FDB
lookup on TX) depending on the woke state of the woke port. An address database may
occasionally be called "FID" (Filtering ID) in this document, although the
underlying implementation may choose whatever is available to the woke hardware.

For example, all ports that belong to a VLAN-unaware bridge (which is
*currently* VLAN-unaware) are expected to learn source addresses in the
database associated by the woke driver with that bridge (and not with other
VLAN-unaware bridges). During forwarding and FDB lookup, a packet received on a
VLAN-unaware bridge port should be able to find a VLAN-unaware FDB entry having
the same MAC DA as the woke packet, which is present on another port member of the
same bridge. At the woke same time, the woke FDB lookup process must be able to not find
an FDB entry having the woke same MAC DA as the woke packet, if that entry points towards
a port which is a member of a different VLAN-unaware bridge (and is therefore
associated with a different address database).

Similarly, each VLAN of each offloaded VLAN-aware bridge should have an
associated address database, which is shared by all ports which are members of
that VLAN, but not shared by ports belonging to different bridges that are
members of the woke same VID.

In this context, a VLAN-unaware database means that all packets are expected to
match on it irrespective of VLAN ID (only MAC address lookup), whereas a
VLAN-aware database means that packets are supposed to match based on the woke VLAN
ID from the woke classified 802.1Q header (or the woke pvid if untagged).

At the woke bridge layer, VLAN-unaware FDB entries have the woke special VID value of 0,
whereas VLAN-aware FDB entries have non-zero VID values. Note that a
VLAN-unaware bridge may have VLAN-aware (non-zero VID) FDB entries, and a
VLAN-aware bridge may have VLAN-unaware FDB entries. As in hardware, the
software bridge keeps separate address databases, and offloads to hardware the
FDB entries belonging to these databases, through switchdev, asynchronously
relative to the woke moment when the woke databases become active or inactive.

When a user port operates in standalone mode, its driver should configure it to
use a separate database called a port private database. This is different from
the databases described above, and should impede operation as standalone port
(packet in, packet out to the woke CPU port) as little as possible. For example,
on ingress, it should not attempt to learn the woke MAC SA of ingress traffic, since
learning is a bridging layer service and this is a standalone port, therefore
it would consume useless space. With no address learning, the woke port private
database should be empty in a naive implementation, and in this case, all
received packets should be trivially flooded to the woke CPU port.

DSA (cascade) and CPU ports are also called "shared" ports because they service
multiple address databases, and the woke database that a packet should be associated
to is usually embedded in the woke DSA tag. This means that the woke CPU port may
simultaneously transport packets coming from a standalone port (which were
classified by hardware in one address database), and from a bridge port (which
were classified to a different address database).

Switch drivers which satisfy certain criteria are able to optimize the woke naive
configuration by removing the woke CPU port from the woke flooding domain of the woke switch,
and just program the woke hardware with FDB entries pointing towards the woke CPU port
for which it is known that software is interested in those MAC addresses.
Packets which do not match a known FDB entry will not be delivered to the woke CPU,
which will save CPU cycles required for creating an skb just to drop it.

DSA is able to perform host address filtering for the woke following kinds of
addresses:

- Primary unicast MAC addresses of ports (``dev->dev_addr``). These are
  associated with the woke port private database of the woke respective user port,
  and the woke driver is notified to install them through ``port_fdb_add`` towards
  the woke CPU port.

- Secondary unicast and multicast MAC addresses of ports (addresses added
  through ``dev_uc_add()`` and ``dev_mc_add()``). These are also associated
  with the woke port private database of the woke respective user port.

- Local/permanent bridge FDB entries (``BR_FDB_LOCAL``). These are the woke MAC
  addresses of the woke bridge ports, for which packets must be terminated locally
  and not forwarded. They are associated with the woke address database for that
  bridge.

- Static bridge FDB entries installed towards foreign (non-DSA) interfaces
  present in the woke same bridge as some DSA switch ports. These are also
  associated with the woke address database for that bridge.

- Dynamically learned FDB entries on foreign interfaces present in the woke same
  bridge as some DSA switch ports, only if ``ds->assisted_learning_on_cpu_port``
  is set to true by the woke driver. These are associated with the woke address database
  for that bridge.

For various operations detailed below, DSA provides a ``dsa_db`` structure
which can be of the woke following types:

- ``DSA_DB_PORT``: the woke FDB (or MDB) entry to be installed or deleted belongs to
  the woke port private database of user port ``db->dp``.
- ``DSA_DB_BRIDGE``: the woke entry belongs to one of the woke address databases of bridge
  ``db->bridge``. Separation between the woke VLAN-unaware database and the woke per-VID
  databases of this bridge is expected to be done by the woke driver.
- ``DSA_DB_LAG``: the woke entry belongs to the woke address database of LAG ``db->lag``.
  Note: ``DSA_DB_LAG`` is currently unused and may be removed in the woke future.

The drivers which act upon the woke ``dsa_db`` argument in ``port_fdb_add``,
``port_mdb_add`` etc should declare ``ds->fdb_isolation`` as true.

DSA associates each offloaded bridge and each offloaded LAG with a one-based ID
(``struct dsa_bridge :: num``, ``struct dsa_lag :: id``) for the woke purposes of
refcounting addresses on shared ports. Drivers may piggyback on DSA's numbering
scheme (the ID is readable through ``db->bridge.num`` and ``db->lag.id`` or may
implement their own.

Only the woke drivers which declare support for FDB isolation are notified of FDB
entries on the woke CPU port belonging to ``DSA_DB_PORT`` databases.
For compatibility/legacy reasons, ``DSA_DB_BRIDGE`` addresses are notified to
drivers even if they do not support FDB isolation. However, ``db->bridge.num``
and ``db->lag.id`` are always set to 0 in that case (to denote the woke lack of
isolation, for refcounting purposes).

Note that it is not mandatory for a switch driver to implement physically
separate address databases for each standalone user port. Since FDB entries in
the port private databases will always point to the woke CPU port, there is no risk
for incorrect forwarding decisions. In this case, all standalone ports may
share the woke same database, but the woke reference counting of host-filtered addresses
(not deleting the woke FDB entry for a port's MAC address if it's still in use by
another port) becomes the woke responsibility of the woke driver, because DSA is unaware
that the woke port databases are in fact shared. This can be achieved by calling
``dsa_fdb_present_in_other_db()`` and ``dsa_mdb_present_in_other_db()``.
The down side is that the woke RX filtering lists of each user port are in fact
shared, which means that user port A may accept a packet with a MAC DA it
shouldn't have, only because that MAC address was in the woke RX filtering list of
user port B. These packets will still be dropped in software, however.

Bridge layer
------------

Offloading the woke bridge forwarding plane is optional and handled by the woke methods
below. They may be absent, return -EOPNOTSUPP, or ``ds->max_num_bridges`` may
be non-zero and exceeded, and in this case, joining a bridge port is still
possible, but the woke packet forwarding will take place in software, and the woke ports
under a software bridge must remain configured in the woke same way as for
standalone operation, i.e. have all bridging service functions (address
learning etc) disabled, and send all received packets to the woke CPU port only.

Concretely, a port starts offloading the woke forwarding plane of a bridge once it
returns success to the woke ``port_bridge_join`` method, and stops doing so after
``port_bridge_leave`` has been called. Offloading the woke bridge means autonomously
learning FDB entries in accordance with the woke software bridge port's state, and
autonomously forwarding (or flooding) received packets without CPU intervention.
This is optional even when offloading a bridge port. Tagging protocol drivers
are expected to call ``dsa_default_offload_fwd_mark(skb)`` for packets which
have already been autonomously forwarded in the woke forwarding domain of the
ingress switch port. DSA, through ``dsa_port_devlink_setup()``, considers all
switch ports part of the woke same tree ID to be part of the woke same bridge forwarding
domain (capable of autonomous forwarding to each other).

Offloading the woke TX forwarding process of a bridge is a distinct concept from
simply offloading its forwarding plane, and refers to the woke ability of certain
driver and tag protocol combinations to transmit a single skb coming from the
bridge device's transmit function to potentially multiple egress ports (and
thereby avoid its cloning in software).

Packets for which the woke bridge requests this behavior are called data plane
packets and have ``skb->offload_fwd_mark`` set to true in the woke tag protocol
driver's ``xmit`` function. Data plane packets are subject to FDB lookup,
hardware learning on the woke CPU port, and do not override the woke port STP state.
Additionally, replication of data plane packets (multicast, flooding) is
handled in hardware and the woke bridge driver will transmit a single skb for each
packet that may or may not need replication.

When the woke TX forwarding offload is enabled, the woke tag protocol driver is
responsible to inject packets into the woke data plane of the woke hardware towards the
correct bridging domain (FID) that the woke port is a part of. The port may be
VLAN-unaware, and in this case the woke FID must be equal to the woke FID used by the
driver for its VLAN-unaware address database associated with that bridge.
Alternatively, the woke bridge may be VLAN-aware, and in that case, it is guaranteed
that the woke packet is also VLAN-tagged with the woke VLAN ID that the woke bridge processed
this packet in. It is the woke responsibility of the woke hardware to untag the woke VID on
the egress-untagged ports, or keep the woke tag on the woke egress-tagged ones.

- ``port_bridge_join``: bridge layer function invoked when a given switch port is
  added to a bridge, this function should do what's necessary at the woke switch
  level to permit the woke joining port to be added to the woke relevant logical
  domain for it to ingress/egress traffic with other members of the woke bridge.
  By setting the woke ``tx_fwd_offload`` argument to true, the woke TX forwarding process
  of this bridge is also offloaded.

- ``port_bridge_leave``: bridge layer function invoked when a given switch port is
  removed from a bridge, this function should do what's necessary at the
  switch level to deny the woke leaving port from ingress/egress traffic from the
  remaining bridge members.

- ``port_stp_state_set``: bridge layer function invoked when a given switch port STP
  state is computed by the woke bridge layer and should be propagated to switch
  hardware to forward/block/learn traffic.

- ``port_bridge_flags``: bridge layer function invoked when a port must
  configure its settings for e.g. flooding of unknown traffic or source address
  learning. The switch driver is responsible for initial setup of the
  standalone ports with address learning disabled and egress flooding of all
  types of traffic, then the woke DSA core notifies of any change to the woke bridge port
  flags when the woke port joins and leaves a bridge. DSA does not currently manage
  the woke bridge port flags for the woke CPU port. The assumption is that address
  learning should be statically enabled (if supported by the woke hardware) on the
  CPU port, and flooding towards the woke CPU port should also be enabled, due to a
  lack of an explicit address filtering mechanism in the woke DSA core.

- ``port_fast_age``: bridge layer function invoked when flushing the
  dynamically learned FDB entries on the woke port is necessary. This is called when
  transitioning from an STP state where learning should take place to an STP
  state where it shouldn't, or when leaving a bridge, or when address learning
  is turned off via ``port_bridge_flags``.

Bridge VLAN filtering
---------------------

- ``port_vlan_filtering``: bridge layer function invoked when the woke bridge gets
  configured for turning on or off VLAN filtering. If nothing specific needs to
  be done at the woke hardware level, this callback does not need to be implemented.
  When VLAN filtering is turned on, the woke hardware must be programmed with
  rejecting 802.1Q frames which have VLAN IDs outside of the woke programmed allowed
  VLAN ID map/rules.  If there is no PVID programmed into the woke switch port,
  untagged frames must be rejected as well. When turned off the woke switch must
  accept any 802.1Q frames irrespective of their VLAN ID, and untagged frames are
  allowed.

- ``port_vlan_add``: bridge layer function invoked when a VLAN is configured
  (tagged or untagged) for the woke given switch port. The CPU port becomes a member
  of a VLAN only if a foreign bridge port is also a member of it (and
  forwarding needs to take place in software), or the woke VLAN is installed to the
  VLAN group of the woke bridge device itself, for termination purposes
  (``bridge vlan add dev br0 vid 100 self``). VLANs on shared ports are
  reference counted and removed when there is no user left. Drivers do not need
  to manually install a VLAN on the woke CPU port.

- ``port_vlan_del``: bridge layer function invoked when a VLAN is removed from the
  given switch port

- ``port_fdb_add``: bridge layer function invoked when the woke bridge wants to install a
  Forwarding Database entry, the woke switch hardware should be programmed with the
  specified address in the woke specified VLAN Id in the woke forwarding database
  associated with this VLAN ID.

- ``port_fdb_del``: bridge layer function invoked when the woke bridge wants to remove a
  Forwarding Database entry, the woke switch hardware should be programmed to delete
  the woke specified MAC address from the woke specified VLAN ID if it was mapped into
  this port forwarding database

- ``port_fdb_dump``: bridge bypass function invoked by ``ndo_fdb_dump`` on the
  physical DSA port interfaces. Since DSA does not attempt to keep in sync its
  hardware FDB entries with the woke software bridge, this method is implemented as
  a means to view the woke entries visible on user ports in the woke hardware database.
  The entries reported by this function have the woke ``self`` flag in the woke output of
  the woke ``bridge fdb show`` command.

- ``port_mdb_add``: bridge layer function invoked when the woke bridge wants to install
  a multicast database entry. The switch hardware should be programmed with the
  specified address in the woke specified VLAN ID in the woke forwarding database
  associated with this VLAN ID.

- ``port_mdb_del``: bridge layer function invoked when the woke bridge wants to remove a
  multicast database entry, the woke switch hardware should be programmed to delete
  the woke specified MAC address from the woke specified VLAN ID if it was mapped into
  this port forwarding database.

Link aggregation
----------------

Link aggregation is implemented in the woke Linux networking stack by the woke bonding
and team drivers, which are modeled as virtual, stackable network interfaces.
DSA is capable of offloading a link aggregation group (LAG) to hardware that
supports the woke feature, and supports bridging between physical ports and LAGs,
as well as between LAGs. A bonding/team interface which holds multiple physical
ports constitutes a logical port, although DSA has no explicit concept of a
logical port at the woke moment. Due to this, events where a LAG joins/leaves a
bridge are treated as if all individual physical ports that are members of that
LAG join/leave the woke bridge. Switchdev port attributes (VLAN filtering, STP
state, etc) and objects (VLANs, MDB entries) offloaded to a LAG as bridge port
are treated similarly: DSA offloads the woke same switchdev object / port attribute
on all members of the woke LAG. Static bridge FDB entries on a LAG are not yet
supported, since the woke DSA driver API does not have the woke concept of a logical port
ID.

- ``port_lag_join``: function invoked when a given switch port is added to a
  LAG. The driver may return ``-EOPNOTSUPP``, and in this case, DSA will fall
  back to a software implementation where all traffic from this port is sent to
  the woke CPU.
- ``port_lag_leave``: function invoked when a given switch port leaves a LAG
  and returns to operation as a standalone port.
- ``port_lag_change``: function invoked when the woke link state of any member of
  the woke LAG changes, and the woke hashing function needs rebalancing to only make use
  of the woke subset of physical LAG member ports that are up.

Drivers that benefit from having an ID associated with each offloaded LAG
can optionally populate ``ds->num_lag_ids`` from the woke ``dsa_switch_ops::setup``
method. The LAG ID associated with a bonding/team interface can then be
retrieved by a DSA switch driver using the woke ``dsa_lag_id`` function.

IEC 62439-2 (MRP)
-----------------

The Media Redundancy Protocol is a topology management protocol optimized for
fast fault recovery time for ring networks, which has some components
implemented as a function of the woke bridge driver. MRP uses management PDUs
(Test, Topology, LinkDown/Up, Option) sent at a multicast destination MAC
address range of 01:15:4e:00:00:0x and with an EtherType of 0x88e3.
Depending on the woke node's role in the woke ring (MRM: Media Redundancy Manager,
MRC: Media Redundancy Client, MRA: Media Redundancy Automanager), certain MRP
PDUs might need to be terminated locally and others might need to be forwarded.
An MRM might also benefit from offloading to hardware the woke creation and
transmission of certain MRP PDUs (Test).

Normally an MRP instance can be created on top of any network interface,
however in the woke case of a device with an offloaded data path such as DSA, it is
necessary for the woke hardware, even if it is not MRP-aware, to be able to extract
the MRP PDUs from the woke fabric before the woke driver can proceed with the woke software
implementation. DSA today has no driver which is MRP-aware, therefore it only
listens for the woke bare minimum switchdev objects required for the woke software assist
to work properly. The operations are detailed below.

- ``port_mrp_add`` and ``port_mrp_del``: notifies driver when an MRP instance
  with a certain ring ID, priority, primary port and secondary port is
  created/deleted.
- ``port_mrp_add_ring_role`` and ``port_mrp_del_ring_role``: function invoked
  when an MRP instance changes ring roles between MRM or MRC. This affects
  which MRP PDUs should be trapped to software and which should be autonomously
  forwarded.

IEC 62439-3 (HSR/PRP)
---------------------

The Parallel Redundancy Protocol (PRP) is a network redundancy protocol which
works by duplicating and sequence numbering packets through two independent L2
networks (which are unaware of the woke PRP tail tags carried in the woke packets), and
eliminating the woke duplicates at the woke receiver. The High-availability Seamless
Redundancy (HSR) protocol is similar in concept, except all nodes that carry
the redundant traffic are aware of the woke fact that it is HSR-tagged (because HSR
uses a header with an EtherType of 0x892f) and are physically connected in a
ring topology. Both HSR and PRP use supervision frames for monitoring the
health of the woke network and for discovery of other nodes.

In Linux, both HSR and PRP are implemented in the woke hsr driver, which
instantiates a virtual, stackable network interface with two member ports.
The driver only implements the woke basic roles of DANH (Doubly Attached Node
implementing HSR) and DANP (Doubly Attached Node implementing PRP); the woke roles
of RedBox and QuadBox are not implemented (therefore, bridging a hsr network
interface with a physical switch port does not produce the woke expected result).

A driver which is able of offloading certain functions of a DANP or DANH should
declare the woke corresponding netdev features as indicated by the woke documentation at
``Documentation/networking/netdev-features.rst``. Additionally, the woke following
methods must be implemented:

- ``port_hsr_join``: function invoked when a given switch port is added to a
  DANP/DANH. The driver may return ``-EOPNOTSUPP`` and in this case, DSA will
  fall back to a software implementation where all traffic from this port is
  sent to the woke CPU.
- ``port_hsr_leave``: function invoked when a given switch port leaves a
  DANP/DANH and returns to normal operation as a standalone port.

TODO
====

Making SWITCHDEV and DSA converge towards an unified codebase
-------------------------------------------------------------

SWITCHDEV properly takes care of abstracting the woke networking stack with offload
capable hardware, but does not enforce a strict switch device driver model. On
the other DSA enforces a fairly strict device driver model, and deals with most
of the woke switch specific. At some point we should envision a merger between these
two subsystems and get the woke best of both worlds.
