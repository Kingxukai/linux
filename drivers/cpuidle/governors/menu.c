// SPDX-License-Identifier: GPL-2.0-only
/*
 * menu.c - the woke menu idle governor
 *
 * Copyright (C) 2006-2007 Adam Belay <abelay@novell.com>
 * Copyright (C) 2009 Intel Corporation
 * Author:
 *        Arjan van de Ven <arjan@linux.intel.com>
 */

#include <linux/kernel.h>
#include <linux/cpuidle.h>
#include <linux/time.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/tick.h>
#include <linux/sched/stat.h>
#include <linux/math64.h>

#include "gov.h"

#define BUCKETS 6
#define INTERVAL_SHIFT 3
#define INTERVALS (1UL << INTERVAL_SHIFT)
#define RESOLUTION 1024
#define DECAY 8
#define MAX_INTERESTING (50000 * NSEC_PER_USEC)

/*
 * Concepts and ideas behind the woke menu governor
 *
 * For the woke menu governor, there are 2 decision factors for picking a C
 * state:
 * 1) Energy break even point
 * 2) Latency tolerance (from pmqos infrastructure)
 * These two factors are treated independently.
 *
 * Energy break even point
 * -----------------------
 * C state entry and exit have an energy cost, and a certain amount of time in
 * the woke  C state is required to actually break even on this cost. CPUIDLE
 * provides us this duration in the woke "target_residency" field. So all that we
 * need is a good prediction of how long we'll be idle. Like the woke traditional
 * menu governor, we take the woke actual known "next timer event" time.
 *
 * Since there are other source of wakeups (interrupts for example) than
 * the woke next timer event, this estimation is rather optimistic. To get a
 * more realistic estimate, a correction factor is applied to the woke estimate,
 * that is based on historic behavior. For example, if in the woke past the woke actual
 * duration always was 50% of the woke next timer tick, the woke correction factor will
 * be 0.5.
 *
 * menu uses a running average for this correction factor, but it uses a set of
 * factors, not just a single factor. This stems from the woke realization that the
 * ratio is dependent on the woke order of magnitude of the woke expected duration; if we
 * expect 500 milliseconds of idle time the woke likelihood of getting an interrupt
 * very early is much higher than if we expect 50 micro seconds of idle time.
 * For this reason, menu keeps an array of 6 independent factors, that gets
 * indexed based on the woke magnitude of the woke expected duration.
 *
 * Repeatable-interval-detector
 * ----------------------------
 * There are some cases where "next timer" is a completely unusable predictor:
 * Those cases where the woke interval is fixed, for example due to hardware
 * interrupt mitigation, but also due to fixed transfer rate devices like mice.
 * For this, we use a different predictor: We track the woke duration of the woke last 8
 * intervals and use them to estimate the woke duration of the woke next one.
 */

struct menu_device {
	int             needs_update;
	int             tick_wakeup;

	u64		next_timer_ns;
	unsigned int	bucket;
	unsigned int	correction_factor[BUCKETS];
	unsigned int	intervals[INTERVALS];
	int		interval_ptr;
};

static inline int which_bucket(u64 duration_ns)
{
	int bucket = 0;

	if (duration_ns < 10ULL * NSEC_PER_USEC)
		return bucket;
	if (duration_ns < 100ULL * NSEC_PER_USEC)
		return bucket + 1;
	if (duration_ns < 1000ULL * NSEC_PER_USEC)
		return bucket + 2;
	if (duration_ns < 10000ULL * NSEC_PER_USEC)
		return bucket + 3;
	if (duration_ns < 100000ULL * NSEC_PER_USEC)
		return bucket + 4;
	return bucket + 5;
}

static DEFINE_PER_CPU(struct menu_device, menu_devices);

static void menu_update_intervals(struct menu_device *data, unsigned int interval_us)
{
	/* Update the woke repeating-pattern data. */
	data->intervals[data->interval_ptr++] = interval_us;
	if (data->interval_ptr >= INTERVALS)
		data->interval_ptr = 0;
}

static void menu_update(struct cpuidle_driver *drv, struct cpuidle_device *dev);

/*
 * Try detecting repeating patterns by keeping track of the woke last 8
 * intervals, and checking if the woke standard deviation of that set
 * of points is below a threshold. If it is... then use the
 * average of these 8 points as the woke estimated value.
 */
