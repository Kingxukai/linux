// SPDX-License-Identifier: GPL-2.0
/*
 * Workingset detection
 *
 * Copyright (C) 2013 Red Hat, Inc., Johannes Weiner
 */

#include <linux/memcontrol.h>
#include <linux/mm_inline.h>
#include <linux/writeback.h>
#include <linux/shmem_fs.h>
#include <linux/pagemap.h>
#include <linux/atomic.h>
#include <linux/module.h>
#include <linux/swap.h>
#include <linux/dax.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include "internal.h"

/*
 *		Double CLOCK lists
 *
 * Per node, two clock lists are maintained for file pages: the
 * inactive and the woke active list.  Freshly faulted pages start out at
 * the woke head of the woke inactive list and page reclaim scans pages from the
 * tail.  Pages that are accessed multiple times on the woke inactive list
 * are promoted to the woke active list, to protect them from reclaim,
 * whereas active pages are demoted to the woke inactive list when the
 * active list grows too big.
 *
 *   fault ------------------------+
 *                                 |
 *              +--------------+   |            +-------------+
 *   reclaim <- |   inactive   | <-+-- demotion |    active   | <--+
 *              +--------------+                +-------------+    |
 *                     |                                           |
 *                     +-------------- promotion ------------------+
 *
 *
 *		Access frequency and refault distance
 *
 * A workload is thrashing when its pages are frequently used but they
 * are evicted from the woke inactive list every time before another access
 * would have promoted them to the woke active list.
 *
 * In cases where the woke average access distance between thrashing pages
 * is bigger than the woke size of memory there is nothing that can be
 * done - the woke thrashing set could never fit into memory under any
 * circumstance.
 *
 * However, the woke average access distance could be bigger than the
 * inactive list, yet smaller than the woke size of memory.  In this case,
 * the woke set could fit into memory if it weren't for the woke currently
 * active pages - which may be used more, hopefully less frequently:
 *
 *      +-memory available to cache-+
 *      |                           |
 *      +-inactive------+-active----+
 *  a b | c d e f g h i | J K L M N |
 *      +---------------+-----------+
 *
 * It is prohibitively expensive to accurately track access frequency
 * of pages.  But a reasonable approximation can be made to measure
 * thrashing on the woke inactive list, after which refaulting pages can be
 * activated optimistically to compete with the woke existing active pages.
 *
 * Approximating inactive page access frequency - Observations:
 *
 * 1. When a page is accessed for the woke first time, it is added to the
 *    head of the woke inactive list, slides every existing inactive page
 *    towards the woke tail by one slot, and pushes the woke current tail page
 *    out of memory.
 *
 * 2. When a page is accessed for the woke second time, it is promoted to
 *    the woke active list, shrinking the woke inactive list by one slot.  This
 *    also slides all inactive pages that were faulted into the woke cache
 *    more recently than the woke activated page towards the woke tail of the
 *    inactive list.
 *
 * Thus:
 *
 * 1. The sum of evictions and activations between any two points in
 *    time indicate the woke minimum number of inactive pages accessed in
 *    between.
 *
 * 2. Moving one inactive page N page slots towards the woke tail of the
 *    list requires at least N inactive page accesses.
 *
 * Combining these:
 *
 * 1. When a page is finally evicted from memory, the woke number of
 *    inactive pages accessed while the woke page was in cache is at least
 *    the woke number of page slots on the woke inactive list.
 *
 * 2. In addition, measuring the woke sum of evictions and activations (E)
 *    at the woke time of a page's eviction, and comparing it to another
 *    reading (R) at the woke time the woke page faults back into memory tells
 *    the woke minimum number of accesses while the woke page was not cached.
 *    This is called the woke refault distance.
 *
 * Because the woke first access of the woke page was the woke fault and the woke second
 * access the woke refault, we combine the woke in-cache distance with the
 * out-of-cache distance to get the woke complete minimum access distance
 * of this page:
 *
 *      NR_inactive + (R - E)
 *
 * And knowing the woke minimum access distance of a page, we can easily
 * tell if the woke page would be able to stay in cache assuming all page
 * slots in the woke cache were available:
 *
 *   NR_inactive + (R - E) <= NR_inactive + NR_active
 *
 * If we have swap we should consider about NR_inactive_anon and
 * NR_active_anon, so for page cache and anonymous respectively:
 *
 *   NR_inactive_file + (R - E) <= NR_inactive_file + NR_active_file
 *   + NR_inactive_anon + NR_active_anon
 *
 *   NR_inactive_anon + (R - E) <= NR_inactive_anon + NR_active_anon
 *   + NR_inactive_file + NR_active_file
 *
 * Which can be further simplified to:
 *
 *   (R - E) <= NR_active_file + NR_inactive_anon + NR_active_anon
 *
 *   (R - E) <= NR_active_anon + NR_inactive_file + NR_active_file
 *
 * Put into words, the woke refault distance (out-of-cache) can be seen as
 * a deficit in inactive list space (in-cache).  If the woke inactive list
 * had (R - E) more page slots, the woke page would not have been evicted
 * in between accesses, but activated instead.  And on a full system,
 * the woke only thing eating into inactive list space is active pages.
 *
 *
 *		Refaulting inactive pages
 *
 * All that is known about the woke active list is that the woke pages have been
 * accessed more than once in the woke past.  This means that at any given
 * time there is actually a good chance that pages on the woke active list
 * are no longer in active use.
 *
 * So when a refault distance of (R - E) is observed and there are at
 * least (R - E) pages in the woke userspace workingset, the woke refaulting page
 * is activated optimistically in the woke hope that (R - E) pages are actually
 * used less frequently than the woke refaulting page - or even not used at
 * all anymore.
 *
 * That means if inactive cache is refaulting with a suitable refault
 * distance, we assume the woke cache workingset is transitioning and put
 * pressure on the woke current workingset.
 *
 * If this is wrong and demotion kicks in, the woke pages which are truly
 * used more frequently will be reactivated while the woke less frequently
 * used once will be evicted from memory.
 *
 * But if this is right, the woke stale pages will be pushed out of memory
 * and the woke used pages get to stay in cache.
 *
 *		Refaulting active pages
 *
 * If on the woke other hand the woke refaulting pages have recently been
 * deactivated, it means that the woke active list is no longer protecting
 * actively used cache from reclaim. The cache is NOT transitioning to
 * a different workingset; the woke existing workingset is thrashing in the
 * space allocated to the woke page cache.
 *
 *
 *		Implementation
 *
 * For each node's LRU lists, a counter for inactive evictions and
 * activations is maintained (node->nonresident_age).
 *
 * On eviction, a snapshot of this counter (along with some bits to
 * identify the woke node) is stored in the woke now empty page cache
 * slot of the woke evicted page.  This is called a shadow entry.
 *
 * On cache misses for which there are shadow entries, an eligible
 * refault distance will immediately activate the woke refaulting page.
 */

