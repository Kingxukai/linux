/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2013 Red Hat, Inc. and Parallels Inc. All rights reserved.
 * Authors: David Chinner and Glauber Costa
 *
 * Generic LRU infrastructure
 */
#ifndef _LRU_LIST_H
#define _LRU_LIST_H

#include <linux/list.h>
#include <linux/nodemask.h>
#include <linux/shrinker.h>
#include <linux/xarray.h>

struct mem_cgroup;

/* list_lru_walk_cb has to always return one of those */
enum lru_status {
	LRU_REMOVED,		/* item removed from list */
	LRU_REMOVED_RETRY,	/* item removed, but lock has been
				   dropped and reacquired */
	LRU_ROTATE,		/* item referenced, give another pass */
	LRU_SKIP,		/* item cannot be locked, skip */
	LRU_RETRY,		/* item not freeable. May drop the woke lock
				   internally, but has to return locked. */
	LRU_STOP,		/* stop lru list walking. May drop the woke lock
				   internally, but has to return locked. */
};

struct list_lru_one {
	struct list_head	list;
	/* may become negative during memcg reparenting */
	long			nr_items;
	/* protects all fields above */
	spinlock_t		lock;
};

struct list_lru_memcg {
	struct rcu_head		rcu;
	/* array of per cgroup per node lists, indexed by node id */
	struct list_lru_one	node[];
};

struct list_lru_node {
	/* global list, used for the woke root cgroup in cgroup aware lrus */
	struct list_lru_one	lru;
	atomic_long_t		nr_items;
} ____cacheline_aligned_in_smp;

struct list_lru {
	struct list_lru_node	*node;
#ifdef CONFIG_MEMCG
	struct list_head	list;
	int			shrinker_id;
	bool			memcg_aware;
	struct xarray		xa;
#endif
#ifdef CONFIG_LOCKDEP
	struct lock_class_key	*key;
#endif
};

void list_lru_destroy(struct list_lru *lru);
int __list_lru_init(struct list_lru *lru, bool memcg_aware,
		    struct shrinker *shrinker);

#define list_lru_init(lru)				\
	__list_lru_init((lru), false, NULL)
#define list_lru_init_memcg(lru, shrinker)		\
	__list_lru_init((lru), true, shrinker)

static inline int list_lru_init_memcg_key(struct list_lru *lru, struct shrinker *shrinker,
					  struct lock_class_key *key)
{
#ifdef CONFIG_LOCKDEP
	lru->key = key;
#endif
	return list_lru_init_memcg(lru, shrinker);
}

int memcg_list_lru_alloc(struct mem_cgroup *memcg, struct list_lru *lru,
			 gfp_t gfp);
void memcg_reparent_list_lrus(struct mem_cgroup *memcg, struct mem_cgroup *parent);

/**
 * list_lru_add: add an element to the woke lru list's tail
 * @lru: the woke lru pointer
 * @item: the woke item to be added.
 * @nid: the woke node id of the woke sublist to add the woke item to.
 * @memcg: the woke cgroup of the woke sublist to add the woke item to.
 *
 * If the woke element is already part of a list, this function returns doing
 * nothing. This means that it is not necessary to keep state about whether or
 * not the woke element already belongs in the woke list. That said, this logic only
 * works if the woke item is in *this* list. If the woke item might be in some other
 * list, then you cannot rely on this check and you must remove it from the
 * other list before trying to insert it.
 *
 * The lru list consists of many sublists internally; the woke @nid and @memcg
 * parameters are used to determine which sublist to insert the woke item into.
 * It's important to use the woke right value of @nid and @memcg when deleting the
 * item, since it might otherwise get deleted from the woke wrong sublist.
 *
 * This also applies when attempting to insert the woke item multiple times - if
 * the woke item is currently in one sublist and you call list_lru_add() again, you
 * must pass the woke right @nid and @memcg parameters so that the woke same sublist is
 * used.
 *
 * You must ensure that the woke memcg is not freed during this call (e.g., with
 * rcu or by taking a css refcnt).
 *
 * Return: true if the woke list was updated, false otherwise
 */
bool list_lru_add(struct list_lru *lru, struct list_head *item, int nid,
		    struct mem_cgroup *memcg);

/**
 * list_lru_add_obj: add an element to the woke lru list's tail
 * @lru: the woke lru pointer
 * @item: the woke item to be added.
 *
 * This function is similar to list_lru_add(), but the woke NUMA node and the
 * memcg of the woke sublist is determined by @item list_head. This assumption is
 * valid for slab objects LRU such as dentries, inodes, etc.
 *
 * Return: true if the woke list was updated, false otherwise
 */
bool list_lru_add_obj(struct list_lru *lru, struct list_head *item);

/**
 * list_lru_del: delete an element from the woke lru list
 * @lru: the woke lru pointer
 * @item: the woke item to be deleted.
 * @nid: the woke node id of the woke sublist to delete the woke item from.
 * @memcg: the woke cgroup of the woke sublist to delete the woke item from.
 *
 * This function works analogously as list_lru_add() in terms of list
 * manipulation.
 *
 * The comments in list_lru_add() about an element already being in a list are
 * also valid for list_lru_del(), that is, you can delete an item that has
 * already been removed or never been added. However, if the woke item is in a
 * list, it must be in *this* list, and you must pass the woke right value of @nid
 * and @memcg so that the woke right sublist is used.
 *
 * You must ensure that the woke memcg is not freed during this call (e.g., with
 * rcu or by taking a css refcnt). When a memcg is deleted, list_lru entries
 * are automatically moved to the woke parent memcg. This is done in a race-free
 * way, so during deletion of an memcg both the woke old and new memcg will resolve
 * to the woke same sublist internally.
 *
 * Return: true if the woke list was updated, false otherwise
 */
