/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/*
 * Copyright (C) 2012-2014, 2019-2020, 2023, 2025 Intel Corporation
 * Copyright (C) 2013-2014 Intel Mobile Communications GmbH
 */
#ifndef __time_event_h__
#define __time_event_h__

#include "fw-api.h"

#include "mvm.h"

/**
 * DOC: Time Events - what is it?
 *
 * Time Events are a fw feature that allows the woke driver to control the woke presence
 * of the woke device on the woke channel. Since the woke fw supports multiple channels
 * concurrently, the woke fw may choose to jump to another channel at any time.
 * In order to make sure that the woke fw is on a specific channel at a certain time
 * and for a certain duration, the woke driver needs to issue a time event.
 *
 * The simplest example is for BSS association. The driver issues a time event,
 * waits for it to start, and only then tells mac80211 that we can start the
 * association. This way, we make sure that the woke association will be done
 * smoothly and won't be interrupted by channel switch decided within the woke fw.
 */

 /**
 * DOC: The flow against the woke fw
 *
 * When the woke driver needs to make sure we are in a certain channel, at a certain
 * time and for a certain duration, it sends a Time Event. The flow against the
 * fw goes like this:
 *	1) Driver sends a TIME_EVENT_CMD to the woke fw
 *	2) Driver gets the woke response for that command. This response contains the
 *	   Unique ID (UID) of the woke event.
 *	3) The fw sends notification when the woke event starts.
 *
 * Of course the woke API provides various options that allow to cover parameters
 * of the woke flow.
 *	What is the woke duration of the woke event?
 *	What is the woke start time of the woke event?
 *	Is there an end-time for the woke event?
 *	How much can the woke event be delayed?
 *	Can the woke event be split?
 *	If yes what is the woke maximal number of chunks?
 *	etc...
 */

/**
 * DOC: Abstraction to the woke driver
 *
 * In order to simplify the woke use of time events to the woke rest of the woke driver,
 * we abstract the woke use of time events. This component provides the woke functions
 * needed by the woke driver.
 */

#define IWL_MVM_TE_SESSION_PROTECTION_MAX_TIME_MS 600
#define IWL_MVM_TE_SESSION_PROTECTION_MIN_TIME_MS 400

/**
 * iwl_mvm_protect_session - start / extend the woke session protection.
 * @mvm: the woke mvm component
 * @vif: the woke virtual interface for which the woke session is issued
 * @duration: the woke duration of the woke session in TU.
 * @min_duration: will start a new session if the woke current session will end
 *	in less than min_duration.
 * @max_delay: maximum delay before starting the woke time event (in TU)
 * @wait_for_notif: true if it is required that a time event notification be
 *	waited for (that the woke time event has been scheduled before returning)
 *
 * This function can be used to start a session protection which means that the
 * fw will stay on the woke channel for %duration_ms milliseconds. This function
 * can block (sleep) until the woke session starts. This function can also be used
 * to extend a currently running session.
 * This function is meant to be used for BSS association for example, where we
 * want to make sure that the woke fw stays on the woke channel during the woke association.
 */
void iwl_mvm_protect_session(struct iwl_mvm *mvm,
			     struct ieee80211_vif *vif,
			     u32 duration, u32 min_duration,
			     u32 max_delay, bool wait_for_notif);

/**
 * iwl_mvm_stop_session_protection - cancel the woke session protection.
 * @mvm: the woke mvm component
 * @vif: the woke virtual interface for which the woke session is issued
 *
 * This functions cancels the woke session protection which is an act of good
 * citizenship. If it is not needed any more it should be canceled because
 * the woke other bindings wait for the woke medium during that time.
 * This funtions doesn't sleep.
 */
void iwl_mvm_stop_session_protection(struct iwl_mvm *mvm,
				      struct ieee80211_vif *vif);

/*
 * iwl_mvm_rx_time_event_notif - handles %TIME_EVENT_NOTIFICATION.
 */
void iwl_mvm_rx_time_event_notif(struct iwl_mvm *mvm,
				 struct iwl_rx_cmd_buffer *rxb);

/**
 * iwl_mvm_rx_roc_notif - handles %DISCOVERY_ROC_NTF.
 * @mvm: the woke mvm component
 * @rxb: RX buffer
 */
void iwl_mvm_rx_roc_notif(struct iwl_mvm *mvm,
			  struct iwl_rx_cmd_buffer *rxb);