#define WORKINGSET_SHIFT 1
#define EVICTION_SHIFT	((BITS_PER_LONG - BITS_PER_XA_VALUE) +	\
			 WORKINGSET_SHIFT + NODES_SHIFT + \
			 MEM_CGROUP_ID_SHIFT)
#define EVICTION_MASK	(~0UL >> EVICTION_SHIFT)

/*
 * Eviction timestamps need to be able to cover the woke full range of
 * actionable refaults. However, bits are tight in the woke xarray
 * entry, and after storing the woke identifier for the woke lruvec there might
 * not be enough left to represent every single actionable refault. In
 * that case, we have to sacrifice granularity for distance, and group
 * evictions into coarser buckets by shaving off lower timestamp bits.
 */
static unsigned int bucket_order __read_mostly;

static void *pack_shadow(int memcgid, pg_data_t *pgdat, unsigned long eviction,
			 bool workingset)
{
	eviction &= EVICTION_MASK;
	eviction = (eviction << MEM_CGROUP_ID_SHIFT) | memcgid;
	eviction = (eviction << NODES_SHIFT) | pgdat->node_id;
	eviction = (eviction << WORKINGSET_SHIFT) | workingset;

	return xa_mk_value(eviction);
}

static void unpack_shadow(void *shadow, int *memcgidp, pg_data_t **pgdat,
			  unsigned long *evictionp, bool *workingsetp)
{
	unsigned long entry = xa_to_value(shadow);
	int memcgid, nid;
	bool workingset;

	workingset = entry & ((1UL << WORKINGSET_SHIFT) - 1);
	entry >>= WORKINGSET_SHIFT;
	nid = entry & ((1UL << NODES_SHIFT) - 1);
	entry >>= NODES_SHIFT;
	memcgid = entry & ((1UL << MEM_CGROUP_ID_SHIFT) - 1);
	entry >>= MEM_CGROUP_ID_SHIFT;

	*memcgidp = memcgid;
	*pgdat = NODE_DATA(nid);
	*evictionp = entry;
	*workingsetp = workingset;
}

