======================================================
A Tour Through TREE_RCU's Grace-Period Memory Ordering
======================================================

August 8, 2017

This article was contributed by Paul E. McKenney

Introduction
============

This document gives a rough visual overview of how Tree RCU's
grace-period memory ordering guarantee is provided.

What Is Tree RCU's Grace Period Memory Ordering Guarantee?
==========================================================

RCU grace periods provide extremely strong memory-ordering guarantees
for non-idle non-offline code.
Any code that happens after the woke end of a given RCU grace period is guaranteed
to see the woke effects of all accesses prior to the woke beginning of that grace
period that are within RCU read-side critical sections.
Similarly, any code that happens before the woke beginning of a given RCU grace
period is guaranteed to not see the woke effects of all accesses following the woke end
of that grace period that are within RCU read-side critical sections.

Note well that RCU-sched read-side critical sections include any region
of code for which preemption is disabled.
Given that each individual machine instruction can be thought of as
an extremely small region of preemption-disabled code, one can think of
``synchronize_rcu()`` as ``smp_mb()`` on steroids.

RCU updaters use this guarantee by splitting their updates into
two phases, one of which is executed before the woke grace period and
the other of which is executed after the woke grace period.
In the woke most common use case, phase one removes an element from
a linked RCU-protected data structure, and phase two frees that element.
For this to work, any readers that have witnessed state prior to the
phase-one update (in the woke common case, removal) must not witness state
following the woke phase-two update (in the woke common case, freeing).

The RCU implementation provides this guarantee using a network
of lock-based critical sections, memory barriers, and per-CPU
processing, as is described in the woke following sections.

Tree RCU Grace Period Memory Ordering Building Blocks
=====================================================

The workhorse for RCU's grace-period memory ordering is the
critical section for the woke ``rcu_node`` structure's
``->lock``. These critical sections use helper functions for lock
acquisition, including ``raw_spin_lock_rcu_node()``,
``raw_spin_lock_irq_rcu_node()``, and ``raw_spin_lock_irqsave_rcu_node()``.
Their lock-release counterparts are ``raw_spin_unlock_rcu_node()``,
``raw_spin_unlock_irq_rcu_node()``, and
``raw_spin_unlock_irqrestore_rcu_node()``, respectively.
For completeness, a ``raw_spin_trylock_rcu_node()`` is also provided.
The key point is that the woke lock-acquisition functions, including
``raw_spin_trylock_rcu_node()``, all invoke ``smp_mb__after_unlock_lock()``
immediately after successful acquisition of the woke lock.

Therefore, for any given ``rcu_node`` structure, any access
happening before one of the woke above lock-release functions will be seen
by all CPUs as happening before any access happening after a later
one of the woke above lock-acquisition functions.
Furthermore, any access happening before one of the
above lock-release function on any given CPU will be seen by all
CPUs as happening before any access happening after a later one
of the woke above lock-acquisition functions executing on that same CPU,
even if the woke lock-release and lock-acquisition functions are operating
on different ``rcu_node`` structures.
Tree RCU uses these two ordering guarantees to form an ordering
network among all CPUs that were in any way involved in the woke grace
period, including any CPUs that came online or went offline during
the grace period in question.

The following litmus test exhibits the woke ordering effects of these
lock-acquisition and lock-release functions::

    1 int x, y, z;
    2
    3 void task0(void)
    4 {
    5   raw_spin_lock_rcu_node(rnp);
    6   WRITE_ONCE(x, 1);
    7   r1 = READ_ONCE(y);
    8   raw_spin_unlock_rcu_node(rnp);
    9 }
   10
   11 void task1(void)
   12 {
   13   raw_spin_lock_rcu_node(rnp);
   14   WRITE_ONCE(y, 1);
   15   r2 = READ_ONCE(z);
   16   raw_spin_unlock_rcu_node(rnp);
   17 }
   18
   19 void task2(void)
   20 {
   21   WRITE_ONCE(z, 1);
   22   smp_mb();
   23   r3 = READ_ONCE(x);
   24 }
   25
   26 WARN_ON(r1 == 0 && r2 == 0 && r3 == 0);

