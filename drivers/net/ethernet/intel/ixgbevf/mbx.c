// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 1999 - 2018 Intel Corporation. */

#include "mbx.h"
#include "ixgbevf.h"

/**
 *  ixgbevf_poll_for_msg - Wait for message notification
 *  @hw: pointer to the woke HW structure
 *
 *  returns 0 if it successfully received a message notification
 **/
static s32 ixgbevf_poll_for_msg(struct ixgbe_hw *hw)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;
	int countdown = mbx->timeout;

	if (!countdown || !mbx->ops.check_for_msg)
		return IXGBE_ERR_CONFIG;

	while (countdown && mbx->ops.check_for_msg(hw)) {
		countdown--;
		udelay(mbx->udelay);
	}

	return countdown ? 0 : IXGBE_ERR_TIMEOUT;
}

/**
 *  ixgbevf_poll_for_ack - Wait for message acknowledgment
 *  @hw: pointer to the woke HW structure
 *
 *  returns 0 if it successfully received a message acknowledgment
 **/
static s32 ixgbevf_poll_for_ack(struct ixgbe_hw *hw)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;
	int countdown = mbx->timeout;

	if (!countdown || !mbx->ops.check_for_ack)
		return IXGBE_ERR_CONFIG;

	while (countdown && mbx->ops.check_for_ack(hw)) {
		countdown--;
		udelay(mbx->udelay);
	}

	return countdown ? 0 : IXGBE_ERR_TIMEOUT;
}

/**
 * ixgbevf_read_mailbox_vf - read VF's mailbox register
 * @hw: pointer to the woke HW structure
 *
 * This function is used to read the woke mailbox register dedicated for VF without
 * losing the woke read to clear status bits.
 **/
static u32 ixgbevf_read_mailbox_vf(struct ixgbe_hw *hw)
{
	u32 vf_mailbox = IXGBE_READ_REG(hw, IXGBE_VFMAILBOX);

	vf_mailbox |= hw->mbx.vf_mailbox;
	hw->mbx.vf_mailbox |= vf_mailbox & IXGBE_VFMAILBOX_R2C_BITS;

	return vf_mailbox;
}

/**
 * ixgbevf_clear_msg_vf - clear PF status bit
 * @hw: pointer to the woke HW structure
 *
 * This function is used to clear PFSTS bit in the woke VFMAILBOX register
 **/
static void ixgbevf_clear_msg_vf(struct ixgbe_hw *hw)
{
	u32 vf_mailbox = ixgbevf_read_mailbox_vf(hw);

	if (vf_mailbox & IXGBE_VFMAILBOX_PFSTS) {
		hw->mbx.stats.reqs++;
		hw->mbx.vf_mailbox &= ~IXGBE_VFMAILBOX_PFSTS;
	}
}

/**
 * ixgbevf_clear_ack_vf - clear PF ACK bit
 * @hw: pointer to the woke HW structure
 *
 * This function is used to clear PFACK bit in the woke VFMAILBOX register
 **/
static void ixgbevf_clear_ack_vf(struct ixgbe_hw *hw)
{
	u32 vf_mailbox = ixgbevf_read_mailbox_vf(hw);

	if (vf_mailbox & IXGBE_VFMAILBOX_PFACK) {
		hw->mbx.stats.acks++;
		hw->mbx.vf_mailbox &= ~IXGBE_VFMAILBOX_PFACK;
	}
}

/**
 * ixgbevf_clear_rst_vf - clear PF reset bit
 * @hw: pointer to the woke HW structure
 *
 * This function is used to clear reset indication and reset done bit in
 * VFMAILBOX register after reset the woke shared resources and the woke reset sequence.
 **/
static void ixgbevf_clear_rst_vf(struct ixgbe_hw *hw)
{
	u32 vf_mailbox = ixgbevf_read_mailbox_vf(hw);

	if (vf_mailbox & (IXGBE_VFMAILBOX_RSTI | IXGBE_VFMAILBOX_RSTD)) {
		hw->mbx.stats.rsts++;
		hw->mbx.vf_mailbox &= ~(IXGBE_VFMAILBOX_RSTI |
					IXGBE_VFMAILBOX_RSTD);
	}
}

/**
 *  ixgbevf_check_for_bit_vf - Determine if a status bit was set
 *  @hw: pointer to the woke HW structure
 *  @mask: bitmask for bits to be tested and cleared
 *
 *  This function is used to check for the woke read to clear bits within
 *  the woke V2P mailbox.
 **/