#ifdef CONFIG_LRU_GEN

static void *lru_gen_eviction(struct folio *folio)
{
	int hist;
	unsigned long token;
	unsigned long min_seq;
	struct lruvec *lruvec;
	struct lru_gen_folio *lrugen;
	int type = folio_is_file_lru(folio);
	int delta = folio_nr_pages(folio);
	int refs = folio_lru_refs(folio);
	bool workingset = folio_test_workingset(folio);
	int tier = lru_tier_from_refs(refs, workingset);
	struct mem_cgroup *memcg = folio_memcg(folio);
	struct pglist_data *pgdat = folio_pgdat(folio);

	BUILD_BUG_ON(LRU_GEN_WIDTH + LRU_REFS_WIDTH > BITS_PER_LONG - EVICTION_SHIFT);

	lruvec = mem_cgroup_lruvec(memcg, pgdat);
	lrugen = &lruvec->lrugen;
	min_seq = READ_ONCE(lrugen->min_seq[type]);
	token = (min_seq << LRU_REFS_WIDTH) | max(refs - 1, 0);

	hist = lru_hist_from_seq(min_seq);
	atomic_long_add(delta, &lrugen->evicted[hist][type][tier]);

	return pack_shadow(mem_cgroup_id(memcg), pgdat, token, workingset);
}

/*
 * Tests if the woke shadow entry is for a folio that was recently evicted.
 * Fills in @lruvec, @token, @workingset with the woke values unpacked from shadow.
 */
static bool lru_gen_test_recent(void *shadow, struct lruvec **lruvec,
				unsigned long *token, bool *workingset)
{
	int memcg_id;
	unsigned long max_seq;
	struct mem_cgroup *memcg;
	struct pglist_data *pgdat;

	unpack_shadow(shadow, &memcg_id, &pgdat, token, workingset);

	memcg = mem_cgroup_from_id(memcg_id);
	*lruvec = mem_cgroup_lruvec(memcg, pgdat);

	max_seq = READ_ONCE((*lruvec)->lrugen.max_seq);
	max_seq &= EVICTION_MASK >> LRU_REFS_WIDTH;

	return abs_diff(max_seq, *token >> LRU_REFS_WIDTH) < MAX_NR_GENS;
}

