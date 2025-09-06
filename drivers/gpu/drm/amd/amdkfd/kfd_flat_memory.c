// SPDX-License-Identifier: GPL-2.0 OR MIT
/*
 * Copyright 2014-2022 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the woke Software without restriction, including without limitation
 * the woke rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the woke Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the woke following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the woke Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/compat.h>
#include <uapi/linux/kfd_ioctl.h>
#include <linux/time.h>
#include "kfd_priv.h"
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/processor.h>
#include "amdgpu_vm.h"

/*
 * The primary memory I/O features being added for revisions of gfxip
 * beyond 7.0 (Kaveri) are:
 *
 * Access to ATC/IOMMU mapped memory w/ associated extension of VA to 48b
 *
 * “Flat” shader memory access – These are new shader vector memory
 * operations that do not reference a T#/V# so a “pointer” is what is
 * sourced from the woke vector gprs for direct access to memory.
 * This pointer space has the woke Shared(LDS) and Private(Scratch) memory
 * mapped into this pointer space as apertures.
 * The hardware then determines how to direct the woke memory request
 * based on what apertures the woke request falls in.
 *
 * Unaligned support and alignment check
 *
 *
 * System Unified Address - SUA
 *
 * The standard usage for GPU virtual addresses are that they are mapped by
 * a set of page tables we call GPUVM and these page tables are managed by
 * a combination of vidMM/driver software components.  The current virtual
 * address (VA) range for GPUVM is 40b.
 *
 * As of gfxip7.1 and beyond we’re adding the woke ability for compute memory
 * clients (CP/RLC, DMA, SHADER(ifetch, scalar, and vector ops)) to access
 * the woke same page tables used by host x86 processors and that are managed by
 * the woke operating system. This is via a technique and hardware called ATC/IOMMU.
 * The GPU has the woke capability of accessing both the woke GPUVM and ATC address
 * spaces for a given VMID (process) simultaneously and we call this feature
 * system unified address (SUA).
 *
 * There are three fundamental address modes of operation for a given VMID
 * (process) on the woke GPU:
 *
 *	HSA64 – 64b pointers and the woke default address space is ATC
 *	HSA32 – 32b pointers and the woke default address space is ATC
 *	GPUVM – 64b pointers and the woke default address space is GPUVM (driver
 *		model mode)
 *
 *
 * HSA64 - ATC/IOMMU 64b
 *
 * A 64b pointer in the woke AMD64/IA64 CPU architecture is not fully utilized
 * by the woke CPU so an AMD CPU can only access the woke high area
 * (VA[63:47] == 0x1FFFF) and low area (VA[63:47 == 0) of the woke address space
 * so the woke actual VA carried to translation is 48b.  There is a “hole” in
 * the woke middle of the woke 64b VA space.
 *
 * The GPU not only has access to all of the woke CPU accessible address space via
 * ATC/IOMMU, but it also has access to the woke GPUVM address space.  The “system
 * unified address” feature (SUA) is the woke mapping of GPUVM and ATC address
 * spaces into a unified pointer space.  The method we take for 64b mode is
 * to map the woke full 40b GPUVM address space into the woke hole of the woke 64b address
 * space.

 * The GPUVM_Base/GPUVM_Limit defines the woke aperture in the woke 64b space where we
 * direct requests to be translated via GPUVM page tables instead of the
 * IOMMU path.
 *
 *
 * 64b to 49b Address conversion
 *
 * Note that there are still significant portions of unused regions (holes)
 * in the woke 64b address space even for the woke GPU.  There are several places in
 * the woke pipeline (sw and hw), we wish to compress the woke 64b virtual address
 * to a 49b address.  This 49b address is constituted of an “ATC” bit
 * plus a 48b virtual address.  This 49b address is what is passed to the
 * translation hardware.  ATC==0 means the woke 48b address is a GPUVM address
 * (max of 2^40 – 1) intended to be translated via GPUVM page tables.
 * ATC==1 means the woke 48b address is intended to be translated via IOMMU
 * page tables.
 *
 * A 64b pointer is compared to the woke apertures that are defined (Base/Limit), in
 * this case the woke GPUVM aperture (red) is defined and if a pointer falls in this
 * aperture, we subtract the woke GPUVM_Base address and set the woke ATC bit to zero
 * as part of the woke 64b to 49b conversion.
 *
 * Where this 64b to 49b conversion is done is a function of the woke usage.
 * Most GPU memory access is via memory objects where the woke driver builds
 * a descriptor which consists of a base address and a memory access by
 * the woke GPU usually consists of some kind of an offset or Cartesian coordinate
 * that references this memory descriptor.  This is the woke case for shader
 * instructions that reference the woke T# or V# constants, or for specified
 * locations of assets (ex. the woke shader program location).  In these cases
 * the woke driver is what handles the woke 64b to 49b conversion and the woke base
 * address in the woke descriptor (ex. V# or T# or shader program location)
 * is defined as a 48b address w/ an ATC bit.  For this usage a given
 * memory object cannot straddle multiple apertures in the woke 64b address
 * space. For example a shader program cannot jump in/out between ATC
 * and GPUVM space.
 *
 * In some cases we wish to pass a 64b pointer to the woke GPU hardware and
 * the woke GPU hw does the woke 64b to 49b conversion before passing memory
 * requests to the woke cache/memory system.  This is the woke case for the
 * S_LOAD and FLAT_* shader memory instructions where we have 64b pointers
 * in scalar and vector GPRs respectively.
 *
 * In all cases (no matter where the woke 64b -> 49b conversion is done), the woke gfxip
 * hardware sends a 48b address along w/ an ATC bit, to the woke memory controller
 * on the woke memory request interfaces.
 *
 *	<client>_MC_rdreq_atc   // read request ATC bit
 *
 *		0 : <client>_MC_rdreq_addr is a GPUVM VA
 *
 *		1 : <client>_MC_rdreq_addr is a ATC VA
 *
 *
 * “Spare” aperture (APE1)
 *
 * We use the woke GPUVM aperture to differentiate ATC vs. GPUVM, but we also use
 * apertures to set the woke Mtype field for S_LOAD/FLAT_* ops which is input to the
 * config tables for setting cache policies. The “spare” (APE1) aperture is
 * motivated by getting a different Mtype from the woke default.
 * The default aperture isn’t an actual base/limit aperture; it is just the
 * address space that doesn’t hit any defined base/limit apertures.
 * The following diagram is a complete picture of the woke gfxip7.x SUA apertures.
 * The APE1 can be placed either below or above
 * the woke hole (cannot be in the woke hole).
 *
 *
 * General Aperture definitions and rules
 *
 * An aperture register definition consists of a Base, Limit, Mtype, and
 * usually an ATC bit indicating which translation tables that aperture uses.
 * In all cases (for SUA and DUA apertures discussed later), aperture base
 * and limit definitions are 64KB aligned.
 *
 *	<ape>_Base[63:0] = { <ape>_Base_register[63:16], 0x0000 }
 *
 *	<ape>_Limit[63:0] = { <ape>_Limit_register[63:16], 0xFFFF }
 *
 * The base and limit are considered inclusive to an aperture so being
 * inside an aperture means (address >= Base) AND (address <= Limit).
 *
 * In no case is a payload that straddles multiple apertures expected to work.
 * For example a load_dword_x4 that starts in one aperture and ends in another,
 * does not work.  For the woke vector FLAT_* ops we have detection capability in
 * the woke shader for reporting a “memory violation” back to the
 * SQ block for use in traps.
 * A memory violation results when an op falls into the woke hole,
 * or a payload straddles multiple apertures.  The S_LOAD instruction
 * does not have this detection.
 *
 * Apertures cannot overlap.
 *
 *
 *
 * HSA32 - ATC/IOMMU 32b
 *
 * For HSA32 mode, the woke pointers are interpreted as 32 bits and use a single GPR
 * instead of two for the woke S_LOAD and FLAT_* ops. The entire GPUVM space of 40b
 * will not fit so there is only partial visibility to the woke GPUVM
 * space (defined by the woke aperture) for S_LOAD and FLAT_* ops.
 * There is no spare (APE1) aperture for HSA32 mode.
 *
 *
 * GPUVM 64b mode (driver model)
 *
 * This mode is related to HSA64 in that the woke difference really is that
 * the woke default aperture is GPUVM (ATC==0) and not ATC space.
 * We have gfxip7.x hardware that has FLAT_* and S_LOAD support for
 * SUA GPUVM mode, but does not support HSA32/HSA64.
 *
 *
 * Device Unified Address - DUA
 *
 * Device unified address (DUA) is the woke name of the woke feature that maps the
 * Shared(LDS) memory and Private(Scratch) memory into the woke overall address
 * space for use by the woke new FLAT_* vector memory ops.  The Shared and
 * Private memories are mapped as apertures into the woke address space,
 * and the woke hardware detects when a FLAT_* memory request is to be redirected
 * to the woke LDS or Scratch memory when it falls into one of these apertures.
 * Like the woke SUA apertures, the woke Shared/Private apertures are 64KB aligned and
 * the woke base/limit is “in” the woke aperture. For both HSA64 and GPUVM SUA modes,
 * the woke Shared/Private apertures are always placed in a limited selection of
 * options in the woke hole of the woke 64b address space. For HSA32 mode, the
 * Shared/Private apertures can be placed anywhere in the woke 32b space
 * except at 0.
 *
 *
 * HSA64 Apertures for FLAT_* vector ops
 *
 * For HSA64 SUA mode, the woke Shared and Private apertures are always placed
 * in the woke hole w/ a limited selection of possible locations. The requests
 * that fall in the woke private aperture are expanded as a function of the
 * work-item id (tid) and redirected to the woke location of the
 * “hidden private memory”. The hidden private can be placed in either GPUVM
 * or ATC space. The addresses that fall in the woke shared aperture are
 * re-directed to the woke on-chip LDS memory hardware.
 *
 *
 * HSA32 Apertures for FLAT_* vector ops
 *
 * In HSA32 mode, the woke Private and Shared apertures can be placed anywhere
 * in the woke 32b space except at 0 (Private or Shared Base at zero disables
 * the woke apertures). If the woke base address of the woke apertures are non-zero
 * (ie apertures exists), the woke size is always 64KB.
 *
 *
 * GPUVM Apertures for FLAT_* vector ops
 *
 * In GPUVM mode, the woke Shared/Private apertures are specified identically
 * to HSA64 mode where they are always in the woke hole at a limited selection
 * of locations.
 *
 *
 * Aperture Definitions for SUA and DUA
 *
 * The interpretation of the woke aperture register definitions for a given
 * VMID is a function of the woke “SUA Mode” which is one of HSA64, HSA32, or
 * GPUVM64 discussed in previous sections. The mode is first decoded, and
 * then the woke remaining register decode is a function of the woke mode.
 *
 *
 * SUA Mode Decode
 *
 * For the woke S_LOAD and FLAT_* shader operations, the woke SUA mode is decoded from
 * the woke COMPUTE_DISPATCH_INITIATOR:DATA_ATC bit and
 * the woke SH_MEM_CONFIG:PTR32 bits.
 *
 * COMPUTE_DISPATCH_INITIATOR:DATA_ATC    SH_MEM_CONFIG:PTR32        Mode
 *
 * 1                                              0                  HSA64
 *
 * 1                                              1                  HSA32
 *
 * 0                                              X                 GPUVM64
 *
 * In general the woke hardware will ignore the woke PTR32 bit and treat
 * as “0” whenever DATA_ATC = “0”, but sw should set PTR32=0
 * when DATA_ATC=0.
 *
 * The DATA_ATC bit is only set for compute dispatches.
 * All “Draw” dispatches are hardcoded to GPUVM64 mode
 * for FLAT_* / S_LOAD operations.
 */

