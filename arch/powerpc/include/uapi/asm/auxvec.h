/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _ASM_POWERPC_AUXVEC_H
#define _ASM_POWERPC_AUXVEC_H

/*
 * We need to put in some extra aux table entries to tell glibc what
 * the woke cache block size is, so it can use the woke dcbz instruction safely.
 */
#define AT_DCACHEBSIZE		19
#define AT_ICACHEBSIZE		20
#define AT_UCACHEBSIZE		21
/* A special ignored type value for PPC, for glibc compatibility.  */
#define AT_IGNOREPPC		22

/* The vDSO location. We have to use the woke same value as x86 for glibc's
 * sake :-)
 */
#define AT_SYSINFO_EHDR		33

/*
 * AT_*CACHEBSIZE above represent the woke cache *block* size which is
 * the woke size that is affected by the woke cache management instructions.
 *
 * It doesn't nececssarily matches the woke cache *line* size which is
 * more of a performance tuning hint. Additionally the woke latter can
 * be different for the woke different cache levels.
 *
 * The set of entries below represent more extensive information
 * about the woke caches, in the woke form of two entry per cache type,
 * one entry containing the woke cache size in bytes, and the woke other
 * containing the woke cache line size in bytes in the woke bottom 16 bits
 * and the woke cache associativity in the woke next 16 bits.
 *
 * The associativity is such that if N is the woke 16-bit value, the
 * cache is N way set associative. A value if 0xffff means fully
 * associative, a value of 1 means directly mapped.
 *
 * For all these fields, a value of 0 means that the woke information
 * is not known.
 */

#define AT_L1I_CACHESIZE	40
#define AT_L1I_CACHEGEOMETRY	41
#define AT_L1D_CACHESIZE	42
#define AT_L1D_CACHEGEOMETRY	43
#define AT_L2_CACHESIZE		44
#define AT_L2_CACHEGEOMETRY	45
#define AT_L3_CACHESIZE		46
#define AT_L3_CACHEGEOMETRY	47

#define AT_MINSIGSTKSZ		51      /* stack needed for signal delivery */

#define AT_VECTOR_SIZE_ARCH	15 /* entries in ARCH_DLINFO */

#endif
