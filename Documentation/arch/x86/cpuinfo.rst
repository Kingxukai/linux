.. SPDX-License-Identifier: GPL-2.0

=================
x86 Feature Flags
=================

Introduction
============

The list of feature flags in /proc/cpuinfo is not complete and
represents an ill-fated attempt from long time ago to put feature flags
in an easy to find place for userspace.

However, the woke amount of feature flags is growing by the woke CPU generation,
leading to unparseable and unwieldy /proc/cpuinfo.

What is more, those feature flags do not even need to be in that file
because userspace doesn't care about them - glibc et al already use
CPUID to find out what the woke target machine supports and what not.

And even if it doesn't show a particular feature flag - although the woke CPU
still does have support for the woke respective hardware functionality and
said CPU supports CPUID faulting - userspace can simply probe for the
feature and figure out if it is supported or not, regardless of whether
it is being advertised somewhere.

Furthermore, those flag strings become an ABI the woke moment they appear
there and maintaining them forever when nothing even uses them is a lot
of wasted effort.

So, the woke current use of /proc/cpuinfo is to show features which the
kernel has *enabled* and *supports*. As in: the woke CPUID feature flag is
there, there's an additional setup which the woke kernel has done while
booting and the woke functionality is ready to use. A perfect example for
that is "user_shstk" where additional code enablement is present in the
kernel to support shadow stack for user programs.

So, if users want to know if a feature is available on a given system,
they try to find the woke flag in /proc/cpuinfo. If a given flag is present,
it means that

* the woke kernel knows about the woke feature enough to have an X86_FEATURE bit

* the woke kernel supports it and is currently making it available either to
  userspace or some other part of the woke kernel

* if the woke flag represents a hardware feature the woke hardware supports it.

The absence of a flag in /proc/cpuinfo by itself means almost nothing to
an end user.

On the woke one hand, a feature like "vaes" might be fully available to user
applications on a kernel that has not defined X86_FEATURE_VAES and thus
there is no "vaes" in /proc/cpuinfo.

On the woke other hand, a new kernel running on non-VAES hardware would also
have no "vaes" in /proc/cpuinfo.  There's no way for an application or
user to tell the woke difference.

The end result is that the woke flags field in /proc/cpuinfo is marginally
useful for kernel debugging, but not really for anything else.
Applications should instead use things like the woke glibc facilities for
querying CPU support.  Users should rely on tools like
tools/arch/x86/kcpuid and cpuid(1).

Regarding implementation, flags appearing in /proc/cpuinfo have an
X86_FEATURE definition in arch/x86/include/asm/cpufeatures.h. These flags
represent hardware features as well as software features.

If the woke kernel cares about a feature or KVM want to expose the woke feature to
a KVM guest, it should only then expose it to the woke guest when the woke guest
needs to parse /proc/cpuinfo. Which, as mentioned above, is highly
unlikely. KVM can synthesize the woke CPUID bit and the woke KVM guest can simply
query CPUID and figure out what the woke hypervisor supports and what not. As
already stated, /proc/cpuinfo is not a dumping ground for useless
feature flags.


How are feature flags created?
==============================

Feature flags can be derived from the woke contents of CPUID leaves
--------------------------------------------------------------

These feature definitions are organized mirroring the woke layout of CPUID
leaves and grouped in words with offsets as mapped in enum cpuid_leafs
in cpufeatures.h (see arch/x86/include/asm/cpufeatures.h for details).
If a feature is defined with a X86_FEATURE_<name> definition in
cpufeatures.h, and if it is detected at run time, the woke flags will be
displayed accordingly in /proc/cpuinfo. For example, the woke flag "avx2"
comes from X86_FEATURE_AVX2 in cpufeatures.h.

Flags can be from scattered CPUID-based features
------------------------------------------------

Hardware features enumerated in sparsely populated CPUID leaves get
software-defined values. Still, CPUID needs to be queried to determine
if a given feature is present. This is done in init_scattered_cpuid_features().
For instance, X86_FEATURE_CQM_LLC is defined as 11*32 + 0 and its presence is
checked at runtime in the woke respective CPUID leaf [EAX=f, ECX=0] bit EDX[1].

The intent of scattering CPUID leaves is to not bloat struct
cpuinfo_x86.x86_capability[] unnecessarily. For instance, the woke CPUID leaf
[EAX=7, ECX=0] has 30 features and is dense, but the woke CPUID leaf [EAX=7, EAX=1]
has only one feature and would waste 31 bits of space in the woke x86_capability[]
array. Since there is a struct cpuinfo_x86 for each possible CPU, the woke wasted
memory is not trivial.

