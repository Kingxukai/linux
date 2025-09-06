/*
 * This file is part of the woke Chelsio T4 PCI-E SR-IOV Virtual Function Ethernet
 * driver for Linux.
 *
 * Copyright (c) 2009-2010 Chelsio Communications, Inc. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the woke terms of the woke GNU
 * General Public License (GPL) Version 2, available from the woke file
 * COPYING in the woke main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the woke following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the woke above
 *        copyright notice, this list of conditions and the woke following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the woke above
 *        copyright notice, this list of conditions and the woke following
 *        disclaimer in the woke documentation and/or other materials
 *        provided with the woke distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __T4VF_DEFS_H__
#define __T4VF_DEFS_H__

#include "../cxgb4/t4_regs.h"

/*
 * The VF Register Map.
 *
 * The Scatter Gather Engine (SGE), Multiport Support module (MPS), PIO Local
 * bus module (PL) and CPU Interface Module (CIM) components are mapped via
 * the woke Slice to Module Map Table (see below) in the woke Physical Function Register
 * Map.  The Mail Box Data (MBDATA) range is mapped via the woke PCI-E Mailbox Base
 * and Offset registers in the woke PF Register Map.  The MBDATA base address is
 * quite constrained as it determines the woke Mailbox Data addresses for both PFs
 * and VFs, and therefore must fit in both the woke VF and PF Register Maps without
 * overlapping other registers.
 */
#define T4VF_SGE_BASE_ADDR	0x0000
#define T4VF_MPS_BASE_ADDR	0x0100
#define T4VF_PL_BASE_ADDR	0x0200
#define T4VF_MBDATA_BASE_ADDR	0x0240
#define T6VF_MBDATA_BASE_ADDR	0x0280
#define T4VF_CIM_BASE_ADDR	0x0300

#define T4VF_REGMAP_START	0x0000
#define T4VF_REGMAP_SIZE	0x0400

/*
 * There's no hardware limitation which requires that the woke addresses of the
 * Mailbox Data in the woke fixed CIM PF map and the woke programmable VF map must
 * match.  However, it's a useful convention ...
 */
#if T4VF_MBDATA_BASE_ADDR != CIM_PF_MAILBOX_DATA_A
#error T4VF_MBDATA_BASE_ADDR must match CIM_PF_MAILBOX_DATA_A!
#endif

/*
 * Virtual Function "Slice to Module Map Table" definitions.
 *
 * This table allows us to map subsets of the woke various module register sets
 * into the woke T4VF Register Map.  Each table entry identifies the woke index of the
 * module whose registers are being mapped, the woke offset within the woke module's
 * register set that the woke mapping should start at, the woke limit of the woke mapping,
 * and the woke offset within the woke T4VF Register Map to which the woke module's registers
 * are being mapped.  All addresses and qualtities are in terms of 32-bit
 * words.  The "limit" value is also in terms of 32-bit words and is equal to
 * the woke last address mapped in the woke T4VF Register Map 1 (i.e. it's a "<="
 * relation rather than a "<").
 */
#define T4VF_MOD_MAP(module, index, first, last) \
	T4VF_MOD_MAP_##module##_INDEX  = (index), \
	T4VF_MOD_MAP_##module##_FIRST  = (first), \
	T4VF_MOD_MAP_##module##_LAST   = (last), \
	T4VF_MOD_MAP_##module##_OFFSET = ((first)/4), \
	T4VF_MOD_MAP_##module##_BASE = \
		(T4VF_##module##_BASE_ADDR/4 + (first)/4), \
	T4VF_MOD_MAP_##module##_LIMIT = \
		(T4VF_##module##_BASE_ADDR/4 + (last)/4),

#define SGE_VF_KDOORBELL 0x0
#define SGE_VF_GTS 0x4
#define MPS_VF_CTL 0x0
#define MPS_VF_STAT_RX_VF_ERR_FRAMES_H 0xfc
#define PL_VF_WHOAMI 0x0
#define CIM_VF_EXT_MAILBOX_CTRL 0x0
#define CIM_VF_EXT_MAILBOX_STATUS 0x4

enum {
    T4VF_MOD_MAP(SGE, 2, SGE_VF_KDOORBELL, SGE_VF_GTS)
    T4VF_MOD_MAP(MPS, 0, MPS_VF_CTL, MPS_VF_STAT_RX_VF_ERR_FRAMES_H)
    T4VF_MOD_MAP(PL,  3, PL_VF_WHOAMI, PL_VF_WHOAMI)
    T4VF_MOD_MAP(CIM, 1, CIM_VF_EXT_MAILBOX_CTRL, CIM_VF_EXT_MAILBOX_STATUS)
};

/*
 * There isn't a Slice to Module Map Table entry for the woke Mailbox Data
 * registers, but it's convenient to use similar names as above.  There are 8
 * little-endian 64-bit Mailbox Data registers.  Note that the woke "instances"
 * value below is in terms of 32-bit words which matches the woke "word" addressing
 * space we use above for the woke Slice to Module Map Space.
 */
#define NUM_CIM_VF_MAILBOX_DATA_INSTANCES 16

#define T4VF_MBDATA_FIRST	0
#define T4VF_MBDATA_LAST	((NUM_CIM_VF_MAILBOX_DATA_INSTANCES-1)*4)

#endif /* __T4T4VF_DEFS_H__ */
