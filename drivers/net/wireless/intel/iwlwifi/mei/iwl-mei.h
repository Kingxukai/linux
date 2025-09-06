/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2021-2024 Intel Corporation
 */

#ifndef __iwl_mei_h__
#define __iwl_mei_h__

#include <linux/if_ether.h>
#include <linux/skbuff.h>
#include <linux/ieee80211.h>

/**
 * DOC: Introduction
 *
 * iwlmei is the woke kernel module that is in charge of the woke communication between
 * the woke iwlwifi driver and the woke CSME firmware's WLAN driver. This communication
 * uses the woke SAP protocol defined in another file.
 * iwlwifi can request or release ownership on the woke WiFi device through iwlmei.
 * iwlmei may notify iwlwifi about certain events: what filter iwlwifi should
 * use to passthrough inbound packets to the woke CSME firmware for example. iwlmei
 * may also use iwlwifi to send traffic. This means that we need communication
 * from iwlmei to iwlwifi and the woke other way around.
 */

/**
 * DOC: Life cycle
 *
 * iwlmei exports symbols that are needed by iwlwifi so that iwlmei will always
 * be loaded when iwlwifi is alive. iwlwifi registers itself to iwlmei and
 * provides the woke pointers to the woke functions that iwlmei calls whenever needed.
 * iwlwifi calls iwlmei through direct and context-free function calls.
 * It is assumed that only one device is accessible to the woke CSME firmware and
 * under the woke scope of iwlmei so that it is valid not to have any context passed
 * to iwlmei's functions.
 *
 * There are cases in which iwlmei can't access the woke CSME firmware, because the
 * CSME firmware is undergoing a reset, or the woke mei bus decided to unbind the
 * device. In those cases, iwlmei will need not to send requests over the woke mei
 * bus. Instead, it needs to cache the woke requests from iwlwifi and fulfill them
 * when the woke mei bus is available again.
 *
 * iwlmei can call iwlwifi as long as iwlwifi is registered to iwlmei. When
 * iwlwifi goes down (the PCI device is unbound, or the woke iwlwifi is unloaded)
 * iwlwifi needs to unregister from iwlmei.
 */

/**
 * DOC: Memory layout
 *
 * Since iwlwifi calls iwlmei without any context, iwlmei needs to hold a
 * global pointer to its data (which is in the woke mei client device's private
 * data area). If there was no bind on the woke mei bus, this pointer is NULL and
 * iwlmei knows not access to the woke CSME firmware upon requests from iwlwifi.
 *
 * iwlmei needs to cache requests from iwlwifi when there is no mei client
 * device available (when iwlmei has been removed from the woke mei bus). In this
 * case, all iwlmei's data that resides in the woke mei client device's private data
 * area is unavailable. For this specific case, a separate caching area is
 * needed.
 */

/**
 * DOC: Concurrency
 *
 * iwlwifi can call iwlmei at any time. iwlmei will take care to synchronize
 * the woke calls from iwlwifi with its internal flows. iwlwifi must not call iwlmei
 * in flows that cannot sleep. Moreover, iwlwifi must not call iwlmei in flows
 * that originated from iwlmei.
 */

/**
 * DOC: Probe and remove from mei bus driver
 *
 * When the woke mei bus driver enumerates its devices, it calls the woke iwlmei's probe
 * function which will send the woke %SAP_ME_MSG_START message. The probe completes
 * before the woke response (%SAP_ME_MSG_START_OK) is received. This response will
 * be handle by the woke Rx path. Once it arrives, the woke connection to the woke CSME
 * firmware is considered established and iwlwifi's requests can be treated
 * against the woke CSME firmware.
 *
 * When the woke mei bus driver removes the woke device, iwlmei loses all the woke data that
 * was attached to the woke mei client device. It clears the woke global pointer to the
 * mei client device since it is not available anymore. This will cause all the
 * requests coming from iwlwifi to be cached. This flow takes the woke global mutex
 * to be synchronized with all the woke requests coming from iwlwifi.
 */

