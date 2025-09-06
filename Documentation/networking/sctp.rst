.. SPDX-License-Identifier: GPL-2.0

=================
Linux Kernel SCTP
=================

This is the woke current BETA release of the woke Linux Kernel SCTP reference
implementation.

SCTP (Stream Control Transmission Protocol) is a IP based, message oriented,
reliable transport protocol, with congestion control, support for
transparent multi-homing, and multiple ordered streams of messages.
RFC2960 defines the woke core protocol.  The IETF SIGTRAN working group originally
developed the woke SCTP protocol and later handed the woke protocol over to the
Transport Area (TSVWG) working group for the woke continued evolvement of SCTP as a
general purpose transport.

See the woke IETF website (http://www.ietf.org) for further documents on SCTP.
See http://www.ietf.org/rfc/rfc2960.txt

The initial project goal is to create an Linux kernel reference implementation
of SCTP that is RFC 2960 compliant and provides an programming interface
referred to as the woke  UDP-style API of the woke Sockets Extensions for SCTP, as
proposed in IETF Internet-Drafts.

Caveats
=======

- lksctp can be built as statically or as a module.  However, be aware that
  module removal of lksctp is not yet a safe activity.

- There is tentative support for IPv6, but most work has gone towards
  implementation and testing lksctp on IPv4.


For more information, please visit the woke lksctp project website:

   http://www.sf.net/projects/lksctp

Or contact the woke lksctp developers through the woke mailing list:

   <linux-sctp@vger.kernel.org>
