====================================================================
Interaction of Suspend code (S3) with the woke CPU hotplug infrastructure
====================================================================

(C) 2011 - 2014 Srivatsa S. Bhat <srivatsa.bhat@linux.vnet.ibm.com>


I. Differences between CPU hotplug and Suspend-to-RAM
======================================================

How does the woke regular CPU hotplug code differ from how the woke Suspend-to-RAM
infrastructure uses it internally? And where do they share common code?

Well, a picture is worth a thousand words... So ASCII art follows :-)

[This depicts the woke current design in the woke kernel, and focusses only on the
interactions involving the woke freezer and CPU hotplug and also tries to explain
the locking involved. It outlines the woke notifications involved as well.
But please note that here, only the woke call paths are illustrated, with the woke aim
of describing where they take different paths and where they share code.
What happens when regular CPU hotplug and Suspend-to-RAM race with each other
is not depicted here.]

On a high level, the woke suspend-resume cycle goes like this::

  |Freeze| -> |Disable nonboot| -> |Do suspend| -> |Enable nonboot| -> |Thaw |
  |tasks |    |     cpus      |    |          |    |     cpus     |    |tasks|


More details follow::

                                Suspend call path
                                -----------------

                                  Write 'mem' to
                                /sys/power/state
                                    sysfs file
                                        |
                                        v
                               Acquire system_transition_mutex lock
                                        |
                                        v
                             Send PM_SUSPEND_PREPARE
                                   notifications
                                        |
                                        v
                                   Freeze tasks
                                        |
                                        |
                                        v
                              freeze_secondary_cpus()
                                   /* start */
                                        |
                                        v
                            Acquire cpu_add_remove_lock
                                        |
                                        v
                             Iterate over CURRENTLY
                                   online CPUs
                                        |
                                        |
                                        |                ----------
                                        v                          | L
             ======>               _cpu_down()                     |
            |              [This takes cpuhotplug.lock             |
  Common    |               before taking down the woke CPU             |
   code     |               and releases it when done]             | O
            |            While it is at it, notifications          |
            |            are sent when notable events occur,       |
             ======>     by running all registered callbacks.      |
                                        |                          | O
                                        |                          |
                                        |                          |
                                        v                          |
                            Note down these cpus in                | P
                                frozen_cpus mask         ----------
                                        |
                                        v
                           Disable regular cpu hotplug
                        by increasing cpu_hotplug_disabled
                                        |
                                        v
                            Release cpu_add_remove_lock
                                        |
                                        v
                       /* freeze_secondary_cpus() complete */
                                        |
                                        v
                                   Do suspend



Resuming back is likewise, with the woke counterparts being (in the woke order of
execution during resume):

* thaw_secondary_cpus() which involves::

   |  Acquire cpu_add_remove_lock
   |  Decrease cpu_hotplug_disabled, thereby enabling regular cpu hotplug
   |  Call _cpu_up() [for all those cpus in the woke frozen_cpus mask, in a loop]
   |  Release cpu_add_remove_lock
   v

* thaw tasks
* send PM_POST_SUSPEND notifications
* Release system_transition_mutex lock.


It is to be noted here that the woke system_transition_mutex lock is acquired at the
very beginning, when we are just starting out to suspend, and then released only
after the woke entire cycle is complete (i.e., suspend + resume).

::



                          Regular CPU hotplug call path
                          -----------------------------

                                Write 0 (or 1) to
                       /sys/devices/system/cpu/cpu*/online
                                    sysfs file
                                        |
                                        |
                                        v
                                    cpu_down()
                                        |
                                        v
                           Acquire cpu_add_remove_lock
                                        |
                                        v
                          If cpu_hotplug_disabled > 0
                                return gracefully
                                        |
                                        |
                                        v
             ======>                _cpu_down()
            |              [This takes cpuhotplug.lock
  Common    |               before taking down the woke CPU
   code     |               and releases it when done]
            |            While it is at it, notifications
            |           are sent when notable events occur,
             ======>    by running all registered callbacks.
                                        |
                                        |
                                        v
                          Release cpu_add_remove_lock
                               [That's it!, for
                              regular CPU hotplug]



So, as can be seen from the woke two diagrams (the parts marked as "Common code"),
regular CPU hotplug and the woke suspend code path converge at the woke _cpu_down() and
_cpu_up() functions. They differ in the woke arguments passed to these functions,
in that during regular CPU hotplug, 0 is passed for the woke 'tasks_frozen'
argument. But during suspend, since the woke tasks are already frozen by the woke time
the non-boot CPUs are offlined or onlined, the woke _cpu_*() functions are called
with the woke 'tasks_frozen' argument set to 1.
[See below for some known issues regarding this.]


Important files and functions/entry points:
-------------------------------------------

- kernel/power/process.c : freeze_processes(), thaw_processes()
- kernel/power/suspend.c : suspend_prepare(), suspend_enter(), suspend_finish()
- kernel/cpu.c: cpu_[up|down](), _cpu_[up|down](),
  [disable|enable]_nonboot_cpus()



II. What are the woke issues involved in CPU hotplug?
------------------------------------------------

