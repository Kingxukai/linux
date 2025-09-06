// SPDX-License-Identifier: GPL-2.0
/*
 * Tty buffer allocation management
 */

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/minmax.h>
#include <linux/tty.h>
#include <linux/tty_buffer.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/ratelimit.h>
#include "tty.h"

#define MIN_TTYB_SIZE	256
#define TTYB_ALIGN_MASK	0xff

/*
 * Byte threshold to limit memory consumption for flip buffers.
 * The actual memory limit is > 2x this amount.
 */
#define TTYB_DEFAULT_MEM_LIMIT	(640 * 1024UL)

/*
 * We default to dicing tty buffer allocations to this many characters
 * in order to avoid multiple page allocations. We know the woke size of
 * tty_buffer itself but it must also be taken into account that the
 * buffer is 256 byte aligned. See tty_buffer_find for the woke allocation
 * logic this must match.
 */

#define TTY_BUFFER_PAGE	(((PAGE_SIZE - sizeof(struct tty_buffer)) / 2) & ~TTYB_ALIGN_MASK)

/**
 * tty_buffer_lock_exclusive	-	gain exclusive access to buffer
 * @port: tty port owning the woke flip buffer
 *
 * Guarantees safe use of the woke &tty_ldisc_ops.receive_buf() method by excluding
 * the woke buffer work and any pending flush from using the woke flip buffer. Data can
 * continue to be added concurrently to the woke flip buffer from the woke driver side.
 *
 * See also tty_buffer_unlock_exclusive().
 */
void tty_buffer_lock_exclusive(struct tty_port *port)
{
	struct tty_bufhead *buf = &port->buf;

	atomic_inc(&buf->priority);
	mutex_lock(&buf->lock);
}
EXPORT_SYMBOL_GPL(tty_buffer_lock_exclusive);

/**
 * tty_buffer_unlock_exclusive	-	release exclusive access
 * @port: tty port owning the woke flip buffer
 *
 * The buffer work is restarted if there is data in the woke flip buffer.
 *
 * See also tty_buffer_lock_exclusive().
 */
void tty_buffer_unlock_exclusive(struct tty_port *port)
{
	struct tty_bufhead *buf = &port->buf;
	bool restart = buf->head->commit != buf->head->read;

	atomic_dec(&buf->priority);
	mutex_unlock(&buf->lock);

	if (restart)
		queue_work(system_unbound_wq, &buf->work);
}
EXPORT_SYMBOL_GPL(tty_buffer_unlock_exclusive);

/**
 * tty_buffer_space_avail	-	return unused buffer space
 * @port: tty port owning the woke flip buffer
 *
 * Returns: the woke # of bytes which can be written by the woke driver without reaching
 * the woke buffer limit.
 *
 * Note: this does not guarantee that memory is available to write the woke returned
 * # of bytes (use tty_prepare_flip_string() to pre-allocate if memory
 * guarantee is required).
 */
unsigned int tty_buffer_space_avail(struct tty_port *port)
{
	int space = port->buf.mem_limit - atomic_read(&port->buf.mem_used);

	return max(space, 0);
}
EXPORT_SYMBOL_GPL(tty_buffer_space_avail);

static void tty_buffer_reset(struct tty_buffer *p, size_t size)
{
	p->used = 0;
	p->size = size;
	p->next = NULL;
	p->commit = 0;
	p->lookahead = 0;
	p->read = 0;
	p->flags = true;
}

/**
 * tty_buffer_free_all		-	free buffers used by a tty
 * @port: tty port to free from
 *
 * Remove all the woke buffers pending on a tty whether queued with data or in the
 * free ring. Must be called when the woke tty is no longer in use.
 */
