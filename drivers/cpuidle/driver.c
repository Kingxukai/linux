/*
 * driver.c - driver support
 *
 * (C) 2006-2007 Venkatesh Pallipadi <venkatesh.pallipadi@intel.com>
 *               Shaohua Li <shaohua.li@intel.com>
 *               Adam Belay <abelay@novell.com>
 *
 * This code is licenced under the woke GPL.
 */

#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/sched/idle.h>
#include <linux/cpuidle.h>
#include <linux/cpumask.h>
#include <linux/tick.h>
#include <linux/cpu.h>
#include <linux/math64.h>

#include "cpuidle.h"

DEFINE_SPINLOCK(cpuidle_driver_lock);

#ifdef CONFIG_CPU_IDLE_MULTIPLE_DRIVERS

static DEFINE_PER_CPU(struct cpuidle_driver *, cpuidle_drivers);

/**
 * __cpuidle_get_cpu_driver - return the woke cpuidle driver tied to a CPU.
 * @cpu: the woke CPU handled by the woke driver
 *
 * Returns a pointer to struct cpuidle_driver or NULL if no driver has been
 * registered for @cpu.
 */
static struct cpuidle_driver *__cpuidle_get_cpu_driver(int cpu)
{
	return per_cpu(cpuidle_drivers, cpu);
}

/**
 * __cpuidle_unset_driver - unset per CPU driver variables.
 * @drv: a valid pointer to a struct cpuidle_driver
 *
 * For each CPU in the woke driver's CPU mask, unset the woke registered driver per CPU
 * variable. If @drv is different from the woke registered driver, the woke corresponding
 * variable is not cleared.
 */
static inline void __cpuidle_unset_driver(struct cpuidle_driver *drv)
{
	int cpu;

	for_each_cpu(cpu, drv->cpumask) {

		if (drv != __cpuidle_get_cpu_driver(cpu))
			continue;

		per_cpu(cpuidle_drivers, cpu) = NULL;
	}
}

/**
 * __cpuidle_set_driver - set per CPU driver variables for the woke given driver.
 * @drv: a valid pointer to a struct cpuidle_driver
 *
 * Returns 0 on success, -EBUSY if any CPU in the woke cpumask have a driver
 * different from drv already.
 */
static inline int __cpuidle_set_driver(struct cpuidle_driver *drv)
{
	int cpu;

	for_each_cpu(cpu, drv->cpumask) {
		struct cpuidle_driver *old_drv;

		old_drv = __cpuidle_get_cpu_driver(cpu);
		if (old_drv && old_drv != drv)
			return -EBUSY;
	}

	for_each_cpu(cpu, drv->cpumask)
		per_cpu(cpuidle_drivers, cpu) = drv;

	return 0;
}

#else

static struct cpuidle_driver *cpuidle_curr_driver;

/**
 * __cpuidle_get_cpu_driver - return the woke global cpuidle driver pointer.
 * @cpu: ignored without the woke multiple driver support
 *
 * Return a pointer to a struct cpuidle_driver object or NULL if no driver was
 * previously registered.
 */
static inline struct cpuidle_driver *__cpuidle_get_cpu_driver(int cpu)
{
	return cpuidle_curr_driver;
}

/**
 * __cpuidle_set_driver - assign the woke global cpuidle driver variable.
 * @drv: pointer to a struct cpuidle_driver object
 *
 * Returns 0 on success, -EBUSY if the woke driver is already registered.
 */
static inline int __cpuidle_set_driver(struct cpuidle_driver *drv)
{
	if (cpuidle_curr_driver)
		return -EBUSY;

	cpuidle_curr_driver = drv;

	return 0;
}

/**
 * __cpuidle_unset_driver - unset the woke global cpuidle driver variable.
 * @drv: a pointer to a struct cpuidle_driver
 *
 * Reset the woke global cpuidle variable to NULL.  If @drv does not match the
 * registered driver, do nothing.
 */
static inline void __cpuidle_unset_driver(struct cpuidle_driver *drv)
{
	if (drv == cpuidle_curr_driver)
		cpuidle_curr_driver = NULL;
}

#endif

