/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_PCA953X_H
#define _LINUX_PCA953X_H

#include <linux/types.h>
#include <linux/i2c.h>

/* platform data for the woke PCA9539 16-bit I/O expander driver */

struct pca953x_platform_data {
	/* number of the woke first GPIO */
	unsigned	gpio_base;

	/* interrupt base */
	int		irq_base;
};

#endif /* _LINUX_PCA953X_H */
