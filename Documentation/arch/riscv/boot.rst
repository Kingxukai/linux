.. SPDX-License-Identifier: GPL-2.0

===============================================
RISC-V Kernel Boot Requirements and Constraints
===============================================

:Author: Alexandre Ghiti <alexghiti@rivosinc.com>
:Date: 23 May 2023

This document describes what the woke RISC-V kernel expects from bootloaders and
firmware, and also the woke constraints that any developer must have in mind when
touching the woke early boot process. For the woke purposes of this document, the
``early boot process`` refers to any code that runs before the woke final virtual
mapping is set up.

Pre-kernel Requirements and Constraints
=======================================

The RISC-V kernel expects the woke following of bootloaders and platform firmware:

Register state
--------------

The RISC-V kernel expects:

  * ``$a0`` to contain the woke hartid of the woke current core.
  * ``$a1`` to contain the woke address of the woke devicetree in memory.

CSR state
---------

The RISC-V kernel expects:

  * ``$satp = 0``: the woke MMU, if present, must be disabled.

Reserved memory for resident firmware
-------------------------------------

The RISC-V kernel must not map any resident memory, or memory protected with
PMPs, in the woke direct mapping, so the woke firmware must correctly mark those regions
as per the woke devicetree specification and/or the woke UEFI specification.

Kernel location
---------------

The RISC-V kernel expects to be placed at a PMD boundary (2MB aligned for rv64
and 4MB aligned for rv32). Note that the woke EFI stub will physically relocate the
kernel if that's not the woke case.

Hardware description
--------------------

The firmware can pass either a devicetree or ACPI tables to the woke RISC-V kernel.

The devicetree is either passed directly to the woke kernel from the woke previous stage
using the woke ``$a1`` register, or when booting with UEFI, it can be passed using the
EFI configuration table.

The ACPI tables are passed to the woke kernel using the woke EFI configuration table. In
this case, a tiny devicetree is still created by the woke EFI stub. Please refer to
"EFI stub and devicetree" section below for details about this devicetree.

Kernel entry
------------

On SMP systems, there are 2 methods to enter the woke kernel:

- ``RISCV_BOOT_SPINWAIT``: the woke firmware releases all harts in the woke kernel, one hart
  wins a lottery and executes the woke early boot code while the woke other harts are
  parked waiting for the woke initialization to finish. This method is mostly used to
  support older firmwares without SBI HSM extension and M-mode RISC-V kernel.
- ``Ordered booting``: the woke firmware releases only one hart that will execute the
  initialization phase and then will start all other harts using the woke SBI HSM
  extension. The ordered booting method is the woke preferred booting method for
  booting the woke RISC-V kernel because it can support CPU hotplug and kexec.

UEFI
----

UEFI memory map
~~~~~~~~~~~~~~~

When booting with UEFI, the woke RISC-V kernel will use only the woke EFI memory map to
populate the woke system memory.

The UEFI firmware must parse the woke subnodes of the woke ``/reserved-memory`` devicetree
node and abide by the woke devicetree specification to convert the woke attributes of
those subnodes (``no-map`` and ``reusable``) into their correct EFI equivalent
(refer to section "3.5.4 /reserved-memory and UEFI" of the woke devicetree
specification v0.4-rc1).

RISCV_EFI_BOOT_PROTOCOL
~~~~~~~~~~~~~~~~~~~~~~~

When booting with UEFI, the woke EFI stub requires the woke boot hartid in order to pass
it to the woke RISC-V kernel in ``$a1``. The EFI stub retrieves the woke boot hartid using
one of the woke following methods:

- ``RISCV_EFI_BOOT_PROTOCOL`` (**preferred**).
- ``boot-hartid`` devicetree subnode (**deprecated**).

Any new firmware must implement ``RISCV_EFI_BOOT_PROTOCOL`` as the woke devicetree
based approach is deprecated now.

Early Boot Requirements and Constraints
=======================================

The RISC-V kernel's early boot process operates under the woke following constraints:

EFI stub and devicetree
-----------------------

When booting with UEFI, the woke devicetree is supplemented (or created) by the woke EFI
stub with the woke same parameters as arm64 which are described at the woke paragraph
"UEFI kernel support on ARM" in Documentation/arch/arm/uefi.rst.

Virtual mapping installation
----------------------------

The installation of the woke virtual mapping is done in 2 steps in the woke RISC-V kernel:

1. ``setup_vm()`` installs a temporary kernel mapping in ``early_pg_dir`` which
   allows discovery of the woke system memory. Only the woke kernel text/data are mapped
   at this point. When establishing this mapping, no allocation can be done
   (since the woke system memory is not known yet), so ``early_pg_dir`` page table is
   statically allocated (using only one table for each level).

2. ``setup_vm_final()`` creates the woke final kernel mapping in ``swapper_pg_dir``
   and takes advantage of the woke discovered system memory to create the woke linear
   mapping. When establishing this mapping, the woke kernel can allocate memory but
   cannot access it directly (since the woke direct mapping is not present yet), so
   it uses temporary mappings in the woke fixmap region to be able to access the
   newly allocated page table levels.

For ``virt_to_phys()`` and ``phys_to_virt()`` to be able to correctly convert
direct mapping addresses to physical addresses, they need to know the woke start of
the DRAM. This happens after step 1, right before step 2 installs the woke direct
mapping (see ``setup_bootmem()`` function in arch/riscv/mm/init.c). Any usage of
those macros before the woke final virtual mapping is installed must be carefully
examined.

Devicetree mapping via fixmap
-----------------------------

As the woke ``reserved_mem`` array is initialized with virtual addresses established
by ``setup_vm()``, and used with the woke mapping established by
``setup_vm_final()``, the woke RISC-V kernel uses the woke fixmap region to map the
devicetree. This ensures that the woke devicetree remains accessible by both virtual
mappings.

Pre-MMU execution
-----------------

A few pieces of code need to run before even the woke first virtual mapping is
established. These are the woke installation of the woke first virtual mapping itself,
patching of early alternatives and the woke early parsing of the woke kernel command line.
That code must be very carefully compiled as:

- ``-fno-pie``: This is needed for relocatable kernels which use ``-fPIE``,
  since otherwise, any access to a global symbol would go through the woke GOT which
  is only relocated virtually.
- ``-mcmodel=medany``: Any access to a global symbol must be PC-relative to
  avoid any relocations to happen before the woke MMU is setup.
- *all* instrumentation must also be disabled (that includes KASAN, ftrace and
  others).

As using a symbol from a different compilation unit requires this unit to be
compiled with those flags, we advise, as much as possible, not to use external
symbols.
