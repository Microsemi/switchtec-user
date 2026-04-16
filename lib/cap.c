/*
 * Microsemi Switchtec(tm) PCIe Management Library
 * Copyright (c) 2017, Microsemi Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */

/**
 * @file
 * @brief Switchtec library functions for PCI capability access
 */

#define SWITCHTEC_LIB_CORE

#include "switchtec/cap.h"
#include "switchtec/switchtec.h"
#include "switchtec/mrpc.h"
#include "switchtec_priv.h"

#include <stddef.h>
#include <errno.h>
#include <string.h>

/**
 * @defgroup Capability PCI Capability Access
 * @brief Access PCI capabilities through GAS
 *
 * These functions provide access to PCI capabilities in the switch's
 * Global Address Space. The multicast capability allows configuring
 * PCIe multicast functionality.
 *
 * @{
 */

struct gas_cap_read_cmd {
	uint32_t offset;
	uint32_t len;
};

struct gas_cap_write_cmd {
	uint32_t offset;
	uint32_t len;
	uint8_t data[];
};

/**
 * @brief Read the multicast capability registers
 * @param[in]  dev	Switchtec device handle
 * @param[in]  gas_base	GAS base address of the port
 * @param[out] cap	Multicast capability structure to fill
 * @return 0 on success, negative on error
 */
int switchtec_multicast_cap_get(struct switchtec_dev *dev, uint32_t gas_base,
				struct switchtec_multicast_cap *cap)
{
	struct gas_cap_read_cmd cmd;
	int ret;

	if (!dev || !cap)
		return -EINVAL;

	if (switchtec_boot_phase(dev) != SWITCHTEC_BOOT_PHASE_FW) {
		errno = ENODEV;
		return -1;
	}

	cmd.offset = htole32(gas_base + SWITCHTEC_CAP_MULTICAST_OFFSET);
	cmd.len = htole32(sizeof(*cap));

	ret = switchtec_cmd(dev, MRPC_GAS_READ, &cmd, sizeof(cmd),
			    cap, sizeof(*cap));
	if (ret)
		return ret;

	cap->header = le32toh(cap->header);
	cap->cap_ctrl = le32toh(cap->cap_ctrl);
	cap->mc_base_addr = le64toh(cap->mc_base_addr);
	cap->mc_receive = le64toh(cap->mc_receive);
	cap->mc_block_all = le64toh(cap->mc_block_all);
	cap->mc_block_untranslated = le64toh(cap->mc_block_untranslated);
	cap->mc_overlay_bar = le64toh(cap->mc_overlay_bar);

	return 0;
}

/**
 * @brief Set multicast capability registers
 * @param[in] dev	Switchtec device handle
 * @param[in] gas_base	GAS base address of the port
 * @param[in] set	Parameters specifying which fields to set
 * @return 0 on success, negative on error
 *
 * The set structure specifies which fields to modify:
 * - enable/disable: Set the MC Enable bit
 * - num_group: Set the number of multicast groups (0-63), -1 to skip
 * - set_base_addr/base_addr: Set the MC Base Address
 * - set_receive/receive: Set the MC Receive register
 * - set_block_all/block_all: Set the MC Block All register
 * - set_block_untranslated/block_untranslated: Set the MC Block Untranslated register
 */
int switchtec_multicast_cap_set(struct switchtec_dev *dev, uint32_t gas_base,
				struct switchtec_multicast_set *set)
{
	struct switchtec_multicast_cap cap;
	uint16_t new_ctrl;
	uint32_t addr;
	int ret;
	struct {
		uint32_t offset;
		uint32_t len;
		uint8_t data[8];
	} cmd;

	if (!dev || !set)
		return -EINVAL;

	if (switchtec_boot_phase(dev) != SWITCHTEC_BOOT_PHASE_FW) {
		errno = ENODEV;
		return -1;
	}

	ret = switchtec_multicast_cap_get(dev, gas_base, &cap);
	if (ret)
		return ret;

	addr = gas_base + SWITCHTEC_CAP_MULTICAST_OFFSET;

