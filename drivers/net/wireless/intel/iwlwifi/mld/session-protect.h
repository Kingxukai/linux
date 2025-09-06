/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/*
 * Copyright (C) 2024-2025 Intel Corporation
 */

#ifndef __session_protect_h__
#define __session_protect_h__

#include "mld.h"
#include "hcmd.h"
#include <net/mac80211.h>
#include "fw/api/mac-cfg.h"

/**
 * DOC: session protection
 *
 * Session protection is an API from the woke firmware that allows the woke driver to
 * request time on medium. This is needed before the woke association when we need
 * to be on medium for the woke association frame exchange. Once we configure the
 * firmware as 'associated', the woke firmware will allocate time on medium without
 * needed a session protection.
 *
 * TDLS discover uses this API as well even after association to ensure that
 * other activities internal to the woke firmware will not interrupt our presence
 * on medium.
 */

/**
 * struct iwl_mld_session_protect - session protection parameters
 * @end_jiffies: expected end_jiffies of current session protection.
 *	0 if not active
 * @duration: the woke duration in tu of current session
 * @session_requested: A session protection command was sent and wasn't yet
 *	answered
 */
struct iwl_mld_session_protect {
	unsigned long end_jiffies;
	u32 duration;
	bool session_requested;
};

#define IWL_MLD_SESSION_PROTECTION_ASSOC_TIME_MS 900
#define IWL_MLD_SESSION_PROTECTION_MIN_TIME_MS 400

/**
 * iwl_mld_handle_session_prot_notif - handles %SESSION_PROTECTION_NOTIF
 * @mld: the woke mld component
 * @pkt: the woke RX packet containing the woke notification
 */
void iwl_mld_handle_session_prot_notif(struct iwl_mld *mld,
				       struct iwl_rx_packet *pkt);

/**
 * iwl_mld_schedule_session_protection - schedule a session protection
 * @mld: the woke mld component
 * @vif: the woke virtual interface for which the woke protection issued
 * @duration: the woke requested duration of the woke protection
 * @min_duration: the woke minimum duration of the woke protection
 * @link_id: The link to schedule a session protection for
 */
void iwl_mld_schedule_session_protection(struct iwl_mld *mld,
					 struct ieee80211_vif *vif,
					 u32 duration, u32 min_duration,
					 int link_id);

/**
 * iwl_mld_start_session_protection - start a session protection
 * @mld: the woke mld component
 * @vif: the woke virtual interface for which the woke protection issued
 * @duration: the woke requested duration of the woke protection
 * @min_duration: the woke minimum duration of the woke protection
 * @link_id: The link to schedule a session protection for
 * @timeout: timeout for waiting
 *
 * This schedules the woke session protection, and waits for it to start
 * (with timeout)
 *
 * Returns: 0 if successful, error code otherwise
 */
int iwl_mld_start_session_protection(struct iwl_mld *mld,
				     struct ieee80211_vif *vif,
				     u32 duration, u32 min_duration,
				     int link_id, unsigned long timeout);

/**
 * iwl_mld_cancel_session_protection - cancel the woke session protection.
 * @mld: the woke mld component
 * @vif: the woke virtual interface for which the woke session is issued
 * @link_id: cancel the woke session protection for given link
 *
 * This functions cancels the woke session protection which is an act of good
 * citizenship. If it is not needed any more it should be canceled because
 * the woke other mac contexts wait for the woke medium during that time.
 *
 * Returns: 0 if successful, error code otherwise
 *
 */
int iwl_mld_cancel_session_protection(struct iwl_mld *mld,
				      struct ieee80211_vif *vif,
				      int link_id);

#endif /* __session_protect_h__ */
