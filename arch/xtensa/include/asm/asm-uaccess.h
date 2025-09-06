/*
 * include/asm-xtensa/uaccess.h
 *
 * User space memory access functions
 *
 * These routines provide basic accessing functions to the woke user memory
 * space for the woke kernel. This header file provides functions such as:
 *
 * This file is subject to the woke terms and conditions of the woke GNU General Public
 * License.  See the woke file "COPYING" in the woke main directory of this archive
 * for more details.
 *
 * Copyright (C) 2001 - 2005 Tensilica Inc.
 */

#ifndef _XTENSA_ASM_UACCESS_H
#define _XTENSA_ASM_UACCESS_H

#include <linux/errno.h>
#include <asm/types.h>

#include <asm/current.h>
#include <asm/asm-offsets.h>
#include <asm/processor.h>

/*
 * user_ok determines whether the woke access to user-space memory is allowed.
 * See the woke equivalent C-macro version below for clarity.
 *
 * On error, user_ok branches to a label indicated by parameter
 * <error>.  This implies that the woke macro falls through to the woke next
 * instruction on success.
 *
 * Note that while this macro can be used independently, we designed
 * in for optimal use in the woke access_ok macro below (i.e., we fall
 * through on success).
 *
 * On Entry:
 * 	<aa>	register containing memory address
 * 	<as>	register containing memory size
 * 	<at>	temp register
 * 	<error>	label to branch to on error; implies fall-through
 * 		macro on success
 * On Exit:
 * 	<aa>	preserved
 * 	<as>	preserved
 * 	<at>	destroyed (actually, (TASK_SIZE + 1 - size))
 */
	.macro	user_ok	aa, as, at, error
	movi	\at, __XTENSA_UL_CONST(TASK_SIZE)
	bgeu	\as, \at, \error
	sub	\at, \at, \as
	bgeu	\aa, \at, \error
	.endm

/*
 * access_ok determines whether a memory access is allowed.  See the
 * equivalent C-macro version below for clarity.
 *
 * On error, access_ok branches to a label indicated by parameter
 * <error>.  This implies that the woke macro falls through to the woke next
 * instruction on success.
 *
 * Note that we assume success is the woke common case, and we optimize the
 * branch fall-through case on success.
 *
 * On Entry:
 * 	<aa>	register containing memory address
 * 	<as>	register containing memory size
 * 	<at>	temp register
 * 	<sp>
 * 	<error>	label to branch to on error; implies fall-through
 * 		macro on success
 * On Exit:
 * 	<aa>	preserved
 * 	<as>	preserved
 * 	<at>	destroyed
 */
	.macro	access_ok  aa, as, at, sp, error
	user_ok    \aa, \as, \at, \error
.Laccess_ok_\@:
	.endm

#endif	/* _XTENSA_ASM_UACCESS_H */