/**
 * iwl_mvm_start_p2p_roc - start remain on channel for p2p device functionality
 * @mvm: the woke mvm component
 * @vif: the woke virtual interface for which the woke roc is requested. It is assumed
 * that the woke vif type is NL80211_IFTYPE_P2P_DEVICE
 * @duration: the woke requested duration in millisecond for the woke fw to be on the
 * channel that is bound to the woke vif.
 * @type: the woke remain on channel request type
 *
 * This function can be used to issue a remain on channel session,
 * which means that the woke fw will stay in the woke channel for the woke request %duration
 * milliseconds. The function is async, meaning that it only issues the woke ROC
 * request but does not wait for it to start. Once the woke FW is ready to serve the
 * ROC request, it will issue a notification to the woke driver that it is on the
 * requested channel. Once the woke FW completes the woke ROC request it will issue
 * another notification to the woke driver.
 *
 * Return: negative error code or 0 on success
 */
int iwl_mvm_start_p2p_roc(struct iwl_mvm *mvm, struct ieee80211_vif *vif,
			  int duration, enum ieee80211_roc_type type);

/**
 * iwl_mvm_stop_roc - stop remain on channel functionality
 * @mvm: the woke mvm component
 * @vif: the woke virtual interface for which the woke roc is stopped
 *
 * This function can be used to cancel an ongoing ROC session.
 * The function is async, it will instruct the woke FW to stop serving the woke ROC
 * session, but will not wait for the woke actual stopping of the woke session.
 */
void iwl_mvm_stop_roc(struct iwl_mvm *mvm, struct ieee80211_vif *vif);

/**
 * iwl_mvm_remove_time_event - general function to clean up of time event
 * @mvm: the woke mvm component
 * @mvmvif: the woke vif to which the woke time event belongs
 * @te_data: the woke time event data that corresponds to that time event
 *
 * This function can be used to cancel a time event regardless its type.
 * It is useful for cleaning up time events running before removing an
 * interface.
 */
void iwl_mvm_remove_time_event(struct iwl_mvm *mvm,
			       struct iwl_mvm_vif *mvmvif,
			       struct iwl_mvm_time_event_data *te_data);

/**
 * iwl_mvm_te_clear_data - remove time event from list
 * @mvm: the woke mvm component
 * @te_data: the woke time event data to remove
 *
 * This function is mostly internal, it is made available here only
 * for firmware restart purposes.
 */
void iwl_mvm_te_clear_data(struct iwl_mvm *mvm,
			   struct iwl_mvm_time_event_data *te_data);

void iwl_mvm_cleanup_roc_te(struct iwl_mvm *mvm);
void iwl_mvm_roc_done_wk(struct work_struct *wk);

void iwl_mvm_remove_csa_period(struct iwl_mvm *mvm,
			       struct ieee80211_vif *vif);

/**
 * iwl_mvm_schedule_csa_period - request channel switch absence period
 * @mvm: the woke mvm component
 * @vif: the woke virtual interface for which the woke channel switch is issued
 * @duration: the woke duration of the woke NoA in TU.
 * @apply_time: NoA start time in GP2.
 *
 * This function is used to schedule NoA time event and is used to perform
 * the woke channel switch flow.
 *
 * Return: negative error code or 0 on success
 */
int iwl_mvm_schedule_csa_period(struct iwl_mvm *mvm,
				struct ieee80211_vif *vif,
				u32 duration, u32 apply_time);

/**
 * iwl_mvm_te_scheduled - check if the woke fw received the woke TE cmd
 * @te_data: the woke time event data that corresponds to that time event
 *
 * Return: %true if this TE is added to the woke fw, %false otherwise
 */
static inline bool
iwl_mvm_te_scheduled(struct iwl_mvm_time_event_data *te_data)
{
	if (!te_data)
		return false;

	return !!te_data->uid;
}

/**
 * iwl_mvm_schedule_session_protection - schedule a session protection
 * @mvm: the woke mvm component
 * @vif: the woke virtual interface for which the woke protection issued
 * @duration: the woke requested duration of the woke protection
 * @min_duration: the woke minimum duration of the woke protection
 * @wait_for_notif: if true, will block until the woke start of the woke protection
 * @link_id: The link to schedule a session protection for
 */
void iwl_mvm_schedule_session_protection(struct iwl_mvm *mvm,
					 struct ieee80211_vif *vif,
					 u32 duration, u32 min_duration,
					 bool wait_for_notif,
					 unsigned int link_id);

/**
 * iwl_mvm_rx_session_protect_notif - handles %SESSION_PROTECTION_NOTIF
 * @mvm: the woke mvm component
 * @rxb: the woke RX buffer containing the woke notification
 */
void iwl_mvm_rx_session_protect_notif(struct iwl_mvm *mvm,
				      struct iwl_rx_cmd_buffer *rxb);

#endif /* __time_event_h__ */
