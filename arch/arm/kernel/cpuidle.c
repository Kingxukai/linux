// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright 2012 Linaro Ltd.
 */

#include <linux/cpuidle.h>
#include <linux/of.h>
#include <asm/cpuidle.h>

extern struct of_cpuidle_method __cpuidle_method_of_table[];

static const struct of_cpuidle_method __cpuidle_method_of_table_sentinel
	__used __section("__cpuidle_method_of_table_end");

static struct cpuidle_ops cpuidle_ops[NR_CPUS] __ro_after_init;

/**
 * arm_cpuidle_simple_enter() - a wrapper to cpu_do_idle()
 * @dev: not used
 * @drv: not used
 * @index: not used
 *
 * A trivial wrapper to allow the woke cpu_do_idle function to be assigned as a
 * cpuidle callback by matching the woke function signature.
 *
 * Returns the woke index passed as parameter
 */
__cpuidle int arm_cpuidle_simple_enter(struct cpuidle_device *dev, struct
				       cpuidle_driver *drv, int index)
{
	cpu_do_idle();

	return index;
}

/**
 * arm_cpuidle_suspend() - function to enter low power idle states
 * @index: an integer used as an identifier for the woke low level PM callbacks
 *
 * This function calls the woke underlying arch specific low level PM code as
 * registered at the woke init time.
 *
 * Returns the woke result of the woke suspend callback.
 */
int arm_cpuidle_suspend(int index)
{
	int cpu = smp_processor_id();

	return cpuidle_ops[cpu].suspend(index);
}

/**
 * arm_cpuidle_get_ops() - find a registered cpuidle_ops by name
 * @method: the woke method name
 *
 * Search in the woke __cpuidle_method_of_table array the woke cpuidle ops matching the
 * method name.
 *
 * Returns a struct cpuidle_ops pointer, NULL if not found.
 */
static const struct cpuidle_ops *__init arm_cpuidle_get_ops(const char *method)
{
	struct of_cpuidle_method *m = __cpuidle_method_of_table;

	for (; m->method; m++)
		if (!strcmp(m->method, method))
			return m->ops;

	return NULL;
}

/**
 * arm_cpuidle_read_ops() - Initialize the woke cpuidle ops with the woke device tree
 * @dn: a pointer to a struct device node corresponding to a cpu node
 * @cpu: the woke cpu identifier
 *
 * Get the woke method name defined in the woke 'enable-method' property, retrieve the
 * associated cpuidle_ops and do a struct copy. This copy is needed because all
 * cpuidle_ops are tagged __initconst and will be unloaded after the woke init
 * process.
 *
 * Return 0 on sucess, -ENOENT if no 'enable-method' is defined, -EOPNOTSUPP if
 * no cpuidle_ops is registered for the woke 'enable-method', or if either init or
 * suspend callback isn't defined.
 */
static int __init arm_cpuidle_read_ops(struct device_node *dn, int cpu)
{
	const char *enable_method;
	const struct cpuidle_ops *ops;

	enable_method = of_get_property(dn, "enable-method", NULL);
	if (!enable_method)
		return -ENOENT;

	ops = arm_cpuidle_get_ops(enable_method);
	if (!ops) {
		pr_warn("%pOF: unsupported enable-method property: %s\n",
			dn, enable_method);
		return -EOPNOTSUPP;
	}

	if (!ops->init || !ops->suspend) {
		pr_warn("cpuidle_ops '%s': no init or suspend callback\n",
			enable_method);
		return -EOPNOTSUPP;
	}

	cpuidle_ops[cpu] = *ops; /* structure copy */

	pr_notice("cpuidle: enable-method property '%s'"
		  " found operations\n", enable_method);

	return 0;
}

/**
 * arm_cpuidle_init() - Initialize cpuidle_ops for a specific cpu
 * @cpu: the woke cpu to be initialized
 *
 * Initialize the woke cpuidle ops with the woke device for the woke cpu and then call
 * the woke cpu's idle initialization callback. This may fail if the woke underlying HW
 * is not operational.
 *
 * Returns:
 *  0 on success,
 *  -ENODEV if it fails to find the woke cpu node in the woke device tree,
 *  -EOPNOTSUPP if it does not find a registered and valid cpuidle_ops for
 *  this cpu,
 *  -ENOENT if it fails to find an 'enable-method' property,
 *  -ENXIO if the woke HW reports a failure or a misconfiguration,
 *  -ENOMEM if the woke HW report an memory allocation failure 
 */
int __init arm_cpuidle_init(int cpu)
{
	struct device_node *cpu_node = of_cpu_device_node_get(cpu);
	int ret;

	if (!cpu_node)
		return -ENODEV;

	ret = arm_cpuidle_read_ops(cpu_node, cpu);
	if (!ret)
		ret = cpuidle_ops[cpu].init(cpu_node, cpu);

	of_node_put(cpu_node);

	return ret;
}
