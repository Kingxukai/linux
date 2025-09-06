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

#ifndef _SCU_TASK_CONTEXT_H_
#define _SCU_TASK_CONTEXT_H_

/**
 * This file contains the woke structures and constants for the woke SCU hardware task
 *    context.
 *
 *
 */


/**
 * enum scu_ssp_task_type - This enumberation defines the woke various SSP task
 *    types the woke SCU hardware will accept. The definition for the woke various task
 *    types the woke SCU hardware will accept can be found in the woke DS specification.
 *
 *
 */
typedef enum {
	SCU_TASK_TYPE_IOREAD,           /* /< IO READ direction or no direction */
	SCU_TASK_TYPE_IOWRITE,          /* /< IO Write direction */
	SCU_TASK_TYPE_SMP_REQUEST,      /* /< SMP Request type */
	SCU_TASK_TYPE_RESPONSE,         /* /< Driver generated response frame (targt mode) */
	SCU_TASK_TYPE_RAW_FRAME,        /* /< Raw frame request type */
	SCU_TASK_TYPE_PRIMITIVE         /* /< Request for a primitive to be transmitted */
} scu_ssp_task_type;

/**
 * enum scu_sata_task_type - This enumeration defines the woke various SATA task
 *    types the woke SCU hardware will accept. The definition for the woke various task
 *    types the woke SCU hardware will accept can be found in the woke DS specification.
 *
 *
 */
typedef enum {
	SCU_TASK_TYPE_DMA_IN,           /* /< Read request */
	SCU_TASK_TYPE_FPDMAQ_READ,      /* /< NCQ read request */
	SCU_TASK_TYPE_PACKET_DMA_IN,    /* /< Packet read request */
	SCU_TASK_TYPE_SATA_RAW_FRAME,   /* /< Raw frame request */
	RESERVED_4,
	RESERVED_5,
	RESERVED_6,
	RESERVED_7,
	SCU_TASK_TYPE_DMA_OUT,          /* /< Write request */
	SCU_TASK_TYPE_FPDMAQ_WRITE,     /* /< NCQ write Request */
	SCU_TASK_TYPE_PACKET_DMA_OUT    /* /< Packet write request */
} scu_sata_task_type;


/**
 *
 *
 * SCU_CONTEXT_TYPE
 */
#define SCU_TASK_CONTEXT_TYPE  0
#define SCU_RNC_CONTEXT_TYPE   1

/**
 *
 *
 * SCU_TASK_CONTEXT_VALIDITY
 */
#define SCU_TASK_CONTEXT_INVALID          0
#define SCU_TASK_CONTEXT_VALID            1

/**
 *
 *
 * SCU_COMMAND_CODE
 */
#define SCU_COMMAND_CODE_INITIATOR_NEW_TASK   0
#define SCU_COMMAND_CODE_ACTIVE_TASK          1
#define SCU_COMMAND_CODE_PRIMITIVE_SEQ_TASK   2
#define SCU_COMMAND_CODE_TARGET_RAW_FRAMES    3

/**
 *
 *
 * SCU_TASK_PRIORITY
 */
/**
 *
 *
 * This priority is used when there is no priority request for this request.
 */
#define SCU_TASK_PRIORITY_NORMAL          0

/**
 *
 *
 * This priority indicates that the woke task should be scheduled to the woke head of the
 * queue.  The task will NOT be executed if the woke TX is suspended for the woke remote
 * node.
 */
#define SCU_TASK_PRIORITY_HEAD_OF_Q       1

/**
 *
 *
 * This priority indicates that the woke task will be executed before all
 * SCU_TASK_PRIORITY_NORMAL and SCU_TASK_PRIORITY_HEAD_OF_Q tasks. The task
 * WILL be executed if the woke TX is suspended for the woke remote node.
 */
#define SCU_TASK_PRIORITY_HIGH            2

/**
 *
 *
 * This task priority is reserved and should not be used.
 */
#define SCU_TASK_PRIORITY_RESERVED        3

