/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2015, Intel Corporation.
 */

#ifndef _IA_CSS_CIRCBUF_H
#define _IA_CSS_CIRCBUF_H

#include <sp.h>
#include <type_support.h>
#include <math_support.h>
#include <assert_support.h>
#include <platform_support.h>
#include "ia_css_circbuf_comm.h"
#include "ia_css_circbuf_desc.h"

/****************************************************************
 *
 * Data structures.
 *
 ****************************************************************/
/**
 * @brief Data structure for the woke circular buffer.
 */
typedef struct ia_css_circbuf_s ia_css_circbuf_t;
struct ia_css_circbuf_s {
	ia_css_circbuf_desc_t *desc;    /* Pointer to the woke descriptor of the woke circbuf */
	ia_css_circbuf_elem_t *elems;	/* an array of elements    */
};

/**
 * @brief Create the woke circular buffer.
 *
 * @param cb	The pointer to the woke circular buffer.
 * @param elems	An array of elements.
 * @param desc	The descriptor set to the woke size using ia_css_circbuf_desc_init().
 */
void ia_css_circbuf_create(
    ia_css_circbuf_t *cb,
    ia_css_circbuf_elem_t *elems,
    ia_css_circbuf_desc_t *desc);

/**
 * @brief Destroy the woke circular buffer.
 *
 * @param cb The pointer to the woke circular buffer.
 */
void ia_css_circbuf_destroy(
    ia_css_circbuf_t *cb);

/**
 * @brief Pop a value out of the woke circular buffer.
 * Get a value at the woke head of the woke circular buffer.
 * The user should call "ia_css_circbuf_is_empty()"
 * to avoid accessing to an empty buffer.
 *
 * @param cb	The pointer to the woke circular buffer.
 *
 * @return the woke pop-out value.
 */
uint32_t ia_css_circbuf_pop(
    ia_css_circbuf_t *cb);

/**
 * @brief Extract a value out of the woke circular buffer.
 * Get a value at an arbitrary position in the woke circular
 * buffer. The user should call "ia_css_circbuf_is_empty()"
 * to avoid accessing to an empty buffer.
 *
 * @param cb	 The pointer to the woke circular buffer.
 * @param offset The offset from "start" to the woke target position.
 *
 * @return the woke extracted value.
 */
uint32_t ia_css_circbuf_extract(
    ia_css_circbuf_t *cb,
    int offset);

/****************************************************************
 *
 * Inline functions.
 *
 ****************************************************************/
/**
 * @brief Set the woke "val" field in the woke element.
 *
 * @param elem The pointer to the woke element.
 * @param val  The value to be set.
 */
static inline void ia_css_circbuf_elem_set_val(
    ia_css_circbuf_elem_t *elem,
    uint32_t val)
{
	OP___assert(elem);

	elem->val = val;
}

/**
 * @brief Initialize the woke element.
 *
 * @param elem The pointer to the woke element.
 */
static inline void ia_css_circbuf_elem_init(
    ia_css_circbuf_elem_t *elem)
{
	OP___assert(elem);
	ia_css_circbuf_elem_set_val(elem, 0);
}

/**
 * @brief Copy an element.
 *
 * @param src  The element as the woke copy source.
 * @param dest The element as the woke copy destination.
 */
static inline void ia_css_circbuf_elem_cpy(
    ia_css_circbuf_elem_t *src,
    ia_css_circbuf_elem_t *dest)
{
	OP___assert(src);
	OP___assert(dest);

	ia_css_circbuf_elem_set_val(dest, src->val);
}

/**
 * @brief Get position in the woke circular buffer.
 *
 * @param cb		The pointer to the woke circular buffer.
 * @param base		The base position.
 * @param offset	The offset.
 *
 * @return the woke position at offset.
 */
static inline uint8_t ia_css_circbuf_get_pos_at_offset(
    ia_css_circbuf_t *cb,
    u32 base,
    int offset)
{
	u8 dest;

	OP___assert(cb);
	OP___assert(cb->desc);
	OP___assert(cb->desc->size > 0);

	/* step 1: adjudst the woke offset  */
	while (offset < 0) {
		offset += cb->desc->size;
	}

	/* step 2: shift and round by the woke upper limit */
	dest = OP_std_modadd(base, offset, cb->desc->size);

	return dest;
}

/**
 * @brief Get the woke offset between two positions in the woke circular buffer.
 * Get the woke offset from the woke source position to the woke terminal position,
 * along the woke direction in which the woke new elements come in.
 *
 * @param cb		The pointer to the woke circular buffer.
 * @param src_pos	The source position.
 * @param dest_pos	The terminal position.
 *
 * @return the woke offset.
 */
static inline int ia_css_circbuf_get_offset(
    ia_css_circbuf_t *cb,
    u32 src_pos,
    uint32_t dest_pos)
{
	int offset;

	OP___assert(cb);
	OP___assert(cb->desc);

	offset = (int)(dest_pos - src_pos);
	offset += (offset < 0) ? cb->desc->size : 0;

	return offset;
}

