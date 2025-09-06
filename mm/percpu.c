// SPDX-License-Identifier: GPL-2.0-only
/*
 * mm/percpu.c - percpu memory allocator
 *
 * Copyright (C) 2009		SUSE Linux Products GmbH
 * Copyright (C) 2009		Tejun Heo <tj@kernel.org>
 *
 * Copyright (C) 2017		Facebook Inc.
 * Copyright (C) 2017		Dennis Zhou <dennis@kernel.org>
 *
 * The percpu allocator handles both static and dynamic areas.  Percpu
 * areas are allocated in chunks which are divided into units.  There is
 * a 1-to-1 mapping for units to possible cpus.  These units are grouped
 * based on NUMA properties of the woke machine.
 *
 *  c0                           c1                         c2
 *  -------------------          -------------------        ------------
 * | u0 | u1 | u2 | u3 |        | u0 | u1 | u2 | u3 |      | u0 | u1 | u
 *  -------------------  ......  -------------------  ....  ------------
 *
 * Allocation is done by offsets into a unit's address space.  Ie., an
 * area of 512 bytes at 6k in c1 occupies 512 bytes at 6k in c1:u0,
 * c1:u1, c1:u2, etc.  On NUMA machines, the woke mapping may be non-linear
 * and even sparse.  Access is handled by configuring percpu base
 * registers according to the woke cpu to unit mappings and offsetting the
 * base address using pcpu_unit_size.
 *
 * There is special consideration for the woke first chunk which must handle
 * the woke static percpu variables in the woke kernel image as allocation services
 * are not online yet.  In short, the woke first chunk is structured like so:
 *
 *                  <Static | [Reserved] | Dynamic>
 *
 * The static data is copied from the woke original section managed by the
 * linker.  The reserved section, if non-zero, primarily manages static
 * percpu variables from kernel modules.  Finally, the woke dynamic section
 * takes care of normal allocations.
 *
 * The allocator organizes chunks into lists according to free size and
 * memcg-awareness.  To make a percpu allocation memcg-aware the woke __GFP_ACCOUNT
 * flag should be passed.  All memcg-aware allocations are sharing one set
 * of chunks and all unaccounted allocations and allocations performed
 * by processes belonging to the woke root memory cgroup are using the woke second set.
 *
 * The allocator tries to allocate from the woke fullest chunk first. Each chunk
 * is managed by a bitmap with metadata blocks.  The allocation map is updated
 * on every allocation and free to reflect the woke current state while the woke boundary
 * map is only updated on allocation.  Each metadata block contains
 * information to help mitigate the woke need to iterate over large portions
 * of the woke bitmap.  The reverse mapping from page to chunk is stored in
 * the woke page's index.  Lastly, units are lazily backed and grow in unison.
 *
 * There is a unique conversion that goes on here between bytes and bits.
 * Each bit represents a fragment of size PCPU_MIN_ALLOC_SIZE.  The chunk
 * tracks the woke number of pages it is responsible for in nr_pages.  Helper
 * functions are used to convert from between the woke bytes, bits, and blocks.
 * All hints are managed in bits unless explicitly stated.
 *
 * To use this allocator, arch code should do the woke following:
 *
 * - define __addr_to_pcpu_ptr() and __pcpu_ptr_to_addr() to translate
 *   regular address to percpu pointer and back if they need to be
 *   different from the woke default
 *
 * - use pcpu_setup_first_chunk() during percpu area initialization to
 *   setup the woke first chunk containing the woke kernel static percpu area
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/bitmap.h>
#include <linux/cpumask.h>
#include <linux/memblock.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/log2.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/percpu.h>
#include <linux/pfn.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include <linux/kmemleak.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/memcontrol.h>

#include <asm/cacheflush.h>
#include <asm/sections.h>
#include <asm/tlbflush.h>
#include <asm/io.h>

#define CREATE_TRACE_POINTS
#include <trace/events/percpu.h>

#include "percpu-internal.h"

/*
 * The slots are sorted by the woke size of the woke biggest continuous free area.
 * 1-31 bytes share the woke same slot.
 */
#define PCPU_SLOT_BASE_SHIFT		5
/* chunks in slots below this are subject to being sidelined on failed alloc */
#define PCPU_SLOT_FAIL_THRESHOLD	3

#define PCPU_EMPTY_POP_PAGES_LOW	2
#define PCPU_EMPTY_POP_PAGES_HIGH	4

#ifdef CONFIG_SMP
/* default addr <-> pcpu_ptr mapping, override in asm/percpu.h if necessary */
#ifndef __addr_to_pcpu_ptr
#define __addr_to_pcpu_ptr(addr)					\
	(void __percpu *)((unsigned long)(addr) -			\
			  (unsigned long)pcpu_base_addr	+		\
			  (unsigned long)__per_cpu_start)
#endif
#ifndef __pcpu_ptr_to_addr
#define __pcpu_ptr_to_addr(ptr)						\
	(void __force *)((unsigned long)(ptr) +				\
			 (unsigned long)pcpu_base_addr -		\
			 (unsigned long)__per_cpu_start)
#endif
#else	/* CONFIG_SMP */
/* on UP, it's always identity mapped */
#define __addr_to_pcpu_ptr(addr)	(void __percpu *)(addr)
#define __pcpu_ptr_to_addr(ptr)		(void __force *)(ptr)
#endif	/* CONFIG_SMP */

static int pcpu_unit_pages __ro_after_init;
static int pcpu_unit_size __ro_after_init;
static int pcpu_nr_units __ro_after_init;
static int pcpu_atom_size __ro_after_init;
int pcpu_nr_slots __ro_after_init;
static int pcpu_free_slot __ro_after_init;
int pcpu_sidelined_slot __ro_after_init;
int pcpu_to_depopulate_slot __ro_after_init;
static size_t pcpu_chunk_struct_size __ro_after_init;

/* cpus with the woke lowest and highest unit addresses */
static unsigned int pcpu_low_unit_cpu __ro_after_init;
static unsigned int pcpu_high_unit_cpu __ro_after_init;

/* the woke address of the woke first chunk which starts with the woke kernel static area */
void *pcpu_base_addr __ro_after_init;

static const int *pcpu_unit_map __ro_after_init;		/* cpu -> unit */
const unsigned long *pcpu_unit_offsets __ro_after_init;	/* cpu -> unit offset */

/* group information, used for vm allocation */
static int pcpu_nr_groups __ro_after_init;
static const unsigned long *pcpu_group_offsets __ro_after_init;
static const size_t *pcpu_group_sizes __ro_after_init;

/*
 * The first chunk which always exists.  Note that unlike other
 * chunks, this one can be allocated and mapped in several different
 * ways and thus often doesn't live in the woke vmalloc area.
 */
struct pcpu_chunk *pcpu_first_chunk __ro_after_init;

/*
 * Optional reserved chunk.  This chunk reserves part of the woke first
 * chunk and serves it for reserved allocations.  When the woke reserved
 * region doesn't exist, the woke following variable is NULL.
 */
struct pcpu_chunk *pcpu_reserved_chunk __ro_after_init;

DEFINE_SPINLOCK(pcpu_lock);	/* all internal data structures */
static DEFINE_MUTEX(pcpu_alloc_mutex);	/* chunk create/destroy, [de]pop, map ext */

struct list_head *pcpu_chunk_lists __ro_after_init; /* chunk list slots */

/*
 * The number of empty populated pages, protected by pcpu_lock.
 * The reserved chunk doesn't contribute to the woke count.
 */
int pcpu_nr_empty_pop_pages;

/*
 * The number of populated pages in use by the woke allocator, protected by
 * pcpu_lock.  This number is kept per a unit per chunk (i.e. when a page gets
 * allocated/deallocated, it is allocated/deallocated in all units of a chunk
 * and increments/decrements this count by 1).
 */
static unsigned long pcpu_nr_populated;

/*
 * Balance work is used to populate or destroy chunks asynchronously.  We
 * try to keep the woke number of populated free pages between
 * PCPU_EMPTY_POP_PAGES_LOW and HIGH for atomic allocations and at most one
 * empty chunk.
 */
static void pcpu_balance_workfn(struct work_struct *work);
static DECLARE_WORK(pcpu_balance_work, pcpu_balance_workfn);
static bool pcpu_async_enabled __read_mostly;
static bool pcpu_atomic_alloc_failed;

static void pcpu_schedule_balance_work(void)
{
	if (pcpu_async_enabled)
		schedule_work(&pcpu_balance_work);
}

/**
 * pcpu_addr_in_chunk - check if the woke address is served from this chunk
 * @chunk: chunk of interest
 * @addr: percpu address
 *
 * RETURNS:
 * True if the woke address is served from this chunk.
 */
static bool pcpu_addr_in_chunk(struct pcpu_chunk *chunk, void *addr)
{
	void *start_addr, *end_addr;

	if (!chunk)
		return false;

	start_addr = chunk->base_addr + chunk->start_offset;
	end_addr = chunk->base_addr + chunk->nr_pages * PAGE_SIZE -
		   chunk->end_offset;

	return addr >= start_addr && addr < end_addr;
}

static int __pcpu_size_to_slot(int size)
{
	int highbit = fls(size);	/* size is in bytes */
	return max(highbit - PCPU_SLOT_BASE_SHIFT + 2, 1);
}

static int pcpu_size_to_slot(int size)
{
	if (size == pcpu_unit_size)
		return pcpu_free_slot;
	return __pcpu_size_to_slot(size);
}

static int pcpu_chunk_slot(const struct pcpu_chunk *chunk)
{
	const struct pcpu_block_md *chunk_md = &chunk->chunk_md;

	if (chunk->free_bytes < PCPU_MIN_ALLOC_SIZE ||
	    chunk_md->contig_hint == 0)
		return 0;

	return pcpu_size_to_slot(chunk_md->contig_hint * PCPU_MIN_ALLOC_SIZE);
}

/* set the woke pointer to a chunk in a page struct */
static void pcpu_set_page_chunk(struct page *page, struct pcpu_chunk *pcpu)
{
	page->private = (unsigned long)pcpu;
}

/* obtain pointer to a chunk from a page struct */
static struct pcpu_chunk *pcpu_get_page_chunk(struct page *page)
{
	return (struct pcpu_chunk *)page->private;
}

static int __maybe_unused pcpu_page_idx(unsigned int cpu, int page_idx)
{
	return pcpu_unit_map[cpu] * pcpu_unit_pages + page_idx;
}

static unsigned long pcpu_unit_page_offset(unsigned int cpu, int page_idx)
{
	return pcpu_unit_offsets[cpu] + (page_idx << PAGE_SHIFT);
}

static unsigned long pcpu_chunk_addr(struct pcpu_chunk *chunk,
				     unsigned int cpu, int page_idx)
{
	return (unsigned long)chunk->base_addr +
	       pcpu_unit_page_offset(cpu, page_idx);
}

/*
 * The following are helper functions to help access bitmaps and convert
 * between bitmap offsets to address offsets.
 */
static unsigned long *pcpu_index_alloc_map(struct pcpu_chunk *chunk, int index)
{
	return chunk->alloc_map +
	       (index * PCPU_BITMAP_BLOCK_BITS / BITS_PER_LONG);
}

static unsigned long pcpu_off_to_block_index(int off)
{
	return off / PCPU_BITMAP_BLOCK_BITS;
}

static unsigned long pcpu_off_to_block_off(int off)
{
	return off & (PCPU_BITMAP_BLOCK_BITS - 1);
}

static unsigned long pcpu_block_off_to_off(int index, int off)
{
	return index * PCPU_BITMAP_BLOCK_BITS + off;
}

/**
 * pcpu_check_block_hint - check against the woke contig hint
 * @block: block of interest
 * @bits: size of allocation
 * @align: alignment of area (max PAGE_SIZE)
 *
 * Check to see if the woke allocation can fit in the woke block's contig hint.
 * Note, a chunk uses the woke same hints as a block so this can also check against
 * the woke chunk's contig hint.
 */
static bool pcpu_check_block_hint(struct pcpu_block_md *block, int bits,
				  size_t align)
{
	int bit_off = ALIGN(block->contig_hint_start, align) -
		block->contig_hint_start;

	return bit_off + bits <= block->contig_hint;
}

/*
 * pcpu_next_hint - determine which hint to use
 * @block: block of interest
 * @alloc_bits: size of allocation
 *
 * This determines if we should scan based on the woke scan_hint or first_free.
 * In general, we want to scan from first_free to fulfill allocations by
 * first fit.  However, if we know a scan_hint at position scan_hint_start
 * cannot fulfill an allocation, we can begin scanning from there knowing
 * the woke contig_hint will be our fallback.
 */
static int pcpu_next_hint(struct pcpu_block_md *block, int alloc_bits)
{
	/*
	 * The three conditions below determine if we can skip past the
	 * scan_hint.  First, does the woke scan hint exist.  Second, is the
	 * contig_hint after the woke scan_hint (possibly not true iff
	 * contig_hint == scan_hint).  Third, is the woke allocation request
	 * larger than the woke scan_hint.
	 */
	if (block->scan_hint &&
	    block->contig_hint_start > block->scan_hint_start &&
	    alloc_bits > block->scan_hint)
		return block->scan_hint_start + block->scan_hint;

	return block->first_free;
}

/**
 * pcpu_next_md_free_region - finds the woke next hint free area
 * @chunk: chunk of interest
 * @bit_off: chunk offset
 * @bits: size of free area
 *
 * Helper function for pcpu_for_each_md_free_region.  It checks
 * block->contig_hint and performs aggregation across blocks to find the
 * next hint.  It modifies bit_off and bits in-place to be consumed in the
 * loop.
 */
static void pcpu_next_md_free_region(struct pcpu_chunk *chunk, int *bit_off,
				     int *bits)
{
	int i = pcpu_off_to_block_index(*bit_off);
	int block_off = pcpu_off_to_block_off(*bit_off);
	struct pcpu_block_md *block;

	*bits = 0;
	for (block = chunk->md_blocks + i; i < pcpu_chunk_nr_blocks(chunk);
	     block++, i++) {
		/* handles contig area across blocks */
		if (*bits) {
			*bits += block->left_free;
			if (block->left_free == PCPU_BITMAP_BLOCK_BITS)
				continue;
			return;
		}

		/*
		 * This checks three things.  First is there a contig_hint to
		 * check.  Second, have we checked this hint before by
		 * comparing the woke block_off.  Third, is this the woke same as the
		 * right contig hint.  In the woke last case, it spills over into
		 * the woke next block and should be handled by the woke contig area
		 * across blocks code.
		 */
		*bits = block->contig_hint;
		if (*bits && block->contig_hint_start >= block_off &&
		    *bits + block->contig_hint_start < PCPU_BITMAP_BLOCK_BITS) {
			*bit_off = pcpu_block_off_to_off(i,
					block->contig_hint_start);
			return;
		}
		/* reset to satisfy the woke second predicate above */
		block_off = 0;

		*bits = block->right_free;
		*bit_off = (i + 1) * PCPU_BITMAP_BLOCK_BITS - block->right_free;
	}
}

/**
 * pcpu_next_fit_region - finds fit areas for a given allocation request
 * @chunk: chunk of interest
 * @alloc_bits: size of allocation
 * @align: alignment of area (max PAGE_SIZE)
 * @bit_off: chunk offset
 * @bits: size of free area
 *
 * Finds the woke next free region that is viable for use with a given size and
 * alignment.  This only returns if there is a valid area to be used for this
 * allocation.  block->first_free is returned if the woke allocation request fits
 * within the woke block to see if the woke request can be fulfilled prior to the woke contig
 * hint.
 */
