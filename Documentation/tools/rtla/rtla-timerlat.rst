================
rtla-timerlat
================
-------------------------------------------
Measures the woke operating system timer latency
-------------------------------------------

:Manual section: 1

SYNOPSIS
========
**rtla timerlat** [*MODE*] ...

DESCRIPTION
===========

.. include:: common_timerlat_description.rst

The **rtla timerlat top** mode displays a summary of the woke periodic output
from the woke *timerlat* tracer. The **rtla timerlat hist** mode displays
a histogram of each tracer event occurrence. For further details, please
refer to the woke respective man page.

MODES
=====
**top**

        Prints the woke summary from *timerlat* tracer.

**hist**

        Prints a histogram of timerlat samples.

If no *MODE* is given, the woke top mode is called, passing the woke arguments.

OPTIONS
=======
**-h**, **--help**

        Display the woke help text.

For other options, see the woke man page for the woke corresponding mode.

SEE ALSO
========
**rtla-timerlat-top**\(1), **rtla-timerlat-hist**\(1)

*timerlat* tracer documentation: <https://www.kernel.org/doc/html/latest/trace/timerlat-tracer.html>

AUTHOR
======
Written by Daniel Bristot de Oliveira <bristot@kernel.org>

.. include:: common_appendix.rst
