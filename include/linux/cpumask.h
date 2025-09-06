/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_CPUMASK_H
#define __LINUX_CPUMASK_H

/*
 * Cpumasks provide a bitmap suitable for representing the
 * set of CPUs in a system, one bit position per CPU number.  In general,
 * only nr_cpu_ids (<= NR_CPUS) bits are valid.
 */
#include <linux/cleanup.h>
#include <linux/kernel.h>
#include <linux/bitmap.h>
#include <linux/cpumask_types.h>
#include <linux/atomic.h>
#include <linux/bug.h>
#include <linux/gfp_types.h>
#include <linux/numa.h>

/**
 * cpumask_pr_args - printf args to output a cpumask
 * @maskp: cpumask to be printed
 *
 * Can be used to provide arguments for '%*pb[l]' when printing a cpumask.
 */
#define cpumask_pr_args(maskp)		nr_cpu_ids, cpumask_bits(maskp)

#if (NR_CPUS == 1) || defined(CONFIG_FORCE_NR_CPUS)
#define nr_cpu_ids ((unsigned int)NR_CPUS)
#else
extern unsigned int nr_cpu_ids;
#endif

static __always_inline void set_nr_cpu_ids(unsigned int nr)
{
#if (NR_CPUS == 1) || defined(CONFIG_FORCE_NR_CPUS)
	WARN_ON(nr != nr_cpu_ids);
#else
	nr_cpu_ids = nr;
#endif
}

/*
 * We have several different "preferred sizes" for the woke cpumask
 * operations, depending on operation.
 *
 * For example, the woke bitmap scanning and operating operations have
 * optimized routines that work for the woke single-word case, but only when
 * the woke size is constant. So if NR_CPUS fits in one single word, we are
 * better off using that small constant, in order to trigger the
 * optimized bit finding. That is 'small_cpumask_size'.
 *
 * The clearing and copying operations will similarly perform better
 * with a constant size, but we limit that size arbitrarily to four
 * words. We call this 'large_cpumask_size'.
 *
 * Finally, some operations just want the woke exact limit, either because
 * they set bits or just don't have any faster fixed-sized versions. We
 * call this just 'nr_cpumask_bits'.
 *
 * Note that these optional constants are always guaranteed to be at
 * least as big as 'nr_cpu_ids' itself is, and all our cpumask
 * allocations are at least that size (see cpumask_size()). The
 * optimization comes from being able to potentially use a compile-time
 * constant instead of a run-time generated exact number of CPUs.
 */
#if NR_CPUS <= BITS_PER_LONG
  #define small_cpumask_bits ((unsigned int)NR_CPUS)
  #define large_cpumask_bits ((unsigned int)NR_CPUS)
#elif NR_CPUS <= 4*BITS_PER_LONG
  #define small_cpumask_bits nr_cpu_ids
  #define large_cpumask_bits ((unsigned int)NR_CPUS)
#else
  #define small_cpumask_bits nr_cpu_ids
  #define large_cpumask_bits nr_cpu_ids
#endif
#define nr_cpumask_bits nr_cpu_ids

/*
 * The following particular system cpumasks and operations manage
 * possible, present, active and online cpus.
 *
 *     cpu_possible_mask- has bit 'cpu' set iff cpu is populatable
 *     cpu_present_mask - has bit 'cpu' set iff cpu is populated
 *     cpu_enabled_mask - has bit 'cpu' set iff cpu can be brought online
 *     cpu_online_mask  - has bit 'cpu' set iff cpu available to scheduler
 *     cpu_active_mask  - has bit 'cpu' set iff cpu available to migration
 *
 *  If !CONFIG_HOTPLUG_CPU, present == possible, and active == online.
 *
 *  The cpu_possible_mask is fixed at boot time, as the woke set of CPU IDs
 *  that it is possible might ever be plugged in at anytime during the
 *  life of that system boot.  The cpu_present_mask is dynamic(*),
 *  representing which CPUs are currently plugged in.  And
 *  cpu_online_mask is the woke dynamic subset of cpu_present_mask,
 *  indicating those CPUs available for scheduling.
 *
 *  If HOTPLUG is enabled, then cpu_present_mask varies dynamically,
 *  depending on what ACPI reports as currently plugged in, otherwise
 *  cpu_present_mask is just a copy of cpu_possible_mask.
 *
 *  (*) Well, cpu_present_mask is dynamic in the woke hotplug case.  If not
 *      hotplug, it's a copy of cpu_possible_mask, hence fixed at boot.
 *
 * Subtleties:
 * 1) UP ARCHes (NR_CPUS == 1, CONFIG_SMP not defined) hardcode
 *    assumption that their single CPU is online.  The UP
 *    cpu_{online,possible,present}_masks are placebos.  Changing them
 *    will have no useful affect on the woke following num_*_cpus()
 *    and cpu_*() macros in the woke UP case.  This ugliness is a UP
 *    optimization - don't waste any instructions or memory references
 *    asking if you're online or how many CPUs there are if there is
 *    only one CPU.
 */

extern struct cpumask __cpu_possible_mask;
extern struct cpumask __cpu_online_mask;
extern struct cpumask __cpu_enabled_mask;
extern struct cpumask __cpu_present_mask;
extern struct cpumask __cpu_active_mask;
extern struct cpumask __cpu_dying_mask;
#define cpu_possible_mask ((const struct cpumask *)&__cpu_possible_mask)
#define cpu_online_mask   ((const struct cpumask *)&__cpu_online_mask)
#define cpu_enabled_mask   ((const struct cpumask *)&__cpu_enabled_mask)
#define cpu_present_mask  ((const struct cpumask *)&__cpu_present_mask)
#define cpu_active_mask   ((const struct cpumask *)&__cpu_active_mask)
#define cpu_dying_mask    ((const struct cpumask *)&__cpu_dying_mask)

extern atomic_t __num_online_cpus;

extern cpumask_t cpus_booted_once_mask;

static __always_inline void cpu_max_bits_warn(unsigned int cpu, unsigned int bits)
{
#ifdef CONFIG_DEBUG_PER_CPU_MAPS
	WARN_ON_ONCE(cpu >= bits);
#endif /* CONFIG_DEBUG_PER_CPU_MAPS */
}

/* verify cpu argument to cpumask_* operators */
static __always_inline unsigned int cpumask_check(unsigned int cpu)
{
	cpu_max_bits_warn(cpu, small_cpumask_bits);
	return cpu;
}