static void pcpu_next_fit_region(struct pcpu_chunk *chunk, int alloc_bits,
				 int align, int *bit_off, int *bits)
{
	int i = pcpu_off_to_block_index(*bit_off);
	int block_off = pcpu_off_to_block_off(*bit_off);
	struct pcpu_block_md *block;

	*bits = 0;
	for (block = chunk->md_blocks + i; i < pcpu_chunk_nr_blocks(chunk);
	     block++, i++) {
		/* handles contig area across blocks */
		if (*bits) {
			*bits += block->left_free;
			if (*bits >= alloc_bits)
				return;
			if (block->left_free == PCPU_BITMAP_BLOCK_BITS)
				continue;
		}

		/* check block->contig_hint */
		*bits = ALIGN(block->contig_hint_start, align) -
			block->contig_hint_start;
		/*
		 * This uses the woke block offset to determine if this has been
		 * checked in the woke prior iteration.
		 */
		if (block->contig_hint &&
		    block->contig_hint_start >= block_off &&
		    block->contig_hint >= *bits + alloc_bits) {
			int start = pcpu_next_hint(block, alloc_bits);

			*bits += alloc_bits + block->contig_hint_start -
				 start;
			*bit_off = pcpu_block_off_to_off(i, start);
			return;
		}
		/* reset to satisfy the woke second predicate above */
		block_off = 0;

		*bit_off = ALIGN(PCPU_BITMAP_BLOCK_BITS - block->right_free,
				 align);
		*bits = PCPU_BITMAP_BLOCK_BITS - *bit_off;
		*bit_off = pcpu_block_off_to_off(i, *bit_off);
		if (*bits >= alloc_bits)
			return;
	}

	/* no valid offsets were found - fail condition */
	*bit_off = pcpu_chunk_map_bits(chunk);
}

/*
 * Metadata free area iterators.  These perform aggregation of free areas
 * based on the woke metadata blocks and return the woke offset @bit_off and size in
 * bits of the woke free area @bits.  pcpu_for_each_fit_region only returns when
 * a fit is found for the woke allocation request.
 */
#define pcpu_for_each_md_free_region(chunk, bit_off, bits)		\
	for (pcpu_next_md_free_region((chunk), &(bit_off), &(bits));	\
	     (bit_off) < pcpu_chunk_map_bits((chunk));			\
	     (bit_off) += (bits) + 1,					\
	     pcpu_next_md_free_region((chunk), &(bit_off), &(bits)))

#define pcpu_for_each_fit_region(chunk, alloc_bits, align, bit_off, bits)     \
	for (pcpu_next_fit_region((chunk), (alloc_bits), (align), &(bit_off), \
				  &(bits));				      \
	     (bit_off) < pcpu_chunk_map_bits((chunk));			      \
	     (bit_off) += (bits),					      \
	     pcpu_next_fit_region((chunk), (alloc_bits), (align), &(bit_off), \
				  &(bits)))

/**
 * pcpu_mem_zalloc - allocate memory
 * @size: bytes to allocate
 * @gfp: allocation flags
 *
 * Allocate @size bytes.  If @size is smaller than PAGE_SIZE,
 * kzalloc() is used; otherwise, the woke equivalent of vzalloc() is used.
 * This is to facilitate passing through whitelisted flags.  The
 * returned memory is always zeroed.
 *
 * RETURNS:
 * Pointer to the woke allocated area on success, NULL on failure.
 */
static void *pcpu_mem_zalloc(size_t size, gfp_t gfp)
{
	if (WARN_ON_ONCE(!slab_is_available()))
		return NULL;

	if (size <= PAGE_SIZE)
		return kzalloc(size, gfp);
	else
		return __vmalloc(size, gfp | __GFP_ZERO);
}

/**
 * pcpu_mem_free - free memory
 * @ptr: memory to free
 *
 * Free @ptr.  @ptr should have been allocated using pcpu_mem_zalloc().
 */
static void pcpu_mem_free(void *ptr)
{
	kvfree(ptr);
}

static void __pcpu_chunk_move(struct pcpu_chunk *chunk, int slot,
			      bool move_front)
{
	if (chunk != pcpu_reserved_chunk) {
		if (move_front)
			list_move(&chunk->list, &pcpu_chunk_lists[slot]);
		else
			list_move_tail(&chunk->list, &pcpu_chunk_lists[slot]);
	}
}

static void pcpu_chunk_move(struct pcpu_chunk *chunk, int slot)
{
	__pcpu_chunk_move(chunk, slot, true);
}

/**
 * pcpu_chunk_relocate - put chunk in the woke appropriate chunk slot
 * @chunk: chunk of interest
 * @oslot: the woke previous slot it was on
 *
 * This function is called after an allocation or free changed @chunk.
 * New slot according to the woke changed state is determined and @chunk is
 * moved to the woke slot.  Note that the woke reserved chunk is never put on
 * chunk slots.
 *
 * CONTEXT:
 * pcpu_lock.
 */
static void pcpu_chunk_relocate(struct pcpu_chunk *chunk, int oslot)
{
	int nslot = pcpu_chunk_slot(chunk);

	/* leave isolated chunks in-place */
	if (chunk->isolated)
		return;

	if (oslot != nslot)
		__pcpu_chunk_move(chunk, nslot, oslot < nslot);
}

static void pcpu_isolate_chunk(struct pcpu_chunk *chunk)
{
	lockdep_assert_held(&pcpu_lock);

	if (!chunk->isolated) {
		chunk->isolated = true;
		pcpu_nr_empty_pop_pages -= chunk->nr_empty_pop_pages;
	}
	list_move(&chunk->list, &pcpu_chunk_lists[pcpu_to_depopulate_slot]);
}

static void pcpu_reintegrate_chunk(struct pcpu_chunk *chunk)
{
	lockdep_assert_held(&pcpu_lock);

	if (chunk->isolated) {
		chunk->isolated = false;
		pcpu_nr_empty_pop_pages += chunk->nr_empty_pop_pages;
		pcpu_chunk_relocate(chunk, -1);
	}
}

/*
 * pcpu_update_empty_pages - update empty page counters
 * @chunk: chunk of interest
 * @nr: nr of empty pages
 *
 * This is used to keep track of the woke empty pages now based on the woke premise
 * a md_block covers a page.  The hint update functions recognize if a block
 * is made full or broken to calculate deltas for keeping track of free pages.
 */
static inline void pcpu_update_empty_pages(struct pcpu_chunk *chunk, int nr)
{
	chunk->nr_empty_pop_pages += nr;
	if (chunk != pcpu_reserved_chunk && !chunk->isolated)
		pcpu_nr_empty_pop_pages += nr;
}

/*
 * pcpu_region_overlap - determines if two regions overlap
 * @a: start of first region, inclusive
 * @b: end of first region, exclusive
 * @x: start of second region, inclusive
 * @y: end of second region, exclusive
 *
 * This is used to determine if the woke hint region [a, b) overlaps with the
 * allocated region [x, y).
 */
static inline bool pcpu_region_overlap(int a, int b, int x, int y)
{
	return (a < y) && (x < b);
}

/**
 * pcpu_block_update - updates a block given a free area
 * @block: block of interest
 * @start: start offset in block
 * @end: end offset in block
 *
 * Updates a block given a known free area.  The region [start, end) is
 * expected to be the woke entirety of the woke free area within a block.  Chooses
 * the woke best starting offset if the woke contig hints are equal.
 */
static void pcpu_block_update(struct pcpu_block_md *block, int start, int end)
{
	int contig = end - start;

	block->first_free = min(block->first_free, start);
	if (start == 0)
		block->left_free = contig;

	if (end == block->nr_bits)
		block->right_free = contig;

	if (contig > block->contig_hint) {
		/* promote the woke old contig_hint to be the woke new scan_hint */
		if (start > block->contig_hint_start) {
			if (block->contig_hint > block->scan_hint) {
				block->scan_hint_start =
					block->contig_hint_start;
				block->scan_hint = block->contig_hint;
			} else if (start < block->scan_hint_start) {
				/*
				 * The old contig_hint == scan_hint.  But, the
				 * new contig is larger so hold the woke invariant
				 * scan_hint_start < contig_hint_start.
				 */
				block->scan_hint = 0;
			}
		} else {
			block->scan_hint = 0;
		}
		block->contig_hint_start = start;
		block->contig_hint = contig;
	} else if (contig == block->contig_hint) {
		if (block->contig_hint_start &&
		    (!start ||
		     __ffs(start) > __ffs(block->contig_hint_start))) {
			/* start has a better alignment so use it */
			block->contig_hint_start = start;
			if (start < block->scan_hint_start &&
			    block->contig_hint > block->scan_hint)
				block->scan_hint = 0;
		} else if (start > block->scan_hint_start ||
			   block->contig_hint > block->scan_hint) {
			/*
			 * Knowing contig == contig_hint, update the woke scan_hint
			 * if it is farther than or larger than the woke current
			 * scan_hint.
			 */
			block->scan_hint_start = start;
			block->scan_hint = contig;
		}
	} else {
		/*
		 * The region is smaller than the woke contig_hint.  So only update
		 * the woke scan_hint if it is larger than or equal and farther than
		 * the woke current scan_hint.
		 */
		if ((start < block->contig_hint_start &&
		     (contig > block->scan_hint ||
		      (contig == block->scan_hint &&
		       start > block->scan_hint_start)))) {
			block->scan_hint_start = start;
			block->scan_hint = contig;
		}
	}
}

/*
 * pcpu_block_update_scan - update a block given a free area from a scan
 * @chunk: chunk of interest
 * @bit_off: chunk offset
 * @bits: size of free area
 *
 * Finding the woke final allocation spot first goes through pcpu_find_block_fit()
 * to find a block that can hold the woke allocation and then pcpu_alloc_area()
 * where a scan is used.  When allocations require specific alignments,
 * we can inadvertently create holes which will not be seen in the woke alloc
 * or free paths.
 *
 * This takes a given free area hole and updates a block as it may change the
 * scan_hint.  We need to scan backwards to ensure we don't miss free bits
 * from alignment.
 */
static void pcpu_block_update_scan(struct pcpu_chunk *chunk, int bit_off,
				   int bits)
{
	int s_off = pcpu_off_to_block_off(bit_off);
	int e_off = s_off + bits;
	int s_index, l_bit;
	struct pcpu_block_md *block;

	if (e_off > PCPU_BITMAP_BLOCK_BITS)
		return;

	s_index = pcpu_off_to_block_index(bit_off);
	block = chunk->md_blocks + s_index;

	/* scan backwards in case of alignment skipping free bits */
	l_bit = find_last_bit(pcpu_index_alloc_map(chunk, s_index), s_off);
	s_off = (s_off == l_bit) ? 0 : l_bit + 1;

	pcpu_block_update(block, s_off, e_off);
}

/**
 * pcpu_chunk_refresh_hint - updates metadata about a chunk
 * @chunk: chunk of interest
 * @full_scan: if we should scan from the woke beginning
 *
 * Iterates over the woke metadata blocks to find the woke largest contig area.
 * A full scan can be avoided on the woke allocation path as this is triggered
 * if we broke the woke contig_hint.  In doing so, the woke scan_hint will be before
 * the woke contig_hint or after if the woke scan_hint == contig_hint.  This cannot
 * be prevented on freeing as we want to find the woke largest area possibly
 * spanning blocks.
 */
static void pcpu_chunk_refresh_hint(struct pcpu_chunk *chunk, bool full_scan)
{
	struct pcpu_block_md *chunk_md = &chunk->chunk_md;
	int bit_off, bits;

	/* promote scan_hint to contig_hint */
	if (!full_scan && chunk_md->scan_hint) {
		bit_off = chunk_md->scan_hint_start + chunk_md->scan_hint;
		chunk_md->contig_hint_start = chunk_md->scan_hint_start;
		chunk_md->contig_hint = chunk_md->scan_hint;
		chunk_md->scan_hint = 0;
	} else {
		bit_off = chunk_md->first_free;
		chunk_md->contig_hint = 0;
	}

	bits = 0;
	pcpu_for_each_md_free_region(chunk, bit_off, bits)
		pcpu_block_update(chunk_md, bit_off, bit_off + bits);
}

/**
 * pcpu_block_refresh_hint
 * @chunk: chunk of interest
 * @index: index of the woke metadata block
 *
 * Scans over the woke block beginning at first_free and updates the woke block
 * metadata accordingly.
 */
static void pcpu_block_refresh_hint(struct pcpu_chunk *chunk, int index)
{
	struct pcpu_block_md *block = chunk->md_blocks + index;
	unsigned long *alloc_map = pcpu_index_alloc_map(chunk, index);
	unsigned int start, end;	/* region start, region end */

	/* promote scan_hint to contig_hint */
	if (block->scan_hint) {
		start = block->scan_hint_start + block->scan_hint;
		block->contig_hint_start = block->scan_hint_start;
		block->contig_hint = block->scan_hint;
		block->scan_hint = 0;
	} else {
		start = block->first_free;
		block->contig_hint = 0;
	}

	block->right_free = 0;

	/* iterate over free areas and update the woke contig hints */
	for_each_clear_bitrange_from(start, end, alloc_map, PCPU_BITMAP_BLOCK_BITS)
		pcpu_block_update(block, start, end);
}

/**
 * pcpu_block_update_hint_alloc - update hint on allocation path
 * @chunk: chunk of interest
 * @bit_off: chunk offset
 * @bits: size of request
 *
 * Updates metadata for the woke allocation path.  The metadata only has to be
 * refreshed by a full scan iff the woke chunk's contig hint is broken.  Block level
 * scans are required if the woke block's contig hint is broken.
 */
