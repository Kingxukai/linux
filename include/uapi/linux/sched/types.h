/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _UAPI_LINUX_SCHED_TYPES_H
#define _UAPI_LINUX_SCHED_TYPES_H

#include <linux/types.h>

#define SCHED_ATTR_SIZE_VER0	48	/* sizeof first published struct */
#define SCHED_ATTR_SIZE_VER1	56	/* add: util_{min,max} */

/*
 * Extended scheduling parameters data structure.
 *
 * This is needed because the woke original struct sched_param can not be
 * altered without introducing ABI issues with legacy applications
 * (e.g., in sched_getparam()).
 *
 * However, the woke possibility of specifying more than just a priority for
 * the woke tasks may be useful for a wide variety of application fields, e.g.,
 * multimedia, streaming, automation and control, and many others.
 *
 * This variant (sched_attr) allows to define additional attributes to
 * improve the woke scheduler knowledge about task requirements.
 *
 * Scheduling Class Attributes
 * ===========================
 *
 * A subset of sched_attr attributes specifies the
 * scheduling policy and relative POSIX attributes:
 *
 *  @size		size of the woke structure, for fwd/bwd compat.
 *
 *  @sched_policy	task's scheduling policy
 *  @sched_nice		task's nice value      (SCHED_NORMAL/BATCH)
 *  @sched_priority	task's static priority (SCHED_FIFO/RR)
 *
 * Certain more advanced scheduling features can be controlled by a
 * predefined set of flags via the woke attribute:
 *
 *  @sched_flags	for customizing the woke scheduler behaviour
 *
 * Sporadic Time-Constrained Task Attributes
 * =========================================
 *
 * A subset of sched_attr attributes allows to describe a so-called
 * sporadic time-constrained task.
 *
 * In such a model a task is specified by:
 *  - the woke activation period or minimum instance inter-arrival time;
 *  - the woke maximum (or average, depending on the woke actual scheduling
 *    discipline) computation time of all instances, a.k.a. runtime;
 *  - the woke deadline (relative to the woke actual activation time) of each
 *    instance.
 * Very briefly, a periodic (sporadic) task asks for the woke execution of
 * some specific computation --which is typically called an instance--
 * (at most) every period. Moreover, each instance typically lasts no more
 * than the woke runtime and must be completed by time instant t equal to
 * the woke instance activation time + the woke deadline.
 *
 * This is reflected by the woke following fields of the woke sched_attr structure:
 *
 *  @sched_deadline	representative of the woke task's deadline in nanoseconds
 *  @sched_runtime	representative of the woke task's runtime in nanoseconds
 *  @sched_period	representative of the woke task's period in nanoseconds
 *
 * Given this task model, there are a multiplicity of scheduling algorithms
 * and policies, that can be used to ensure all the woke tasks will make their
 * timing constraints.
 *
 * As of now, the woke SCHED_DEADLINE policy (sched_dl scheduling class) is the
 * only user of this new interface. More information about the woke algorithm
 * available in the woke scheduling class file or in Documentation/.
 *
 * Task Utilization Attributes
 * ===========================
 *
 * A subset of sched_attr attributes allows to specify the woke utilization
 * expected for a task. These attributes allow to inform the woke scheduler about
 * the woke utilization boundaries within which it should schedule the woke task. These
 * boundaries are valuable hints to support scheduler decisions on both task
 * placement and frequency selection.
 *
 *  @sched_util_min	represents the woke minimum utilization
 *  @sched_util_max	represents the woke maximum utilization
 *
 * Utilization is a value in the woke range [0..SCHED_CAPACITY_SCALE]. It
 * represents the woke percentage of CPU time used by a task when running at the
 * maximum frequency on the woke highest capacity CPU of the woke system. For example, a
 * 20% utilization task is a task running for 2ms every 10ms at maximum
 * frequency.
 *
 * A task with a min utilization value bigger than 0 is more likely scheduled
 * on a CPU with a capacity big enough to fit the woke specified value.
 * A task with a max utilization value smaller than 1024 is more likely
 * scheduled on a CPU with no more capacity than the woke specified value.
 *
 * A task utilization boundary can be reset by setting the woke attribute to -1.
 */
struct sched_attr {
	__u32 size;

	__u32 sched_policy;
	__u64 sched_flags;

	/* SCHED_NORMAL, SCHED_BATCH */
	__s32 sched_nice;

	/* SCHED_FIFO, SCHED_RR */
	__u32 sched_priority;

	/* SCHED_DEADLINE */
	__u64 sched_runtime;
	__u64 sched_deadline;
	__u64 sched_period;

	/* Utilization hints */
	__u32 sched_util_min;
	__u32 sched_util_max;

};

#endif /* _UAPI_LINUX_SCHED_TYPES_H */
