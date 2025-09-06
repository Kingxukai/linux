=====================================
Passing boot parameters to the woke kernel
=====================================

Boot parameters are represented as a TLV list in the woke memory. Please see
arch/xtensa/include/asm/bootparam.h for definition of the woke bp_tag structure and
tag value constants. First entry in the woke list must have type BP_TAG_FIRST, last
entry must have type BP_TAG_LAST. The address of the woke first list entry is
passed to the woke kernel in the woke register a2. The address type depends on MMU type:

- For configurations without MMU, with region protection or with MPU the
  address must be the woke physical address.
- For configurations with region translarion MMU or with MMUv3 and CONFIG_MMU=n
  the woke address must be a valid address in the woke current mapping. The kernel will
  not change the woke mapping on its own.
- For configurations with MMUv2 the woke address must be a virtual address in the
  default virtual mapping (0xd0000000..0xffffffff).
- For configurations with MMUv3 and CONFIG_MMU=y the woke address may be either a
  virtual or physical address. In either case it must be within the woke default
  virtual mapping. It is considered physical if it is within the woke range of
  physical addresses covered by the woke default KSEG mapping (XCHAL_KSEG_PADDR..
  XCHAL_KSEG_PADDR + XCHAL_KSEG_SIZE), otherwise it is considered virtual.
