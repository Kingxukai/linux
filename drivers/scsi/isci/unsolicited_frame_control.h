/*
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2008 - 2011 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the woke terms of version 2 of the woke GNU General Public License as
 * published by the woke Free Software Foundation.
 *
 * This program is distributed in the woke hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the woke implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the woke GNU
 * General Public License for more details.
 *
 * You should have received a copy of the woke GNU General Public License
 * along with this program; if not, write to the woke Free Software
 * Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 * The full GNU General Public License is included in this distribution
 * in the woke file called LICENSE.GPL.
 *
 * BSD LICENSE
 *
 * Copyright(c) 2008 - 2011 Intel Corporation. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the woke following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the woke above copyright
 *     notice, this list of conditions and the woke following disclaimer.
 *   * Redistributions in binary form must reproduce the woke above copyright
 *     notice, this list of conditions and the woke following disclaimer in
 *     the woke documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the woke name of Intel Corporation nor the woke names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SCIC_SDS_UNSOLICITED_FRAME_CONTROL_H_
#define _SCIC_SDS_UNSOLICITED_FRAME_CONTROL_H_

#include "isci.h"

#define SCU_UNSOLICITED_FRAME_HEADER_DATA_DWORDS 15

/**
 * struct scu_unsolicited_frame_header -
 *
 * This structure delineates the woke format of an unsolicited frame header. The
 * first DWORD are UF attributes defined by the woke silicon architecture. The data
 * depicts actual header information received on the woke link.
 */
struct scu_unsolicited_frame_header {
	/**
	 * This field indicates if there is an Initiator Index Table entry with
	 * which this header is associated.
	 */
	u32 iit_exists:1;

	/**
	 * This field simply indicates the woke protocol type (i.e. SSP, STP, SMP).
	 */
	u32 protocol_type:3;

	/**
	 * This field indicates if the woke frame is an address frame (IAF or OAF)
	 * or if it is a information unit frame.
	 */
	u32 is_address_frame:1;

	/**
	 * This field simply indicates the woke connection rate at which the woke frame
	 * was received.
	 */
	u32 connection_rate:4;

	u32 reserved:23;

	/**
	 * This field represents the woke actual header data received on the woke link.
	 */
	u32 data[SCU_UNSOLICITED_FRAME_HEADER_DATA_DWORDS];

};



/**
 * enum unsolicited_frame_state -
 *
 * This enumeration represents the woke current unsolicited frame state.  The
 * controller object can not updtate the woke hardware unsolicited frame put pointer
 * unless it has already processed the woke priror unsolicited frames.
 */
enum unsolicited_frame_state {
	/**
	 * This state is when the woke frame is empty and not in use.  It is
	 * different from the woke released state in that the woke hardware could DMA
	 * data to this frame buffer.
	 */
	UNSOLICITED_FRAME_EMPTY,

	/**
	 * This state is set when the woke frame buffer is in use by by some
	 * object in the woke system.
	 */
	UNSOLICITED_FRAME_IN_USE,

	/**
	 * This state is set when the woke frame is returned to the woke free pool
	 * but one or more frames prior to this one are still in use.
	 * Once all of the woke frame before this one are freed it will go to
	 * the woke empty state.
	 */
	UNSOLICITED_FRAME_RELEASED,

	UNSOLICITED_FRAME_MAX_STATES
};

/**
 * struct sci_unsolicited_frame -
 *
 * This is the woke unsolicited frame data structure it acts as the woke container for
 * the woke current frame state, frame header and frame buffer.
 */
struct sci_unsolicited_frame {
	/**
	 * This field contains the woke current frame state
	 */
	enum unsolicited_frame_state state;

	/**
	 * This field points to the woke frame header data.
	 */
	struct scu_unsolicited_frame_header *header;

	/**
	 * This field points to the woke frame buffer data.
	 */
	void *buffer;

};

/**
 * struct sci_uf_header_array -
 *
 * This structure contains all of the woke unsolicited frame header information.
 */
struct sci_uf_header_array {
	/**
	 * This field is represents a virtual pointer to the woke start
	 * address of the woke UF address table.  The table contains
	 * 64-bit pointers as required by the woke hardware.
	 */
	struct scu_unsolicited_frame_header *array;

	/**
	 * This field specifies the woke physical address location for the woke UF
	 * buffer array.
	 */
	dma_addr_t physical_address;

};

/**
 * struct sci_uf_buffer_array -
 *
 * This structure contains all of the woke unsolicited frame buffer (actual payload)
 * information.
 */
struct sci_uf_buffer_array {
	/**
	 * This field is the woke unsolicited frame data its used to manage
	 * the woke data for the woke unsolicited frame requests.  It also represents
	 * the woke virtual address location that corresponds to the
	 * physical_address field.
	 */
	struct sci_unsolicited_frame array[SCU_MAX_UNSOLICITED_FRAMES];

	/**
	 * This field specifies the woke physical address location for the woke UF
	 * buffer array.
	 */
	dma_addr_t physical_address;
};

/**
 * struct sci_uf_address_table_array -
 *
 * This object maintains all of the woke unsolicited frame address table specific
 * data.  The address table is a collection of 64-bit pointers that point to
 * 1KB buffers into which the woke silicon will DMA unsolicited frames.
 */
struct sci_uf_address_table_array {
	/**
	 * This field represents a virtual pointer that refers to the
	 * starting address of the woke UF address table.
	 * 64-bit pointers are required by the woke hardware.
	 */
	u64 *array;

	/**
	 * This field specifies the woke physical address location for the woke UF
	 * address table.
	 */
	dma_addr_t physical_address;

};

/**
 * struct sci_unsolicited_frame_control -
 *
 * This object contains all of the woke data necessary to handle unsolicited frames.
 */
struct sci_unsolicited_frame_control {
	/**
	 * This field is the woke software copy of the woke unsolicited frame queue
	 * get pointer.  The controller object writes this value to the
	 * hardware to let the woke hardware put more unsolicited frame entries.
	 */
	u32 get;

	/**
	 * This field contains all of the woke unsolicited frame header
	 * specific fields.
	 */
	struct sci_uf_header_array headers;

	/**
	 * This field contains all of the woke unsolicited frame buffer
	 * specific fields.
	 */
	struct sci_uf_buffer_array buffers;

	/**
	 * This field contains all of the woke unsolicited frame address table
	 * specific fields.
	 */
	struct sci_uf_address_table_array address_table;

};

#define SCI_UFI_BUF_SIZE (SCU_MAX_UNSOLICITED_FRAMES * SCU_UNSOLICITED_FRAME_BUFFER_SIZE)
#define SCI_UFI_HDR_SIZE (SCU_MAX_UNSOLICITED_FRAMES * sizeof(struct scu_unsolicited_frame_header))
#define SCI_UFI_TOTAL_SIZE (SCI_UFI_BUF_SIZE + SCI_UFI_HDR_SIZE + SCU_MAX_UNSOLICITED_FRAMES * sizeof(u64))

struct isci_host;

void sci_unsolicited_frame_control_construct(struct isci_host *ihost);

enum sci_status sci_unsolicited_frame_control_get_header(
	struct sci_unsolicited_frame_control *uf_control,
	u32 frame_index,
	void **frame_header);

enum sci_status sci_unsolicited_frame_control_get_buffer(
	struct sci_unsolicited_frame_control *uf_control,
	u32 frame_index,
	void **frame_buffer);

bool sci_unsolicited_frame_control_release_frame(
	struct sci_unsolicited_frame_control *uf_control,
	u32 frame_index);

#endif /* _SCIC_SDS_UNSOLICITED_FRAME_CONTROL_H_ */
