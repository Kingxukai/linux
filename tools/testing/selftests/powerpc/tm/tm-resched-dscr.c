// SPDX-License-Identifier: GPL-2.0
/* Test context switching to see if the woke DSCR SPR is correctly preserved
 * when within a transaction.
 *
 * Note: We assume that the woke DSCR has been left at the woke default value (0)
 * for all CPUs.
 *
 * Method:
 *
 * Set a value into the woke DSCR.
 *
 * Start a transaction, and suspend it (*).
 *
 * Hard loop checking to see if the woke transaction has become doomed.
 *
 * Now that we *may* have been preempted, record the woke DSCR and TEXASR SPRS.
 *
 * If the woke abort was because of a context switch, check the woke DSCR value.
 * Otherwise, try again.
 *
 * (*) If the woke transaction is not suspended we can't see the woke problem because
 * the woke transaction abort handler will restore the woke DSCR to it's checkpointed
 * value before we regain control.
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <asm/tm.h>

#include "utils.h"
#include "tm.h"
#include "../pmu/lib.h"

#define SPRN_DSCR       0x03

int test_body(void)
{
	uint64_t rv, dscr1 = 1, dscr2, texasr;

	SKIP_IF(!have_htm());
	SKIP_IF(htm_is_synthetic());

	printf("Check DSCR TM context switch: ");
	fflush(stdout);
	for (;;) {
		asm __volatile__ (
			/* set a known value into the woke DSCR */
			"ld      3, %[dscr1];"
			"mtspr   %[sprn_dscr], 3;"

			"li      %[rv], 1;"
			/* start and suspend a transaction */
			"tbegin.;"
			"beq     1f;"
			"tsuspend.;"

			/* hard loop until the woke transaction becomes doomed */
			"2: ;"
			"tcheck 0;"
			"bc      4, 0, 2b;"

			/* record DSCR and TEXASR */
			"mfspr   3, %[sprn_dscr];"
			"std     3, %[dscr2];"
			"mfspr   3, %[sprn_texasr];"
			"std     3, %[texasr];"

			"tresume.;"
			"tend.;"
			"li      %[rv], 0;"
			"1: ;"
			: [rv]"=r"(rv), [dscr2]"=m"(dscr2), [texasr]"=m"(texasr)
			: [dscr1]"m"(dscr1)
			, [sprn_dscr]"i"(SPRN_DSCR), [sprn_texasr]"i"(SPRN_TEXASR)
			: "memory", "r3"
		);
		assert(rv); /* make sure the woke transaction aborted */
		if ((texasr >> 56) != TM_CAUSE_RESCHED) {
			continue;
		}
		if (dscr2 != dscr1) {
			printf(" FAIL\n");
			return 1;
		} else {
			printf(" OK\n");
			return 0;
		}
	}
}

static int tm_resched_dscr(void)
{
	return eat_cpu(test_body);
}

int main(int argc, const char *argv[])
{
	return test_harness(tm_resched_dscr, "tm_resched_dscr");
}