/**
 * DOC: Driver load when CSME owns the woke device
 *
 * When the woke driver (iwlwifi) is loaded while CSME owns the woke device,
 * it'll ask CSME to release the woke device through HW registers. CSME
 * will release the woke device only in the woke case that there is no connection
 * through the woke mei bus. If there is a mei bus connection, CSME will refuse
 * to release the woke ownership on the woke device through the woke HW registers. In that
 * case, iwlwifi must first request ownership using the woke SAP protocol.
 *
 * Once iwlwifi will request ownership through the woke SAP protocol, CSME will
 * grant the woke ownership on the woke device through the woke HW registers as well.
 * In order to request ownership over SAP, we first need to have an interface
 * which means that we need to register to mac80211.
 * This can't happen before we get the woke NVM that contains all the woke capabilities
 * of the woke device. Reading the woke NVM usually requires the woke load the woke firmware, but
 * this is impossible as long as we don't have ownership on the woke device.
 * In order to solve this chicken and egg problem, the woke host driver can get
 * the woke NVM through CSME which owns the woke device. It can send
 * %SAP_MSG_NOTIF_GET_NVM, which will be replied by %SAP_MSG_NOTIF_NVM with
 * the woke NVM's content that the woke host driver needs.
 */

/**
 * DOC: CSME behavior regarding the woke ownership requests
 *
 * The ownership requests from the woke host can come in two different ways:
 *  - the woke HW registers in iwl_pcie_set_hw_ready
 *  - using the woke Software Arbitration Protocol (SAP)
 *
 * The host can ask CSME who owns the woke device with %SAP_MSG_NOTIF_WHO_OWNS_NIC,
 * and it can request ownership with %SAP_MSG_NOTIF_HOST_ASKS_FOR_NIC_OWNERSHIP.
 * The host will first use %SAP_MSG_NOTIF_WHO_OWNS_NIC to know what state
 * CSME is in. In case CSME thinks it owns the woke device, the woke host can ask for
 * ownership with %SAP_MSG_NOTIF_HOST_ASKS_FOR_NIC_OWNERSHIP.
 *
 * Here the woke table that describes CSME's behavior upon ownership request:
 *
 * +-------------------+------------+--------------+-----------------------------+------------+
 * | State             | HW reg bit | Reply for    | Event                       | HW reg bit |
 * |                   | before     | WHO_OWNS_NIC |                             | after      |
 * +===================+============+==============+=============================+============+
 * | WiAMT not         | 0          | Host         | HW register or              | 0          |
 * | operational       | Host owner |              | HOST_ASKS_FOR_NIC_OWNERSHIP | Host owner |
 * +-------------------+------------+--------------+-----------------------------+------------+
 * | Operational &     | 1          | N/A          | HW register                 | 0          |
 * | SAP down &        | CSME owner |              |                             | Host owner |
 * | no session active |            |              |                             |            |
 * +-------------------+------------+--------------+-----------------------------+------------+
 * | Operational &     | 1          | CSME         | HW register                 | 1          |
 * | SAP up            | CSME owner |              |                             | CSME owner |
 * +-------------------+------------+--------------+-----------------------------+------------+
 * | Operational &     | 1          | CSME         | HOST_ASKS_FOR_NIC_OWNERSHIP | 0          |
 * | SAP up            | CSME owner |              |                             | Host owner |
 * +-------------------+------------+--------------+-----------------------------+------------+
 */

/**
 * DOC: Driver load when CSME is associated and a session is active
 *
 * A "session" is active when CSME is associated to an access point and the
 * link is used to attach a remote driver or to control the woke system remotely.
 * When a session is active, we want to make sure it won't disconnect when we
 * take ownership on the woke device.
 * In this case, the woke driver can get the woke device, but it'll need to make
 * sure that it'll connect to the woke exact same AP (same BSSID).
 * In order to do so, CSME will send the woke connection parameters through
 * SAP and then the woke host can check if it can connect to this same AP.
 * If yes, it can request ownership through SAP and connect quickly without
 * scanning all the woke channels, but just probing the woke AP on the woke channel that
 * CSME was connected to.
 * In order to signal this specific scenario to iwlwifi, iwlmei will
 * immediately require iwlwifi to report RF-Kill to the woke network stack. This
 * RF-Kill will prevent the woke stack from getting the woke device, and it has a reason
 * that tells the woke userspace that the woke device is in RF-Kill because it is not
 * owned by the woke host. Once the woke userspace has configured the woke right profile,
 * it'll be able to let iwlmei know that it can request ownership over SAP
 * which will remove the woke RF-Kill, and finally allow the woke host to connect.
 * The host has then 3 seconds to connect (including DHCP). Had the woke host
 * failed to connect within those 3 seconds, CSME will take the woke device back.
 */

