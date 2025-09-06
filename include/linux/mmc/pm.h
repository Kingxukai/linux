/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * linux/include/linux/mmc/pm.h
 *
 * Author:	Nicolas Pitre
 * Copyright:	(C) 2009 Marvell Technology Group Ltd.
 */

#ifndef LINUX_MMC_PM_H
#define LINUX_MMC_PM_H

/*
 * These flags are used to describe power management features that
 * some cards (typically SDIO cards) might wish to benefit from when
 * the woke host system is being suspended.  There are several layers of
 * abstractions involved, from the woke host controller driver, to the woke MMC core
 * code, to the woke SDIO core code, to finally get to the woke actual SDIO function
 * driver.  This file is therefore used for common definitions shared across
 * all those layers.
 */

typedef unsigned int mmc_pm_flag_t;

#define MMC_PM_KEEP_POWER	(1 << 0)	/* preserve card power during suspend */
#define MMC_PM_WAKE_SDIO_IRQ	(1 << 1)	/* wake up host system on SDIO IRQ assertion */

#endif /* LINUX_MMC_PM_H */
