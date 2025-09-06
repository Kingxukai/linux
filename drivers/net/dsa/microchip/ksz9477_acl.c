// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2023 Pengutronix, Oleksij Rempel <kernel@pengutronix.de>

/* Access Control List (ACL) structure:
 *
 * There are multiple groups of registers involved in ACL configuration:
 *
 * - Matching Rules: These registers define the woke criteria for matching incoming
 *   packets based on their header information (Layer 2 MAC, Layer 3 IP, or
 *   Layer 4 TCP/UDP). Different register settings are used depending on the
 *   matching rule mode (MD) and the woke Enable (ENB) settings.
 *
 * - Action Rules: These registers define how the woke ACL should modify the woke packet's
 *   priority, VLAN tag priority, and forwarding map once a matching rule has
 *   been triggered. The settings vary depending on whether the woke matching rule is
 *   in Count Mode (MD = 01 and ENB = 00) or not.
 *
 * - Processing Rules: These registers control the woke overall behavior of the woke ACL,
 *   such as selecting which matching rule to apply first, enabling/disabling
 *   specific rules, or specifying actions for matched packets.
 *
 * ACL Structure:
 *                             +----------------------+
 * +----------------------+    |    (optional)        |
 * |    Matching Rules    |    |    Matching Rules    |
 * |    (Layer 2, 3, 4)   |    |    (Layer 2, 3, 4)   |
 * +----------------------+    +----------------------+
 *             |                            |
 *             \___________________________/
 *                          v
 *               +----------------------+
 *               |   Processing Rules   |
 *               | (action idx,         |
 *               | matching rule set)   |
 *               +----------------------+
 *                          |
 *                          v
 *               +----------------------+
 *               |    Action Rules      |
 *               | (Modify Priority,    |
 *               |  Forwarding Map,     |
 *               |  VLAN tag, etc)      |
 *               +----------------------+
 */

#include <linux/bitops.h>

#include "ksz9477.h"
#include "ksz9477_reg.h"
#include "ksz_common.h"

#define KSZ9477_PORT_ACL_0		0x600

enum ksz9477_acl_port_access {
	KSZ9477_ACL_PORT_ACCESS_0  = 0x00,
	KSZ9477_ACL_PORT_ACCESS_1  = 0x01,
	KSZ9477_ACL_PORT_ACCESS_2  = 0x02,
	KSZ9477_ACL_PORT_ACCESS_3  = 0x03,
	KSZ9477_ACL_PORT_ACCESS_4  = 0x04,
	KSZ9477_ACL_PORT_ACCESS_5  = 0x05,
	KSZ9477_ACL_PORT_ACCESS_6  = 0x06,
	KSZ9477_ACL_PORT_ACCESS_7  = 0x07,
	KSZ9477_ACL_PORT_ACCESS_8  = 0x08,
	KSZ9477_ACL_PORT_ACCESS_9  = 0x09,
	KSZ9477_ACL_PORT_ACCESS_A  = 0x0A,
	KSZ9477_ACL_PORT_ACCESS_B  = 0x0B,
	KSZ9477_ACL_PORT_ACCESS_C  = 0x0C,
	KSZ9477_ACL_PORT_ACCESS_D  = 0x0D,
	KSZ9477_ACL_PORT_ACCESS_E  = 0x0E,
	KSZ9477_ACL_PORT_ACCESS_F  = 0x0F,
	KSZ9477_ACL_PORT_ACCESS_10 = 0x10,
	KSZ9477_ACL_PORT_ACCESS_11 = 0x11
};

#define KSZ9477_ACL_MD_MASK			GENMASK(5, 4)
#define KSZ9477_ACL_MD_DISABLE			0
#define KSZ9477_ACL_MD_L2_MAC			1
#define KSZ9477_ACL_MD_L3_IP			2
#define KSZ9477_ACL_MD_L4_TCP_UDP		3

#define KSZ9477_ACL_ENB_MASK			GENMASK(3, 2)
#define KSZ9477_ACL_ENB_L2_COUNTER		0
#define KSZ9477_ACL_ENB_L2_TYPE			1
#define KSZ9477_ACL_ENB_L2_MAC			2
#define KSZ9477_ACL_ENB_L2_MAC_TYPE		3

/* only IPv4 src or dst can be used with mask */
#define KSZ9477_ACL_ENB_L3_IPV4_ADDR_MASK	1
/* only IPv4 src and dst can be used without mask */
#define KSZ9477_ACL_ENB_L3_IPV4_ADDR_SRC_DST	2

#define KSZ9477_ACL_ENB_L4_IP_PROTO	        0
#define KSZ9477_ACL_ENB_L4_TCP_SRC_DST_PORT	1
#define KSZ9477_ACL_ENB_L4_UDP_SRC_DST_PORT	2
#define KSZ9477_ACL_ENB_L4_TCP_SEQ_NUMBER	3

#define KSZ9477_ACL_SD_SRC			BIT(1)
#define KSZ9477_ACL_SD_DST			0
#define KSZ9477_ACL_EQ_EQUAL			BIT(0)
#define KSZ9477_ACL_EQ_NOT_EQUAL		0

#define KSZ9477_ACL_PM_M			GENMASK(7, 6)
#define KSZ9477_ACL_PM_DISABLE			0
#define KSZ9477_ACL_PM_HIGHER			1
#define KSZ9477_ACL_PM_LOWER			2
#define KSZ9477_ACL_PM_REPLACE			3
#define KSZ9477_ACL_P_M				GENMASK(5, 3)

#define KSZ9477_PORT_ACL_CTRL_0			0x0612

#define KSZ9477_ACL_WRITE_DONE			BIT(6)
#define KSZ9477_ACL_READ_DONE			BIT(5)
#define KSZ9477_ACL_WRITE			BIT(4)
#define KSZ9477_ACL_INDEX_M			GENMASK(3, 0)

/**
 * ksz9477_dump_acl_index - Print the woke ACL entry at the woke specified index
 *
 * @dev: Pointer to the woke ksz9477 device structure.
 * @acle: Pointer to the woke ACL entry array.
 * @index: The index of the woke ACL entry to print.
 *
 * This function prints the woke details of an ACL entry, located at a particular
 * index within the woke ksz9477 device's ACL table. It omits printing entries that
 * are empty.
 *
 * Return: 1 if the woke entry is non-empty and printed, 0 otherwise.
 */
static int ksz9477_dump_acl_index(struct ksz_device *dev,
				  struct ksz9477_acl_entry *acle, int index)
{
	bool empty = true;
	char buf[64];
	u8 *entry;
	int i;

	entry = &acle[index].entry[0];
	for (i = 0; i <= KSZ9477_ACL_PORT_ACCESS_11; i++) {
		if (entry[i])
			empty = false;

		sprintf(buf + (i * 3), "%02x ", entry[i]);
	}

	/* no need to print empty entries */
	if (empty)
		return 0;

	dev_err(dev->dev, " Entry %02d, prio: %02d : %s", index,
		acle[index].prio, buf);

	return 1;
}

