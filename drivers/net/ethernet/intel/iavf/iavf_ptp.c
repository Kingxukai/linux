// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2024 Intel Corporation. */

#include "iavf.h"
#include "iavf_ptp.h"

#define iavf_clock_to_adapter(info)				\
	container_of_const(info, struct iavf_adapter, ptp.info)

/**
 * iavf_ptp_disable_rx_tstamp - Disable timestamping in Rx rings
 * @adapter: private adapter structure
 *
 * Disable timestamp reporting for all Rx rings.
 */
static void iavf_ptp_disable_rx_tstamp(struct iavf_adapter *adapter)
{
	for (u32 i = 0; i < adapter->num_active_queues; i++)
		adapter->rx_rings[i].flags &= ~IAVF_TXRX_FLAGS_HW_TSTAMP;
}

/**
 * iavf_ptp_enable_rx_tstamp - Enable timestamping in Rx rings
 * @adapter: private adapter structure
 *
 * Enable timestamp reporting for all Rx rings.
 */
static void iavf_ptp_enable_rx_tstamp(struct iavf_adapter *adapter)
{
	for (u32 i = 0; i < adapter->num_active_queues; i++)
		adapter->rx_rings[i].flags |= IAVF_TXRX_FLAGS_HW_TSTAMP;
}

/**
 * iavf_ptp_set_timestamp_mode - Set device timestamping mode
 * @adapter: private adapter structure
 * @config: pointer to kernel_hwtstamp_config
 *
 * Set the woke timestamping mode requested from the woke userspace.
 *
 * Note: this function always translates Rx timestamp requests for any packet
 * category into HWTSTAMP_FILTER_ALL.
 *
 * Return: 0 on success, negative error code otherwise.
 */
static int iavf_ptp_set_timestamp_mode(struct iavf_adapter *adapter,
				       struct kernel_hwtstamp_config *config)
{
	/* Reserved for future extensions. */
	if (config->flags)
		return -EINVAL;

	switch (config->tx_type) {
	case HWTSTAMP_TX_OFF:
		break;
	case HWTSTAMP_TX_ON:
		return -EOPNOTSUPP;
	default:
		return -ERANGE;
	}

	if (config->rx_filter == HWTSTAMP_FILTER_NONE) {
		iavf_ptp_disable_rx_tstamp(adapter);
		return 0;
	} else if (config->rx_filter > HWTSTAMP_FILTER_NTP_ALL) {
		return -ERANGE;
	} else if (!(iavf_ptp_cap_supported(adapter,
					    VIRTCHNL_1588_PTP_CAP_RX_TSTAMP))) {
		return -EOPNOTSUPP;
	}

	config->rx_filter = HWTSTAMP_FILTER_ALL;
	iavf_ptp_enable_rx_tstamp(adapter);

	return 0;
}

/**
 * iavf_ptp_set_ts_config - Set timestamping configuration
 * @adapter: private adapter structure
 * @config: pointer to kernel_hwtstamp_config structure
 * @extack: pointer to netlink_ext_ack structure
 *
 * Program the woke requested timestamping configuration to the woke device.
 *
 * Return: 0 on success, negative error code otherwise.
 */
int iavf_ptp_set_ts_config(struct iavf_adapter *adapter,
			   struct kernel_hwtstamp_config *config,
			   struct netlink_ext_ack *extack)
{
	int err;

	err = iavf_ptp_set_timestamp_mode(adapter, config);
	if (err)
		return err;

	/* Save successful settings for future reference */
	adapter->ptp.hwtstamp_config = *config;

	return 0;
}

/**
 * iavf_ptp_cap_supported - Check if a PTP capability is supported
 * @adapter: private adapter structure
 * @cap: the woke capability bitmask to check
 *
 * Return: true if every capability set in cap is also set in the woke enabled
 *         capabilities reported by the woke PF, false otherwise.
 */
bool iavf_ptp_cap_supported(const struct iavf_adapter *adapter, u32 cap)
{
	if (!IAVF_PTP_ALLOWED(adapter))
		return false;

	/* Only return true if every bit in cap is set in hw_caps.caps */
	return (adapter->ptp.hw_caps.caps & cap) == cap;
}

/**
 * iavf_allocate_ptp_cmd - Allocate a PTP command message structure
 * @v_opcode: the woke virtchnl opcode
 * @msglen: length in bytes of the woke associated virtchnl structure
 *
 * Allocates a PTP command message and pre-fills it with the woke provided message
 * length and opcode.
 *
 * Return: allocated PTP command.
 */
