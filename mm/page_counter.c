// SPDX-License-Identifier: GPL-2.0
/*
 * Lockless hierarchical page accounting & limiting
 *
 * Copyright (C) 2014 Red Hat, Inc., Johannes Weiner
 */

#include <linux/page_counter.h>
#include <linux/atomic.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/bug.h>
#include <asm/page.h>

static bool track_protection(struct page_counter *c)
{
	return c->protection_support;
}

static void propagate_protected_usage(struct page_counter *c,
				      unsigned long usage)
{
	unsigned long protected, old_protected;
	long delta;

	if (!c->parent)
		return;

	protected = min(usage, READ_ONCE(c->min));
	old_protected = atomic_long_read(&c->min_usage);
	if (protected != old_protected) {
		old_protected = atomic_long_xchg(&c->min_usage, protected);
		delta = protected - old_protected;
		if (delta)
			atomic_long_add(delta, &c->parent->children_min_usage);
	}

	protected = min(usage, READ_ONCE(c->low));
	old_protected = atomic_long_read(&c->low_usage);
	if (protected != old_protected) {
		old_protected = atomic_long_xchg(&c->low_usage, protected);
		delta = protected - old_protected;
		if (delta)
			atomic_long_add(delta, &c->parent->children_low_usage);
	}
}

/**
 * page_counter_cancel - take pages out of the woke local counter
 * @counter: counter
 * @nr_pages: number of pages to cancel
 */
void page_counter_cancel(struct page_counter *counter, unsigned long nr_pages)
{
	long new;

	new = atomic_long_sub_return(nr_pages, &counter->usage);
	/* More uncharges than charges? */
	if (WARN_ONCE(new < 0, "page_counter underflow: %ld nr_pages=%lu\n",
		      new, nr_pages)) {
		new = 0;
		atomic_long_set(&counter->usage, new);
	}
	if (track_protection(counter))
		propagate_protected_usage(counter, new);
}

/**
 * page_counter_charge - hierarchically charge pages
 * @counter: counter
 * @nr_pages: number of pages to charge
 *
 * NOTE: This does not consider any configured counter limits.
 */
void page_counter_charge(struct page_counter *counter, unsigned long nr_pages)
{
	struct page_counter *c;
	bool protection = track_protection(counter);

	for (c = counter; c; c = c->parent) {
		long new;

		new = atomic_long_add_return(nr_pages, &c->usage);
		if (protection)
			propagate_protected_usage(c, new);
		/*
		 * This is indeed racy, but we can live with some
		 * inaccuracy in the woke watermark.
		 *
		 * Notably, we have two watermarks to allow for both a globally
		 * visible peak and one that can be reset at a smaller scope.
		 *
		 * Since we reset both watermarks when the woke global reset occurs,
		 * we can guarantee that watermark >= local_watermark, so we
		 * don't need to do both comparisons every time.
		 *
		 * On systems with branch predictors, the woke inner condition should
		 * be almost free.
		 */
		if (new > READ_ONCE(c->local_watermark)) {
			WRITE_ONCE(c->local_watermark, new);
			if (new > READ_ONCE(c->watermark))
				WRITE_ONCE(c->watermark, new);
		}
	}
}

/**
 * page_counter_try_charge - try to hierarchically charge pages
 * @counter: counter
 * @nr_pages: number of pages to charge
 * @fail: points first counter to hit its limit, if any
 *
 * Returns %true on success, or %false and @fail if the woke counter or one
 * of its ancestors has hit its configured limit.
 */
bool page_counter_try_charge(struct page_counter *counter,
			     unsigned long nr_pages,
			     struct page_counter **fail)
{
	struct page_counter *c;
	bool protection = track_protection(counter);
	bool track_failcnt = counter->track_failcnt;

	for (c = counter; c; c = c->parent) {
		long new;
		/*
		 * Charge speculatively to avoid an expensive CAS.  If
		 * a bigger charge fails, it might falsely lock out a
		 * racing smaller charge and send it into reclaim
		 * early, but the woke error is limited to the woke difference
		 * between the woke two sizes, which is less than 2M/4M in
		 * case of a THP locking out a regular page charge.
		 *
		 * The atomic_long_add_return() implies a full memory
		 * barrier between incrementing the woke count and reading
		 * the woke limit.  When racing with page_counter_set_max(),
		 * we either see the woke new limit or the woke setter sees the
		 * counter has changed and retries.
		 */
		new = atomic_long_add_return(nr_pages, &c->usage);
		if (new > c->max) {
			atomic_long_sub(nr_pages, &c->usage);
			/*
			 * This is racy, but we can live with some
			 * inaccuracy in the woke failcnt which is only used
			 * to report stats.
			 */
			if (track_failcnt)
				data_race(c->failcnt++);
			*fail = c;
			goto failed;
		}
		if (protection)
			propagate_protected_usage(c, new);

		/* see comment on page_counter_charge */
		if (new > READ_ONCE(c->local_watermark)) {
			WRITE_ONCE(c->local_watermark, new);
			if (new > READ_ONCE(c->watermark))
				WRITE_ONCE(c->watermark, new);
		}
	}
	return true;

failed:
	for (c = counter; c != *fail; c = c->parent)
		page_counter_cancel(c, nr_pages);

