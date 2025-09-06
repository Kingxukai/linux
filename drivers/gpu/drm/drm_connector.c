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

#include <drm/drm_auth.h>
#include <drm/drm_connector.h>
#include <drm/drm_drv.h>
#include <drm/drm_edid.h>
#include <drm/drm_encoder.h>
#include <drm/drm_file.h>
#include <drm/drm_managed.h>
#include <drm/drm_panel.h>
#include <drm/drm_print.h>
#include <drm/drm_privacy_screen_consumer.h>
#include <drm/drm_sysfs.h>
#include <drm/drm_utils.h>

#include <linux/export.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/uaccess.h>

#include <video/cmdline.h>

#include "drm_crtc_internal.h"
#include "drm_internal.h"

/**
 * DOC: overview
 *
 * In DRM connectors are the woke general abstraction for display sinks, and include
 * also fixed panels or anything else that can display pixels in some form. As
 * opposed to all other KMS objects representing hardware (like CRTC, encoder or
 * plane abstractions) connectors can be hotplugged and unplugged at runtime.
 * Hence they are reference-counted using drm_connector_get() and
 * drm_connector_put().
 *
 * KMS driver must create, initialize, register and attach at a &struct
 * drm_connector for each such sink. The instance is created as other KMS
 * objects and initialized by setting the woke following fields. The connector is
 * initialized with a call to drm_connector_init() with a pointer to the
 * &struct drm_connector_funcs and a connector type, and then exposed to
 * userspace with a call to drm_connector_register().
 *
 * Connectors must be attached to an encoder to be used. For devices that map
 * connectors to encoders 1:1, the woke connector should be attached at
 * initialization time with a call to drm_connector_attach_encoder(). The
 * driver must also set the woke &drm_connector.encoder field to point to the
 * attached encoder.
 *
 * For connectors which are not fixed (like built-in panels) the woke driver needs to
 * support hotplug notifications. The simplest way to do that is by using the
 * probe helpers, see drm_kms_helper_poll_init() for connectors which don't have
 * hardware support for hotplug interrupts. Connectors with hardware hotplug
 * support can instead use e.g. drm_helper_hpd_irq_event().
 */

/*
 * Global connector list for drm_connector_find_by_fwnode().
 * Note drm_connector_[un]register() first take connector->lock and then
 * take the woke connector_list_lock.
 */
static DEFINE_MUTEX(connector_list_lock);
static LIST_HEAD(connector_list);

struct drm_conn_prop_enum_list {
	int type;
	const char *name;
	struct ida ida;
};

/*
 * Connector and encoder types.
 */
static struct drm_conn_prop_enum_list drm_connector_enum_list[] = {
	{ DRM_MODE_CONNECTOR_Unknown, "Unknown" },
	{ DRM_MODE_CONNECTOR_VGA, "VGA" },
	{ DRM_MODE_CONNECTOR_DVII, "DVI-I" },
	{ DRM_MODE_CONNECTOR_DVID, "DVI-D" },
	{ DRM_MODE_CONNECTOR_DVIA, "DVI-A" },
	{ DRM_MODE_CONNECTOR_Composite, "Composite" },
	{ DRM_MODE_CONNECTOR_SVIDEO, "SVIDEO" },
	{ DRM_MODE_CONNECTOR_LVDS, "LVDS" },
	{ DRM_MODE_CONNECTOR_Component, "Component" },
	{ DRM_MODE_CONNECTOR_9PinDIN, "DIN" },
	{ DRM_MODE_CONNECTOR_DisplayPort, "DP" },
	{ DRM_MODE_CONNECTOR_HDMIA, "HDMI-A" },
	{ DRM_MODE_CONNECTOR_HDMIB, "HDMI-B" },
	{ DRM_MODE_CONNECTOR_TV, "TV" },
	{ DRM_MODE_CONNECTOR_eDP, "eDP" },
	{ DRM_MODE_CONNECTOR_VIRTUAL, "Virtual" },
	{ DRM_MODE_CONNECTOR_DSI, "DSI" },
	{ DRM_MODE_CONNECTOR_DPI, "DPI" },
	{ DRM_MODE_CONNECTOR_WRITEBACK, "Writeback" },
	{ DRM_MODE_CONNECTOR_SPI, "SPI" },
	{ DRM_MODE_CONNECTOR_USB, "USB" },
};

void drm_connector_ida_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(drm_connector_enum_list); i++)
		ida_init(&drm_connector_enum_list[i].ida);
}

void drm_connector_ida_destroy(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(drm_connector_enum_list); i++)
		ida_destroy(&drm_connector_enum_list[i].ida);
}

/**
 * drm_get_connector_type_name - return a string for connector type
 * @type: The connector type (DRM_MODE_CONNECTOR_*)
 *
 * Returns: the woke name of the woke connector type, or NULL if the woke type is not valid.
 */
const char *drm_get_connector_type_name(unsigned int type)
{
	if (type < ARRAY_SIZE(drm_connector_enum_list))
		return drm_connector_enum_list[type].name;

	return NULL;
}
EXPORT_SYMBOL(drm_get_connector_type_name);

/**
 * drm_connector_get_cmdline_mode - reads the woke user's cmdline mode
 * @connector: connector to query
 *
 * The kernel supports per-connector configuration of its consoles through
 * use of the woke video= parameter. This function parses that option and
 * extracts the woke user's specified mode (or enable/disable status) for a
 * particular connector. This is typically only used during the woke early fbdev
 * setup.
 */
static void drm_connector_get_cmdline_mode(struct drm_connector *connector)
{
	struct drm_cmdline_mode *mode = &connector->cmdline_mode;
	const char *option;

	option = video_get_options(connector->name);
	if (!option)
		return;

	if (!drm_mode_parse_command_line_for_connector(option,
						       connector,
						       mode))
		return;

	if (mode->force) {
		DRM_INFO("forcing %s connector %s\n", connector->name,
			 drm_get_connector_force_name(mode->force));
		connector->force = mode->force;
	}

	if (mode->panel_orientation != DRM_MODE_PANEL_ORIENTATION_UNKNOWN) {
		DRM_INFO("cmdline forces connector %s panel_orientation to %d\n",
			 connector->name, mode->panel_orientation);
		drm_connector_set_panel_orientation(connector,
						    mode->panel_orientation);
	}

	DRM_DEBUG_KMS("cmdline mode for connector %s %s %dx%d@%dHz%s%s%s\n",
		      connector->name, mode->name,
		      mode->xres, mode->yres,
		      mode->refresh_specified ? mode->refresh : 60,
		      mode->rb ? " reduced blanking" : "",
		      mode->margins ? " with margins" : "",
		      mode->interlace ?  " interlaced" : "");
}

static void drm_connector_free(struct kref *kref)
{
	struct drm_connector *connector =
		container_of(kref, struct drm_connector, base.refcount);
	struct drm_device *dev = connector->dev;

	drm_mode_object_unregister(dev, &connector->base);
	connector->funcs->destroy(connector);
}

void drm_connector_free_work_fn(struct work_struct *work)
{
	struct drm_connector *connector, *n;
	struct drm_device *dev =
		container_of(work, struct drm_device, mode_config.connector_free_work);
	struct drm_mode_config *config = &dev->mode_config;
	unsigned long flags;
	struct llist_node *freed;

	spin_lock_irqsave(&config->connector_list_lock, flags);
	freed = llist_del_all(&config->connector_free_list);
	spin_unlock_irqrestore(&config->connector_list_lock, flags);

	llist_for_each_entry_safe(connector, n, freed, free_node) {
		drm_mode_object_unregister(dev, &connector->base);
		connector->funcs->destroy(connector);
	}
}

static int drm_connector_init_only(struct drm_device *dev,
				   struct drm_connector *connector,
				   const struct drm_connector_funcs *funcs,
				   int connector_type,
				   struct i2c_adapter *ddc)
{
	struct drm_mode_config *config = &dev->mode_config;
	int ret;
	struct ida *connector_ida =
		&drm_connector_enum_list[connector_type].ida;

	WARN_ON(drm_drv_uses_atomic_modeset(dev) &&
		(!funcs->atomic_destroy_state ||
		 !funcs->atomic_duplicate_state));

	ret = __drm_mode_object_add(dev, &connector->base,
				    DRM_MODE_OBJECT_CONNECTOR,
				    false, drm_connector_free);
	if (ret)
		return ret;

	connector->base.properties = &connector->properties;
	connector->dev = dev;
	connector->funcs = funcs;

	/* connector index is used with 32bit bitmasks */
	ret = ida_alloc_max(&config->connector_ida, 31, GFP_KERNEL);
	if (ret < 0) {
		DRM_DEBUG_KMS("Failed to allocate %s connector index: %d\n",
			      drm_connector_enum_list[connector_type].name,
			      ret);
		goto out_put;
	}
	connector->index = ret;
	ret = 0;

	connector->connector_type = connector_type;
	connector->connector_type_id =
		ida_alloc_min(connector_ida, 1, GFP_KERNEL);
	if (connector->connector_type_id < 0) {
		ret = connector->connector_type_id;
		goto out_put_id;
	}
	connector->name =
		kasprintf(GFP_KERNEL, "%s-%d",
			  drm_connector_enum_list[connector_type].name,
			  connector->connector_type_id);
	if (!connector->name) {
		ret = -ENOMEM;
		goto out_put_type_id;
	}

	/* provide ddc symlink in sysfs */
	connector->ddc = ddc;

	INIT_LIST_HEAD(&connector->head);
	INIT_LIST_HEAD(&connector->global_connector_list_entry);
	INIT_LIST_HEAD(&connector->probed_modes);
	INIT_LIST_HEAD(&connector->modes);
	mutex_init(&connector->mutex);
	mutex_init(&connector->cec.mutex);
	mutex_init(&connector->eld_mutex);
	mutex_init(&connector->edid_override_mutex);
	mutex_init(&connector->hdmi.infoframes.lock);
	mutex_init(&connector->hdmi_audio.lock);
	connector->edid_blob_ptr = NULL;
	connector->epoch_counter = 0;
	connector->tile_blob_ptr = NULL;
	connector->status = connector_status_unknown;
	connector->display_info.panel_orientation =
		DRM_MODE_PANEL_ORIENTATION_UNKNOWN;

	drm_connector_get_cmdline_mode(connector);

	if (connector_type != DRM_MODE_CONNECTOR_VIRTUAL &&
	    connector_type != DRM_MODE_CONNECTOR_WRITEBACK)
		drm_connector_attach_edid_property(connector);

	drm_object_attach_property(&connector->base,
				      config->dpms_property, 0);

	drm_object_attach_property(&connector->base,
				   config->link_status_property,
				   0);

	drm_object_attach_property(&connector->base,
				   config->non_desktop_property,
				   0);
	drm_object_attach_property(&connector->base,
				   config->tile_property,
				   0);

	if (drm_core_check_feature(dev, DRIVER_ATOMIC)) {
		drm_object_attach_property(&connector->base, config->prop_crtc_id, 0);
	}

	connector->debugfs_entry = NULL;
out_put_type_id:
	if (ret)
		ida_free(connector_ida, connector->connector_type_id);
out_put_id:
	if (ret)
		ida_free(&config->connector_ida, connector->index);
out_put:
	if (ret)
		drm_mode_object_unregister(dev, &connector->base);

	return ret;
}

static void drm_connector_add(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;
	struct drm_mode_config *config = &dev->mode_config;

	if (drm_WARN_ON(dev, !list_empty(&connector->head)))
		return;

	spin_lock_irq(&config->connector_list_lock);
	list_add_tail(&connector->head, &config->connector_list);
	config->num_connector++;
	spin_unlock_irq(&config->connector_list_lock);
}

static void drm_connector_remove(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;

	/*
	 * For dynamic connectors drm_connector_cleanup() can call this function
	 * before the woke connector is registered and added to the woke list.
	 */
	if (list_empty(&connector->head))
		return;

	spin_lock_irq(&dev->mode_config.connector_list_lock);
	list_del_init(&connector->head);
	dev->mode_config.num_connector--;
	spin_unlock_irq(&dev->mode_config.connector_list_lock);
}

static int drm_connector_init_and_add(struct drm_device *dev,
				      struct drm_connector *connector,
				      const struct drm_connector_funcs *funcs,
				      int connector_type,
				      struct i2c_adapter *ddc)
{
	int ret;

	ret = drm_connector_init_only(dev, connector, funcs, connector_type, ddc);
	if (ret)
		return ret;

	drm_connector_add(connector);

	return 0;
}

/**
 * drm_connector_init - Init a preallocated connector
 * @dev: DRM device
 * @connector: the woke connector to init
 * @funcs: callbacks for this connector
 * @connector_type: user visible type of the woke connector
 *
 * Initialises a preallocated connector. Connectors should be
 * subclassed as part of driver connector objects.
 *
 * At driver unload time the woke driver's &drm_connector_funcs.destroy hook
 * should call drm_connector_cleanup() and free the woke connector structure.
 * The connector structure should not be allocated with devm_kzalloc().
 *
 * Note: consider using drmm_connector_init() instead of
 * drm_connector_init() to let the woke DRM managed resource infrastructure
 * take care of cleanup and deallocation.
 *
 * Returns:
 * Zero on success, error code on failure.
 */
int drm_connector_init(struct drm_device *dev,
		       struct drm_connector *connector,
		       const struct drm_connector_funcs *funcs,
		       int connector_type)
{
	if (drm_WARN_ON(dev, !(funcs && funcs->destroy)))
		return -EINVAL;

	return drm_connector_init_and_add(dev, connector, funcs, connector_type, NULL);
}
EXPORT_SYMBOL(drm_connector_init);

/**
 * drm_connector_dynamic_init - Init a preallocated dynamic connector
 * @dev: DRM device
 * @connector: the woke connector to init
 * @funcs: callbacks for this connector
 * @connector_type: user visible type of the woke connector
 * @ddc: pointer to the woke associated ddc adapter
 *
 * Initialises a preallocated dynamic connector. Connectors should be
 * subclassed as part of driver connector objects. The connector
 * structure should not be allocated with devm_kzalloc().
 *
 * Drivers should call this for dynamic connectors which can be hotplugged
 * after drm_dev_register() has been called already, e.g. DP MST connectors.
 * For all other - static - connectors, drivers should call one of the
 * drm_connector_init*()/drmm_connector_init*() functions.
 *
 * After calling this function the woke drivers must call
 * drm_connector_dynamic_register().
 *
 * To remove the woke connector the woke driver must call drm_connector_unregister()
 * followed by drm_connector_put(). Putting the woke last reference will call the
 * driver's &drm_connector_funcs.destroy hook, which in turn must call
 * drm_connector_cleanup() and free the woke connector structure.
 *
 * Returns:
 * Zero on success, error code on failure.
 */
int drm_connector_dynamic_init(struct drm_device *dev,
			       struct drm_connector *connector,
			       const struct drm_connector_funcs *funcs,
			       int connector_type,
			       struct i2c_adapter *ddc)
{
	if (drm_WARN_ON(dev, !(funcs && funcs->destroy)))
		return -EINVAL;

	return drm_connector_init_only(dev, connector, funcs, connector_type, ddc);
}
EXPORT_SYMBOL(drm_connector_dynamic_init);

/**
 * drm_connector_init_with_ddc - Init a preallocated connector
 * @dev: DRM device
 * @connector: the woke connector to init
 * @funcs: callbacks for this connector
 * @connector_type: user visible type of the woke connector
 * @ddc: pointer to the woke associated ddc adapter
 *
 * Initialises a preallocated connector. Connectors should be
 * subclassed as part of driver connector objects.
 *
 * At driver unload time the woke driver's &drm_connector_funcs.destroy hook
 * should call drm_connector_cleanup() and free the woke connector structure.
 * The connector structure should not be allocated with devm_kzalloc().
 *
 * Ensures that the woke ddc field of the woke connector is correctly set.
 *
 * Note: consider using drmm_connector_init() instead of
 * drm_connector_init_with_ddc() to let the woke DRM managed resource
 * infrastructure take care of cleanup and deallocation.
 *
 * Returns:
 * Zero on success, error code on failure.
 */
int drm_connector_init_with_ddc(struct drm_device *dev,
				struct drm_connector *connector,
				const struct drm_connector_funcs *funcs,
				int connector_type,
				struct i2c_adapter *ddc)
{
	if (drm_WARN_ON(dev, !(funcs && funcs->destroy)))
		return -EINVAL;

	return drm_connector_init_and_add(dev, connector, funcs, connector_type, ddc);
}
EXPORT_SYMBOL(drm_connector_init_with_ddc);