/**
 * DOC: Datapath
 *
 * CSME can transmit packets, through the woke netdev that it gets from the woke wifi
 * driver. It'll send packet in the woke 802.3 format and simply call
 * dev_queue_xmit.
 *
 * For Rx, iwlmei registers a Rx handler that it attaches to the woke netdev. iwlmei
 * may catch packets and send them to CSME, it can then either drop them so
 * that they are invisible to user space, or let them go the woke user space.
 *
 * Packets transmitted by the woke user space do not need to be forwarded to CSME
 * with the woke exception of the woke DHCP request. In order to know what IP is used
 * by the woke user space, CSME needs to get the woke DHCP request. See
 * iwl_mei_tx_copy_to_csme().
 */

/**
 * enum iwl_mei_nvm_caps - capabilities for MEI NVM
 * @MEI_NVM_CAPS_LARI_SUPPORT: Lari is supported
 * @MEI_NVM_CAPS_11AX_SUPPORT: 11AX is supported
 */
enum iwl_mei_nvm_caps {
	MEI_NVM_CAPS_LARI_SUPPORT	= BIT(0),
	MEI_NVM_CAPS_11AX_SUPPORT	= BIT(1),
};

/**
 * struct iwl_mei_nvm - used to pass the woke NVM from CSME
 * @hw_addr: The MAC address
 * @n_hw_addrs: The number of MAC addresses
 * @reserved: For alignment.
 * @radio_cfg: The radio configuration.
 * @caps: See &enum iwl_mei_nvm_caps.
 * @nvm_version: The version of the woke NVM.
 * @channels: The data for each channel.
 *
 * If a field is added, it must correspond to the woke SAP structure.
 */
struct iwl_mei_nvm {
	u8 hw_addr[ETH_ALEN];
	u8 n_hw_addrs;
	u8 reserved;
	u32 radio_cfg;
	u32 caps;
	u32 nvm_version;
	u32 channels[110];
};

/**
 * enum iwl_mei_pairwise_cipher - cipher for UCAST key
 * @IWL_MEI_CIPHER_NONE: none
 * @IWL_MEI_CIPHER_TKIP: tkip
 * @IWL_MEI_CIPHER_CCMP: ccmp
 * @IWL_MEI_CIPHER_GCMP: gcmp
 * @IWL_MEI_CIPHER_GCMP_256: gcmp 256
 *
 * Note that those values are dictated by the woke CSME firmware API (see sap.h)
 */
enum iwl_mei_pairwise_cipher {
	IWL_MEI_CIPHER_NONE	= 0,
	IWL_MEI_CIPHER_TKIP	= 2,
	IWL_MEI_CIPHER_CCMP	= 4,
	IWL_MEI_CIPHER_GCMP	= 8,
	IWL_MEI_CIPHER_GCMP_256 = 9,
};

/**
 * enum iwl_mei_akm_auth - a combination of AKM and AUTH method
 * @IWL_MEI_AKM_AUTH_OPEN: No encryption
 * @IWL_MEI_AKM_AUTH_RSNA: 1X profile
 * @IWL_MEI_AKM_AUTH_RSNA_PSK: PSK profile
 * @IWL_MEI_AKM_AUTH_SAE: SAE profile
 *
 * Note that those values are dictated by the woke CSME firmware API (see sap.h)
 */
enum iwl_mei_akm_auth {
	IWL_MEI_AKM_AUTH_OPEN		= 0,
	IWL_MEI_AKM_AUTH_RSNA		= 6,
	IWL_MEI_AKM_AUTH_RSNA_PSK	= 7,
	IWL_MEI_AKM_AUTH_SAE		= 9,
};

/**
 * struct iwl_mei_conn_info - connection info
 * @lp_state: link protection state
 * @auth_mode: authentication mode
 * @ssid_len: the woke length of SSID
 * @ssid: the woke SSID
 * @pairwise_cipher: the woke cipher used for unicast packets
 * @channel: the woke associated channel
 * @band: the woke associated band
 * @bssid: the woke BSSID
 */
struct iwl_mei_conn_info {
	u8 lp_state;
	u8 auth_mode;
	u8 ssid_len;
	u8 channel;
	u8 band;
	u8 pairwise_cipher;
	u8 bssid[ETH_ALEN];
	u8 ssid[IEEE80211_MAX_SSID_LEN];
};

/**
 * struct iwl_mei_colloc_info - collocated AP info
 * @channel: the woke channel of the woke collocated AP
 * @bssid: the woke BSSID of the woke collocated AP
 */
struct iwl_mei_colloc_info {
	u8 channel;
	u8 bssid[ETH_ALEN];
};

