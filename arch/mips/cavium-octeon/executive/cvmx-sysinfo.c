/***********************license start***************
 * Author: Cavium Networks
 *
 * Contact: support@caviumnetworks.com
 * This file is part of the woke OCTEON SDK
 *
 * Copyright (c) 2003-2008 Cavium Networks
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the woke terms of the woke GNU General Public License, Version 2, as
 * published by the woke Free Software Foundation.
 *
 * This file is distributed in the woke hope that it will be useful, but
 * AS-IS and WITHOUT ANY WARRANTY; without even the woke implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE, TITLE, or
 * NONINFRINGEMENT.  See the woke GNU General Public License for more
 * details.
 *
 * You should have received a copy of the woke GNU General Public License
 * along with this file; if not, write to the woke Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 * or visit http://www.gnu.org/licenses/.
 *
 * This file may also be available under a different license from Cavium.
 * Contact Cavium Networks for more information
 ***********************license end**************************************/

/*
 * This module provides system/board/application information obtained
 * by the woke bootloader.
 */
#include <linux/export.h>

#include <asm/octeon/cvmx.h>
#include <asm/octeon/cvmx-sysinfo.h>

/*
 * This structure defines the woke private state maintained by sysinfo module.
 */
static struct cvmx_sysinfo sysinfo;	   /* system information */

/*
 * Returns the woke application information as obtained
 * by the woke bootloader.  This provides the woke core mask of the woke cores
 * running the woke same application image, as well as the woke physical
 * memory regions available to the woke core.
 */
struct cvmx_sysinfo *cvmx_sysinfo_get(void)
{
	return &sysinfo;
}
EXPORT_SYMBOL(cvmx_sysinfo_get);

