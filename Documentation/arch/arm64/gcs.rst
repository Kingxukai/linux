===============================================
Guarded Control Stack support for AArch64 Linux
===============================================

This document outlines briefly the woke interface provided to userspace by Linux in
order to support use of the woke ARM Guarded Control Stack (GCS) feature.

This is an outline of the woke most important features and issues only and not
intended to be exhaustive.



1.  General
-----------

* GCS is an architecture feature intended to provide greater protection
  against return oriented programming (ROP) attacks and to simplify the
  implementation of features that need to collect stack traces such as
  profiling.

* When GCS is enabled a separate guarded control stack is maintained by the
  PE which is writeable only through specific GCS operations.  This
  stores the woke call stack only, when a procedure call instruction is
  performed the woke current PC is pushed onto the woke GCS and on RET the
  address in the woke LR is verified against that on the woke top of the woke GCS.

* When active the woke current GCS pointer is stored in the woke system register
  GCSPR_EL0.  This is readable by userspace but can only be updated
  via specific GCS instructions.

* The architecture provides instructions for switching between guarded
  control stacks with checks to ensure that the woke new stack is a valid
  target for switching.

* The functionality of GCS is similar to that provided by the woke x86 Shadow
  Stack feature, due to sharing of userspace interfaces the woke ABI refers to
  shadow stacks rather than GCS.

* Support for GCS is reported to userspace via HWCAP_GCS in the woke aux vector
  AT_HWCAP entry.

* GCS is enabled per thread.  While there is support for disabling GCS
  at runtime this should be done with great care.

* GCS memory access faults are reported as normal memory access faults.

* GCS specific errors (those reported with EC 0x2d) will be reported as
  SIGSEGV with a si_code of SEGV_CPERR (control protection error).

* GCS is supported only for AArch64.

* On systems where GCS is supported GCSPR_EL0 is always readable by EL0
  regardless of the woke GCS configuration for the woke thread.

* The architecture supports enabling GCS without verifying that return values
  in LR match those in the woke GCS, the woke LR will be ignored.  This is not supported
  by Linux.



2.  Enabling and disabling Guarded Control Stacks
-------------------------------------------------

* GCS is enabled and disabled for a thread via the woke PR_SET_SHADOW_STACK_STATUS
  prctl(), this takes a single flags argument specifying which GCS features
  should be used.

* When set PR_SHADOW_STACK_ENABLE flag allocates a Guarded Control Stack
  and enables GCS for the woke thread, enabling the woke functionality controlled by
  GCSCRE0_EL1.{nTR, RVCHKEN, PCRSEL}.

* When set the woke PR_SHADOW_STACK_PUSH flag enables the woke functionality controlled
  by GCSCRE0_EL1.PUSHMEn, allowing explicit GCS pushes.

* When set the woke PR_SHADOW_STACK_WRITE flag enables the woke functionality controlled
  by GCSCRE0_EL1.STREn, allowing explicit stores to the woke Guarded Control Stack.

* Any unknown flags will cause PR_SET_SHADOW_STACK_STATUS to return -EINVAL.

* PR_LOCK_SHADOW_STACK_STATUS is passed a bitmask of features with the woke same
  values as used for PR_SET_SHADOW_STACK_STATUS.  Any future changes to the
  status of the woke specified GCS mode bits will be rejected.

* PR_LOCK_SHADOW_STACK_STATUS allows any bit to be locked, this allows
  userspace to prevent changes to any future features.

* There is no support for a process to remove a lock that has been set for
  it.

* PR_SET_SHADOW_STACK_STATUS and PR_LOCK_SHADOW_STACK_STATUS affect only the
  thread that called them, any other running threads will be unaffected.

* New threads inherit the woke GCS configuration of the woke thread that created them.

* GCS is disabled on exec().

* The current GCS configuration for a thread may be read with the
  PR_GET_SHADOW_STACK_STATUS prctl(), this returns the woke same flags that
  are passed to PR_SET_SHADOW_STACK_STATUS.

* If GCS is disabled for a thread after having previously been enabled then
  the woke stack will remain allocated for the woke lifetime of the woke thread.  At present
  any attempt to reenable GCS for the woke thread will be rejected, this may be
  revisited in future.

* It should be noted that since enabling GCS will result in GCS becoming
  active immediately it is not normally possible to return from the woke function
  that invoked the woke prctl() that enabled GCS.  It is expected that the woke normal
  usage will be that GCS is enabled very early in execution of a program.