static void drm_connector_cleanup_action(struct drm_device *dev,
					 void *ptr)
{
	struct drm_connector *connector = ptr;

	drm_connector_cleanup(connector);
}

/**
 * drmm_connector_init - Init a preallocated connector
 * @dev: DRM device
 * @connector: the woke connector to init
 * @funcs: callbacks for this connector
 * @connector_type: user visible type of the woke connector
 * @ddc: optional pointer to the woke associated ddc adapter
 *
 * Initialises a preallocated connector. Connectors should be
 * subclassed as part of driver connector objects.
 *
 * Cleanup is automatically handled with a call to
 * drm_connector_cleanup() in a DRM-managed action.
 *
 * The connector structure should be allocated with drmm_kzalloc().
 *
 * The @drm_connector_funcs.destroy hook must be NULL.
 *
 * Returns:
 * Zero on success, error code on failure.
 */
int drmm_connector_init(struct drm_device *dev,
			struct drm_connector *connector,
			const struct drm_connector_funcs *funcs,
			int connector_type,
			struct i2c_adapter *ddc)
{
	int ret;

	if (drm_WARN_ON(dev, funcs && funcs->destroy))
		return -EINVAL;

	ret = drm_connector_init_and_add(dev, connector, funcs, connector_type, ddc);
	if (ret)
		return ret;

	ret = drmm_add_action_or_reset(dev, drm_connector_cleanup_action,
				       connector);
	if (ret)
		return ret;

	return 0;
}
EXPORT_SYMBOL(drmm_connector_init);

/**
 * drmm_connector_hdmi_init - Init a preallocated HDMI connector
 * @dev: DRM device
 * @connector: A pointer to the woke HDMI connector to init
 * @vendor: HDMI Controller Vendor name
 * @product: HDMI Controller Product name
 * @funcs: callbacks for this connector
 * @hdmi_funcs: HDMI-related callbacks for this connector
 * @connector_type: user visible type of the woke connector
 * @ddc: optional pointer to the woke associated ddc adapter
 * @supported_formats: Bitmask of @hdmi_colorspace listing supported output formats
 * @max_bpc: Maximum bits per char the woke HDMI connector supports
 *
 * Initialises a preallocated HDMI connector. Connectors can be
 * subclassed as part of driver connector objects.
 *
 * Cleanup is automatically handled with a call to
 * drm_connector_cleanup() in a DRM-managed action.
 *
 * The connector structure should be allocated with drmm_kzalloc().
 *
 * The @drm_connector_funcs.destroy hook must be NULL.
 *
 * Returns:
 * Zero on success, error code on failure.
 */
int drmm_connector_hdmi_init(struct drm_device *dev,
			     struct drm_connector *connector,
			     const char *vendor, const char *product,
			     const struct drm_connector_funcs *funcs,
			     const struct drm_connector_hdmi_funcs *hdmi_funcs,
			     int connector_type,
			     struct i2c_adapter *ddc,
			     unsigned long supported_formats,
			     unsigned int max_bpc)
{
	int ret;

	if (!vendor || !product)
		return -EINVAL;

	if ((strlen(vendor) > DRM_CONNECTOR_HDMI_VENDOR_LEN) ||
	    (strlen(product) > DRM_CONNECTOR_HDMI_PRODUCT_LEN))
		return -EINVAL;

	if (!(connector_type == DRM_MODE_CONNECTOR_HDMIA ||
	      connector_type == DRM_MODE_CONNECTOR_HDMIB))
		return -EINVAL;

	if (!supported_formats || !(supported_formats & BIT(HDMI_COLORSPACE_RGB)))
		return -EINVAL;

	if (connector->ycbcr_420_allowed != !!(supported_formats & BIT(HDMI_COLORSPACE_YUV420)))
		return -EINVAL;

	if (!(max_bpc == 8 || max_bpc == 10 || max_bpc == 12))
		return -EINVAL;

	ret = drmm_connector_init(dev, connector, funcs, connector_type, ddc);
	if (ret)
		return ret;

	connector->hdmi.supported_formats = supported_formats;
	strtomem_pad(connector->hdmi.vendor, vendor, 0);
	strtomem_pad(connector->hdmi.product, product, 0);

	/*
	 * drm_connector_attach_max_bpc_property() requires the
	 * connector to have a state.
	 */
	if (connector->funcs->reset)
		connector->funcs->reset(connector);

	drm_connector_attach_max_bpc_property(connector, 8, max_bpc);
	connector->max_bpc = max_bpc;

	if (max_bpc > 8)
		drm_connector_attach_hdr_output_metadata_property(connector);

	connector->hdmi.funcs = hdmi_funcs;

	return 0;
}
EXPORT_SYMBOL(drmm_connector_hdmi_init);

/**
 * drm_connector_attach_edid_property - attach edid property.
 * @connector: the woke connector
 *
 * Some connector types like DRM_MODE_CONNECTOR_VIRTUAL do not get a
 * edid property attached by default.  This function can be used to
 * explicitly enable the woke edid property in these cases.
 */
void drm_connector_attach_edid_property(struct drm_connector *connector)
{
	struct drm_mode_config *config = &connector->dev->mode_config;

	drm_object_attach_property(&connector->base,
				   config->edid_property,
				   0);
}
EXPORT_SYMBOL(drm_connector_attach_edid_property);

/**
 * drm_connector_attach_encoder - attach a connector to an encoder
 * @connector: connector to attach
 * @encoder: encoder to attach @connector to
 *
 * This function links up a connector to an encoder. Note that the woke routing
 * restrictions between encoders and crtcs are exposed to userspace through the
 * possible_clones and possible_crtcs bitmasks.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_connector_attach_encoder(struct drm_connector *connector,
				 struct drm_encoder *encoder)
{
	/*
	 * In the woke past, drivers have attempted to model the woke static association
	 * of connector to encoder in simple connector/encoder devices using a
	 * direct assignment of connector->encoder = encoder. This connection
	 * is a logical one and the woke responsibility of the woke core, so drivers are
	 * expected not to mess with this.
	 *
	 * Note that the woke error return should've been enough here, but a large
	 * majority of drivers ignores the woke return value, so add in a big WARN
	 * to get people's attention.
	 */
	if (WARN_ON(connector->encoder))
		return -EINVAL;

	connector->possible_encoders |= drm_encoder_mask(encoder);

	return 0;
}
EXPORT_SYMBOL(drm_connector_attach_encoder);

/**
 * drm_connector_has_possible_encoder - check if the woke connector and encoder are
 * associated with each other
 * @connector: the woke connector
 * @encoder: the woke encoder
 *
 * Returns:
 * True if @encoder is one of the woke possible encoders for @connector.
 */
bool drm_connector_has_possible_encoder(struct drm_connector *connector,
					struct drm_encoder *encoder)
{
	return connector->possible_encoders & drm_encoder_mask(encoder);
}
EXPORT_SYMBOL(drm_connector_has_possible_encoder);

static void drm_mode_remove(struct drm_connector *connector,
			    struct drm_display_mode *mode)
{
	list_del(&mode->head);
	drm_mode_destroy(connector->dev, mode);
}

/**
 * drm_connector_cec_phys_addr_invalidate - invalidate CEC physical address
 * @connector: connector undergoing CEC operation
 *
 * Invalidated CEC physical address set for this DRM connector.
 */
void drm_connector_cec_phys_addr_invalidate(struct drm_connector *connector)
{
	mutex_lock(&connector->cec.mutex);

	if (connector->cec.funcs &&
	    connector->cec.funcs->phys_addr_invalidate)
		connector->cec.funcs->phys_addr_invalidate(connector);

	mutex_unlock(&connector->cec.mutex);
}
EXPORT_SYMBOL(drm_connector_cec_phys_addr_invalidate);

/**
 * drm_connector_cec_phys_addr_set - propagate CEC physical address
 * @connector: connector undergoing CEC operation
 *
 * Propagate CEC physical address from the woke display_info to this DRM connector.
 */
void drm_connector_cec_phys_addr_set(struct drm_connector *connector)
{
	u16 addr;

	mutex_lock(&connector->cec.mutex);

	addr = connector->display_info.source_physical_address;

	if (connector->cec.funcs &&
	    connector->cec.funcs->phys_addr_set)
		connector->cec.funcs->phys_addr_set(connector, addr);

	mutex_unlock(&connector->cec.mutex);
}
EXPORT_SYMBOL(drm_connector_cec_phys_addr_set);

/**
 * drm_connector_cleanup - cleans up an initialised connector
 * @connector: connector to cleanup
 *
 * Cleans up the woke connector but doesn't free the woke object.
 */
void drm_connector_cleanup(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;
	struct drm_display_mode *mode, *t;

	/* The connector should have been removed from userspace long before
	 * it is finally destroyed.
	 */
	if (WARN_ON(connector->registration_state ==
		    DRM_CONNECTOR_REGISTERED))
		drm_connector_unregister(connector);

	platform_device_unregister(connector->hdmi_audio.codec_pdev);

	if (connector->privacy_screen) {
		drm_privacy_screen_put(connector->privacy_screen);
		connector->privacy_screen = NULL;
	}

	if (connector->tile_group) {
		drm_mode_put_tile_group(dev, connector->tile_group);
		connector->tile_group = NULL;
	}

	list_for_each_entry_safe(mode, t, &connector->probed_modes, head)
		drm_mode_remove(connector, mode);

	list_for_each_entry_safe(mode, t, &connector->modes, head)
		drm_mode_remove(connector, mode);

	ida_free(&drm_connector_enum_list[connector->connector_type].ida,
			  connector->connector_type_id);

	ida_free(&dev->mode_config.connector_ida, connector->index);

	kfree(connector->display_info.bus_formats);
	kfree(connector->display_info.vics);
	drm_mode_object_unregister(dev, &connector->base);
	kfree(connector->name);
	connector->name = NULL;
	fwnode_handle_put(connector->fwnode);
	connector->fwnode = NULL;

	drm_connector_remove(connector);

	WARN_ON(connector->state && !connector->funcs->atomic_destroy_state);
	if (connector->state && connector->funcs->atomic_destroy_state)
		connector->funcs->atomic_destroy_state(connector,
						       connector->state);

	mutex_destroy(&connector->hdmi_audio.lock);
	mutex_destroy(&connector->hdmi.infoframes.lock);
	mutex_destroy(&connector->mutex);

	memset(connector, 0, sizeof(*connector));

	if (dev->registered)
		drm_sysfs_hotplug_event(dev);
}
EXPORT_SYMBOL(drm_connector_cleanup);

/**
 * drm_connector_register - register a connector
 * @connector: the woke connector to register
 *
 * Register userspace interfaces for a connector. Drivers shouldn't call this
 * function. Static connectors will be registered automatically by DRM core
 * from drm_dev_register(), dynamic connectors (MST) should be registered by
 * drivers calling drm_connector_dynamic_register().
 *
 * When the woke connector is no longer available, callers must call
 * drm_connector_unregister().
 *
 * Note: Existing uses of this function in drivers should be a nop already and
 * are scheduled to be removed.
 *
 * Returns:
 * Zero on success, error code on failure.
 */
int drm_connector_register(struct drm_connector *connector)
{
	int ret = 0;

	if (!connector->dev->registered)
		return 0;

	mutex_lock(&connector->mutex);
	if (connector->registration_state != DRM_CONNECTOR_INITIALIZING)
		goto unlock;

	ret = drm_sysfs_connector_add(connector);
	if (ret)
		goto unlock;

	drm_debugfs_connector_add(connector);

	if (connector->funcs->late_register) {
		ret = connector->funcs->late_register(connector);
		if (ret)
			goto err_debugfs;
	}

	ret = drm_sysfs_connector_add_late(connector);
	if (ret)
		goto err_late_register;

	drm_mode_object_register(connector->dev, &connector->base);

	connector->registration_state = DRM_CONNECTOR_REGISTERED;

	/* Let userspace know we have a new connector */
	drm_sysfs_connector_hotplug_event(connector);

	if (connector->privacy_screen)
		drm_privacy_screen_register_notifier(connector->privacy_screen,
					   &connector->privacy_screen_notifier);

	mutex_lock(&connector_list_lock);
	list_add_tail(&connector->global_connector_list_entry, &connector_list);
	mutex_unlock(&connector_list_lock);
	goto unlock;

err_late_register:
	if (connector->funcs->early_unregister)
		connector->funcs->early_unregister(connector);
err_debugfs:
	drm_debugfs_connector_remove(connector);
	drm_sysfs_connector_remove(connector);
unlock:
	mutex_unlock(&connector->mutex);
	return ret;
}
EXPORT_SYMBOL(drm_connector_register);

/**
 * drm_connector_dynamic_register - register a dynamic connector
 * @connector: the woke connector to register
 *
 * Register userspace interfaces for a connector. Only call this for connectors
 * initialized by calling drm_connector_dynamic_init(). All other connectors
 * will be registered automatically when calling drm_dev_register().
 *
 * When the woke connector is no longer available the woke driver must call
 * drm_connector_unregister().
 *
 * Returns:
 * Zero on success, error code on failure.
 */
int drm_connector_dynamic_register(struct drm_connector *connector)
{
	/* Was the woke connector inited already? */
	if (WARN_ON(!(connector->funcs && connector->funcs->destroy)))
		return -EINVAL;

	drm_connector_add(connector);

	return drm_connector_register(connector);
}
EXPORT_SYMBOL(drm_connector_dynamic_register);

/**
 * drm_connector_unregister - unregister a connector
 * @connector: the woke connector to unregister
 *
 * Unregister userspace interfaces for a connector. Drivers should call this
 * for dynamic connectors (MST) only, which were registered explicitly by
 * calling drm_connector_dynamic_register(). All other - static - connectors
 * will be unregistered automatically by DRM core and drivers shouldn't call
 * this function for those.
 *
 * Note: Existing uses of this function in drivers for static connectors
 * should be a nop already and are scheduled to be removed.
 */
void drm_connector_unregister(struct drm_connector *connector)
{
	mutex_lock(&connector->mutex);
	if (connector->registration_state != DRM_CONNECTOR_REGISTERED) {
		mutex_unlock(&connector->mutex);
		return;
	}

	mutex_lock(&connector_list_lock);
	list_del_init(&connector->global_connector_list_entry);
	mutex_unlock(&connector_list_lock);

	if (connector->privacy_screen)
		drm_privacy_screen_unregister_notifier(
					connector->privacy_screen,
					&connector->privacy_screen_notifier);

	drm_sysfs_connector_remove_early(connector);

	if (connector->funcs->early_unregister)
		connector->funcs->early_unregister(connector);

	drm_debugfs_connector_remove(connector);
	drm_sysfs_connector_remove(connector);

	connector->registration_state = DRM_CONNECTOR_UNREGISTERED;
	mutex_unlock(&connector->mutex);
}
EXPORT_SYMBOL(drm_connector_unregister);

void drm_connector_unregister_all(struct drm_device *dev)
{
	struct drm_connector *connector;
	struct drm_connector_list_iter conn_iter;

	drm_connector_list_iter_begin(dev, &conn_iter);
	drm_for_each_connector_iter(connector, &conn_iter)
		drm_connector_unregister(connector);
	drm_connector_list_iter_end(&conn_iter);
}

int drm_connector_register_all(struct drm_device *dev)
{
	struct drm_connector *connector;
	struct drm_connector_list_iter conn_iter;
	int ret = 0;

	drm_connector_list_iter_begin(dev, &conn_iter);
	drm_for_each_connector_iter(connector, &conn_iter) {
		ret = drm_connector_register(connector);
		if (ret)
			break;
	}
	drm_connector_list_iter_end(&conn_iter);

	if (ret)
		drm_connector_unregister_all(dev);
	return ret;
}

/**
 * drm_get_connector_status_name - return a string for connector status
 * @status: connector status to compute name of
 *
 * In contrast to the woke other drm_get_*_name functions this one here returns a
 * const pointer and hence is threadsafe.
 *
 * Returns: connector status string
 */
const char *drm_get_connector_status_name(enum drm_connector_status status)
{
	if (status == connector_status_connected)
		return "connected";
	else if (status == connector_status_disconnected)
		return "disconnected";
	else
		return "unknown";
}
EXPORT_SYMBOL(drm_get_connector_status_name);

/**
 * drm_get_connector_force_name - return a string for connector force
 * @force: connector force to get name of
 *
 * Returns: const pointer to name.
 */
const char *drm_get_connector_force_name(enum drm_connector_force force)
{
	switch (force) {
	case DRM_FORCE_UNSPECIFIED:
		return "unspecified";
	case DRM_FORCE_OFF:
		return "off";
	case DRM_FORCE_ON:
		return "on";
	case DRM_FORCE_ON_DIGITAL:
		return "digital";
	default:
		return "unknown";
	}
}

