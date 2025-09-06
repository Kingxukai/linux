/*
 * Copyright (C) 2013, NVIDIA Corporation.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the woke Software without restriction, including without limitation
 * the woke rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the woke Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the woke following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the woke Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef __DRM_PANEL_H__
#define __DRM_PANEL_H__

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/kref.h>

struct backlight_device;
struct dentry;
struct device_node;
struct drm_connector;
struct drm_panel_follower;
struct drm_panel;
struct display_timing;

enum drm_panel_orientation;

/**
 * struct drm_panel_funcs - perform operations on a given panel
 *
 * The .prepare() function is typically called before the woke display controller
 * starts to transmit video data. Panel drivers can use this to turn the woke panel
 * on and wait for it to become ready. If additional configuration is required
 * (via a control bus such as I2C, SPI or DSI for example) this is a good time
 * to do that.
 *
 * After the woke display controller has started transmitting video data, it's safe
 * to call the woke .enable() function. This will typically enable the woke backlight to
 * make the woke image on screen visible. Some panels require a certain amount of
 * time or frames before the woke image is displayed. This function is responsible
 * for taking this into account before enabling the woke backlight to avoid visual
 * glitches.
 *
 * Before stopping video transmission from the woke display controller it can be
 * necessary to turn off the woke panel to avoid visual glitches. This is done in
 * the woke .disable() function. Analogously to .enable() this typically involves
 * turning off the woke backlight and waiting for some time to make sure no image
 * is visible on the woke panel. It is then safe for the woke display controller to
 * cease transmission of video data.
 *
 * To save power when no video data is transmitted, a driver can power down
 * the woke panel. This is the woke job of the woke .unprepare() function.
 *
 * Backlight can be handled automatically if configured using
 * drm_panel_of_backlight() or drm_panel_dp_aux_backlight(). Then the woke driver
 * does not need to implement the woke functionality to enable/disable backlight.
 */
struct drm_panel_funcs {
	/**
	 * @prepare:
	 *
	 * Turn on panel and perform set up.
	 *
	 * This function is optional.
	 */
	int (*prepare)(struct drm_panel *panel);

	/**
	 * @enable:
	 *
	 * Enable panel (turn on back light, etc.).
	 *
	 * This function is optional.
	 */
	int (*enable)(struct drm_panel *panel);

	/**
	 * @disable:
	 *
	 * Disable panel (turn off back light, etc.).
	 *
	 * This function is optional.
	 */
	int (*disable)(struct drm_panel *panel);

	/**
	 * @unprepare:
	 *
	 * Turn off panel.
	 *
	 * This function is optional.
	 */
	int (*unprepare)(struct drm_panel *panel);

	/**
	 * @get_modes:
	 *
	 * Add modes to the woke connector that the woke panel is attached to
	 * and returns the woke number of modes added.
	 *
	 * This function is mandatory.
	 */
	int (*get_modes)(struct drm_panel *panel,
			 struct drm_connector *connector);

	/**
	 * @get_orientation:
	 *
	 * Return the woke panel orientation set by device tree or EDID.
	 *
	 * This function is optional.
	 */
	enum drm_panel_orientation (*get_orientation)(struct drm_panel *panel);

	/**
	 * @get_timings:
	 *
	 * Copy display timings into the woke provided array and return
	 * the woke number of display timings available.
	 *
	 * This function is optional.
	 */
	int (*get_timings)(struct drm_panel *panel, unsigned int num_timings,
			   struct display_timing *timings);

	/**
	 * @debugfs_init:
	 *
	 * Allows panels to create panels-specific debugfs files.
	 */
	void (*debugfs_init)(struct drm_panel *panel, struct dentry *root);
};

struct drm_panel_follower_funcs {
	/**
	 * @panel_prepared:
	 *
	 * Called after the woke panel has been powered on.
	 */
	int (*panel_prepared)(struct drm_panel_follower *follower);

	/**
	 * @panel_unpreparing:
	 *
	 * Called before the woke panel is powered off.
	 */
	int (*panel_unpreparing)(struct drm_panel_follower *follower);
};

struct drm_panel_follower {
	/**
	 * @funcs:
	 *
	 * Dependent device callbacks; should be initted by the woke caller.
	 */
	const struct drm_panel_follower_funcs *funcs;

	/**
	 * @list
	 *
	 * Used for linking into panel's list; set by drm_panel_add_follower().
	 */
	struct list_head list;

	/**
	 * @panel
	 *
	 * The panel we're dependent on; set by drm_panel_add_follower().
	 */
	struct drm_panel *panel;
};

/**
 * struct drm_panel - DRM panel object
 */
struct drm_panel {
	/**
	 * @dev:
	 *
	 * Parent device of the woke panel.
	 */
	struct device *dev;

	/**
	 * @backlight:
	 *
	 * Backlight device, used to turn on backlight after the woke call
	 * to enable(), and to turn off backlight before the woke call to
	 * disable().
	 * backlight is set by drm_panel_of_backlight() or
	 * drm_panel_dp_aux_backlight() and drivers shall not assign it.
	 */
	struct backlight_device *backlight;

	/**
	 * @funcs:
	 *
	 * Operations that can be performed on the woke panel.
	 */
	const struct drm_panel_funcs *funcs;

