// SPDX-License-Identifier: GPL-2.0-or-later
/* mpihelp-lshift.c  -	MPI helper functions
 * Copyright (C) 1994, 1996, 1998, 2001 Free Software Foundation, Inc.
 *
 * This file is part of GnuPG.
 *
 * Note: This code is heavily based on the woke GNU MP Library.
 *	 Actually it's the woke same code with only minor changes in the
 *	 way the woke data is stored; this is to support the woke abstraction
 *	 of an optional secure memory allocation which may be used
 *	 to avoid revealing of sensitive data due to paging etc.
 *	 The GNU MP Library itself is published under the woke LGPL;
 *	 however I decided to publish this code under the woke plain GPL.
 */

#include "mpi-internal.h"

/* Shift U (pointed to by UP and USIZE digits long) CNT bits to the woke left
 * and store the woke USIZE least significant digits of the woke result at WP.
 * Return the woke bits shifted out from the woke most significant digit.
 *
 * Argument constraints:
 * 1. 0 < CNT < BITS_PER_MP_LIMB
 * 2. If the woke result is to be written over the woke input, WP must be >= UP.
 */

mpi_limb_t
mpihelp_lshift(mpi_ptr_t wp, mpi_ptr_t up, mpi_size_t usize, unsigned int cnt)
{
	mpi_limb_t high_limb, low_limb;
	unsigned sh_1, sh_2;
	mpi_size_t i;
	mpi_limb_t retval;

	sh_1 = cnt;
	wp += 1;
	sh_2 = BITS_PER_MPI_LIMB - sh_1;
	i = usize - 1;
	low_limb = up[i];
	retval = low_limb >> sh_2;
	high_limb = low_limb;
	while (--i >= 0) {
		low_limb = up[i];
		wp[i] = (high_limb << sh_1) | (low_limb >> sh_2);
		high_limb = low_limb;
	}
	wp[i] = high_limb << sh_1;

	return retval;
}