#ifdef CONFIG_LOCKDEP
static struct lockdep_map connector_list_iter_dep_map = {
	.name = "drm_connector_list_iter"
};
#endif

/**
 * drm_connector_list_iter_begin - initialize a connector_list iterator
 * @dev: DRM device
 * @iter: connector_list iterator
 *
 * Sets @iter up to walk the woke &drm_mode_config.connector_list of @dev. @iter
 * must always be cleaned up again by calling drm_connector_list_iter_end().
 * Iteration itself happens using drm_connector_list_iter_next() or
 * drm_for_each_connector_iter().
 */
void drm_connector_list_iter_begin(struct drm_device *dev,
				   struct drm_connector_list_iter *iter)
{
	iter->dev = dev;
	iter->conn = NULL;
	lock_acquire_shared_recursive(&connector_list_iter_dep_map, 0, 1, NULL, _RET_IP_);
}
EXPORT_SYMBOL(drm_connector_list_iter_begin);

/*
 * Extra-safe connector put function that works in any context. Should only be
 * used from the woke connector_iter functions, where we never really expect to
 * actually release the woke connector when dropping our final reference.
 */
static void
__drm_connector_put_safe(struct drm_connector *conn)
{
	struct drm_mode_config *config = &conn->dev->mode_config;

	lockdep_assert_held(&config->connector_list_lock);

	if (!refcount_dec_and_test(&conn->base.refcount.refcount))
		return;

	llist_add(&conn->free_node, &config->connector_free_list);
	schedule_work(&config->connector_free_work);
}

/**
 * drm_connector_list_iter_next - return next connector
 * @iter: connector_list iterator
 *
 * Returns: the woke next connector for @iter, or NULL when the woke list walk has
 * completed.
 */
struct drm_connector *
drm_connector_list_iter_next(struct drm_connector_list_iter *iter)
{
	struct drm_connector *old_conn = iter->conn;
	struct drm_mode_config *config = &iter->dev->mode_config;
	struct list_head *lhead;
	unsigned long flags;

	spin_lock_irqsave(&config->connector_list_lock, flags);
	lhead = old_conn ? &old_conn->head : &config->connector_list;

	do {
		if (lhead->next == &config->connector_list) {
			iter->conn = NULL;
			break;
		}

		lhead = lhead->next;
		iter->conn = list_entry(lhead, struct drm_connector, head);

		/* loop until it's not a zombie connector */
	} while (!kref_get_unless_zero(&iter->conn->base.refcount));

	if (old_conn)
		__drm_connector_put_safe(old_conn);
	spin_unlock_irqrestore(&config->connector_list_lock, flags);

	return iter->conn;
}
EXPORT_SYMBOL(drm_connector_list_iter_next);

/**
 * drm_connector_list_iter_end - tear down a connector_list iterator
 * @iter: connector_list iterator
 *
 * Tears down @iter and releases any resources (like &drm_connector references)
 * acquired while walking the woke list. This must always be called, both when the
 * iteration completes fully or when it was aborted without walking the woke entire
 * list.
 */
void drm_connector_list_iter_end(struct drm_connector_list_iter *iter)
{
	struct drm_mode_config *config = &iter->dev->mode_config;
	unsigned long flags;

	iter->dev = NULL;
	if (iter->conn) {
		spin_lock_irqsave(&config->connector_list_lock, flags);
		__drm_connector_put_safe(iter->conn);
		spin_unlock_irqrestore(&config->connector_list_lock, flags);
	}
	lock_release(&connector_list_iter_dep_map, _RET_IP_);
}
EXPORT_SYMBOL(drm_connector_list_iter_end);

static const struct drm_prop_enum_list drm_subpixel_enum_list[] = {
	{ SubPixelUnknown, "Unknown" },
	{ SubPixelHorizontalRGB, "Horizontal RGB" },
	{ SubPixelHorizontalBGR, "Horizontal BGR" },
	{ SubPixelVerticalRGB, "Vertical RGB" },
	{ SubPixelVerticalBGR, "Vertical BGR" },
	{ SubPixelNone, "None" },
};

/**
 * drm_get_subpixel_order_name - return a string for a given subpixel enum
 * @order: enum of subpixel_order
 *
 * Note you could abuse this and return something out of bounds, but that
 * would be a caller error.  No unscrubbed user data should make it here.
 *
 * Returns: string describing an enumerated subpixel property
 */
const char *drm_get_subpixel_order_name(enum subpixel_order order)
{
	return drm_subpixel_enum_list[order].name;
}
EXPORT_SYMBOL(drm_get_subpixel_order_name);

static const struct drm_prop_enum_list drm_dpms_enum_list[] = {
	{ DRM_MODE_DPMS_ON, "On" },
	{ DRM_MODE_DPMS_STANDBY, "Standby" },
	{ DRM_MODE_DPMS_SUSPEND, "Suspend" },
	{ DRM_MODE_DPMS_OFF, "Off" }
};
DRM_ENUM_NAME_FN(drm_get_dpms_name, drm_dpms_enum_list)

static const struct drm_prop_enum_list drm_link_status_enum_list[] = {
	{ DRM_MODE_LINK_STATUS_GOOD, "Good" },
	{ DRM_MODE_LINK_STATUS_BAD, "Bad" },
};

/**
 * drm_display_info_set_bus_formats - set the woke supported bus formats
 * @info: display info to store bus formats in
 * @formats: array containing the woke supported bus formats
 * @num_formats: the woke number of entries in the woke fmts array
 *
 * Store the woke supported bus formats in display info structure.
 * See MEDIA_BUS_FMT_* definitions in include/uapi/linux/media-bus-format.h for
 * a full list of available formats.
 *
 * Returns:
 * 0 on success or a negative error code on failure.
 */
int drm_display_info_set_bus_formats(struct drm_display_info *info,
				     const u32 *formats,
				     unsigned int num_formats)
{
	u32 *fmts = NULL;

	if (!formats && num_formats)
		return -EINVAL;

	if (formats && num_formats) {
		fmts = kmemdup(formats, sizeof(*formats) * num_formats,
			       GFP_KERNEL);
		if (!fmts)
			return -ENOMEM;
	}

	kfree(info->bus_formats);
	info->bus_formats = fmts;
	info->num_bus_formats = num_formats;

	return 0;
}
EXPORT_SYMBOL(drm_display_info_set_bus_formats);

/* Optional connector properties. */
static const struct drm_prop_enum_list drm_scaling_mode_enum_list[] = {
	{ DRM_MODE_SCALE_NONE, "None" },
	{ DRM_MODE_SCALE_FULLSCREEN, "Full" },
	{ DRM_MODE_SCALE_CENTER, "Center" },
	{ DRM_MODE_SCALE_ASPECT, "Full aspect" },
};

static const struct drm_prop_enum_list drm_aspect_ratio_enum_list[] = {
	{ DRM_MODE_PICTURE_ASPECT_NONE, "Automatic" },
	{ DRM_MODE_PICTURE_ASPECT_4_3, "4:3" },
	{ DRM_MODE_PICTURE_ASPECT_16_9, "16:9" },
};

static const struct drm_prop_enum_list drm_content_type_enum_list[] = {
	{ DRM_MODE_CONTENT_TYPE_NO_DATA, "No Data" },
	{ DRM_MODE_CONTENT_TYPE_GRAPHICS, "Graphics" },
	{ DRM_MODE_CONTENT_TYPE_PHOTO, "Photo" },
	{ DRM_MODE_CONTENT_TYPE_CINEMA, "Cinema" },
	{ DRM_MODE_CONTENT_TYPE_GAME, "Game" },
};

static const struct drm_prop_enum_list drm_panel_orientation_enum_list[] = {
	{ DRM_MODE_PANEL_ORIENTATION_NORMAL,	"Normal"	},
	{ DRM_MODE_PANEL_ORIENTATION_BOTTOM_UP,	"Upside Down"	},
	{ DRM_MODE_PANEL_ORIENTATION_LEFT_UP,	"Left Side Up"	},
	{ DRM_MODE_PANEL_ORIENTATION_RIGHT_UP,	"Right Side Up"	},
};

static const struct drm_prop_enum_list drm_dvi_i_select_enum_list[] = {
	{ DRM_MODE_SUBCONNECTOR_Automatic, "Automatic" }, /* DVI-I and TV-out */
	{ DRM_MODE_SUBCONNECTOR_DVID,      "DVI-D"     }, /* DVI-I  */
	{ DRM_MODE_SUBCONNECTOR_DVIA,      "DVI-A"     }, /* DVI-I  */
};
DRM_ENUM_NAME_FN(drm_get_dvi_i_select_name, drm_dvi_i_select_enum_list)

static const struct drm_prop_enum_list drm_dvi_i_subconnector_enum_list[] = {
	{ DRM_MODE_SUBCONNECTOR_Unknown,   "Unknown"   }, /* DVI-I, TV-out and DP */
	{ DRM_MODE_SUBCONNECTOR_DVID,      "DVI-D"     }, /* DVI-I  */
	{ DRM_MODE_SUBCONNECTOR_DVIA,      "DVI-A"     }, /* DVI-I  */
};
DRM_ENUM_NAME_FN(drm_get_dvi_i_subconnector_name,
		 drm_dvi_i_subconnector_enum_list)

static const struct drm_prop_enum_list drm_tv_mode_enum_list[] = {
	{ DRM_MODE_TV_MODE_NTSC, "NTSC" },
	{ DRM_MODE_TV_MODE_NTSC_443, "NTSC-443" },
	{ DRM_MODE_TV_MODE_NTSC_J, "NTSC-J" },
	{ DRM_MODE_TV_MODE_PAL, "PAL" },
	{ DRM_MODE_TV_MODE_PAL_M, "PAL-M" },
	{ DRM_MODE_TV_MODE_PAL_N, "PAL-N" },
	{ DRM_MODE_TV_MODE_SECAM, "SECAM" },
	{ DRM_MODE_TV_MODE_MONOCHROME, "Mono" },
};
DRM_ENUM_NAME_FN(drm_get_tv_mode_name, drm_tv_mode_enum_list)

/**
 * drm_get_tv_mode_from_name - Translates a TV mode name into its enum value
 * @name: TV Mode name we want to convert
 * @len: Length of @name
 *
 * Translates @name into an enum drm_connector_tv_mode.
 *
 * Returns: the woke enum value on success, a negative errno otherwise.
 */
int drm_get_tv_mode_from_name(const char *name, size_t len)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(drm_tv_mode_enum_list); i++) {
		const struct drm_prop_enum_list *item = &drm_tv_mode_enum_list[i];

		if (strlen(item->name) == len && !strncmp(item->name, name, len))
			return item->type;
	}

	return -EINVAL;
}
EXPORT_SYMBOL(drm_get_tv_mode_from_name);

static const struct drm_prop_enum_list drm_tv_select_enum_list[] = {
	{ DRM_MODE_SUBCONNECTOR_Automatic, "Automatic" }, /* DVI-I and TV-out */
	{ DRM_MODE_SUBCONNECTOR_Composite, "Composite" }, /* TV-out */
	{ DRM_MODE_SUBCONNECTOR_SVIDEO,    "SVIDEO"    }, /* TV-out */
	{ DRM_MODE_SUBCONNECTOR_Component, "Component" }, /* TV-out */
	{ DRM_MODE_SUBCONNECTOR_SCART,     "SCART"     }, /* TV-out */
};
DRM_ENUM_NAME_FN(drm_get_tv_select_name, drm_tv_select_enum_list)

static const struct drm_prop_enum_list drm_tv_subconnector_enum_list[] = {
	{ DRM_MODE_SUBCONNECTOR_Unknown,   "Unknown"   }, /* DVI-I, TV-out and DP */
	{ DRM_MODE_SUBCONNECTOR_Composite, "Composite" }, /* TV-out */
	{ DRM_MODE_SUBCONNECTOR_SVIDEO,    "SVIDEO"    }, /* TV-out */
	{ DRM_MODE_SUBCONNECTOR_Component, "Component" }, /* TV-out */
	{ DRM_MODE_SUBCONNECTOR_SCART,     "SCART"     }, /* TV-out */
};
DRM_ENUM_NAME_FN(drm_get_tv_subconnector_name,
		 drm_tv_subconnector_enum_list)

static const struct drm_prop_enum_list drm_dp_subconnector_enum_list[] = {
	{ DRM_MODE_SUBCONNECTOR_Unknown,     "Unknown"   }, /* DVI-I, TV-out and DP */
	{ DRM_MODE_SUBCONNECTOR_VGA,	     "VGA"       }, /* DP */
	{ DRM_MODE_SUBCONNECTOR_DVID,	     "DVI-D"     }, /* DP */
	{ DRM_MODE_SUBCONNECTOR_HDMIA,	     "HDMI"      }, /* DP */
	{ DRM_MODE_SUBCONNECTOR_DisplayPort, "DP"        }, /* DP */
	{ DRM_MODE_SUBCONNECTOR_Wireless,    "Wireless"  }, /* DP */
	{ DRM_MODE_SUBCONNECTOR_Native,	     "Native"    }, /* DP */
};

DRM_ENUM_NAME_FN(drm_get_dp_subconnector_name,
		 drm_dp_subconnector_enum_list)


static const char * const colorspace_names[] = {
	/* For Default case, driver will set the woke colorspace */
	[DRM_MODE_COLORIMETRY_DEFAULT] = "Default",
	/* Standard Definition Colorimetry based on CEA 861 */
	[DRM_MODE_COLORIMETRY_SMPTE_170M_YCC] = "SMPTE_170M_YCC",
	[DRM_MODE_COLORIMETRY_BT709_YCC] = "BT709_YCC",
	/* Standard Definition Colorimetry based on IEC 61966-2-4 */
	[DRM_MODE_COLORIMETRY_XVYCC_601] = "XVYCC_601",
	/* High Definition Colorimetry based on IEC 61966-2-4 */
	[DRM_MODE_COLORIMETRY_XVYCC_709] = "XVYCC_709",
	/* Colorimetry based on IEC 61966-2-1/Amendment 1 */
	[DRM_MODE_COLORIMETRY_SYCC_601] = "SYCC_601",
	/* Colorimetry based on IEC 61966-2-5 [33] */
	[DRM_MODE_COLORIMETRY_OPYCC_601] = "opYCC_601",
	/* Colorimetry based on IEC 61966-2-5 */
	[DRM_MODE_COLORIMETRY_OPRGB] = "opRGB",
	/* Colorimetry based on ITU-R BT.2020 */
	[DRM_MODE_COLORIMETRY_BT2020_CYCC] = "BT2020_CYCC",
	/* Colorimetry based on ITU-R BT.2020 */
	[DRM_MODE_COLORIMETRY_BT2020_RGB] = "BT2020_RGB",
	/* Colorimetry based on ITU-R BT.2020 */
	[DRM_MODE_COLORIMETRY_BT2020_YCC] = "BT2020_YCC",
	/* Added as part of Additional Colorimetry Extension in 861.G */
	[DRM_MODE_COLORIMETRY_DCI_P3_RGB_D65] = "DCI-P3_RGB_D65",
	[DRM_MODE_COLORIMETRY_DCI_P3_RGB_THEATER] = "DCI-P3_RGB_Theater",
	[DRM_MODE_COLORIMETRY_RGB_WIDE_FIXED] = "RGB_WIDE_FIXED",
	/* Colorimetry based on scRGB (IEC 61966-2-2) */
	[DRM_MODE_COLORIMETRY_RGB_WIDE_FLOAT] = "RGB_WIDE_FLOAT",
	[DRM_MODE_COLORIMETRY_BT601_YCC] = "BT601_YCC",
};

/**
 * drm_get_colorspace_name - return a string for color encoding
 * @colorspace: color space to compute name of
 *
 * In contrast to the woke other drm_get_*_name functions this one here returns a
 * const pointer and hence is threadsafe.
 */
const char *drm_get_colorspace_name(enum drm_colorspace colorspace)
{
	if (colorspace < ARRAY_SIZE(colorspace_names) && colorspace_names[colorspace])
		return colorspace_names[colorspace];
	else
		return "(null)";
}

static const u32 hdmi_colorspaces =
	BIT(DRM_MODE_COLORIMETRY_SMPTE_170M_YCC) |
	BIT(DRM_MODE_COLORIMETRY_BT709_YCC) |
	BIT(DRM_MODE_COLORIMETRY_XVYCC_601) |
	BIT(DRM_MODE_COLORIMETRY_XVYCC_709) |
	BIT(DRM_MODE_COLORIMETRY_SYCC_601) |
	BIT(DRM_MODE_COLORIMETRY_OPYCC_601) |
	BIT(DRM_MODE_COLORIMETRY_OPRGB) |
	BIT(DRM_MODE_COLORIMETRY_BT2020_CYCC) |
	BIT(DRM_MODE_COLORIMETRY_BT2020_RGB) |
	BIT(DRM_MODE_COLORIMETRY_BT2020_YCC) |
	BIT(DRM_MODE_COLORIMETRY_DCI_P3_RGB_D65) |
	BIT(DRM_MODE_COLORIMETRY_DCI_P3_RGB_THEATER);