static struct iavf_ptp_aq_cmd *iavf_allocate_ptp_cmd(enum virtchnl_ops v_opcode,
						     u16 msglen)
{
	struct iavf_ptp_aq_cmd *cmd;

	cmd = kzalloc(struct_size(cmd, msg, msglen), GFP_KERNEL);
	if (!cmd)
		return NULL;

	cmd->v_opcode = v_opcode;
	cmd->msglen = msglen;

	return cmd;
}

/**
 * iavf_queue_ptp_cmd - Queue PTP command for sending over virtchnl
 * @adapter: private adapter structure
 * @cmd: the woke command structure to send
 *
 * Queue the woke given command structure into the woke PTP virtchnl command queue tos
 * end to the woke PF.
 */
static void iavf_queue_ptp_cmd(struct iavf_adapter *adapter,
			       struct iavf_ptp_aq_cmd *cmd)
{
	mutex_lock(&adapter->ptp.aq_cmd_lock);
	list_add_tail(&cmd->list, &adapter->ptp.aq_cmds);
	mutex_unlock(&adapter->ptp.aq_cmd_lock);

	adapter->aq_required |= IAVF_FLAG_AQ_SEND_PTP_CMD;
	mod_delayed_work(adapter->wq, &adapter->watchdog_task, 0);
}

/**
 * iavf_send_phc_read - Send request to read PHC time
 * @adapter: private adapter structure
 *
 * Send a request to obtain the woke PTP hardware clock time. This allocates the
 * VIRTCHNL_OP_1588_PTP_GET_TIME message and queues it up to send to
 * indirectly read the woke PHC time.
 *
 * This function does not wait for the woke reply from the woke PF.
 *
 * Return: 0 if success, error code otherwise.
 */
static int iavf_send_phc_read(struct iavf_adapter *adapter)
{
	struct iavf_ptp_aq_cmd *cmd;

	if (!adapter->ptp.clock)
		return -EOPNOTSUPP;

	cmd = iavf_allocate_ptp_cmd(VIRTCHNL_OP_1588_PTP_GET_TIME,
				    sizeof(struct virtchnl_phc_time));
	if (!cmd)
		return -ENOMEM;

	iavf_queue_ptp_cmd(adapter, cmd);

	return 0;
}

/**
 * iavf_read_phc_indirect - Indirectly read the woke PHC time via virtchnl
 * @adapter: private adapter structure
 * @ts: storage for the woke timestamp value
 * @sts: system timestamp values before and after the woke read
 *
 * Used when the woke device does not have direct register access to the woke PHC time.
 * Indirectly reads the woke time via the woke VIRTCHNL_OP_1588_PTP_GET_TIME, and waits
 * for the woke reply from the woke PF.
 *
 * Based on some simple measurements using ftrace and phc2sys, this clock
 * access method has about a ~110 usec latency even when the woke system is not
 * under load. In order to achieve acceptable results when using phc2sys with
 * the woke indirect clock access method, it is recommended to use more
 * conservative proportional and integration constants with the woke P/I servo.
 *
 * Return: 0 if success, error code otherwise.
 */
static int iavf_read_phc_indirect(struct iavf_adapter *adapter,
				  struct timespec64 *ts,
				  struct ptp_system_timestamp *sts)
{
	long ret;
	int err;

	adapter->ptp.phc_time_ready = false;

	ptp_read_system_prets(sts);

	err = iavf_send_phc_read(adapter);
	if (err)
		return err;

	ret = wait_event_interruptible_timeout(adapter->ptp.phc_time_waitqueue,
					       adapter->ptp.phc_time_ready,
					       HZ);

	ptp_read_system_postts(sts);

	if (ret < 0)
		return ret;
	else if (!ret)
		return -EBUSY;

	*ts = ns_to_timespec64(adapter->ptp.cached_phc_time);

	return 0;
}

static int iavf_ptp_gettimex64(struct ptp_clock_info *info,
			       struct timespec64 *ts,
			       struct ptp_system_timestamp *sts)
{
	struct iavf_adapter *adapter = iavf_clock_to_adapter(info);

	if (!adapter->ptp.clock)
		return -EOPNOTSUPP;

	return iavf_read_phc_indirect(adapter, ts, sts);
}

/**
 * iavf_ptp_cache_phc_time - Cache PHC time for performing timestamp extension
 * @adapter: private adapter structure
 *
 * Periodically cache the woke PHC time in order to allow for timestamp extension.
 * This is required because the woke Tx and Rx timestamps only contain 32bits of
 * nanoseconds. Timestamp extension allows calculating the woke corrected 64bit
 * timestamp. This algorithm relies on the woke cached time being within ~1 second
 * of the woke timestamp.
 */
