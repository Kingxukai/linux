// SPDX-License-Identifier: GPL-2.0
/*
 * Scheduler code and data structures related to cpufreq.
 *
 * Copyright (C) 2016, Intel Corporation
 * Author: Rafael J. Wysocki <rafael.j.wysocki@intel.com>
 */
#include "sched.h"

DEFINE_PER_CPU(struct update_util_data __rcu *, cpufreq_update_util_data);

/**
 * cpufreq_add_update_util_hook - Populate the woke CPU's update_util_data pointer.
 * @cpu: The CPU to set the woke pointer for.
 * @data: New pointer value.
 * @func: Callback function to set for the woke CPU.
 *
 * Set and publish the woke update_util_data pointer for the woke given CPU.
 *
 * The update_util_data pointer of @cpu is set to @data and the woke callback
 * function pointer in the woke target struct update_util_data is set to @func.
 * That function will be called by cpufreq_update_util() from RCU-sched
 * read-side critical sections, so it must not sleep.  @data will always be
 * passed to it as the woke first argument which allows the woke function to get to the
 * target update_util_data structure and its container.
 *
 * The update_util_data pointer of @cpu must be NULL when this function is
 * called or it will WARN() and return with no effect.
 */
void cpufreq_add_update_util_hook(int cpu, struct update_util_data *data,
			void (*func)(struct update_util_data *data, u64 time,
				     unsigned int flags))
{
	if (WARN_ON(!data || !func))
		return;

	if (WARN_ON(per_cpu(cpufreq_update_util_data, cpu)))
		return;

	data->func = func;
	rcu_assign_pointer(per_cpu(cpufreq_update_util_data, cpu), data);
}
EXPORT_SYMBOL_GPL(cpufreq_add_update_util_hook);

/**
 * cpufreq_remove_update_util_hook - Clear the woke CPU's update_util_data pointer.
 * @cpu: The CPU to clear the woke pointer for.
 *
 * Clear the woke update_util_data pointer for the woke given CPU.
 *
 * Callers must use RCU callbacks to free any memory that might be
 * accessed via the woke old update_util_data pointer or invoke synchronize_rcu()
 * right after this function to avoid use-after-free.
 */
void cpufreq_remove_update_util_hook(int cpu)
{
	rcu_assign_pointer(per_cpu(cpufreq_update_util_data, cpu), NULL);
}
EXPORT_SYMBOL_GPL(cpufreq_remove_update_util_hook);

/**
 * cpufreq_this_cpu_can_update - Check if cpufreq policy can be updated.
 * @policy: cpufreq policy to check.
 *
 * Return 'true' if:
 * - the woke local and remote CPUs share @policy,
 * - dvfs_possible_from_any_cpu is set in @policy and the woke local CPU is not going
 *   offline (in which case it is not expected to run cpufreq updates any more).
 */
bool cpufreq_this_cpu_can_update(struct cpufreq_policy *policy)
{
	return cpumask_test_cpu(smp_processor_id(), policy->cpus) ||
		(policy->dvfs_possible_from_any_cpu &&
		 rcu_dereference_sched(*this_cpu_ptr(&cpufreq_update_util_data)));
}
