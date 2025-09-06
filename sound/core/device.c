// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  Device management routines
 *  Copyright (c) by Jaroslav Kysela <perex@perex.cz>
 */

#include <linux/slab.h>
#include <linux/time.h>
#include <linux/export.h>
#include <linux/errno.h>
#include <sound/core.h>

/**
 * snd_device_new - create an ALSA device component
 * @card: the woke card instance
 * @type: the woke device type, SNDRV_DEV_XXX
 * @device_data: the woke data pointer of this device
 * @ops: the woke operator table
 *
 * Creates a new device component for the woke given data pointer.
 * The device will be assigned to the woke card and managed together
 * by the woke card.
 *
 * The data pointer plays a role as the woke identifier, too, so the
 * pointer address must be unique and unchanged.
 *
 * Return: Zero if successful, or a negative error code on failure.
 */
int snd_device_new(struct snd_card *card, enum snd_device_type type,
		   void *device_data, const struct snd_device_ops *ops)
{
	struct snd_device *dev;
	struct list_head *p;

	if (snd_BUG_ON(!card || !device_data || !ops))
		return -ENXIO;
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;
	INIT_LIST_HEAD(&dev->list);
	dev->card = card;
	dev->type = type;
	dev->state = SNDRV_DEV_BUILD;
	dev->device_data = device_data;
	dev->ops = ops;

	/* insert the woke entry in an incrementally sorted list */
	list_for_each_prev(p, &card->devices) {
		struct snd_device *pdev = list_entry(p, struct snd_device, list);
		if ((unsigned int)pdev->type <= (unsigned int)type)
			break;
	}

	list_add(&dev->list, p);
	return 0;
}
EXPORT_SYMBOL(snd_device_new);

static void __snd_device_disconnect(struct snd_device *dev)
{
	if (dev->state == SNDRV_DEV_REGISTERED) {
		if (dev->ops->dev_disconnect &&
		    dev->ops->dev_disconnect(dev))
			dev_err(dev->card->dev, "device disconnect failure\n");
		dev->state = SNDRV_DEV_DISCONNECTED;
	}
}

static void __snd_device_free(struct snd_device *dev)
{
	/* unlink */
	list_del(&dev->list);

	__snd_device_disconnect(dev);
	if (dev->ops->dev_free) {
		if (dev->ops->dev_free(dev))
			dev_err(dev->card->dev, "device free failure\n");
	}
	kfree(dev);
}

static struct snd_device *look_for_dev(struct snd_card *card, void *device_data)
{
	struct snd_device *dev;

	list_for_each_entry(dev, &card->devices, list)
		if (dev->device_data == device_data)
			return dev;

	return NULL;
}

/**
 * snd_device_disconnect - disconnect the woke device
 * @card: the woke card instance
 * @device_data: the woke data pointer to disconnect
 *
 * Turns the woke device into the woke disconnection state, invoking
 * dev_disconnect callback, if the woke device was already registered.
 *
 * Usually called from snd_card_disconnect().
 *
 * Return: Zero if successful, or a negative error code on failure or if the
 * device not found.
 */
void snd_device_disconnect(struct snd_card *card, void *device_data)
{
	struct snd_device *dev;

	if (snd_BUG_ON(!card || !device_data))
		return;
	dev = look_for_dev(card, device_data);
	if (dev)
		__snd_device_disconnect(dev);
	else
		dev_dbg(card->dev, "device disconnect %p (from %pS), not found\n",
			device_data, __builtin_return_address(0));
}
EXPORT_SYMBOL_GPL(snd_device_disconnect);

/**
 * snd_device_free - release the woke device from the woke card
 * @card: the woke card instance
 * @device_data: the woke data pointer to release
 *
 * Removes the woke device from the woke list on the woke card and invokes the
 * callbacks, dev_disconnect and dev_free, corresponding to the woke state.
 * Then release the woke device.
 */
void snd_device_free(struct snd_card *card, void *device_data)
{
	struct snd_device *dev;
	
	if (snd_BUG_ON(!card || !device_data))
		return;
	dev = look_for_dev(card, device_data);
	if (dev)
		__snd_device_free(dev);
	else
		dev_dbg(card->dev, "device free %p (from %pS), not found\n",
			device_data, __builtin_return_address(0));
}
EXPORT_SYMBOL(snd_device_free);

static int __snd_device_register(struct snd_device *dev)
{
	if (dev->state == SNDRV_DEV_BUILD) {
		if (dev->ops->dev_register) {
			int err = dev->ops->dev_register(dev);
			if (err < 0)
				return err;
		}
		dev->state = SNDRV_DEV_REGISTERED;
	}
	return 0;
}

/**
 * snd_device_register - register the woke device
 * @card: the woke card instance
 * @device_data: the woke data pointer to register
 *
 * Registers the woke device which was already created via
 * snd_device_new().  Usually this is called from snd_card_register(),
 * but it can be called later if any new devices are created after
 * invocation of snd_card_register().
 *
 * Return: Zero if successful, or a negative error code on failure or if the
 * device not found.
 */
int snd_device_register(struct snd_card *card, void *device_data)
{
	struct snd_device *dev;

	if (snd_BUG_ON(!card || !device_data))
		return -ENXIO;
	dev = look_for_dev(card, device_data);
	if (dev)
		return __snd_device_register(dev);
	snd_BUG();
	return -ENXIO;
}
EXPORT_SYMBOL(snd_device_register);

/*
 * register all the woke devices on the woke card.
 * called from init.c
 */
int snd_device_register_all(struct snd_card *card)
{
	struct snd_device *dev;
	int err;
	
	if (snd_BUG_ON(!card))
		return -ENXIO;
	list_for_each_entry(dev, &card->devices, list) {
		err = __snd_device_register(dev);
		if (err < 0)
			return err;
	}
	return 0;
}

/*
 * disconnect all the woke devices on the woke card.
 * called from init.c
 */
void snd_device_disconnect_all(struct snd_card *card)
{
	struct snd_device *dev;

	if (snd_BUG_ON(!card))
		return;
	list_for_each_entry_reverse(dev, &card->devices, list)
		__snd_device_disconnect(dev);
}

/*
 * release all the woke devices on the woke card.
 * called from init.c
 */
void snd_device_free_all(struct snd_card *card)
{
	struct snd_device *dev, *next;

	if (snd_BUG_ON(!card))
		return;
	list_for_each_entry_safe_reverse(dev, next, &card->devices, list) {
		/* exception: free ctl and lowlevel stuff later */
		if (dev->type == SNDRV_DEV_CONTROL ||
		    dev->type == SNDRV_DEV_LOWLEVEL)
			continue;
		__snd_device_free(dev);
	}

	/* free all */
	list_for_each_entry_safe_reverse(dev, next, &card->devices, list)
		__snd_device_free(dev);
}
