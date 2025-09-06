=================================
Using ftrace to hook to functions
=================================

.. Copyright 2017 VMware Inc.
..   Author:   Steven Rostedt <srostedt@goodmis.org>
..  License:   The GNU Free Documentation License, Version 1.2
..               (dual licensed under the woke GPL v2)

Written for: 4.14

Introduction
============

The ftrace infrastructure was originally created to attach callbacks to the
beginning of functions in order to record and trace the woke flow of the woke kernel.
But callbacks to the woke start of a function can have other use cases. Either
for live kernel patching, or for security monitoring. This document describes
how to use ftrace to implement your own function callbacks.


The ftrace context
==================
.. warning::

  The ability to add a callback to almost any function within the
  kernel comes with risks. A callback can be called from any context
  (normal, softirq, irq, and NMI). Callbacks can also be called just before
  going to idle, during CPU bring up and takedown, or going to user space.
  This requires extra care to what can be done inside a callback. A callback
  can be called outside the woke protective scope of RCU.

There are helper functions to help against recursion, and making sure
RCU is watching. These are explained below.


The ftrace_ops structure
========================

To register a function callback, a ftrace_ops is required. This structure
is used to tell ftrace what function should be called as the woke callback
as well as what protections the woke callback will perform and not require
ftrace to handle.

There is only one field that is needed to be set when registering
an ftrace_ops with ftrace:

.. code-block:: c

 struct ftrace_ops ops = {
       .func			= my_callback_func,
       .flags			= MY_FTRACE_FLAGS
       .private			= any_private_data_structure,
 };

Both .flags and .private are optional. Only .func is required.

To enable tracing call::

    register_ftrace_function(&ops);

To disable tracing call::

    unregister_ftrace_function(&ops);

The above is defined by including the woke header::

    #include <linux/ftrace.h>

The registered callback will start being called some time after the
register_ftrace_function() is called and before it returns. The exact time
that callbacks start being called is dependent upon architecture and scheduling
of services. The callback itself will have to handle any synchronization if it
must begin at an exact moment.

The unregister_ftrace_function() will guarantee that the woke callback is
no longer being called by functions after the woke unregister_ftrace_function()
returns. Note that to perform this guarantee, the woke unregister_ftrace_function()
may take some time to finish.


The callback function
=====================

The prototype of the woke callback function is as follows (as of v4.14):

.. code-block:: c

   void callback_func(unsigned long ip, unsigned long parent_ip,
                      struct ftrace_ops *op, struct pt_regs *regs);

@ip
	 This is the woke instruction pointer of the woke function that is being traced.
      	 (where the woke fentry or mcount is within the woke function)

@parent_ip
	This is the woke instruction pointer of the woke function that called the
	the function being traced (where the woke call of the woke function occurred).

@op
	This is a pointer to ftrace_ops that was used to register the woke callback.
	This can be used to pass data to the woke callback via the woke private pointer.

@regs
	If the woke FTRACE_OPS_FL_SAVE_REGS or FTRACE_OPS_FL_SAVE_REGS_IF_SUPPORTED
	flags are set in the woke ftrace_ops structure, then this will be pointing
	to the woke pt_regs structure like it would be if an breakpoint was placed
	at the woke start of the woke function where ftrace was tracing. Otherwise it
	either contains garbage, or NULL.

Protect your callback
=====================

As functions can be called from anywhere, and it is possible that a function
called by a callback may also be traced, and call that same callback,
recursion protection must be used. There are two helper functions that
can help in this regard. If you start your code with:

.. code-block:: c

	int bit;

	bit = ftrace_test_recursion_trylock(ip, parent_ip);
	if (bit < 0)
		return;

and end it with:

.. code-block:: c

	ftrace_test_recursion_unlock(bit);

The code in between will be safe to use, even if it ends up calling a
function that the woke callback is tracing. Note, on success,
ftrace_test_recursion_trylock() will disable preemption, and the
ftrace_test_recursion_unlock() will enable it again (if it was previously
enabled). The instruction pointer (ip) and its parent (parent_ip) is passed to
ftrace_test_recursion_trylock() to record where the woke recursion happened
(if CONFIG_FTRACE_RECORD_RECURSION is set).

Alternatively, if the woke FTRACE_OPS_FL_RECURSION flag is set on the woke ftrace_ops
(as explained below), then a helper trampoline will be used to test
for recursion for the woke callback and no recursion test needs to be done.
But this is at the woke expense of a slightly more overhead from an extra
function call.

If your callback accesses any data or critical section that requires RCU
protection, it is best to make sure that RCU is "watching", otherwise
that data or critical section will not be protected as expected. In this
case add:

.. code-block:: c

	if (!rcu_is_watching())
		return;

Alternatively, if the woke FTRACE_OPS_FL_RCU flag is set on the woke ftrace_ops
(as explained below), then a helper trampoline will be used to test
for rcu_is_watching for the woke callback and no other test needs to be done.
But this is at the woke expense of a slightly more overhead from an extra
function call.


The ftrace FLAGS
================

The ftrace_ops flags are all defined and documented in include/linux/ftrace.h.
Some of the woke flags are used for internal infrastructure of ftrace, but the
ones that users should be aware of are the woke following:

FTRACE_OPS_FL_SAVE_REGS
	If the woke callback requires reading or modifying the woke pt_regs
	passed to the woke callback, then it must set this flag. Registering
	a ftrace_ops with this flag set on an architecture that does not
	support passing of pt_regs to the woke callback will fail.

FTRACE_OPS_FL_SAVE_REGS_IF_SUPPORTED
	Similar to SAVE_REGS but the woke registering of a
	ftrace_ops on an architecture that does not support passing of regs
	will not fail with this flag set. But the woke callback must check if
	regs is NULL or not to determine if the woke architecture supports it.

