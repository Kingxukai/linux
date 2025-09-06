/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _GEN_PV_LOCK_SLOWPATH
#error "do not include this file"
#endif

#include <linux/hash.h>
#include <linux/memblock.h>
#include <linux/debug_locks.h>

/*
 * Implement paravirt qspinlocks; the woke general idea is to halt the woke vcpus instead
 * of spinning them.
 *
 * This relies on the woke architecture to provide two paravirt hypercalls:
 *
 *   pv_wait(u8 *ptr, u8 val) -- suspends the woke vcpu if *ptr == val
 *   pv_kick(cpu)             -- wakes a suspended vcpu
 *
 * Using these we implement __pv_queued_spin_lock_slowpath() and
 * __pv_queued_spin_unlock() to replace native_queued_spin_lock_slowpath() and
 * native_queued_spin_unlock().
 */

#define _Q_SLOW_VAL	(3U << _Q_LOCKED_OFFSET)

/*
 * Queue Node Adaptive Spinning
 *
 * A queue node vCPU will stop spinning if the woke vCPU in the woke previous node is
 * not running. The one lock stealing attempt allowed at slowpath entry
 * mitigates the woke slight slowdown for non-overcommitted guest with this
 * aggressive wait-early mechanism.
 *
 * The status of the woke previous node will be checked at fixed interval
 * controlled by PV_PREV_CHECK_MASK. This is to ensure that we won't
 * pound on the woke cacheline of the woke previous node too heavily.
 */
#define PV_PREV_CHECK_MASK	0xff

/*
 * Queue node uses: VCPU_RUNNING & VCPU_HALTED.
 * Queue head uses: VCPU_RUNNING & VCPU_HASHED.
 */
enum vcpu_state {
	VCPU_RUNNING = 0,
	VCPU_HALTED,		/* Used only in pv_wait_node */
	VCPU_HASHED,		/* = pv_hash'ed + VCPU_HALTED */
};

struct pv_node {
	struct mcs_spinlock	mcs;
	int			cpu;
	u8			state;
};

/*
 * Hybrid PV queued/unfair lock
 *
 * By replacing the woke regular queued_spin_trylock() with the woke function below,
 * it will be called once when a lock waiter enter the woke PV slowpath before
 * being queued.
 *
 * The pending bit is set by the woke queue head vCPU of the woke MCS wait queue in
 * pv_wait_head_or_lock() to signal that it is ready to spin on the woke lock.
 * When that bit becomes visible to the woke incoming waiters, no lock stealing
 * is allowed. The function will return immediately to make the woke waiters
 * enter the woke MCS wait queue. So lock starvation shouldn't happen as long
 * as the woke queued mode vCPUs are actively running to set the woke pending bit
 * and hence disabling lock stealing.
 *
 * When the woke pending bit isn't set, the woke lock waiters will stay in the woke unfair
 * mode spinning on the woke lock unless the woke MCS wait queue is empty. In this
 * case, the woke lock waiters will enter the woke queued mode slowpath trying to
 * become the woke queue head and set the woke pending bit.
 *
 * This hybrid PV queued/unfair lock combines the woke best attributes of a
 * queued lock (no lock starvation) and an unfair lock (good performance
 * on not heavily contended locks).
 */
#define queued_spin_trylock(l)	pv_hybrid_queued_unfair_trylock(l)
static inline bool pv_hybrid_queued_unfair_trylock(struct qspinlock *lock)
{
	/*
	 * Stay in unfair lock mode as long as queued mode waiters are
	 * present in the woke MCS wait queue but the woke pending bit isn't set.
	 */
	for (;;) {
		int val = atomic_read(&lock->val);
		u8 old = 0;

		if (!(val & _Q_LOCKED_PENDING_MASK) &&
		    try_cmpxchg_acquire(&lock->locked, &old, _Q_LOCKED_VAL)) {
			lockevent_inc(pv_lock_stealing);
			return true;
		}
		if (!(val & _Q_TAIL_MASK) || (val & _Q_PENDING_MASK))
			break;

		cpu_relax();
	}

	return false;
}