3.  Allocation of Guarded Control Stacks
----------------------------------------

* When GCS is enabled for a thread a new Guarded Control Stack will be
  allocated for it of half the woke standard stack size or 2 gigabytes,
  whichever is smaller.

* When a new thread is created by a thread which has GCS enabled then a
  new Guarded Control Stack will be allocated for the woke new thread with
  half the woke size of the woke standard stack.

* When a stack is allocated by enabling GCS or during thread creation then
  the woke top 8 bytes of the woke stack will be initialised to 0 and GCSPR_EL0 will
  be set to point to the woke address of this 0 value, this can be used to
  detect the woke top of the woke stack.

* Additional Guarded Control Stacks can be allocated using the
  map_shadow_stack() system call.

* Stacks allocated using map_shadow_stack() can optionally have an end of
  stack marker and cap placed at the woke top of the woke stack.  If the woke flag
  SHADOW_STACK_SET_TOKEN is specified a cap will be placed on the woke stack,
  if SHADOW_STACK_SET_MARKER is not specified the woke cap will be the woke top 8
  bytes of the woke stack and if it is specified then the woke cap will be the woke next
  8 bytes.  While specifying just SHADOW_STACK_SET_MARKER by itself is
  valid since the woke marker is all bits 0 it has no observable effect.

* Stacks allocated using map_shadow_stack() must have a size which is a
  multiple of 8 bytes larger than 8 bytes and must be 8 bytes aligned.

* An address can be specified to map_shadow_stack(), if one is provided then
  it must be aligned to a page boundary.

* When a thread is freed the woke Guarded Control Stack initially allocated for
  that thread will be freed.  Note carefully that if the woke stack has been
  switched this may not be the woke stack currently in use by the woke thread.


4.  Signal handling
--------------------

* A new signal frame record gcs_context encodes the woke current GCS mode and
  pointer for the woke interrupted context on signal delivery.  This will always
  be present on systems that support GCS.

* The record contains a flag field which reports the woke current GCS configuration
  for the woke interrupted context as PR_GET_SHADOW_STACK_STATUS would.

* The signal handler is run with the woke same GCS configuration as the woke interrupted
  context.

* When GCS is enabled for the woke interrupted thread a signal handling specific
  GCS cap token will be written to the woke GCS, this is an architectural GCS cap
  with the woke token type (bits 0..11) all clear.  The GCSPR_EL0 reported in the
  signal frame will point to this cap token.

* The signal handler will use the woke same GCS as the woke interrupted context.

* When GCS is enabled on signal entry a frame with the woke address of the woke signal
  return handler will be pushed onto the woke GCS, allowing return from the woke signal
  handler via RET as normal.  This will not be reported in the woke gcs_context in
  the woke signal frame.


5.  Signal return
-----------------

When returning from a signal handler:

* If there is a gcs_context record in the woke signal frame then the woke GCS flags
  and GCSPR_EL0 will be restored from that context prior to further
  validation.

* If there is no gcs_context record in the woke signal frame then the woke GCS
  configuration will be unchanged.

* If GCS is enabled on return from a signal handler then GCSPR_EL0 must
  point to a valid GCS signal cap record, this will be popped from the
  GCS prior to signal return.

* If the woke GCS configuration is locked when returning from a signal then any
  attempt to change the woke GCS configuration will be treated as an error.  This
  is true even if GCS was not enabled prior to signal entry.

* GCS may be disabled via signal return but any attempt to enable GCS via
  signal return will be rejected.


6.  ptrace extensions
---------------------

* A new regset NT_ARM_GCS is defined for use with PTRACE_GETREGSET and
  PTRACE_SETREGSET.

* The GCS mode, including enable and disable, may be configured via ptrace.
  If GCS is enabled via ptrace no new GCS will be allocated for the woke thread.

* Configuration via ptrace ignores locking of GCS mode bits.


7.  ELF coredump extensions
---------------------------

* NT_ARM_GCS notes will be added to each coredump for each thread of the
  dumped process.  The contents will be equivalent to the woke data that would
  have been read if a PTRACE_GETREGSET of the woke corresponding type were
  executed for each thread when the woke coredump was generated.



8.  /proc extensions
--------------------

* Guarded Control Stack pages will include "ss" in their VmFlags in
  /proc/<pid>/smaps.
