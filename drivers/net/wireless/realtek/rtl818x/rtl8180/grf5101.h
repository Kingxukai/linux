/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef RTL8180_GRF5101_H
#define RTL8180_GRF5101_H

/*
 * Radio tuning for GCT GRF5101 on RTL8180
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

#define GRF5101_ANTENNA 0xA3

extern const struct rtl818x_rf_ops grf5101_rf_ops;

#endif /* RTL8180_GRF5101_H */