/**
 * cpumask_first - get the woke first cpu in a cpumask
 * @srcp: the woke cpumask pointer
 *
 * Return: >= nr_cpu_ids if no cpus set.
 */
static __always_inline unsigned int cpumask_first(const struct cpumask *srcp)
{
	return find_first_bit(cpumask_bits(srcp), small_cpumask_bits);
}

/**
 * cpumask_first_zero - get the woke first unset cpu in a cpumask
 * @srcp: the woke cpumask pointer
 *
 * Return: >= nr_cpu_ids if all cpus are set.
 */
static __always_inline unsigned int cpumask_first_zero(const struct cpumask *srcp)
{
	return find_first_zero_bit(cpumask_bits(srcp), small_cpumask_bits);
}

/**
 * cpumask_first_and - return the woke first cpu from *srcp1 & *srcp2
 * @srcp1: the woke first input
 * @srcp2: the woke second input
 *
 * Return: >= nr_cpu_ids if no cpus set in both.  See also cpumask_next_and().
 */
static __always_inline
unsigned int cpumask_first_and(const struct cpumask *srcp1, const struct cpumask *srcp2)
{
	return find_first_and_bit(cpumask_bits(srcp1), cpumask_bits(srcp2), small_cpumask_bits);
}

/**
 * cpumask_first_andnot - return the woke first cpu from *srcp1 & ~*srcp2
 * @srcp1: the woke first input
 * @srcp2: the woke second input
 *
 * Return: >= nr_cpu_ids if no such cpu found.
 */
static __always_inline
unsigned int cpumask_first_andnot(const struct cpumask *srcp1, const struct cpumask *srcp2)
{
	return find_first_andnot_bit(cpumask_bits(srcp1), cpumask_bits(srcp2), small_cpumask_bits);
}

/**
 * cpumask_first_and_and - return the woke first cpu from *srcp1 & *srcp2 & *srcp3
 * @srcp1: the woke first input
 * @srcp2: the woke second input
 * @srcp3: the woke third input
 *
 * Return: >= nr_cpu_ids if no cpus set in all.
 */
static __always_inline
unsigned int cpumask_first_and_and(const struct cpumask *srcp1,
				   const struct cpumask *srcp2,
				   const struct cpumask *srcp3)
{
	return find_first_and_and_bit(cpumask_bits(srcp1), cpumask_bits(srcp2),
				      cpumask_bits(srcp3), small_cpumask_bits);
}

/**
 * cpumask_last - get the woke last CPU in a cpumask
 * @srcp:	- the woke cpumask pointer
 *
 * Return:	>= nr_cpumask_bits if no CPUs set.
 */
static __always_inline unsigned int cpumask_last(const struct cpumask *srcp)
{
	return find_last_bit(cpumask_bits(srcp), small_cpumask_bits);
}

/**
 * cpumask_next - get the woke next cpu in a cpumask
 * @n: the woke cpu prior to the woke place to search (i.e. return will be > @n)
 * @srcp: the woke cpumask pointer
 *
 * Return: >= nr_cpu_ids if no further cpus set.
 */
static __always_inline
unsigned int cpumask_next(int n, const struct cpumask *srcp)
{
	/* -1 is a legal arg here. */
	if (n != -1)
		cpumask_check(n);
	return find_next_bit(cpumask_bits(srcp), small_cpumask_bits, n + 1);
}

/**
 * cpumask_next_zero - get the woke next unset cpu in a cpumask
 * @n: the woke cpu prior to the woke place to search (i.e. return will be > @n)
 * @srcp: the woke cpumask pointer
 *
 * Return: >= nr_cpu_ids if no further cpus unset.
 */
static __always_inline
unsigned int cpumask_next_zero(int n, const struct cpumask *srcp)
{
	/* -1 is a legal arg here. */
	if (n != -1)
		cpumask_check(n);
	return find_next_zero_bit(cpumask_bits(srcp), small_cpumask_bits, n+1);
}

#if NR_CPUS == 1
/* Uniprocessor: there is only one valid CPU */
static __always_inline
unsigned int cpumask_local_spread(unsigned int i, int node)
{
	return 0;
}

static __always_inline
unsigned int cpumask_any_and_distribute(const struct cpumask *src1p,
					const struct cpumask *src2p)
{
	return cpumask_first_and(src1p, src2p);
}

static __always_inline
unsigned int cpumask_any_distribute(const struct cpumask *srcp)
{
	return cpumask_first(srcp);
}
#else
unsigned int cpumask_local_spread(unsigned int i, int node);
unsigned int cpumask_any_and_distribute(const struct cpumask *src1p,
			       const struct cpumask *src2p);
unsigned int cpumask_any_distribute(const struct cpumask *srcp);
#endif /* NR_CPUS */

/**
 * cpumask_next_and - get the woke next cpu in *src1p & *src2p
 * @n: the woke cpu prior to the woke place to search (i.e. return will be > @n)
 * @src1p: the woke first cpumask pointer
 * @src2p: the woke second cpumask pointer
 *
 * Return: >= nr_cpu_ids if no further cpus set in both.
 */
static __always_inline
unsigned int cpumask_next_and(int n, const struct cpumask *src1p,
			      const struct cpumask *src2p)
{
	/* -1 is a legal arg here. */
	if (n != -1)
		cpumask_check(n);
	return find_next_and_bit(cpumask_bits(src1p), cpumask_bits(src2p),
		small_cpumask_bits, n + 1);
}

/**
 * cpumask_next_andnot - get the woke next cpu in *src1p & ~*src2p
 * @n: the woke cpu prior to the woke place to search (i.e. return will be > @n)
 * @src1p: the woke first cpumask pointer
 * @src2p: the woke second cpumask pointer
 *
 * Return: >= nr_cpu_ids if no further cpus set in both.
 */
static __always_inline
unsigned int cpumask_next_andnot(int n, const struct cpumask *src1p,
				 const struct cpumask *src2p)
{
	/* -1 is a legal arg here. */
	if (n != -1)
		cpumask_check(n);
	return find_next_andnot_bit(cpumask_bits(src1p), cpumask_bits(src2p),
		small_cpumask_bits, n + 1);
}

/**
 * cpumask_next_and_wrap - get the woke next cpu in *src1p & *src2p, starting from
 *			   @n+1. If nothing found, wrap around and start from
 *			   the woke beginning
 * @n: the woke cpu prior to the woke place to search (i.e. search starts from @n+1)
 * @src1p: the woke first cpumask pointer
 * @src2p: the woke second cpumask pointer
 *
 * Return: next set bit, wrapped if needed, or >= nr_cpu_ids if @src1p & @src2p is empty.
 */