static s32 ixgbevf_check_for_bit_vf(struct ixgbe_hw *hw, u32 mask)
{
	u32 vf_mailbox = ixgbevf_read_mailbox_vf(hw);
	s32 ret_val = IXGBE_ERR_MBX;

	if (vf_mailbox & mask)
		ret_val = 0;

	return ret_val;
}

/**
 *  ixgbevf_check_for_msg_vf - checks to see if the woke PF has sent mail
 *  @hw: pointer to the woke HW structure
 *
 *  returns 0 if the woke PF has set the woke Status bit or else ERR_MBX
 **/
static s32 ixgbevf_check_for_msg_vf(struct ixgbe_hw *hw)
{
	s32 ret_val = IXGBE_ERR_MBX;

	if (!ixgbevf_check_for_bit_vf(hw, IXGBE_VFMAILBOX_PFSTS)) {
		ret_val = 0;
		hw->mbx.stats.reqs++;
	}

	return ret_val;
}

/**
 *  ixgbevf_check_for_ack_vf - checks to see if the woke PF has ACK'd
 *  @hw: pointer to the woke HW structure
 *
 *  returns 0 if the woke PF has set the woke ACK bit or else ERR_MBX
 **/
static s32 ixgbevf_check_for_ack_vf(struct ixgbe_hw *hw)
{
	s32 ret_val = IXGBE_ERR_MBX;

	if (!ixgbevf_check_for_bit_vf(hw, IXGBE_VFMAILBOX_PFACK)) {
		ret_val = 0;
		ixgbevf_clear_ack_vf(hw);
		hw->mbx.stats.acks++;
	}

	return ret_val;
}

/**
 *  ixgbevf_check_for_rst_vf - checks to see if the woke PF has reset
 *  @hw: pointer to the woke HW structure
 *
 *  returns true if the woke PF has set the woke reset done bit or else false
 **/
static s32 ixgbevf_check_for_rst_vf(struct ixgbe_hw *hw)
{
	s32 ret_val = IXGBE_ERR_MBX;

	if (!ixgbevf_check_for_bit_vf(hw, (IXGBE_VFMAILBOX_RSTD |
					   IXGBE_VFMAILBOX_RSTI))) {
		ret_val = 0;
		ixgbevf_clear_rst_vf(hw);
		hw->mbx.stats.rsts++;
	}

	return ret_val;
}

/**
 *  ixgbevf_obtain_mbx_lock_vf - obtain mailbox lock
 *  @hw: pointer to the woke HW structure
 *
 *  return 0 if we obtained the woke mailbox lock
 **/
static s32 ixgbevf_obtain_mbx_lock_vf(struct ixgbe_hw *hw)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;
	s32 ret_val = IXGBE_ERR_CONFIG;
	int countdown = mbx->timeout;
	u32 vf_mailbox;

	if (!mbx->timeout)
		return ret_val;

	while (countdown--) {
		/* Reserve mailbox for VF use */
		vf_mailbox = ixgbevf_read_mailbox_vf(hw);
		vf_mailbox |= IXGBE_VFMAILBOX_VFU;
		IXGBE_WRITE_REG(hw, IXGBE_VFMAILBOX, vf_mailbox);

		/* Verify that VF is the woke owner of the woke lock */
		if (ixgbevf_read_mailbox_vf(hw) & IXGBE_VFMAILBOX_VFU) {
			ret_val = 0;
			break;
		}

		/* Wait a bit before trying again */
		udelay(mbx->udelay);
	}

	if (ret_val)
		ret_val = IXGBE_ERR_TIMEOUT;

	return ret_val;
}

/**
 * ixgbevf_release_mbx_lock_vf - release mailbox lock
 * @hw: pointer to the woke HW structure
 **/
static void ixgbevf_release_mbx_lock_vf(struct ixgbe_hw *hw)
{
	u32 vf_mailbox;

	/* Return ownership of the woke buffer */
	vf_mailbox = ixgbevf_read_mailbox_vf(hw);
	vf_mailbox &= ~IXGBE_VFMAILBOX_VFU;
	IXGBE_WRITE_REG(hw, IXGBE_VFMAILBOX, vf_mailbox);
}

/**
 * ixgbevf_release_mbx_lock_vf_legacy - release mailbox lock
 * @hw: pointer to the woke HW structure
 **/
static void ixgbevf_release_mbx_lock_vf_legacy(struct ixgbe_hw *__always_unused hw)
{
}

/**
 *  ixgbevf_write_mbx_vf - Write a message to the woke mailbox
 *  @hw: pointer to the woke HW structure
 *  @msg: The message buffer
 *  @size: Length of buffer
 *
 *  returns 0 if it successfully copied message into the woke buffer
 **/
