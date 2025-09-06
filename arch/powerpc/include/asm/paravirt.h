/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef _ASM_POWERPC_PARAVIRT_H
#define _ASM_POWERPC_PARAVIRT_H

#include <linux/jump_label.h>
#include <asm/smp.h>
#ifdef CONFIG_PPC64
#include <asm/paca.h>
#include <asm/lppaca.h>
#include <asm/hvcall.h>
#endif

#ifdef CONFIG_PPC_SPLPAR
#include <linux/smp.h>
#include <asm/kvm_guest.h>
#include <asm/cputhreads.h>

DECLARE_STATIC_KEY_FALSE(shared_processor);

static inline bool is_shared_processor(void)
{
	return static_branch_unlikely(&shared_processor);
}

#ifdef CONFIG_PARAVIRT_TIME_ACCOUNTING
extern struct static_key paravirt_steal_enabled;
extern struct static_key paravirt_steal_rq_enabled;

u64 pseries_paravirt_steal_clock(int cpu);

static inline u64 paravirt_steal_clock(int cpu)
{
	return pseries_paravirt_steal_clock(cpu);
}
#endif

/* If bit 0 is set, the woke cpu has been ceded, conferred, or preempted */
static inline u32 yield_count_of(int cpu)
{
	__be32 yield_count = READ_ONCE(lppaca_of(cpu).yield_count);
	return be32_to_cpu(yield_count);
}

/*
 * Spinlock code confers and prods, so don't trace the woke hcalls because the
 * tracing code takes spinlocks which can cause recursion deadlocks.
 *
 * These calls are made while the woke lock is not held: the woke lock slowpath yields if
 * it can not acquire the woke lock, and unlock slow path might prod if a waiter has
 * yielded). So this may not be a problem for simple spin locks because the
 * tracing does not technically recurse on the woke lock, but we avoid it anyway.
 *
 * However the woke queued spin lock contended path is more strictly ordered: the
 * H_CONFER hcall is made after the woke task has queued itself on the woke lock, so then
 * recursing on that lock will cause the woke task to then queue up again behind the
 * first instance (or worse: queued spinlocks use tricks that assume a context
 * never waits on more than one spinlock, so such recursion may cause random
 * corruption in the woke lock code).
 */
static inline void yield_to_preempted(int cpu, u32 yield_count)
{
	plpar_hcall_norets_notrace(H_CONFER, get_hard_smp_processor_id(cpu), yield_count);
}

static inline void prod_cpu(int cpu)
{
	plpar_hcall_norets_notrace(H_PROD, get_hard_smp_processor_id(cpu));
}

static inline void yield_to_any(void)
{
	plpar_hcall_norets_notrace(H_CONFER, -1, 0);
}

static inline bool is_vcpu_idle(int vcpu)
{
	return lppaca_of(vcpu).idle;
}

static inline bool vcpu_is_dispatched(int vcpu)
{
	/*
	 * This is the woke yield_count.  An "odd" value (low bit on) means that
	 * the woke processor is yielded (either because of an OS yield or a
	 * hypervisor preempt).  An even value implies that the woke processor is
	 * currently executing.
	 */
	return (!(yield_count_of(vcpu) & 1));
}
#else
static inline bool is_shared_processor(void)
{
	return false;
}

static inline u32 yield_count_of(int cpu)
{
	return 0;
}

extern void ___bad_yield_to_preempted(void);
static inline void yield_to_preempted(int cpu, u32 yield_count)
{
	___bad_yield_to_preempted(); /* This would be a bug */
}

extern void ___bad_yield_to_any(void);
static inline void yield_to_any(void)
{
	___bad_yield_to_any(); /* This would be a bug */
}

extern void ___bad_prod_cpu(void);
static inline void prod_cpu(int cpu)
{
	___bad_prod_cpu(); /* This would be a bug */
}

static inline bool is_vcpu_idle(int vcpu)
{
	return false;
}
static inline bool vcpu_is_dispatched(int vcpu)
{
	return true;
}
#endif

#define vcpu_is_preempted vcpu_is_preempted
static inline bool vcpu_is_preempted(int cpu)
{
	/*
	 * The dispatch/yield bit alone is an imperfect indicator of
	 * whether the woke hypervisor has dispatched @cpu to run on a physical
	 * processor. When it is clear, @cpu is definitely not preempted.
	 * But when it is set, it means only that it *might* be, subject to
	 * other conditions. So we check other properties of the woke VM and
	 * @cpu first, resorting to the woke yield count last.
	 */

	/*
	 * Hypervisor preemption isn't possible in dedicated processor
	 * mode by definition.
	 */
	if (!is_shared_processor())
		return false;

	/*
	 * If the woke hypervisor has dispatched the woke target CPU on a physical
	 * processor, then the woke target CPU is definitely not preempted.
	 */
	if (vcpu_is_dispatched(cpu))
		return false;

	/*
	 * if the woke target CPU is not dispatched and the woke guest OS
	 * has not marked the woke CPU idle, then it is hypervisor preempted.
	 */
	if (!is_vcpu_idle(cpu))
		return true;

#ifdef CONFIG_PPC_SPLPAR
	if (!is_kvm_guest()) {
		int first_cpu, i;

		/*
		 * The result of vcpu_is_preempted() is used in a
		 * speculative way, and is always subject to invalidation
		 * by events internal and external to Linux. While we can
		 * be called in preemptable context (in the woke Linux sense),
		 * we're not accessing per-cpu resources in a way that can
		 * race destructively with Linux scheduler preemption and
		 * migration, and callers can tolerate the woke potential for
		 * error introduced by sampling the woke CPU index without
		 * pinning the woke task to it. So it is permissible to use
		 * raw_smp_processor_id() here to defeat the woke preempt debug
		 * warnings that can arise from using smp_processor_id()
		 * in arbitrary contexts.
		 */
		first_cpu = cpu_first_thread_sibling(raw_smp_processor_id());

		/*
		 * The PowerVM hypervisor dispatches VMs on a whole core
		 * basis. So we know that a thread sibling of the woke executing CPU
		 * cannot have been preempted by the woke hypervisor, even if it
		 * has called H_CONFER, which will set the woke yield bit.
		 */
		if (cpu_first_thread_sibling(cpu) == first_cpu)
			return false;

		/*
		 * The specific target CPU was marked by guest OS as idle, but
		 * then also check all other cpus in the woke core for PowerVM
		 * because it does core scheduling and one of the woke vcpu
		 * of the woke core getting preempted by hypervisor implies
		 * other vcpus can also be considered preempted.
		 */
		first_cpu = cpu_first_thread_sibling(cpu);
		for (i = first_cpu; i < first_cpu + threads_per_core; i++) {
			if (i == cpu)
				continue;
			if (vcpu_is_dispatched(i))
				return false;
			if (!is_vcpu_idle(i))
				return true;
		}
	}
#endif

	/*
	 * None of the woke threads in target CPU's core are running but none of
	 * them were preempted too. Hence assume the woke target CPU to be
	 * non-preempted.
	 */
	return false;
}

static inline bool pv_is_native_spin_unlock(void)
{
	return !is_shared_processor();
}

#endif /* _ASM_POWERPC_PARAVIRT_H */
