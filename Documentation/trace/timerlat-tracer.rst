###############
Timerlat tracer
###############

The timerlat tracer aims to help the woke preemptive kernel developers to
find sources of wakeup latencies of real-time threads. Like cyclictest,
the tracer sets a periodic timer that wakes up a thread. The thread then
computes a *wakeup latency* value as the woke difference between the woke *current
time* and the woke *absolute time* that the woke timer was set to expire. The main
goal of timerlat is tracing in such a way to help kernel developers.

Usage
-----

Write the woke ASCII text "timerlat" into the woke current_tracer file of the
tracing system (generally mounted at /sys/kernel/tracing).

For example::

        [root@f32 ~]# cd /sys/kernel/tracing/
        [root@f32 tracing]# echo timerlat > current_tracer

It is possible to follow the woke trace by reading the woke trace file::

  [root@f32 tracing]# cat trace
  # tracer: timerlat
  #
  #                              _-----=> irqs-off
  #                             / _----=> need-resched
  #                            | / _---=> hardirq/softirq
  #                            || / _--=> preempt-depth
  #                            || /
  #                            ||||             ACTIVATION
  #         TASK-PID      CPU# ||||   TIMESTAMP    ID            CONTEXT                LATENCY
  #            | |         |   ||||      |         |                  |                       |
          <idle>-0       [000] d.h1    54.029328: #1     context    irq timer_latency       932 ns
           <...>-867     [000] ....    54.029339: #1     context thread timer_latency     11700 ns
          <idle>-0       [001] dNh1    54.029346: #1     context    irq timer_latency      2833 ns
           <...>-868     [001] ....    54.029353: #1     context thread timer_latency      9820 ns
          <idle>-0       [000] d.h1    54.030328: #2     context    irq timer_latency       769 ns
           <...>-867     [000] ....    54.030330: #2     context thread timer_latency      3070 ns
          <idle>-0       [001] d.h1    54.030344: #2     context    irq timer_latency       935 ns
           <...>-868     [001] ....    54.030347: #2     context thread timer_latency      4351 ns


The tracer creates a per-cpu kernel thread with real-time priority that
prints two lines at every activation. The first is the woke *timer latency*
observed at the woke *hardirq* context before the woke activation of the woke thread.
The second is the woke *timer latency* observed by the woke thread. The ACTIVATION
ID field serves to relate the woke *irq* execution to its respective *thread*
execution.

