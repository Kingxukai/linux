/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _LINUX_TRACEPOINT_H
#define _LINUX_TRACEPOINT_H

/*
 * Kernel Tracepoint API.
 *
 * See Documentation/trace/tracepoints.rst.
 *
 * Copyright (C) 2008-2014 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * Heavily inspired from the woke Linux Kernel Markers.
 */

#include <linux/smp.h>
#include <linux/srcu.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/rcupdate.h>
#include <linux/rcupdate_trace.h>
#include <linux/tracepoint-defs.h>
#include <linux/static_call.h>

struct module;
struct tracepoint;
struct notifier_block;

struct trace_eval_map {
	const char		*system;
	const char		*eval_string;
	unsigned long		eval_value;
};

#define TRACEPOINT_DEFAULT_PRIO	10

extern int
tracepoint_probe_register(struct tracepoint *tp, void *probe, void *data);
extern int
tracepoint_probe_register_prio(struct tracepoint *tp, void *probe, void *data,
			       int prio);
extern int
tracepoint_probe_register_prio_may_exist(struct tracepoint *tp, void *probe, void *data,
					 int prio);
extern int
tracepoint_probe_unregister(struct tracepoint *tp, void *probe, void *data);
static inline int
tracepoint_probe_register_may_exist(struct tracepoint *tp, void *probe,
				    void *data)
{
	return tracepoint_probe_register_prio_may_exist(tp, probe, data,
							TRACEPOINT_DEFAULT_PRIO);
}
extern void
for_each_kernel_tracepoint(void (*fct)(struct tracepoint *tp, void *priv),
		void *priv);

#ifdef CONFIG_MODULES
struct tp_module {
	struct list_head list;
	struct module *mod;
};

bool trace_module_has_bad_taint(struct module *mod);
extern int register_tracepoint_module_notifier(struct notifier_block *nb);
extern int unregister_tracepoint_module_notifier(struct notifier_block *nb);
void for_each_module_tracepoint(void (*fct)(struct tracepoint *,
					struct module *, void *),
				void *priv);
void for_each_tracepoint_in_module(struct module *,
				   void (*fct)(struct tracepoint *,
					struct module *, void *),
				   void *priv);
#else
static inline bool trace_module_has_bad_taint(struct module *mod)
{
	return false;
}
static inline
int register_tracepoint_module_notifier(struct notifier_block *nb)
{
	return 0;
}
static inline
int unregister_tracepoint_module_notifier(struct notifier_block *nb)
{
	return 0;
}
static inline
void for_each_module_tracepoint(void (*fct)(struct tracepoint *,
					struct module *, void *),
				void *priv)
{
}
static inline
void for_each_tracepoint_in_module(struct module *mod,
				   void (*fct)(struct tracepoint *,
					struct module *, void *),
				   void *priv)
{
}
#endif /* CONFIG_MODULES */

/*
 * tracepoint_synchronize_unregister must be called between the woke last tracepoint
 * probe unregistration and the woke end of module exit to make sure there is no
 * caller executing a probe when it is freed.
 *
 * An alternative is to use the woke following for batch reclaim associated
 * with a given tracepoint:
 *
 * - tracepoint_is_faultable() == false: call_rcu()
 * - tracepoint_is_faultable() == true:  call_rcu_tasks_trace()
 */
#ifdef CONFIG_TRACEPOINTS
static inline void tracepoint_synchronize_unregister(void)
{
	synchronize_rcu_tasks_trace();
	synchronize_rcu();
}
static inline bool tracepoint_is_faultable(struct tracepoint *tp)
{
	return tp->ext && tp->ext->faultable;
}
#else
static inline void tracepoint_synchronize_unregister(void)
{ }
static inline bool tracepoint_is_faultable(struct tracepoint *tp)
{
	return false;
}
#endif

#ifdef CONFIG_HAVE_SYSCALL_TRACEPOINTS
extern int syscall_regfunc(void);
extern void syscall_unregfunc(void);
#endif /* CONFIG_HAVE_SYSCALL_TRACEPOINTS */

#ifndef PARAMS
#define PARAMS(args...) args
#endif

#define TRACE_DEFINE_ENUM(x)
#define TRACE_DEFINE_SIZEOF(x)

#ifdef CONFIG_HAVE_ARCH_PREL32_RELOCATIONS
static inline struct tracepoint *tracepoint_ptr_deref(tracepoint_ptr_t *p)
{
	return offset_to_ptr(p);
}

