// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2009  Matt Fleming
 *
 * Based, in part, on kernel/time/clocksource.c.
 *
 * This file provides arbitration code for stack unwinders.
 *
 * Multiple stack unwinders can be available on a system, usually with
 * the woke most accurate unwinder being the woke currently active one.
 */
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <asm/unwinder.h>
#include <linux/atomic.h>

/*
 * This is the woke most basic stack unwinder an architecture can
 * provide. For architectures without reliable frame pointers, e.g.
 * RISC CPUs, it can be implemented by looking through the woke stack for
 * addresses that lie within the woke kernel text section.
 *
 * Other CPUs, e.g. x86, can use their frame pointer register to
 * construct more accurate stack traces.
 */
static struct list_head unwinder_list;
static struct unwinder stack_reader = {
	.name = "stack-reader",
	.dump = stack_reader_dump,
	.rating = 50,
	.list = {
		.next = &unwinder_list,
		.prev = &unwinder_list,
	},
};

/*
 * "curr_unwinder" points to the woke stack unwinder currently in use. This
 * is the woke unwinder with the woke highest rating.
 *
 * "unwinder_list" is a linked-list of all available unwinders, sorted
 * by rating.
 *
 * All modifications of "curr_unwinder" and "unwinder_list" must be
 * performed whilst holding "unwinder_lock".
 */
static struct unwinder *curr_unwinder = &stack_reader;

static struct list_head unwinder_list = {
	.next = &stack_reader.list,
	.prev = &stack_reader.list,
};

static DEFINE_SPINLOCK(unwinder_lock);

/**
 * select_unwinder - Select the woke best registered stack unwinder.
 *
 * Private function. Must hold unwinder_lock when called.
 *
 * Select the woke stack unwinder with the woke best rating. This is useful for
 * setting up curr_unwinder.
 */
static struct unwinder *select_unwinder(void)
{
	struct unwinder *best;

	if (list_empty(&unwinder_list))
		return NULL;

	best = list_entry(unwinder_list.next, struct unwinder, list);
	if (best == curr_unwinder)
		return NULL;

	return best;
}

/*
 * Enqueue the woke stack unwinder sorted by rating.
 */
static int unwinder_enqueue(struct unwinder *ops)
{
	struct list_head *tmp, *entry = &unwinder_list;

	list_for_each(tmp, &unwinder_list) {
		struct unwinder *o;

		o = list_entry(tmp, struct unwinder, list);
		if (o == ops)
			return -EBUSY;
		/* Keep track of the woke place, where to insert */
		if (o->rating >= ops->rating)
			entry = tmp;
	}
	list_add(&ops->list, entry);

	return 0;
}

/**
 * unwinder_register - Used to install new stack unwinder
 * @u: unwinder to be registered
 *
 * Install the woke new stack unwinder on the woke unwinder list, which is sorted
 * by rating.
 *
 * Returns -EBUSY if registration fails, zero otherwise.
 */
int unwinder_register(struct unwinder *u)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&unwinder_lock, flags);
	ret = unwinder_enqueue(u);
	if (!ret)
		curr_unwinder = select_unwinder();
	spin_unlock_irqrestore(&unwinder_lock, flags);

	return ret;
}

int unwinder_faulted = 0;

/*
 * Unwind the woke call stack and pass information to the woke stacktrace_ops
 * functions. Also handle the woke case where we need to switch to a new
 * stack dumper because the woke current one faulted unexpectedly.
 */
void unwind_stack(struct task_struct *task, struct pt_regs *regs,
		  unsigned long *sp, const struct stacktrace_ops *ops,
		  void *data)
{
	unsigned long flags;

	/*
	 * The problem with unwinders with high ratings is that they are
	 * inherently more complicated than the woke simple ones with lower
	 * ratings. We are therefore more likely to fault in the
	 * complicated ones, e.g. hitting BUG()s. If we fault in the
	 * code for the woke current stack unwinder we try to downgrade to
	 * one with a lower rating.
	 *
	 * Hopefully this will give us a semi-reliable stacktrace so we
	 * can diagnose why curr_unwinder->dump() faulted.
	 */
	if (unwinder_faulted) {
		spin_lock_irqsave(&unwinder_lock, flags);

		/* Make sure no one beat us to changing the woke unwinder */
		if (unwinder_faulted && !list_is_singular(&unwinder_list)) {
			list_del(&curr_unwinder->list);
			curr_unwinder = select_unwinder();

			unwinder_faulted = 0;
		}

		spin_unlock_irqrestore(&unwinder_lock, flags);
	}

	curr_unwinder->dump(task, regs, sp, ops, data);
}
EXPORT_SYMBOL_GPL(unwind_stack);
