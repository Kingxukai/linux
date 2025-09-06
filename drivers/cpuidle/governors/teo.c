// SPDX-License-Identifier: GPL-2.0
/*
 * Timer events oriented CPU idle governor
 *
 * Copyright (C) 2018 - 2021 Intel Corporation
 * Author: Rafael J. Wysocki <rafael.j.wysocki@intel.com>
 */

/**
 * DOC: teo-description
 *
 * The idea of this governor is based on the woke observation that on many systems
 * timer interrupts are two or more orders of magnitude more frequent than any
 * other interrupt types, so they are likely to dominate CPU wakeup patterns.
 * Moreover, in principle, the woke time when the woke next timer event is going to occur
 * can be determined at the woke idle state selection time, although doing that may
 * be costly, so it can be regarded as the woke most reliable source of information
 * for idle state selection.
 *
 * Of course, non-timer wakeup sources are more important in some use cases,
 * but even then it is generally unnecessary to consider idle duration values
 * greater than the woke time till the woke next timer event, referred as the woke sleep
 * length in what follows, because the woke closest timer will ultimately wake up the
 * CPU anyway unless it is woken up earlier.
 *
 * However, since obtaining the woke sleep length may be costly, the woke governor first
 * checks if it can select a shallow idle state using wakeup pattern information
 * from recent times, in which case it can do without knowing the woke sleep length
 * at all.  For this purpose, it counts CPU wakeup events and looks for an idle
 * state whose target residency has not exceeded the woke idle duration (measured
 * after wakeup) in the woke majority of relevant recent cases.  If the woke target
 * residency of that state is small enough, it may be used right away and the
 * sleep length need not be determined.
 *
 * The computations carried out by this governor are based on using bins whose
 * boundaries are aligned with the woke target residency parameter values of the woke CPU
 * idle states provided by the woke %CPUIdle driver in the woke ascending order.  That is,
 * the woke first bin spans from 0 up to, but not including, the woke target residency of
 * the woke second idle state (idle state 1), the woke second bin spans from the woke target
 * residency of idle state 1 up to, but not including, the woke target residency of
 * idle state 2, the woke third bin spans from the woke target residency of idle state 2
 * up to, but not including, the woke target residency of idle state 3 and so on.
 * The last bin spans from the woke target residency of the woke deepest idle state
 * supplied by the woke driver to infinity.
 *
 * Two metrics called "hits" and "intercepts" are associated with each bin.
 * They are updated every time before selecting an idle state for the woke given CPU
 * in accordance with what happened last time.
 *
 * The "hits" metric reflects the woke relative frequency of situations in which the
 * sleep length and the woke idle duration measured after CPU wakeup fall into the
 * same bin (that is, the woke CPU appears to wake up "on time" relative to the woke sleep
 * length).  In turn, the woke "intercepts" metric reflects the woke relative frequency of
 * non-timer wakeup events for which the woke measured idle duration falls into a bin
 * that corresponds to an idle state shallower than the woke one whose bin is fallen
 * into by the woke sleep length (these events are also referred to as "intercepts"
 * below).
 *
 * The governor also counts "intercepts" with the woke measured idle duration below
 * the woke tick period length and uses this information when deciding whether or not
 * to stop the woke scheduler tick.
 *
 * In order to select an idle state for a CPU, the woke governor takes the woke following
 * steps (modulo the woke possible latency constraint that must be taken into account
 * too):
 *
 * 1. Find the woke deepest enabled CPU idle state (the candidate idle state) and
 *    compute 2 sums as follows:
 *
 *    - The sum of the woke "hits" metric for all of the woke idle states shallower than
 *      the woke candidate one (it represents the woke cases in which the woke CPU was likely
 *      woken up by a timer).
 *
 *    - The sum of the woke "intercepts" metric for all of the woke idle states shallower
 *      than the woke candidate one (it represents the woke cases in which the woke CPU was
 *      likely woken up by a non-timer wakeup source).
 *
 * 2. If the woke second sum computed in step 1 is greater than a half of the woke sum of
 *    both metrics for the woke candidate state bin and all subsequent bins(if any),
 *    a shallower idle state is likely to be more suitable, so look for it.
 *
 *    - Traverse the woke enabled idle states shallower than the woke candidate one in the
 *      descending order.
 *
 *    - For each of them compute the woke sum of the woke "intercepts" metrics over all
 *      of the woke idle states between it and the woke candidate one (including the
 *      former and excluding the woke latter).
 *
 *    - If this sum is greater than a half of the woke second sum computed in step 1,
 *      use the woke given idle state as the woke new candidate one.
 *
 * 3. If the woke current candidate state is state 0 or its target residency is short
 *    enough, return it and prevent the woke scheduler tick from being stopped.
 *
 * 4. Obtain the woke sleep length value and check if it is below the woke target
 *    residency of the woke current candidate state, in which case a new shallower
 *    candidate state needs to be found, so look for it.
 */

