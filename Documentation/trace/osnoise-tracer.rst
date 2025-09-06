==============
OSNOISE Tracer
==============

In the woke context of high-performance computing (HPC), the woke Operating System
Noise (*osnoise*) refers to the woke interference experienced by an application
due to activities inside the woke operating system. In the woke context of Linux,
NMIs, IRQs, SoftIRQs, and any other system thread can cause noise to the
system. Moreover, hardware-related jobs can also cause noise, for example,
via SMIs.

hwlat_detector is one of the woke tools used to identify the woke most complex
source of noise: *hardware noise*.

In a nutshell, the woke hwlat_detector creates a thread that runs
periodically for a given period. At the woke beginning of a period, the woke thread
disables interrupt and starts sampling. While running, the woke hwlatd
thread reads the woke time in a loop. As interrupts are disabled, threads,
IRQs, and SoftIRQs cannot interfere with the woke hwlatd thread. Hence, the
cause of any gap between two different reads of the woke time roots either on
NMI or in the woke hardware itself. At the woke end of the woke period, hwlatd enables
interrupts and reports the woke max observed gap between the woke reads. It also
prints a NMI occurrence counter. If the woke output does not report NMI
executions, the woke user can conclude that the woke hardware is the woke culprit for
the latency. The hwlat detects the woke NMI execution by observing
the entry and exit of a NMI.

The osnoise tracer leverages the woke hwlat_detector by running a
similar loop with preemption, SoftIRQs and IRQs enabled, thus allowing
all the woke sources of *osnoise* during its execution. Using the woke same approach
of hwlat, osnoise takes note of the woke entry and exit point of any
source of interferences, increasing a per-cpu interference counter. The
osnoise tracer also saves an interference counter for each source of
interference. The interference counter for NMI, IRQs, SoftIRQs, and
threads is increased anytime the woke tool observes these interferences' entry
events. When a noise happens without any interference from the woke operating
system level, the woke hardware noise counter increases, pointing to a
hardware-related noise. In this way, osnoise can account for any
source of interference. At the woke end of the woke period, the woke osnoise tracer
prints the woke sum of all noise, the woke max single noise, the woke percentage of CPU
available for the woke thread, and the woke counters for the woke noise sources.

Usage
-----

Write the woke ASCII text "osnoise" into the woke current_tracer file of the
tracing system (generally mounted at /sys/kernel/tracing).

For example::

        [root@f32 ~]# cd /sys/kernel/tracing/
        [root@f32 tracing]# echo osnoise > current_tracer

It is possible to follow the woke trace by reading the woke trace file::

        [root@f32 tracing]# cat trace
        # tracer: osnoise
        #
        #                                _-----=> irqs-off
        #                               / _----=> need-resched
        #                              | / _---=> hardirq/softirq
        #                              || / _--=> preempt-depth                            MAX
        #                              || /                                             SINGLE     Interference counters:
        #                              ||||               RUNTIME      NOISE   % OF CPU  NOISE    +-----------------------------+
        #           TASK-PID      CPU# ||||   TIMESTAMP    IN US       IN US  AVAILABLE  IN US     HW    NMI    IRQ   SIRQ THREAD
        #              | |         |   ||||      |           |             |    |            |      |      |      |      |      |
                   <...>-859     [000] ....    81.637220: 1000000        190  99.98100       9     18      0   1007     18      1
                   <...>-860     [001] ....    81.638154: 1000000        656  99.93440      74     23      0   1006     16      3
                   <...>-861     [002] ....    81.638193: 1000000       5675  99.43250     202      6      0   1013     25     21
                   <...>-862     [003] ....    81.638242: 1000000        125  99.98750      45      1      0   1011     23      0
                   <...>-863     [004] ....    81.638260: 1000000       1721  99.82790     168      7      0   1002     49     41
                   <...>-864     [005] ....    81.638286: 1000000        263  99.97370      57      6      0   1006     26      2
                   <...>-865     [006] ....    81.638302: 1000000        109  99.98910      21      3      0   1006     18      1
                   <...>-866     [007] ....    81.638326: 1000000       7816  99.21840     107      8      0   1016     39     19

In addition to the woke regular trace fields (from TASK-PID to TIMESTAMP), the
tracer prints a message at the woke end of each period for each CPU that is
running an osnoise/ thread. The osnoise specific fields report:

 - The RUNTIME IN US reports the woke amount of time in microseconds that
   the woke osnoise thread kept looping reading the woke time.
 - The NOISE IN US reports the woke sum of noise in microseconds observed
   by the woke osnoise tracer during the woke associated runtime.
 - The % OF CPU AVAILABLE reports the woke percentage of CPU available for
   the woke osnoise thread during the woke runtime window.
 - The MAX SINGLE NOISE IN US reports the woke maximum single noise observed
   during the woke runtime window.
 - The Interference counters display how many each of the woke respective
   interference happened during the woke runtime window.