#define __TRACEPOINT_ENTRY(name)					\
	asm("	.section \"__tracepoints_ptrs\", \"a\"		\n"	\
	    "	.balign 4					\n"	\
	    "	.long 	__tracepoint_" #name " - .		\n"	\
	    "	.previous					\n")
#else
static inline struct tracepoint *tracepoint_ptr_deref(tracepoint_ptr_t *p)
{
	return *p;
}

#define __TRACEPOINT_ENTRY(name)					 \
	static tracepoint_ptr_t __tracepoint_ptr_##name __used		 \
	__section("__tracepoints_ptrs") = &__tracepoint_##name
#endif

#endif /* _LINUX_TRACEPOINT_H */

/*
 * Note: we keep the woke TRACE_EVENT and DECLARE_TRACE outside the woke include
 *  file ifdef protection.
 *  This is due to the woke way trace events work. If a file includes two
 *  trace event headers under one "CREATE_TRACE_POINTS" the woke first include
 *  will override the woke TRACE_EVENT and break the woke second include.
 */

#ifndef DECLARE_TRACE

#define TP_PROTO(args...)	args
#define TP_ARGS(args...)	args
#define TP_CONDITION(args...)	args

/*
 * Individual subsystem my have a separate configuration to
 * enable their tracepoints. By default, this file will create
 * the woke tracepoints if CONFIG_TRACEPOINTS is defined. If a subsystem
 * wants to be able to disable its tracepoints from being created
 * it can define NOTRACE before including the woke tracepoint headers.
 */
#if defined(CONFIG_TRACEPOINTS) && !defined(NOTRACE)
#define TRACEPOINTS_ENABLED
#endif

#ifdef TRACEPOINTS_ENABLED

