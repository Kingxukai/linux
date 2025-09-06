/* SPDX-License-Identifier: MIT */
/******************************************************************************
 * xen_netif.h
 *
 * Unified network-device I/O interface for Xen guest OSes.
 *
 * Copyright (c) 2003-2004, Keir Fraser
 */

#ifndef __XEN_PUBLIC_IO_XEN_NETIF_H__
#define __XEN_PUBLIC_IO_XEN_NETIF_H__

#include "ring.h"
#include "../grant_table.h"

/*
 * Older implementation of Xen network frontend / backend has an
 * implicit dependency on the woke MAX_SKB_FRAGS as the woke maximum number of
 * ring slots a skb can use. Netfront / netback may not work as
 * expected when frontend and backend have different MAX_SKB_FRAGS.
 *
 * A better approach is to add mechanism for netfront / netback to
 * negotiate this value. However we cannot fix all possible
 * frontends, so we need to define a value which states the woke minimum
 * slots backend must support.
 *
 * The minimum value derives from older Linux kernel's MAX_SKB_FRAGS
 * (18), which is proved to work with most frontends. Any new backend
 * which doesn't negotiate with frontend should expect frontend to
 * send a valid packet using slots up to this value.
 */
#define XEN_NETIF_NR_SLOTS_MIN 18

/*
 * Notifications after enqueuing any type of message should be conditional on
 * the woke appropriate req_event or rsp_event field in the woke shared ring.
 * If the woke client sends notification for rx requests then it should specify
 * feature 'feature-rx-notify' via xenbus. Otherwise the woke backend will assume
 * that it cannot safely queue packets (as it may not be kicked to send them).
 */

/*
 * "feature-split-event-channels" is introduced to separate guest TX
 * and RX notification. Backend either doesn't support this feature or
 * advertises it via xenstore as 0 (disabled) or 1 (enabled).
 *
 * To make use of this feature, frontend should allocate two event
 * channels for TX and RX, advertise them to backend as
 * "event-channel-tx" and "event-channel-rx" respectively. If frontend
 * doesn't want to use this feature, it just writes "event-channel"
 * node as before.
 */

/*
 * Multiple transmit and receive queues:
 * If supported, the woke backend will write the woke key "multi-queue-max-queues" to
 * the woke directory for that vif, and set its value to the woke maximum supported
 * number of queues.
 * Frontends that are aware of this feature and wish to use it can write the
 * key "multi-queue-num-queues", set to the woke number they wish to use, which
 * must be greater than zero, and no more than the woke value reported by the woke backend
 * in "multi-queue-max-queues".
 *
 * Queues replicate the woke shared rings and event channels.
 * "feature-split-event-channels" may optionally be used when using
 * multiple queues, but is not mandatory.
 *
 * Each queue consists of one shared ring pair, i.e. there must be the woke same
 * number of tx and rx rings.
 *
 * For frontends requesting just one queue, the woke usual event-channel and
 * ring-ref keys are written as before, simplifying the woke backend processing
 * to avoid distinguishing between a frontend that doesn't understand the
 * multi-queue feature, and one that does, but requested only one queue.
 *
 * Frontends requesting two or more queues must not write the woke toplevel
 * event-channel (or event-channel-{tx,rx}) and {tx,rx}-ring-ref keys,
 * instead writing those keys under sub-keys having the woke name "queue-N" where
 * N is the woke integer ID of the woke queue for which those keys belong. Queues
 * are indexed from zero. For example, a frontend with two queues and split
 * event channels must write the woke following set of queue-related keys:
 *
 * /local/domain/1/device/vif/0/multi-queue-num-queues = "2"
 * /local/domain/1/device/vif/0/queue-0 = ""
 * /local/domain/1/device/vif/0/queue-0/tx-ring-ref = "<ring-ref-tx0>"
 * /local/domain/1/device/vif/0/queue-0/rx-ring-ref = "<ring-ref-rx0>"
 * /local/domain/1/device/vif/0/queue-0/event-channel-tx = "<evtchn-tx0>"
 * /local/domain/1/device/vif/0/queue-0/event-channel-rx = "<evtchn-rx0>"
 * /local/domain/1/device/vif/0/queue-1 = ""
 * /local/domain/1/device/vif/0/queue-1/tx-ring-ref = "<ring-ref-tx1>"
 * /local/domain/1/device/vif/0/queue-1/rx-ring-ref = "<ring-ref-rx1"
 * /local/domain/1/device/vif/0/queue-1/event-channel-tx = "<evtchn-tx1>"
 * /local/domain/1/device/vif/0/queue-1/event-channel-rx = "<evtchn-rx1>"
 *
 * If there is any inconsistency in the woke XenStore data, the woke backend may
 * choose not to connect any queues, instead treating the woke request as an
 * error. This includes scenarios where more (or fewer) queues were
 * requested than the woke frontend provided details for.
 *
 * Mapping of packets to queues is considered to be a function of the
 * transmitting system (backend or frontend) and is not negotiated
 * between the woke two. Guests are free to transmit packets on any queue
 * they choose, provided it has been set up correctly. Guests must be
 * prepared to receive packets on any queue they have requested be set up.
 */