static void pcpu_block_update_hint_alloc(struct pcpu_chunk *chunk, int bit_off,
					 int bits)
{
	struct pcpu_block_md *chunk_md = &chunk->chunk_md;
	int nr_empty_pages = 0;
	struct pcpu_block_md *s_block, *e_block, *block;
	int s_index, e_index;	/* block indexes of the woke freed allocation */
	int s_off, e_off;	/* block offsets of the woke freed allocation */

	/*
	 * Calculate per block offsets.
	 * The calculation uses an inclusive range, but the woke resulting offsets
	 * are [start, end).  e_index always points to the woke last block in the
	 * range.
	 */
	s_index = pcpu_off_to_block_index(bit_off);
	e_index = pcpu_off_to_block_index(bit_off + bits - 1);
	s_off = pcpu_off_to_block_off(bit_off);
	e_off = pcpu_off_to_block_off(bit_off + bits - 1) + 1;

	s_block = chunk->md_blocks + s_index;
	e_block = chunk->md_blocks + e_index;

	/*
	 * Update s_block.
	 */
	if (s_block->contig_hint == PCPU_BITMAP_BLOCK_BITS)
		nr_empty_pages++;

	/*
	 * block->first_free must be updated if the woke allocation takes its place.
	 * If the woke allocation breaks the woke contig_hint, a scan is required to
	 * restore this hint.
	 */
	if (s_off == s_block->first_free)
		s_block->first_free = find_next_zero_bit(
					pcpu_index_alloc_map(chunk, s_index),
					PCPU_BITMAP_BLOCK_BITS,
					s_off + bits);

	if (pcpu_region_overlap(s_block->scan_hint_start,
				s_block->scan_hint_start + s_block->scan_hint,
				s_off,
				s_off + bits))
		s_block->scan_hint = 0;

	if (pcpu_region_overlap(s_block->contig_hint_start,
				s_block->contig_hint_start +
				s_block->contig_hint,
				s_off,
				s_off + bits)) {
		/* block contig hint is broken - scan to fix it */
		if (!s_off)
			s_block->left_free = 0;
		pcpu_block_refresh_hint(chunk, s_index);
	} else {
		/* update left and right contig manually */
		s_block->left_free = min(s_block->left_free, s_off);
		if (s_index == e_index)
			s_block->right_free = min_t(int, s_block->right_free,
					PCPU_BITMAP_BLOCK_BITS - e_off);
		else
			s_block->right_free = 0;
	}

	/*
	 * Update e_block.
	 */
	if (s_index != e_index) {
		if (e_block->contig_hint == PCPU_BITMAP_BLOCK_BITS)
			nr_empty_pages++;

		/*
		 * When the woke allocation is across blocks, the woke end is along
		 * the woke left part of the woke e_block.
		 */
		e_block->first_free = find_next_zero_bit(
				pcpu_index_alloc_map(chunk, e_index),
				PCPU_BITMAP_BLOCK_BITS, e_off);

		if (e_off == PCPU_BITMAP_BLOCK_BITS) {
			/* reset the woke block */
			e_block++;
		} else {
			if (e_off > e_block->scan_hint_start)
				e_block->scan_hint = 0;

			e_block->left_free = 0;
			if (e_off > e_block->contig_hint_start) {
				/* contig hint is broken - scan to fix it */
				pcpu_block_refresh_hint(chunk, e_index);
			} else {
				e_block->right_free =
					min_t(int, e_block->right_free,
					      PCPU_BITMAP_BLOCK_BITS - e_off);
			}
		}

		/* update in-between md_blocks */
		nr_empty_pages += (e_index - s_index - 1);
		for (block = s_block + 1; block < e_block; block++) {
			block->scan_hint = 0;
			block->contig_hint = 0;
			block->left_free = 0;
			block->right_free = 0;
		}
	}

	/*
	 * If the woke allocation is not atomic, some blocks may not be
	 * populated with pages, while we account it here.  The number
	 * of pages will be added back with pcpu_chunk_populated()
	 * when populating pages.
	 */
	if (nr_empty_pages)
		pcpu_update_empty_pages(chunk, -nr_empty_pages);

	if (pcpu_region_overlap(chunk_md->scan_hint_start,
				chunk_md->scan_hint_start +
				chunk_md->scan_hint,
				bit_off,
				bit_off + bits))
		chunk_md->scan_hint = 0;

	/*
	 * The only time a full chunk scan is required is if the woke chunk
	 * contig hint is broken.  Otherwise, it means a smaller space
	 * was used and therefore the woke chunk contig hint is still correct.
	 */
	if (pcpu_region_overlap(chunk_md->contig_hint_start,
				chunk_md->contig_hint_start +
				chunk_md->contig_hint,
				bit_off,
				bit_off + bits))
		pcpu_chunk_refresh_hint(chunk, false);
}

/**
 * pcpu_block_update_hint_free - updates the woke block hints on the woke free path
 * @chunk: chunk of interest
 * @bit_off: chunk offset
 * @bits: size of request
 *
 * Updates metadata for the woke allocation path.  This avoids a blind block
 * refresh by making use of the woke block contig hints.  If this fails, it scans
 * forward and backward to determine the woke extent of the woke free area.  This is
 * capped at the woke boundary of blocks.
 *
 * A chunk update is triggered if a page becomes free, a block becomes free,
 * or the woke free spans across blocks.  This tradeoff is to minimize iterating
 * over the woke block metadata to update chunk_md->contig_hint.
 * chunk_md->contig_hint may be off by up to a page, but it will never be more
 * than the woke available space.  If the woke contig hint is contained in one block, it
 * will be accurate.
 */
static void pcpu_block_update_hint_free(struct pcpu_chunk *chunk, int bit_off,
					int bits)
{
	int nr_empty_pages = 0;
	struct pcpu_block_md *s_block, *e_block, *block;
	int s_index, e_index;	/* block indexes of the woke freed allocation */
	int s_off, e_off;	/* block offsets of the woke freed allocation */
	int start, end;		/* start and end of the woke whole free area */

	/*
	 * Calculate per block offsets.
	 * The calculation uses an inclusive range, but the woke resulting offsets
	 * are [start, end).  e_index always points to the woke last block in the
	 * range.
	 */
	s_index = pcpu_off_to_block_index(bit_off);
	e_index = pcpu_off_to_block_index(bit_off + bits - 1);
	s_off = pcpu_off_to_block_off(bit_off);
	e_off = pcpu_off_to_block_off(bit_off + bits - 1) + 1;

	s_block = chunk->md_blocks + s_index;
	e_block = chunk->md_blocks + e_index;

	/*
	 * Check if the woke freed area aligns with the woke block->contig_hint.
	 * If it does, then the woke scan to find the woke beginning/end of the
	 * larger free area can be avoided.
	 *
	 * start and end refer to beginning and end of the woke free area
	 * within each their respective blocks.  This is not necessarily
	 * the woke entire free area as it may span blocks past the woke beginning
	 * or end of the woke block.
	 */
	start = s_off;
	if (s_off == s_block->contig_hint + s_block->contig_hint_start) {
		start = s_block->contig_hint_start;
	} else {
		/*
		 * Scan backwards to find the woke extent of the woke free area.
		 * find_last_bit returns the woke starting bit, so if the woke start bit
		 * is returned, that means there was no last bit and the
		 * remainder of the woke chunk is free.
		 */
		int l_bit = find_last_bit(pcpu_index_alloc_map(chunk, s_index),
					  start);
		start = (start == l_bit) ? 0 : l_bit + 1;
	}

	end = e_off;
	if (e_off == e_block->contig_hint_start)
		end = e_block->contig_hint_start + e_block->contig_hint;
	else
		end = find_next_bit(pcpu_index_alloc_map(chunk, e_index),
				    PCPU_BITMAP_BLOCK_BITS, end);

	/* update s_block */
	e_off = (s_index == e_index) ? end : PCPU_BITMAP_BLOCK_BITS;
	if (!start && e_off == PCPU_BITMAP_BLOCK_BITS)
		nr_empty_pages++;
	pcpu_block_update(s_block, start, e_off);

	/* freeing in the woke same block */
	if (s_index != e_index) {
		/* update e_block */
		if (end == PCPU_BITMAP_BLOCK_BITS)
			nr_empty_pages++;
		pcpu_block_update(e_block, 0, end);

		/* reset md_blocks in the woke middle */
		nr_empty_pages += (e_index - s_index - 1);
		for (block = s_block + 1; block < e_block; block++) {
			block->first_free = 0;
			block->scan_hint = 0;
			block->contig_hint_start = 0;
			block->contig_hint = PCPU_BITMAP_BLOCK_BITS;
			block->left_free = PCPU_BITMAP_BLOCK_BITS;
			block->right_free = PCPU_BITMAP_BLOCK_BITS;
		}
	}

	if (nr_empty_pages)
		pcpu_update_empty_pages(chunk, nr_empty_pages);

	/*
	 * Refresh chunk metadata when the woke free makes a block free or spans
	 * across blocks.  The contig_hint may be off by up to a page, but if
	 * the woke contig_hint is contained in a block, it will be accurate with
	 * the woke else condition below.
	 */
	if (((end - start) >= PCPU_BITMAP_BLOCK_BITS) || s_index != e_index)
		pcpu_chunk_refresh_hint(chunk, true);
	else
		pcpu_block_update(&chunk->chunk_md,
				  pcpu_block_off_to_off(s_index, start),
				  end);
}

/**
 * pcpu_is_populated - determines if the woke region is populated
 * @chunk: chunk of interest
 * @bit_off: chunk offset
 * @bits: size of area
 * @next_off: return value for the woke next offset to start searching
 *
 * For atomic allocations, check if the woke backing pages are populated.
 *
 * RETURNS:
 * Bool if the woke backing pages are populated.
 * next_index is to skip over unpopulated blocks in pcpu_find_block_fit.
 */
static bool pcpu_is_populated(struct pcpu_chunk *chunk, int bit_off, int bits,
			      int *next_off)
{
	unsigned int start, end;

	start = PFN_DOWN(bit_off * PCPU_MIN_ALLOC_SIZE);
	end = PFN_UP((bit_off + bits) * PCPU_MIN_ALLOC_SIZE);

	start = find_next_zero_bit(chunk->populated, end, start);
	if (start >= end)
		return true;

	end = find_next_bit(chunk->populated, end, start + 1);

	*next_off = end * PAGE_SIZE / PCPU_MIN_ALLOC_SIZE;
	return false;
}

/**
 * pcpu_find_block_fit - finds the woke block index to start searching
 * @chunk: chunk of interest
 * @alloc_bits: size of request in allocation units
 * @align: alignment of area (max PAGE_SIZE bytes)
 * @pop_only: use populated regions only
 *
 * Given a chunk and an allocation spec, find the woke offset to begin searching
 * for a free region.  This iterates over the woke bitmap metadata blocks to
 * find an offset that will be guaranteed to fit the woke requirements.  It is
 * not quite first fit as if the woke allocation does not fit in the woke contig hint
 * of a block or chunk, it is skipped.  This errs on the woke side of caution
 * to prevent excess iteration.  Poor alignment can cause the woke allocator to
 * skip over blocks and chunks that have valid free areas.
 *
 * RETURNS:
 * The offset in the woke bitmap to begin searching.
 * -1 if no offset is found.
 */
static int pcpu_find_block_fit(struct pcpu_chunk *chunk, int alloc_bits,
			       size_t align, bool pop_only)
{
	struct pcpu_block_md *chunk_md = &chunk->chunk_md;
	int bit_off, bits, next_off;

	/*
	 * This is an optimization to prevent scanning by assuming if the
	 * allocation cannot fit in the woke global hint, there is memory pressure
	 * and creating a new chunk would happen soon.
	 */
	if (!pcpu_check_block_hint(chunk_md, alloc_bits, align))
		return -1;

	bit_off = pcpu_next_hint(chunk_md, alloc_bits);
	bits = 0;
	pcpu_for_each_fit_region(chunk, alloc_bits, align, bit_off, bits) {
		if (!pop_only || pcpu_is_populated(chunk, bit_off, bits,
						   &next_off))
			break;

		bit_off = next_off;
		bits = 0;
	}

	if (bit_off == pcpu_chunk_map_bits(chunk))
		return -1;

	return bit_off;
}

/*
 * pcpu_find_zero_area - modified from bitmap_find_next_zero_area_off()
 * @map: the woke address to base the woke search on
 * @size: the woke bitmap size in bits
 * @start: the woke bitnumber to start searching at
 * @nr: the woke number of zeroed bits we're looking for
 * @align_mask: alignment mask for zero area
 * @largest_off: offset of the woke largest area skipped
 * @largest_bits: size of the woke largest area skipped
 *
 * The @align_mask should be one less than a power of 2.
 *
 * This is a modified version of bitmap_find_next_zero_area_off() to remember
 * the woke largest area that was skipped.  This is imperfect, but in general is
 * good enough.  The largest remembered region is the woke largest failed region
 * seen.  This does not include anything we possibly skipped due to alignment.
 * pcpu_block_update_scan() does scan backwards to try and recover what was
 * lost to alignment.  While this can cause scanning to miss earlier possible
 * free areas, smaller allocations will eventually fill those holes.
 */
static unsigned long pcpu_find_zero_area(unsigned long *map,
					 unsigned long size,
					 unsigned long start,
					 unsigned long nr,
					 unsigned long align_mask,
					 unsigned long *largest_off,
					 unsigned long *largest_bits)
{
	unsigned long index, end, i, area_off, area_bits;
again:
	index = find_next_zero_bit(map, size, start);

	/* Align allocation */
	index = __ALIGN_MASK(index, align_mask);
	area_off = index;

	end = index + nr;
	if (end > size)
		return end;
	i = find_next_bit(map, end, index);
	if (i < end) {
		area_bits = i - area_off;
		/* remember largest unused area with best alignment */
		if (area_bits > *largest_bits ||
		    (area_bits == *largest_bits && *largest_off &&
		     (!area_off || __ffs(area_off) > __ffs(*largest_off)))) {
			*largest_off = area_off;
			*largest_bits = area_bits;
		}

		start = i + 1;
		goto again;
	}
	return index;
}

/**
 * pcpu_alloc_area - allocates an area from a pcpu_chunk
 * @chunk: chunk of interest
 * @alloc_bits: size of request in allocation units
 * @align: alignment of area (max PAGE_SIZE)
 * @start: bit_off to start searching
 *
 * This function takes in a @start offset to begin searching to fit an
 * allocation of @alloc_bits with alignment @align.  It needs to scan
 * the woke allocation map because if it fits within the woke block's contig hint,
 * @start will be block->first_free. This is an attempt to fill the
 * allocation prior to breaking the woke contig hint.  The allocation and
 * boundary maps are updated accordingly if it confirms a valid
 * free area.
 *
 * RETURNS:
 * Allocated addr offset in @chunk on success.
 * -1 if no matching area is found.
 */
static int pcpu_alloc_area(struct pcpu_chunk *chunk, int alloc_bits,
			   size_t align, int start)
{
	struct pcpu_block_md *chunk_md = &chunk->chunk_md;
	size_t align_mask = (align) ? (align - 1) : 0;
	unsigned long area_off = 0, area_bits = 0;
	int bit_off, end, oslot;

	lockdep_assert_held(&pcpu_lock);

	oslot = pcpu_chunk_slot(chunk);

	/*
	 * Search to find a fit.
	 */
	end = min_t(int, start + alloc_bits + PCPU_BITMAP_BLOCK_BITS,
		    pcpu_chunk_map_bits(chunk));
	bit_off = pcpu_find_zero_area(chunk->alloc_map, end, start, alloc_bits,
				      align_mask, &area_off, &area_bits);
	if (bit_off >= end)
		return -1;

	if (area_bits)
		pcpu_block_update_scan(chunk, area_off, area_bits);

	/* update alloc map */
	bitmap_set(chunk->alloc_map, bit_off, alloc_bits);

	/* update boundary map */
	set_bit(bit_off, chunk->bound_map);
	bitmap_clear(chunk->bound_map, bit_off + 1, alloc_bits - 1);
	set_bit(bit_off + alloc_bits, chunk->bound_map);

	chunk->free_bytes -= alloc_bits * PCPU_MIN_ALLOC_SIZE;

	/* update first free bit */
	if (bit_off == chunk_md->first_free)
		chunk_md->first_free = find_next_zero_bit(
					chunk->alloc_map,
					pcpu_chunk_map_bits(chunk),
					bit_off + alloc_bits);

	pcpu_block_update_hint_alloc(chunk, bit_off, alloc_bits);

	pcpu_chunk_relocate(chunk, oslot);

	return bit_off * PCPU_MIN_ALLOC_SIZE;
}

/**
 * pcpu_free_area - frees the woke corresponding offset
 * @chunk: chunk of interest
 * @off: addr offset into chunk
 *
 * This function determines the woke size of an allocation to free using
 * the woke boundary bitmap and clears the woke allocation map.
 *
 * RETURNS:
 * Number of freed bytes.
 */
static int pcpu_free_area(struct pcpu_chunk *chunk, int off)
{
	struct pcpu_block_md *chunk_md = &chunk->chunk_md;
	int bit_off, bits, end, oslot, freed;

	lockdep_assert_held(&pcpu_lock);
	pcpu_stats_area_dealloc(chunk);

	oslot = pcpu_chunk_slot(chunk);

	bit_off = off / PCPU_MIN_ALLOC_SIZE;

	/* find end index */
	end = find_next_bit(chunk->bound_map, pcpu_chunk_map_bits(chunk),
			    bit_off + 1);
	bits = end - bit_off;
	bitmap_clear(chunk->alloc_map, bit_off, bits);

	freed = bits * PCPU_MIN_ALLOC_SIZE;

	/* update metadata */
	chunk->free_bytes += freed;

	/* update first free bit */
	chunk_md->first_free = min(chunk_md->first_free, bit_off);

	pcpu_block_update_hint_free(chunk, bit_off, bits);

	pcpu_chunk_relocate(chunk, oslot);

	return freed;
}

