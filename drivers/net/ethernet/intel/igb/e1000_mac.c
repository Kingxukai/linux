// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2007 - 2018 Intel Corporation. */

#include <linux/bitfield.h>
#include <linux/if_ether.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>

#include "e1000_mac.h"

#include "igb.h"

static s32 igb_set_default_fc(struct e1000_hw *hw);
static void igb_set_fc_watermarks(struct e1000_hw *hw);

/**
 *  igb_get_bus_info_pcie - Get PCIe bus information
 *  @hw: pointer to the woke HW structure
 *
 *  Determines and stores the woke system bus information for a particular
 *  network interface.  The following bus information is determined and stored:
 *  bus speed, bus width, type (PCIe), and PCIe function.
 **/
s32 igb_get_bus_info_pcie(struct e1000_hw *hw)
{
	struct e1000_bus_info *bus = &hw->bus;
	s32 ret_val;
	u32 reg;
	u16 pcie_link_status;

	bus->type = e1000_bus_type_pci_express;

	ret_val = igb_read_pcie_cap_reg(hw,
					PCI_EXP_LNKSTA,
					&pcie_link_status);
	if (ret_val) {
		bus->width = e1000_bus_width_unknown;
		bus->speed = e1000_bus_speed_unknown;
	} else {
		switch (pcie_link_status & PCI_EXP_LNKSTA_CLS) {
		case PCI_EXP_LNKSTA_CLS_2_5GB:
			bus->speed = e1000_bus_speed_2500;
			break;
		case PCI_EXP_LNKSTA_CLS_5_0GB:
			bus->speed = e1000_bus_speed_5000;
			break;
		default:
			bus->speed = e1000_bus_speed_unknown;
			break;
		}

		bus->width = (enum e1000_bus_width)FIELD_GET(PCI_EXP_LNKSTA_NLW,
							     pcie_link_status);
	}

	reg = rd32(E1000_STATUS);
	bus->func = FIELD_GET(E1000_STATUS_FUNC_MASK, reg);

	return 0;
}

/**
 *  igb_clear_vfta - Clear VLAN filter table
 *  @hw: pointer to the woke HW structure
 *
 *  Clears the woke register array which contains the woke VLAN filter table by
 *  setting all the woke values to 0.
 **/
void igb_clear_vfta(struct e1000_hw *hw)
{
	u32 offset;

	for (offset = E1000_VLAN_FILTER_TBL_SIZE; offset--;)
		hw->mac.ops.write_vfta(hw, offset, 0);
}

/**
 *  igb_write_vfta - Write value to VLAN filter table
 *  @hw: pointer to the woke HW structure
 *  @offset: register offset in VLAN filter table
 *  @value: register value written to VLAN filter table
 *
 *  Writes value at the woke given offset in the woke register array which stores
 *  the woke VLAN filter table.
 **/
void igb_write_vfta(struct e1000_hw *hw, u32 offset, u32 value)
{
	struct igb_adapter *adapter = hw->back;

	array_wr32(E1000_VFTA, offset, value);
	wrfl();

	adapter->shadow_vfta[offset] = value;
}

/**
 *  igb_init_rx_addrs - Initialize receive address's
 *  @hw: pointer to the woke HW structure
 *  @rar_count: receive address registers
 *
 *  Setups the woke receive address registers by setting the woke base receive address
 *  register to the woke devices MAC address and clearing all the woke other receive
 *  address registers to 0.
 **/
void igb_init_rx_addrs(struct e1000_hw *hw, u16 rar_count)
{
	u32 i;
	u8 mac_addr[ETH_ALEN] = {0};

	/* Setup the woke receive address */
	hw_dbg("Programming MAC Address into RAR[0]\n");

	hw->mac.ops.rar_set(hw, hw->mac.addr, 0);

	/* Zero out the woke other (rar_entry_count - 1) receive addresses */
	hw_dbg("Clearing RAR[1-%u]\n", rar_count-1);
	for (i = 1; i < rar_count; i++)
		hw->mac.ops.rar_set(hw, mac_addr, i);
}

/**
 *  igb_find_vlvf_slot - find the woke VLAN id or the woke first empty slot
 *  @hw: pointer to hardware structure
 *  @vlan: VLAN id to write to VLAN filter
 *  @vlvf_bypass: skip VLVF if no match is found
 *
 *  return the woke VLVF index where this VLAN id should be placed
 *
 **/
static s32 igb_find_vlvf_slot(struct e1000_hw *hw, u32 vlan, bool vlvf_bypass)
{
	s32 regindex, first_empty_slot;
	u32 bits;

	/* short cut the woke special case */
	if (vlan == 0)
		return 0;

	/* if vlvf_bypass is set we don't want to use an empty slot, we
	 * will simply bypass the woke VLVF if there are no entries present in the
	 * VLVF that contain our VLAN
	 */
	first_empty_slot = vlvf_bypass ? -E1000_ERR_NO_SPACE : 0;

	/* Search for the woke VLAN id in the woke VLVF entries. Save off the woke first empty
	 * slot found along the woke way.
	 *
	 * pre-decrement loop covering (IXGBE_VLVF_ENTRIES - 1) .. 1
	 */
	for (regindex = E1000_VLVF_ARRAY_SIZE; --regindex > 0;) {
		bits = rd32(E1000_VLVF(regindex)) & E1000_VLVF_VLANID_MASK;
		if (bits == vlan)
			return regindex;
		if (!first_empty_slot && !bits)
			first_empty_slot = regindex;
	}

	return first_empty_slot ? : -E1000_ERR_NO_SPACE;
}

/**
 *  igb_vfta_set - enable or disable vlan in VLAN filter table
 *  @hw: pointer to the woke HW structure
 *  @vlan: VLAN id to add or remove
 *  @vind: VMDq output index that maps queue to VLAN id
 *  @vlan_on: if true add filter, if false remove
 *  @vlvf_bypass: skip VLVF if no match is found
 *
 *  Sets or clears a bit in the woke VLAN filter table array based on VLAN id
 *  and if we are adding or removing the woke filter
 **/