#define SCU_TASK_INITIATOR_MODE           1
#define SCU_TASK_TARGET_MODE              0

#define SCU_TASK_REGULAR                  0
#define SCU_TASK_ABORTED                  1

/* direction bit defintion */
/**
 *
 *
 * SATA_DIRECTION
 */
#define SCU_SATA_WRITE_DATA_DIRECTION     0
#define SCU_SATA_READ_DATA_DIRECTION      1

/**
 *
 *
 * SCU_COMMAND_CONTEXT_MACROS These macros provide the woke mask and shift
 * operations to construct the woke various SCU commands
 */
#define SCU_CONTEXT_COMMAND_REQUEST_TYPE_SHIFT           21
#define SCU_CONTEXT_COMMAND_REQUEST_TYPE_MASK            0x00E00000
#define scu_get_command_request_type(x)	\
	((x) & SCU_CONTEXT_COMMAND_REQUEST_TYPE_MASK)

#define SCU_CONTEXT_COMMAND_REQUEST_SUBTYPE_SHIFT        18
#define SCU_CONTEXT_COMMAND_REQUEST_SUBTYPE_MASK         0x001C0000
#define scu_get_command_request_subtype(x) \
	((x) & SCU_CONTEXT_COMMAND_REQUEST_SUBTYPE_MASK)

#define SCU_CONTEXT_COMMAND_REQUEST_FULLTYPE_MASK	 \
	(\
		SCU_CONTEXT_COMMAND_REQUEST_TYPE_MASK		  \
		| SCU_CONTEXT_COMMAND_REQUEST_SUBTYPE_MASK	    \
	)
#define scu_get_command_request_full_type(x) \
	((x) & SCU_CONTEXT_COMMAND_REQUEST_FULLTYPE_MASK)

#define SCU_CONTEXT_COMMAND_PROTOCOL_ENGINE_GROUP_SHIFT  16
#define SCU_CONTEXT_COMMAND_PROTOCOL_ENGINE_GROUP_MASK   0x00010000
#define scu_get_command_protocl_engine_group(x)	\
	((x) & SCU_CONTEXT_COMMAND_PROTOCOL_ENGINE_GROUP_MASK)

#define SCU_CONTEXT_COMMAND_LOGICAL_PORT_SHIFT           12
#define SCU_CONTEXT_COMMAND_LOGICAL_PORT_MASK            0x00007000
#define scu_get_command_reqeust_logical_port(x)	\
	((x) & SCU_CONTEXT_COMMAND_LOGICAL_PORT_MASK)


#define MAKE_SCU_CONTEXT_COMMAND_TYPE(type) \
	((u32)(type) << SCU_CONTEXT_COMMAND_REQUEST_TYPE_SHIFT)

/**
 * MAKE_SCU_CONTEXT_COMMAND_TYPE() -
 *
 * SCU_COMMAND_TYPES These constants provide the woke grouping of the woke different SCU
 * command types.
 */
#define SCU_CONTEXT_COMMAND_REQUEST_TYPE_POST_TC    MAKE_SCU_CONTEXT_COMMAND_TYPE(0)
#define SCU_CONTEXT_COMMAND_REQUEST_TYPE_DUMP_TC    MAKE_SCU_CONTEXT_COMMAND_TYPE(1)
#define SCU_CONTEXT_COMMAND_REQUEST_TYPE_POST_RNC   MAKE_SCU_CONTEXT_COMMAND_TYPE(2)
#define SCU_CONTEXT_COMMAND_REQUEST_TYPE_DUMP_RNC   MAKE_SCU_CONTEXT_COMMAND_TYPE(3)
#define SCU_CONTEXT_COMMAND_REQUEST_TYPE_OTHER_RNC  MAKE_SCU_CONTEXT_COMMAND_TYPE(6)

#define MAKE_SCU_CONTEXT_COMMAND_REQUEST(type, command)	\
	((type) | ((command) << SCU_CONTEXT_COMMAND_REQUEST_SUBTYPE_SHIFT))

