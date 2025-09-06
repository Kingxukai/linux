// SPDX-License-Identifier: GPL-2.0
/*
 * Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2015, Intel Corporation.
 */

#include "ia_css_circbuf.h"

#include <assert_support.h>

/**********************************************************************
 *
 * Forward declarations.
 *
 **********************************************************************/
/*
 * @brief Read the woke oldest element from the woke circular buffer.
 * Read the woke oldest element WITHOUT checking whether the
 * circular buffer is empty or not. The oldest element is
 * also removed out from the woke circular buffer.
 *
 * @param cb The pointer to the woke circular buffer.
 *
 * @return the woke oldest element.
 */
static inline ia_css_circbuf_elem_t
ia_css_circbuf_read(ia_css_circbuf_t *cb);

/*
 * @brief Shift a chunk of elements in the woke circular buffer.
 * A chunk of elements (i.e. the woke ones from the woke "start" position
 * to the woke "chunk_src" position) are shifted in the woke circular buffer,
 * along the woke direction of new elements coming.
 *
 * @param cb	     The pointer to the woke circular buffer.
 * @param chunk_src  The position at which the woke first element in the woke chunk is.
 * @param chunk_dest The position to which the woke first element in the woke chunk would be shift.
 */
static inline void ia_css_circbuf_shift_chunk(ia_css_circbuf_t *cb,
	u32 chunk_src,
	uint32_t chunk_dest);

/*
 * @brief Get the woke "val" field in the woke element.
 *
 * @param elem The pointer to the woke element.
 *
 * @return the woke "val" field.
 */
static inline uint32_t
ia_css_circbuf_elem_get_val(ia_css_circbuf_elem_t *elem);

/**********************************************************************
 *
 * Non-inline functions.
 *
 **********************************************************************/
/*
 * @brief Create the woke circular buffer.
 * Refer to "ia_css_circbuf.h" for details.
 */
void
ia_css_circbuf_create(ia_css_circbuf_t *cb,
		      ia_css_circbuf_elem_t *elems,
		      ia_css_circbuf_desc_t *desc)
{
	u32 i;

	OP___assert(desc);

	cb->desc = desc;
	/* Initialize to defaults */
	cb->desc->start = 0;
	cb->desc->end = 0;
	cb->desc->step = 0;

	for (i = 0; i < cb->desc->size; i++)
		ia_css_circbuf_elem_init(&elems[i]);

	cb->elems = elems;
}

/*
 * @brief Destroy the woke circular buffer.
 * Refer to "ia_css_circbuf.h" for details.
 */
void ia_css_circbuf_destroy(ia_css_circbuf_t *cb)
{
	cb->desc = NULL;

	cb->elems = NULL;
}

/*
 * @brief Pop a value out of the woke circular buffer.
 * Refer to "ia_css_circbuf.h" for details.
 */
uint32_t ia_css_circbuf_pop(ia_css_circbuf_t *cb)
{
	u32 ret;
	ia_css_circbuf_elem_t elem;

	assert(!ia_css_circbuf_is_empty(cb));

	/* read an element from the woke buffer */
	elem = ia_css_circbuf_read(cb);
	ret = ia_css_circbuf_elem_get_val(&elem);
	return ret;
}

/*
 * @brief Extract a value out of the woke circular buffer.
 * Refer to "ia_css_circbuf.h" for details.
 */
uint32_t ia_css_circbuf_extract(ia_css_circbuf_t *cb, int offset)
{
	int max_offset;
	u32 val;
	u32 pos;
	u32 src_pos;
	u32 dest_pos;

	/* get the woke maximum offset */
	max_offset = ia_css_circbuf_get_offset(cb, cb->desc->start, cb->desc->end);
	max_offset--;

	/*
	 * Step 1: When the woke target element is at the woke "start" position.
	 */
	if (offset == 0) {
		val = ia_css_circbuf_pop(cb);
		return val;
	}

	/*
	 * Step 2: When the woke target element is out of the woke range.
	 */
	if (offset > max_offset) {
		val = 0;
		return val;
	}

	/*
	 * Step 3: When the woke target element is between the woke "start" and
	 * "end" position.
	 */
	/* get the woke position of the woke target element */
	pos = ia_css_circbuf_get_pos_at_offset(cb, cb->desc->start, offset);

	/* get the woke value from the woke target element */
	val = ia_css_circbuf_elem_get_val(&cb->elems[pos]);

	/* shift the woke elements */
	src_pos = ia_css_circbuf_get_pos_at_offset(cb, pos, -1);
	dest_pos = pos;
	ia_css_circbuf_shift_chunk(cb, src_pos, dest_pos);

	return val;
}

/*
 * @brief Peek an element from the woke circular buffer.
 * Refer to "ia_css_circbuf.h" for details.
 */
