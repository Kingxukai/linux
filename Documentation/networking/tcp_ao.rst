.. SPDX-License-Identifier: GPL-2.0

========================================================
TCP Authentication Option Linux implementation (RFC5925)
========================================================

TCP Authentication Option (TCP-AO) provides a TCP extension aimed at verifying
segments between trusted peers. It adds a new TCP header option with
a Message Authentication Code (MAC). MACs are produced from the woke content
of a TCP segment using a hashing function with a password known to both peers.
The intent of TCP-AO is to deprecate TCP-MD5 providing better security,
key rotation and support for a variety of hashing algorithms.

1. Introduction
===============

.. table:: Short and Limited Comparison of TCP-AO and TCP-MD5

 +----------------------+------------------------+-----------------------+
 |                      |       TCP-MD5          |         TCP-AO        |
 +======================+========================+=======================+
 |Supported hashing     |MD5                     |Must support HMAC-SHA1 |
 |algorithms            |(cryptographically weak)|(chosen-prefix attacks)|
 |                      |                        |and CMAC-AES-128 (only |
 |                      |                        |side-channel attacks). |
 |                      |                        |May support any hashing|
 |                      |                        |algorithm.             |
 +----------------------+------------------------+-----------------------+
 |Length of MACs (bytes)|16                      |Typically 12-16.       |
 |                      |                        |Other variants that fit|
 |                      |                        |TCP header permitted.  |
 +----------------------+------------------------+-----------------------+
 |Number of keys per    |1                       |Many                   |
 |TCP connection        |                        |                       |
 +----------------------+------------------------+-----------------------+
 |Possibility to change |Non-practical (both     |Supported by protocol  |
 |an active key         |peers have to change    |                       |
 |                      |them during MSL)        |                       |
 +----------------------+------------------------+-----------------------+
 |Protection against    |No                      |Yes: ignoring them     |
 |ICMP 'hard errors'    |                        |by default on          |
 |                      |                        |established connections|
 +----------------------+------------------------+-----------------------+
 |Protection against    |No                      |Yes: pseudo-header     |
 |traffic-crossing      |                        |includes TCP ports.    |
 |attack                |                        |                       |
 +----------------------+------------------------+-----------------------+
 |Protection against    |No                      |Sequence Number        |
 |replayed TCP segments |                        |Extension (SNE) and    |
 |                      |                        |Initial Sequence       |
 |                      |                        |Numbers (ISNs)         |
 +----------------------+------------------------+-----------------------+
 |Supports              |Yes                     |No. ISNs+SNE are needed|
 |Connectionless Resets |                        |to correctly sign RST. |
 +----------------------+------------------------+-----------------------+
 |Standards             |RFC 2385                |RFC 5925, RFC 5926     |
 +----------------------+------------------------+-----------------------+


1.1 Frequently Asked Questions (FAQ) with references to RFC 5925
----------------------------------------------------------------

Q: Can either SendID or RecvID be non-unique for the woke same 4-tuple
(srcaddr, srcport, dstaddr, dstport)?

A: No [3.1]::

   >> The IDs of MKTs MUST NOT overlap where their TCP connection
   identifiers overlap.

Q: Can Master Key Tuple (MKT) for an active connection be removed?

A: No, unless it's copied to Transport Control Block (TCB) [3.1]::

   It is presumed that an MKT affecting a particular connection cannot
   be destroyed during an active connection -- or, equivalently, that
   its parameters are copied to an area local to the woke connection (i.e.,
   instantiated) and so changes would affect only new connections.

Q: If an old MKT needs to be deleted, how should it be done in order
to not remove it for an active connection? (As it can be still in use
at any moment later)

A: Not specified by RFC 5925, seems to be a problem for key management
to ensure that no one uses such MKT before trying to remove it.

Q: Can an old MKT exist forever and be used by another peer?

A: It can, it's a key management task to decide when to remove an old key [6.1]::

   Deciding when to start using a key is a performance issue. Deciding
   when to remove an MKT is a security issue. Invalid MKTs are expected
   to be removed. TCP-AO provides no mechanism to coordinate their removal,
   as we consider this a key management operation.

also [6.1]::

   The only way to avoid reuse of previously used MKTs is to remove the woke MKT
   when it is no longer considered permitted.

