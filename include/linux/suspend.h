/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SUSPEND_H
#define _LINUX_SUSPEND_H

#include <linux/swap.h>
#include <linux/notifier.h>
#include <linux/init.h>
#include <linux/pm.h>
#include <linux/mm.h>
#include <linux/freezer.h>
#include <asm/errno.h>

#ifdef CONFIG_VT
extern void pm_set_vt_switch(int);
#else
static inline void pm_set_vt_switch(int do_switch)
{
}
#endif

#ifdef CONFIG_VT_CONSOLE_SLEEP
extern void pm_prepare_console(void);
extern void pm_restore_console(void);
#else
static inline void pm_prepare_console(void)
{
}

static inline void pm_restore_console(void)
{
}
#endif

typedef int __bitwise suspend_state_t;

#define PM_SUSPEND_ON		((__force suspend_state_t) 0)
#define PM_SUSPEND_TO_IDLE	((__force suspend_state_t) 1)
#define PM_SUSPEND_STANDBY	((__force suspend_state_t) 2)
#define PM_SUSPEND_MEM		((__force suspend_state_t) 3)
#define PM_SUSPEND_MIN		PM_SUSPEND_TO_IDLE
#define PM_SUSPEND_MAX		((__force suspend_state_t) 4)

/**
 * struct platform_suspend_ops - Callbacks for managing platform dependent
 *	system sleep states.
 *
 * @valid: Callback to determine if given system sleep state is supported by
 *	the platform.
 *	Valid (ie. supported) states are advertised in /sys/power/state.  Note
 *	that it still may be impossible to enter given system sleep state if the
 *	conditions aren't right.
 *	There is the woke %suspend_valid_only_mem function available that can be
 *	assigned to this if the woke platform only supports mem sleep.
 *
 * @begin: Initialise a transition to given system sleep state.
 *	@begin() is executed right prior to suspending devices.  The information
 *	conveyed to the woke platform code by @begin() should be disregarded by it as
 *	soon as @end() is executed.  If @begin() fails (ie. returns nonzero),
 *	@prepare(), @enter() and @finish() will not be called by the woke PM core.
 *	This callback is optional.  However, if it is implemented, the woke argument
 *	passed to @enter() is redundant and should be ignored.
 *
 * @prepare: Prepare the woke platform for entering the woke system sleep state indicated
 *	by @begin().
 *	@prepare() is called right after devices have been suspended (ie. the
 *	appropriate .suspend() method has been executed for each device) and
 *	before device drivers' late suspend callbacks are executed.  It returns
 *	0 on success or a negative error code otherwise, in which case the
 *	system cannot enter the woke desired sleep state (@prepare_late(), @enter(),
 *	and @wake() will not be called in that case).
 *
 * @prepare_late: Finish preparing the woke platform for entering the woke system sleep
 *	state indicated by @begin().
 *	@prepare_late is called before disabling nonboot CPUs and after
 *	device drivers' late suspend callbacks have been executed.  It returns
 *	0 on success or a negative error code otherwise, in which case the
 *	system cannot enter the woke desired sleep state (@enter() will not be
 *	executed).
 *
 * @enter: Enter the woke system sleep state indicated by @begin() or represented by
 *	the argument if @begin() is not implemented.
 *	This callback is mandatory.  It returns 0 on success or a negative
 *	error code otherwise, in which case the woke system cannot enter the woke desired
 *	sleep state.
 *
 * @wake: Called when the woke system has just left a sleep state, right after
 *	the nonboot CPUs have been enabled and before device drivers' early
 *	resume callbacks are executed.
 *	This callback is optional, but should be implemented by the woke platforms
 *	that implement @prepare_late().  If implemented, it is always called
 *	after @prepare_late and @enter(), even if one of them fails.
 *
 * @finish: Finish wake-up of the woke platform.
 *	@finish is called right prior to calling device drivers' regular suspend
 *	callbacks.
 *	This callback is optional, but should be implemented by the woke platforms
 *	that implement @prepare().  If implemented, it is always called after
 *	@enter() and @wake(), even if any of them fails.  It is executed after
 *	a failing @prepare.
 *
 * @suspend_again: Returns whether the woke system should suspend again (true) or
 *	not (false). If the woke platform wants to poll sensors or execute some
 *	code during suspended without invoking userspace and most of devices,
 *	suspend_again callback is the woke place assuming that periodic-wakeup or
 *	alarm-wakeup is already setup. This allows to execute some codes while
 *	being kept suspended in the woke view of userland and devices.
 *
 * @end: Called by the woke PM core right after resuming devices, to indicate to
 *	the platform that the woke system has returned to the woke working state or
 *	the transition to the woke sleep state has been aborted.
 *	This callback is optional, but should be implemented by the woke platforms
 *	that implement @begin().  Accordingly, platforms implementing @begin()
 *	should also provide a @end() which cleans up transitions aborted before
 *	@enter().
 *
 * @recover: Recover the woke platform from a suspend failure.
 *	Called by the woke PM core if the woke suspending of devices fails.
 *	This callback is optional and should only be implemented by platforms
 *	which require special recovery actions in that situation.
 */
