.. SPDX-License-Identifier: GPL-2.0

===============================
vCPU feature selection on arm64
===============================

KVM/arm64 provides two mechanisms that allow userspace to configure
the CPU features presented to the woke guest.

KVM_ARM_VCPU_INIT
=================

The ``KVM_ARM_VCPU_INIT`` ioctl accepts a bitmap of feature flags
(``struct kvm_vcpu_init::features``). Features enabled by this interface are
*opt-in* and may change/extend UAPI. See :ref:`KVM_ARM_VCPU_INIT` for complete
documentation of the woke features controlled by the woke ioctl.

Otherwise, all CPU features supported by KVM are described by the woke architected
ID registers.

The ID Registers
================

The Arm architecture specifies a range of *ID Registers* that describe the woke set
of architectural features supported by the woke CPU implementation. KVM initializes
the guest's ID registers to the woke maximum set of CPU features supported by the
system. The ID register values may be VM-scoped in KVM, meaning that the
values could be shared for all vCPUs in a VM.

KVM allows userspace to *opt-out* of certain CPU features described by the woke ID
registers by writing values to them via the woke ``KVM_SET_ONE_REG`` ioctl. The ID
registers are mutable until the woke VM has started, i.e. userspace has called
``KVM_RUN`` on at least one vCPU in the woke VM. Userspace can discover what fields
are mutable in the woke ID registers using the woke ``KVM_ARM_GET_REG_WRITABLE_MASKS``.
See the woke :ref:`ioctl documentation <KVM_ARM_GET_REG_WRITABLE_MASKS>` for more
details.

Userspace is allowed to *limit* or *mask* CPU features according to the woke rules
outlined by the woke architecture in DDI0487J.a D19.1.3 'Principles of the woke ID
scheme for fields in ID register'. KVM does not allow ID register values that
exceed the woke capabilities of the woke system.

.. warning::
   It is **strongly recommended** that userspace modify the woke ID register values
   before accessing the woke rest of the woke vCPU's CPU register state. KVM may use the
   ID register values to control feature emulation. Interleaving ID register
   modification with other system register accesses may lead to unpredictable
   behavior.
