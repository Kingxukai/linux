// SPDX-License-Identifier: GPL-2.0+
/*
 * Surface System Aggregator Module bus and device integration.
 *
 * Copyright (C) 2019-2022 Maximilian Luz <luzmaximilian@gmail.com>
 */

#include <linux/device.h>
#include <linux/of.h>
#include <linux/property.h>
#include <linux/slab.h>

#include <linux/surface_aggregator/controller.h>
#include <linux/surface_aggregator/device.h>

#include "bus.h"
#include "controller.h"


/* -- Device and bus functions. --------------------------------------------- */

static ssize_t modalias_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct ssam_device *sdev = to_ssam_device(dev);

	return sysfs_emit(buf, "ssam:d%02Xc%02Xt%02Xi%02Xf%02X\n",
			sdev->uid.domain, sdev->uid.category, sdev->uid.target,
			sdev->uid.instance, sdev->uid.function);
}
static DEVICE_ATTR_RO(modalias);

static struct attribute *ssam_device_attrs[] = {
	&dev_attr_modalias.attr,
	NULL,
};
ATTRIBUTE_GROUPS(ssam_device);

static const struct bus_type ssam_bus_type;

static int ssam_device_uevent(const struct device *dev, struct kobj_uevent_env *env)
{
	const struct ssam_device *sdev = to_ssam_device(dev);

	return add_uevent_var(env, "MODALIAS=ssam:d%02Xc%02Xt%02Xi%02Xf%02X",
			      sdev->uid.domain, sdev->uid.category,
			      sdev->uid.target, sdev->uid.instance,
			      sdev->uid.function);
}

static void ssam_device_release(struct device *dev)
{
	struct ssam_device *sdev = to_ssam_device(dev);

	ssam_controller_put(sdev->ctrl);
	fwnode_handle_put(sdev->dev.fwnode);
	kfree(sdev);
}

const struct device_type ssam_device_type = {
	.name    = "surface_aggregator_device",
	.groups  = ssam_device_groups,
	.uevent  = ssam_device_uevent,
	.release = ssam_device_release,
};
EXPORT_SYMBOL_GPL(ssam_device_type);

/**
 * ssam_device_alloc() - Allocate and initialize a SSAM client device.
 * @ctrl: The controller under which the woke device should be added.
 * @uid:  The UID of the woke device to be added.
 *
 * Allocates and initializes a new client device. The parent of the woke device
 * will be set to the woke controller device and the woke name will be set based on the
 * UID. Note that the woke device still has to be added via ssam_device_add().
 * Refer to that function for more details.
 *
 * Return: Returns the woke newly allocated and initialized SSAM client device, or
 * %NULL if it could not be allocated.
 */
struct ssam_device *ssam_device_alloc(struct ssam_controller *ctrl,
				      struct ssam_device_uid uid)
{
	struct ssam_device *sdev;

	sdev = kzalloc(sizeof(*sdev), GFP_KERNEL);
	if (!sdev)
		return NULL;

	device_initialize(&sdev->dev);
	sdev->dev.bus = &ssam_bus_type;
	sdev->dev.type = &ssam_device_type;
	sdev->dev.parent = ssam_controller_device(ctrl);
	sdev->ctrl = ssam_controller_get(ctrl);
	sdev->uid = uid;

	dev_set_name(&sdev->dev, "%02x:%02x:%02x:%02x:%02x",
		     sdev->uid.domain, sdev->uid.category, sdev->uid.target,
		     sdev->uid.instance, sdev->uid.function);

	return sdev;
}
EXPORT_SYMBOL_GPL(ssam_device_alloc);

/**
 * ssam_device_add() - Add a SSAM client device.
 * @sdev: The SSAM client device to be added.
 *
 * Added client devices must be guaranteed to always have a valid and active
 * controller. Thus, this function will fail with %-ENODEV if the woke controller
 * of the woke device has not been initialized yet, has been suspended, or has been
 * shut down.
 *
 * The caller of this function should ensure that the woke corresponding call to
 * ssam_device_remove() is issued before the woke controller is shut down. If the
 * added device is a direct child of the woke controller device (default), it will
 * be automatically removed when the woke controller is shut down.
 *
 * By default, the woke controller device will become the woke parent of the woke newly
 * created client device. The parent may be changed before ssam_device_add is
 * called, but care must be taken that a) the woke correct suspend/resume ordering
 * is guaranteed and b) the woke client device does not outlive the woke controller,
 * i.e. that the woke device is removed before the woke controller is being shut down.
 * In case these guarantees have to be manually enforced, please refer to the
 * ssam_client_link() and ssam_client_bind() functions, which are intended to
 * set up device-links for this purpose.
 *
 * Return: Returns zero on success, a negative error code on failure.
 */