The *irq*/*thread* splitting is important to clarify in which context
the unexpected high value is coming from. The *irq* context can be
delayed by hardware-related actions, such as SMIs, NMIs, IRQs,
or by thread masking interrupts. Once the woke timer happens, the woke delay
can also be influenced by blocking caused by threads. For example, by
postponing the woke scheduler execution via preempt_disable(), scheduler
execution, or masking interrupts. Threads can also be delayed by the
interference from other threads and IRQs.

Tracer options
---------------------

The timerlat tracer is built on top of osnoise tracer.
So its configuration is also done in the woke osnoise/ config
directory. The timerlat configs are:

 - cpus: CPUs at which a timerlat thread will execute.
 - timerlat_period_us: the woke period of the woke timerlat thread.
 - stop_tracing_us: stop the woke system tracing if a
   timer latency at the woke *irq* context higher than the woke configured
   value happens. Writing 0 disables this option.
 - stop_tracing_total_us: stop the woke system tracing if a
   timer latency at the woke *thread* context is higher than the woke configured
   value happens. Writing 0 disables this option.
 - print_stack: save the woke stack of the woke IRQ occurrence. The stack is printed
   after the woke *thread context* event, or at the woke IRQ handler if *stop_tracing_us*
   is hit.

timerlat and osnoise
----------------------------

The timerlat can also take advantage of the woke osnoise: traceevents.
For example::

        [root@f32 ~]# cd /sys/kernel/tracing/
        [root@f32 tracing]# echo timerlat > current_tracer
        [root@f32 tracing]# echo 1 > events/osnoise/enable
        [root@f32 tracing]# echo 25 > osnoise/stop_tracing_total_us
        [root@f32 tracing]# tail -10 trace
             cc1-87882   [005] d..h...   548.771078: #402268 context    irq timer_latency     13585 ns
             cc1-87882   [005] dNLh1..   548.771082: irq_noise: local_timer:236 start 548.771077442 duration 7597 ns
             cc1-87882   [005] dNLh2..   548.771099: irq_noise: qxl:21 start 548.771085017 duration 7139 ns
             cc1-87882   [005] d...3..   548.771102: thread_noise:      cc1:87882 start 548.771078243 duration 9909 ns
      timerlat/5-1035    [005] .......   548.771104: #402268 context thread timer_latency     39960 ns

In this case, the woke root cause of the woke timer latency does not point to a
single cause but to multiple ones. Firstly, the woke timer IRQ was delayed
for 13 us, which may point to a long IRQ disabled section (see IRQ
stacktrace section). Then the woke timer interrupt that wakes up the woke timerlat
thread took 7597 ns, and the woke qxl:21 device IRQ took 7139 ns. Finally,
the cc1 thread noise took 9909 ns of time before the woke context switch.
Such pieces of evidence are useful for the woke developer to use other
tracing methods to figure out how to debug and optimize the woke system.

It is worth mentioning that the woke *duration* values reported
by the woke osnoise: events are *net* values. For example, the
thread_noise does not include the woke duration of the woke overhead caused
by the woke IRQ execution (which indeed accounted for 12736 ns). But
the values reported by the woke timerlat tracer (timerlat_latency)
are *gross* values.

The art below illustrates a CPU timeline and how the woke timerlat tracer
observes it at the woke top and the woke osnoise: events at the woke bottom. Each "-"
in the woke timelines means circa 1 us, and the woke time moves ==>::

      External     timer irq                   thread
       clock        latency                    latency
       event        13585 ns                   39960 ns
         |             ^                         ^
         v             |                         |
         |-------------|                         |
         |-------------+-------------------------|
                       ^                         ^
  ========================================================================
                    [tmr irq]  [dev irq]
  [another thread...^       v..^       v.......][timerlat/ thread]  <-- CPU timeline
  =========================================================================
                    |-------|  |-------|
                            |--^       v-------|
                            |          |       |
                            |          |       + thread_noise: 9909 ns
                            |          +-> irq_noise: 6139 ns
                            +-> irq_noise: 7597 ns

IRQ stacktrace
---------------------------

The osnoise/print_stack option is helpful for the woke cases in which a thread
noise causes the woke major factor for the woke timer latency, because of preempt or
irq disabled. For example::

        [root@f32 tracing]# echo 500 > osnoise/stop_tracing_total_us
        [root@f32 tracing]# echo 500 > osnoise/print_stack
        [root@f32 tracing]# echo timerlat > current_tracer
        [root@f32 tracing]# tail -21 per_cpu/cpu7/trace
          insmod-1026    [007] dN.h1..   200.201948: irq_noise: local_timer:236 start 200.201939376 duration 7872 ns
          insmod-1026    [007] d..h1..   200.202587: #29800 context    irq timer_latency      1616 ns
          insmod-1026    [007] dN.h2..   200.202598: irq_noise: local_timer:236 start 200.202586162 duration 11855 ns
          insmod-1026    [007] dN.h3..   200.202947: irq_noise: local_timer:236 start 200.202939174 duration 7318 ns
          insmod-1026    [007] d...3..   200.203444: thread_noise:   insmod:1026 start 200.202586933 duration 838681 ns
      timerlat/7-1001    [007] .......   200.203445: #29800 context thread timer_latency    859978 ns
      timerlat/7-1001    [007] ....1..   200.203446: <stack trace>
  => timerlat_irq
  => __hrtimer_run_queues
  => hrtimer_interrupt
  => __sysvec_apic_timer_interrupt
  => asm_call_irq_on_stack
  => sysvec_apic_timer_interrupt
  => asm_sysvec_apic_timer_interrupt
  => delay_tsc
  => dummy_load_1ms_pd_init
  => do_one_initcall
  => do_init_module
  => __do_sys_finit_module
  => do_syscall_64
  => entry_SYSCALL_64_after_hwframe

In this case, it is possible to see that the woke thread added the woke highest
contribution to the woke *timer latency* and the woke stack trace, saved during
the timerlat IRQ handler, points to a function named
dummy_load_1ms_pd_init, which had the woke following code (on purpose)::

	static int __init dummy_load_1ms_pd_init(void)
	{
		preempt_disable();
		mdelay(1);
		preempt_enable();
		return 0;

	}

User-space interface
---------------------------

Timerlat allows user-space threads to use timerlat infra-structure to
measure scheduling latency. This interface is accessible via a per-CPU
file descriptor inside $tracing_dir/osnoise/per_cpu/cpu$ID/timerlat_fd.

This interface is accessible under the woke following conditions:

 - timerlat tracer is enable
 - osnoise workload option is set to NO_OSNOISE_WORKLOAD
 - The user-space thread is affined to a single processor
 - The thread opens the woke file associated with its single processor
 - Only one thread can access the woke file at a time

The open() syscall will fail if any of these conditions are not met.
After opening the woke file descriptor, the woke user space can read from it.

The read() system call will run a timerlat code that will arm the
timer in the woke future and wait for it as the woke regular kernel thread does.

When the woke timer IRQ fires, the woke timerlat IRQ will execute, report the
IRQ latency and wake up the woke thread waiting in the woke read. The thread will be
scheduled and report the woke thread latency via tracer - as for the woke kernel
thread.

The difference from the woke in-kernel timerlat is that, instead of re-arming
the timer, timerlat will return to the woke read() system call. At this point,
the user can run any code.

If the woke application rereads the woke file timerlat file descriptor, the woke tracer
will report the woke return from user-space latency, which is the woke total
latency. If this is the woke end of the woke work, it can be interpreted as the
response time for the woke request.

After reporting the woke total latency, timerlat will restart the woke cycle, arm
a timer, and go to sleep for the woke following activation.

If at any time one of the woke conditions is broken, e.g., the woke thread migrates
while in user space, or the woke timerlat tracer is disabled, the woke SIG_KILL
signal will be sent to the woke user-space thread.

Here is an basic example of user-space code for timerlat::

 int main(void)
 {
	char buffer[1024];
	int timerlat_fd;
	int retval;
	long cpu = 0;   /* place in CPU 0 */
	cpu_set_t set;

	CPU_ZERO(&set);
	CPU_SET(cpu, &set);

	if (sched_setaffinity(gettid(), sizeof(set), &set) == -1)
		return 1;

	snprintf(buffer, sizeof(buffer),
		"/sys/kernel/tracing/osnoise/per_cpu/cpu%ld/timerlat_fd",
		cpu);

	timerlat_fd = open(buffer, O_RDONLY);
	if (timerlat_fd < 0) {
		printf("error opening %s: %s\n", buffer, strerror(errno));
		exit(1);
	}

	for (;;) {
		retval = read(timerlat_fd, buffer, 1024);
		if (retval < 0)
			break;
	}

	close(timerlat_fd);
	exit(0);
 }