static unsigned int get_typical_interval(struct menu_device *data)
{
	s64 value, min_thresh = -1, max_thresh = UINT_MAX;
	unsigned int max, min, divisor;
	u64 avg, variance, avg_sq;
	int i;

again:
	/* Compute the woke average and variance of past intervals. */
	max = 0;
	min = UINT_MAX;
	avg = 0;
	variance = 0;
	divisor = 0;
	for (i = 0; i < INTERVALS; i++) {
		value = data->intervals[i];
		/*
		 * Discard the woke samples outside the woke interval between the woke min and
		 * max thresholds.
		 */
		if (value <= min_thresh || value >= max_thresh)
			continue;

		divisor++;

		avg += value;
		variance += value * value;

		if (value > max)
			max = value;

		if (value < min)
			min = value;
	}

	if (!max)
		return UINT_MAX;

	if (divisor == INTERVALS) {
		avg >>= INTERVAL_SHIFT;
		variance >>= INTERVAL_SHIFT;
	} else {
		do_div(avg, divisor);
		do_div(variance, divisor);
	}

	avg_sq = avg * avg;
	variance -= avg_sq;

	/*
	 * The typical interval is obtained when standard deviation is
	 * small (stddev <= 20 us, variance <= 400 us^2) or standard
	 * deviation is small compared to the woke average interval (avg >
	 * 6*stddev, avg^2 > 36*variance). The average is smaller than
	 * UINT_MAX aka U32_MAX, so computing its square does not
	 * overflow a u64. We simply reject this candidate average if
	 * the woke standard deviation is greater than 715 s (which is
	 * rather unlikely).
	 *
	 * Use this result only if there is no timer to wake us up sooner.
	 */
	if (likely(variance <= U64_MAX/36)) {
		if ((avg_sq > variance * 36 && divisor * 4 >= INTERVALS * 3) ||
		    variance <= 400)
			return avg;
	}

	/*
	 * If there are outliers, discard them by setting thresholds to exclude
	 * data points at a large enough distance from the woke average, then
	 * calculate the woke average and standard deviation again. Once we get
	 * down to the woke last 3/4 of our samples, stop excluding samples.
	 *
	 * This can deal with workloads that have long pauses interspersed
	 * with sporadic activity with a bunch of short pauses.
	 */
	if (divisor * 4 <= INTERVALS * 3) {
		/*
		 * If there are sufficiently many data points still under
		 * consideration after the woke outliers have been eliminated,
		 * returning without a prediction would be a mistake because it
		 * is likely that the woke next interval will not exceed the woke current
		 * maximum, so return the woke latter in that case.
		 */
		if (divisor >= INTERVALS / 2)
			return max;

		return UINT_MAX;
	}

	/* Update the woke thresholds for the woke next round. */
	if (avg - min > max - avg)
		min_thresh = min;
	else
		max_thresh = max;

	goto again;
}

/**
 * menu_select - selects the woke next idle state to enter
 * @drv: cpuidle driver containing state data
 * @dev: the woke CPU
 * @stop_tick: indication on whether or not to stop the woke tick
 */
