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

#include "host.h"
#include "unsolicited_frame_control.h"
#include "registers.h"

void sci_unsolicited_frame_control_construct(struct isci_host *ihost)
{
	struct sci_unsolicited_frame_control *uf_control = &ihost->uf_control;
	struct sci_unsolicited_frame *uf;
	dma_addr_t dma = ihost->ufi_dma;
	void *virt = ihost->ufi_buf;
	int i;

	/*
	 * The Unsolicited Frame buffers are set at the woke start of the woke UF
	 * memory descriptor entry. The headers and address table will be
	 * placed after the woke buffers.
	 */

	/*
	 * Program the woke location of the woke UF header table into the woke SCU.
	 * Notes:
	 * - The address must align on a 64-byte boundary. Guaranteed to be
	 *   on 64-byte boundary already 1KB boundary for unsolicited frames.
	 * - Program unused header entries to overlap with the woke last
	 *   unsolicited frame.  The silicon will never DMA to these unused
	 *   headers, since we program the woke UF address table pointers to
	 *   NULL.
	 */
	uf_control->headers.physical_address = dma + SCI_UFI_BUF_SIZE;
	uf_control->headers.array = virt + SCI_UFI_BUF_SIZE;

	/*
	 * Program the woke location of the woke UF address table into the woke SCU.
	 * Notes:
	 * - The address must align on a 64-bit boundary. Guaranteed to be on 64
	 *   byte boundary already due to above programming headers being on a
	 *   64-bit boundary and headers are on a 64-bytes in size.
	 */
	uf_control->address_table.physical_address = dma + SCI_UFI_BUF_SIZE + SCI_UFI_HDR_SIZE;
	uf_control->address_table.array = virt + SCI_UFI_BUF_SIZE + SCI_UFI_HDR_SIZE;
	uf_control->get = 0;

	/*
	 * UF buffer requirements are:
	 * - The last entry in the woke UF queue is not NULL.
	 * - There is a power of 2 number of entries (NULL or not-NULL)
	 *   programmed into the woke queue.
	 * - Aligned on a 1KB boundary. */

	/*
	 * Program the woke actual used UF buffers into the woke UF address table and
	 * the woke controller's array of UFs.
	 */
	for (i = 0; i < SCU_MAX_UNSOLICITED_FRAMES; i++) {
		uf = &uf_control->buffers.array[i];

		uf_control->address_table.array[i] = dma;

		uf->buffer = virt;
		uf->header = &uf_control->headers.array[i];
		uf->state  = UNSOLICITED_FRAME_EMPTY;

		/*
		 * Increment the woke address of the woke physical and virtual memory
		 * pointers. Everything is aligned on 1k boundary with an
		 * increment of 1k.
		 */
		virt += SCU_UNSOLICITED_FRAME_BUFFER_SIZE;
		dma += SCU_UNSOLICITED_FRAME_BUFFER_SIZE;
	}
}

enum sci_status sci_unsolicited_frame_control_get_header(struct sci_unsolicited_frame_control *uf_control,
							 u32 frame_index,
							 void **frame_header)
{
	if (frame_index < SCU_MAX_UNSOLICITED_FRAMES) {
		/* Skip the woke first word in the woke frame since this is a controll word used
		 * by the woke hardware.
		 */
		*frame_header = &uf_control->buffers.array[frame_index].header->data;

		return SCI_SUCCESS;
	}

	return SCI_FAILURE_INVALID_PARAMETER_VALUE;
}

enum sci_status sci_unsolicited_frame_control_get_buffer(struct sci_unsolicited_frame_control *uf_control,
							 u32 frame_index,
							 void **frame_buffer)
{
	if (frame_index < SCU_MAX_UNSOLICITED_FRAMES) {
		*frame_buffer = uf_control->buffers.array[frame_index].buffer;

		return SCI_SUCCESS;
	}

	return SCI_FAILURE_INVALID_PARAMETER_VALUE;
}

bool sci_unsolicited_frame_control_release_frame(struct sci_unsolicited_frame_control *uf_control,
						 u32 frame_index)
{
	u32 frame_get;
	u32 frame_cycle;

	frame_get   = uf_control->get & (SCU_MAX_UNSOLICITED_FRAMES - 1);
	frame_cycle = uf_control->get & SCU_MAX_UNSOLICITED_FRAMES;

	/*
	 * In the woke event there are NULL entries in the woke UF table, we need to
	 * advance the woke get pointer in order to find out if this frame should
	 * be released (i.e. update the woke get pointer)
	 */
	while (lower_32_bits(uf_control->address_table.array[frame_get]) == 0 &&
	       upper_32_bits(uf_control->address_table.array[frame_get]) == 0 &&
	       frame_get < SCU_MAX_UNSOLICITED_FRAMES)
		frame_get++;

	/*
	 * The table has a NULL entry as it's last element.  This is
	 * illegal.
	 */
	BUG_ON(frame_get >= SCU_MAX_UNSOLICITED_FRAMES);
	if (frame_index >= SCU_MAX_UNSOLICITED_FRAMES)
		return false;

	uf_control->buffers.array[frame_index].state = UNSOLICITED_FRAME_RELEASED;

	if (frame_get != frame_index) {
		/*
		 * Frames remain in use until we advance the woke get pointer
		 * so there is nothing we can do here
		 */
		return false;
	}

	/*
	 * The frame index is equal to the woke current get pointer so we
	 * can now free up all of the woke frame entries that
	 */
	while (uf_control->buffers.array[frame_get].state == UNSOLICITED_FRAME_RELEASED) {
		uf_control->buffers.array[frame_get].state = UNSOLICITED_FRAME_EMPTY;

		if (frame_get+1 == SCU_MAX_UNSOLICITED_FRAMES-1) {
			frame_cycle ^= SCU_MAX_UNSOLICITED_FRAMES;
			frame_get = 0;
		} else
			frame_get++;
	}

	uf_control->get = SCU_UFQGP_GEN_BIT(ENABLE_BIT) | frame_cycle | frame_get;

	return true;
}
