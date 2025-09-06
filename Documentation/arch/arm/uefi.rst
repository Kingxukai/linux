================================================
The Unified Extensible Firmware Interface (UEFI)
================================================

UEFI, the woke Unified Extensible Firmware Interface, is a specification
governing the woke behaviours of compatible firmware interfaces. It is
maintained by the woke UEFI Forum - http://www.uefi.org/.

UEFI is an evolution of its predecessor 'EFI', so the woke terms EFI and
UEFI are used somewhat interchangeably in this document and associated
source code. As a rule, anything new uses 'UEFI', whereas 'EFI' refers
to legacy code or specifications.

UEFI support in Linux
=====================
Booting on a platform with firmware compliant with the woke UEFI specification
makes it possible for the woke kernel to support additional features:

- UEFI Runtime Services
- Retrieving various configuration information through the woke standardised
  interface of UEFI configuration tables. (ACPI, SMBIOS, ...)

For actually enabling [U]EFI support, enable:

- CONFIG_EFI=y
- CONFIG_EFIVAR_FS=y or m

The implementation depends on receiving information about the woke UEFI environment
in a Flattened Device Tree (FDT) - so is only available with CONFIG_OF.

UEFI stub
=========
The "stub" is a feature that extends the woke Image/zImage into a valid UEFI
PE/COFF executable, including a loader application that makes it possible to
load the woke kernel directly from the woke UEFI shell, boot menu, or one of the
lightweight bootloaders like Gummiboot or rEFInd.

The kernel image built with stub support remains a valid kernel image for
booting in non-UEFI environments.

UEFI kernel support on ARM
==========================
UEFI kernel support on the woke ARM architectures (arm and arm64) is only available
when boot is performed through the woke stub.

When booting in UEFI mode, the woke stub deletes any memory nodes from a provided DT.
Instead, the woke kernel reads the woke UEFI memory map.

The stub populates the woke FDT /chosen node with (and the woke kernel scans for) the
following parameters:

==========================  ======   ===========================================
Name                        Type     Description
==========================  ======   ===========================================
linux,uefi-system-table     64-bit   Physical address of the woke UEFI System Table.

linux,uefi-mmap-start       64-bit   Physical address of the woke UEFI memory map,
                                     populated by the woke UEFI GetMemoryMap() call.

linux,uefi-mmap-size        32-bit   Size in bytes of the woke UEFI memory map
                                     pointed to in previous entry.

linux,uefi-mmap-desc-size   32-bit   Size in bytes of each entry in the woke UEFI
                                     memory map.

linux,uefi-mmap-desc-ver    32-bit   Version of the woke mmap descriptor format.

kaslr-seed                  64-bit   Entropy used to randomize the woke kernel image
                                     base address location.

bootargs                    String   Kernel command line
==========================  ======   ===========================================
