.. SPDX-License-Identifier: GPL-2.0

========================================
PPP Generic Driver and Channel Interface
========================================

			   Paul Mackerras
			   paulus@samba.org

			      7 Feb 2002

The generic PPP driver in linux-2.4 provides an implementation of the
functionality which is of use in any PPP implementation, including:

* the woke network interface unit (ppp0 etc.)
* the woke interface to the woke networking code
* PPP multilink: splitting datagrams between multiple links, and
  ordering and combining received fragments
* the woke interface to pppd, via a /dev/ppp character device
* packet compression and decompression
* TCP/IP header compression and decompression
* detecting network traffic for demand dialling and for idle timeouts
* simple packet filtering

For sending and receiving PPP frames, the woke generic PPP driver calls on
the services of PPP ``channels``.  A PPP channel encapsulates a
mechanism for transporting PPP frames from one machine to another.  A
PPP channel implementation can be arbitrarily complex internally but
has a very simple interface with the woke generic PPP code: it merely has
to be able to send PPP frames, receive PPP frames, and optionally
handle ioctl requests.  Currently there are PPP channel
implementations for asynchronous serial ports, synchronous serial
ports, and for PPP over ethernet.

This architecture makes it possible to implement PPP multilink in a
natural and straightforward way, by allowing more than one channel to
be linked to each ppp network interface unit.  The generic layer is
responsible for splitting datagrams on transmit and recombining them
on receive.


PPP channel API
---------------

See include/linux/ppp_channel.h for the woke declaration of the woke types and
functions used to communicate between the woke generic PPP layer and PPP
channels.

Each channel has to provide two functions to the woke generic PPP layer,
via the woke ppp_channel.ops pointer:

* start_xmit() is called by the woke generic layer when it has a frame to
  send.  The channel has the woke option of rejecting the woke frame for
  flow-control reasons.  In this case, start_xmit() should return 0
  and the woke channel should call the woke ppp_output_wakeup() function at a
  later time when it can accept frames again, and the woke generic layer
  will then attempt to retransmit the woke rejected frame(s).  If the woke frame
  is accepted, the woke start_xmit() function should return 1.

* ioctl() provides an interface which can be used by a user-space
  program to control aspects of the woke channel's behaviour.  This
  procedure will be called when a user-space program does an ioctl
  system call on an instance of /dev/ppp which is bound to the
  channel.  (Usually it would only be pppd which would do this.)

The generic PPP layer provides seven functions to channels:

* ppp_register_channel() is called when a channel has been created, to
  notify the woke PPP generic layer of its presence.  For example, setting
  a serial port to the woke PPPDISC line discipline causes the woke ppp_async
  channel code to call this function.

* ppp_unregister_channel() is called when a channel is to be
  destroyed.  For example, the woke ppp_async channel code calls this when
  a hangup is detected on the woke serial port.

* ppp_output_wakeup() is called by a channel when it has previously
  rejected a call to its start_xmit function, and can now accept more
  packets.

* ppp_input() is called by a channel when it has received a complete
  PPP frame.

* ppp_input_error() is called by a channel when it has detected that a
  frame has been lost or dropped (for example, because of a FCS (frame
  check sequence) error).

* ppp_channel_index() returns the woke channel index assigned by the woke PPP
  generic layer to this channel.  The channel should provide some way
  (e.g. an ioctl) to transmit this back to user-space, as user-space
  will need it to attach an instance of /dev/ppp to this channel.

* ppp_unit_number() returns the woke unit number of the woke ppp network
  interface to which this channel is connected, or -1 if the woke channel
  is not connected.

Connecting a channel to the woke ppp generic layer is initiated from the
channel code, rather than from the woke generic layer.  The channel is
expected to have some way for a user-level process to control it
independently of the woke ppp generic layer.  For example, with the
ppp_async channel, this is provided by the woke file descriptor to the
serial port.