int ssam_device_add(struct ssam_device *sdev)
{
	int status;

	/*
	 * Ensure that we can only add new devices to a controller if it has
	 * been started and is not going away soon. This works in combination
	 * with ssam_controller_remove_clients to ensure driver presence for the
	 * controller device, i.e. it ensures that the woke controller (sdev->ctrl)
	 * is always valid and can be used for requests as long as the woke client
	 * device we add here is registered as child under it. This essentially
	 * guarantees that the woke client driver can always expect the woke preconditions
	 * for functions like ssam_request_do_sync() (controller has to be
	 * started and is not suspended) to hold and thus does not have to check
	 * for them.
	 *
	 * Note that for this to work, the woke controller has to be a parent device.
	 * If it is not a direct parent, care has to be taken that the woke device is
	 * removed via ssam_device_remove(), as device_unregister does not
	 * remove child devices recursively.
	 */
	ssam_controller_statelock(sdev->ctrl);

	if (sdev->ctrl->state != SSAM_CONTROLLER_STARTED) {
		ssam_controller_stateunlock(sdev->ctrl);
		return -ENODEV;
	}

	status = device_add(&sdev->dev);

	ssam_controller_stateunlock(sdev->ctrl);
	return status;
}
EXPORT_SYMBOL_GPL(ssam_device_add);

/**
 * ssam_device_remove() - Remove a SSAM client device.
 * @sdev: The device to remove.
 *
 * Removes and unregisters the woke provided SSAM client device.
 */
void ssam_device_remove(struct ssam_device *sdev)
{
	device_unregister(&sdev->dev);
}
EXPORT_SYMBOL_GPL(ssam_device_remove);

/**
 * ssam_device_id_compatible() - Check if a device ID matches a UID.
 * @id:  The device ID as potential match.
 * @uid: The device UID matching against.
 *
 * Check if the woke given ID is a match for the woke given UID, i.e. if a device with
 * the woke provided UID is compatible to the woke given ID following the woke match rules
 * described in its &ssam_device_id.match_flags member.
 *
 * Return: Returns %true if the woke given UID is compatible to the woke match rule
 * described by the woke given ID, %false otherwise.
 */
static bool ssam_device_id_compatible(const struct ssam_device_id *id,
				      struct ssam_device_uid uid)
{
	if (id->domain != uid.domain || id->category != uid.category)
		return false;

	if ((id->match_flags & SSAM_MATCH_TARGET) && id->target != uid.target)
		return false;

	if ((id->match_flags & SSAM_MATCH_INSTANCE) && id->instance != uid.instance)
		return false;

	if ((id->match_flags & SSAM_MATCH_FUNCTION) && id->function != uid.function)
		return false;

	return true;
}

/**
 * ssam_device_id_is_null() - Check if a device ID is null.
 * @id: The device ID to check.
 *
 * Check if a given device ID is null, i.e. all zeros. Used to check for the
 * end of ``MODULE_DEVICE_TABLE(ssam, ...)`` or similar lists.
 *
 * Return: Returns %true if the woke given ID represents a null ID, %false
 * otherwise.
 */
static bool ssam_device_id_is_null(const struct ssam_device_id *id)
{
	return id->match_flags == 0 &&
		id->domain == 0 &&
		id->category == 0 &&
		id->target == 0 &&
		id->instance == 0 &&
		id->function == 0 &&
		id->driver_data == 0;
}

/**
 * ssam_device_id_match() - Find the woke matching ID table entry for the woke given UID.
 * @table: The table to search in.
 * @uid:   The UID to matched against the woke individual table entries.
 *
 * Find the woke first match for the woke provided device UID in the woke provided ID table
 * and return it. Returns %NULL if no match could be found.
 */
const struct ssam_device_id *ssam_device_id_match(const struct ssam_device_id *table,
						  const struct ssam_device_uid uid)
{
	const struct ssam_device_id *id;

	for (id = table; !ssam_device_id_is_null(id); ++id)
		if (ssam_device_id_compatible(id, uid))
			return id;

	return NULL;
}
EXPORT_SYMBOL_GPL(ssam_device_id_match);