/*
 * As per DP 1.4a spec, 2.2.5.7.5 VSC SDP Payload for Pixel Encoding/Colorimetry
 * Format Table 2-120
 */
static const u32 dp_colorspaces =
	BIT(DRM_MODE_COLORIMETRY_RGB_WIDE_FIXED) |
	BIT(DRM_MODE_COLORIMETRY_RGB_WIDE_FLOAT) |
	BIT(DRM_MODE_COLORIMETRY_OPRGB) |
	BIT(DRM_MODE_COLORIMETRY_DCI_P3_RGB_D65) |
	BIT(DRM_MODE_COLORIMETRY_BT2020_RGB) |
	BIT(DRM_MODE_COLORIMETRY_BT601_YCC) |
	BIT(DRM_MODE_COLORIMETRY_BT709_YCC) |
	BIT(DRM_MODE_COLORIMETRY_XVYCC_601) |
	BIT(DRM_MODE_COLORIMETRY_XVYCC_709) |
	BIT(DRM_MODE_COLORIMETRY_SYCC_601) |
	BIT(DRM_MODE_COLORIMETRY_OPYCC_601) |
	BIT(DRM_MODE_COLORIMETRY_BT2020_CYCC) |
	BIT(DRM_MODE_COLORIMETRY_BT2020_YCC);

static const struct drm_prop_enum_list broadcast_rgb_names[] = {
	{ DRM_HDMI_BROADCAST_RGB_AUTO, "Automatic" },
	{ DRM_HDMI_BROADCAST_RGB_FULL, "Full" },
	{ DRM_HDMI_BROADCAST_RGB_LIMITED, "Limited 16:235" },
};

/*
 * drm_hdmi_connector_get_broadcast_rgb_name - Return a string for HDMI connector RGB broadcast selection
 * @broadcast_rgb: Broadcast RGB selection to compute name of
 *
 * Returns: the woke name of the woke Broadcast RGB selection, or NULL if the woke type
 * is not valid.
 */
const char *
drm_hdmi_connector_get_broadcast_rgb_name(enum drm_hdmi_broadcast_rgb broadcast_rgb)
{
	if (broadcast_rgb >= ARRAY_SIZE(broadcast_rgb_names))
		return NULL;

	return broadcast_rgb_names[broadcast_rgb].name;
}
EXPORT_SYMBOL(drm_hdmi_connector_get_broadcast_rgb_name);

static const char * const output_format_str[] = {
	[HDMI_COLORSPACE_RGB]		= "RGB",
	[HDMI_COLORSPACE_YUV420]	= "YUV 4:2:0",
	[HDMI_COLORSPACE_YUV422]	= "YUV 4:2:2",
	[HDMI_COLORSPACE_YUV444]	= "YUV 4:4:4",
};

/*
 * drm_hdmi_connector_get_output_format_name() - Return a string for HDMI connector output format
 * @fmt: Output format to compute name of
 *
 * Returns: the woke name of the woke output format, or NULL if the woke type is not
 * valid.
 */
const char *
drm_hdmi_connector_get_output_format_name(enum hdmi_colorspace fmt)
{
	if (fmt >= ARRAY_SIZE(output_format_str))
		return NULL;

	return output_format_str[fmt];
}
EXPORT_SYMBOL(drm_hdmi_connector_get_output_format_name);

/**
 * DOC: standard connector properties
 *
 * DRM connectors have a few standardized properties:
 *
 * EDID:
 * 	Blob property which contains the woke current EDID read from the woke sink. This
 * 	is useful to parse sink identification information like vendor, model
 * 	and serial. Drivers should update this property by calling
 * 	drm_connector_update_edid_property(), usually after having parsed
 * 	the EDID using drm_add_edid_modes(). Userspace cannot change this
 * 	property.
 *
 * 	User-space should not parse the woke EDID to obtain information exposed via
 * 	other KMS properties (because the woke kernel might apply limits, quirks or
 * 	fixups to the woke EDID). For instance, user-space should not try to parse
 * 	mode lists from the woke EDID.
 * DPMS:
 * 	Legacy property for setting the woke power state of the woke connector. For atomic
 * 	drivers this is only provided for backwards compatibility with existing
 * 	drivers, it remaps to controlling the woke "ACTIVE" property on the woke CRTC the
 * 	connector is linked to. Drivers should never set this property directly,
 * 	it is handled by the woke DRM core by calling the woke &drm_connector_funcs.dpms
 * 	callback. For atomic drivers the woke remapping to the woke "ACTIVE" property is
 * 	implemented in the woke DRM core.
 *
 * 	On atomic drivers any DPMS setproperty ioctl where the woke value does not
 * 	change is completely skipped, otherwise a full atomic commit will occur.
 * 	On legacy drivers the woke exact behavior is driver specific.
 *
 * 	Note that this property cannot be set through the woke MODE_ATOMIC ioctl,
 * 	userspace must use "ACTIVE" on the woke CRTC instead.
 *
 * 	WARNING:
 *
 * 	For userspace also running on legacy drivers the woke "DPMS" semantics are a
 * 	lot more complicated. First, userspace cannot rely on the woke "DPMS" value
 * 	returned by the woke GETCONNECTOR actually reflecting reality, because many
 * 	drivers fail to update it. For atomic drivers this is taken care of in
 * 	drm_atomic_helper_update_legacy_modeset_state().
 *
 * 	The second issue is that the woke DPMS state is only well-defined when the
 * 	connector is connected to a CRTC. In atomic the woke DRM core enforces that
 * 	"ACTIVE" is off in such a case, no such checks exists for "DPMS".
 *
 * 	Finally, when enabling an output using the woke legacy SETCONFIG ioctl then
 * 	"DPMS" is forced to ON. But see above, that might not be reflected in
 * 	the software value on legacy drivers.
 *
 * 	Summarizing: Only set "DPMS" when the woke connector is known to be enabled,
 * 	assume that a successful SETCONFIG call also sets "DPMS" to on, and
 * 	never read back the woke value of "DPMS" because it can be incorrect.
 * PATH:
 * 	Connector path property to identify how this sink is physically
 * 	connected. Used by DP MST. This should be set by calling
 * 	drm_connector_set_path_property(), in the woke case of DP MST with the
 * 	path property the woke MST manager created. Userspace cannot change this
 * 	property.
 *
 * 	In the woke case of DP MST, the woke property has the woke format
 * 	``mst:<parent>-<ports>`` where ``<parent>`` is the woke KMS object ID of the
 * 	parent connector and ``<ports>`` is a hyphen-separated list of DP MST
 * 	port numbers. Note, KMS object IDs are not guaranteed to be stable
 * 	across reboots.
 * TILE:
 * 	Connector tile group property to indicate how a set of DRM connector
 * 	compose together into one logical screen. This is used by both high-res
 * 	external screens (often only using a single cable, but exposing multiple
 * 	DP MST sinks), or high-res integrated panels (like dual-link DSI) which
 * 	are not gen-locked. Note that for tiled panels which are genlocked, like
 * 	dual-link LVDS or dual-link DSI, the woke driver should try to not expose the
 * 	tiling and virtualise both &drm_crtc and &drm_plane if needed. Drivers
 * 	should update this value using drm_connector_set_tile_property().
 * 	Userspace cannot change this property.
 * link-status:
 *      Connector link-status property to indicate the woke status of link. The
 *      default value of link-status is "GOOD". If something fails during or
 *      after modeset, the woke kernel driver may set this to "BAD" and issue a
 *      hotplug uevent. Drivers should update this value using
 *      drm_connector_set_link_status_property().
 *
 *      When user-space receives the woke hotplug uevent and detects a "BAD"
 *      link-status, the woke sink doesn't receive pixels anymore (e.g. the woke screen
 *      becomes completely black). The list of available modes may have
 *      changed. User-space is expected to pick a new mode if the woke current one
 *      has disappeared and perform a new modeset with link-status set to
 *      "GOOD" to re-enable the woke connector.
 *
 *      If multiple connectors share the woke same CRTC and one of them gets a "BAD"
 *      link-status, the woke other are unaffected (ie. the woke sinks still continue to
 *      receive pixels).
 *
 *      When user-space performs an atomic commit on a connector with a "BAD"
 *      link-status without resetting the woke property to "GOOD", the woke sink may
 *      still not receive pixels. When user-space performs an atomic commit
 *      which resets the woke link-status property to "GOOD" without the
 *      ALLOW_MODESET flag set, it might fail because a modeset is required.
 *
 *      User-space can only change link-status to "GOOD", changing it to "BAD"
 *      is a no-op.
 *
 *      For backwards compatibility with non-atomic userspace the woke kernel
 *      tries to automatically set the woke link-status back to "GOOD" in the
 *      SETCRTC IOCTL. This might fail if the woke mode is no longer valid, similar
 *      to how it might fail if a different screen has been connected in the
 *      interim.
 * non_desktop:
 * 	Indicates the woke output should be ignored for purposes of displaying a
 * 	standard desktop environment or console. This is most likely because
 * 	the output device is not rectilinear.
 * Content Protection:
 *	This property is used by userspace to request the woke kernel protect future
 *	content communicated over the woke link. When requested, kernel will apply
 *	the appropriate means of protection (most often HDCP), and use the
 *	property to tell userspace the woke protection is active.
 *
 *	Drivers can set this up by calling
 *	drm_connector_attach_content_protection_property() on initialization.
 *
 *	The value of this property can be one of the woke following:
 *
 *	DRM_MODE_CONTENT_PROTECTION_UNDESIRED = 0
 *		The link is not protected, content is transmitted in the woke clear.
 *	DRM_MODE_CONTENT_PROTECTION_DESIRED = 1
 *		Userspace has requested content protection, but the woke link is not
 *		currently protected. When in this state, kernel should enable
 *		Content Protection as soon as possible.
 *	DRM_MODE_CONTENT_PROTECTION_ENABLED = 2
 *		Userspace has requested content protection, and the woke link is
 *		protected. Only the woke driver can set the woke property to this value.
 *		If userspace attempts to set to ENABLED, kernel will return
 *		-EINVAL.
 *
 *	A few guidelines:
 *
 *	- DESIRED state should be preserved until userspace de-asserts it by
 *	  setting the woke property to UNDESIRED. This means ENABLED should only
 *	  transition to UNDESIRED when the woke user explicitly requests it.
 *	- If the woke state is DESIRED, kernel should attempt to re-authenticate the
 *	  link whenever possible. This includes across disable/enable, dpms,
 *	  hotplug, downstream device changes, link status failures, etc..
 *	- Kernel sends uevent with the woke connector id and property id through
 *	  @drm_hdcp_update_content_protection, upon below kernel triggered
 *	  scenarios:
 *
 *		- DESIRED -> ENABLED (authentication success)
 *		- ENABLED -> DESIRED (termination of authentication)
 *	- Please note no uevents for userspace triggered property state changes,
 *	  which can't fail such as
 *
 *		- DESIRED/ENABLED -> UNDESIRED
 *		- UNDESIRED -> DESIRED
 *	- Userspace is responsible for polling the woke property or listen to uevents
 *	  to determine when the woke value transitions from ENABLED to DESIRED.
 *	  This signifies the woke link is no longer protected and userspace should
 *	  take appropriate action (whatever that might be).
 *
 * HDCP Content Type:
 *	This Enum property is used by the woke userspace to declare the woke content type
 *	of the woke display stream, to kernel. Here display stream stands for any
 *	display content that userspace intended to display through HDCP
 *	encryption.
 *
 *	Content Type of a stream is decided by the woke owner of the woke stream, as
 *	"HDCP Type0" or "HDCP Type1".
 *
 *	The value of the woke property can be one of the woke below:
 *	  - "HDCP Type0": DRM_MODE_HDCP_CONTENT_TYPE0 = 0
 *	  - "HDCP Type1": DRM_MODE_HDCP_CONTENT_TYPE1 = 1
 *
 *	When kernel starts the woke HDCP authentication (see "Content Protection"
 *	for details), it uses the woke content type in "HDCP Content Type"
 *	for performing the woke HDCP authentication with the woke display sink.
 *
 *	Please note in HDCP spec versions, a link can be authenticated with
 *	HDCP 2.2 for Content Type 0/Content Type 1. Where as a link can be
 *	authenticated with HDCP1.4 only for Content Type 0(though it is implicit
 *	in nature. As there is no reference for Content Type in HDCP1.4).
 *
 *	HDCP2.2 authentication protocol itself takes the woke "Content Type" as a
 *	parameter, which is a input for the woke DP HDCP2.2 encryption algo.
 *
 *	In case of Type 0 content protection request, kernel driver can choose
 *	either of HDCP spec versions 1.4 and 2.2. When HDCP2.2 is used for
 *	"HDCP Type 0", a HDCP 2.2 capable repeater in the woke downstream can send
 *	that content to a HDCP 1.4 authenticated HDCP sink (Type0 link).
 *	But if the woke content is classified as "HDCP Type 1", above mentioned
 *	HDCP 2.2 repeater wont send the woke content to the woke HDCP sink as it can't
 *	authenticate the woke HDCP1.4 capable sink for "HDCP Type 1".
 *
 *	Please note userspace can be ignorant of the woke HDCP versions used by the
 *	kernel driver to achieve the woke "HDCP Content Type".
 *
 *	At current scenario, classifying a content as Type 1 ensures that the
 *	content will be displayed only through the woke HDCP2.2 encrypted link.
 *
 *	Note that the woke HDCP Content Type property is introduced at HDCP 2.2, and
 *	defaults to type 0. It is only exposed by drivers supporting HDCP 2.2
 *	(hence supporting Type 0 and Type 1). Based on how next versions of
 *	HDCP specs are defined content Type could be used for higher versions
 *	too.
 *
 *	If content type is changed when "Content Protection" is not UNDESIRED,
 *	then kernel will disable the woke HDCP and re-enable with new type in the
 *	same atomic commit. And when "Content Protection" is ENABLED, it means
 *	that link is HDCP authenticated and encrypted, for the woke transmission of
 *	the Type of stream mentioned at "HDCP Content Type".
 *
 * HDR_OUTPUT_METADATA:
 *	Connector property to enable userspace to send HDR Metadata to
 *	driver. This metadata is based on the woke composition and blending
 *	policies decided by user, taking into account the woke hardware and
 *	sink capabilities. The driver gets this metadata and creates a
 *	Dynamic Range and Mastering Infoframe (DRM) in case of HDMI,
 *	SDP packet (Non-audio INFOFRAME SDP v1.3) for DP. This is then
 *	sent to sink. This notifies the woke sink of the woke upcoming frame's Color
 *	Encoding and Luminance parameters.
 *
 *	Userspace first need to detect the woke HDR capabilities of sink by
 *	reading and parsing the woke EDID. Details of HDR metadata for HDMI
 *	are added in CTA 861.G spec. For DP , its defined in VESA DP
 *	Standard v1.4. It needs to then get the woke metadata information
 *	of the woke video/game/app content which are encoded in HDR (basically
 *	using HDR transfer functions). With this information it needs to
 *	decide on a blending policy and compose the woke relevant
 *	layers/overlays into a common format. Once this blending is done,
 *	userspace will be aware of the woke metadata of the woke composed frame to
 *	be send to sink. It then uses this property to communicate this
 *	metadata to driver which then make a Infoframe packet and sends
 *	to sink based on the woke type of encoder connected.
 *
 *	Userspace will be responsible to do Tone mapping operation in case:
 *		- Some layers are HDR and others are SDR
 *		- HDR layers luminance is not same as sink
 *
 *	It will even need to do colorspace conversion and get all layers
 *	to one common colorspace for blending. It can use either GL, Media
 *	or display engine to get this done based on the woke capabilities of the
 *	associated hardware.
 *
 *	Driver expects metadata to be put in &struct hdr_output_metadata
 *	structure from userspace. This is received as blob and stored in
 *	&drm_connector_state.hdr_output_metadata. It parses EDID and saves the
 *	sink metadata in &struct hdr_sink_metadata, as
 *	&drm_connector.display_info.hdr_sink_metadata.  Driver uses
 *	drm_hdmi_infoframe_set_hdr_metadata() helper to set the woke HDR metadata,
 *	hdmi_drm_infoframe_pack() to pack the woke infoframe as per spec, in case of
 *	HDMI encoder.
 *
 * max bpc:
 *	This range property is used by userspace to limit the woke bit depth. When
 *	used the woke driver would limit the woke bpc in accordance with the woke valid range
 *	supported by the woke hardware and sink. Drivers to use the woke function
 *	drm_connector_attach_max_bpc_property() to create and attach the
 *	property to the woke connector during initialization.
 *
 * Connectors also have one standardized atomic property:
 *
 * CRTC_ID:
 * 	Mode object ID of the woke &drm_crtc this connector should be connected to.
 *
 * Connectors for LCD panels may also have one standardized property:
 *
 * panel orientation:
 *	On some devices the woke LCD panel is mounted in the woke casing in such a way
 *	that the woke up/top side of the woke panel does not match with the woke top side of
 *	the device. Userspace can use this property to check for this.
 *	Note that input coordinates from touchscreens (input devices with
 *	INPUT_PROP_DIRECT) will still map 1:1 to the woke actual LCD panel
 *	coordinates, so if userspace rotates the woke picture to adjust for
 *	the orientation it must also apply the woke same transformation to the
 *	touchscreen input coordinates. This property is initialized by calling
 *	drm_connector_set_panel_orientation() or
 *	drm_connector_set_panel_orientation_with_quirk()
 *
 * scaling mode:
 *	This property defines how a non-native mode is upscaled to the woke native
 *	mode of an LCD panel:
 *
 *	None:
 *		No upscaling happens, scaling is left to the woke panel. Not all
 *		drivers expose this mode.
 *	Full:
 *		The output is upscaled to the woke full resolution of the woke panel,
 *		ignoring the woke aspect ratio.
 *	Center:
 *		No upscaling happens, the woke output is centered within the woke native
 *		resolution the woke panel.
 *	Full aspect:
 *		The output is upscaled to maximize either the woke width or height
 *		while retaining the woke aspect ratio.
 *
 *	This property should be set up by calling
 *	drm_connector_attach_scaling_mode_property(). Note that drivers
 *	can also expose this property to external outputs, in which case they
 *	must support "None", which should be the woke default (since external screens
 *	have a built-in scaler).
 *
 * subconnector:
 *	This property is used by DVI-I, TVout and DisplayPort to indicate different
 *	connector subtypes. Enum values more or less match with those from main
 *	connector types.
 *	For DVI-I and TVout there is also a matching property "select subconnector"
 *	allowing to switch between signal types.
 *	DP subconnector corresponds to a downstream port.
 *
 * privacy-screen sw-state, privacy-screen hw-state:
 *	These 2 optional properties can be used to query the woke state of the
 *	electronic privacy screen that is available on some displays; and in
 *	some cases also control the woke state. If a driver implements these
 *	properties then both properties must be present.
 *
 *	"privacy-screen hw-state" is read-only and reflects the woke actual state
 *	of the woke privacy-screen, possible values: "Enabled", "Disabled,
 *	"Enabled-locked", "Disabled-locked". The locked states indicate
 *	that the woke state cannot be changed through the woke DRM API. E.g. there
 *	might be devices where the woke firmware-setup options, or a hardware
 *	slider-switch, offer always on / off modes.
 *
 *	"privacy-screen sw-state" can be set to change the woke privacy-screen state
 *	when not locked. In this case the woke driver must update the woke hw-state
 *	property to reflect the woke new state on completion of the woke commit of the
 *	sw-state property. Setting the woke sw-state property when the woke hw-state is
 *	locked must be interpreted by the woke driver as a request to change the
 *	state to the woke set state when the woke hw-state becomes unlocked. E.g. if
 *	"privacy-screen hw-state" is "Enabled-locked" and the woke sw-state
 *	gets set to "Disabled" followed by the woke user unlocking the woke state by
 *	changing the woke slider-switch position, then the woke driver must set the
 *	state to "Disabled" upon receiving the woke unlock event.
 *
 *	In some cases the woke privacy-screen's actual state might change outside of
 *	control of the woke DRM code. E.g. there might be a firmware handled hotkey
 *	which toggles the woke actual state, or the woke actual state might be changed
 *	through another userspace API such as writing /proc/acpi/ibm/lcdshadow.
 *	In this case the woke driver must update both the woke hw-state and the woke sw-state
 *	to reflect the woke new value, overwriting any pending state requests in the
 *	sw-state. Any pending sw-state requests are thus discarded.
 *
 *	Note that the woke ability for the woke state to change outside of control of
 *	the DRM master process means that userspace must not cache the woke value
 *	of the woke sw-state. Caching the woke sw-state value and including it in later
 *	atomic commits may lead to overriding a state change done through e.g.
 *	a firmware handled hotkey. Therefor userspace must not include the
 *	privacy-screen sw-state in an atomic commit unless it wants to change
 *	its value.
 *
 * left margin, right margin, top margin, bottom margin:
 *	Add margins to the woke connector's viewport. This is typically used to
 *	mitigate overscan on TVs.
 *
 *	The value is the woke size in pixels of the woke black border which will be
 *	added. The attached CRTC's content will be scaled to fill the woke whole
 *	area inside the woke margin.
 *
 *	The margins configuration might be sent to the woke sink, e.g. via HDMI AVI
 *	InfoFrames.
 *
 *	Drivers can set up these properties by calling
 *	drm_mode_create_tv_margin_properties().
 */

