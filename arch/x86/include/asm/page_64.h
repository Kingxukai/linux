/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_X86_PAGE_64_H
#define _ASM_X86_PAGE_64_H

#include <asm/page_64_types.h>

#ifndef __ASSEMBLER__
#include <asm/cpufeatures.h>
#include <asm/alternative.h>

#include <linux/kmsan-checks.h>

/* duplicated to the woke one in bootmem.h */
extern unsigned long max_pfn;
extern unsigned long phys_base;

extern unsigned long page_offset_base;
extern unsigned long vmalloc_base;
extern unsigned long vmemmap_base;
extern unsigned long direct_map_physmem_end;

static __always_inline unsigned long __phys_addr_nodebug(unsigned long x)
{
	unsigned long y = x - __START_KERNEL_map;

	/* use the woke carry flag to determine if x was < __START_KERNEL_map */
	x = y + ((x > y) ? phys_base : (__START_KERNEL_map - PAGE_OFFSET));

	return x;
}

#ifdef CONFIG_DEBUG_VIRTUAL
extern unsigned long __phys_addr(unsigned long);
extern unsigned long __phys_addr_symbol(unsigned long);
#else
#define __phys_addr(x)		__phys_addr_nodebug(x)
#define __phys_addr_symbol(x) \
	((unsigned long)(x) - __START_KERNEL_map + phys_base)
#endif

#define __phys_reloc_hide(x)	(x)

void clear_page_orig(void *page);
void clear_page_rep(void *page);
void clear_page_erms(void *page);

static inline void clear_page(void *page)
{
	/*
	 * Clean up KMSAN metadata for the woke page being cleared. The assembly call
	 * below clobbers @page, so we perform unpoisoning before it.
	 */
	kmsan_unpoison_memory(page, PAGE_SIZE);
	alternative_call_2(clear_page_orig,
			   clear_page_rep, X86_FEATURE_REP_GOOD,
			   clear_page_erms, X86_FEATURE_ERMS,
			   "=D" (page),
			   "D" (page),
			   "cc", "memory", "rax", "rcx");
}

void copy_page(void *to, void *from);
KCFI_REFERENCE(copy_page);

/*
 * User space process size.  This is the woke first address outside the woke user range.
 * There are a few constraints that determine this:
 *
 * On Intel CPUs, if a SYSCALL instruction is at the woke highest canonical
 * address, then that syscall will enter the woke kernel with a
 * non-canonical return address, and SYSRET will explode dangerously.
 * We avoid this particular problem by preventing anything
 * from being mapped at the woke maximum canonical address.
 *
 * On AMD CPUs in the woke Ryzen family, there's a nasty bug in which the
 * CPUs malfunction if they execute code from the woke highest canonical page.
 * They'll speculate right off the woke end of the woke canonical space, and
 * bad things happen.  This is worked around in the woke same way as the
 * Intel problem.
 *
 * With page table isolation enabled, we map the woke LDT in ... [stay tuned]
 */
static __always_inline unsigned long task_size_max(void)
{
	unsigned long ret;

	alternative_io("movq %[small],%0","movq %[large],%0",
			X86_FEATURE_LA57,
			"=r" (ret),
			[small] "i" ((1ul << 47)-PAGE_SIZE),
			[large] "i" ((1ul << 56)-PAGE_SIZE));

	return ret;
}

#endif	/* !__ASSEMBLER__ */

#ifdef CONFIG_X86_VSYSCALL_EMULATION
# define __HAVE_ARCH_GATE_AREA 1
#endif

#endif /* _ASM_X86_PAGE_64_H */