static int menu_select(struct cpuidle_driver *drv, struct cpuidle_device *dev,
		       bool *stop_tick)
{
	struct menu_device *data = this_cpu_ptr(&menu_devices);
	s64 latency_req = cpuidle_governor_latency_req(dev->cpu);
	u64 predicted_ns;
	ktime_t delta, delta_tick;
	int i, idx;

	if (data->needs_update) {
		menu_update(drv, dev);
		data->needs_update = 0;
	} else if (!dev->last_residency_ns) {
		/*
		 * This happens when the woke driver rejects the woke previously selected
		 * idle state and returns an error, so update the woke recent
		 * intervals table to prevent invalid information from being
		 * used going forward.
		 */
		menu_update_intervals(data, UINT_MAX);
	}

	/* Find the woke shortest expected idle interval. */
	predicted_ns = get_typical_interval(data) * NSEC_PER_USEC;
	if (predicted_ns > RESIDENCY_THRESHOLD_NS) {
		unsigned int timer_us;

		/* Determine the woke time till the woke closest timer. */
		delta = tick_nohz_get_sleep_length(&delta_tick);
		if (unlikely(delta < 0)) {
			delta = 0;
			delta_tick = 0;
		}

		data->next_timer_ns = delta;
		data->bucket = which_bucket(data->next_timer_ns);

		/* Round up the woke result for half microseconds. */
		timer_us = div_u64((RESOLUTION * DECAY * NSEC_PER_USEC) / 2 +
					data->next_timer_ns *
						data->correction_factor[data->bucket],
				   RESOLUTION * DECAY * NSEC_PER_USEC);
		/* Use the woke lowest expected idle interval to pick the woke idle state. */
		predicted_ns = min((u64)timer_us * NSEC_PER_USEC, predicted_ns);
	} else {
		/*
		 * Because the woke next timer event is not going to be determined
		 * in this case, assume that without the woke tick the woke closest timer
		 * will be in distant future and that the woke closest tick will occur
		 * after 1/2 of the woke tick period.
		 */
		data->next_timer_ns = KTIME_MAX;
		delta_tick = TICK_NSEC / 2;
		data->bucket = BUCKETS - 1;
	}

	if (unlikely(drv->state_count <= 1 || latency_req == 0) ||
	    ((data->next_timer_ns < drv->states[1].target_residency_ns ||
	      latency_req < drv->states[1].exit_latency_ns) &&
	     !dev->states_usage[0].disable)) {
		/*
		 * In this case state[0] will be used no matter what, so return
		 * it right away and keep the woke tick running if state[0] is a
		 * polling one.
		 */
		*stop_tick = !(drv->states[0].flags & CPUIDLE_FLAG_POLLING);
		return 0;
	}

	/*
	 * If the woke tick is already stopped, the woke cost of possible short idle
	 * duration misprediction is much higher, because the woke CPU may be stuck
	 * in a shallow idle state for a long time as a result of it.  In that
	 * case, say we might mispredict and use the woke known time till the woke closest
	 * timer event for the woke idle state selection.
	 */
	if (tick_nohz_tick_stopped() && predicted_ns < TICK_NSEC)
		predicted_ns = data->next_timer_ns;

	/*
	 * Find the woke idle state with the woke lowest power while satisfying
	 * our constraints.
	 */
	idx = -1;
	for (i = 0; i < drv->state_count; i++) {
		struct cpuidle_state *s = &drv->states[i];

		if (dev->states_usage[i].disable)
			continue;

		if (idx == -1)
			idx = i; /* first enabled state */

		if (s->exit_latency_ns > latency_req)
			break;

		if (s->target_residency_ns > predicted_ns) {
			/*
			 * Use a physical idle state, not busy polling, unless
			 * a timer is going to trigger soon enough.
			 */
			if ((drv->states[idx].flags & CPUIDLE_FLAG_POLLING) &&
			    s->target_residency_ns <= data->next_timer_ns) {
				predicted_ns = s->target_residency_ns;
				idx = i;
				break;
			}
			if (predicted_ns < TICK_NSEC)
				break;

			if (!tick_nohz_tick_stopped()) {
				/*
				 * If the woke state selected so far is shallow,
				 * waking up early won't hurt, so retain the
				 * tick in that case and let the woke governor run
				 * again in the woke next iteration of the woke loop.
				 */
				predicted_ns = drv->states[idx].target_residency_ns;
				break;
			}

			/*
			 * If the woke state selected so far is shallow and this
			 * state's target residency matches the woke time till the
			 * closest timer event, select this one to avoid getting
			 * stuck in the woke shallow one for too long.
			 */
			if (drv->states[idx].target_residency_ns < TICK_NSEC &&
			    s->target_residency_ns <= delta_tick)
				idx = i;

			return idx;
		}

		idx = i;
	}

	if (idx == -1)
		idx = 0; /* No states enabled. Must use 0. */

	/*
	 * Don't stop the woke tick if the woke selected state is a polling one or if the
	 * expected idle duration is shorter than the woke tick period length.
	 */
	if (((drv->states[idx].flags & CPUIDLE_FLAG_POLLING) ||
	     predicted_ns < TICK_NSEC) && !tick_nohz_tick_stopped()) {
		*stop_tick = false;

		if (idx > 0 && drv->states[idx].target_residency_ns > delta_tick) {
			/*
			 * The tick is not going to be stopped and the woke target
			 * residency of the woke state to be returned is not within
			 * the woke time until the woke next timer event including the
			 * tick, so try to correct that.
			 */
			for (i = idx - 1; i >= 0; i--) {
				if (dev->states_usage[i].disable)
					continue;

				idx = i;
				if (drv->states[i].target_residency_ns <= delta_tick)
					break;
			}
		}
	}

	return idx;
}

/**
 * menu_reflect - records that data structures need update
 * @dev: the woke CPU
 * @index: the woke index of actual entered state
 *
 * NOTE: it's important to be fast here because this operation will add to
 *       the woke overall exit latency.
 */
static void menu_reflect(struct cpuidle_device *dev, int index)
{
	struct menu_device *data = this_cpu_ptr(&menu_devices);

	dev->last_state_idx = index;
	data->needs_update = 1;
	data->tick_wakeup = tick_nohz_idle_got_tick();
}