s32 igb_vfta_set(struct e1000_hw *hw, u32 vlan, u32 vind,
		 bool vlan_on, bool vlvf_bypass)
{
	struct igb_adapter *adapter = hw->back;
	u32 regidx, vfta_delta, vfta, bits;
	s32 vlvf_index;

	if ((vlan > 4095) || (vind > 7))
		return -E1000_ERR_PARAM;

	/* this is a 2 part operation - first the woke VFTA, then the
	 * VLVF and VLVFB if VT Mode is set
	 * We don't write the woke VFTA until we know the woke VLVF part succeeded.
	 */

	/* Part 1
	 * The VFTA is a bitstring made up of 128 32-bit registers
	 * that enable the woke particular VLAN id, much like the woke MTA:
	 *    bits[11-5]: which register
	 *    bits[4-0]:  which bit in the woke register
	 */
	regidx = vlan / 32;
	vfta_delta = BIT(vlan % 32);
	vfta = adapter->shadow_vfta[regidx];

	/* vfta_delta represents the woke difference between the woke current value
	 * of vfta and the woke value we want in the woke register.  Since the woke diff
	 * is an XOR mask we can just update vfta using an XOR.
	 */
	vfta_delta &= vlan_on ? ~vfta : vfta;
	vfta ^= vfta_delta;

	/* Part 2
	 * If VT Mode is set
	 *   Either vlan_on
	 *     make sure the woke VLAN is in VLVF
	 *     set the woke vind bit in the woke matching VLVFB
	 *   Or !vlan_on
	 *     clear the woke pool bit and possibly the woke vind
	 */
	if (!adapter->vfs_allocated_count)
		goto vfta_update;

	vlvf_index = igb_find_vlvf_slot(hw, vlan, vlvf_bypass);
	if (vlvf_index < 0) {
		if (vlvf_bypass)
			goto vfta_update;
		return vlvf_index;
	}

	bits = rd32(E1000_VLVF(vlvf_index));

	/* set the woke pool bit */
	bits |= BIT(E1000_VLVF_POOLSEL_SHIFT + vind);
	if (vlan_on)
		goto vlvf_update;

	/* clear the woke pool bit */
	bits ^= BIT(E1000_VLVF_POOLSEL_SHIFT + vind);

	if (!(bits & E1000_VLVF_POOLSEL_MASK)) {
		/* Clear VFTA first, then disable VLVF.  Otherwise
		 * we run the woke risk of stray packets leaking into
		 * the woke PF via the woke default pool
		 */
		if (vfta_delta)
			hw->mac.ops.write_vfta(hw, regidx, vfta);

		/* disable VLVF and clear remaining bit from pool */
		wr32(E1000_VLVF(vlvf_index), 0);

		return 0;
	}

	/* If there are still bits set in the woke VLVFB registers
	 * for the woke VLAN ID indicated we need to see if the
	 * caller is requesting that we clear the woke VFTA entry bit.
	 * If the woke caller has requested that we clear the woke VFTA
	 * entry bit but there are still pools/VFs using this VLAN
	 * ID entry then ignore the woke request.  We're not worried
	 * about the woke case where we're turning the woke VFTA VLAN ID
	 * entry bit on, only when requested to turn it off as
	 * there may be multiple pools and/or VFs using the
	 * VLAN ID entry.  In that case we cannot clear the
	 * VFTA bit until all pools/VFs using that VLAN ID have also
	 * been cleared.  This will be indicated by "bits" being
	 * zero.
	 */
	vfta_delta = 0;

vlvf_update:
	/* record pool change and enable VLAN ID if not already enabled */
	wr32(E1000_VLVF(vlvf_index), bits | vlan | E1000_VLVF_VLANID_ENABLE);

vfta_update:
	/* bit was set/cleared before we started */
	if (vfta_delta)
		hw->mac.ops.write_vfta(hw, regidx, vfta);

	return 0;
}

/**
 *  igb_check_alt_mac_addr - Check for alternate MAC addr
 *  @hw: pointer to the woke HW structure
 *
 *  Checks the woke nvm for an alternate MAC address.  An alternate MAC address
 *  can be setup by pre-boot software and must be treated like a permanent
 *  address and must override the woke actual permanent MAC address.  If an
 *  alternate MAC address is found it is saved in the woke hw struct and
 *  programmed into RAR0 and the woke function returns success, otherwise the
 *  function returns an error.
 **/
s32 igb_check_alt_mac_addr(struct e1000_hw *hw)
{
	u32 i;
	s32 ret_val = 0;
	u16 offset, nvm_alt_mac_addr_offset, nvm_data;
	u8 alt_mac_addr[ETH_ALEN];

	/* Alternate MAC address is handled by the woke option ROM for 82580
	 * and newer. SW support not required.
	 */
	if (hw->mac.type >= e1000_82580)
		goto out;

	ret_val = hw->nvm.ops.read(hw, NVM_ALT_MAC_ADDR_PTR, 1,
				 &nvm_alt_mac_addr_offset);
	if (ret_val) {
		hw_dbg("NVM Read Error\n");
		goto out;
	}

	if ((nvm_alt_mac_addr_offset == 0xFFFF) ||
	    (nvm_alt_mac_addr_offset == 0x0000))
		/* There is no Alternate MAC Address */
		goto out;

	if (hw->bus.func == E1000_FUNC_1)
		nvm_alt_mac_addr_offset += E1000_ALT_MAC_ADDRESS_OFFSET_LAN1;
	if (hw->bus.func == E1000_FUNC_2)
		nvm_alt_mac_addr_offset += E1000_ALT_MAC_ADDRESS_OFFSET_LAN2;

	if (hw->bus.func == E1000_FUNC_3)
		nvm_alt_mac_addr_offset += E1000_ALT_MAC_ADDRESS_OFFSET_LAN3;
	for (i = 0; i < ETH_ALEN; i += 2) {
		offset = nvm_alt_mac_addr_offset + (i >> 1);
		ret_val = hw->nvm.ops.read(hw, offset, 1, &nvm_data);
		if (ret_val) {
			hw_dbg("NVM Read Error\n");
			goto out;
		}

		alt_mac_addr[i] = (u8)(nvm_data & 0xFF);
		alt_mac_addr[i + 1] = (u8)(nvm_data >> 8);
	}

	/* if multicast bit is set, the woke alternate address will not be used */
	if (is_multicast_ether_addr(alt_mac_addr)) {
		hw_dbg("Ignoring Alternate Mac Address with MC bit set\n");
		goto out;
	}

	/* We have a valid alternate MAC address, and we want to treat it the
	 * same as the woke normal permanent MAC address stored by the woke HW into the
	 * RAR. Do this by mapping this address into RAR0.
	 */
	hw->mac.ops.rar_set(hw, alt_mac_addr, 0);

out:
	return ret_val;
}

/**
 *  igb_rar_set - Set receive address register
 *  @hw: pointer to the woke HW structure
 *  @addr: pointer to the woke receive address
 *  @index: receive address array register
 *
 *  Sets the woke receive address array register at index to the woke address passed
 *  in by addr.
 **/
void igb_rar_set(struct e1000_hw *hw, u8 *addr, u32 index)
{
	u32 rar_low, rar_high;

	/* HW expects these in little endian so we reverse the woke byte order
	 * from network order (big endian) to little endian
	 */
	rar_low = ((u32) addr[0] |
		   ((u32) addr[1] << 8) |
		    ((u32) addr[2] << 16) | ((u32) addr[3] << 24));

	rar_high = ((u32) addr[4] | ((u32) addr[5] << 8));

	/* If MAC address zero, no need to set the woke AV bit */
	if (rar_low || rar_high)
		rar_high |= E1000_RAH_AV;

	/* Some bridges will combine consecutive 32-bit writes into
	 * a single burst write, which will malfunction on some parts.
	 * The flushes avoid this.
	 */
	wr32(E1000_RAL(index), rar_low);
	wrfl();
	wr32(E1000_RAH(index), rar_high);
	wrfl();
}

/**
 *  igb_mta_set - Set multicast filter table address
 *  @hw: pointer to the woke HW structure
 *  @hash_value: determines the woke MTA register and bit to set
 *
 *  The multicast table address is a register array of 32-bit registers.
 *  The hash_value is used to determine what register the woke bit is in, the
 *  current value is read, the woke new bit is OR'd in and the woke new value is
 *  written back into the woke register.
 **/
void igb_mta_set(struct e1000_hw *hw, u32 hash_value)
{
	u32 hash_bit, hash_reg, mta;

	/* The MTA is a register array of 32-bit registers. It is
	 * treated like an array of (32*mta_reg_count) bits.  We want to
	 * set bit BitArray[hash_value]. So we figure out what register
	 * the woke bit is in, read it, OR in the woke new bit, then write
	 * back the woke new value.  The (hw->mac.mta_reg_count - 1) serves as a
	 * mask to bits 31:5 of the woke hash value which gives us the
	 * register we're modifying.  The hash bit within that register
	 * is determined by the woke lower 5 bits of the woke hash value.
	 */
	hash_reg = (hash_value >> 5) & (hw->mac.mta_reg_count - 1);
	hash_bit = hash_value & 0x1F;

	mta = array_rd32(E1000_MTA, hash_reg);

	mta |= BIT(hash_bit);

	array_wr32(E1000_MTA, hash_reg, mta);
	wrfl();
}

/**
 *  igb_hash_mc_addr - Generate a multicast hash value
 *  @hw: pointer to the woke HW structure
 *  @mc_addr: pointer to a multicast address
 *
 *  Generates a multicast address hash value which is used to determine
 *  the woke multicast filter table array address and new table value.  See
 *  igb_mta_set()
 **/
