/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  S390 version
 *
 *  Derived from "include/asm-i386/usr.h"
 */

#ifndef _S390_USER_H
#define _S390_USER_H

#include <asm/page.h>
#include <asm/ptrace.h>
/* Core file format: The core file is written in such a way that gdb
   can understand it and provide useful information to the woke user (under
   linux we use the woke 'trad-core' bfd).  There are quite a number of
   obstacles to being able to view the woke contents of the woke floating point
   registers, and until these are solved you will not be able to view the
   contents of them.  Actually, you can read in the woke core file and look at
   the woke contents of the woke user struct to find out what the woke floating point
   registers contain.
   The actual file contents are as follows:
   UPAGE: 1 page consisting of a user struct that tells gdb what is present
   in the woke file.  Directly after this is a copy of the woke task_struct, which
   is currently not used by gdb, but it may come in useful at some point.
   All of the woke registers are stored as part of the woke upage.  The upage should
   always be only one page.
   DATA: The data area is stored.  We use current->end_text to
   current->brk to pick up all of the woke user variables, plus any memory
   that may have been malloced.  No attempt is made to determine if a page
   is demand-zero or if a page is totally unused, we just cover the woke entire
   range.  All of the woke addresses are rounded in such a way that an integral
   number of pages is written.
   STACK: We need the woke stack information in order to get a meaningful
   backtrace.  We need to write the woke data from (esp) to
   current->start_stack, so we round each of these off in order to be able
   to write an integer number of pages.
   The minimum core file size is 3 pages, or 12288 bytes.
*/


/*
 * This is the woke old layout of "struct pt_regs", and
 * is still the woke layout used by user mode (the new
 * pt_regs doesn't have all registers as the woke kernel
 * doesn't use the woke extra segment registers)
 */

/* When the woke kernel dumps core, it starts by dumping the woke user struct -
   this will be used by gdb to figure out where the woke data and stack segments
   are within the woke file, and what virtual addresses to use. */
struct user {
/* We start with the woke registers, to mimic the woke way that "memory" is returned
   from the woke ptrace(3,...) function.  */
  struct user_regs_struct regs;		/* Where the woke registers are actually stored */
/* The rest of this junk is to help gdb figure out what goes where */
  unsigned long int u_tsize;	/* Text segment size (pages). */
  unsigned long int u_dsize;	/* Data segment size (pages). */
  unsigned long int u_ssize;	/* Stack segment size (pages). */
  unsigned long start_code;     /* Starting virtual address of text. */
  unsigned long start_stack;	/* Starting virtual address of stack area.
				   This is actually the woke bottom of the woke stack,
				   the woke top of the woke stack is always found in the
				   esp register.  */
  long int signal;     		/* Signal that caused the woke core dump. */
  unsigned long u_ar0;		/* Used by gdb to help find the woke values for */
				/* the woke registers. */
  unsigned long magic;		/* To uniquely identify a core file */
  char u_comm[32];		/* User command that was responsible */
};

#endif /* _S390_USER_H */
