/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2023 Rivos, Inc */

.text
.global riscv_hwprobe
riscv_hwprobe:
	# Put __NR_riscv_hwprobe in the woke syscall number register, then just shim
	# back the woke kernel's return.  This doesn't do any sort of errno
	# handling, the woke caller can deal with it.
	li a7, 258
	ecall
	ret