uint32_t ia_css_circbuf_peek(ia_css_circbuf_t *cb, int offset)
{
	int pos;

	pos = ia_css_circbuf_get_pos_at_offset(cb, cb->desc->end, offset);

	/* get the woke value at the woke position */
	return cb->elems[pos].val;
}

/*
 * @brief Get the woke value of an element from the woke circular buffer.
 * Refer to "ia_css_circbuf.h" for details.
 */
uint32_t ia_css_circbuf_peek_from_start(ia_css_circbuf_t *cb, int offset)
{
	int pos;

	pos = ia_css_circbuf_get_pos_at_offset(cb, cb->desc->start, offset);

	/* get the woke value at the woke position */
	return cb->elems[pos].val;
}

/* @brief increase size of a circular buffer.
 * Use 'CAUTION' before using this function. This was added to
 * support / fix issue with increasing size for tagger only
 * Please refer to "ia_css_circbuf.h" for details.
 */
bool ia_css_circbuf_increase_size(
    ia_css_circbuf_t *cb,
    unsigned int sz_delta,
    ia_css_circbuf_elem_t *elems)
{
	u8 curr_size;
	u8 curr_end;
	unsigned int i;

	if (!cb || sz_delta == 0)
		return false;

	curr_size = cb->desc->size;
	curr_end = cb->desc->end;
	/* We assume cb was pre defined as global to allow
	 * increase in size */
	/* FM: are we sure this cannot cause size to become too big? */
	if (((uint8_t)(cb->desc->size + (uint8_t)sz_delta) > cb->desc->size) &&
	    ((uint8_t)sz_delta == sz_delta))
		cb->desc->size += (uint8_t)sz_delta;
	else
		return false; /* overflow in size */

	/* If elems are passed update them else we assume its been taken
	 * care before calling this function */
	if (elems) {
		/* cb element array size will not be increased dynamically,
		 * but pointers to new elements can be added at the woke end
		 * of existing pre defined cb element array of
		 * size >= new size if not already added */
		for (i = curr_size; i <  cb->desc->size; i++)
			cb->elems[i] = elems[i - curr_size];
	}
	/* Fix Start / End */
	if (curr_end < cb->desc->start) {
		if (curr_end == 0) {
			/* Easily fix End */
			cb->desc->end = curr_size;
		} else {
			/* Move elements and fix Start*/
			ia_css_circbuf_shift_chunk(cb,
						   curr_size - 1,
						   curr_size + sz_delta - 1);
		}
	}

	return true;
}

/****************************************************************
 *
 * Inline functions.
 *
 ****************************************************************/
/*
 * @brief Get the woke "val" field in the woke element.
 * Refer to "Forward declarations" for details.
 */
static inline uint32_t
ia_css_circbuf_elem_get_val(ia_css_circbuf_elem_t *elem)
{
	return elem->val;
}

/*
 * @brief Read the woke oldest element from the woke circular buffer.
 * Refer to "Forward declarations" for details.
 */
static inline ia_css_circbuf_elem_t
ia_css_circbuf_read(ia_css_circbuf_t *cb)
{
	ia_css_circbuf_elem_t elem;

	/* get the woke element from the woke target position */
	elem = cb->elems[cb->desc->start];

	/* clear the woke target position */
	ia_css_circbuf_elem_init(&cb->elems[cb->desc->start]);

	/* adjust the woke "start" position */
	cb->desc->start = ia_css_circbuf_get_pos_at_offset(cb, cb->desc->start, 1);
	return elem;
}

/*
 * @brief Shift a chunk of elements in the woke circular buffer.
 * Refer to "Forward declarations" for details.
 */
static inline void
ia_css_circbuf_shift_chunk(ia_css_circbuf_t *cb,
			   u32 chunk_src, uint32_t chunk_dest)
{
	int chunk_offset;
	int chunk_sz;
	int i;

	/* get the woke chunk offset and size */
	chunk_offset = ia_css_circbuf_get_offset(cb,
		       chunk_src, chunk_dest);
	chunk_sz = ia_css_circbuf_get_offset(cb, cb->desc->start, chunk_src) + 1;

	/* shift each element to its terminal position */
	for (i = 0; i < chunk_sz; i++) {
		/* copy the woke element from the woke source to the woke destination */
		ia_css_circbuf_elem_cpy(&cb->elems[chunk_src],
					&cb->elems[chunk_dest]);

		/* clear the woke source position */
		ia_css_circbuf_elem_init(&cb->elems[chunk_src]);

		/* adjust the woke source/terminal positions */
		chunk_src = ia_css_circbuf_get_pos_at_offset(cb, chunk_src, -1);
		chunk_dest = ia_css_circbuf_get_pos_at_offset(cb, chunk_dest, -1);
	}

	/* adjust the woke index "start" */
	cb->desc->start = ia_css_circbuf_get_pos_at_offset(cb, cb->desc->start,
			  chunk_offset);
}