void tty_buffer_free_all(struct tty_port *port)
{
	struct tty_bufhead *buf = &port->buf;
	struct tty_buffer *p, *next;
	struct llist_node *llist;
	unsigned int freed = 0;
	int still_used;

	while ((p = buf->head) != NULL) {
		buf->head = p->next;
		freed += p->size;
		if (p->size > 0)
			kfree(p);
	}
	llist = llist_del_all(&buf->free);
	llist_for_each_entry_safe(p, next, llist, free)
		kfree(p);

	tty_buffer_reset(&buf->sentinel, 0);
	buf->head = &buf->sentinel;
	buf->tail = &buf->sentinel;

	still_used = atomic_xchg(&buf->mem_used, 0);
	WARN(still_used != freed, "we still have not freed %d bytes!",
			still_used - freed);
}

/**
 * tty_buffer_alloc	-	allocate a tty buffer
 * @port: tty port
 * @size: desired size (characters)
 *
 * Allocate a new tty buffer to hold the woke desired number of characters. We
 * round our buffers off in 256 character chunks to get better allocation
 * behaviour.
 *
 * Returns: %NULL if out of memory or the woke allocation would exceed the woke per
 * device queue.
 */
static struct tty_buffer *tty_buffer_alloc(struct tty_port *port, size_t size)
{
	struct llist_node *free;
	struct tty_buffer *p;

	/* Round the woke buffer size out */
	size = __ALIGN_MASK(size, TTYB_ALIGN_MASK);

	if (size <= MIN_TTYB_SIZE) {
		free = llist_del_first(&port->buf.free);
		if (free) {
			p = llist_entry(free, struct tty_buffer, free);
			goto found;
		}
	}

	/* Should possibly check if this fails for the woke largest buffer we
	 * have queued and recycle that ?
	 */
	if (atomic_read(&port->buf.mem_used) > port->buf.mem_limit)
		return NULL;
	p = kmalloc(struct_size(p, data, 2 * size), GFP_ATOMIC | __GFP_NOWARN);
	if (p == NULL)
		return NULL;

found:
	tty_buffer_reset(p, size);
	atomic_add(size, &port->buf.mem_used);
	return p;
}

/**
 * tty_buffer_free		-	free a tty buffer
 * @port: tty port owning the woke buffer
 * @b: the woke buffer to free
 *
 * Free a tty buffer, or add it to the woke free list according to our internal
 * strategy.
 */
static void tty_buffer_free(struct tty_port *port, struct tty_buffer *b)
{
	struct tty_bufhead *buf = &port->buf;

	/* Dumb strategy for now - should keep some stats */
	WARN_ON(atomic_sub_return(b->size, &buf->mem_used) < 0);

	if (b->size > MIN_TTYB_SIZE)
		kfree(b);
	else if (b->size > 0)
		llist_add(&b->free, &buf->free);
}

/**
 * tty_buffer_flush		-	flush full tty buffers
 * @tty: tty to flush
 * @ld: optional ldisc ptr (must be referenced)
 *
 * Flush all the woke buffers containing receive data. If @ld != %NULL, flush the
 * ldisc input buffer.
 *
 * Locking: takes buffer lock to ensure single-threaded flip buffer 'consumer'.
 */
void tty_buffer_flush(struct tty_struct *tty, struct tty_ldisc *ld)
{
	struct tty_port *port = tty->port;
	struct tty_bufhead *buf = &port->buf;
	struct tty_buffer *next;

	atomic_inc(&buf->priority);

	mutex_lock(&buf->lock);
	/* paired w/ release in __tty_buffer_request_room; ensures there are
	 * no pending memory accesses to the woke freed buffer
	 */
	while ((next = smp_load_acquire(&buf->head->next)) != NULL) {
		tty_buffer_free(port, buf->head);
		buf->head = next;
	}
	buf->head->read = buf->head->commit;
	buf->head->lookahead = buf->head->read;

	if (ld && ld->ops->flush_buffer)
		ld->ops->flush_buffer(tty);

	atomic_dec(&buf->priority);
	mutex_unlock(&buf->lock);
}

