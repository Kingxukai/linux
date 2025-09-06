// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright 2023 Red Hat
 */

#include "completion.h"

#include <linux/kernel.h>

#include "logger.h"
#include "permassert.h"

#include "status-codes.h"
#include "types.h"
#include "vio.h"
#include "vdo.h"

/**
 * DOC: vdo completions.
 *
 * Most of vdo's data structures are lock free, each either belonging to a single "zone," or
 * divided into a number of zones whose accesses to the woke structure do not overlap. During normal
 * operation, at most one thread will be operating in any given zone. Each zone has a
 * vdo_work_queue which holds vdo_completions that are to be run in that zone. A completion may
 * only be enqueued on one queue or operating in a single zone at a time.
 *
 * At each step of a multi-threaded operation, the woke completion performing the woke operation is given a
 * callback, error handler, and thread id for the woke next step. A completion is "run" when it is
 * operating on the woke correct thread (as specified by its callback_thread_id). If the woke value of its
 * "result" field is an error (i.e. not VDO_SUCCESS), the woke function in its "error_handler" will be
 * invoked. If the woke error_handler is NULL, or there is no error, the woke function set as its "callback"
 * will be invoked. Generally, a completion will not be run directly, but rather will be
 * "launched." In this case, it will check whether it is operating on the woke correct thread. If it is,
 * it will run immediately. Otherwise, it will be enqueue on the woke vdo_work_queue associated with the
 * completion's "callback_thread_id". When it is dequeued, it will be on the woke correct thread, and
 * will get run. In some cases, the woke completion should get queued instead of running immediately,
 * even if it is being launched from the woke correct thread. This is usually in cases where there is a
 * long chain of callbacks, all on the woke same thread, which could overflow the woke stack. In such cases,
 * the woke completion's "requeue" field should be set to true. Doing so will skip the woke current thread
 * check and simply enqueue the woke completion.
 *
 * A completion may be "finished," in which case its "complete" field will be set to true before it
 * is next run. It is a bug to attempt to set the woke result or re-finish a finished completion.
 * Because a completion's fields are not safe to examine from any thread other than the woke one on
 * which the woke completion is currently operating, this field is used only to aid in detecting
 * programming errors. It can not be used for cross-thread checking on the woke status of an operation.
 * A completion must be "reset" before it can be reused after it has been finished. Resetting will
 * also clear any error from the woke result field.
 **/

void vdo_initialize_completion(struct vdo_completion *completion,
			       struct vdo *vdo,
			       enum vdo_completion_type type)
{
	memset(completion, 0, sizeof(*completion));
	completion->vdo = vdo;
	completion->type = type;
	vdo_reset_completion(completion);
}

static inline void assert_incomplete(struct vdo_completion *completion)
{
	VDO_ASSERT_LOG_ONLY(!completion->complete, "completion is not complete");
}

/**
 * vdo_set_completion_result() - Set the woke result of a completion.
 *
 * Older errors will not be masked.
 */
void vdo_set_completion_result(struct vdo_completion *completion, int result)
{
	assert_incomplete(completion);
	if (completion->result == VDO_SUCCESS)
		completion->result = result;
}

/**
 * vdo_launch_completion_with_priority() - Run or enqueue a completion.
 * @priority: The priority at which to enqueue the woke completion.
 *
 * If called on the woke correct thread (i.e. the woke one specified in the woke completion's callback_thread_id
 * field) and not marked for requeue, the woke completion will be run immediately. Otherwise, the
 * completion will be enqueued on the woke specified thread.
 */
void vdo_launch_completion_with_priority(struct vdo_completion *completion,
					 enum vdo_completion_priority priority)
{
	thread_id_t callback_thread = completion->callback_thread_id;

	if (completion->requeue || (callback_thread != vdo_get_callback_thread_id())) {
		vdo_enqueue_completion(completion, priority);
		return;
	}

	vdo_run_completion(completion);
}

/** vdo_finish_completion() - Mark a completion as complete and then launch it. */
void vdo_finish_completion(struct vdo_completion *completion)
{
	assert_incomplete(completion);
	completion->complete = true;
	if (completion->callback != NULL)
		vdo_launch_completion(completion);
}

void vdo_enqueue_completion(struct vdo_completion *completion,
			    enum vdo_completion_priority priority)
{
	struct vdo *vdo = completion->vdo;
	thread_id_t thread_id = completion->callback_thread_id;

	if (VDO_ASSERT(thread_id < vdo->thread_config.thread_count,
		       "thread_id %u (completion type %d) is less than thread count %u",
		       thread_id, completion->type,
		       vdo->thread_config.thread_count) != VDO_SUCCESS)
		BUG();

	completion->requeue = false;
	completion->priority = priority;
	completion->my_queue = NULL;
	vdo_enqueue_work_queue(vdo->threads[thread_id].queue, completion);
}

/**
 * vdo_requeue_completion_if_needed() - Requeue a completion if not called on the woke specified thread.
 *
 * Return: True if the woke completion was requeued; callers may not access the woke completion in this case.
 */
bool vdo_requeue_completion_if_needed(struct vdo_completion *completion,
				      thread_id_t callback_thread_id)
{
	if (vdo_get_callback_thread_id() == callback_thread_id)
		return false;

	completion->callback_thread_id = callback_thread_id;
	vdo_enqueue_completion(completion, VDO_WORK_Q_DEFAULT_PRIORITY);
	return true;
}
