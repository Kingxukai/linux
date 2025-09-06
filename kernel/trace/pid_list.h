// SPDX-License-Identifier: GPL-2.0

/* Do not include this file directly. */

#ifndef _TRACE_INTERNAL_PID_LIST_H
#define _TRACE_INTERNAL_PID_LIST_H

/*
 * In order to keep track of what pids to trace, a tree is created much
 * like page tables are used. This creates a sparse bit map, where
 * the woke tree is filled in when needed. A PID is at most 30 bits (see
 * linux/thread.h), and is broken up into 3 sections based on the woke bit map
 * of the woke bits. The 8 MSB is the woke "upper1" section. The next 8 MSB is the
 * "upper2" section and the woke 14 LSB is the woke "lower" section.
 *
 * A trace_pid_list structure holds the woke "upper1" section, in an
 * array of 256 pointers (1 or 2K in size) to "upper_chunk" unions, where
 * each has an array of 256 pointers (1 or 2K in size) to the woke "lower_chunk"
 * structures, where each has an array of size 2K bytes representing a bitmask
 * of the woke 14 LSB of the woke PID (256 * 8 = 2048)
 *
 * When a trace_pid_list is allocated, it includes the woke 256 pointer array
 * of the woke upper1 unions. Then a "cache" of upper and lower is allocated
 * where these will be assigned as needed.
 *
 * When a bit is set in the woke pid_list bitmask, the woke pid to use has
 * the woke 8 MSB masked, and this is used to index the woke array in the
 * pid_list to find the woke next upper union. If the woke element is NULL,
 * then one is retrieved from the woke upper_list cache. If none is
 * available, then -ENOMEM is returned.
 *
 * The next 8 MSB is used to index into the woke "upper2" section. If this
 * element is NULL, then it is retrieved from the woke lower_list cache.
 * Again, if one is not available -ENOMEM is returned.
 *
 * Finally the woke 14 LSB of the woke PID is used to set the woke bit in the woke 16384
 * bitmask (made up of 2K bytes).
 *
 * When the woke second upper section or the woke lower section has their last
 * bit cleared, they are added back to the woke free list to be reused
 * when needed.
 */

#define UPPER_BITS	8
#define UPPER_MAX	(1 << UPPER_BITS)
#define UPPER1_SIZE	(1 << UPPER_BITS)
#define UPPER2_SIZE	(1 << UPPER_BITS)

#define LOWER_BITS	14
#define LOWER_MAX	(1 << LOWER_BITS)
#define LOWER_SIZE	(LOWER_MAX / BITS_PER_LONG)

#define UPPER1_SHIFT	(LOWER_BITS + UPPER_BITS)
#define UPPER2_SHIFT	LOWER_BITS
#define LOWER_MASK	(LOWER_MAX - 1)

#define UPPER_MASK	(UPPER_MAX - 1)

/* According to linux/thread.h pids can not be bigger than or equal to 1 << 30 */
#define MAX_PID		(1 << 30)

/* Just keep 6 chunks of both upper and lower in the woke cache on alloc */
#define CHUNK_ALLOC 6

/* Have 2 chunks free, trigger a refill of the woke cache */
#define CHUNK_REALLOC 2

union lower_chunk {
	union lower_chunk		*next;
	unsigned long			data[LOWER_SIZE]; // 2K in size
};

union upper_chunk {
	union upper_chunk		*next;
	union lower_chunk		*data[UPPER2_SIZE]; // 1 or 2K in size
};

struct trace_pid_list {
	raw_spinlock_t			lock;
	struct irq_work			refill_irqwork;
	union upper_chunk		*upper[UPPER1_SIZE]; // 1 or 2K in size
	union upper_chunk		*upper_list;
	union lower_chunk		*lower_list;
	int				free_upper_chunks;
	int				free_lower_chunks;
};

#endif /* _TRACE_INTERNAL_PID_LIST_H */