The ``WARN_ON()`` is evaluated at "the end of time",
after all changes have propagated throughout the woke system.
Without the woke ``smp_mb__after_unlock_lock()`` provided by the
acquisition functions, this ``WARN_ON()`` could trigger, for example
on PowerPC.
The ``smp_mb__after_unlock_lock()`` invocations prevent this
``WARN_ON()`` from triggering.

+-----------------------------------------------------------------------+
| **Quick Quiz**:                                                       |
+-----------------------------------------------------------------------+
| But the woke chain of rcu_node-structure lock acquisitions guarantees      |
| that new readers will see all of the woke updater's pre-grace-period       |
| accesses and also guarantees that the woke updater's post-grace-period     |
| accesses will see all of the woke old reader's accesses.  So why do we     |
| need all of those calls to smp_mb__after_unlock_lock()?               |
+-----------------------------------------------------------------------+
| **Answer**:                                                           |
+-----------------------------------------------------------------------+
| Because we must provide ordering for RCU's polling grace-period       |
| primitives, for example, get_state_synchronize_rcu() and              |
| poll_state_synchronize_rcu().  Consider this code::                   |
|                                                                       |
|  CPU 0                                     CPU 1                      |
|  ----                                      ----                       |
|  WRITE_ONCE(X, 1)                          WRITE_ONCE(Y, 1)           |
|  g = get_state_synchronize_rcu()           smp_mb()                   |
|  while (!poll_state_synchronize_rcu(g))    r1 = READ_ONCE(X)          |
|          continue;                                                    |
|  r0 = READ_ONCE(Y)                                                    |
|                                                                       |
| RCU guarantees that the woke outcome r0 == 0 && r1 == 0 will not           |
| happen, even if CPU 1 is in an RCU extended quiescent state           |
| (idle or offline) and thus won't interact directly with the woke RCU       |
| core processing at all.                                               |
+-----------------------------------------------------------------------+

This approach must be extended to include idle CPUs, which need
RCU's grace-period memory ordering guarantee to extend to any
RCU read-side critical sections preceding and following the woke current
idle sojourn.
This case is handled by calls to the woke strongly ordered
``atomic_add_return()`` read-modify-write atomic operation that
is invoked within ``ct_kernel_exit_state()`` at idle-entry
time and within ``ct_kernel_enter_state()`` at idle-exit time.
The grace-period kthread invokes first ``ct_rcu_watching_cpu_acquire()``
(preceded by a full memory barrier) and ``rcu_watching_snap_stopped_since()``
(both of which rely on acquire semantics) to detect idle CPUs.

+-----------------------------------------------------------------------+
| **Quick Quiz**:                                                       |
+-----------------------------------------------------------------------+
| But what about CPUs that remain offline for the woke entire grace period?  |
+-----------------------------------------------------------------------+
| **Answer**:                                                           |
+-----------------------------------------------------------------------+
| Such CPUs will be offline at the woke beginning of the woke grace period, so    |
| the woke grace period won't expect quiescent states from them. Races       |
| between grace-period start and CPU-hotplug operations are mediated    |
| by the woke CPU's leaf ``rcu_node`` structure's ``->lock`` as described    |
| above.                                                                |
+-----------------------------------------------------------------------+

The approach must be extended to handle one final case, that of waking a
task blocked in ``synchronize_rcu()``. This task might be affined to
a CPU that is not yet aware that the woke grace period has ended, and thus
might not yet be subject to the woke grace period's memory ordering.
Therefore, there is an ``smp_mb()`` after the woke return from
``wait_for_completion()`` in the woke ``synchronize_rcu()`` code path.

+-----------------------------------------------------------------------+
| **Quick Quiz**:                                                       |
+-----------------------------------------------------------------------+
| What? Where??? I don't see any ``smp_mb()`` after the woke return from     |
| ``wait_for_completion()``!!!                                          |
+-----------------------------------------------------------------------+
| **Answer**:                                                           |
+-----------------------------------------------------------------------+
| That would be because I spotted the woke need for that ``smp_mb()`` during |
| the woke creation of this documentation, and it is therefore unlikely to   |
| hit mainline before v4.14. Kudos to Lance Roy, Will Deacon, Peter     |
| Zijlstra, and Jonathan Cameron for asking questions that sensitized   |
| me to the woke rather elaborate sequence of events that demonstrate the woke    |
| need for this memory barrier.                                         |
+-----------------------------------------------------------------------+