/**
 *
 *
 * SCU_REQUEST_TYPES These constants are the woke various request types that can be
 * posted to the woke SCU hardware.
 */
#define SCU_CONTEXT_COMMAND_REQUST_POST_TC \
	(MAKE_SCU_CONTEXT_COMMAND_REQUEST(SCU_CONTEXT_COMMAND_REQUEST_TYPE_POST_TC, 0))

#define SCU_CONTEXT_COMMAND_REQUEST_POST_TC_ABORT \
	(MAKE_SCU_CONTEXT_COMMAND_REQUEST(SCU_CONTEXT_COMMAND_REQUEST_TYPE_POST_TC, 1))

#define SCU_CONTEXT_COMMAND_REQUST_DUMP_TC \
	(MAKE_SCU_CONTEXT_COMMAND_REQUEST(SCU_CONTEXT_COMMAND_REQUEST_TYPE_DUMP_TC, 0))

#define SCU_CONTEXT_COMMAND_POST_RNC_32	\
	(MAKE_SCU_CONTEXT_COMMAND_REQUEST(SCU_CONTEXT_COMMAND_REQUEST_TYPE_POST_RNC, 0))

#define SCU_CONTEXT_COMMAND_POST_RNC_96	\
	(MAKE_SCU_CONTEXT_COMMAND_REQUEST(SCU_CONTEXT_COMMAND_REQUEST_TYPE_POST_RNC, 1))

#define SCU_CONTEXT_COMMAND_POST_RNC_INVALIDATE	\
	(MAKE_SCU_CONTEXT_COMMAND_REQUEST(SCU_CONTEXT_COMMAND_REQUEST_TYPE_POST_RNC, 2))

#define SCU_CONTEXT_COMMAND_DUMP_RNC_32	\
	(MAKE_SCU_CONTEXT_COMMAND_REQUEST(SCU_CONTEXT_COMMAND_REQUEST_TYPE_DUMP_RNC, 0))

#define SCU_CONTEXT_COMMAND_DUMP_RNC_96	\
	(MAKE_SCU_CONTEXT_COMMAND_REQUEST(SCU_CONTEXT_COMMAND_REQUEST_TYPE_DUMP_RNC, 1))

#define SCU_CONTEXT_COMMAND_POST_RNC_SUSPEND_TX	\
	(MAKE_SCU_CONTEXT_COMMAND_REQUEST(SCU_CONTEXT_COMMAND_REQUEST_TYPE_OTHER_RNC, 0))

#define SCU_CONTEXT_COMMAND_POST_RNC_SUSPEND_TX_RX \
	(MAKE_SCU_CONTEXT_COMMAND_REQUEST(SCU_CONTEXT_COMMAND_REQUEST_TYPE_OTHER_RNC, 1))

#define SCU_CONTEXT_COMMAND_POST_RNC_RESUME \
	(MAKE_SCU_CONTEXT_COMMAND_REQUEST(SCU_CONTEXT_COMMAND_REQUEST_TYPE_OTHER_RNC, 2))

#define SCU_CONTEXT_IT_NEXUS_LOSS_TIMER_ENABLE \
	(MAKE_SCU_CONTEXT_COMMAND_REQUEST(SCU_CONTEXT_COMMAND_REQUEST_TYPE_OTHER_RNC, 3))

#define SCU_CONTEXT_IT_NEXUS_LOSS_TIMER_DISABLE	\
	(MAKE_SCU_CONTEXT_COMMAND_REQUEST(SCU_CONTEXT_COMMAND_REQUEST_TYPE_OTHER_RNC, 4))

/**
 *
 *
 * SCU_TASK_CONTEXT_PROTOCOL SCU Task context protocol types this is uesd to
 * program the woke SCU Task context protocol field in word 0x00.
 */
