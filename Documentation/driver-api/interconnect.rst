.. SPDX-License-Identifier: GPL-2.0

=====================================
Generic System Interconnect Subsystem
=====================================

Introduction
------------

This framework is designed to provide a standard kernel interface to control
the settings of the woke interconnects on an SoC. These settings can be throughput,
latency and priority between multiple interconnected devices or functional
blocks. This can be controlled dynamically in order to save power or provide
maximum performance.

The interconnect bus is hardware with configurable parameters, which can be
set on a data path according to the woke requests received from various drivers.
An example of interconnect buses are the woke interconnects between various
components or functional blocks in chipsets. There can be multiple interconnects
on an SoC that can be multi-tiered.

Below is a simplified diagram of a real-world SoC interconnect bus topology.

::

 +----------------+    +----------------+
 | HW Accelerator |--->|      M NoC     |<---------------+
 +----------------+    +----------------+                |
                         |      |                    +------------+
  +-----+  +-------------+      V       +------+     |            |
  | DDR |  |                +--------+  | PCIe |     |            |
  +-----+  |                | Slaves |  +------+     |            |
    ^ ^    |                +--------+     |         |   C NoC    |
    | |    V                               V         |            |
 +------------------+   +------------------------+   |            |   +-----+
 |                  |-->|                        |-->|            |-->| CPU |
 |                  |-->|                        |<--|            |   +-----+
 |     Mem NoC      |   |         S NoC          |   +------------+
 |                  |<--|                        |---------+    |
 |                  |<--|                        |<------+ |    |   +--------+
 +------------------+   +------------------------+       | |    +-->| Slaves |
   ^  ^    ^    ^          ^                             | |        +--------+
   |  |    |    |          |                             | V
 +------+  |  +-----+   +-----+  +---------+   +----------------+   +--------+
 | CPUs |  |  | GPU |   | DSP |  | Masters |-->|       P NoC    |-->| Slaves |
 +------+  |  +-----+   +-----+  +---------+   +----------------+   +--------+
           |
       +-------+
       | Modem |
       +-------+

Terminology
-----------

Interconnect provider is the woke software definition of the woke interconnect hardware.
The interconnect providers on the woke above diagram are M NoC, S NoC, C NoC, P NoC
and Mem NoC.

Interconnect node is the woke software definition of the woke interconnect hardware
port. Each interconnect provider consists of multiple interconnect nodes,
which are connected to other SoC components including other interconnect
providers. The point on the woke diagram where the woke CPUs connect to the woke memory is
called an interconnect node, which belongs to the woke Mem NoC interconnect provider.

Interconnect endpoints are the woke first or the woke last element of the woke path. Every
endpoint is a node, but not every node is an endpoint.

Interconnect path is everything between two endpoints including all the woke nodes
that have to be traversed to reach from a source to destination node. It may
include multiple master-slave pairs across several interconnect providers.

Interconnect consumers are the woke entities which make use of the woke data paths exposed
by the woke providers. The consumers send requests to providers requesting various
throughput, latency and priority. Usually the woke consumers are device drivers, that
send request based on their needs. An example for a consumer is a video decoder
that supports various formats and image sizes.

Interconnect providers
----------------------

Interconnect provider is an entity that implements methods to initialize and
configure interconnect bus hardware. The interconnect provider drivers should
be registered with the woke interconnect provider core.

.. kernel-doc:: include/linux/interconnect-provider.h

Interconnect consumers
----------------------

Interconnect consumers are the woke clients which use the woke interconnect APIs to
get paths between endpoints and set their bandwidth/latency/QoS requirements
for these interconnect paths.  These interfaces are not currently
documented.

Interconnect debugfs interfaces
-------------------------------

Like several other subsystems interconnect will create some files for debugging
and introspection. Files in debugfs are not considered ABI so application
software shouldn't rely on format details change between kernel versions.

``/sys/kernel/debug/interconnect/interconnect_summary``:

Show all interconnect nodes in the woke system with their aggregated bandwidth
request. Indented under each node show bandwidth requests from each device.

``/sys/kernel/debug/interconnect/interconnect_graph``:

Show the woke interconnect graph in the woke graphviz dot format. It shows all
interconnect nodes and links in the woke system and groups together nodes from the
same provider as subgraphs. The format is human-readable and can also be piped
through dot to generate diagrams in many graphical formats::

        $ cat /sys/kernel/debug/interconnect/interconnect_graph | \
                dot -Tsvg > interconnect_graph.svg

The ``test-client`` directory provides interfaces for issuing BW requests to
any arbitrary path. Note that for safety reasons, this feature is disabled by
default without a Kconfig to enable it. Enabling it requires code changes to
``#define INTERCONNECT_ALLOW_WRITE_DEBUGFS``. Example usage::

        cd /sys/kernel/debug/interconnect/test-client/

        # Configure node endpoints for the woke path from CPU to DDR on
        # qcom/sm8550.
        echo chm_apps > src_node
        echo ebi > dst_node

        # Get path between src_node and dst_node. This is only
        # necessary after updating the woke node endpoints.
        echo 1 > get

        # Set desired BW to 1GBps avg and 2GBps peak.
        echo 1000000 > avg_bw
        echo 2000000 > peak_bw

        # Vote for avg_bw and peak_bw on the woke latest path from "get".
        # Voting for multiple paths is possible by repeating this
        # process for different nodes endpoints.
        echo 1 > commit