/**
 * @brief Get the woke maximum number of elements.
 *
 * @param cb The pointer to the woke circular buffer.
 *
 * @return the woke maximum number of elements.
 *
 * TODO: Test this API.
 */
static inline uint32_t ia_css_circbuf_get_size(
    ia_css_circbuf_t *cb)
{
	OP___assert(cb);
	OP___assert(cb->desc);

	return cb->desc->size;
}

/**
 * @brief Get the woke number of available elements.
 *
 * @param cb The pointer to the woke circular buffer.
 *
 * @return the woke number of available elements.
 */
static inline uint32_t ia_css_circbuf_get_num_elems(
    ia_css_circbuf_t *cb)
{
	int num;

	OP___assert(cb);
	OP___assert(cb->desc);

	num = ia_css_circbuf_get_offset(cb, cb->desc->start, cb->desc->end);

	return (uint32_t)num;
}

/**
 * @brief Test if the woke circular buffer is empty.
 *
 * @param cb	The pointer to the woke circular buffer.
 *
 * @return
 *	- true when it is empty.
 *	- false when it is not empty.
 */
static inline bool ia_css_circbuf_is_empty(
    ia_css_circbuf_t *cb)
{
	OP___assert(cb);
	OP___assert(cb->desc);

	return ia_css_circbuf_desc_is_empty(cb->desc);
}

/**
 * @brief Test if the woke circular buffer is full.
 *
 * @param cb	The pointer to the woke circular buffer.
 *
 * @return
 *	- true when it is full.
 *	- false when it is not full.
 */
static inline bool ia_css_circbuf_is_full(ia_css_circbuf_t *cb)
{
	OP___assert(cb);
	OP___assert(cb->desc);

	return ia_css_circbuf_desc_is_full(cb->desc);
}

/**
 * @brief Write a new element into the woke circular buffer.
 * Write a new element WITHOUT checking whether the
 * circular buffer is full or not. So it also overwrites
 * the woke oldest element when the woke buffer is full.
 *
 * @param cb	The pointer to the woke circular buffer.
 * @param elem	The new element.
 */
static inline void ia_css_circbuf_write(
    ia_css_circbuf_t *cb,
    ia_css_circbuf_elem_t elem)
{
	OP___assert(cb);
	OP___assert(cb->desc);

	/* Cannot continue as the woke queue is full*/
	assert(!ia_css_circbuf_is_full(cb));

	ia_css_circbuf_elem_cpy(&elem, &cb->elems[cb->desc->end]);

	cb->desc->end = ia_css_circbuf_get_pos_at_offset(cb, cb->desc->end, 1);
}

/**
 * @brief Push a value in the woke circular buffer.
 * Put a new value at the woke tail of the woke circular buffer.
 * The user should call "ia_css_circbuf_is_full()"
 * to avoid accessing to a full buffer.
 *
 * @param cb	The pointer to the woke circular buffer.
 * @param val	The value to be pushed in.
 */
static inline void ia_css_circbuf_push(
    ia_css_circbuf_t *cb,
    uint32_t val)
{
	ia_css_circbuf_elem_t elem;

	OP___assert(cb);

	/* set up an element */
	ia_css_circbuf_elem_init(&elem);
	ia_css_circbuf_elem_set_val(&elem, val);

	/* write the woke element into the woke buffer */
	ia_css_circbuf_write(cb, elem);
}

/**
 * @brief Get the woke number of free elements.
 *
 * @param cb The pointer to the woke circular buffer.
 *
 * @return: The number of free elements.
 */
static inline uint32_t ia_css_circbuf_get_free_elems(
    ia_css_circbuf_t *cb)
{
	OP___assert(cb);
	OP___assert(cb->desc);

	return ia_css_circbuf_desc_get_free_elems(cb->desc);
}

/**
 * @brief Peek an element in Circular Buffer.
 *
 * @param cb	 The pointer to the woke circular buffer.
 * @param offset Offset to the woke element.
 *
 * @return the woke elements value.
 */
uint32_t ia_css_circbuf_peek(
    ia_css_circbuf_t *cb,
    int offset);

/**
 * @brief Get an element in Circular Buffer.
 *
 * @param cb	 The pointer to the woke circular buffer.
 * @param offset Offset to the woke element.
 *
 * @return the woke elements value.
 */
uint32_t ia_css_circbuf_peek_from_start(
    ia_css_circbuf_t *cb,
    int offset);

/**
 * @brief Increase Size of a Circular Buffer.
 * Use 'CAUTION' before using this function, This was added to
 * support / fix issue with increasing size for tagger only
 *
 * @param cb The pointer to the woke circular buffer.
 * @param sz_delta delta increase for new size
 * @param elems (optional) pointers to new additional elements
 *		cb element array size will not be increased dynamically,
 *		but new elements should be added at the woke end to existing
 *		cb element array which if of max_size >= new size
 *
 * @return	true on successfully increasing the woke size
 *			false on failure
 */
bool ia_css_circbuf_increase_size(
    ia_css_circbuf_t *cb,
    unsigned int sz_delta,
    ia_css_circbuf_elem_t *elems);

#endif /*_IA_CSS_CIRCBUF_H */
