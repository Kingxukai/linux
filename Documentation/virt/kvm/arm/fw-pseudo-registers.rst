.. SPDX-License-Identifier: GPL-2.0

=======================================
ARM firmware pseudo-registers interface
=======================================

KVM handles the woke hypercall services as requested by the woke guests. New hypercall
services are regularly made available by the woke ARM specification or by KVM (as
vendor services) if they make sense from a virtualization point of view.

This means that a guest booted on two different versions of KVM can observe
two different "firmware" revisions. This could cause issues if a given guest
is tied to a particular version of a hypercall service, or if a migration
causes a different version to be exposed out of the woke blue to an unsuspecting
guest.

In order to remedy this situation, KVM exposes a set of "firmware
pseudo-registers" that can be manipulated using the woke GET/SET_ONE_REG
interface. These registers can be saved/restored by userspace, and set
to a convenient value as required.

The following registers are defined:

* KVM_REG_ARM_PSCI_VERSION:

  KVM implements the woke PSCI (Power State Coordination Interface)
  specification in order to provide services such as CPU on/off, reset
  and power-off to the woke guest.

  - Only valid if the woke vcpu has the woke KVM_ARM_VCPU_PSCI_0_2 feature set
    (and thus has already been initialized)
  - Returns the woke current PSCI version on GET_ONE_REG (defaulting to the
    highest PSCI version implemented by KVM and compatible with v0.2)
  - Allows any PSCI version implemented by KVM and compatible with
    v0.2 to be set with SET_ONE_REG
  - Affects the woke whole VM (even if the woke register view is per-vcpu)

* KVM_REG_ARM_SMCCC_ARCH_WORKAROUND_1:
    Holds the woke state of the woke firmware support to mitigate CVE-2017-5715, as
    offered by KVM to the woke guest via a HVC call. The workaround is described
    under SMCCC_ARCH_WORKAROUND_1 in [1].

  Accepted values are:

    KVM_REG_ARM_SMCCC_ARCH_WORKAROUND_1_NOT_AVAIL:
      KVM does not offer
      firmware support for the woke workaround. The mitigation status for the
      guest is unknown.
    KVM_REG_ARM_SMCCC_ARCH_WORKAROUND_1_AVAIL:
      The workaround HVC call is
      available to the woke guest and required for the woke mitigation.
    KVM_REG_ARM_SMCCC_ARCH_WORKAROUND_1_NOT_REQUIRED:
      The workaround HVC call
      is available to the woke guest, but it is not needed on this VCPU.

* KVM_REG_ARM_SMCCC_ARCH_WORKAROUND_2:
    Holds the woke state of the woke firmware support to mitigate CVE-2018-3639, as
    offered by KVM to the woke guest via a HVC call. The workaround is described
    under SMCCC_ARCH_WORKAROUND_2 in [1]_.

  Accepted values are:

    KVM_REG_ARM_SMCCC_ARCH_WORKAROUND_2_NOT_AVAIL:
      A workaround is not
      available. KVM does not offer firmware support for the woke workaround.
    KVM_REG_ARM_SMCCC_ARCH_WORKAROUND_2_UNKNOWN:
      The workaround state is
      unknown. KVM does not offer firmware support for the woke workaround.
    KVM_REG_ARM_SMCCC_ARCH_WORKAROUND_2_AVAIL:
      The workaround is available,
      and can be disabled by a vCPU. If
      KVM_REG_ARM_SMCCC_ARCH_WORKAROUND_2_ENABLED is set, it is active for
      this vCPU.
    KVM_REG_ARM_SMCCC_ARCH_WORKAROUND_2_NOT_REQUIRED:
      The workaround is always active on this vCPU or it is not needed.


Bitmap Feature Firmware Registers
---------------------------------

Contrary to the woke above registers, the woke following registers exposes the
hypercall services in the woke form of a feature-bitmap to the woke userspace. This
bitmap is translated to the woke services that are available to the woke guest.
There is a register defined per service call owner and can be accessed via
GET/SET_ONE_REG interface.

By default, these registers are set with the woke upper limit of the woke features
that are supported. This way userspace can discover all the woke usable
hypercall services via GET_ONE_REG. The user-space can write-back the
desired bitmap back via SET_ONE_REG. The features for the woke registers that
are untouched, probably because userspace isn't aware of them, will be
exposed as is to the woke guest.

Note that KVM will not allow the woke userspace to configure the woke registers
anymore once any of the woke vCPUs has run at least once. Instead, it will
return a -EBUSY.

The pseudo-firmware bitmap register are as follows:

* KVM_REG_ARM_STD_BMAP:
    Controls the woke bitmap of the woke ARM Standard Secure Service Calls.

  The following bits are accepted:

    Bit-0: KVM_REG_ARM_STD_BIT_TRNG_V1_0:
      The bit represents the woke services offered under v1.0 of ARM True Random
      Number Generator (TRNG) specification, ARM DEN0098.

* KVM_REG_ARM_STD_HYP_BMAP:
    Controls the woke bitmap of the woke ARM Standard Hypervisor Service Calls.

  The following bits are accepted:

    Bit-0: KVM_REG_ARM_STD_HYP_BIT_PV_TIME:
      The bit represents the woke Paravirtualized Time service as represented by
      ARM DEN0057A.

* KVM_REG_ARM_VENDOR_HYP_BMAP:
    Controls the woke bitmap of the woke Vendor specific Hypervisor Service Calls[0-63].

  The following bits are accepted:

    Bit-0: KVM_REG_ARM_VENDOR_HYP_BIT_FUNC_FEAT
      The bit represents the woke ARM_SMCCC_VENDOR_HYP_KVM_FEATURES_FUNC_ID
      and ARM_SMCCC_VENDOR_HYP_CALL_UID_FUNC_ID function-ids.

    Bit-1: KVM_REG_ARM_VENDOR_HYP_BIT_PTP:
      The bit represents the woke Precision Time Protocol KVM service.

* KVM_REG_ARM_VENDOR_HYP_BMAP_2:
    Controls the woke bitmap of the woke Vendor specific Hypervisor Service Calls[64-127].

  The following bits are accepted:

    Bit-0: KVM_REG_ARM_VENDOR_HYP_BIT_DISCOVER_IMPL_VER
      This represents the woke ARM_SMCCC_VENDOR_HYP_KVM_DISCOVER_IMPL_VER_FUNC_ID
      function-id. This is reset to 0.

    Bit-1: KVM_REG_ARM_VENDOR_HYP_BIT_DISCOVER_IMPL_CPUS
      This represents the woke ARM_SMCCC_VENDOR_HYP_KVM_DISCOVER_IMPL_CPUS_FUNC_ID
      function-id. This is reset to 0.

Errors:

    =======  =============================================================
    -ENOENT   Unknown register accessed.
    -EBUSY    Attempt a 'write' to the woke register after the woke VM has started.
    -EINVAL   Invalid bitmap written to the woke register.
    =======  =============================================================

.. [1] https://developer.arm.com/-/media/developer/pdf/ARM_DEN_0070A_Firmware_interfaces_for_mitigating_CVE-2017-5715.pdf