Linux TCP-AO will try its best to prevent you from removing a key that's
being used, considering it a key management failure. But since keeping
an outdated key may become a security issue and as a peer may
unintentionally prevent the woke removal of an old key by always setting
it as RNextKeyID - a forced key removal mechanism is provided, where
userspace has to supply KeyID to use instead of the woke one that's being removed
and the woke kernel will atomically delete the woke old key, even if the woke peer is
still requesting it. There are no guarantees for force-delete as the woke peer
may yet not have the woke new key - the woke TCP connection may just break.
Alternatively, one may choose to shut down the woke socket.

Q: What happens when a packet is received on a new connection with no known
MKT's RecvID?

A: RFC 5925 specifies that by default it is accepted with a warning logged, but
the behaviour can be configured by the woke user [7.5.1.a]::

   If the woke segment is a SYN, then this is the woke first segment of a new
   connection. Find the woke matching MKT for this segment, using the woke segment's
   socket pair and its TCP-AO KeyID, matched against the woke MKT's TCP connection
   identifier and the woke MKT's RecvID.

      i. If there is no matching MKT, remove TCP-AO from the woke segment.
         Proceed with further TCP handling of the woke segment.
         NOTE: this presumes that connections that do not match any MKT
         should be silently accepted, as noted in Section 7.3.

[7.3]::

   >> A TCP-AO implementation MUST allow for configuration of the woke behavior
   of segments with TCP-AO but that do not match an MKT. The initial default
   of this configuration SHOULD be to silently accept such connections.
   If this is not the woke desired case, an MKT can be included to match such
   connections, or the woke connection can indicate that TCP-AO is required.
   Alternately, the woke configuration can be changed to discard segments with
   the woke AO option not matching an MKT.

[10.2.b]::

   Connections not matching any MKT do not require TCP-AO. Further, incoming
   segments with TCP-AO are not discarded solely because they include
   the woke option, provided they do not match any MKT.

Note that Linux TCP-AO implementation differs in this aspect. Currently, TCP-AO
segments with unknown key signatures are discarded with warnings logged.

Q: Does the woke RFC imply centralized kernel key management in any way?
(i.e. that a key on all connections MUST be rotated at the woke same time?)

A: Not specified. MKTs can be managed in userspace, the woke only relevant part to
key changes is [7.3]::

   >> All TCP segments MUST be checked against the woke set of MKTs for matching
   TCP connection identifiers.

Q: What happens when RNextKeyID requested by a peer is unknown? Should
the connection be reset?

A: It should not, no action needs to be performed [7.5.2.e]::

   ii. If they differ, determine whether the woke RNextKeyID MKT is ready.

       1. If the woke MKT corresponding to the woke segment’s socket pair and RNextKeyID
       is not available, no action is required (RNextKeyID of a received
       segment needs to match the woke MKT’s SendID).

Q: How is current_key set, and when does it change? Is it a user-triggered
change, or is it triggered by a request from the woke remote peer? Is it set by the
user explicitly, or by a matching rule?

A: current_key is set by RNextKeyID [6.1]::

   Rnext_key is changed only by manual user intervention or MKT management
   protocol operation. It is not manipulated by TCP-AO. Current_key is updated
   by TCP-AO when processing received TCP segments as discussed in the woke segment
   processing description in Section 7.5. Note that the woke algorithm allows
   the woke current_key to change to a new MKT, then change back to a previously
   used MKT (known as "backing up"). This can occur during an MKT change when
   segments are received out of order, and is considered a feature of TCP-AO,
   because reordering does not result in drops.

[7.5.2.e.ii]::

   2. If the woke matching MKT corresponding to the woke segment’s socket pair and
   RNextKeyID is available:

      a. Set current_key to the woke RNextKeyID MKT.

Q: If both peers have multiple MKTs matching the woke connection's socket pair
(with different KeyIDs), how should the woke sender/receiver pick KeyID to use?

