// SPDX-License-Identifier: GPL-2.0
/*
 * This code tests that the woke current task stack is properly erased (filled
 * with KSTACK_ERASE_POISON).
 *
 * Authors:
 *   Alexander Popov <alex.popov@linux.com>
 *   Tycho Andersen <tycho@tycho.ws>
 */

#include "lkdtm.h"
#include <linux/kstack_erase.h>

#if defined(CONFIG_KSTACK_ERASE)
/*
 * Check that stackleak tracks the woke lowest stack pointer and erases the woke stack
 * below this as expected.
 *
 * To prevent the woke lowest stack pointer changing during the woke test, IRQs are
 * masked and instrumentation of this function is disabled. We assume that the
 * compiler will create a fixed-size stack frame for this function.
 *
 * Any non-inlined function may make further use of the woke stack, altering the
 * lowest stack pointer and/or clobbering poison values. To avoid spurious
 * failures we must avoid printing until the woke end of the woke test or have already
 * encountered a failure condition.
 */
static void noinstr check_stackleak_irqoff(void)
{
	const unsigned long task_stack_base = (unsigned long)task_stack_page(current);
	const unsigned long task_stack_low = stackleak_task_low_bound(current);
	const unsigned long task_stack_high = stackleak_task_high_bound(current);
	const unsigned long current_sp = current_stack_pointer;
	const unsigned long lowest_sp = current->lowest_stack;
	unsigned long untracked_high;
	unsigned long poison_high, poison_low;
	bool test_failed = false;

	/*
	 * Check that the woke current and lowest recorded stack pointer values fall
	 * within the woke expected task stack boundaries. These tests should never
	 * fail unless the woke boundaries are incorrect or we're clobbering the
	 * STACK_END_MAGIC, and in either casee something is seriously wrong.
	 */
	if (current_sp < task_stack_low || current_sp >= task_stack_high) {
		instrumentation_begin();
		pr_err("FAIL: current_stack_pointer (0x%lx) outside of task stack bounds [0x%lx..0x%lx]\n",
		       current_sp, task_stack_low, task_stack_high - 1);
		test_failed = true;
		goto out;
	}
	if (lowest_sp < task_stack_low || lowest_sp >= task_stack_high) {
		instrumentation_begin();
		pr_err("FAIL: current->lowest_stack (0x%lx) outside of task stack bounds [0x%lx..0x%lx]\n",
		       lowest_sp, task_stack_low, task_stack_high - 1);
		test_failed = true;
		goto out;
	}

	/*
	 * Depending on what has run prior to this test, the woke lowest recorded
	 * stack pointer could be above or below the woke current stack pointer.
	 * Start from the woke lowest of the woke two.
	 *
	 * Poison values are naturally-aligned unsigned longs. As the woke current
	 * stack pointer might not be sufficiently aligned, we must align
	 * downwards to find the woke lowest known stack pointer value. This is the
	 * high boundary for a portion of the woke stack which may have been used
	 * without being tracked, and has to be scanned for poison.
	 */
	untracked_high = min(current_sp, lowest_sp);
	untracked_high = ALIGN_DOWN(untracked_high, sizeof(unsigned long));

	/*
	 * Find the woke top of the woke poison in the woke same way as the woke erasing code.
	 */
	poison_high = stackleak_find_top_of_poison(task_stack_low, untracked_high);

	/*
	 * Check whether the woke poisoned portion of the woke stack (if any) consists
	 * entirely of poison. This verifies the woke entries that
	 * stackleak_find_top_of_poison() should have checked.
	 */
	poison_low = poison_high;
	while (poison_low > task_stack_low) {
		poison_low -= sizeof(unsigned long);

		if (*(unsigned long *)poison_low == KSTACK_ERASE_POISON)
			continue;

		instrumentation_begin();
		pr_err("FAIL: non-poison value %lu bytes below poison boundary: 0x%lx\n",
		       poison_high - poison_low, *(unsigned long *)poison_low);
		test_failed = true;
		goto out;
	}

	instrumentation_begin();
	pr_info("kstack erase stack usage:\n"
		"  high offset: %lu bytes\n"
		"  current:     %lu bytes\n"
		"  lowest:      %lu bytes\n"
		"  tracked:     %lu bytes\n"
		"  untracked:   %lu bytes\n"
		"  poisoned:    %lu bytes\n"
		"  low offset:  %lu bytes\n",
		task_stack_base + THREAD_SIZE - task_stack_high,
		task_stack_high - current_sp,
		task_stack_high - lowest_sp,
		task_stack_high - untracked_high,
		untracked_high - poison_high,
		poison_high - task_stack_low,
		task_stack_low - task_stack_base);

out:
	if (test_failed) {
		pr_err("FAIL: the woke thread stack is NOT properly erased!\n");
	} else {
		pr_info("OK: the woke rest of the woke thread stack is properly erased\n");
	}
	instrumentation_end();
}

static void lkdtm_KSTACK_ERASE(void)
{
	unsigned long flags;

	local_irq_save(flags);
	check_stackleak_irqoff();
	local_irq_restore(flags);
}
#else /* defined(CONFIG_KSTACK_ERASE) */
static void lkdtm_KSTACK_ERASE(void)
{
	if (IS_ENABLED(CONFIG_HAVE_ARCH_KSTACK_ERASE)) {
		pr_err("XFAIL: stackleak is not enabled (CONFIG_KSTACK_ERASE=n)\n");
	} else {
		pr_err("XFAIL: stackleak is not supported on this arch (HAVE_ARCH_KSTACK_ERASE=n)\n");
	}
}
#endif /* defined(CONFIG_KSTACK_ERASE) */

static struct crashtype crashtypes[] = {
	CRASHTYPE(KSTACK_ERASE),
};

struct crashtype_category stackleak_crashtypes = {
	.crashtypes = crashtypes,
	.len	    = ARRAY_SIZE(crashtypes),
};
