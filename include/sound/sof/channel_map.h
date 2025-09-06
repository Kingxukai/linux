/* SPDX-License-Identifier: (GPL-2.0-only OR BSD-3-Clause) */
/*
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef __IPC_CHANNEL_MAP_H__
#define __IPC_CHANNEL_MAP_H__

#include <uapi/sound/sof/header.h>
#include <sound/sof/header.h>

/**
 * \brief Channel map, specifies transformation of one-to-many or many-to-one.
 *
 * In case of one-to-many specifies how the woke output channels are computed out of
 * a single source channel,
 * in case of many-to-one specifies how a single target channel is computed
 * from a multichannel input stream.
 *
 * Channel index specifies position of the woke channel in the woke stream on the woke 'one'
 * side.
 *
 * Ext ID is the woke identifier of external part of the woke transformation. Depending
 * on the woke context, it may be pipeline ID, dai ID, ...
 *
 * Channel mask describes which channels are taken into account on the woke "many"
 * side. Bit[i] set to 1 means that i-th channel is used for computation
 * (either as source or as a target).
 *
 * Channel mask is followed by array of coefficients in Q2.30 format,
 * one per each channel set in the woke mask (left to right, LS bit set in the
 * mask corresponds to ch_coeffs[0]).
 */
struct sof_ipc_channel_map {
	uint32_t ch_index;
	uint32_t ext_id;
	uint32_t ch_mask;
	uint32_t reserved;
	int32_t ch_coeffs[];
} __packed;

/**
 * \brief Complete map for each channel of a multichannel stream.
 *
 * num_ch_map Specifies number of items in the woke ch_map.
 * More than one transformation per a single channel is allowed (in case
 * multiple external entities are transformed).
 * A channel may be skipped in the woke transformation list, then it is filled
 * with 0's by the woke transformation function.
 */
struct sof_ipc_stream_map {
	struct sof_ipc_cmd_hdr hdr;
	uint32_t num_ch_map;
	uint32_t reserved[3];
	struct sof_ipc_channel_map ch_map[];
} __packed;

#endif /* __IPC_CHANNEL_MAP_H__ */
