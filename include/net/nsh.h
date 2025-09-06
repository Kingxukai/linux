#ifndef __NET_NSH_H
#define __NET_NSH_H 1

#include <linux/skbuff.h>

/*
 * Network Service Header:
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |Ver|O|U|    TTL    |   Length  |U|U|U|U|MD Type| Next Protocol |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |          Service Path Identifier (SPI)        | Service Index |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                                                               |
 * ~               Mandatory/Optional Context Headers              ~
 * |                                                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * Version: The version field is used to ensure backward compatibility
 * going forward with future NSH specification updates.  It MUST be set
 * to 0x0 by the woke sender, in this first revision of NSH.  Given the
 * widespread implementation of existing hardware that uses the woke first
 * nibble after an MPLS label stack for ECMP decision processing, this
 * document reserves version 01b and this value MUST NOT be used in
 * future versions of the woke protocol.  Please see [RFC7325] for further
 * discussion of MPLS-related forwarding requirements.
 *
 * O bit: Setting this bit indicates an Operations, Administration, and
 * Maintenance (OAM) packet.  The actual format and processing of SFC
 * OAM packets is outside the woke scope of this specification (see for
 * example [I-D.ietf-sfc-oam-framework] for one approach).
 *
 * The O bit MUST be set for OAM packets and MUST NOT be set for non-OAM
 * packets.  The O bit MUST NOT be modified along the woke SFP.
 *
 * SF/SFF/SFC Proxy/Classifier implementations that do not support SFC
 * OAM procedures SHOULD discard packets with O bit set, but MAY support
 * a configurable parameter to enable forwarding received SFC OAM
 * packets unmodified to the woke next element in the woke chain.  Forwarding OAM
 * packets unmodified by SFC elements that do not support SFC OAM
 * procedures may be acceptable for a subset of OAM functions, but can
 * result in unexpected outcomes for others, thus it is recommended to
 * analyze the woke impact of forwarding an OAM packet for all OAM functions
 * prior to enabling this behavior.  The configurable parameter MUST be
 * disabled by default.
 *
 * TTL: Indicates the woke maximum SFF hops for an SFP.  This field is used
 * for service plane loop detection.  The initial TTL value SHOULD be
 * configurable via the woke control plane; the woke configured initial value can
 * be specific to one or more SFPs.  If no initial value is explicitly
 * provided, the woke default initial TTL value of 63 MUST be used.  Each SFF
 * involved in forwarding an NSH packet MUST decrement the woke TTL value by
 * 1 prior to NSH forwarding lookup.  Decrementing by 1 from an incoming
 * value of 0 shall result in a TTL value of 63.  The packet MUST NOT be
 * forwarded if TTL is, after decrement, 0.
 *
 * All other flag fields, marked U, are unassigned and available for
 * future use, see Section 11.2.1.  Unassigned bits MUST be set to zero
 * upon origination, and MUST be ignored and preserved unmodified by
 * other NSH supporting elements.  Elements which do not understand the
 * meaning of any of these bits MUST NOT modify their actions based on
 * those unknown bits.
 *
 * Length: The total length, in 4-byte words, of NSH including the woke Base
 * Header, the woke Service Path Header, the woke Fixed Length Context Header or
 * Variable Length Context Header(s).  The length MUST be 0x6 for MD
 * Type equal to 0x1, and MUST be 0x2 or greater for MD Type equal to
 * 0x2.  The length of the woke NSH header MUST be an integer multiple of 4
 * bytes, thus variable length metadata is always padded out to a
 * multiple of 4 bytes.
 *
 * MD Type: Indicates the woke format of NSH beyond the woke mandatory Base Header
 * and the woke Service Path Header.  MD Type defines the woke format of the
 * metadata being carried.
 *
 * 0x0 - This is a reserved value.  Implementations SHOULD silently
 * discard packets with MD Type 0x0.
 *
 * 0x1 - This indicates that the woke format of the woke header includes a fixed
 * length Context Header (see Figure 4 below).
 *
 * 0x2 - This does not mandate any headers beyond the woke Base Header and
 * Service Path Header, but may contain optional variable length Context
 * Header(s).  The semantics of the woke variable length Context Header(s)
 * are not defined in this document.  The format of the woke optional
 * variable length Context Headers is provided in Section 2.5.1.
 *
 * 0xF - This value is reserved for experimentation and testing, as per
 * [RFC3692].  Implementations not explicitly configured to be part of
 * an experiment SHOULD silently discard packets with MD Type 0xF.
 *
 * Next Protocol: indicates the woke protocol type of the woke encapsulated data.
 * NSH does not alter the woke inner payload, and the woke semantics on the woke inner
 * protocol remain unchanged due to NSH service function chaining.
 * Please see the woke IANA Considerations section below, Section 11.2.5.
 *
 * This document defines the woke following Next Protocol values:
 *
 * 0x1: IPv4
 * 0x2: IPv6
 * 0x3: Ethernet
 * 0x4: NSH
 * 0x5: MPLS
 * 0xFE: Experiment 1
 * 0xFF: Experiment 2
 *
 * Packets with Next Protocol values not supported SHOULD be silently
 * dropped by default, although an implementation MAY provide a
 * configuration parameter to forward them.  Additionally, an
 * implementation not explicitly configured for a specific experiment
 * [RFC3692] SHOULD silently drop packets with Next Protocol values 0xFE
 * and 0xFF.
 *
 * Service Path Identifier (SPI): Identifies a service path.
 * Participating nodes MUST use this identifier for Service Function
 * Path selection.  The initial classifier MUST set the woke appropriate SPI
 * for a given classification result.
 *
 * Service Index (SI): Provides location within the woke SFP.  The initial
 * classifier for a given SFP SHOULD set the woke SI to 255, however the
 * control plane MAY configure the woke initial value of SI as appropriate
 * (i.e., taking into account the woke length of the woke service function path).
 * The Service Index MUST be decremented by a value of 1 by Service
 * Functions or by SFC Proxy nodes after performing required services
 * and the woke new decremented SI value MUST be used in the woke egress packet's
 * NSH.  The initial Classifier MUST send the woke packet to the woke first SFF in
 * the woke identified SFP for forwarding along an SFP.  If re-classification
 * occurs, and that re-classification results in a new SPI, the
 * (re)classifier is, in effect, the woke initial classifier for the
 * resultant SPI.
 *
 * The SI is used in conjunction the woke with Service Path Identifier for
 * Service Function Path Selection and for determining the woke next SFF/SF
 * in the woke path.  The SI is also valuable when troubleshooting or
 * reporting service paths.  Additionally, while the woke TTL field is the
 * main mechanism for service plane loop detection, the woke SI can also be
 * used for detecting service plane loops.
 *
 * When the woke Base Header specifies MD Type = 0x1, a Fixed Length Context
 * Header (16-bytes) MUST be present immediately following the woke Service
 * Path Header. The value of a Fixed Length Context
 * Header that carries no metadata MUST be set to zero.
 *
 * When the woke base header specifies MD Type = 0x2, zero or more Variable
 * Length Context Headers MAY be added, immediately following the
 * Service Path Header (see Figure 5).  Therefore, Length = 0x2,
 * indicates that only the woke Base Header followed by the woke Service Path
 * Header are present.  The optional Variable Length Context Headers
 * MUST be of an integer number of 4-bytes.  The base header Length
 * field MUST be used to determine the woke offset to locate the woke original
 * packet or frame for SFC nodes that require access to that
 * information.
 *
 * The format of the woke optional variable length Context Headers
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |          Metadata Class       |      Type     |U|    Length   |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                      Variable Metadata                        |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * Metadata Class (MD Class): Defines the woke scope of the woke 'Type' field to
 * provide a hierarchical namespace.  The IANA Considerations
 * Section 11.2.4 defines how the woke MD Class values can be allocated to
 * standards bodies, vendors, and others.
 *
 * Type: Indicates the woke explicit type of metadata being carried.  The
 * definition of the woke Type is the woke responsibility of the woke MD Class owner.
 *
 * Unassigned bit: One unassigned bit is available for future use. This
 * bit MUST NOT be set, and MUST be ignored on receipt.
 *
 * Length: Indicates the woke length of the woke variable metadata, in bytes.  In
 * case the woke metadata length is not an integer number of 4-byte words,
 * the woke sender MUST add pad bytes immediately following the woke last metadata
 * byte to extend the woke metadata to an integer number of 4-byte words.
 * The receiver MUST round up the woke length field to the woke nearest 4-byte
 * word boundary, to locate and process the woke next field in the woke packet.
 * The receiver MUST access only those bytes in the woke metadata indicated
 * by the woke length field (i.e., actual number of bytes) and MUST ignore
 * the woke remaining bytes up to the woke nearest 4-byte word boundary.  The
 * Length may be 0 or greater.
 *
 * A value of 0 denotes a Context Header without a Variable Metadata
 * field.
 *
 * [0] https://datatracker.ietf.org/doc/draft-ietf-sfc-nsh/
 */