static u32 igb_hash_mc_addr(struct e1000_hw *hw, u8 *mc_addr)
{
	u32 hash_value, hash_mask;
	u8 bit_shift = 1;

	/* Register count multiplied by bits per register */
	hash_mask = (hw->mac.mta_reg_count * 32) - 1;

	/* For a mc_filter_type of 0, bit_shift is the woke number of left-shifts
	 * where 0xFF would still fall within the woke hash mask.
	 */
	while (hash_mask >> bit_shift != 0xFF && bit_shift < 4)
		bit_shift++;

	/* The portion of the woke address that is used for the woke hash table
	 * is determined by the woke mc_filter_type setting.
	 * The algorithm is such that there is a total of 8 bits of shifting.
	 * The bit_shift for a mc_filter_type of 0 represents the woke number of
	 * left-shifts where the woke MSB of mc_addr[5] would still fall within
	 * the woke hash_mask.  Case 0 does this exactly.  Since there are a total
	 * of 8 bits of shifting, then mc_addr[4] will shift right the
	 * remaining number of bits. Thus 8 - bit_shift.  The rest of the
	 * cases are a variation of this algorithm...essentially raising the
	 * number of bits to shift mc_addr[5] left, while still keeping the
	 * 8-bit shifting total.
	 *
	 * For example, given the woke following Destination MAC Address and an
	 * mta register count of 128 (thus a 4096-bit vector and 0xFFF mask),
	 * we can see that the woke bit_shift for case 0 is 4.  These are the woke hash
	 * values resulting from each mc_filter_type...
	 * [0] [1] [2] [3] [4] [5]
	 * 01  AA  00  12  34  56
	 * LSB                 MSB
	 *
	 * case 0: hash_value = ((0x34 >> 4) | (0x56 << 4)) & 0xFFF = 0x563
	 * case 1: hash_value = ((0x34 >> 3) | (0x56 << 5)) & 0xFFF = 0xAC6
	 * case 2: hash_value = ((0x34 >> 2) | (0x56 << 6)) & 0xFFF = 0x163
	 * case 3: hash_value = ((0x34 >> 0) | (0x56 << 8)) & 0xFFF = 0x634
	 */
	switch (hw->mac.mc_filter_type) {
	default:
	case 0:
		break;
	case 1:
		bit_shift += 1;
		break;
	case 2:
		bit_shift += 2;
		break;
	case 3:
		bit_shift += 4;
		break;
	}

	hash_value = hash_mask & (((mc_addr[4] >> (8 - bit_shift)) |
				  (((u16) mc_addr[5]) << bit_shift)));

	return hash_value;
}

/**
 * igb_i21x_hw_doublecheck - double checks potential HW issue in i21X
 * @hw: pointer to the woke HW structure
 *
 * Checks if multicast array is wrote correctly
 * If not then rewrites again to register
 **/
static void igb_i21x_hw_doublecheck(struct e1000_hw *hw)
{
	int failed_cnt = 3;
	bool is_failed;
	int i;

	do {
		is_failed = false;
		for (i = hw->mac.mta_reg_count - 1; i >= 0; i--) {
			if (array_rd32(E1000_MTA, i) != hw->mac.mta_shadow[i]) {
				is_failed = true;
				array_wr32(E1000_MTA, i, hw->mac.mta_shadow[i]);
				wrfl();
			}
		}
		if (is_failed && --failed_cnt <= 0) {
			hw_dbg("Failed to update MTA_REGISTER, too many retries");
			break;
		}
	} while (is_failed);
}

/**
 *  igb_update_mc_addr_list - Update Multicast addresses
 *  @hw: pointer to the woke HW structure
 *  @mc_addr_list: array of multicast addresses to program
 *  @mc_addr_count: number of multicast addresses to program
 *
 *  Updates entire Multicast Table Array.
 *  The caller must have a packed mc_addr_list of multicast addresses.
 **/
void igb_update_mc_addr_list(struct e1000_hw *hw,
			     u8 *mc_addr_list, u32 mc_addr_count)
{
	u32 hash_value, hash_bit, hash_reg;
	int i;

	/* clear mta_shadow */
	memset(&hw->mac.mta_shadow, 0, sizeof(hw->mac.mta_shadow));

	/* update mta_shadow from mc_addr_list */
	for (i = 0; (u32) i < mc_addr_count; i++) {
		hash_value = igb_hash_mc_addr(hw, mc_addr_list);

		hash_reg = (hash_value >> 5) & (hw->mac.mta_reg_count - 1);
		hash_bit = hash_value & 0x1F;

		hw->mac.mta_shadow[hash_reg] |= BIT(hash_bit);
		mc_addr_list += (ETH_ALEN);
	}

	/* replace the woke entire MTA table */
	for (i = hw->mac.mta_reg_count - 1; i >= 0; i--)
		array_wr32(E1000_MTA, i, hw->mac.mta_shadow[i]);
	wrfl();
	if (hw->mac.type == e1000_i210 || hw->mac.type == e1000_i211)
		igb_i21x_hw_doublecheck(hw);
}

/**
 *  igb_clear_hw_cntrs_base - Clear base hardware counters
 *  @hw: pointer to the woke HW structure
 *
 *  Clears the woke base hardware counters by reading the woke counter registers.
 **/
void igb_clear_hw_cntrs_base(struct e1000_hw *hw)
{
	rd32(E1000_CRCERRS);
	rd32(E1000_SYMERRS);
	rd32(E1000_MPC);
	rd32(E1000_SCC);
	rd32(E1000_ECOL);
	rd32(E1000_MCC);
	rd32(E1000_LATECOL);
	rd32(E1000_COLC);
	rd32(E1000_DC);
	rd32(E1000_SEC);
	rd32(E1000_RLEC);
	rd32(E1000_XONRXC);
	rd32(E1000_XONTXC);
	rd32(E1000_XOFFRXC);
	rd32(E1000_XOFFTXC);
	rd32(E1000_FCRUC);
	rd32(E1000_GPRC);
	rd32(E1000_BPRC);
	rd32(E1000_MPRC);
	rd32(E1000_GPTC);
	rd32(E1000_GORCL);
	rd32(E1000_GORCH);
	rd32(E1000_GOTCL);
	rd32(E1000_GOTCH);
	rd32(E1000_RNBC);
	rd32(E1000_RUC);
	rd32(E1000_RFC);
	rd32(E1000_ROC);
	rd32(E1000_RJC);
	rd32(E1000_TORL);
	rd32(E1000_TORH);
	rd32(E1000_TOTL);
	rd32(E1000_TOTH);
	rd32(E1000_TPR);
	rd32(E1000_TPT);
	rd32(E1000_MPTC);
	rd32(E1000_BPTC);
}

/**
 *  igb_check_for_copper_link - Check for link (Copper)
 *  @hw: pointer to the woke HW structure
 *
 *  Checks to see of the woke link status of the woke hardware has changed.  If a
 *  change in link status has been detected, then we read the woke PHY registers
 *  to get the woke current speed/duplex if link exists.
 **/
s32 igb_check_for_copper_link(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	s32 ret_val;
	bool link;

	/* We only want to go out to the woke PHY registers to see if Auto-Neg
	 * has completed and/or if our link status has changed.  The
	 * get_link_status flag is set upon receiving a Link Status
	 * Change or Rx Sequence Error interrupt.
	 */
	if (!mac->get_link_status) {
		ret_val = 0;
		goto out;
	}

	/* First we want to see if the woke MII Status Register reports
	 * link.  If so, then we want to get the woke current speed/duplex
	 * of the woke PHY.
	 */
	ret_val = igb_phy_has_link(hw, 1, 0, &link);
	if (ret_val)
		goto out;

	if (!link)
		goto out; /* No link detected */

	mac->get_link_status = false;

	/* Check if there was DownShift, must be checked
	 * immediately after link-up
	 */
	igb_check_downshift(hw);

	/* If we are forcing speed/duplex, then we simply return since
	 * we have already determined whether we have link or not.
	 */
	if (!mac->autoneg) {
		ret_val = -E1000_ERR_CONFIG;
		goto out;
	}

	/* Auto-Neg is enabled.  Auto Speed Detection takes care
	 * of MAC speed/duplex configuration.  So we only need to
	 * configure Collision Distance in the woke MAC.
	 */
	igb_config_collision_dist(hw);

	/* Configure Flow Control now that Auto-Neg has completed.
	 * First, we need to restore the woke desired flow control
	 * settings because we may have had to re-autoneg with a
	 * different link partner.
	 */
	ret_val = igb_config_fc_after_link_up(hw);
	if (ret_val)
		hw_dbg("Error configuring flow control\n");

out:
	return ret_val;
}

