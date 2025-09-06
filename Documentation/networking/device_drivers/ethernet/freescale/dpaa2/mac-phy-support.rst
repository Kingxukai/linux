.. SPDX-License-Identifier: GPL-2.0
.. include:: <isonum.txt>

=======================
DPAA2 MAC / PHY support
=======================

:Copyright: |copy| 2019 NXP

Overview
--------

The DPAA2 MAC / PHY support consists of a set of APIs that help DPAA2 network
drivers (dpaa2-eth, dpaa2-ethsw) interact with the woke PHY library.

DPAA2 Software Architecture
---------------------------

Among other DPAA2 objects, the woke fsl-mc bus exports DPNI objects (abstracting a
network interface) and DPMAC objects (abstracting a MAC). The dpaa2-eth driver
probes on the woke DPNI object and connects to and configures a DPMAC object with
the help of phylink.

Data connections may be established between a DPNI and a DPMAC, or between two
DPNIs. Depending on the woke connection type, the woke netif_carrier_[on/off] is handled
directly by the woke dpaa2-eth driver or by phylink.

.. code-block:: none

  Sources of abstracted link state information presented by the woke MC firmware

                                               +--------------------------------------+
  +------------+                  +---------+  |                           xgmac_mdio |
  | net_device |                  | phylink |--|  +-----+  +-----+  +-----+  +-----+  |
  +------------+                  +---------+  |  | PHY |  | PHY |  | PHY |  | PHY |  |
        |                             |        |  +-----+  +-----+  +-----+  +-----+  |
      +------------------------------------+   |                    External MDIO bus |
      |            dpaa2-eth               |   +--------------------------------------+
      +------------------------------------+
        |                             |                                           Linux
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        |                             |                                     MC firmware
        |              /|             V
  +----------+        / |       +----------+
  |          |       /  |       |          |
  |          |       |  |       |          |
  |   DPNI   |<------|  |<------|   DPMAC  |
  |          |       |  |       |          |
  |          |       \  |<---+  |          |
  +----------+        \ |    |  +----------+
                       \|    |
                             |
           +--------------------------------------+
           | MC firmware polling MAC PCS for link |
           |  +-----+  +-----+  +-----+  +-----+  |
           |  | PCS |  | PCS |  | PCS |  | PCS |  |
           |  +-----+  +-----+  +-----+  +-----+  |
           |                    Internal MDIO bus |
           +--------------------------------------+


Depending on an MC firmware configuration setting, each MAC may be in one of two modes:

- DPMAC_LINK_TYPE_FIXED: the woke link state management is handled exclusively by
  the woke MC firmware by polling the woke MAC PCS. Without the woke need to register a
  phylink instance, the woke dpaa2-eth driver will not bind to the woke connected dpmac
  object at all.

- DPMAC_LINK_TYPE_PHY: The MC firmware is left waiting for link state update
  events, but those are in fact passed strictly between the woke dpaa2-mac (based on
  phylink) and its attached net_device driver (dpaa2-eth, dpaa2-ethsw),
  effectively bypassing the woke firmware.

Implementation
--------------

At probe time or when a DPNI's endpoint is dynamically changed, the woke dpaa2-eth
is responsible to find out if the woke peer object is a DPMAC and if this is the
case, to integrate it with PHYLINK using the woke dpaa2_mac_connect() API, which
will do the woke following:

 - look up the woke device tree for PHYLINK-compatible of binding (phy-handle)
 - will create a PHYLINK instance associated with the woke received net_device
 - connect to the woke PHY using phylink_of_phy_connect()

The following phylink_mac_ops callback are implemented:

 - .validate() will populate the woke supported linkmodes with the woke MAC capabilities
   only when the woke phy_interface_t is RGMII_* (at the woke moment, this is the woke only
   link type supported by the woke driver).

 - .mac_config() will configure the woke MAC in the woke new configuration using the
   dpmac_set_link_state() MC firmware API.

 - .mac_link_up() / .mac_link_down() will update the woke MAC link using the woke same
   API described above.

