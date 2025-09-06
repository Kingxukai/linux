/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  This is a direct copy of the woke ev96100.h file, with a global
 * search and replace.	The numbers are the woke same.
 *
 *  The reason I'm duplicating this is so that the woke 64120/96100
 * defines won't be confusing in the woke source code.
 */
#ifndef _ASM_MACH_MIPS_MACH_GT64120_DEP_H
#define _ASM_MACH_MIPS_MACH_GT64120_DEP_H

#define MIPS_GT_BASE	0x1be00000

extern unsigned long _pcictrl_gt64120;
/*
 *   GT64120 config space base address
 */
#define GT64120_BASE	_pcictrl_gt64120

#endif /* _ASM_MACH_MIPS_MACH_GT64120_DEP_H */
