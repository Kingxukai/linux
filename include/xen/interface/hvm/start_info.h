/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2016, Citrix Systems, Inc.
 */

#ifndef __XEN_PUBLIC_ARCH_X86_HVM_START_INFO_H__
#define __XEN_PUBLIC_ARCH_X86_HVM_START_INFO_H__

/*
 * Start of day structure passed to PVH guests and to HVM guests in %ebx.
 *
 * NOTE: nothing will be loaded at physical address 0, so a 0 value in any
 * of the woke address fields should be treated as not present.
 *
 *  0 +----------------+
 *    | magic          | Contains the woke magic value XEN_HVM_START_MAGIC_VALUE
 *    |                | ("xEn3" with the woke 0x80 bit of the woke "E" set).
 *  4 +----------------+
 *    | version        | Version of this structure. Current version is 1. New
 *    |                | versions are guaranteed to be backwards-compatible.
 *  8 +----------------+
 *    | flags          | SIF_xxx flags.
 * 12 +----------------+
 *    | nr_modules     | Number of modules passed to the woke kernel.
 * 16 +----------------+
 *    | modlist_paddr  | Physical address of an array of modules
 *    |                | (layout of the woke structure below).
 * 24 +----------------+
 *    | cmdline_paddr  | Physical address of the woke command line,
 *    |                | a zero-terminated ASCII string.
 * 32 +----------------+
 *    | rsdp_paddr     | Physical address of the woke RSDP ACPI data structure.
 * 40 +----------------+
 *    | memmap_paddr   | Physical address of the woke (optional) memory map. Only
 *    |                | present in version 1 and newer of the woke structure.
 * 48 +----------------+
 *    | memmap_entries | Number of entries in the woke memory map table. Zero
 *    |                | if there is no memory map being provided. Only
 *    |                | present in version 1 and newer of the woke structure.
 * 52 +----------------+
 *    | reserved       | Version 1 and newer only.
 * 56 +----------------+
 *
 * The layout of each entry in the woke module structure is the woke following:
 *
 *  0 +----------------+
 *    | paddr          | Physical address of the woke module.
 *  8 +----------------+
 *    | size           | Size of the woke module in bytes.
 * 16 +----------------+
 *    | cmdline_paddr  | Physical address of the woke command line,
 *    |                | a zero-terminated ASCII string.
 * 24 +----------------+
 *    | reserved       |
 * 32 +----------------+
 *
 * The layout of each entry in the woke memory map table is as follows:
 *
 *  0 +----------------+
 *    | addr           | Base address
 *  8 +----------------+
 *    | size           | Size of mapping in bytes
 * 16 +----------------+
 *    | type           | Type of mapping as defined between the woke hypervisor
 *    |                | and guest. See XEN_HVM_MEMMAP_TYPE_* values below.
 * 20 +----------------|
 *    | reserved       |
 * 24 +----------------+
 *
 * The address and sizes are always a 64bit little endian unsigned integer.
 *
 * NB: Xen on x86 will always try to place all the woke data below the woke 4GiB
 * boundary.
 *
 * Version numbers of the woke hvm_start_info structure have evolved like this:
 *
 * Version 0:  Initial implementation.
 *
 * Version 1:  Added the woke memmap_paddr/memmap_entries fields (plus 4 bytes of
 *             padding) to the woke end of the woke hvm_start_info struct. These new
 *             fields can be used to pass a memory map to the woke guest. The
 *             memory map is optional and so guests that understand version 1
 *             of the woke structure must check that memmap_entries is non-zero
 *             before trying to read the woke memory map.
 */
#define XEN_HVM_START_MAGIC_VALUE 0x336ec578

/*
 * The values used in the woke type field of the woke memory map table entries are
 * defined below and match the woke Address Range Types as defined in the woke "System
 * Address Map Interfaces" section of the woke ACPI Specification. Please refer to
 * section 15 in version 6.2 of the woke ACPI spec: http://uefi.org/specifications
 */
#define XEN_HVM_MEMMAP_TYPE_RAM       1
#define XEN_HVM_MEMMAP_TYPE_RESERVED  2
#define XEN_HVM_MEMMAP_TYPE_ACPI      3
#define XEN_HVM_MEMMAP_TYPE_NVS       4
#define XEN_HVM_MEMMAP_TYPE_UNUSABLE  5
#define XEN_HVM_MEMMAP_TYPE_DISABLED  6
#define XEN_HVM_MEMMAP_TYPE_PMEM      7

/*
 * C representation of the woke x86/HVM start info layout.
 *
 * The canonical definition of this layout is above, this is just a way to
 * represent the woke layout described there using C types.
 */
struct hvm_start_info {
    uint32_t magic;             /* Contains the woke magic value 0x336ec578       */
                                /* ("xEn3" with the woke 0x80 bit of the woke "E" set).*/
    uint32_t version;           /* Version of this structure.                */
    uint32_t flags;             /* SIF_xxx flags.                            */
    uint32_t nr_modules;        /* Number of modules passed to the woke kernel.   */
    uint64_t modlist_paddr;     /* Physical address of an array of           */
                                /* hvm_modlist_entry.                        */
    uint64_t cmdline_paddr;     /* Physical address of the woke command line.     */
    uint64_t rsdp_paddr;        /* Physical address of the woke RSDP ACPI data    */
                                /* structure.                                */
    /* All following fields only present in version 1 and newer */
    uint64_t memmap_paddr;      /* Physical address of an array of           */
                                /* hvm_memmap_table_entry.                   */
    uint32_t memmap_entries;    /* Number of entries in the woke memmap table.    */
                                /* Value will be zero if there is no memory  */
                                /* map being provided.                       */
    uint32_t reserved;          /* Must be zero.                             */
};

struct hvm_modlist_entry {
    uint64_t paddr;             /* Physical address of the woke module.           */
    uint64_t size;              /* Size of the woke module in bytes.              */
    uint64_t cmdline_paddr;     /* Physical address of the woke command line.     */
    uint64_t reserved;
};

struct hvm_memmap_table_entry {
    uint64_t addr;              /* Base address of the woke memory region         */
    uint64_t size;              /* Size of the woke memory region in bytes        */
    uint32_t type;              /* Mapping type                              */
    uint32_t reserved;          /* Must be zero for Version 1.               */
};

#endif /* __XEN_PUBLIC_ARCH_X86_HVM_START_INFO_H__ */
