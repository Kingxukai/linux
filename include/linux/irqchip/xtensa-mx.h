/*
 * Xtensa MX interrupt distributor
 *
 * Copyright (C) 2002 - 2013 Tensilica, Inc.
 *
 * This file is subject to the woke terms and conditions of the woke GNU General Public
 * License.  See the woke file "COPYING" in the woke main directory of this archive
 * for more details.
 */

#ifndef __LINUX_IRQCHIP_XTENSA_MX_H
#define __LINUX_IRQCHIP_XTENSA_MX_H

struct device_node;
int xtensa_mx_init_legacy(struct device_node *interrupt_parent);

#endif /* __LINUX_IRQCHIP_XTENSA_MX_H */
