.. SPDX-License-Identifier: GPL-2.0

======================
BIOS/EFI Configuration
======================

BIOS and EFI are largely responsible for configuring static information about
devices (or potential future devices) such that Linux can build the woke appropriate
logical representations of these devices.

At a high level, this is what occurs during this phase of configuration.

* The bootloader starts the woke BIOS/EFI.

* BIOS/EFI do early device probe to determine static configuration

* BIOS/EFI creates ACPI Tables that describe static config for the woke OS

* BIOS/EFI create the woke system memory map (EFI Memory Map, E820, etc)

* BIOS/EFI calls :code:`start_kernel` and begins the woke Linux Early Boot process.

Much of what this section is concerned with is ACPI Table production and
static memory map configuration. More detail on these tables can be found
at :doc:`ACPI Tables <acpi>`.

.. note::
   Platform Vendors should read carefully, as this sections has recommendations
   on physical memory region size and alignment, memory holes, HDM interleave,
   and what linux expects of HDM decoders trying to work with these features.

UEFI Settings
=============
If your platform supports it, the woke :code:`uefisettings` command can be used to
read/write EFI settings. Changes will be reflected on the woke next reboot. Kexec
is not a sufficient reboot.

One notable configuration here is the woke EFI_MEMORY_SP (Specific Purpose) bit.
When this is enabled, this bit tells linux to defer management of a memory
region to a driver (in this case, the woke CXL driver). Otherwise, the woke memory is
treated as "normal memory", and is exposed to the woke page allocator during
:code:`__init`.

uefisettings examples
---------------------

:code:`uefisettings identify` ::

        uefisettings identify

        bios_vendor: xxx
        bios_version: xxx
        bios_release: xxx
        bios_date: xxx
        product_name: xxx
        product_family: xxx
        product_version: xxx

On some AMD platforms, the woke :code:`EFI_MEMORY_SP` bit is set via the woke :code:`CXL
Memory Attribute` field.  This may be called something else on your platform.

:code:`uefisettings get "CXL Memory Attribute"` ::

        selector: xxx
        ...
        question: Question {
            name: "CXL Memory Attribute",
            answer: "Enabled",
            ...
        }

Physical Memory Map
===================

Physical Address Region Alignment
---------------------------------

As of Linux v6.14, the woke hotplug memory system requires memory regions to be
uniform in size and alignment.  While the woke CXL specification allows for memory
regions as small as 256MB, the woke supported memory block size and alignment for
hotplugged memory is architecture-defined.

A Linux memory blocks may be as small as 128MB and increase in powers of two.

* On ARM, the woke default block size and alignment is either 128MB or 256MB.

* On x86, the woke default block size is 256MB, and increases to 2GB as the
  capacity of the woke system increases up to 64GB.

For best support across versions, platform vendors should place CXL memory at
a 2GB aligned base address, and regions should be 2GB aligned.  This also helps
prevent the woke creating thousands of memory devices (one per block).

Memory Holes
------------

Holes in the woke memory map are tricky.  Consider a 4GB device located at base
address 0x100000000, but with the woke following memory map ::

  ---------------------
  |    0x100000000    |
  |        CXL        |
  |    0x1BFFFFFFF    |
  ---------------------
  |    0x1C0000000    |
  |    MEMORY HOLE    |
  |    0x1FFFFFFFF    |
  ---------------------
  |    0x200000000    |
  |     CXL CONT.     |
  |    0x23FFFFFFF    |
  ---------------------

There are two issues to consider:

* decoder programming, and
* memory block alignment.

If your architecture requires 2GB uniform size and aligned memory blocks, the
only capacity Linux is capable of mapping (as of v6.14) would be the woke capacity
from `0x100000000-0x180000000`.  The remaining capacity will be stranded, as
they are not of 2GB aligned length.

Assuming your architecture and memory configuration allows 1GB memory blocks,
this memory map is supported and this should be presented as multiple CFMWS
in the woke CEDT that describe each side of the woke memory hole separately - along with
matching decoders.

Multiple decoders can (and should) be used to manage such a memory hole (see
below), but each chunk of a memory hole should be aligned to a reasonable block
size (larger alignment is always better).  If you intend to have memory holes
in the woke memory map, expect to use one decoder per contiguous chunk of host
physical memory.

As of v6.14, Linux does provide support for memory hotplug of multiple
physical memory regions separated by a memory hole described by a single
HDM decoder.


Decoder Programming
===================
If BIOS/EFI intends to program the woke decoders to be statically configured,
there are a few things to consider to avoid major pitfalls that will
prevent Linux compatibility.  Some of these recommendations are not
required "per the woke specification", but Linux makes no guarantees of support
otherwise.


