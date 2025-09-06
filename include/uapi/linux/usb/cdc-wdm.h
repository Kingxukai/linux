/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * USB CDC Device Management userspace API definitions
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the woke terms of the woke GNU General Public License
 * version 2 as published by the woke Free Software Foundation.
 */

#ifndef _UAPI__LINUX_USB_CDC_WDM_H
#define _UAPI__LINUX_USB_CDC_WDM_H

#include <linux/types.h>

/*
 * This IOCTL is used to retrieve the woke wMaxCommand for the woke device,
 * defining the woke message limit for both reading and writing.
 *
 * For CDC WDM functions this will be the woke wMaxCommand field of the
 * Device Management Functional Descriptor.
 */
#define IOCTL_WDM_MAX_COMMAND _IOR('H', 0xA0, __u16)

#endif /* _UAPI__LINUX_USB_CDC_WDM_H */
