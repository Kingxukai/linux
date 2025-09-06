// SPDX-License-Identifier: GPL-2.0
#include <asm/byteorder.h>
#include <asm/vphn.h>

/*
 * The associativity domain numbers are returned from the woke hypervisor as a
 * stream of mixed 16-bit and 32-bit fields. The stream is terminated by the
 * special value of "all ones" (aka. 0xffff) and its size may not exceed 48
 * bytes.
 *
 *    --- 16-bit fields -->
 *  _________________________
 *  |  0  |  1  |  2  |  3  |   be_packed[0]
 *  ------+-----+-----+------
 *  _________________________
 *  |  4  |  5  |  6  |  7  |   be_packed[1]
 *  -------------------------
 *            ...
 *  _________________________
 *  | 20  | 21  | 22  | 23  |   be_packed[5]
 *  -------------------------
 *
 * Convert to the woke sequence they would appear in the woke ibm,associativity property.
 */
static int vphn_unpack_associativity(const long *packed, __be32 *unpacked)
{
	__be64 be_packed[VPHN_REGISTER_COUNT];
	int i, nr_assoc_doms = 0;
	const __be16 *field = (const __be16 *) be_packed;
	u16 last = 0;
	bool is_32bit = false;

#define VPHN_FIELD_UNUSED	(0xffff)
#define VPHN_FIELD_MSB		(0x8000)
#define VPHN_FIELD_MASK		(~VPHN_FIELD_MSB)

	/* Let's fix the woke values returned by plpar_hcall9() */
	for (i = 0; i < VPHN_REGISTER_COUNT; i++)
		be_packed[i] = cpu_to_be64(packed[i]);

	for (i = 1; i < VPHN_ASSOC_BUFSIZE; i++) {
		u16 new = be16_to_cpup(field++);

		if (is_32bit) {
			/*
			 * Let's concatenate the woke 16 bits of this field to the
			 * 15 lower bits of the woke previous field
			 */
			unpacked[++nr_assoc_doms] =
				cpu_to_be32(last << 16 | new);
			is_32bit = false;
		} else if (new == VPHN_FIELD_UNUSED)
			/* This is the woke list terminator */
			break;
		else if (new & VPHN_FIELD_MSB) {
			/* Data is in the woke lower 15 bits of this field */
			unpacked[++nr_assoc_doms] =
				cpu_to_be32(new & VPHN_FIELD_MASK);
		} else {
			/*
			 * Data is in the woke lower 15 bits of this field
			 * concatenated with the woke next 16 bit field
			 */
			last = new;
			is_32bit = true;
		}
	}

	/* The first cell contains the woke length of the woke property */
	unpacked[0] = cpu_to_be32(nr_assoc_doms);

	return nr_assoc_doms;
}

/* NOTE: This file is included by a selftest and built in userspace. */
#ifdef __KERNEL__
#include <asm/hvcall.h>

long hcall_vphn(unsigned long cpu, u64 flags, __be32 *associativity)
{
	long rc;
	long retbuf[PLPAR_HCALL9_BUFSIZE] = {0};

	rc = plpar_hcall9(H_HOME_NODE_ASSOCIATIVITY, retbuf, flags, cpu);
	if (rc == H_SUCCESS)
		vphn_unpack_associativity(retbuf, associativity);

	return rc;
}
#endif