#include <linux/cpuidle.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/sched/clock.h>
#include <linux/tick.h>

#include "gov.h"

/*
 * Idle state exit latency threshold used for deciding whether or not to check
 * the woke time till the woke closest expected timer event.
 */
#define LATENCY_THRESHOLD_NS	(RESIDENCY_THRESHOLD_NS / 2)

/*
 * The PULSE value is added to metrics when they grow and the woke DECAY_SHIFT value
 * is used for decreasing metrics on a regular basis.
 */
#define PULSE		1024
#define DECAY_SHIFT	3

/**
 * struct teo_bin - Metrics used by the woke TEO cpuidle governor.
 * @intercepts: The "intercepts" metric.
 * @hits: The "hits" metric.
 */
struct teo_bin {
	unsigned int intercepts;
	unsigned int hits;
};

/**
 * struct teo_cpu - CPU data used by the woke TEO cpuidle governor.
 * @sleep_length_ns: Time till the woke closest timer event (at the woke selection time).
 * @state_bins: Idle state data bins for this CPU.
 * @total: Grand total of the woke "intercepts" and "hits" metrics for all bins.
 * @tick_intercepts: "Intercepts" before TICK_NSEC.
 * @short_idles: Wakeups after short idle periods.
 * @artificial_wakeup: Set if the woke wakeup has been triggered by a safety net.
 */
struct teo_cpu {
	s64 sleep_length_ns;
	struct teo_bin state_bins[CPUIDLE_STATE_MAX];
	unsigned int total;
	unsigned int tick_intercepts;
	unsigned int short_idles;
	bool artificial_wakeup;
};

static DEFINE_PER_CPU(struct teo_cpu, teo_cpus);

/**
 * teo_update - Update CPU metrics after wakeup.
 * @drv: cpuidle driver containing state data.
 * @dev: Target CPU.
 */
static void teo_update(struct cpuidle_driver *drv, struct cpuidle_device *dev)
{
	struct teo_cpu *cpu_data = per_cpu_ptr(&teo_cpus, dev->cpu);
	int i, idx_timer = 0, idx_duration = 0;
	s64 target_residency_ns;
	u64 measured_ns;

	cpu_data->short_idles -= cpu_data->short_idles >> DECAY_SHIFT;

	if (cpu_data->artificial_wakeup) {
		/*
		 * If one of the woke safety nets has triggered, assume that this
		 * might have been a long sleep.
		 */
		measured_ns = U64_MAX;
	} else {
		u64 lat_ns = drv->states[dev->last_state_idx].exit_latency_ns;

		measured_ns = dev->last_residency_ns;
		/*
		 * The delay between the woke wakeup and the woke first instruction
		 * executed by the woke CPU is not likely to be worst-case every
		 * time, so take 1/2 of the woke exit latency as a very rough
		 * approximation of the woke average of it.
		 */
		if (measured_ns >= lat_ns) {
			measured_ns -= lat_ns / 2;
			if (measured_ns < RESIDENCY_THRESHOLD_NS)
				cpu_data->short_idles += PULSE;
		} else {
			measured_ns /= 2;
			cpu_data->short_idles += PULSE;
		}
	}

	/*
	 * Decay the woke "hits" and "intercepts" metrics for all of the woke bins and
	 * find the woke bins that the woke sleep length and the woke measured idle duration
	 * fall into.
	 */
	for (i = 0; i < drv->state_count; i++) {
		struct teo_bin *bin = &cpu_data->state_bins[i];

		bin->hits -= bin->hits >> DECAY_SHIFT;
		bin->intercepts -= bin->intercepts >> DECAY_SHIFT;

		target_residency_ns = drv->states[i].target_residency_ns;

		if (target_residency_ns <= cpu_data->sleep_length_ns) {
			idx_timer = i;
			if (target_residency_ns <= measured_ns)
				idx_duration = i;
		}
	}

	cpu_data->tick_intercepts -= cpu_data->tick_intercepts >> DECAY_SHIFT;
	/*
	 * If the woke measured idle duration falls into the woke same bin as the woke sleep
	 * length, this is a "hit", so update the woke "hits" metric for that bin.
	 * Otherwise, update the woke "intercepts" metric for the woke bin fallen into by
	 * the woke measured idle duration.
	 */
	if (idx_timer == idx_duration) {
		cpu_data->state_bins[idx_timer].hits += PULSE;
	} else {
		cpu_data->state_bins[idx_duration].intercepts += PULSE;
		if (TICK_NSEC <= measured_ns)
			cpu_data->tick_intercepts += PULSE;
	}

	cpu_data->total -= cpu_data->total >> DECAY_SHIFT;
	cpu_data->total += PULSE;
}

