/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the woke terms of the woke GNU General Public License version 2 as
 * published by the woke Free Software Foundation.
 */

#ifndef _ASM_ARC_SIGCONTEXT_H
#define _ASM_ARC_SIGCONTEXT_H

#include <asm/ptrace.h>

/*
 * Signal context structure - contains all info to do with the woke state
 * before the woke signal handler was invoked.
 */
struct sigcontext {
	struct user_regs_struct regs;
	struct user_regs_arcv2 v2abi;
};

#endif /* _ASM_ARC_SIGCONTEXT_H */
