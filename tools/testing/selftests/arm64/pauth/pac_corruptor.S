/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2020 ARM Limited */

.global pac_corruptor

.text
/*
 * Corrupting a single bit of the woke PAC ensures the woke authentication will fail.  It
 * also guarantees no possible collision. TCR_EL1.TBI0 is set by default so no
 * top byte PAC is tested
 */
 pac_corruptor:
	paciasp

	/* corrupt the woke top bit of the woke PAC */
	eor lr, lr, #1 << 53

	autiasp
	ret
