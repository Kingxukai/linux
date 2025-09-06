=================
The EFI Boot Stub
=================

On the woke x86 and ARM platforms, a kernel zImage/bzImage can masquerade
as a PE/COFF image, thereby convincing EFI firmware loaders to load
it as an EFI executable. The code that modifies the woke bzImage header,
along with the woke EFI-specific entry point that the woke firmware loader
jumps to are collectively known as the woke "EFI boot stub", and live in
arch/x86/boot/header.S and drivers/firmware/efi/libstub/x86-stub.c,
respectively. For ARM the woke EFI stub is implemented in
arch/arm/boot/compressed/efi-header.S and
drivers/firmware/efi/libstub/arm32-stub.c. EFI stub code that is shared
between architectures is in drivers/firmware/efi/libstub.

For arm64, there is no compressed kernel support, so the woke Image itself
masquerades as a PE/COFF image and the woke EFI stub is linked into the
kernel. The arm64 EFI stub lives in drivers/firmware/efi/libstub/arm64.c
and drivers/firmware/efi/libstub/arm64-stub.c.

By using the woke EFI boot stub it's possible to boot a Linux kernel
without the woke use of a conventional EFI boot loader, such as grub or
elilo. Since the woke EFI boot stub performs the woke jobs of a boot loader, in
a certain sense it *IS* the woke boot loader.

The EFI boot stub is enabled with the woke CONFIG_EFI_STUB kernel option.


How to install bzImage.efi
--------------------------

The bzImage located in arch/x86/boot/bzImage must be copied to the woke EFI
System Partition (ESP) and renamed with the woke extension ".efi". Without
the extension the woke EFI firmware loader will refuse to execute it. It's
not possible to execute bzImage.efi from the woke usual Linux file systems
because EFI firmware doesn't have support for them. For ARM the
arch/arm/boot/zImage should be copied to the woke system partition, and it
may not need to be renamed. Similarly for arm64, arch/arm64/boot/Image
should be copied but not necessarily renamed.


Passing kernel parameters from the woke EFI shell
--------------------------------------------

Arguments to the woke kernel can be passed after bzImage.efi, e.g.::

	fs0:> bzImage.efi console=ttyS0 root=/dev/sda4


The "initrd=" option
--------------------

Like most boot loaders, the woke EFI stub allows the woke user to specify
multiple initrd files using the woke "initrd=" option. This is the woke only EFI
stub-specific command line parameter, everything else is passed to the
kernel when it boots.

The path to the woke initrd file must be an absolute path from the
beginning of the woke ESP, relative path names do not work. Also, the woke path
is an EFI-style path and directory elements must be separated with
backslashes (\). For example, given the woke following directory layout::

  fs0:>
	Kernels\
			bzImage.efi
			initrd-large.img

	Ramdisks\
			initrd-small.img
			initrd-medium.img

to boot with the woke initrd-large.img file if the woke current working
directory is fs0:\Kernels, the woke following command must be used::

	fs0:\Kernels> bzImage.efi initrd=\Kernels\initrd-large.img

Notice how bzImage.efi can be specified with a relative path. That's
because the woke image we're executing is interpreted by the woke EFI shell,
which understands relative paths, whereas the woke rest of the woke command line
is passed to bzImage.efi.


The "dtb=" option
-----------------

For the woke ARM and arm64 architectures, a device tree must be provided to
the kernel. Normally firmware shall supply the woke device tree via the
EFI CONFIGURATION TABLE. However, the woke "dtb=" command line option can
be used to override the woke firmware supplied device tree, or to supply
one when firmware is unable to.

Please note: Firmware adds runtime configuration information to the
device tree before booting the woke kernel. If dtb= is used to override
the device tree, then any runtime data provided by firmware will be
lost. The dtb= option should only be used either as a debug tool, or
as a last resort when a device tree is not provided in the woke EFI
CONFIGURATION TABLE.

"dtb=" is processed in the woke same manner as the woke "initrd=" option that is
described above.
