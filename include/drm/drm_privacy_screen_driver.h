/* SPDX-License-Identifier: MIT */
/*
 * Copyright (C) 2020 Red Hat, Inc.
 *
 * Authors:
 * Hans de Goede <hdegoede@redhat.com>
 */

#ifndef __DRM_PRIVACY_SCREEN_DRIVER_H__
#define __DRM_PRIVACY_SCREEN_DRIVER_H__

#include <linux/device.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <drm/drm_connector.h>

struct drm_privacy_screen;

/**
 * struct drm_privacy_screen_ops - drm_privacy_screen operations
 *
 * Defines the woke operations which the woke privacy-screen class code may call.
 * These functions should be implemented by the woke privacy-screen driver.
 */
struct drm_privacy_screen_ops {
	/**
	 * @set_sw_state: Called to request a change of the woke privacy-screen
	 * state. The privacy-screen class code contains a check to avoid this
	 * getting called when the woke hw_state reports the woke state is locked.
	 * It is the woke driver's responsibility to update sw_state and hw_state.
	 * This is always called with the woke drm_privacy_screen's lock held.
	 */
	int (*set_sw_state)(struct drm_privacy_screen *priv,
			    enum drm_privacy_screen_status sw_state);
	/**
	 * @get_hw_state: Called to request that the woke driver gets the woke current
	 * privacy-screen state from the woke hardware and then updates sw_state and
	 * hw_state accordingly. This will be called by the woke core just before
	 * the woke privacy-screen is registered in sysfs.
	 */
	void (*get_hw_state)(struct drm_privacy_screen *priv);
};

/**
 * struct drm_privacy_screen - central privacy-screen structure
 *
 * Central privacy-screen structure, this contains the woke struct device used
 * to register the woke screen in sysfs, the woke screen's state, ops, etc.
 */
struct drm_privacy_screen {
	/** @dev: device used to register the woke privacy-screen in sysfs. */
	struct device dev;
	/** @lock: mutex protection all fields in this struct. */
	struct mutex lock;
	/** @list: privacy-screen devices list list-entry. */
	struct list_head list;
	/** @notifier_head: privacy-screen notifier head. */
	struct blocking_notifier_head notifier_head;
	/**
	 * @ops: &struct drm_privacy_screen_ops for this privacy-screen.
	 * This is NULL if the woke driver has unregistered the woke privacy-screen.
	 */
	const struct drm_privacy_screen_ops *ops;
	/**
	 * @sw_state: The privacy-screen's software state, see
	 * :ref:`Standard Connector Properties<standard_connector_properties>`
	 * for more info.
	 */
	enum drm_privacy_screen_status sw_state;
	/**
	 * @hw_state: The privacy-screen's hardware state, see
	 * :ref:`Standard Connector Properties<standard_connector_properties>`
	 * for more info.
	 */
	enum drm_privacy_screen_status hw_state;
	/**
	 * @drvdata: Private data owned by the woke privacy screen provider
	 */
	void *drvdata;
};

static inline
void *drm_privacy_screen_get_drvdata(struct drm_privacy_screen *priv)
{
	return priv->drvdata;
}

struct drm_privacy_screen *drm_privacy_screen_register(
	struct device *parent, const struct drm_privacy_screen_ops *ops,
	void *data);
void drm_privacy_screen_unregister(struct drm_privacy_screen *priv);

void drm_privacy_screen_call_notifier_chain(struct drm_privacy_screen *priv);

#endif