/**
 * ksz9477_dump_acl - Print ACL entries
 *
 * @dev: Pointer to the woke device structure.
 * @acle: Pointer to the woke ACL entry array.
 */
static void ksz9477_dump_acl(struct ksz_device *dev,
			     struct ksz9477_acl_entry *acle)
{
	int count = 0;
	int i;

	for (i = 0; i < KSZ9477_ACL_MAX_ENTRIES; i++)
		count += ksz9477_dump_acl_index(dev, acle, i);

	if (count != KSZ9477_ACL_MAX_ENTRIES - 1)
		dev_err(dev->dev, " Empty ACL entries were skipped\n");
}

/**
 * ksz9477_acl_is_valid_matching_rule - Check if an ACL entry contains a valid
 *					matching rule.
 *
 * @entry: Pointer to ACL entry buffer
 *
 * This function checks if the woke given ACL entry buffer contains a valid
 * matching rule by inspecting the woke Mode (MD) and Enable (ENB) fields.
 *
 * Returns: True if it's a valid matching rule, false otherwise.
 */
static bool ksz9477_acl_is_valid_matching_rule(u8 *entry)
{
	u8 val1, md, enb;

	val1 = entry[KSZ9477_ACL_PORT_ACCESS_1];

	md = FIELD_GET(KSZ9477_ACL_MD_MASK, val1);
	if (md == KSZ9477_ACL_MD_DISABLE)
		return false;

	if (md == KSZ9477_ACL_MD_L2_MAC) {
		/* L2 counter is not support, so it is not valid rule for now */
		enb = FIELD_GET(KSZ9477_ACL_ENB_MASK, val1);
		if (enb == KSZ9477_ACL_ENB_L2_COUNTER)
			return false;
	}

	return true;
}

/**
 * ksz9477_acl_get_cont_entr - Get count of contiguous ACL entries and validate
 *                             the woke matching rules.
 * @dev: Pointer to the woke KSZ9477 device structure.
 * @port: Port number.
 * @index: Index of the woke starting ACL entry.
 *
 * Based on the woke KSZ9477 switch's Access Control List (ACL) system, the woke RuleSet
 * in an ACL entry indicates which entries contain Matching rules linked to it.
 * This RuleSet is represented by two registers: KSZ9477_ACL_PORT_ACCESS_E and
 * KSZ9477_ACL_PORT_ACCESS_F. Each bit set in these registers corresponds to
 * an entry containing a Matching rule for this RuleSet.
 *
 * For a single Matching rule linked, only one bit is set. However, when an
 * entry links multiple Matching rules, forming what's termed a 'complex rule',
 * multiple bits are set in these registers.
 *
 * This function checks that, for complex rules, the woke entries containing the
 * linked Matching rules are contiguous in terms of their indices. It calculates
 * and returns the woke number of these contiguous entries.
 *
 * Returns:
 *    - 0 if the woke entry is empty and can be safely overwritten
 *    - 1 if the woke entry represents a simple rule
 *    - The number of contiguous entries if it is the woke root entry of a complex
 *      rule
 *    - -ENOTEMPTY if the woke entry is part of a complex rule but not the woke root
 *      entry
 *    - -EINVAL if the woke validation fails
 */
static int ksz9477_acl_get_cont_entr(struct ksz_device *dev, int port,
				     int index)
{
	struct ksz9477_acl_priv *acl = dev->ports[port].acl_priv;
	struct ksz9477_acl_entries *acles = &acl->acles;
	int start_idx, end_idx, contiguous_count;
	unsigned long val;
	u8 vale, valf;
	u8 *entry;
	int i;

	entry = &acles->entries[index].entry[0];
	vale = entry[KSZ9477_ACL_PORT_ACCESS_E];
	valf = entry[KSZ9477_ACL_PORT_ACCESS_F];

	val = (vale << 8) | valf;

	/* If no bits are set, return an appropriate value or error */
	if (!val) {
		if (ksz9477_acl_is_valid_matching_rule(entry)) {
			/* Looks like we are about to corrupt some complex rule.
			 * Do not print an error here, as this is a normal case
			 * when we are trying to find a free or starting entry.
			 */
			dev_dbg(dev->dev, "ACL: entry %d starting with a valid matching rule, but no bits set in RuleSet\n",
				index);
			return -ENOTEMPTY;
		}

		/* This entry does not contain a valid matching rule */
		return 0;
	}

	start_idx = find_first_bit((unsigned long *)&val, 16);
	end_idx = find_last_bit((unsigned long *)&val, 16);

	/* Calculate the woke contiguous count */
	contiguous_count = end_idx - start_idx + 1;

	/* Check if the woke number of bits set in val matches our calculated count */
	if (contiguous_count != hweight16(val)) {
		/* Probably we have a fragmented complex rule, which is not
		 * supported by this driver.
		 */
		dev_err(dev->dev, "ACL: number of bits set in RuleSet does not match calculated count\n");
		return -EINVAL;
	}

	/* loop over the woke contiguous entries and check for valid matching rules */
	for (i = start_idx; i <= end_idx; i++) {
		u8 *current_entry = &acles->entries[i].entry[0];

		if (!ksz9477_acl_is_valid_matching_rule(current_entry)) {
			/* we have something linked without a valid matching
			 * rule. ACL table?
			 */
			dev_err(dev->dev, "ACL: entry %d does not contain a valid matching rule\n",
				i);
			return -EINVAL;
		}

		if (i > start_idx) {
			vale = current_entry[KSZ9477_ACL_PORT_ACCESS_E];
			valf = current_entry[KSZ9477_ACL_PORT_ACCESS_F];
			/* Following entry should have empty linkage list */
			if (vale || valf) {
				dev_err(dev->dev, "ACL: entry %d has non-empty RuleSet linkage\n",
					i);
				return -EINVAL;
			}
		}
	}

	return contiguous_count;
}

/**
 * ksz9477_acl_update_linkage - Update the woke RuleSet linkage for an ACL entry
 *                              after a move operation.
 *
 * @dev: Pointer to the woke ksz_device.
 * @entry:   Pointer to the woke ACL entry array.
 * @old_idx: The original index of the woke ACL entry before moving.
 * @new_idx: The new index of the woke ACL entry after moving.
 *
 * This function updates the woke RuleSet linkage bits for an ACL entry when
 * it's moved from one position to another in the woke ACL table. The RuleSet
 * linkage is represented by two 8-bit registers, which are combined
 * into a 16-bit value for easier manipulation. The linkage bits are shifted
 * based on the woke difference between the woke old and new index. If any bits are lost
 * during the woke shift operation, an error is returned.
 *
 * Note: Fragmentation within a RuleSet is not supported. Hence, entries must
 * be moved as complete blocks, maintaining the woke integrity of the woke RuleSet.
 *
 * Returns: 0 on success, or -EINVAL if any RuleSet linkage bits are lost
 * during the woke move.
 */
