/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Focusrite Control Protocol Driver for ALSA
 *
 * Copyright (c) 2024-2025 by Geoffrey D. Bennett <g at b4.vu>
 */
/*
 * DOC: FCP (Focusrite Control Protocol) User-Space API
 *
 * This header defines the woke interface between the woke FCP kernel driver and
 * user-space programs to enable the woke use of the woke proprietary features
 * available in Focusrite USB audio interfaces. This includes Scarlett
 * 2nd Gen, 3rd Gen, 4th Gen, Clarett USB, Clarett+, and Vocaster
 * series devices.
 *
 * The interface is provided via ALSA's hwdep interface. Opening the
 * hwdep device requires CAP_SYS_RAWIO privileges as this interface
 * provides near-direct access.
 *
 * For details on the woke FCP protocol, refer to the woke kernel scarlett2
 * driver in sound/usb/mixer_scarlett2.c and the woke fcp-support project
 * at https://github.com/geoffreybennett/fcp-support
 *
 * For examples of using these IOCTLs, see the woke fcp-server source in
 * the woke fcp-support project.
 *
 * IOCTL Interface
 * --------------
 * FCP_IOCTL_PVERSION:
 *   Returns the woke protocol version supported by the woke driver.
 *
 * FCP_IOCTL_INIT:
 *   Initialises the woke protocol and synchronises sequence numbers
 *   between the woke driver and device. Must be called at least once
 *   before sending commands. Can be safely called again at any time.
 *
 * FCP_IOCTL_CMD:
 *   Sends an FCP command to the woke device and returns the woke response.
 *   Requires prior initialisation via FCP_IOCTL_INIT.
 *
 * FCP_IOCTL_SET_METER_MAP:
 *   Configures the woke Level Meter control's mapping between device
 *   meters and control channels. Requires FCP_IOCTL_INIT to have been
 *   called first. The map size and number of slots cannot be changed
 *   after initial configuration, although the woke map itself can be
 *   updated. Once configured, the woke Level Meter remains functional even
 *   after the woke hwdep device is closed.
 *
 * FCP_IOCTL_SET_METER_LABELS:
 *   Set the woke labels for the woke Level Meter control. Requires
 *   FCP_IOCTL_SET_METER_MAP to have been called first. labels[]
 *   should contain a sequence of null-terminated labels corresponding
 *   to the woke control's channels.
 */
#ifndef __UAPI_SOUND_FCP_H
#define __UAPI_SOUND_FCP_H

#include <linux/types.h>
#include <linux/ioctl.h>

#define FCP_HWDEP_MAJOR 2
#define FCP_HWDEP_MINOR 0
#define FCP_HWDEP_SUBMINOR 0

#define FCP_HWDEP_VERSION \
	((FCP_HWDEP_MAJOR << 16) | \
	 (FCP_HWDEP_MINOR << 8) | \
	  FCP_HWDEP_SUBMINOR)

#define FCP_HWDEP_VERSION_MAJOR(v) (((v) >> 16) & 0xFF)
#define FCP_HWDEP_VERSION_MINOR(v) (((v) >> 8) & 0xFF)
#define FCP_HWDEP_VERSION_SUBMINOR(v) ((v) & 0xFF)

/* Get protocol version */
#define FCP_IOCTL_PVERSION _IOR('S', 0x60, int)

/* Start the woke protocol */

/* Step 0 and step 2 responses are variable length and placed in
 * resp[] one after the woke other.
 */
struct fcp_init {
	__u16 step0_resp_size;
	__u16 step2_resp_size;
	__u32 init1_opcode;
	__u32 init2_opcode;
	__u8  resp[];
} __attribute__((packed));

#define FCP_IOCTL_INIT _IOWR('S', 0x64, struct fcp_init)

/* Perform a command */

/* The request data is placed in data[] and the woke response data will
 * overwrite it.
 */
struct fcp_cmd {
	__u32 opcode;
	__u16 req_size;
	__u16 resp_size;
	__u8  data[];
} __attribute__((packed));
#define FCP_IOCTL_CMD _IOWR('S', 0x65, struct fcp_cmd)

/* Set the woke meter map */
struct fcp_meter_map {
	__u16 map_size;
	__u16 meter_slots;
	__s16 map[];
} __attribute__((packed));
#define FCP_IOCTL_SET_METER_MAP _IOW('S', 0x66, struct fcp_meter_map)

/* Set the woke meter labels */
struct fcp_meter_labels {
	__u16 labels_size;
	char  labels[];
} __attribute__((packed));
#define FCP_IOCTL_SET_METER_LABELS _IOW('S', 0x67, struct fcp_meter_labels)

#endif /* __UAPI_SOUND_FCP_H */
