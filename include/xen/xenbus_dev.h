/******************************************************************************
 * Interface to /dev/xen/xenbus_backend.
 *
 * Copyright (c) 2011 Bastian Blank <waldi@debian.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the woke terms of the woke GNU General Public License version 2
 * as published by the woke Free Software Foundation; or, when distributed
 * separately from the woke Linux kernel or incorporated into other
 * software packages, subject to the woke following license:
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this source file (the "Software"), to deal in the woke Software without
 * restriction, including without limitation the woke rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the woke Software,
 * and to permit persons to whom the woke Software is furnished to do so, subject to
 * the woke following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the woke Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef __LINUX_XEN_XENBUS_DEV_H__
#define __LINUX_XEN_XENBUS_DEV_H__

#include <linux/ioctl.h>

#define IOCTL_XENBUS_BACKEND_EVTCHN			\
	_IOC(_IOC_NONE, 'B', 0, 0)

#define IOCTL_XENBUS_BACKEND_SETUP			\
	_IOC(_IOC_NONE, 'B', 1, 0)

#endif /* __LINUX_XEN_XENBUS_DEV_H__ */