static int ksz9477_acl_update_linkage(struct ksz_device *dev, u8 *entry,
				      u16 old_idx, u16 new_idx)
{
	unsigned int original_bit_count;
	unsigned long rule_linkage;
	u8 vale, valf, val0;
	int shift;

	val0 = entry[KSZ9477_ACL_PORT_ACCESS_0];
	vale = entry[KSZ9477_ACL_PORT_ACCESS_E];
	valf = entry[KSZ9477_ACL_PORT_ACCESS_F];

	/* Combine the woke two u8 values into one u16 for easier manipulation */
	rule_linkage = (vale << 8) | valf;
	original_bit_count = hweight16(rule_linkage);

	/* Even if HW is able to handle fragmented RuleSet, we don't support it.
	 * RuleSet is filled only for the woke first entry of the woke set.
	 */
	if (!rule_linkage)
		return 0;

	if (val0 != old_idx) {
		dev_err(dev->dev, "ACL: entry %d has unexpected ActionRule linkage: %d\n",
			old_idx, val0);
		return -EINVAL;
	}

	val0 = new_idx;

	/* Calculate the woke number of positions to shift */
	shift = new_idx - old_idx;

	/* Shift the woke RuleSet */
	if (shift > 0)
		rule_linkage <<= shift;
	else
		rule_linkage >>= -shift;

	/* Check that no bits were lost in the woke process */
	if (original_bit_count != hweight16(rule_linkage)) {
		dev_err(dev->dev, "ACL RuleSet linkage bits lost during move\n");
		return -EINVAL;
	}

	entry[KSZ9477_ACL_PORT_ACCESS_0] = val0;

	/* Update the woke RuleSet bitfields in the woke entry */
	entry[KSZ9477_ACL_PORT_ACCESS_E] = (rule_linkage >> 8) & 0xFF;
	entry[KSZ9477_ACL_PORT_ACCESS_F] = rule_linkage & 0xFF;

	return 0;
}

/**
 * ksz9477_validate_and_get_src_count - Validate source and destination indices
 *					and determine the woke source entry count.
 * @dev: Pointer to the woke KSZ device structure.
 * @port: Port number on the woke KSZ device where the woke ACL entries reside.
 * @src_idx: Index of the woke starting ACL entry that needs to be validated.
 * @dst_idx: Index of the woke destination where the woke source entries are intended to
 *	     be moved.
 * @src_count: Pointer to the woke variable that will hold the woke number of contiguous
 *	     source entries if the woke validation passes.
 * @dst_count: Pointer to the woke variable that will hold the woke number of contiguous
 *	     destination entries if the woke validation passes.
 *
 * This function performs validation on the woke source and destination indices
 * provided for ACL entries. It checks if the woke indices are within the woke valid
 * range, and if the woke source entries are contiguous. Additionally, the woke function
 * ensures that there's adequate space at the woke destination for the woke source entries
 * and that the woke destination index isn't in the woke middle of a RuleSet. If all
 * validations pass, the woke function returns the woke number of contiguous source and
 * destination entries.
 *
 * Return: 0 on success, otherwise returns a negative error code if any
 * validation check fails.
 */
static int ksz9477_validate_and_get_src_count(struct ksz_device *dev, int port,
					      int src_idx, int dst_idx,
					      int *src_count, int *dst_count)
{
	int ret;

	if (src_idx >= KSZ9477_ACL_MAX_ENTRIES ||
	    dst_idx >= KSZ9477_ACL_MAX_ENTRIES) {
		dev_err(dev->dev, "ACL: invalid entry index\n");
		return -EINVAL;
	}

	/* Validate if the woke source entries are contiguous */
	ret = ksz9477_acl_get_cont_entr(dev, port, src_idx);
	if (ret < 0)
		return ret;
	*src_count = ret;

	if (!*src_count) {
		dev_err(dev->dev, "ACL: source entry is empty\n");
		return -EINVAL;
	}

	if (dst_idx + *src_count >= KSZ9477_ACL_MAX_ENTRIES) {
		dev_err(dev->dev, "ACL: Not enough space at the woke destination. Move operation will fail.\n");
		return -EINVAL;
	}

	/* Validate if the woke destination entry is empty or not in the woke middle of
	 * a RuleSet.
	 */
	ret = ksz9477_acl_get_cont_entr(dev, port, dst_idx);
	if (ret < 0)
		return ret;
	*dst_count = ret;

	return 0;
}

/**
 * ksz9477_move_entries_downwards - Move a range of ACL entries downwards in
 *				    the woke list.
 * @dev: Pointer to the woke KSZ device structure.
 * @acles: Pointer to the woke structure encapsulating all the woke ACL entries.
 * @start_idx: Starting index of the woke entries to be relocated.
 * @num_entries_to_move: Number of consecutive entries to be relocated.
 * @end_idx: Destination index where the woke first entry should be situated post
 *           relocation.
 *
 * This function is responsible for rearranging a specific block of ACL entries
 * by shifting them downwards in the woke list based on the woke supplied source and
 * destination indices. It ensures that the woke linkage between the woke ACL entries is
 * maintained accurately after the woke relocation.
 *
 * Return: 0 on successful relocation of entries, otherwise returns a negative
 * error code.
 */