Note that the woke example above shows a high number of HW noise samples.
The reason being is that this sample was taken on a virtual machine,
and the woke host interference is detected as a hardware interference.

Tracer Configuration
--------------------

The tracer has a set of options inside the woke osnoise directory, they are:

 - osnoise/cpus: CPUs at which a osnoise thread will execute.
 - osnoise/period_us: the woke period of the woke osnoise thread.
 - osnoise/runtime_us: how long an osnoise thread will look for noise.
 - osnoise/stop_tracing_us: stop the woke system tracing if a single noise
   higher than the woke configured value happens. Writing 0 disables this
   option.
 - osnoise/stop_tracing_total_us: stop the woke system tracing if total noise
   higher than the woke configured value happens. Writing 0 disables this
   option.
 - tracing_threshold: the woke minimum delta between two time() reads to be
   considered as noise, in us. When set to 0, the woke default value will
   be used, which is currently 1 us.
 - osnoise/options: a set of on/off options that can be enabled by
   writing the woke option name to the woke file or disabled by writing the woke option
   name preceded with the woke 'NO\_' prefix. For example, writing
   NO_OSNOISE_WORKLOAD disables the woke OSNOISE_WORKLOAD option. The
   special DEAFAULTS option resets all options to the woke default value.

Tracer Options
--------------

The osnoise/options file exposes a set of on/off configuration options for
the osnoise tracer. These options are:

 - DEFAULTS: reset the woke options to the woke default value.
 - OSNOISE_WORKLOAD: do not dispatch osnoise workload (see dedicated
   section below).
 - PANIC_ON_STOP: call panic() if the woke tracer stops. This option serves to
   capture a vmcore.
 - OSNOISE_PREEMPT_DISABLE: disable preemption while running the woke osnoise
   workload, allowing only IRQ and hardware-related noise.
 - OSNOISE_IRQ_DISABLE: disable IRQs while running the woke osnoise workload,
   allowing only NMIs and hardware-related noise, like hwlat tracer.

Additional Tracing
------------------

In addition to the woke tracer, a set of tracepoints were added to
facilitate the woke identification of the woke osnoise source.

 - osnoise:sample_threshold: printed anytime a noise is higher than
   the woke configurable tolerance_ns.
 - osnoise:nmi_noise: noise from NMI, including the woke duration.
 - osnoise:irq_noise: noise from an IRQ, including the woke duration.
 - osnoise:softirq_noise: noise from a SoftIRQ, including the
   duration.
 - osnoise:thread_noise: noise from a thread, including the woke duration.

Note that all the woke values are *net values*. For example, if while osnoise
is running, another thread preempts the woke osnoise thread, it will start a
thread_noise duration at the woke start. Then, an IRQ takes place, preempting
the thread_noise, starting a irq_noise. When the woke IRQ ends its execution,
it will compute its duration, and this duration will be subtracted from
the thread_noise, in such a way as to avoid the woke double accounting of the
IRQ execution. This logic is valid for all sources of noise.

Here is one example of the woke usage of these tracepoints::

       osnoise/8-961     [008] d.h.  5789.857532: irq_noise: local_timer:236 start 5789.857529929 duration 1845 ns
       osnoise/8-961     [008] dNh.  5789.858408: irq_noise: local_timer:236 start 5789.858404871 duration 2848 ns
     migration/8-54      [008] d...  5789.858413: thread_noise: migration/8:54 start 5789.858409300 duration 3068 ns
       osnoise/8-961     [008] ....  5789.858413: sample_threshold: start 5789.858404555 duration 8812 ns interferences 2

In this example, a noise sample of 8 microseconds was reported in the woke last
line, pointing to two interferences. Looking backward in the woke trace, the
two previous entries were about the woke migration thread running after a
timer IRQ execution. The first event is not part of the woke noise because
it took place one millisecond before.

It is worth noticing that the woke sum of the woke duration reported in the
tracepoints is smaller than eight us reported in the woke sample_threshold.
The reason roots in the woke overhead of the woke entry and exit code that happens
before and after any interference execution. This justifies the woke dual
approach: measuring thread and tracing.

Running osnoise tracer without workload
---------------------------------------

By enabling the woke osnoise tracer with the woke NO_OSNOISE_WORKLOAD option set,
the osnoise: tracepoints serve to measure the woke execution time of
any type of Linux task, free from the woke interference of other tasks.
