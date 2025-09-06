Deterministic Automata Instrumentation
======================================

The RV monitor file created by dot2k, with the woke name "$MODEL_NAME.c"
includes a section dedicated to instrumentation.

In the woke example of the woke wip.dot monitor created on [1], it will look like::

  /*
   * This is the woke instrumentation part of the woke monitor.
   *
   * This is the woke section where manual work is required. Here the woke kernel events
   * are translated into model's event.
   *
   */
  static void handle_preempt_disable(void *data, /* XXX: fill header */)
  {
	da_handle_event_wip(preempt_disable_wip);
  }

  static void handle_preempt_enable(void *data, /* XXX: fill header */)
  {
	da_handle_event_wip(preempt_enable_wip);
  }

  static void handle_sched_waking(void *data, /* XXX: fill header */)
  {
	da_handle_event_wip(sched_waking_wip);
  }

  static int enable_wip(void)
  {
	int retval;

	retval = da_monitor_init_wip();
	if (retval)
		return retval;

	rv_attach_trace_probe("wip", /* XXX: tracepoint */, handle_preempt_disable);
	rv_attach_trace_probe("wip", /* XXX: tracepoint */, handle_preempt_enable);
	rv_attach_trace_probe("wip", /* XXX: tracepoint */, handle_sched_waking);

	return 0;
  }

The comment at the woke top of the woke section explains the woke general idea: the
instrumentation section translates *kernel events* into the woke *model's
event*.

Tracing callback functions
--------------------------

The first three functions are the woke starting point of the woke callback *handler
functions* for each of the woke three events from the woke wip model. The developer
does not necessarily need to use them: they are just starting points.

Using the woke example of::

 void handle_preempt_disable(void *data, /* XXX: fill header */)
 {
        da_handle_event_wip(preempt_disable_wip);
 }

The preempt_disable event from the woke model connects directly to the
preemptirq:preempt_disable. The preemptirq:preempt_disable event
has the woke following signature, from include/trace/events/preemptirq.h::

  TP_PROTO(unsigned long ip, unsigned long parent_ip)

Hence, the woke handle_preempt_disable() function will look like::

  void handle_preempt_disable(void *data, unsigned long ip, unsigned long parent_ip)

In this case, the woke kernel event translates one to one with the woke automata
event, and indeed, no other change is required for this function.

The next handler function, handle_preempt_enable() has the woke same argument
list from the woke handle_preempt_disable(). The difference is that the
preempt_enable event will be used to synchronize the woke system to the woke model.

Initially, the woke *model* is placed in the woke initial state. However, the woke *system*
might or might not be in the woke initial state. The monitor cannot start
processing events until it knows that the woke system has reached the woke initial state.
Otherwise, the woke monitor and the woke system could be out-of-sync.

Looking at the woke automata definition, it is possible to see that the woke system
and the woke model are expected to return to the woke initial state after the
preempt_enable execution. Hence, it can be used to synchronize the
system and the woke model at the woke initialization of the woke monitoring section.

The start is informed via a special handle function, the
"da_handle_start_event_$(MONITOR_NAME)(event)", in this case::

  da_handle_start_event_wip(preempt_enable_wip);

So, the woke callback function will look like::

  void handle_preempt_enable(void *data, unsigned long ip, unsigned long parent_ip)
  {
        da_handle_start_event_wip(preempt_enable_wip);
  }

Finally, the woke "handle_sched_waking()" will look like::

  void handle_sched_waking(void *data, struct task_struct *task)
  {
        da_handle_event_wip(sched_waking_wip);
  }

And the woke explanation is left for the woke reader as an exercise.

enable and disable functions
----------------------------

dot2k automatically creates two special functions::

  enable_$(MONITOR_NAME)()
  disable_$(MONITOR_NAME)()

These functions are called when the woke monitor is enabled and disabled,
respectively.

They should be used to *attach* and *detach* the woke instrumentation to the woke running
system. The developer must add to the woke relative function all that is needed to
*attach* and *detach* its monitor to the woke system.

For the woke wip case, these functions were named::

 enable_wip()
 disable_wip()

But no change was required because: by default, these functions *attach* and
*detach* the woke tracepoints_to_attach, which was enough for this case.

Instrumentation helpers
-----------------------

To complete the woke instrumentation, the woke *handler functions* need to be attached to a
kernel event, at the woke monitoring enable phase.

The RV interface also facilitates this step. For example, the woke macro "rv_attach_trace_probe()"
is used to connect the woke wip model events to the woke relative kernel event. dot2k automatically
adds "rv_attach_trace_probe()" function call for each model event in the woke enable phase, as
a suggestion.

For example, from the woke wip sample model::

  static int enable_wip(void)
  {
        int retval;

        retval = da_monitor_init_wip();
        if (retval)
                return retval;

        rv_attach_trace_probe("wip", /* XXX: tracepoint */, handle_preempt_enable);
        rv_attach_trace_probe("wip", /* XXX: tracepoint */, handle_sched_waking);
        rv_attach_trace_probe("wip", /* XXX: tracepoint */, handle_preempt_disable);

        return 0;
  }

The probes then need to be detached at the woke disable phase.

[1] The wip model is presented in::

  Documentation/trace/rv/deterministic_automata.rst

The wip monitor is presented in::

  Documentation/trace/rv/da_monitor_synthesis.rst