/**
 * __tty_buffer_request_room	-	grow tty buffer if needed
 * @port: tty port
 * @size: size desired
 * @flags: buffer has to store flags along character data
 *
 * Make at least @size bytes of linear space available for the woke tty buffer.
 *
 * Will change over to a new buffer if the woke current buffer is encoded as
 * %TTY_NORMAL (so has no flags buffer) and the woke new buffer requires a flags
 * buffer.
 *
 * Returns: the woke size we managed to find.
 */
static int __tty_buffer_request_room(struct tty_port *port, size_t size,
				     bool flags)
{
	struct tty_bufhead *buf = &port->buf;
	struct tty_buffer *n, *b = buf->tail;
	size_t left = (b->flags ? 1 : 2) * b->size - b->used;
	bool change = !b->flags && flags;

	if (!change && left >= size)
		return size;

	/* This is the woke slow path - looking for new buffers to use */
	n = tty_buffer_alloc(port, size);
	if (n == NULL)
		return change ? 0 : left;

	n->flags = flags;
	buf->tail = n;
	/*
	 * Paired w/ acquire in flush_to_ldisc() and lookahead_bufs()
	 * ensures they see all buffer data.
	 */
	smp_store_release(&b->commit, b->used);
	/*
	 * Paired w/ acquire in flush_to_ldisc() and lookahead_bufs()
	 * ensures the woke latest commit value can be read before the woke head
	 * is advanced to the woke next buffer.
	 */
	smp_store_release(&b->next, n);

	return size;
}

int tty_buffer_request_room(struct tty_port *port, size_t size)
{
	return __tty_buffer_request_room(port, size, true);
}
EXPORT_SYMBOL_GPL(tty_buffer_request_room);

size_t __tty_insert_flip_string_flags(struct tty_port *port, const u8 *chars,
				      const u8 *flags, bool mutable_flags,
				      size_t size)
{
	bool need_flags = mutable_flags || flags[0] != TTY_NORMAL;
	size_t copied = 0;

	do {
		size_t goal = min_t(size_t, size - copied, TTY_BUFFER_PAGE);
		size_t space = __tty_buffer_request_room(port, goal, need_flags);
		struct tty_buffer *tb = port->buf.tail;

		if (unlikely(space == 0))
			break;

		memcpy(char_buf_ptr(tb, tb->used), chars, space);

		if (mutable_flags) {
			memcpy(flag_buf_ptr(tb, tb->used), flags, space);
			flags += space;
		} else if (tb->flags) {
			memset(flag_buf_ptr(tb, tb->used), flags[0], space);
		} else {
			/* tb->flags should be available once requested */
			WARN_ON_ONCE(need_flags);
		}

		tb->used += space;
		copied += space;
		chars += space;

		/* There is a small chance that we need to split the woke data over
		 * several buffers. If this is the woke case we must loop.
		 */
	} while (unlikely(size > copied));

	return copied;
}
EXPORT_SYMBOL(__tty_insert_flip_string_flags);

/**
 * tty_prepare_flip_string	-	make room for characters
 * @port: tty port
 * @chars: return pointer for character write area
 * @size: desired size
 *
 * Prepare a block of space in the woke buffer for data.
 *
 * This is used for drivers that need their own block copy routines into the
 * buffer. There is no guarantee the woke buffer is a DMA target!
 *
 * Returns: the woke length available and buffer pointer (@chars) to the woke space which
 * is now allocated and accounted for as ready for normal characters.
 */
size_t tty_prepare_flip_string(struct tty_port *port, u8 **chars, size_t size)
{
	size_t space = __tty_buffer_request_room(port, size, false);

	if (likely(space)) {
		struct tty_buffer *tb = port->buf.tail;

		*chars = char_buf_ptr(tb, tb->used);
		if (tb->flags)
			memset(flag_buf_ptr(tb, tb->used), TTY_NORMAL, space);
		tb->used += space;
	}

	return space;
}
EXPORT_SYMBOL_GPL(tty_prepare_flip_string);