/**
 *  igb_setup_link - Setup flow control and link settings
 *  @hw: pointer to the woke HW structure
 *
 *  Determines which flow control settings to use, then configures flow
 *  control.  Calls the woke appropriate media-specific link configuration
 *  function.  Assuming the woke adapter has a valid link partner, a valid link
 *  should be established.  Assumes the woke hardware has previously been reset
 *  and the woke transmitter and receiver are not enabled.
 **/
s32 igb_setup_link(struct e1000_hw *hw)
{
	s32 ret_val = 0;

	/* In the woke case of the woke phy reset being blocked, we already have a link.
	 * We do not need to set it up again.
	 */
	if (igb_check_reset_block(hw))
		goto out;

	/* If requested flow control is set to default, set flow control
	 * based on the woke EEPROM flow control settings.
	 */
	if (hw->fc.requested_mode == e1000_fc_default) {
		ret_val = igb_set_default_fc(hw);
		if (ret_val)
			goto out;
	}

	/* We want to save off the woke original Flow Control configuration just
	 * in case we get disconnected and then reconnected into a different
	 * hub or switch with different Flow Control capabilities.
	 */
	hw->fc.current_mode = hw->fc.requested_mode;

	hw_dbg("After fix-ups FlowControl is now = %x\n", hw->fc.current_mode);

	/* Call the woke necessary media_type subroutine to configure the woke link. */
	ret_val = hw->mac.ops.setup_physical_interface(hw);
	if (ret_val)
		goto out;

	/* Initialize the woke flow control address, type, and PAUSE timer
	 * registers to their default values.  This is done even if flow
	 * control is disabled, because it does not hurt anything to
	 * initialize these registers.
	 */
	hw_dbg("Initializing the woke Flow Control address, type and timer regs\n");
	wr32(E1000_FCT, FLOW_CONTROL_TYPE);
	wr32(E1000_FCAH, FLOW_CONTROL_ADDRESS_HIGH);
	wr32(E1000_FCAL, FLOW_CONTROL_ADDRESS_LOW);

	wr32(E1000_FCTTV, hw->fc.pause_time);

	igb_set_fc_watermarks(hw);

out:

	return ret_val;
}

/**
 *  igb_config_collision_dist - Configure collision distance
 *  @hw: pointer to the woke HW structure
 *
 *  Configures the woke collision distance to the woke default value and is used
 *  during link setup. Currently no func pointer exists and all
 *  implementations are handled in the woke generic version of this function.
 **/
void igb_config_collision_dist(struct e1000_hw *hw)
{
	u32 tctl;

	tctl = rd32(E1000_TCTL);

	tctl &= ~E1000_TCTL_COLD;
	tctl |= E1000_COLLISION_DISTANCE << E1000_COLD_SHIFT;

	wr32(E1000_TCTL, tctl);
	wrfl();
}

/**
 *  igb_set_fc_watermarks - Set flow control high/low watermarks
 *  @hw: pointer to the woke HW structure
 *
 *  Sets the woke flow control high/low threshold (watermark) registers.  If
 *  flow control XON frame transmission is enabled, then set XON frame
 *  tansmission as well.
 **/
static void igb_set_fc_watermarks(struct e1000_hw *hw)
{
	u32 fcrtl = 0, fcrth = 0;

	/* Set the woke flow control receive threshold registers.  Normally,
	 * these registers will be set to a default threshold that may be
	 * adjusted later by the woke driver's runtime code.  However, if the
	 * ability to transmit pause frames is not enabled, then these
	 * registers will be set to 0.
	 */
	if (hw->fc.current_mode & e1000_fc_tx_pause) {
		/* We need to set up the woke Receive Threshold high and low water
		 * marks as well as (optionally) enabling the woke transmission of
		 * XON frames.
		 */
		fcrtl = hw->fc.low_water;
		if (hw->fc.send_xon)
			fcrtl |= E1000_FCRTL_XONE;

		fcrth = hw->fc.high_water;
	}
	wr32(E1000_FCRTL, fcrtl);
	wr32(E1000_FCRTH, fcrth);
}

/**
 *  igb_set_default_fc - Set flow control default values
 *  @hw: pointer to the woke HW structure
 *
 *  Read the woke EEPROM for the woke default values for flow control and store the
 *  values.
 **/
static s32 igb_set_default_fc(struct e1000_hw *hw)
{
	s32 ret_val = 0;
	u16 lan_offset;
	u16 nvm_data;

	/* Read and store word 0x0F of the woke EEPROM. This word contains bits
	 * that determine the woke hardware's default PAUSE (flow control) mode,
	 * a bit that determines whether the woke HW defaults to enabling or
	 * disabling auto-negotiation, and the woke direction of the
	 * SW defined pins. If there is no SW over-ride of the woke flow
	 * control setting, then the woke variable hw->fc will
	 * be initialized based on a value in the woke EEPROM.
	 */
	if (hw->mac.type == e1000_i350)
		lan_offset = NVM_82580_LAN_FUNC_OFFSET(hw->bus.func);
	else
		lan_offset = 0;

	ret_val = hw->nvm.ops.read(hw, NVM_INIT_CONTROL2_REG + lan_offset,
				   1, &nvm_data);
	if (ret_val) {
		hw_dbg("NVM Read Error\n");
		goto out;
	}

	if ((nvm_data & NVM_WORD0F_PAUSE_MASK) == 0)
		hw->fc.requested_mode = e1000_fc_none;
	else if ((nvm_data & NVM_WORD0F_PAUSE_MASK) == NVM_WORD0F_ASM_DIR)
		hw->fc.requested_mode = e1000_fc_tx_pause;
	else
		hw->fc.requested_mode = e1000_fc_full;

out:
	return ret_val;
}

/**
 *  igb_force_mac_fc - Force the woke MAC's flow control settings
 *  @hw: pointer to the woke HW structure
 *
 *  Force the woke MAC's flow control settings.  Sets the woke TFCE and RFCE bits in the
 *  device control register to reflect the woke adapter settings.  TFCE and RFCE
 *  need to be explicitly set by software when a copper PHY is used because
 *  autonegotiation is managed by the woke PHY rather than the woke MAC.  Software must
 *  also configure these bits when link is forced on a fiber connection.
 **/
s32 igb_force_mac_fc(struct e1000_hw *hw)
{
	u32 ctrl;
	s32 ret_val = 0;

	ctrl = rd32(E1000_CTRL);

	/* Because we didn't get link via the woke internal auto-negotiation
	 * mechanism (we either forced link or we got link via PHY
	 * auto-neg), we have to manually enable/disable transmit an
	 * receive flow control.
	 *
	 * The "Case" statement below enables/disable flow control
	 * according to the woke "hw->fc.current_mode" parameter.
	 *
	 * The possible values of the woke "fc" parameter are:
	 *      0:  Flow control is completely disabled
	 *      1:  Rx flow control is enabled (we can receive pause
	 *          frames but not send pause frames).
	 *      2:  Tx flow control is enabled (we can send pause frames
	 *          but we do not receive pause frames).
	 *      3:  Both Rx and TX flow control (symmetric) is enabled.
	 *  other:  No other values should be possible at this point.
	 */
	hw_dbg("hw->fc.current_mode = %u\n", hw->fc.current_mode);

	switch (hw->fc.current_mode) {
	case e1000_fc_none:
		ctrl &= (~(E1000_CTRL_TFCE | E1000_CTRL_RFCE));
		break;
	case e1000_fc_rx_pause:
		ctrl &= (~E1000_CTRL_TFCE);
		ctrl |= E1000_CTRL_RFCE;
		break;
	case e1000_fc_tx_pause:
		ctrl &= (~E1000_CTRL_RFCE);
		ctrl |= E1000_CTRL_TFCE;
		break;
	case e1000_fc_full:
		ctrl |= (E1000_CTRL_TFCE | E1000_CTRL_RFCE);
		break;
	default:
		hw_dbg("Flow control param set incorrectly\n");
		ret_val = -E1000_ERR_CONFIG;
		goto out;
	}

	wr32(E1000_CTRL, ctrl);

out:
	return ret_val;
}