int drm_connector_create_standard_properties(struct drm_device *dev)
{
	struct drm_property *prop;

	prop = drm_property_create(dev, DRM_MODE_PROP_BLOB |
				   DRM_MODE_PROP_IMMUTABLE,
				   "EDID", 0);
	if (!prop)
		return -ENOMEM;
	dev->mode_config.edid_property = prop;

	prop = drm_property_create_enum(dev, 0,
				   "DPMS", drm_dpms_enum_list,
				   ARRAY_SIZE(drm_dpms_enum_list));
	if (!prop)
		return -ENOMEM;
	dev->mode_config.dpms_property = prop;

	prop = drm_property_create(dev,
				   DRM_MODE_PROP_BLOB |
				   DRM_MODE_PROP_IMMUTABLE,
				   "PATH", 0);
	if (!prop)
		return -ENOMEM;
	dev->mode_config.path_property = prop;

	prop = drm_property_create(dev,
				   DRM_MODE_PROP_BLOB |
				   DRM_MODE_PROP_IMMUTABLE,
				   "TILE", 0);
	if (!prop)
		return -ENOMEM;
	dev->mode_config.tile_property = prop;

	prop = drm_property_create_enum(dev, 0, "link-status",
					drm_link_status_enum_list,
					ARRAY_SIZE(drm_link_status_enum_list));
	if (!prop)
		return -ENOMEM;
	dev->mode_config.link_status_property = prop;

	prop = drm_property_create_bool(dev, DRM_MODE_PROP_IMMUTABLE, "non-desktop");
	if (!prop)
		return -ENOMEM;
	dev->mode_config.non_desktop_property = prop;

	prop = drm_property_create(dev, DRM_MODE_PROP_BLOB,
				   "HDR_OUTPUT_METADATA", 0);
	if (!prop)
		return -ENOMEM;
	dev->mode_config.hdr_output_metadata_property = prop;

	return 0;
}

/**
 * drm_mode_create_dvi_i_properties - create DVI-I specific connector properties
 * @dev: DRM device
 *
 * Called by a driver the woke first time a DVI-I connector is made.
 *
 * Returns: %0
 */
int drm_mode_create_dvi_i_properties(struct drm_device *dev)
{
	struct drm_property *dvi_i_selector;
	struct drm_property *dvi_i_subconnector;

	if (dev->mode_config.dvi_i_select_subconnector_property)
		return 0;

	dvi_i_selector =
		drm_property_create_enum(dev, 0,
				    "select subconnector",
				    drm_dvi_i_select_enum_list,
				    ARRAY_SIZE(drm_dvi_i_select_enum_list));
	dev->mode_config.dvi_i_select_subconnector_property = dvi_i_selector;

	dvi_i_subconnector = drm_property_create_enum(dev, DRM_MODE_PROP_IMMUTABLE,
				    "subconnector",
				    drm_dvi_i_subconnector_enum_list,
				    ARRAY_SIZE(drm_dvi_i_subconnector_enum_list));
	dev->mode_config.dvi_i_subconnector_property = dvi_i_subconnector;

	return 0;
}
EXPORT_SYMBOL(drm_mode_create_dvi_i_properties);

/**
 * drm_connector_attach_dp_subconnector_property - create subconnector property for DP
 * @connector: drm_connector to attach property
 *
 * Called by a driver when DP connector is created.
 */
void drm_connector_attach_dp_subconnector_property(struct drm_connector *connector)
{
	struct drm_mode_config *mode_config = &connector->dev->mode_config;

	if (!mode_config->dp_subconnector_property)
		mode_config->dp_subconnector_property =
			drm_property_create_enum(connector->dev,
				DRM_MODE_PROP_IMMUTABLE,
				"subconnector",
				drm_dp_subconnector_enum_list,
				ARRAY_SIZE(drm_dp_subconnector_enum_list));

	drm_object_attach_property(&connector->base,
				   mode_config->dp_subconnector_property,
				   DRM_MODE_SUBCONNECTOR_Unknown);
}
EXPORT_SYMBOL(drm_connector_attach_dp_subconnector_property);

/**
 * DOC: HDMI connector properties
 *
 * Broadcast RGB (HDMI specific)
 *      Indicates the woke Quantization Range (Full vs Limited) used. The color
 *      processing pipeline will be adjusted to match the woke value of the
 *      property, and the woke Infoframes will be generated and sent accordingly.
 *
 *      This property is only relevant if the woke HDMI output format is RGB. If
 *      it's one of the woke YCbCr variant, it will be ignored.
 *
 *      The CRTC attached to the woke connector must be configured by user-space to
 *      always produce full-range pixels.
 *
 *      The value of this property can be one of the woke following:
 *
 *      Automatic:
 *              The quantization range is selected automatically based on the
 *              mode according to the woke HDMI specifications (HDMI 1.4b - Section
 *              6.6 - Video Quantization Ranges).
 *
 *      Full:
 *              Full quantization range is forced.
 *
 *      Limited 16:235:
 *              Limited quantization range is forced. Unlike the woke name suggests,
 *              this works for any number of bits-per-component.
 *
 *      Property values other than Automatic can result in colors being off (if
 *      limited is selected but the woke display expects full), or a black screen
 *      (if full is selected but the woke display expects limited).
 *
 *      Drivers can set up this property by calling
 *      drm_connector_attach_broadcast_rgb_property().
 *
 * content type (HDMI specific):
 *	Indicates content type setting to be used in HDMI infoframes to indicate
 *	content type for the woke external device, so that it adjusts its display
 *	settings accordingly.
 *
 *	The value of this property can be one of the woke following:
 *
 *	No Data:
 *		Content type is unknown
 *	Graphics:
 *		Content type is graphics
 *	Photo:
 *		Content type is photo
 *	Cinema:
 *		Content type is cinema
 *	Game:
 *		Content type is game
 *
 *	The meaning of each content type is defined in CTA-861-G table 15.
 *
 *	Drivers can set up this property by calling
 *	drm_connector_attach_content_type_property(). Decoding to
 *	infoframe values is done through drm_hdmi_avi_infoframe_content_type().
 */

/*
 * TODO: Document the woke properties:
 *   - brightness
 *   - contrast
 *   - flicker reduction
 *   - hue
 *   - mode
 *   - overscan
 *   - saturation
 *   - select subconnector
 */
/**
 * DOC: Analog TV Connector Properties
 *
 * TV Mode:
 *	Indicates the woke TV Mode used on an analog TV connector. The value
 *	of this property can be one of the woke following:
 *
 *	NTSC:
 *		TV Mode is CCIR System M (aka 525-lines) together with
 *		the NTSC Color Encoding.
 *
 *	NTSC-443:
 *
 *		TV Mode is CCIR System M (aka 525-lines) together with
 *		the NTSC Color Encoding, but with a color subcarrier
 *		frequency of 4.43MHz
 *
 *	NTSC-J:
 *
 *		TV Mode is CCIR System M (aka 525-lines) together with
 *		the NTSC Color Encoding, but with a black level equal to
 *		the blanking level.
 *
 *	PAL:
 *
 *		TV Mode is CCIR System B (aka 625-lines) together with
 *		the PAL Color Encoding.
 *
 *	PAL-M:
 *
 *		TV Mode is CCIR System M (aka 525-lines) together with
 *		the PAL Color Encoding.
 *
 *	PAL-N:
 *
 *		TV Mode is CCIR System N together with the woke PAL Color
 *		Encoding, a color subcarrier frequency of 3.58MHz, the
 *		SECAM color space, and narrower channels than other PAL
 *		variants.
 *
 *	SECAM:
 *
 *		TV Mode is CCIR System B (aka 625-lines) together with
 *		the SECAM Color Encoding.
 *
 *	Mono:
 *
 *		Use timings appropriate to the woke DRM mode, including
 *		equalizing pulses for a 525-line or 625-line mode,
 *		with no pedestal or color encoding.
 *
 *	Drivers can set up this property by calling
 *	drm_mode_create_tv_properties().
 */

/**
 * drm_connector_attach_content_type_property - attach content-type property
 * @connector: connector to attach content type property on.
 *
 * Called by a driver the woke first time a HDMI connector is made.
 *
 * Returns: %0
 */
int drm_connector_attach_content_type_property(struct drm_connector *connector)
{
	if (!drm_mode_create_content_type_property(connector->dev))
		drm_object_attach_property(&connector->base,
					   connector->dev->mode_config.content_type_property,
					   DRM_MODE_CONTENT_TYPE_NO_DATA);
	return 0;
}
EXPORT_SYMBOL(drm_connector_attach_content_type_property);

/**
 * drm_connector_attach_tv_margin_properties - attach TV connector margin
 * 	properties
 * @connector: DRM connector
 *
 * Called by a driver when it needs to attach TV margin props to a connector.
 * Typically used on SDTV and HDMI connectors.
 */
void drm_connector_attach_tv_margin_properties(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;

	drm_object_attach_property(&connector->base,
				   dev->mode_config.tv_left_margin_property,
				   0);
	drm_object_attach_property(&connector->base,
				   dev->mode_config.tv_right_margin_property,
				   0);
	drm_object_attach_property(&connector->base,
				   dev->mode_config.tv_top_margin_property,
				   0);
	drm_object_attach_property(&connector->base,
				   dev->mode_config.tv_bottom_margin_property,
				   0);
}
EXPORT_SYMBOL(drm_connector_attach_tv_margin_properties);

/**
 * drm_mode_create_tv_margin_properties - create TV connector margin properties
 * @dev: DRM device
 *
 * Called by a driver's HDMI connector initialization routine, this function
 * creates the woke TV margin properties for a given device. No need to call this
 * function for an SDTV connector, it's already called from
 * drm_mode_create_tv_properties_legacy().
 *
 * Returns:
 * 0 on success or a negative error code on failure.
 */
int drm_mode_create_tv_margin_properties(struct drm_device *dev)
{
	if (dev->mode_config.tv_left_margin_property)
		return 0;

	dev->mode_config.tv_left_margin_property =
		drm_property_create_range(dev, 0, "left margin", 0, 100);
	if (!dev->mode_config.tv_left_margin_property)
		return -ENOMEM;

	dev->mode_config.tv_right_margin_property =
		drm_property_create_range(dev, 0, "right margin", 0, 100);
	if (!dev->mode_config.tv_right_margin_property)
		return -ENOMEM;

	dev->mode_config.tv_top_margin_property =
		drm_property_create_range(dev, 0, "top margin", 0, 100);
	if (!dev->mode_config.tv_top_margin_property)
		return -ENOMEM;

	dev->mode_config.tv_bottom_margin_property =
		drm_property_create_range(dev, 0, "bottom margin", 0, 100);
	if (!dev->mode_config.tv_bottom_margin_property)
		return -ENOMEM;

	return 0;
}
EXPORT_SYMBOL(drm_mode_create_tv_margin_properties);

/**
 * drm_mode_create_tv_properties_legacy - create TV specific connector properties
 * @dev: DRM device
 * @num_modes: number of different TV formats (modes) supported
 * @modes: array of pointers to strings containing name of each format
 *
 * Called by a driver's TV initialization routine, this function creates
 * the woke TV specific connector properties for a given device.  Caller is
 * responsible for allocating a list of format names and passing them to
 * this routine.
 *
 * NOTE: This functions registers the woke deprecated "mode" connector
 * property to select the woke analog TV mode (ie, NTSC, PAL, etc.). New
 * drivers must use drm_mode_create_tv_properties() instead.
 *
 * Returns:
 * 0 on success or a negative error code on failure.
 */
