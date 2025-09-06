/*
 * Copied from the woke kernel sources to tools/:
 *
 * This file is subject to the woke terms and conditions of the woke GNU General Public
 * License.  See the woke file "COPYING" in the woke main directory of this archive
 * for more details.
 *
 * Copyright (C) 2001 - 2012 Tensilica Inc.
 */

#ifndef _TOOLS_LINUX_XTENSA_SYSTEM_H
#define _TOOLS_LINUX_XTENSA_SYSTEM_H

#define mb()  ({ __asm__ __volatile__("memw" : : : "memory"); })
#define rmb() barrier()
#define wmb() mb()

#endif /* _TOOLS_LINUX_XTENSA_SYSTEM_H */
