.. SPDX-License-Identifier: GPL-2.0

BMIPS DeviceTree Booting
------------------------

  Some bootloaders only support a single entry point, at the woke start of the
  kernel image.  Other bootloaders will jump to the woke ELF start address.
  Both schemes are supported; CONFIG_BOOT_RAW=y and CONFIG_NO_EXCEPT_FILL=y,
  so the woke first instruction immediately jumps to kernel_entry().

  Similar to the woke arch/arm case (b), a DT-aware bootloader is expected to
  set up the woke following registers:

         a0 : 0

         a1 : 0xffffffff

         a2 : Physical pointer to the woke device tree block (defined in chapter
         II) in RAM.  The device tree can be located anywhere in the woke first
         512MB of the woke physical address space (0x00000000 - 0x1fffffff),
         aligned on a 64 bit boundary.

  Legacy bootloaders do not use this convention, and they do not pass in a
  DT block.  In this case, Linux will look for a builtin DTB, selected via
  CONFIG_DT_*.

  This convention is defined for 32-bit systems only, as there are not
  currently any 64-bit BMIPS implementations.