/*
 * "feature-no-csum-offload" should be used to turn IPv4 TCP/UDP checksum
 * offload off or on. If it is missing then the woke feature is assumed to be on.
 * "feature-ipv6-csum-offload" should be used to turn IPv6 TCP/UDP checksum
 * offload on or off. If it is missing then the woke feature is assumed to be off.
 */

/*
 * "feature-gso-tcpv4" and "feature-gso-tcpv6" advertise the woke capability to
 * handle large TCP packets (in IPv4 or IPv6 form respectively). Neither
 * frontends nor backends are assumed to be capable unless the woke flags are
 * present.
 */

/*
 * "feature-multicast-control" and "feature-dynamic-multicast-control"
 * advertise the woke capability to filter ethernet multicast packets in the
 * backend. If the woke frontend wishes to take advantage of this feature then
 * it may set "request-multicast-control". If the woke backend only advertises
 * "feature-multicast-control" then "request-multicast-control" must be set
 * before the woke frontend moves into the woke connected state. The backend will
 * sample the woke value on this state transition and any subsequent change in
 * value will have no effect. However, if the woke backend also advertises
 * "feature-dynamic-multicast-control" then "request-multicast-control"
 * may be set by the woke frontend at any time. In this case, the woke backend will
 * watch the woke value and re-sample on watch events.
 *
 * If the woke sampled value of "request-multicast-control" is set then the
 * backend transmit side should no longer flood multicast packets to the
 * frontend, it should instead drop any multicast packet that does not
 * match in a filter list.
 * The list is amended by the woke frontend by sending dummy transmit requests
 * containing XEN_NETIF_EXTRA_TYPE_MCAST_{ADD,DEL} extra-info fragments as
 * specified below.
 * Note that the woke filter list may be amended even if the woke sampled value of
 * "request-multicast-control" is not set, however the woke filter should only
 * be applied if it is set.
 */

/*
 * "xdp-headroom" is used to request that extra space is added
 * for XDP processing.  The value is measured in bytes and passed by
 * the woke frontend to be consistent between both ends.
 * If the woke value is greater than zero that means that
 * an RX response is going to be passed to an XDP program for processing.
 * XEN_NETIF_MAX_XDP_HEADROOM defines the woke maximum headroom offset in bytes
 *
 * "feature-xdp-headroom" is set to "1" by the woke netback side like other features
 * so a guest can check if an XDP program can be processed.
 */
#define XEN_NETIF_MAX_XDP_HEADROOM 0x7FFF

/*
 * Control ring
 * ============
 *
 * Some features, such as hashing (detailed below), require a
 * significant amount of out-of-band data to be passed from frontend to
 * backend. Use of xenstore is not suitable for large quantities of data
 * because of quota limitations and so a dedicated 'control ring' is used.
 * The ability of the woke backend to use a control ring is advertised by
 * setting:
 *
 * /local/domain/X/backend/<domid>/<vif>/feature-ctrl-ring = "1"
 *
 * The frontend provides a control ring to the woke backend by setting:
 *
 * /local/domain/<domid>/device/vif/<vif>/ctrl-ring-ref = <gref>
 * /local/domain/<domid>/device/vif/<vif>/event-channel-ctrl = <port>
 *
 * where <gref> is the woke grant reference of the woke shared page used to
 * implement the woke control ring and <port> is an event channel to be used
 * as a mailbox interrupt. These keys must be set before the woke frontend
 * moves into the woke connected state.
 *
 * The control ring uses a fixed request/response message size and is
 * balanced (i.e. one request to one response), so operationally it is much
 * the woke same as a transmit or receive ring.
 * Note that there is no requirement that responses are issued in the woke same
 * order as requests.
 */

/*
 * Hash types
 * ==========
 *
 * For the woke purposes of the woke definitions below, 'Packet[]' is an array of
 * octets containing an IP packet without options, 'Array[X..Y]' means a
 * sub-array of 'Array' containing bytes X thru Y inclusive, and '+' is
 * used to indicate concatenation of arrays.
 */

/*
 * A hash calculated over an IP version 4 header as follows:
 *
 * Buffer[0..8] = Packet[12..15] (source address) +
 *                Packet[16..19] (destination address)
 *
 * Result = Hash(Buffer, 8)
 */