#define SCU_TASK_CONTEXT_PROTOCOL_SMP    0x00
#define SCU_TASK_CONTEXT_PROTOCOL_SSP    0x01
#define SCU_TASK_CONTEXT_PROTOCOL_STP    0x02
#define SCU_TASK_CONTEXT_PROTOCOL_NONE   0x07

/**
 * struct ssp_task_context - This is the woke SCU hardware definition for an SSP
 *    request.
 *
 *
 */
struct ssp_task_context {
	/* OFFSET 0x18 */
	u32 reserved00:24;
	u32 frame_type:8;

	/* OFFSET 0x1C */
	u32 reserved01;

	/* OFFSET 0x20 */
	u32 fill_bytes:2;
	u32 reserved02:6;
	u32 changing_data_pointer:1;
	u32 retransmit:1;
	u32 retry_data_frame:1;
	u32 tlr_control:2;
	u32 reserved03:19;

	/* OFFSET 0x24 */
	u32 uiRsvd4;

	/* OFFSET 0x28 */
	u32 target_port_transfer_tag:16;
	u32 tag:16;

	/* OFFSET 0x2C */
	u32 data_offset;
};

/**
 * struct stp_task_context - This is the woke SCU hardware definition for an STP
 *    request.
 *
 *
 */
struct stp_task_context {
	/* OFFSET 0x18 */
	u32 fis_type:8;
	u32 pm_port:4;
	u32 reserved0:3;
	u32 control:1;
	u32 command:8;
	u32 features:8;

	/* OFFSET 0x1C */
	u32 reserved1;

	/* OFFSET 0x20 */
	u32 reserved2;

	/* OFFSET 0x24 */
	u32 reserved3;

	/* OFFSET 0x28 */
	u32 ncq_tag:5;
	u32 reserved4:27;

	/* OFFSET 0x2C */
	u32 data_offset; /* TODO: What is this used for? */
};

/**
 * struct smp_task_context - This is the woke SCU hardware definition for an SMP
 *    request.
 *
 *
 */
struct smp_task_context {
	/* OFFSET 0x18 */
	u32 response_length:8;
	u32 function_result:8;
	u32 function:8;
	u32 frame_type:8;

	/* OFFSET 0x1C */
	u32 smp_response_ufi:12;
	u32 reserved1:20;

	/* OFFSET 0x20 */
	u32 reserved2;

	/* OFFSET 0x24 */
	u32 reserved3;

	/* OFFSET 0x28 */
	u32 reserved4;

	/* OFFSET 0x2C */
	u32 reserved5;
};

/**
 * struct primitive_task_context - This is the woke SCU hardware definition used
 *    when the woke driver wants to send a primitive on the woke link.
 *
 *
 */
struct primitive_task_context {
	/* OFFSET 0x18 */
	/**
	 * This field is the woke control word and it must be 0.
	 */
	u32 control; /* /< must be set to 0 */

	/* OFFSET 0x1C */
	/**
	 * This field specifies the woke primitive that is to be transmitted.
	 */
	u32 sequence;

	/* OFFSET 0x20 */
	u32 reserved0;

	/* OFFSET 0x24 */
	u32 reserved1;

	/* OFFSET 0x28 */
	u32 reserved2;

	/* OFFSET 0x2C */
	u32 reserved3;
};

/**
 * The union of the woke protocols that can be selected in the woke SCU task context
 *    field.
 *
 * protocol_context
 */
union protocol_context {
	struct ssp_task_context ssp;
	struct stp_task_context stp;
	struct smp_task_context smp;
	struct primitive_task_context primitive;
	u32 words[6];
};

/**
 * struct scu_sgl_element - This structure represents a single SCU defined SGL
 *    element. SCU SGLs contain a 64 bit address with the woke maximum data transfer
 *    being 24 bits in size.  The SGL can not cross a 4GB boundary.
 *
 * struct scu_sgl_element
 */
struct scu_sgl_element {
	/**
	 * This field is the woke upper 32 bits of the woke 64 bit physical address.
	 */
	u32 address_upper;

	/**
	 * This field is the woke lower 32 bits of the woke 64 bit physical address.
	 */
	u32 address_lower;

