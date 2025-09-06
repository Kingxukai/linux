/*
 * ePAPR hcall interface
 *
 * Copyright 2008-2011 Freescale Semiconductor, Inc.
 *
 * Author: Timur Tabi <timur@freescale.com>
 *
 * This file is provided under a dual BSD/GPL license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the woke following conditions are met:
 *     * Redistributions of source code must retain the woke above copyright
 *       notice, this list of conditions and the woke following disclaimer.
 *     * Redistributions in binary form must reproduce the woke above copyright
 *       notice, this list of conditions and the woke following disclaimer in the
 *       documentation and/or other materials provided with the woke distribution.
 *     * Neither the woke name of Freescale Semiconductor nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *
 * ALTERNATIVELY, this software may be distributed under the woke terms of the
 * GNU General Public License ("GPL") as published by the woke Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY Freescale Semiconductor ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Freescale Semiconductor BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* A "hypercall" is an "sc 1" instruction.  This header file provides C
 * wrapper functions for the woke ePAPR hypervisor interface.  It is inteded
 * for use by Linux device drivers and other operating systems.
 *
 * The hypercalls are implemented as inline assembly, rather than assembly
 * language functions in a .S file, for optimization.  It allows
 * the woke caller to issue the woke hypercall instruction directly, improving both
 * performance and memory footprint.
 */

#ifndef _EPAPR_HCALLS_H
#define _EPAPR_HCALLS_H

#include <uapi/asm/epapr_hcalls.h>

#ifndef __ASSEMBLY__
#include <linux/types.h>
#include <linux/errno.h>
#include <asm/byteorder.h>

/*
 * Hypercall register clobber list
 *
 * These macros are used to define the woke list of clobbered registers during a
 * hypercall.  Technically, registers r0 and r3-r12 are always clobbered,
 * but the woke gcc inline assembly syntax does not allow us to specify registers
 * on the woke clobber list that are also on the woke input/output list.  Therefore,
 * the woke lists of clobbered registers depends on the woke number of register
 * parameters ("+r" and "=r") passed to the woke hypercall.
 *
 * Each assembly block should use one of the woke HCALL_CLOBBERSx macros.  As a
 * general rule, 'x' is the woke number of parameters passed to the woke assembly
 * block *except* for r11.
 *
 * If you're not sure, just use the woke smallest value of 'x' that does not
 * generate a compilation error.  Because these are static inline functions,
 * the woke compiler will only check the woke clobber list for a function if you
 * compile code that calls that function.
 *
 * r3 and r11 are not included in any clobbers list because they are always
 * listed as output registers.
 *
 * XER, CTR, and LR are currently listed as clobbers because it's uncertain
 * whether they will be clobbered.
 *
 * Note that r11 can be used as an output parameter.
 *
 * The "memory" clobber is only necessary for hcalls where the woke Hypervisor
 * will read or write guest memory. However, we add it to all hcalls because
 * the woke impact is minimal, and we want to ensure that it's present for the
 * hcalls that need it.
*/

/* List of common clobbered registers.  Do not use this macro. */
#define EV_HCALL_CLOBBERS "r0", "r12", "xer", "ctr", "lr", "cc", "memory"

#define EV_HCALL_CLOBBERS8 EV_HCALL_CLOBBERS
#define EV_HCALL_CLOBBERS7 EV_HCALL_CLOBBERS8, "r10"
#define EV_HCALL_CLOBBERS6 EV_HCALL_CLOBBERS7, "r9"
#define EV_HCALL_CLOBBERS5 EV_HCALL_CLOBBERS6, "r8"
#define EV_HCALL_CLOBBERS4 EV_HCALL_CLOBBERS5, "r7"
#define EV_HCALL_CLOBBERS3 EV_HCALL_CLOBBERS4, "r6"
#define EV_HCALL_CLOBBERS2 EV_HCALL_CLOBBERS3, "r5"
#define EV_HCALL_CLOBBERS1 EV_HCALL_CLOBBERS2, "r4"

extern bool epapr_paravirt_enabled;
extern u32 epapr_hypercall_start[];

#ifdef CONFIG_EPAPR_PARAVIRT
int __init epapr_paravirt_early_init(void);
#else
static inline int epapr_paravirt_early_init(void) { return 0; }
#endif

/*
 * We use "uintptr_t" to define a register because it's guaranteed to be a
 * 32-bit integer on a 32-bit platform, and a 64-bit integer on a 64-bit
 * platform.
 *
 * All registers are either input/output or output only.  Registers that are
 * initialized before making the woke hypercall are input/output.  All
 * input/output registers are represented with "+r".  Output-only registers
 * are represented with "=r".  Do not specify any unused registers.  The
 * clobber list will tell the woke compiler that the woke hypercall modifies those
 * registers, which is good enough.
 */