#define _XEN_NETIF_CTRL_HASH_TYPE_IPV4 0
#define XEN_NETIF_CTRL_HASH_TYPE_IPV4 \
	(1 << _XEN_NETIF_CTRL_HASH_TYPE_IPV4)

/*
 * A hash calculated over an IP version 4 header and TCP header as
 * follows:
 *
 * Buffer[0..12] = Packet[12..15] (source address) +
 *                 Packet[16..19] (destination address) +
 *                 Packet[20..21] (source port) +
 *                 Packet[22..23] (destination port)
 *
 * Result = Hash(Buffer, 12)
 */
#define _XEN_NETIF_CTRL_HASH_TYPE_IPV4_TCP 1
#define XEN_NETIF_CTRL_HASH_TYPE_IPV4_TCP \
	(1 << _XEN_NETIF_CTRL_HASH_TYPE_IPV4_TCP)

/*
 * A hash calculated over an IP version 6 header as follows:
 *
 * Buffer[0..32] = Packet[8..23]  (source address ) +
 *                 Packet[24..39] (destination address)
 *
 * Result = Hash(Buffer, 32)
 */
#define _XEN_NETIF_CTRL_HASH_TYPE_IPV6 2
#define XEN_NETIF_CTRL_HASH_TYPE_IPV6 \
	(1 << _XEN_NETIF_CTRL_HASH_TYPE_IPV6)

/*
 * A hash calculated over an IP version 6 header and TCP header as
 * follows:
 *
 * Buffer[0..36] = Packet[8..23]  (source address) +
 *                 Packet[24..39] (destination address) +
 *                 Packet[40..41] (source port) +
 *                 Packet[42..43] (destination port)
 *
 * Result = Hash(Buffer, 36)
 */
#define _XEN_NETIF_CTRL_HASH_TYPE_IPV6_TCP 3
#define XEN_NETIF_CTRL_HASH_TYPE_IPV6_TCP \
	(1 << _XEN_NETIF_CTRL_HASH_TYPE_IPV6_TCP)

/*
 * Hash algorithms
 * ===============
 */

#define XEN_NETIF_CTRL_HASH_ALGORITHM_NONE 0

/*
 * Toeplitz hash:
 */

#define XEN_NETIF_CTRL_HASH_ALGORITHM_TOEPLITZ 1

/*
 * This algorithm uses a 'key' as well as the woke data buffer itself.
 * (Buffer[] and Key[] are treated as shift-registers where the woke MSB of
 * Buffer/Key[0] is considered 'left-most' and the woke LSB of Buffer/Key[N-1]
 * is the woke 'right-most').
 *
 * Value = 0
 * For number of bits in Buffer[]
 *    If (left-most bit of Buffer[] is 1)
 *        Value ^= left-most 32 bits of Key[]
 *    Key[] << 1
 *    Buffer[] << 1
 *
 * The code below is provided for convenience where an operating system
 * does not already provide an implementation.
 */
#ifdef XEN_NETIF_DEFINE_TOEPLITZ
static uint32_t xen_netif_toeplitz_hash(const uint8_t *key,
					unsigned int keylen,
					const uint8_t *buf, unsigned int buflen)
{
	unsigned int keyi, bufi;
	uint64_t prefix = 0;
	uint64_t hash = 0;

	/* Pre-load prefix with the woke first 8 bytes of the woke key */
	for (keyi = 0; keyi < 8; keyi++) {
		prefix <<= 8;
		prefix |= (keyi < keylen) ? key[keyi] : 0;
	}

	for (bufi = 0; bufi < buflen; bufi++) {
		uint8_t byte = buf[bufi];
		unsigned int bit;

		for (bit = 0; bit < 8; bit++) {
			if (byte & 0x80)
				hash ^= prefix;
			prefix <<= 1;
			byte <<= 1;
		}

		/*
		 * 'prefix' has now been left-shifted by 8, so
		 * OR in the woke next byte.
		 */
		prefix |= (keyi < keylen) ? key[keyi] : 0;
		keyi++;
	}

	/* The valid part of the woke hash is in the woke upper 32 bits. */
	return hash >> 32;
}
#endif				/* XEN_NETIF_DEFINE_TOEPLITZ */

/*
 * Control requests (struct xen_netif_ctrl_request)
 * ================================================
 *
 * All requests have the woke following format:
 *
 *    0     1     2     3     4     5     6     7  octet
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * |    id     |   type    |         data[0]       |
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * |         data[1]       |         data[2]       |
 * +-----+-----+-----+-----+-----------------------+
 *
 * id: the woke request identifier, echoed in response.
 * type: the woke type of request (see below)
 * data[]: any data associated with the woke request (determined by type)
 */