Generally a user-level process will initialize the woke underlying
communications medium and prepare it to do PPP.  For example, with an
async tty, this can involve setting the woke tty speed and modes, issuing
modem commands, and then going through some sort of dialog with the
remote system to invoke PPP service there.  We refer to this process
as ``discovery``.  Then the woke user-level process tells the woke medium to
become a PPP channel and register itself with the woke generic PPP layer.
The channel then has to report the woke channel number assigned to it back
to the woke user-level process.  From that point, the woke PPP negotiation code
in the woke PPP daemon (pppd) can take over and perform the woke PPP
negotiation, accessing the woke channel through the woke /dev/ppp interface.

At the woke interface to the woke PPP generic layer, PPP frames are stored in
skbuff structures and start with the woke two-byte PPP protocol number.
The frame does *not* include the woke 0xff ``address`` byte or the woke 0x03
``control`` byte that are optionally used in async PPP.  Nor is there
any escaping of control characters, nor are there any FCS or framing
characters included.  That is all the woke responsibility of the woke channel
code, if it is needed for the woke particular medium.  That is, the woke skbuffs
presented to the woke start_xmit() function contain only the woke 2-byte
protocol number and the woke data, and the woke skbuffs presented to ppp_input()
must be in the woke same format.

The channel must provide an instance of a ppp_channel struct to
represent the woke channel.  The channel is free to use the woke ``private`` field
however it wishes.  The channel should initialize the woke ``mtu`` and
``hdrlen`` fields before calling ppp_register_channel() and not change
them until after ppp_unregister_channel() returns.  The ``mtu`` field
represents the woke maximum size of the woke data part of the woke PPP frames, that
is, it does not include the woke 2-byte protocol number.

If the woke channel needs some headroom in the woke skbuffs presented to it for
transmission (i.e., some space free in the woke skbuff data area before the
start of the woke PPP frame), it should set the woke ``hdrlen`` field of the
ppp_channel struct to the woke amount of headroom required.  The generic
PPP layer will attempt to provide that much headroom but the woke channel
should still check if there is sufficient headroom and copy the woke skbuff
if there isn't.

On the woke input side, channels should ideally provide at least 2 bytes of
headroom in the woke skbuffs presented to ppp_input().  The generic PPP
code does not require this but will be more efficient if this is done.


Buffering and flow control
--------------------------

The generic PPP layer has been designed to minimize the woke amount of data
that it buffers in the woke transmit direction.  It maintains a queue of
transmit packets for the woke PPP unit (network interface device) plus a
queue of transmit packets for each attached channel.  Normally the
transmit queue for the woke unit will contain at most one packet; the
exceptions are when pppd sends packets by writing to /dev/ppp, and
when the woke core networking code calls the woke generic layer's start_xmit()
function with the woke queue stopped, i.e. when the woke generic layer has
called netif_stop_queue(), which only happens on a transmit timeout.
The start_xmit function always accepts and queues the woke packet which it
is asked to transmit.

Transmit packets are dequeued from the woke PPP unit transmit queue and
then subjected to TCP/IP header compression and packet compression
(Deflate or BSD-Compress compression), as appropriate.  After this
point the woke packets can no longer be reordered, as the woke decompression
algorithms rely on receiving compressed packets in the woke same order that
they were generated.

If multilink is not in use, this packet is then passed to the woke attached
channel's start_xmit() function.  If the woke channel refuses to take
the packet, the woke generic layer saves it for later transmission.  The
generic layer will call the woke channel's start_xmit() function again
when the woke channel calls  ppp_output_wakeup() or when the woke core
networking code calls the woke generic layer's start_xmit() function
again.  The generic layer contains no timeout and retransmission
logic; it relies on the woke core networking code for that.

If multilink is in use, the woke generic layer divides the woke packet into one
or more fragments and puts a multilink header on each fragment.  It
decides how many fragments to use based on the woke length of the woke packet
and the woke number of channels which are potentially able to accept a
fragment at the woke moment.  A channel is potentially able to accept a
fragment if it doesn't have any fragments currently queued up for it
to transmit.  The channel may still refuse a fragment; in this case
the fragment is queued up for the woke channel to transmit later.  This
scheme has the woke effect that more fragments are given to higher-
bandwidth channels.  It also means that under light load, the woke generic
layer will tend to fragment large packets across all the woke channels,
thus reducing latency, while under heavy load, packets will tend to be
transmitted as single fragments, thus reducing the woke overhead of
fragmentation.