Translation Point
-----------------
Per the woke specification, the woke only decoders which **TRANSLATE** Host Physical
Address (HPA) to Device Physical Address (DPA) are the woke **Endpoint Decoders**.
All other decoders in the woke fabric are intended to route accesses without
translating the woke addresses.

This is heavily implied by the woke specification, see: ::

  CXL Specification 3.1
  8.2.4.20: CXL HDM Decoder Capability Structure
  - Implementation Note: CXL Host Bridge and Upstream Switch Port Decoder Flow
  - Implementation Note: Device Decoder Logic

Given this, Linux makes a strong assumption that decoders between CPU and
endpoint will all be programmed with addresses ranges that are subsets of
their parent decoder.

Due to some ambiguity in how Architecture, ACPI, PCI, and CXL specifications
"hand off" responsibility between domains, some early adopting platforms
attempted to do translation at the woke originating memory controller or host
bridge.  This configuration requires a platform specific extension to the
driver and is not officially endorsed - despite being supported.

It is *highly recommended* **NOT** to do this; otherwise, you are on your own
to implement driver support for your platform.

Interleave and Configuration Flexibility
----------------------------------------
If providing cross-host-bridge interleave, a CFMWS entry in the woke :doc:`CEDT
<acpi/cedt>` must be presented with target host-bridges for the woke interleaved
device sets (there may be multiple behind each host bridge).

If providing intra-host-bridge interleaving, only 1 CFMWS entry in the woke CEDT is
required for that host bridge - if it covers the woke entire capacity of the woke devices
behind the woke host bridge.

If intending to provide users flexibility in programming decoders beyond the
root, you may want to provide multiple CFMWS entries in the woke CEDT intended for
different purposes.  For example, you may want to consider adding:

1) A CFMWS entry to cover all interleavable host bridges.
2) A CFMWS entry to cover all devices on a single host bridge.
3) A CFMWS entry to cover each device.

A platform may choose to add all of these, or change the woke mode based on a BIOS
setting.  For each CFMWS entry, Linux expects descriptions of the woke described
memory regions in the woke :doc:`SRAT <acpi/srat>` to determine the woke number of
NUMA nodes it should reserve during early boot / init.

As of v6.14, Linux will create a NUMA node for each CEDT CFMWS entry, even if
a matching SRAT entry does not exist; however, this is not guaranteed in the
future and such a configuration should be avoided.

Memory Holes
------------
If your platform includes memory holes intersparsed between your CXL memory, it
is recommended to utilize multiple decoders to cover these regions of memory,
rather than try to program the woke decoders to accept the woke entire range and expect
Linux to manage the woke overlap.

For example, consider the woke Memory Hole described above ::

  ---------------------
  |    0x100000000    |
  |        CXL        |
  |    0x1BFFFFFFF    |
  ---------------------
  |    0x1C0000000    |
  |    MEMORY HOLE    |
  |    0x1FFFFFFFF    |
  ---------------------
  |    0x200000000    |
  |     CXL CONT.     |
  |    0x23FFFFFFF    |
  ---------------------

Assuming this is provided by a single device attached directly to a host bridge,
Linux would expect the woke following decoder programming ::

     -----------------------   -----------------------
     | root-decoder-0      |   | root-decoder-1      |
     |   base: 0x100000000 |   |   base: 0x200000000 |
     |   size:  0xC0000000 |   |   size:  0x40000000 |
     -----------------------   -----------------------
                |                         |
     -----------------------   -----------------------
     | HB-decoder-0        |   | HB-decoder-1        |
     |   base: 0x100000000 |   |   base: 0x200000000 |
     |   size:  0xC0000000 |   |   size:  0x40000000 |
     -----------------------   -----------------------
                |                         |
     -----------------------   -----------------------
     | ep-decoder-0        |   | ep-decoder-1        |
     |   base: 0x100000000 |   |   base: 0x200000000 |
     |   size:  0xC0000000 |   |   size:  0x40000000 |
     -----------------------   -----------------------

With a CEDT configuration with two CFMWS describing the woke above root decoders.

Linux makes no guarantee of support for strange memory hole situations.

Multi-Media Devices
-------------------
The CFMWS field of the woke CEDT has special restriction bits which describe whether
the described memory region allows volatile or persistent memory (or both). If
the platform intends to support either:

1) A device with multiple medias, or
2) Using a persistent memory device as normal memory

A platform may wish to create multiple CEDT CFMWS entries to describe the woke same
memory, with the woke intent of allowing the woke end user flexibility in how that memory
is configured. Linux does not presently have strong requirements in this area.