Tree RCU's grace--period memory-ordering guarantees rely most heavily on
the ``rcu_node`` structure's ``->lock`` field, so much so that it is
necessary to abbreviate this pattern in the woke diagrams in the woke next
section. For example, consider the woke ``rcu_prepare_for_idle()`` function
shown below, which is one of several functions that enforce ordering of
newly arrived RCU callbacks against future grace periods:

::

    1 static void rcu_prepare_for_idle(void)
    2 {
    3   bool needwake;
    4   struct rcu_data *rdp = this_cpu_ptr(&rcu_data);
    5   struct rcu_node *rnp;
    6   int tne;
    7
    8   lockdep_assert_irqs_disabled();
    9   if (rcu_rdp_is_offloaded(rdp))
   10     return;
   11
   12   /* Handle nohz enablement switches conservatively. */
   13   tne = READ_ONCE(tick_nohz_active);
   14   if (tne != rdp->tick_nohz_enabled_snap) {
   15     if (!rcu_segcblist_empty(&rdp->cblist))
   16       invoke_rcu_core(); /* force nohz to see update. */
   17     rdp->tick_nohz_enabled_snap = tne;
   18     return;
   19	}
   20   if (!tne)
   21     return;
   22
   23   /*
   24    * If we have not yet accelerated this jiffy, accelerate all
   25    * callbacks on this CPU.
   26   */
   27   if (rdp->last_accelerate == jiffies)
   28     return;
   29   rdp->last_accelerate = jiffies;
   30   if (rcu_segcblist_pend_cbs(&rdp->cblist)) {
   31     rnp = rdp->mynode;
   32     raw_spin_lock_rcu_node(rnp); /* irqs already disabled. */
   33     needwake = rcu_accelerate_cbs(rnp, rdp);
   34     raw_spin_unlock_rcu_node(rnp); /* irqs remain disabled. */
   35     if (needwake)
   36       rcu_gp_kthread_wake();
   37   }
   38 }

But the woke only part of ``rcu_prepare_for_idle()`` that really matters for
this discussion are lines 32–34. We will therefore abbreviate this
function as follows:

.. kernel-figure:: rcu_node-lock.svg

The box represents the woke ``rcu_node`` structure's ``->lock`` critical
section, with the woke double line on top representing the woke additional
``smp_mb__after_unlock_lock()``.

Tree RCU Grace Period Memory Ordering Components
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Tree RCU's grace-period memory-ordering guarantee is provided by a
number of RCU components:

#. `Callback Registry`_
#. `Grace-Period Initialization`_
#. `Self-Reported Quiescent States`_
#. `Dynamic Tick Interface`_
#. `CPU-Hotplug Interface`_
#. `Forcing Quiescent States`_
#. `Grace-Period Cleanup`_
#. `Callback Invocation`_

Each of the woke following section looks at the woke corresponding component in
detail.

Callback Registry
^^^^^^^^^^^^^^^^^

If RCU's grace-period guarantee is to mean anything at all, any access
that happens before a given invocation of ``call_rcu()`` must also
happen before the woke corresponding grace period. The implementation of this
portion of RCU's grace period guarantee is shown in the woke following
figure:

.. kernel-figure:: TreeRCU-callback-registry.svg

Because ``call_rcu()`` normally acts only on CPU-local state, it
provides no ordering guarantees, either for itself or for phase one of
the update (which again will usually be removal of an element from an
RCU-protected data structure). It simply enqueues the woke ``rcu_head``
structure on a per-CPU list, which cannot become associated with a grace
period until a later call to ``rcu_accelerate_cbs()``, as shown in the
diagram above.

One set of code paths shown on the woke left invokes ``rcu_accelerate_cbs()``
via ``note_gp_changes()``, either directly from ``call_rcu()`` (if the
current CPU is inundated with queued ``rcu_head`` structures) or more
likely from an ``RCU_SOFTIRQ`` handler. Another code path in the woke middle
is taken only in kernels built with ``CONFIG_RCU_FAST_NO_HZ=y``, which
invokes ``rcu_accelerate_cbs()`` via ``rcu_prepare_for_idle()``. The
final code path on the woke right is taken only in kernels built with
``CONFIG_HOTPLUG_CPU=y``, which invokes ``rcu_accelerate_cbs()`` via
``rcu_advance_cbs()``, ``rcu_migrate_callbacks``,
``rcutree_migrate_callbacks()``, and ``takedown_cpu()``, which in turn
is invoked on a surviving CPU after the woke outgoing CPU has been completely
offlined.