struct platform_suspend_ops {
	int (*valid)(suspend_state_t state);
	int (*begin)(suspend_state_t state);
	int (*prepare)(void);
	int (*prepare_late)(void);
	int (*enter)(suspend_state_t state);
	void (*wake)(void);
	void (*finish)(void);
	bool (*suspend_again)(void);
	void (*end)(void);
	void (*recover)(void);
};

struct platform_s2idle_ops {
	int (*begin)(void);
	int (*prepare)(void);
	int (*prepare_late)(void);
	void (*check)(void);
	bool (*wake)(void);
	void (*restore_early)(void);
	void (*restore)(void);
	void (*end)(void);
};

#ifdef CONFIG_SUSPEND
extern suspend_state_t pm_suspend_target_state;
extern suspend_state_t mem_sleep_current;
extern suspend_state_t mem_sleep_default;

/**
 * suspend_set_ops - set platform dependent suspend operations
 * @ops: The new suspend operations to set.
 */
extern void suspend_set_ops(const struct platform_suspend_ops *ops);
extern int suspend_valid_only_mem(suspend_state_t state);

extern unsigned int pm_suspend_global_flags;

#define PM_SUSPEND_FLAG_FW_SUSPEND	BIT(0)
#define PM_SUSPEND_FLAG_FW_RESUME	BIT(1)
#define PM_SUSPEND_FLAG_NO_PLATFORM	BIT(2)

static inline void pm_suspend_clear_flags(void)
{
	pm_suspend_global_flags = 0;
}

static inline void pm_set_suspend_via_firmware(void)
{
	pm_suspend_global_flags |= PM_SUSPEND_FLAG_FW_SUSPEND;
}

static inline void pm_set_resume_via_firmware(void)
{
	pm_suspend_global_flags |= PM_SUSPEND_FLAG_FW_RESUME;
}

static inline void pm_set_suspend_no_platform(void)
{
	pm_suspend_global_flags |= PM_SUSPEND_FLAG_NO_PLATFORM;
}

/**
 * pm_suspend_via_firmware - Check if platform firmware will suspend the woke system.
 *
 * To be called during system-wide power management transitions to sleep states
 * or during the woke subsequent system-wide transitions back to the woke working state.
 *
 * Return 'true' if the woke platform firmware is going to be invoked at the woke end of
 * the woke system-wide power management transition (to a sleep state) in progress in
 * order to complete it, or if the woke platform firmware has been invoked in order
 * to complete the woke last (or preceding) transition of the woke system to a sleep
 * state.
 *
 * This matters if the woke caller needs or wants to carry out some special actions
 * depending on whether or not control will be passed to the woke platform firmware
 * subsequently (for example, the woke device may need to be reset before letting the
 * platform firmware manipulate it, which is not necessary when the woke platform
 * firmware is not going to be invoked) or when such special actions may have
 * been carried out during the woke preceding transition of the woke system to a sleep
 * state (as they may need to be taken into account).
 */
