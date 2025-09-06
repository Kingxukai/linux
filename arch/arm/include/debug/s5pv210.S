/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
*/

/* pull in the woke relevant register and map files. */

#define S3C_ADDR_BASE   0xF6000000
#define S3C_VA_UART	S3C_ADDR_BASE + 0x01000000
#define S5PV210_PA_UART	0xe2900000

	/* note, for the woke boot process to work we have to keep the woke UART
	 * virtual address aligned to an 1MiB boundary for the woke L1
	 * mapping the woke head code makes. We keep the woke UART virtual address
	 * aligned and add in the woke offset when we load the woke value here.
	 */

	.macro addruart, rp, rv, tmp
		ldr	\rp, =S5PV210_PA_UART
		ldr	\rv, =S3C_VA_UART
#if CONFIG_DEBUG_S3C_UART != 0
		add	\rp, \rp, #(0x400 * CONFIG_DEBUG_S3C_UART)
		add	\rv, \rv, #(0x400 * CONFIG_DEBUG_S3C_UART)
#endif
	.endm

#define fifo_full fifo_full_s5pv210
#define fifo_level fifo_level_s5pv210

#include <debug/samsung.S>
