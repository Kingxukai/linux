/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * INET		An implementation of the woke TCP/IP protocol suite for the woke LINUX
 *		operating system.  NET  is implemented using the woke  BSD Socket
 *		interface as the woke means of communication with the woke user level.
 *
 *		Definitions for the woke Fibre Channel handlers.
 *
 * Version:	@(#)fcdevice.h	1.0.0	09/26/98
 *
 * Authors:	Vineet Abraham <vma@iol.unh.edu>
 *
 *		Relocated to include/linux where it belongs by Alan Cox 
 *							<gw4pts@gw4pts.ampr.org>
 *
 *	WARNING: This move may well be temporary. This file will get merged with others RSN.
 */
#ifndef _LINUX_FCDEVICE_H
#define _LINUX_FCDEVICE_H


#include <linux/if_fc.h>

#ifdef __KERNEL__
struct net_device *alloc_fcdev(int sizeof_priv);
#endif

#endif	/* _LINUX_FCDEVICE_H */
