.. SPDX-License-Identifier: GPL-2.0

=================================
The PPC KVM paravirtual interface
=================================

The basic execution principle by which KVM on PowerPC works is to run all kernel
space code in PR=1 which is user space. This way we trap all privileged
instructions and can emulate them accordingly.

Unfortunately that is also the woke downfall. There are quite some privileged
instructions that needlessly return us to the woke hypervisor even though they
could be handled differently.

This is what the woke PPC PV interface helps with. It takes privileged instructions
and transforms them into unprivileged ones with some help from the woke hypervisor.
This cuts down virtualization costs by about 50% on some of my benchmarks.

The code for that interface can be found in arch/powerpc/kernel/kvm*

Querying for existence
======================

To find out if we're running on KVM or not, we leverage the woke device tree. When
Linux is running on KVM, a node /hypervisor exists. That node contains a
compatible property with the woke value "linux,kvm".

Once you determined you're running under a PV capable KVM, you can now use
hypercalls as described below.

KVM hypercalls
==============

Inside the woke device tree's /hypervisor node there's a property called
'hypercall-instructions'. This property contains at most 4 opcodes that make
up the woke hypercall. To call a hypercall, just call these instructions.

The parameters are as follows:

        ========	================	================
	Register	IN			OUT
        ========	================	================
	r0		-			volatile
	r3		1st parameter		Return code
	r4		2nd parameter		1st output value
	r5		3rd parameter		2nd output value
	r6		4th parameter		3rd output value
	r7		5th parameter		4th output value
	r8		6th parameter		5th output value
	r9		7th parameter		6th output value
	r10		8th parameter		7th output value
	r11		hypercall number	8th output value
	r12		-			volatile
        ========	================	================

Hypercall definitions are shared in generic code, so the woke same hypercall numbers
apply for x86 and powerpc alike with the woke exception that each KVM hypercall
also needs to be ORed with the woke KVM vendor code which is (42 << 16).

Return codes can be as follows:

	====		=========================
	Code		Meaning
	====		=========================
	0		Success
	12		Hypercall not implemented
	<0		Error
	====		=========================

The magic page
==============

To enable communication between the woke hypervisor and guest there is a new shared
page that contains parts of supervisor visible register state. The guest can
map this shared page using the woke KVM hypercall KVM_HC_PPC_MAP_MAGIC_PAGE.

With this hypercall issued the woke guest always gets the woke magic page mapped at the
desired location. The first parameter indicates the woke effective address when the
MMU is enabled. The second parameter indicates the woke address in real mode, if
applicable to the woke target. For now, we always map the woke page to -4096. This way we
can access it using absolute load and store functions. The following
instruction reads the woke first field of the woke magic page::

	ld	rX, -4096(0)

The interface is designed to be extensible should there be need later to add
additional registers to the woke magic page. If you add fields to the woke magic page,
also define a new hypercall feature to indicate that the woke host can give you more
registers. Only if the woke host supports the woke additional features, make use of them.

The magic page layout is described by struct kvm_vcpu_arch_shared
in arch/powerpc/include/uapi/asm/kvm_para.h.

Magic page features
===================

When mapping the woke magic page using the woke KVM hypercall KVM_HC_PPC_MAP_MAGIC_PAGE,
a second return value is passed to the woke guest. This second return value contains
a bitmap of available features inside the woke magic page.

The following enhancements to the woke magic page are currently available:

  ============================  =======================================
  KVM_MAGIC_FEAT_SR		Maps SR registers r/w in the woke magic page
  KVM_MAGIC_FEAT_MAS0_TO_SPRG7	Maps MASn, ESR, PIR and high SPRGs
  ============================  =======================================

For enhanced features in the woke magic page, please check for the woke existence of the
feature before using them!

Magic page flags
================

In addition to features that indicate whether a host is capable of a particular
feature we also have a channel for a guest to tell the woke host whether it's capable
of something. This is what we call "flags".

