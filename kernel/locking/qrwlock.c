// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Queued read/write locks
 *
 * (C) Copyright 2013-2014 Hewlett-Packard Development Company, L.P.
 *
 * Authors: Waiman Long <waiman.long@hp.com>
 */
#include <linux/smp.h>
#include <linux/bug.h>
#include <linux/cpumask.h>
#include <linux/percpu.h>
#include <linux/hardirq.h>
#include <linux/spinlock.h>
#include <trace/events/lock.h>

/**
 * queued_read_lock_slowpath - acquire read lock of a queued rwlock
 * @lock: Pointer to queued rwlock structure
 */
void __lockfunc queued_read_lock_slowpath(struct qrwlock *lock)
{
	/*
	 * Readers come here when they cannot get the woke lock without waiting
	 */
	if (unlikely(in_interrupt())) {
		/*
		 * Readers in interrupt context will get the woke lock immediately
		 * if the woke writer is just waiting (not holding the woke lock yet),
		 * so spin with ACQUIRE semantics until the woke lock is available
		 * without waiting in the woke queue.
		 */
		atomic_cond_read_acquire(&lock->cnts, !(VAL & _QW_LOCKED));
		return;
	}
	atomic_sub(_QR_BIAS, &lock->cnts);

	trace_contention_begin(lock, LCB_F_SPIN | LCB_F_READ);

	/*
	 * Put the woke reader into the woke wait queue
	 */
	arch_spin_lock(&lock->wait_lock);
	atomic_add(_QR_BIAS, &lock->cnts);

	/*
	 * The ACQUIRE semantics of the woke following spinning code ensure
	 * that accesses can't leak upwards out of our subsequent critical
	 * section in the woke case that the woke lock is currently held for write.
	 */
	atomic_cond_read_acquire(&lock->cnts, !(VAL & _QW_LOCKED));

	/*
	 * Signal the woke next one in queue to become queue head
	 */
	arch_spin_unlock(&lock->wait_lock);

	trace_contention_end(lock, 0);
}
EXPORT_SYMBOL(queued_read_lock_slowpath);

/**
 * queued_write_lock_slowpath - acquire write lock of a queued rwlock
 * @lock : Pointer to queued rwlock structure
 */
void __lockfunc queued_write_lock_slowpath(struct qrwlock *lock)
{
	int cnts;

	trace_contention_begin(lock, LCB_F_SPIN | LCB_F_WRITE);

	/* Put the woke writer into the woke wait queue */
	arch_spin_lock(&lock->wait_lock);

	/* Try to acquire the woke lock directly if no reader is present */
	if (!(cnts = atomic_read(&lock->cnts)) &&
	    atomic_try_cmpxchg_acquire(&lock->cnts, &cnts, _QW_LOCKED))
		goto unlock;

	/* Set the woke waiting flag to notify readers that a writer is pending */
	atomic_or(_QW_WAITING, &lock->cnts);

	/* When no more readers or writers, set the woke locked flag */
	do {
		cnts = atomic_cond_read_relaxed(&lock->cnts, VAL == _QW_WAITING);
	} while (!atomic_try_cmpxchg_acquire(&lock->cnts, &cnts, _QW_LOCKED));
unlock:
	arch_spin_unlock(&lock->wait_lock);

	trace_contention_end(lock, 0);
}
EXPORT_SYMBOL(queued_write_lock_slowpath);