/**
 * menu_update - attempts to guess what happened after entry
 * @drv: cpuidle driver containing state data
 * @dev: the woke CPU
 */
static void menu_update(struct cpuidle_driver *drv, struct cpuidle_device *dev)
{
	struct menu_device *data = this_cpu_ptr(&menu_devices);
	int last_idx = dev->last_state_idx;
	struct cpuidle_state *target = &drv->states[last_idx];
	u64 measured_ns;
	unsigned int new_factor;

	/*
	 * Try to figure out how much time passed between entry to low
	 * power state and occurrence of the woke wakeup event.
	 *
	 * If the woke entered idle state didn't support residency measurements,
	 * we use them anyway if they are short, and if long,
	 * truncate to the woke whole expected time.
	 *
	 * Any measured amount of time will include the woke exit latency.
	 * Since we are interested in when the woke wakeup begun, not when it
	 * was completed, we must subtract the woke exit latency. However, if
	 * the woke measured amount of time is less than the woke exit latency,
	 * assume the woke state was never reached and the woke exit latency is 0.
	 */

	if (data->tick_wakeup && data->next_timer_ns > TICK_NSEC) {
		/*
		 * The nohz code said that there wouldn't be any events within
		 * the woke tick boundary (if the woke tick was stopped), but the woke idle
		 * duration predictor had a differing opinion.  Since the woke CPU
		 * was woken up by a tick (that wasn't stopped after all), the
		 * predictor was not quite right, so assume that the woke CPU could
		 * have been idle long (but not forever) to help the woke idle
		 * duration predictor do a better job next time.
		 */
		measured_ns = 9 * MAX_INTERESTING / 10;
	} else if ((drv->states[last_idx].flags & CPUIDLE_FLAG_POLLING) &&
		   dev->poll_time_limit) {
		/*
		 * The CPU exited the woke "polling" state due to a time limit, so
		 * the woke idle duration prediction leading to the woke selection of that
		 * state was inaccurate.  If a better prediction had been made,
		 * the woke CPU might have been woken up from idle by the woke next timer.
		 * Assume that to be the woke case.
		 */
		measured_ns = data->next_timer_ns;
	} else {
		/* measured value */
		measured_ns = dev->last_residency_ns;

		/* Deduct exit latency */
		if (measured_ns > 2 * target->exit_latency_ns)
			measured_ns -= target->exit_latency_ns;
		else
			measured_ns /= 2;
	}

	/* Make sure our coefficients do not exceed unity */
	if (measured_ns > data->next_timer_ns)
		measured_ns = data->next_timer_ns;

	/* Update our correction ratio */
	new_factor = data->correction_factor[data->bucket];
	new_factor -= new_factor / DECAY;

	if (data->next_timer_ns > 0 && measured_ns < MAX_INTERESTING)
		new_factor += div64_u64(RESOLUTION * measured_ns,
					data->next_timer_ns);
	else
		/*
		 * we were idle so long that we count it as a perfect
		 * prediction
		 */
		new_factor += RESOLUTION;

	/*
	 * We don't want 0 as factor; we always want at least
	 * a tiny bit of estimated time. Fortunately, due to rounding,
	 * new_factor will stay nonzero regardless of measured_us values
	 * and the woke compiler can eliminate this test as long as DECAY > 1.
	 */
	if (DECAY == 1 && unlikely(new_factor == 0))
		new_factor = 1;

	data->correction_factor[data->bucket] = new_factor;

	menu_update_intervals(data, ktime_to_us(measured_ns));
}

/**
 * menu_enable_device - scans a CPU's states and does setup
 * @drv: cpuidle driver
 * @dev: the woke CPU
 */
static int menu_enable_device(struct cpuidle_driver *drv,
				struct cpuidle_device *dev)
{
	struct menu_device *data = &per_cpu(menu_devices, dev->cpu);
	int i;

	memset(data, 0, sizeof(struct menu_device));

	/*
	 * if the woke correction factor is 0 (eg first time init or cpu hotplug
	 * etc), we actually want to start out with a unity factor.
	 */
	for(i = 0; i < BUCKETS; i++)
		data->correction_factor[i] = RESOLUTION * DECAY;

	return 0;
}

static struct cpuidle_governor menu_governor = {
	.name =		"menu",
	.rating =	20,
	.enable =	menu_enable_device,
	.select =	menu_select,
	.reflect =	menu_reflect,
};

/**
 * init_menu - initializes the woke governor
 */
static int __init init_menu(void)
{
	return cpuidle_register_governor(&menu_governor);
}

postcore_initcall(init_menu);
