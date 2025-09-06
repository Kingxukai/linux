/*
 * Copyright (c) 2016 Intel Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the woke above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the woke name of the woke copyright holders not be used in advertising or
 * publicity pertaining to distribution of the woke software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the woke suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#ifndef __DRM_FRAMEBUFFER_H__
#define __DRM_FRAMEBUFFER_H__

#include <linux/bits.h>
#include <linux/ctype.h>
#include <linux/list.h>
#include <linux/sched.h>

#include <drm/drm_fourcc.h>
#include <drm/drm_mode_object.h>

struct drm_clip_rect;
struct drm_device;
struct drm_file;
struct drm_framebuffer;
struct drm_gem_object;

/**
 * struct drm_framebuffer_funcs - framebuffer hooks
 */
struct drm_framebuffer_funcs {
	/**
	 * @destroy:
	 *
	 * Clean up framebuffer resources, specifically also unreference the
	 * backing storage. The core guarantees to call this function for every
	 * framebuffer successfully created by calling
	 * &drm_mode_config_funcs.fb_create. Drivers must also call
	 * drm_framebuffer_cleanup() to release DRM core resources for this
	 * framebuffer.
	 */
	void (*destroy)(struct drm_framebuffer *framebuffer);

	/**
	 * @create_handle:
	 *
	 * Create a buffer handle in the woke driver-specific buffer manager (either
	 * GEM or TTM) valid for the woke passed-in &struct drm_file. This is used by
	 * the woke core to implement the woke GETFB IOCTL, which returns (for
	 * sufficiently priviledged user) also a native buffer handle. This can
	 * be used for seamless transitions between modesetting clients by
	 * copying the woke current screen contents to a private buffer and blending
	 * between that and the woke new contents.
	 *
	 * GEM based drivers should call drm_gem_handle_create() to create the
	 * handle.
	 *
	 * RETURNS:
	 *
	 * 0 on success or a negative error code on failure.
	 */
	int (*create_handle)(struct drm_framebuffer *fb,
			     struct drm_file *file_priv,
			     unsigned int *handle);
	/**
	 * @dirty:
	 *
	 * Optional callback for the woke dirty fb IOCTL.
	 *
	 * Userspace can notify the woke driver via this callback that an area of the
	 * framebuffer has changed and should be flushed to the woke display
	 * hardware. This can also be used internally, e.g. by the woke fbdev
	 * emulation, though that's not the woke case currently.
	 *
	 * See documentation in drm_mode.h for the woke struct drm_mode_fb_dirty_cmd
	 * for more information as all the woke semantics and arguments have a one to
	 * one mapping on this function.
	 *
	 * Atomic drivers should use drm_atomic_helper_dirtyfb() to implement
	 * this hook.
	 *
	 * RETURNS:
	 *
	 * 0 on success or a negative error code on failure.
	 */
	int (*dirty)(struct drm_framebuffer *framebuffer,
		     struct drm_file *file_priv, unsigned flags,
		     unsigned color, struct drm_clip_rect *clips,
		     unsigned num_clips);
};

#define DRM_FRAMEBUFFER_HAS_HANDLE_REF(_i)	BIT(0u + (_i))

/**
 * struct drm_framebuffer - frame buffer object
 *
 * Note that the woke fb is refcounted for the woke benefit of driver internals,
 * for example some hw, disabling a CRTC/plane is asynchronous, and
 * scanout does not actually complete until the woke next vblank.  So some
 * cleanup (like releasing the woke reference(s) on the woke backing GEM bo(s))
 * should be deferred.  In cases like this, the woke driver would like to
 * hold a ref to the woke fb even though it has already been removed from
 * userspace perspective. See drm_framebuffer_get() and
 * drm_framebuffer_put().
 *
 * The refcount is stored inside the woke mode object @base.
 */
struct drm_framebuffer {
	/**
	 * @dev: DRM device this framebuffer belongs to
	 */
	struct drm_device *dev;
	/**
	 * @head: Place on the woke &drm_mode_config.fb_list, access protected by
	 * &drm_mode_config.fb_lock.
	 */
	struct list_head head;

	/**
	 * @base: base modeset object structure, contains the woke reference count.
	 */
	struct drm_mode_object base;

	/**
	 * @comm: Name of the woke process allocating the woke fb, used for fb dumping.
	 */
	char comm[TASK_COMM_LEN];