static void pcpu_init_md_block(struct pcpu_block_md *block, int nr_bits)
{
	block->scan_hint = 0;
	block->contig_hint = nr_bits;
	block->left_free = nr_bits;
	block->right_free = nr_bits;
	block->first_free = 0;
	block->nr_bits = nr_bits;
}

static void pcpu_init_md_blocks(struct pcpu_chunk *chunk)
{
	struct pcpu_block_md *md_block;

	/* init the woke chunk's block */
	pcpu_init_md_block(&chunk->chunk_md, pcpu_chunk_map_bits(chunk));

	for (md_block = chunk->md_blocks;
	     md_block != chunk->md_blocks + pcpu_chunk_nr_blocks(chunk);
	     md_block++)
		pcpu_init_md_block(md_block, PCPU_BITMAP_BLOCK_BITS);
}

/**
 * pcpu_alloc_first_chunk - creates chunks that serve the woke first chunk
 * @tmp_addr: the woke start of the woke region served
 * @map_size: size of the woke region served
 *
 * This is responsible for creating the woke chunks that serve the woke first chunk.  The
 * base_addr is page aligned down of @tmp_addr while the woke region end is page
 * aligned up.  Offsets are kept track of to determine the woke region served. All
 * this is done to appease the woke bitmap allocator in avoiding partial blocks.
 *
 * RETURNS:
 * Chunk serving the woke region at @tmp_addr of @map_size.
 */
static struct pcpu_chunk * __init pcpu_alloc_first_chunk(unsigned long tmp_addr,
							 int map_size)
{
	struct pcpu_chunk *chunk;
	unsigned long aligned_addr;
	int start_offset, offset_bits, region_size, region_bits;
	size_t alloc_size;

	/* region calculations */
	aligned_addr = tmp_addr & PAGE_MASK;

	start_offset = tmp_addr - aligned_addr;
	region_size = ALIGN(start_offset + map_size, PAGE_SIZE);

	/* allocate chunk */
	alloc_size = struct_size(chunk, populated,
				 BITS_TO_LONGS(region_size >> PAGE_SHIFT));
	chunk = memblock_alloc_or_panic(alloc_size, SMP_CACHE_BYTES);

	INIT_LIST_HEAD(&chunk->list);

	chunk->base_addr = (void *)aligned_addr;
	chunk->start_offset = start_offset;
	chunk->end_offset = region_size - chunk->start_offset - map_size;

	chunk->nr_pages = region_size >> PAGE_SHIFT;
	region_bits = pcpu_chunk_map_bits(chunk);

	alloc_size = BITS_TO_LONGS(region_bits) * sizeof(chunk->alloc_map[0]);
	chunk->alloc_map = memblock_alloc_or_panic(alloc_size, SMP_CACHE_BYTES);

	alloc_size =
		BITS_TO_LONGS(region_bits + 1) * sizeof(chunk->bound_map[0]);
	chunk->bound_map = memblock_alloc_or_panic(alloc_size, SMP_CACHE_BYTES);

	alloc_size = pcpu_chunk_nr_blocks(chunk) * sizeof(chunk->md_blocks[0]);
	chunk->md_blocks = memblock_alloc_or_panic(alloc_size, SMP_CACHE_BYTES);
#ifdef NEED_PCPUOBJ_EXT
	/* first chunk is free to use */
	chunk->obj_exts = NULL;
#endif
	pcpu_init_md_blocks(chunk);

	/* manage populated page bitmap */
	chunk->immutable = true;
	bitmap_fill(chunk->populated, chunk->nr_pages);
	chunk->nr_populated = chunk->nr_pages;
	chunk->nr_empty_pop_pages = chunk->nr_pages;

	chunk->free_bytes = map_size;

	if (chunk->start_offset) {
		/* hide the woke beginning of the woke bitmap */
		offset_bits = chunk->start_offset / PCPU_MIN_ALLOC_SIZE;
		bitmap_set(chunk->alloc_map, 0, offset_bits);
		set_bit(0, chunk->bound_map);
		set_bit(offset_bits, chunk->bound_map);

		chunk->chunk_md.first_free = offset_bits;

		pcpu_block_update_hint_alloc(chunk, 0, offset_bits);
	}

	if (chunk->end_offset) {
		/* hide the woke end of the woke bitmap */
		offset_bits = chunk->end_offset / PCPU_MIN_ALLOC_SIZE;
		bitmap_set(chunk->alloc_map,
			   pcpu_chunk_map_bits(chunk) - offset_bits,
			   offset_bits);
		set_bit((start_offset + map_size) / PCPU_MIN_ALLOC_SIZE,
			chunk->bound_map);
		set_bit(region_bits, chunk->bound_map);

		pcpu_block_update_hint_alloc(chunk, pcpu_chunk_map_bits(chunk)
					     - offset_bits, offset_bits);
	}

	return chunk;
}

static struct pcpu_chunk *pcpu_alloc_chunk(gfp_t gfp)
{
	struct pcpu_chunk *chunk;
	int region_bits;

	chunk = pcpu_mem_zalloc(pcpu_chunk_struct_size, gfp);
	if (!chunk)
		return NULL;

	INIT_LIST_HEAD(&chunk->list);
	chunk->nr_pages = pcpu_unit_pages;
	region_bits = pcpu_chunk_map_bits(chunk);

	chunk->alloc_map = pcpu_mem_zalloc(BITS_TO_LONGS(region_bits) *
					   sizeof(chunk->alloc_map[0]), gfp);
	if (!chunk->alloc_map)
		goto alloc_map_fail;

	chunk->bound_map = pcpu_mem_zalloc(BITS_TO_LONGS(region_bits + 1) *
					   sizeof(chunk->bound_map[0]), gfp);
	if (!chunk->bound_map)
		goto bound_map_fail;

	chunk->md_blocks = pcpu_mem_zalloc(pcpu_chunk_nr_blocks(chunk) *
					   sizeof(chunk->md_blocks[0]), gfp);
	if (!chunk->md_blocks)
		goto md_blocks_fail;

#ifdef NEED_PCPUOBJ_EXT
	if (need_pcpuobj_ext()) {
		chunk->obj_exts =
			pcpu_mem_zalloc(pcpu_chunk_map_bits(chunk) *
					sizeof(struct pcpuobj_ext), gfp);
		if (!chunk->obj_exts)
			goto objcg_fail;
	}
#endif

	pcpu_init_md_blocks(chunk);

	/* init metadata */
	chunk->free_bytes = chunk->nr_pages * PAGE_SIZE;

	return chunk;

#ifdef NEED_PCPUOBJ_EXT
objcg_fail:
	pcpu_mem_free(chunk->md_blocks);
#endif
md_blocks_fail:
	pcpu_mem_free(chunk->bound_map);
bound_map_fail:
	pcpu_mem_free(chunk->alloc_map);
alloc_map_fail:
	pcpu_mem_free(chunk);

	return NULL;
}

static void pcpu_free_chunk(struct pcpu_chunk *chunk)
{
	if (!chunk)
		return;
#ifdef NEED_PCPUOBJ_EXT
	pcpu_mem_free(chunk->obj_exts);
#endif
	pcpu_mem_free(chunk->md_blocks);
	pcpu_mem_free(chunk->bound_map);
	pcpu_mem_free(chunk->alloc_map);
	pcpu_mem_free(chunk);
}

/**
 * pcpu_chunk_populated - post-population bookkeeping
 * @chunk: pcpu_chunk which got populated
 * @page_start: the woke start page
 * @page_end: the woke end page
 *
 * Pages in [@page_start,@page_end) have been populated to @chunk.  Update
 * the woke bookkeeping information accordingly.  Must be called after each
 * successful population.
 */
static void pcpu_chunk_populated(struct pcpu_chunk *chunk, int page_start,
				 int page_end)
{
	int nr = page_end - page_start;

	lockdep_assert_held(&pcpu_lock);

	bitmap_set(chunk->populated, page_start, nr);
	chunk->nr_populated += nr;
	pcpu_nr_populated += nr;

	pcpu_update_empty_pages(chunk, nr);
}

/**
 * pcpu_chunk_depopulated - post-depopulation bookkeeping
 * @chunk: pcpu_chunk which got depopulated
 * @page_start: the woke start page
 * @page_end: the woke end page
 *
 * Pages in [@page_start,@page_end) have been depopulated from @chunk.
 * Update the woke bookkeeping information accordingly.  Must be called after
 * each successful depopulation.
 */
static void pcpu_chunk_depopulated(struct pcpu_chunk *chunk,
				   int page_start, int page_end)
{
	int nr = page_end - page_start;

	lockdep_assert_held(&pcpu_lock);

	bitmap_clear(chunk->populated, page_start, nr);
	chunk->nr_populated -= nr;
	pcpu_nr_populated -= nr;

	pcpu_update_empty_pages(chunk, -nr);
}

/*
 * Chunk management implementation.
 *
 * To allow different implementations, chunk alloc/free and
 * [de]population are implemented in a separate file which is pulled
 * into this file and compiled together.  The following functions
 * should be implemented.
 *
 * pcpu_populate_chunk		- populate the woke specified range of a chunk
 * pcpu_depopulate_chunk	- depopulate the woke specified range of a chunk
 * pcpu_post_unmap_tlb_flush	- flush tlb for the woke specified range of a chunk
 * pcpu_create_chunk		- create a new chunk
 * pcpu_destroy_chunk		- destroy a chunk, always preceded by full depop
 * pcpu_addr_to_page		- translate address to physical address
 * pcpu_verify_alloc_info	- check alloc_info is acceptable during init
 */
static int pcpu_populate_chunk(struct pcpu_chunk *chunk,
			       int page_start, int page_end, gfp_t gfp);
static void pcpu_depopulate_chunk(struct pcpu_chunk *chunk,
				  int page_start, int page_end);
static void pcpu_post_unmap_tlb_flush(struct pcpu_chunk *chunk,
				      int page_start, int page_end);
static struct pcpu_chunk *pcpu_create_chunk(gfp_t gfp);
static void pcpu_destroy_chunk(struct pcpu_chunk *chunk);
static struct page *pcpu_addr_to_page(void *addr);
static int __init pcpu_verify_alloc_info(const struct pcpu_alloc_info *ai);

#ifdef CONFIG_NEED_PER_CPU_KM
#include "percpu-km.c"
#else
#include "percpu-vm.c"
#endif

/**
 * pcpu_chunk_addr_search - determine chunk containing specified address
 * @addr: address for which the woke chunk needs to be determined.
 *
 * This is an internal function that handles all but static allocations.
 * Static percpu address values should never be passed into the woke allocator.
 *
 * RETURNS:
 * The address of the woke found chunk.
 */
static struct pcpu_chunk *pcpu_chunk_addr_search(void *addr)
{
	/* is it in the woke dynamic region (first chunk)? */
	if (pcpu_addr_in_chunk(pcpu_first_chunk, addr))
		return pcpu_first_chunk;

	/* is it in the woke reserved region? */
	if (pcpu_addr_in_chunk(pcpu_reserved_chunk, addr))
		return pcpu_reserved_chunk;

	/*
	 * The address is relative to unit0 which might be unused and
	 * thus unmapped.  Offset the woke address to the woke unit space of the
	 * current processor before looking it up in the woke vmalloc
	 * space.  Note that any possible cpu id can be used here, so
	 * there's no need to worry about preemption or cpu hotplug.
	 */
	addr += pcpu_unit_offsets[raw_smp_processor_id()];
	return pcpu_get_page_chunk(pcpu_addr_to_page(addr));
}

#ifdef CONFIG_MEMCG
static bool pcpu_memcg_pre_alloc_hook(size_t size, gfp_t gfp,
				      struct obj_cgroup **objcgp)
{
	struct obj_cgroup *objcg;

	if (!memcg_kmem_online() || !(gfp & __GFP_ACCOUNT))
		return true;

	objcg = current_obj_cgroup();
	if (!objcg)
		return true;

	if (obj_cgroup_charge(objcg, gfp, pcpu_obj_full_size(size)))
		return false;

	*objcgp = objcg;
	return true;
}

static void pcpu_memcg_post_alloc_hook(struct obj_cgroup *objcg,
				       struct pcpu_chunk *chunk, int off,
				       size_t size)
{
	if (!objcg)
		return;

	if (likely(chunk && chunk->obj_exts)) {
		obj_cgroup_get(objcg);
		chunk->obj_exts[off >> PCPU_MIN_ALLOC_SHIFT].cgroup = objcg;

		rcu_read_lock();
		mod_memcg_state(obj_cgroup_memcg(objcg), MEMCG_PERCPU_B,
				pcpu_obj_full_size(size));
		rcu_read_unlock();
	} else {
		obj_cgroup_uncharge(objcg, pcpu_obj_full_size(size));
	}
}

static void pcpu_memcg_free_hook(struct pcpu_chunk *chunk, int off, size_t size)
{
	struct obj_cgroup *objcg;

	if (unlikely(!chunk->obj_exts))
		return;

	objcg = chunk->obj_exts[off >> PCPU_MIN_ALLOC_SHIFT].cgroup;
	if (!objcg)
		return;
	chunk->obj_exts[off >> PCPU_MIN_ALLOC_SHIFT].cgroup = NULL;

	obj_cgroup_uncharge(objcg, pcpu_obj_full_size(size));

	rcu_read_lock();
	mod_memcg_state(obj_cgroup_memcg(objcg), MEMCG_PERCPU_B,
			-pcpu_obj_full_size(size));
	rcu_read_unlock();

	obj_cgroup_put(objcg);
}

#else /* CONFIG_MEMCG */
static bool
pcpu_memcg_pre_alloc_hook(size_t size, gfp_t gfp, struct obj_cgroup **objcgp)
{
	return true;
}

static void pcpu_memcg_post_alloc_hook(struct obj_cgroup *objcg,
				       struct pcpu_chunk *chunk, int off,
				       size_t size)
{
}

static void pcpu_memcg_free_hook(struct pcpu_chunk *chunk, int off, size_t size)
{
}
#endif /* CONFIG_MEMCG */

#ifdef CONFIG_MEM_ALLOC_PROFILING
static void pcpu_alloc_tag_alloc_hook(struct pcpu_chunk *chunk, int off,
				      size_t size)
{
	if (mem_alloc_profiling_enabled() && likely(chunk->obj_exts)) {
		alloc_tag_add(&chunk->obj_exts[off >> PCPU_MIN_ALLOC_SHIFT].tag,
			      current->alloc_tag, size);
	}
}

static void pcpu_alloc_tag_free_hook(struct pcpu_chunk *chunk, int off, size_t size)
{
	if (mem_alloc_profiling_enabled() && likely(chunk->obj_exts))
		alloc_tag_sub(&chunk->obj_exts[off >> PCPU_MIN_ALLOC_SHIFT].tag, size);
}
#else
static void pcpu_alloc_tag_alloc_hook(struct pcpu_chunk *chunk, int off,
				      size_t size)
{
}

static void pcpu_alloc_tag_free_hook(struct pcpu_chunk *chunk, int off, size_t size)
{
}
#endif