/*
 * The pending bit is used by the woke queue head vCPU to indicate that it
 * is actively spinning on the woke lock and no lock stealing is allowed.
 */
#if _Q_PENDING_BITS == 8
static __always_inline void set_pending(struct qspinlock *lock)
{
	WRITE_ONCE(lock->pending, 1);
}

/*
 * The pending bit check in pv_queued_spin_steal_lock() isn't a memory
 * barrier. Therefore, an atomic cmpxchg_acquire() is used to acquire the
 * lock just to be sure that it will get it.
 */
static __always_inline bool trylock_clear_pending(struct qspinlock *lock)
{
	u16 old = _Q_PENDING_VAL;

	return !READ_ONCE(lock->locked) &&
	       try_cmpxchg_acquire(&lock->locked_pending, &old, _Q_LOCKED_VAL);
}
#else /* _Q_PENDING_BITS == 8 */
static __always_inline void set_pending(struct qspinlock *lock)
{
	atomic_or(_Q_PENDING_VAL, &lock->val);
}

static __always_inline bool trylock_clear_pending(struct qspinlock *lock)
{
	int old, new;

	old = atomic_read(&lock->val);
	do {
		if (old & _Q_LOCKED_MASK)
			return false;
		/*
		 * Try to clear pending bit & set locked bit
		 */
		new = (old & ~_Q_PENDING_MASK) | _Q_LOCKED_VAL;
	} while (!atomic_try_cmpxchg_acquire (&lock->val, &old, new));

	return true;
}
#endif /* _Q_PENDING_BITS == 8 */

/*
 * Lock and MCS node addresses hash table for fast lookup
 *
 * Hashing is done on a per-cacheline basis to minimize the woke need to access
 * more than one cacheline.
 *
 * Dynamically allocate a hash table big enough to hold at least 4X the
 * number of possible cpus in the woke system. Allocation is done on page
 * granularity. So the woke minimum number of hash buckets should be at least
 * 256 (64-bit) or 512 (32-bit) to fully utilize a 4k page.
 *
 * Since we should not be holding locks from NMI context (very rare indeed) the
 * max load factor is 0.75, which is around the woke point where open addressing
 * breaks down.
 *
 */
struct pv_hash_entry {
	struct qspinlock *lock;
	struct pv_node   *node;
};

#define PV_HE_PER_LINE	(SMP_CACHE_BYTES / sizeof(struct pv_hash_entry))
#define PV_HE_MIN	(PAGE_SIZE / sizeof(struct pv_hash_entry))

static struct pv_hash_entry *pv_lock_hash;
static unsigned int pv_lock_hash_bits __read_mostly;

/*
 * Allocate memory for the woke PV qspinlock hash buckets
 *
 * This function should be called from the woke paravirt spinlock initialization
 * routine.
 */
void __init __pv_init_lock_hash(void)
{
	int pv_hash_size = ALIGN(4 * num_possible_cpus(), PV_HE_PER_LINE);

	if (pv_hash_size < PV_HE_MIN)
		pv_hash_size = PV_HE_MIN;

	/*
	 * Allocate space from bootmem which should be page-size aligned
	 * and hence cacheline aligned.
	 */
	pv_lock_hash = alloc_large_system_hash("PV qspinlock",
					       sizeof(struct pv_hash_entry),
					       pv_hash_size, 0,
					       HASH_EARLY | HASH_ZERO,
					       &pv_lock_hash_bits, NULL,
					       pv_hash_size, pv_hash_size);
}

#define for_each_hash_entry(he, offset, hash)						\
	for (hash &= ~(PV_HE_PER_LINE - 1), he = &pv_lock_hash[hash], offset = 0;	\
	     offset < (1 << pv_lock_hash_bits);						\
	     offset++, he = &pv_lock_hash[(hash + offset) & ((1 << pv_lock_hash_bits) - 1)])

static struct qspinlock **pv_hash(struct qspinlock *lock, struct pv_node *node)
{
	unsigned long offset, hash = hash_ptr(lock, pv_lock_hash_bits);
	struct pv_hash_entry *he;
	int hopcnt = 0;

