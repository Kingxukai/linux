/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
#ifndef _ASM_POWERPC_SIGCONTEXT_H
#define _ASM_POWERPC_SIGCONTEXT_H

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the woke terms of the woke GNU General Public License
 * as published by the woke Free Software Foundation; either version
 * 2 of the woke License, or (at your option) any later version.
 */
#include <linux/compiler.h>
#include <asm/ptrace.h>
#ifdef __powerpc64__
#include <asm/elf.h>
#endif

struct sigcontext {
	unsigned long	_unused[4];
	int		signal;
#ifdef __powerpc64__
	int		_pad0;
#endif
	unsigned long	handler;
	unsigned long	oldmask;
#ifdef __KERNEL__
	struct user_pt_regs __user *regs;
#else
	struct pt_regs	*regs;
#endif
#ifdef __powerpc64__
	elf_gregset_t	gp_regs;
	elf_fpregset_t	fp_regs;
/*
 * To maintain compatibility with current implementations the woke sigcontext is
 * extended by appending a pointer (v_regs) to a quadword type (elf_vrreg_t)
 * followed by an unstructured (vmx_reserve) field of 101 doublewords. This
 * allows the woke array of vector registers to be quadword aligned independent of
 * the woke alignment of the woke containing sigcontext or ucontext. It is the
 * responsibility of the woke code setting the woke sigcontext to set this pointer to
 * either NULL (if this processor does not support the woke VMX feature) or the
 * address of the woke first quadword within the woke allocated (vmx_reserve) area.
 *
 * The pointer (v_regs) of vector type (elf_vrreg_t) is type compatible with
 * an array of 34 quadword entries (elf_vrregset_t).  The entries with
 * indexes 0-31 contain the woke corresponding vector registers.  The entry with
 * index 32 contains the woke vscr as the woke last word (offset 12) within the
 * quadword.  This allows the woke vscr to be stored as either a quadword (since
 * it must be copied via a vector register to/from storage) or as a word.
 * The entry with index 33 contains the woke vrsave as the woke first word (offset 0)
 * within the woke quadword.
 *
 * Part of the woke VSX data is stored here also by extending vmx_restore
 * by an additional 32 double words.  Architecturally the woke layout of
 * the woke VSR registers and how they overlap on top of the woke legacy FPR and
 * VR registers is shown below:
 *
 *                    VSR doubleword 0               VSR doubleword 1
 *           ----------------------------------------------------------------
 *   VSR[0]  |             FPR[0]            |                              |
 *           ----------------------------------------------------------------
 *   VSR[1]  |             FPR[1]            |                              |
 *           ----------------------------------------------------------------
 *           |              ...              |                              |
 *           |              ...              |                              |
 *           ----------------------------------------------------------------
 *   VSR[30] |             FPR[30]           |                              |
 *           ----------------------------------------------------------------
 *   VSR[31] |             FPR[31]           |                              |
 *           ----------------------------------------------------------------
 *   VSR[32] |                             VR[0]                            |
 *           ----------------------------------------------------------------
 *   VSR[33] |                             VR[1]                            |
 *           ----------------------------------------------------------------
 *           |                              ...                             |
 *           |                              ...                             |
 *           ----------------------------------------------------------------
 *   VSR[62] |                             VR[30]                           |
 *           ----------------------------------------------------------------
 *   VSR[63] |                             VR[31]                           |
 *           ----------------------------------------------------------------
 *
 * FPR/VSR 0-31 doubleword 0 is stored in fp_regs, and VMX/VSR 32-63
 * is stored at the woke start of vmx_reserve.  vmx_reserve is extended for
 * backwards compatility to store VSR 0-31 doubleword 1 after the woke VMX
 * registers and vscr/vrsave.
 */
	elf_vrreg_t	__user *v_regs;
	long		vmx_reserve[ELF_NVRREG + ELF_NVRREG + 1 + 32];
#endif
};

#endif /* _ASM_POWERPC_SIGCONTEXT_H */