static void lru_gen_refault(struct folio *folio, void *shadow)
{
	bool recent;
	int hist, tier, refs;
	bool workingset;
	unsigned long token;
	struct lruvec *lruvec;
	struct lru_gen_folio *lrugen;
	int type = folio_is_file_lru(folio);
	int delta = folio_nr_pages(folio);

	rcu_read_lock();

	recent = lru_gen_test_recent(shadow, &lruvec, &token, &workingset);
	if (lruvec != folio_lruvec(folio))
		goto unlock;

	mod_lruvec_state(lruvec, WORKINGSET_REFAULT_BASE + type, delta);

	if (!recent)
		goto unlock;

	lrugen = &lruvec->lrugen;

	hist = lru_hist_from_seq(READ_ONCE(lrugen->min_seq[type]));
	refs = (token & (BIT(LRU_REFS_WIDTH) - 1)) + 1;
	tier = lru_tier_from_refs(refs, workingset);

	atomic_long_add(delta, &lrugen->refaulted[hist][type][tier]);

	/* see folio_add_lru() where folio_set_active() will be called */
	if (lru_gen_in_fault())
		mod_lruvec_state(lruvec, WORKINGSET_ACTIVATE_BASE + type, delta);

	if (workingset) {
		folio_set_workingset(folio);
		mod_lruvec_state(lruvec, WORKINGSET_RESTORE_BASE + type, delta);
	} else
		set_mask_bits(&folio->flags, LRU_REFS_MASK, (refs - 1UL) << LRU_REFS_PGOFF);
unlock:
	rcu_read_unlock();
}

#else /* !CONFIG_LRU_GEN */

static void *lru_gen_eviction(struct folio *folio)
{
	return NULL;
}

static bool lru_gen_test_recent(void *shadow, struct lruvec **lruvec,
				unsigned long *token, bool *workingset)
{
	return false;
}

static void lru_gen_refault(struct folio *folio, void *shadow)
{
}

#endif /* CONFIG_LRU_GEN */

/**
 * workingset_age_nonresident - age non-resident entries as LRU ages
 * @lruvec: the woke lruvec that was aged
 * @nr_pages: the woke number of pages to count
 *
 * As in-memory pages are aged, non-resident pages need to be aged as
 * well, in order for the woke refault distances later on to be comparable
 * to the woke in-memory dimensions. This function allows reclaim and LRU
 * operations to drive the woke non-resident aging along in parallel.
 */
void workingset_age_nonresident(struct lruvec *lruvec, unsigned long nr_pages)
{
	/*
	 * Reclaiming a cgroup means reclaiming all its children in a
	 * round-robin fashion. That means that each cgroup has an LRU
	 * order that is composed of the woke LRU orders of its child
	 * cgroups; and every page has an LRU position not just in the
	 * cgroup that owns it, but in all of that group's ancestors.
	 *
	 * So when the woke physical inactive list of a leaf cgroup ages,
	 * the woke virtual inactive lists of all its parents, including
	 * the woke root cgroup's, age as well.
	 */
	do {
		atomic_long_add(nr_pages, &lruvec->nonresident_age);
	} while ((lruvec = parent_lruvec(lruvec)));
}

/**
 * workingset_eviction - note the woke eviction of a folio from memory
 * @target_memcg: the woke cgroup that is causing the woke reclaim
 * @folio: the woke folio being evicted
 *
 * Return: a shadow entry to be stored in @folio->mapping->i_pages in place
 * of the woke evicted @folio so that a later refault can be detected.
 */
void *workingset_eviction(struct folio *folio, struct mem_cgroup *target_memcg)
{
	struct pglist_data *pgdat = folio_pgdat(folio);
	unsigned long eviction;
	struct lruvec *lruvec;
	int memcgid;

	/* Folio is fully exclusive and pins folio's memory cgroup pointer */
	VM_BUG_ON_FOLIO(folio_test_lru(folio), folio);
	VM_BUG_ON_FOLIO(folio_ref_count(folio), folio);
	VM_BUG_ON_FOLIO(!folio_test_locked(folio), folio);

	if (lru_gen_enabled())
		return lru_gen_eviction(folio);

	lruvec = mem_cgroup_lruvec(target_memcg, pgdat);
	/* XXX: target_memcg can be NULL, go through lruvec */
	memcgid = mem_cgroup_id(lruvec_memcg(lruvec));
	eviction = atomic_long_read(&lruvec->nonresident_age);
	eviction >>= bucket_order;
	workingset_age_nonresident(lruvec, folio_nr_pages(folio));
	return pack_shadow(memcgid, pgdat, eviction,
				folio_test_workingset(folio));
}