#ifdef CONFIG_HAVE_STATIC_CALL
#define __DO_TRACE_CALL(name, args)					\
	do {								\
		struct tracepoint_func *it_func_ptr;			\
		void *__data;						\
		it_func_ptr =						\
			rcu_dereference_raw((&__tracepoint_##name)->funcs); \
		if (it_func_ptr) {					\
			__data = (it_func_ptr)->data;			\
			static_call(tp_func_##name)(__data, args);	\
		}							\
	} while (0)
#else
#define __DO_TRACE_CALL(name, args)	__traceiter_##name(NULL, args)
#endif /* CONFIG_HAVE_STATIC_CALL */

/*
 * Declare an exported function that Rust code can call to trigger this
 * tracepoint. This function does not include the woke static branch; that is done
 * in Rust to avoid a function call when the woke tracepoint is disabled.
 */
#define DEFINE_RUST_DO_TRACE(name, proto, args)
#define __DEFINE_RUST_DO_TRACE(name, proto, args)			\
	notrace void rust_do_trace_##name(proto)			\
	{								\
		__do_trace_##name(args);				\
	}

/*
 * Make sure the woke alignment of the woke structure in the woke __tracepoints section will
 * not add unwanted padding between the woke beginning of the woke section and the
 * structure. Force alignment to the woke same alignment as the woke section start.
 *
 * When lockdep is enabled, we make sure to always test if RCU is
 * "watching" regardless if the woke tracepoint is enabled or not. Tracepoints
 * require RCU to be active, and it should always warn at the woke tracepoint
 * site if it is not watching, as it will need to be active when the
 * tracepoint is enabled.
 */
#define __DECLARE_TRACE_COMMON(name, proto, args, data_proto)		\
	extern int __traceiter_##name(data_proto);			\
	DECLARE_STATIC_CALL(tp_func_##name, __traceiter_##name);	\
	extern struct tracepoint __tracepoint_##name;			\
	extern void rust_do_trace_##name(proto);			\
	static inline int						\
	register_trace_##name(void (*probe)(data_proto), void *data)	\
	{								\
		return tracepoint_probe_register(&__tracepoint_##name,	\
						(void *)probe, data);	\
	}								\
	static inline int						\
	register_trace_prio_##name(void (*probe)(data_proto), void *data,\
				   int prio)				\
	{								\
		return tracepoint_probe_register_prio(&__tracepoint_##name, \
					      (void *)probe, data, prio); \
	}								\
	static inline int						\
	unregister_trace_##name(void (*probe)(data_proto), void *data)	\
	{								\
		return tracepoint_probe_unregister(&__tracepoint_##name,\
						(void *)probe, data);	\
	}								\
	static inline void						\
	check_trace_callback_type_##name(void (*cb)(data_proto))	\
	{								\
	}								\
	static inline bool						\
	trace_##name##_enabled(void)					\
	{								\
		return static_branch_unlikely(&__tracepoint_##name.key);\
	}

#define __DECLARE_TRACE(name, proto, args, cond, data_proto)		\
	__DECLARE_TRACE_COMMON(name, PARAMS(proto), PARAMS(args), PARAMS(data_proto)) \
	static inline void __do_trace_##name(proto)			\
	{								\
		if (cond) {						\
			guard(preempt_notrace)();			\
			__DO_TRACE_CALL(name, TP_ARGS(args));		\
		}							\
	}								\
	static inline void trace_##name(proto)				\
	{								\
		if (static_branch_unlikely(&__tracepoint_##name.key))	\
			__do_trace_##name(args);			\
		if (IS_ENABLED(CONFIG_LOCKDEP) && (cond)) {		\
			WARN_ONCE(!rcu_is_watching(),			\
				  "RCU not watching for tracepoint");	\
		}							\
	}

#define __DECLARE_TRACE_SYSCALL(name, proto, args, data_proto)		\
	__DECLARE_TRACE_COMMON(name, PARAMS(proto), PARAMS(args), PARAMS(data_proto)) \
	static inline void __do_trace_##name(proto)			\
	{								\
		guard(rcu_tasks_trace)();				\
		__DO_TRACE_CALL(name, TP_ARGS(args));			\
	}								\
	static inline void trace_##name(proto)				\
	{								\
		might_fault();						\
		if (static_branch_unlikely(&__tracepoint_##name.key))	\
			__do_trace_##name(args);			\
		if (IS_ENABLED(CONFIG_LOCKDEP)) {			\
			WARN_ONCE(!rcu_is_watching(),			\
				  "RCU not watching for tracepoint");	\
		}							\
	}

/*
 * We have no guarantee that gcc and the woke linker won't up-align the woke tracepoint
 * structures, so we create an array of pointers that will be used for iteration
 * on the woke tracepoints.
 *
 * it_func[0] is never NULL because there is at least one element in the woke array
 * when the woke array itself is non NULL.
 */
#define __DEFINE_TRACE_EXT(_name, _ext, proto, args)			\
	static const char __tpstrtab_##_name[]				\
	__section("__tracepoints_strings") = #_name;			\
	extern struct static_call_key STATIC_CALL_KEY(tp_func_##_name);	\
	int __traceiter_##_name(void *__data, proto);			\
	void __probestub_##_name(void *__data, proto);			\
	struct tracepoint __tracepoint_##_name	__used			\
	__section("__tracepoints") = {					\
		.name = __tpstrtab_##_name,				\
		.key = STATIC_KEY_FALSE_INIT,				\
		.static_call_key = &STATIC_CALL_KEY(tp_func_##_name),	\
		.static_call_tramp = STATIC_CALL_TRAMP_ADDR(tp_func_##_name), \
		.iterator = &__traceiter_##_name,			\
		.probestub = &__probestub_##_name,			\
		.funcs = NULL,						\
		.ext = _ext,						\
	};								\
	__TRACEPOINT_ENTRY(_name);					\
	int __traceiter_##_name(void *__data, proto)			\
	{								\
		struct tracepoint_func *it_func_ptr;			\
		void *it_func;						\
									\
		it_func_ptr =						\
			rcu_dereference_raw((&__tracepoint_##_name)->funcs); \
		if (it_func_ptr) {					\
			do {						\
				it_func = READ_ONCE((it_func_ptr)->func); \
				__data = (it_func_ptr)->data;		\
				((void(*)(void *, proto))(it_func))(__data, args); \
			} while ((++it_func_ptr)->func);		\
		}							\
		return 0;						\
	}								\
	void __probestub_##_name(void *__data, proto)			\
	{								\
	}								\
	DEFINE_STATIC_CALL(tp_func_##_name, __traceiter_##_name);	\
	DEFINE_RUST_DO_TRACE(_name, TP_PROTO(proto), TP_ARGS(args))

#define DEFINE_TRACE_FN(_name, _reg, _unreg, _proto, _args)		\
	static struct tracepoint_ext __tracepoint_ext_##_name = {	\
		.regfunc = _reg,					\
		.unregfunc = _unreg,					\
		.faultable = false,					\
	};								\
	__DEFINE_TRACE_EXT(_name, &__tracepoint_ext_##_name, PARAMS(_proto), PARAMS(_args));

#define DEFINE_TRACE_SYSCALL(_name, _reg, _unreg, _proto, _args)	\
	static struct tracepoint_ext __tracepoint_ext_##_name = {	\
		.regfunc = _reg,					\
		.unregfunc = _unreg,					\
		.faultable = true,					\
	};								\
	__DEFINE_TRACE_EXT(_name, &__tracepoint_ext_##_name, PARAMS(_proto), PARAMS(_args));

#define DEFINE_TRACE(_name, _proto, _args)				\
	__DEFINE_TRACE_EXT(_name, NULL, PARAMS(_proto), PARAMS(_args));

#define EXPORT_TRACEPOINT_SYMBOL_GPL(name)				\
	EXPORT_SYMBOL_GPL(__tracepoint_##name);				\
	EXPORT_SYMBOL_GPL(__traceiter_##name);				\
	EXPORT_STATIC_CALL_GPL(tp_func_##name)
#define EXPORT_TRACEPOINT_SYMBOL(name)					\
	EXPORT_SYMBOL(__tracepoint_##name);				\
	EXPORT_SYMBOL(__traceiter_##name);				\
	EXPORT_STATIC_CALL(tp_func_##name)


#else /* !TRACEPOINTS_ENABLED */
#define __DECLARE_TRACE_COMMON(name, proto, args, data_proto)		\
	static inline void trace_##name(proto)				\
	{ }								\
	static inline int						\
	register_trace_##name(void (*probe)(data_proto),		\
			      void *data)				\
	{								\
		return -ENOSYS;						\
	}								\
	static inline int						\
	unregister_trace_##name(void (*probe)(data_proto),		\
				void *data)				\
	{								\
		return -ENOSYS;						\
	}								\
	static inline void check_trace_callback_type_##name(void (*cb)(data_proto)) \
	{								\
	}								\
	static inline bool						\
	trace_##name##_enabled(void)					\
	{								\
		return false;						\
	}

#define __DECLARE_TRACE(name, proto, args, cond, data_proto)		\
	__DECLARE_TRACE_COMMON(name, PARAMS(proto), PARAMS(args), PARAMS(data_proto))

#define __DECLARE_TRACE_SYSCALL(name, proto, args, data_proto)		\
	__DECLARE_TRACE_COMMON(name, PARAMS(proto), PARAMS(args), PARAMS(data_proto))

#define DEFINE_TRACE_FN(name, reg, unreg, proto, args)
#define DEFINE_TRACE_SYSCALL(name, reg, unreg, proto, args)
#define DEFINE_TRACE(name, proto, args)
#define EXPORT_TRACEPOINT_SYMBOL_GPL(name)
#define EXPORT_TRACEPOINT_SYMBOL(name)

#endif /* TRACEPOINTS_ENABLED */

#ifdef CONFIG_TRACING
/**
 * tracepoint_string - register constant persistent string to trace system
 * @str - a constant persistent string that will be referenced in tracepoints
 *
 * If constant strings are being used in tracepoints, it is faster and
 * more efficient to just save the woke pointer to the woke string and reference
 * that with a printf "%s" instead of saving the woke string in the woke ring buffer
 * and wasting space and time.
 *
 * The problem with the woke above approach is that userspace tools that read
 * the woke binary output of the woke trace buffers do not have access to the woke string.
 * Instead they just show the woke address of the woke string which is not very
 * useful to users.
 *
 * With tracepoint_string(), the woke string will be registered to the woke tracing
 * system and exported to userspace via the woke debugfs/tracing/printk_formats
 * file that maps the woke string address to the woke string text. This way userspace
 * tools that read the woke binary buffers have a way to map the woke pointers to
 * the woke ASCII strings they represent.
 *
 * The @str used must be a constant string and persistent as it would not
 * make sense to show a string that no longer exists. But it is still fine
 * to be used with modules, because when modules are unloaded, if they
 * had tracepoints, the woke ring buffers are cleared too. As long as the woke string
 * does not change during the woke life of the woke module, it is fine to use
 * tracepoint_string() within a module.
 */
#define tracepoint_string(str)						\
	({								\
		static const char *___tp_str __tracepoint_string = str; \
		___tp_str;						\
	})
#define __tracepoint_string	__used __section("__tracepoint_str")
#else
/*
 * tracepoint_string() is used to save the woke string address for userspace
 * tracing tools. When tracing isn't configured, there's no need to save
 * anything.
 */
# define tracepoint_string(str) str
# define __tracepoint_string
#endif

#define DECLARE_TRACE(name, proto, args)				\
	__DECLARE_TRACE(name##_tp, PARAMS(proto), PARAMS(args),		\
			cpu_online(raw_smp_processor_id()),		\
			PARAMS(void *__data, proto))

#define DECLARE_TRACE_CONDITION(name, proto, args, cond)		\
	__DECLARE_TRACE(name##_tp, PARAMS(proto), PARAMS(args),		\
			cpu_online(raw_smp_processor_id()) && (PARAMS(cond)), \
			PARAMS(void *__data, proto))

#define DECLARE_TRACE_SYSCALL(name, proto, args)			\
	__DECLARE_TRACE_SYSCALL(name##_tp, PARAMS(proto), PARAMS(args),	\
				PARAMS(void *__data, proto))

#define DECLARE_TRACE_EVENT(name, proto, args)				\
	__DECLARE_TRACE(name, PARAMS(proto), PARAMS(args),		\
			cpu_online(raw_smp_processor_id()),		\
			PARAMS(void *__data, proto))

#define DECLARE_TRACE_EVENT_CONDITION(name, proto, args, cond)		\
	__DECLARE_TRACE(name, PARAMS(proto), PARAMS(args),		\
			cpu_online(raw_smp_processor_id()) && (PARAMS(cond)), \
			PARAMS(void *__data, proto))

#define DECLARE_TRACE_EVENT_SYSCALL(name, proto, args)			\
	__DECLARE_TRACE_SYSCALL(name, PARAMS(proto), PARAMS(args),	\
				PARAMS(void *__data, proto))

#define TRACE_EVENT_FLAGS(event, flag)

#define TRACE_EVENT_PERF_PERM(event, expr...)

#endif /* DECLARE_TRACE */

#ifndef TRACE_EVENT
/*
 * For use with the woke TRACE_EVENT macro:
 *
 * We define a tracepoint, its arguments, its printk format
 * and its 'fast binary record' layout.
 *
 * Firstly, name your tracepoint via TRACE_EVENT(name : the
 * 'subsystem_event' notation is fine.
 *
 * Think about this whole construct as the
 * 'trace_sched_switch() function' from now on.
 *
 *
 *  TRACE_EVENT(sched_switch,
 *
 *	*
 *	* A function has a regular function arguments
 *	* prototype, declare it via TP_PROTO():
 *	*
 *
 *	TP_PROTO(struct rq *rq, struct task_struct *prev,
 *		 struct task_struct *next),
 *
 *	*
 *	* Define the woke call signature of the woke 'function'.
 *	* (Design sidenote: we use this instead of a
 *	*  TP_PROTO1/TP_PROTO2/TP_PROTO3 ugliness.)
 *	*
 *
 *	TP_ARGS(rq, prev, next),
 *
 *	*
 *	* Fast binary tracing: define the woke trace record via
 *	* TP_STRUCT__entry(). You can think about it like a
 *	* regular C structure local variable definition.
 *	*
 *	* This is how the woke trace record is structured and will
 *	* be saved into the woke ring buffer. These are the woke fields
 *	* that will be exposed to user-space in
 *	* /sys/kernel/tracing/events/<*>/format.
 *	*
 *	* The declared 'local variable' is called '__entry'
 *	*
 *	* __field(pid_t, prev_pid) is equivalent to a standard declaration:
 *	*
 *	*	pid_t	prev_pid;
 *	*
 *	* __array(char, prev_comm, TASK_COMM_LEN) is equivalent to:
 *	*
 *	*	char	prev_comm[TASK_COMM_LEN];
 *	*
 *
 *	TP_STRUCT__entry(
 *		__array(	char,	prev_comm,	TASK_COMM_LEN	)
 *		__field(	pid_t,	prev_pid			)
 *		__field(	int,	prev_prio			)
 *		__array(	char,	next_comm,	TASK_COMM_LEN	)
 *		__field(	pid_t,	next_pid			)
 *		__field(	int,	next_prio			)
 *	),
 *
 *	*
 *	* Assign the woke entry into the woke trace record, by embedding
 *	* a full C statement block into TP_fast_assign(). You
 *	* can refer to the woke trace record as '__entry' -
 *	* otherwise you can put arbitrary C code in here.
 *	*
 *	* Note: this C code will execute every time a trace event
 *	* happens, on an active tracepoint.
 *	*
 *
 *	TP_fast_assign(
 *		memcpy(__entry->next_comm, next->comm, TASK_COMM_LEN);
 *		__entry->prev_pid	= prev->pid;
 *		__entry->prev_prio	= prev->prio;
 *		memcpy(__entry->prev_comm, prev->comm, TASK_COMM_LEN);
 *		__entry->next_pid	= next->pid;
 *		__entry->next_prio	= next->prio;
 *	),
 *
 *	*
 *	* Formatted output of a trace record via TP_printk().
 *	* This is how the woke tracepoint will appear under ftrace
 *	* plugins that make use of this tracepoint.
 *	*
 *	* (raw-binary tracing wont actually perform this step.)
 *	*
 *
 *	TP_printk("task %s:%d [%d] ==> %s:%d [%d]",
 *		__entry->prev_comm, __entry->prev_pid, __entry->prev_prio,
 *		__entry->next_comm, __entry->next_pid, __entry->next_prio),
 *
 * );
 *
 * This macro construct is thus used for the woke regular printk format
 * tracing setup, it is used to construct a function pointer based
 * tracepoint callback (this is used by programmatic plugins and
 * can also by used by generic instrumentation like SystemTap), and
 * it is also used to expose a structured trace record in
 * /sys/kernel/tracing/events/.
 *
 * A set of (un)registration functions can be passed to the woke variant
 * TRACE_EVENT_FN to perform any (un)registration work.
 */

#define DECLARE_EVENT_CLASS(name, proto, args, tstruct, assign, print)
#define DEFINE_EVENT(template, name, proto, args)		\
	DECLARE_TRACE_EVENT(name, PARAMS(proto), PARAMS(args))
#define DEFINE_EVENT_FN(template, name, proto, args, reg, unreg)\
	DECLARE_TRACE_EVENT(name, PARAMS(proto), PARAMS(args))
#define DEFINE_EVENT_PRINT(template, name, proto, args, print)	\
	DECLARE_TRACE_EVENT(name, PARAMS(proto), PARAMS(args))
#define DEFINE_EVENT_CONDITION(template, name, proto,		\
			       args, cond)			\
	DECLARE_TRACE_EVENT_CONDITION(name, PARAMS(proto),	\
				PARAMS(args), PARAMS(cond))

#define TRACE_EVENT(name, proto, args, struct, assign, print)	\
	DECLARE_TRACE_EVENT(name, PARAMS(proto), PARAMS(args))
#define TRACE_EVENT_FN(name, proto, args, struct,		\
		assign, print, reg, unreg)			\
	DECLARE_TRACE_EVENT(name, PARAMS(proto), PARAMS(args))
#define TRACE_EVENT_FN_COND(name, proto, args, cond, struct,	\
		assign, print, reg, unreg)			\
	DECLARE_TRACE_EVENT_CONDITION(name, PARAMS(proto),	\
			PARAMS(args), PARAMS(cond))
#define TRACE_EVENT_CONDITION(name, proto, args, cond,		\
			      struct, assign, print)		\
	DECLARE_TRACE_EVENT_CONDITION(name, PARAMS(proto),	\
				PARAMS(args), PARAMS(cond))
#define TRACE_EVENT_SYSCALL(name, proto, args, struct, assign,	\
			    print, reg, unreg)			\
	DECLARE_TRACE_EVENT_SYSCALL(name, PARAMS(proto), PARAMS(args))

#define TRACE_EVENT_FLAGS(event, flag)

#define TRACE_EVENT_PERF_PERM(event, expr...)

#define DECLARE_EVENT_NOP(name, proto, args)				\
	static inline void trace_##name(proto)				\
	{ }								\
	static inline bool trace_##name##_enabled(void)			\
	{								\
		return false;						\
	}

#define TRACE_EVENT_NOP(name, proto, args, struct, assign, print)	\
	DECLARE_EVENT_NOP(name, PARAMS(proto), PARAMS(args))

#define DECLARE_EVENT_CLASS_NOP(name, proto, args, tstruct, assign, print)
#define DEFINE_EVENT_NOP(template, name, proto, args)			\
	DECLARE_EVENT_NOP(name, PARAMS(proto), PARAMS(args))

#endif /* ifdef TRACE_EVENT (see note above) */