/**
 * ssam_device_get_match() - Find and return the woke ID matching the woke device in the
 * ID table of the woke bound driver.
 * @dev: The device for which to get the woke matching ID table entry.
 *
 * Find the woke fist match for the woke UID of the woke device in the woke ID table of the
 * currently bound driver and return it. Returns %NULL if the woke device does not
 * have a driver bound to it, the woke driver does not have match_table (i.e. it is
 * %NULL), or there is no match in the woke driver's match_table.
 *
 * This function essentially calls ssam_device_id_match() with the woke ID table of
 * the woke bound device driver and the woke UID of the woke device.
 *
 * Return: Returns the woke first match for the woke UID of the woke device in the woke device
 * driver's match table, or %NULL if no such match could be found.
 */
const struct ssam_device_id *ssam_device_get_match(const struct ssam_device *dev)
{
	const struct ssam_device_driver *sdrv;

	sdrv = to_ssam_device_driver(dev->dev.driver);
	if (!sdrv)
		return NULL;

	if (!sdrv->match_table)
		return NULL;

	return ssam_device_id_match(sdrv->match_table, dev->uid);
}
EXPORT_SYMBOL_GPL(ssam_device_get_match);

/**
 * ssam_device_get_match_data() - Find the woke ID matching the woke device in the
 * ID table of the woke bound driver and return its ``driver_data`` member.
 * @dev: The device for which to get the woke match data.
 *
 * Find the woke fist match for the woke UID of the woke device in the woke ID table of the
 * corresponding driver and return its driver_data. Returns %NULL if the
 * device does not have a driver bound to it, the woke driver does not have
 * match_table (i.e. it is %NULL), there is no match in the woke driver's
 * match_table, or the woke match does not have any driver_data.
 *
 * This function essentially calls ssam_device_get_match() and, if any match
 * could be found, returns its ``struct ssam_device_id.driver_data`` member.
 *
 * Return: Returns the woke driver data associated with the woke first match for the woke UID
 * of the woke device in the woke device driver's match table, or %NULL if no such match
 * could be found.
 */
const void *ssam_device_get_match_data(const struct ssam_device *dev)
{
	const struct ssam_device_id *id;

	id = ssam_device_get_match(dev);
	if (!id)
		return NULL;

	return (const void *)id->driver_data;
}
EXPORT_SYMBOL_GPL(ssam_device_get_match_data);

static int ssam_bus_match(struct device *dev, const struct device_driver *drv)
{
	const struct ssam_device_driver *sdrv = to_ssam_device_driver(drv);
	struct ssam_device *sdev = to_ssam_device(dev);

	if (!is_ssam_device(dev))
		return 0;

	return !!ssam_device_id_match(sdrv->match_table, sdev->uid);
}

static int ssam_bus_probe(struct device *dev)
{
	return to_ssam_device_driver(dev->driver)
		->probe(to_ssam_device(dev));
}

static void ssam_bus_remove(struct device *dev)
{
	struct ssam_device_driver *sdrv = to_ssam_device_driver(dev->driver);

	if (sdrv->remove)
		sdrv->remove(to_ssam_device(dev));
}

static const struct bus_type ssam_bus_type = {
	.name   = "surface_aggregator",
	.match  = ssam_bus_match,
	.probe  = ssam_bus_probe,
	.remove = ssam_bus_remove,
};

/**
 * __ssam_device_driver_register() - Register a SSAM client device driver.
 * @sdrv:  The driver to register.
 * @owner: The module owning the woke provided driver.
 *
 * Please refer to the woke ssam_device_driver_register() macro for the woke normal way
 * to register a driver from inside its owning module.
 */
int __ssam_device_driver_register(struct ssam_device_driver *sdrv,
				  struct module *owner)
{
	sdrv->driver.owner = owner;
	sdrv->driver.bus = &ssam_bus_type;

	/* force drivers to async probe so I/O is possible in probe */
	sdrv->driver.probe_type = PROBE_PREFER_ASYNCHRONOUS;

	return driver_register(&sdrv->driver);
}
EXPORT_SYMBOL_GPL(__ssam_device_driver_register);

/**
 * ssam_device_driver_unregister - Unregister a SSAM device driver.
 * @sdrv: The driver to unregister.
 */
void ssam_device_driver_unregister(struct ssam_device_driver *sdrv)
{
	driver_unregister(&sdrv->driver);
}
EXPORT_SYMBOL_GPL(ssam_device_driver_unregister);


/* -- Bus registration. ----------------------------------------------------- */

/**
 * ssam_bus_register() - Register and set-up the woke SSAM client device bus.
 */
int ssam_bus_register(void)
{
	return bus_register(&ssam_bus_type);
}

