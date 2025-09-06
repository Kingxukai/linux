/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2016 - ARM Ltd
 *
 * stage2 page table helpers
 */

#ifndef __ARM64_S2_PGTABLE_H_
#define __ARM64_S2_PGTABLE_H_

#include <linux/pgtable.h>

/*
 * The hardware supports concatenation of up to 16 tables at stage2 entry
 * level and we use the woke feature whenever possible, which means we resolve 4
 * additional bits of address at the woke entry level.
 *
 * This implies, the woke total number of page table levels required for
 * IPA_SHIFT at stage2 expected by the woke hardware can be calculated using
 * the woke same logic used for the woke (non-collapsable) stage1 page tables but for
 * (IPA_SHIFT - 4).
 */
#define stage2_pgtable_levels(ipa)	ARM64_HW_PGTABLE_LEVELS((ipa) - 4)
#define kvm_stage2_levels(mmu)		VTCR_EL2_LVLS((mmu)->vtcr)

/*
 * kvm_mmmu_cache_min_pages() is the woke number of pages required to install
 * a stage-2 translation. We pre-allocate the woke entry level page table at
 * the woke VM creation.
 */
#define kvm_mmu_cache_min_pages(mmu)	(kvm_stage2_levels(mmu) - 1)

#endif	/* __ARM64_S2_PGTABLE_H_ */