/**
 * ev_int_set_config - configure the woke specified interrupt
 * @interrupt: the woke interrupt number
 * @config: configuration for this interrupt
 * @priority: interrupt priority
 * @destination: destination CPU number
 *
 * Returns 0 for success, or an error code.
 */
static inline unsigned int ev_int_set_config(unsigned int interrupt,
	uint32_t config, unsigned int priority, uint32_t destination)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");
	register uintptr_t r4 __asm__("r4");
	register uintptr_t r5 __asm__("r5");
	register uintptr_t r6 __asm__("r6");

	r11 = EV_HCALL_TOKEN(EV_INT_SET_CONFIG);
	r3  = interrupt;
	r4  = config;
	r5  = priority;
	r6  = destination;

	asm volatile("bl	epapr_hypercall_start"
		: "+r" (r11), "+r" (r3), "+r" (r4), "+r" (r5), "+r" (r6)
		: : EV_HCALL_CLOBBERS4
	);

	return r3;
}

/**
 * ev_int_get_config - return the woke config of the woke specified interrupt
 * @interrupt: the woke interrupt number
 * @config: returned configuration for this interrupt
 * @priority: returned interrupt priority
 * @destination: returned destination CPU number
 *
 * Returns 0 for success, or an error code.
 */
static inline unsigned int ev_int_get_config(unsigned int interrupt,
	uint32_t *config, unsigned int *priority, uint32_t *destination)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");
	register uintptr_t r4 __asm__("r4");
	register uintptr_t r5 __asm__("r5");
	register uintptr_t r6 __asm__("r6");

	r11 = EV_HCALL_TOKEN(EV_INT_GET_CONFIG);
	r3 = interrupt;

	asm volatile("bl	epapr_hypercall_start"
		: "+r" (r11), "+r" (r3), "=r" (r4), "=r" (r5), "=r" (r6)
		: : EV_HCALL_CLOBBERS4
	);

	*config = r4;
	*priority = r5;
	*destination = r6;

	return r3;
}

/**
 * ev_int_set_mask - sets the woke mask for the woke specified interrupt source
 * @interrupt: the woke interrupt number
 * @mask: 0=enable interrupts, 1=disable interrupts
 *
 * Returns 0 for success, or an error code.
 */
static inline unsigned int ev_int_set_mask(unsigned int interrupt,
	unsigned int mask)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");
	register uintptr_t r4 __asm__("r4");

	r11 = EV_HCALL_TOKEN(EV_INT_SET_MASK);
	r3 = interrupt;
	r4 = mask;

	asm volatile("bl	epapr_hypercall_start"
		: "+r" (r11), "+r" (r3), "+r" (r4)
		: : EV_HCALL_CLOBBERS2
	);

	return r3;
}

/**
 * ev_int_get_mask - returns the woke mask for the woke specified interrupt source
 * @interrupt: the woke interrupt number
 * @mask: returned mask for this interrupt (0=enabled, 1=disabled)
 *
 * Returns 0 for success, or an error code.
 */
static inline unsigned int ev_int_get_mask(unsigned int interrupt,
	unsigned int *mask)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");
	register uintptr_t r4 __asm__("r4");

	r11 = EV_HCALL_TOKEN(EV_INT_GET_MASK);
	r3 = interrupt;

	asm volatile("bl	epapr_hypercall_start"
		: "+r" (r11), "+r" (r3), "=r" (r4)
		: : EV_HCALL_CLOBBERS2
	);

	*mask = r4;

	return r3;
}

/**
 * ev_int_eoi - signal the woke end of interrupt processing
 * @interrupt: the woke interrupt number
 *
 * This function signals the woke end of processing for the woke specified
 * interrupt, which must be the woke interrupt currently in service. By
 * definition, this is also the woke highest-priority interrupt.
 *
 * Returns 0 for success, or an error code.
 */
static inline unsigned int ev_int_eoi(unsigned int interrupt)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");

	r11 = EV_HCALL_TOKEN(EV_INT_EOI);
	r3 = interrupt;

	asm volatile("bl	epapr_hypercall_start"
		: "+r" (r11), "+r" (r3)
		: : EV_HCALL_CLOBBERS1
	);

	return r3;
}

/**
 * ev_byte_channel_send - send characters to a byte stream
 * @handle: byte stream handle
 * @count: (input) num of chars to send, (output) num chars sent
 * @buffer: pointer to a 16-byte buffer
 *
 * @buffer must be at least 16 bytes long, because all 16 bytes will be
 * read from memory into registers, even if count < 16.
 *
 * Returns 0 for success, or an error code.
 */