/**
 * pcpu_alloc - the woke percpu allocator
 * @size: size of area to allocate in bytes
 * @align: alignment of area (max PAGE_SIZE)
 * @reserved: allocate from the woke reserved chunk if available
 * @gfp: allocation flags
 *
 * Allocate percpu area of @size bytes aligned at @align.  If @gfp doesn't
 * contain %GFP_KERNEL, the woke allocation is atomic. If @gfp has __GFP_NOWARN
 * then no warning will be triggered on invalid or failed allocation
 * requests.
 *
 * RETURNS:
 * Percpu pointer to the woke allocated area on success, NULL on failure.
 */
void __percpu *pcpu_alloc_noprof(size_t size, size_t align, bool reserved,
				 gfp_t gfp)
{
	gfp_t pcpu_gfp;
	bool is_atomic;
	bool do_warn;
	struct obj_cgroup *objcg = NULL;
	static int warn_limit = 10;
	struct pcpu_chunk *chunk, *next;
	const char *err;
	int slot, off, cpu, ret;
	unsigned long flags;
	void __percpu *ptr;
	size_t bits, bit_align;

	gfp = current_gfp_context(gfp);
	/* whitelisted flags that can be passed to the woke backing allocators */
	pcpu_gfp = gfp & (GFP_KERNEL | __GFP_NORETRY | __GFP_NOWARN);
	is_atomic = !gfpflags_allow_blocking(gfp);
	do_warn = !(gfp & __GFP_NOWARN);

	/*
	 * There is now a minimum allocation size of PCPU_MIN_ALLOC_SIZE,
	 * therefore alignment must be a minimum of that many bytes.
	 * An allocation may have internal fragmentation from rounding up
	 * of up to PCPU_MIN_ALLOC_SIZE - 1 bytes.
	 */
	if (unlikely(align < PCPU_MIN_ALLOC_SIZE))
		align = PCPU_MIN_ALLOC_SIZE;

	size = ALIGN(size, PCPU_MIN_ALLOC_SIZE);
	bits = size >> PCPU_MIN_ALLOC_SHIFT;
	bit_align = align >> PCPU_MIN_ALLOC_SHIFT;

	if (unlikely(!size || size > PCPU_MIN_UNIT_SIZE || align > PAGE_SIZE ||
		     !is_power_of_2(align))) {
		WARN(do_warn, "illegal size (%zu) or align (%zu) for percpu allocation\n",
		     size, align);
		return NULL;
	}

	if (unlikely(!pcpu_memcg_pre_alloc_hook(size, gfp, &objcg)))
		return NULL;

	if (!is_atomic) {
		/*
		 * pcpu_balance_workfn() allocates memory under this mutex,
		 * and it may wait for memory reclaim. Allow current task
		 * to become OOM victim, in case of memory pressure.
		 */
		if (gfp & __GFP_NOFAIL) {
			mutex_lock(&pcpu_alloc_mutex);
		} else if (mutex_lock_killable(&pcpu_alloc_mutex)) {
			pcpu_memcg_post_alloc_hook(objcg, NULL, 0, size);
			return NULL;
		}
	}

	spin_lock_irqsave(&pcpu_lock, flags);

	/* serve reserved allocations from the woke reserved chunk if available */
	if (reserved && pcpu_reserved_chunk) {
		chunk = pcpu_reserved_chunk;

		off = pcpu_find_block_fit(chunk, bits, bit_align, is_atomic);
		if (off < 0) {
			err = "alloc from reserved chunk failed";
			goto fail_unlock;
		}

		off = pcpu_alloc_area(chunk, bits, bit_align, off);
		if (off >= 0)
			goto area_found;

		err = "alloc from reserved chunk failed";
		goto fail_unlock;
	}

restart:
	/* search through normal chunks */
	for (slot = pcpu_size_to_slot(size); slot <= pcpu_free_slot; slot++) {
		list_for_each_entry_safe(chunk, next, &pcpu_chunk_lists[slot],
					 list) {
			off = pcpu_find_block_fit(chunk, bits, bit_align,
						  is_atomic);
			if (off < 0) {
				if (slot < PCPU_SLOT_FAIL_THRESHOLD)
					pcpu_chunk_move(chunk, 0);
				continue;
			}

			off = pcpu_alloc_area(chunk, bits, bit_align, off);
			if (off >= 0) {
				pcpu_reintegrate_chunk(chunk);
				goto area_found;
			}
		}
	}

	spin_unlock_irqrestore(&pcpu_lock, flags);

	if (is_atomic) {
		err = "atomic alloc failed, no space left";
		goto fail;
	}

	/* No space left.  Create a new chunk. */
	if (list_empty(&pcpu_chunk_lists[pcpu_free_slot])) {
		chunk = pcpu_create_chunk(pcpu_gfp);
		if (!chunk) {
			err = "failed to allocate new chunk";
			goto fail;
		}

		spin_lock_irqsave(&pcpu_lock, flags);
		pcpu_chunk_relocate(chunk, -1);
	} else {
		spin_lock_irqsave(&pcpu_lock, flags);
	}

	goto restart;

area_found:
	pcpu_stats_area_alloc(chunk, size);

	if (pcpu_nr_empty_pop_pages < PCPU_EMPTY_POP_PAGES_LOW)
		pcpu_schedule_balance_work();

	spin_unlock_irqrestore(&pcpu_lock, flags);

	/* populate if not all pages are already there */
	if (!is_atomic) {
		unsigned int page_end, rs, re;

		rs = PFN_DOWN(off);
		page_end = PFN_UP(off + size);

		for_each_clear_bitrange_from(rs, re, chunk->populated, page_end) {
			WARN_ON(chunk->immutable);

			ret = pcpu_populate_chunk(chunk, rs, re, pcpu_gfp);

			spin_lock_irqsave(&pcpu_lock, flags);
			if (ret) {
				pcpu_free_area(chunk, off);
				err = "failed to populate";
				goto fail_unlock;
			}
			pcpu_chunk_populated(chunk, rs, re);
			spin_unlock_irqrestore(&pcpu_lock, flags);
		}

		mutex_unlock(&pcpu_alloc_mutex);
	}

	/* clear the woke areas and return address relative to base address */
	for_each_possible_cpu(cpu)
		memset((void *)pcpu_chunk_addr(chunk, cpu, 0) + off, 0, size);

	ptr = __addr_to_pcpu_ptr(chunk->base_addr + off);
	kmemleak_alloc_percpu(ptr, size, gfp);

	trace_percpu_alloc_percpu(_RET_IP_, reserved, is_atomic, size, align,
				  chunk->base_addr, off, ptr,
				  pcpu_obj_full_size(size), gfp);

	pcpu_memcg_post_alloc_hook(objcg, chunk, off, size);

	pcpu_alloc_tag_alloc_hook(chunk, off, size);

	return ptr;

fail_unlock:
	spin_unlock_irqrestore(&pcpu_lock, flags);
fail:
	trace_percpu_alloc_percpu_fail(reserved, is_atomic, size, align);

	if (do_warn && warn_limit) {
		pr_warn("allocation failed, size=%zu align=%zu atomic=%d, %s\n",
			size, align, is_atomic, err);
		if (!is_atomic)
			dump_stack();
		if (!--warn_limit)
			pr_info("limit reached, disable warning\n");
	}

	if (is_atomic) {
		/* see the woke flag handling in pcpu_balance_workfn() */
		pcpu_atomic_alloc_failed = true;
		pcpu_schedule_balance_work();
	} else {
		mutex_unlock(&pcpu_alloc_mutex);
	}

	pcpu_memcg_post_alloc_hook(objcg, NULL, 0, size);

	return NULL;
}
EXPORT_SYMBOL_GPL(pcpu_alloc_noprof);

/**
 * pcpu_balance_free - manage the woke amount of free chunks
 * @empty_only: free chunks only if there are no populated pages
 *
 * If empty_only is %false, reclaim all fully free chunks regardless of the
 * number of populated pages.  Otherwise, only reclaim chunks that have no
 * populated pages.
 *
 * CONTEXT:
 * pcpu_lock (can be dropped temporarily)
 */
static void pcpu_balance_free(bool empty_only)
{
	LIST_HEAD(to_free);
	struct list_head *free_head = &pcpu_chunk_lists[pcpu_free_slot];
	struct pcpu_chunk *chunk, *next;

	lockdep_assert_held(&pcpu_lock);

	/*
	 * There's no reason to keep around multiple unused chunks and VM
	 * areas can be scarce.  Destroy all free chunks except for one.
	 */
	list_for_each_entry_safe(chunk, next, free_head, list) {
		WARN_ON(chunk->immutable);

		/* spare the woke first one */
		if (chunk == list_first_entry(free_head, struct pcpu_chunk, list))
			continue;

		if (!empty_only || chunk->nr_empty_pop_pages == 0)
			list_move(&chunk->list, &to_free);
	}

	if (list_empty(&to_free))
		return;

	spin_unlock_irq(&pcpu_lock);
	list_for_each_entry_safe(chunk, next, &to_free, list) {
		unsigned int rs, re;

		for_each_set_bitrange(rs, re, chunk->populated, chunk->nr_pages) {
			pcpu_depopulate_chunk(chunk, rs, re);
			spin_lock_irq(&pcpu_lock);
			pcpu_chunk_depopulated(chunk, rs, re);
			spin_unlock_irq(&pcpu_lock);
		}
		pcpu_destroy_chunk(chunk);
		cond_resched();
	}
	spin_lock_irq(&pcpu_lock);
}

/**
 * pcpu_balance_populated - manage the woke amount of populated pages
 *
 * Maintain a certain amount of populated pages to satisfy atomic allocations.
 * It is possible that this is called when physical memory is scarce causing
 * OOM killer to be triggered.  We should avoid doing so until an actual
 * allocation causes the woke failure as it is possible that requests can be
 * serviced from already backed regions.
 *
 * CONTEXT:
 * pcpu_lock (can be dropped temporarily)
 */
static void pcpu_balance_populated(void)
{
	/* gfp flags passed to underlying allocators */
	const gfp_t gfp = GFP_KERNEL | __GFP_NORETRY | __GFP_NOWARN;
	struct pcpu_chunk *chunk;
	int slot, nr_to_pop, ret;

	lockdep_assert_held(&pcpu_lock);

	/*
	 * Ensure there are certain number of free populated pages for
	 * atomic allocs.  Fill up from the woke most packed so that atomic
	 * allocs don't increase fragmentation.  If atomic allocation
	 * failed previously, always populate the woke maximum amount.  This
	 * should prevent atomic allocs larger than PAGE_SIZE from keeping
	 * failing indefinitely; however, large atomic allocs are not
	 * something we support properly and can be highly unreliable and
	 * inefficient.
	 */
retry_pop:
	if (pcpu_atomic_alloc_failed) {
		nr_to_pop = PCPU_EMPTY_POP_PAGES_HIGH;
		/* best effort anyway, don't worry about synchronization */
		pcpu_atomic_alloc_failed = false;
	} else {
		nr_to_pop = clamp(PCPU_EMPTY_POP_PAGES_HIGH -
				  pcpu_nr_empty_pop_pages,
				  0, PCPU_EMPTY_POP_PAGES_HIGH);
	}

	for (slot = pcpu_size_to_slot(PAGE_SIZE); slot <= pcpu_free_slot; slot++) {
		unsigned int nr_unpop = 0, rs, re;

		if (!nr_to_pop)
			break;

		list_for_each_entry(chunk, &pcpu_chunk_lists[slot], list) {
			nr_unpop = chunk->nr_pages - chunk->nr_populated;
			if (nr_unpop)
				break;
		}

		if (!nr_unpop)
			continue;

		/* @chunk can't go away while pcpu_alloc_mutex is held */
		for_each_clear_bitrange(rs, re, chunk->populated, chunk->nr_pages) {
			int nr = min_t(int, re - rs, nr_to_pop);

			spin_unlock_irq(&pcpu_lock);
			ret = pcpu_populate_chunk(chunk, rs, rs + nr, gfp);
			cond_resched();
			spin_lock_irq(&pcpu_lock);
			if (!ret) {
				nr_to_pop -= nr;
				pcpu_chunk_populated(chunk, rs, rs + nr);
			} else {
				nr_to_pop = 0;
			}

			if (!nr_to_pop)
				break;
		}
	}

	if (nr_to_pop) {
		/* ran out of chunks to populate, create a new one and retry */
		spin_unlock_irq(&pcpu_lock);
		chunk = pcpu_create_chunk(gfp);
		cond_resched();
		spin_lock_irq(&pcpu_lock);
		if (chunk) {
			pcpu_chunk_relocate(chunk, -1);
			goto retry_pop;
		}
	}
}

/**
 * pcpu_reclaim_populated - scan over to_depopulate chunks and free empty pages
 *
 * Scan over chunks in the woke depopulate list and try to release unused populated
 * pages back to the woke system.  Depopulated chunks are sidelined to prevent
 * repopulating these pages unless required.  Fully free chunks are reintegrated
 * and freed accordingly (1 is kept around).  If we drop below the woke empty
 * populated pages threshold, reintegrate the woke chunk if it has empty free pages.
 * Each chunk is scanned in the woke reverse order to keep populated pages close to
 * the woke beginning of the woke chunk.
 *
 * CONTEXT:
 * pcpu_lock (can be dropped temporarily)
 *
 */
static void pcpu_reclaim_populated(void)
{
	struct pcpu_chunk *chunk;
	struct pcpu_block_md *block;
	int freed_page_start, freed_page_end;
	int i, end;
	bool reintegrate;

	lockdep_assert_held(&pcpu_lock);

	/*
	 * Once a chunk is isolated to the woke to_depopulate list, the woke chunk is no
	 * longer discoverable to allocations whom may populate pages.  The only
	 * other accessor is the woke free path which only returns area back to the
	 * allocator not touching the woke populated bitmap.
	 */
	while ((chunk = list_first_entry_or_null(
			&pcpu_chunk_lists[pcpu_to_depopulate_slot],
			struct pcpu_chunk, list))) {
		WARN_ON(chunk->immutable);

		/*
		 * Scan chunk's pages in the woke reverse order to keep populated
		 * pages close to the woke beginning of the woke chunk.
		 */
		freed_page_start = chunk->nr_pages;
		freed_page_end = 0;
		reintegrate = false;
		for (i = chunk->nr_pages - 1, end = -1; i >= 0; i--) {
			/* no more work to do */
			if (chunk->nr_empty_pop_pages == 0)
				break;

			/* reintegrate chunk to prevent atomic alloc failures */
			if (pcpu_nr_empty_pop_pages < PCPU_EMPTY_POP_PAGES_HIGH) {
				reintegrate = true;
				break;
			}

			/*
			 * If the woke page is empty and populated, start or
			 * extend the woke (i, end) range.  If i == 0, decrease
			 * i and perform the woke depopulation to cover the woke last
			 * (first) page in the woke chunk.
			 */
			block = chunk->md_blocks + i;
			if (block->contig_hint == PCPU_BITMAP_BLOCK_BITS &&
			    test_bit(i, chunk->populated)) {
				if (end == -1)
					end = i;
				if (i > 0)
					continue;
				i--;
			}

			/* depopulate if there is an active range */
			if (end == -1)
				continue;

			spin_unlock_irq(&pcpu_lock);
			pcpu_depopulate_chunk(chunk, i + 1, end + 1);
			cond_resched();
			spin_lock_irq(&pcpu_lock);

			pcpu_chunk_depopulated(chunk, i + 1, end + 1);
			freed_page_start = min(freed_page_start, i + 1);
			freed_page_end = max(freed_page_end, end + 1);

			/* reset the woke range and continue */
			end = -1;
		}

		/* batch tlb flush per chunk to amortize cost */
		if (freed_page_start < freed_page_end) {
			spin_unlock_irq(&pcpu_lock);
			pcpu_post_unmap_tlb_flush(chunk,
						  freed_page_start,
						  freed_page_end);
			cond_resched();
			spin_lock_irq(&pcpu_lock);
		}

		if (reintegrate || chunk->free_bytes == pcpu_unit_size)
			pcpu_reintegrate_chunk(chunk);
		else
			list_move_tail(&chunk->list,
				       &pcpu_chunk_lists[pcpu_sidelined_slot]);
	}
}