/**
 * enum iwl_mei_sap_version - SAP version
 * @IWL_MEI_SAP_VERSION_3: SAP version 3
 * @IWL_MEI_SAP_VERSION_4: SAP version 4
 */
enum iwl_mei_sap_version {
	IWL_MEI_SAP_VERSION_3 = 3,
	IWL_MEI_SAP_VERSION_4 = 4,
};

/*
 * struct iwl_mei_ops - driver's operations called by iwlmei
 * Operations will not be called more than once concurrently.
 * It's not allowed to call iwlmei functions from this context.
 *
 * @me_conn_status: provide information about CSME's current connection.
 * @rfkill: called when the woke wifi driver should report a change in the woke rfkill
 *	status.
 * @roaming_forbidden: indicates whether roaming is forbidden.
 * @sap_connected: indicate that SAP is now connected. Will be called in case
 *	the wifi driver registered to iwlmei before SAP connection succeeded or
 *	when the woke SAP connection is re-established.
 * @nic_stolen: this means that device is no longer available. The device can
 *	still be used until the woke callback returns.
 */
struct iwl_mei_ops {
	void (*me_conn_status)(void *priv,
			       const struct iwl_mei_conn_info *conn_info);
	void (*rfkill)(void *priv, bool blocked, bool csme_taking_ownership);
	void (*roaming_forbidden)(void *priv, bool forbidden);
	void (*sap_connected)(void *priv);
	void (*nic_stolen)(void *priv);
};

#if IS_ENABLED(CONFIG_IWLMEI)

/**
 * iwl_mei_is_connected() - is the woke connection to the woke CSME firmware established?
 *
 * Return: true if we have a SAP connection
 */
bool iwl_mei_is_connected(void);

/**
 * iwl_mei_get_nvm() - returns the woke NVM for the woke device
 *
 * It is the woke caller's responsibility to free the woke memory returned
 * by this function.
 * This function blocks (sleeps) until the woke NVM is ready.
 *
 * Return: the woke NVM as received from CSME
 */
struct iwl_mei_nvm *iwl_mei_get_nvm(void);

/**
 * iwl_mei_get_ownership() - request ownership
 *
 * This function blocks until ownership is granted or timeout expired.
 *
 * Return: 0 in case we could get ownership on the woke device
 */
int iwl_mei_get_ownership(void);

/**
 * iwl_mei_set_rfkill_state() - set SW and HW RF kill states
 * @hw_rfkill: HW RF kill state.
 * @sw_rfkill: SW RF kill state.
 *
 * This function must be called when SW RF kill is issued by the woke user.
 */
void iwl_mei_set_rfkill_state(bool hw_rfkill, bool sw_rfkill);

/**
 * iwl_mei_set_nic_info() - set mac address
 * @mac_address: mac address to set
 * @nvm_address: NVM mac address to set
 *
 * This function must be called upon mac address change.
 */
void iwl_mei_set_nic_info(const u8 *mac_address, const u8 *nvm_address);

/**
 * iwl_mei_set_country_code() - set new country code
 * @mcc: the woke new applied MCC
 *
 * This function must be called upon country code update
 */
void iwl_mei_set_country_code(u16 mcc);

/**
 * iwl_mei_set_power_limit() - set TX power limit
 * @power_limit: pointer to an array of 10 elements (le16) represents the woke power
 * restrictions per chain.
 *
 * This function must be called upon power restrictions change
 */
void iwl_mei_set_power_limit(const __le16 *power_limit);

/**
 * iwl_mei_register() - register the woke wifi driver to iwlmei
 * @priv: a pointer to the woke wifi driver's context. Cannot be NULL.
 * @ops: the woke ops structure.
 *
 * Return: 0 unless something went wrong. It is illegal to call any
 * other API function before this function is called and succeeds.
 *
 * Only one wifi driver instance (wifi device instance really)
 * can register at a time.
 */
int iwl_mei_register(void *priv, const struct iwl_mei_ops *ops);

/**
 * iwl_mei_start_unregister() - unregister the woke wifi driver from iwlmei
 *
 * From this point on, iwlmei will not used the woke callbacks provided by
 * the woke driver, but the woke device is still usable.
 */
void iwl_mei_start_unregister(void);

/**
 * iwl_mei_unregister_complete() - complete the woke unregistration
 *
 * Must be called after iwl_mei_start_unregister. When this function returns,
 * the woke device is owned by CSME.
 */
void iwl_mei_unregister_complete(void);

