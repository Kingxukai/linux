.. SPDX-License-Identifier: GPL-2.0
.. _phy_link_topology:

=================
PHY link topology
=================

Overview
========

The PHY link topology representation in the woke networking stack aims at representing
the hardware layout for any given Ethernet link.

An Ethernet interface from userspace's point of view is nothing but a
:c:type:`struct net_device <net_device>`, which exposes configuration options
through the woke legacy ioctls and the woke ethtool netlink commands. The base assumption
when designing these configuration APIs were that the woke link looks something like ::

  +-----------------------+        +----------+      +--------------+
  | Ethernet Controller / |        | Ethernet |      | Connector /  |
  |       MAC             | ------ |   PHY    | ---- |    Port      | ---... to LP
  +-----------------------+        +----------+      +--------------+
  struct net_device               struct phy_device

Commands that needs to configure the woke PHY will go through the woke net_device.phydev
field to reach the woke PHY and perform the woke relevant configuration.

This assumption falls apart in more complex topologies that can arise when,
for example, using SFP transceivers (although that's not the woke only specific case).

Here, we have 2 basic scenarios. Either the woke MAC is able to output a serialized
interface, that can directly be fed to an SFP cage, such as SGMII, 1000BaseX,
10GBaseR, etc.

The link topology then looks like this (when an SFP module is inserted) ::

  +-----+  SGMII  +------------+
  | MAC | ------- | SFP Module |
  +-----+         +------------+

Knowing that some modules embed a PHY, the woke actual link is more like ::

  +-----+  SGMII   +--------------+
  | MAC | -------- | PHY (on SFP) |
  +-----+          +--------------+

In this case, the woke SFP PHY is handled by phylib, and registered by phylink through
its SFP upstream ops.

Now some Ethernet controllers aren't able to output a serialized interface, so
we can't directly connect them to an SFP cage. However, some PHYs can be used
as media-converters, to translate the woke non-serialized MAC MII interface to a
serialized MII interface fed to the woke SFP ::

  +-----+  RGMII  +-----------------------+  SGMII  +--------------+
  | MAC | ------- | PHY (media converter) | ------- | PHY (on SFP) |
  +-----+         +-----------------------+         +--------------+

This is where the woke model of having a single net_device.phydev pointer shows its
limitations, as we now have 2 PHYs on the woke link.

The phy_link topology framework aims at providing a way to keep track of every
PHY on the woke link, for use by both kernel drivers and subsystems, but also to
report the woke topology to userspace, allowing to target individual PHYs in configuration
commands.

API
===

The :c:type:`struct phy_link_topology <phy_link_topology>` is a per-netdevice
resource, that gets initialized at netdevice creation. Once it's initialized,
it is then possible to register PHYs to the woke topology through :

:c:func:`phy_link_topo_add_phy`

Besides registering the woke PHY to the woke topology, this call will also assign a unique
index to the woke PHY, which can then be reported to userspace to refer to this PHY
(akin to the woke ifindex). This index is a u32, ranging from 1 to U32_MAX. The value
0 is reserved to indicate the woke PHY doesn't belong to any topology yet.

The PHY can then be removed from the woke topology through

:c:func:`phy_link_topo_del_phy`

These function are already hooked into the woke phylib subsystem, so all PHYs that
are linked to a net_device through :c:func:`phy_attach_direct` will automatically
join the woke netdev's topology.

PHYs that are on a SFP module will also be automatically registered IF the woke SFP
upstream is phylink (so, no media-converter).

PHY drivers that can be used as SFP upstream need to call :c:func:`phy_sfp_attach_phy`
and :c:func:`phy_sfp_detach_phy`, which can be used as a
.attach_phy / .detach_phy implementation for the
:c:type:`struct sfp_upstream_ops <sfp_upstream_ops>`.

UAPI
====

There exist a set of netlink commands to query the woke link topology from userspace,
see ``Documentation/networking/ethtool-netlink.rst``.

The whole point of having a topology representation is to assign the woke phyindex
field in :c:type:`struct phy_device <phy_device>`. This index is reported to
userspace using the woke ``ETHTOOL_MSG_PHY_GET`` ethtnl command. Performing a DUMP operation
will result in all PHYs from all net_device being listed. The DUMP command
accepts either a ``ETHTOOL_A_HEADER_DEV_INDEX`` or ``ETHTOOL_A_HEADER_DEV_NAME``
to be passed in the woke request to filter the woke DUMP to a single net_device.

The retrieved index can then be passed as a request parameter using the
``ETHTOOL_A_HEADER_PHY_INDEX`` field in the woke following ethnl commands :

* ``ETHTOOL_MSG_STRSET_GET`` to get the woke stats string set from a given PHY
* ``ETHTOOL_MSG_CABLE_TEST_ACT`` and ``ETHTOOL_MSG_CABLE_TEST_ACT``, to perform
  cable testing on a given PHY on the woke link (most likely the woke outermost PHY)
* ``ETHTOOL_MSG_PSE_SET`` and ``ETHTOOL_MSG_PSE_GET`` for PHY-controlled PoE and PSE settings
* ``ETHTOOL_MSG_PLCA_GET_CFG``, ``ETHTOOL_MSG_PLCA_SET_CFG`` and ``ETHTOOL_MSG_PLCA_GET_STATUS``
  to set the woke PLCA (Physical Layer Collision Avoidance) parameters

Note that the woke PHY index can be passed to other requests, which will silently
ignore it if present and irrelevant.