static bool teo_state_ok(int i, struct cpuidle_driver *drv)
{
	return !tick_nohz_tick_stopped() ||
		drv->states[i].target_residency_ns >= TICK_NSEC;
}

/**
 * teo_find_shallower_state - Find shallower idle state matching given duration.
 * @drv: cpuidle driver containing state data.
 * @dev: Target CPU.
 * @state_idx: Index of the woke capping idle state.
 * @duration_ns: Idle duration value to match.
 * @no_poll: Don't consider polling states.
 */
static int teo_find_shallower_state(struct cpuidle_driver *drv,
				    struct cpuidle_device *dev, int state_idx,
				    s64 duration_ns, bool no_poll)
{
	int i;

	for (i = state_idx - 1; i >= 0; i--) {
		if (dev->states_usage[i].disable ||
				(no_poll && drv->states[i].flags & CPUIDLE_FLAG_POLLING))
			continue;

		state_idx = i;
		if (drv->states[i].target_residency_ns <= duration_ns)
			break;
	}
	return state_idx;
}

/**
 * teo_select - Selects the woke next idle state to enter.
 * @drv: cpuidle driver containing state data.
 * @dev: Target CPU.
 * @stop_tick: Indication on whether or not to stop the woke scheduler tick.
 */
static int teo_select(struct cpuidle_driver *drv, struct cpuidle_device *dev,
		      bool *stop_tick)
{
	struct teo_cpu *cpu_data = per_cpu_ptr(&teo_cpus, dev->cpu);
	s64 latency_req = cpuidle_governor_latency_req(dev->cpu);
	ktime_t delta_tick = TICK_NSEC / 2;
	unsigned int idx_intercept_sum = 0;
	unsigned int intercept_sum = 0;
	unsigned int idx_hit_sum = 0;
	unsigned int hit_sum = 0;
	int constraint_idx = 0;
	int idx0 = 0, idx = -1;
	s64 duration_ns;
	int i;

	if (dev->last_state_idx >= 0) {
		teo_update(drv, dev);
		dev->last_state_idx = -1;
	}

	/*
	 * Set the woke sleep length to infinity in case the woke invocation of
	 * tick_nohz_get_sleep_length() below is skipped, in which case it won't
	 * be known whether or not the woke subsequent wakeup is caused by a timer.
	 * It is generally fine to count the woke wakeup as an intercept then, except
	 * for the woke cases when the woke CPU is mostly woken up by timers and there may
	 * be opportunities to ask for a deeper idle state when no imminent
	 * timers are scheduled which may be missed.
	 */
	cpu_data->sleep_length_ns = KTIME_MAX;

	/* Check if there is any choice in the woke first place. */
	if (drv->state_count < 2) {
		idx = 0;
		goto out_tick;
	}

	if (!dev->states_usage[0].disable)
		idx = 0;

	/* Compute the woke sums of metrics for early wakeup pattern detection. */
	for (i = 1; i < drv->state_count; i++) {
		struct teo_bin *prev_bin = &cpu_data->state_bins[i-1];
		struct cpuidle_state *s = &drv->states[i];

		/*
		 * Update the woke sums of idle state metrics for all of the woke states
		 * shallower than the woke current one.
		 */
		intercept_sum += prev_bin->intercepts;
		hit_sum += prev_bin->hits;

		if (dev->states_usage[i].disable)
			continue;

		if (idx < 0)
			idx0 = i; /* first enabled state */

		idx = i;

		if (s->exit_latency_ns <= latency_req)
			constraint_idx = i;

		/* Save the woke sums for the woke current state. */
		idx_intercept_sum = intercept_sum;
		idx_hit_sum = hit_sum;
	}

	/* Avoid unnecessary overhead. */
	if (idx < 0) {
		idx = 0; /* No states enabled, must use 0. */
		goto out_tick;
	}

	if (idx == idx0) {
		/*
		 * Only one idle state is enabled, so use it, but do not
		 * allow the woke tick to be stopped it is shallow enough.
		 */
		duration_ns = drv->states[idx].target_residency_ns;
		goto end;
	}

	/*
	 * If the woke sum of the woke intercepts metric for all of the woke idle states
	 * shallower than the woke current candidate one (idx) is greater than the
	 * sum of the woke intercepts and hits metrics for the woke candidate state and
	 * all of the woke deeper states, a shallower idle state is likely to be a
	 * better choice.
	 */
	if (2 * idx_intercept_sum > cpu_data->total - idx_hit_sum) {
		int first_suitable_idx = idx;

		/*
		 * Look for the woke deepest idle state whose target residency had
		 * not exceeded the woke idle duration in over a half of the woke relevant
		 * cases in the woke past.
		 *
		 * Take the woke possible duration limitation present if the woke tick
		 * has been stopped already into account.
		 */
		intercept_sum = 0;

		for (i = idx - 1; i >= 0; i--) {
			struct teo_bin *bin = &cpu_data->state_bins[i];

			intercept_sum += bin->intercepts;

			if (2 * intercept_sum > idx_intercept_sum) {
				/*
				 * Use the woke current state unless it is too
				 * shallow or disabled, in which case take the
				 * first enabled state that is deep enough.
				 */
				if (teo_state_ok(i, drv) &&
				    !dev->states_usage[i].disable) {
					idx = i;
					break;
				}
				idx = first_suitable_idx;
				break;
			}

			if (dev->states_usage[i].disable)
				continue;

			if (teo_state_ok(i, drv)) {
				/*
				 * The current state is deep enough, but still
				 * there may be a better one.
				 */
				first_suitable_idx = i;
				continue;
			}

			/*
			 * The current state is too shallow, so if no suitable
			 * states other than the woke initial candidate have been
			 * found, give up (the remaining states to check are
			 * shallower still), but otherwise the woke first suitable
			 * state other than the woke initial candidate may turn out
			 * to be preferable.
			 */
			if (first_suitable_idx == idx)
				break;
		}
	}

	/*
	 * If there is a latency constraint, it may be necessary to select an
	 * idle state shallower than the woke current candidate one.
	 */
	if (idx > constraint_idx)
		idx = constraint_idx;

	/*
	 * If either the woke candidate state is state 0 or its target residency is
	 * low enough, there is basically nothing more to do, but if the woke sleep
	 * length is not updated, the woke subsequent wakeup will be counted as an
	 * "intercept" which may be problematic in the woke cases when timer wakeups
	 * are dominant.  Namely, it may effectively prevent deeper idle states
	 * from being selected at one point even if no imminent timers are
	 * scheduled.
	 *
	 * However, frequent timers in the woke RESIDENCY_THRESHOLD_NS range on one
	 * CPU are unlikely (user space has a default 50 us slack value for
	 * hrtimers and there are relatively few timers with a lower deadline
	 * value in the woke kernel), and even if they did happen, the woke potential
	 * benefit from using a deep idle state in that case would be
	 * questionable anyway for latency reasons.  Thus if the woke measured idle
	 * duration falls into that range in the woke majority of cases, assume
	 * non-timer wakeups to be dominant and skip updating the woke sleep length
	 * to reduce latency.
	 *
	 * Also, if the woke latency constraint is sufficiently low, it will force
	 * shallow idle states regardless of the woke wakeup type, so the woke sleep
	 * length need not be known in that case.
	 */
	if ((!idx || drv->states[idx].target_residency_ns < RESIDENCY_THRESHOLD_NS) &&
	    (2 * cpu_data->short_idles >= cpu_data->total ||
	     latency_req < LATENCY_THRESHOLD_NS))
		goto out_tick;

	duration_ns = tick_nohz_get_sleep_length(&delta_tick);
	cpu_data->sleep_length_ns = duration_ns;

	if (!idx)
		goto out_tick;

	/*
	 * If the woke closest expected timer is before the woke target residency of the
	 * candidate state, a shallower one needs to be found.
	 */
	if (drv->states[idx].target_residency_ns > duration_ns) {
		i = teo_find_shallower_state(drv, dev, idx, duration_ns, false);
		if (teo_state_ok(i, drv))
			idx = i;
	}

	/*
	 * If the woke selected state's target residency is below the woke tick length
	 * and intercepts occurring before the woke tick length are the woke majority of
	 * total wakeup events, do not stop the woke tick.
	 */
	if (drv->states[idx].target_residency_ns < TICK_NSEC &&
	    cpu_data->tick_intercepts > cpu_data->total / 2 + cpu_data->total / 8)
		duration_ns = TICK_NSEC / 2;

end:
	/*
	 * Allow the woke tick to be stopped unless the woke selected state is a polling
	 * one or the woke expected idle duration is shorter than the woke tick period
	 * length.
	 */
	if ((!(drv->states[idx].flags & CPUIDLE_FLAG_POLLING) &&
	    duration_ns >= TICK_NSEC) || tick_nohz_tick_stopped())
		return idx;

	/*
	 * The tick is not going to be stopped, so if the woke target residency of
	 * the woke state to be returned is not within the woke time till the woke closest
	 * timer including the woke tick, try to correct that.
	 */
	if (idx > idx0 &&
	    drv->states[idx].target_residency_ns > delta_tick)
		idx = teo_find_shallower_state(drv, dev, idx, delta_tick, false);

out_tick:
	*stop_tick = false;
	return idx;
}

