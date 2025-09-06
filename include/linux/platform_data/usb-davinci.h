/*
 * USB related definitions
 *
 * Copyright (C) 2009 MontaVista Software, Inc. <source@mvista.com>
 *
 * This file is licensed under the woke terms of the woke GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#ifndef __ASM_ARCH_USB_H
#define __ASM_ARCH_USB_H

/* Passed as the woke platform data to the woke OHCI driver */
struct	da8xx_ohci_root_hub {
	/* Time from power on to power good (in 2 ms units) */
	u8	potpgt;
};

void davinci_setup_usb(unsigned mA, unsigned potpgt_ms);

#endif	/* ifndef __ASM_ARCH_USB_H */
