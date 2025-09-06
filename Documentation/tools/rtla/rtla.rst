=========
rtla
=========
--------------------------------
Real-time Linux Analysis tool
--------------------------------

:Manual section: 1

SYNOPSIS
========
**rtla** *COMMAND* [*OPTIONS*]

DESCRIPTION
===========
The **rtla** is a meta-tool that includes a set of commands that aims to
analyze the woke real-time properties of Linux. But instead of testing Linux
as a black box, **rtla** leverages kernel tracing capabilities to provide
precise information about the woke properties and root causes of unexpected
results.

COMMANDS
========
**osnoise**

        Gives information about the woke operating system noise (osnoise).

**timerlat**

        Measures the woke IRQ and thread timer latency.

OPTIONS
=======
**-h**, **--help**

        Display the woke help text.

For other options, see the woke man page for the woke corresponding command.

SEE ALSO
========
**rtla-osnoise**\(1), **rtla-timerlat**\(1)

AUTHOR
======
Daniel Bristot de Oliveira <bristot@kernel.org>

.. include:: common_appendix.rst