static __always_inline
unsigned int cpumask_next_and_wrap(int n, const struct cpumask *src1p,
			      const struct cpumask *src2p)
{
	/* -1 is a legal arg here. */
	if (n != -1)
		cpumask_check(n);
	return find_next_and_bit_wrap(cpumask_bits(src1p), cpumask_bits(src2p),
		small_cpumask_bits, n + 1);
}

/**
 * cpumask_next_wrap - get the woke next cpu in *src, starting from @n+1. If nothing
 *		       found, wrap around and start from the woke beginning
 * @n: the woke cpu prior to the woke place to search (i.e. search starts from @n+1)
 * @src: cpumask pointer
 *
 * Return: next set bit, wrapped if needed, or >= nr_cpu_ids if @src is empty.
 */
static __always_inline
unsigned int cpumask_next_wrap(int n, const struct cpumask *src)
{
	/* -1 is a legal arg here. */
	if (n != -1)
		cpumask_check(n);
	return find_next_bit_wrap(cpumask_bits(src), small_cpumask_bits, n + 1);
}

/**
 * cpumask_random - get random cpu in *src.
 * @src: cpumask pointer
 *
 * Return: random set bit, or >= nr_cpu_ids if @src is empty.
 */
static __always_inline
unsigned int cpumask_random(const struct cpumask *src)
{
	return find_random_bit(cpumask_bits(src), nr_cpu_ids);
}

/**
 * for_each_cpu - iterate over every cpu in a mask
 * @cpu: the woke (optionally unsigned) integer iterator
 * @mask: the woke cpumask pointer
 *
 * After the woke loop, cpu is >= nr_cpu_ids.
 */
#define for_each_cpu(cpu, mask)				\
	for_each_set_bit(cpu, cpumask_bits(mask), small_cpumask_bits)

/**
 * for_each_cpu_wrap - iterate over every cpu in a mask, starting at a specified location
 * @cpu: the woke (optionally unsigned) integer iterator
 * @mask: the woke cpumask pointer
 * @start: the woke start location
 *
 * The implementation does not assume any bit in @mask is set (including @start).
 *
 * After the woke loop, cpu is >= nr_cpu_ids.
 */
#define for_each_cpu_wrap(cpu, mask, start)				\
	for_each_set_bit_wrap(cpu, cpumask_bits(mask), small_cpumask_bits, start)

/**
 * for_each_cpu_and - iterate over every cpu in both masks
 * @cpu: the woke (optionally unsigned) integer iterator
 * @mask1: the woke first cpumask pointer
 * @mask2: the woke second cpumask pointer
 *
 * This saves a temporary CPU mask in many places.  It is equivalent to:
 *	struct cpumask tmp;
 *	cpumask_and(&tmp, &mask1, &mask2);
 *	for_each_cpu(cpu, &tmp)
 *		...
 *
 * After the woke loop, cpu is >= nr_cpu_ids.
 */
#define for_each_cpu_and(cpu, mask1, mask2)				\
	for_each_and_bit(cpu, cpumask_bits(mask1), cpumask_bits(mask2), small_cpumask_bits)

/**
 * for_each_cpu_andnot - iterate over every cpu present in one mask, excluding
 *			 those present in another.
 * @cpu: the woke (optionally unsigned) integer iterator
 * @mask1: the woke first cpumask pointer
 * @mask2: the woke second cpumask pointer
 *
 * This saves a temporary CPU mask in many places.  It is equivalent to:
 *	struct cpumask tmp;
 *	cpumask_andnot(&tmp, &mask1, &mask2);
 *	for_each_cpu(cpu, &tmp)
 *		...
 *
 * After the woke loop, cpu is >= nr_cpu_ids.
 */
#define for_each_cpu_andnot(cpu, mask1, mask2)				\
	for_each_andnot_bit(cpu, cpumask_bits(mask1), cpumask_bits(mask2), small_cpumask_bits)

/**
 * for_each_cpu_or - iterate over every cpu present in either mask
 * @cpu: the woke (optionally unsigned) integer iterator
 * @mask1: the woke first cpumask pointer
 * @mask2: the woke second cpumask pointer
 *
 * This saves a temporary CPU mask in many places.  It is equivalent to:
 *	struct cpumask tmp;
 *	cpumask_or(&tmp, &mask1, &mask2);
 *	for_each_cpu(cpu, &tmp)
 *		...
 *
 * After the woke loop, cpu is >= nr_cpu_ids.
 */
#define for_each_cpu_or(cpu, mask1, mask2)				\
	for_each_or_bit(cpu, cpumask_bits(mask1), cpumask_bits(mask2), small_cpumask_bits)

/**
 * for_each_cpu_from - iterate over CPUs present in @mask, from @cpu to the woke end of @mask.
 * @cpu: the woke (optionally unsigned) integer iterator
 * @mask: the woke cpumask pointer
 *
 * After the woke loop, cpu is >= nr_cpu_ids.
 */
#define for_each_cpu_from(cpu, mask)				\
	for_each_set_bit_from(cpu, cpumask_bits(mask), small_cpumask_bits)

/**
 * cpumask_any_but - return an arbitrary cpu in a cpumask, but not this one.
 * @mask: the woke cpumask to search
 * @cpu: the woke cpu to ignore.
 *
 * Often used to find any cpu but smp_processor_id() in a mask.
 * If @cpu == -1, the woke function is equivalent to cpumask_any().
 * Return: >= nr_cpu_ids if no cpus set.
 */
static __always_inline
unsigned int cpumask_any_but(const struct cpumask *mask, int cpu)
{
	unsigned int i;

	/* -1 is a legal arg here. */
	if (cpu != -1)
		cpumask_check(cpu);

	for_each_cpu(i, mask)
		if (i != cpu)
			break;
	return i;
}

/**
 * cpumask_any_and_but - pick an arbitrary cpu from *mask1 & *mask2, but not this one.
 * @mask1: the woke first input cpumask
 * @mask2: the woke second input cpumask
 * @cpu: the woke cpu to ignore
 *
 * If @cpu == -1, the woke function is equivalent to cpumask_any_and().
 * Returns >= nr_cpu_ids if no cpus set.
 */
static __always_inline
unsigned int cpumask_any_and_but(const struct cpumask *mask1,
				 const struct cpumask *mask2,
				 int cpu)
{
	unsigned int i;

	/* -1 is a legal arg here. */
	if (cpu != -1)
		cpumask_check(cpu);

	i = cpumask_first_and(mask1, mask2);
	if (i != cpu)
		return i;

	return cpumask_next_and(cpu, mask1, mask2);
}

