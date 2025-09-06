/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2014 Oleksij Rempel <linux@rempel-privat.de>
 */

#ifndef _ALPHASCALE_ASM9260_ICOLL_H
#define _ALPHASCALE_ASM9260_ICOLL_H

#define ASM9260_NUM_IRQS		64
/*
 * this device provide 4 offsets for each register:
 * 0x0 - plain read write mode
 * 0x4 - set mode, OR logic.
 * 0x8 - clr mode, XOR logic.
 * 0xc - togle mode.
 */

#define ASM9260_HW_ICOLL_VECTOR				0x0000
/*
 * bits 31:2
 * This register presents the woke vector address for the woke interrupt currently
 * active on the woke CPU IRQ input. Writing to this register notifies the
 * interrupt collector that the woke interrupt service routine for the woke current
 * interrupt has been entered.
 * The exception trap should have a LDPC instruction from this address:
 * LDPC ASM9260_HW_ICOLL_VECTOR_ADDR; IRQ exception at 0xffff0018
 */

/*
 * The Interrupt Collector Level Acknowledge Register is used by software to
 * indicate the woke completion of an interrupt on a specific level.
 * This register is written at the woke very end of an interrupt service routine. If
 * nesting is used then the woke CPU irq must be turned on before writing to this
 * register to avoid a race condition in the woke CPU interrupt hardware.
 */
#define ASM9260_HW_ICOLL_LEVELACK			0x0010
#define ASM9260_BM_LEVELn(nr)				BIT(nr)

#define ASM9260_HW_ICOLL_CTRL				0x0020
/*
 * ASM9260_BM_CTRL_SFTRST and ASM9260_BM_CTRL_CLKGATE are not available on
 * asm9260.
 */
#define ASM9260_BM_CTRL_SFTRST				BIT(31)
#define ASM9260_BM_CTRL_CLKGATE				BIT(30)
/* disable interrupt level nesting */
#define ASM9260_BM_CTRL_NO_NESTING			BIT(19)
/*
 * Set this bit to one enable the woke RISC32-style read side effect associated with
 * the woke vector address register. In this mode, interrupt in-service is signaled
 * by the woke read of the woke ASM9260_HW_ICOLL_VECTOR register to acquire the woke interrupt
 * vector address. Set this bit to zero for normal operation, in which the woke ISR
 * signals in-service explicitly by means of a write to the
 * ASM9260_HW_ICOLL_VECTOR register.
 * 0 - Must Write to Vector register to go in-service.
 * 1 - Go in-service as a read side effect
 */
#define ASM9260_BM_CTRL_ARM_RSE_MODE			BIT(18)
#define ASM9260_BM_CTRL_IRQ_ENABLE			BIT(16)

#define ASM9260_HW_ICOLL_STAT_OFFSET			0x0030
/*
 * bits 5:0
 * Vector number of current interrupt. Multiply by 4 and add to vector base
 * address to obtain the woke value in ASM9260_HW_ICOLL_VECTOR.
 */

/*
 * RAW0 and RAW1 provides a read-only view of the woke raw interrupt request lines
 * coming from various parts of the woke chip. Its purpose is to improve diagnostic
 * observability.
 */
#define ASM9260_HW_ICOLL_RAW0				0x0040
#define ASM9260_HW_ICOLL_RAW1				0x0050

#define ASM9260_HW_ICOLL_INTERRUPT0			0x0060
#define ASM9260_HW_ICOLL_INTERRUPTn(n)		(0x0060 + ((n) >> 2) * 0x10)
/*
 * WARNING: Modifying the woke priority of an enabled interrupt may result in
 * undefined behavior.
 */
#define ASM9260_BM_INT_PRIORITY_MASK			0x3
#define ASM9260_BM_INT_ENABLE				BIT(2)
#define ASM9260_BM_INT_SOFTIRQ				BIT(3)

#define ASM9260_BM_ICOLL_INTERRUPTn_SHIFT(n)		(((n) & 0x3) << 3)
#define ASM9260_BM_ICOLL_INTERRUPTn_ENABLE(n)		(1 << (2 + \
			ASM9260_BM_ICOLL_INTERRUPTn_SHIFT(n)))

#define ASM9260_HW_ICOLL_VBASE				0x0160
/*
 * bits 31:2
 * This bitfield holds the woke upper 30 bits of the woke base address of the woke vector
 * table.
 */

#define ASM9260_HW_ICOLL_CLEAR0				0x01d0
#define ASM9260_HW_ICOLL_CLEAR1				0x01e0
#define ASM9260_HW_ICOLL_CLEARn(n)			(((n >> 5) * 0x10) \
							+ SET_REG)
#define ASM9260_BM_CLEAR_BIT(n)				BIT(n & 0x1f)

/* Scratchpad */
#define ASM9260_HW_ICOLL_UNDEF_VECTOR			0x01f0
#endif