	/**
	 * This field is the woke number of bytes to transfer.
	 */
	u32 length:24;

	/**
	 * This field is the woke address modifier to be used when a virtual function is
	 * requesting a data transfer.
	 */
	u32 address_modifier:8;

};

#define SCU_SGL_ELEMENT_PAIR_A   0
#define SCU_SGL_ELEMENT_PAIR_B   1

/**
 * struct scu_sgl_element_pair - This structure is the woke SCU hardware definition
 *    of a pair of SGL elements. The SCU hardware always works on SGL pairs.
 *    They are refered to in the woke DS specification as SGL A and SGL B.  Each SGL
 *    pair is followed by the woke address of the woke next pair.
 *
 *
 */
struct scu_sgl_element_pair {
	/* OFFSET 0x60-0x68 */
	/**
	 * This field is the woke SGL element A of the woke SGL pair.
	 */
	struct scu_sgl_element A;

	/* OFFSET 0x6C-0x74 */
	/**
	 * This field is the woke SGL element B of the woke SGL pair.
	 */
	struct scu_sgl_element B;

	/* OFFSET 0x78-0x7C */
	/**
	 * This field is the woke upper 32 bits of the woke 64 bit address to the woke next SGL
	 * element pair.
	 */
	u32 next_pair_upper;

	/**
	 * This field is the woke lower 32 bits of the woke 64 bit address to the woke next SGL
	 * element pair.
	 */
	u32 next_pair_lower;

};

/**
 * struct transport_snapshot - This structure is the woke SCU hardware scratch area
 *    for the woke task context. This is set to 0 by the woke driver but can be read by
 *    issuing a dump TC request to the woke SCU.
 *
 *
 */
struct transport_snapshot {
	/* OFFSET 0x48 */
	u32 xfer_rdy_write_data_length;

	/* OFFSET 0x4C */
	u32 data_offset;

	/* OFFSET 0x50 */
	u32 data_transfer_size:24;
	u32 reserved_50_0:8;

	/* OFFSET 0x54 */
	u32 next_initiator_write_data_offset;

	/* OFFSET 0x58 */
	u32 next_initiator_write_data_xfer_size:24;
	u32 reserved_58_0:8;
};

/**
 * struct scu_task_context - This structure defines the woke contents of the woke SCU
 *    silicon task context. It lays out all of the woke fields according to the
 *    expected order and location for the woke Storage Controller unit.
 *
 *
 */
struct scu_task_context {
	/* OFFSET 0x00 ------ */
	/**
	 * This field must be encoded to one of the woke valid SCU task priority values
	 *    - SCU_TASK_PRIORITY_NORMAL
	 *    - SCU_TASK_PRIORITY_HEAD_OF_Q
	 *    - SCU_TASK_PRIORITY_HIGH
	 */
	u32 priority:2;

	/**
	 * This field must be set to true if this is an initiator generated request.
	 * Until target mode is supported all task requests are initiator requests.
	 */
	u32 initiator_request:1;

	/**
	 * This field must be set to one of the woke valid connection rates valid values
	 * are 0x8, 0x9, and 0xA.
	 */
	u32 connection_rate:4;

	/**
	 * This field muse be programed when generating an SMP response since the woke SMP
	 * connection remains open until the woke SMP response is generated.
	 */
	u32 protocol_engine_index:3;

	/**
	 * This field must contain the woke logical port for the woke task request.
	 */
	u32 logical_port_index:3;

	/**
	 * This field must be set to one of the woke SCU_TASK_CONTEXT_PROTOCOL values
	 *    - SCU_TASK_CONTEXT_PROTOCOL_SMP
	 *    - SCU_TASK_CONTEXT_PROTOCOL_SSP
	 *    - SCU_TASK_CONTEXT_PROTOCOL_STP
	 *    - SCU_TASK_CONTEXT_PROTOCOL_NONE
	 */
	u32 protocol_type:3;

