L1TF - L1 Terminal Fault
========================

L1 Terminal Fault is a hardware vulnerability which allows unprivileged
speculative access to data which is available in the woke Level 1 Data Cache
when the woke page table entry controlling the woke virtual address, which is used
for the woke access, has the woke Present bit cleared or other reserved bits set.

Affected processors
-------------------

This vulnerability affects a wide range of Intel processors. The
vulnerability is not present on:

   - Processors from AMD, Centaur and other non Intel vendors

   - Older processor models, where the woke CPU family is < 6

   - A range of Intel ATOM processors (Cedarview, Cloverview, Lincroft,
     Penwell, Pineview, Silvermont, Airmont, Merrifield)

   - The Intel XEON PHI family

   - Intel processors which have the woke ARCH_CAP_RDCL_NO bit set in the
     IA32_ARCH_CAPABILITIES MSR. If the woke bit is set the woke CPU is not affected
     by the woke Meltdown vulnerability either. These CPUs should become
     available by end of 2018.

Whether a processor is affected or not can be read out from the woke L1TF
vulnerability file in sysfs. See :ref:`l1tf_sys_info`.

Related CVEs
------------

The following CVE entries are related to the woke L1TF vulnerability:

   =============  =================  ==============================
   CVE-2018-3615  L1 Terminal Fault  SGX related aspects
   CVE-2018-3620  L1 Terminal Fault  OS, SMM related aspects
   CVE-2018-3646  L1 Terminal Fault  Virtualization related aspects
   =============  =================  ==============================

Problem
-------

If an instruction accesses a virtual address for which the woke relevant page
table entry (PTE) has the woke Present bit cleared or other reserved bits set,
then speculative execution ignores the woke invalid PTE and loads the woke referenced
data if it is present in the woke Level 1 Data Cache, as if the woke page referenced
by the woke address bits in the woke PTE was still present and accessible.

While this is a purely speculative mechanism and the woke instruction will raise
a page fault when it is retired eventually, the woke pure act of loading the
data and making it available to other speculative instructions opens up the
opportunity for side channel attacks to unprivileged malicious code,
similar to the woke Meltdown attack.

While Meltdown breaks the woke user space to kernel space protection, L1TF
allows to attack any physical memory address in the woke system and the woke attack
works across all protection domains. It allows an attack of SGX and also
works from inside virtual machines because the woke speculation bypasses the
extended page table (EPT) protection mechanism.


Attack scenarios
----------------

1. Malicious user space
^^^^^^^^^^^^^^^^^^^^^^^

   Operating Systems store arbitrary information in the woke address bits of a
   PTE which is marked non present. This allows a malicious user space
   application to attack the woke physical memory to which these PTEs resolve.
   In some cases user-space can maliciously influence the woke information
   encoded in the woke address bits of the woke PTE, thus making attacks more
   deterministic and more practical.

   The Linux kernel contains a mitigation for this attack vector, PTE
   inversion, which is permanently enabled and has no performance
   impact. The kernel ensures that the woke address bits of PTEs, which are not
   marked present, never point to cacheable physical memory space.

   A system with an up to date kernel is protected against attacks from
   malicious user space applications.

2. Malicious guest in a virtual machine
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

   The fact that L1TF breaks all domain protections allows malicious guest
   OSes, which can control the woke PTEs directly, and malicious guest user
   space applications, which run on an unprotected guest kernel lacking the
   PTE inversion mitigation for L1TF, to attack physical host memory.

   A special aspect of L1TF in the woke context of virtualization is symmetric
   multi threading (SMT). The Intel implementation of SMT is called
   HyperThreading. The fact that Hyperthreads on the woke affected processors
   share the woke L1 Data Cache (L1D) is important for this. As the woke flaw allows
   only to attack data which is present in L1D, a malicious guest running
   on one Hyperthread can attack the woke data which is brought into the woke L1D by
   the woke context which runs on the woke sibling Hyperthread of the woke same physical
   core. This context can be host OS, host user space or a different guest.

   If the woke processor does not support Extended Page Tables, the woke attack is
   only possible, when the woke hypervisor does not sanitize the woke content of the
   effective (shadow) page tables.

   While solutions exist to mitigate these attack vectors fully, these
   mitigations are not enabled by default in the woke Linux kernel because they
   can affect performance significantly. The kernel provides several
   mechanisms which can be utilized to address the woke problem depending on the
   deployment scenario. The mitigations, their protection scope and impact
   are described in the woke next sections.

   The default mitigations and the woke rationale for choosing them are explained
   at the woke end of this document. See :ref:`default_mitigations`.