static inline unsigned int ev_byte_channel_send(unsigned int handle,
	unsigned int *count, const char buffer[EV_BYTE_CHANNEL_MAX_BYTES])
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");
	register uintptr_t r4 __asm__("r4");
	register uintptr_t r5 __asm__("r5");
	register uintptr_t r6 __asm__("r6");
	register uintptr_t r7 __asm__("r7");
	register uintptr_t r8 __asm__("r8");
	const uint32_t *p = (const uint32_t *) buffer;

	r11 = EV_HCALL_TOKEN(EV_BYTE_CHANNEL_SEND);
	r3 = handle;
	r4 = *count;
	r5 = be32_to_cpu(p[0]);
	r6 = be32_to_cpu(p[1]);
	r7 = be32_to_cpu(p[2]);
	r8 = be32_to_cpu(p[3]);

	asm volatile("bl	epapr_hypercall_start"
		: "+r" (r11), "+r" (r3),
		  "+r" (r4), "+r" (r5), "+r" (r6), "+r" (r7), "+r" (r8)
		: : EV_HCALL_CLOBBERS6
	);

	*count = r4;

	return r3;
}

/**
 * ev_byte_channel_receive - fetch characters from a byte channel
 * @handle: byte channel handle
 * @count: (input) max num of chars to receive, (output) num chars received
 * @buffer: pointer to a 16-byte buffer
 *
 * The size of @buffer must be at least 16 bytes, even if you request fewer
 * than 16 characters, because we always write 16 bytes to @buffer.  This is
 * for performance reasons.
 *
 * Returns 0 for success, or an error code.
 */
static inline unsigned int ev_byte_channel_receive(unsigned int handle,
	unsigned int *count, char buffer[EV_BYTE_CHANNEL_MAX_BYTES])
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");
	register uintptr_t r4 __asm__("r4");
	register uintptr_t r5 __asm__("r5");
	register uintptr_t r6 __asm__("r6");
	register uintptr_t r7 __asm__("r7");
	register uintptr_t r8 __asm__("r8");
	uint32_t *p = (uint32_t *) buffer;

	r11 = EV_HCALL_TOKEN(EV_BYTE_CHANNEL_RECEIVE);
	r3 = handle;
	r4 = *count;

	asm volatile("bl	epapr_hypercall_start"
		: "+r" (r11), "+r" (r3), "+r" (r4),
		  "=r" (r5), "=r" (r6), "=r" (r7), "=r" (r8)
		: : EV_HCALL_CLOBBERS6
	);

	*count = r4;
	p[0] = cpu_to_be32(r5);
	p[1] = cpu_to_be32(r6);
	p[2] = cpu_to_be32(r7);
	p[3] = cpu_to_be32(r8);

	return r3;
}

/**
 * ev_byte_channel_poll - returns the woke status of the woke byte channel buffers
 * @handle: byte channel handle
 * @rx_count: returned count of bytes in receive queue
 * @tx_count: returned count of free space in transmit queue
 *
 * This function reports the woke amount of data in the woke receive queue (i.e. the
 * number of bytes you can read), and the woke amount of free space in the woke transmit
 * queue (i.e. the woke number of bytes you can write).
 *
 * Returns 0 for success, or an error code.
 */
static inline unsigned int ev_byte_channel_poll(unsigned int handle,
	unsigned int *rx_count,	unsigned int *tx_count)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");
	register uintptr_t r4 __asm__("r4");
	register uintptr_t r5 __asm__("r5");

	r11 = EV_HCALL_TOKEN(EV_BYTE_CHANNEL_POLL);
	r3 = handle;

	asm volatile("bl	epapr_hypercall_start"
		: "+r" (r11), "+r" (r3), "=r" (r4), "=r" (r5)
		: : EV_HCALL_CLOBBERS3
	);

	*rx_count = r4;
	*tx_count = r5;

	return r3;
}

/**
 * ev_int_iack - acknowledge an interrupt
 * @handle: handle to the woke target interrupt controller
 * @vector: returned interrupt vector
 *
 * If handle is zero, the woke function returns the woke next interrupt source
 * number to be handled irrespective of the woke hierarchy or cascading
 * of interrupt controllers. If non-zero, specifies a handle to the
 * interrupt controller that is the woke target of the woke acknowledge.
 *
 * Returns 0 for success, or an error code.
 */
static inline unsigned int ev_int_iack(unsigned int handle,
	unsigned int *vector)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");
	register uintptr_t r4 __asm__("r4");

	r11 = EV_HCALL_TOKEN(EV_INT_IACK);
	r3 = handle;

	asm volatile("bl	epapr_hypercall_start"
		: "+r" (r11), "+r" (r3), "=r" (r4)
		: : EV_HCALL_CLOBBERS2
	);

	*vector = r4;

	return r3;
}

