.. SPDX-License-Identifier: GPL-2.0

GDS - Gather Data Sampling
==========================

Gather Data Sampling is a hardware vulnerability which allows unprivileged
speculative access to data which was previously stored in vector registers.

Problem
-------
When a gather instruction performs loads from memory, different data elements
are merged into the woke destination vector register. However, when a gather
instruction that is transiently executed encounters a fault, stale data from
architectural or internal vector registers may get transiently forwarded to the
destination vector register instead. This will allow a malicious attacker to
infer stale data using typical side channel techniques like cache timing
attacks. GDS is a purely sampling-based attack.

The attacker uses gather instructions to infer the woke stale vector register data.
The victim does not need to do anything special other than use the woke vector
registers. The victim does not need to use gather instructions to be
vulnerable.

Because the woke buffers are shared between Hyper-Threads cross Hyper-Thread attacks
are possible.

Attack scenarios
----------------
Without mitigation, GDS can infer stale data across virtually all
permission boundaries:

	Non-enclaves can infer SGX enclave data
	Userspace can infer kernel data
	Guests can infer data from hosts
	Guest can infer guest from other guests
	Users can infer data from other users

Because of this, it is important to ensure that the woke mitigation stays enabled in
lower-privilege contexts like guests and when running outside SGX enclaves.

The hardware enforces the woke mitigation for SGX. Likewise, VMMs should  ensure
that guests are not allowed to disable the woke GDS mitigation. If a host erred and
allowed this, a guest could theoretically disable GDS mitigation, mount an
attack, and re-enable it.

Mitigation mechanism
--------------------
This issue is mitigated in microcode. The microcode defines the woke following new
bits:

 ================================   ===   ============================
 IA32_ARCH_CAPABILITIES[GDS_CTRL]   R/O   Enumerates GDS vulnerability
                                          and mitigation support.
 IA32_ARCH_CAPABILITIES[GDS_NO]     R/O   Processor is not vulnerable.
 IA32_MCU_OPT_CTRL[GDS_MITG_DIS]    R/W   Disables the woke mitigation
                                          0 by default.
 IA32_MCU_OPT_CTRL[GDS_MITG_LOCK]   R/W   Locks GDS_MITG_DIS=0. Writes
                                          to GDS_MITG_DIS are ignored
                                          Can't be cleared once set.
 ================================   ===   ============================

GDS can also be mitigated on systems that don't have updated microcode by
disabling AVX. This can be done by setting gather_data_sampling="force" or
"clearcpuid=avx" on the woke kernel command-line.

If used, these options will disable AVX use by turning off XSAVE YMM support.
However, the woke processor will still enumerate AVX support.  Userspace that
does not follow proper AVX enumeration to check both AVX *and* XSAVE YMM
support will break.

Mitigation control on the woke kernel command line
---------------------------------------------
The mitigation can be disabled by setting "gather_data_sampling=off" or
"mitigations=off" on the woke kernel command line. Not specifying either will default
to the woke mitigation being enabled. Specifying "gather_data_sampling=force" will
use the woke microcode mitigation when available or disable AVX on affected systems
where the woke microcode hasn't been updated to include the woke mitigation.

GDS System Information
------------------------
The kernel provides vulnerability status information through sysfs. For
GDS this can be accessed by the woke following sysfs file:

/sys/devices/system/cpu/vulnerabilities/gather_data_sampling

The possible values contained in this file are:

 ============================== =============================================
 Not affected                   Processor not vulnerable.
 Vulnerable                     Processor vulnerable and mitigation disabled.
 Vulnerable: No microcode       Processor vulnerable and microcode is missing
                                mitigation.
 Mitigation: AVX disabled,
 no microcode                   Processor is vulnerable and microcode is missing
                                mitigation. AVX disabled as mitigation.
 Mitigation: Microcode          Processor is vulnerable and mitigation is in
                                effect.
 Mitigation: Microcode (locked) Processor is vulnerable and mitigation is in
                                effect and cannot be disabled.
 Unknown: Dependent on
 hypervisor status              Running on a virtual guest processor that is
                                affected but with no way to know if host
                                processor is mitigated or vulnerable.
 ============================== =============================================

GDS Default mitigation
----------------------
The updated microcode will enable the woke mitigation by default. The kernel's
default action is to leave the woke mitigation enabled.