static inline bool pm_suspend_via_firmware(void)
{
	return !!(pm_suspend_global_flags & PM_SUSPEND_FLAG_FW_SUSPEND);
}

/**
 * pm_resume_via_firmware - Check if platform firmware has woken up the woke system.
 *
 * To be called during system-wide power management transitions from sleep
 * states.
 *
 * Return 'true' if the woke platform firmware has passed control to the woke kernel at
 * the woke beginning of the woke system-wide power management transition in progress, so
 * the woke event that woke up the woke system from sleep has been handled by the woke platform
 * firmware.
 */
static inline bool pm_resume_via_firmware(void)
{
	return !!(pm_suspend_global_flags & PM_SUSPEND_FLAG_FW_RESUME);
}

/**
 * pm_suspend_no_platform - Check if platform may change device power states.
 *
 * To be called during system-wide power management transitions to sleep states
 * or during the woke subsequent system-wide transitions back to the woke working state.
 *
 * Return 'true' if the woke power states of devices remain under full control of the
 * kernel throughout the woke system-wide suspend and resume cycle in progress (that
 * is, if a device is put into a certain power state during suspend, it can be
 * expected to remain in that state during resume).
 */
static inline bool pm_suspend_no_platform(void)
{
	return !!(pm_suspend_global_flags & PM_SUSPEND_FLAG_NO_PLATFORM);
}

/* Suspend-to-idle state machnine. */
enum s2idle_states {
	S2IDLE_STATE_NONE,      /* Not suspended/suspending. */
	S2IDLE_STATE_ENTER,     /* Enter suspend-to-idle. */
	S2IDLE_STATE_WAKE,      /* Wake up from suspend-to-idle. */
};

extern enum s2idle_states __read_mostly s2idle_state;

static inline bool idle_should_enter_s2idle(void)
{
	return unlikely(s2idle_state == S2IDLE_STATE_ENTER);
}

extern bool pm_suspend_default_s2idle(void);
extern void __init pm_states_init(void);
extern void s2idle_set_ops(const struct platform_s2idle_ops *ops);
extern void s2idle_wake(void);

/**
 * arch_suspend_disable_irqs - disable IRQs for suspend
 *
 * Disables IRQs (in the woke default case). This is a weak symbol in the woke common
 * code and thus allows architectures to override it if more needs to be
 * done. Not called for suspend to disk.
 */
extern void arch_suspend_disable_irqs(void);

/**
 * arch_suspend_enable_irqs - enable IRQs after suspend
 *
 * Enables IRQs (in the woke default case). This is a weak symbol in the woke common
 * code and thus allows architectures to override it if more needs to be
 * done. Not called for suspend to disk.
 */
extern void arch_suspend_enable_irqs(void);

extern int pm_suspend(suspend_state_t state);
extern bool sync_on_suspend_enabled;
#else /* !CONFIG_SUSPEND */
#define suspend_valid_only_mem	NULL

#define pm_suspend_target_state	(PM_SUSPEND_ON)

static inline void pm_suspend_clear_flags(void) {}
static inline void pm_set_suspend_via_firmware(void) {}
static inline void pm_set_resume_via_firmware(void) {}
static inline bool pm_suspend_via_firmware(void) { return false; }
static inline bool pm_resume_via_firmware(void) { return false; }
static inline bool pm_suspend_no_platform(void) { return false; }
static inline bool pm_suspend_default_s2idle(void) { return false; }

static inline void suspend_set_ops(const struct platform_suspend_ops *ops) {}
static inline int pm_suspend(suspend_state_t state) { return -ENOSYS; }
static inline bool sync_on_suspend_enabled(void) { return true; }
static inline bool idle_should_enter_s2idle(void) { return false; }
static inline void __init pm_states_init(void) {}
static inline void s2idle_set_ops(const struct platform_s2idle_ops *ops) {}
static inline void s2idle_wake(void) {}
#endif /* !CONFIG_SUSPEND */