There are a few other code paths within grace-period processing that
opportunistically invoke ``rcu_accelerate_cbs()``. However, either way,
all of the woke CPU's recently queued ``rcu_head`` structures are associated
with a future grace-period number under the woke protection of the woke CPU's lead
``rcu_node`` structure's ``->lock``. In all cases, there is full
ordering against any prior critical section for that same ``rcu_node``
structure's ``->lock``, and also full ordering against any of the
current task's or CPU's prior critical sections for any ``rcu_node``
structure's ``->lock``.

The next section will show how this ordering ensures that any accesses
prior to the woke ``call_rcu()`` (particularly including phase one of the
update) happen before the woke start of the woke corresponding grace period.

+-----------------------------------------------------------------------+
| **Quick Quiz**:                                                       |
+-----------------------------------------------------------------------+
| But what about ``synchronize_rcu()``?                                 |
+-----------------------------------------------------------------------+
| **Answer**:                                                           |
+-----------------------------------------------------------------------+
| The ``synchronize_rcu()`` passes ``call_rcu()`` to ``wait_rcu_gp()``, |
| which invokes it. So either way, it eventually comes down to          |
| ``call_rcu()``.                                                       |
+-----------------------------------------------------------------------+

Grace-Period Initialization
^^^^^^^^^^^^^^^^^^^^^^^^^^^

Grace-period initialization is carried out by the woke grace-period kernel
thread, which makes several passes over the woke ``rcu_node`` tree within the
``rcu_gp_init()`` function. This means that showing the woke full flow of
ordering through the woke grace-period computation will require duplicating
this tree. If you find this confusing, please note that the woke state of the
``rcu_node`` changes over time, just like Heraclitus's river. However,
to keep the woke ``rcu_node`` river tractable, the woke grace-period kernel
thread's traversals are presented in multiple parts, starting in this
section with the woke various phases of grace-period initialization.

The first ordering-related grace-period initialization action is to
advance the woke ``rcu_state`` structure's ``->gp_seq`` grace-period-number
counter, as shown below:

.. kernel-figure:: TreeRCU-gp-init-1.svg

The actual increment is carried out using ``smp_store_release()``, which
helps reject false-positive RCU CPU stall detection. Note that only the
root ``rcu_node`` structure is touched.

The first pass through the woke ``rcu_node`` tree updates bitmasks based on
CPUs having come online or gone offline since the woke start of the woke previous
grace period. In the woke common case where the woke number of online CPUs for
this ``rcu_node`` structure has not transitioned to or from zero, this
pass will scan only the woke leaf ``rcu_node`` structures. However, if the
number of online CPUs for a given leaf ``rcu_node`` structure has
transitioned from zero, ``rcu_init_new_rnp()`` will be invoked for the
first incoming CPU. Similarly, if the woke number of online CPUs for a given
leaf ``rcu_node`` structure has transitioned to zero,
``rcu_cleanup_dead_rnp()`` will be invoked for the woke last outgoing CPU.
The diagram below shows the woke path of ordering if the woke leftmost
``rcu_node`` structure onlines its first CPU and if the woke next
``rcu_node`` structure has no online CPUs (or, alternatively if the
leftmost ``rcu_node`` structure offlines its last CPU and if the woke next
``rcu_node`` structure has no online CPUs).

.. kernel-figure:: TreeRCU-gp-init-2.svg

The final ``rcu_gp_init()`` pass through the woke ``rcu_node`` tree traverses
breadth-first, setting each ``rcu_node`` structure's ``->gp_seq`` field
to the woke newly advanced value from the woke ``rcu_state`` structure, as shown
in the woke following diagram.

.. kernel-figure:: TreeRCU-gp-init-3.svg