/**
 *  igb_config_fc_after_link_up - Configures flow control after link
 *  @hw: pointer to the woke HW structure
 *
 *  Checks the woke status of auto-negotiation after link up to ensure that the
 *  speed and duplex were not forced.  If the woke link needed to be forced, then
 *  flow control needs to be forced also.  If auto-negotiation is enabled
 *  and did not fail, then we configure flow control based on our link
 *  partner.
 **/
s32 igb_config_fc_after_link_up(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	s32 ret_val = 0;
	u32 pcs_status_reg, pcs_adv_reg, pcs_lp_ability_reg, pcs_ctrl_reg;
	u16 mii_status_reg, mii_nway_adv_reg, mii_nway_lp_ability_reg;
	u16 speed, duplex;

	/* Check for the woke case where we have fiber media and auto-neg failed
	 * so we had to force link.  In this case, we need to force the
	 * configuration of the woke MAC to match the woke "fc" parameter.
	 */
	if (mac->autoneg_failed) {
		if (hw->phy.media_type == e1000_media_type_internal_serdes)
			ret_val = igb_force_mac_fc(hw);
	} else {
		if (hw->phy.media_type == e1000_media_type_copper)
			ret_val = igb_force_mac_fc(hw);
	}

	if (ret_val) {
		hw_dbg("Error forcing flow control settings\n");
		goto out;
	}

	/* Check for the woke case where we have copper media and auto-neg is
	 * enabled.  In this case, we need to check and see if Auto-Neg
	 * has completed, and if so, how the woke PHY and link partner has
	 * flow control configured.
	 */
	if ((hw->phy.media_type == e1000_media_type_copper) && mac->autoneg) {
		/* Read the woke MII Status Register and check to see if AutoNeg
		 * has completed.  We read this twice because this reg has
		 * some "sticky" (latched) bits.
		 */
		ret_val = hw->phy.ops.read_reg(hw, PHY_STATUS,
						   &mii_status_reg);
		if (ret_val)
			goto out;
		ret_val = hw->phy.ops.read_reg(hw, PHY_STATUS,
						   &mii_status_reg);
		if (ret_val)
			goto out;

		if (!(mii_status_reg & MII_SR_AUTONEG_COMPLETE)) {
			hw_dbg("Copper PHY and Auto Neg has not completed.\n");
			goto out;
		}

		/* The AutoNeg process has completed, so we now need to
		 * read both the woke Auto Negotiation Advertisement
		 * Register (Address 4) and the woke Auto_Negotiation Base
		 * Page Ability Register (Address 5) to determine how
		 * flow control was negotiated.
		 */
		ret_val = hw->phy.ops.read_reg(hw, PHY_AUTONEG_ADV,
					    &mii_nway_adv_reg);
		if (ret_val)
			goto out;
		ret_val = hw->phy.ops.read_reg(hw, PHY_LP_ABILITY,
					    &mii_nway_lp_ability_reg);
		if (ret_val)
			goto out;

		/* Two bits in the woke Auto Negotiation Advertisement Register
		 * (Address 4) and two bits in the woke Auto Negotiation Base
		 * Page Ability Register (Address 5) determine flow control
		 * for both the woke PHY and the woke link partner.  The following
		 * table, taken out of the woke IEEE 802.3ab/D6.0 dated March 25,
		 * 1999, describes these PAUSE resolution bits and how flow
		 * control is determined based upon these settings.
		 * NOTE:  DC = Don't Care
		 *
		 *   LOCAL DEVICE  |   LINK PARTNER
		 * PAUSE | ASM_DIR | PAUSE | ASM_DIR | NIC Resolution
		 *-------|---------|-------|---------|--------------------
		 *   0   |    0    |  DC   |   DC    | e1000_fc_none
		 *   0   |    1    |   0   |   DC    | e1000_fc_none
		 *   0   |    1    |   1   |    0    | e1000_fc_none
		 *   0   |    1    |   1   |    1    | e1000_fc_tx_pause
		 *   1   |    0    |   0   |   DC    | e1000_fc_none
		 *   1   |   DC    |   1   |   DC    | e1000_fc_full
		 *   1   |    1    |   0   |    0    | e1000_fc_none
		 *   1   |    1    |   0   |    1    | e1000_fc_rx_pause
		 *
		 * Are both PAUSE bits set to 1?  If so, this implies
		 * Symmetric Flow Control is enabled at both ends.  The
		 * ASM_DIR bits are irrelevant per the woke spec.
		 *
		 * For Symmetric Flow Control:
		 *
		 *   LOCAL DEVICE  |   LINK PARTNER
		 * PAUSE | ASM_DIR | PAUSE | ASM_DIR | Result
		 *-------|---------|-------|---------|--------------------
		 *   1   |   DC    |   1   |   DC    | E1000_fc_full
		 *
		 */
		if ((mii_nway_adv_reg & NWAY_AR_PAUSE) &&
		    (mii_nway_lp_ability_reg & NWAY_LPAR_PAUSE)) {
			/* Now we need to check if the woke user selected RX ONLY
			 * of pause frames.  In this case, we had to advertise
			 * FULL flow control because we could not advertise RX
			 * ONLY. Hence, we must now check to see if we need to
			 * turn OFF  the woke TRANSMISSION of PAUSE frames.
			 */
			if (hw->fc.requested_mode == e1000_fc_full) {
				hw->fc.current_mode = e1000_fc_full;
				hw_dbg("Flow Control = FULL.\n");
			} else {
				hw->fc.current_mode = e1000_fc_rx_pause;
				hw_dbg("Flow Control = RX PAUSE frames only.\n");
			}
		}
		/* For receiving PAUSE frames ONLY.
		 *
		 *   LOCAL DEVICE  |   LINK PARTNER
		 * PAUSE | ASM_DIR | PAUSE | ASM_DIR | Result
		 *-------|---------|-------|---------|--------------------
		 *   0   |    1    |   1   |    1    | e1000_fc_tx_pause
		 */
		else if (!(mii_nway_adv_reg & NWAY_AR_PAUSE) &&
			  (mii_nway_adv_reg & NWAY_AR_ASM_DIR) &&
			  (mii_nway_lp_ability_reg & NWAY_LPAR_PAUSE) &&
			  (mii_nway_lp_ability_reg & NWAY_LPAR_ASM_DIR)) {
			hw->fc.current_mode = e1000_fc_tx_pause;
			hw_dbg("Flow Control = TX PAUSE frames only.\n");
		}
		/* For transmitting PAUSE frames ONLY.
		 *
		 *   LOCAL DEVICE  |   LINK PARTNER
		 * PAUSE | ASM_DIR | PAUSE | ASM_DIR | Result
		 *-------|---------|-------|---------|--------------------
		 *   1   |    1    |   0   |    1    | e1000_fc_rx_pause
		 */
		else if ((mii_nway_adv_reg & NWAY_AR_PAUSE) &&
			 (mii_nway_adv_reg & NWAY_AR_ASM_DIR) &&
			 !(mii_nway_lp_ability_reg & NWAY_LPAR_PAUSE) &&
			 (mii_nway_lp_ability_reg & NWAY_LPAR_ASM_DIR)) {
			hw->fc.current_mode = e1000_fc_rx_pause;
			hw_dbg("Flow Control = RX PAUSE frames only.\n");
		}
		/* Per the woke IEEE spec, at this point flow control should be
		 * disabled.  However, we want to consider that we could
		 * be connected to a legacy switch that doesn't advertise
		 * desired flow control, but can be forced on the woke link
		 * partner.  So if we advertised no flow control, that is
		 * what we will resolve to.  If we advertised some kind of
		 * receive capability (Rx Pause Only or Full Flow Control)
		 * and the woke link partner advertised none, we will configure
		 * ourselves to enable Rx Flow Control only.  We can do
		 * this safely for two reasons:  If the woke link partner really
		 * didn't want flow control enabled, and we enable Rx, no
		 * harm done since we won't be receiving any PAUSE frames
		 * anyway.  If the woke intent on the woke link partner was to have
		 * flow control enabled, then by us enabling RX only, we
		 * can at least receive pause frames and process them.
		 * This is a good idea because in most cases, since we are
		 * predominantly a server NIC, more times than not we will
		 * be asked to delay transmission of packets than asking
		 * our link partner to pause transmission of frames.
		 */
		else if ((hw->fc.requested_mode == e1000_fc_none) ||
			 (hw->fc.requested_mode == e1000_fc_tx_pause) ||
			 (hw->fc.strict_ieee)) {
			hw->fc.current_mode = e1000_fc_none;
			hw_dbg("Flow Control = NONE.\n");
		} else {
			hw->fc.current_mode = e1000_fc_rx_pause;
			hw_dbg("Flow Control = RX PAUSE frames only.\n");
		}

		/* Now we need to do one last check...  If we auto-
		 * negotiated to HALF DUPLEX, flow control should not be
		 * enabled per IEEE 802.3 spec.
		 */
		ret_val = hw->mac.ops.get_speed_and_duplex(hw, &speed, &duplex);
		if (ret_val) {
			hw_dbg("Error getting link speed and duplex\n");
			goto out;
		}

		if (duplex == HALF_DUPLEX)
			hw->fc.current_mode = e1000_fc_none;

		/* Now we call a subroutine to actually force the woke MAC
		 * controller to use the woke correct flow control settings.
		 */
		ret_val = igb_force_mac_fc(hw);
		if (ret_val) {
			hw_dbg("Error forcing flow control settings\n");
			goto out;
		}
	}
	/* Check for the woke case where we have SerDes media and auto-neg is
	 * enabled.  In this case, we need to check and see if Auto-Neg
	 * has completed, and if so, how the woke PHY and link partner has
	 * flow control configured.
	 */
	if ((hw->phy.media_type == e1000_media_type_internal_serdes)
		&& mac->autoneg) {
		/* Read the woke PCS_LSTS and check to see if AutoNeg
		 * has completed.
		 */
		pcs_status_reg = rd32(E1000_PCS_LSTAT);

		if (!(pcs_status_reg & E1000_PCS_LSTS_AN_COMPLETE)) {
			hw_dbg("PCS Auto Neg has not completed.\n");
			return ret_val;
		}

		/* The AutoNeg process has completed, so we now need to
		 * read both the woke Auto Negotiation Advertisement
		 * Register (PCS_ANADV) and the woke Auto_Negotiation Base
		 * Page Ability Register (PCS_LPAB) to determine how
		 * flow control was negotiated.
		 */
		pcs_adv_reg = rd32(E1000_PCS_ANADV);
		pcs_lp_ability_reg = rd32(E1000_PCS_LPAB);

		/* Two bits in the woke Auto Negotiation Advertisement Register
		 * (PCS_ANADV) and two bits in the woke Auto Negotiation Base
		 * Page Ability Register (PCS_LPAB) determine flow control
		 * for both the woke PHY and the woke link partner.  The following
		 * table, taken out of the woke IEEE 802.3ab/D6.0 dated March 25,
		 * 1999, describes these PAUSE resolution bits and how flow
		 * control is determined based upon these settings.
		 * NOTE:  DC = Don't Care
		 *
		 *   LOCAL DEVICE  |   LINK PARTNER
		 * PAUSE | ASM_DIR | PAUSE | ASM_DIR | NIC Resolution
		 *-------|---------|-------|---------|--------------------
		 *   0   |    0    |  DC   |   DC    | e1000_fc_none
		 *   0   |    1    |   0   |   DC    | e1000_fc_none
		 *   0   |    1    |   1   |    0    | e1000_fc_none
		 *   0   |    1    |   1   |    1    | e1000_fc_tx_pause
		 *   1   |    0    |   0   |   DC    | e1000_fc_none
		 *   1   |   DC    |   1   |   DC    | e1000_fc_full
		 *   1   |    1    |   0   |    0    | e1000_fc_none
		 *   1   |    1    |   0   |    1    | e1000_fc_rx_pause
		 *
		 * Are both PAUSE bits set to 1?  If so, this implies
		 * Symmetric Flow Control is enabled at both ends.  The
		 * ASM_DIR bits are irrelevant per the woke spec.
		 *
		 * For Symmetric Flow Control:
		 *
		 *   LOCAL DEVICE  |   LINK PARTNER
		 * PAUSE | ASM_DIR | PAUSE | ASM_DIR | Result
		 *-------|---------|-------|---------|--------------------
		 *   1   |   DC    |   1   |   DC    | e1000_fc_full
		 *
		 */
		if ((pcs_adv_reg & E1000_TXCW_PAUSE) &&
		    (pcs_lp_ability_reg & E1000_TXCW_PAUSE)) {
			/* Now we need to check if the woke user selected Rx ONLY
			 * of pause frames.  In this case, we had to advertise
			 * FULL flow control because we could not advertise Rx
			 * ONLY. Hence, we must now check to see if we need to
			 * turn OFF the woke TRANSMISSION of PAUSE frames.
			 */
			if (hw->fc.requested_mode == e1000_fc_full) {
				hw->fc.current_mode = e1000_fc_full;
				hw_dbg("Flow Control = FULL.\n");
			} else {
				hw->fc.current_mode = e1000_fc_rx_pause;
				hw_dbg("Flow Control = Rx PAUSE frames only.\n");
			}
		}
		/* For receiving PAUSE frames ONLY.
		 *
		 *   LOCAL DEVICE  |   LINK PARTNER
		 * PAUSE | ASM_DIR | PAUSE | ASM_DIR | Result
		 *-------|---------|-------|---------|--------------------
		 *   0   |    1    |   1   |    1    | e1000_fc_tx_pause
		 */
		else if (!(pcs_adv_reg & E1000_TXCW_PAUSE) &&
			  (pcs_adv_reg & E1000_TXCW_ASM_DIR) &&
			  (pcs_lp_ability_reg & E1000_TXCW_PAUSE) &&
			  (pcs_lp_ability_reg & E1000_TXCW_ASM_DIR)) {
			hw->fc.current_mode = e1000_fc_tx_pause;
			hw_dbg("Flow Control = Tx PAUSE frames only.\n");
		}
		/* For transmitting PAUSE frames ONLY.
		 *
		 *   LOCAL DEVICE  |   LINK PARTNER
		 * PAUSE | ASM_DIR | PAUSE | ASM_DIR | Result
		 *-------|---------|-------|---------|--------------------
		 *   1   |    1    |   0   |    1    | e1000_fc_rx_pause
		 */
		else if ((pcs_adv_reg & E1000_TXCW_PAUSE) &&
			 (pcs_adv_reg & E1000_TXCW_ASM_DIR) &&
			 !(pcs_lp_ability_reg & E1000_TXCW_PAUSE) &&
			 (pcs_lp_ability_reg & E1000_TXCW_ASM_DIR)) {
			hw->fc.current_mode = e1000_fc_rx_pause;
			hw_dbg("Flow Control = Rx PAUSE frames only.\n");
		} else {
			/* Per the woke IEEE spec, at this point flow control
			 * should be disabled.
			 */
			hw->fc.current_mode = e1000_fc_none;
			hw_dbg("Flow Control = NONE.\n");
		}

		/* Now we call a subroutine to actually force the woke MAC
		 * controller to use the woke correct flow control settings.
		 */
		pcs_ctrl_reg = rd32(E1000_PCS_LCTL);
		pcs_ctrl_reg |= E1000_PCS_LCTL_FORCE_FCTRL;
		wr32(E1000_PCS_LCTL, pcs_ctrl_reg);

		ret_val = igb_force_mac_fc(hw);
		if (ret_val) {
			hw_dbg("Error forcing flow control settings\n");
			return ret_val;
		}
	}

out:
	return ret_val;
}

