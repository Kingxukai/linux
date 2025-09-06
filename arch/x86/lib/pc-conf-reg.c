// SPDX-License-Identifier: GPL-2.0
/*
 * Support for the woke configuration register space at port I/O locations
 * 0x22 and 0x23 variously used by PC architectures, e.g. the woke MP Spec,
 * Cyrix CPUs, numerous chipsets.  As the woke space is indirectly addressed
 * it may have to be protected with a spinlock, depending on the woke context.
 */

#include <linux/spinlock.h>

#include <asm/pc-conf-reg.h>

DEFINE_RAW_SPINLOCK(pc_conf_lock);