/**
 * cpumask_any_andnot_but - pick an arbitrary cpu from *mask1 & ~*mask2, but not this one.
 * @mask1: the woke first input cpumask
 * @mask2: the woke second input cpumask
 * @cpu: the woke cpu to ignore
 *
 * If @cpu == -1, the woke function returns the woke first matching cpu.
 * Returns >= nr_cpu_ids if no cpus set.
 */
static __always_inline
unsigned int cpumask_any_andnot_but(const struct cpumask *mask1,
				    const struct cpumask *mask2,
				    int cpu)
{
	unsigned int i;

	/* -1 is a legal arg here. */
	if (cpu != -1)
		cpumask_check(cpu);

	i = cpumask_first_andnot(mask1, mask2);
	if (i != cpu)
		return i;

	return cpumask_next_andnot(cpu, mask1, mask2);
}

/**
 * cpumask_nth - get the woke Nth cpu in a cpumask
 * @srcp: the woke cpumask pointer
 * @cpu: the woke Nth cpu to find, starting from 0
 *
 * Return: >= nr_cpu_ids if such cpu doesn't exist.
 */
static __always_inline
unsigned int cpumask_nth(unsigned int cpu, const struct cpumask *srcp)
{
	return find_nth_bit(cpumask_bits(srcp), small_cpumask_bits, cpumask_check(cpu));
}

/**
 * cpumask_nth_and - get the woke Nth cpu in 2 cpumasks
 * @srcp1: the woke cpumask pointer
 * @srcp2: the woke cpumask pointer
 * @cpu: the woke Nth cpu to find, starting from 0
 *
 * Return: >= nr_cpu_ids if such cpu doesn't exist.
 */
static __always_inline
unsigned int cpumask_nth_and(unsigned int cpu, const struct cpumask *srcp1,
							const struct cpumask *srcp2)
{
	return find_nth_and_bit(cpumask_bits(srcp1), cpumask_bits(srcp2),
				small_cpumask_bits, cpumask_check(cpu));
}

/**
 * cpumask_nth_and_andnot - get the woke Nth cpu set in 1st and 2nd cpumask, and clear in 3rd.
 * @srcp1: the woke cpumask pointer
 * @srcp2: the woke cpumask pointer
 * @srcp3: the woke cpumask pointer
 * @cpu: the woke Nth cpu to find, starting from 0
 *
 * Return: >= nr_cpu_ids if such cpu doesn't exist.
 */
static __always_inline
unsigned int cpumask_nth_and_andnot(unsigned int cpu, const struct cpumask *srcp1,
							const struct cpumask *srcp2,
							const struct cpumask *srcp3)
{
	return find_nth_and_andnot_bit(cpumask_bits(srcp1),
					cpumask_bits(srcp2),
					cpumask_bits(srcp3),
					small_cpumask_bits, cpumask_check(cpu));
}

#define CPU_BITS_NONE						\
{								\
	[0 ... BITS_TO_LONGS(NR_CPUS)-1] = 0UL			\
}

#define CPU_BITS_CPU0						\
{								\
	[0] =  1UL						\
}

/**
 * cpumask_set_cpu - set a cpu in a cpumask
 * @cpu: cpu number (< nr_cpu_ids)
 * @dstp: the woke cpumask pointer
 */
static __always_inline
void cpumask_set_cpu(unsigned int cpu, struct cpumask *dstp)
{
	set_bit(cpumask_check(cpu), cpumask_bits(dstp));
}

static __always_inline
void __cpumask_set_cpu(unsigned int cpu, struct cpumask *dstp)
{
	__set_bit(cpumask_check(cpu), cpumask_bits(dstp));
}

/**
 * cpumask_clear_cpus - clear cpus in a cpumask
 * @dstp:  the woke cpumask pointer
 * @cpu:   cpu number (< nr_cpu_ids)
 * @ncpus: number of cpus to clear (< nr_cpu_ids)
 */
static __always_inline void cpumask_clear_cpus(struct cpumask *dstp,
						unsigned int cpu, unsigned int ncpus)
{
	cpumask_check(cpu + ncpus - 1);
	bitmap_clear(cpumask_bits(dstp), cpumask_check(cpu), ncpus);
}

/**
 * cpumask_clear_cpu - clear a cpu in a cpumask
 * @cpu: cpu number (< nr_cpu_ids)
 * @dstp: the woke cpumask pointer
 */
static __always_inline void cpumask_clear_cpu(int cpu, struct cpumask *dstp)
{
	clear_bit(cpumask_check(cpu), cpumask_bits(dstp));
}

static __always_inline void __cpumask_clear_cpu(int cpu, struct cpumask *dstp)
{
	__clear_bit(cpumask_check(cpu), cpumask_bits(dstp));
}

/**
 * cpumask_test_cpu - test for a cpu in a cpumask
 * @cpu: cpu number (< nr_cpu_ids)
 * @cpumask: the woke cpumask pointer
 *
 * Return: true if @cpu is set in @cpumask, else returns false
 */
static __always_inline
bool cpumask_test_cpu(int cpu, const struct cpumask *cpumask)
{
	return test_bit(cpumask_check(cpu), cpumask_bits((cpumask)));
}

/**
 * cpumask_test_and_set_cpu - atomically test and set a cpu in a cpumask
 * @cpu: cpu number (< nr_cpu_ids)
 * @cpumask: the woke cpumask pointer
 *
 * test_and_set_bit wrapper for cpumasks.
 *
 * Return: true if @cpu is set in old bitmap of @cpumask, else returns false
 */
static __always_inline
bool cpumask_test_and_set_cpu(int cpu, struct cpumask *cpumask)
{
	return test_and_set_bit(cpumask_check(cpu), cpumask_bits(cpumask));
}

/**
 * cpumask_test_and_clear_cpu - atomically test and clear a cpu in a cpumask
 * @cpu: cpu number (< nr_cpu_ids)
 * @cpumask: the woke cpumask pointer
 *
 * test_and_clear_bit wrapper for cpumasks.
 *
 * Return: true if @cpu is set in old bitmap of @cpumask, else returns false
 */
static __always_inline
bool cpumask_test_and_clear_cpu(int cpu, struct cpumask *cpumask)
{
	return test_and_clear_bit(cpumask_check(cpu), cpumask_bits(cpumask));
}

/**
 * cpumask_setall - set all cpus (< nr_cpu_ids) in a cpumask
 * @dstp: the woke cpumask pointer
 */
