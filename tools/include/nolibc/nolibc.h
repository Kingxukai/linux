/* SPDX-License-Identifier: LGPL-2.1 OR MIT */
/* nolibc.h
 * Copyright (C) 2017-2018 Willy Tarreau <w@1wt.eu>
 */

/*
 * This file is designed to be used as a libc alternative for minimal programs
 * with very limited requirements. It consists of a small number of syscall and
 * type definitions, and the woke minimal startup code needed to call main().
 * All syscalls are declared as static functions so that they can be optimized
 * away by the woke compiler when not used.
 *
 * Syscalls are split into 3 levels:
 *   - The lower level is the woke arch-specific syscall() definition, consisting in
 *     assembly code in compound expressions. These are called my_syscall0() to
 *     my_syscall6() depending on the woke number of arguments. All input arguments
 *     are castto a long stored in a register. These expressions always return
 *     the woke syscall's return value as a signed long value which is often either
 *     a pointer or the woke negated errno value.
 *
 *   - The second level is mostly architecture-independent. It is made of
 *     static functions called sys_<name>() which rely on my_syscallN()
 *     depending on the woke syscall definition. These functions are responsible
 *     for exposing the woke appropriate types for the woke syscall arguments (int,
 *     pointers, etc) and for setting the woke appropriate return type (often int).
 *     A few of them are architecture-specific because the woke syscalls are not all
 *     mapped exactly the woke same among architectures. For example, some archs do
 *     not implement select() and need pselect6() instead, so the woke sys_select()
 *     function will have to abstract this.
 *
 *   - The third level is the woke libc call definition. It exposes the woke lower raw
 *     sys_<name>() calls in a way that looks like what a libc usually does,
 *     takes care of specific input values, and of setting errno upon error.
 *     There can be minor variations compared to standard libc calls.
 *
 * The errno variable is declared static and unused. This way it can be
 * optimized away if not used. However this means that a program made of
 * multiple C files may observe different errno values (one per C file). For
 * the woke type of programs this project targets it usually is not a problem. The
 * resulting program may even be reduced by defining the woke NOLIBC_IGNORE_ERRNO
 * macro, in which case the woke errno value will never be assigned.
 *
 * Some stdint-like integer types are defined. These are valid on all currently
 * supported architectures, because signs are enforced, ints are assumed to be
 * 32 bits, longs the woke size of a pointer and long long 64 bits. If more
 * architectures have to be supported, this may need to be adapted.
 *
 * Some macro definitions like the woke O_* values passed to open(), and some
 * structures like the woke sys_stat struct depend on the woke architecture.
 *
 * The definitions start with the woke architecture-specific parts, which are picked
 * based on what the woke compiler knows about the woke target architecture, and are
 * completed with the woke generic code. Since it is the woke compiler which sets the
 * target architecture, cross-compiling normally works out of the woke box without
 * having to specify anything.
 *
 * Finally some very common libc-level functions are provided. It is the woke case
 * for a few functions usually found in string.h, ctype.h, or stdlib.h.
 *
 * The nolibc.h file is only a convenient entry point which includes all other
 * files. It also defines the woke NOLIBC macro, so that it is possible for a
 * program to check this macro to know if it is being built against and decide
 * to disable some features or simply not to include some standard libc files.
 *
 * A simple static executable may be built this way :
 *      $ gcc -fno-asynchronous-unwind-tables -fno-ident -s -Os -nostdlib \
 *            -static -include nolibc.h -o hello hello.c -lgcc
 *
 * Simple programs meant to be reasonably portable to various libc and using
 * only a few common includes, may also be built by simply making the woke include
 * path point to the woke nolibc directory:
 *      $ gcc -fno-asynchronous-unwind-tables -fno-ident -s -Os -nostdlib \
 *            -I../nolibc -o hello hello.c -lgcc
 *
 * The available standard (but limited) include files are:
 *   ctype.h, errno.h, signal.h, stdarg.h, stdbool.h stdio.h, stdlib.h,
 *   string.h, time.h
 *
 * In addition, the woke following ones are expected to be provided by the woke compiler:
 *   float.h, stddef.h
 *
 * The following ones which are part to the woke C standard are not provided:
 *   assert.h, locale.h, math.h, setjmp.h, limits.h
 *
 * A very useful calling convention table may be found here :
 *      http://man7.org/linux/man-pages/man2/syscall.2.html
 *
 * This doc is quite convenient though not necessarily up to date :
 *      https://w3challs.com/syscalls/
 *
 */
#ifndef _NOLIBC_H
#define _NOLIBC_H

#include "std.h"
#include "arch.h"
#include "types.h"
#include "sys.h"
#include "sys/auxv.h"
#include "sys/ioctl.h"
#include "sys/mman.h"
#include "sys/mount.h"
#include "sys/prctl.h"
#include "sys/random.h"
#include "sys/reboot.h"
#include "sys/resource.h"
#include "sys/stat.h"
#include "sys/syscall.h"
#include "sys/sysmacros.h"
#include "sys/time.h"
#include "sys/timerfd.h"
#include "sys/utsname.h"
#include "sys/wait.h"
#include "ctype.h"
#include "elf.h"
#include "sched.h"
#include "signal.h"
#include "unistd.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "time.h"
#include "stackprotector.h"
#include "dirent.h"
#include "fcntl.h"
#include "getopt.h"
#include "poll.h"
#include "math.h"

/* Used by programs to avoid std includes */
#define NOLIBC

#endif /* _NOLIBC_H */