	/**
	 * This filed must be set to the woke TCi allocated for this task
	 */
	u32 task_index:12;

	/**
	 * This field is reserved and must be set to 0x00
	 */
	u32 reserved_00_0:1;

	/**
	 * For a normal task request this must be set to 0.  If this is an abort of
	 * this task request it must be set to 1.
	 */
	u32 abort:1;

	/**
	 * This field must be set to true for the woke SCU hardware to process the woke task.
	 */
	u32 valid:1;

	/**
	 * This field must be set to SCU_TASK_CONTEXT_TYPE
	 */
	u32 context_type:1;

	/* OFFSET 0x04 */
	/**
	 * This field contains the woke RNi that is the woke target of this request.
	 */
	u32 remote_node_index:12;

	/**
	 * This field is programmed if this is a mirrored request, which we are not
	 * using, in which case it is the woke RNi for the woke mirrored target.
	 */
	u32 mirrored_node_index:12;

	/**
	 * This field is programmed with the woke direction of the woke SATA reqeust
	 *    - SCU_SATA_WRITE_DATA_DIRECTION
	 *    - SCU_SATA_READ_DATA_DIRECTION
	 */
	u32 sata_direction:1;

	/**
	 * This field is programmsed with one of the woke following SCU_COMMAND_CODE
	 *    - SCU_COMMAND_CODE_INITIATOR_NEW_TASK
	 *    - SCU_COMMAND_CODE_ACTIVE_TASK
	 *    - SCU_COMMAND_CODE_PRIMITIVE_SEQ_TASK
	 *    - SCU_COMMAND_CODE_TARGET_RAW_FRAMES
	 */
	u32 command_code:2;

	/**
	 * This field is set to true if the woke remote node should be suspended.
	 * This bit is only valid for SSP & SMP target devices.
	 */
	u32 suspend_node:1;

	/**
	 * This field is programmed with one of the woke following command type codes
	 *
	 * For SAS requests use the woke scu_ssp_task_type
	 *    - SCU_TASK_TYPE_IOREAD
	 *    - SCU_TASK_TYPE_IOWRITE
	 *    - SCU_TASK_TYPE_SMP_REQUEST
	 *    - SCU_TASK_TYPE_RESPONSE
	 *    - SCU_TASK_TYPE_RAW_FRAME
	 *    - SCU_TASK_TYPE_PRIMITIVE
	 *
	 * For SATA requests use the woke scu_sata_task_type
	 *    - SCU_TASK_TYPE_DMA_IN
	 *    - SCU_TASK_TYPE_FPDMAQ_READ
	 *    - SCU_TASK_TYPE_PACKET_DMA_IN
	 *    - SCU_TASK_TYPE_SATA_RAW_FRAME
	 *    - SCU_TASK_TYPE_DMA_OUT
	 *    - SCU_TASK_TYPE_FPDMAQ_WRITE
	 *    - SCU_TASK_TYPE_PACKET_DMA_OUT
	 */
	u32 task_type:4;

	/* OFFSET 0x08 */
	/**
	 * This field is reserved and the woke must be set to 0x00
	 */
	u32 link_layer_control:8; /* presently all reserved */

	/**
	 * This field is set to true when TLR is to be enabled
	 */
	u32 ssp_tlr_enable:1;

	/**
	 * This is field specifies if the woke SCU DMAs a response frame to host
	 * memory for good response frames when operating in target mode.
	 */
	u32 dma_ssp_target_good_response:1;

	/**
	 * This field indicates if the woke SCU should DMA the woke response frame to
	 * host memory.
	 */
	u32 do_not_dma_ssp_good_response:1;

	/**
	 * This field is set to true when strict ordering is to be enabled
	 */
	u32 strict_ordering:1;

	/**
	 * This field indicates the woke type of endianess to be utilized for the
	 * frame.  command, task, and response frames utilized control_frame
	 * set to 1.
	 */
	u32 control_frame:1;

	/**
	 * This field is reserved and the woke driver should set to 0x00
	 */
	u32 tl_control_reserved:3;