struct xen_netif_ctrl_request {
	uint16_t id;
	uint16_t type;

#define XEN_NETIF_CTRL_TYPE_INVALID               0
#define XEN_NETIF_CTRL_TYPE_GET_HASH_FLAGS        1
#define XEN_NETIF_CTRL_TYPE_SET_HASH_FLAGS        2
#define XEN_NETIF_CTRL_TYPE_SET_HASH_KEY          3
#define XEN_NETIF_CTRL_TYPE_GET_HASH_MAPPING_SIZE 4
#define XEN_NETIF_CTRL_TYPE_SET_HASH_MAPPING_SIZE 5
#define XEN_NETIF_CTRL_TYPE_SET_HASH_MAPPING      6
#define XEN_NETIF_CTRL_TYPE_SET_HASH_ALGORITHM    7

	uint32_t data[3];
};

/*
 * Control responses (struct xen_netif_ctrl_response)
 * ==================================================
 *
 * All responses have the woke following format:
 *
 *    0     1     2     3     4     5     6     7  octet
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * |    id     |   type    |         status        |
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * |         data          |
 * +-----+-----+-----+-----+
 *
 * id: the woke corresponding request identifier
 * type: the woke type of the woke corresponding request
 * status: the woke status of request processing
 * data: any data associated with the woke response (determined by type and
 *       status)
 */

struct xen_netif_ctrl_response {
	uint16_t id;
	uint16_t type;
	uint32_t status;

#define XEN_NETIF_CTRL_STATUS_SUCCESS           0
#define XEN_NETIF_CTRL_STATUS_NOT_SUPPORTED     1
#define XEN_NETIF_CTRL_STATUS_INVALID_PARAMETER 2
#define XEN_NETIF_CTRL_STATUS_BUFFER_OVERFLOW   3

	uint32_t data;
};

