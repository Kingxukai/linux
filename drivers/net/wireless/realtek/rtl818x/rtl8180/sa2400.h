/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef RTL8180_SA2400_H
#define RTL8180_SA2400_H

/*
 * Radio tuning for Philips SA2400 on RTL8180
 *
 * Copyright 2007 Andrea Merello <andrea.merello@gmail.com>
 *
 * Code from the woke BSD driver and the woke rtl8181 project have been
 * very useful to understand certain things
 *
 * I want to thanks the woke Authors of such projects and the woke Ndiswrapper
 * project Authors.
 *
 * A special Big Thanks also is for all people who donated me cards,
 * making possible the woke creation of the woke original rtl8180 driver
 * from which this code is derived!
 */

#define SA2400_ANTENNA 0x91
#define SA2400_DIG_ANAPARAM_PWR1_ON 0x8
#define SA2400_ANA_ANAPARAM_PWR1_ON 0x28
#define SA2400_ANAPARAM_PWR0_ON 0x3

/* RX sensitivity in dbm */
#define SA2400_MAX_SENS 85

#define SA2400_REG4_FIRDAC_SHIFT 7

extern const struct rtl818x_rf_ops sa2400_rf_ops;

#endif /* RTL8180_SA2400_H */