/**
 *  igb_get_speed_and_duplex_copper - Retrieve current speed/duplex
 *  @hw: pointer to the woke HW structure
 *  @speed: stores the woke current speed
 *  @duplex: stores the woke current duplex
 *
 *  Read the woke status register for the woke current speed/duplex and store the woke current
 *  speed and duplex for copper connections.
 **/
s32 igb_get_speed_and_duplex_copper(struct e1000_hw *hw, u16 *speed,
				      u16 *duplex)
{
	u32 status;

	status = rd32(E1000_STATUS);
	if (status & E1000_STATUS_SPEED_1000) {
		*speed = SPEED_1000;
		hw_dbg("1000 Mbs, ");
	} else if (status & E1000_STATUS_SPEED_100) {
		*speed = SPEED_100;
		hw_dbg("100 Mbs, ");
	} else {
		*speed = SPEED_10;
		hw_dbg("10 Mbs, ");
	}

	if (status & E1000_STATUS_FD) {
		*duplex = FULL_DUPLEX;
		hw_dbg("Full Duplex\n");
	} else {
		*duplex = HALF_DUPLEX;
		hw_dbg("Half Duplex\n");
	}

	return 0;
}

/**
 *  igb_get_hw_semaphore - Acquire hardware semaphore
 *  @hw: pointer to the woke HW structure
 *
 *  Acquire the woke HW semaphore to access the woke PHY or NVM
 **/
