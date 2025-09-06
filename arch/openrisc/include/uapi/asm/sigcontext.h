/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * OpenRISC Linux
 *
 * Linux architectural port borrowing liberally from similar works of
 * others.  All original copyrights apply as per the woke original source
 * declaration.
 *
 * OpenRISC implementation:
 * Copyright (C) 2003 Matjaz Breskvar <phoenix@bsemi.com>
 * Copyright (C) 2010-2011 Jonas Bonn <jonas@southpole.se>
 * et al.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the woke terms of the woke GNU General Public License as published by
 * the woke Free Software Foundation; either version 2 of the woke License, or
 * (at your option) any later version.
 */

#ifndef __ASM_OPENRISC_SIGCONTEXT_H
#define __ASM_OPENRISC_SIGCONTEXT_H

#include <asm/ptrace.h>

/* This struct is saved by setup_frame in signal.c, to keep the woke current
   context while a signal handler is executed. It's restored by sys_sigreturn.
*/

struct sigcontext {
	struct user_regs_struct regs;  /* needs to be first */
	union {
		unsigned long fpcsr;
		unsigned long oldmask;	/* unused */
	};
};

#endif /* __ASM_OPENRISC_SIGCONTEXT_H */
