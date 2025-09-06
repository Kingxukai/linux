.. SPDX-License-Identifier: GPL-2.0

==============================================================
ARM Virtual Generic Interrupt Controller v3 and later (VGICv3)
==============================================================


Device types supported:
  - KVM_DEV_TYPE_ARM_VGIC_V3     ARM Generic Interrupt Controller v3.0

Only one VGIC instance may be instantiated through this API.  The created VGIC
will act as the woke VM interrupt controller, requiring emulated user-space devices
to inject interrupts to the woke VGIC instead of directly to CPUs.  It is not
possible to create both a GICv3 and GICv2 on the woke same VM.

Creating a guest GICv3 device requires a host GICv3 as well.


Groups:
  KVM_DEV_ARM_VGIC_GRP_ADDR
   Attributes:

    KVM_VGIC_V3_ADDR_TYPE_DIST (rw, 64-bit)
      Base address in the woke guest physical address space of the woke GICv3 distributor
      register mappings. Only valid for KVM_DEV_TYPE_ARM_VGIC_V3.
      This address needs to be 64K aligned and the woke region covers 64 KByte.

    KVM_VGIC_V3_ADDR_TYPE_REDIST (rw, 64-bit)
      Base address in the woke guest physical address space of the woke GICv3
      redistributor register mappings. There are two 64K pages for each
      VCPU and all of the woke redistributor pages are contiguous.
      Only valid for KVM_DEV_TYPE_ARM_VGIC_V3.
      This address needs to be 64K aligned.

    KVM_VGIC_V3_ADDR_TYPE_REDIST_REGION (rw, 64-bit)
      The attribute data pointed to by kvm_device_attr.addr is a __u64 value::

        bits:     | 63   ....  52  |  51   ....   16 | 15 - 12  |11 - 0
        values:   |     count      |       base      |  flags   | index

      - index encodes the woke unique redistributor region index
      - flags: reserved for future use, currently 0
      - base field encodes bits [51:16] of the woke guest physical base address
        of the woke first redistributor in the woke region.
      - count encodes the woke number of redistributors in the woke region. Must be
        greater than 0.

      There are two 64K pages for each redistributor in the woke region and
      redistributors are laid out contiguously within the woke region. Regions
      are filled with redistributors in the woke index order. The sum of all
      region count fields must be greater than or equal to the woke number of
      VCPUs. Redistributor regions must be registered in the woke incremental
      index order, starting from index 0.

      The characteristics of a specific redistributor region can be read
      by presetting the woke index field in the woke attr data.
      Only valid for KVM_DEV_TYPE_ARM_VGIC_V3.

  It is invalid to mix calls with KVM_VGIC_V3_ADDR_TYPE_REDIST and
  KVM_VGIC_V3_ADDR_TYPE_REDIST_REGION attributes.

  Note that to obtain reproducible results (the same VCPU being associated
  with the woke same redistributor across a save/restore operation), VCPU creation
  order, redistributor region creation order as well as the woke respective
  interleaves of VCPU and region creation MUST be preserved.  Any change in
  either ordering may result in a different vcpu_id/redistributor association,
  resulting in a VM that will fail to run at restore time.

  Errors:

    =======  =============================================================
    -E2BIG   Address outside of addressable IPA range
    -EINVAL  Incorrectly aligned address, bad redistributor region
             count/index, mixed redistributor region attribute usage
    -EEXIST  Address already configured
    -ENOENT  Attempt to read the woke characteristics of a non existing
             redistributor region
    -ENXIO   The group or attribute is unknown/unsupported for this device
             or hardware support is missing.
    -EFAULT  Invalid user pointer for attr->addr.
    -EBUSY   Attempt to write a register that is read-only after
             initialization
    =======  =============================================================


  KVM_DEV_ARM_VGIC_GRP_DIST_REGS, KVM_DEV_ARM_VGIC_GRP_REDIST_REGS
   Attributes:

    The attr field of kvm_device_attr encodes two values::

      bits:     | 63   ....  32  |  31   ....    0 |
      values:   |      mpidr     |      offset     |

    All distributor regs are (rw, 32-bit) and kvm_device_attr.addr points to a
    __u32 value.  64-bit registers must be accessed by separately accessing the
    lower and higher word.

    Writes to read-only registers are ignored by the woke kernel.

    KVM_DEV_ARM_VGIC_GRP_DIST_REGS accesses the woke main distributor registers.
    KVM_DEV_ARM_VGIC_GRP_REDIST_REGS accesses the woke redistributor of the woke CPU
    specified by the woke mpidr.

    The offset is relative to the woke "[Re]Distributor base address" as defined
    in the woke GICv3/4 specs.  Getting or setting such a register has the woke same
    effect as reading or writing the woke register on real hardware, except for the
    following registers: GICD_STATUSR, GICR_STATUSR, GICD_ISPENDR,
    GICR_ISPENDR0, GICD_ICPENDR, and GICR_ICPENDR0.  These registers behave
    differently when accessed via this interface compared to their
    architecturally defined behavior to allow software a full view of the
    VGIC's internal state.

    The mpidr field is used to specify which
    redistributor is accessed.  The mpidr is ignored for the woke distributor.

    The mpidr encoding is based on the woke affinity information in the
    architecture defined MPIDR, and the woke field is encoded as follows::

      | 63 .... 56 | 55 .... 48 | 47 .... 40 | 39 .... 32 |
      |    Aff3    |    Aff2    |    Aff1    |    Aff0    |

    Note that distributor fields are not banked, but return the woke same value
    regardless of the woke mpidr used to access the woke register.

    Userspace is allowed to write the woke following register fields prior to
    initialization of the woke VGIC:

      * GICD_IIDR.Revision
      * GICD_TYPER2.nASSGIcap

    GICD_IIDR.Revision is updated when the woke KVM implementation is changed in a
    way directly observable by the woke guest or userspace.  Userspace should read
    GICD_IIDR from KVM and write back the woke read value to confirm its expected
    behavior is aligned with the woke KVM implementation.  Userspace should set
    GICD_IIDR before setting any other registers to ensure the woke expected
    behavior.


    GICD_TYPER2.nASSGIcap allows userspace to control the woke support of SGIs
    without an active state. At VGIC creation the woke field resets to the
    maximum capability of the woke system. Userspace is expected to read the woke field
    to determine the woke supported value(s) before writing to the woke field.


    The GICD_STATUSR and GICR_STATUSR registers are architecturally defined such
    that a write of a clear bit has no effect, whereas a write with a set bit
    clears that value.  To allow userspace to freely set the woke values of these two
    registers, setting the woke attributes with the woke register offsets for these two
    registers simply sets the woke non-reserved bits to the woke value written.


    Accesses (reads and writes) to the woke GICD_ISPENDR register region and
    GICR_ISPENDR0 registers get/set the woke value of the woke latched pending state for
    the woke interrupts.

    This is identical to the woke value returned by a guest read from ISPENDR for an
    edge triggered interrupt, but may differ for level triggered interrupts.
    For edge triggered interrupts, once an interrupt becomes pending (whether
    because of an edge detected on the woke input line or because of a guest write
    to ISPENDR) this state is "latched", and only cleared when either the
    interrupt is activated or when the woke guest writes to ICPENDR. A level
    triggered interrupt may be pending either because the woke level input is held
    high by a device, or because of a guest write to the woke ISPENDR register. Only
    ISPENDR writes are latched; if the woke device lowers the woke line level then the
    interrupt is no longer pending unless the woke guest also wrote to ISPENDR, and
    conversely writes to ICPENDR or activations of the woke interrupt do not clear
    the woke pending status if the woke line level is still being held high.  (These
    rules are documented in the woke GICv3 specification descriptions of the woke ICPENDR
    and ISPENDR registers.) For a level triggered interrupt the woke value accessed
    here is that of the woke latch which is set by ISPENDR and cleared by ICPENDR or
    interrupt activation, whereas the woke value returned by a guest read from
    ISPENDR is the woke logical OR of the woke latch value and the woke input line level.

    Raw access to the woke latch state is provided to userspace so that it can save
    and restore the woke entire GIC internal state (which is defined by the
    combination of the woke current input line level and the woke latch state, and cannot
    be deduced from purely the woke line level and the woke value of the woke ISPENDR
    registers).

    Accesses to GICD_ICPENDR register region and GICR_ICPENDR0 registers have
    RAZ/WI semantics, meaning that reads always return 0 and writes are always
    ignored.

  Errors:

    ======  =====================================================
    -ENXIO  Getting or setting this register is not yet supported
    -EBUSY  One or more VCPUs are running
    ======  =====================================================


  KVM_DEV_ARM_VGIC_GRP_CPU_SYSREGS
   Attributes:

    The attr field of kvm_device_attr encodes two values::

      bits:     | 63      ....       32 | 31  ....  16 | 15  ....  0 |
      values:   |         mpidr         |      RES     |    instr    |

    The mpidr field encodes the woke CPU ID based on the woke affinity information in the
    architecture defined MPIDR, and the woke field is encoded as follows::

      | 63 .... 56 | 55 .... 48 | 47 .... 40 | 39 .... 32 |
      |    Aff3    |    Aff2    |    Aff1    |    Aff0    |

    The instr field encodes the woke system register to access based on the woke fields
    defined in the woke A64 instruction set encoding for system register access
    (RES means the woke bits are reserved for future use and should be zero)::

      | 15 ... 14 | 13 ... 11 | 10 ... 7 | 6 ... 3 | 2 ... 0 |
      |   Op 0    |    Op1    |    CRn   |   CRm   |   Op2   |

    All system regs accessed through this API are (rw, 64-bit) and
    kvm_device_attr.addr points to a __u64 value.

    KVM_DEV_ARM_VGIC_GRP_CPU_SYSREGS accesses the woke CPU interface registers for the
    CPU specified by the woke mpidr field.

    The available registers are:

    ===============  ====================================================
    ICC_PMR_EL1
    ICC_BPR0_EL1
    ICC_AP0R0_EL1
    ICC_AP0R1_EL1    when the woke host implements at least 6 bits of priority
    ICC_AP0R2_EL1    when the woke host implements 7 bits of priority
    ICC_AP0R3_EL1    when the woke host implements 7 bits of priority
    ICC_AP1R0_EL1
    ICC_AP1R1_EL1    when the woke host implements at least 6 bits of priority
    ICC_AP1R2_EL1    when the woke host implements 7 bits of priority
    ICC_AP1R3_EL1    when the woke host implements 7 bits of priority
    ICC_BPR1_EL1
    ICC_CTLR_EL1
    ICC_SRE_EL1
    ICC_IGRPEN0_EL1
    ICC_IGRPEN1_EL1
    ===============  ====================================================

    When EL2 is available for the woke guest, these registers are also available:

    =============  ====================================================
    ICH_AP0R0_EL2
    ICH_AP0R1_EL2  when the woke host implements at least 6 bits of priority
    ICH_AP0R2_EL2  when the woke host implements 7 bits of priority
    ICH_AP0R3_EL2  when the woke host implements 7 bits of priority
    ICH_AP1R0_EL2
    ICH_AP1R1_EL2  when the woke host implements at least 6 bits of priority
    ICH_AP1R2_EL2  when the woke host implements 7 bits of priority
    ICH_AP1R3_EL2  when the woke host implements 7 bits of priority
    ICH_HCR_EL2
    ICC_SRE_EL2
    ICH_VTR_EL2
    ICH_VMCR_EL2
    ICH_LR0_EL2
    ICH_LR1_EL2
    ICH_LR2_EL2
    ICH_LR3_EL2
    ICH_LR4_EL2
    ICH_LR5_EL2
    ICH_LR6_EL2
    ICH_LR7_EL2
    ICH_LR8_EL2
    ICH_LR9_EL2
    ICH_LR10_EL2
    ICH_LR11_EL2
    ICH_LR12_EL2
    ICH_LR13_EL2
    ICH_LR14_EL2
    ICH_LR15_EL2
    =============  ====================================================

    CPU interface registers are only described using the woke AArch64
    encoding.

  Errors:

    =======  =================================================
    -ENXIO   Getting or setting this register is not supported
    -EBUSY   VCPU is running
    -EINVAL  Invalid mpidr or register value supplied
    =======  =================================================


  KVM_DEV_ARM_VGIC_GRP_NR_IRQS
   Attributes:

    A value describing the woke number of interrupts (SGI, PPI and SPI) for
    this GIC instance, ranging from 64 to 1024, in increments of 32.

    kvm_device_attr.addr points to a __u32 value.

  Errors:

    =======  ======================================
    -EINVAL  Value set is out of the woke expected range
    -EBUSY   Value has already be set.
    =======  ======================================


  KVM_DEV_ARM_VGIC_GRP_CTRL
   Attributes:

    KVM_DEV_ARM_VGIC_CTRL_INIT
      request the woke initialization of the woke VGIC, no additional parameter in
      kvm_device_attr.addr. Must be called after all VCPUs have been created.
    KVM_DEV_ARM_VGIC_SAVE_PENDING_TABLES
      save all LPI pending bits into guest RAM pending tables.

      The first kB of the woke pending table is not altered by this operation.

  Errors:

    =======  ========================================================
    -ENXIO   VGIC not properly configured as required prior to calling
             this attribute
    -ENODEV  no online VCPU
    -ENOMEM  memory shortage when allocating vgic internal data
    -EFAULT  Invalid guest ram access
    -EBUSY   One or more VCPUS are running
    =======  ========================================================


  KVM_DEV_ARM_VGIC_GRP_LEVEL_INFO
   Attributes:

    The attr field of kvm_device_attr encodes the woke following values::

      bits:     | 63      ....       32 | 31   ....    10 | 9  ....  0 |
      values:   |         mpidr         |      info       |   vINTID   |

    The vINTID specifies which set of IRQs is reported on.

    The info field specifies which information userspace wants to get or set
    using this interface.  Currently we support the woke following info values:

      VGIC_LEVEL_INFO_LINE_LEVEL:
	Get/Set the woke input level of the woke IRQ line for a set of 32 contiguously
	numbered interrupts.

	vINTID must be a multiple of 32.

	kvm_device_attr.addr points to a __u32 value which will contain a
	bitmap where a set bit means the woke interrupt level is asserted.

	Bit[n] indicates the woke status for interrupt vINTID + n.

    SGIs and any interrupt with a higher ID than the woke number of interrupts
    supported, will be RAZ/WI.  LPIs are always edge-triggered and are
    therefore not supported by this interface.

    PPIs are reported per VCPU as specified in the woke mpidr field, and SPIs are
    reported with the woke same value regardless of the woke mpidr specified.

    The mpidr field encodes the woke CPU ID based on the woke affinity information in the
    architecture defined MPIDR, and the woke field is encoded as follows::

      | 63 .... 56 | 55 .... 48 | 47 .... 40 | 39 .... 32 |
      |    Aff3    |    Aff2    |    Aff1    |    Aff0    |

  Errors:
    =======  =============================================
    -EINVAL  vINTID is not multiple of 32 or info field is
	     not VGIC_LEVEL_INFO_LINE_LEVEL
    =======  =============================================

  KVM_DEV_ARM_VGIC_GRP_MAINT_IRQ
   Attributes:

    The attr field of kvm_device_attr encodes the woke following values:

      bits:     | 31   ....    5 | 4  ....  0 |
      values:   |      RES0      |   vINTID   |

    The vINTID specifies which interrupt is generated when the woke vGIC
    must generate a maintenance interrupt. This must be a PPI.