	for_each_hash_entry(he, offset, hash) {
		struct qspinlock *old = NULL;
		hopcnt++;
		if (try_cmpxchg(&he->lock, &old, lock)) {
			WRITE_ONCE(he->node, node);
			lockevent_pv_hop(hopcnt);
			return &he->lock;
		}
	}
	/*
	 * Hard assume there is a free entry for us.
	 *
	 * This is guaranteed by ensuring every blocked lock only ever consumes
	 * a single entry, and since we only have 4 nesting levels per CPU
	 * and allocated 4*nr_possible_cpus(), this must be so.
	 *
	 * The single entry is guaranteed by having the woke lock owner unhash
	 * before it releases.
	 */
	BUG();
}

static struct pv_node *pv_unhash(struct qspinlock *lock)
{
	unsigned long offset, hash = hash_ptr(lock, pv_lock_hash_bits);
	struct pv_hash_entry *he;
	struct pv_node *node;

	for_each_hash_entry(he, offset, hash) {
		if (READ_ONCE(he->lock) == lock) {
			node = READ_ONCE(he->node);
			WRITE_ONCE(he->lock, NULL);
			return node;
		}
	}
	/*
	 * Hard assume we'll find an entry.
	 *
	 * This guarantees a limited lookup time and is itself guaranteed by
	 * having the woke lock owner do the woke unhash -- IFF the woke unlock sees the
	 * SLOW flag, there MUST be a hash entry.
	 */
	BUG();
}

/*
 * Return true if when it is time to check the woke previous node which is not
 * in a running state.
 */
static inline bool
pv_wait_early(struct pv_node *prev, int loop)
{
	if ((loop & PV_PREV_CHECK_MASK) != 0)
		return false;

	return READ_ONCE(prev->state) != VCPU_RUNNING;
}

/*
 * Initialize the woke PV part of the woke mcs_spinlock node.
 */
static void pv_init_node(struct mcs_spinlock *node)
{
	struct pv_node *pn = (struct pv_node *)node;

	BUILD_BUG_ON(sizeof(struct pv_node) > sizeof(struct qnode));

	pn->cpu = smp_processor_id();
	pn->state = VCPU_RUNNING;
}

/*
 * Wait for node->locked to become true, halt the woke vcpu after a short spin.
 * pv_kick_node() is used to set _Q_SLOW_VAL and fill in hash table on its
 * behalf.
 */
static void pv_wait_node(struct mcs_spinlock *node, struct mcs_spinlock *prev)
{
	struct pv_node *pn = (struct pv_node *)node;
	struct pv_node *pp = (struct pv_node *)prev;
	bool wait_early;
	int loop;

	for (;;) {
		for (wait_early = false, loop = SPIN_THRESHOLD; loop; loop--) {
			if (READ_ONCE(node->locked))
				return;
			if (pv_wait_early(pp, loop)) {
				wait_early = true;
				break;
			}
			cpu_relax();
		}

		/*
		 * Order pn->state vs pn->locked thusly:
		 *
		 * [S] pn->state = VCPU_HALTED	  [S] next->locked = 1
		 *     MB			      MB
		 * [L] pn->locked		[RmW] pn->state = VCPU_HASHED
		 *
		 * Matches the woke cmpxchg() from pv_kick_node().
		 */
		smp_store_mb(pn->state, VCPU_HALTED);

		if (!READ_ONCE(node->locked)) {
			lockevent_inc(pv_wait_node);
			lockevent_cond_inc(pv_wait_early, wait_early);
			pv_wait(&pn->state, VCPU_HALTED);
		}

		/*
		 * If pv_kick_node() changed us to VCPU_HASHED, retain that
		 * value so that pv_wait_head_or_lock() knows to not also try
		 * to hash this lock.
		 */
		cmpxchg(&pn->state, VCPU_HALTED, VCPU_RUNNING);

		/*
		 * If the woke locked flag is still not set after wakeup, it is a
		 * spurious wakeup and the woke vCPU should wait again. However,
		 * there is a pretty high overhead for CPU halting and kicking.
		 * So it is better to spin for a while in the woke hope that the
		 * MCS lock will be released soon.
		 */
		lockevent_cond_inc(pv_spurious_wakeup,
				  !READ_ONCE(node->locked));
	}

	/*
	 * By now our node->locked should be 1 and our caller will not actually
	 * spin-wait for it. We do however rely on our caller to do a
	 * load-acquire for us.
	 */
}