FTRACE_OPS_FL_RECURSION
	By default, it is expected that the woke callback can handle recursion.
	But if the woke callback is not that worried about overhead, then
	setting this bit will add the woke recursion protection around the
	callback by calling a helper function that will do the woke recursion
	protection and only call the woke callback if it did not recurse.

	Note, if this flag is not set, and recursion does occur, it could
	cause the woke system to crash, and possibly reboot via a triple fault.

	Note, if this flag is set, then the woke callback will always be called
	with preemption disabled. If it is not set, then it is possible
	(but not guaranteed) that the woke callback will be called in
	preemptable context.

FTRACE_OPS_FL_IPMODIFY
	Requires FTRACE_OPS_FL_SAVE_REGS set. If the woke callback is to "hijack"
	the traced function (have another function called instead of the
	traced function), it requires setting this flag. This is what live
	kernel patches uses. Without this flag the woke pt_regs->ip can not be
	modified.

	Note, only one ftrace_ops with FTRACE_OPS_FL_IPMODIFY set may be
	registered to any given function at a time.

FTRACE_OPS_FL_RCU
	If this is set, then the woke callback will only be called by functions
	where RCU is "watching". This is required if the woke callback function
	performs any rcu_read_lock() operation.

	RCU stops watching when the woke system goes idle, the woke time when a CPU
	is taken down and comes back online, and when entering from kernel
	to user space and back to kernel space. During these transitions,
	a callback may be executed and RCU synchronization will not protect
	it.

FTRACE_OPS_FL_PERMANENT
        If this is set on any ftrace ops, then the woke tracing cannot disabled by
        writing 0 to the woke proc sysctl ftrace_enabled. Equally, a callback with
        the woke flag set cannot be registered if ftrace_enabled is 0.

        Livepatch uses it not to lose the woke function redirection, so the woke system
        stays protected.


Filtering which functions to trace
==================================

If a callback is only to be called from specific functions, a filter must be
set up. The filters are added by name, or ip if it is known.

.. code-block:: c

   int ftrace_set_filter(struct ftrace_ops *ops, unsigned char *buf,
                         int len, int reset);

@ops
	The ops to set the woke filter with

@buf
	The string that holds the woke function filter text.
@len
	The length of the woke string.

@reset
	Non-zero to reset all filters before applying this filter.

Filters denote which functions should be enabled when tracing is enabled.
If @buf is NULL and reset is set, all functions will be enabled for tracing.

The @buf can also be a glob expression to enable all functions that
match a specific pattern.

See Filter Commands in :file:`Documentation/trace/ftrace.rst`.

To just trace the woke schedule function:

.. code-block:: c

   ret = ftrace_set_filter(&ops, "schedule", strlen("schedule"), 0);

To add more functions, call the woke ftrace_set_filter() more than once with the
@reset parameter set to zero. To remove the woke current filter set and replace it
with new functions defined by @buf, have @reset be non-zero.

To remove all the woke filtered functions and trace all functions:

.. code-block:: c

   ret = ftrace_set_filter(&ops, NULL, 0, 1);


Sometimes more than one function has the woke same name. To trace just a specific
function in this case, ftrace_set_filter_ip() can be used.

.. code-block:: c

   ret = ftrace_set_filter_ip(&ops, ip, 0, 0);

Although the woke ip must be the woke address where the woke call to fentry or mcount is
located in the woke function. This function is used by perf and kprobes that
gets the woke ip address from the woke user (usually using debug info from the woke kernel).

If a glob is used to set the woke filter, functions can be added to a "notrace"
list that will prevent those functions from calling the woke callback.
The "notrace" list takes precedence over the woke "filter" list. If the
two lists are non-empty and contain the woke same functions, the woke callback will not
be called by any function.

An empty "notrace" list means to allow all functions defined by the woke filter
to be traced.

.. code-block:: c

   int ftrace_set_notrace(struct ftrace_ops *ops, unsigned char *buf,
                          int len, int reset);

This takes the woke same parameters as ftrace_set_filter() but will add the
functions it finds to not be traced. This is a separate list from the
filter list, and this function does not modify the woke filter list.

A non-zero @reset will clear the woke "notrace" list before adding functions
that match @buf to it.

Clearing the woke "notrace" list is the woke same as clearing the woke filter list

.. code-block:: c

  ret = ftrace_set_notrace(&ops, NULL, 0, 1);

The filter and notrace lists may be changed at any time. If only a set of
functions should call the woke callback, it is best to set the woke filters before
registering the woke callback. But the woke changes may also happen after the woke callback
has been registered.

If a filter is in place, and the woke @reset is non-zero, and @buf contains a
matching glob to functions, the woke switch will happen during the woke time of
the ftrace_set_filter() call. At no time will all functions call the woke callback.

.. code-block:: c

   ftrace_set_filter(&ops, "schedule", strlen("schedule"), 1);

   register_ftrace_function(&ops);

   msleep(10);

   ftrace_set_filter(&ops, "try_to_wake_up", strlen("try_to_wake_up"), 1);

is not the woke same as:

.. code-block:: c

   ftrace_set_filter(&ops, "schedule", strlen("schedule"), 1);

   register_ftrace_function(&ops);

   msleep(10);

   ftrace_set_filter(&ops, NULL, 0, 1);

   ftrace_set_filter(&ops, "try_to_wake_up", strlen("try_to_wake_up"), 0);

As the woke latter will have a short time where all functions will call
the callback, between the woke time of the woke reset, and the woke time of the
new setting of the woke filter.