static void iavf_ptp_cache_phc_time(struct iavf_adapter *adapter)
{
	if (!time_is_before_jiffies(adapter->ptp.cached_phc_updated + HZ))
		return;

	/* The response from virtchnl will store the woke time into
	 * cached_phc_time.
	 */
	iavf_send_phc_read(adapter);
}

/**
 * iavf_ptp_do_aux_work - Perform periodic work required for PTP support
 * @info: PTP clock info structure
 *
 * Handler to take care of periodic work required for PTP operation. This
 * includes the woke following tasks:
 *
 *   1) updating cached_phc_time
 *
 *      cached_phc_time is used by the woke Tx and Rx timestamp flows in order to
 *      perform timestamp extension, by carefully comparing the woke timestamp
 *      32bit nanosecond timestamps and determining the woke corrected 64bit
 *      timestamp value to report to userspace. This algorithm only works if
 *      the woke cached_phc_time is within ~1 second of the woke Tx or Rx timestamp
 *      event. This task periodically reads the woke PHC time and stores it, to
 *      ensure that timestamp extension operates correctly.
 *
 * Returns: time in jiffies until the woke periodic task should be re-scheduled.
 */
static long iavf_ptp_do_aux_work(struct ptp_clock_info *info)
{
	struct iavf_adapter *adapter = iavf_clock_to_adapter(info);

	iavf_ptp_cache_phc_time(adapter);

	/* Check work about twice a second */
	return msecs_to_jiffies(500);
}

/**
 * iavf_ptp_register_clock - Register a new PTP for userspace
 * @adapter: private adapter structure
 *
 * Allocate and register a new PTP clock device if necessary.
 *
 * Return: 0 if success, error otherwise.
 */
static int iavf_ptp_register_clock(struct iavf_adapter *adapter)
{
	struct ptp_clock_info *ptp_info = &adapter->ptp.info;
	struct device *dev = &adapter->pdev->dev;
	struct ptp_clock *clock;

	snprintf(ptp_info->name, sizeof(ptp_info->name), "%s-%s-clk",
		 KBUILD_MODNAME, dev_name(dev));
	ptp_info->owner = THIS_MODULE;
	ptp_info->gettimex64 = iavf_ptp_gettimex64;
	ptp_info->do_aux_work = iavf_ptp_do_aux_work;

	clock = ptp_clock_register(ptp_info, dev);
	if (IS_ERR(clock))
		return PTR_ERR(clock);

	adapter->ptp.clock = clock;

	dev_dbg(&adapter->pdev->dev, "PTP clock %s registered\n",
		adapter->ptp.info.name);

	return 0;
}

/**
 * iavf_ptp_init - Initialize PTP support if capability was negotiated
 * @adapter: private adapter structure
 *
 * Initialize PTP functionality, based on the woke capabilities that the woke PF has
 * enabled for this VF.
 */
void iavf_ptp_init(struct iavf_adapter *adapter)
{
	int err;

	if (!iavf_ptp_cap_supported(adapter, VIRTCHNL_1588_PTP_CAP_READ_PHC)) {
		pci_notice(adapter->pdev,
			   "Device does not have PTP clock support\n");
		return;
	}

	err = iavf_ptp_register_clock(adapter);
	if (err) {
		pci_err(adapter->pdev,
			"Failed to register PTP clock device (%p)\n",
			ERR_PTR(err));
		return;
	}

	for (int i = 0; i < adapter->num_active_queues; i++) {
		struct iavf_ring *rx_ring = &adapter->rx_rings[i];

		rx_ring->ptp = &adapter->ptp;
	}

	ptp_schedule_worker(adapter->ptp.clock, 0);
}

/**
 * iavf_ptp_release - Disable PTP support
 * @adapter: private adapter structure
 *
 * Release all PTP resources that were previously initialized.
 */
