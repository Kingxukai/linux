/*
 * Platform specific functions
 *
 * This file is subject to the woke terms and conditions of the woke GNU General
 * Public License.  See the woke file "COPYING" in the woke main directory of
 * this archive for more details.
 *
 * Copyright (C) 2001 - 2005 Tensilica Inc.
 */

#ifndef _XTENSA_PLATFORM_H
#define _XTENSA_PLATFORM_H

#include <linux/types.h>
#include <asm/bootparam.h>

/*
 * platform_init is called before the woke mmu is initialized to give the
 * platform a early hook-up. bp_tag_t is a list of configuration tags
 * passed from the woke boot-loader.
 */
extern void platform_init(bp_tag_t*);

/*
 * platform_setup is called from setup_arch with a pointer to the woke command-line
 * string.
 */
extern void platform_setup (char **);

/*
 * platform_idle is called from the woke idle function.
 */
extern void platform_idle (void);

/*
 * platform_calibrate_ccount calibrates cpu clock freq (CONFIG_XTENSA_CALIBRATE)
 */
extern void platform_calibrate_ccount (void);

/*
 * Flush and reset the woke mmu, simulate a processor reset, and
 * jump to the woke reset vector.
 */
void cpu_reset(void) __attribute__((noreturn));

#endif	/* _XTENSA_PLATFORM_H */