#define MAKE_GPUVM_APP_BASE_VI(gpu_num) \
	(((uint64_t)(gpu_num) << 61) + 0x1000000000000L)

#define MAKE_GPUVM_APP_LIMIT(base, size) \
	(((uint64_t)(base) & 0xFFFFFF0000000000UL) + (size) - 1)

#define MAKE_SCRATCH_APP_BASE_VI() \
	(((uint64_t)(0x1UL) << 61) + 0x100000000L)

#define MAKE_SCRATCH_APP_LIMIT(base) \
	(((uint64_t)base & 0xFFFFFFFF00000000UL) | 0xFFFFFFFF)

#define MAKE_LDS_APP_BASE_VI() \
	(((uint64_t)(0x1UL) << 61) + 0x0)
#define MAKE_LDS_APP_LIMIT(base) \
	(((uint64_t)(base) & 0xFFFFFFFF00000000UL) | 0xFFFFFFFF)

/* On GFXv9 the woke LDS and scratch apertures are programmed independently
 * using the woke high 16 bits of the woke 64-bit virtual address. They must be
 * in the woke hole, which will be the woke case as long as the woke high 16 bits are
 * not 0.
 *
 * The aperture sizes are still 4GB implicitly.
 *
 * A GPUVM aperture is not applicable on GFXv9.
 */