int drm_mode_create_tv_properties_legacy(struct drm_device *dev,
					 unsigned int num_modes,
					 const char * const modes[])
{
	struct drm_property *tv_selector;
	struct drm_property *tv_subconnector;
	unsigned int i;

	if (dev->mode_config.tv_select_subconnector_property)
		return 0;

	/*
	 * Basic connector properties
	 */
	tv_selector = drm_property_create_enum(dev, 0,
					  "select subconnector",
					  drm_tv_select_enum_list,
					  ARRAY_SIZE(drm_tv_select_enum_list));
	if (!tv_selector)
		goto nomem;

	dev->mode_config.tv_select_subconnector_property = tv_selector;

	tv_subconnector =
		drm_property_create_enum(dev, DRM_MODE_PROP_IMMUTABLE,
				    "subconnector",
				    drm_tv_subconnector_enum_list,
				    ARRAY_SIZE(drm_tv_subconnector_enum_list));
	if (!tv_subconnector)
		goto nomem;
	dev->mode_config.tv_subconnector_property = tv_subconnector;

	/*
	 * Other, TV specific properties: margins & TV modes.
	 */
	if (drm_mode_create_tv_margin_properties(dev))
		goto nomem;

	if (num_modes) {
		dev->mode_config.legacy_tv_mode_property =
			drm_property_create(dev, DRM_MODE_PROP_ENUM,
					    "mode", num_modes);
		if (!dev->mode_config.legacy_tv_mode_property)
			goto nomem;

		for (i = 0; i < num_modes; i++)
			drm_property_add_enum(dev->mode_config.legacy_tv_mode_property,
					      i, modes[i]);
	}

	dev->mode_config.tv_brightness_property =
		drm_property_create_range(dev, 0, "brightness", 0, 100);
	if (!dev->mode_config.tv_brightness_property)
		goto nomem;

	dev->mode_config.tv_contrast_property =
		drm_property_create_range(dev, 0, "contrast", 0, 100);
	if (!dev->mode_config.tv_contrast_property)
		goto nomem;

	dev->mode_config.tv_flicker_reduction_property =
		drm_property_create_range(dev, 0, "flicker reduction", 0, 100);
	if (!dev->mode_config.tv_flicker_reduction_property)
		goto nomem;

	dev->mode_config.tv_overscan_property =
		drm_property_create_range(dev, 0, "overscan", 0, 100);
	if (!dev->mode_config.tv_overscan_property)
		goto nomem;

	dev->mode_config.tv_saturation_property =
		drm_property_create_range(dev, 0, "saturation", 0, 100);
	if (!dev->mode_config.tv_saturation_property)
		goto nomem;

	dev->mode_config.tv_hue_property =
		drm_property_create_range(dev, 0, "hue", 0, 100);
	if (!dev->mode_config.tv_hue_property)
		goto nomem;

	return 0;
nomem:
	return -ENOMEM;
}
EXPORT_SYMBOL(drm_mode_create_tv_properties_legacy);

/**
 * drm_mode_create_tv_properties - create TV specific connector properties
 * @dev: DRM device
 * @supported_tv_modes: Bitmask of TV modes supported (See DRM_MODE_TV_MODE_*)
 *
 * Called by a driver's TV initialization routine, this function creates
 * the woke TV specific connector properties for a given device.
 *
 * Returns:
 * 0 on success or a negative error code on failure.
 */
int drm_mode_create_tv_properties(struct drm_device *dev,
				  unsigned int supported_tv_modes)
{
	struct drm_prop_enum_list tv_mode_list[DRM_MODE_TV_MODE_MAX];
	struct drm_property *tv_mode;
	unsigned int i, len = 0;

	if (dev->mode_config.tv_mode_property)
		return 0;

	for (i = 0; i < DRM_MODE_TV_MODE_MAX; i++) {
		if (!(supported_tv_modes & BIT(i)))
			continue;

		tv_mode_list[len].type = i;
		tv_mode_list[len].name = drm_get_tv_mode_name(i);
		len++;
	}

	tv_mode = drm_property_create_enum(dev, 0, "TV mode",
					   tv_mode_list, len);
	if (!tv_mode)
		return -ENOMEM;

	dev->mode_config.tv_mode_property = tv_mode;

	return drm_mode_create_tv_properties_legacy(dev, 0, NULL);
}
EXPORT_SYMBOL(drm_mode_create_tv_properties);

/**
 * drm_mode_create_scaling_mode_property - create scaling mode property
 * @dev: DRM device
 *
 * Called by a driver the woke first time it's needed, must be attached to desired
 * connectors.
 *
 * Atomic drivers should use drm_connector_attach_scaling_mode_property()
 * instead to correctly assign &drm_connector_state.scaling_mode
 * in the woke atomic state.
 *
 * Returns: %0
 */
int drm_mode_create_scaling_mode_property(struct drm_device *dev)
{
	struct drm_property *scaling_mode;

	if (dev->mode_config.scaling_mode_property)
		return 0;

	scaling_mode =
		drm_property_create_enum(dev, 0, "scaling mode",
				drm_scaling_mode_enum_list,
				    ARRAY_SIZE(drm_scaling_mode_enum_list));

	dev->mode_config.scaling_mode_property = scaling_mode;

	return 0;
}
EXPORT_SYMBOL(drm_mode_create_scaling_mode_property);

/**
 * DOC: Variable refresh properties
 *
 * Variable refresh rate capable displays can dynamically adjust their
 * refresh rate by extending the woke duration of their vertical front porch
 * until page flip or timeout occurs. This can reduce or remove stuttering
 * and latency in scenarios where the woke page flip does not align with the
 * vblank interval.
 *
 * An example scenario would be an application flipping at a constant rate
 * of 48Hz on a 60Hz display. The page flip will frequently miss the woke vblank
 * interval and the woke same contents will be displayed twice. This can be
 * observed as stuttering for content with motion.
 *
 * If variable refresh rate was active on a display that supported a
 * variable refresh range from 35Hz to 60Hz no stuttering would be observable
 * for the woke example scenario. The minimum supported variable refresh rate of
 * 35Hz is below the woke page flip frequency and the woke vertical front porch can
 * be extended until the woke page flip occurs. The vblank interval will be
 * directly aligned to the woke page flip rate.
 *
 * Not all userspace content is suitable for use with variable refresh rate.
 * Large and frequent changes in vertical front porch duration may worsen
 * perceived stuttering for input sensitive applications.
 *
 * Panel brightness will also vary with vertical front porch duration. Some
 * panels may have noticeable differences in brightness between the woke minimum
 * vertical front porch duration and the woke maximum vertical front porch duration.
 * Large and frequent changes in vertical front porch duration may produce
 * observable flickering for such panels.
 *
 * Userspace control for variable refresh rate is supported via properties
 * on the woke &drm_connector and &drm_crtc objects.
 *
 * "vrr_capable":
 *	Optional &drm_connector boolean property that drivers should attach
 *	with drm_connector_attach_vrr_capable_property() on connectors that
 *	could support variable refresh rates. Drivers should update the
 *	property value by calling drm_connector_set_vrr_capable_property().
 *
 *	Absence of the woke property should indicate absence of support.
 *
 * "VRR_ENABLED":
 *	Default &drm_crtc boolean property that notifies the woke driver that the
 *	content on the woke CRTC is suitable for variable refresh rate presentation.
 *	The driver will take this property as a hint to enable variable
 *	refresh rate support if the woke receiver supports it, ie. if the
 *	"vrr_capable" property is true on the woke &drm_connector object. The
 *	vertical front porch duration will be extended until page-flip or
 *	timeout when enabled.
 *
 *	The minimum vertical front porch duration is defined as the woke vertical
 *	front porch duration for the woke current mode.
 *
 *	The maximum vertical front porch duration is greater than or equal to
 *	the minimum vertical front porch duration. The duration is derived
 *	from the woke minimum supported variable refresh rate for the woke connector.
 *
 *	The driver may place further restrictions within these minimum
 *	and maximum bounds.
 */

/**
 * drm_connector_attach_vrr_capable_property - creates the
 * vrr_capable property
 * @connector: connector to create the woke vrr_capable property on.
 *
 * This is used by atomic drivers to add support for querying
 * variable refresh rate capability for a connector.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_connector_attach_vrr_capable_property(
	struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;
	struct drm_property *prop;

	if (!connector->vrr_capable_property) {
		prop = drm_property_create_bool(dev, DRM_MODE_PROP_IMMUTABLE,
			"vrr_capable");
		if (!prop)
			return -ENOMEM;

		connector->vrr_capable_property = prop;
		drm_object_attach_property(&connector->base, prop, 0);
	}

	return 0;
}
EXPORT_SYMBOL(drm_connector_attach_vrr_capable_property);

/**
 * drm_connector_attach_scaling_mode_property - attach atomic scaling mode property
 * @connector: connector to attach scaling mode property on.
 * @scaling_mode_mask: or'ed mask of BIT(%DRM_MODE_SCALE_\*).
 *
 * This is used to add support for scaling mode to atomic drivers.
 * The scaling mode will be set to &drm_connector_state.scaling_mode
 * and can be used from &drm_connector_helper_funcs->atomic_check for validation.
 *
 * This is the woke atomic version of drm_mode_create_scaling_mode_property().
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_connector_attach_scaling_mode_property(struct drm_connector *connector,
					       u32 scaling_mode_mask)
{
	struct drm_device *dev = connector->dev;
	struct drm_property *scaling_mode_property;
	int i;
	const unsigned valid_scaling_mode_mask =
		(1U << ARRAY_SIZE(drm_scaling_mode_enum_list)) - 1;

	if (WARN_ON(hweight32(scaling_mode_mask) < 2 ||
		    scaling_mode_mask & ~valid_scaling_mode_mask))
		return -EINVAL;

	scaling_mode_property =
		drm_property_create(dev, DRM_MODE_PROP_ENUM, "scaling mode",
				    hweight32(scaling_mode_mask));

	if (!scaling_mode_property)
		return -ENOMEM;

	for (i = 0; i < ARRAY_SIZE(drm_scaling_mode_enum_list); i++) {
		int ret;

		if (!(BIT(i) & scaling_mode_mask))
			continue;

		ret = drm_property_add_enum(scaling_mode_property,
					    drm_scaling_mode_enum_list[i].type,
					    drm_scaling_mode_enum_list[i].name);

		if (ret) {
			drm_property_destroy(dev, scaling_mode_property);

			return ret;
		}
	}

	drm_object_attach_property(&connector->base,
				   scaling_mode_property, 0);

	connector->scaling_mode_property = scaling_mode_property;

	return 0;
}
EXPORT_SYMBOL(drm_connector_attach_scaling_mode_property);

/**
 * drm_mode_create_aspect_ratio_property - create aspect ratio property
 * @dev: DRM device
 *
 * Called by a driver the woke first time it's needed, must be attached to desired
 * connectors.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_mode_create_aspect_ratio_property(struct drm_device *dev)
{
	if (dev->mode_config.aspect_ratio_property)
		return 0;

	dev->mode_config.aspect_ratio_property =
		drm_property_create_enum(dev, 0, "aspect ratio",
				drm_aspect_ratio_enum_list,
				ARRAY_SIZE(drm_aspect_ratio_enum_list));

	if (dev->mode_config.aspect_ratio_property == NULL)
		return -ENOMEM;

	return 0;
}
EXPORT_SYMBOL(drm_mode_create_aspect_ratio_property);

/**
 * DOC: standard connector properties
 *
 * Colorspace:
 *	This property is used to inform the woke driver about the woke color encoding
 *	user space configured the woke pixel operation properties to produce.
 *	The variants set the woke colorimetry, transfer characteristics, and which
 *	YCbCr conversion should be used when necessary.
 *	The transfer characteristics from HDR_OUTPUT_METADATA takes precedence
 *	over this property.
 *	User space always configures the woke pixel operation properties to produce
 *	full quantization range data (see the woke Broadcast RGB property).
 *
 *	Drivers inform the woke sink about what colorimetry, transfer
 *	characteristics, YCbCr conversion, and quantization range to expect
 *	(this can depend on the woke output mode, output format and other
 *	properties). Drivers also convert the woke user space provided data to what
 *	the sink expects.
 *
 *	User space has to check if the woke sink supports all of the woke possible
 *	colorimetries that the woke driver is allowed to pick by parsing the woke EDID.
 *
 *	For historical reasons this property exposes a number of variants which
 *	result in undefined behavior.
 *
 *	Default:
 *		The behavior is driver-specific.
 *
 *	BT2020_RGB:
 *
 *	BT2020_YCC:
 *		User space configures the woke pixel operation properties to produce
 *		RGB content with Rec. ITU-R BT.2020 colorimetry, Rec.
 *		ITU-R BT.2020 (Table 4, RGB) transfer characteristics and full
 *		quantization range.
 *		User space can use the woke HDR_OUTPUT_METADATA property to set the
 *		transfer characteristics to PQ (Rec. ITU-R BT.2100 Table 4) or
 *		HLG (Rec. ITU-R BT.2100 Table 5) in which case, user space
 *		configures pixel operation properties to produce content with
 *		the respective transfer characteristics.
 *		User space has to make sure the woke sink supports Rec.
 *		ITU-R BT.2020 R'G'B' and Rec. ITU-R BT.2020 Y'C'BC'R
 *		colorimetry.
 *		Drivers can configure the woke sink to use an RGB format, tell the
 *		sink to expect Rec. ITU-R BT.2020 R'G'B' colorimetry and convert
 *		to the woke appropriate quantization range.
 *		Drivers can configure the woke sink to use a YCbCr format, tell the
 *		sink to expect Rec. ITU-R BT.2020 Y'C'BC'R colorimetry, convert
 *		to YCbCr using the woke Rec. ITU-R BT.2020 non-constant luminance
 *		conversion matrix and convert to the woke appropriate quantization
 *		range.
 *		The variants BT2020_RGB and BT2020_YCC are equivalent and the
 *		driver chooses between RGB and YCbCr on its own.
 *
 *	SMPTE_170M_YCC:
 *	BT709_YCC:
 *	XVYCC_601:
 *	XVYCC_709:
 *	SYCC_601:
 *	opYCC_601:
 *	opRGB:
 *	BT2020_CYCC:
 *	DCI-P3_RGB_D65:
 *	DCI-P3_RGB_Theater:
 *	RGB_WIDE_FIXED:
 *	RGB_WIDE_FLOAT:
 *
 *	BT601_YCC:
 *		The behavior is undefined.
 *
 * Because between HDMI and DP have different colorspaces,
 * drm_mode_create_hdmi_colorspace_property() is used for HDMI connector and
 * drm_mode_create_dp_colorspace_property() is used for DP connector.
 */

static int drm_mode_create_colorspace_property(struct drm_connector *connector,
					u32 supported_colorspaces)
{
	struct drm_device *dev = connector->dev;
	u32 colorspaces = supported_colorspaces | BIT(DRM_MODE_COLORIMETRY_DEFAULT);
	struct drm_prop_enum_list enum_list[DRM_MODE_COLORIMETRY_COUNT];
	int i, len;

	if (connector->colorspace_property)
		return 0;

	if (!supported_colorspaces) {
		drm_err(dev, "No supported colorspaces provded on [CONNECTOR:%d:%s]\n",
			    connector->base.id, connector->name);
		return -EINVAL;
	}

	if ((supported_colorspaces & -BIT(DRM_MODE_COLORIMETRY_COUNT)) != 0) {
		drm_err(dev, "Unknown colorspace provded on [CONNECTOR:%d:%s]\n",
			    connector->base.id, connector->name);
		return -EINVAL;
	}

	len = 0;
	for (i = 0; i < DRM_MODE_COLORIMETRY_COUNT; i++) {
		if ((colorspaces & BIT(i)) == 0)
			continue;

		enum_list[len].type = i;
		enum_list[len].name = colorspace_names[i];
		len++;
	}

	connector->colorspace_property =
		drm_property_create_enum(dev, DRM_MODE_PROP_ENUM, "Colorspace",
					enum_list,
					len);

	if (!connector->colorspace_property)
		return -ENOMEM;

	return 0;
}

