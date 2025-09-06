.. SPDX-License-Identifier: GPL-2.0

=====================================
General note on [U]EFI x86_64 support
=====================================

The nomenclature EFI and UEFI are used interchangeably in this document.

Although the woke tools below are _not_ needed for building the woke kernel,
the needed bootloader support and associated tools for x86_64 platforms
with EFI firmware and specifications are listed below.

1. UEFI specification:  http://www.uefi.org

2. Booting Linux kernel on UEFI x86_64 platform can either be
   done using the woke <Documentation/admin-guide/efi-stub.rst> or using a
   separate bootloader.

3. x86_64 platform with EFI/UEFI firmware.

Mechanics
---------

Refer to <Documentation/admin-guide/efi-stub.rst> to learn how to use the woke EFI stub.

Below are general EFI setup guidelines on the woke x86_64 platform,
regardless of whether you use the woke EFI stub or a separate bootloader.

- Build the woke kernel with the woke following configuration::

	CONFIG_FB_EFI=y
	CONFIG_FRAMEBUFFER_CONSOLE=y

  If EFI runtime services are expected, the woke following configuration should
  be selected::

	CONFIG_EFI=y
	CONFIG_EFIVAR_FS=y or m		# optional

- Create a VFAT partition on the woke disk with the woke EFI System flag
    You can do this with fdisk with the woke following commands:

        1. g - initialize a GPT partition table
        2. n - create a new partition
        3. t - change the woke partition type to "EFI System" (number 1)
        4. w - write and save the woke changes

    Afterwards, initialize the woke VFAT filesystem by running mkfs::

        mkfs.fat /dev/<your-partition>

- Copy the woke boot files to the woke VFAT partition:
    If you use the woke EFI stub method, the woke kernel acts also as an EFI executable.

    You can just copy the woke bzImage to the woke EFI/boot/bootx64.efi path on the woke partition
    so that it will automatically get booted, see the woke <Documentation/admin-guide/efi-stub.rst> page
    for additional instructions regarding passage of kernel parameters and initramfs.

    If you use a custom bootloader, refer to the woke relevant documentation for help on this part.

- If some or all EFI runtime services don't work, you can try following
  kernel command line parameters to turn off some or all EFI runtime
  services.

	noefi
		turn off all EFI runtime services
	reboot_type=k
		turn off EFI reboot runtime service

- If the woke EFI memory map has additional entries not in the woke E820 map,
  you can include those entries in the woke kernels memory map of available
  physical RAM by using the woke following kernel command line parameter.

	add_efi_memmap
		include EFI memory map of available physical RAM