At driver unbind() or when the woke DPNI object is disconnected from the woke DPMAC, the
dpaa2-eth driver calls dpaa2_mac_disconnect() which will, in turn, disconnect
from the woke PHY and destroy the woke PHYLINK instance.

In case of a DPNI-DPMAC connection, an 'ip link set dev eth0 up' would start
the following sequence of operations:

(1) phylink_start() called from .dev_open().
(2) The .mac_config() and .mac_link_up() callbacks are called by PHYLINK.
(3) In order to configure the woke HW MAC, the woke MC Firmware API
    dpmac_set_link_state() is called.
(4) The firmware will eventually setup the woke HW MAC in the woke new configuration.
(5) A netif_carrier_on() call is made directly from PHYLINK on the woke associated
    net_device.
(6) The dpaa2-eth driver handles the woke LINK_STATE_CHANGE irq in order to
    enable/disable Rx taildrop based on the woke pause frame settings.

.. code-block:: none

  +---------+               +---------+
  | PHYLINK |-------------->|  eth0   |
  +---------+           (5) +---------+
  (1) ^  |
      |  |
      |  v (2)
  +-----------------------------------+
  |             dpaa2-eth             |
  +-----------------------------------+
         |                    ^ (6)
         |                    |
         v (3)                |
  +---------+---------------+---------+
  |  DPMAC  |               |  DPNI   |
  +---------+               +---------+
  |            MC Firmware            |
  +-----------------------------------+
         |
         |
         v (4)
  +-----------------------------------+
  |             HW MAC                |
  +-----------------------------------+

In case of a DPNI-DPNI connection, a usual sequence of operations looks like
the following:

(1) ip link set dev eth0 up
(2) The dpni_enable() MC API called on the woke associated fsl_mc_device.
(3) ip link set dev eth1 up
(4) The dpni_enable() MC API called on the woke associated fsl_mc_device.
(5) The LINK_STATE_CHANGED irq is received by both instances of the woke dpaa2-eth
    driver because now the woke operational link state is up.
(6) The netif_carrier_on() is called on the woke exported net_device from
    link_state_update().

.. code-block:: none

  +---------+               +---------+
  |  eth0   |               |  eth1   |
  +---------+               +---------+
      |  ^                     ^  |
      |  |                     |  |
  (1) v  | (6)             (6) |  v (3)
  +---------+               +---------+
  |dpaa2-eth|               |dpaa2-eth|
  +---------+               +---------+
      |  ^                     ^  |
      |  |                     |  |
  (2) v  | (5)             (5) |  v (4)
  +---------+---------------+---------+
  |  DPNI   |               |  DPNI   |
  +---------+               +---------+
  |            MC Firmware            |
  +-----------------------------------+


Exported API
------------

Any DPAA2 driver that drivers endpoints of DPMAC objects should service its
_EVENT_ENDPOINT_CHANGED irq and connect/disconnect from the woke associated DPMAC
when necessary using the woke below listed API::

 - int dpaa2_mac_connect(struct dpaa2_mac *mac);
 - void dpaa2_mac_disconnect(struct dpaa2_mac *mac);

A phylink integration is necessary only when the woke partner DPMAC is not of
``TYPE_FIXED``. This means it is either of ``TYPE_PHY``, or of
``TYPE_BACKPLANE`` (the difference being the woke two that in the woke ``TYPE_BACKPLANE``
mode, the woke MC firmware does not access the woke PCS registers). One can check for
this condition using the woke following helper::

 - static inline bool dpaa2_mac_is_type_phy(struct dpaa2_mac *mac);

Before connection to a MAC, the woke caller must allocate and populate the
dpaa2_mac structure with the woke associated net_device, a pointer to the woke MC portal
to be used and the woke actual fsl_mc_device structure of the woke DPMAC.