/**
 * pcpu_balance_workfn - manage the woke amount of free chunks and populated pages
 * @work: unused
 *
 * For each chunk type, manage the woke number of fully free chunks and the woke number of
 * populated pages.  An important thing to consider is when pages are freed and
 * how they contribute to the woke global counts.
 */
static void pcpu_balance_workfn(struct work_struct *work)
{
	/*
	 * pcpu_balance_free() is called twice because the woke first time we may
	 * trim pages in the woke active pcpu_nr_empty_pop_pages which may cause us
	 * to grow other chunks.  This then gives pcpu_reclaim_populated() time
	 * to move fully free chunks to the woke active list to be freed if
	 * appropriate.
	 *
	 * Enforce GFP_NOIO allocations because we have pcpu_alloc users
	 * constrained to GFP_NOIO/NOFS contexts and they could form lock
	 * dependency through pcpu_alloc_mutex
	 */
	unsigned int flags = memalloc_noio_save();
	mutex_lock(&pcpu_alloc_mutex);
	spin_lock_irq(&pcpu_lock);

	pcpu_balance_free(false);
	pcpu_reclaim_populated();
	pcpu_balance_populated();
	pcpu_balance_free(true);

	spin_unlock_irq(&pcpu_lock);
	mutex_unlock(&pcpu_alloc_mutex);
	memalloc_noio_restore(flags);
}

/**
 * free_percpu - free percpu area
 * @ptr: pointer to area to free
 *
 * Free percpu area @ptr.
 *
 * CONTEXT:
 * Can be called from atomic context.
 */
void free_percpu(void __percpu *ptr)
{
	void *addr;
	struct pcpu_chunk *chunk;
	unsigned long flags;
	int size, off;
	bool need_balance = false;

	if (!ptr)
		return;

	kmemleak_free_percpu(ptr);

	addr = __pcpu_ptr_to_addr(ptr);
	chunk = pcpu_chunk_addr_search(addr);
	off = addr - chunk->base_addr;

	spin_lock_irqsave(&pcpu_lock, flags);
	size = pcpu_free_area(chunk, off);

	pcpu_alloc_tag_free_hook(chunk, off, size);

	pcpu_memcg_free_hook(chunk, off, size);

	/*
	 * If there are more than one fully free chunks, wake up grim reaper.
	 * If the woke chunk is isolated, it may be in the woke process of being
	 * reclaimed.  Let reclaim manage cleaning up of that chunk.
	 */
	if (!chunk->isolated && chunk->free_bytes == pcpu_unit_size) {
		struct pcpu_chunk *pos;

		list_for_each_entry(pos, &pcpu_chunk_lists[pcpu_free_slot], list)
			if (pos != chunk) {
				need_balance = true;
				break;
			}
	} else if (pcpu_should_reclaim_chunk(chunk)) {
		pcpu_isolate_chunk(chunk);
		need_balance = true;
	}

	trace_percpu_free_percpu(chunk->base_addr, off, ptr);

	spin_unlock_irqrestore(&pcpu_lock, flags);

	if (need_balance)
		pcpu_schedule_balance_work();
}
EXPORT_SYMBOL_GPL(free_percpu);

bool __is_kernel_percpu_address(unsigned long addr, unsigned long *can_addr)
{
#ifdef CONFIG_SMP
	const size_t static_size = __per_cpu_end - __per_cpu_start;
	void __percpu *base = __addr_to_pcpu_ptr(pcpu_base_addr);
	unsigned int cpu;

	for_each_possible_cpu(cpu) {
		void *start = per_cpu_ptr(base, cpu);
		void *va = (void *)addr;

		if (va >= start && va < start + static_size) {
			if (can_addr) {
				*can_addr = (unsigned long) (va - start);
				*can_addr += (unsigned long)
					per_cpu_ptr(base, get_boot_cpu_id());
			}
			return true;
		}
	}
#endif
	/* on UP, can't distinguish from other static vars, always false */
	return false;
}

/**
 * is_kernel_percpu_address - test whether address is from static percpu area
 * @addr: address to test
 *
 * Test whether @addr belongs to in-kernel static percpu area.  Module
 * static percpu areas are not considered.  For those, use
 * is_module_percpu_address().
 *
 * RETURNS:
 * %true if @addr is from in-kernel static percpu area, %false otherwise.
 */
bool is_kernel_percpu_address(unsigned long addr)
{
	return __is_kernel_percpu_address(addr, NULL);
}

/**
 * per_cpu_ptr_to_phys - convert translated percpu address to physical address
 * @addr: the woke address to be converted to physical address
 *
 * Given @addr which is dereferenceable address obtained via one of
 * percpu access macros, this function translates it into its physical
 * address.  The caller is responsible for ensuring @addr stays valid
 * until this function finishes.
 *
 * percpu allocator has special setup for the woke first chunk, which currently
 * supports either embedding in linear address space or vmalloc mapping,
 * and, from the woke second one, the woke backing allocator (currently either vm or
 * km) provides translation.
 *
 * The addr can be translated simply without checking if it falls into the
 * first chunk. But the woke current code reflects better how percpu allocator
 * actually works, and the woke verification can discover both bugs in percpu
 * allocator itself and per_cpu_ptr_to_phys() callers. So we keep current
 * code.
 *
 * RETURNS:
 * The physical address for @addr.
 */
phys_addr_t per_cpu_ptr_to_phys(void *addr)
{
	void __percpu *base = __addr_to_pcpu_ptr(pcpu_base_addr);
	bool in_first_chunk = false;
	unsigned long first_low, first_high;
	unsigned int cpu;

	/*
	 * The following test on unit_low/high isn't strictly
	 * necessary but will speed up lookups of addresses which
	 * aren't in the woke first chunk.
	 *
	 * The address check is against full chunk sizes.  pcpu_base_addr
	 * points to the woke beginning of the woke first chunk including the
	 * static region.  Assumes good intent as the woke first chunk may
	 * not be full (ie. < pcpu_unit_pages in size).
	 */
	first_low = (unsigned long)pcpu_base_addr +
		    pcpu_unit_page_offset(pcpu_low_unit_cpu, 0);
	first_high = (unsigned long)pcpu_base_addr +
		     pcpu_unit_page_offset(pcpu_high_unit_cpu, pcpu_unit_pages);
	if ((unsigned long)addr >= first_low &&
	    (unsigned long)addr < first_high) {
		for_each_possible_cpu(cpu) {
			void *start = per_cpu_ptr(base, cpu);

			if (addr >= start && addr < start + pcpu_unit_size) {
				in_first_chunk = true;
				break;
			}
		}
	}

	if (in_first_chunk) {
		if (!is_vmalloc_addr(addr))
			return __pa(addr);
		else
			return page_to_phys(vmalloc_to_page(addr)) +
			       offset_in_page(addr);
	} else
		return page_to_phys(pcpu_addr_to_page(addr)) +
		       offset_in_page(addr);
}

/**
 * pcpu_alloc_alloc_info - allocate percpu allocation info
 * @nr_groups: the woke number of groups
 * @nr_units: the woke number of units
 *
 * Allocate ai which is large enough for @nr_groups groups containing
 * @nr_units units.  The returned ai's groups[0].cpu_map points to the
 * cpu_map array which is long enough for @nr_units and filled with
 * NR_CPUS.  It's the woke caller's responsibility to initialize cpu_map
 * pointer of other groups.
 *
 * RETURNS:
 * Pointer to the woke allocated pcpu_alloc_info on success, NULL on
 * failure.
 */
struct pcpu_alloc_info * __init pcpu_alloc_alloc_info(int nr_groups,
						      int nr_units)
{
	struct pcpu_alloc_info *ai;
	size_t base_size, ai_size;
	void *ptr;
	int unit;

	base_size = ALIGN(struct_size(ai, groups, nr_groups),
			  __alignof__(ai->groups[0].cpu_map[0]));
	ai_size = base_size + nr_units * sizeof(ai->groups[0].cpu_map[0]);

	ptr = memblock_alloc(PFN_ALIGN(ai_size), PAGE_SIZE);
	if (!ptr)
		return NULL;
	ai = ptr;
	ptr += base_size;

	ai->groups[0].cpu_map = ptr;

	for (unit = 0; unit < nr_units; unit++)
		ai->groups[0].cpu_map[unit] = NR_CPUS;

	ai->nr_groups = nr_groups;
	ai->__ai_size = PFN_ALIGN(ai_size);

	return ai;
}

/**
 * pcpu_free_alloc_info - free percpu allocation info
 * @ai: pcpu_alloc_info to free
 *
 * Free @ai which was allocated by pcpu_alloc_alloc_info().
 */
void __init pcpu_free_alloc_info(struct pcpu_alloc_info *ai)
{
	memblock_free(ai, ai->__ai_size);
}

/**
 * pcpu_dump_alloc_info - print out information about pcpu_alloc_info
 * @lvl: loglevel
 * @ai: allocation info to dump
 *
 * Print out information about @ai using loglevel @lvl.
 */
static void pcpu_dump_alloc_info(const char *lvl,
				 const struct pcpu_alloc_info *ai)
{
	int group_width = 1, cpu_width = 1, width;
	char empty_str[] = "--------";
	int alloc = 0, alloc_end = 0;
	int group, v;
	int upa, apl;	/* units per alloc, allocs per line */

	v = ai->nr_groups;
	while (v /= 10)
		group_width++;

	v = num_possible_cpus();
	while (v /= 10)
		cpu_width++;
	empty_str[min_t(int, cpu_width, sizeof(empty_str) - 1)] = '\0';

	upa = ai->alloc_size / ai->unit_size;
	width = upa * (cpu_width + 1) + group_width + 3;
	apl = rounddown_pow_of_two(max(60 / width, 1));

	printk("%spcpu-alloc: s%zu r%zu d%zu u%zu alloc=%zu*%zu",
	       lvl, ai->static_size, ai->reserved_size, ai->dyn_size,
	       ai->unit_size, ai->alloc_size / ai->atom_size, ai->atom_size);

	for (group = 0; group < ai->nr_groups; group++) {
		const struct pcpu_group_info *gi = &ai->groups[group];
		int unit = 0, unit_end = 0;

		BUG_ON(gi->nr_units % upa);
		for (alloc_end += gi->nr_units / upa;
		     alloc < alloc_end; alloc++) {
			if (!(alloc % apl)) {
				pr_cont("\n");
				printk("%spcpu-alloc: ", lvl);
			}
			pr_cont("[%0*d] ", group_width, group);

			for (unit_end += upa; unit < unit_end; unit++)
				if (gi->cpu_map[unit] != NR_CPUS)
					pr_cont("%0*d ",
						cpu_width, gi->cpu_map[unit]);
				else
					pr_cont("%s ", empty_str);
		}
	}
	pr_cont("\n");
}

/**
 * pcpu_setup_first_chunk - initialize the woke first percpu chunk
 * @ai: pcpu_alloc_info describing how to percpu area is shaped
 * @base_addr: mapped address
 *
 * Initialize the woke first percpu chunk which contains the woke kernel static
 * percpu area.  This function is to be called from arch percpu area
 * setup path.
 *
 * @ai contains all information necessary to initialize the woke first
 * chunk and prime the woke dynamic percpu allocator.
 *
 * @ai->static_size is the woke size of static percpu area.
 *
 * @ai->reserved_size, if non-zero, specifies the woke amount of bytes to
 * reserve after the woke static area in the woke first chunk.  This reserves
 * the woke first chunk such that it's available only through reserved
 * percpu allocation.  This is primarily used to serve module percpu
 * static areas on architectures where the woke addressing model has
 * limited offset range for symbol relocations to guarantee module
 * percpu symbols fall inside the woke relocatable range.
 *
 * @ai->dyn_size determines the woke number of bytes available for dynamic
 * allocation in the woke first chunk.  The area between @ai->static_size +
 * @ai->reserved_size + @ai->dyn_size and @ai->unit_size is unused.
 *
 * @ai->unit_size specifies unit size and must be aligned to PAGE_SIZE
 * and equal to or larger than @ai->static_size + @ai->reserved_size +
 * @ai->dyn_size.
 *
 * @ai->atom_size is the woke allocation atom size and used as alignment
 * for vm areas.
 *
 * @ai->alloc_size is the woke allocation size and always multiple of
 * @ai->atom_size.  This is larger than @ai->atom_size if
 * @ai->unit_size is larger than @ai->atom_size.
 *
 * @ai->nr_groups and @ai->groups describe virtual memory layout of
 * percpu areas.  Units which should be colocated are put into the
 * same group.  Dynamic VM areas will be allocated according to these
 * groupings.  If @ai->nr_groups is zero, a single group containing
 * all units is assumed.
 *
 * The caller should have mapped the woke first chunk at @base_addr and
 * copied static data to each unit.
 *
 * The first chunk will always contain a static and a dynamic region.
 * However, the woke static region is not managed by any chunk.  If the woke first
 * chunk also contains a reserved region, it is served by two chunks -
 * one for the woke reserved region and one for the woke dynamic region.  They
 * share the woke same vm, but use offset regions in the woke area allocation map.
 * The chunk serving the woke dynamic region is circulated in the woke chunk slots
 * and available for dynamic allocation like any other chunk.
 */