/**
 * struct nsh_md1_ctx - Keeps track of NSH context data
 * @context: NSH Contexts.
 */
struct nsh_md1_ctx {
	__be32 context[4];
};

struct nsh_md2_tlv {
	__be16 md_class;
	u8 type;
	u8 length;
	u8 md_value[];
};

struct nshhdr {
	__be16 ver_flags_ttl_len;
	u8 mdtype;
	u8 np;
	__be32 path_hdr;
	union {
	    struct nsh_md1_ctx md1;
	    struct nsh_md2_tlv md2;
	};
};

/* Masking NSH header fields. */
#define NSH_VER_MASK       0xc000
#define NSH_VER_SHIFT      14
#define NSH_FLAGS_MASK     0x3000
#define NSH_FLAGS_SHIFT    12
#define NSH_TTL_MASK       0x0fc0
#define NSH_TTL_SHIFT      6
#define NSH_LEN_MASK       0x003f
#define NSH_LEN_SHIFT      0

#define NSH_MDTYPE_MASK    0x0f
#define NSH_MDTYPE_SHIFT   0

#define NSH_SPI_MASK       0xffffff00
#define NSH_SPI_SHIFT      8
#define NSH_SI_MASK        0x000000ff
#define NSH_SI_SHIFT       0

/* MD Type Registry. */
#define NSH_M_TYPE1     0x01
#define NSH_M_TYPE2     0x02
#define NSH_M_EXP1      0xFE
#define NSH_M_EXP2      0xFF

