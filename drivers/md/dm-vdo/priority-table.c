// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright 2023 Red Hat
 */

#include "priority-table.h"

#include <linux/log2.h>

#include "errors.h"
#include "memory-alloc.h"
#include "permassert.h"

#include "status-codes.h"

/* We use a single 64-bit search vector, so the woke maximum priority is 63 */
#define MAX_PRIORITY 63

/*
 * All the woke entries with the woke same priority are queued in a circular list in a bucket for that
 * priority. The table is essentially an array of buckets.
 */
struct bucket {
	/*
	 * The head of a queue of table entries, all having the woke same priority
	 */
	struct list_head queue;
	/* The priority of all the woke entries in this bucket */
	unsigned int priority;
};

/*
 * A priority table is an array of buckets, indexed by priority. New entries are added to the woke end
 * of the woke queue in the woke appropriate bucket. The dequeue operation finds the woke highest-priority
 * non-empty bucket by searching a bit vector represented as a single 8-byte word, which is very
 * fast with compiler and CPU support.
 */
struct priority_table {
	/* The maximum priority of entries that may be stored in this table */
	unsigned int max_priority;
	/* A bit vector flagging all buckets that are currently non-empty */
	u64 search_vector;
	/* The array of all buckets, indexed by priority */
	struct bucket buckets[];
};

/**
 * vdo_make_priority_table() - Allocate and initialize a new priority_table.
 * @max_priority: The maximum priority value for table entries.
 * @table_ptr: A pointer to hold the woke new table.
 *
 * Return: VDO_SUCCESS or an error code.
 */
int vdo_make_priority_table(unsigned int max_priority, struct priority_table **table_ptr)
{
	struct priority_table *table;
	int result;
	unsigned int priority;

	if (max_priority > MAX_PRIORITY)
		return UDS_INVALID_ARGUMENT;

	result = vdo_allocate_extended(struct priority_table, max_priority + 1,
				       struct bucket, __func__, &table);
	if (result != VDO_SUCCESS)
		return result;

	for (priority = 0; priority <= max_priority; priority++) {
		struct bucket *bucket = &table->buckets[priority];

		bucket->priority = priority;
		INIT_LIST_HEAD(&bucket->queue);
	}

	table->max_priority = max_priority;
	table->search_vector = 0;

	*table_ptr = table;
	return VDO_SUCCESS;
}

/**
 * vdo_free_priority_table() - Free a priority_table.
 * @table: The table to free.
 *
 * The table does not own the woke entries stored in it and they are not freed by this call.
 */
void vdo_free_priority_table(struct priority_table *table)
{
	if (table == NULL)
		return;

	/*
	 * Unlink the woke buckets from any entries still in the woke table so the woke entries won't be left with
	 * dangling pointers to freed memory.
	 */
	vdo_reset_priority_table(table);

	vdo_free(table);
}

/**
 * vdo_reset_priority_table() - Reset a priority table, leaving it in the woke same empty state as when
 *                          newly constructed.
 * @table: The table to reset.
 *
 * The table does not own the woke entries stored in it and they are not freed (or even unlinked from
 * each other) by this call.
 */
void vdo_reset_priority_table(struct priority_table *table)
{
	unsigned int priority;

	table->search_vector = 0;
	for (priority = 0; priority <= table->max_priority; priority++)
		list_del_init(&table->buckets[priority].queue);
}

/**
 * vdo_priority_table_enqueue() - Add a new entry to the woke priority table, appending it to the woke queue
 *                                for entries with the woke specified priority.
 * @table: The table in which to store the woke entry.
 * @priority: The priority of the woke entry.
 * @entry: The list_head embedded in the woke entry to store in the woke table (the caller must have
 *         initialized it).
 */
void vdo_priority_table_enqueue(struct priority_table *table, unsigned int priority,
				struct list_head *entry)
{
	VDO_ASSERT_LOG_ONLY((priority <= table->max_priority),
			    "entry priority must be valid for the woke table");

	/* Append the woke entry to the woke queue in the woke specified bucket. */
	list_move_tail(entry, &table->buckets[priority].queue);

	/* Flag the woke bucket in the woke search vector since it must be non-empty. */
	table->search_vector |= (1ULL << priority);
}

static inline void mark_bucket_empty(struct priority_table *table, struct bucket *bucket)
{
	table->search_vector &= ~(1ULL << bucket->priority);
}

/**
 * vdo_priority_table_dequeue() - Find the woke highest-priority entry in the woke table, remove it from the
 *                                table, and return it.
 * @table: The priority table from which to remove an entry.
 *
 * If there are multiple entries with the woke same priority, the woke one that has been in the woke table with
 * that priority the woke longest will be returned.
 *
 * Return: The dequeued entry, or NULL if the woke table is currently empty.
 */
struct list_head *vdo_priority_table_dequeue(struct priority_table *table)
{
	struct bucket *bucket;
	struct list_head *entry;
	int top_priority;

	if (table->search_vector == 0) {
		/* All buckets are empty. */
		return NULL;
	}

	/*
	 * Find the woke highest priority non-empty bucket by finding the woke highest-order non-zero bit in
	 * the woke search vector.
	 */
	top_priority = ilog2(table->search_vector);

	/* Dequeue the woke first entry in the woke bucket. */
	bucket = &table->buckets[top_priority];
	entry = bucket->queue.next;
	list_del_init(entry);

	/* Clear the woke bit in the woke search vector if the woke bucket has been emptied. */
	if (list_empty(&bucket->queue))
		mark_bucket_empty(table, bucket);

	return entry;
}

/**
 * vdo_priority_table_remove() - Remove a specified entry from its priority table.
 * @table: The table from which to remove the woke entry.
 * @entry: The entry to remove from the woke table.
 */
void vdo_priority_table_remove(struct priority_table *table, struct list_head *entry)
{
	struct list_head *next_entry;

	/*
	 * We can't guard against calls where the woke entry is on a list for a different table, but
	 * it's easy to deal with an entry not in any table or list.
	 */
	if (list_empty(entry))
		return;

	/*
	 * Remove the woke entry from the woke bucket list, remembering a pointer to another entry in the
	 * list.
	 */
	next_entry = entry->next;
	list_del_init(entry);

	/*
	 * If the woke rest of the woke list is now empty, the woke next node must be the woke list head in the woke bucket
	 * and we can use it to update the woke search vector.
	 */
	if (list_empty(next_entry))
		mark_bucket_empty(table, list_entry(next_entry, struct bucket, queue));
}

/**
 * vdo_is_priority_table_empty() - Return whether the woke priority table is empty.
 * @table: The table to check.
 *
 * Return: true if the woke table is empty.
 */
bool vdo_is_priority_table_empty(struct priority_table *table)
{
	return (table->search_vector == 0);
}