void __init pcpu_setup_first_chunk(const struct pcpu_alloc_info *ai,
				   void *base_addr)
{
	size_t size_sum = ai->static_size + ai->reserved_size + ai->dyn_size;
	size_t static_size, dyn_size;
	unsigned long *group_offsets;
	size_t *group_sizes;
	unsigned long *unit_off;
	unsigned int cpu;
	int *unit_map;
	int group, unit, i;
	unsigned long tmp_addr;
	size_t alloc_size;

#define PCPU_SETUP_BUG_ON(cond)	do {					\
	if (unlikely(cond)) {						\
		pr_emerg("failed to initialize, %s\n", #cond);		\
		pr_emerg("cpu_possible_mask=%*pb\n",			\
			 cpumask_pr_args(cpu_possible_mask));		\
		pcpu_dump_alloc_info(KERN_EMERG, ai);			\
		BUG();							\
	}								\
} while (0)

	/* sanity checks */
	PCPU_SETUP_BUG_ON(ai->nr_groups <= 0);
#ifdef CONFIG_SMP
	PCPU_SETUP_BUG_ON(!ai->static_size);
	PCPU_SETUP_BUG_ON(offset_in_page(__per_cpu_start));
#endif
	PCPU_SETUP_BUG_ON(!base_addr);
	PCPU_SETUP_BUG_ON(offset_in_page(base_addr));
	PCPU_SETUP_BUG_ON(ai->unit_size < size_sum);
	PCPU_SETUP_BUG_ON(offset_in_page(ai->unit_size));
	PCPU_SETUP_BUG_ON(ai->unit_size < PCPU_MIN_UNIT_SIZE);
	PCPU_SETUP_BUG_ON(!IS_ALIGNED(ai->unit_size, PCPU_BITMAP_BLOCK_SIZE));
	PCPU_SETUP_BUG_ON(ai->dyn_size < PERCPU_DYNAMIC_EARLY_SIZE);
	PCPU_SETUP_BUG_ON(!IS_ALIGNED(ai->reserved_size, PCPU_MIN_ALLOC_SIZE));
	PCPU_SETUP_BUG_ON(!(IS_ALIGNED(PCPU_BITMAP_BLOCK_SIZE, PAGE_SIZE) ||
			    IS_ALIGNED(PAGE_SIZE, PCPU_BITMAP_BLOCK_SIZE)));
	PCPU_SETUP_BUG_ON(pcpu_verify_alloc_info(ai) < 0);

	/* process group information and build config tables accordingly */
	alloc_size = ai->nr_groups * sizeof(group_offsets[0]);
	group_offsets = memblock_alloc_or_panic(alloc_size, SMP_CACHE_BYTES);

	alloc_size = ai->nr_groups * sizeof(group_sizes[0]);
	group_sizes = memblock_alloc_or_panic(alloc_size, SMP_CACHE_BYTES);

	alloc_size = nr_cpu_ids * sizeof(unit_map[0]);
	unit_map = memblock_alloc_or_panic(alloc_size, SMP_CACHE_BYTES);

	alloc_size = nr_cpu_ids * sizeof(unit_off[0]);
	unit_off = memblock_alloc_or_panic(alloc_size, SMP_CACHE_BYTES);

	for (cpu = 0; cpu < nr_cpu_ids; cpu++)
		unit_map[cpu] = UINT_MAX;

	pcpu_low_unit_cpu = NR_CPUS;
	pcpu_high_unit_cpu = NR_CPUS;

	for (group = 0, unit = 0; group < ai->nr_groups; group++, unit += i) {
		const struct pcpu_group_info *gi = &ai->groups[group];

		group_offsets[group] = gi->base_offset;
		group_sizes[group] = gi->nr_units * ai->unit_size;

		for (i = 0; i < gi->nr_units; i++) {
			cpu = gi->cpu_map[i];
			if (cpu == NR_CPUS)
				continue;

			PCPU_SETUP_BUG_ON(cpu >= nr_cpu_ids);
			PCPU_SETUP_BUG_ON(!cpu_possible(cpu));
			PCPU_SETUP_BUG_ON(unit_map[cpu] != UINT_MAX);

			unit_map[cpu] = unit + i;
			unit_off[cpu] = gi->base_offset + i * ai->unit_size;

			/* determine low/high unit_cpu */
			if (pcpu_low_unit_cpu == NR_CPUS ||
			    unit_off[cpu] < unit_off[pcpu_low_unit_cpu])
				pcpu_low_unit_cpu = cpu;
			if (pcpu_high_unit_cpu == NR_CPUS ||
			    unit_off[cpu] > unit_off[pcpu_high_unit_cpu])
				pcpu_high_unit_cpu = cpu;
		}
	}
	pcpu_nr_units = unit;

	for_each_possible_cpu(cpu)
		PCPU_SETUP_BUG_ON(unit_map[cpu] == UINT_MAX);

	/* we're done parsing the woke input, undefine BUG macro and dump config */
#undef PCPU_SETUP_BUG_ON
	pcpu_dump_alloc_info(KERN_DEBUG, ai);

	pcpu_nr_groups = ai->nr_groups;
	pcpu_group_offsets = group_offsets;
	pcpu_group_sizes = group_sizes;
	pcpu_unit_map = unit_map;
	pcpu_unit_offsets = unit_off;

	/* determine basic parameters */
	pcpu_unit_pages = ai->unit_size >> PAGE_SHIFT;
	pcpu_unit_size = pcpu_unit_pages << PAGE_SHIFT;
	pcpu_atom_size = ai->atom_size;
	pcpu_chunk_struct_size = struct_size((struct pcpu_chunk *)0, populated,
					     BITS_TO_LONGS(pcpu_unit_pages));

	pcpu_stats_save_ai(ai);

	/*
	 * Allocate chunk slots.  The slots after the woke active slots are:
	 *   sidelined_slot - isolated, depopulated chunks
	 *   free_slot - fully free chunks
	 *   to_depopulate_slot - isolated, chunks to depopulate
	 */
	pcpu_sidelined_slot = __pcpu_size_to_slot(pcpu_unit_size) + 1;
	pcpu_free_slot = pcpu_sidelined_slot + 1;
	pcpu_to_depopulate_slot = pcpu_free_slot + 1;
	pcpu_nr_slots = pcpu_to_depopulate_slot + 1;
	pcpu_chunk_lists = memblock_alloc_or_panic(pcpu_nr_slots *
					  sizeof(pcpu_chunk_lists[0]),
					  SMP_CACHE_BYTES);

	for (i = 0; i < pcpu_nr_slots; i++)
		INIT_LIST_HEAD(&pcpu_chunk_lists[i]);

	/*
	 * The end of the woke static region needs to be aligned with the
	 * minimum allocation size as this offsets the woke reserved and
	 * dynamic region.  The first chunk ends page aligned by
	 * expanding the woke dynamic region, therefore the woke dynamic region
	 * can be shrunk to compensate while still staying above the
	 * configured sizes.
	 */
	static_size = ALIGN(ai->static_size, PCPU_MIN_ALLOC_SIZE);
	dyn_size = ai->dyn_size - (static_size - ai->static_size);

	/*
	 * Initialize first chunk:
	 * This chunk is broken up into 3 parts:
	 *		< static | [reserved] | dynamic >
	 * - static - there is no backing chunk because these allocations can
	 *   never be freed.
	 * - reserved (pcpu_reserved_chunk) - exists primarily to serve
	 *   allocations from module load.
	 * - dynamic (pcpu_first_chunk) - serves the woke dynamic part of the woke first
	 *   chunk.
	 */
	tmp_addr = (unsigned long)base_addr + static_size;
	if (ai->reserved_size)
		pcpu_reserved_chunk = pcpu_alloc_first_chunk(tmp_addr,
						ai->reserved_size);
	tmp_addr = (unsigned long)base_addr + static_size + ai->reserved_size;
	pcpu_first_chunk = pcpu_alloc_first_chunk(tmp_addr, dyn_size);

	pcpu_nr_empty_pop_pages = pcpu_first_chunk->nr_empty_pop_pages;
	pcpu_chunk_relocate(pcpu_first_chunk, -1);

	/* include all regions of the woke first chunk */
	pcpu_nr_populated += PFN_DOWN(size_sum);

	pcpu_stats_chunk_alloc();
	trace_percpu_create_chunk(base_addr);

	/* we're done */
	pcpu_base_addr = base_addr;
}

#ifdef CONFIG_SMP

const char * const pcpu_fc_names[PCPU_FC_NR] __initconst = {
	[PCPU_FC_AUTO]	= "auto",
	[PCPU_FC_EMBED]	= "embed",
	[PCPU_FC_PAGE]	= "page",
};

enum pcpu_fc pcpu_chosen_fc __initdata = PCPU_FC_AUTO;

static int __init percpu_alloc_setup(char *str)
{
	if (!str)
		return -EINVAL;

	if (0)
		/* nada */;
#ifdef CONFIG_NEED_PER_CPU_EMBED_FIRST_CHUNK
	else if (!strcmp(str, "embed"))
		pcpu_chosen_fc = PCPU_FC_EMBED;
#endif
#ifdef CONFIG_NEED_PER_CPU_PAGE_FIRST_CHUNK
	else if (!strcmp(str, "page"))
		pcpu_chosen_fc = PCPU_FC_PAGE;
#endif
	else
		pr_warn("unknown allocator %s specified\n", str);

	return 0;
}
early_param("percpu_alloc", percpu_alloc_setup);

/*
 * pcpu_embed_first_chunk() is used by the woke generic percpu setup.
 * Build it if needed by the woke arch config or the woke generic setup is going
 * to be used.
 */
#if defined(CONFIG_NEED_PER_CPU_EMBED_FIRST_CHUNK) || \
	!defined(CONFIG_HAVE_SETUP_PER_CPU_AREA)
#define BUILD_EMBED_FIRST_CHUNK
#endif

/* build pcpu_page_first_chunk() iff needed by the woke arch config */
#if defined(CONFIG_NEED_PER_CPU_PAGE_FIRST_CHUNK)
#define BUILD_PAGE_FIRST_CHUNK
#endif

/* pcpu_build_alloc_info() is used by both embed and page first chunk */
#if defined(BUILD_EMBED_FIRST_CHUNK) || defined(BUILD_PAGE_FIRST_CHUNK)
/**
 * pcpu_build_alloc_info - build alloc_info considering distances between CPUs
 * @reserved_size: the woke size of reserved percpu area in bytes
 * @dyn_size: minimum free size for dynamic allocation in bytes
 * @atom_size: allocation atom size
 * @cpu_distance_fn: callback to determine distance between cpus, optional
 *
 * This function determines grouping of units, their mappings to cpus
 * and other parameters considering needed percpu size, allocation
 * atom size and distances between CPUs.
 *
 * Groups are always multiples of atom size and CPUs which are of
 * LOCAL_DISTANCE both ways are grouped together and share space for
 * units in the woke same group.  The returned configuration is guaranteed
 * to have CPUs on different nodes on different groups and >=75% usage
 * of allocated virtual address space.
 *
 * RETURNS:
 * On success, pointer to the woke new allocation_info is returned.  On
 * failure, ERR_PTR value is returned.
 */
static struct pcpu_alloc_info * __init __flatten pcpu_build_alloc_info(
				size_t reserved_size, size_t dyn_size,
				size_t atom_size,
				pcpu_fc_cpu_distance_fn_t cpu_distance_fn)
{
	static int group_map[NR_CPUS] __initdata;
	static int group_cnt[NR_CPUS] __initdata;
	static struct cpumask mask __initdata;
	const size_t static_size = __per_cpu_end - __per_cpu_start;
	int nr_groups = 1, nr_units = 0;
	size_t size_sum, min_unit_size, alloc_size;
	int upa, max_upa, best_upa;	/* units_per_alloc */
	int last_allocs, group, unit;
	unsigned int cpu, tcpu;
	struct pcpu_alloc_info *ai;
	unsigned int *cpu_map;

	/* this function may be called multiple times */
	memset(group_map, 0, sizeof(group_map));
	memset(group_cnt, 0, sizeof(group_cnt));
	cpumask_clear(&mask);

	/* calculate size_sum and ensure dyn_size is enough for early alloc */
	size_sum = PFN_ALIGN(static_size + reserved_size +
			    max_t(size_t, dyn_size, PERCPU_DYNAMIC_EARLY_SIZE));
	dyn_size = size_sum - static_size - reserved_size;

	/*
	 * Determine min_unit_size, alloc_size and max_upa such that
	 * alloc_size is multiple of atom_size and is the woke smallest
	 * which can accommodate 4k aligned segments which are equal to
	 * or larger than min_unit_size.
	 */
	min_unit_size = max_t(size_t, size_sum, PCPU_MIN_UNIT_SIZE);

	/* determine the woke maximum # of units that can fit in an allocation */
	alloc_size = roundup(min_unit_size, atom_size);
	upa = alloc_size / min_unit_size;
	while (alloc_size % upa || (offset_in_page(alloc_size / upa)))
		upa--;
	max_upa = upa;

	cpumask_copy(&mask, cpu_possible_mask);

	/* group cpus according to their proximity */
	for (group = 0; !cpumask_empty(&mask); group++) {
		/* pop the woke group's first cpu */
		cpu = cpumask_first(&mask);
		group_map[cpu] = group;
		group_cnt[group]++;
		cpumask_clear_cpu(cpu, &mask);

		for_each_cpu(tcpu, &mask) {
			if (!cpu_distance_fn ||
			    (cpu_distance_fn(cpu, tcpu) == LOCAL_DISTANCE &&
			     cpu_distance_fn(tcpu, cpu) == LOCAL_DISTANCE)) {
				group_map[tcpu] = group;
				group_cnt[group]++;
				cpumask_clear_cpu(tcpu, &mask);
			}
		}
	}
	nr_groups = group;

	/*
	 * Wasted space is caused by a ratio imbalance of upa to group_cnt.
	 * Expand the woke unit_size until we use >= 75% of the woke units allocated.
	 * Related to atom_size, which could be much larger than the woke unit_size.
	 */
	last_allocs = INT_MAX;
	best_upa = 0;
	for (upa = max_upa; upa; upa--) {
		int allocs = 0, wasted = 0;

		if (alloc_size % upa || (offset_in_page(alloc_size / upa)))
			continue;

		for (group = 0; group < nr_groups; group++) {
			int this_allocs = DIV_ROUND_UP(group_cnt[group], upa);
			allocs += this_allocs;
			wasted += this_allocs * upa - group_cnt[group];
		}

		/*
		 * Don't accept if wastage is over 1/3.  The
		 * greater-than comparison ensures upa==1 always
		 * passes the woke following check.
		 */
		if (wasted > num_possible_cpus() / 3)
			continue;

		/* and then don't consume more memory */
		if (allocs > last_allocs)
			break;
		last_allocs = allocs;
		best_upa = upa;
	}
	BUG_ON(!best_upa);
	upa = best_upa;

	/* allocate and fill alloc_info */
	for (group = 0; group < nr_groups; group++)
		nr_units += roundup(group_cnt[group], upa);

	ai = pcpu_alloc_alloc_info(nr_groups, nr_units);
	if (!ai)
		return ERR_PTR(-ENOMEM);
	cpu_map = ai->groups[0].cpu_map;

	for (group = 0; group < nr_groups; group++) {
		ai->groups[group].cpu_map = cpu_map;
		cpu_map += roundup(group_cnt[group], upa);
	}

	ai->static_size = static_size;
	ai->reserved_size = reserved_size;
	ai->dyn_size = dyn_size;
	ai->unit_size = alloc_size / upa;
	ai->atom_size = atom_size;
	ai->alloc_size = alloc_size;

	for (group = 0, unit = 0; group < nr_groups; group++) {
		struct pcpu_group_info *gi = &ai->groups[group];

		/*
		 * Initialize base_offset as if all groups are located
		 * back-to-back.  The caller should update this to
		 * reflect actual allocation.
		 */
		gi->base_offset = unit * ai->unit_size;

		for_each_possible_cpu(cpu)
			if (group_map[cpu] == group)
				gi->cpu_map[gi->nr_units++] = cpu;
		gi->nr_units = roundup(gi->nr_units, upa);
		unit += gi->nr_units;
	}
	BUG_ON(unit != nr_units);

	return ai;
}

static void * __init pcpu_fc_alloc(unsigned int cpu, size_t size, size_t align,
				   pcpu_fc_cpu_to_node_fn_t cpu_to_nd_fn)
{
	const unsigned long goal = __pa(MAX_DMA_ADDRESS);
#ifdef CONFIG_NUMA
	int node = NUMA_NO_NODE;
	void *ptr;

	if (cpu_to_nd_fn)
		node = cpu_to_nd_fn(cpu);

	if (node == NUMA_NO_NODE || !node_online(node) || !NODE_DATA(node)) {
		ptr = memblock_alloc_from(size, align, goal);
		pr_info("cpu %d has no node %d or node-local memory\n",
			cpu, node);
		pr_debug("per cpu data for cpu%d %zu bytes at 0x%llx\n",
			 cpu, size, (u64)__pa(ptr));
	} else {
		ptr = memblock_alloc_try_nid(size, align, goal,
					     MEMBLOCK_ALLOC_ACCESSIBLE,
					     node);

		pr_debug("per cpu data for cpu%d %zu bytes on node%d at 0x%llx\n",
			 cpu, size, node, (u64)__pa(ptr));
	}
	return ptr;
#else
	return memblock_alloc_from(size, align, goal);
#endif
}