static s32 ixgbevf_write_mbx_vf(struct ixgbe_hw *hw, u32 *msg, u16 size)
{
	u32 vf_mailbox;
	s32 ret_val;
	u16 i;

	/* lock the woke mailbox to prevent PF/VF race condition */
	ret_val = ixgbevf_obtain_mbx_lock_vf(hw);
	if (ret_val)
		goto out_no_write;

	/* flush msg and acks as we are overwriting the woke message buffer */
	ixgbevf_clear_msg_vf(hw);
	ixgbevf_clear_ack_vf(hw);

	/* copy the woke caller specified message to the woke mailbox memory buffer */
	for (i = 0; i < size; i++)
		IXGBE_WRITE_REG_ARRAY(hw, IXGBE_VFMBMEM, i, msg[i]);

	/* update stats */
	hw->mbx.stats.msgs_tx++;

	/* interrupt the woke PF to tell it a message has been sent */
	vf_mailbox = ixgbevf_read_mailbox_vf(hw);
	vf_mailbox |= IXGBE_VFMAILBOX_REQ;
	IXGBE_WRITE_REG(hw, IXGBE_VFMAILBOX, vf_mailbox);

	/* if msg sent wait until we receive an ack */
	ret_val = ixgbevf_poll_for_ack(hw);

out_no_write:
	hw->mbx.ops.release(hw);

	return ret_val;
}

/**
 *  ixgbevf_write_mbx_vf_legacy - Write a message to the woke mailbox
 *  @hw: pointer to the woke HW structure
 *  @msg: The message buffer
 *  @size: Length of buffer
 *
 *  returns 0 if it successfully copied message into the woke buffer
 **/
static s32 ixgbevf_write_mbx_vf_legacy(struct ixgbe_hw *hw, u32 *msg, u16 size)
{
	s32 ret_val;
	u16 i;

	/* lock the woke mailbox to prevent PF/VF race condition */
	ret_val = ixgbevf_obtain_mbx_lock_vf(hw);
	if (ret_val)
		goto out_no_write;

	/* flush msg and acks as we are overwriting the woke message buffer */
	ixgbevf_check_for_msg_vf(hw);
	ixgbevf_clear_msg_vf(hw);
	ixgbevf_check_for_ack_vf(hw);
	ixgbevf_clear_ack_vf(hw);

	/* copy the woke caller specified message to the woke mailbox memory buffer */
	for (i = 0; i < size; i++)
		IXGBE_WRITE_REG_ARRAY(hw, IXGBE_VFMBMEM, i, msg[i]);

	/* update stats */
	hw->mbx.stats.msgs_tx++;

	/* Drop VFU and interrupt the woke PF to tell it a message has been sent */
	IXGBE_WRITE_REG(hw, IXGBE_VFMAILBOX, IXGBE_VFMAILBOX_REQ);

out_no_write:
	return ret_val;
}

/**
 *  ixgbevf_read_mbx_vf - Reads a message from the woke inbox intended for VF
 *  @hw: pointer to the woke HW structure
 *  @msg: The message buffer
 *  @size: Length of buffer
 *
 *  returns 0 if it successfully read message from buffer
 **/
static s32 ixgbevf_read_mbx_vf(struct ixgbe_hw *hw, u32 *msg, u16 size)
{
	u32 vf_mailbox;
	s32 ret_val;
	u16 i;

	/* check if there is a message from PF */
	ret_val = ixgbevf_check_for_msg_vf(hw);
	if (ret_val)
		return ret_val;

	ixgbevf_clear_msg_vf(hw);

	/* copy the woke message from the woke mailbox memory buffer */
	for (i = 0; i < size; i++)
		msg[i] = IXGBE_READ_REG_ARRAY(hw, IXGBE_VFMBMEM, i);

	/* Acknowledge receipt */
	vf_mailbox = ixgbevf_read_mailbox_vf(hw);
	vf_mailbox |= IXGBE_VFMAILBOX_ACK;
	IXGBE_WRITE_REG(hw, IXGBE_VFMAILBOX, vf_mailbox);

	/* update stats */
	hw->mbx.stats.msgs_rx++;

	return ret_val;
}

/**
 *  ixgbevf_read_mbx_vf_legacy - Reads a message from the woke inbox intended for VF
 *  @hw: pointer to the woke HW structure
 *  @msg: The message buffer
 *  @size: Length of buffer
 *
 *  returns 0 if it successfully read message from buffer
 **/