	/* Control register is in upper 16 bits of cap_ctrl (DW1) */
	new_ctrl = SWITCHTEC_MC_CTRL(cap.cap_ctrl);

	if (set->enable && !set->disable)
		new_ctrl |= (1 << 15);
	else if (set->disable)
		new_ctrl &= ~(1 << 15);

	if (set->num_group >= 0 && set->num_group <= 63)
		new_ctrl = (new_ctrl & ~0x3F) | (set->num_group & 0x3F);

	if (new_ctrl != SWITCHTEC_MC_CTRL(cap.cap_ctrl)) {
		/* Write to upper 16 bits of DW1 (offset +4 bytes, then +2 for upper half) */
		cmd.offset = htole32(addr +
			offsetof(struct switchtec_multicast_cap, cap_ctrl) + 2);
		cmd.len = htole32(2);
		*(uint16_t *)cmd.data = htole16(new_ctrl);
		ret = switchtec_cmd(dev, MRPC_GAS_WRITE, &cmd,
				    sizeof(cmd.offset) + sizeof(cmd.len) + 2,
				    NULL, 0);
		if (ret)
			return ret;
	}

	if (set->set_base_addr) {
		uint64_t new_base = set->base_addr;

		/* Preserve or update index_pos in lower bits */
		if (set->index_pos >= 0 && set->index_pos <= 63)
			new_base = (new_base & ~0x3FULL) | (set->index_pos & 0x3F);

		cmd.offset = htole32(addr +
			offsetof(struct switchtec_multicast_cap, mc_base_addr));
		cmd.len = htole32(8);
		*(uint64_t *)cmd.data = htole64(new_base);
		ret = switchtec_cmd(dev, MRPC_GAS_WRITE, &cmd,
				    sizeof(cmd.offset) + sizeof(cmd.len) + 8,
				    NULL, 0);
		if (ret)
			return ret;
	} else if (set->index_pos >= 0) {
		/* Update only index_pos without changing base_addr */
		uint64_t new_base = cap.mc_base_addr;

		if (set->index_pos >= 0 && set->index_pos <= 63)
			new_base = (new_base & ~0x3FULL) | (set->index_pos & 0x3F);

		cmd.offset = htole32(addr +
			offsetof(struct switchtec_multicast_cap, mc_base_addr));
		cmd.len = htole32(8);
		*(uint64_t *)cmd.data = htole64(new_base);
		ret = switchtec_cmd(dev, MRPC_GAS_WRITE, &cmd,
				    sizeof(cmd.offset) + sizeof(cmd.len) + 8,
				    NULL, 0);
		if (ret)
			return ret;
	}

	if (set->set_receive) {
		cmd.offset = htole32(addr +
			offsetof(struct switchtec_multicast_cap, mc_receive));
		cmd.len = htole32(8);
		*(uint64_t *)cmd.data = htole64(set->receive);
		ret = switchtec_cmd(dev, MRPC_GAS_WRITE, &cmd,
				    sizeof(cmd.offset) + sizeof(cmd.len) + 8,
				    NULL, 0);
		if (ret)
			return ret;
	}

	if (set->set_block_all) {
		cmd.offset = htole32(addr +
			offsetof(struct switchtec_multicast_cap, mc_block_all));
		cmd.len = htole32(8);
		*(uint64_t *)cmd.data = htole64(set->block_all);
		ret = switchtec_cmd(dev, MRPC_GAS_WRITE, &cmd,
				    sizeof(cmd.offset) + sizeof(cmd.len) + 8,
				    NULL, 0);
		if (ret)
			return ret;
	}

	if (set->set_block_untranslated) {
		cmd.offset = htole32(addr +
			offsetof(struct switchtec_multicast_cap, mc_block_untranslated));
		cmd.len = htole32(8);
		*(uint64_t *)cmd.data = htole64(set->block_untranslated);
		ret = switchtec_cmd(dev, MRPC_GAS_WRITE, &cmd,
				    sizeof(cmd.offset) + sizeof(cmd.len) + 8,
				    NULL, 0);
		if (ret)
			return ret;
	}

	return 0;
}

/**@}*/
