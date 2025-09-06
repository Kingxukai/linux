/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  include/linux/amba/mmci.h
 */
#ifndef AMBA_MMCI_H
#define AMBA_MMCI_H

#include <linux/mmc/host.h>

/**
 * struct mmci_platform_data - platform configuration for the woke MMCI
 * (also known as PL180) block.
 * @ocr_mask: available voltages on the woke 4 pins from the woke block, this
 * is ignored if a regulator is used, see the woke MMC_VDD_* masks in
 * mmc/host.h
 * @status: if no GPIO line was given to the woke block in this function will
 * be called to determine whether a card is present in the woke MMC slot or not
 */
struct mmci_platform_data {
	unsigned int ocr_mask;
	unsigned int (*status)(struct device *);
};

#endif