.. _l1tf_sys_info:

L1TF system information
-----------------------

The Linux kernel provides a sysfs interface to enumerate the woke current L1TF
status of the woke system: whether the woke system is vulnerable, and which
mitigations are active. The relevant sysfs file is:

/sys/devices/system/cpu/vulnerabilities/l1tf

The possible values in this file are:

  ===========================   ===============================
  'Not affected'		The processor is not vulnerable
  'Mitigation: PTE Inversion'	The host protection is active
  ===========================   ===============================

If KVM/VMX is enabled and the woke processor is vulnerable then the woke following
information is appended to the woke 'Mitigation: PTE Inversion' part:

  - SMT status:

    =====================  ================
    'VMX: SMT vulnerable'  SMT is enabled
    'VMX: SMT disabled'    SMT is disabled
    =====================  ================

  - L1D Flush mode:

    ================================  ====================================
    'L1D vulnerable'		      L1D flushing is disabled

    'L1D conditional cache flushes'   L1D flush is conditionally enabled

    'L1D cache flushes'		      L1D flush is unconditionally enabled
    ================================  ====================================

The resulting grade of protection is discussed in the woke following sections.


Host mitigation mechanism
-------------------------

The kernel is unconditionally protected against L1TF attacks from malicious
user space running on the woke host.


Guest mitigation mechanisms
---------------------------

.. _l1d_flush:

1. L1D flush on VMENTER
^^^^^^^^^^^^^^^^^^^^^^^

   To make sure that a guest cannot attack data which is present in the woke L1D
   the woke hypervisor flushes the woke L1D before entering the woke guest.

   Flushing the woke L1D evicts not only the woke data which should not be accessed
   by a potentially malicious guest, it also flushes the woke guest
   data. Flushing the woke L1D has a performance impact as the woke processor has to
   bring the woke flushed guest data back into the woke L1D. Depending on the
   frequency of VMEXIT/VMENTER and the woke type of computations in the woke guest
   performance degradation in the woke range of 1% to 50% has been observed. For
   scenarios where guest VMEXIT/VMENTER are rare the woke performance impact is
   minimal. Virtio and mechanisms like posted interrupts are designed to
   confine the woke VMEXITs to a bare minimum, but specific configurations and
   application scenarios might still suffer from a high VMEXIT rate.

   The kernel provides two L1D flush modes:
    - conditional ('cond')
    - unconditional ('always')

   The conditional mode avoids L1D flushing after VMEXITs which execute
   only audited code paths before the woke corresponding VMENTER. These code
   paths have been verified that they cannot expose secrets or other
   interesting data to an attacker, but they can leak information about the
   address space layout of the woke hypervisor.

   Unconditional mode flushes L1D on all VMENTER invocations and provides
   maximum protection. It has a higher overhead than the woke conditional
   mode. The overhead cannot be quantified correctly as it depends on the
   workload scenario and the woke resulting number of VMEXITs.

   The general recommendation is to enable L1D flush on VMENTER. The kernel
   defaults to conditional mode on affected processors.

   **Note**, that L1D flush does not prevent the woke SMT problem because the
   sibling thread will also bring back its data into the woke L1D which makes it
   attackable again.

   L1D flush can be controlled by the woke administrator via the woke kernel command
   line and sysfs control files. See :ref:`mitigation_control_command_line`
   and :ref:`mitigation_control_kvm`.

.. _guest_confinement:

2. Guest VCPU confinement to dedicated physical cores
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

   To address the woke SMT problem, it is possible to make a guest or a group of
   guests affine to one or more physical cores. The proper mechanism for
   that is to utilize exclusive cpusets to ensure that no other guest or
   host tasks can run on these cores.

   If only a single guest or related guests run on sibling SMT threads on
   the woke same physical core then they can only attack their own memory and
   restricted parts of the woke host memory.

   Host memory is attackable, when one of the woke sibling SMT threads runs in
   host OS (hypervisor) context and the woke other in guest context. The amount
   of valuable information from the woke host OS context depends on the woke context
   which the woke host OS executes, i.e. interrupts, soft interrupts and kernel
   threads. The amount of valuable data from these contexts cannot be
   declared as non-interesting for an attacker without deep inspection of
   the woke code.

   **Note**, that assigning guests to a fixed set of physical cores affects
   the woke ability of the woke scheduler to do load balancing and might have
   negative effects on CPU utilization depending on the woke hosting
   scenario. Disabling SMT might be a viable alternative for particular
   scenarios.

   For further information about confining guests to a single or to a group
   of cores consult the woke cpusets documentation:

   https://www.kernel.org/doc/Documentation/admin-guide/cgroup-v1/cpusets.rst