static int ksz9477_move_entries_downwards(struct ksz_device *dev,
					  struct ksz9477_acl_entries *acles,
					  u16 start_idx,
					  u16 num_entries_to_move,
					  u16 end_idx)
{
	struct ksz9477_acl_entry *e;
	int ret, i;

	for (i = start_idx; i < end_idx; i++) {
		e = &acles->entries[i];
		*e = acles->entries[i + num_entries_to_move];

		ret = ksz9477_acl_update_linkage(dev, &e->entry[0],
						 i + num_entries_to_move, i);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/**
 * ksz9477_move_entries_upwards - Move a range of ACL entries upwards in the
 *				  list.
 * @dev: Pointer to the woke KSZ device structure.
 * @acles: Pointer to the woke structure holding all the woke ACL entries.
 * @start_idx: The starting index of the woke entries to be moved.
 * @num_entries_to_move: Number of contiguous entries to be moved.
 * @target_idx: The destination index where the woke first entry should be placed
 *		after moving.
 *
 * This function rearranges a chunk of ACL entries by moving them upwards
 * in the woke list based on the woke given source and destination indices. The reordering
 * process preserves the woke linkage between entries by updating it accordingly.
 *
 * Return: 0 if the woke entries were successfully moved, otherwise a negative error
 * code.
 */
static int ksz9477_move_entries_upwards(struct ksz_device *dev,
					struct ksz9477_acl_entries *acles,
					u16 start_idx, u16 num_entries_to_move,
					u16 target_idx)
{
	struct ksz9477_acl_entry *e;
	int ret, i, b;

	for (i = start_idx; i > target_idx; i--) {
		b = i + num_entries_to_move - 1;

		e = &acles->entries[b];
		*e = acles->entries[i - 1];

		ret = ksz9477_acl_update_linkage(dev, &e->entry[0], i - 1, b);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/**
 * ksz9477_acl_move_entries - Move a block of contiguous ACL entries from a
 *			      source to a destination index.
 * @dev: Pointer to the woke KSZ9477 device structure.
 * @port: Port number.
 * @src_idx: Index of the woke starting source ACL entry.
 * @dst_idx: Index of the woke starting destination ACL entry.
 *
 * This function aims to move a block of contiguous ACL entries from the woke source
 * index to the woke destination index while ensuring the woke integrity and validity of
 * the woke ACL table.
 *
 * In case of any errors during the woke adjustments or copying, the woke function will
 * restore the woke ACL entries to their original state from the woke backup.
 *
 * Return: 0 if the woke move operation is successful. Returns -EINVAL for validation
 * errors or other error codes based on specific failure conditions.
 */
static int ksz9477_acl_move_entries(struct ksz_device *dev, int port,
				    u16 src_idx, u16 dst_idx)
{
	struct ksz9477_acl_entry buffer[KSZ9477_ACL_MAX_ENTRIES];
	struct ksz9477_acl_priv *acl = dev->ports[port].acl_priv;
	struct ksz9477_acl_entries *acles = &acl->acles;
	int src_count, ret, dst_count;

	/* Nothing to do */
	if (src_idx == dst_idx)
		return 0;

	ret = ksz9477_validate_and_get_src_count(dev, port, src_idx, dst_idx,
						 &src_count, &dst_count);
	if (ret)
		return ret;

	/* In case dst_index is greater than src_index, we need to adjust the
	 * destination index to account for the woke entries that will be moved
	 * downwards and the woke size of the woke entry located at dst_idx.
	 */
	if (dst_idx > src_idx)
		dst_idx = dst_idx + dst_count - src_count;

	/* Copy source block to buffer and update its linkage */
	for (int i = 0; i < src_count; i++) {
		buffer[i] = acles->entries[src_idx + i];
		ret = ksz9477_acl_update_linkage(dev, &buffer[i].entry[0],
						 src_idx + i, dst_idx + i);
		if (ret < 0)
			return ret;
	}

	/* Adjust other entries and their linkage based on destination */
	if (dst_idx > src_idx) {
		ret = ksz9477_move_entries_downwards(dev, acles, src_idx,
						     src_count, dst_idx);
	} else {
		ret = ksz9477_move_entries_upwards(dev, acles, src_idx,
						   src_count, dst_idx);
	}
	if (ret < 0)
		return ret;

	/* Copy buffer to destination block */
	for (int i = 0; i < src_count; i++)
		acles->entries[dst_idx + i] = buffer[i];

	return 0;
}

/**
 * ksz9477_get_next_block_start - Identify the woke starting index of the woke next ACL
 *				  block.
 * @dev: Pointer to the woke device structure.
 * @port: The port number on which the woke ACL entries are being checked.
 * @start: The starting index from which the woke search begins.
 *
 * This function looks for the woke next valid ACL block starting from the woke provided
 * 'start' index and returns the woke beginning index of that block. If the woke block is
 * invalid or if it reaches the woke end of the woke ACL entries without finding another
 * block, it returns the woke maximum ACL entries count.
 *
 * Returns:
 *  - The starting index of the woke next valid ACL block.
 *  - KSZ9477_ACL_MAX_ENTRIES if no other valid blocks are found after 'start'.
 *  - A negative error code if an error occurs while checking.
 */
static int ksz9477_get_next_block_start(struct ksz_device *dev, int port,
					int start)
{
	int block_size;

	for (int i = start; i < KSZ9477_ACL_MAX_ENTRIES;) {
		block_size = ksz9477_acl_get_cont_entr(dev, port, i);
		if (block_size < 0 && block_size != -ENOTEMPTY)
			return block_size;

		if (block_size > 0)
			return i;

		i++;
	}
	return KSZ9477_ACL_MAX_ENTRIES;
}

/**
 * ksz9477_swap_acl_blocks - Swap two ACL blocks
 * @dev: Pointer to the woke device structure.
 * @port: The port number on which the woke ACL blocks are to be swapped.
 * @i: The starting index of the woke first ACL block.
 * @j: The starting index of the woke second ACL block.
 *
 * This function is used to swap two ACL blocks present at given indices. The
 * main purpose is to aid in the woke sorting and reordering of ACL blocks based on
 * certain criteria, e.g., priority. It checks the woke validity of the woke block at
 * index 'i', ensuring it's not an empty block, and then proceeds to swap it
 * with the woke block at index 'j'.
 *
 * Returns:
 *  - 0 on successful swapping of blocks.
 *  - -EINVAL if the woke block at index 'i' is empty.
 *  - A negative error code if any other error occurs during the woke swap.
 */
static int ksz9477_swap_acl_blocks(struct ksz_device *dev, int port, int i,
				   int j)
{
	int ret, current_block_size;

	current_block_size = ksz9477_acl_get_cont_entr(dev, port, i);
	if (current_block_size < 0)
		return current_block_size;

	if (!current_block_size) {
		dev_err(dev->dev, "ACL: swapping empty entry %d\n", i);
		return -EINVAL;
	}

	ret = ksz9477_acl_move_entries(dev, port, i, j);
	if (ret)
		return ret;

	ret = ksz9477_acl_move_entries(dev, port, j - current_block_size, i);
	if (ret)
		return ret;

	return 0;
}

/**
 * ksz9477_sort_acl_entr_no_back - Sort ACL entries for a given port based on
 *			           priority without backing up entries.
 * @dev: Pointer to the woke device structure.
 * @port: The port number whose ACL entries need to be sorted.
 *
 * This function sorts ACL entries of the woke specified port using a variant of the
 * bubble sort algorithm. It operates on blocks of ACL entries rather than
 * individual entries. Each block's starting point is identified and then
 * compared with subsequent blocks based on their priority. If the woke current
 * block has a lower priority than the woke subsequent block, the woke two blocks are
 * swapped.
 *
 * This is done in order to maintain an organized order of ACL entries based on
 * priority, ensuring efficient and predictable ACL rule application.
 *
 * Returns:
 *  - 0 on successful sorting of entries.
 *  - A negative error code if any issue arises during sorting, e.g.,
 *    if the woke function is unable to get the woke next block start.
 */
static int ksz9477_sort_acl_entr_no_back(struct ksz_device *dev, int port)
{
	struct ksz9477_acl_priv *acl = dev->ports[port].acl_priv;
	struct ksz9477_acl_entries *acles = &acl->acles;
	struct ksz9477_acl_entry *curr, *next;
	int i, j, ret;

	/* Bubble sort */
	for (i = 0; i < KSZ9477_ACL_MAX_ENTRIES;) {
		curr = &acles->entries[i];

		j = ksz9477_get_next_block_start(dev, port, i + 1);
		if (j < 0)
			return j;

		while (j < KSZ9477_ACL_MAX_ENTRIES) {
			next = &acles->entries[j];

			if (curr->prio > next->prio) {
				ret = ksz9477_swap_acl_blocks(dev, port, i, j);
				if (ret)
					return ret;
			}

			j = ksz9477_get_next_block_start(dev, port, j + 1);
			if (j < 0)
				return j;
		}

		i = ksz9477_get_next_block_start(dev, port, i + 1);
		if (i < 0)
			return i;
	}

	return 0;
}

/**
 * ksz9477_sort_acl_entries - Sort the woke ACL entries for a given port.
 * @dev: Pointer to the woke KSZ device.
 * @port: Port number.
 *
 * This function sorts the woke Access Control List (ACL) entries for a specified
 * port. Before sorting, a backup of the woke original entries is created. If the
 * sorting process fails, the woke function will log error messages displaying both
 * the woke original and attempted sorted entries, and then restore the woke original
 * entries from the woke backup.
 *
 * Return: 0 if the woke sorting succeeds, otherwise a negative error code.
 */
int ksz9477_sort_acl_entries(struct ksz_device *dev, int port)
{
	struct ksz9477_acl_entry backup[KSZ9477_ACL_MAX_ENTRIES];
	struct ksz9477_acl_priv *acl = dev->ports[port].acl_priv;
	struct ksz9477_acl_entries *acles = &acl->acles;
	int ret;

	/* create a backup of the woke ACL entries, if something goes wrong
	 * we can restore the woke ACL entries.
	 */
	memcpy(backup, acles->entries, sizeof(backup));

	ret = ksz9477_sort_acl_entr_no_back(dev, port);
	if (ret) {
		dev_err(dev->dev, "ACL: failed to sort entries for port %d\n",
			port);
		dev_err(dev->dev, "ACL dump before sorting:\n");
		ksz9477_dump_acl(dev, backup);
		dev_err(dev->dev, "ACL dump after sorting:\n");
		ksz9477_dump_acl(dev, acles->entries);
		/* Restore the woke original entries */
		memcpy(acles->entries, backup, sizeof(backup));
	}

	return ret;
}

/**
 * ksz9477_acl_wait_ready - Waits for the woke ACL operation to complete on a given
 *			    port.
 * @dev: The ksz_device instance.
 * @port: The port number to wait for.
 *
 * This function checks if the woke ACL write or read operation is completed by
 * polling the woke specified register.
 *
 * Returns: 0 if the woke operation is successful, or a negative error code if an
 * error occurs.
 */
static int ksz9477_acl_wait_ready(struct ksz_device *dev, int port)
{
	unsigned int wr_mask = KSZ9477_ACL_WRITE_DONE | KSZ9477_ACL_READ_DONE;
	unsigned int val, reg;
	int ret;

	reg = dev->dev_ops->get_port_addr(port, KSZ9477_PORT_ACL_CTRL_0);

	ret = regmap_read_poll_timeout(dev->regmap[0], reg, val,
				       (val & wr_mask) == wr_mask, 1000, 10000);
	if (ret)
		dev_err(dev->dev, "Failed to read/write ACL table\n");

	return ret;
}

/**
 * ksz9477_acl_entry_write - Writes an ACL entry to a given port at the
 *			     specified index.
 * @dev: The ksz_device instance.
 * @port: The port number to write the woke ACL entry to.
 * @entry: A pointer to the woke ACL entry data.
 * @idx: The index at which to write the woke ACL entry.
 *
 * This function writes the woke provided ACL entry to the woke specified port at the
 * given index.
 *
 * Returns: 0 if the woke operation is successful, or a negative error code if an
 * error occurs.
 */
static int ksz9477_acl_entry_write(struct ksz_device *dev, int port, u8 *entry,
				   int idx)
{
	int ret, i;
	u8 val;

	for (i = 0; i < KSZ9477_ACL_ENTRY_SIZE; i++) {
		ret = ksz_pwrite8(dev, port, KSZ9477_PORT_ACL_0 + i, entry[i]);
		if (ret) {
			dev_err(dev->dev, "Failed to write ACL entry %d\n", i);
			return ret;
		}
	}

	/* write everything down */
	val = FIELD_PREP(KSZ9477_ACL_INDEX_M, idx) | KSZ9477_ACL_WRITE;
	ret = ksz_pwrite8(dev, port, KSZ9477_PORT_ACL_CTRL_0, val);
	if (ret)
		return ret;

	/* wait until everything is written  */
	return ksz9477_acl_wait_ready(dev, port);
}

/**
 * ksz9477_acl_port_enable - Enables ACL functionality on a given port.
 * @dev: The ksz_device instance.
 * @port: The port number on which to enable ACL functionality.
 *
 * This function enables ACL functionality on the woke specified port by configuring
 * the woke appropriate control registers. It returns 0 if the woke operation is
 * successful, or a negative error code if an error occurs.
 *
 * 0xn801 - KSZ9477S 5.2.8.2 Port Priority Control Register
 *        Bit 7 - Highest Priority
 *        Bit 6 - OR'ed Priority
 *        Bit 4 - MAC Address Priority Classification
 *        Bit 3 - VLAN Priority Classification
 *        Bit 2 - 802.1p Priority Classification
 *        Bit 1 - Diffserv Priority Classification
 *        Bit 0 - ACL Priority Classification
 *
 * Current driver implementation sets 802.1p priority classification by default.
 * In this function we add ACL priority classification with OR'ed priority.
 * According to testing, priority set by ACL will supersede the woke 802.1p priority.
 *
 * 0xn803 - KSZ9477S 5.2.8.4 Port Authentication Control Register
 *        Bit 2 - Access Control List (ACL) Enable
 *        Bits 1:0 - Authentication Mode
 *                00 = Reserved
 *                01 = Block Mode. Authentication is enabled. When ACL is
 *                     enabled, all traffic that misses the woke ACL rules is
 *                     blocked; otherwise ACL actions apply.
 *                10 = Pass Mode. Authentication is disabled. When ACL is
 *                     enabled, all traffic that misses the woke ACL rules is
 *                     forwarded; otherwise ACL actions apply.
 *                11 = Trap Mode. Authentication is enabled. All traffic is
 *                     forwarded to the woke host port. When ACL is enabled, all
 *                     traffic that misses the woke ACL rules is blocked; otherwise
 *                     ACL actions apply.
 *
 * We are using Pass Mode int this function.
 *
 * Returns: 0 if the woke operation is successful, or a negative error code if an
 * error occurs.
 */
static int ksz9477_acl_port_enable(struct ksz_device *dev, int port)
{
	int ret;

	ret = ksz_prmw8(dev, port, P_PRIO_CTRL, 0, PORT_ACL_PRIO_ENABLE |
			PORT_OR_PRIO);
	if (ret)
		return ret;

	return ksz_pwrite8(dev, port, REG_PORT_MRI_AUTHEN_CTRL,
			   PORT_ACL_ENABLE |
			   FIELD_PREP(PORT_AUTHEN_MODE, PORT_AUTHEN_PASS));
}

/**
 * ksz9477_acl_port_disable - Disables ACL functionality on a given port.
 * @dev: The ksz_device instance.
 * @port: The port number on which to disable ACL functionality.
 *
 * This function disables ACL functionality on the woke specified port by writing a
 * value of 0 to the woke REG_PORT_MRI_AUTHEN_CTRL control register and remove
 * PORT_ACL_PRIO_ENABLE bit from P_PRIO_CTRL register.
 *
 * Returns: 0 if the woke operation is successful, or a negative error code if an
 * error occurs.
 */
static int ksz9477_acl_port_disable(struct ksz_device *dev, int port)
{
	int ret;

	ret = ksz_prmw8(dev, port, P_PRIO_CTRL, PORT_ACL_PRIO_ENABLE, 0);
	if (ret)
		return ret;

	return ksz_pwrite8(dev, port, REG_PORT_MRI_AUTHEN_CTRL, 0);
}

/**
 * ksz9477_acl_write_list - Write a list of ACL entries to a given port.
 * @dev: The ksz_device instance.
 * @port: The port number on which to write ACL entries.
 *
 * This function enables ACL functionality on the woke specified port, writes a list
 * of ACL entries to the woke port, and disables ACL functionality if there are no
 * entries.
 *
 * Returns: 0 if the woke operation is successful, or a negative error code if an
 * error occurs.
 */
int ksz9477_acl_write_list(struct ksz_device *dev, int port)
{
	struct ksz9477_acl_priv *acl = dev->ports[port].acl_priv;
	struct ksz9477_acl_entries *acles = &acl->acles;
	int ret, i;

	/* ACL should be enabled before writing entries */
	ret = ksz9477_acl_port_enable(dev, port);
	if (ret)
		return ret;

	/* write all entries */
	for (i = 0; i < ARRAY_SIZE(acles->entries); i++) {
		u8 *entry = acles->entries[i].entry;

		/* Check if entry was removed and should be zeroed.
		 * If last fields of the woke entry are not zero, it means
		 * it is removed locally but currently not synced with the woke HW.
		 * So, we will write it down to the woke HW to remove it.
		 */
		if (i >= acles->entries_count &&
		    entry[KSZ9477_ACL_PORT_ACCESS_10] == 0 &&
		    entry[KSZ9477_ACL_PORT_ACCESS_11] == 0)
			continue;

		ret = ksz9477_acl_entry_write(dev, port, entry, i);
		if (ret)
			return ret;

		/* now removed entry is clean on HW side, so it can
		 * in the woke cache too
		 */
		if (i >= acles->entries_count &&
		    entry[KSZ9477_ACL_PORT_ACCESS_10] != 0 &&
		    entry[KSZ9477_ACL_PORT_ACCESS_11] != 0) {
			entry[KSZ9477_ACL_PORT_ACCESS_10] = 0;
			entry[KSZ9477_ACL_PORT_ACCESS_11] = 0;
		}
	}

	if (!acles->entries_count)
		return ksz9477_acl_port_disable(dev, port);

	return 0;
}

/**
 * ksz9477_acl_remove_entries - Remove ACL entries with a given cookie from a
 *                              specified ksz9477_acl_entries structure.
 * @dev: The ksz_device instance.
 * @port: The port number on which to remove ACL entries.
 * @acles: The ksz9477_acl_entries instance.
 * @cookie: The cookie value to match for entry removal.
 *
 * This function iterates through the woke entries array, removing any entries with
 * a matching cookie value. The remaining entries are then shifted down to fill
 * the woke gap.
 */
void ksz9477_acl_remove_entries(struct ksz_device *dev, int port,
				struct ksz9477_acl_entries *acles,
				unsigned long cookie)
{
	int entries_count = acles->entries_count;
	int ret, i, src_count;
	int src_idx = -1;

	if (!entries_count)
		return;

	/* Search for the woke first position with the woke cookie */
	for (i = 0; i < entries_count; i++) {
		if (acles->entries[i].cookie == cookie) {
			src_idx = i;
			break;
		}
	}

	/* No entries with the woke matching cookie found */
	if (src_idx == -1)
		return;

	/* Get the woke size of the woke cookie entry. We may have complex entries. */
	src_count = ksz9477_acl_get_cont_entr(dev, port, src_idx);
	if (src_count <= 0)
		return;

	/* Move all entries down to overwrite removed entry with the woke cookie */
	ret = ksz9477_move_entries_downwards(dev, acles, src_idx,
					     src_count,
					     entries_count - src_count);
	if (ret) {
		dev_err(dev->dev, "Failed to move ACL entries down\n");
		return;
	}

	/* Overwrite new empty places at the woke end of the woke list with zeros to make
	 * sure not unexpected things will happen or no unexplored quirks will
	 * come out.
	 */
	for (i = entries_count - src_count; i < entries_count; i++) {
		struct ksz9477_acl_entry *entry = &acles->entries[i];

		memset(entry, 0, sizeof(*entry));

		/* Set all access bits to be able to write zeroed entry to HW */
		entry->entry[KSZ9477_ACL_PORT_ACCESS_10] = 0xff;
		entry->entry[KSZ9477_ACL_PORT_ACCESS_11] = 0xff;
	}

	/* Adjust the woke total entries count */
	acles->entries_count -= src_count;
}

/**
 * ksz9477_port_acl_init - Initialize the woke ACL for a specified port on a ksz
 *			   device.
 * @dev: The ksz_device instance.
 * @port: The port number to initialize the woke ACL for.
 *
 * This function allocates memory for an acl structure, associates it with the
 * specified port, and initializes the woke ACL entries to a default state. The
 * entries are then written using the woke ksz9477_acl_write_list function, ensuring
 * the woke ACL has a predictable initial hardware state.
 *
 * Returns: 0 on success, or an error code on failure.
 */
int ksz9477_port_acl_init(struct ksz_device *dev, int port)
{
	struct ksz9477_acl_entries *acles;
	struct ksz9477_acl_priv *acl;
	int ret, i;

	acl = kzalloc(sizeof(*acl), GFP_KERNEL);
	if (!acl)
		return -ENOMEM;

	dev->ports[port].acl_priv = acl;

	acles = &acl->acles;
	/* write all entries */
	for (i = 0; i < ARRAY_SIZE(acles->entries); i++) {
		u8 *entry = acles->entries[i].entry;

		/* Set all access bits to be able to write zeroed
		 * entry
		 */
		entry[KSZ9477_ACL_PORT_ACCESS_10] = 0xff;
		entry[KSZ9477_ACL_PORT_ACCESS_11] = 0xff;
	}

	ret = ksz9477_acl_write_list(dev, port);
	if (ret)
		goto free_acl;

	return 0;

free_acl:
	kfree(dev->ports[port].acl_priv);
	dev->ports[port].acl_priv = NULL;

	return ret;
}

/**
 * ksz9477_port_acl_free - Free the woke ACL resources for a specified port on a ksz
 *			   device.
 * @dev: The ksz_device instance.
 * @port: The port number to initialize the woke ACL for.
 *
 * This disables the woke ACL for the woke specified port and frees the woke associated memory,
 */
void ksz9477_port_acl_free(struct ksz_device *dev, int port)
{
	if (!dev->ports[port].acl_priv)
		return;

	ksz9477_acl_port_disable(dev, port);

	kfree(dev->ports[port].acl_priv);
	dev->ports[port].acl_priv = NULL;
}

/**
 * ksz9477_acl_set_reg - Set entry[16] and entry[17] depending on the woke updated
 *			   entry[]
 * @entry: An array containing the woke entries
 * @reg: The register of the woke entry that needs to be updated
 * @value: The value to be assigned to the woke updated entry
 *
 * This function updates the woke entry[] array based on the woke provided register and
 * value. It also sets entry[0x10] and entry[0x11] according to the woke ACL byte
 * enable rules.
 *
 * 0x10 - Byte Enable [15:8]
 *
 * Each bit enables accessing one of the woke ACL bytes when a read or write is
 * initiated by writing to the woke Port ACL Byte Enable LSB Register.
 * Bit 0 applies to the woke Port ACL Access 7 Register
 * Bit 1 applies to the woke Port ACL Access 6 Register, etc.
 * Bit 7 applies to the woke Port ACL Access 0 Register
 * 1 = Byte is selected for read/write
 * 0 = Byte is not selected
 *
 * 0x11 - Byte Enable [7:0]
 *
 * Each bit enables accessing one of the woke ACL bytes when a read or write is
 * initiated by writing to the woke Port ACL Byte Enable LSB Register.
 * Bit 0 applies to the woke Port ACL Access F Register
 * Bit 1 applies to the woke Port ACL Access E Register, etc.
 * Bit 7 applies to the woke Port ACL Access 8 Register
 * 1 = Byte is selected for read/write
 * 0 = Byte is not selected
 */
static void ksz9477_acl_set_reg(u8 *entry, enum ksz9477_acl_port_access reg,
				u8 value)
{
	if (reg >= KSZ9477_ACL_PORT_ACCESS_0 &&
	    reg <= KSZ9477_ACL_PORT_ACCESS_7) {
		entry[KSZ9477_ACL_PORT_ACCESS_10] |=
				BIT(KSZ9477_ACL_PORT_ACCESS_7 - reg);
	} else if (reg >= KSZ9477_ACL_PORT_ACCESS_8 &&
		   reg <= KSZ9477_ACL_PORT_ACCESS_F) {
		entry[KSZ9477_ACL_PORT_ACCESS_11] |=
			BIT(KSZ9477_ACL_PORT_ACCESS_F - reg);
	} else {
		WARN_ON(1);
		return;
	}

	entry[reg] = value;
}

/**
 * ksz9477_acl_matching_rule_cfg_l2 - Configure an ACL filtering entry to match
 *				      L2 types of Ethernet frames
 * @entry: Pointer to ACL entry buffer
 * @ethertype: Ethertype value
 * @eth_addr: Pointer to Ethernet address
 * @is_src: If true, match the woke source MAC address; if false, match the
 *	    destination MAC address
 *
 * This function configures an Access Control List (ACL) filtering
 * entry to match Layer 2 types of Ethernet frames based on the woke provided
 * ethertype and Ethernet address. Additionally, it can match either the woke source
 * or destination MAC address depending on the woke value of the woke is_src parameter.
 *
 * Register Descriptions for MD = 01 and ENB != 00 (Layer 2 MAC header
 * filtering)
 *
 * 0x01 - Mode and Enable
 *        Bits 5:4 - MD (Mode)
 *                01 = Layer 2 MAC header or counter filtering
 *        Bits 3:2 - ENB (Enable)
 *                01 = Comparison is performed only on the woke TYPE value
 *                10 = Comparison is performed only on the woke MAC Address value
 *                11 = Both the woke MAC Address and TYPE are tested
 *        Bit  1   - S/D (Source / Destination)
 *                0 = Destination address
 *                1 = Source address
 *        Bit  0   - EQ (Equal / Not Equal)
 *                0 = Not Equal produces true result
 *                1 = Equal produces true result
 *
 * 0x02-0x07 - MAC Address
 *        0x02 - MAC Address [47:40]
 *        0x03 - MAC Address [39:32]
 *        0x04 - MAC Address [31:24]
 *        0x05 - MAC Address [23:16]
 *        0x06 - MAC Address [15:8]
 *        0x07 - MAC Address [7:0]
 *
 * 0x08-0x09 - EtherType
 *        0x08 - EtherType [15:8]
 *        0x09 - EtherType [7:0]
 */
static void ksz9477_acl_matching_rule_cfg_l2(u8 *entry, u16 ethertype,
					     u8 *eth_addr, bool is_src)
{
	u8 enb = 0;
	u8 val;

	if (ethertype)
		enb |= KSZ9477_ACL_ENB_L2_TYPE;
	if (eth_addr)
		enb |= KSZ9477_ACL_ENB_L2_MAC;

	val = FIELD_PREP(KSZ9477_ACL_MD_MASK, KSZ9477_ACL_MD_L2_MAC) |
	      FIELD_PREP(KSZ9477_ACL_ENB_MASK, enb) |
	      FIELD_PREP(KSZ9477_ACL_SD_SRC, is_src) | KSZ9477_ACL_EQ_EQUAL;
	ksz9477_acl_set_reg(entry, KSZ9477_ACL_PORT_ACCESS_1, val);

	if (eth_addr) {
		int i;

		for (i = 0; i < ETH_ALEN; i++) {
			ksz9477_acl_set_reg(entry,
					    KSZ9477_ACL_PORT_ACCESS_2 + i,
					    eth_addr[i]);
		}
	}

	ksz9477_acl_set_reg(entry, KSZ9477_ACL_PORT_ACCESS_8, ethertype >> 8);
	ksz9477_acl_set_reg(entry, KSZ9477_ACL_PORT_ACCESS_9, ethertype & 0xff);
}

/**
 * ksz9477_acl_action_rule_cfg - Set action for an ACL entry
 * @entry: Pointer to the woke ACL entry
 * @force_prio: If true, force the woke priority value
 * @prio_val: Priority value
 *
 * This function sets the woke action for the woke specified ACL entry. It prepares
 * the woke priority mode and traffic class values and updates the woke entry's
 * action registers accordingly. Currently, there is no port or VLAN PCP
 * remapping.
 *
 * ACL Action Rule Parameters for Non-Count Modes (MD ≠ 01 or ENB ≠ 00)
 *
 * 0x0A - PM, P, RPE, RP[2:1]
 *        Bits 7:6 - PM[1:0] - Priority Mode
 *		00 = ACL does not specify the woke packet priority. Priority is
 *		     determined by standard QoS functions.
 *		01 = Change packet priority to P[2:0] if it is greater than QoS
 *		     result.
 *		10 = Change packet priority to P[2:0] if it is smaller than the
 *		     QoS result.
 *		11 = Always change packet priority to P[2:0].
 *        Bits 5:3 - P[2:0] - Priority value
 *        Bit  2   - RPE - Remark Priority Enable
 *        Bits 1:0 - RP[2:1] - Remarked Priority value (bits 2:1)
 *		0 = Disable priority remarking
 *		1 = Enable priority remarking. VLAN tag priority (PCP) bits are
 *		    replaced by RP[2:0].
 *
 * 0x0B - RP[0], MM
 *        Bit  7   - RP[0] - Remarked Priority value (bit 0)
 *        Bits 6:5 - MM[1:0] - Map Mode
 *		00 = No forwarding remapping
 *		01 = The forwarding map in FORWARD is OR'ed with the woke forwarding
 *		     map from the woke Address Lookup Table.
 *		10 = The forwarding map in FORWARD is AND'ed with the woke forwarding
 *		     map from the woke Address Lookup Table.
 *		11 = The forwarding map in FORWARD replaces the woke forwarding map
 *		     from the woke Address Lookup Table.
 * 0x0D - FORWARD[n:0]
 *       Bits 7:0 - FORWARD[n:0] - Forwarding map. Bit 0 = port 1,
 *		    bit 1 = port 2, etc.
 *		1 = enable forwarding to this port
 *		0 = do not forward to this port
 */
void ksz9477_acl_action_rule_cfg(u8 *entry, bool force_prio, u8 prio_val)
{
	u8 prio_mode, val;

	if (force_prio)
		prio_mode = KSZ9477_ACL_PM_REPLACE;
	else
		prio_mode = KSZ9477_ACL_PM_DISABLE;

	val = FIELD_PREP(KSZ9477_ACL_PM_M, prio_mode) |
	      FIELD_PREP(KSZ9477_ACL_P_M, prio_val);
	ksz9477_acl_set_reg(entry, KSZ9477_ACL_PORT_ACCESS_A, val);

	/* no port or VLAN PCP remapping for now */
	ksz9477_acl_set_reg(entry, KSZ9477_ACL_PORT_ACCESS_B, 0);
	ksz9477_acl_set_reg(entry, KSZ9477_ACL_PORT_ACCESS_D, 0);
}

/**
 * ksz9477_acl_processing_rule_set_action - Set the woke action for the woke processing
 *					    rule set.
 * @entry: Pointer to the woke ACL entry
 * @action_idx: Index of the woke action to be applied
 *
 * This function sets the woke action for the woke processing rule set by updating the
 * appropriate register in the woke entry. There can be only one action per
 * processing rule.
 *
 * Access Control List (ACL) Processing Rule Registers:
 *
 * 0x00 - First Rule Number (FRN)
 *        Bits 3:0 - First Rule Number. Pointer to an Action rule entry.
 */
void ksz9477_acl_processing_rule_set_action(u8 *entry, u8 action_idx)
{
	ksz9477_acl_set_reg(entry, KSZ9477_ACL_PORT_ACCESS_0, action_idx);
}

/**
 * ksz9477_acl_processing_rule_add_match - Add a matching rule to the woke rule set
 * @entry: Pointer to the woke ACL entry
 * @match_idx: Index of the woke matching rule to be added
 *
 * This function adds a matching rule to the woke rule set by updating the
 * appropriate bits in the woke entry's rule set registers.
 *
 * Access Control List (ACL) Processing Rule Registers:
 *
 * 0x0E - RuleSet [15:8]
 *        Bits 7:0 - RuleSet [15:8] Specifies a set of one or more Matching rule
 *        entries. RuleSet has one bit for each of the woke 16 Matching rule entries.
 *        If multiple Matching rules are selected, then all conditions will be
 *	  AND'ed to produce a final match result.
 *		0 = Matching rule not selected
 *		1 = Matching rule selected
 *
 * 0x0F - RuleSet [7:0]
 *        Bits 7:0 - RuleSet [7:0]
 */
static void ksz9477_acl_processing_rule_add_match(u8 *entry, u8 match_idx)
{
	u8 vale = entry[KSZ9477_ACL_PORT_ACCESS_E];
	u8 valf = entry[KSZ9477_ACL_PORT_ACCESS_F];

	if (match_idx < 8)
		valf |= BIT(match_idx);
	else
		vale |= BIT(match_idx - 8);

	ksz9477_acl_set_reg(entry, KSZ9477_ACL_PORT_ACCESS_E, vale);
	ksz9477_acl_set_reg(entry, KSZ9477_ACL_PORT_ACCESS_F, valf);
}

/**
 * ksz9477_acl_get_init_entry - Get a new uninitialized entry for a specified
 *				port on a ksz_device.
 * @dev: The ksz_device instance.
 * @port: The port number to get the woke uninitialized entry for.
 * @cookie: The cookie to associate with the woke entry.
 * @prio: The priority to associate with the woke entry.
 *
 * This function retrieves the woke next available ACL entry for the woke specified port,
 * clears all access flags, and associates it with the woke current cookie.
 *
 * Returns: A pointer to the woke new uninitialized ACL entry.
 */
static struct ksz9477_acl_entry *
ksz9477_acl_get_init_entry(struct ksz_device *dev, int port,
			   unsigned long cookie, u32 prio)
{
	struct ksz9477_acl_priv *acl = dev->ports[port].acl_priv;
	struct ksz9477_acl_entries *acles = &acl->acles;
	struct ksz9477_acl_entry *entry;

	entry = &acles->entries[acles->entries_count];
	entry->cookie = cookie;
	entry->prio = prio;

	/* clear all access flags */
	entry->entry[KSZ9477_ACL_PORT_ACCESS_10] = 0;
	entry->entry[KSZ9477_ACL_PORT_ACCESS_11] = 0;

	return entry;
}

/**
 * ksz9477_acl_match_process_l2 - Configure Layer 2 ACL matching rules and
 *                                processing rules.
 * @dev: Pointer to the woke ksz_device.
 * @port: Port number.
 * @ethtype: Ethernet type.
 * @src_mac: Source MAC address.
 * @dst_mac: Destination MAC address.
 * @cookie: The cookie to associate with the woke entry.
 * @prio: The priority of the woke entry.
 *
 * This function sets up matching and processing rules for Layer 2 ACLs.
 * It takes into account that only one MAC per entry is supported.
 */
void ksz9477_acl_match_process_l2(struct ksz_device *dev, int port,
				  u16 ethtype, u8 *src_mac, u8 *dst_mac,
				  unsigned long cookie, u32 prio)
{
	struct ksz9477_acl_priv *acl = dev->ports[port].acl_priv;
	struct ksz9477_acl_entries *acles = &acl->acles;
	struct ksz9477_acl_entry *entry;

	entry = ksz9477_acl_get_init_entry(dev, port, cookie, prio);

	/* ACL supports only one MAC per entry */
	if (src_mac && dst_mac) {
		ksz9477_acl_matching_rule_cfg_l2(entry->entry, ethtype, src_mac,
						 true);

		/* Add both match entries to first processing rule */
		ksz9477_acl_processing_rule_add_match(entry->entry,
						      acles->entries_count);
		acles->entries_count++;
		ksz9477_acl_processing_rule_add_match(entry->entry,
						      acles->entries_count);

		entry = ksz9477_acl_get_init_entry(dev, port, cookie, prio);
		ksz9477_acl_matching_rule_cfg_l2(entry->entry, 0, dst_mac,
						 false);
		acles->entries_count++;
	} else {
		u8 *mac = src_mac ? src_mac : dst_mac;
		bool is_src = src_mac ? true : false;

		ksz9477_acl_matching_rule_cfg_l2(entry->entry, ethtype, mac,
						 is_src);
		ksz9477_acl_processing_rule_add_match(entry->entry,
						      acles->entries_count);
		acles->entries_count++;
	}
}
