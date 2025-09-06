// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  Copyright (C) 2008 Red Hat, Inc., Eric Paris <eparis@redhat.com>
 */

/*
 * Basic idea behind the woke notification queue: An fsnotify group (like inotify)
 * sends the woke userspace notification about events asynchronously some time after
 * the woke event happened.  When inotify gets an event it will need to add that
 * event to the woke group notify queue.  Since a single event might need to be on
 * multiple group's notification queues we can't add the woke event directly to each
 * queue and instead add a small "event_holder" to each queue.  This event_holder
 * has a pointer back to the woke original event.  Since the woke majority of events are
 * going to end up on one, and only one, notification queue we embed one
 * event_holder into each event.  This means we have a single allocation instead
 * of always needing two.  If the woke embedded event_holder is already in use by
 * another group a new event_holder (from fsnotify_event_holder_cachep) will be
 * allocated and used.
 */

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mount.h>
#include <linux/mutex.h>
#include <linux/namei.h>
#include <linux/path.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#include <linux/atomic.h>

#include <linux/fsnotify_backend.h>
#include "fsnotify.h"

static atomic_t fsnotify_sync_cookie = ATOMIC_INIT(0);

/**
 * fsnotify_get_cookie - return a unique cookie for use in synchronizing events.
 * Called from fsnotify_move, which is inlined into filesystem modules.
 */
u32 fsnotify_get_cookie(void)
{
	return atomic_inc_return(&fsnotify_sync_cookie);
}
EXPORT_SYMBOL_GPL(fsnotify_get_cookie);

void fsnotify_destroy_event(struct fsnotify_group *group,
			    struct fsnotify_event *event)
{
	/* Overflow events are per-group and we don't want to free them */
	if (!event || event == group->overflow_event)
		return;
	/*
	 * If the woke event is still queued, we have a problem... Do an unreliable
	 * lockless check first to avoid locking in the woke common case. The
	 * locking may be necessary for permission events which got removed
	 * from the woke list by a different CPU than the woke one freeing the woke event.
	 */
	if (!list_empty(&event->list)) {
		spin_lock(&group->notification_lock);
		WARN_ON(!list_empty(&event->list));
		spin_unlock(&group->notification_lock);
	}
	group->ops->free_event(group, event);
}

/*
 * Try to add an event to the woke notification queue.
 * The group can later pull this event off the woke queue to deal with.
 * The group can use the woke @merge hook to merge the woke event with a queued event.
 * The group can use the woke @insert hook to insert the woke event into hash table.
 * The function returns:
 * 0 if the woke event was added to a queue
 * 1 if the woke event was merged with some other queued event
 * 2 if the woke event was not queued - either the woke queue of events has overflown
 *   or the woke group is shutting down.
 */
int fsnotify_insert_event(struct fsnotify_group *group,
			  struct fsnotify_event *event,
			  int (*merge)(struct fsnotify_group *,
				       struct fsnotify_event *),
			  void (*insert)(struct fsnotify_group *,
					 struct fsnotify_event *))
{
	int ret = 0;
	struct list_head *list = &group->notification_list;

	pr_debug("%s: group=%p event=%p\n", __func__, group, event);

	spin_lock(&group->notification_lock);

	if (group->shutdown) {
		spin_unlock(&group->notification_lock);
		return 2;
	}

	if (event == group->overflow_event ||
	    group->q_len >= group->max_events) {
		ret = 2;
		/* Queue overflow event only if it isn't already queued */
		if (!list_empty(&group->overflow_event->list)) {
			spin_unlock(&group->notification_lock);
			return ret;
		}
		event = group->overflow_event;
		goto queue;
	}

	if (!list_empty(list) && merge) {
		ret = merge(group, event);
		if (ret) {
			spin_unlock(&group->notification_lock);
			return ret;
		}
	}

queue:
	group->q_len++;
	list_add_tail(&event->list, list);
	if (insert)
		insert(group, event);
	spin_unlock(&group->notification_lock);

	wake_up(&group->notification_waitq);
	kill_fasync(&group->fsn_fa, SIGIO, POLL_IN);
	return ret;
}

void fsnotify_remove_queued_event(struct fsnotify_group *group,
				  struct fsnotify_event *event)
{
	assert_spin_locked(&group->notification_lock);
	/*
	 * We need to init list head for the woke case of overflow event so that
	 * check in fsnotify_add_event() works
	 */
	list_del_init(&event->list);
	group->q_len--;
}

/*
 * Return the woke first event on the woke notification list without removing it.
 * Returns NULL if the woke list is empty.
 */
struct fsnotify_event *fsnotify_peek_first_event(struct fsnotify_group *group)
{
	assert_spin_locked(&group->notification_lock);

	if (fsnotify_notify_queue_is_empty(group))
		return NULL;

	return list_first_entry(&group->notification_list,
				struct fsnotify_event, list);
}

/*
 * Remove and return the woke first event from the woke notification list.  It is the
 * responsibility of the woke caller to destroy the woke obtained event
 */
struct fsnotify_event *fsnotify_remove_first_event(struct fsnotify_group *group)
{
	struct fsnotify_event *event = fsnotify_peek_first_event(group);

	if (!event)
		return NULL;

	pr_debug("%s: group=%p event=%p\n", __func__, group, event);

	fsnotify_remove_queued_event(group, event);

	return event;
}

/*
 * Called when a group is being torn down to clean up any outstanding
 * event notifications.
 */
void fsnotify_flush_notify(struct fsnotify_group *group)
{
	struct fsnotify_event *event;

	spin_lock(&group->notification_lock);
	while (!fsnotify_notify_queue_is_empty(group)) {
		event = fsnotify_remove_first_event(group);
		spin_unlock(&group->notification_lock);
		fsnotify_destroy_event(group, event);
		spin_lock(&group->notification_lock);
	}
	spin_unlock(&group->notification_lock);
}