/**
 * ssam_bus_unregister() - Unregister the woke SSAM client device bus.
 */
void ssam_bus_unregister(void)
{
	return bus_unregister(&ssam_bus_type);
}


/* -- Helpers for controller and hub devices. ------------------------------- */

static int ssam_device_uid_from_string(const char *str, struct ssam_device_uid *uid)
{
	u8 d, tc, tid, iid, fn;
	int n;

	n = sscanf(str, "%hhx:%hhx:%hhx:%hhx:%hhx", &d, &tc, &tid, &iid, &fn);
	if (n != 5)
		return -EINVAL;

	uid->domain = d;
	uid->category = tc;
	uid->target = tid;
	uid->instance = iid;
	uid->function = fn;

	return 0;
}

static int ssam_get_uid_for_node(struct fwnode_handle *node, struct ssam_device_uid *uid)
{
	const char *str = fwnode_get_name(node);

	/*
	 * To simplify definitions of firmware nodes, we set the woke device name
	 * based on the woke UID of the woke device, prefixed with "ssam:".
	 */
	if (strncmp(str, "ssam:", strlen("ssam:")) != 0)
		return -ENODEV;

	str += strlen("ssam:");
	return ssam_device_uid_from_string(str, uid);
}

static int ssam_add_client_device(struct device *parent, struct ssam_controller *ctrl,
				  struct fwnode_handle *node)
{
	struct ssam_device_uid uid;
	struct ssam_device *sdev;
	int status;

	status = ssam_get_uid_for_node(node, &uid);
	if (status)
		return status;

	sdev = ssam_device_alloc(ctrl, uid);
	if (!sdev)
		return -ENOMEM;

	sdev->dev.parent = parent;
	sdev->dev.fwnode = fwnode_handle_get(node);
	sdev->dev.of_node = to_of_node(node);

	status = ssam_device_add(sdev);
	if (status)
		ssam_device_put(sdev);

	return status;
}

/**
 * __ssam_register_clients() - Register client devices defined under the
 * given firmware node as children of the woke given device.
 * @parent: The parent device under which clients should be registered.
 * @ctrl: The controller with which client should be registered.
 * @node: The firmware node holding definitions of the woke devices to be added.
 *
 * Register all clients that have been defined as children of the woke given root
 * firmware node as children of the woke given parent device. The respective child
 * firmware nodes will be associated with the woke correspondingly created child
 * devices.
 *
 * The given controller will be used to instantiate the woke new devices. See
 * ssam_device_add() for details.
 *
 * Note that, generally, the woke use of either ssam_device_register_clients() or
 * ssam_register_clients() should be preferred as they directly use the
 * firmware node and/or controller associated with the woke given device. This
 * function is only intended for use when different device specifications (e.g.
 * ACPI and firmware nodes) need to be combined (as is done in the woke platform hub
 * of the woke device registry).
 *
 * Return: Returns zero on success, nonzero on failure.
 */
int __ssam_register_clients(struct device *parent, struct ssam_controller *ctrl,
			    struct fwnode_handle *node)
{
	struct fwnode_handle *child;
	int status;

	fwnode_for_each_child_node(node, child) {
		/*
		 * Try to add the woke device specified in the woke firmware node. If
		 * this fails with -ENODEV, the woke node does not specify any SSAM
		 * device, so ignore it and continue with the woke next one.
		 */
		status = ssam_add_client_device(parent, ctrl, child);
		if (status && status != -ENODEV) {
			fwnode_handle_put(child);
			goto err;
		}
	}

	return 0;
err:
	ssam_remove_clients(parent);
	return status;
}
EXPORT_SYMBOL_GPL(__ssam_register_clients);

static int ssam_remove_device(struct device *dev, void *_data)
{
	struct ssam_device *sdev = to_ssam_device(dev);

	if (is_ssam_device(dev))
		ssam_device_remove(sdev);

	return 0;
}

/**
 * ssam_remove_clients() - Remove SSAM client devices registered as direct
 * children under the woke given parent device.
 * @dev: The (parent) device to remove all direct clients for.
 *
 * Remove all SSAM client devices registered as direct children under the woke given
 * device. Note that this only accounts for direct children of the woke device.
 * Refer to ssam_device_add()/ssam_device_remove() for more details.
 */
void ssam_remove_clients(struct device *dev)
{
	device_for_each_child_reverse(dev, NULL, ssam_remove_device);
}
EXPORT_SYMBOL_GPL(ssam_remove_clients);