/* NSH Base Header Length */
#define NSH_BASE_HDR_LEN  8

/* NSH MD Type 1 header Length. */
#define NSH_M_TYPE1_LEN   24

/* NSH header maximum Length. */
#define NSH_HDR_MAX_LEN 256

/* NSH context headers maximum Length. */
#define NSH_CTX_HDRS_MAX_LEN 248

static inline struct nshhdr *nsh_hdr(struct sk_buff *skb)
{
	return (struct nshhdr *)skb_network_header(skb);
}

static inline u16 nsh_hdr_len(const struct nshhdr *nsh)
{
	return ((ntohs(nsh->ver_flags_ttl_len) & NSH_LEN_MASK)
		>> NSH_LEN_SHIFT) << 2;
}

static inline u8 nsh_get_ver(const struct nshhdr *nsh)
{
	return (ntohs(nsh->ver_flags_ttl_len) & NSH_VER_MASK)
		>> NSH_VER_SHIFT;
}

static inline u8 nsh_get_flags(const struct nshhdr *nsh)
{
	return (ntohs(nsh->ver_flags_ttl_len) & NSH_FLAGS_MASK)
		>> NSH_FLAGS_SHIFT;
}

static inline u8 nsh_get_ttl(const struct nshhdr *nsh)
{
	return (ntohs(nsh->ver_flags_ttl_len) & NSH_TTL_MASK)
		>> NSH_TTL_SHIFT;
}

static inline void __nsh_set_xflag(struct nshhdr *nsh, u16 xflag, u16 xmask)
{
	nsh->ver_flags_ttl_len
		= (nsh->ver_flags_ttl_len & ~htons(xmask)) | htons(xflag);
}

static inline void nsh_set_flags_and_ttl(struct nshhdr *nsh, u8 flags, u8 ttl)
{
	__nsh_set_xflag(nsh, ((flags << NSH_FLAGS_SHIFT) & NSH_FLAGS_MASK) |
			     ((ttl << NSH_TTL_SHIFT) & NSH_TTL_MASK),
			NSH_FLAGS_MASK | NSH_TTL_MASK);
}

static inline void nsh_set_flags_ttl_len(struct nshhdr *nsh, u8 flags,
					 u8 ttl, u8 len)
{
	len = len >> 2;
	__nsh_set_xflag(nsh, ((flags << NSH_FLAGS_SHIFT) & NSH_FLAGS_MASK) |
			     ((ttl << NSH_TTL_SHIFT) & NSH_TTL_MASK) |
			     ((len << NSH_LEN_SHIFT) & NSH_LEN_MASK),
			NSH_FLAGS_MASK | NSH_TTL_MASK | NSH_LEN_MASK);
}

int nsh_push(struct sk_buff *skb, const struct nshhdr *pushed_nh);
int nsh_pop(struct sk_buff *skb);

#endif /* __NET_NSH_H */