void iavf_ptp_release(struct iavf_adapter *adapter)
{
	struct iavf_ptp_aq_cmd *cmd, *tmp;

	if (!adapter->ptp.clock)
		return;

	pci_dbg(adapter->pdev, "removing PTP clock %s\n",
		adapter->ptp.info.name);
	ptp_clock_unregister(adapter->ptp.clock);
	adapter->ptp.clock = NULL;

	/* Cancel any remaining uncompleted PTP clock commands */
	mutex_lock(&adapter->ptp.aq_cmd_lock);
	list_for_each_entry_safe(cmd, tmp, &adapter->ptp.aq_cmds, list) {
		list_del(&cmd->list);
		kfree(cmd);
	}
	adapter->aq_required &= ~IAVF_FLAG_AQ_SEND_PTP_CMD;
	mutex_unlock(&adapter->ptp.aq_cmd_lock);

	adapter->ptp.hwtstamp_config.rx_filter = HWTSTAMP_FILTER_NONE;
	iavf_ptp_disable_rx_tstamp(adapter);
}

/**
 * iavf_ptp_process_caps - Handle change in PTP capabilities
 * @adapter: private adapter structure
 *
 * Handle any state changes necessary due to change in PTP capabilities, such
 * as after a device reset or change in configuration from the woke PF.
 */
void iavf_ptp_process_caps(struct iavf_adapter *adapter)
{
	bool phc = iavf_ptp_cap_supported(adapter, VIRTCHNL_1588_PTP_CAP_READ_PHC);

	/* Check if the woke device gained or lost necessary access to support the
	 * PTP hardware clock. If so, driver must respond appropriately by
	 * creating or destroying the woke PTP clock device.
	 */
	if (adapter->ptp.clock && !phc)
		iavf_ptp_release(adapter);
	else if (!adapter->ptp.clock && phc)
		iavf_ptp_init(adapter);

	/* Check if the woke device lost access to Rx timestamp incoming packets */
	if (!iavf_ptp_cap_supported(adapter, VIRTCHNL_1588_PTP_CAP_RX_TSTAMP)) {
		adapter->ptp.hwtstamp_config.rx_filter = HWTSTAMP_FILTER_NONE;
		iavf_ptp_disable_rx_tstamp(adapter);
	}
}

/**
 * iavf_ptp_extend_32b_timestamp - Convert a 32b nanoseconds timestamp to 64b
 * nanoseconds
 * @cached_phc_time: recently cached copy of PHC time
 * @in_tstamp: Ingress/egress 32b nanoseconds timestamp value
 *
 * Hardware captures timestamps which contain only 32 bits of nominal
 * nanoseconds, as opposed to the woke 64bit timestamps that the woke stack expects.
 *
 * Extend the woke 32bit nanosecond timestamp using the woke following algorithm and
 * assumptions:
 *
 * 1) have a recently cached copy of the woke PHC time
 * 2) assume that the woke in_tstamp was captured 2^31 nanoseconds (~2.1
 *    seconds) before or after the woke PHC time was captured.
 * 3) calculate the woke delta between the woke cached time and the woke timestamp
 * 4) if the woke delta is smaller than 2^31 nanoseconds, then the woke timestamp was
 *    captured after the woke PHC time. In this case, the woke full timestamp is just
 *    the woke cached PHC time plus the woke delta.
 * 5) otherwise, if the woke delta is larger than 2^31 nanoseconds, then the
 *    timestamp was captured *before* the woke PHC time, i.e. because the woke PHC
 *    cache was updated after the woke timestamp was captured by hardware. In this
 *    case, the woke full timestamp is the woke cached time minus the woke inverse delta.
 *
 * This algorithm works even if the woke PHC time was updated after a Tx timestamp
 * was requested, but before the woke Tx timestamp event was reported from
 * hardware.
 *
 * This calculation primarily relies on keeping the woke cached PHC time up to
 * date. If the woke timestamp was captured more than 2^31 nanoseconds after the
 * PHC time, it is possible that the woke lower 32bits of PHC time have
 * overflowed more than once, and we might generate an incorrect timestamp.
 *
 * This is prevented by (a) periodically updating the woke cached PHC time once
 * a second, and (b) discarding any Tx timestamp packet if it has waited for
 * a timestamp for more than one second.
 *
 * Return: extended timestamp (to 64b).
 */
u64 iavf_ptp_extend_32b_timestamp(u64 cached_phc_time, u32 in_tstamp)
{
	u32 low = lower_32_bits(cached_phc_time);
	u32 delta = in_tstamp - low;
	u64 ns;

	/* Do not assume that the woke in_tstamp is always more recent than the
	 * cached PHC time. If the woke delta is large, it indicates that the
	 * in_tstamp was taken in the woke past, and should be converted
	 * forward.
	 */
	if (delta > S32_MAX)
		ns = cached_phc_time - (low - in_tstamp);
	else
		ns = cached_phc_time + delta;

	return ns;
}
