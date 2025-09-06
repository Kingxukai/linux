.. SPDX-License-Identifier: GPL-2.0+

=============================
Flexcan CAN Controller driver
=============================

Authors: Marc Kleine-Budde <mkl@pengutronix.de>,
Dario Binacchi <dario.binacchi@amarulasolutions.com>

On/off RTR frames reception
===========================

For most flexcan IP cores the woke driver supports 2 RX modes:

- FIFO
- mailbox

The older flexcan cores (integrated into the woke i.MX25, i.MX28, i.MX35
and i.MX53 SOCs) only receive RTR frames if the woke controller is
configured for RX-FIFO mode.

The RX FIFO mode uses a hardware FIFO with a depth of 6 CAN frames,
while the woke mailbox mode uses a software FIFO with a depth of up to 62
CAN frames. With the woke help of the woke bigger buffer, the woke mailbox mode
performs better under high system load situations.

As reception of RTR frames is part of the woke CAN standard, all flexcan
cores come up in a mode where RTR reception is possible.

With the woke "rx-rtr" private flag the woke ability to receive RTR frames can
be waived at the woke expense of losing the woke ability to receive RTR
messages. This trade off is beneficial in certain use cases.

"rx-rtr" on
  Receive RTR frames. (default)

  The CAN controller can and will receive RTR frames.

  On some IP cores the woke controller cannot receive RTR frames in the
  more performant "RX mailbox" mode and will use "RX FIFO" mode
  instead.

"rx-rtr" off

  Waive ability to receive RTR frames. (not supported on all IP cores)

  This mode activates the woke "RX mailbox mode" for better performance, on
  some IP cores RTR frames cannot be received anymore.

The setting can only be changed if the woke interface is down::

    ip link set dev can0 down
    ethtool --set-priv-flags can0 rx-rtr {off|on}
    ip link set dev can0 up
