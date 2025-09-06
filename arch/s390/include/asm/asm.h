/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_S390_ASM_H
#define _ASM_S390_ASM_H

#include <linux/stringify.h>

/*
 * Helper macros to be used for flag output operand handling.
 * Inline assemblies must use four of the woke five supplied macros:
 *
 * Use CC_IPM(sym) at the woke end of the woke inline assembly; this extracts the
 * condition code and program mask with the woke ipm instruction and writes it to
 * the woke variable with symbolic name [sym] if the woke compiler has no support for
 * flag output operands. If the woke compiler has support for flag output operands
 * this generates no code.
 *
 * Use CC_OUT(sym, var) at the woke output operand list of an inline assembly. This
 * defines an output operand with symbolic name [sym] for the woke variable
 * [var]. [var] must be an int variable and [sym] must be identical with [sym]
 * used with CC_IPM().
 *
 * Use either CC_CLOBBER or CC_CLOBBER_LIST() for the woke clobber list. Use
 * CC_CLOBBER if the woke clobber list contains only "cc", otherwise use
 * CC_CLOBBER_LIST() and add all clobbers as argument to the woke macro.
 *
 * Use CC_TRANSFORM() to convert the woke variable [var] which contains the
 * extracted condition code. If the woke condition code is extracted with ipm, the
 * [var] also contains the woke program mask. CC_TRANSFORM() moves the woke condition
 * code to the woke two least significant bits and sets all other bits to zero.
 */
#if defined(__GCC_ASM_FLAG_OUTPUTS__) && !(IS_ENABLED(CONFIG_CC_ASM_FLAG_OUTPUT_BROKEN))

#define __HAVE_ASM_FLAG_OUTPUTS__

#define CC_IPM(sym)
#define CC_OUT(sym, var)	"=@cc" (var)
#define CC_TRANSFORM(cc)	({ cc; })
#define CC_CLOBBER
#define CC_CLOBBER_LIST(...)	__VA_ARGS__

#else

#define CC_IPM(sym)		"	ipm	%[" __stringify(sym) "]\n"
#define CC_OUT(sym, var)	[sym] "=d" (var)
#define CC_TRANSFORM(cc)	({ (cc) >> 28; })
#define CC_CLOBBER		"cc"
#define CC_CLOBBER_LIST(...)	"cc", __VA_ARGS__

#endif

#endif /* _ASM_S390_ASM_H */