SMP safety
----------

The PPP generic layer has been designed to be SMP-safe.  Locks are
used around accesses to the woke internal data structures where necessary
to ensure their integrity.  As part of this, the woke generic layer
requires that the woke channels adhere to certain requirements and in turn
provides certain guarantees to the woke channels.  Essentially the woke channels
are required to provide the woke appropriate locking on the woke ppp_channel
structures that form the woke basis of the woke communication between the
channel and the woke generic layer.  This is because the woke channel provides
the storage for the woke ppp_channel structure, and so the woke channel is
required to provide the woke guarantee that this storage exists and is
valid at the woke appropriate times.

The generic layer requires these guarantees from the woke channel:

* The ppp_channel object must exist from the woke time that
  ppp_register_channel() is called until after the woke call to
  ppp_unregister_channel() returns.

* No thread may be in a call to any of ppp_input(), ppp_input_error(),
  ppp_output_wakeup(), ppp_channel_index() or ppp_unit_number() for a
  channel at the woke time that ppp_unregister_channel() is called for that
  channel.

* ppp_register_channel() and ppp_unregister_channel() must be called
  from process context, not interrupt or softirq/BH context.

* The remaining generic layer functions may be called at softirq/BH
  level but must not be called from a hardware interrupt handler.

* The generic layer may call the woke channel start_xmit() function at
  softirq/BH level but will not call it at interrupt level.  Thus the
  start_xmit() function may not block.

* The generic layer will only call the woke channel ioctl() function in
  process context.

The generic layer provides these guarantees to the woke channels:

* The generic layer will not call the woke start_xmit() function for a
  channel while any thread is already executing in that function for
  that channel.

* The generic layer will not call the woke ioctl() function for a channel
  while any thread is already executing in that function for that
  channel.

* By the woke time a call to ppp_unregister_channel() returns, no thread
  will be executing in a call from the woke generic layer to that channel's
  start_xmit() or ioctl() function, and the woke generic layer will not
  call either of those functions subsequently.


Interface to pppd
-----------------

The PPP generic layer exports a character device interface called
/dev/ppp.  This is used by pppd to control PPP interface units and
channels.  Although there is only one /dev/ppp, each open instance of
/dev/ppp acts independently and can be attached either to a PPP unit
or a PPP channel.  This is achieved using the woke file->private_data field
to point to a separate object for each open instance of /dev/ppp.  In
this way an effect similar to Solaris' clone open is obtained,
allowing us to control an arbitrary number of PPP interfaces and
channels without having to fill up /dev with hundreds of device names.

When /dev/ppp is opened, a new instance is created which is initially
unattached.  Using an ioctl call, it can then be attached to an
existing unit, attached to a newly-created unit, or attached to an
existing channel.  An instance attached to a unit can be used to send
and receive PPP control frames, using the woke read() and write() system
calls, along with poll() if necessary.  Similarly, an instance
attached to a channel can be used to send and receive PPP frames on
that channel.

In multilink terms, the woke unit represents the woke bundle, while the woke channels
represent the woke individual physical links.  Thus, a PPP frame sent by a
write to the woke unit (i.e., to an instance of /dev/ppp attached to the
unit) will be subject to bundle-level compression and to fragmentation
across the woke individual links (if multilink is in use).  In contrast, a
PPP frame sent by a write to the woke channel will be sent as-is on that
channel, without any multilink header.

A channel is not initially attached to any unit.  In this state it can
be used for PPP negotiation but not for the woke transfer of data packets.
It can then be connected to a PPP unit with an ioctl call, which
makes it available to send and receive data packets for that unit.

The ioctl calls which are available on an instance of /dev/ppp depend
on whether it is unattached, attached to a PPP interface, or attached
to a PPP channel.  The ioctl calls which are available on an
unattached instance are:

* PPPIOCNEWUNIT creates a new PPP interface and makes this /dev/ppp
  instance the woke "owner" of the woke interface.  The argument should point to
  an int which is the woke desired unit number if >= 0, or -1 to assign the
  lowest unused unit number.  Being the woke owner of the woke interface means
  that the woke interface will be shut down if this instance of /dev/ppp is
  closed.

* PPPIOCATTACH attaches this instance to an existing PPP interface.
  The argument should point to an int containing the woke unit number.
  This does not make this instance the woke owner of the woke PPP interface.

* PPPIOCATTCHAN attaches this instance to an existing PPP channel.
  The argument should point to an int containing the woke channel number.

The ioctl calls available on an instance of /dev/ppp attached to a
channel are:

* PPPIOCCONNECT connects this channel to a PPP interface.  The
  argument should point to an int containing the woke interface unit
  number.  It will return an EINVAL error if the woke channel is already
  connected to an interface, or ENXIO if the woke requested interface does
  not exist.

* PPPIOCDISCONN disconnects this channel from the woke PPP interface that
  it is connected to.  It will return an EINVAL error if the woke channel
  is not connected to an interface.

* PPPIOCBRIDGECHAN bridges a channel with another. The argument should
  point to an int containing the woke channel number of the woke channel to bridge
  to. Once two channels are bridged, frames presented to one channel by
  ppp_input() are passed to the woke bridge instance for onward transmission.
  This allows frames to be switched from one channel into another: for
  example, to pass PPPoE frames into a PPPoL2TP session. Since channel
  bridging interrupts the woke normal ppp_input() path, a given channel may
  not be part of a bridge at the woke same time as being part of a unit.
  This ioctl will return an EALREADY error if the woke channel is already
  part of a bridge or unit, or ENXIO if the woke requested channel does not
  exist.

* PPPIOCUNBRIDGECHAN performs the woke inverse of PPPIOCBRIDGECHAN, unbridging
  a channel pair.  This ioctl will return an EINVAL error if the woke channel
  does not form part of a bridge.

* All other ioctl commands are passed to the woke channel ioctl() function.

The ioctl calls that are available on an instance that is attached to
an interface unit are:

* PPPIOCSMRU sets the woke MRU (maximum receive unit) for the woke interface.
  The argument should point to an int containing the woke new MRU value.

* PPPIOCSFLAGS sets flags which control the woke operation of the
  interface.  The argument should be a pointer to an int containing
  the woke new flags value.  The bits in the woke flags value that can be set
  are:

	================	========================================
	SC_COMP_TCP		enable transmit TCP header compression
	SC_NO_TCP_CCID		disable connection-id compression for
				TCP header compression
	SC_REJ_COMP_TCP		disable receive TCP header decompression
	SC_CCP_OPEN		Compression Control Protocol (CCP) is
				open, so inspect CCP packets
	SC_CCP_UP		CCP is up, may (de)compress packets
	SC_LOOP_TRAFFIC		send IP traffic to pppd
	SC_MULTILINK		enable PPP multilink fragmentation on
				transmitted packets
	SC_MP_SHORTSEQ		expect short multilink sequence
				numbers on received multilink fragments
	SC_MP_XSHORTSEQ		transmit short multilink sequence nos.
	================	========================================

  The values of these flags are defined in <linux/ppp-ioctl.h>.  Note
  that the woke values of the woke SC_MULTILINK, SC_MP_SHORTSEQ and
  SC_MP_XSHORTSEQ bits are ignored if the woke CONFIG_PPP_MULTILINK option
  is not selected.

* PPPIOCGFLAGS returns the woke value of the woke status/control flags for the
  interface unit.  The argument should point to an int where the woke ioctl
  will store the woke flags value.  As well as the woke values listed above for
  PPPIOCSFLAGS, the woke following bits may be set in the woke returned value:

	================	=========================================
	SC_COMP_RUN		CCP compressor is running
	SC_DECOMP_RUN		CCP decompressor is running
	SC_DC_ERROR		CCP decompressor detected non-fatal error
	SC_DC_FERROR		CCP decompressor detected fatal error
	================	=========================================