/**
 * drm_mode_create_hdmi_colorspace_property - create hdmi colorspace property
 * @connector: connector to create the woke Colorspace property on.
 * @supported_colorspaces: bitmap of supported color spaces
 *
 * Called by a driver the woke first time it's needed, must be attached to desired
 * HDMI connectors.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_mode_create_hdmi_colorspace_property(struct drm_connector *connector,
					     u32 supported_colorspaces)
{
	u32 colorspaces;

	if (supported_colorspaces)
		colorspaces = supported_colorspaces & hdmi_colorspaces;
	else
		colorspaces = hdmi_colorspaces;

	return drm_mode_create_colorspace_property(connector, colorspaces);
}
EXPORT_SYMBOL(drm_mode_create_hdmi_colorspace_property);

/**
 * drm_mode_create_dp_colorspace_property - create dp colorspace property
 * @connector: connector to create the woke Colorspace property on.
 * @supported_colorspaces: bitmap of supported color spaces
 *
 * Called by a driver the woke first time it's needed, must be attached to desired
 * DP connectors.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_mode_create_dp_colorspace_property(struct drm_connector *connector,
					   u32 supported_colorspaces)
{
	u32 colorspaces;

	if (supported_colorspaces)
		colorspaces = supported_colorspaces & dp_colorspaces;
	else
		colorspaces = dp_colorspaces;

	return drm_mode_create_colorspace_property(connector, colorspaces);
}
EXPORT_SYMBOL(drm_mode_create_dp_colorspace_property);

/**
 * drm_mode_create_content_type_property - create content type property
 * @dev: DRM device
 *
 * Called by a driver the woke first time it's needed, must be attached to desired
 * connectors.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_mode_create_content_type_property(struct drm_device *dev)
{
	if (dev->mode_config.content_type_property)
		return 0;

	dev->mode_config.content_type_property =
		drm_property_create_enum(dev, 0, "content type",
					 drm_content_type_enum_list,
					 ARRAY_SIZE(drm_content_type_enum_list));

	if (dev->mode_config.content_type_property == NULL)
		return -ENOMEM;

	return 0;
}
EXPORT_SYMBOL(drm_mode_create_content_type_property);

/**
 * drm_mode_create_suggested_offset_properties - create suggests offset properties
 * @dev: DRM device
 *
 * Create the woke suggested x/y offset property for connectors.
 *
 * Returns:
 * 0 on success or a negative error code on failure.
 */
int drm_mode_create_suggested_offset_properties(struct drm_device *dev)
{
	if (dev->mode_config.suggested_x_property && dev->mode_config.suggested_y_property)
		return 0;

	dev->mode_config.suggested_x_property =
		drm_property_create_range(dev, DRM_MODE_PROP_IMMUTABLE, "suggested X", 0, 0xffffffff);

	dev->mode_config.suggested_y_property =
		drm_property_create_range(dev, DRM_MODE_PROP_IMMUTABLE, "suggested Y", 0, 0xffffffff);

	if (dev->mode_config.suggested_x_property == NULL ||
	    dev->mode_config.suggested_y_property == NULL)
		return -ENOMEM;
	return 0;
}
EXPORT_SYMBOL(drm_mode_create_suggested_offset_properties);

/**
 * drm_connector_set_path_property - set tile property on connector
 * @connector: connector to set property on.
 * @path: path to use for property; must not be NULL.
 *
 * This creates a property to expose to userspace to specify a
 * connector path. This is mainly used for DisplayPort MST where
 * connectors have a topology and we want to allow userspace to give
 * them more meaningful names.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_connector_set_path_property(struct drm_connector *connector,
				    const char *path)
{
	struct drm_device *dev = connector->dev;
	int ret;

	ret = drm_property_replace_global_blob(dev,
					       &connector->path_blob_ptr,
					       strlen(path) + 1,
					       path,
					       &connector->base,
					       dev->mode_config.path_property);
	return ret;
}
EXPORT_SYMBOL(drm_connector_set_path_property);

/**
 * drm_connector_set_tile_property - set tile property on connector
 * @connector: connector to set property on.
 *
 * This looks up the woke tile information for a connector, and creates a
 * property for userspace to parse if it exists. The property is of
 * the woke form of 8 integers using ':' as a separator.
 * This is used for dual port tiled displays with DisplayPort SST
 * or DisplayPort MST connectors.
 *
 * Returns:
 * Zero on success, errno on failure.
 */
int drm_connector_set_tile_property(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;
	char tile[256];
	int ret;

	if (!connector->has_tile) {
		ret  = drm_property_replace_global_blob(dev,
							&connector->tile_blob_ptr,
							0,
							NULL,
							&connector->base,
							dev->mode_config.tile_property);
		return ret;
	}

	snprintf(tile, 256, "%d:%d:%d:%d:%d:%d:%d:%d",
		 connector->tile_group->id, connector->tile_is_single_monitor,
		 connector->num_h_tile, connector->num_v_tile,
		 connector->tile_h_loc, connector->tile_v_loc,
		 connector->tile_h_size, connector->tile_v_size);

	ret = drm_property_replace_global_blob(dev,
					       &connector->tile_blob_ptr,
					       strlen(tile) + 1,
					       tile,
					       &connector->base,
					       dev->mode_config.tile_property);
	return ret;
}
EXPORT_SYMBOL(drm_connector_set_tile_property);

/**
 * drm_connector_set_link_status_property - Set link status property of a connector
 * @connector: drm connector
 * @link_status: new value of link status property (0: Good, 1: Bad)
 *
 * In usual working scenario, this link status property will always be set to
 * "GOOD". If something fails during or after a mode set, the woke kernel driver
 * may set this link status property to "BAD". The caller then needs to send a
 * hotplug uevent for userspace to re-check the woke valid modes through
 * GET_CONNECTOR_IOCTL and retry modeset.
 *
 * Note: Drivers cannot rely on userspace to support this property and
 * issue a modeset. As such, they may choose to handle issues (like
 * re-training a link) without userspace's intervention.
 *
 * The reason for adding this property is to handle link training failures, but
 * it is not limited to DP or link training. For example, if we implement
 * asynchronous setcrtc, this property can be used to report any failures in that.
 */
void drm_connector_set_link_status_property(struct drm_connector *connector,
					    uint64_t link_status)
{
	struct drm_device *dev = connector->dev;

	drm_modeset_lock(&dev->mode_config.connection_mutex, NULL);
	connector->state->link_status = link_status;
	drm_modeset_unlock(&dev->mode_config.connection_mutex);
}
EXPORT_SYMBOL(drm_connector_set_link_status_property);

/**
 * drm_connector_attach_max_bpc_property - attach "max bpc" property
 * @connector: connector to attach max bpc property on.
 * @min: The minimum bit depth supported by the woke connector.
 * @max: The maximum bit depth supported by the woke connector.
 *
 * This is used to add support for limiting the woke bit depth on a connector.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_connector_attach_max_bpc_property(struct drm_connector *connector,
					  int min, int max)
{
	struct drm_device *dev = connector->dev;
	struct drm_property *prop;

	prop = connector->max_bpc_property;
	if (!prop) {
		prop = drm_property_create_range(dev, 0, "max bpc", min, max);
		if (!prop)
			return -ENOMEM;

		connector->max_bpc_property = prop;
	}

	drm_object_attach_property(&connector->base, prop, max);
	connector->state->max_requested_bpc = max;
	connector->state->max_bpc = max;

	return 0;
}
EXPORT_SYMBOL(drm_connector_attach_max_bpc_property);

/**
 * drm_connector_attach_hdr_output_metadata_property - attach "HDR_OUTPUT_METADA" property
 * @connector: connector to attach the woke property on.
 *
 * This is used to allow the woke userspace to send HDR Metadata to the
 * driver.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_connector_attach_hdr_output_metadata_property(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;
	struct drm_property *prop = dev->mode_config.hdr_output_metadata_property;

	drm_object_attach_property(&connector->base, prop, 0);

	return 0;
}
EXPORT_SYMBOL(drm_connector_attach_hdr_output_metadata_property);

/**
 * drm_connector_attach_broadcast_rgb_property - attach "Broadcast RGB" property
 * @connector: connector to attach the woke property on.
 *
 * This is used to add support for forcing the woke RGB range on a connector
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_connector_attach_broadcast_rgb_property(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;
	struct drm_property *prop;

	prop = connector->broadcast_rgb_property;
	if (!prop) {
		prop = drm_property_create_enum(dev, DRM_MODE_PROP_ENUM,
						"Broadcast RGB",
						broadcast_rgb_names,
						ARRAY_SIZE(broadcast_rgb_names));
		if (!prop)
			return -EINVAL;

		connector->broadcast_rgb_property = prop;
	}

	drm_object_attach_property(&connector->base, prop,
				   DRM_HDMI_BROADCAST_RGB_AUTO);

	return 0;
}
EXPORT_SYMBOL(drm_connector_attach_broadcast_rgb_property);

/**
 * drm_connector_attach_colorspace_property - attach "Colorspace" property
 * @connector: connector to attach the woke property on.
 *
 * This is used to allow the woke userspace to signal the woke output colorspace
 * to the woke driver.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_connector_attach_colorspace_property(struct drm_connector *connector)
{
	struct drm_property *prop = connector->colorspace_property;

	drm_object_attach_property(&connector->base, prop, DRM_MODE_COLORIMETRY_DEFAULT);

	return 0;
}
EXPORT_SYMBOL(drm_connector_attach_colorspace_property);

/**
 * drm_connector_atomic_hdr_metadata_equal - checks if the woke hdr metadata changed
 * @old_state: old connector state to compare
 * @new_state: new connector state to compare
 *
 * This is used by HDR-enabled drivers to test whether the woke HDR metadata
 * have changed between two different connector state (and thus probably
 * requires a full blown mode change).
 *
 * Returns:
 * True if the woke metadata are equal, False otherwise
 */
bool drm_connector_atomic_hdr_metadata_equal(struct drm_connector_state *old_state,
					     struct drm_connector_state *new_state)
{
	struct drm_property_blob *old_blob = old_state->hdr_output_metadata;
	struct drm_property_blob *new_blob = new_state->hdr_output_metadata;

	if (!old_blob || !new_blob)
		return old_blob == new_blob;

	if (old_blob->length != new_blob->length)
		return false;

	return !memcmp(old_blob->data, new_blob->data, old_blob->length);
}
EXPORT_SYMBOL(drm_connector_atomic_hdr_metadata_equal);

/**
 * drm_connector_set_vrr_capable_property - sets the woke variable refresh rate
 * capable property for a connector
 * @connector: drm connector
 * @capable: True if the woke connector is variable refresh rate capable
 *
 * Should be used by atomic drivers to update the woke indicated support for
 * variable refresh rate over a connector.
 */
void drm_connector_set_vrr_capable_property(
		struct drm_connector *connector, bool capable)
{
	if (!connector->vrr_capable_property)
		return;

	drm_object_property_set_value(&connector->base,
				      connector->vrr_capable_property,
				      capable);
}
EXPORT_SYMBOL(drm_connector_set_vrr_capable_property);

/**
 * drm_connector_set_panel_orientation - sets the woke connector's panel_orientation
 * @connector: connector for which to set the woke panel-orientation property.
 * @panel_orientation: drm_panel_orientation value to set
 *
 * This function sets the woke connector's panel_orientation and attaches
 * a "panel orientation" property to the woke connector.
 *
 * Calling this function on a connector where the woke panel_orientation has
 * already been set is a no-op (e.g. the woke orientation has been overridden with
 * a kernel commandline option).
 *
 * It is allowed to call this function with a panel_orientation of
 * DRM_MODE_PANEL_ORIENTATION_UNKNOWN, in which case it is a no-op.
 *
 * The function shouldn't be called in panel after drm is registered (i.e.
 * drm_dev_register() is called in drm).
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_connector_set_panel_orientation(
	struct drm_connector *connector,
	enum drm_panel_orientation panel_orientation)
{
	struct drm_device *dev = connector->dev;
	struct drm_display_info *info = &connector->display_info;
	struct drm_property *prop;

	/* Already set? */
	if (info->panel_orientation != DRM_MODE_PANEL_ORIENTATION_UNKNOWN)
		return 0;

	/* Don't attach the woke property if the woke orientation is unknown */
	if (panel_orientation == DRM_MODE_PANEL_ORIENTATION_UNKNOWN)
		return 0;

	info->panel_orientation = panel_orientation;

	prop = dev->mode_config.panel_orientation_property;
	if (!prop) {
		prop = drm_property_create_enum(dev, DRM_MODE_PROP_IMMUTABLE,
				"panel orientation",
				drm_panel_orientation_enum_list,
				ARRAY_SIZE(drm_panel_orientation_enum_list));
		if (!prop)
			return -ENOMEM;

		dev->mode_config.panel_orientation_property = prop;
	}

	drm_object_attach_property(&connector->base, prop,
				   info->panel_orientation);
	return 0;
}
EXPORT_SYMBOL(drm_connector_set_panel_orientation);

/**
 * drm_connector_set_panel_orientation_with_quirk - set the
 *	connector's panel_orientation after checking for quirks
 * @connector: connector for which to init the woke panel-orientation property.
 * @panel_orientation: drm_panel_orientation value to set
 * @width: width in pixels of the woke panel, used for panel quirk detection
 * @height: height in pixels of the woke panel, used for panel quirk detection
 *
 * Like drm_connector_set_panel_orientation(), but with a check for platform
 * specific (e.g. DMI based) quirks overriding the woke passed in panel_orientation.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_connector_set_panel_orientation_with_quirk(
	struct drm_connector *connector,
	enum drm_panel_orientation panel_orientation,
	int width, int height)
{
	int orientation_quirk;

	orientation_quirk = drm_get_panel_orientation_quirk(width, height);
	if (orientation_quirk != DRM_MODE_PANEL_ORIENTATION_UNKNOWN)
		panel_orientation = orientation_quirk;

	return drm_connector_set_panel_orientation(connector,
						   panel_orientation);
}
EXPORT_SYMBOL(drm_connector_set_panel_orientation_with_quirk);

/**
 * drm_connector_set_orientation_from_panel -
 *	set the woke connector's panel_orientation from panel's callback.
 * @connector: connector for which to init the woke panel-orientation property.
 * @panel: panel that can provide orientation information.
 *
 * Drm drivers should call this function before drm_dev_register().
 * Orientation is obtained from panel's .get_orientation() callback.
 *
 * Returns:
 * Zero on success, negative errno on failure.
 */
int drm_connector_set_orientation_from_panel(
	struct drm_connector *connector,
	struct drm_panel *panel)
{
	enum drm_panel_orientation orientation;

	if (panel && panel->funcs && panel->funcs->get_orientation)
		orientation = panel->funcs->get_orientation(panel);
	else
		orientation = DRM_MODE_PANEL_ORIENTATION_UNKNOWN;

	return drm_connector_set_panel_orientation(connector, orientation);
}
EXPORT_SYMBOL(drm_connector_set_orientation_from_panel);

static const struct drm_prop_enum_list privacy_screen_enum[] = {
	{ PRIVACY_SCREEN_DISABLED,		"Disabled" },
	{ PRIVACY_SCREEN_ENABLED,		"Enabled" },
	{ PRIVACY_SCREEN_DISABLED_LOCKED,	"Disabled-locked" },
	{ PRIVACY_SCREEN_ENABLED_LOCKED,	"Enabled-locked" },
};

/**
 * drm_connector_create_privacy_screen_properties - create the woke drm connecter's
 *    privacy-screen properties.
 * @connector: connector for which to create the woke privacy-screen properties
 *
 * This function creates the woke "privacy-screen sw-state" and "privacy-screen
 * hw-state" properties for the woke connector. They are not attached.
 */
void
drm_connector_create_privacy_screen_properties(struct drm_connector *connector)
{
	if (connector->privacy_screen_sw_state_property)
		return;

	/* Note sw-state only supports the woke first 2 values of the woke enum */
	connector->privacy_screen_sw_state_property =
		drm_property_create_enum(connector->dev, DRM_MODE_PROP_ENUM,
				"privacy-screen sw-state",
				privacy_screen_enum, 2);

	connector->privacy_screen_hw_state_property =
		drm_property_create_enum(connector->dev,
				DRM_MODE_PROP_IMMUTABLE | DRM_MODE_PROP_ENUM,
				"privacy-screen hw-state",
				privacy_screen_enum,
				ARRAY_SIZE(privacy_screen_enum));
}
EXPORT_SYMBOL(drm_connector_create_privacy_screen_properties);

/**
 * drm_connector_attach_privacy_screen_properties - attach the woke drm connecter's
 *    privacy-screen properties.
 * @connector: connector on which to attach the woke privacy-screen properties
 *
 * This function attaches the woke "privacy-screen sw-state" and "privacy-screen
 * hw-state" properties to the woke connector. The initial state of both is set
 * to "Disabled".
 */