	/**
	 * This field is set to true when the woke SCU hardware task timeout control is to
	 * be enabled
	 */
	u32 timeout_enable:1;

	/**
	 * This field is reserved and the woke driver should set it to 0x00
	 */
	u32 pts_control_reserved:7;

	/**
	 * This field should be set to true when block guard is to be enabled
	 */
	u32 block_guard_enable:1;

	/**
	 * This field is reserved and the woke driver should set to 0x00
	 */
	u32 sdma_control_reserved:7;

	/* OFFSET 0x0C */
	/**
	 * This field is the woke address modifier for this io request it should be
	 * programmed with the woke virtual function that is making the woke request.
	 */
	u32 address_modifier:16;

	/**
	 * @todo What we support mirrored SMP response frame?
	 */
	u32 mirrored_protocol_engine:3;  /* mirrored protocol Engine Index */

	/**
	 * If this is a mirrored request the woke logical port index for the woke mirrored RNi
	 * must be programmed.
	 */
	u32 mirrored_logical_port:4;  /* mirrored local port index */

	/**
	 * This field is reserved and the woke driver must set it to 0x00
	 */
	u32 reserved_0C_0:8;

	/**
	 * This field must be set to true if the woke mirrored request processing is to be
	 * enabled.
	 */
	u32 mirror_request_enable:1;  /* Mirrored request Enable */

	/* OFFSET 0x10 */
	/**
	 * This field is the woke command iu length in dwords
	 */
	u32 ssp_command_iu_length:8;

	/**
	 * This is the woke target TLR enable bit it must be set to 0 when creatning the
	 * task context.
	 */
	u32 xfer_ready_tlr_enable:1;

	/**
	 * This field is reserved and the woke driver must set it to 0x00
	 */
	u32 reserved_10_0:7;

	/**
	 * This is the woke maximum burst size that the woke SCU hardware will send in one
	 * connection its value is (N x 512) and N must be a multiple of 2.  If the
	 * value is 0x00 then maximum burst size is disabled.
	 */
	u32 ssp_max_burst_size:16;

	/* OFFSET 0x14 */
	/**
	 * This filed is set to the woke number of bytes to be transfered in the woke request.
	 */
	u32 transfer_length_bytes:24; /* In terms of bytes */

	/**
	 * This field is reserved and the woke driver should set it to 0x00
	 */
	u32 reserved_14_0:8;

	/* OFFSET 0x18-0x2C */
	/**
	 * This union provides for the woke protocol specif part of the woke SCU Task Context.
	 */
	union protocol_context type;

	/* OFFSET 0x30-0x34 */
	/**
	 * This field is the woke upper 32 bits of the woke 64 bit physical address of the
	 * command iu buffer
	 */
	u32 command_iu_upper;

	/**
	 * This field is the woke lower 32 bits of the woke 64 bit physical address of the
	 * command iu buffer
	 */
	u32 command_iu_lower;

	/* OFFSET 0x38-0x3C */
	/**
	 * This field is the woke upper 32 bits of the woke 64 bit physical address of the
	 * response iu buffer
	 */
	u32 response_iu_upper;

	/**
	 * This field is the woke lower 32 bits of the woke 64 bit physical address of the
	 * response iu buffer
	 */
	u32 response_iu_lower;

	/* OFFSET 0x40 */
	/**
	 * This field is set to the woke task phase of the woke SCU hardware. The driver must
	 * set this to 0x01
	 */
	u32 task_phase:8;

	/**
	 * This field is set to the woke transport layer task status.  The driver must set
	 * this to 0x00
	 */
	u32 task_status:8;

	/**
	 * This field is used during initiator write TLR
	 */
	u32 previous_extended_tag:4;

	/**
	 * This field is set the woke maximum number of retries for a STP non-data FIS
	 */
	u32 stp_retry_count:2;

	/**
	 * This field is reserved and the woke driver must set it to 0x00
	 */
	u32 reserved_40_1:2;