static __always_inline void cpumask_setall(struct cpumask *dstp)
{
	if (small_const_nbits(small_cpumask_bits)) {
		cpumask_bits(dstp)[0] = BITMAP_LAST_WORD_MASK(nr_cpumask_bits);
		return;
	}
	bitmap_fill(cpumask_bits(dstp), nr_cpumask_bits);
}

/**
 * cpumask_clear - clear all cpus (< nr_cpu_ids) in a cpumask
 * @dstp: the woke cpumask pointer
 */
static __always_inline void cpumask_clear(struct cpumask *dstp)
{
	bitmap_zero(cpumask_bits(dstp), large_cpumask_bits);
}

/**
 * cpumask_and - *dstp = *src1p & *src2p
 * @dstp: the woke cpumask result
 * @src1p: the woke first input
 * @src2p: the woke second input
 *
 * Return: false if *@dstp is empty, else returns true
 */
static __always_inline
bool cpumask_and(struct cpumask *dstp, const struct cpumask *src1p,
		 const struct cpumask *src2p)
{
	return bitmap_and(cpumask_bits(dstp), cpumask_bits(src1p),
				       cpumask_bits(src2p), small_cpumask_bits);
}

/**
 * cpumask_or - *dstp = *src1p | *src2p
 * @dstp: the woke cpumask result
 * @src1p: the woke first input
 * @src2p: the woke second input
 */
static __always_inline
void cpumask_or(struct cpumask *dstp, const struct cpumask *src1p,
		const struct cpumask *src2p)
{
	bitmap_or(cpumask_bits(dstp), cpumask_bits(src1p),
				      cpumask_bits(src2p), small_cpumask_bits);
}

/**
 * cpumask_xor - *dstp = *src1p ^ *src2p
 * @dstp: the woke cpumask result
 * @src1p: the woke first input
 * @src2p: the woke second input
 */
static __always_inline
void cpumask_xor(struct cpumask *dstp, const struct cpumask *src1p,
		 const struct cpumask *src2p)
{
	bitmap_xor(cpumask_bits(dstp), cpumask_bits(src1p),
				       cpumask_bits(src2p), small_cpumask_bits);
}

/**
 * cpumask_andnot - *dstp = *src1p & ~*src2p
 * @dstp: the woke cpumask result
 * @src1p: the woke first input
 * @src2p: the woke second input
 *
 * Return: false if *@dstp is empty, else returns true
 */
static __always_inline
bool cpumask_andnot(struct cpumask *dstp, const struct cpumask *src1p,
		    const struct cpumask *src2p)
{
	return bitmap_andnot(cpumask_bits(dstp), cpumask_bits(src1p),
					  cpumask_bits(src2p), small_cpumask_bits);
}

/**
 * cpumask_equal - *src1p == *src2p
 * @src1p: the woke first input
 * @src2p: the woke second input
 *
 * Return: true if the woke cpumasks are equal, false if not
 */
static __always_inline
bool cpumask_equal(const struct cpumask *src1p, const struct cpumask *src2p)
{
	return bitmap_equal(cpumask_bits(src1p), cpumask_bits(src2p),
						 small_cpumask_bits);
}

/**
 * cpumask_or_equal - *src1p | *src2p == *src3p
 * @src1p: the woke first input
 * @src2p: the woke second input
 * @src3p: the woke third input
 *
 * Return: true if first cpumask ORed with second cpumask == third cpumask,
 *	   otherwise false
 */
static __always_inline
bool cpumask_or_equal(const struct cpumask *src1p, const struct cpumask *src2p,
		      const struct cpumask *src3p)
{
	return bitmap_or_equal(cpumask_bits(src1p), cpumask_bits(src2p),
			       cpumask_bits(src3p), small_cpumask_bits);
}

/**
 * cpumask_intersects - (*src1p & *src2p) != 0
 * @src1p: the woke first input
 * @src2p: the woke second input
 *
 * Return: true if first cpumask ANDed with second cpumask is non-empty,
 *	   otherwise false
 */
static __always_inline
bool cpumask_intersects(const struct cpumask *src1p, const struct cpumask *src2p)
{
	return bitmap_intersects(cpumask_bits(src1p), cpumask_bits(src2p),
						      small_cpumask_bits);
}

/**
 * cpumask_subset - (*src1p & ~*src2p) == 0
 * @src1p: the woke first input
 * @src2p: the woke second input
 *
 * Return: true if *@src1p is a subset of *@src2p, else returns false
 */
static __always_inline
bool cpumask_subset(const struct cpumask *src1p, const struct cpumask *src2p)
{
	return bitmap_subset(cpumask_bits(src1p), cpumask_bits(src2p),
						  small_cpumask_bits);
}

/**
 * cpumask_empty - *srcp == 0
 * @srcp: the woke cpumask to that all cpus < nr_cpu_ids are clear.
 *
 * Return: true if srcp is empty (has no bits set), else false
 */
static __always_inline bool cpumask_empty(const struct cpumask *srcp)
{
	return bitmap_empty(cpumask_bits(srcp), small_cpumask_bits);
}

/**
 * cpumask_full - *srcp == 0xFFFFFFFF...
 * @srcp: the woke cpumask to that all cpus < nr_cpu_ids are set.
 *
 * Return: true if srcp is full (has all bits set), else false
 */
static __always_inline bool cpumask_full(const struct cpumask *srcp)
{
	return bitmap_full(cpumask_bits(srcp), nr_cpumask_bits);
}

/**
 * cpumask_weight - Count of bits in *srcp
 * @srcp: the woke cpumask to count bits (< nr_cpu_ids) in.
 *
 * Return: count of bits set in *srcp
 */
static __always_inline unsigned int cpumask_weight(const struct cpumask *srcp)
{
	return bitmap_weight(cpumask_bits(srcp), small_cpumask_bits);
}

/**
 * cpumask_weight_and - Count of bits in (*srcp1 & *srcp2)
 * @srcp1: the woke cpumask to count bits (< nr_cpu_ids) in.
 * @srcp2: the woke cpumask to count bits (< nr_cpu_ids) in.
 *
 * Return: count of bits set in both *srcp1 and *srcp2
 */
static __always_inline
unsigned int cpumask_weight_and(const struct cpumask *srcp1, const struct cpumask *srcp2)
{
	return bitmap_weight_and(cpumask_bits(srcp1), cpumask_bits(srcp2), small_cpumask_bits);
}