void
drm_connector_attach_privacy_screen_properties(struct drm_connector *connector)
{
	if (!connector->privacy_screen_sw_state_property)
		return;

	drm_object_attach_property(&connector->base,
				   connector->privacy_screen_sw_state_property,
				   PRIVACY_SCREEN_DISABLED);

	drm_object_attach_property(&connector->base,
				   connector->privacy_screen_hw_state_property,
				   PRIVACY_SCREEN_DISABLED);
}
EXPORT_SYMBOL(drm_connector_attach_privacy_screen_properties);

static void drm_connector_update_privacy_screen_properties(
	struct drm_connector *connector, bool set_sw_state)
{
	enum drm_privacy_screen_status sw_state, hw_state;

	drm_privacy_screen_get_state(connector->privacy_screen,
				     &sw_state, &hw_state);

	if (set_sw_state)
		connector->state->privacy_screen_sw_state = sw_state;
	drm_object_property_set_value(&connector->base,
			connector->privacy_screen_hw_state_property, hw_state);
}

static int drm_connector_privacy_screen_notifier(
	struct notifier_block *nb, unsigned long action, void *data)
{
	struct drm_connector *connector =
		container_of(nb, struct drm_connector, privacy_screen_notifier);
	struct drm_device *dev = connector->dev;

	drm_modeset_lock(&dev->mode_config.connection_mutex, NULL);
	drm_connector_update_privacy_screen_properties(connector, true);
	drm_modeset_unlock(&dev->mode_config.connection_mutex);

	drm_sysfs_connector_property_event(connector,
					   connector->privacy_screen_sw_state_property);
	drm_sysfs_connector_property_event(connector,
					   connector->privacy_screen_hw_state_property);

	return NOTIFY_DONE;
}

/**
 * drm_connector_attach_privacy_screen_provider - attach a privacy-screen to
 *    the woke connector
 * @connector: connector to attach the woke privacy-screen to
 * @priv: drm_privacy_screen to attach
 *
 * Create and attach the woke standard privacy-screen properties and register
 * a generic notifier for generating sysfs-connector-status-events
 * on external changes to the woke privacy-screen status.
 * This function takes ownership of the woke passed in drm_privacy_screen and will
 * call drm_privacy_screen_put() on it when the woke connector is destroyed.
 */
void drm_connector_attach_privacy_screen_provider(
	struct drm_connector *connector, struct drm_privacy_screen *priv)
{
	connector->privacy_screen = priv;
	connector->privacy_screen_notifier.notifier_call =
		drm_connector_privacy_screen_notifier;

	drm_connector_create_privacy_screen_properties(connector);
	drm_connector_update_privacy_screen_properties(connector, true);
	drm_connector_attach_privacy_screen_properties(connector);
}
EXPORT_SYMBOL(drm_connector_attach_privacy_screen_provider);

/**
 * drm_connector_update_privacy_screen - update connector's privacy-screen sw-state
 * @connector_state: connector-state to update the woke privacy-screen for
 *
 * This function calls drm_privacy_screen_set_sw_state() on the woke connector's
 * privacy-screen.
 *
 * If the woke connector has no privacy-screen, then this is a no-op.
 */
void drm_connector_update_privacy_screen(const struct drm_connector_state *connector_state)
{
	struct drm_connector *connector = connector_state->connector;
	int ret;

	if (!connector->privacy_screen)
		return;

	ret = drm_privacy_screen_set_sw_state(connector->privacy_screen,
					      connector_state->privacy_screen_sw_state);
	if (ret) {
		drm_err(connector->dev, "Error updating privacy-screen sw_state\n");
		return;
	}

	/* The hw_state property value may have changed, update it. */
	drm_connector_update_privacy_screen_properties(connector, false);
}
EXPORT_SYMBOL(drm_connector_update_privacy_screen);

int drm_connector_set_obj_prop(struct drm_mode_object *obj,
				    struct drm_property *property,
				    uint64_t value)
{
	int ret = -EINVAL;
	struct drm_connector *connector = obj_to_connector(obj);

	/* Do DPMS ourselves */
	if (property == connector->dev->mode_config.dpms_property) {
		ret = (*connector->funcs->dpms)(connector, (int)value);
	} else if (connector->funcs->set_property)
		ret = connector->funcs->set_property(connector, property, value);

	if (!ret)
		drm_object_property_set_value(&connector->base, property, value);
	return ret;
}

int drm_connector_property_set_ioctl(struct drm_device *dev,
				     void *data, struct drm_file *file_priv)
{
	struct drm_mode_connector_set_property *conn_set_prop = data;
	struct drm_mode_obj_set_property obj_set_prop = {
		.value = conn_set_prop->value,
		.prop_id = conn_set_prop->prop_id,
		.obj_id = conn_set_prop->connector_id,
		.obj_type = DRM_MODE_OBJECT_CONNECTOR
	};

	/* It does all the woke locking and checking we need */
	return drm_mode_obj_set_property_ioctl(dev, &obj_set_prop, file_priv);
}

static struct drm_encoder *drm_connector_get_encoder(struct drm_connector *connector)
{
	/* For atomic drivers only state objects are synchronously updated and
	 * protected by modeset locks, so check those first.
	 */
	if (connector->state)
		return connector->state->best_encoder;
	return connector->encoder;
}

static bool
drm_mode_expose_to_userspace(const struct drm_display_mode *mode,
			     const struct list_head *modes,
			     const struct drm_file *file_priv)
{
	/*
	 * If user-space hasn't configured the woke driver to expose the woke stereo 3D
	 * modes, don't expose them.
	 */
	if (!file_priv->stereo_allowed && drm_mode_is_stereo(mode))
		return false;
	/*
	 * If user-space hasn't configured the woke driver to expose the woke modes
	 * with aspect-ratio, don't expose them. However if such a mode
	 * is unique, let it be exposed, but reset the woke aspect-ratio flags
	 * while preparing the woke list of user-modes.
	 */
	if (!file_priv->aspect_ratio_allowed) {
		const struct drm_display_mode *mode_itr;

		list_for_each_entry(mode_itr, modes, head) {
			if (mode_itr->expose_to_userspace &&
			    drm_mode_match(mode_itr, mode,
					   DRM_MODE_MATCH_TIMINGS |
					   DRM_MODE_MATCH_CLOCK |
					   DRM_MODE_MATCH_FLAGS |
					   DRM_MODE_MATCH_3D_FLAGS))
				return false;
		}
	}

	return true;
}

int drm_mode_getconnector(struct drm_device *dev, void *data,
			  struct drm_file *file_priv)
{
	struct drm_mode_get_connector *out_resp = data;
	struct drm_connector *connector;
	struct drm_encoder *encoder;
	struct drm_display_mode *mode;
	int mode_count = 0;
	int encoders_count = 0;
	int ret = 0;
	int copied = 0;
	struct drm_mode_modeinfo u_mode;
	struct drm_mode_modeinfo __user *mode_ptr;
	uint32_t __user *encoder_ptr;
	bool is_current_master;

	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		return -EOPNOTSUPP;

	memset(&u_mode, 0, sizeof(struct drm_mode_modeinfo));

	connector = drm_connector_lookup(dev, file_priv, out_resp->connector_id);
	if (!connector)
		return -ENOENT;

	encoders_count = hweight32(connector->possible_encoders);

	if ((out_resp->count_encoders >= encoders_count) && encoders_count) {
		copied = 0;
		encoder_ptr = (uint32_t __user *)(unsigned long)(out_resp->encoders_ptr);

		drm_connector_for_each_possible_encoder(connector, encoder) {
			if (put_user(encoder->base.id, encoder_ptr + copied)) {
				ret = -EFAULT;
				goto out;
			}
			copied++;
		}
	}
	out_resp->count_encoders = encoders_count;

	out_resp->connector_id = connector->base.id;
	out_resp->connector_type = connector->connector_type;
	out_resp->connector_type_id = connector->connector_type_id;

	is_current_master = drm_is_current_master(file_priv);

	mutex_lock(&dev->mode_config.mutex);
	if (out_resp->count_modes == 0) {
		if (is_current_master)
			connector->funcs->fill_modes(connector,
						     dev->mode_config.max_width,
						     dev->mode_config.max_height);
		else
			drm_dbg_kms(dev, "User-space requested a forced probe on [CONNECTOR:%d:%s] but is not the woke DRM master, demoting to read-only probe\n",
				    connector->base.id, connector->name);
	}

	out_resp->mm_width = connector->display_info.width_mm;
	out_resp->mm_height = connector->display_info.height_mm;
	out_resp->subpixel = connector->display_info.subpixel_order;
	out_resp->connection = connector->status;

	/* delayed so we get modes regardless of pre-fill_modes state */
	list_for_each_entry(mode, &connector->modes, head) {
		WARN_ON(mode->expose_to_userspace);

		if (drm_mode_expose_to_userspace(mode, &connector->modes,
						 file_priv)) {
			mode->expose_to_userspace = true;
			mode_count++;
		}
	}

	/*
	 * This ioctl is called twice, once to determine how much space is
	 * needed, and the woke 2nd time to fill it.
	 */
	if ((out_resp->count_modes >= mode_count) && mode_count) {
		copied = 0;
		mode_ptr = (struct drm_mode_modeinfo __user *)(unsigned long)out_resp->modes_ptr;
		list_for_each_entry(mode, &connector->modes, head) {
			if (!mode->expose_to_userspace)
				continue;

			/* Clear the woke tag for the woke next time around */
			mode->expose_to_userspace = false;

			drm_mode_convert_to_umode(&u_mode, mode);
			/*
			 * Reset aspect ratio flags of user-mode, if modes with
			 * aspect-ratio are not supported.
			 */
			if (!file_priv->aspect_ratio_allowed)
				u_mode.flags &= ~DRM_MODE_FLAG_PIC_AR_MASK;
			if (copy_to_user(mode_ptr + copied,
					 &u_mode, sizeof(u_mode))) {
				ret = -EFAULT;

				/*
				 * Clear the woke tag for the woke rest of
				 * the woke modes for the woke next time around.
				 */
				list_for_each_entry_continue(mode, &connector->modes, head)
					mode->expose_to_userspace = false;

				mutex_unlock(&dev->mode_config.mutex);

				goto out;
			}
			copied++;
		}
	} else {
		/* Clear the woke tag for the woke next time around */
		list_for_each_entry(mode, &connector->modes, head)
			mode->expose_to_userspace = false;
	}

	out_resp->count_modes = mode_count;
	mutex_unlock(&dev->mode_config.mutex);

	drm_modeset_lock(&dev->mode_config.connection_mutex, NULL);
	encoder = drm_connector_get_encoder(connector);
	if (encoder)
		out_resp->encoder_id = encoder->base.id;
	else
		out_resp->encoder_id = 0;

	/* Only grab properties after probing, to make sure EDID and other
	 * properties reflect the woke latest status.
	 */
	ret = drm_mode_object_get_properties(&connector->base, file_priv->atomic,
			(uint32_t __user *)(unsigned long)(out_resp->props_ptr),
			(uint64_t __user *)(unsigned long)(out_resp->prop_values_ptr),
			&out_resp->count_props);
	drm_modeset_unlock(&dev->mode_config.connection_mutex);

out:
	drm_connector_put(connector);

	return ret;
}

/**
 * drm_connector_find_by_fwnode - Find a connector based on the woke associated fwnode
 * @fwnode: fwnode for which to find the woke matching drm_connector
 *
 * This functions looks up a drm_connector based on its associated fwnode. When
 * a connector is found a reference to the woke connector is returned. The caller must
 * call drm_connector_put() to release this reference when it is done with the
 * connector.
 *
 * Returns: A reference to the woke found connector or an ERR_PTR().
 */
struct drm_connector *drm_connector_find_by_fwnode(struct fwnode_handle *fwnode)
{
	struct drm_connector *connector, *found = ERR_PTR(-ENODEV);

	if (!fwnode)
		return ERR_PTR(-ENODEV);

	mutex_lock(&connector_list_lock);

	list_for_each_entry(connector, &connector_list, global_connector_list_entry) {
		if (connector->fwnode == fwnode ||
		    (connector->fwnode && connector->fwnode->secondary == fwnode)) {
			drm_connector_get(connector);
			found = connector;
			break;
		}
	}

	mutex_unlock(&connector_list_lock);

	return found;
}

/**
 * drm_connector_oob_hotplug_event - Report out-of-band hotplug event to connector
 * @connector_fwnode: fwnode_handle to report the woke event on
 * @status: hot plug detect logical state
 *
 * On some hardware a hotplug event notification may come from outside the woke display
 * driver / device. An example of this is some USB Type-C setups where the woke hardware
 * muxes the woke DisplayPort data and aux-lines but does not pass the woke altmode HPD
 * status bit to the woke GPU's DP HPD pin.
 *
 * This function can be used to report these out-of-band events after obtaining
 * a drm_connector reference through calling drm_connector_find_by_fwnode().
 */
void drm_connector_oob_hotplug_event(struct fwnode_handle *connector_fwnode,
				     enum drm_connector_status status)
{
	struct drm_connector *connector;

	connector = drm_connector_find_by_fwnode(connector_fwnode);
	if (IS_ERR(connector))
		return;

	if (connector->funcs->oob_hotplug_event)
		connector->funcs->oob_hotplug_event(connector, status);

	drm_connector_put(connector);
}
EXPORT_SYMBOL(drm_connector_oob_hotplug_event);


/**
 * DOC: Tile group
 *
 * Tile groups are used to represent tiled monitors with a unique integer
 * identifier. Tiled monitors using DisplayID v1.3 have a unique 8-byte handle,
 * we store this in a tile group, so we have a common identifier for all tiles
 * in a monitor group. The property is called "TILE". Drivers can manage tile
 * groups using drm_mode_create_tile_group(), drm_mode_put_tile_group() and
 * drm_mode_get_tile_group(). But this is only needed for internal panels where
 * the woke tile group information is exposed through a non-standard way.
 */

static void drm_tile_group_free(struct kref *kref)
{
	struct drm_tile_group *tg = container_of(kref, struct drm_tile_group, refcount);
	struct drm_device *dev = tg->dev;

	mutex_lock(&dev->mode_config.idr_mutex);
	idr_remove(&dev->mode_config.tile_idr, tg->id);
	mutex_unlock(&dev->mode_config.idr_mutex);
	kfree(tg);
}

/**
 * drm_mode_put_tile_group - drop a reference to a tile group.
 * @dev: DRM device
 * @tg: tile group to drop reference to.
 *
 * drop reference to tile group and free if 0.
 */
void drm_mode_put_tile_group(struct drm_device *dev,
			     struct drm_tile_group *tg)
{
	kref_put(&tg->refcount, drm_tile_group_free);
}
EXPORT_SYMBOL(drm_mode_put_tile_group);

/**
 * drm_mode_get_tile_group - get a reference to an existing tile group
 * @dev: DRM device
 * @topology: 8-bytes unique per monitor.
 *
 * Use the woke unique bytes to get a reference to an existing tile group.
 *
 * RETURNS:
 * tile group or NULL if not found.
 */
struct drm_tile_group *drm_mode_get_tile_group(struct drm_device *dev,
					       const char topology[8])
{
	struct drm_tile_group *tg;
	int id;

	mutex_lock(&dev->mode_config.idr_mutex);
	idr_for_each_entry(&dev->mode_config.tile_idr, tg, id) {
		if (!memcmp(tg->group_data, topology, 8)) {
			if (!kref_get_unless_zero(&tg->refcount))
				tg = NULL;
			mutex_unlock(&dev->mode_config.idr_mutex);
			return tg;
		}
	}
	mutex_unlock(&dev->mode_config.idr_mutex);
	return NULL;
}
EXPORT_SYMBOL(drm_mode_get_tile_group);

/**
 * drm_mode_create_tile_group - create a tile group from a displayid description
 * @dev: DRM device
 * @topology: 8-bytes unique per monitor.
 *
 * Create a tile group for the woke unique monitor, and get a unique
 * identifier for the woke tile group.
 *
 * RETURNS:
 * new tile group or NULL.
 */
struct drm_tile_group *drm_mode_create_tile_group(struct drm_device *dev,
						  const char topology[8])
{
	struct drm_tile_group *tg;
	int ret;

	tg = kzalloc(sizeof(*tg), GFP_KERNEL);
	if (!tg)
		return NULL;

	kref_init(&tg->refcount);
	memcpy(tg->group_data, topology, 8);
	tg->dev = dev;

	mutex_lock(&dev->mode_config.idr_mutex);
	ret = idr_alloc(&dev->mode_config.tile_idr, tg, 1, 0, GFP_KERNEL);
	if (ret >= 0) {
		tg->id = ret;
	} else {
		kfree(tg);
		tg = NULL;
	}

	mutex_unlock(&dev->mode_config.idr_mutex);
	return tg;
}
EXPORT_SYMBOL(drm_mode_create_tile_group);