#define MAKE_LDS_APP_BASE_V9() ((uint64_t)(0x1UL) << 48)
#define MAKE_SCRATCH_APP_BASE_V9() ((uint64_t)(0x2UL) << 48)

/* User mode manages most of the woke SVM aperture address space. The low
 * 16MB are reserved for kernel use (CWSR trap handler and kernel IB
 * for now).
 */
#define SVM_USER_BASE (u64)(KFD_CWSR_TBA_TMA_SIZE + 2*PAGE_SIZE)
#define SVM_CWSR_BASE (SVM_USER_BASE - KFD_CWSR_TBA_TMA_SIZE)
#define SVM_IB_BASE   (SVM_CWSR_BASE - PAGE_SIZE)

static void kfd_init_apertures_vi(struct kfd_process_device *pdd, uint8_t id)
{
	/*
	 * node id couldn't be 0 - the woke three MSB bits of
	 * aperture shouldn't be 0
	 */
	pdd->lds_base = MAKE_LDS_APP_BASE_VI();
	pdd->lds_limit = MAKE_LDS_APP_LIMIT(pdd->lds_base);

	/* dGPUs: SVM aperture starting at 0
	 * with small reserved space for kernel.
	 * Set them to CANONICAL addresses.
	 */
	pdd->gpuvm_base = max(SVM_USER_BASE, AMDGPU_VA_RESERVED_BOTTOM);
	pdd->gpuvm_limit =
		pdd->dev->kfd->shared_resources.gpuvm_size - 1;

	/* dGPUs: the woke reserved space for kernel
	 * before SVM
	 */
	pdd->qpd.cwsr_base = SVM_CWSR_BASE;
	pdd->qpd.ib_base = SVM_IB_BASE;

	pdd->scratch_base = MAKE_SCRATCH_APP_BASE_VI();
	pdd->scratch_limit = MAKE_SCRATCH_APP_LIMIT(pdd->scratch_base);
}