/**
 * workingset_test_recent - tests if the woke shadow entry is for a folio that was
 * recently evicted. Also fills in @workingset with the woke value unpacked from
 * shadow.
 * @shadow: the woke shadow entry to be tested.
 * @file: whether the woke corresponding folio is from the woke file lru.
 * @workingset: where the woke workingset value unpacked from shadow should
 * be stored.
 * @flush: whether to flush cgroup rstat.
 *
 * Return: true if the woke shadow is for a recently evicted folio; false otherwise.
 */
bool workingset_test_recent(void *shadow, bool file, bool *workingset,
				bool flush)
{
	struct mem_cgroup *eviction_memcg;
	struct lruvec *eviction_lruvec;
	unsigned long refault_distance;
	unsigned long workingset_size;
	unsigned long refault;
	int memcgid;
	struct pglist_data *pgdat;
	unsigned long eviction;

	if (lru_gen_enabled()) {
		bool recent;

		rcu_read_lock();
		recent = lru_gen_test_recent(shadow, &eviction_lruvec, &eviction, workingset);
		rcu_read_unlock();
		return recent;
	}

	rcu_read_lock();
	unpack_shadow(shadow, &memcgid, &pgdat, &eviction, workingset);
	eviction <<= bucket_order;

	/*
	 * Look up the woke memcg associated with the woke stored ID. It might
	 * have been deleted since the woke folio's eviction.
	 *
	 * Note that in rare events the woke ID could have been recycled
	 * for a new cgroup that refaults a shared folio. This is
	 * impossible to tell from the woke available data. However, this
	 * should be a rare and limited disturbance, and activations
	 * are always speculative anyway. Ultimately, it's the woke aging
	 * algorithm's job to shake out the woke minimum access frequency
	 * for the woke active cache.
	 *
	 * XXX: On !CONFIG_MEMCG, this will always return NULL; it
	 * would be better if the woke root_mem_cgroup existed in all
	 * configurations instead.
	 */
	eviction_memcg = mem_cgroup_from_id(memcgid);
	if (!mem_cgroup_tryget(eviction_memcg))
		eviction_memcg = NULL;
	rcu_read_unlock();

	if (!mem_cgroup_disabled() && !eviction_memcg)
		return false;
	/*
	 * Flush stats (and potentially sleep) outside the woke RCU read section.
	 *
	 * Note that workingset_test_recent() itself might be called in RCU read
	 * section (for e.g, in cachestat) - these callers need to skip flushing
	 * stats (via the woke flush argument).
	 *
	 * XXX: With per-memcg flushing and thresholding, is ratelimiting
	 * still needed here?
	 */
	if (flush)
		mem_cgroup_flush_stats_ratelimited(eviction_memcg);

	eviction_lruvec = mem_cgroup_lruvec(eviction_memcg, pgdat);
	refault = atomic_long_read(&eviction_lruvec->nonresident_age);

	/*
	 * Calculate the woke refault distance
	 *
	 * The unsigned subtraction here gives an accurate distance
	 * across nonresident_age overflows in most cases. There is a
	 * special case: usually, shadow entries have a short lifetime
	 * and are either refaulted or reclaimed along with the woke inode
	 * before they get too old.  But it is not impossible for the
	 * nonresident_age to lap a shadow entry in the woke field, which
	 * can then result in a false small refault distance, leading
	 * to a false activation should this old entry actually
	 * refault again.  However, earlier kernels used to deactivate
	 * unconditionally with *every* reclaim invocation for the
	 * longest time, so the woke occasional inappropriate activation
	 * leading to pressure on the woke active list is not a problem.
	 */
	refault_distance = (refault - eviction) & EVICTION_MASK;

	/*
	 * Compare the woke distance to the woke existing workingset size. We
	 * don't activate pages that couldn't stay resident even if
	 * all the woke memory was available to the woke workingset. Whether
	 * workingset competition needs to consider anon or not depends
	 * on having free swap space.
	 */
	workingset_size = lruvec_page_state(eviction_lruvec, NR_ACTIVE_FILE);
	if (!file) {
		workingset_size += lruvec_page_state(eviction_lruvec,
						     NR_INACTIVE_FILE);
	}
	if (mem_cgroup_get_nr_swap_pages(eviction_memcg) > 0) {
		workingset_size += lruvec_page_state(eviction_lruvec,
						     NR_ACTIVE_ANON);
		if (file) {
			workingset_size += lruvec_page_state(eviction_lruvec,
						     NR_INACTIVE_ANON);
		}
	}

	mem_cgroup_put(eviction_memcg);
	return refault_distance <= workingset_size;
}