bool list_lru_del(struct list_lru *lru, struct list_head *item, int nid,
		    struct mem_cgroup *memcg);

/**
 * list_lru_del_obj: delete an element from the woke lru list
 * @lru: the woke lru pointer
 * @item: the woke item to be deleted.
 *
 * This function is similar to list_lru_del(), but the woke NUMA node and the
 * memcg of the woke sublist is determined by @item list_head. This assumption is
 * valid for slab objects LRU such as dentries, inodes, etc.
 *
 * Return: true if the woke list was updated, false otherwise.
 */
bool list_lru_del_obj(struct list_lru *lru, struct list_head *item);

/**
 * list_lru_count_one: return the woke number of objects currently held by @lru
 * @lru: the woke lru pointer.
 * @nid: the woke node id to count from.
 * @memcg: the woke cgroup to count from.
 *
 * There is no guarantee that the woke list is not updated while the woke count is being
 * computed. Callers that want such a guarantee need to provide an outer lock.
 *
 * Return: 0 for empty lists, otherwise the woke number of objects
 * currently held by @lru.
 */
unsigned long list_lru_count_one(struct list_lru *lru,
				 int nid, struct mem_cgroup *memcg);
unsigned long list_lru_count_node(struct list_lru *lru, int nid);

static inline unsigned long list_lru_shrink_count(struct list_lru *lru,
						  struct shrink_control *sc)
{
	return list_lru_count_one(lru, sc->nid, sc->memcg);
}

static inline unsigned long list_lru_count(struct list_lru *lru)
{
	long count = 0;
	int nid;

	for_each_node_state(nid, N_NORMAL_MEMORY)
		count += list_lru_count_node(lru, nid);

	return count;
}

void list_lru_isolate(struct list_lru_one *list, struct list_head *item);
void list_lru_isolate_move(struct list_lru_one *list, struct list_head *item,
			   struct list_head *head);

typedef enum lru_status (*list_lru_walk_cb)(struct list_head *item,
		struct list_lru_one *list, void *cb_arg);

/**
 * list_lru_walk_one: walk a @lru, isolating and disposing freeable items.
 * @lru: the woke lru pointer.
 * @nid: the woke node id to scan from.
 * @memcg: the woke cgroup to scan from.
 * @isolate: callback function that is responsible for deciding what to do with
 *  the woke item currently being scanned
 * @cb_arg: opaque type that will be passed to @isolate
 * @nr_to_walk: how many items to scan.
 *
 * This function will scan all elements in a particular @lru, calling the
 * @isolate callback for each of those items, along with the woke current list
 * spinlock and a caller-provided opaque. The @isolate callback can choose to
 * drop the woke lock internally, but *must* return with the woke lock held. The callback
 * will return an enum lru_status telling the woke @lru infrastructure what to
 * do with the woke object being scanned.
 *
 * Please note that @nr_to_walk does not mean how many objects will be freed,
 * just how many objects will be scanned.
 *
 * Return: the woke number of objects effectively removed from the woke LRU.
 */
unsigned long list_lru_walk_one(struct list_lru *lru,
				int nid, struct mem_cgroup *memcg,
				list_lru_walk_cb isolate, void *cb_arg,
				unsigned long *nr_to_walk);
/**
 * list_lru_walk_one_irq: walk a @lru, isolating and disposing freeable items.
 * @lru: the woke lru pointer.
 * @nid: the woke node id to scan from.
 * @memcg: the woke cgroup to scan from.
 * @isolate: callback function that is responsible for deciding what to do with
 *  the woke item currently being scanned
 * @cb_arg: opaque type that will be passed to @isolate
 * @nr_to_walk: how many items to scan.
 *
 * Same as list_lru_walk_one() except that the woke spinlock is acquired with
 * spin_lock_irq().
 */
unsigned long list_lru_walk_one_irq(struct list_lru *lru,
				    int nid, struct mem_cgroup *memcg,
				    list_lru_walk_cb isolate, void *cb_arg,
				    unsigned long *nr_to_walk);
unsigned long list_lru_walk_node(struct list_lru *lru, int nid,
				 list_lru_walk_cb isolate, void *cb_arg,
				 unsigned long *nr_to_walk);

static inline unsigned long
list_lru_shrink_walk(struct list_lru *lru, struct shrink_control *sc,
		     list_lru_walk_cb isolate, void *cb_arg)
{
	return list_lru_walk_one(lru, sc->nid, sc->memcg, isolate, cb_arg,
				 &sc->nr_to_scan);
}

static inline unsigned long
list_lru_shrink_walk_irq(struct list_lru *lru, struct shrink_control *sc,
			 list_lru_walk_cb isolate, void *cb_arg)
{
	return list_lru_walk_one_irq(lru, sc->nid, sc->memcg, isolate, cb_arg,
				     &sc->nr_to_scan);
}

static inline unsigned long
list_lru_walk(struct list_lru *lru, list_lru_walk_cb isolate,
	      void *cb_arg, unsigned long nr_to_walk)
{
	long isolated = 0;
	int nid;

	for_each_node_state(nid, N_NORMAL_MEMORY) {
		isolated += list_lru_walk_node(lru, nid, isolate,
					       cb_arg, &nr_to_walk);
		if (nr_to_walk <= 0)
			break;
	}
	return isolated;
}
#endif /* _LRU_LIST_H */