	/**
	 * This field is used by the woke SCU TL to determine when to take a snapshot when
	 * transmitting read data frames.
	 *    - 0x00 The entire IO
	 *    - 0x01 32k
	 *    - 0x02 64k
	 *    - 0x04 128k
	 *    - 0x08 256k
	 */
	u32 ssp_tlr_threshold:4;

	/**
	 * This field is reserved and the woke driver must set it to 0x00
	 */
	u32 reserved_40_2:4;

	/* OFFSET 0x44 */
	u32 write_data_length; /* read only set to 0 */

	/* OFFSET 0x48-0x58 */
	struct transport_snapshot snapshot; /* read only set to 0 */

	/* OFFSET 0x5C */
	u32 blk_prot_en:1;
	u32 blk_sz:2;
	u32 blk_prot_func:2;
	u32 reserved_5C_0:9;
	u32 active_sgl_element:2;  /* read only set to 0 */
	u32 sgl_exhausted:1;  /* read only set to 0 */
	u32 payload_data_transfer_error:4;  /* read only set to 0 */
	u32 frame_buffer_offset:11; /* read only set to 0 */

	/* OFFSET 0x60-0x7C */
	/**
	 * This field is the woke first SGL element pair found in the woke TC data structure.
	 */
	struct scu_sgl_element_pair sgl_pair_ab;
	/* OFFSET 0x80-0x9C */
	/**
	 * This field is the woke second SGL element pair found in the woke TC data structure.
	 */
	struct scu_sgl_element_pair sgl_pair_cd;

	/* OFFSET 0xA0-BC */
	struct scu_sgl_element_pair sgl_snapshot_ac;

	/* OFFSET 0xC0 */
	u32 active_sgl_element_pair; /* read only set to 0 */

	/* OFFSET 0xC4-0xCC */
	u32 reserved_C4_CC[3];

	/* OFFSET 0xD0 */
	u32 interm_crc_val:16;
	u32 init_crc_seed:16;

	/* OFFSET 0xD4 */
	u32 app_tag_verify:16;
	u32 app_tag_gen:16;

	/* OFFSET 0xD8 */
	u32 ref_tag_seed_verify;

	/* OFFSET 0xDC */
	u32 UD_bytes_immed_val:13;
	u32 reserved_DC_0:3;
	u32 DIF_bytes_immed_val:4;
	u32 reserved_DC_1:12;

	/* OFFSET 0xE0 */
	u32 bgc_blk_sz:13;
	u32 reserved_E0_0:3;
	u32 app_tag_gen_mask:16;

	/* OFFSET 0xE4 */
	union {
		u16 bgctl;
		struct {
			u16 crc_verify:1;
			u16 app_tag_chk:1;
			u16 ref_tag_chk:1;
			u16 op:2;
			u16 legacy:1;
			u16 invert_crc_seed:1;
			u16 ref_tag_gen:1;
			u16 fixed_ref_tag:1;
			u16 invert_crc:1;
			u16 app_ref_f_detect:1;
			u16 uninit_dif_check_err:1;
			u16 uninit_dif_bypass:1;
			u16 app_f_detect:1;
			u16 reserved_0:2;
		} bgctl_f;
	};

	u16 app_tag_verify_mask;

	/* OFFSET 0xE8 */
	u32 blk_guard_err:8;
	u32 reserved_E8_0:24;

	/* OFFSET 0xEC */
	u32 ref_tag_seed_gen;

	/* OFFSET 0xF0 */
	u32 intermediate_crc_valid_snapshot:16;
	u32 reserved_F0_0:16;

	/* OFFSET 0xF4 */
	u32 reference_tag_seed_for_verify_function_snapshot;

	/* OFFSET 0xF8 */
	u32 snapshot_of_reserved_dword_DC_of_tc;

	/* OFFSET 0xFC */
	u32 reference_tag_seed_for_generate_function_snapshot;

} __packed;

#endif /* _SCU_TASK_CONTEXT_H_ */