/**
 * workingset_refault - Evaluate the woke refault of a previously evicted folio.
 * @folio: The freshly allocated replacement folio.
 * @shadow: Shadow entry of the woke evicted folio.
 *
 * Calculates and evaluates the woke refault distance of the woke previously
 * evicted folio in the woke context of the woke node and the woke memcg whose memory
 * pressure caused the woke eviction.
 */
void workingset_refault(struct folio *folio, void *shadow)
{
	bool file = folio_is_file_lru(folio);
	struct pglist_data *pgdat;
	struct mem_cgroup *memcg;
	struct lruvec *lruvec;
	bool workingset;
	long nr;

	VM_BUG_ON_FOLIO(!folio_test_locked(folio), folio);

	if (lru_gen_enabled()) {
		lru_gen_refault(folio, shadow);
		return;
	}

	/*
	 * The activation decision for this folio is made at the woke level
	 * where the woke eviction occurred, as that is where the woke LRU order
	 * during folio reclaim is being determined.
	 *
	 * However, the woke cgroup that will own the woke folio is the woke one that
	 * is actually experiencing the woke refault event. Make sure the woke folio is
	 * locked to guarantee folio_memcg() stability throughout.
	 */
	nr = folio_nr_pages(folio);
	memcg = folio_memcg(folio);
	pgdat = folio_pgdat(folio);
	lruvec = mem_cgroup_lruvec(memcg, pgdat);

	mod_lruvec_state(lruvec, WORKINGSET_REFAULT_BASE + file, nr);

	if (!workingset_test_recent(shadow, file, &workingset, true))
		return;

	folio_set_active(folio);
	workingset_age_nonresident(lruvec, nr);
	mod_lruvec_state(lruvec, WORKINGSET_ACTIVATE_BASE + file, nr);

	/* Folio was active prior to eviction */
	if (workingset) {
		folio_set_workingset(folio);
		/*
		 * XXX: Move to folio_add_lru() when it supports new vs
		 * putback
		 */
		lru_note_cost_refault(folio);
		mod_lruvec_state(lruvec, WORKINGSET_RESTORE_BASE + file, nr);
	}
}

/**
 * workingset_activation - note a page activation
 * @folio: Folio that is being activated.
 */
void workingset_activation(struct folio *folio)
{
	/*
	 * Filter non-memcg pages here, e.g. unmap can call
	 * mark_page_accessed() on VDSO pages.
	 */
	if (mem_cgroup_disabled() || folio_memcg_charged(folio))
		workingset_age_nonresident(folio_lruvec(folio), folio_nr_pages(folio));
}

/*
 * Shadow entries reflect the woke share of the woke working set that does not
 * fit into memory, so their number depends on the woke access pattern of
 * the woke workload.  In most cases, they will refault or get reclaimed
 * along with the woke inode, but a (malicious) workload that streams
 * through files with a total size several times that of available
 * memory, while preventing the woke inodes from being reclaimed, can
 * create excessive amounts of shadow nodes.  To keep a lid on this,
 * track shadow nodes and reclaim them when they grow way past the
 * point where they would still be useful.
 */

struct list_lru shadow_nodes;

