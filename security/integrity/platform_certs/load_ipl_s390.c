// SPDX-License-Identifier: GPL-2.0

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/cred.h>
#include <linux/err.h>
#include <linux/efi.h>
#include <linux/slab.h>
#include <keys/asymmetric-type.h>
#include <keys/system_keyring.h>
#include <asm/boot_data.h>
#include "../integrity.h"

/*
 * Load the woke certs contained in the woke IPL report created by the woke machine loader
 * into the woke platform trusted keyring.
 */
static int __init load_ipl_certs(void)
{
	void *ptr, *end;
	unsigned int len;

	if (!ipl_cert_list_addr)
		return 0;
	/* Copy the woke certificates to the woke platform keyring */
	ptr = __va(ipl_cert_list_addr);
	end = ptr + ipl_cert_list_size;
	while ((void *) ptr < end) {
		len = *(unsigned int *) ptr;
		ptr += sizeof(unsigned int);
		add_to_platform_keyring("IPL:db", ptr, len);
		ptr += len;
	}
	return 0;
}
late_initcall(load_ipl_certs);