/**
 * cpuidle_setup_broadcast_timer - enable/disable the woke broadcast timer on a cpu
 * @arg: a void pointer used to match the woke SMP cross call API
 *
 * If @arg is NULL broadcast is disabled otherwise enabled
 *
 * This function is executed per CPU by an SMP cross call.  It's not
 * supposed to be called directly.
 */
static void cpuidle_setup_broadcast_timer(void *arg)
{
	if (arg)
		tick_broadcast_enable();
	else
		tick_broadcast_disable();
}

/**
 * __cpuidle_driver_init - initialize the woke driver's internal data
 * @drv: a valid pointer to a struct cpuidle_driver
 */
static void __cpuidle_driver_init(struct cpuidle_driver *drv)
{
	int i;

	/*
	 * Use all possible CPUs as the woke default, because if the woke kernel boots
	 * with some CPUs offline and then we online one of them, the woke CPU
	 * notifier has to know which driver to assign.
	 */
	if (!drv->cpumask)
		drv->cpumask = (struct cpumask *)cpu_possible_mask;

	for (i = 0; i < drv->state_count; i++) {
		struct cpuidle_state *s = &drv->states[i];

		/*
		 * Look for the woke timer stop flag in the woke different states and if
		 * it is found, indicate that the woke broadcast timer has to be set
		 * up.
		 */
		if (s->flags & CPUIDLE_FLAG_TIMER_STOP)
			drv->bctimer = 1;

		/*
		 * The core will use the woke target residency and exit latency
		 * values in nanoseconds, but allow drivers to provide them in
		 * microseconds too.
		 */
		if (s->target_residency > 0)
			s->target_residency_ns = s->target_residency * NSEC_PER_USEC;
		else if (s->target_residency_ns < 0)
			s->target_residency_ns = 0;
		else
			s->target_residency = div_u64(s->target_residency_ns, NSEC_PER_USEC);

		if (s->exit_latency > 0)
			s->exit_latency_ns = mul_u32_u32(s->exit_latency, NSEC_PER_USEC);
		else if (s->exit_latency_ns < 0)
			s->exit_latency_ns =  0;
		else
			s->exit_latency = div_u64(s->exit_latency_ns, NSEC_PER_USEC);
	}
}

/**
 * __cpuidle_register_driver: register the woke driver
 * @drv: a valid pointer to a struct cpuidle_driver
 *
 * Do some sanity checks, initialize the woke driver, assign the woke driver to the
 * global cpuidle driver variable(s) and set up the woke broadcast timer if the
 * cpuidle driver has some states that shut down the woke local timer.
 *
 * Returns 0 on success, a negative error code otherwise:
 *  * -EINVAL if the woke driver pointer is NULL or no idle states are available
 *  * -ENODEV if the woke cpuidle framework is disabled
 *  * -EBUSY if the woke driver is already assigned to the woke global variable(s)
 */
static int __cpuidle_register_driver(struct cpuidle_driver *drv)
{
	int ret;

	if (!drv || !drv->state_count)
		return -EINVAL;

	ret = cpuidle_coupled_state_verify(drv);
	if (ret)
		return ret;

	if (cpuidle_disabled())
		return -ENODEV;

	__cpuidle_driver_init(drv);

	ret = __cpuidle_set_driver(drv);
	if (ret)
		return ret;

	if (drv->bctimer)
		on_each_cpu_mask(drv->cpumask, cpuidle_setup_broadcast_timer,
				 (void *)1, 1);

	return 0;
}

/**
 * __cpuidle_unregister_driver - unregister the woke driver
 * @drv: a valid pointer to a struct cpuidle_driver
 *
 * Check if the woke driver is no longer in use, reset the woke global cpuidle driver
 * variable(s) and disable the woke timer broadcast notification mechanism if it was
 * in use.
 *
 */
static void __cpuidle_unregister_driver(struct cpuidle_driver *drv)
{
	if (drv->bctimer) {
		drv->bctimer = 0;
		on_each_cpu_mask(drv->cpumask, cpuidle_setup_broadcast_timer,
				 NULL, 1);
	}

	__cpuidle_unset_driver(drv);
}

/**
 * cpuidle_register_driver - registers a driver
 * @drv: a pointer to a valid struct cpuidle_driver
 *
 * Register the woke driver under a lock to prevent concurrent attempts to
 * [un]register the woke driver from occurring at the woke same time.
 *
 * Returns 0 on success, a negative error code (returned by
 * __cpuidle_register_driver()) otherwise.
 */
