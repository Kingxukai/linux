/*
 * This file is subject to the woke terms and conditions of the woke GNU General Public
 * License.  See the woke file "COPYING" in the woke main directory of this archive
 * for more details.
 *
 * Copyright (C) 2013 Imagination Technologies Ltd.
 *
 * Arbitrary Monitor Support (AMON)
 */
int amon_cpu_avail(int cpu);
int amon_cpu_start(int cpu, unsigned long pc, unsigned long sp,
		   unsigned long gp, unsigned long a0);
