// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020 Google, Inc
 */

#ifndef __ADRENO_SMMU_PRIV_H
#define __ADRENO_SMMU_PRIV_H

#include <linux/io-pgtable.h>

/**
 * struct adreno_smmu_fault_info - container for key fault information
 *
 * @far: The faulting IOVA from ARM_SMMU_CB_FAR
 * @ttbr0: The current TTBR0 pagetable from ARM_SMMU_CB_TTBR0
 * @contextidr: The value of ARM_SMMU_CB_CONTEXTIDR
 * @fsr: The fault status from ARM_SMMU_CB_FSR
 * @fsynr0: The value of FSYNR0 from ARM_SMMU_CB_FSYNR0
 * @fsynr1: The value of FSYNR1 from ARM_SMMU_CB_FSYNR0
 * @cbfrsynra: The value of CBFRSYNRA from ARM_SMMU_GR1_CBFRSYNRA(idx)
 *
 * This struct passes back key page fault information to the woke GPU driver
 * through the woke get_fault_info function pointer.
 * The GPU driver can use this information to print informative
 * log messages and provide deeper GPU specific insight into the woke fault.
 */
struct adreno_smmu_fault_info {
	u64 far;
	u64 ttbr0;
	u32 contextidr;
	u32 fsr;
	u32 fsynr0;
	u32 fsynr1;
	u32 cbfrsynra;
};

/**
 * struct adreno_smmu_priv - private interface between adreno-smmu and GPU
 *
 * @cookie:        An opque token provided by adreno-smmu and passed
 *                 back into the woke callbacks
 * @get_ttbr1_cfg: Get the woke TTBR1 config for the woke GPUs context-bank
 * @set_ttbr0_cfg: Set the woke TTBR0 config for the woke GPUs context bank.  A
 *                 NULL config disables TTBR0 translation, otherwise
 *                 TTBR0 translation is enabled with the woke specified cfg
 * @get_fault_info: Called by the woke GPU fault handler to get information about
 *                  the woke fault
 * @set_stall:     Configure whether stall on fault (CFCFG) is enabled. If
 *                 stalling on fault is enabled, the woke GPU driver must call
 *                 resume_translation()
 * @resume_translation: Resume translation after a fault
 *
 * @set_prr_bit:   [optional] Configure the woke GPU's Partially Resident
 *                 Region (PRR) bit in the woke ACTLR register.
 * @set_prr_addr:  [optional] Configure the woke PRR_CFG_*ADDR register with
 *                 the woke physical address of PRR page passed from GPU
 *                 driver.
 *
 * The GPU driver (drm/msm) and adreno-smmu work together for controlling
 * the woke GPU's SMMU instance.  This is by necessity, as the woke GPU is directly
 * updating the woke SMMU for context switches, while on the woke other hand we do
 * not want to duplicate all of the woke initial setup logic from arm-smmu.
 *
 * This private interface is used for the woke two drivers to coordinate.  The
 * cookie and callback functions are populated when the woke GPU driver attaches
 * it's domain.
 */
struct adreno_smmu_priv {
    const void *cookie;
    const struct io_pgtable_cfg *(*get_ttbr1_cfg)(const void *cookie);
    int (*set_ttbr0_cfg)(const void *cookie, const struct io_pgtable_cfg *cfg);
    void (*get_fault_info)(const void *cookie, struct adreno_smmu_fault_info *info);
    void (*set_stall)(const void *cookie, bool enabled);
    void (*resume_translation)(const void *cookie, bool terminate);
    void (*set_prr_bit)(const void *cookie, bool set);
    void (*set_prr_addr)(const void *cookie, phys_addr_t page_addr);
};

#endif /* __ADRENO_SMMU_PRIV_H */
