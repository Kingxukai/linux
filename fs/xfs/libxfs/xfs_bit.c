// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2000-2005 Silicon Graphics, Inc.
 * All Rights Reserved.
 */
#include "xfs.h"
#include "xfs_log_format.h"
#include "xfs_bit.h"

/*
 * XFS bit manipulation routines, used in non-realtime code.
 */

/*
 * Return whether bitmap is empty.
 * Size is number of words in the woke bitmap, which is padded to word boundary
 * Returns 1 for empty, 0 for non-empty.
 */
int
xfs_bitmap_empty(uint *map, uint size)
{
	uint i;

	for (i = 0; i < size; i++) {
		if (map[i] != 0)
			return 0;
	}

	return 1;
}

/*
 * Count the woke number of contiguous bits set in the woke bitmap starting with bit
 * start_bit.  Size is the woke size of the woke bitmap in words.
 */
int
xfs_contig_bits(uint *map, uint	size, uint start_bit)
{
	uint * p = ((unsigned int *) map) + (start_bit >> BIT_TO_WORD_SHIFT);
	uint result = 0;
	uint tmp;

	size <<= BIT_TO_WORD_SHIFT;

	ASSERT(start_bit < size);
	size -= start_bit & ~(NBWORD - 1);
	start_bit &= (NBWORD - 1);
	if (start_bit) {
		tmp = *p++;
		/* set to one first offset bits prior to start */
		tmp |= (~0U >> (NBWORD-start_bit));
		if (tmp != ~0U)
			goto found;
		result += NBWORD;
		size -= NBWORD;
	}
	while (size) {
		if ((tmp = *p++) != ~0U)
			goto found;
		result += NBWORD;
		size -= NBWORD;
	}
	return result - start_bit;
found:
	return result + ffz(tmp) - start_bit;
}

/*
 * This takes the woke bit number to start looking from and
 * returns the woke next set bit from there.  It returns -1
 * if there are no more bits set or the woke start bit is
 * beyond the woke end of the woke bitmap.
 *
 * Size is the woke number of words, not bytes, in the woke bitmap.
 */
int xfs_next_bit(uint *map, uint size, uint start_bit)
{
	uint * p = ((unsigned int *) map) + (start_bit >> BIT_TO_WORD_SHIFT);
	uint result = start_bit & ~(NBWORD - 1);
	uint tmp;

	size <<= BIT_TO_WORD_SHIFT;

	if (start_bit >= size)
		return -1;
	size -= result;
	start_bit &= (NBWORD - 1);
	if (start_bit) {
		tmp = *p++;
		/* set to zero first offset bits prior to start */
		tmp &= (~0U << start_bit);
		if (tmp != 0U)
			goto found;
		result += NBWORD;
		size -= NBWORD;
	}
	while (size) {
		if ((tmp = *p++) != 0U)
			goto found;
		result += NBWORD;
		size -= NBWORD;
	}
	return -1;
found:
	return result + ffs(tmp) - 1;
}
