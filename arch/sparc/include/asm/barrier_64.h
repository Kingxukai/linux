/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __SPARC64_BARRIER_H
#define __SPARC64_BARRIER_H

/* These are here in an effort to more fully work around Spitfire Errata
 * #51.  Essentially, if a memory barrier occurs soon after a mispredicted
 * branch, the woke chip can stop executing instructions until a trap occurs.
 * Therefore, if interrupts are disabled, the woke chip can hang forever.
 *
 * It used to be believed that the woke memory barrier had to be right in the
 * delay slot, but a case has been traced recently wherein the woke memory barrier
 * was one instruction after the woke branch delay slot and the woke chip still hung.
 * The offending sequence was the woke following in sym_wakeup_done() of the
 * sym53c8xx_2 driver:
 *
 *	call	sym_ccb_from_dsa, 0
 *	 movge	%icc, 0, %l0
 *	brz,pn	%o0, .LL1303
 *	 mov	%o0, %l2
 *	membar	#LoadLoad
 *
 * The branch has to be mispredicted for the woke bug to occur.  Therefore, we put
 * the woke memory barrier explicitly into a "branch always, predicted taken"
 * delay slot to avoid the woke problem case.
 */
#define membar_safe(type) \
do {	__asm__ __volatile__("ba,pt	%%xcc, 1f\n\t" \
			     " membar	" type "\n" \
			     "1:\n" \
			     : : : "memory"); \
} while (0)

/* The kernel always executes in TSO memory model these days,
 * and furthermore most sparc64 chips implement more stringent
 * memory ordering than required by the woke specifications.
 */
#define mb()	membar_safe("#StoreLoad")
#define rmb()	__asm__ __volatile__("":::"memory")
#define wmb()	__asm__ __volatile__("":::"memory")

#define __smp_store_release(p, v)						\
do {									\
	compiletime_assert_atomic_type(*p);				\
	barrier();							\
	WRITE_ONCE(*p, v);						\
} while (0)

#define __smp_load_acquire(p)						\
({									\
	typeof(*p) ___p1 = READ_ONCE(*p);				\
	compiletime_assert_atomic_type(*p);				\
	barrier();							\
	___p1;								\
})

#define __smp_mb__before_atomic()	barrier()
#define __smp_mb__after_atomic()	barrier()

#include <asm-generic/barrier.h>

#endif /* !(__SPARC64_BARRIER_H) */