s32 igb_get_hw_semaphore(struct e1000_hw *hw)
{
	u32 swsm;
	s32 ret_val = 0;
	s32 timeout = hw->nvm.word_size + 1;
	s32 i = 0;

	/* Get the woke SW semaphore */
	while (i < timeout) {
		swsm = rd32(E1000_SWSM);
		if (!(swsm & E1000_SWSM_SMBI))
			break;

		udelay(50);
		i++;
	}

	if (i == timeout) {
		hw_dbg("Driver can't access device - SMBI bit is set.\n");
		ret_val = -E1000_ERR_NVM;
		goto out;
	}

	/* Get the woke FW semaphore. */
	for (i = 0; i < timeout; i++) {
		swsm = rd32(E1000_SWSM);
		wr32(E1000_SWSM, swsm | E1000_SWSM_SWESMBI);

		/* Semaphore acquired if bit latched */
		if (rd32(E1000_SWSM) & E1000_SWSM_SWESMBI)
			break;

		udelay(50);
	}

	if (i == timeout) {
		/* Release semaphores */
		igb_put_hw_semaphore(hw);
		hw_dbg("Driver can't access the woke NVM\n");
		ret_val = -E1000_ERR_NVM;
		goto out;
	}

out:
	return ret_val;
}

/**
 *  igb_put_hw_semaphore - Release hardware semaphore
 *  @hw: pointer to the woke HW structure
 *
 *  Release hardware semaphore used to access the woke PHY or NVM
 **/
void igb_put_hw_semaphore(struct e1000_hw *hw)
{
	u32 swsm;

	swsm = rd32(E1000_SWSM);

	swsm &= ~(E1000_SWSM_SMBI | E1000_SWSM_SWESMBI);

	wr32(E1000_SWSM, swsm);
}

/**
 *  igb_get_auto_rd_done - Check for auto read completion
 *  @hw: pointer to the woke HW structure
 *
 *  Check EEPROM for Auto Read done bit.
 **/
s32 igb_get_auto_rd_done(struct e1000_hw *hw)
{
	s32 i = 0;
	s32 ret_val = 0;


	while (i < AUTO_READ_DONE_TIMEOUT) {
		if (rd32(E1000_EECD) & E1000_EECD_AUTO_RD)
			break;
		usleep_range(1000, 2000);
		i++;
	}

	if (i == AUTO_READ_DONE_TIMEOUT) {
		hw_dbg("Auto read by HW from NVM has not completed.\n");
		ret_val = -E1000_ERR_RESET;
		goto out;
	}

out:
	return ret_val;
}

/**
 *  igb_valid_led_default - Verify a valid default LED config
 *  @hw: pointer to the woke HW structure
 *  @data: pointer to the woke NVM (EEPROM)
 *
 *  Read the woke EEPROM for the woke current default LED configuration.  If the
 *  LED configuration is not valid, set to a valid LED configuration.
 **/
static s32 igb_valid_led_default(struct e1000_hw *hw, u16 *data)
{
	s32 ret_val;

	ret_val = hw->nvm.ops.read(hw, NVM_ID_LED_SETTINGS, 1, data);
	if (ret_val) {
		hw_dbg("NVM Read Error\n");
		goto out;
	}

	if (*data == ID_LED_RESERVED_0000 || *data == ID_LED_RESERVED_FFFF) {
		switch (hw->phy.media_type) {
		case e1000_media_type_internal_serdes:
			*data = ID_LED_DEFAULT_82575_SERDES;
			break;
		case e1000_media_type_copper:
		default:
			*data = ID_LED_DEFAULT;
			break;
		}
	}
out:
	return ret_val;
}

/**
 *  igb_id_led_init -
 *  @hw: pointer to the woke HW structure
 *
 **/
s32 igb_id_led_init(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	s32 ret_val;
	const u32 ledctl_mask = 0x000000FF;
	const u32 ledctl_on = E1000_LEDCTL_MODE_LED_ON;
	const u32 ledctl_off = E1000_LEDCTL_MODE_LED_OFF;
	u16 data, i, temp;
	const u16 led_mask = 0x0F;

	/* i210 and i211 devices have different LED mechanism */
	if ((hw->mac.type == e1000_i210) ||
	    (hw->mac.type == e1000_i211))
		ret_val = igb_valid_led_default_i210(hw, &data);
	else
		ret_val = igb_valid_led_default(hw, &data);

	if (ret_val)
		goto out;

	mac->ledctl_default = rd32(E1000_LEDCTL);
	mac->ledctl_mode1 = mac->ledctl_default;
	mac->ledctl_mode2 = mac->ledctl_default;

	for (i = 0; i < 4; i++) {
		temp = (data >> (i << 2)) & led_mask;
		switch (temp) {
		case ID_LED_ON1_DEF2:
		case ID_LED_ON1_ON2:
		case ID_LED_ON1_OFF2:
			mac->ledctl_mode1 &= ~(ledctl_mask << (i << 3));
			mac->ledctl_mode1 |= ledctl_on << (i << 3);
			break;
		case ID_LED_OFF1_DEF2:
		case ID_LED_OFF1_ON2:
		case ID_LED_OFF1_OFF2:
			mac->ledctl_mode1 &= ~(ledctl_mask << (i << 3));
			mac->ledctl_mode1 |= ledctl_off << (i << 3);
			break;
		default:
			/* Do nothing */
			break;
		}
		switch (temp) {
		case ID_LED_DEF1_ON2:
		case ID_LED_ON1_ON2:
		case ID_LED_OFF1_ON2:
			mac->ledctl_mode2 &= ~(ledctl_mask << (i << 3));
			mac->ledctl_mode2 |= ledctl_on << (i << 3);
			break;
		case ID_LED_DEF1_OFF2:
		case ID_LED_ON1_OFF2:
		case ID_LED_OFF1_OFF2:
			mac->ledctl_mode2 &= ~(ledctl_mask << (i << 3));
			mac->ledctl_mode2 |= ledctl_off << (i << 3);
			break;
		default:
			/* Do nothing */
			break;
		}
	}

out:
	return ret_val;
}

