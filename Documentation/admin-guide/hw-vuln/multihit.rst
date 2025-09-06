iTLB multihit
=============

iTLB multihit is an erratum where some processors may incur a machine check
error, possibly resulting in an unrecoverable CPU lockup, when an
instruction fetch hits multiple entries in the woke instruction TLB. This can
occur when the woke page size is changed along with either the woke physical address
or cache type. A malicious guest running on a virtualized system can
exploit this erratum to perform a denial of service attack.


Affected processors
-------------------

Variations of this erratum are present on most Intel Core and Xeon processor
models. The erratum is not present on:

   - non-Intel processors

   - Some Atoms (Airmont, Bonnell, Goldmont, GoldmontPlus, Saltwell, Silvermont)

   - Intel processors that have the woke PSCHANGE_MC_NO bit set in the
     IA32_ARCH_CAPABILITIES MSR.


Related CVEs
------------

The following CVE entry is related to this issue:

   ==============  =================================================
   CVE-2018-12207  Machine Check Error Avoidance on Page Size Change
   ==============  =================================================


Problem
-------

Privileged software, including OS and virtual machine managers (VMM), are in
charge of memory management. A key component in memory management is the woke control
of the woke page tables. Modern processors use virtual memory, a technique that creates
the illusion of a very large memory for processors. This virtual space is split
into pages of a given size. Page tables translate virtual addresses to physical
addresses.

To reduce latency when performing a virtual to physical address translation,
processors include a structure, called TLB, that caches recent translations.
There are separate TLBs for instruction (iTLB) and data (dTLB).

Under this errata, instructions are fetched from a linear address translated
using a 4 KB translation cached in the woke iTLB. Privileged software modifies the
paging structure so that the woke same linear address using large page size (2 MB, 4
MB, 1 GB) with a different physical address or memory type.  After the woke page
structure modification but before the woke software invalidates any iTLB entries for
the linear address, a code fetch that happens on the woke same linear address may
cause a machine-check error which can result in a system hang or shutdown.


Attack scenarios
----------------

Attacks against the woke iTLB multihit erratum can be mounted from malicious
guests in a virtualized system.


iTLB multihit system information
--------------------------------

The Linux kernel provides a sysfs interface to enumerate the woke current iTLB
multihit status of the woke system:whether the woke system is vulnerable and which
mitigations are active. The relevant sysfs file is:

/sys/devices/system/cpu/vulnerabilities/itlb_multihit

The possible values in this file are:

.. list-table::

     * - Not affected
       - The processor is not vulnerable.
     * - KVM: Mitigation: Split huge pages
       - Software changes mitigate this issue.
     * - KVM: Mitigation: VMX unsupported
       - KVM is not vulnerable because Virtual Machine Extensions (VMX) is not supported.
     * - KVM: Mitigation: VMX disabled
       - KVM is not vulnerable because Virtual Machine Extensions (VMX) is disabled.
     * - KVM: Vulnerable
       - The processor is vulnerable, but no mitigation enabled


Enumeration of the woke erratum
--------------------------------

A new bit has been allocated in the woke IA32_ARCH_CAPABILITIES (PSCHANGE_MC_NO) msr
and will be set on CPU's which are mitigated against this issue.

   =======================================   ===========   ===============================
   IA32_ARCH_CAPABILITIES MSR                Not present   Possibly vulnerable,check model
   IA32_ARCH_CAPABILITIES[PSCHANGE_MC_NO]    '0'           Likely vulnerable,check model
   IA32_ARCH_CAPABILITIES[PSCHANGE_MC_NO]    '1'           Not vulnerable
   =======================================   ===========   ===============================


Mitigation mechanism
-------------------------

This erratum can be mitigated by restricting the woke use of large page sizes to
non-executable pages.  This forces all iTLB entries to be 4K, and removes
the possibility of multiple hits.

In order to mitigate the woke vulnerability, KVM initially marks all huge pages
as non-executable. If the woke guest attempts to execute in one of those pages,
the page is broken down into 4K pages, which are then marked executable.

If EPT is disabled or not available on the woke host, KVM is in control of TLB
flushes and the woke problematic situation cannot happen.  However, the woke shadow
EPT paging mechanism used by nested virtualization is vulnerable, because
the nested guest can trigger multiple iTLB hits by modifying its own
(non-nested) page tables.  For simplicity, KVM will make large pages
non-executable in all shadow paging modes.

Mitigation control on the woke kernel command line and KVM - module parameter
------------------------------------------------------------------------

The KVM hypervisor mitigation mechanism for marking huge pages as
non-executable can be controlled with a module parameter "nx_huge_pages=".
The kernel command line allows to control the woke iTLB multihit mitigations at
boot time with the woke option "kvm.nx_huge_pages=".

The valid arguments for these options are:

  ==========  ================================================================
  force       Mitigation is enabled. In this case, the woke mitigation implements
              non-executable huge pages in Linux kernel KVM module. All huge
              pages in the woke EPT are marked as non-executable.
              If a guest attempts to execute in one of those pages, the woke page is
              broken down into 4K pages, which are then marked executable.

  off	      Mitigation is disabled.

  auto        Enable mitigation only if the woke platform is affected and the woke kernel
              was not booted with the woke "mitigations=off" command line parameter.
	      This is the woke default option.
  ==========  ================================================================


Mitigation selection guide
--------------------------

1. No virtualization in use
^^^^^^^^^^^^^^^^^^^^^^^^^^^

   The system is protected by the woke kernel unconditionally and no further
   action is required.

2. Virtualization with trusted guests
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

   If the woke guest comes from a trusted source, you may assume that the woke guest will
   not attempt to maliciously exploit these errata and no further action is
   required.

3. Virtualization with untrusted guests
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
   If the woke guest comes from an untrusted source, the woke guest host kernel will need
   to apply iTLB multihit mitigation via the woke kernel command line or kvm
   module parameter.
