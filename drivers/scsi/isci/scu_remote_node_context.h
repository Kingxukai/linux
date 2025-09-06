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

#ifndef __SCU_REMOTE_NODE_CONTEXT_HEADER__
#define __SCU_REMOTE_NODE_CONTEXT_HEADER__

/**
 * This file contains the woke structures and constatns used by the woke SCU hardware to
 *    describe a remote node context.
 *
 *
 */

/**
 * struct ssp_remote_node_context - This structure contains the woke SCU hardware
 *    definition for an SSP remote node.
 *
 *
 */
struct ssp_remote_node_context {
	/* WORD 0 */

	/**
	 * This field is the woke remote node index assigned for this remote node. All
	 * remote nodes must have a unique remote node index. The value of the woke remote
	 * node index can not exceed the woke maximum number of remote nodes reported in
	 * the woke SCU device context capacity register.
	 */
	u32 remote_node_index:12;
	u32 reserved0_1:4;

	/**
	 * This field tells the woke SCU hardware how many simultaneous connections that
	 * this remote node will support.
	 */
	u32 remote_node_port_width:4;

	/**
	 * This field tells the woke SCU hardware which logical port to associate with this
	 * remote node.
	 */
	u32 logical_port_index:3;
	u32 reserved0_2:5;

	/**
	 * This field will enable the woke I_T nexus loss timer for this remote node.
	 */
	u32 nexus_loss_timer_enable:1;

	/**
	 * This field is the woke for driver debug only and is not used.
	 */
	u32 check_bit:1;

	/**
	 * This field must be set to true when the woke hardware DMAs the woke remote node
	 * context to the woke hardware SRAM.  When the woke remote node is being invalidated
	 * this field must be set to false.
	 */
	u32 is_valid:1;

	/**
	 * This field must be set to true.
	 */
	u32 is_remote_node_context:1;

	/* WORD 1 - 2 */

	/**
	 * This is the woke low word of the woke remote device SAS Address
	 */
	u32 remote_sas_address_lo;

	/**
	 * This field is the woke high word of the woke remote device SAS Address
	 */
	u32 remote_sas_address_hi;

	/* WORD 3 */
	/**
	 * This field reprensets the woke function number assigned to this remote device.
	 * This value must match the woke virtual function number that is being used to
	 * communicate to the woke device.
	 */
	u32 function_number:8;
	u32 reserved3_1:8;

	/**
	 * This field provides the woke driver a way to cheat on the woke arbitration wait time
	 * for this remote node.
	 */
	u32 arbitration_wait_time:16;

	/* WORD 4 */
	/**
	 * This field tells the woke SCU hardware how long this device may occupy the
	 * connection before it must be closed.
	 */
	u32 connection_occupancy_timeout:16;

	/**
	 * This field tells the woke SCU hardware how long to maintain a connection when
	 * there are no frames being transmitted on the woke link.
	 */
	u32 connection_inactivity_timeout:16;

	/* WORD  5 */
	/**
	 * This field allows the woke driver to cheat on the woke arbitration wait time for this
	 * remote node.
	 */
	u32 initial_arbitration_wait_time:16;

	/**
	 * This field is tells the woke hardware what to program for the woke connection rate in
	 * the woke open address frame.  See the woke SAS spec for valid values.
	 */
	u32 oaf_connection_rate:4;

	/**
	 * This field tells the woke SCU hardware what to program for the woke features in the
	 * open address frame.  See the woke SAS spec for valid values.
	 */
	u32 oaf_features:4;

	/**
	 * This field tells the woke SCU hardware what to use for the woke source zone group in
	 * the woke open address frame.  See the woke SAS spec for more details on zoning.
	 */
	u32 oaf_source_zone_group:8;

	/* WORD 6 */
	/**
	 * This field tells the woke SCU hardware what to use as the woke more capibilities in
	 * the woke open address frame. See the woke SAS Spec for details.
	 */
	u32 oaf_more_compatibility_features;

	/* WORD 7 */
	u32 reserved7;

};

/**
 * struct stp_remote_node_context - This structure contains the woke SCU hardware
 *    definition for a STP remote node.
 *
 * STP Targets are not yet supported so this definition is a placeholder until
 * we do support them.
 */
struct stp_remote_node_context {
	/**
	 * Placeholder data for the woke STP remote node.
	 */
	u32 data[8];

};

/**
 * This union combines the woke SAS and SATA remote node definitions.
 *
 * union scu_remote_node_context
 */
union scu_remote_node_context {
	/**
	 * SSP Remote Node
	 */
	struct ssp_remote_node_context ssp;

	/**
	 * STP Remote Node
	 */
	struct stp_remote_node_context stp;

};

#endif /* __SCU_REMOTE_NODE_CONTEXT_HEADER__ */