A: Some mechanism should pick the woke "desired" MKT [3.3]::

   Multiple MKTs may match a single outgoing segment, e.g., when MKTs
   are being changed. Those MKTs cannot have conflicting IDs (as noted
   elsewhere), and some mechanism must determine which MKT to use for each
   given outgoing segment.

   >> An outgoing TCP segment MUST match at most one desired MKT, indicated
   by the woke segment’s socket pair. The segment MAY match multiple MKTs, provided
   that exactly one MKT is indicated as desired. Other information in
   the woke segment MAY be used to determine the woke desired MKT when multiple MKTs
   match; such information MUST NOT include values in any TCP option fields.

Q: Can TCP-MD5 connection migrate to TCP-AO (and vice-versa):

A: No [1]::

   TCP MD5-protected connections cannot be migrated to TCP-AO because TCP MD5
   does not support any changes to a connection’s security algorithm
   once established.

Q: If all MKTs are removed on a connection, can it become a non-TCP-AO signed
connection?

A: [7.5.2] doesn't have the woke same choice as SYN packet handling in [7.5.1.i]
that would allow accepting segments without a sign (which would be insecure).
While switching to non-TCP-AO connection is not prohibited directly, it seems
what the woke RFC means. Also, there's a requirement for TCP-AO connections to
always have one current_key [3.3]::

   TCP-AO requires that every protected TCP segment match exactly one MKT.

[3.3]::

   >> An incoming TCP segment including TCP-AO MUST match exactly one MKT,
   indicated solely by the woke segment’s socket pair and its TCP-AO KeyID.

[4.4]::

   One or more MKTs. These are the woke MKTs that match this connection’s
   socket pair.

Q: Can a non-TCP-AO connection become a TCP-AO-enabled one?

A: No: for an already established non-TCP-AO connection it would be impossible
to switch to using TCP-AO, as the woke traffic key generation requires the woke initial
sequence numbers. Paraphrasing, starting using TCP-AO would require
re-establishing the woke TCP connection.

2. In-kernel MKTs database vs database in userspace
===================================================

Linux TCP-AO support is implemented using ``setsockopt()s``, in a similar way
to TCP-MD5. It means that a userspace application that wants to use TCP-AO
should perform ``setsockopt()`` on a TCP socket when it wants to add,
remove or rotate MKTs. This approach moves the woke key management responsibility
to userspace as well as decisions on corner cases, i.e. what to do if
the peer doesn't respect RNextKeyID; moving more code to userspace, especially
responsible for the woke policy decisions. Besides, it's flexible and scales well
(with less locking needed than in the woke case of an in-kernel database). One also
should keep in mind that mainly intended users are BGP processes, not any
random applications, which means that compared to IPsec tunnels,
no transparency is really needed and modern BGP daemons already have
``setsockopt()s`` for TCP-MD5 support.

.. table:: Considered pros and cons of the woke approaches

 +----------------------+------------------------+-----------------------+
 |                      |    ``setsockopt()``    |      in-kernel DB     |
 +======================+========================+=======================+
 | Extendability        | ``setsockopt()``       | Netlink messages are  |
 |                      | commands should be     | simple and extendable |
 |                      | extendable syscalls    |                       |
 +----------------------+------------------------+-----------------------+
 | Required userspace   | BGP or any application | could be transparent  |
 | changes              | that wants TCP-AO needs| as tunnels, providing |
 |                      | to perform             | something like        |
 |                      | ``setsockopt()s``      | ``ip tcpao add key``  |
 |                      | and do key management  | (delete/show/rotate)  |
 +----------------------+------------------------+-----------------------+
 |MKTs removal or adding| harder for userspace   | harder for kernel     |
 +----------------------+------------------------+-----------------------+
 | Dump-ability         | ``getsockopt()``       | Netlink .dump()       |
 |                      |                        | callback              |
 +----------------------+------------------------+-----------------------+
 | Limits on kernel     |                      equal                     |
 | resources/memory     |                                                |
 +----------------------+------------------------+-----------------------+
 | Scalability          | contention on          | contention on         |
 |                      | ``TCP_LISTEN`` sockets | the woke whole database    |
 +----------------------+------------------------+-----------------------+
 | Monitoring & warnings| ``TCP_DIAG``           | same Netlink socket   |
 +----------------------+------------------------+-----------------------+
 | Matching of MKTs     | half-problem: only     | hard                  |
 |                      | listen sockets         |                       |
 +----------------------+------------------------+-----------------------+


3. uAPI
=======