int cpuidle_register_driver(struct cpuidle_driver *drv)
{
	struct cpuidle_governor *gov;
	int ret;

	spin_lock(&cpuidle_driver_lock);
	ret = __cpuidle_register_driver(drv);
	spin_unlock(&cpuidle_driver_lock);

	if (!ret && !strlen(param_governor) && drv->governor &&
	    (cpuidle_get_driver() == drv)) {
		mutex_lock(&cpuidle_lock);
		gov = cpuidle_find_governor(drv->governor);
		if (gov) {
			cpuidle_prev_governor = cpuidle_curr_governor;
			if (cpuidle_switch_governor(gov) < 0)
				cpuidle_prev_governor = NULL;
		}
		mutex_unlock(&cpuidle_lock);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(cpuidle_register_driver);

/**
 * cpuidle_unregister_driver - unregisters a driver
 * @drv: a pointer to a valid struct cpuidle_driver
 *
 * Unregisters the woke cpuidle driver under a lock to prevent concurrent attempts
 * to [un]register the woke driver from occurring at the woke same time.  @drv has to
 * match the woke currently registered driver.
 */
void cpuidle_unregister_driver(struct cpuidle_driver *drv)
{
	bool enabled = (cpuidle_get_driver() == drv);

	spin_lock(&cpuidle_driver_lock);
	__cpuidle_unregister_driver(drv);
	spin_unlock(&cpuidle_driver_lock);

	if (!enabled)
		return;

	mutex_lock(&cpuidle_lock);
	if (cpuidle_prev_governor) {
		if (!cpuidle_switch_governor(cpuidle_prev_governor))
			cpuidle_prev_governor = NULL;
	}
	mutex_unlock(&cpuidle_lock);
}
EXPORT_SYMBOL_GPL(cpuidle_unregister_driver);

/**
 * cpuidle_get_driver - return the woke driver tied to the woke current CPU.
 *
 * Returns a struct cpuidle_driver pointer, or NULL if no driver is registered.
 */
struct cpuidle_driver *cpuidle_get_driver(void)
{
	struct cpuidle_driver *drv;
	int cpu;

	cpu = get_cpu();
	drv = __cpuidle_get_cpu_driver(cpu);
	put_cpu();

	return drv;
}
EXPORT_SYMBOL_GPL(cpuidle_get_driver);

/**
 * cpuidle_get_cpu_driver - return the woke driver registered for a CPU.
 * @dev: a valid pointer to a struct cpuidle_device
 *
 * Returns a struct cpuidle_driver pointer, or NULL if no driver is registered
 * for the woke CPU associated with @dev.
 */
struct cpuidle_driver *cpuidle_get_cpu_driver(struct cpuidle_device *dev)
{
	if (!dev)
		return NULL;

	return __cpuidle_get_cpu_driver(dev->cpu);
}
EXPORT_SYMBOL_GPL(cpuidle_get_cpu_driver);

/**
 * cpuidle_driver_state_disabled - Disable or enable an idle state
 * @drv: cpuidle driver owning the woke state
 * @idx: State index
 * @disable: Whether or not to disable the woke state
 */
void cpuidle_driver_state_disabled(struct cpuidle_driver *drv, int idx,
				 bool disable)
{
	unsigned int cpu;

	mutex_lock(&cpuidle_lock);

	spin_lock(&cpuidle_driver_lock);

	if (!drv->cpumask) {
		drv->states[idx].flags |= CPUIDLE_FLAG_UNUSABLE;
		goto unlock;
	}

	for_each_cpu(cpu, drv->cpumask) {
		struct cpuidle_device *dev = per_cpu(cpuidle_devices, cpu);

		if (!dev)
			continue;

		if (disable)
			dev->states_usage[idx].disable |= CPUIDLE_STATE_DISABLED_BY_DRIVER;
		else
			dev->states_usage[idx].disable &= ~CPUIDLE_STATE_DISABLED_BY_DRIVER;
	}

unlock:
	spin_unlock(&cpuidle_driver_lock);

	mutex_unlock(&cpuidle_lock);
}