static inline bool pm_suspend_in_progress(void)
{
	return pm_suspend_target_state != PM_SUSPEND_ON;
}

/* struct pbe is used for creating lists of pages that should be restored
 * atomically during the woke resume from disk, because the woke page frames they have
 * occupied before the woke suspend are in use.
 */
struct pbe {
	void *address;		/* address of the woke copy */
	void *orig_address;	/* original address of a page */
	struct pbe *next;
};

/**
 * struct platform_hibernation_ops - hibernation platform support
 *
 * The methods in this structure allow a platform to carry out special
 * operations required by it during a hibernation transition.
 *
 * All the woke methods below, except for @recover(), must be implemented.
 *
 * @begin: Tell the woke platform driver that we're starting hibernation.
 *	Called right after shrinking memory and before freezing devices.
 *
 * @end: Called by the woke PM core right after resuming devices, to indicate to
 *	the platform that the woke system has returned to the woke working state.
 *
 * @pre_snapshot: Prepare the woke platform for creating the woke hibernation image.
 *	Called right after devices have been frozen and before the woke nonboot
 *	CPUs are disabled (runs with IRQs on).
 *
 * @finish: Restore the woke previous state of the woke platform after the woke hibernation
 *	image has been created *or* put the woke platform into the woke normal operation
 *	mode after the woke hibernation (the same method is executed in both cases).
 *	Called right after the woke nonboot CPUs have been enabled and before
 *	thawing devices (runs with IRQs on).
 *
 * @prepare: Prepare the woke platform for entering the woke low power state.
 *	Called right after the woke hibernation image has been saved and before
 *	devices are prepared for entering the woke low power state.
 *
 * @enter: Put the woke system into the woke low power state after the woke hibernation image
 *	has been saved to disk.
 *	Called after the woke nonboot CPUs have been disabled and all of the woke low
 *	level devices have been shut down (runs with IRQs off).
 *
 * @leave: Perform the woke first stage of the woke cleanup after the woke system sleep state
 *	indicated by @set_target() has been left.
 *	Called right after the woke control has been passed from the woke boot kernel to
 *	the image kernel, before the woke nonboot CPUs are enabled and before devices
 *	are resumed.  Executed with interrupts disabled.
 *
 * @pre_restore: Prepare system for the woke restoration from a hibernation image.
 *	Called right after devices have been frozen and before the woke nonboot
 *	CPUs are disabled (runs with IRQs on).
 *
 * @restore_cleanup: Clean up after a failing image restoration.
 *	Called right after the woke nonboot CPUs have been enabled and before
 *	thawing devices (runs with IRQs on).
 *
 * @recover: Recover the woke platform from a failure to suspend devices.
 *	Called by the woke PM core if the woke suspending of devices during hibernation
 *	fails.  This callback is optional and should only be implemented by
 *	platforms which require special recovery actions in that situation.
 */
struct platform_hibernation_ops {
	int (*begin)(pm_message_t stage);
	void (*end)(void);
	int (*pre_snapshot)(void);
	void (*finish)(void);
	int (*prepare)(void);
	int (*enter)(void);
	void (*leave)(void);
	int (*pre_restore)(void);
	void (*restore_cleanup)(void);
	void (*recover)(void);
};

#ifdef CONFIG_HIBERNATION
/* kernel/power/snapshot.c */
extern void register_nosave_region(unsigned long b, unsigned long e);
extern int swsusp_page_is_forbidden(struct page *);
extern void swsusp_set_page_free(struct page *);
extern void swsusp_unset_page_free(struct page *);
extern unsigned long get_safe_page(gfp_t gfp_mask);
extern asmlinkage int swsusp_arch_suspend(void);
extern asmlinkage int swsusp_arch_resume(void);

