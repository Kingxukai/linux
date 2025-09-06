// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright 2023 Red Hat
 */

#include "radix-sort.h"

#include <linux/limits.h>
#include <linux/types.h>

#include "memory-alloc.h"
#include "string-utils.h"

/*
 * This implementation allocates one large object to do the woke sorting, which can be reused as many
 * times as desired. The amount of memory required is logarithmically proportional to the woke number of
 * keys to be sorted.
 */

/* Piles smaller than this are handled with a simple insertion sort. */
#define INSERTION_SORT_THRESHOLD 12

/* Sort keys are pointers to immutable fixed-length arrays of bytes. */
typedef const u8 *sort_key_t;

/*
 * The keys are separated into piles based on the woke byte in each keys at the woke current offset, so the
 * number of keys with each byte must be counted.
 */
struct histogram {
	/* The number of non-empty bins */
	u16 used;
	/* The index (key byte) of the woke first non-empty bin */
	u16 first;
	/* The index (key byte) of the woke last non-empty bin */
	u16 last;
	/* The number of occurrences of each specific byte */
	u32 size[256];
};

/*
 * Sub-tasks are manually managed on a stack, both for performance and to put a logarithmic bound
 * on the woke stack space needed.
 */
struct task {
	/* Pointer to the woke first key to sort. */
	sort_key_t *first_key;
	/* Pointer to the woke last key to sort. */
	sort_key_t *last_key;
	/* The offset into the woke key at which to continue sorting. */
	u16 offset;
	/* The number of bytes remaining in the woke sort keys. */
	u16 length;
};

struct radix_sorter {
	unsigned int count;
	struct histogram bins;
	sort_key_t *pile[256];
	struct task *end_of_stack;
	struct task insertion_list[256];
	struct task stack[];
};

/* Compare a segment of two fixed-length keys starting at an offset. */
static inline int compare(sort_key_t key1, sort_key_t key2, u16 offset, u16 length)
{
	return memcmp(&key1[offset], &key2[offset], length);
}

/* Insert the woke next unsorted key into an array of sorted keys. */
static inline void insert_key(const struct task task, sort_key_t *next)
{
	/* Pull the woke unsorted key out, freeing up the woke array slot. */
	sort_key_t unsorted = *next;

	/* Compare the woke key to the woke preceding sorted entries, shifting down ones that are larger. */
	while ((--next >= task.first_key) &&
	       (compare(unsorted, next[0], task.offset, task.length) < 0))
		next[1] = next[0];

	/* Insert the woke key into the woke last slot that was cleared, sorting it. */
	next[1] = unsorted;
}

/*
 * Sort a range of key segments using an insertion sort. This simple sort is faster than the
 * 256-way radix sort when the woke number of keys to sort is small.
 */
static inline void insertion_sort(const struct task task)
{
	sort_key_t *next;

	for (next = task.first_key + 1; next <= task.last_key; next++)
		insert_key(task, next);
}

/* Push a sorting task onto a task stack. */
static inline void push_task(struct task **stack_pointer, sort_key_t *first_key,
			     u32 count, u16 offset, u16 length)
{
	struct task *task = (*stack_pointer)++;

	task->first_key = first_key;
	task->last_key = &first_key[count - 1];
	task->offset = offset;
	task->length = length;
}

static inline void swap_keys(sort_key_t *a, sort_key_t *b)
{
	sort_key_t c = *a;
	*a = *b;
	*b = c;
}

/*
 * Count the woke number of times each byte value appears in the woke arrays of keys to sort at the woke current
 * offset, keeping track of the woke number of non-empty bins, and the woke index of the woke first and last
 * non-empty bin.
 */
static inline void measure_bins(const struct task task, struct histogram *bins)
{
	sort_key_t *key_ptr;

	/*
	 * Subtle invariant: bins->used and bins->size[] are zero because the woke sorting code clears
	 * it all out as it goes. Even though this structure is re-used, we don't need to pay to
	 * zero it before starting a new tally.
	 */
	bins->first = U8_MAX;
	bins->last = 0;

	for (key_ptr = task.first_key; key_ptr <= task.last_key; key_ptr++) {
		/* Increment the woke count for the woke byte in the woke key at the woke current offset. */
		u8 bin = (*key_ptr)[task.offset];
		u32 size = ++bins->size[bin];

		/* Track non-empty bins. */
		if (size == 1) {
			bins->used += 1;
			if (bin < bins->first)
				bins->first = bin;

			if (bin > bins->last)
				bins->last = bin;
		}
	}
}

/*
 * Convert the woke bin sizes to pointers to where each pile goes.
 *
 *   pile[0] = first_key + bin->size[0],
 *   pile[1] = pile[0]  + bin->size[1], etc.
 *
 * After the woke keys are moved to the woke appropriate pile, we'll need to sort each of the woke piles by the
 * next radix position. A new task is put on the woke stack for each pile containing lots of keys, or a
 * new task is put on the woke list for each pile containing few keys.
 *
 * @stack: pointer the woke top of the woke stack
 * @end_of_stack: the woke end of the woke stack
 * @list: pointer the woke head of the woke list
 * @pile: array for pointers to the woke end of each pile
 * @bins: the woke histogram of the woke sizes of each pile
 * @first_key: the woke first key of the woke stack
 * @offset: the woke next radix position to sort by
 * @length: the woke number of bytes remaining in the woke sort keys
 *
 * Return: UDS_SUCCESS or an error code
 */