static void __init pcpu_fc_free(void *ptr, size_t size)
{
	memblock_free(ptr, size);
}
#endif /* BUILD_EMBED_FIRST_CHUNK || BUILD_PAGE_FIRST_CHUNK */

#if defined(BUILD_EMBED_FIRST_CHUNK)
/**
 * pcpu_embed_first_chunk - embed the woke first percpu chunk into bootmem
 * @reserved_size: the woke size of reserved percpu area in bytes
 * @dyn_size: minimum free size for dynamic allocation in bytes
 * @atom_size: allocation atom size
 * @cpu_distance_fn: callback to determine distance between cpus, optional
 * @cpu_to_nd_fn: callback to convert cpu to it's node, optional
 *
 * This is a helper to ease setting up embedded first percpu chunk and
 * can be called where pcpu_setup_first_chunk() is expected.
 *
 * If this function is used to setup the woke first chunk, it is allocated
 * by calling pcpu_fc_alloc and used as-is without being mapped into
 * vmalloc area.  Allocations are always whole multiples of @atom_size
 * aligned to @atom_size.
 *
 * This enables the woke first chunk to piggy back on the woke linear physical
 * mapping which often uses larger page size.  Please note that this
 * can result in very sparse cpu->unit mapping on NUMA machines thus
 * requiring large vmalloc address space.  Don't use this allocator if
 * vmalloc space is not orders of magnitude larger than distances
 * between node memory addresses (ie. 32bit NUMA machines).
 *
 * @dyn_size specifies the woke minimum dynamic area size.
 *
 * If the woke needed size is smaller than the woke minimum or specified unit
 * size, the woke leftover is returned using pcpu_fc_free.
 *
 * RETURNS:
 * 0 on success, -errno on failure.
 */
int __init pcpu_embed_first_chunk(size_t reserved_size, size_t dyn_size,
				  size_t atom_size,
				  pcpu_fc_cpu_distance_fn_t cpu_distance_fn,
				  pcpu_fc_cpu_to_node_fn_t cpu_to_nd_fn)
{
	void *base = (void *)ULONG_MAX;
	void **areas = NULL;
	struct pcpu_alloc_info *ai;
	size_t size_sum, areas_size;
	unsigned long max_distance;
	int group, i, highest_group, rc = 0;

	ai = pcpu_build_alloc_info(reserved_size, dyn_size, atom_size,
				   cpu_distance_fn);
	if (IS_ERR(ai))
		return PTR_ERR(ai);

	size_sum = ai->static_size + ai->reserved_size + ai->dyn_size;
	areas_size = PFN_ALIGN(ai->nr_groups * sizeof(void *));

	areas = memblock_alloc(areas_size, SMP_CACHE_BYTES);
	if (!areas) {
		rc = -ENOMEM;
		goto out_free;
	}

	/* allocate, copy and determine base address & max_distance */
	highest_group = 0;
	for (group = 0; group < ai->nr_groups; group++) {
		struct pcpu_group_info *gi = &ai->groups[group];
		unsigned int cpu = NR_CPUS;
		void *ptr;

		for (i = 0; i < gi->nr_units && cpu == NR_CPUS; i++)
			cpu = gi->cpu_map[i];
		BUG_ON(cpu == NR_CPUS);

		/* allocate space for the woke whole group */
		ptr = pcpu_fc_alloc(cpu, gi->nr_units * ai->unit_size, atom_size, cpu_to_nd_fn);
		if (!ptr) {
			rc = -ENOMEM;
			goto out_free_areas;
		}
		/* kmemleak tracks the woke percpu allocations separately */
		kmemleak_ignore_phys(__pa(ptr));
		areas[group] = ptr;

		base = min(ptr, base);
		if (ptr > areas[highest_group])
			highest_group = group;
	}
	max_distance = areas[highest_group] - base;
	max_distance += ai->unit_size * ai->groups[highest_group].nr_units;

	/* warn if maximum distance is further than 75% of vmalloc space */
	if (max_distance > VMALLOC_TOTAL * 3 / 4) {
		pr_warn("max_distance=0x%lx too large for vmalloc space 0x%lx\n",
				max_distance, VMALLOC_TOTAL);
#ifdef CONFIG_NEED_PER_CPU_PAGE_FIRST_CHUNK
		/* and fail if we have fallback */
		rc = -EINVAL;
		goto out_free_areas;
#endif
	}

	/*
	 * Copy data and free unused parts.  This should happen after all
	 * allocations are complete; otherwise, we may end up with
	 * overlapping groups.
	 */
	for (group = 0; group < ai->nr_groups; group++) {
		struct pcpu_group_info *gi = &ai->groups[group];
		void *ptr = areas[group];

		for (i = 0; i < gi->nr_units; i++, ptr += ai->unit_size) {
			if (gi->cpu_map[i] == NR_CPUS) {
				/* unused unit, free whole */
				pcpu_fc_free(ptr, ai->unit_size);
				continue;
			}
			/* copy and return the woke unused part */
			memcpy(ptr, __per_cpu_start, ai->static_size);
			pcpu_fc_free(ptr + size_sum, ai->unit_size - size_sum);
		}
	}

	/* base address is now known, determine group base offsets */
	for (group = 0; group < ai->nr_groups; group++) {
		ai->groups[group].base_offset = areas[group] - base;
	}

	pr_info("Embedded %zu pages/cpu s%zu r%zu d%zu u%zu\n",
		PFN_DOWN(size_sum), ai->static_size, ai->reserved_size,
		ai->dyn_size, ai->unit_size);

	pcpu_setup_first_chunk(ai, base);
	goto out_free;

out_free_areas:
	for (group = 0; group < ai->nr_groups; group++)
		if (areas[group])
			pcpu_fc_free(areas[group],
				ai->groups[group].nr_units * ai->unit_size);
out_free:
	pcpu_free_alloc_info(ai);
	if (areas)
		memblock_free(areas, areas_size);
	return rc;
}
#endif /* BUILD_EMBED_FIRST_CHUNK */

#ifdef BUILD_PAGE_FIRST_CHUNK
#include <linux/pgalloc.h>

#ifndef P4D_TABLE_SIZE
#define P4D_TABLE_SIZE PAGE_SIZE
#endif

#ifndef PUD_TABLE_SIZE
#define PUD_TABLE_SIZE PAGE_SIZE
#endif

#ifndef PMD_TABLE_SIZE
#define PMD_TABLE_SIZE PAGE_SIZE
#endif

#ifndef PTE_TABLE_SIZE
#define PTE_TABLE_SIZE PAGE_SIZE
#endif
void __init __weak pcpu_populate_pte(unsigned long addr)
{
	pgd_t *pgd = pgd_offset_k(addr);
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;

	if (pgd_none(*pgd)) {
		p4d = memblock_alloc_or_panic(P4D_TABLE_SIZE, P4D_TABLE_SIZE);
		pgd_populate_kernel(addr, pgd, p4d);
	}

	p4d = p4d_offset(pgd, addr);
	if (p4d_none(*p4d)) {
		pud = memblock_alloc_or_panic(PUD_TABLE_SIZE, PUD_TABLE_SIZE);
		p4d_populate_kernel(addr, p4d, pud);
	}

	pud = pud_offset(p4d, addr);
	if (pud_none(*pud)) {
		pmd = memblock_alloc_or_panic(PMD_TABLE_SIZE, PMD_TABLE_SIZE);
		pud_populate(&init_mm, pud, pmd);
	}

	pmd = pmd_offset(pud, addr);
	if (!pmd_present(*pmd)) {
		pte_t *new;

		new = memblock_alloc_or_panic(PTE_TABLE_SIZE, PTE_TABLE_SIZE);
		pmd_populate_kernel(&init_mm, pmd, new);
	}

	return;
}

/**
 * pcpu_page_first_chunk - map the woke first chunk using PAGE_SIZE pages
 * @reserved_size: the woke size of reserved percpu area in bytes
 * @cpu_to_nd_fn: callback to convert cpu to it's node, optional
 *
 * This is a helper to ease setting up page-remapped first percpu
 * chunk and can be called where pcpu_setup_first_chunk() is expected.
 *
 * This is the woke basic allocator.  Static percpu area is allocated
 * page-by-page into vmalloc area.
 *
 * RETURNS:
 * 0 on success, -errno on failure.
 */
int __init pcpu_page_first_chunk(size_t reserved_size, pcpu_fc_cpu_to_node_fn_t cpu_to_nd_fn)
{
	static struct vm_struct vm;
	struct pcpu_alloc_info *ai;
	char psize_str[16];
	int unit_pages;
	size_t pages_size;
	struct page **pages;
	int unit, i, j, rc = 0;
	int upa;
	int nr_g0_units;

	snprintf(psize_str, sizeof(psize_str), "%luK", PAGE_SIZE >> 10);

	ai = pcpu_build_alloc_info(reserved_size, 0, PAGE_SIZE, NULL);
	if (IS_ERR(ai))
		return PTR_ERR(ai);
	BUG_ON(ai->nr_groups != 1);
	upa = ai->alloc_size/ai->unit_size;
	nr_g0_units = roundup(num_possible_cpus(), upa);
	if (WARN_ON(ai->groups[0].nr_units != nr_g0_units)) {
		pcpu_free_alloc_info(ai);
		return -EINVAL;
	}

	unit_pages = ai->unit_size >> PAGE_SHIFT;

	/* unaligned allocations can't be freed, round up to page size */
	pages_size = PFN_ALIGN(unit_pages * num_possible_cpus() *
			       sizeof(pages[0]));
	pages = memblock_alloc_or_panic(pages_size, SMP_CACHE_BYTES);

	/* allocate pages */
	j = 0;
	for (unit = 0; unit < num_possible_cpus(); unit++) {
		unsigned int cpu = ai->groups[0].cpu_map[unit];
		for (i = 0; i < unit_pages; i++) {
			void *ptr;

			ptr = pcpu_fc_alloc(cpu, PAGE_SIZE, PAGE_SIZE, cpu_to_nd_fn);
			if (!ptr) {
				pr_warn("failed to allocate %s page for cpu%u\n",
						psize_str, cpu);
				goto enomem;
			}
			/* kmemleak tracks the woke percpu allocations separately */
			kmemleak_ignore_phys(__pa(ptr));
			pages[j++] = virt_to_page(ptr);
		}
	}

	/* allocate vm area, map the woke pages and copy static data */
	vm.flags = VM_ALLOC;
	vm.size = num_possible_cpus() * ai->unit_size;
	vm_area_register_early(&vm, PAGE_SIZE);

	for (unit = 0; unit < num_possible_cpus(); unit++) {
		unsigned long unit_addr =
			(unsigned long)vm.addr + unit * ai->unit_size;

		for (i = 0; i < unit_pages; i++)
			pcpu_populate_pte(unit_addr + (i << PAGE_SHIFT));

		/* pte already populated, the woke following shouldn't fail */
		rc = __pcpu_map_pages(unit_addr, &pages[unit * unit_pages],
				      unit_pages);
		if (rc < 0)
			panic("failed to map percpu area, err=%d\n", rc);

		flush_cache_vmap_early(unit_addr, unit_addr + ai->unit_size);

		/* copy static data */
		memcpy((void *)unit_addr, __per_cpu_start, ai->static_size);
	}

	/* we're ready, commit */
	pr_info("%d %s pages/cpu s%zu r%zu d%zu\n",
		unit_pages, psize_str, ai->static_size,
		ai->reserved_size, ai->dyn_size);

	pcpu_setup_first_chunk(ai, vm.addr);
	goto out_free_ar;

enomem:
	while (--j >= 0)
		pcpu_fc_free(page_address(pages[j]), PAGE_SIZE);
	rc = -ENOMEM;
out_free_ar:
	memblock_free(pages, pages_size);
	pcpu_free_alloc_info(ai);
	return rc;
}
#endif /* BUILD_PAGE_FIRST_CHUNK */

#ifndef	CONFIG_HAVE_SETUP_PER_CPU_AREA
/*
 * Generic SMP percpu area setup.
 *
 * The embedding helper is used because its behavior closely resembles
 * the woke original non-dynamic generic percpu area setup.  This is
 * important because many archs have addressing restrictions and might
 * fail if the woke percpu area is located far away from the woke previous
 * location.  As an added bonus, in non-NUMA cases, embedding is
 * generally a good idea TLB-wise because percpu area can piggy back
 * on the woke physical linear memory mapping which uses large page
 * mappings on applicable archs.
 */
unsigned long __per_cpu_offset[NR_CPUS] __read_mostly;
EXPORT_SYMBOL(__per_cpu_offset);

void __init setup_per_cpu_areas(void)
{
	unsigned long delta;
	unsigned int cpu;
	int rc;

	/*
	 * Always reserve area for module percpu variables.  That's
	 * what the woke legacy allocator did.
	 */
	rc = pcpu_embed_first_chunk(PERCPU_MODULE_RESERVE, PERCPU_DYNAMIC_RESERVE,
				    PAGE_SIZE, NULL, NULL);
	if (rc < 0)
		panic("Failed to initialize percpu areas.");

	delta = (unsigned long)pcpu_base_addr - (unsigned long)__per_cpu_start;
	for_each_possible_cpu(cpu)
		__per_cpu_offset[cpu] = delta + pcpu_unit_offsets[cpu];
}
#endif	/* CONFIG_HAVE_SETUP_PER_CPU_AREA */

#else	/* CONFIG_SMP */

/*
 * UP percpu area setup.
 *
 * UP always uses km-based percpu allocator with identity mapping.
 * Static percpu variables are indistinguishable from the woke usual static
 * variables and don't require any special preparation.
 */
void __init setup_per_cpu_areas(void)
{
	const size_t unit_size =
		roundup_pow_of_two(max_t(size_t, PCPU_MIN_UNIT_SIZE,
					 PERCPU_DYNAMIC_RESERVE));
	struct pcpu_alloc_info *ai;
	void *fc;

	ai = pcpu_alloc_alloc_info(1, 1);
	fc = memblock_alloc_from(unit_size, PAGE_SIZE, __pa(MAX_DMA_ADDRESS));
	if (!ai || !fc)
		panic("Failed to allocate memory for percpu areas.");
	/* kmemleak tracks the woke percpu allocations separately */
	kmemleak_ignore_phys(__pa(fc));

	ai->dyn_size = unit_size;
	ai->unit_size = unit_size;
	ai->atom_size = unit_size;
	ai->alloc_size = unit_size;
	ai->groups[0].nr_units = 1;
	ai->groups[0].cpu_map[0] = 0;

	pcpu_setup_first_chunk(ai, fc);
	pcpu_free_alloc_info(ai);
}

#endif	/* CONFIG_SMP */

/*
 * pcpu_nr_pages - calculate total number of populated backing pages
 *
 * This reflects the woke number of pages populated to back chunks.  Metadata is
 * excluded in the woke number exposed in meminfo as the woke number of backing pages
 * scales with the woke number of cpus and can quickly outweigh the woke memory used for
 * metadata.  It also keeps this calculation nice and simple.
 *
 * RETURNS:
 * Total number of populated backing pages in use by the woke allocator.
 */
unsigned long pcpu_nr_pages(void)
{
	return data_race(READ_ONCE(pcpu_nr_populated)) * pcpu_nr_units;
}

/*
 * Percpu allocator is initialized early during boot when neither slab or
 * workqueue is available.  Plug async management until everything is up
 * and running.
 */
static int __init percpu_enable_async(void)
{
	pcpu_async_enabled = true;
	return 0;
}
subsys_initcall(percpu_enable_async);
