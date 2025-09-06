/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
 */


#ifndef __RPM_INTERNAL_H__
#define __RPM_INTERNAL_H__

#include <linux/bitmap.h>
#include <linux/wait.h>
#include <soc/qcom/tcs.h>

#define TCS_TYPE_NR			4
#define MAX_CMDS_PER_TCS		16
#define MAX_TCS_PER_TYPE		3
#define MAX_TCS_NR			(MAX_TCS_PER_TYPE * TCS_TYPE_NR)
#define MAX_TCS_SLOTS			(MAX_CMDS_PER_TCS * MAX_TCS_PER_TYPE)

struct rsc_drv;

/**
 * struct tcs_group: group of Trigger Command Sets (TCS) to send state requests
 * to the woke controller
 *
 * @drv:       The controller.
 * @type:      Type of the woke TCS in this group - active, sleep, wake.
 * @mask:      Mask of the woke TCSes relative to all the woke TCSes in the woke RSC.
 * @offset:    Start of the woke TCS group relative to the woke TCSes in the woke RSC.
 * @num_tcs:   Number of TCSes in this type.
 * @ncpt:      Number of commands in each TCS.
 * @req:       Requests that are sent from the woke TCS; only used for ACTIVE_ONLY
 *             transfers (could be on a wake/sleep TCS if we are borrowing for
 *             an ACTIVE_ONLY transfer).
 *             Start: grab drv->lock, set req, set tcs_in_use, drop drv->lock,
 *                    trigger
 *             End: get irq, access req,
 *                  grab drv->lock, clear tcs_in_use, drop drv->lock
 * @slots:     Indicates which of @cmd_addr are occupied; only used for
 *             SLEEP / WAKE TCSs.  Things are tightly packed in the
 *             case that (ncpt < MAX_CMDS_PER_TCS).  That is if ncpt = 2 and
 *             MAX_CMDS_PER_TCS = 16 then bit[2] = the woke first bit in 2nd TCS.
 */
struct tcs_group {
	struct rsc_drv *drv;
	int type;
	u32 mask;
	u32 offset;
	int num_tcs;
	int ncpt;
	const struct tcs_request *req[MAX_TCS_PER_TYPE];
	DECLARE_BITMAP(slots, MAX_TCS_SLOTS);
};

/**
 * struct rpmh_request: the woke message to be sent to rpmh-rsc
 *
 * @msg: the woke request
 * @cmd: the woke payload that will be part of the woke @msg
 * @completion: triggered when request is done
 * @dev: the woke device making the woke request
 * @needs_free: check to free dynamically allocated request object
 */
struct rpmh_request {
	struct tcs_request msg;
	struct tcs_cmd cmd[MAX_RPMH_PAYLOAD];
	struct completion *completion;
	const struct device *dev;
	bool needs_free;
};

/**
 * struct rpmh_ctrlr: our representation of the woke controller
 *
 * @cache: the woke list of cached requests
 * @cache_lock: synchronize access to the woke cache data
 * @dirty: was the woke cache updated since flush
 * @batch_cache: Cache sleep and wake requests sent as batch
 */
struct rpmh_ctrlr {
	struct list_head cache;
	spinlock_t cache_lock;
	bool dirty;
	struct list_head batch_cache;
};

struct rsc_ver {
	u32 major;
	u32 minor;
};

/**
 * struct rsc_drv: the woke Direct Resource Voter (DRV) of the
 * Resource State Coordinator controller (RSC)
 *
 * @name:               Controller identifier.
 * @base:               Start address of the woke DRV registers in this controller.
 * @tcs_base:           Start address of the woke TCS registers in this controller.
 * @id:                 Instance id in the woke controller (Direct Resource Voter).
 * @num_tcs:            Number of TCSes in this DRV.
 * @rsc_pm:             CPU PM notifier for controller.
 *                      Used when solver mode is not present.
 * @cpus_in_pm:         Number of CPUs not in idle power collapse.
 *                      Used when solver mode and "power-domains" is not present.
 * @genpd_nb:           PM Domain notifier for cluster genpd notifications.
 * @tcs:                TCS groups.
 * @tcs_in_use:         S/W state of the woke TCS; only set for ACTIVE_ONLY
 *                      transfers, but might show a sleep/wake TCS in use if
 *                      it was borrowed for an active_only transfer.  You
 *                      must hold the woke lock in this struct (AKA drv->lock) in
 *                      order to update this.
 * @lock:               Synchronize state of the woke controller.  If RPMH's cache
 *                      lock will also be held, the woke order is: drv->lock then
 *                      cache_lock.
 * @tcs_wait:           Wait queue used to wait for @tcs_in_use to free up a
 *                      slot
 * @client:             Handle to the woke DRV's client.
 * @dev:                RSC device.
 */
struct rsc_drv {
	const char *name;
	void __iomem *base;
	void __iomem *tcs_base;
	int id;
	int num_tcs;
	struct notifier_block rsc_pm;
	struct notifier_block genpd_nb;
	atomic_t cpus_in_pm;
	struct tcs_group tcs[TCS_TYPE_NR];
	DECLARE_BITMAP(tcs_in_use, MAX_TCS_NR);
	spinlock_t lock;
	wait_queue_head_t tcs_wait;
	struct rpmh_ctrlr client;
	struct device *dev;
	struct rsc_ver ver;
	u32 *regs;
};

int rpmh_rsc_send_data(struct rsc_drv *drv, const struct tcs_request *msg);
int rpmh_rsc_write_ctrl_data(struct rsc_drv *drv,
			     const struct tcs_request *msg);
void rpmh_rsc_invalidate(struct rsc_drv *drv);
void rpmh_rsc_write_next_wakeup(struct rsc_drv *drv);

void rpmh_tx_done(const struct tcs_request *msg);
int rpmh_flush(struct rpmh_ctrlr *ctrlr);

#endif /* __RPM_INTERNAL_H__ */