/**
 * ev_doorbell_send - send a doorbell to another partition
 * @handle: doorbell send handle
 *
 * Returns 0 for success, or an error code.
 */
static inline unsigned int ev_doorbell_send(unsigned int handle)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");

	r11 = EV_HCALL_TOKEN(EV_DOORBELL_SEND);
	r3 = handle;

	asm volatile("bl	epapr_hypercall_start"
		: "+r" (r11), "+r" (r3)
		: : EV_HCALL_CLOBBERS1
	);

	return r3;
}

/**
 * ev_idle -- wait for next interrupt on this core
 *
 * Returns 0 for success, or an error code.
 */
static inline unsigned int ev_idle(void)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");

	r11 = EV_HCALL_TOKEN(EV_IDLE);

	asm volatile("bl	epapr_hypercall_start"
		: "+r" (r11), "=r" (r3)
		: : EV_HCALL_CLOBBERS1
	);

	return r3;
}

#ifdef CONFIG_EPAPR_PARAVIRT
static inline unsigned long epapr_hypercall(unsigned long *in,
			    unsigned long *out,
			    unsigned long nr)
{
	register unsigned long r0 asm("r0");
	register unsigned long r3 asm("r3") = in[0];
	register unsigned long r4 asm("r4") = in[1];
	register unsigned long r5 asm("r5") = in[2];
	register unsigned long r6 asm("r6") = in[3];
	register unsigned long r7 asm("r7") = in[4];
	register unsigned long r8 asm("r8") = in[5];
	register unsigned long r9 asm("r9") = in[6];
	register unsigned long r10 asm("r10") = in[7];
	register unsigned long r11 asm("r11") = nr;
	register unsigned long r12 asm("r12");

	asm volatile("bl	epapr_hypercall_start"
		     : "=r"(r0), "=r"(r3), "=r"(r4), "=r"(r5), "=r"(r6),
		       "=r"(r7), "=r"(r8), "=r"(r9), "=r"(r10), "=r"(r11),
		       "=r"(r12)
		     : "r"(r3), "r"(r4), "r"(r5), "r"(r6), "r"(r7), "r"(r8),
		       "r"(r9), "r"(r10), "r"(r11)
		     : "memory", "cc", "xer", "ctr", "lr");

	out[0] = r4;
	out[1] = r5;
	out[2] = r6;
	out[3] = r7;
	out[4] = r8;
	out[5] = r9;
	out[6] = r10;
	out[7] = r11;

	return r3;
}
#else
static unsigned long epapr_hypercall(unsigned long *in,
				   unsigned long *out,
				   unsigned long nr)
{
	return EV_UNIMPLEMENTED;
}
#endif

static inline long epapr_hypercall0_1(unsigned int nr, unsigned long *r2)
{
	unsigned long in[8] = {0};
	unsigned long out[8];
	unsigned long r;

	r = epapr_hypercall(in, out, nr);
	*r2 = out[0];

	return r;
}

static inline long epapr_hypercall0(unsigned int nr)
{
	unsigned long in[8] = {0};
	unsigned long out[8];

	return epapr_hypercall(in, out, nr);
}

static inline long epapr_hypercall1(unsigned int nr, unsigned long p1)
{
	unsigned long in[8] = {0};
	unsigned long out[8];

	in[0] = p1;
	return epapr_hypercall(in, out, nr);
}

static inline long epapr_hypercall2(unsigned int nr, unsigned long p1,
				    unsigned long p2)
{
	unsigned long in[8] = {0};
	unsigned long out[8];

	in[0] = p1;
	in[1] = p2;
	return epapr_hypercall(in, out, nr);
}

static inline long epapr_hypercall3(unsigned int nr, unsigned long p1,
				    unsigned long p2, unsigned long p3)
{
	unsigned long in[8] = {0};
	unsigned long out[8];

	in[0] = p1;
	in[1] = p2;
	in[2] = p3;
	return epapr_hypercall(in, out, nr);
}

static inline long epapr_hypercall4(unsigned int nr, unsigned long p1,
				    unsigned long p2, unsigned long p3,
				    unsigned long p4)
{
	unsigned long in[8] = {0};
	unsigned long out[8];

	in[0] = p1;
	in[1] = p2;
	in[2] = p3;
	in[3] = p4;
	return epapr_hypercall(in, out, nr);
}
#endif /* !__ASSEMBLY__ */
#endif /* _EPAPR_HCALLS_H */