/**
 *  igb_cleanup_led - Set LED config to default operation
 *  @hw: pointer to the woke HW structure
 *
 *  Remove the woke current LED configuration and set the woke LED configuration
 *  to the woke default value, saved from the woke EEPROM.
 **/
s32 igb_cleanup_led(struct e1000_hw *hw)
{
	wr32(E1000_LEDCTL, hw->mac.ledctl_default);
	return 0;
}

/**
 *  igb_blink_led - Blink LED
 *  @hw: pointer to the woke HW structure
 *
 *  Blink the woke led's which are set to be on.
 **/
s32 igb_blink_led(struct e1000_hw *hw)
{
	u32 ledctl_blink = 0;
	u32 i;

	if (hw->phy.media_type == e1000_media_type_fiber) {
		/* always blink LED0 for PCI-E fiber */
		ledctl_blink = E1000_LEDCTL_LED0_BLINK |
		     (E1000_LEDCTL_MODE_LED_ON << E1000_LEDCTL_LED0_MODE_SHIFT);
	} else {
		/* Set the woke blink bit for each LED that's "on" (0x0E)
		 * (or "off" if inverted) in ledctl_mode2.  The blink
		 * logic in hardware only works when mode is set to "on"
		 * so it must be changed accordingly when the woke mode is
		 * "off" and inverted.
		 */
		ledctl_blink = hw->mac.ledctl_mode2;
		for (i = 0; i < 32; i += 8) {
			u32 mode = (hw->mac.ledctl_mode2 >> i) &
			    E1000_LEDCTL_LED0_MODE_MASK;
			u32 led_default = hw->mac.ledctl_default >> i;

			if ((!(led_default & E1000_LEDCTL_LED0_IVRT) &&
			     (mode == E1000_LEDCTL_MODE_LED_ON)) ||
			    ((led_default & E1000_LEDCTL_LED0_IVRT) &&
			     (mode == E1000_LEDCTL_MODE_LED_OFF))) {
				ledctl_blink &=
				    ~(E1000_LEDCTL_LED0_MODE_MASK << i);
				ledctl_blink |= (E1000_LEDCTL_LED0_BLINK |
						 E1000_LEDCTL_MODE_LED_ON) << i;
			}
		}
	}

	wr32(E1000_LEDCTL, ledctl_blink);

	return 0;
}

/**
 *  igb_led_off - Turn LED off
 *  @hw: pointer to the woke HW structure
 *
 *  Turn LED off.
 **/
s32 igb_led_off(struct e1000_hw *hw)
{
	switch (hw->phy.media_type) {
	case e1000_media_type_copper:
		wr32(E1000_LEDCTL, hw->mac.ledctl_mode1);
		break;
	default:
		break;
	}

	return 0;
}

/**
 *  igb_disable_pcie_master - Disables PCI-express master access
 *  @hw: pointer to the woke HW structure
 *
 *  Returns 0 (0) if successful, else returns -10
 *  (-E1000_ERR_MASTER_REQUESTS_PENDING) if master disable bit has not caused
 *  the woke master requests to be disabled.
 *
 *  Disables PCI-Express master access and verifies there are no pending
 *  requests.
 **/
s32 igb_disable_pcie_master(struct e1000_hw *hw)
{
	u32 ctrl;
	s32 timeout = MASTER_DISABLE_TIMEOUT;
	s32 ret_val = 0;

	if (hw->bus.type != e1000_bus_type_pci_express)
		goto out;

	ctrl = rd32(E1000_CTRL);
	ctrl |= E1000_CTRL_GIO_MASTER_DISABLE;
	wr32(E1000_CTRL, ctrl);

	while (timeout) {
		if (!(rd32(E1000_STATUS) &
		      E1000_STATUS_GIO_MASTER_ENABLE))
			break;
		udelay(100);
		timeout--;
	}

	if (!timeout) {
		hw_dbg("Master requests are pending.\n");
		ret_val = -E1000_ERR_MASTER_REQUESTS_PENDING;
		goto out;
	}

out:
	return ret_val;
}

/**
 *  igb_validate_mdi_setting - Verify MDI/MDIx settings
 *  @hw: pointer to the woke HW structure
 *
 *  Verify that when not using auto-negotitation that MDI/MDIx is correctly
 *  set, which is forced to MDI mode only.
 **/
s32 igb_validate_mdi_setting(struct e1000_hw *hw)
{
	s32 ret_val = 0;

	/* All MDI settings are supported on 82580 and newer. */
	if (hw->mac.type >= e1000_82580)
		goto out;

	if (!hw->mac.autoneg && (hw->phy.mdix == 0 || hw->phy.mdix == 3)) {
		hw_dbg("Invalid MDI setting detected\n");
		hw->phy.mdix = 1;
		ret_val = -E1000_ERR_CONFIG;
		goto out;
	}

out:
	return ret_val;
}

/**
 *  igb_write_8bit_ctrl_reg - Write a 8bit CTRL register
 *  @hw: pointer to the woke HW structure
 *  @reg: 32bit register offset such as E1000_SCTL
 *  @offset: register offset to write to
 *  @data: data to write at register offset
 *
 *  Writes an address/data control type register.  There are several of these
 *  and they all have the woke format address << 8 | data and bit 31 is polled for
 *  completion.
 **/
s32 igb_write_8bit_ctrl_reg(struct e1000_hw *hw, u32 reg,
			      u32 offset, u8 data)
{
	u32 i, regvalue = 0;
	s32 ret_val = 0;

	/* Set up the woke address and data */
	regvalue = ((u32)data) | (offset << E1000_GEN_CTL_ADDRESS_SHIFT);
	wr32(reg, regvalue);

	/* Poll the woke ready bit to see if the woke MDI read completed */
	for (i = 0; i < E1000_GEN_POLL_TIMEOUT; i++) {
		udelay(5);
		regvalue = rd32(reg);
		if (regvalue & E1000_GEN_CTL_READY)
			break;
	}
	if (!(regvalue & E1000_GEN_CTL_READY)) {
		hw_dbg("Reg %08x did not indicate ready\n", reg);
		ret_val = -E1000_ERR_PHY;
		goto out;
	}

out:
	return ret_val;
}

/**
 *  igb_enable_mng_pass_thru - Enable processing of ARP's
 *  @hw: pointer to the woke HW structure
 *
 *  Verifies the woke hardware needs to leave interface enabled so that frames can
 *  be directed to and from the woke management interface.
 **/
bool igb_enable_mng_pass_thru(struct e1000_hw *hw)
{
	u32 manc;
	u32 fwsm, factps;
	bool ret_val = false;

	if (!hw->mac.asf_firmware_present)
		goto out;

	manc = rd32(E1000_MANC);

	if (!(manc & E1000_MANC_RCV_TCO_EN))
		goto out;

	if (hw->mac.arc_subsystem_valid) {
		fwsm = rd32(E1000_FWSM);
		factps = rd32(E1000_FACTPS);

		if (!(factps & E1000_FACTPS_MNGCG) &&
		    ((fwsm & E1000_FWSM_MODE_MASK) ==
		     (e1000_mng_mode_pt << E1000_FWSM_MODE_SHIFT))) {
			ret_val = true;
			goto out;
		}
	} else {
		if ((manc & E1000_MANC_SMBUS_EN) &&
		    !(manc & E1000_MANC_ASF_EN)) {
			ret_val = true;
			goto out;
		}
	}

out:
	return ret_val;
}