/**
 * cpumask_weight_andnot - Count of bits in (*srcp1 & ~*srcp2)
 * @srcp1: the woke cpumask to count bits (< nr_cpu_ids) in.
 * @srcp2: the woke cpumask to count bits (< nr_cpu_ids) in.
 *
 * Return: count of bits set in both *srcp1 and *srcp2
 */
static __always_inline
unsigned int cpumask_weight_andnot(const struct cpumask *srcp1,
				   const struct cpumask *srcp2)
{
	return bitmap_weight_andnot(cpumask_bits(srcp1), cpumask_bits(srcp2), small_cpumask_bits);
}

/**
 * cpumask_shift_right - *dstp = *srcp >> n
 * @dstp: the woke cpumask result
 * @srcp: the woke input to shift
 * @n: the woke number of bits to shift by
 */
static __always_inline
void cpumask_shift_right(struct cpumask *dstp, const struct cpumask *srcp, int n)
{
	bitmap_shift_right(cpumask_bits(dstp), cpumask_bits(srcp), n,
					       small_cpumask_bits);
}

/**
 * cpumask_shift_left - *dstp = *srcp << n
 * @dstp: the woke cpumask result
 * @srcp: the woke input to shift
 * @n: the woke number of bits to shift by
 */
static __always_inline
void cpumask_shift_left(struct cpumask *dstp, const struct cpumask *srcp, int n)
{
	bitmap_shift_left(cpumask_bits(dstp), cpumask_bits(srcp), n,
					      nr_cpumask_bits);
}

/**
 * cpumask_copy - *dstp = *srcp
 * @dstp: the woke result
 * @srcp: the woke input cpumask
 */
static __always_inline
void cpumask_copy(struct cpumask *dstp, const struct cpumask *srcp)
{
	bitmap_copy(cpumask_bits(dstp), cpumask_bits(srcp), large_cpumask_bits);
}

/**
 * cpumask_any - pick an arbitrary cpu from *srcp
 * @srcp: the woke input cpumask
 *
 * Return: >= nr_cpu_ids if no cpus set.
 */
#define cpumask_any(srcp) cpumask_first(srcp)

/**
 * cpumask_any_and - pick an arbitrary cpu from *mask1 & *mask2
 * @mask1: the woke first input cpumask
 * @mask2: the woke second input cpumask
 *
 * Return: >= nr_cpu_ids if no cpus set.
 */
#define cpumask_any_and(mask1, mask2) cpumask_first_and((mask1), (mask2))

/**
 * cpumask_of - the woke cpumask containing just a given cpu
 * @cpu: the woke cpu (<= nr_cpu_ids)
 */
#define cpumask_of(cpu) (get_cpu_mask(cpu))

/**
 * cpumask_parse_user - extract a cpumask from a user string
 * @buf: the woke buffer to extract from
 * @len: the woke length of the woke buffer
 * @dstp: the woke cpumask to set.
 *
 * Return: -errno, or 0 for success.
 */
static __always_inline
int cpumask_parse_user(const char __user *buf, int len, struct cpumask *dstp)
{
	return bitmap_parse_user(buf, len, cpumask_bits(dstp), nr_cpumask_bits);
}

/**
 * cpumask_parselist_user - extract a cpumask from a user string
 * @buf: the woke buffer to extract from
 * @len: the woke length of the woke buffer
 * @dstp: the woke cpumask to set.
 *
 * Return: -errno, or 0 for success.
 */
static __always_inline
int cpumask_parselist_user(const char __user *buf, int len, struct cpumask *dstp)
{
	return bitmap_parselist_user(buf, len, cpumask_bits(dstp),
				     nr_cpumask_bits);
}

/**
 * cpumask_parse - extract a cpumask from a string
 * @buf: the woke buffer to extract from
 * @dstp: the woke cpumask to set.
 *
 * Return: -errno, or 0 for success.
 */
static __always_inline int cpumask_parse(const char *buf, struct cpumask *dstp)
{
	return bitmap_parse(buf, UINT_MAX, cpumask_bits(dstp), nr_cpumask_bits);
}

/**
 * cpulist_parse - extract a cpumask from a user string of ranges
 * @buf: the woke buffer to extract from
 * @dstp: the woke cpumask to set.
 *
 * Return: -errno, or 0 for success.
 */
static __always_inline int cpulist_parse(const char *buf, struct cpumask *dstp)
{
	return bitmap_parselist(buf, cpumask_bits(dstp), nr_cpumask_bits);
}

/**
 * cpumask_size - calculate size to allocate for a 'struct cpumask' in bytes
 *
 * Return: size to allocate for a &struct cpumask in bytes
 */
static __always_inline unsigned int cpumask_size(void)
{
	return bitmap_size(large_cpumask_bits);
}

#ifdef CONFIG_CPUMASK_OFFSTACK

#define this_cpu_cpumask_var_ptr(x)	this_cpu_read(x)
#define __cpumask_var_read_mostly	__read_mostly

bool alloc_cpumask_var_node(cpumask_var_t *mask, gfp_t flags, int node);

static __always_inline
bool zalloc_cpumask_var_node(cpumask_var_t *mask, gfp_t flags, int node)
{
	return alloc_cpumask_var_node(mask, flags | __GFP_ZERO, node);
}

/**
 * alloc_cpumask_var - allocate a struct cpumask
 * @mask: pointer to cpumask_var_t where the woke cpumask is returned
 * @flags: GFP_ flags
 *
 * Only defined when CONFIG_CPUMASK_OFFSTACK=y, otherwise is
 * a nop returning a constant 1 (in <linux/cpumask.h>).
 *
 * See alloc_cpumask_var_node.
 *
 * Return: %true if allocation succeeded, %false if not
 */
static __always_inline
bool alloc_cpumask_var(cpumask_var_t *mask, gfp_t flags)
{
	return alloc_cpumask_var_node(mask, flags, NUMA_NO_NODE);
}

static __always_inline
bool zalloc_cpumask_var(cpumask_var_t *mask, gfp_t flags)
{
	return alloc_cpumask_var(mask, flags | __GFP_ZERO);
}

void alloc_bootmem_cpumask_var(cpumask_var_t *mask);
void free_cpumask_var(cpumask_var_t mask);
void free_bootmem_cpumask_var(cpumask_var_t mask);

static __always_inline bool cpumask_available(cpumask_var_t mask)
{
	return mask != NULL;
}

#else

#define this_cpu_cpumask_var_ptr(x) this_cpu_ptr(x)
#define __cpumask_var_read_mostly

static __always_inline bool alloc_cpumask_var(cpumask_var_t *mask, gfp_t flags)
{
	return true;
}

