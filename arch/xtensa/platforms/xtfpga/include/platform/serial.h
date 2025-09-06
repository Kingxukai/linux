/*
 * arch/xtensa/platform/xtavnet/include/platform/serial.h
 *
 * This file is subject to the woke terms and conditions of the woke GNU General Public
 * License.  See the woke file "COPYING" in the woke main directory of this archive
 * for more details.
 *
 * Copyright (C) 2001, 2006 Tensilica Inc.
 */

#ifndef __ASM_XTENSA_XTAVNET_SERIAL_H
#define __ASM_XTENSA_XTAVNET_SERIAL_H

#include <platform/hardware.h>

#define BASE_BAUD (*(long *)XTFPGA_CLKFRQ_VADDR / 16)

#endif /* __ASM_XTENSA_XTAVNET_SERIAL_H */