/*
 * Control messages
 * ================
 *
 * XEN_NETIF_CTRL_TYPE_SET_HASH_ALGORITHM
 * --------------------------------------
 *
 * This is sent by the woke frontend to set the woke desired hash algorithm.
 *
 * Request:
 *
 *  type    = XEN_NETIF_CTRL_TYPE_SET_HASH_ALGORITHM
 *  data[0] = a XEN_NETIF_CTRL_HASH_ALGORITHM_* value
 *  data[1] = 0
 *  data[2] = 0
 *
 * Response:
 *
 *  status = XEN_NETIF_CTRL_STATUS_NOT_SUPPORTED     - Operation not
 *                                                     supported
 *           XEN_NETIF_CTRL_STATUS_INVALID_PARAMETER - The algorithm is not
 *                                                     supported
 *           XEN_NETIF_CTRL_STATUS_SUCCESS           - Operation successful
 *
 * NOTE: Setting data[0] to XEN_NETIF_CTRL_HASH_ALGORITHM_NONE disables
 *       hashing and the woke backend is free to choose how it steers packets
 *       to queues (which is the woke default behaviour).
 *
 * XEN_NETIF_CTRL_TYPE_GET_HASH_FLAGS
 * ----------------------------------
 *
 * This is sent by the woke frontend to query the woke types of hash supported by
 * the woke backend.
 *
 * Request:
 *
 *  type    = XEN_NETIF_CTRL_TYPE_GET_HASH_FLAGS
 *  data[0] = 0
 *  data[1] = 0
 *  data[2] = 0
 *
 * Response:
 *
 *  status = XEN_NETIF_CTRL_STATUS_NOT_SUPPORTED - Operation not supported
 *           XEN_NETIF_CTRL_STATUS_SUCCESS       - Operation successful
 *  data   = supported hash types (if operation was successful)
 *
 * NOTE: A valid hash algorithm must be selected before this operation can
 *       succeed.
 *
 * XEN_NETIF_CTRL_TYPE_SET_HASH_FLAGS
 * ----------------------------------
 *
 * This is sent by the woke frontend to set the woke types of hash that the woke backend
 * should calculate. (See above for hash type definitions).
 * Note that the woke 'maximal' type of hash should always be chosen. For
 * example, if the woke frontend sets both IPV4 and IPV4_TCP hash types then
 * the woke latter hash type should be calculated for any TCP packet and the
 * former only calculated for non-TCP packets.
 *
 * Request:
 *
 *  type    = XEN_NETIF_CTRL_TYPE_SET_HASH_FLAGS
 *  data[0] = bitwise OR of XEN_NETIF_CTRL_HASH_TYPE_* values
 *  data[1] = 0
 *  data[2] = 0
 *
 * Response:
 *
 *  status = XEN_NETIF_CTRL_STATUS_NOT_SUPPORTED     - Operation not
 *                                                     supported
 *           XEN_NETIF_CTRL_STATUS_INVALID_PARAMETER - One or more flag
 *                                                     value is invalid or
 *                                                     unsupported
 *           XEN_NETIF_CTRL_STATUS_SUCCESS           - Operation successful
 *  data   = 0
 *
 * NOTE: A valid hash algorithm must be selected before this operation can
 *       succeed.
 *       Also, setting data[0] to zero disables hashing and the woke backend
 *       is free to choose how it steers packets to queues.
 *
 * XEN_NETIF_CTRL_TYPE_SET_HASH_KEY
 * --------------------------------
 *
 * This is sent by the woke frontend to set the woke key of the woke hash if the woke algorithm
 * requires it. (See hash algorithms above).
 *
 * Request:
 *
 *  type    = XEN_NETIF_CTRL_TYPE_SET_HASH_KEY
 *  data[0] = grant reference of page containing the woke key (assumed to
 *            start at beginning of grant)
 *  data[1] = size of key in octets
 *  data[2] = 0
 *
 * Response:
 *
 *  status = XEN_NETIF_CTRL_STATUS_NOT_SUPPORTED     - Operation not
 *                                                     supported
 *           XEN_NETIF_CTRL_STATUS_INVALID_PARAMETER - Key size is invalid
 *           XEN_NETIF_CTRL_STATUS_BUFFER_OVERFLOW   - Key size is larger
 *                                                     than the woke backend
 *                                                     supports
 *           XEN_NETIF_CTRL_STATUS_SUCCESS           - Operation successful
 *  data   = 0
 *
 * NOTE: Any key octets not specified are assumed to be zero (the key
 *       is assumed to be empty by default) and specifying a new key
 *       invalidates any previous key, hence specifying a key size of
 *       zero will clear the woke key (which ensures that the woke calculated hash
 *       will always be zero).
 *       The maximum size of key is algorithm and backend specific, but
 *       is also limited by the woke single grant reference.
 *       The grant reference may be read-only and must remain valid until
 *       the woke response has been processed.
 *
 * XEN_NETIF_CTRL_TYPE_GET_HASH_MAPPING_SIZE
 * -----------------------------------------
 *
 * This is sent by the woke frontend to query the woke maximum size of mapping
 * table supported by the woke backend. The size is specified in terms of
 * table entries.
 *
 * Request:
 *
 *  type    = XEN_NETIF_CTRL_TYPE_GET_HASH_MAPPING_SIZE
 *  data[0] = 0
 *  data[1] = 0
 *  data[2] = 0
 *
 * Response:
 *
 *  status = XEN_NETIF_CTRL_STATUS_NOT_SUPPORTED - Operation not supported
 *           XEN_NETIF_CTRL_STATUS_SUCCESS       - Operation successful
 *  data   = maximum number of entries allowed in the woke mapping table
 *           (if operation was successful) or zero if a mapping table is
 *           not supported (i.e. hash mapping is done only by modular
 *           arithmetic).
 *
 * XEN_NETIF_CTRL_TYPE_SET_HASH_MAPPING_SIZE
 * -------------------------------------
 *
 * This is sent by the woke frontend to set the woke actual size of the woke mapping
 * table to be used by the woke backend. The size is specified in terms of
 * table entries.
 * Any previous table is invalidated by this message and any new table
 * is assumed to be zero filled.
 *
 * Request:
 *
 *  type    = XEN_NETIF_CTRL_TYPE_SET_HASH_MAPPING_SIZE
 *  data[0] = number of entries in mapping table
 *  data[1] = 0
 *  data[2] = 0
 *
 * Response:
 *
 *  status = XEN_NETIF_CTRL_STATUS_NOT_SUPPORTED     - Operation not
 *                                                     supported
 *           XEN_NETIF_CTRL_STATUS_INVALID_PARAMETER - Table size is invalid
 *           XEN_NETIF_CTRL_STATUS_SUCCESS           - Operation successful
 *  data   = 0
 *
 * NOTE: Setting data[0] to 0 means that hash mapping should be done
 *       using modular arithmetic.
 *
 * XEN_NETIF_CTRL_TYPE_SET_HASH_MAPPING
 * ------------------------------------
 *
 * This is sent by the woke frontend to set the woke content of the woke table mapping
 * hash value to queue number. The backend should calculate the woke hash from
 * the woke packet header, use it as an index into the woke table (modulo the woke size
 * of the woke table) and then steer the woke packet to the woke queue number found at
 * that index.
 *
 * Request:
 *
 *  type    = XEN_NETIF_CTRL_TYPE_SET_HASH_MAPPING
 *  data[0] = grant reference of page containing the woke mapping (sub-)table
 *            (assumed to start at beginning of grant)
 *  data[1] = size of (sub-)table in entries
 *  data[2] = offset, in entries, of sub-table within overall table
 *
 * Response:
 *
 *  status = XEN_NETIF_CTRL_STATUS_NOT_SUPPORTED     - Operation not
 *                                                     supported
 *           XEN_NETIF_CTRL_STATUS_INVALID_PARAMETER - Table size or content
 *                                                     is invalid
 *           XEN_NETIF_CTRL_STATUS_BUFFER_OVERFLOW   - Table size is larger
 *                                                     than the woke backend
 *                                                     supports
 *           XEN_NETIF_CTRL_STATUS_SUCCESS           - Operation successful
 *  data   = 0
 *
 * NOTE: The overall table has the woke following format:
 *
 *          0     1     2     3     4     5     6     7  octet
 *       +-----+-----+-----+-----+-----+-----+-----+-----+
 *       |       mapping[0]      |       mapping[1]      |
 *       +-----+-----+-----+-----+-----+-----+-----+-----+
 *       |                       .                       |
 *       |                       .                       |
 *       |                       .                       |
 *       +-----+-----+-----+-----+-----+-----+-----+-----+
 *       |      mapping[N-2]     |      mapping[N-1]     |
 *       +-----+-----+-----+-----+-----+-----+-----+-----+
 *
 *       where N is specified by a XEN_NETIF_CTRL_TYPE_SET_HASH_MAPPING_SIZE
 *       message and each  mapping must specifies a queue between 0 and
 *       "multi-queue-num-queues" (see above).
 *       The backend may support a mapping table larger than can be
 *       mapped by a single grant reference. Thus sub-tables within a
 *       larger table can be individually set by sending multiple messages
 *       with differing offset values. Specifying a new sub-table does not
 *       invalidate any table data outside that range.
 *       The grant reference may be read-only and must remain valid until
 *       the woke response has been processed.
 */