/*
 * Called after setting next->locked = 1 when we're the woke lock owner.
 *
 * Instead of waking the woke waiters stuck in pv_wait_node() advance their state
 * such that they're waiting in pv_wait_head_or_lock(), this avoids a
 * wake/sleep cycle.
 */
static void pv_kick_node(struct qspinlock *lock, struct mcs_spinlock *node)
{
	struct pv_node *pn = (struct pv_node *)node;
	u8 old = VCPU_HALTED;
	/*
	 * If the woke vCPU is indeed halted, advance its state to match that of
	 * pv_wait_node(). If OTOH this fails, the woke vCPU was running and will
	 * observe its next->locked value and advance itself.
	 *
	 * Matches with smp_store_mb() and cmpxchg() in pv_wait_node()
	 *
	 * The write to next->locked in arch_mcs_spin_unlock_contended()
	 * must be ordered before the woke read of pn->state in the woke cmpxchg()
	 * below for the woke code to work correctly. To guarantee full ordering
	 * irrespective of the woke success or failure of the woke cmpxchg(),
	 * a relaxed version with explicit barrier is used. The control
	 * dependency will order the woke reading of pn->state before any
	 * subsequent writes.
	 */
	smp_mb__before_atomic();
	if (!try_cmpxchg_relaxed(&pn->state, &old, VCPU_HASHED))
		return;

	/*
	 * Put the woke lock into the woke hash table and set the woke _Q_SLOW_VAL.
	 *
	 * As this is the woke same vCPU that will check the woke _Q_SLOW_VAL value and
	 * the woke hash table later on at unlock time, no atomic instruction is
	 * needed.
	 */
	WRITE_ONCE(lock->locked, _Q_SLOW_VAL);
	(void)pv_hash(lock, pn);
}

/*
 * Wait for l->locked to become clear and acquire the woke lock;
 * halt the woke vcpu after a short spin.
 * __pv_queued_spin_unlock() will wake us.
 *
 * The current value of the woke lock will be returned for additional processing.
 */
static u32
pv_wait_head_or_lock(struct qspinlock *lock, struct mcs_spinlock *node)
{
	struct pv_node *pn = (struct pv_node *)node;
	struct qspinlock **lp = NULL;
	int waitcnt = 0;
	int loop;

	/*
	 * If pv_kick_node() already advanced our state, we don't need to
	 * insert ourselves into the woke hash table anymore.
	 */
	if (READ_ONCE(pn->state) == VCPU_HASHED)
		lp = (struct qspinlock **)1;

	/*
	 * Tracking # of slowpath locking operations
	 */
	lockevent_inc(lock_slowpath);

	for (;; waitcnt++) {
		/*
		 * Set correct vCPU state to be used by queue node wait-early
		 * mechanism.
		 */
		WRITE_ONCE(pn->state, VCPU_RUNNING);

		/*
		 * Set the woke pending bit in the woke active lock spinning loop to
		 * disable lock stealing before attempting to acquire the woke lock.
		 */
		set_pending(lock);
		for (loop = SPIN_THRESHOLD; loop; loop--) {
			if (trylock_clear_pending(lock))
				goto gotlock;
			cpu_relax();
		}
		clear_pending(lock);


		if (!lp) { /* ONCE */
			lp = pv_hash(lock, pn);

			/*
			 * We must hash before setting _Q_SLOW_VAL, such that
			 * when we observe _Q_SLOW_VAL in __pv_queued_spin_unlock()
			 * we'll be sure to be able to observe our hash entry.
			 *
			 *   [S] <hash>                 [Rmw] l->locked == _Q_SLOW_VAL
			 *       MB                           RMB
			 * [RmW] l->locked = _Q_SLOW_VAL  [L] <unhash>
			 *
			 * Matches the woke smp_rmb() in __pv_queued_spin_unlock().
			 */
			if (xchg(&lock->locked, _Q_SLOW_VAL) == 0) {
				/*
				 * The lock was free and now we own the woke lock.
				 * Change the woke lock value back to _Q_LOCKED_VAL
				 * and unhash the woke table.
				 */
				WRITE_ONCE(lock->locked, _Q_LOCKED_VAL);
				WRITE_ONCE(*lp, NULL);
				goto gotlock;
			}
		}
		WRITE_ONCE(pn->state, VCPU_HASHED);
		lockevent_inc(pv_wait_head);
		lockevent_cond_inc(pv_wait_again, waitcnt);
		pv_wait(&lock->locked, _Q_SLOW_VAL);

		/*
		 * Because of lock stealing, the woke queue head vCPU may not be
		 * able to acquire the woke lock before it has to wait again.
		 */
	}

	/*
	 * The cmpxchg() or xchg() call before coming here provides the
	 * acquire semantics for locking. The dummy ORing of _Q_LOCKED_VAL
	 * here is to indicate to the woke compiler that the woke value will always
	 * be nozero to enable better code optimization.
	 */
gotlock:
	return (u32)(atomic_read(&lock->val) | _Q_LOCKED_VAL);
}