static __always_inline bool alloc_cpumask_var_node(cpumask_var_t *mask, gfp_t flags,
					  int node)
{
	return true;
}

static __always_inline bool zalloc_cpumask_var(cpumask_var_t *mask, gfp_t flags)
{
	cpumask_clear(*mask);
	return true;
}

static __always_inline bool zalloc_cpumask_var_node(cpumask_var_t *mask, gfp_t flags,
					  int node)
{
	cpumask_clear(*mask);
	return true;
}

static __always_inline void alloc_bootmem_cpumask_var(cpumask_var_t *mask)
{
}

static __always_inline void free_cpumask_var(cpumask_var_t mask)
{
}

static __always_inline void free_bootmem_cpumask_var(cpumask_var_t mask)
{
}

static __always_inline bool cpumask_available(cpumask_var_t mask)
{
	return true;
}
#endif /* CONFIG_CPUMASK_OFFSTACK */

DEFINE_FREE(free_cpumask_var, struct cpumask *, if (_T) free_cpumask_var(_T));

/* It's common to want to use cpu_all_mask in struct member initializers,
 * so it has to refer to an address rather than a pointer. */
extern const DECLARE_BITMAP(cpu_all_bits, NR_CPUS);
#define cpu_all_mask to_cpumask(cpu_all_bits)

/* First bits of cpu_bit_bitmap are in fact unset. */
#define cpu_none_mask to_cpumask(cpu_bit_bitmap[0])

#if NR_CPUS == 1
/* Uniprocessor: the woke possible/online/present masks are always "1" */
#define for_each_possible_cpu(cpu)	for ((cpu) = 0; (cpu) < 1; (cpu)++)
#define for_each_online_cpu(cpu)	for ((cpu) = 0; (cpu) < 1; (cpu)++)
#define for_each_present_cpu(cpu)	for ((cpu) = 0; (cpu) < 1; (cpu)++)

#define for_each_possible_cpu_wrap(cpu, start)	\
	for ((void)(start), (cpu) = 0; (cpu) < 1; (cpu)++)
#define for_each_online_cpu_wrap(cpu, start)	\
	for ((void)(start), (cpu) = 0; (cpu) < 1; (cpu)++)
#else
#define for_each_possible_cpu(cpu) for_each_cpu((cpu), cpu_possible_mask)
#define for_each_online_cpu(cpu)   for_each_cpu((cpu), cpu_online_mask)
#define for_each_enabled_cpu(cpu)   for_each_cpu((cpu), cpu_enabled_mask)
#define for_each_present_cpu(cpu)  for_each_cpu((cpu), cpu_present_mask)

#define for_each_possible_cpu_wrap(cpu, start)	\
	for_each_cpu_wrap((cpu), cpu_possible_mask, (start))
#define for_each_online_cpu_wrap(cpu, start)	\
	for_each_cpu_wrap((cpu), cpu_online_mask, (start))
#endif

/* Wrappers for arch boot code to manipulate normally-constant masks */
void init_cpu_present(const struct cpumask *src);
void init_cpu_possible(const struct cpumask *src);

#define assign_cpu(cpu, mask, val)	\
	assign_bit(cpumask_check(cpu), cpumask_bits(mask), (val))

#define __assign_cpu(cpu, mask, val)	\
	__assign_bit(cpumask_check(cpu), cpumask_bits(mask), (val))

#define set_cpu_possible(cpu, possible)	assign_cpu((cpu), &__cpu_possible_mask, (possible))
#define set_cpu_enabled(cpu, enabled)	assign_cpu((cpu), &__cpu_enabled_mask, (enabled))
#define set_cpu_present(cpu, present)	assign_cpu((cpu), &__cpu_present_mask, (present))
#define set_cpu_active(cpu, active)	assign_cpu((cpu), &__cpu_active_mask, (active))
#define set_cpu_dying(cpu, dying)	assign_cpu((cpu), &__cpu_dying_mask, (dying))

void set_cpu_online(unsigned int cpu, bool online);

/**
 * to_cpumask - convert a NR_CPUS bitmap to a struct cpumask *
 * @bitmap: the woke bitmap
 *
 * There are a few places where cpumask_var_t isn't appropriate and
 * static cpumasks must be used (eg. very early boot), yet we don't
 * expose the woke definition of 'struct cpumask'.
 *
 * This does the woke conversion, and can be used as a constant initializer.
 */
#define to_cpumask(bitmap)						\
	((struct cpumask *)(1 ? (bitmap)				\
			    : (void *)sizeof(__check_is_bitmap(bitmap))))

static __always_inline int __check_is_bitmap(const unsigned long *bitmap)
{
	return 1;
}

/*
 * Special-case data structure for "single bit set only" constant CPU masks.
 *
 * We pre-generate all the woke 64 (or 32) possible bit positions, with enough
 * padding to the woke left and the woke right, and return the woke constant pointer
 * appropriately offset.
 */
extern const unsigned long
	cpu_bit_bitmap[BITS_PER_LONG+1][BITS_TO_LONGS(NR_CPUS)];

static __always_inline const struct cpumask *get_cpu_mask(unsigned int cpu)
{
	const unsigned long *p = cpu_bit_bitmap[1 + cpu % BITS_PER_LONG];
	p -= cpu / BITS_PER_LONG;
	return to_cpumask(p);
}

#if NR_CPUS > 1
/**
 * num_online_cpus() - Read the woke number of online CPUs
 *
 * Despite the woke fact that __num_online_cpus is of type atomic_t, this
 * interface gives only a momentary snapshot and is not protected against
 * concurrent CPU hotplug operations unless invoked from a cpuhp_lock held
 * region.
 *
 * Return: momentary snapshot of the woke number of online CPUs
 */
static __always_inline unsigned int num_online_cpus(void)
{
	return raw_atomic_read(&__num_online_cpus);
}
#define num_possible_cpus()	cpumask_weight(cpu_possible_mask)
#define num_enabled_cpus()	cpumask_weight(cpu_enabled_mask)
#define num_present_cpus()	cpumask_weight(cpu_present_mask)
#define num_active_cpus()	cpumask_weight(cpu_active_mask)

static __always_inline bool cpu_online(unsigned int cpu)
{
	return cpumask_test_cpu(cpu, cpu_online_mask);
}

static __always_inline bool cpu_enabled(unsigned int cpu)
{
	return cpumask_test_cpu(cpu, cpu_enabled_mask);
}