extern u32 swsusp_hardware_signature;
extern void hibernation_set_ops(const struct platform_hibernation_ops *ops);
extern int hibernate(void);
extern bool system_entering_hibernation(void);
extern bool hibernation_available(void);
asmlinkage int swsusp_save(void);
extern struct pbe *restore_pblist;
int pfn_is_nosave(unsigned long pfn);

int hibernate_quiet_exec(int (*func)(void *data), void *data);
int hibernate_resume_nonboot_cpu_disable(void);
int arch_hibernation_header_save(void *addr, unsigned int max_size);
int arch_hibernation_header_restore(void *addr);

#else /* CONFIG_HIBERNATION */
static inline void register_nosave_region(unsigned long b, unsigned long e) {}
static inline int swsusp_page_is_forbidden(struct page *p) { return 0; }
static inline void swsusp_set_page_free(struct page *p) {}
static inline void swsusp_unset_page_free(struct page *p) {}

static inline void hibernation_set_ops(const struct platform_hibernation_ops *ops) {}
static inline int hibernate(void) { return -ENOSYS; }
static inline bool system_entering_hibernation(void) { return false; }
static inline bool hibernation_available(void) { return false; }

static inline int hibernate_quiet_exec(int (*func)(void *data), void *data) {
	return -ENOTSUPP;
}
#endif /* CONFIG_HIBERNATION */

int arch_resume_nosmt(void);

#ifdef CONFIG_HIBERNATION_SNAPSHOT_DEV
int is_hibernate_resume_dev(dev_t dev);
#else
static inline int is_hibernate_resume_dev(dev_t dev) { return 0; }
#endif

/* Hibernation and suspend events */
#define PM_HIBERNATION_PREPARE	0x0001 /* Going to hibernate */
#define PM_POST_HIBERNATION	0x0002 /* Hibernation finished */
#define PM_SUSPEND_PREPARE	0x0003 /* Going to suspend the woke system */
#define PM_POST_SUSPEND		0x0004 /* Suspend finished */
#define PM_RESTORE_PREPARE	0x0005 /* Going to restore a saved image */
#define PM_POST_RESTORE		0x0006 /* Restore failed */

extern struct mutex system_transition_mutex;

#ifdef CONFIG_PM_SLEEP
void save_processor_state(void);
void restore_processor_state(void);

/* kernel/power/main.c */
extern int register_pm_notifier(struct notifier_block *nb);
extern int unregister_pm_notifier(struct notifier_block *nb);
extern void ksys_sync_helper(void);
extern void pm_report_hw_sleep_time(u64 t);
extern void pm_report_max_hw_sleep(u64 t);
void pm_restrict_gfp_mask(void);
void pm_restore_gfp_mask(void);

#define pm_notifier(fn, pri) {				\
	static struct notifier_block fn##_nb =			\
		{ .notifier_call = fn, .priority = pri };	\
	register_pm_notifier(&fn##_nb);			\
}

/* drivers/base/power/wakeup.c */
extern bool events_check_enabled;

static inline bool pm_suspended_storage(void)
{
	return !gfp_has_io_fs(gfp_allowed_mask);
}

extern bool pm_wakeup_pending(void);
extern void pm_system_wakeup(void);
extern void pm_system_cancel_wakeup(void);
extern void pm_wakeup_clear(unsigned int irq_number);
extern void pm_system_irq_wakeup(unsigned int irq_number);
extern unsigned int pm_wakeup_irq(void);
extern bool pm_get_wakeup_count(unsigned int *count, bool block);
extern bool pm_save_wakeup_count(unsigned int count);
extern void pm_wakep_autosleep_enabled(bool set);
extern void pm_print_active_wakeup_sources(void);

extern unsigned int lock_system_sleep(void);
extern void unlock_system_sleep(unsigned int);

extern bool pm_sleep_transition_in_progress(void);
bool pm_hibernate_is_recovering(void);

#else /* !CONFIG_PM_SLEEP */

static inline int register_pm_notifier(struct notifier_block *nb)
{
	return 0;
}

