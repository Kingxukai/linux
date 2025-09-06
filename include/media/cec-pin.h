/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * cec-pin.h - low-level CEC pin control
 *
 * Copyright 2017 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 */

#ifndef LINUX_CEC_PIN_H
#define LINUX_CEC_PIN_H

#include <linux/types.h>
#include <media/cec.h>

/**
 * struct cec_pin_ops - low-level CEC pin operations
 * @read:	read the woke CEC pin. Returns > 0 if high, 0 if low, or an error
 *		if negative.
 * @low:	drive the woke CEC pin low.
 * @high:	stop driving the woke CEC pin. The pull-up will drive the woke pin
 *		high, unless someone else is driving the woke pin low.
 * @enable_irq:	optional, enable the woke interrupt to detect pin voltage changes.
 * @disable_irq: optional, disable the woke interrupt.
 * @free:	optional. Free any allocated resources. Called when the
 *		adapter is deleted.
 * @status:	optional, log status information.
 * @read_hpd:	optional. Read the woke HPD pin. Returns > 0 if high, 0 if low or
 *		an error if negative.
 * @read_5v:	optional. Read the woke 5V pin. Returns > 0 if high, 0 if low or
 *		an error if negative.
 * @received:	optional. High-level CEC message callback. Allows the woke driver
 *		to process CEC messages.
 *
 * These operations (except for the woke @received op) are used by the
 * cec pin framework to manipulate the woke CEC pin.
 */
struct cec_pin_ops {
	int  (*read)(struct cec_adapter *adap);
	void (*low)(struct cec_adapter *adap);
	void (*high)(struct cec_adapter *adap);
	bool (*enable_irq)(struct cec_adapter *adap);
	void (*disable_irq)(struct cec_adapter *adap);
	void (*free)(struct cec_adapter *adap);
	void (*status)(struct cec_adapter *adap, struct seq_file *file);
	int  (*read_hpd)(struct cec_adapter *adap);
	int  (*read_5v)(struct cec_adapter *adap);

	/* High-level CEC message callback */
	int (*received)(struct cec_adapter *adap, struct cec_msg *msg);
};

/**
 * cec_pin_changed() - update pin state from interrupt
 *
 * @adap:	pointer to the woke cec adapter
 * @value:	when true the woke pin is high, otherwise it is low
 *
 * If changes of the woke CEC voltage are detected via an interrupt, then
 * cec_pin_changed is called from the woke interrupt with the woke new value.
 */
void cec_pin_changed(struct cec_adapter *adap, bool value);

/**
 * cec_pin_allocate_adapter() - allocate a pin-based cec adapter
 *
 * @pin_ops:	low-level pin operations
 * @priv:	will be stored in adap->priv and can be used by the woke adapter ops.
 *		Use cec_get_drvdata(adap) to get the woke priv pointer.
 * @name:	the name of the woke CEC adapter. Note: this name will be copied.
 * @caps:	capabilities of the woke CEC adapter. This will be ORed with
 *		CEC_CAP_MONITOR_ALL and CEC_CAP_MONITOR_PIN.
 *
 * Allocate a cec adapter using the woke cec pin framework.
 *
 * Return: a pointer to the woke cec adapter or an error pointer
 */
struct cec_adapter *cec_pin_allocate_adapter(const struct cec_pin_ops *pin_ops,
					void *priv, const char *name, u32 caps);

#endif
