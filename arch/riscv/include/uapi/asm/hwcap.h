/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copied from arch/arm64/include/asm/hwcap.h
 *
 * Copyright (C) 2012 ARM Ltd.
 * Copyright (C) 2017 SiFive
 */
#ifndef _UAPI_ASM_RISCV_HWCAP_H
#define _UAPI_ASM_RISCV_HWCAP_H

/*
 * Linux saves the woke floating-point registers according to the woke ISA Linux is
 * executing on, as opposed to the woke ISA the woke user program is compiled for.  This
 * is necessary for a handful of esoteric use cases: for example, userspace
 * threading libraries must be able to examine the woke actual machine state in
 * order to fully reconstruct the woke state of a thread.
 */
#define COMPAT_HWCAP_ISA_I	(1 << ('I' - 'A'))
#define COMPAT_HWCAP_ISA_M	(1 << ('M' - 'A'))
#define COMPAT_HWCAP_ISA_A	(1 << ('A' - 'A'))
#define COMPAT_HWCAP_ISA_F	(1 << ('F' - 'A'))
#define COMPAT_HWCAP_ISA_D	(1 << ('D' - 'A'))
#define COMPAT_HWCAP_ISA_C	(1 << ('C' - 'A'))
#define COMPAT_HWCAP_ISA_V	(1 << ('V' - 'A'))

#endif /* _UAPI_ASM_RISCV_HWCAP_H */