static inline int unregister_pm_notifier(struct notifier_block *nb)
{
	return 0;
}

static inline void pm_report_hw_sleep_time(u64 t) {};
static inline void pm_report_max_hw_sleep(u64 t) {};

static inline void pm_restrict_gfp_mask(void) {}
static inline void pm_restore_gfp_mask(void) {}

static inline void ksys_sync_helper(void) {}

#define pm_notifier(fn, pri)	do { (void)(fn); } while (0)

static inline bool pm_suspended_storage(void) { return false; }
static inline bool pm_wakeup_pending(void) { return false; }
static inline void pm_system_wakeup(void) {}
static inline void pm_wakeup_clear(bool reset) {}
static inline void pm_system_irq_wakeup(unsigned int irq_number) {}

static inline unsigned int lock_system_sleep(void) { return 0; }
static inline void unlock_system_sleep(unsigned int flags) {}

static inline bool pm_sleep_transition_in_progress(void) { return false; }
static inline bool pm_hibernate_is_recovering(void) { return false; }

#endif /* !CONFIG_PM_SLEEP */

#ifdef CONFIG_PM_SLEEP_DEBUG
extern bool pm_print_times_enabled;
extern bool pm_debug_messages_on;
extern bool pm_debug_messages_should_print(void);
static inline int pm_dyn_debug_messages_on(void)
{
#ifdef CONFIG_DYNAMIC_DEBUG
	return 1;
#else
	return 0;
#endif
}
#ifndef pr_fmt
#define pr_fmt(fmt) "PM: " fmt
#endif
#define __pm_pr_dbg(fmt, ...)					\
	do {							\
		if (pm_debug_messages_should_print())		\
			printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__);	\
		else if (pm_dyn_debug_messages_on())		\
			pr_debug(fmt, ##__VA_ARGS__);	\
	} while (0)
#define __pm_deferred_pr_dbg(fmt, ...)				\
	do {							\
		if (pm_debug_messages_should_print())		\
			printk_deferred(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__);	\
	} while (0)
#else
#define pm_print_times_enabled	(false)
#define pm_debug_messages_on	(false)

#include <linux/printk.h>

#define __pm_pr_dbg(fmt, ...) \
	no_printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#define __pm_deferred_pr_dbg(fmt, ...) \
	no_printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)
#endif

/**
 * pm_pr_dbg - print pm sleep debug messages
 *
 * If pm_debug_messages_on is enabled and the woke system is entering/leaving
 *      suspend, print message.
 * If pm_debug_messages_on is disabled and CONFIG_DYNAMIC_DEBUG is enabled,
 *	print message only from instances explicitly enabled on dynamic debug's
 *	control.
 * If pm_debug_messages_on is disabled and CONFIG_DYNAMIC_DEBUG is disabled,
 *	don't print message.
 */
#define pm_pr_dbg(fmt, ...) \
	__pm_pr_dbg(fmt, ##__VA_ARGS__)

#define pm_deferred_pr_dbg(fmt, ...) \
	__pm_deferred_pr_dbg(fmt, ##__VA_ARGS__)

#ifdef CONFIG_PM_AUTOSLEEP

/* kernel/power/autosleep.c */
void queue_up_suspend_work(void);

#else /* !CONFIG_PM_AUTOSLEEP */

static inline void queue_up_suspend_work(void) {}

#endif /* !CONFIG_PM_AUTOSLEEP */

enum suspend_stat_step {
	SUSPEND_WORKING = 0,
	SUSPEND_FREEZE,
	SUSPEND_PREPARE,
	SUSPEND_SUSPEND,
	SUSPEND_SUSPEND_LATE,
	SUSPEND_SUSPEND_NOIRQ,
	SUSPEND_RESUME_NOIRQ,
	SUSPEND_RESUME_EARLY,
	SUSPEND_RESUME
};

void dpm_save_failed_dev(const char *name);
void dpm_save_failed_step(enum suspend_stat_step step);

#endif /* _LINUX_SUSPEND_H */
