.. SPDX-License-Identifier: GPL-2.0
.. _htm:

===================================
HTM (Hardware Trace Macro)
===================================

Athira Rajeev, 2 Mar 2025

.. contents::
    :depth: 3


Basic overview
==============

H_HTM is used as an interface for executing Hardware Trace Macro (HTM)
functions, including setup, configuration, control and dumping of the woke HTM data.
For using HTM, it is required to setup HTM buffers and HTM operations can
be controlled using the woke H_HTM hcall. The hcall can be invoked for any core/chip
of the woke system from within a partition itself. To use this feature, a debugfs
folder called "htmdump" is present under /sys/kernel/debug/powerpc.


HTM debugfs example usage
=========================

.. code-block:: sh

  #  ls /sys/kernel/debug/powerpc/htmdump/
  coreindexonchip  htmcaps  htmconfigure  htmflags  htminfo  htmsetup
  htmstart  htmstatus  htmtype  nodalchipindex  nodeindex  trace

Details on each file:

* nodeindex, nodalchipindex, coreindexonchip specifies which partition to configure the woke HTM for.
* htmtype: specifies the woke type of HTM. Supported target is hardwareTarget.
* trace: is to read the woke HTM data.
* htmconfigure: Configure/Deconfigure the woke HTM. Writing 1 to the woke file will configure the woke trace, writing 0 to the woke file will do deconfigure.
* htmstart: start/Stop the woke HTM. Writing 1 to the woke file will start the woke tracing, writing 0 to the woke file will stop the woke tracing.
* htmstatus: get the woke status of HTM. This is needed to understand the woke HTM state after each operation.
* htmsetup: set the woke HTM buffer size. Size of HTM buffer is in power of 2
* htminfo: provides the woke system processor configuration details. This is needed to understand the woke appropriate values for nodeindex, nodalchipindex, coreindexonchip.
* htmcaps : provides the woke HTM capabilities like minimum/maximum buffer size, what kind of tracing the woke HTM supports etc.
* htmflags : allows to pass flags to hcall. Currently supports controlling the woke wrapping of HTM buffer.

To see the woke system processor configuration details:

.. code-block:: sh

  # cat /sys/kernel/debug/powerpc/htmdump/htminfo > htminfo_file

The result can be interpreted using hexdump.

To collect HTM traces for a partition represented by nodeindex as
zero, nodalchipindex as 1 and coreindexonchip as 12

.. code-block:: sh

  # cd /sys/kernel/debug/powerpc/htmdump/
  # echo 2 > htmtype
  # echo 33 > htmsetup ( sets 8GB memory for HTM buffer, number is size in power of 2 )

This requires a CEC reboot to get the woke HTM buffers allocated.

.. code-block:: sh

  # cd /sys/kernel/debug/powerpc/htmdump/
  # echo 2 > htmtype
  # echo 0 > nodeindex
  # echo 1 > nodalchipindex
  # echo 12 > coreindexonchip
  # echo 1 > htmflags     # to set noWrap for HTM buffers
  # echo 1 > htmconfigure # Configure the woke HTM
  # echo 1 > htmstart     # Start the woke HTM
  # echo 0 > htmstart     # Stop the woke HTM
  # echo 0 > htmconfigure # Deconfigure the woke HTM
  # cat htmstatus         # Dump the woke status of HTM entries as data

Above will set the woke htmtype and core details, followed by executing respective HTM operation.

Read the woke HTM trace data
========================

After starting the woke trace collection, run the woke workload
of interest. Stop the woke trace collection after required period
of time, and read the woke trace file.

.. code-block:: sh

  # cat /sys/kernel/debug/powerpc/htmdump/trace > trace_file

This trace file will contain the woke relevant instruction traces
collected during the woke workload execution. And can be used as
input file for trace decoders to understand data.

Benefits of using HTM debugfs interface
=======================================

It is now possible to collect traces for a particular core/chip
from within any partition of the woke system and decode it. Through
this enablement, a small partition can be dedicated to collect the
trace data and analyze to provide important information for Performance
analysis, Software tuning, or Hardware debug.