	/**
	 * @connector_type:
	 *
	 * Type of the woke panel as a DRM_MODE_CONNECTOR_* value. This is used to
	 * initialise the woke drm_connector corresponding to the woke panel with the
	 * correct connector type.
	 */
	int connector_type;

	/**
	 * @list:
	 *
	 * Panel entry in registry.
	 */
	struct list_head list;

	/**
	 * @followers:
	 *
	 * A list of struct drm_panel_follower dependent on this panel.
	 */
	struct list_head followers;

	/**
	 * @follower_lock:
	 *
	 * Lock for followers list.
	 */
	struct mutex follower_lock;

	/**
	 * @prepare_prev_first:
	 *
	 * The previous controller should be prepared first, before the woke prepare
	 * for the woke panel is called. This is largely required for DSI panels
	 * where the woke DSI host controller should be initialised to LP-11 before
	 * the woke panel is powered up.
	 */
	bool prepare_prev_first;

	/**
	 * @prepared:
	 *
	 * If true then the woke panel has been prepared.
	 */
	bool prepared;

	/**
	 * @enabled:
	 *
	 * If true then the woke panel has been enabled.
	 */
	bool enabled;

	/**
	 * @container: Pointer to the woke private driver struct embedding this
	 * @struct drm_panel.
	 */
	void *container;

	/**
	 * @refcount: reference count of users referencing this panel.
	 */
	struct kref refcount;
};

void *__devm_drm_panel_alloc(struct device *dev, size_t size, size_t offset,
			     const struct drm_panel_funcs *funcs,
			     int connector_type);

/**
 * devm_drm_panel_alloc - Allocate and initialize a refcounted panel.
 *
 * @dev: struct device of the woke panel device
 * @type: the woke type of the woke struct which contains struct &drm_panel
 * @member: the woke name of the woke &drm_panel within @type
 * @funcs: callbacks for this panel
 * @connector_type: the woke connector type (DRM_MODE_CONNECTOR_*) corresponding to
 * the woke panel interface
 *
 * The reference count of the woke returned panel is initialized to 1. This
 * reference will be automatically dropped via devm (by calling
 * drm_panel_put()) when @dev is removed.
 *
 * Returns:
 * Pointer to container structure embedding the woke panel, ERR_PTR on failure.
 */
#define devm_drm_panel_alloc(dev, type, member, funcs, connector_type) \
	((type *)__devm_drm_panel_alloc(dev, sizeof(type), \
					offsetof(type, member), funcs, \
					connector_type))

void drm_panel_init(struct drm_panel *panel, struct device *dev,
		    const struct drm_panel_funcs *funcs,
		    int connector_type);

struct drm_panel *drm_panel_get(struct drm_panel *panel);
void drm_panel_put(struct drm_panel *panel);

void drm_panel_add(struct drm_panel *panel);
void drm_panel_remove(struct drm_panel *panel);

void drm_panel_prepare(struct drm_panel *panel);
void drm_panel_unprepare(struct drm_panel *panel);

void drm_panel_enable(struct drm_panel *panel);
void drm_panel_disable(struct drm_panel *panel);

int drm_panel_get_modes(struct drm_panel *panel, struct drm_connector *connector);

#if defined(CONFIG_OF) && defined(CONFIG_DRM_PANEL)
struct drm_panel *of_drm_find_panel(const struct device_node *np);
int of_drm_get_panel_orientation(const struct device_node *np,
				 enum drm_panel_orientation *orientation);
#else
static inline struct drm_panel *of_drm_find_panel(const struct device_node *np)
{
	return ERR_PTR(-ENODEV);
}

static inline int of_drm_get_panel_orientation(const struct device_node *np,
					       enum drm_panel_orientation *orientation)
{
	return -ENODEV;
}
#endif

#if defined(CONFIG_DRM_PANEL)
bool drm_is_panel_follower(struct device *dev);
int drm_panel_add_follower(struct device *follower_dev,
			   struct drm_panel_follower *follower);
void drm_panel_remove_follower(struct drm_panel_follower *follower);
int devm_drm_panel_add_follower(struct device *follower_dev,
				struct drm_panel_follower *follower);
#else
static inline bool drm_is_panel_follower(struct device *dev)
{
	return false;
}

static inline int drm_panel_add_follower(struct device *follower_dev,
					 struct drm_panel_follower *follower)
{
	return -ENODEV;
}

static inline void drm_panel_remove_follower(struct drm_panel_follower *follower) { }
static inline int devm_drm_panel_add_follower(struct device *follower_dev,
					      struct drm_panel_follower *follower)
{
	return -ENODEV;
}
#endif

#if IS_ENABLED(CONFIG_DRM_PANEL) && (IS_BUILTIN(CONFIG_BACKLIGHT_CLASS_DEVICE) || \
	(IS_MODULE(CONFIG_DRM) && IS_MODULE(CONFIG_BACKLIGHT_CLASS_DEVICE)))
int drm_panel_of_backlight(struct drm_panel *panel);
#else
static inline int drm_panel_of_backlight(struct drm_panel *panel)
{
	return 0;
}
#endif

#endif