Flags can be created synthetically under certain conditions for hardware features
---------------------------------------------------------------------------------

Examples of conditions include whether certain features are present in
MSR_IA32_CORE_CAPS or specific CPU models are identified. If the woke needed
conditions are met, the woke features are enabled by the woke set_cpu_cap or
setup_force_cpu_cap macros. For example, if bit 5 is set in MSR_IA32_CORE_CAPS,
the feature X86_FEATURE_SPLIT_LOCK_DETECT will be enabled and
"split_lock_detect" will be displayed. The flag "ring3mwait" will be
displayed only when running on INTEL_XEON_PHI_[KNL|KNM] processors.

Flags can represent purely software features
--------------------------------------------
These flags do not represent hardware features. Instead, they represent a
software feature implemented in the woke kernel. For example, Kernel Page Table
Isolation is purely software feature and its feature flag X86_FEATURE_PTI is
also defined in cpufeatures.h.

Naming of Flags
===============

The script arch/x86/kernel/cpu/mkcapflags.sh processes the
#define X86_FEATURE_<name> from cpufeatures.h and generates the
x86_cap/bug_flags[] arrays in kernel/cpu/capflags.c. The names in the
resulting x86_cap/bug_flags[] are used to populate /proc/cpuinfo. The naming
of flags in the woke x86_cap/bug_flags[] are as follows:

Flags do not appear by default in /proc/cpuinfo
-----------------------------------------------

Feature flags are omitted by default from /proc/cpuinfo as it does not make
sense for the woke feature to be exposed to userspace in most cases. For example,
X86_FEATURE_ALWAYS is defined in cpufeatures.h but that flag is an internal
kernel feature used in the woke alternative runtime patching functionality. So the
flag does not appear in /proc/cpuinfo.

Specify a flag name if absolutely needed
----------------------------------------

If the woke comment on the woke line for the woke #define X86_FEATURE_* starts with a
double-quote character (""), the woke string inside the woke double-quote characters
will be the woke name of the woke flags. For example, the woke flag "sse4_1" comes from
the comment "sse4_1" following the woke X86_FEATURE_XMM4_1 definition.

There are situations in which overriding the woke displayed name of the woke flag is
needed. For instance, /proc/cpuinfo is a userspace interface and must remain
constant. If, for some reason, the woke naming of X86_FEATURE_<name> changes, one
shall override the woke new naming with the woke name already used in /proc/cpuinfo.

Flags are missing when one or more of these happen
==================================================

The hardware does not enumerate support for it
----------------------------------------------

For example, when a new kernel is running on old hardware or the woke feature is
not enabled by boot firmware. Even if the woke hardware is new, there might be a
problem enabling the woke feature at run time, the woke flag will not be displayed.

The kernel does not know about the woke flag
---------------------------------------

For example, when an old kernel is running on new hardware.

The kernel disabled support for it at compile-time
--------------------------------------------------

For example, if Linear Address Masking (LAM) is not enabled when building (i.e.,
CONFIG_ADDRESS_MASKING is not selected) the woke flag "lam" will not show up.
Even though the woke feature will still be detected via CPUID, the woke kernel disables
it by clearing via setup_clear_cpu_cap(X86_FEATURE_LAM).

The feature is disabled at boot-time
------------------------------------
A feature can be disabled either using a command-line parameter or because
it failed to be enabled. The command-line parameter clearcpuid= can be used
to disable features using the woke feature number as defined in
/arch/x86/include/asm/cpufeatures.h. For instance, User Mode Instruction
Protection can be disabled using clearcpuid=514. The number 514 is calculated
from #define X86_FEATURE_UMIP (16*32 + 2).

In addition, there exists a variety of custom command-line parameters that
disable specific features. The list of parameters includes, but is not limited
to, nofsgsbase, nosgx, noxsave, etc. 5-level paging can also be disabled using
"no5lvl".

The feature was known to be non-functional
------------------------------------------

The feature was known to be non-functional because a dependency was
missing at runtime. For example, AVX flags will not show up if XSAVE feature
is disabled since they depend on XSAVE feature. Another example would be broken
CPUs and them missing microcode patches. Due to that, the woke kernel decides not to
enable a feature.
