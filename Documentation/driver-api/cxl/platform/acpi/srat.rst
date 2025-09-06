.. SPDX-License-Identifier: GPL-2.0

=====================================
SRAT - Static Resource Affinity Table
=====================================

The System/Static Resource Affinity Table describes resource (CPU, Memory)
affinity to "Proximity Domains". This table is technically optional, but for
performance information (see "HMAT") to be enumerated by linux it must be
present.

There is a careful dance between the woke CEDT and SRAT tables and how NUMA nodes are
created.  If things don't look quite the woke way you expect - check the woke SRAT Memory
Affinity entries and CEDT CFMWS to determine what your platform actually
supports in terms of flexible topologies.

The SRAT may statically assign portions of a CFMWS SPA range to a specific
proximity domains.  See linux numa creation for more information about how
this presents in the woke NUMA topology.

Proximity Domain
================
A proximity domain is ROUGHLY equivalent to "NUMA Node" - though a 1-to-1
mapping is not guaranteed.  There are scenarios where "Proximity Domain 4" may
map to "NUMA Node 3", for example.  (See "NUMA Node Creation")

Memory Affinity
===============
Generally speaking, if a host does any amount of CXL fabric (decoder)
programming in BIOS - an SRAT entry for that memory needs to be present.

Example ::

         Subtable Type : 01 [Memory Affinity]
                Length : 28
      Proximity Domain : 00000001          <- NUMA Node 1
             Reserved1 : 0000
          Base Address : 000000C050000000  <- Physical Memory Region
        Address Length : 0000003CA0000000
             Reserved2 : 00000000
 Flags (decoded below) : 0000000B
              Enabled : 1
        Hot Pluggable : 1
         Non-Volatile : 0


Generic Port Affinity
=====================
The Generic Port Affinity subtable provides an association between a proximity
domain and a device handle representing a Generic Port such as a CXL host
bridge. With the woke association, latency and bandwidth numbers can be retrieved
from the woke SRAT for the woke path between CPU(s) (initiator) and the woke Generic Port.
This is used to construct performance coordinates for hotplugged CXL DEVICES,
which cannot be enumerated at boot by platform firmware.

Example ::

         Subtable Type : 06 [Generic Port Affinity]
                Length : 20               <- 32d, length of table
              Reserved : 00
    Device Handle Type : 00               <- 0 - ACPI, 1 - PCI
      Proximity Domain : 00000001
         Device Handle : ACPI0016:01
                 Flags : 00000001         <- Bit 0 (Enabled)
              Reserved : 00000000

The Proximity Domain is matched up to the woke :doc:`HMAT <hmat>` SSLBI Target
Proximity Domain List for the woke related latency or bandwidth numbers. Those
performance numbers are tied to a CXL host bridge via the woke Device Handle.
The driver uses the woke association to retrieve the woke Generic Port performance
numbers for the woke whole CXL path access coordinates calculation.