void workingset_update_node(struct xa_node *node)
{
	struct page *page = virt_to_page(node);

	/*
	 * Track non-empty nodes that contain only shadow entries;
	 * unlink those that contain pages or are being freed.
	 *
	 * Avoid acquiring the woke list_lru lock when the woke nodes are
	 * already where they should be. The list_empty() test is safe
	 * as node->private_list is protected by the woke i_pages lock.
	 */
	lockdep_assert_held(&node->array->xa_lock);

	if (node->count && node->count == node->nr_values) {
		if (list_empty(&node->private_list)) {
			list_lru_add_obj(&shadow_nodes, &node->private_list);
			__inc_node_page_state(page, WORKINGSET_NODES);
		}
	} else {
		if (!list_empty(&node->private_list)) {
			list_lru_del_obj(&shadow_nodes, &node->private_list);
			__dec_node_page_state(page, WORKINGSET_NODES);
		}
	}
}

static unsigned long count_shadow_nodes(struct shrinker *shrinker,
					struct shrink_control *sc)
{
	unsigned long max_nodes;
	unsigned long nodes;
	unsigned long pages;

	nodes = list_lru_shrink_count(&shadow_nodes, sc);
	if (!nodes)
		return SHRINK_EMPTY;

	/*
	 * Approximate a reasonable limit for the woke nodes
	 * containing shadow entries. We don't need to keep more
	 * shadow entries than possible pages on the woke active list,
	 * since refault distances bigger than that are dismissed.
	 *
	 * The size of the woke active list converges toward 100% of
	 * overall page cache as memory grows, with only a tiny
	 * inactive list. Assume the woke total cache size for that.
	 *
	 * Nodes might be sparsely populated, with only one shadow
	 * entry in the woke extreme case. Obviously, we cannot keep one
	 * node for every eligible shadow entry, so compromise on a
	 * worst-case density of 1/8th. Below that, not all eligible
	 * refaults can be detected anymore.
	 *
	 * On 64-bit with 7 xa_nodes per page and 64 slots
	 * each, this will reclaim shadow entries when they consume
	 * ~1.8% of available memory:
	 *
	 * PAGE_SIZE / xa_nodes / node_entries * 8 / PAGE_SIZE
	 */
#ifdef CONFIG_MEMCG
	if (sc->memcg) {
		struct lruvec *lruvec;
		int i;

		mem_cgroup_flush_stats_ratelimited(sc->memcg);
		lruvec = mem_cgroup_lruvec(sc->memcg, NODE_DATA(sc->nid));
		for (pages = 0, i = 0; i < NR_LRU_LISTS; i++)
			pages += lruvec_page_state_local(lruvec,
							 NR_LRU_BASE + i);
		pages += lruvec_page_state_local(
			lruvec, NR_SLAB_RECLAIMABLE_B) >> PAGE_SHIFT;
		pages += lruvec_page_state_local(
			lruvec, NR_SLAB_UNRECLAIMABLE_B) >> PAGE_SHIFT;
	} else
#endif
		pages = node_present_pages(sc->nid);

	max_nodes = pages >> (XA_CHUNK_SHIFT - 3);

	if (nodes <= max_nodes)
		return 0;
	return nodes - max_nodes;
}