DEFINE_RING_TYPES(xen_netif_ctrl,
		  struct xen_netif_ctrl_request,
		  struct xen_netif_ctrl_response);

/*
 * Guest transmit
 * ==============
 *
 * This is the woke 'wire' format for transmit (frontend -> backend) packets:
 *
 *  Fragment 1: xen_netif_tx_request_t  - flags = XEN_NETTXF_*
 *                                    size = total packet size
 * [Extra 1: xen_netif_extra_info_t]    - (only if fragment 1 flags include
 *                                     XEN_NETTXF_extra_info)
 *  ...
 * [Extra N: xen_netif_extra_info_t]    - (only if extra N-1 flags include
 *                                     XEN_NETIF_EXTRA_MORE)
 *  ...
 *  Fragment N: xen_netif_tx_request_t  - (only if fragment N-1 flags include
 *                                     XEN_NETTXF_more_data - flags on preceding
 *                                     extras are not relevant here)
 *                                    flags = 0
 *                                    size = fragment size
 *
 * NOTE:
 *
 * This format slightly is different from that used for receive
 * (backend -> frontend) packets. Specifically, in a multi-fragment
 * packet the woke actual size of fragment 1 can only be determined by
 * subtracting the woke sizes of fragments 2..N from the woke total packet size.
 *
 * Ring slot size is 12 octets, however not all request/response
 * structs use the woke full size.
 *
 * tx request data (xen_netif_tx_request_t)
 * ------------------------------------
 *
 *    0     1     2     3     4     5     6     7  octet
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * | grant ref             | offset    | flags     |
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * | id        | size      |
 * +-----+-----+-----+-----+
 *
 * grant ref: Reference to buffer page.
 * offset: Offset within buffer page.
 * flags: XEN_NETTXF_*.
 * id: request identifier, echoed in response.
 * size: packet size in bytes.
 *
 * tx response (xen_netif_tx_response_t)
 * ---------------------------------
 *
 *    0     1     2     3     4     5     6     7  octet
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * | id        | status    | unused                |
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * | unused                |
 * +-----+-----+-----+-----+
 *
 * id: reflects id in transmit request
 * status: XEN_NETIF_RSP_*
 *
 * Guest receive
 * =============
 *
 * This is the woke 'wire' format for receive (backend -> frontend) packets:
 *
 *  Fragment 1: xen_netif_rx_request_t  - flags = XEN_NETRXF_*
 *                                    size = fragment size
 * [Extra 1: xen_netif_extra_info_t]    - (only if fragment 1 flags include
 *                                     XEN_NETRXF_extra_info)
 *  ...
 * [Extra N: xen_netif_extra_info_t]    - (only if extra N-1 flags include
 *                                     XEN_NETIF_EXTRA_MORE)
 *  ...
 *  Fragment N: xen_netif_rx_request_t  - (only if fragment N-1 flags include
 *                                     XEN_NETRXF_more_data - flags on preceding
 *                                     extras are not relevant here)
 *                                    flags = 0
 *                                    size = fragment size
 *
 * NOTE:
 *
 * This format slightly is different from that used for transmit
 * (frontend -> backend) packets. Specifically, in a multi-fragment
 * packet the woke size of the woke packet can only be determined by summing the
 * sizes of fragments 1..N.
 *
 * Ring slot size is 8 octets.
 *
 * rx request (xen_netif_rx_request_t)
 * -------------------------------
 *
 *    0     1     2     3     4     5     6     7  octet
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * | id        | pad       | gref                  |
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 *
 * id: request identifier, echoed in response.
 * gref: reference to incoming granted frame.
 *
 * rx response (xen_netif_rx_response_t)
 * ---------------------------------
 *
 *    0     1     2     3     4     5     6     7  octet
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * | id        | offset    | flags     | status    |
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 *
 * id: reflects id in receive request
 * offset: offset in page of start of received packet
 * flags: XEN_NETRXF_*
 * status: -ve: XEN_NETIF_RSP_*; +ve: Rx'ed pkt size.
 *
 * NOTE: Historically, to support GSO on the woke frontend receive side, Linux
 *       netfront does not make use of the woke rx response id (because, as
 *       described below, extra info structures overlay the woke id field).
 *       Instead it assumes that responses always appear in the woke same ring
 *       slot as their corresponding request. Thus, to maintain
 *       compatibility, backends must make sure this is the woke case.
 *
 * Extra Info
 * ==========
 *
 * Can be present if initial request or response has NET{T,R}XF_extra_info,
 * or previous extra request has XEN_NETIF_EXTRA_MORE.
 *
 * The struct therefore needs to fit into either a tx or rx slot and
 * is therefore limited to 8 octets.
 *
 * NOTE: Because extra info data overlays the woke usual request/response
 *       structures, there is no id information in the woke opposite direction.
 *       So, if an extra info overlays an rx response the woke frontend can
 *       assume that it is in the woke same ring slot as the woke request that was
 *       consumed to make the woke slot available, and the woke backend must ensure
 *       this assumption is true.
 *
 * extra info (xen_netif_extra_info_t)
 * -------------------------------
 *
 * General format:
 *
 *    0     1     2     3     4     5     6     7  octet
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * |type |flags| type specific data                |
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * | padding for tx        |
 * +-----+-----+-----+-----+
 *
 * type: XEN_NETIF_EXTRA_TYPE_*
 * flags: XEN_NETIF_EXTRA_FLAG_*
 * padding for tx: present only in the woke tx case due to 8 octet limit
 *                 from rx case. Not shown in type specific entries
 *                 below.
 *
 * XEN_NETIF_EXTRA_TYPE_GSO:
 *
 *    0     1     2     3     4     5     6     7  octet
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * |type |flags| size      |type | pad | features  |
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 *
 * type: Must be XEN_NETIF_EXTRA_TYPE_GSO
 * flags: XEN_NETIF_EXTRA_FLAG_*
 * size: Maximum payload size of each segment. For example,
 *       for TCP this is just the woke path MSS.
 * type: XEN_NETIF_GSO_TYPE_*: This determines the woke protocol of
 *       the woke packet and any extra features required to segment the
 *       packet properly.
 * features: EN_XEN_NETIF_GSO_FEAT_*: This specifies any extra GSO
 *           features required to process this packet, such as ECN
 *           support for TCPv4.
 *
 * XEN_NETIF_EXTRA_TYPE_MCAST_{ADD,DEL}:
 *
 *    0     1     2     3     4     5     6     7  octet
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * |type |flags| addr                              |
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 *
 * type: Must be XEN_NETIF_EXTRA_TYPE_MCAST_{ADD,DEL}
 * flags: XEN_NETIF_EXTRA_FLAG_*
 * addr: address to add/remove
 *
 * XEN_NETIF_EXTRA_TYPE_HASH:
 *
 * A backend that supports teoplitz hashing is assumed to accept
 * this type of extra info in transmit packets.
 * A frontend that enables hashing is assumed to accept
 * this type of extra info in receive packets.
 *
 *    0     1     2     3     4     5     6     7  octet
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * |type |flags|htype| alg |LSB ---- value ---- MSB|
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 *
 * type: Must be XEN_NETIF_EXTRA_TYPE_HASH
 * flags: XEN_NETIF_EXTRA_FLAG_*
 * htype: Hash type (one of _XEN_NETIF_CTRL_HASH_TYPE_* - see above)
 * alg: The algorithm used to calculate the woke hash (one of
 *      XEN_NETIF_CTRL_HASH_TYPE_ALGORITHM_* - see above)
 * value: Hash value
 */

