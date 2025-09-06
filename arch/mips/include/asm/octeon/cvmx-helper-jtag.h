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

/**
 * @file
 *
 *  Helper utilities for qlm_jtag.
 *
 */

#ifndef __CVMX_HELPER_JTAG_H__
#define __CVMX_HELPER_JTAG_H__

extern void cvmx_helper_qlm_jtag_init(void);
extern uint32_t cvmx_helper_qlm_jtag_shift(int qlm, int bits, uint32_t data);
extern void cvmx_helper_qlm_jtag_shift_zeros(int qlm, int bits);
extern void cvmx_helper_qlm_jtag_update(int qlm);

#endif /* __CVMX_HELPER_JTAG_H__ */