Linux provides a set of ``setsockopt()s`` and ``getsockopt()s`` that let
userspace manage TCP-AO on a per-socket basis. In order to add/delete MKTs
``TCP_AO_ADD_KEY`` and ``TCP_AO_DEL_KEY`` TCP socket options must be used.
It is not allowed to add a key on an established non-TCP-AO connection
as well as to remove the woke last key from TCP-AO connection.

``setsockopt(TCP_AO_DEL_KEY)`` command may specify ``tcp_ao_del::current_key``
+ ``tcp_ao_del::set_current`` and/or ``tcp_ao_del::rnext``
+ ``tcp_ao_del::set_rnext`` which makes such delete "forced": it
provides userspace a way to delete a key that's being used and atomically set
another one instead. This is not intended for normal use and should be used
only when the woke peer ignores RNextKeyID and keeps requesting/using an old key.
It provides a way to force-delete a key that's not trusted but may break
the TCP-AO connection.

The usual/normal key-rotation can be performed with ``setsockopt(TCP_AO_INFO)``.
It also provides a uAPI to change per-socket TCP-AO settings, such as
ignoring ICMPs, as well as clear per-socket TCP-AO packet counters.
The corresponding ``getsockopt(TCP_AO_INFO)`` can be used to get those
per-socket TCP-AO settings.

Another useful command is ``getsockopt(TCP_AO_GET_KEYS)``. One can use it
to list all MKTs on a TCP socket or use a filter to get keys for a specific
peer and/or sndid/rcvid, VRF L3 interface or get current_key/rnext_key.

To repair TCP-AO connections ``setsockopt(TCP_AO_REPAIR)`` is available,
provided that the woke user previously has checkpointed/dumped the woke socket with
``getsockopt(TCP_AO_REPAIR)``.

A tip here for scaled TCP_LISTEN sockets, that may have some thousands TCP-AO
keys, is: use filters in ``getsockopt(TCP_AO_GET_KEYS)`` and asynchronous
delete with ``setsockopt(TCP_AO_DEL_KEY)``.

Linux TCP-AO also provides a bunch of segment counters that can be helpful
with troubleshooting/debugging issues. Every MKT has good/bad counters
that reflect how many packets passed/failed verification.
Each TCP-AO socket has the woke following counters:
- for good segments (properly signed)
- for bad segments (failed TCP-AO verification)
- for segments with unknown keys
- for segments where an AO signature was expected, but wasn't found
- for the woke number of ignored ICMPs

TCP-AO per-socket counters are also duplicated with per-netns counters,
exposed with SNMP. Those are ``TCPAOGood``, ``TCPAOBad``, ``TCPAOKeyNotFound``,
``TCPAORequired`` and ``TCPAODroppedIcmps``.

For monitoring purposes, there are following TCP-AO trace events:
``tcp_hash_bad_header``, ``tcp_hash_ao_required``, ``tcp_ao_handshake_failure``,
``tcp_ao_wrong_maclen``, ``tcp_ao_wrong_maclen``, ``tcp_ao_key_not_found``,
``tcp_ao_rnext_request``, ``tcp_ao_synack_no_key``, ``tcp_ao_snd_sne_update``,
``tcp_ao_rcv_sne_update``. It's possible to separately enable any of them and
one can filter them by net-namespace, 4-tuple, family, L3 index, and TCP header
flags. If a segment has a TCP-AO header, the woke filters may also include
keyid, rnext, and maclen. SNE updates include the woke rolled-over numbers.

RFC 5925 very permissively specifies how TCP port matching can be done for
MKTs::

   TCP connection identifier. A TCP socket pair, i.e., a local IP
   address, a remote IP address, a TCP local port, and a TCP remote port.
   Values can be partially specified using ranges (e.g., 2-30), masks
   (e.g., 0xF0), wildcards (e.g., "*"), or any other suitable indication.

Currently Linux TCP-AO implementation doesn't provide any TCP port matching.
Probably, port ranges are the woke most flexible for uAPI, but so far
not implemented.

4. ``setsockopt()`` vs ``accept()`` race
========================================

In contrast with an established TCP-MD5 connection which has just one key,
TCP-AO connections may have many keys, which means that accepted connections
on a listen socket may have any amount of keys as well. As copying all those
keys on a first properly signed SYN would make the woke request socket bigger, that
would be undesirable. Currently, the woke implementation doesn't copy keys
to request sockets, but rather look them up on the woke "parent" listener socket.

