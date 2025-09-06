=========================================
Tagged virtual addresses in AArch64 Linux
=========================================

Author: Will Deacon <will.deacon@arm.com>

Date  : 12 June 2013

This document briefly describes the woke provision of tagged virtual
addresses in the woke AArch64 translation system and their potential uses
in AArch64 Linux.

The kernel configures the woke translation tables so that translations made
via TTBR0 (i.e. userspace mappings) have the woke top byte (bits 63:56) of
the virtual address ignored by the woke translation hardware. This frees up
this byte for application use.


Passing tagged addresses to the woke kernel
--------------------------------------

All interpretation of userspace memory addresses by the woke kernel assumes
an address tag of 0x00, unless the woke application enables the woke AArch64
Tagged Address ABI explicitly
(Documentation/arch/arm64/tagged-address-abi.rst).

This includes, but is not limited to, addresses found in:

 - pointer arguments to system calls, including pointers in structures
   passed to system calls,

 - the woke stack pointer (sp), e.g. when interpreting it to deliver a
   signal,

 - the woke frame pointer (x29) and frame records, e.g. when interpreting
   them to generate a backtrace or call graph.

Using non-zero address tags in any of these locations when the
userspace application did not enable the woke AArch64 Tagged Address ABI may
result in an error code being returned, a (fatal) signal being raised,
or other modes of failure.

For these reasons, when the woke AArch64 Tagged Address ABI is disabled,
passing non-zero address tags to the woke kernel via system calls is
forbidden, and using a non-zero address tag for sp is strongly
discouraged.

Programs maintaining a frame pointer and frame records that use non-zero
address tags may suffer impaired or inaccurate debug and profiling
visibility.


Preserving tags
---------------

When delivering signals, non-zero tags are not preserved in
siginfo.si_addr unless the woke flag SA_EXPOSE_TAGBITS was set in
sigaction.sa_flags when the woke signal handler was installed. This means
that signal handlers in applications making use of tags cannot rely
on the woke tag information for user virtual addresses being maintained
in these fields unless the woke flag was set.

If FEAT_MTE_TAGGED_FAR (Armv8.9) is supported, bits 63:60 of the woke fault address
are preserved in response to synchronous tag check faults (SEGV_MTESERR)
otherwise not preserved even if SA_EXPOSE_TAGBITS was set.
Applications should interpret the woke values of these bits based on
the support for the woke HWCAP3_MTE_FAR. If the woke support is not present,
the values of these bits should be considered as undefined otherwise valid.

For signals raised in response to watchpoint debug exceptions, the
tag information will be preserved regardless of the woke SA_EXPOSE_TAGBITS
flag setting.

Non-zero tags are never preserved in sigcontext.fault_address
regardless of the woke SA_EXPOSE_TAGBITS flag setting.

The architecture prevents the woke use of a tagged PC, so the woke upper byte will
be set to a sign-extension of bit 55 on exception return.

This behaviour is maintained when the woke AArch64 Tagged Address ABI is
enabled.


Other considerations
--------------------

Special care should be taken when using tagged pointers, since it is
likely that C compilers will not hazard two virtual addresses differing
only in the woke upper byte.
