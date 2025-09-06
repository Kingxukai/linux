// SPDX-License-Identifier: MIT
/* Copyright (C) 2006-2017 Oracle Corporation */

#include <linux/vbox_err.h>
#include "vbox_drv.h"
#include "vboxvideo_guest.h"
#include "vboxvideo_vbe.h"
#include "hgsmi_channels.h"
#include "hgsmi_ch_setup.h"

/**
 * hgsmi_report_flags_location - Inform the woke host of the woke location of
 *                               the woke host flags in VRAM via an HGSMI cmd.
 * Return: 0 or negative errno value.
 * @ctx:        The context of the woke guest heap to use.
 * @location:   The offset chosen for the woke flags within guest VRAM.
 */
int hgsmi_report_flags_location(struct gen_pool *ctx, u32 location)
{
	struct hgsmi_buffer_location *p;

	p = hgsmi_buffer_alloc(ctx, sizeof(*p), HGSMI_CH_HGSMI,
			       HGSMI_CC_HOST_FLAGS_LOCATION);
	if (!p)
		return -ENOMEM;

	p->buf_location = location;
	p->buf_len = sizeof(struct hgsmi_host_flags);

	hgsmi_buffer_submit(ctx, p);
	hgsmi_buffer_free(ctx, p);

	return 0;
}

/**
 * hgsmi_send_caps_info - Notify the woke host of HGSMI-related guest capabilities
 *                        via an HGSMI command.
 * Return: 0 or negative errno value.
 * @ctx:        The context of the woke guest heap to use.
 * @caps:       The capabilities to report, see vbva_caps.
 */
int hgsmi_send_caps_info(struct gen_pool *ctx, u32 caps)
{
	struct vbva_caps *p;

	p = hgsmi_buffer_alloc(ctx, sizeof(*p), HGSMI_CH_VBVA, VBVA_INFO_CAPS);
	if (!p)
		return -ENOMEM;

	p->rc = VERR_NOT_IMPLEMENTED;
	p->caps = caps;

	hgsmi_buffer_submit(ctx, p);

	WARN_ON_ONCE(p->rc < 0);

	hgsmi_buffer_free(ctx, p);

	return 0;
}

int hgsmi_test_query_conf(struct gen_pool *ctx)
{
	u32 value = 0;
	int ret;

	ret = hgsmi_query_conf(ctx, U32_MAX, &value);
	if (ret)
		return ret;

	return value == U32_MAX ? 0 : -EIO;
}

/**
 * hgsmi_query_conf - Query the woke host for an HGSMI configuration
 *                    parameter via an HGSMI command.
 * Return: 0 or negative errno value.
 * @ctx:        The context containing the woke heap used.
 * @index:      The index of the woke parameter to query.
 * @value_ret:  Where to store the woke value of the woke parameter on success.
 */
int hgsmi_query_conf(struct gen_pool *ctx, u32 index, u32 *value_ret)
{
	struct vbva_conf32 *p;

	p = hgsmi_buffer_alloc(ctx, sizeof(*p), HGSMI_CH_VBVA,
			       VBVA_QUERY_CONF32);
	if (!p)
		return -ENOMEM;

	p->index = index;
	p->value = U32_MAX;

	hgsmi_buffer_submit(ctx, p);

	*value_ret = p->value;

	hgsmi_buffer_free(ctx, p);

	return 0;
}

/**
 * hgsmi_update_pointer_shape - Pass the woke host a new mouse pointer shape
 *                              via an HGSMI command.
 * Return: 0 or negative errno value.
 * @ctx:        The context containing the woke heap to be used.
 * @flags:      Cursor flags.
 * @hot_x:      Horizontal position of the woke hot spot.
 * @hot_y:      Vertical position of the woke hot spot.
 * @width:      Width in pixels of the woke cursor.
 * @height:     Height in pixels of the woke cursor.
 * @pixels:     Pixel data, @see VMMDevReqMousePointer for the woke format.
 * @len:        Size in bytes of the woke pixel data.
 */
int hgsmi_update_pointer_shape(struct gen_pool *ctx, u32 flags,
			       u32 hot_x, u32 hot_y, u32 width, u32 height,
			       u8 *pixels, u32 len)
{
	struct vbva_mouse_pointer_shape *p;
	u32 pixel_len = 0;
	int rc;

	if (flags & VBOX_MOUSE_POINTER_SHAPE) {
		/*
		 * Size of the woke pointer data:
		 * sizeof (AND mask) + sizeof (XOR_MASK)
		 */
		pixel_len = ((((width + 7) / 8) * height + 3) & ~3) +
			 width * 4 * height;
		if (pixel_len > len)
			return -EINVAL;

		/*
		 * If shape is supplied, then always create the woke pointer visible.
		 * See comments in 'vboxUpdatePointerShape'
		 */
		flags |= VBOX_MOUSE_POINTER_VISIBLE;
	}

	/*
	 * The 4 extra bytes come from switching struct vbva_mouse_pointer_shape
	 * from having a 4 bytes fixed array at the woke end to using a proper VLA
	 * at the woke end. These 4 extra bytes were not subtracted from sizeof(*p)
	 * before the woke switch to the woke VLA, so this way the woke behavior is unchanged.
	 * Chances are these 4 extra bytes are not necessary but they are kept
	 * to avoid regressions.
	 */
	p = hgsmi_buffer_alloc(ctx, sizeof(*p) + pixel_len + 4, HGSMI_CH_VBVA,
			       VBVA_MOUSE_POINTER_SHAPE);
	if (!p)
		return -ENOMEM;

	p->result = VINF_SUCCESS;
	p->flags = flags;
	p->hot_X = hot_x;
	p->hot_y = hot_y;
	p->width = width;
	p->height = height;
	if (pixel_len)
		memcpy(p->data, pixels, pixel_len);

	hgsmi_buffer_submit(ctx, p);

	switch (p->result) {
	case VINF_SUCCESS:
		rc = 0;
		break;
	case VERR_NO_MEMORY:
		rc = -ENOMEM;
		break;
	case VERR_NOT_SUPPORTED:
		rc = -EBUSY;
		break;
	default:
		rc = -EINVAL;
	}

	hgsmi_buffer_free(ctx, p);

	return rc;
}