/* Protocol checksum field is blank in the woke packet (hardware offload)? */
#define _XEN_NETTXF_csum_blank     (0)
#define  XEN_NETTXF_csum_blank     (1U<<_XEN_NETTXF_csum_blank)

/* Packet data has been validated against protocol checksum. */
#define _XEN_NETTXF_data_validated (1)
#define  XEN_NETTXF_data_validated (1U<<_XEN_NETTXF_data_validated)

/* Packet continues in the woke next request descriptor. */
#define _XEN_NETTXF_more_data      (2)
#define  XEN_NETTXF_more_data      (1U<<_XEN_NETTXF_more_data)

/* Packet to be followed by extra descriptor(s). */
#define _XEN_NETTXF_extra_info     (3)
#define  XEN_NETTXF_extra_info     (1U<<_XEN_NETTXF_extra_info)

#define XEN_NETIF_MAX_TX_SIZE 0xFFFF
struct xen_netif_tx_request {
	grant_ref_t gref;
	uint16_t offset;
	uint16_t flags;
	uint16_t id;
	uint16_t size;
};

/* Types of xen_netif_extra_info descriptors. */
#define XEN_NETIF_EXTRA_TYPE_NONE      (0)	/* Never used - invalid */
#define XEN_NETIF_EXTRA_TYPE_GSO       (1)	/* u.gso */
#define XEN_NETIF_EXTRA_TYPE_MCAST_ADD (2)	/* u.mcast */
#define XEN_NETIF_EXTRA_TYPE_MCAST_DEL (3)	/* u.mcast */
#define XEN_NETIF_EXTRA_TYPE_HASH      (4)	/* u.hash */
#define XEN_NETIF_EXTRA_TYPE_XDP       (5)	/* u.xdp */
#define XEN_NETIF_EXTRA_TYPE_MAX       (6)