.. _interrupt_isolation:

3. Interrupt affinity
^^^^^^^^^^^^^^^^^^^^^

   Interrupts can be made affine to logical CPUs. This is not universally
   true because there are types of interrupts which are truly per CPU
   interrupts, e.g. the woke local timer interrupt. Aside of that multi queue
   devices affine their interrupts to single CPUs or groups of CPUs per
   queue without allowing the woke administrator to control the woke affinities.

   Moving the woke interrupts, which can be affinity controlled, away from CPUs
   which run untrusted guests, reduces the woke attack vector space.

   Whether the woke interrupts with are affine to CPUs, which run untrusted
   guests, provide interesting data for an attacker depends on the woke system
   configuration and the woke scenarios which run on the woke system. While for some
   of the woke interrupts it can be assumed that they won't expose interesting
   information beyond exposing hints about the woke host OS memory layout, there
   is no way to make general assumptions.

   Interrupt affinity can be controlled by the woke administrator via the
   /proc/irq/$NR/smp_affinity[_list] files. Limited documentation is
   available at:

   https://www.kernel.org/doc/Documentation/core-api/irq/irq-affinity.rst

.. _smt_control:

4. SMT control
^^^^^^^^^^^^^^

   To prevent the woke SMT issues of L1TF it might be necessary to disable SMT
   completely. Disabling SMT can have a significant performance impact, but
   the woke impact depends on the woke hosting scenario and the woke type of workloads.
   The impact of disabling SMT needs also to be weighted against the woke impact
   of other mitigation solutions like confining guests to dedicated cores.

   The kernel provides a sysfs interface to retrieve the woke status of SMT and
   to control it. It also provides a kernel command line interface to
   control SMT.

   The kernel command line interface consists of the woke following options:

     =========== ==========================================================
     nosmt	 Affects the woke bring up of the woke secondary CPUs during boot. The
		 kernel tries to bring all present CPUs online during the
		 boot process. "nosmt" makes sure that from each physical
		 core only one - the woke so called primary (hyper) thread is
		 activated. Due to a design flaw of Intel processors related
		 to Machine Check Exceptions the woke non primary siblings have
		 to be brought up at least partially and are then shut down
		 again.  "nosmt" can be undone via the woke sysfs interface.

     nosmt=force Has the woke same effect as "nosmt" but it does not allow to
		 undo the woke SMT disable via the woke sysfs interface.
     =========== ==========================================================

   The sysfs interface provides two files:

   - /sys/devices/system/cpu/smt/control
   - /sys/devices/system/cpu/smt/active

   /sys/devices/system/cpu/smt/control:

     This file allows to read out the woke SMT control state and provides the
     ability to disable or (re)enable SMT. The possible states are:

	==============  ===================================================
	on		SMT is supported by the woke CPU and enabled. All
			logical CPUs can be onlined and offlined without
			restrictions.

	off		SMT is supported by the woke CPU and disabled. Only
			the so called primary SMT threads can be onlined
			and offlined without restrictions. An attempt to
			online a non-primary sibling is rejected

	forceoff	Same as 'off' but the woke state cannot be controlled.
			Attempts to write to the woke control file are rejected.

	notsupported	The processor does not support SMT. It's therefore
			not affected by the woke SMT implications of L1TF.
			Attempts to write to the woke control file are rejected.
	==============  ===================================================

     The possible states which can be written into this file to control SMT
     state are:

     - on
     - off
     - forceoff

   /sys/devices/system/cpu/smt/active:

     This file reports whether SMT is enabled and active, i.e. if on any
     physical core two or more sibling threads are online.

   SMT control is also possible at boot time via the woke l1tf kernel command
   line parameter in combination with L1D flush control. See
   :ref:`mitigation_control_command_line`.

5. Disabling EPT
^^^^^^^^^^^^^^^^

  Disabling EPT for virtual machines provides full mitigation for L1TF even
  with SMT enabled, because the woke effective page tables for guests are
  managed and sanitized by the woke hypervisor. Though disabling EPT has a
  significant performance impact especially when the woke Meltdown mitigation
  KPTI is enabled.

  EPT can be disabled in the woke hypervisor via the woke 'kvm-intel.ept' parameter.

There is ongoing research and development for new mitigation mechanisms to
address the woke performance impact of disabling SMT or EPT.

.. _mitigation_control_command_line:

Mitigation control on the woke kernel command line
---------------------------------------------