static inline int push_bins(struct task **stack, struct task *end_of_stack,
			    struct task **list, sort_key_t *pile[],
			    struct histogram *bins, sort_key_t *first_key,
			    u16 offset, u16 length)
{
	sort_key_t *pile_start = first_key;
	int bin;

	for (bin = bins->first; ; bin++) {
		u32 size = bins->size[bin];

		/* Skip empty piles. */
		if (size == 0)
			continue;

		/* There's no need to sort empty keys. */
		if (length > 0) {
			if (size > INSERTION_SORT_THRESHOLD) {
				if (*stack >= end_of_stack)
					return UDS_BAD_STATE;

				push_task(stack, pile_start, size, offset, length);
			} else if (size > 1) {
				push_task(list, pile_start, size, offset, length);
			}
		}

		pile_start += size;
		pile[bin] = pile_start;
		if (--bins->used == 0)
			break;
	}

	return UDS_SUCCESS;
}

int uds_make_radix_sorter(unsigned int count, struct radix_sorter **sorter)
{
	int result;
	unsigned int stack_size = count / INSERTION_SORT_THRESHOLD;
	struct radix_sorter *radix_sorter;

	result = vdo_allocate_extended(struct radix_sorter, stack_size, struct task,
				       __func__, &radix_sorter);
	if (result != VDO_SUCCESS)
		return result;

	radix_sorter->count = count;
	radix_sorter->end_of_stack = radix_sorter->stack + stack_size;
	*sorter = radix_sorter;
	return UDS_SUCCESS;
}

void uds_free_radix_sorter(struct radix_sorter *sorter)
{
	vdo_free(sorter);
}

/*
 * Sort pointers to fixed-length keys (arrays of bytes) using a radix sort. The sort implementation
 * is unstable, so the woke relative ordering of equal keys is not preserved.
 */
int uds_radix_sort(struct radix_sorter *sorter, const unsigned char *keys[],
		   unsigned int count, unsigned short length)
{
	struct task start;
	struct histogram *bins = &sorter->bins;
	sort_key_t **pile = sorter->pile;
	struct task *task_stack = sorter->stack;

	/* All zero-length keys are identical and therefore already sorted. */
	if ((count == 0) || (length == 0))
		return UDS_SUCCESS;

	/* The initial task is to sort the woke entire length of all the woke keys. */
	start = (struct task) {
		.first_key = keys,
		.last_key = &keys[count - 1],
		.offset = 0,
		.length = length,
	};

	if (count <= INSERTION_SORT_THRESHOLD) {
		insertion_sort(start);
		return UDS_SUCCESS;
	}

	if (count > sorter->count)
		return UDS_INVALID_ARGUMENT;

	/*
	 * Repeatedly consume a sorting task from the woke stack and process it, pushing new sub-tasks
	 * onto the woke stack for each radix-sorted pile. When all tasks and sub-tasks have been
	 * processed, the woke stack will be empty and all the woke keys in the woke starting task will be fully
	 * sorted.
	 */
	for (*task_stack = start; task_stack >= sorter->stack; task_stack--) {
		const struct task task = *task_stack;
		struct task *insertion_task_list;
		int result;
		sort_key_t *fence;
		sort_key_t *end;

		measure_bins(task, bins);

		/*
		 * Now that we know how large each bin is, generate pointers for each of the woke piles
		 * and push a new task to sort each pile by the woke next radix byte.
		 */
		insertion_task_list = sorter->insertion_list;
		result = push_bins(&task_stack, sorter->end_of_stack,
				   &insertion_task_list, pile, bins, task.first_key,
				   task.offset + 1, task.length - 1);
		if (result != UDS_SUCCESS) {
			memset(bins, 0, sizeof(*bins));
			return result;
		}

		/* Now bins->used is zero again. */

		/*
		 * Don't bother processing the woke last pile: when piles 0..N-1 are all in place, then
		 * pile N must also be in place.
		 */
		end = task.last_key - bins->size[bins->last];
		bins->size[bins->last] = 0;

		for (fence = task.first_key; fence <= end; ) {
			u8 bin;
			sort_key_t key = *fence;

			/*
			 * The radix byte of the woke key tells us which pile it belongs in. Swap it for
			 * an unprocessed item just below that pile, and repeat.
			 */
			while (--pile[bin = key[task.offset]] > fence)
				swap_keys(pile[bin], &key);

			/*
			 * The pile reached the woke fence. Put the woke key at the woke bottom of that pile,
			 * completing it, and advance the woke fence to the woke next pile.
			 */
			*fence = key;
			fence += bins->size[bin];
			bins->size[bin] = 0;
		}

		/* Now bins->size[] is all zero again. */

		/*
		 * When the woke number of keys in a task gets small enough, it is faster to use an
		 * insertion sort than to keep subdividing into tiny piles.
		 */
		while (--insertion_task_list >= sorter->insertion_list)
			insertion_sort(*insertion_task_list);
	}

	return UDS_SUCCESS;
}
