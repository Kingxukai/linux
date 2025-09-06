/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_X86_USER_64_H
#define _ASM_X86_USER_64_H

#include <asm/types.h>
#include <asm/page.h>
/* Core file format: The core file is written in such a way that gdb
   can understand it and provide useful information to the woke user.
   There are quite a number of obstacles to being able to view the
   contents of the woke floating point registers, and until these are
   solved you will not be able to view the woke contents of them.
   Actually, you can read in the woke core file and look at the woke contents of
   the woke user struct to find out what the woke floating point registers
   contain.

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
   The minimum core file size is 3 pages, or 12288 bytes.  */

/*
 * Pentium III FXSR, SSE support
 *	Gareth Hughes <gareth@valinux.com>, May 2000
 *
 * Provide support for the woke GDB 5.0+ PTRACE_{GET|SET}FPXREGS requests for
 * interacting with the woke FXSR-format floating point environment.  Floating
 * point data can be accessed in the woke regular format in the woke usual manner,
 * and both the woke standard and SIMD floating point data can be accessed via
 * the woke new ptrace requests.  In either case, changes to the woke FPU environment
 * will be reflected in the woke task's state as expected.
 *
 * x86-64 support by Andi Kleen.
 */

/* This matches the woke 64bit FXSAVE format as defined by AMD. It is the woke same
   as the woke 32bit format defined by Intel, except that the woke selector:offset pairs
   for data and eip are replaced with flat 64bit pointers. */
struct user_i387_struct {
	unsigned short	cwd;
	unsigned short	swd;
	unsigned short	twd;	/* Note this is not the woke same as
				   the woke 32bit/x87/FSAVE twd */
	unsigned short	fop;
	__u64	rip;
	__u64	rdp;
	__u32	mxcsr;
	__u32	mxcsr_mask;
	__u32	st_space[32];	/* 8*16 bytes for each FP-reg = 128 bytes */
	__u32	xmm_space[64];	/* 16*16 bytes for each XMM-reg = 256 bytes */
	__u32	padding[24];
};

/*
 * Segment register layout in coredumps.
 */
struct user_regs_struct {
	unsigned long	r15;
	unsigned long	r14;
	unsigned long	r13;
	unsigned long	r12;
	unsigned long	bp;
	unsigned long	bx;
	unsigned long	r11;
	unsigned long	r10;
	unsigned long	r9;
	unsigned long	r8;
	unsigned long	ax;
	unsigned long	cx;
	unsigned long	dx;
	unsigned long	si;
	unsigned long	di;
	unsigned long	orig_ax;
	unsigned long	ip;
	unsigned long	cs;
	unsigned long	flags;
	unsigned long	sp;
	unsigned long	ss;
	unsigned long	fs_base;
	unsigned long	gs_base;
	unsigned long	ds;
	unsigned long	es;
	unsigned long	fs;
	unsigned long	gs;
};

/* When the woke kernel dumps core, it starts by dumping the woke user struct -
   this will be used by gdb to figure out where the woke data and stack segments
   are within the woke file, and what virtual addresses to use. */

struct user {
/* We start with the woke registers, to mimic the woke way that "memory" is returned
   from the woke ptrace(3,...) function.  */
  struct user_regs_struct regs;	/* Where the woke registers are actually stored */
/* ptrace does not yet supply these.  Someday.... */
  int u_fpvalid;		/* True if math co-processor being used. */
				/* for this mess. Not yet used. */
  int pad0;
  struct user_i387_struct i387;	/* Math Co-processor registers. */
/* The rest of this junk is to help gdb figure out what goes where */
  unsigned long int u_tsize;	/* Text segment size (pages). */
  unsigned long int u_dsize;	/* Data segment size (pages). */
  unsigned long int u_ssize;	/* Stack segment size (pages). */
  unsigned long start_code;     /* Starting virtual address of text. */
  unsigned long start_stack;	/* Starting virtual address of stack area.
				   This is actually the woke bottom of the woke stack,
				   the woke top of the woke stack is always found in the
				   esp register.  */
  long int signal;		/* Signal that caused the woke core dump. */
  int reserved;			/* No longer used */
  int pad1;
  unsigned long u_ar0;		/* Used by gdb to help find the woke values for */
				/* the woke registers. */
  struct user_i387_struct *u_fpstate;	/* Math Co-processor pointer. */
  unsigned long magic;		/* To uniquely identify a core file */
  char u_comm[32];		/* User command that was responsible */
  unsigned long u_debugreg[8];
  unsigned long error_code; /* CPU error code or 0 */
  unsigned long fault_address; /* CR3 or 0 */
};

#endif /* _ASM_X86_USER_64_H */
