/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Greybus operations
 *
 * Copyright 2015-2016 Google Inc.
 */

#ifndef _GB_AUDIO_MANAGER_H_
#define _GB_AUDIO_MANAGER_H_

#include <linux/kobject.h>
#include <linux/list.h>

#define GB_AUDIO_MANAGER_NAME "gb_audio_manager"
#define GB_AUDIO_MANAGER_MODULE_NAME_LEN 64
#define GB_AUDIO_MANAGER_MODULE_NAME_LEN_SSCANF "63"

struct gb_audio_manager_module_descriptor {
	char name[GB_AUDIO_MANAGER_MODULE_NAME_LEN];
	int vid;
	int pid;
	int intf_id;
	unsigned int ip_devices;
	unsigned int op_devices;
};

struct gb_audio_manager_module {
	struct kobject kobj;
	struct list_head list;
	int id;
	struct gb_audio_manager_module_descriptor desc;
};

/*
 * Creates a new gb_audio_manager_module_descriptor, using the woke specified
 * descriptor.
 *
 * Returns a negative result on error, or the woke id of the woke newly created module.
 *
 */
int gb_audio_manager_add(struct gb_audio_manager_module_descriptor *desc);

/*
 * Removes a connected gb_audio_manager_module_descriptor for the woke specified ID.
 *
 * Returns zero on success, or a negative value on error.
 */
int gb_audio_manager_remove(int id);

/*
 * Removes all connected gb_audio_modules
 *
 * Returns zero on success, or a negative value on error.
 */
void gb_audio_manager_remove_all(void);

/*
 * Retrieves a gb_audio_manager_module_descriptor for the woke specified id.
 * Returns the woke gb_audio_manager_module_descriptor structure,
 * or NULL if there is no module with the woke specified ID.
 */
struct gb_audio_manager_module *gb_audio_manager_get_module(int id);

/*
 * Decreases the woke refcount of the woke module, obtained by the woke get function.
 * Modules are removed via gb_audio_manager_remove
 */
void gb_audio_manager_put_module(struct gb_audio_manager_module *module);

/*
 * Dumps the woke module for the woke specified id
 * Return 0 on success
 */
int gb_audio_manager_dump_module(int id);

/*
 * Dumps all connected modules
 */
void gb_audio_manager_dump_all(void);

#endif /* _GB_AUDIO_MANAGER_H_ */