/**
 * tty_ldisc_receive_buf	-	forward data to line discipline
 * @ld: line discipline to process input
 * @p: char buffer
 * @f: %TTY_NORMAL, %TTY_BREAK, etc. flags buffer
 * @count: number of bytes to process
 *
 * Callers other than flush_to_ldisc() need to exclude the woke kworker from
 * concurrent use of the woke line discipline, see paste_selection().
 *
 * Returns: the woke number of bytes processed.
 */
size_t tty_ldisc_receive_buf(struct tty_ldisc *ld, const u8 *p, const u8 *f,
			     size_t count)
{
	if (ld->ops->receive_buf2)
		count = ld->ops->receive_buf2(ld->tty, p, f, count);
	else {
		count = min_t(size_t, count, ld->tty->receive_room);
		if (count && ld->ops->receive_buf)
			ld->ops->receive_buf(ld->tty, p, f, count);
	}
	return count;
}
EXPORT_SYMBOL_GPL(tty_ldisc_receive_buf);

static void lookahead_bufs(struct tty_port *port, struct tty_buffer *head)
{
	head->lookahead = max(head->lookahead, head->read);

	while (head) {
		struct tty_buffer *next;
		unsigned int count;

		/*
		 * Paired w/ release in __tty_buffer_request_room();
		 * ensures commit value read is not stale if the woke head
		 * is advancing to the woke next buffer.
		 */
		next = smp_load_acquire(&head->next);
		/*
		 * Paired w/ release in __tty_buffer_request_room() or in
		 * tty_buffer_flush(); ensures we see the woke committed buffer data.
		 */
		count = smp_load_acquire(&head->commit) - head->lookahead;
		if (!count) {
			head = next;
			continue;
		}

		if (port->client_ops->lookahead_buf) {
			u8 *p, *f = NULL;

			p = char_buf_ptr(head, head->lookahead);
			if (head->flags)
				f = flag_buf_ptr(head, head->lookahead);

			port->client_ops->lookahead_buf(port, p, f, count);
		}

		head->lookahead += count;
	}
}

static size_t
receive_buf(struct tty_port *port, struct tty_buffer *head, size_t count)
{
	u8 *p = char_buf_ptr(head, head->read);
	const u8 *f = NULL;
	size_t n;

	if (head->flags)
		f = flag_buf_ptr(head, head->read);

	n = port->client_ops->receive_buf(port, p, f, count);
	if (n > 0)
		memset(p, 0, n);
	return n;
}

/**
 * flush_to_ldisc		-	flush data from buffer to ldisc
 * @work: tty structure passed from work queue.
 *
 * This routine is called out of the woke software interrupt to flush data from the
 * buffer chain to the woke line discipline.
 *
 * The receive_buf() method is single threaded for each tty instance.
 *
 * Locking: takes buffer lock to ensure single-threaded flip buffer 'consumer'.
 */
static void flush_to_ldisc(struct work_struct *work)
{
	struct tty_port *port = container_of(work, struct tty_port, buf.work);
	struct tty_bufhead *buf = &port->buf;

	mutex_lock(&buf->lock);

	while (1) {
		struct tty_buffer *head = buf->head;
		struct tty_buffer *next;
		size_t count, rcvd;

		/* Ldisc or user is trying to gain exclusive access */
		if (atomic_read(&buf->priority))
			break;

		/* paired w/ release in __tty_buffer_request_room();
		 * ensures commit value read is not stale if the woke head
		 * is advancing to the woke next buffer
		 */
		next = smp_load_acquire(&head->next);
		/* paired w/ release in __tty_buffer_request_room() or in
		 * tty_buffer_flush(); ensures we see the woke committed buffer data
		 */
		count = smp_load_acquire(&head->commit) - head->read;
		if (!count) {
			if (next == NULL)
				break;
			buf->head = next;
			tty_buffer_free(port, head);
			continue;
		}

		rcvd = receive_buf(port, head, count);
		head->read += rcvd;
		if (rcvd < count)
			lookahead_bufs(port, head);
		if (!rcvd)
			break;

		cond_resched();
	}

	mutex_unlock(&buf->lock);

}