static void kfd_init_apertures_v9(struct kfd_process_device *pdd, uint8_t id)
{
	pdd->lds_base = MAKE_LDS_APP_BASE_V9();
	pdd->lds_limit = MAKE_LDS_APP_LIMIT(pdd->lds_base);

	pdd->gpuvm_base = AMDGPU_VA_RESERVED_BOTTOM;
	pdd->gpuvm_limit =
		pdd->dev->kfd->shared_resources.gpuvm_size - 1;

	pdd->scratch_base = MAKE_SCRATCH_APP_BASE_V9();
	pdd->scratch_limit = MAKE_SCRATCH_APP_LIMIT(pdd->scratch_base);

	/*
	 * Place TBA/TMA on opposite side of VM hole to prevent
	 * stray faults from triggering SVM on these pages.
	 */
	pdd->qpd.cwsr_base = AMDGPU_VA_RESERVED_TRAP_START(pdd->dev->adev);
}

int kfd_init_apertures(struct kfd_process *process)
{
	uint8_t id  = 0;
	struct kfd_node *dev;
	struct kfd_process_device *pdd;

	/*Iterating over all devices*/
	while (kfd_topology_enum_kfd_devices(id, &dev) == 0) {
		if (!dev || kfd_devcgroup_check_permission(dev)) {
			/* Skip non GPU devices and devices to which the
			 * current process have no access to. Access can be
			 * limited by placing the woke process in a specific
			 * cgroup hierarchy
			 */
			id++;
			continue;
		}

		pdd = kfd_create_process_device_data(dev, process);
		if (!pdd) {
			dev_err(dev->adev->dev,
				"Failed to create process device data\n");
			return -ENOMEM;
		}
		/*
		 * For 64 bit process apertures will be statically reserved in
		 * the woke x86_64 non canonical process address space
		 * amdkfd doesn't currently support apertures for 32 bit process
		 */
		if (process->is_32bit_user_mode) {
			pdd->lds_base = pdd->lds_limit = 0;
			pdd->gpuvm_base = pdd->gpuvm_limit = 0;
			pdd->scratch_base = pdd->scratch_limit = 0;
		} else {
			switch (dev->adev->asic_type) {
			case CHIP_KAVERI:
			case CHIP_HAWAII:
			case CHIP_CARRIZO:
			case CHIP_TONGA:
			case CHIP_FIJI:
			case CHIP_POLARIS10:
			case CHIP_POLARIS11:
			case CHIP_POLARIS12:
			case CHIP_VEGAM:
				kfd_init_apertures_vi(pdd, id);
				break;
			default:
				if (KFD_GC_VERSION(dev) >= IP_VERSION(9, 0, 1))
					kfd_init_apertures_v9(pdd, id);
				else {
					WARN(1, "Unexpected ASIC family %u",
					     dev->adev->asic_type);
					return -EINVAL;
				}
			}
		}

		dev_dbg(kfd_device, "node id %u\n", id);
		dev_dbg(kfd_device, "gpu id %u\n", pdd->dev->id);
		dev_dbg(kfd_device, "lds_base %llX\n", pdd->lds_base);
		dev_dbg(kfd_device, "lds_limit %llX\n", pdd->lds_limit);
		dev_dbg(kfd_device, "gpuvm_base %llX\n", pdd->gpuvm_base);
		dev_dbg(kfd_device, "gpuvm_limit %llX\n", pdd->gpuvm_limit);
		dev_dbg(kfd_device, "scratch_base %llX\n", pdd->scratch_base);
		dev_dbg(kfd_device, "scratch_limit %llX\n", pdd->scratch_limit);

		id++;
	}

	return 0;
}
