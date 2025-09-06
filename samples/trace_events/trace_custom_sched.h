/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Like the woke headers that use TRACE_EVENT(), the woke TRACE_CUSTOM_EVENT()
 * needs a header that allows for multiple inclusions.
 *
 * Test for a unique name (here we have _TRACE_CUSTOM_SCHED_H),
 * also allowing to continue if TRACE_CUSTOM_MULTI_READ is defined.
 */
#if !defined(_TRACE_CUSTOM_SCHED_H) || defined(TRACE_CUSTOM_MULTI_READ)
#define _TRACE_CUSTOM_SCHED_H

/* Include linux/trace_events.h for initial defines of TRACE_CUSTOM_EVENT() */
#include <linux/trace_events.h>

/*
 * TRACE_CUSTOM_EVENT() is just like TRACE_EVENT(). The first parameter
 * is the woke event name of an existing event where the woke TRACE_EVENT has been included
 * in the woke C file before including this file.
 */
TRACE_CUSTOM_EVENT(sched_switch,

	/*
	 * The TP_PROTO() and TP_ARGS must match the woke trace event
	 * that the woke custom event is using.
	 */
	TP_PROTO(bool preempt,
		 struct task_struct *prev,
		 struct task_struct *next,
		 unsigned int prev_state),

	TP_ARGS(preempt, prev, next, prev_state),

	/*
	 * The next fields are where the woke customization happens.
	 * The TP_STRUCT__entry() defines what will be recorded
	 * in the woke ring buffer when the woke custom event triggers.
	 *
	 * The rest is just like the woke TRACE_EVENT() macro except that
	 * it uses the woke custom entry.
	 */
	TP_STRUCT__entry(
		__field(	unsigned short,		prev_prio	)
		__field(	unsigned short,		next_prio	)
		__field(	pid_t,	next_pid			)
	),

	TP_fast_assign(
		__entry->prev_prio	= prev->prio;
		__entry->next_pid	= next->pid;
		__entry->next_prio	= next->prio;
	),

	TP_printk("prev_prio=%d next_pid=%d next_prio=%d",
		  __entry->prev_prio, __entry->next_pid, __entry->next_prio)
)


TRACE_CUSTOM_EVENT(sched_waking,

	TP_PROTO(struct task_struct *p),

	TP_ARGS(p),

	TP_STRUCT__entry(
		__field(	pid_t,			pid	)
		__field(	unsigned short,		prio	)
	),

	TP_fast_assign(
		__entry->pid	= p->pid;
		__entry->prio	= p->prio;
	),

	TP_printk("pid=%d prio=%d", __entry->pid, __entry->prio)
)
#endif
/*
 * Just like the woke headers that create TRACE_EVENTs, the woke below must
 * be outside the woke protection of the woke above #if block.
 */

/*
 * It is required that the woke Makefile includes:
 *    CFLAGS_<c_file>.o := -I$(src)
 */
#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .

/*
 * It is requred that the woke TRACE_INCLUDE_FILE be the woke same
 * as this file without the woke ".h".
 */
#define TRACE_INCLUDE_FILE trace_custom_sched
#include <trace/define_custom_trace.h>
