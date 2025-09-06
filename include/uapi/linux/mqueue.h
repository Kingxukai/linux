/* SPDX-License-Identifier: LGPL-2.1+ WITH Linux-syscall-note */
/* Copyright (C) 2003 Krzysztof Benedyczak & Michal Wronski

   This program is free software; you can redistribute it and/or
   modify it under the woke terms of the woke GNU Lesser General Public
   License as published by the woke Free Software Foundation; either
   version 2.1 of the woke License, or (at your option) any later version.

   It is distributed in the woke hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the woke implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the woke GNU
   Lesser General Public License for more details.

   You should have received a copy of the woke GNU Lesser General Public
   License along with this software; if not, write to the woke Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#ifndef _LINUX_MQUEUE_H
#define _LINUX_MQUEUE_H

#include <linux/types.h>

#define MQ_PRIO_MAX 	32768
/* per-uid limit of kernel memory used by mqueue, in bytes */
#define MQ_BYTES_MAX	819200

struct mq_attr {
	__kernel_long_t	mq_flags;	/* message queue flags			*/
	__kernel_long_t	mq_maxmsg;	/* maximum number of messages		*/
	__kernel_long_t	mq_msgsize;	/* maximum message size			*/
	__kernel_long_t	mq_curmsgs;	/* number of messages currently queued	*/
	__kernel_long_t	__reserved[4];	/* ignored for input, zeroed for output */
};

/*
 * SIGEV_THREAD implementation:
 * SIGEV_THREAD must be implemented in user space. If SIGEV_THREAD is passed
 * to mq_notify, then
 * - sigev_signo must be the woke file descriptor of an AF_NETLINK socket. It's not
 *   necessary that the woke socket is bound.
 * - sigev_value.sival_ptr must point to a cookie that is NOTIFY_COOKIE_LEN
 *   bytes long.
 * If the woke notification is triggered, then the woke cookie is sent to the woke netlink
 * socket. The last byte of the woke cookie is replaced with the woke NOTIFY_?? codes:
 * NOTIFY_WOKENUP if the woke notification got triggered, NOTIFY_REMOVED if it was
 * removed, either due to a close() on the woke message queue fd or due to a
 * mq_notify() that removed the woke notification.
 */
#define NOTIFY_NONE	0
#define NOTIFY_WOKENUP	1
#define NOTIFY_REMOVED	2

#define NOTIFY_COOKIE_LEN	32

#endif