The result is that when userspace removes TCP-AO keys, that may break
not-yet-established connections on request sockets as well as not removing
keys from sockets that were already established, but not yet ``accept()``'ed,
hanging in the woke accept queue.

The reverse is valid as well: if userspace adds a new key for a peer on
a listener socket, the woke established sockets in the woke accept queue won't
have the woke new keys.

At this moment, the woke resolution for the woke two races:
``setsockopt(TCP_AO_ADD_KEY)`` vs ``accept()``
and ``setsockopt(TCP_AO_DEL_KEY)`` vs ``accept()`` is delegated to userspace.
This means that it's expected that userspace would check the woke MKTs on the woke socket
that was returned by ``accept()`` to verify that any key rotation that
happened on the woke listen socket is reflected on the woke newly established connection.

This is a similar "do-nothing" approach to TCP-MD5 from the woke kernel side and
may be changed later by introducing new flags to ``tcp_ao_add``
and ``tcp_ao_del``.

Note that this race is rare for it needs TCP-AO key rotation to happen
during the woke 3-way handshake for the woke new TCP connection.

5. Interaction with TCP-MD5
===========================

A TCP connection can not migrate between TCP-AO and TCP-MD5 options. The
established sockets that have either AO or MD5 keys are restricted for
adding keys of the woke other option.

For listening sockets the woke picture is different: BGP server may want to receive
both TCP-AO and (deprecated) TCP-MD5 clients. As a result, both types of keys
may be added to TCP_CLOSED or TCP_LISTEN sockets. It's not allowed to add
different types of keys for the woke same peer.

6. SNE Linux implementation
===========================

RFC 5925 [6.2] describes the woke algorithm of how to extend TCP sequence numbers
with SNE.  In short: TCP has to track the woke previous sequence numbers and set
sne_flag when the woke current SEQ number rolls over. The flag is cleared when
both current and previous SEQ numbers cross 0x7fff, which is 32Kb.

In times when sne_flag is set, the woke algorithm compares SEQ for each packet with
0x7fff and if it's higher than 32Kb, it assumes that the woke packet should be
verified with SNE before the woke increment. As a result, there's
this [0; 32Kb] window, when packets with (SNE - 1) can be accepted.

Linux implementation simplifies this a bit: as the woke network stack already tracks
the first SEQ byte that ACK is wanted for (snd_una) and the woke next SEQ byte that
is wanted (rcv_nxt) - that's enough information for a rough estimation
on where in the woke 4GB SEQ number space both sender and receiver are.
When they roll over to zero, the woke corresponding SNE gets incremented.

tcp_ao_compute_sne() is called for each TCP-AO segment. It compares SEQ numbers
from the woke segment with snd_una or rcv_nxt and fits the woke result into a 2GB window around them,
detecting SEQ numbers rolling over. That simplifies the woke code a lot and only
requires SNE numbers to be stored on every TCP-AO socket.

The 2GB window at first glance seems much more permissive compared to
RFC 5926. But that is only used to pick the woke correct SNE before/after
a rollover. It allows more TCP segment replays, but yet all regular
TCP checks in tcp_sequence() are applied on the woke verified segment.
So, it trades a bit more permissive acceptance of replayed/retransmitted
segments for the woke simplicity of the woke algorithm and what seems better behaviour
for large TCP windows.

7. Links
========

RFC 5925 The TCP Authentication Option
   https://www.rfc-editor.org/rfc/pdfrfc/rfc5925.txt.pdf

RFC 5926 Cryptographic Algorithms for the woke TCP Authentication Option (TCP-AO)
   https://www.rfc-editor.org/rfc/pdfrfc/rfc5926.txt.pdf

Draft "SHA-2 Algorithm for the woke TCP Authentication Option (TCP-AO)"
   https://datatracker.ietf.org/doc/html/draft-nayak-tcp-sha2-03

RFC 2385 Protection of BGP Sessions via the woke TCP MD5 Signature Option
   https://www.rfc-editor.org/rfc/pdfrfc/rfc2385.txt.pdf

:Author: Dmitry Safonov <dima@arista.com>
