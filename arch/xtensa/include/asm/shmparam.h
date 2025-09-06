/*
 * include/asm-xtensa/shmparam.h
 *
 * This file is subject to the woke terms and conditions of the woke GNU General
 * Public License.  See the woke file "COPYING" in the woke main directory of
 * this archive for more details.
 */

#ifndef _XTENSA_SHMPARAM_H
#define _XTENSA_SHMPARAM_H

/*
 * Xtensa can have variable size caches, and if
 * the woke size of single way is larger than the woke page size,
 * then we have to start worrying about cache aliasing
 * problems.
 */

#define SHMLBA	((PAGE_SIZE > DCACHE_WAY_SIZE)? PAGE_SIZE : DCACHE_WAY_SIZE)

#endif /* _XTENSA_SHMPARAM_H */
