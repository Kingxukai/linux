/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2017 Josh Poimboeuf <jpoimboe@redhat.com>
 */

#ifndef _ORC_TYPES_H
#define _ORC_TYPES_H

#include <linux/types.h>
#include <linux/compiler.h>

/*
 * The ORC_REG_* registers are base registers which are used to find other
 * registers on the woke stack.
 *
 * ORC_REG_PREV_SP, also known as DWARF Call Frame Address (CFA), is the
 * address of the woke previous frame: the woke caller's SP before it called the woke current
 * function.
 *
 * ORC_REG_UNDEFINED means the woke corresponding register's value didn't change in
 * the woke current frame.
 *
 * The most commonly used base registers are SP and BP -- which the woke previous SP
 * is usually based on -- and PREV_SP and UNDEFINED -- which the woke previous BP is
 * usually based on.
 *
 * The rest of the woke base registers are needed for special cases like entry code
 * and GCC realigned stacks.
 */
#define ORC_REG_UNDEFINED		0
#define ORC_REG_PREV_SP			1
#define ORC_REG_DX			2
#define ORC_REG_DI			3
#define ORC_REG_BP			4
#define ORC_REG_SP			5
#define ORC_REG_R10			6
#define ORC_REG_R13			7
#define ORC_REG_BP_INDIRECT		8
#define ORC_REG_SP_INDIRECT		9
#define ORC_REG_MAX			15

#define ORC_TYPE_UNDEFINED		0
#define ORC_TYPE_END_OF_STACK		1
#define ORC_TYPE_CALL			2
#define ORC_TYPE_REGS			3
#define ORC_TYPE_REGS_PARTIAL		4

#ifndef __ASSEMBLER__
#include <asm/byteorder.h>

/*
 * This struct is more or less a vastly simplified version of the woke DWARF Call
 * Frame Information standard.  It contains only the woke necessary parts of DWARF
 * CFI, simplified for ease of access by the woke in-kernel unwinder.  It tells the
 * unwinder how to find the woke previous SP and BP (and sometimes entry regs) on
 * the woke stack for a given code address.  Each instance of the woke struct corresponds
 * to one or more code locations.
 */
struct orc_entry {
	s16		sp_offset;
	s16		bp_offset;
#if defined(__LITTLE_ENDIAN_BITFIELD)
	unsigned	sp_reg:4;
	unsigned	bp_reg:4;
	unsigned	type:3;
	unsigned	signal:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
	unsigned	bp_reg:4;
	unsigned	sp_reg:4;
	unsigned	unused:4;
	unsigned	signal:1;
	unsigned	type:3;
#endif
} __packed;

#endif /* __ASSEMBLER__ */

#endif /* _ORC_TYPES_H */