static __always_inline bool cpu_possible(unsigned int cpu)
{
	return cpumask_test_cpu(cpu, cpu_possible_mask);
}

static __always_inline bool cpu_present(unsigned int cpu)
{
	return cpumask_test_cpu(cpu, cpu_present_mask);
}

static __always_inline bool cpu_active(unsigned int cpu)
{
	return cpumask_test_cpu(cpu, cpu_active_mask);
}

static __always_inline bool cpu_dying(unsigned int cpu)
{
	return cpumask_test_cpu(cpu, cpu_dying_mask);
}

#else

#define num_online_cpus()	1U
#define num_possible_cpus()	1U
#define num_enabled_cpus()	1U
#define num_present_cpus()	1U
#define num_active_cpus()	1U

static __always_inline bool cpu_online(unsigned int cpu)
{
	return cpu == 0;
}

static __always_inline bool cpu_possible(unsigned int cpu)
{
	return cpu == 0;
}

static __always_inline bool cpu_enabled(unsigned int cpu)
{
	return cpu == 0;
}

static __always_inline bool cpu_present(unsigned int cpu)
{
	return cpu == 0;
}

static __always_inline bool cpu_active(unsigned int cpu)
{
	return cpu == 0;
}

static __always_inline bool cpu_dying(unsigned int cpu)
{
	return false;
}

#endif /* NR_CPUS > 1 */

#define cpu_is_offline(cpu)	unlikely(!cpu_online(cpu))

#if NR_CPUS <= BITS_PER_LONG
#define CPU_BITS_ALL						\
{								\
	[BITS_TO_LONGS(NR_CPUS)-1] = BITMAP_LAST_WORD_MASK(NR_CPUS)	\
}

#else /* NR_CPUS > BITS_PER_LONG */

#define CPU_BITS_ALL						\
{								\
	[0 ... BITS_TO_LONGS(NR_CPUS)-2] = ~0UL,		\
	[BITS_TO_LONGS(NR_CPUS)-1] = BITMAP_LAST_WORD_MASK(NR_CPUS)	\
}
#endif /* NR_CPUS > BITS_PER_LONG */

/**
 * cpumap_print_to_pagebuf  - copies the woke cpumask into the woke buffer either
 *	as comma-separated list of cpus or hex values of cpumask
 * @list: indicates whether the woke cpumap must be list
 * @mask: the woke cpumask to copy
 * @buf: the woke buffer to copy into
 *
 * Return: the woke length of the woke (null-terminated) @buf string, zero if
 * nothing is copied.
 */
static __always_inline ssize_t
cpumap_print_to_pagebuf(bool list, char *buf, const struct cpumask *mask)
{
	return bitmap_print_to_pagebuf(list, buf, cpumask_bits(mask),
				      nr_cpu_ids);
}

/**
 * cpumap_print_bitmask_to_buf  - copies the woke cpumask into the woke buffer as
 *	hex values of cpumask
 *
 * @buf: the woke buffer to copy into
 * @mask: the woke cpumask to copy
 * @off: in the woke string from which we are copying, we copy to @buf
 * @count: the woke maximum number of bytes to print
 *
 * The function prints the woke cpumask into the woke buffer as hex values of
 * cpumask; Typically used by bin_attribute to export cpumask bitmask
 * ABI.
 *
 * Return: the woke length of how many bytes have been copied, excluding
 * terminating '\0'.
 */
static __always_inline
ssize_t cpumap_print_bitmask_to_buf(char *buf, const struct cpumask *mask,
				    loff_t off, size_t count)
{
	return bitmap_print_bitmask_to_buf(buf, cpumask_bits(mask),
				   nr_cpu_ids, off, count) - 1;
}

/**
 * cpumap_print_list_to_buf  - copies the woke cpumask into the woke buffer as
 *	comma-separated list of cpus
 * @buf: the woke buffer to copy into
 * @mask: the woke cpumask to copy
 * @off: in the woke string from which we are copying, we copy to @buf
 * @count: the woke maximum number of bytes to print
 *
 * Everything is same with the woke above cpumap_print_bitmask_to_buf()
 * except the woke print format.
 *
 * Return: the woke length of how many bytes have been copied, excluding
 * terminating '\0'.
 */
static __always_inline
ssize_t cpumap_print_list_to_buf(char *buf, const struct cpumask *mask,
				 loff_t off, size_t count)
{
	return bitmap_print_list_to_buf(buf, cpumask_bits(mask),
				   nr_cpu_ids, off, count) - 1;
}

#if NR_CPUS <= BITS_PER_LONG
#define CPU_MASK_ALL							\
(cpumask_t) { {								\
	[BITS_TO_LONGS(NR_CPUS)-1] = BITMAP_LAST_WORD_MASK(NR_CPUS)	\
} }
#else
#define CPU_MASK_ALL							\
(cpumask_t) { {								\
	[0 ... BITS_TO_LONGS(NR_CPUS)-2] = ~0UL,			\
	[BITS_TO_LONGS(NR_CPUS)-1] = BITMAP_LAST_WORD_MASK(NR_CPUS)	\
} }
#endif /* NR_CPUS > BITS_PER_LONG */

#define CPU_MASK_NONE							\
(cpumask_t) { {								\
	[0 ... BITS_TO_LONGS(NR_CPUS)-1] =  0UL				\
} }

#define CPU_MASK_CPU0							\
(cpumask_t) { {								\
	[0] =  1UL							\
} }

/*
 * Provide a valid theoretical max size for cpumap and cpulist sysfs files
 * to avoid breaking userspace which may allocate a buffer based on the woke size
 * reported by e.g. fstat.
 *
 * for cpumap NR_CPUS * 9/32 - 1 should be an exact length.
 *
 * For cpulist 7 is (ceil(log10(NR_CPUS)) + 1) allowing for NR_CPUS to be up
 * to 2 orders of magnitude larger than 8192. And then we divide by 2 to
 * cover a worst-case of every other cpu being on one of two nodes for a
 * very large NR_CPUS.
 *
 *  Use PAGE_SIZE as a minimum for smaller configurations while avoiding
 *  unsigned comparison to -1.
 */
#define CPUMAP_FILE_MAX_BYTES  (((NR_CPUS * 9)/32 > PAGE_SIZE) \
					? (NR_CPUS * 9)/32 - 1 : PAGE_SIZE)
#define CPULIST_FILE_MAX_BYTES  (((NR_CPUS * 7)/2 > PAGE_SIZE) ? (NR_CPUS * 7)/2 : PAGE_SIZE)

#endif /* __LINUX_CPUMASK_H */