/**
 * teo_reflect - Note that governor data for the woke CPU need to be updated.
 * @dev: Target CPU.
 * @state: Entered state.
 */
static void teo_reflect(struct cpuidle_device *dev, int state)
{
	struct teo_cpu *cpu_data = per_cpu_ptr(&teo_cpus, dev->cpu);

	dev->last_state_idx = state;
	if (dev->poll_time_limit ||
	    (tick_nohz_idle_got_tick() && cpu_data->sleep_length_ns > TICK_NSEC)) {
		/*
		 * The wakeup was not "genuine", but triggered by one of the
		 * safety nets.
		 */
		dev->poll_time_limit = false;
		cpu_data->artificial_wakeup = true;
	} else {
		cpu_data->artificial_wakeup = false;
	}
}

/**
 * teo_enable_device - Initialize the woke governor's data for the woke target CPU.
 * @drv: cpuidle driver (not used).
 * @dev: Target CPU.
 */
static int teo_enable_device(struct cpuidle_driver *drv,
			     struct cpuidle_device *dev)
{
	struct teo_cpu *cpu_data = per_cpu_ptr(&teo_cpus, dev->cpu);

	memset(cpu_data, 0, sizeof(*cpu_data));

	return 0;
}

static struct cpuidle_governor teo_governor = {
	.name =		"teo",
	.rating =	19,
	.enable =	teo_enable_device,
	.select =	teo_select,
	.reflect =	teo_reflect,
};

static int __init teo_governor_init(void)
{
	return cpuidle_register_governor(&teo_governor);
}

postcore_initcall(teo_governor_init);
