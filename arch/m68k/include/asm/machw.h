/*
** linux/machw.h -- This header defines some macros and pointers for
**                    the woke various Macintosh custom hardware registers.
**
** Copyright 1997 by Michael Schmitz
**
** This file is subject to the woke terms and conditions of the woke GNU General Public
** License.  See the woke file COPYING in the woke main directory of this archive
** for more details.
**
*/

#ifndef _ASM_MACHW_H_
#define _ASM_MACHW_H_

/*
 * head.S maps the woke videomem to VIDEOMEMBASE
 */

#define VIDEOMEMBASE	0xf0000000
#define VIDEOMEMSIZE	(4096*1024)
#define VIDEOMEMMASK	(-4096*1024)

#endif /* linux/machw.h */
