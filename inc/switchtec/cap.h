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

#ifndef LIBSWITCHTEC_CAP_H
#define LIBSWITCHTEC_CAP_H

/**
 * @file
 * @brief PCI Capability access functions
 */

#include <switchtec/switchtec.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SWITCHTEC_CAP_GAS_BASE		0x134000
#define SWITCHTEC_CAP_DEV_STRIDE	0x1000
#define SWITCHTEC_CAP_TYPE_OFFSET	0x0C
#define SWITCHTEC_CAP_MULTICAST_OFFSET	0x170

/**
 * @brief Multicast Extended Capability register structure (PCIe 3.1)
 *
 * DW0:     PCIe Extended Capability Header
 * DW1:     [15:0] MC Capability, [31:16] MC Control
 * DW2-3:   MC Base Address Register
 * DW4-5:   MC Receive Register
 * DW6-7:   MC Block All Register
 * DW8-9:   MC Block Untranslated Register
 * DW10-11: MC Overlay BAR
 */
struct switchtec_multicast_cap {
	uint32_t header;		/* DW0: Extended Capability Header */
	uint32_t cap_ctrl;		/* DW1: [15:0] cap, [31:16] ctrl */
	uint64_t mc_base_addr;		/* DW2-3: MC Base Address */
	uint64_t mc_receive;		/* DW4-5: MC Receive */
	uint64_t mc_block_all;		/* DW6-7: MC Block All */
	uint64_t mc_block_untranslated;	/* DW8-9: MC Block Untranslated */
	uint64_t mc_overlay_bar;	/* DW10-11: MC Overlay BAR */
};

/* DW0: Extended Capability Header fields */
#define SWITCHTEC_MC_HDR_CAP_ID(hdr)		(((hdr) >> 0) & 0xFFFF)
#define SWITCHTEC_MC_HDR_CAP_VER(hdr)		(((hdr) >> 16) & 0xF)
#define SWITCHTEC_MC_HDR_NEXT_CAP(hdr)		(((hdr) >> 20) & 0xFFF)

/* DW1: Multicast Capability Register [15:0] */
#define SWITCHTEC_MC_CAP(cap_ctrl)		(((cap_ctrl) >> 0) & 0xFFFF)
#define SWITCHTEC_MC_CAP_MAX_GROUP(cap_ctrl)	(((cap_ctrl) >> 0) & 0x3F)

/* DW1: Multicast Control Register [31:16] */
#define SWITCHTEC_MC_CTRL(cap_ctrl)		(((cap_ctrl) >> 16) & 0xFFFF)
#define SWITCHTEC_MC_CTRL_NUM_GROUP(cap_ctrl)	(((cap_ctrl) >> 16) & 0x3F)
#define SWITCHTEC_MC_CTRL_ENABLE(cap_ctrl)	(((cap_ctrl) >> 31) & 0x1)

/* DW2-3: MC Base Address Register fields */
#define SWITCHTEC_MC_BASE_INDEX_POS(base)	(((base) >> 0) & 0x3F)
#define SWITCHTEC_MC_BASE_ADDR(base)		((base) & 0xFFFFFFFFFFFFF000ULL)

/* DW10-11: MC Overlay BAR fields */
#define SWITCHTEC_MC_OVERLAY_SIZE(bar)		(((bar) >> 0) & 0x3F)
#define SWITCHTEC_MC_OVERLAY_ADDR(bar)		((bar) & 0xFFFFFFFFFFFFFFC0ULL)

/**
 * @brief Multicast capability set parameters
 */
struct switchtec_multicast_set {
	int enable;
	int disable;
	int num_group;
	int index_pos;
	uint64_t base_addr;
	uint64_t receive;
	uint64_t block_all;
	uint64_t block_untranslated;
	uint64_t overlay_bar;
	int overlay_size;
	int set_base_addr;
	int set_receive;
	int set_block_all;
	int set_block_untranslated;
	int set_overlay_bar;
};

enum switchtec_cap_port_type {
	SWITCHTEC_CAP_PORT_INVALID = -1,
	SWITCHTEC_CAP_PORT_DSP = 0,
	SWITCHTEC_CAP_PORT_USP = 1,
	SWITCHTEC_CAP_PORT_MGMT = 2,
};

struct switchtec_port_info {
	uint32_t gas_base;
	enum switchtec_cap_port_type port_type;
	const char *bdf;
};

int switchtec_multicast_cap_get(struct switchtec_dev *dev, uint32_t gas_base,
				struct switchtec_multicast_cap *cap);
int switchtec_multicast_cap_set(struct switchtec_dev *dev, uint32_t gas_base,
				struct switchtec_multicast_set *set);
int switchtec_find_port_by_bdf(struct switchtec_dev *dev, const char *target_bdf,
			       struct switchtec_port_info *info);

#ifdef __cplusplus
}
#endif

#endif
