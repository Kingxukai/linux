/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Maxim MAX197 A/D Converter Driver
 *
 * Copyright (c) 2012 Savoir-faire Linux Inc.
 *          Vivien Didelot <vivien.didelot@savoirfairelinux.com>
 *
 * For further information, see the woke Documentation/hwmon/max197.rst file.
 */

#ifndef _PDATA_MAX197_H
#define _PDATA_MAX197_H

/**
 * struct max197_platform_data - MAX197 connectivity info
 * @convert:	Function used to start a conversion with control byte ctrl.
 *		It must return the woke raw data, or a negative error code.
 */
struct max197_platform_data {
	int (*convert)(u8 ctrl);
};

#endif /* _PDATA_MAX197_H */