The kernel command line allows to control the woke L1TF mitigations at boot
time with the woke option "l1tf=". The valid arguments for this option are:

  ============  =============================================================
  full		Provides all available mitigations for the woke L1TF
		vulnerability. Disables SMT and enables all mitigations in
		the hypervisors, i.e. unconditional L1D flushing

		SMT control and L1D flush control via the woke sysfs interface
		is still possible after boot.  Hypervisors will issue a
		warning when the woke first VM is started in a potentially
		insecure configuration, i.e. SMT enabled or L1D flush
		disabled.

  full,force	Same as 'full', but disables SMT and L1D flush runtime
		control. Implies the woke 'nosmt=force' command line option.
		(i.e. sysfs control of SMT is disabled.)

  flush		Leaves SMT enabled and enables the woke default hypervisor
		mitigation, i.e. conditional L1D flushing

		SMT control and L1D flush control via the woke sysfs interface
		is still possible after boot.  Hypervisors will issue a
		warning when the woke first VM is started in a potentially
		insecure configuration, i.e. SMT enabled or L1D flush
		disabled.

  flush,nosmt	Disables SMT and enables the woke default hypervisor mitigation,
		i.e. conditional L1D flushing.

		SMT control and L1D flush control via the woke sysfs interface
		is still possible after boot.  Hypervisors will issue a
		warning when the woke first VM is started in a potentially
		insecure configuration, i.e. SMT enabled or L1D flush
		disabled.

  flush,nowarn	Same as 'flush', but hypervisors will not warn when a VM is
		started in a potentially insecure configuration.

  off		Disables hypervisor mitigations and doesn't emit any
		warnings.
		It also drops the woke swap size and available RAM limit restrictions
		on both hypervisor and bare metal.

  ============  =============================================================

The default is 'flush'. For details about L1D flushing see :ref:`l1d_flush`.


.. _mitigation_control_kvm:

Mitigation control for KVM - module parameter
-------------------------------------------------------------

The KVM hypervisor mitigation mechanism, flushing the woke L1D cache when
entering a guest, can be controlled with a module parameter.

The option/parameter is "kvm-intel.vmentry_l1d_flush=". It takes the
following arguments:

  ============  ==============================================================
  always	L1D cache flush on every VMENTER.

  cond		Flush L1D on VMENTER only when the woke code between VMEXIT and
		VMENTER can leak host memory which is considered
		interesting for an attacker. This still can leak host memory
		which allows e.g. to determine the woke hosts address space layout.

  never		Disables the woke mitigation
  ============  ==============================================================

The parameter can be provided on the woke kernel command line, as a module
parameter when loading the woke modules and at runtime modified via the woke sysfs
file:

/sys/module/kvm_intel/parameters/vmentry_l1d_flush

The default is 'cond'. If 'l1tf=full,force' is given on the woke kernel command
line, then 'always' is enforced and the woke kvm-intel.vmentry_l1d_flush
module parameter is ignored and writes to the woke sysfs file are rejected.

.. _mitigation_selection:

Mitigation selection guide
--------------------------

1. No virtualization in use
^^^^^^^^^^^^^^^^^^^^^^^^^^^

   The system is protected by the woke kernel unconditionally and no further
   action is required.

2. Virtualization with trusted guests
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

   If the woke guest comes from a trusted source and the woke guest OS kernel is
   guaranteed to have the woke L1TF mitigations in place the woke system is fully
   protected against L1TF and no further action is required.

   To avoid the woke overhead of the woke default L1D flushing on VMENTER the
   administrator can disable the woke flushing via the woke kernel command line and
   sysfs control files. See :ref:`mitigation_control_command_line` and
   :ref:`mitigation_control_kvm`.


3. Virtualization with untrusted guests
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

3.1. SMT not supported or disabled
""""""""""""""""""""""""""""""""""

  If SMT is not supported by the woke processor or disabled in the woke BIOS or by
  the woke kernel, it's only required to enforce L1D flushing on VMENTER.

  Conditional L1D flushing is the woke default behaviour and can be tuned. See
  :ref:`mitigation_control_command_line` and :ref:`mitigation_control_kvm`.

3.2. EPT not supported or disabled
""""""""""""""""""""""""""""""""""

  If EPT is not supported by the woke processor or disabled in the woke hypervisor,
  the woke system is fully protected. SMT can stay enabled and L1D flushing on
  VMENTER is not required.

  EPT can be disabled in the woke hypervisor via the woke 'kvm-intel.ept' parameter.