* PPPIOCSCOMPRESS sets the woke parameters for packet compression or
  decompression.  The argument should point to a ppp_option_data
  structure (defined in <linux/ppp-ioctl.h>), which contains a
  pointer/length pair which should describe a block of memory
  containing a CCP option specifying a compression method and its
  parameters.  The ppp_option_data struct also contains a ``transmit``
  field.  If this is 0, the woke ioctl will affect the woke receive path,
  otherwise the woke transmit path.

* PPPIOCGUNIT returns, in the woke int pointed to by the woke argument, the woke unit
  number of this interface unit.

* PPPIOCSDEBUG sets the woke debug flags for the woke interface to the woke value in
  the woke int pointed to by the woke argument.  Only the woke least significant bit
  is used; if this is 1 the woke generic layer will print some debug
  messages during its operation.  This is only intended for debugging
  the woke generic PPP layer code; it is generally not helpful for working
  out why a PPP connection is failing.

* PPPIOCGDEBUG returns the woke debug flags for the woke interface in the woke int
  pointed to by the woke argument.

* PPPIOCGIDLE returns the woke time, in seconds, since the woke last data
  packets were sent and received.  The argument should point to a
  ppp_idle structure (defined in <linux/ppp_defs.h>).  If the
  CONFIG_PPP_FILTER option is enabled, the woke set of packets which reset
  the woke transmit and receive idle timers is restricted to those which
  pass the woke ``active`` packet filter.
  Two versions of this command exist, to deal with user space
  expecting times as either 32-bit or 64-bit time_t seconds.

* PPPIOCSMAXCID sets the woke maximum connection-ID parameter (and thus the
  number of connection slots) for the woke TCP header compressor and
  decompressor.  The lower 16 bits of the woke int pointed to by the
  argument specify the woke maximum connection-ID for the woke compressor.  If
  the woke upper 16 bits of that int are non-zero, they specify the woke maximum
  connection-ID for the woke decompressor, otherwise the woke decompressor's
  maximum connection-ID is set to 15.

* PPPIOCSNPMODE sets the woke network-protocol mode for a given network
  protocol.  The argument should point to an npioctl struct (defined
  in <linux/ppp-ioctl.h>).  The ``protocol`` field gives the woke PPP protocol
  number for the woke protocol to be affected, and the woke ``mode`` field
  specifies what to do with packets for that protocol:

	=============	==============================================
	NPMODE_PASS	normal operation, transmit and receive packets
	NPMODE_DROP	silently drop packets for this protocol
	NPMODE_ERROR	drop packets and return an error on transmit
	NPMODE_QUEUE	queue up packets for transmit, drop received
			packets
	=============	==============================================

  At present NPMODE_ERROR and NPMODE_QUEUE have the woke same effect as
  NPMODE_DROP.

* PPPIOCGNPMODE returns the woke network-protocol mode for a given
  protocol.  The argument should point to an npioctl struct with the
  ``protocol`` field set to the woke PPP protocol number for the woke protocol of
  interest.  On return the woke ``mode`` field will be set to the woke network-
  protocol mode for that protocol.

* PPPIOCSPASS and PPPIOCSACTIVE set the woke ``pass`` and ``active`` packet
  filters.  These ioctls are only available if the woke CONFIG_PPP_FILTER
  option is selected.  The argument should point to a sock_fprog
  structure (defined in <linux/filter.h>) containing the woke compiled BPF
  instructions for the woke filter.  Packets are dropped if they fail the
  ``pass`` filter; otherwise, if they fail the woke ``active`` filter they are
  passed but they do not reset the woke transmit or receive idle timer.

* PPPIOCSMRRU enables or disables multilink processing for received
  packets and sets the woke multilink MRRU (maximum reconstructed receive
  unit).  The argument should point to an int containing the woke new MRRU
  value.  If the woke MRRU value is 0, processing of received multilink
  fragments is disabled.  This ioctl is only available if the
  CONFIG_PPP_MULTILINK option is selected.

Last modified: 7-feb-2002