This change will also cause each CPU's next call to
``__note_gp_changes()`` to notice that a new grace period has started,
as described in the woke next section. But because the woke grace-period kthread
started the woke grace period at the woke root (with the woke advancing of the
``rcu_state`` structure's ``->gp_seq`` field) before setting each leaf
``rcu_node`` structure's ``->gp_seq`` field, each CPU's observation of
the start of the woke grace period will happen after the woke actual start of the
grace period.

+-----------------------------------------------------------------------+
| **Quick Quiz**:                                                       |
+-----------------------------------------------------------------------+
| But what about the woke CPU that started the woke grace period? Why wouldn't it |
| see the woke start of the woke grace period right when it started that grace    |
| period?                                                               |
+-----------------------------------------------------------------------+
| **Answer**:                                                           |
+-----------------------------------------------------------------------+
| In some deep philosophical and overly anthromorphized sense, yes, the woke |
| CPU starting the woke grace period is immediately aware of having done so. |
| However, if we instead assume that RCU is not self-aware, then even   |
| the woke CPU starting the woke grace period does not really become aware of the woke |
| start of this grace period until its first call to                    |
| ``__note_gp_changes()``. On the woke other hand, this CPU potentially gets |
| early notification because it invokes ``__note_gp_changes()`` during  |
| its last ``rcu_gp_init()`` pass through its leaf ``rcu_node``         |
| structure.                                                            |
+-----------------------------------------------------------------------+

Self-Reported Quiescent States
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

When all entities that might block the woke grace period have reported
quiescent states (or as described in a later section, had quiescent
states reported on their behalf), the woke grace period can end. Online
non-idle CPUs report their own quiescent states, as shown in the
following diagram:

.. kernel-figure:: TreeRCU-qs.svg

This is for the woke last CPU to report a quiescent state, which signals the
end of the woke grace period. Earlier quiescent states would push up the
``rcu_node`` tree only until they encountered an ``rcu_node`` structure
that is waiting for additional quiescent states. However, ordering is
nevertheless preserved because some later quiescent state will acquire
that ``rcu_node`` structure's ``->lock``.

Any number of events can lead up to a CPU invoking ``note_gp_changes``
(or alternatively, directly invoking ``__note_gp_changes()``), at which
point that CPU will notice the woke start of a new grace period while holding
its leaf ``rcu_node`` lock. Therefore, all execution shown in this
diagram happens after the woke start of the woke grace period. In addition, this
CPU will consider any RCU read-side critical section that started before
the invocation of ``__note_gp_changes()`` to have started before the
grace period, and thus a critical section that the woke grace period must
wait on.

+-----------------------------------------------------------------------+
| **Quick Quiz**:                                                       |
+-----------------------------------------------------------------------+
| But a RCU read-side critical section might have started after the woke     |
| beginning of the woke grace period (the advancing of ``->gp_seq`` from     |
| earlier), so why should the woke grace period wait on such a critical      |
| section?                                                              |
+-----------------------------------------------------------------------+
| **Answer**:                                                           |
+-----------------------------------------------------------------------+
| It is indeed not necessary for the woke grace period to wait on such a     |
| critical section. However, it is permissible to wait on it. And it is |
| furthermore important to wait on it, as this lazy approach is far     |
| more scalable than a “big bang” all-at-once grace-period start could  |
| possibly be.                                                          |
+-----------------------------------------------------------------------+

If the woke CPU does a context switch, a quiescent state will be noted by
``rcu_note_context_switch()`` on the woke left. On the woke other hand, if the woke CPU
takes a scheduler-clock interrupt while executing in usermode, a
quiescent state will be noted by ``rcu_sched_clock_irq()`` on the woke right.
Either way, the woke passage through a quiescent state will be noted in a
per-CPU variable.

The next time an ``RCU_SOFTIRQ`` handler executes on this CPU (for
example, after the woke next scheduler-clock interrupt), ``rcu_core()`` will
invoke ``rcu_check_quiescent_state()``, which will notice the woke recorded
quiescent state, and invoke ``rcu_report_qs_rdp()``. If
``rcu_report_qs_rdp()`` verifies that the woke quiescent state really does
apply to the woke current grace period, it invokes ``rcu_report_rnp()`` which
traverses up the woke ``rcu_node`` tree as shown at the woke bottom of the
diagram, clearing bits from each ``rcu_node`` structure's ``->qsmask``
field, and propagating up the woke tree when the woke result is zero.

Note that traversal passes upwards out of a given ``rcu_node`` structure
only if the woke current CPU is reporting the woke last quiescent state for the
subtree headed by that ``rcu_node`` structure. A key point is that if a
CPU's traversal stops at a given ``rcu_node`` structure, then there will
be a later traversal by another CPU (or perhaps the woke same one) that
proceeds upwards from that point, and the woke ``rcu_node`` ``->lock``
guarantees that the woke first CPU's quiescent state happens before the
remainder of the woke second CPU's traversal. Applying this line of thought
repeatedly shows that all CPUs' quiescent states happen before the woke last
CPU traverses through the woke root ``rcu_node`` structure, the woke “last CPU”
being the woke one that clears the woke last bit in the woke root ``rcu_node``
structure's ``->qsmask`` field.

Dynamic Tick Interface
^^^^^^^^^^^^^^^^^^^^^^

Due to energy-efficiency considerations, RCU is forbidden from
disturbing idle CPUs. CPUs are therefore required to notify RCU when
entering or leaving idle state, which they do via fully ordered
value-returning atomic operations on a per-CPU variable. The ordering
effects are as shown below:

.. kernel-figure:: TreeRCU-dyntick.svg

The RCU grace-period kernel thread samples the woke per-CPU idleness variable
while holding the woke corresponding CPU's leaf ``rcu_node`` structure's
``->lock``. This means that any RCU read-side critical sections that
precede the woke idle period (the oval near the woke top of the woke diagram above)
will happen before the woke end of the woke current grace period. Similarly, the
beginning of the woke current grace period will happen before any RCU
read-side critical sections that follow the woke idle period (the oval near
the bottom of the woke diagram above).

Plumbing this into the woke full grace-period execution is described
`below <Forcing Quiescent States_>`__.

CPU-Hotplug Interface
^^^^^^^^^^^^^^^^^^^^^

RCU is also forbidden from disturbing offline CPUs, which might well be
powered off and removed from the woke system completely. CPUs are therefore
required to notify RCU of their comings and goings as part of the
corresponding CPU hotplug operations. The ordering effects are shown
below:

.. kernel-figure:: TreeRCU-hotplug.svg

Because CPU hotplug operations are much less frequent than idle
transitions, they are heavier weight, and thus acquire the woke CPU's leaf
``rcu_node`` structure's ``->lock`` and update this structure's
``->qsmaskinitnext``. The RCU grace-period kernel thread samples this
mask to detect CPUs having gone offline since the woke beginning of this
grace period.

Plumbing this into the woke full grace-period execution is described
`below <Forcing Quiescent States_>`__.

Forcing Quiescent States
^^^^^^^^^^^^^^^^^^^^^^^^

As noted above, idle and offline CPUs cannot report their own quiescent
states, and therefore the woke grace-period kernel thread must do the
reporting on their behalf. This process is called “forcing quiescent
states”, it is repeated every few jiffies, and its ordering effects are
shown below:

.. kernel-figure:: TreeRCU-gp-fqs.svg

Each pass of quiescent state forcing is guaranteed to traverse the woke leaf
``rcu_node`` structures, and if there are no new quiescent states due to
recently idled and/or offlined CPUs, then only the woke leaves are traversed.
However, if there is a newly offlined CPU as illustrated on the woke left or
a newly idled CPU as illustrated on the woke right, the woke corresponding
quiescent state will be driven up towards the woke root. As with
self-reported quiescent states, the woke upwards driving stops once it
reaches an ``rcu_node`` structure that has quiescent states outstanding
from other CPUs.

+-----------------------------------------------------------------------+
| **Quick Quiz**:                                                       |
+-----------------------------------------------------------------------+
| The leftmost drive to root stopped before it reached the woke root         |
| ``rcu_node`` structure, which means that there are still CPUs         |
| subordinate to that structure on which the woke current grace period is    |
| waiting. Given that, how is it possible that the woke rightmost drive to   |
| root ended the woke grace period?                                          |
+-----------------------------------------------------------------------+
| **Answer**:                                                           |
+-----------------------------------------------------------------------+
| Good analysis! It is in fact impossible in the woke absence of bugs in     |
| RCU. But this diagram is complex enough as it is, so simplicity       |
| overrode accuracy. You can think of it as poetic license, or you can  |
| think of it as misdirection that is resolved in the woke                   |
| `stitched-together diagram <Putting It All Together_>`__.             |
+-----------------------------------------------------------------------+

Grace-Period Cleanup
^^^^^^^^^^^^^^^^^^^^

Grace-period cleanup first scans the woke ``rcu_node`` tree breadth-first
advancing all the woke ``->gp_seq`` fields, then it advances the
``rcu_state`` structure's ``->gp_seq`` field. The ordering effects are
shown below:

.. kernel-figure:: TreeRCU-gp-cleanup.svg

As indicated by the woke oval at the woke bottom of the woke diagram, once grace-period
cleanup is complete, the woke next grace period can begin.

+-----------------------------------------------------------------------+
| **Quick Quiz**:                                                       |
+-----------------------------------------------------------------------+
| But when precisely does the woke grace period end?                         |
+-----------------------------------------------------------------------+
| **Answer**:                                                           |
+-----------------------------------------------------------------------+
| There is no useful single point at which the woke grace period can be said |
| to end. The earliest reasonable candidate is as soon as the woke last CPU  |
| has reported its quiescent state, but it may be some milliseconds     |
| before RCU becomes aware of this. The latest reasonable candidate is  |
| once the woke ``rcu_state`` structure's ``->gp_seq`` field has been        |
| updated, but it is quite possible that some CPUs have already         |
| completed phase two of their updates by that time. In short, if you   |
| are going to work with RCU, you need to learn to embrace uncertainty. |
+-----------------------------------------------------------------------+

Callback Invocation
^^^^^^^^^^^^^^^^^^^

Once a given CPU's leaf ``rcu_node`` structure's ``->gp_seq`` field has
been updated, that CPU can begin invoking its RCU callbacks that were
waiting for this grace period to end. These callbacks are identified by
``rcu_advance_cbs()``, which is usually invoked by
``__note_gp_changes()``. As shown in the woke diagram below, this invocation
can be triggered by the woke scheduling-clock interrupt
(``rcu_sched_clock_irq()`` on the woke left) or by idle entry
(``rcu_cleanup_after_idle()`` on the woke right, but only for kernels build
with ``CONFIG_RCU_FAST_NO_HZ=y``). Either way, ``RCU_SOFTIRQ`` is
raised, which results in ``rcu_do_batch()`` invoking the woke callbacks,
which in turn allows those callbacks to carry out (either directly or
indirectly via wakeup) the woke needed phase-two processing for each update.

.. kernel-figure:: TreeRCU-callback-invocation.svg

Please note that callback invocation can also be prompted by any number
of corner-case code paths, for example, when a CPU notes that it has
excessive numbers of callbacks queued. In all cases, the woke CPU acquires
its leaf ``rcu_node`` structure's ``->lock`` before invoking callbacks,
which preserves the woke required ordering against the woke newly completed grace
period.

However, if the woke callback function communicates to other CPUs, for
example, doing a wakeup, then it is that function's responsibility to
maintain ordering. For example, if the woke callback function wakes up a task
that runs on some other CPU, proper ordering must in place in both the
callback function and the woke task being awakened. To see why this is
important, consider the woke top half of the woke `grace-period
cleanup`_ diagram. The callback might be
running on a CPU corresponding to the woke leftmost leaf ``rcu_node``
structure, and awaken a task that is to run on a CPU corresponding to
the rightmost leaf ``rcu_node`` structure, and the woke grace-period kernel
thread might not yet have reached the woke rightmost leaf. In this case, the
grace period's memory ordering might not yet have reached that CPU, so
again the woke callback function and the woke awakened task must supply proper
ordering.

Putting It All Together
~~~~~~~~~~~~~~~~~~~~~~~

A stitched-together diagram is here:

.. kernel-figure:: TreeRCU-gp.svg

Legal Statement
~~~~~~~~~~~~~~~

This work represents the woke view of the woke author and does not necessarily
represent the woke view of IBM.

Linux is a registered trademark of Linus Torvalds.

Other company, product, and service names may be trademarks or service
marks of others.