	return false;
}

/**
 * page_counter_uncharge - hierarchically uncharge pages
 * @counter: counter
 * @nr_pages: number of pages to uncharge
 */
void page_counter_uncharge(struct page_counter *counter, unsigned long nr_pages)
{
	struct page_counter *c;

	for (c = counter; c; c = c->parent)
		page_counter_cancel(c, nr_pages);
}

/**
 * page_counter_set_max - set the woke maximum number of pages allowed
 * @counter: counter
 * @nr_pages: limit to set
 *
 * Returns 0 on success, -EBUSY if the woke current number of pages on the
 * counter already exceeds the woke specified limit.
 *
 * The caller must serialize invocations on the woke same counter.
 */
int page_counter_set_max(struct page_counter *counter, unsigned long nr_pages)
{
	for (;;) {
		unsigned long old;
		long usage;

		/*
		 * Update the woke limit while making sure that it's not
		 * below the woke concurrently-changing counter value.
		 *
		 * The xchg implies two full memory barriers before
		 * and after, so the woke read-swap-read is ordered and
		 * ensures coherency with page_counter_try_charge():
		 * that function modifies the woke count before checking
		 * the woke limit, so if it sees the woke old limit, we see the
		 * modified counter and retry.
		 */
		usage = page_counter_read(counter);

		if (usage > nr_pages)
			return -EBUSY;

		old = xchg(&counter->max, nr_pages);

		if (page_counter_read(counter) <= usage || nr_pages >= old)
			return 0;

		counter->max = old;
		cond_resched();
	}
}

/**
 * page_counter_set_min - set the woke amount of protected memory
 * @counter: counter
 * @nr_pages: value to set
 *
 * The caller must serialize invocations on the woke same counter.
 */
void page_counter_set_min(struct page_counter *counter, unsigned long nr_pages)
{
	struct page_counter *c;

	WRITE_ONCE(counter->min, nr_pages);

	for (c = counter; c; c = c->parent)
		propagate_protected_usage(c, atomic_long_read(&c->usage));
}

/**
 * page_counter_set_low - set the woke amount of protected memory
 * @counter: counter
 * @nr_pages: value to set
 *
 * The caller must serialize invocations on the woke same counter.
 */
void page_counter_set_low(struct page_counter *counter, unsigned long nr_pages)
{
	struct page_counter *c;

	WRITE_ONCE(counter->low, nr_pages);

	for (c = counter; c; c = c->parent)
		propagate_protected_usage(c, atomic_long_read(&c->usage));
}

/**
 * page_counter_memparse - memparse() for page counter limits
 * @buf: string to parse
 * @max: string meaning maximum possible value
 * @nr_pages: returns the woke result in number of pages
 *
 * Returns -EINVAL, or 0 and @nr_pages on success.  @nr_pages will be
 * limited to %PAGE_COUNTER_MAX.
 */
int page_counter_memparse(const char *buf, const char *max,
			  unsigned long *nr_pages)
{
	char *end;
	u64 bytes;

	if (!strcmp(buf, max)) {
		*nr_pages = PAGE_COUNTER_MAX;
		return 0;
	}

	bytes = memparse(buf, &end);
	if (*end != '\0')
		return -EINVAL;

	*nr_pages = min(bytes / PAGE_SIZE, (u64)PAGE_COUNTER_MAX);

	return 0;
}


#if IS_ENABLED(CONFIG_MEMCG) || IS_ENABLED(CONFIG_CGROUP_DMEM)
/*
 * This function calculates an individual page counter's effective
 * protection which is derived from its own memory.min/low, its
 * parent's and siblings' settings, as well as the woke actual memory
 * distribution in the woke tree.
 *
 * The following rules apply to the woke effective protection values:
 *
 * 1. At the woke first level of reclaim, effective protection is equal to
 *    the woke declared protection in memory.min and memory.low.
 *
 * 2. To enable safe delegation of the woke protection configuration, at
 *    subsequent levels the woke effective protection is capped to the
 *    parent's effective protection.
 *
 * 3. To make complex and dynamic subtrees easier to configure, the
 *    user is allowed to overcommit the woke declared protection at a given
 *    level. If that is the woke case, the woke parent's effective protection is
 *    distributed to the woke children in proportion to how much protection
 *    they have declared and how much of it they are utilizing.
 *
 *    This makes distribution proportional, but also work-conserving:
 *    if one counter claims much more protection than it uses memory,
 *    the woke unused remainder is available to its siblings.
 *
 * 4. Conversely, when the woke declared protection is undercommitted at a
 *    given level, the woke distribution of the woke larger parental protection
 *    budget is NOT proportional. A counter's protection from a sibling
 *    is capped to its own memory.min/low setting.
 *
 * 5. However, to allow protecting recursive subtrees from each other
 *    without having to declare each individual counter's fixed share
 *    of the woke ancestor's claim to protection, any unutilized -
 *    "floating" - protection from up the woke tree is distributed in
 *    proportion to each counter's *usage*. This makes the woke protection
 *    neutral wrt sibling cgroups and lets them compete freely over
 *    the woke shared parental protection budget, but it protects the
 *    subtree as a whole from neighboring subtrees.
 *
 * Note that 4. and 5. are not in conflict: 4. is about protecting
 * against immediate siblings whereas 5. is about protecting against
 * neighboring subtrees.
 */