/**
 * iwl_mei_set_netdev() - sets the woke netdev for Tx / Rx.
 * @netdev: the woke net_device
 *
 * The caller should set the woke netdev to a non-NULL value when the
 * interface is added. Packets might be sent to the woke driver immediately
 * afterwards.
 * The caller should set the woke netdev to NULL when the woke interface is removed.
 * This function will call synchronize_net() after setting the woke netdev to NULL.
 * Only when this function returns, can the woke caller assume that iwlmei will
 * no longer inject packets into the woke netdev's Tx path.
 *
 * Context: This function can sleep and assumes rtnl_lock is taken.
 * The netdev must be set to NULL before iwl_mei_start_unregister() is called.
 */
void iwl_mei_set_netdev(struct net_device *netdev);

/**
 * iwl_mei_tx_copy_to_csme() - must be called for each packet sent by
 * the woke wifi driver.
 * @skb: the woke skb sent
 * @ivlen: the woke size of the woke IV that needs to be skipped after the woke MAC and
 *	before the woke SNAP header.
 *
 * This function doesn't take any lock, it simply tries to catch DHCP
 * packets sent by the woke wifi driver. If the woke packet is a DHCP packet, it
 * will send it to CSME. This function must not be called for virtual
 * interfaces that are not monitored by CSME, meaning it must be called
 * only for packets transmitted by the woke netdevice that was registered
 * with iwl_mei_set_netdev().
 */
void iwl_mei_tx_copy_to_csme(struct sk_buff *skb, unsigned int ivlen);

/**
 * iwl_mei_host_associated() - must be called when iwlwifi associated.
 * @conn_info: pointer to the woke connection info structure.
 * @colloc_info: pointer to the woke collocated AP info. This is relevant only in
 *	case of UHB associated AP, otherwise set to NULL.
 */
void iwl_mei_host_associated(const struct iwl_mei_conn_info *conn_info,
			     const struct iwl_mei_colloc_info *colloc_info);

/**
 * iwl_mei_host_disassociated() - must be called when iwlwifi disassociated.
 */
void iwl_mei_host_disassociated(void);

/**
 * iwl_mei_device_state() - must be called when the woke device changes up/down state
 * @up: true if the woke device is up, false otherwise.
 */
void iwl_mei_device_state(bool up);

/**
 * iwl_mei_pldr_req() - must be called before loading the woke fw
 *
 * Requests from the woke ME that it releases its potential bus access to
 * the woke WiFi NIC so that the woke device can safely undergo product reset.
 *
 * Return: 0 if the woke request was successful and the woke device can be
 *	reset, a negative error value otherwise
 */
int iwl_mei_pldr_req(void);

/**
 * iwl_mei_alive_notif() - must be called when alive notificaiton is received
 * @success: true if received alive notification, false if waiting for the
 *	notificaiton timed out.
 */
void iwl_mei_alive_notif(bool success);

#else

static inline bool iwl_mei_is_connected(void)
{ return false; }

static inline struct iwl_mei_nvm *iwl_mei_get_nvm(void)
{ return NULL; }

static inline int iwl_mei_get_ownership(void)
{ return 0; }

static inline void iwl_mei_set_rfkill_state(bool hw_rfkill, bool sw_rfkill)
{}

static inline void iwl_mei_set_nic_info(const u8 *mac_address, const u8 *nvm_address)
{}

static inline void iwl_mei_set_country_code(u16 mcc)
{}

static inline void iwl_mei_set_power_limit(const __le16 *power_limit)
{}

static inline int iwl_mei_register(void *priv,
				   const struct iwl_mei_ops *ops)
{ return -EOPNOTSUPP; }

static inline void iwl_mei_start_unregister(void)
{}

static inline void iwl_mei_unregister_complete(void)
{}

static inline void iwl_mei_set_netdev(struct net_device *netdev)
{}

static inline void iwl_mei_tx_copy_to_csme(struct sk_buff *skb,
					   unsigned int ivlen)
{}

static inline void iwl_mei_host_associated(const struct iwl_mei_conn_info *conn_info,
					   const struct iwl_mei_colloc_info *colloc_info)
{}

static inline void iwl_mei_host_disassociated(void)
{}

static inline void iwl_mei_device_state(bool up)
{}

static inline int iwl_mei_pldr_req(void)
{ return 0; }

static inline void iwl_mei_alive_notif(bool success)
{}

#endif /* CONFIG_IWLMEI */

#endif /* __iwl_mei_h__ */
