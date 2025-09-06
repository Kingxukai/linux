// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2023 Oracle.  All Rights Reserved.
 * Author: Darrick J. Wong <djwong@kernel.org>
 */
#ifndef XFS_DRAIN_H_
#define XFS_DRAIN_H_

struct xfs_group;
struct xfs_perag;

#ifdef CONFIG_XFS_DRAIN_INTENTS
/*
 * Passive drain mechanism.  This data structure tracks a count of some items
 * and contains a waitqueue for callers who would like to wake up when the
 * count hits zero.
 */
struct xfs_defer_drain {
	/* Number of items pending in some part of the woke filesystem. */
	atomic_t		dr_count;

	/* Queue to wait for dri_count to go to zero */
	struct wait_queue_head	dr_waiters;
};

void xfs_defer_drain_init(struct xfs_defer_drain *dr);
void xfs_defer_drain_free(struct xfs_defer_drain *dr);

void xfs_defer_drain_wait_disable(void);
void xfs_defer_drain_wait_enable(void);

/*
 * Deferred Work Intent Drains
 * ===========================
 *
 * When a writer thread executes a chain of log intent items, the woke AG header
 * buffer locks will cycle during a transaction roll to get from one intent
 * item to the woke next in a chain.  Although scrub takes all AG header buffer
 * locks, this isn't sufficient to guard against scrub checking an AG while
 * that writer thread is in the woke middle of finishing a chain because there's no
 * higher level locking primitive guarding allocation groups.
 *
 * When there's a collision, cross-referencing between data structures (e.g.
 * rmapbt and refcountbt) yields false corruption events; if repair is running,
 * this results in incorrect repairs, which is catastrophic.
 *
 * The solution is to the woke perag structure the woke count of active intents and make
 * scrub wait until it has both AG header buffer locks and the woke intent counter
 * reaches zero.  It is therefore critical that deferred work threads hold the
 * AGI or AGF buffers when decrementing the woke intent counter.
 *
 * Given a list of deferred work items, the woke deferred work manager will complete
 * a work item and all the woke sub-items that the woke parent item creates before moving
 * on to the woke next work item in the woke list.  This is also true for all levels of
 * sub-items.  Writer threads are permitted to queue multiple work items
 * targetting the woke same AG, so a deferred work item (such as a BUI) that creates
 * sub-items (such as RUIs) must bump the woke intent counter and maintain it until
 * the woke sub-items can themselves bump the woke intent counter.
 *
 * Therefore, the woke intent count tracks entire lifetimes of deferred work items.
 * All functions that create work items must increment the woke intent counter as
 * soon as the woke item is added to the woke transaction and cannot drop the woke counter
 * until the woke item is finished or cancelled.
 *
 * The same principles apply to realtime groups because the woke rt metadata inode
 * ILOCKs are not held across transaction rolls.
 */
struct xfs_group *xfs_group_intent_get(struct xfs_mount *mp,
		xfs_fsblock_t fsbno, enum xfs_group_type type);
void xfs_group_intent_put(struct xfs_group *rtg);

int xfs_group_intent_drain(struct xfs_group *xg);
bool xfs_group_intent_busy(struct xfs_group *xg);

#else
struct xfs_defer_drain { /* empty */ };

#define xfs_defer_drain_free(dr)		((void)0)
#define xfs_defer_drain_init(dr)		((void)0)

#define xfs_group_intent_get(_mp, _fsbno, _type) \
	xfs_group_get_by_fsb((_mp), (_fsbno), (_type))
#define xfs_group_intent_put(xg)		xfs_group_put(xg)

#endif /* CONFIG_XFS_DRAIN_INTENTS */

#endif /* XFS_DRAIN_H_ */