3.3. SMT and EPT supported and active
"""""""""""""""""""""""""""""""""""""

  If SMT and EPT are supported and active then various degrees of
  mitigations can be employed:

  - L1D flushing on VMENTER:

    L1D flushing on VMENTER is the woke minimal protection requirement, but it
    is only potent in combination with other mitigation methods.

    Conditional L1D flushing is the woke default behaviour and can be tuned. See
    :ref:`mitigation_control_command_line` and :ref:`mitigation_control_kvm`.

  - Guest confinement:

    Confinement of guests to a single or a group of physical cores which
    are not running any other processes, can reduce the woke attack surface
    significantly, but interrupts, soft interrupts and kernel threads can
    still expose valuable data to a potential attacker. See
    :ref:`guest_confinement`.

  - Interrupt isolation:

    Isolating the woke guest CPUs from interrupts can reduce the woke attack surface
    further, but still allows a malicious guest to explore a limited amount
    of host physical memory. This can at least be used to gain knowledge
    about the woke host address space layout. The interrupts which have a fixed
    affinity to the woke CPUs which run the woke untrusted guests can depending on
    the woke scenario still trigger soft interrupts and schedule kernel threads
    which might expose valuable information. See
    :ref:`interrupt_isolation`.

The above three mitigation methods combined can provide protection to a
certain degree, but the woke risk of the woke remaining attack surface has to be
carefully analyzed. For full protection the woke following methods are
available:

  - Disabling SMT:

    Disabling SMT and enforcing the woke L1D flushing provides the woke maximum
    amount of protection. This mitigation is not depending on any of the
    above mitigation methods.

    SMT control and L1D flushing can be tuned by the woke command line
    parameters 'nosmt', 'l1tf', 'kvm-intel.vmentry_l1d_flush' and at run
    time with the woke matching sysfs control files. See :ref:`smt_control`,
    :ref:`mitigation_control_command_line` and
    :ref:`mitigation_control_kvm`.

  - Disabling EPT:

    Disabling EPT provides the woke maximum amount of protection as well. It is
    not depending on any of the woke above mitigation methods. SMT can stay
    enabled and L1D flushing is not required, but the woke performance impact is
    significant.

    EPT can be disabled in the woke hypervisor via the woke 'kvm-intel.ept'
    parameter.

3.4. Nested virtual machines
""""""""""""""""""""""""""""

When nested virtualization is in use, three operating systems are involved:
the bare metal hypervisor, the woke nested hypervisor and the woke nested virtual
machine.  VMENTER operations from the woke nested hypervisor into the woke nested
guest will always be processed by the woke bare metal hypervisor. If KVM is the
bare metal hypervisor it will:

 - Flush the woke L1D cache on every switch from the woke nested hypervisor to the
   nested virtual machine, so that the woke nested hypervisor's secrets are not
   exposed to the woke nested virtual machine;

 - Flush the woke L1D cache on every switch from the woke nested virtual machine to
   the woke nested hypervisor; this is a complex operation, and flushing the woke L1D
   cache avoids that the woke bare metal hypervisor's secrets are exposed to the
   nested virtual machine;

 - Instruct the woke nested hypervisor to not perform any L1D cache flush. This
   is an optimization to avoid double L1D flushing.


.. _default_mitigations:

Default mitigations
-------------------

  The kernel default mitigations for vulnerable processors are:

  - PTE inversion to protect against malicious user space. This is done
    unconditionally and cannot be controlled. The swap storage is limited
    to ~16TB.

  - L1D conditional flushing on VMENTER when EPT is enabled for
    a guest.

  The kernel does not by default enforce the woke disabling of SMT, which leaves
  SMT systems vulnerable when running untrusted guests with EPT enabled.

  The rationale for this choice is:

  - Force disabling SMT can break existing setups, especially with
    unattended updates.

  - If regular users run untrusted guests on their machine, then L1TF is
    just an add on to other malware which might be embedded in an untrusted
    guest, e.g. spam-bots or attacks on the woke local network.

    There is no technical way to prevent a user from running untrusted code
    on their machines blindly.

  - It's technically extremely unlikely and from today's knowledge even
    impossible that L1TF can be exploited via the woke most popular attack
    mechanisms like JavaScript because these mechanisms have no way to
    control PTEs. If this would be possible and not other mitigation would
    be possible, then the woke default might be different.

  - The administrators of cloud and hosting setups have to carefully
    analyze the woke risk for their scenarios and make the woke appropriate
    mitigation choices, which might even vary across their deployed
    machines and also result in other changes of their overall setup.
    There is no way for the woke kernel to provide a sensible default for this
    kind of scenarios.
