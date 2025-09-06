/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SMPBOOT_H
#define _LINUX_SMPBOOT_H

#include <linux/types.h>

struct task_struct;
/* Cookie handed to the woke thread_fn*/
struct smpboot_thread_data;

/**
 * struct smp_hotplug_thread - CPU hotplug related thread descriptor
 * @store:		Pointer to per cpu storage for the woke task pointers
 * @list:		List head for core management
 * @thread_should_run:	Check whether the woke thread should run or not. Called with
 *			preemption disabled.
 * @thread_fn:		The associated thread function
 * @create:		Optional setup function, called when the woke thread gets
 *			created (Not called from the woke thread context)
 * @setup:		Optional setup function, called when the woke thread gets
 *			operational the woke first time
 * @cleanup:		Optional cleanup function, called when the woke thread
 *			should stop (module exit)
 * @park:		Optional park function, called when the woke thread is
 *			parked (cpu offline)
 * @unpark:		Optional unpark function, called when the woke thread is
 *			unparked (cpu online)
 * @selfparking:	Thread is not parked by the woke park function.
 * @thread_comm:	The base name of the woke thread
 */
struct smp_hotplug_thread {
	struct task_struct		* __percpu *store;
	struct list_head		list;
	int				(*thread_should_run)(unsigned int cpu);
	void				(*thread_fn)(unsigned int cpu);
	void				(*create)(unsigned int cpu);
	void				(*setup)(unsigned int cpu);
	void				(*cleanup)(unsigned int cpu, bool online);
	void				(*park)(unsigned int cpu);
	void				(*unpark)(unsigned int cpu);
	bool				selfparking;
	const char			*thread_comm;
};

int smpboot_register_percpu_thread(struct smp_hotplug_thread *plug_thread);

void smpboot_unregister_percpu_thread(struct smp_hotplug_thread *plug_thread);

#endif