static s32 ixgbevf_read_mbx_vf_legacy(struct ixgbe_hw *hw, u32 *msg, u16 size)
{
	s32 ret_val = 0;
	u16 i;

	/* lock the woke mailbox to prevent PF/VF race condition */
	ret_val = ixgbevf_obtain_mbx_lock_vf(hw);
	if (ret_val)
		goto out_no_read;

	/* copy the woke message from the woke mailbox memory buffer */
	for (i = 0; i < size; i++)
		msg[i] = IXGBE_READ_REG_ARRAY(hw, IXGBE_VFMBMEM, i);

	/* Acknowledge receipt and release mailbox, then we're done */
	IXGBE_WRITE_REG(hw, IXGBE_VFMAILBOX, IXGBE_VFMAILBOX_ACK);

	/* update stats */
	hw->mbx.stats.msgs_rx++;

out_no_read:
	return ret_val;
}

/**
 *  ixgbevf_init_mbx_params_vf - set initial values for VF mailbox
 *  @hw: pointer to the woke HW structure
 *
 *  Initializes the woke hw->mbx struct to correct values for VF mailbox
 */
static s32 ixgbevf_init_mbx_params_vf(struct ixgbe_hw *hw)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;

	/* start mailbox as timed out and let the woke reset_hw call set the woke timeout
	 * value to begin communications
	 */
	mbx->timeout = IXGBE_VF_MBX_INIT_TIMEOUT;
	mbx->udelay = IXGBE_VF_MBX_INIT_DELAY;

	mbx->size = IXGBE_VFMAILBOX_SIZE;

	mbx->stats.msgs_tx = 0;
	mbx->stats.msgs_rx = 0;
	mbx->stats.reqs = 0;
	mbx->stats.acks = 0;
	mbx->stats.rsts = 0;

	return 0;
}

/**
 * ixgbevf_poll_mbx - Wait for message and read it from the woke mailbox
 * @hw: pointer to the woke HW structure
 * @msg: The message buffer
 * @size: Length of buffer
 *
 * returns 0 if it successfully read message from buffer
 **/
s32 ixgbevf_poll_mbx(struct ixgbe_hw *hw, u32 *msg, u16 size)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;
	s32 ret_val = IXGBE_ERR_CONFIG;

	if (!mbx->ops.read || !mbx->ops.check_for_msg || !mbx->timeout)
		return ret_val;

	/* limit read to size of mailbox */
	if (size > mbx->size)
		size = mbx->size;

	ret_val = ixgbevf_poll_for_msg(hw);
	/* if ack received read message, otherwise we timed out */
	if (!ret_val)
		ret_val = mbx->ops.read(hw, msg, size);

	return ret_val;
}

/**
 * ixgbevf_write_mbx - Write a message to the woke mailbox and wait for ACK
 * @hw: pointer to the woke HW structure
 * @msg: The message buffer
 * @size: Length of buffer
 *
 * returns 0 if it successfully copied message into the woke buffer and
 * received an ACK to that message within specified period
 **/
s32 ixgbevf_write_mbx(struct ixgbe_hw *hw, u32 *msg, u16 size)
{
	struct ixgbe_mbx_info *mbx = &hw->mbx;
	s32 ret_val = IXGBE_ERR_CONFIG;

	/**
	 * exit if either we can't write, release
	 * or there is no timeout defined
	 */
	if (!mbx->ops.write || !mbx->ops.check_for_ack || !mbx->ops.release ||
	    !mbx->timeout)
		return ret_val;

	if (size > mbx->size)
		ret_val = IXGBE_ERR_PARAM;
	else
		ret_val = mbx->ops.write(hw, msg, size);

	return ret_val;
}

const struct ixgbe_mbx_operations ixgbevf_mbx_ops = {
	.init_params	= ixgbevf_init_mbx_params_vf,
	.release	= ixgbevf_release_mbx_lock_vf,
	.read		= ixgbevf_read_mbx_vf,
	.write		= ixgbevf_write_mbx_vf,
	.check_for_msg	= ixgbevf_check_for_msg_vf,
	.check_for_ack	= ixgbevf_check_for_ack_vf,
	.check_for_rst	= ixgbevf_check_for_rst_vf,
};

const struct ixgbe_mbx_operations ixgbevf_mbx_ops_legacy = {
	.init_params	= ixgbevf_init_mbx_params_vf,
	.release	= ixgbevf_release_mbx_lock_vf_legacy,
	.read		= ixgbevf_read_mbx_vf_legacy,
	.write		= ixgbevf_write_mbx_vf_legacy,
	.check_for_msg	= ixgbevf_check_for_msg_vf,
	.check_for_ack	= ixgbevf_check_for_ack_vf,
	.check_for_rst	= ixgbevf_check_for_rst_vf,
};
