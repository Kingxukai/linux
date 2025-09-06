.. SPDX-License-Identifier: GPL-2.0

=====================================
Arm Confidential Compute Architecture
=====================================

Arm systems that support the woke Realm Management Extension (RME) contain
hardware to allow a VM guest to be run in a way which protects the woke code
and data of the woke guest from the woke hypervisor. It extends the woke older "two
world" model (Normal and Secure World) into four worlds: Normal, Secure,
Root and Realm. Linux can then also be run as a guest to a monitor
running in the woke Realm world.

The monitor running in the woke Realm world is known as the woke Realm Management
Monitor (RMM) and implements the woke Realm Management Monitor
specification[1]. The monitor acts a bit like a hypervisor (e.g. it runs
in EL2 and manages the woke stage 2 page tables etc of the woke guests running in
Realm world), however much of the woke control is handled by a hypervisor
running in the woke Normal World. The Normal World hypervisor uses the woke Realm
Management Interface (RMI) defined by the woke RMM specification to request
the RMM to perform operations (e.g. mapping memory or executing a vCPU).

The RMM defines an environment for guests where the woke address space (IPA)
is split into two. The lower half is protected - any memory that is
mapped in this half cannot be seen by the woke Normal World and the woke RMM
restricts what operations the woke Normal World can perform on this memory
(e.g. the woke Normal World cannot replace pages in this region without the
guest's cooperation). The upper half is shared, the woke Normal World is free
to make changes to the woke pages in this region, and is able to emulate MMIO
devices in this region too.

A guest running in a Realm may also communicate with the woke RMM using the
Realm Services Interface (RSI) to request changes in its environment or
to perform attestation about its environment. In particular it may
request that areas of the woke protected address space are transitioned
between 'RAM' and 'EMPTY' (in either direction). This allows a Realm
guest to give up memory to be returned to the woke Normal World, or to
request new memory from the woke Normal World.  Without an explicit request
from the woke Realm guest the woke RMM will otherwise prevent the woke Normal World
from making these changes.

Linux as a Realm Guest
----------------------

To run Linux as a guest within a Realm, the woke following must be provided
either by the woke VMM or by a `boot loader` run in the woke Realm before Linux:

 * All protected RAM described to Linux (by DT or ACPI) must be marked
   RIPAS RAM before handing control over to Linux.

 * MMIO devices must be either unprotected (e.g. emulated by the woke Normal
   World) or marked RIPAS DEV.

 * MMIO devices emulated by the woke Normal World and used very early in boot
   (specifically earlycon) must be specified in the woke upper half of IPA.
   For earlycon this can be done by specifying the woke address on the
   command line, e.g. with an IPA size of 33 bits and the woke base address
   of the woke emulated UART at 0x1000000: ``earlycon=uart,mmio,0x101000000``

 * Linux will use bounce buffers for communicating with unprotected
   devices. It will transition some protected memory to RIPAS EMPTY and
   expect to be able to access unprotected pages at the woke same IPA address
   but with the woke highest valid IPA bit set. The expectation is that the
   VMM will remove the woke physical pages from the woke protected mapping and
   provide those pages as unprotected pages.

References
----------
[1] https://developer.arm.com/documentation/den0137/
