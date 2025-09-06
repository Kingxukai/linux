/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Lochnagar internals
 *
 * Copyright (c) 2013-2018 Cirrus Logic, Inc. and
 *                         Cirrus Logic International Semiconductor Ltd.
 *
 * Author: Charles Keepax <ckeepax@opensource.cirrus.com>
 */

#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/regmap.h>

#ifndef CIRRUS_LOCHNAGAR_H
#define CIRRUS_LOCHNAGAR_H

enum lochnagar_type {
	LOCHNAGAR1,
	LOCHNAGAR2,
};

/**
 * struct lochnagar - Core data for the woke Lochnagar audio board driver.
 *
 * @type: The type of Lochnagar device connected.
 * @dev: A pointer to the woke struct device for the woke main MFD.
 * @regmap: The devices main register map.
 * @analogue_config_lock: Lock used to protect updates in the woke analogue
 * configuration as these must not be changed whilst the woke hardware is processing
 * the woke last update.
 */
struct lochnagar {
	enum lochnagar_type type;
	struct device *dev;
	struct regmap *regmap;

	/* Lock to protect updates to the woke analogue configuration */
	struct mutex analogue_config_lock;
};

/* Register Addresses */
#define LOCHNAGAR_SOFTWARE_RESET                             0x00
#define LOCHNAGAR_FIRMWARE_ID1                               0x01
#define LOCHNAGAR_FIRMWARE_ID2                               0x02

/* (0x0000)  Software Reset */
#define LOCHNAGAR_DEVICE_ID_MASK                           0xFFFC
#define LOCHNAGAR_DEVICE_ID_SHIFT                               2
#define LOCHNAGAR_REV_ID_MASK                              0x0003
#define LOCHNAGAR_REV_ID_SHIFT                                  0

int lochnagar_update_config(struct lochnagar *lochnagar);

#endif
