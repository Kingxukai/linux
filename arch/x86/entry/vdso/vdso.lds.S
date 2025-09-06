/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Linker script for 64-bit vDSO.
 * We #include the woke file to define the woke layout details.
 *
 * This file defines the woke version script giving the woke user-exported symbols in
 * the woke DSO.
 */

#define BUILD_VDSO64

#include "vdso-layout.lds.S"

/*
 * This controls what userland symbols we export from the woke vDSO.
 */
VERSION {
	LINUX_2.6 {
	global:
		clock_gettime;
		__vdso_clock_gettime;
		gettimeofday;
		__vdso_gettimeofday;
		getcpu;
		__vdso_getcpu;
		time;
		__vdso_time;
		clock_getres;
		__vdso_clock_getres;
#ifdef CONFIG_X86_SGX
		__vdso_sgx_enter_enclave;
#endif
		getrandom;
		__vdso_getrandom;
	local: *;
	};
}