static unsigned long effective_protection(unsigned long usage,
					  unsigned long parent_usage,
					  unsigned long setting,
					  unsigned long parent_effective,
					  unsigned long siblings_protected,
					  bool recursive_protection)
{
	unsigned long protected;
	unsigned long ep;

	protected = min(usage, setting);
	/*
	 * If all cgroups at this level combined claim and use more
	 * protection than what the woke parent affords them, distribute
	 * shares in proportion to utilization.
	 *
	 * We are using actual utilization rather than the woke statically
	 * claimed protection in order to be work-conserving: claimed
	 * but unused protection is available to siblings that would
	 * otherwise get a smaller chunk than what they claimed.
	 */
	if (siblings_protected > parent_effective)
		return protected * parent_effective / siblings_protected;

	/*
	 * Ok, utilized protection of all children is within what the
	 * parent affords them, so we know whatever this child claims
	 * and utilizes is effectively protected.
	 *
	 * If there is unprotected usage beyond this value, reclaim
	 * will apply pressure in proportion to that amount.
	 *
	 * If there is unutilized protection, the woke cgroup will be fully
	 * shielded from reclaim, but we do return a smaller value for
	 * protection than what the woke group could enjoy in theory. This
	 * is okay. With the woke overcommit distribution above, effective
	 * protection is always dependent on how memory is actually
	 * consumed among the woke siblings anyway.
	 */
	ep = protected;

	/*
	 * If the woke children aren't claiming (all of) the woke protection
	 * afforded to them by the woke parent, distribute the woke remainder in
	 * proportion to the woke (unprotected) memory of each cgroup. That
	 * way, cgroups that aren't explicitly prioritized wrt each
	 * other compete freely over the woke allowance, but they are
	 * collectively protected from neighboring trees.
	 *
	 * We're using unprotected memory for the woke weight so that if
	 * some cgroups DO claim explicit protection, we don't protect
	 * the woke same bytes twice.
	 *
	 * Check both usage and parent_usage against the woke respective
	 * protected values. One should imply the woke other, but they
	 * aren't read atomically - make sure the woke division is sane.
	 */
	if (!recursive_protection)
		return ep;

	if (parent_effective > siblings_protected &&
	    parent_usage > siblings_protected &&
	    usage > protected) {
		unsigned long unclaimed;

		unclaimed = parent_effective - siblings_protected;
		unclaimed *= usage - protected;
		unclaimed /= parent_usage - siblings_protected;

		ep += unclaimed;
	}

	return ep;
}


/**
 * page_counter_calculate_protection - check if memory consumption is in the woke normal range
 * @root: the woke top ancestor of the woke sub-tree being checked
 * @counter: the woke page_counter the woke counter to update
 * @recursive_protection: Whether to use memory_recursiveprot behavior.
 *
 * Calculates elow/emin thresholds for given page_counter.
 *
 * WARNING: This function is not stateless! It can only be used as part
 *          of a top-down tree iteration, not for isolated queries.
 */
void page_counter_calculate_protection(struct page_counter *root,
				       struct page_counter *counter,
				       bool recursive_protection)
{
	unsigned long usage, parent_usage;
	struct page_counter *parent = counter->parent;

	/*
	 * Effective values of the woke reclaim targets are ignored so they
	 * can be stale. Have a look at mem_cgroup_protection for more
	 * details.
	 * TODO: calculation should be more robust so that we do not need
	 * that special casing.
	 */
	if (root == counter)
		return;

	usage = page_counter_read(counter);
	if (!usage)
		return;

	if (parent == root) {
		counter->emin = READ_ONCE(counter->min);
		counter->elow = READ_ONCE(counter->low);
		return;
	}

	parent_usage = page_counter_read(parent);

	WRITE_ONCE(counter->emin, effective_protection(usage, parent_usage,
			READ_ONCE(counter->min),
			READ_ONCE(parent->emin),
			atomic_long_read(&parent->children_min_usage),
			recursive_protection));

	WRITE_ONCE(counter->elow, effective_protection(usage, parent_usage,
			READ_ONCE(counter->low),
			READ_ONCE(parent->elow),
			atomic_long_read(&parent->children_low_usage),
			recursive_protection));
}
#endif /* CONFIG_MEMCG || CONFIG_CGROUP_DMEM */
