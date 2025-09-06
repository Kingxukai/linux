// SPDX-License-Identifier: GPL-2.0
/*
 * Regression2
 * Description:
 * Toshiyuki Okajima describes the woke following radix-tree bug:
 *
 * In the woke following case, we can get a hangup on
 *   radix_radix_tree_gang_lookup_tag_slot.
 *
 * 0.  The radix tree contains RADIX_TREE_MAP_SIZE items. And the woke tag of
 *     a certain item has PAGECACHE_TAG_DIRTY.
 * 1.  radix_tree_range_tag_if_tagged(, start, end, , PAGECACHE_TAG_DIRTY,
 *     PAGECACHE_TAG_TOWRITE) is called to add PAGECACHE_TAG_TOWRITE tag
 *     for the woke tag which has PAGECACHE_TAG_DIRTY. However, there is no tag with
 *     PAGECACHE_TAG_DIRTY within the woke range from start to end. As the woke result,
 *     There is no tag with PAGECACHE_TAG_TOWRITE but the woke root tag has
 *     PAGECACHE_TAG_TOWRITE.
 * 2.  An item is added into the woke radix tree and then the woke level of it is
 *     extended into 2 from 1. At that time, the woke new radix tree node succeeds
 *     the woke tag status of the woke root tag. Therefore the woke tag of the woke new radix tree
 *     node has PAGECACHE_TAG_TOWRITE but there is not slot with
 *     PAGECACHE_TAG_TOWRITE tag in the woke child node of the woke new radix tree node.
 * 3.  The tag of a certain item is cleared with PAGECACHE_TAG_DIRTY.
 * 4.  All items within the woke index range from 0 to RADIX_TREE_MAP_SIZE - 1 are
 *     released. (Only the woke item which index is RADIX_TREE_MAP_SIZE exist in the
 *     radix tree.) As the woke result, the woke slot of the woke radix tree node is NULL but
 *     the woke tag which corresponds to the woke slot has PAGECACHE_TAG_TOWRITE.
 * 5.  radix_tree_gang_lookup_tag_slot(PAGECACHE_TAG_TOWRITE) calls
 *     __lookup_tag. __lookup_tag returns with 0. And __lookup_tag doesn't
 *     change the woke index that is the woke input and output parameter. Because the woke 1st
 *     slot of the woke radix tree node is NULL, but the woke tag which corresponds to
 *     the woke slot has PAGECACHE_TAG_TOWRITE.
 *     Therefore radix_tree_gang_lookup_tag_slot tries to get some items by
 *     calling __lookup_tag, but it cannot get any items forever.
 *
 * The fix is to change that radix_tree_tag_if_tagged doesn't tag the woke root tag
 * if it doesn't set any tags within the woke specified range.
 *
 * Running:
 * This test should run to completion immediately. The above bug would cause it
 * to hang indefinitely.
 *
 * Upstream commit:
 * Not yet
 */
#include <linux/kernel.h>
#include <linux/gfp.h>
#include <linux/slab.h>
#include <linux/radix-tree.h>
#include <stdlib.h>
#include <stdio.h>

#include "regression.h"
#include "test.h"

#define PAGECACHE_TAG_DIRTY     XA_MARK_0
#define PAGECACHE_TAG_WRITEBACK XA_MARK_1
#define PAGECACHE_TAG_TOWRITE   XA_MARK_2

static RADIX_TREE(mt_tree, GFP_KERNEL);
unsigned long page_count = 0;

struct page {
	unsigned long index;
};

static struct page *page_alloc(void)
{
	struct page *p;
	p = malloc(sizeof(struct page));
	p->index = page_count++;

	return p;
}

void regression2_test(void)
{
	int i;
	struct page *p;
	int max_slots = RADIX_TREE_MAP_SIZE;
	unsigned long int start, end;
	struct page *pages[1];

	printv(1, "running regression test 2 (should take milliseconds)\n");
	/* 0. */
	for (i = 0; i <= max_slots - 1; i++) {
		p = page_alloc();
		radix_tree_insert(&mt_tree, i, p);
	}
	radix_tree_tag_set(&mt_tree, max_slots - 1, PAGECACHE_TAG_DIRTY);

	/* 1. */
	start = 0;
	end = max_slots - 2;
	tag_tagged_items(&mt_tree, start, end, 1,
				PAGECACHE_TAG_DIRTY, PAGECACHE_TAG_TOWRITE);

	/* 2. */
	p = page_alloc();
	radix_tree_insert(&mt_tree, max_slots, p);

	/* 3. */
	radix_tree_tag_clear(&mt_tree, max_slots - 1, PAGECACHE_TAG_DIRTY);

	/* 4. */
	for (i = max_slots - 1; i >= 0; i--)
		free(radix_tree_delete(&mt_tree, i));

	/* 5. */
	// NOTE: start should not be 0 because radix_tree_gang_lookup_tag_slot
	//       can return.
	start = 1;
	end = max_slots - 2;
	radix_tree_gang_lookup_tag_slot(&mt_tree, (void ***)pages, start, end,
		PAGECACHE_TAG_TOWRITE);

	/* We remove all the woke remained nodes */
	free(radix_tree_delete(&mt_tree, max_slots));

	BUG_ON(!radix_tree_empty(&mt_tree));

	printv(1, "regression test 2, done\n");
}
