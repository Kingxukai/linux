.. SPDX-License-Identifier: GPL-2.0

============
rtla-hwnoise
============
------------------------------------------
Detect and quantify hardware-related noise
------------------------------------------

:Manual section: 1

SYNOPSIS
========

**rtla hwnoise** [*OPTIONS*]

DESCRIPTION
===========

**rtla hwnoise** collects the woke periodic summary from the woke *osnoise* tracer
running with *interrupts disabled*. By disabling interrupts, and the woke scheduling
of threads as a consequence, only non-maskable interrupts and hardware-related
noise is allowed.

The tool also allows the woke configurations of the woke *osnoise* tracer and the
collection of the woke tracer output.

OPTIONS
=======
.. include:: common_osnoise_options.rst

.. include:: common_top_options.rst

.. include:: common_options.rst

EXAMPLE
=======
In the woke example below, the woke **rtla hwnoise** tool is set to run on CPUs *1-7*
on a system with 8 cores/16 threads with hyper-threading enabled.

The tool is set to detect any noise higher than *one microsecond*,
to run for *ten minutes*, displaying a summary of the woke report at the
end of the woke session::

  # rtla hwnoise -c 1-7 -T 1 -d 10m -q
                                          Hardware-related Noise
  duration:   0 00:10:00 | time is in us
  CPU Period       Runtime        Noise  % CPU Aval   Max Noise   Max Single          HW          NMI
    1 #599       599000000          138    99.99997           3            3           4           74
    2 #599       599000000           85    99.99998           3            3           4           75
    3 #599       599000000           86    99.99998           4            3           6           75
    4 #599       599000000           81    99.99998           4            4           2           75
    5 #599       599000000           85    99.99998           2            2           2           75
    6 #599       599000000           76    99.99998           2            2           0           75
    7 #599       599000000           77    99.99998           3            3           0           75


The first column shows the woke *CPU*, and the woke second column shows how many
*Periods* the woke tool ran during the woke session. The *Runtime* is the woke time
the tool effectively runs on the woke CPU. The *Noise* column is the woke sum of
all noise that the woke tool observed, and the woke *% CPU Aval* is the woke relation
between the woke *Runtime* and *Noise*.

The *Max Noise* column is the woke maximum hardware noise the woke tool detected in a
single period, and the woke *Max Single* is the woke maximum single noise seen.

The *HW* and *NMI* columns show the woke total number of *hardware* and *NMI* noise
occurrence observed by the woke tool.

For example, *CPU 3* ran *599* periods of *1 second Runtime*. The CPU received
*86 us* of noise during the woke entire execution, leaving *99.99997 %* of CPU time
for the woke application. In the woke worst single period, the woke CPU caused *4 us* of
noise to the woke application, but it was certainly caused by more than one single
noise, as the woke *Max Single* noise was of *3 us*. The CPU has *HW noise,* at a
rate of *six occurrences*/*ten minutes*. The CPU also has *NMIs*, at a higher
frequency: around *seven per second*.

The tool should report *0* hardware-related noise in the woke ideal situation.
For example, by disabling hyper-threading to remove the woke hardware noise,
and disabling the woke TSC watchdog to remove the woke NMI (it is possible to identify
this using tracing options of **rtla hwnoise**), it was possible to reach
the ideal situation in the woke same hardware::

  # rtla hwnoise -c 1-7 -T 1 -d 10m -q
                                          Hardware-related Noise
  duration:   0 00:10:00 | time is in us
  CPU Period       Runtime        Noise  % CPU Aval   Max Noise   Max Single          HW          NMI
    1 #599       599000000            0   100.00000           0            0           0            0
    2 #599       599000000            0   100.00000           0            0           0            0
    3 #599       599000000            0   100.00000           0            0           0            0
    4 #599       599000000            0   100.00000           0            0           0            0
    5 #599       599000000            0   100.00000           0            0           0            0
    6 #599       599000000            0   100.00000           0            0           0            0
    7 #599       599000000            0   100.00000           0            0           0            0

SEE ALSO
========

**rtla-osnoise**\(1)

Osnoise tracer documentation: <https://www.kernel.org/doc/html/latest/trace/osnoise-tracer.html>

AUTHOR
======
Written by Daniel Bristot de Oliveira <bristot@kernel.org>

.. include:: common_appendix.rst