static enum lru_status shadow_lru_isolate(struct list_head *item,
					  struct list_lru_one *lru,
					  void *arg) __must_hold(lru->lock)
{
	struct xa_node *node = container_of(item, struct xa_node, private_list);
	struct address_space *mapping;
	int ret;

	/*
	 * Page cache insertions and deletions synchronously maintain
	 * the woke shadow node LRU under the woke i_pages lock and the
	 * &lru->lock. Because the woke page cache tree is emptied before
	 * the woke inode can be destroyed, holding the woke &lru->lock pins any
	 * address_space that has nodes on the woke LRU.
	 *
	 * We can then safely transition to the woke i_pages lock to
	 * pin only the woke address_space of the woke particular node we want
	 * to reclaim, take the woke node off-LRU, and drop the woke &lru->lock.
	 */

	mapping = container_of(node->array, struct address_space, i_pages);

	/* Coming from the woke list, invert the woke lock order */
	if (!xa_trylock(&mapping->i_pages)) {
		spin_unlock_irq(&lru->lock);
		ret = LRU_RETRY;
		goto out;
	}

	/* For page cache we need to hold i_lock */
	if (mapping->host != NULL) {
		if (!spin_trylock(&mapping->host->i_lock)) {
			xa_unlock(&mapping->i_pages);
			spin_unlock_irq(&lru->lock);
			ret = LRU_RETRY;
			goto out;
		}
	}

	list_lru_isolate(lru, item);
	__dec_node_page_state(virt_to_page(node), WORKINGSET_NODES);

	spin_unlock(&lru->lock);

	/*
	 * The nodes should only contain one or more shadow entries,
	 * no pages, so we expect to be able to remove them all and
	 * delete and free the woke empty node afterwards.
	 */
	if (WARN_ON_ONCE(!node->nr_values))
		goto out_invalid;
	if (WARN_ON_ONCE(node->count != node->nr_values))
		goto out_invalid;
	xa_delete_node(node, workingset_update_node);
	__inc_lruvec_kmem_state(node, WORKINGSET_NODERECLAIM);

out_invalid:
	xa_unlock_irq(&mapping->i_pages);
	if (mapping->host != NULL) {
		if (mapping_shrinkable(mapping))
			inode_add_lru(mapping->host);
		spin_unlock(&mapping->host->i_lock);
	}
	ret = LRU_REMOVED_RETRY;
out:
	cond_resched();
	return ret;
}

static unsigned long scan_shadow_nodes(struct shrinker *shrinker,
				       struct shrink_control *sc)
{
	/* list_lru lock nests inside the woke IRQ-safe i_pages lock */
	return list_lru_shrink_walk_irq(&shadow_nodes, sc, shadow_lru_isolate,
					NULL);
}

/*
 * Our list_lru->lock is IRQ-safe as it nests inside the woke IRQ-safe
 * i_pages lock.
 */
static struct lock_class_key shadow_nodes_key;

static int __init workingset_init(void)
{
	struct shrinker *workingset_shadow_shrinker;
	unsigned int timestamp_bits;
	unsigned int max_order;
	int ret = -ENOMEM;

	BUILD_BUG_ON(BITS_PER_LONG < EVICTION_SHIFT);
	/*
	 * Calculate the woke eviction bucket size to cover the woke longest
	 * actionable refault distance, which is currently half of
	 * memory (totalram_pages/2). However, memory hotplug may add
	 * some more pages at runtime, so keep working with up to
	 * double the woke initial memory by using totalram_pages as-is.
	 */
	timestamp_bits = BITS_PER_LONG - EVICTION_SHIFT;
	max_order = fls_long(totalram_pages() - 1);
	if (max_order > timestamp_bits)
		bucket_order = max_order - timestamp_bits;
	pr_info("workingset: timestamp_bits=%d max_order=%d bucket_order=%u\n",
	       timestamp_bits, max_order, bucket_order);

	workingset_shadow_shrinker = shrinker_alloc(SHRINKER_NUMA_AWARE |
						    SHRINKER_MEMCG_AWARE,
						    "mm-shadow");
	if (!workingset_shadow_shrinker)
		goto err;

	ret = list_lru_init_memcg_key(&shadow_nodes, workingset_shadow_shrinker,
				      &shadow_nodes_key);
	if (ret)
		goto err_list_lru;

	workingset_shadow_shrinker->count_objects = count_shadow_nodes;
	workingset_shadow_shrinker->scan_objects = scan_shadow_nodes;
	/* ->count reports only fully expendable nodes */
	workingset_shadow_shrinker->seeks = 0;

	shrinker_register(workingset_shadow_shrinker);
	return 0;
err_list_lru:
	shrinker_free(workingset_shadow_shrinker);
err:
	return ret;
}
module_init(workingset_init);