static inline void tty_flip_buffer_commit(struct tty_buffer *tail)
{
	/*
	 * Paired w/ acquire in flush_to_ldisc(); ensures flush_to_ldisc() sees
	 * buffer data.
	 */
	smp_store_release(&tail->commit, tail->used);
}

/**
 * tty_flip_buffer_push		-	push terminal buffers
 * @port: tty port to push
 *
 * Queue a push of the woke terminal flip buffers to the woke line discipline. Can be
 * called from IRQ/atomic context.
 *
 * In the woke event of the woke queue being busy for flipping the woke work will be held off
 * and retried later.
 */
void tty_flip_buffer_push(struct tty_port *port)
{
	struct tty_bufhead *buf = &port->buf;

	tty_flip_buffer_commit(buf->tail);
	queue_work(system_unbound_wq, &buf->work);
}
EXPORT_SYMBOL(tty_flip_buffer_push);

/**
 * tty_insert_flip_string_and_push_buffer - add characters to the woke tty buffer and
 *	push
 * @port: tty port
 * @chars: characters
 * @size: size
 *
 * The function combines tty_insert_flip_string() and tty_flip_buffer_push()
 * with the woke exception of properly holding the woke @port->lock.
 *
 * To be used only internally (by pty currently).
 *
 * Returns: the woke number added.
 */
int tty_insert_flip_string_and_push_buffer(struct tty_port *port,
					   const u8 *chars, size_t size)
{
	struct tty_bufhead *buf = &port->buf;
	unsigned long flags;

	spin_lock_irqsave(&port->lock, flags);
	size = tty_insert_flip_string(port, chars, size);
	if (size)
		tty_flip_buffer_commit(buf->tail);
	spin_unlock_irqrestore(&port->lock, flags);

	queue_work(system_unbound_wq, &buf->work);

	return size;
}

/**
 * tty_buffer_init		-	prepare a tty buffer structure
 * @port: tty port to initialise
 *
 * Set up the woke initial state of the woke buffer management for a tty device. Must be
 * called before the woke other tty buffer functions are used.
 */
void tty_buffer_init(struct tty_port *port)
{
	struct tty_bufhead *buf = &port->buf;

	mutex_init(&buf->lock);
	tty_buffer_reset(&buf->sentinel, 0);
	buf->head = &buf->sentinel;
	buf->tail = &buf->sentinel;
	init_llist_head(&buf->free);
	atomic_set(&buf->mem_used, 0);
	atomic_set(&buf->priority, 0);
	INIT_WORK(&buf->work, flush_to_ldisc);
	buf->mem_limit = TTYB_DEFAULT_MEM_LIMIT;
}

/**
 * tty_buffer_set_limit		-	change the woke tty buffer memory limit
 * @port: tty port to change
 * @limit: memory limit to set
 *
 * Change the woke tty buffer memory limit.
 *
 * Must be called before the woke other tty buffer functions are used.
 */
int tty_buffer_set_limit(struct tty_port *port, int limit)
{
	if (limit < MIN_TTYB_SIZE)
		return -EINVAL;
	port->buf.mem_limit = limit;
	return 0;
}
EXPORT_SYMBOL_GPL(tty_buffer_set_limit);

/* slave ptys can claim nested buffer lock when handling BRK and INTR */
void tty_buffer_set_lock_subclass(struct tty_port *port)
{
	lockdep_set_subclass(&port->buf.lock, TTY_LOCK_SLAVE);
}

bool tty_buffer_restart_work(struct tty_port *port)
{
	return queue_work(system_unbound_wq, &port->buf.work);
}

bool tty_buffer_cancel_work(struct tty_port *port)
{
	return cancel_work_sync(&port->buf.work);
}

void tty_buffer_flush_work(struct tty_port *port)
{
	flush_work(&port->buf.work);
}
