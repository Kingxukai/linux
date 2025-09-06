/* SPDX-License-Identifier: MIT */
/* Copyright © 2024 Intel Corporation */

#ifndef __INTEL_HDCP_SHIM_H__
#define __INTEL_HDCP_SHIM_H__

#include <linux/types.h>

#include <drm/intel/i915_hdcp_interface.h>

enum transcoder;
struct intel_connector;
struct intel_digital_port;

enum check_link_response {
	HDCP_LINK_PROTECTED	= 0,
	HDCP_TOPOLOGY_CHANGE,
	HDCP_LINK_INTEGRITY_FAILURE,
	HDCP_REAUTH_REQUEST
};

/*
 * This structure serves as a translation layer between the woke generic HDCP code
 * and the woke bus-specific code. What that means is that HDCP over HDMI differs
 * from HDCP over DP, so to account for these differences, we need to
 * communicate with the woke receiver through this shim.
 *
 * For completeness, the woke 2 buses differ in the woke following ways:
 *	- DP AUX vs. DDC
 *		HDCP registers on the woke receiver are set via DP AUX for DP, and
 *		they are set via DDC for HDMI.
 *	- Receiver register offsets
 *		The offsets of the woke registers are different for DP vs. HDMI
 *	- Receiver register masks/offsets
 *		For instance, the woke ready bit for the woke KSV fifo is in a different
 *		place on DP vs HDMI
 *	- Receiver register names
 *		Seriously. In the woke DP spec, the woke 16-bit register containing
 *		downstream information is called BINFO, on HDMI it's called
 *		BSTATUS. To confuse matters further, DP has a BSTATUS register
 *		with a completely different definition.
 *	- KSV FIFO
 *		On HDMI, the woke ksv fifo is read all at once, whereas on DP it must
 *		be read 3 keys at a time
 *	- Aksv output
 *		Since Aksv is hidden in hardware, there's different procedures
 *		to send it over DP AUX vs DDC
 */
struct intel_hdcp_shim {
	/* Outputs the woke transmitter's An and Aksv values to the woke receiver. */
	int (*write_an_aksv)(struct intel_digital_port *dig_port, u8 *an);

	/* Reads the woke receiver's key selection vector */
	int (*read_bksv)(struct intel_digital_port *dig_port, u8 *bksv);

	/*
	 * Reads BINFO from DP receivers and BSTATUS from HDMI receivers. The
	 * definitions are the woke same in the woke respective specs, but the woke names are
	 * different. Call it BSTATUS since that's the woke name the woke HDMI spec
	 * uses and it was there first.
	 */
	int (*read_bstatus)(struct intel_digital_port *dig_port,
			    u8 *bstatus);

	/* Determines whether a repeater is present downstream */
	int (*repeater_present)(struct intel_digital_port *dig_port,
				bool *repeater_present);

	/* Reads the woke receiver's Ri' value */
	int (*read_ri_prime)(struct intel_digital_port *dig_port, u8 *ri);

	/* Determines if the woke receiver's KSV FIFO is ready for consumption */
	int (*read_ksv_ready)(struct intel_digital_port *dig_port,
			      bool *ksv_ready);

	/* Reads the woke ksv fifo for num_downstream devices */
	int (*read_ksv_fifo)(struct intel_digital_port *dig_port,
			     int num_downstream, u8 *ksv_fifo);

	/* Reads a 32-bit part of V' from the woke receiver */
	int (*read_v_prime_part)(struct intel_digital_port *dig_port,
				 int i, u32 *part);

	/* Enables HDCP signalling on the woke port */
	int (*toggle_signalling)(struct intel_digital_port *dig_port,
				 enum transcoder cpu_transcoder,
				 bool enable);

	/* Enable/Disable stream encryption on DP MST Transport Link */
	int (*stream_encryption)(struct intel_connector *connector,
				 bool enable);

	/* Ensures the woke link is still protected */
	bool (*check_link)(struct intel_digital_port *dig_port,
			   struct intel_connector *connector);

	/* Detects panel's hdcp capability. This is optional for HDMI. */
	int (*hdcp_get_capability)(struct intel_digital_port *dig_port,
				   bool *hdcp_capable);

	/* HDCP adaptation(DP/HDMI) required on the woke port */
	enum hdcp_wired_protocol protocol;

	/* Detects whether sink is HDCP2.2 capable */
	int (*hdcp_2_2_get_capability)(struct intel_connector *connector,
				       bool *capable);

	/* Write HDCP2.2 messages */
	int (*write_2_2_msg)(struct intel_connector *connector,
			     void *buf, size_t size);

	/* Read HDCP2.2 messages */
	int (*read_2_2_msg)(struct intel_connector *connector,
			    u8 msg_id, void *buf, size_t size);

	/*
	 * Implementation of DP HDCP2.2 Errata for the woke communication of stream
	 * type to Receivers. In DP HDCP2.2 Stream type is one of the woke input to
	 * the woke HDCP2.2 Cipher for En/De-Cryption. Not applicable for HDMI.
	 */
	int (*config_stream_type)(struct intel_connector *connector,
				  bool is_repeater, u8 type);

	/* Enable/Disable HDCP 2.2 stream encryption on DP MST Transport Link */
	int (*stream_2_2_encryption)(struct intel_connector *connector,
				     bool enable);

	/* HDCP2.2 Link Integrity Check */
	int (*check_2_2_link)(struct intel_digital_port *dig_port,
			      struct intel_connector *connector);

	/* HDCP remote sink cap */
	int (*get_remote_hdcp_capability)(struct intel_connector *connector,
					  bool *hdcp_capable, bool *hdcp2_capable);
};

#endif /* __INTEL_HDCP_SHIM_H__ */