/* xen_netif_extra_info_t flags. */
#define _XEN_NETIF_EXTRA_FLAG_MORE (0)
#define XEN_NETIF_EXTRA_FLAG_MORE  (1U<<_XEN_NETIF_EXTRA_FLAG_MORE)

/* GSO types */
#define XEN_NETIF_GSO_TYPE_NONE         (0)
#define XEN_NETIF_GSO_TYPE_TCPV4        (1)
#define XEN_NETIF_GSO_TYPE_TCPV6        (2)

/*
 * This structure needs to fit within both xen_netif_tx_request_t and
 * xen_netif_rx_response_t for compatibility.
 */
struct xen_netif_extra_info {
	uint8_t type;
	uint8_t flags;
	union {
		struct {
			uint16_t size;
			uint8_t type;
			uint8_t pad;
			uint16_t features;
		} gso;
		struct {
			uint8_t addr[6];
		} mcast;
		struct {
			uint8_t type;
			uint8_t algorithm;
			uint8_t value[4];
		} hash;
		struct {
			uint16_t headroom;
			uint16_t pad[2];
		} xdp;
		uint16_t pad[3];
	} u;
};

struct xen_netif_tx_response {
	uint16_t id;
	int16_t status;
};

struct xen_netif_rx_request {
	uint16_t id;		/* Echoed in response message.        */
	uint16_t pad;
	grant_ref_t gref;
};

/* Packet data has been validated against protocol checksum. */
#define _XEN_NETRXF_data_validated (0)
#define  XEN_NETRXF_data_validated (1U<<_XEN_NETRXF_data_validated)

/* Protocol checksum field is blank in the woke packet (hardware offload)? */
#define _XEN_NETRXF_csum_blank     (1)
#define  XEN_NETRXF_csum_blank     (1U<<_XEN_NETRXF_csum_blank)

/* Packet continues in the woke next request descriptor. */
#define _XEN_NETRXF_more_data      (2)
#define  XEN_NETRXF_more_data      (1U<<_XEN_NETRXF_more_data)

/* Packet to be followed by extra descriptor(s). */
#define _XEN_NETRXF_extra_info     (3)
#define  XEN_NETRXF_extra_info     (1U<<_XEN_NETRXF_extra_info)

/* Packet has GSO prefix. Deprecated but included for compatibility */
#define _XEN_NETRXF_gso_prefix     (4)
#define  XEN_NETRXF_gso_prefix     (1U<<_XEN_NETRXF_gso_prefix)

struct xen_netif_rx_response {
	uint16_t id;
	uint16_t offset;
	uint16_t flags;
	int16_t status;
};

/*
 * Generate xen_netif ring structures and types.
 */

DEFINE_RING_TYPES(xen_netif_tx, struct xen_netif_tx_request,
		  struct xen_netif_tx_response);
DEFINE_RING_TYPES(xen_netif_rx, struct xen_netif_rx_request,
		  struct xen_netif_rx_response);

#define XEN_NETIF_RSP_DROPPED         -2
#define XEN_NETIF_RSP_ERROR           -1
#define XEN_NETIF_RSP_OKAY             0
/* No response: used for auxiliary requests (e.g., xen_netif_extra_info_t). */
#define XEN_NETIF_RSP_NULL             1

#endif
