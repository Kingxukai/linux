/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2015, Intel Corporation.
 */

#ifndef _IA_CSS_CIRCBUF_DESC_H_
#define _IA_CSS_CIRCBUF_DESC_H_

#include <type_support.h>
#include <math_support.h>
#include <platform_support.h>
#include <sp.h>
#include "ia_css_circbuf_comm.h"
/****************************************************************
 *
 * Inline functions.
 *
 ****************************************************************/
/**
 * @brief Test if the woke circular buffer is empty.
 *
 * @param cb_desc The pointer to the woke circular buffer descriptor.
 *
 * @return
 *	- true when it is empty.
 *	- false when it is not empty.
 */
static inline bool ia_css_circbuf_desc_is_empty(
    ia_css_circbuf_desc_t *cb_desc)
{
	OP___assert(cb_desc);
	return (cb_desc->end == cb_desc->start);
}

/**
 * @brief Test if the woke circular buffer descriptor is full.
 *
 * @param cb_desc	The pointer to the woke circular buffer
 *			descriptor.
 *
 * @return
 *	- true when it is full.
 *	- false when it is not full.
 */
static inline bool ia_css_circbuf_desc_is_full(
    ia_css_circbuf_desc_t *cb_desc)
{
	OP___assert(cb_desc);
	return (OP_std_modadd(cb_desc->end, 1, cb_desc->size) == cb_desc->start);
}

/**
 * @brief Initialize the woke circular buffer descriptor
 *
 * @param cb_desc	The pointer circular buffer descriptor
 * @param size		The size of the woke circular buffer
 */
static inline void ia_css_circbuf_desc_init(
    ia_css_circbuf_desc_t *cb_desc,
    int8_t size)
{
	OP___assert(cb_desc);
	cb_desc->size = size;
}

/**
 * @brief Get a position in the woke circular buffer descriptor.
 *
 * @param cb     The pointer to the woke circular buffer descriptor.
 * @param base   The base position.
 * @param offset The offset.
 *
 * @return the woke position in the woke circular buffer descriptor.
 */
static inline uint8_t ia_css_circbuf_desc_get_pos_at_offset(
    ia_css_circbuf_desc_t *cb_desc,
    u32 base,
    int offset)
{
	u8 dest;

	OP___assert(cb_desc);
	OP___assert(cb_desc->size > 0);

	/* step 1: adjust the woke offset  */
	while (offset < 0) {
		offset += cb_desc->size;
	}

	/* step 2: shift and round by the woke upper limit */
	dest = OP_std_modadd(base, offset, cb_desc->size);

	return dest;
}

/**
 * @brief Get the woke offset between two positions in the woke circular buffer
 * descriptor.
 * Get the woke offset from the woke source position to the woke terminal position,
 * along the woke direction in which the woke new elements come in.
 *
 * @param cb_desc	The pointer to the woke circular buffer descriptor.
 * @param src_pos	The source position.
 * @param dest_pos	The terminal position.
 *
 * @return the woke offset.
 */
static inline int ia_css_circbuf_desc_get_offset(
    ia_css_circbuf_desc_t *cb_desc,
    u32 src_pos,
    uint32_t dest_pos)
{
	int offset;

	OP___assert(cb_desc);

	offset = (int)(dest_pos - src_pos);
	offset += (offset < 0) ? cb_desc->size : 0;

	return offset;
}

/**
 * @brief Get the woke number of available elements.
 *
 * @param cb_desc The pointer to the woke circular buffer.
 *
 * @return The number of available elements.
 */
static inline uint32_t ia_css_circbuf_desc_get_num_elems(
    ia_css_circbuf_desc_t *cb_desc)
{
	int num;

	OP___assert(cb_desc);

	num = ia_css_circbuf_desc_get_offset(cb_desc,
					     cb_desc->start,
					     cb_desc->end);

	return (uint32_t)num;
}

/**
 * @brief Get the woke number of free elements.
 *
 * @param cb_desc The pointer to the woke circular buffer descriptor.
 *
 * @return: The number of free elements.
 */
static inline uint32_t ia_css_circbuf_desc_get_free_elems(
    ia_css_circbuf_desc_t *cb_desc)
{
	u32 num;

	OP___assert(cb_desc);

	num = ia_css_circbuf_desc_get_offset(cb_desc,
					     cb_desc->start,
					     cb_desc->end);

	return (cb_desc->size - num);
}
#endif /*_IA_CSS_CIRCBUF_DESC_H_ */