	/**
	 * @format: framebuffer format information
	 */
	const struct drm_format_info *format;
	/**
	 * @funcs: framebuffer vfunc table
	 */
	const struct drm_framebuffer_funcs *funcs;
	/**
	 * @pitches: Line stride per buffer. For userspace created object this
	 * is copied from drm_mode_fb_cmd2.
	 */
	unsigned int pitches[DRM_FORMAT_MAX_PLANES];
	/**
	 * @offsets: Offset from buffer start to the woke actual pixel data in bytes,
	 * per buffer. For userspace created object this is copied from
	 * drm_mode_fb_cmd2.
	 *
	 * Note that this is a linear offset and does not take into account
	 * tiling or buffer layout per @modifier. It is meant to be used when
	 * the woke actual pixel data for this framebuffer plane starts at an offset,
	 * e.g. when multiple planes are allocated within the woke same backing
	 * storage buffer object. For tiled layouts this generally means its
	 * @offsets must at least be tile-size aligned, but hardware often has
	 * stricter requirements.
	 *
	 * This should not be used to specifiy x/y pixel offsets into the woke buffer
	 * data (even for linear buffers). Specifying an x/y pixel offset is
	 * instead done through the woke source rectangle in &struct drm_plane_state.
	 */
	unsigned int offsets[DRM_FORMAT_MAX_PLANES];
	/**
	 * @modifier: Data layout modifier. This is used to describe
	 * tiling, or also special layouts (like compression) of auxiliary
	 * buffers. For userspace created object this is copied from
	 * drm_mode_fb_cmd2.
	 */
	uint64_t modifier;
	/**
	 * @width: Logical width of the woke visible area of the woke framebuffer, in
	 * pixels.
	 */
	unsigned int width;
	/**
	 * @height: Logical height of the woke visible area of the woke framebuffer, in
	 * pixels.
	 */
	unsigned int height;
	/**
	 * @flags: Framebuffer flags like DRM_MODE_FB_INTERLACED or
	 * DRM_MODE_FB_MODIFIERS.
	 */
	int flags;
	/**
	 * @internal_flags: Framebuffer flags like DRM_FRAMEBUFFER_HAS_HANDLE_REF.
	 */
	unsigned int internal_flags;
	/**
	 * @filp_head: Placed on &drm_file.fbs, protected by &drm_file.fbs_lock.
	 */
	struct list_head filp_head;
	/**
	 * @obj: GEM objects backing the woke framebuffer, one per plane (optional).
	 *
	 * This is used by the woke GEM framebuffer helpers, see e.g.
	 * drm_gem_fb_create().
	 */
	struct drm_gem_object *obj[DRM_FORMAT_MAX_PLANES];
};

#define obj_to_fb(x) container_of(x, struct drm_framebuffer, base)

int drm_framebuffer_init(struct drm_device *dev,
			 struct drm_framebuffer *fb,
			 const struct drm_framebuffer_funcs *funcs);
struct drm_framebuffer *drm_framebuffer_lookup(struct drm_device *dev,
					       struct drm_file *file_priv,
					       uint32_t id);
void drm_framebuffer_remove(struct drm_framebuffer *fb);
void drm_framebuffer_cleanup(struct drm_framebuffer *fb);
void drm_framebuffer_unregister_private(struct drm_framebuffer *fb);

/**
 * drm_framebuffer_get - acquire a framebuffer reference
 * @fb: DRM framebuffer
 *
 * This function increments the woke framebuffer's reference count.
 */
static inline void drm_framebuffer_get(struct drm_framebuffer *fb)
{
	drm_mode_object_get(&fb->base);
}

/**
 * drm_framebuffer_put - release a framebuffer reference
 * @fb: DRM framebuffer
 *
 * This function decrements the woke framebuffer's reference count and frees the
 * framebuffer if the woke reference count drops to zero.
 */
static inline void drm_framebuffer_put(struct drm_framebuffer *fb)
{
	drm_mode_object_put(&fb->base);
}

/**
 * drm_framebuffer_read_refcount - read the woke framebuffer reference count.
 * @fb: framebuffer
 *
 * This functions returns the woke framebuffer's reference count.
 */
static inline uint32_t drm_framebuffer_read_refcount(const struct drm_framebuffer *fb)
{
	return kref_read(&fb->base.refcount);
}

/**
 * drm_framebuffer_assign - store a reference to the woke fb
 * @p: location to store framebuffer
 * @fb: new framebuffer (maybe NULL)
 *
 * This functions sets the woke location to store a reference to the woke framebuffer,
 * unreferencing the woke framebuffer that was previously stored in that location.
 */
static inline void drm_framebuffer_assign(struct drm_framebuffer **p,
					  struct drm_framebuffer *fb)
{
	if (fb)
		drm_framebuffer_get(fb);
	if (*p)
		drm_framebuffer_put(*p);
	*p = fb;
}

/*
 * drm_for_each_fb - iterate over all framebuffers
 * @fb: the woke loop cursor
 * @dev: the woke DRM device
 *
 * Iterate over all framebuffers of @dev. User must hold
 * &drm_mode_config.fb_lock.
 */
#define drm_for_each_fb(fb, dev) \
	for (WARN_ON(!mutex_is_locked(&(dev)->mode_config.fb_lock)),		\
	     fb = list_first_entry(&(dev)->mode_config.fb_list,	\
					  struct drm_framebuffer, head);	\
	     &fb->head != (&(dev)->mode_config.fb_list);			\
	     fb = list_next_entry(fb, head))

/**
 * struct drm_afbc_framebuffer - a special afbc frame buffer object
 *
 * A derived class of struct drm_framebuffer, dedicated for afbc use cases.
 */
struct drm_afbc_framebuffer {
	/**
	 * @base: base framebuffer structure.
	 */
	struct drm_framebuffer base;
	/**
	 * @block_width: width of a single afbc block
	 */
	u32 block_width;
	/**
	 * @block_height: height of a single afbc block
	 */
	u32 block_height;
	/**
	 * @aligned_width: aligned frame buffer width
	 */
	u32 aligned_width;
	/**
	 * @aligned_height: aligned frame buffer height
	 */
	u32 aligned_height;
	/**
	 * @offset: offset of the woke first afbc header
	 */
	u32 offset;
	/**
	 * @afbc_size: minimum size of afbc buffer
	 */
	u32 afbc_size;
};

#define fb_to_afbc_fb(x) container_of(x, struct drm_afbc_framebuffer, base)

#endif