Flags are passed to the woke host in the woke low 12 bits of the woke Effective Address.

The following flags are currently available for a guest to expose:

  MAGIC_PAGE_FLAG_NOT_MAPPED_NX Guest handles NX bits correctly wrt magic page

MSR bits
========

The MSR contains bits that require hypervisor intervention and bits that do
not require direct hypervisor intervention because they only get interpreted
when entering the woke guest or don't have any impact on the woke hypervisor's behavior.

The following bits are safe to be set inside the woke guest:

  - MSR_EE
  - MSR_RI

If any other bit changes in the woke MSR, please still use mtmsr(d).

Patched instructions
====================

The "ld" and "std" instructions are transformed to "lwz" and "stw" instructions
respectively on 32-bit systems with an added offset of 4 to accommodate for big
endianness.

The following is a list of mapping the woke Linux kernel performs when running as
guest. Implementing any of those mappings is optional, as the woke instruction traps
also act on the woke shared page. So calling privileged instructions still works as
before.

======================= ================================
From			To
======================= ================================
mfmsr	rX		ld	rX, magic_page->msr
mfsprg	rX, 0		ld	rX, magic_page->sprg0
mfsprg	rX, 1		ld	rX, magic_page->sprg1
mfsprg	rX, 2		ld	rX, magic_page->sprg2
mfsprg	rX, 3		ld	rX, magic_page->sprg3
mfsrr0	rX		ld	rX, magic_page->srr0
mfsrr1	rX		ld	rX, magic_page->srr1
mfdar	rX		ld	rX, magic_page->dar
mfdsisr	rX		lwz	rX, magic_page->dsisr

mtmsr	rX		std	rX, magic_page->msr
mtsprg	0, rX		std	rX, magic_page->sprg0
mtsprg	1, rX		std	rX, magic_page->sprg1
mtsprg	2, rX		std	rX, magic_page->sprg2
mtsprg	3, rX		std	rX, magic_page->sprg3
mtsrr0	rX		std	rX, magic_page->srr0
mtsrr1	rX		std	rX, magic_page->srr1
mtdar	rX		std	rX, magic_page->dar
mtdsisr	rX		stw	rX, magic_page->dsisr

tlbsync			nop

mtmsrd	rX, 0		b	<special mtmsr section>
mtmsr	rX		b	<special mtmsr section>

mtmsrd	rX, 1		b	<special mtmsrd section>

[Book3S only]
mtsrin	rX, rY		b	<special mtsrin section>

[BookE only]
wrteei	[0|1]		b	<special wrteei section>
======================= ================================

Some instructions require more logic to determine what's going on than a load
or store instruction can deliver. To enable patching of those, we keep some
RAM around where we can live translate instructions to. What happens is the
following:

	1) copy emulation code to memory
	2) patch that code to fit the woke emulated instruction
	3) patch that code to return to the woke original pc + 4
	4) patch the woke original instruction to branch to the woke new code

That way we can inject an arbitrary amount of code as replacement for a single
instruction. This allows us to check for pending interrupts when setting EE=1
for example.

Hypercall ABIs in KVM on PowerPC
=================================

1) KVM hypercalls (ePAPR)

These are ePAPR compliant hypercall implementation (mentioned above). Even
generic hypercalls are implemented here, like the woke ePAPR idle hcall. These are
available on all targets.

2) PAPR hypercalls

PAPR hypercalls are needed to run server PowerPC PAPR guests (-M pseries in QEMU).
These are the woke same hypercalls that pHyp, the woke POWER hypervisor, implements. Some of
them are handled in the woke kernel, some are handled in user space. This is only
available on book3s_64.

3) OSI hypercalls

Mac-on-Linux is another user of KVM on PowerPC, which has its own hypercall (long
before KVM). This is supported to maintain compatibility. All these hypercalls get
forwarded to user space. This is only useful on book3s_32, but can be used with
book3s_64 as well.