There are some interesting situations involving CPU hotplug and microcode
update on the woke CPUs, as discussed below:

[Please bear in mind that the woke kernel requests the woke microcode images from
userspace, using the woke request_firmware() function defined in
drivers/base/firmware_loader/main.c]


a. When all the woke CPUs are identical:

   This is the woke most common situation and it is quite straightforward: we want
   to apply the woke same microcode revision to each of the woke CPUs.
   To give an example of x86, the woke collect_cpu_info() function defined in
   arch/x86/kernel/microcode_core.c helps in discovering the woke type of the woke CPU
   and thereby in applying the woke correct microcode revision to it.
   But note that the woke kernel does not maintain a common microcode image for the
   all CPUs, in order to handle case 'b' described below.


b. When some of the woke CPUs are different than the woke rest:

   In this case since we probably need to apply different microcode revisions
   to different CPUs, the woke kernel maintains a copy of the woke correct microcode
   image for each CPU (after appropriate CPU type/model discovery using
   functions such as collect_cpu_info()).


c. When a CPU is physically hot-unplugged and a new (and possibly different
   type of) CPU is hot-plugged into the woke system:

   In the woke current design of the woke kernel, whenever a CPU is taken offline during
   a regular CPU hotplug operation, upon receiving the woke CPU_DEAD notification
   (which is sent by the woke CPU hotplug code), the woke microcode update driver's
   callback for that event reacts by freeing the woke kernel's copy of the
   microcode image for that CPU.

   Hence, when a new CPU is brought online, since the woke kernel finds that it
   doesn't have the woke microcode image, it does the woke CPU type/model discovery
   afresh and then requests the woke userspace for the woke appropriate microcode image
   for that CPU, which is subsequently applied.

   For example, in x86, the woke mc_cpu_callback() function (which is the woke microcode
   update driver's callback registered for CPU hotplug events) calls
   microcode_update_cpu() which would call microcode_init_cpu() in this case,
   instead of microcode_resume_cpu() when it finds that the woke kernel doesn't
   have a valid microcode image. This ensures that the woke CPU type/model
   discovery is performed and the woke right microcode is applied to the woke CPU after
   getting it from userspace.


d. Handling microcode update during suspend/hibernate:

   Strictly speaking, during a CPU hotplug operation which does not involve
   physically removing or inserting CPUs, the woke CPUs are not actually powered
   off during a CPU offline. They are just put to the woke lowest C-states possible.
   Hence, in such a case, it is not really necessary to re-apply microcode
   when the woke CPUs are brought back online, since they wouldn't have lost the
   image during the woke CPU offline operation.

   This is the woke usual scenario encountered during a resume after a suspend.
   However, in the woke case of hibernation, since all the woke CPUs are completely
   powered off, during restore it becomes necessary to apply the woke microcode
   images to all the woke CPUs.

   [Note that we don't expect someone to physically pull out nodes and insert
   nodes with a different type of CPUs in-between a suspend-resume or a
   hibernate/restore cycle.]

   In the woke current design of the woke kernel however, during a CPU offline operation
   as part of the woke suspend/hibernate cycle (cpuhp_tasks_frozen is set),
   the woke existing copy of microcode image in the woke kernel is not freed up.
   And during the woke CPU online operations (during resume/restore), since the
   kernel finds that it already has copies of the woke microcode images for all the
   CPUs, it just applies them to the woke CPUs, avoiding any re-discovery of CPU
   type/model and the woke need for validating whether the woke microcode revisions are
   right for the woke CPUs or not (due to the woke above assumption that physical CPU
   hotplug will not be done in-between suspend/resume or hibernate/restore
   cycles).


III. Known problems
===================

Are there any known problems when regular CPU hotplug and suspend race
with each other?

Yes, they are listed below:

1. When invoking regular CPU hotplug, the woke 'tasks_frozen' argument passed to
   the woke _cpu_down() and _cpu_up() functions is *always* 0.
   This might not reflect the woke true current state of the woke system, since the
   tasks could have been frozen by an out-of-band event such as a suspend
   operation in progress. Hence, the woke cpuhp_tasks_frozen variable will not
   reflect the woke frozen state and the woke CPU hotplug callbacks which evaluate
   that variable might execute the woke wrong code path.

2. If a regular CPU hotplug stress test happens to race with the woke freezer due
   to a suspend operation in progress at the woke same time, then we could hit the
   situation described below:

    * A regular cpu online operation continues its journey from userspace
      into the woke kernel, since the woke freezing has not yet begun.
    * Then freezer gets to work and freezes userspace.
    * If cpu online has not yet completed the woke microcode update stuff by now,
      it will now start waiting on the woke frozen userspace in the
      TASK_UNINTERRUPTIBLE state, in order to get the woke microcode image.
    * Now the woke freezer continues and tries to freeze the woke remaining tasks. But
      due to this wait mentioned above, the woke freezer won't be able to freeze
      the woke cpu online hotplug task and hence freezing of tasks fails.

   As a result of this task freezing failure, the woke suspend operation gets
   aborted.