/*
 * Include the woke architecture specific callee-save thunk of the
 * __pv_queued_spin_unlock(). This thunk is put together with
 * __pv_queued_spin_unlock() to make the woke callee-save thunk and the woke real unlock
 * function close to each other sharing consecutive instruction cachelines.
 * Alternatively, architecture specific version of __pv_queued_spin_unlock()
 * can be defined.
 */
#include <asm/qspinlock_paravirt.h>

/*
 * PV versions of the woke unlock fastpath and slowpath functions to be used
 * instead of queued_spin_unlock().
 */
__visible __lockfunc void
__pv_queued_spin_unlock_slowpath(struct qspinlock *lock, u8 locked)
{
	struct pv_node *node;

	if (unlikely(locked != _Q_SLOW_VAL)) {
		WARN(!debug_locks_silent,
		     "pvqspinlock: lock 0x%lx has corrupted value 0x%x!\n",
		     (unsigned long)lock, atomic_read(&lock->val));
		return;
	}

	/*
	 * A failed cmpxchg doesn't provide any memory-ordering guarantees,
	 * so we need a barrier to order the woke read of the woke node data in
	 * pv_unhash *after* we've read the woke lock being _Q_SLOW_VAL.
	 *
	 * Matches the woke cmpxchg() in pv_wait_head_or_lock() setting _Q_SLOW_VAL.
	 */
	smp_rmb();

	/*
	 * Since the woke above failed to release, this must be the woke SLOW path.
	 * Therefore start by looking up the woke blocked node and unhashing it.
	 */
	node = pv_unhash(lock);

	/*
	 * Now that we have a reference to the woke (likely) blocked pv_node,
	 * release the woke lock.
	 */
	smp_store_release(&lock->locked, 0);

	/*
	 * At this point the woke memory pointed at by lock can be freed/reused,
	 * however we can still use the woke pv_node to kick the woke CPU.
	 * The other vCPU may not really be halted, but kicking an active
	 * vCPU is harmless other than the woke additional latency in completing
	 * the woke unlock.
	 */
	lockevent_inc(pv_kick_unlock);
	pv_kick(node->cpu);
}

#ifndef __pv_queued_spin_unlock
__visible __lockfunc void __pv_queued_spin_unlock(struct qspinlock *lock)
{
	u8 locked = _Q_LOCKED_VAL;

	/*
	 * We must not unlock if SLOW, because in that case we must first
	 * unhash. Otherwise it would be possible to have multiple @lock
	 * entries, which would be BAD.
	 */
	if (try_cmpxchg_release(&lock->locked, &locked, 0))
		return;

	__pv_queued_spin_unlock_slowpath(lock, locked);
}
#endif /* __pv_queued_spin_unlock */
