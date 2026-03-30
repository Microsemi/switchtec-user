/*
 * Microsemi Switchtec(tm) PCIe Management Library
 * Copyright (c) 2019, Microsemi Corporation
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

#ifndef LIBSWITCHTEC_FW_COMMON_H
#define LIBSWITCHTEC_FW_COMMON_H

#include "switchtec_priv.h"
#include "switchtec/switchtec.h"
#include "switchtec/errors.h"
#include "switchtec/endian.h"
#include "switchtec/utils.h"
#include "switchtec/mfg.h"

#include <unistd.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

static inline struct switchtec_fw_part_type *switchtec_fw_type_ptr(struct switchtec_fw_part_summary *summary,
                                                     struct switchtec_fw_image_info *info)
{
	switch (info->type) {
	case SWITCHTEC_FW_TYPE_BOOT:	return &summary->boot;
	case SWITCHTEC_FW_TYPE_MAP:	return &summary->map;
	case SWITCHTEC_FW_TYPE_IMG:	return &summary->img;
	case SWITCHTEC_FW_TYPE_CFG:	return &summary->cfg;
	case SWITCHTEC_FW_TYPE_NVLOG:	return &summary->nvlog;
	case SWITCHTEC_FW_TYPE_SEEPROM: return &summary->seeprom;
	case SWITCHTEC_FW_TYPE_KEY:	return &summary->key;
	case SWITCHTEC_FW_TYPE_BL2:	return &summary->bl2;
	case SWITCHTEC_FW_TYPE_RIOT:	return &summary->riot;
	default:			return NULL;
	}
}

static inline enum switchtec_fw_type
switchtec_fw_id_to_type_gen6(const struct switchtec_fw_image_info *info)
{
	switch (info->part_id) {
	case SWITCHTEC_FW_PART_ID_G6_MAP0: return SWITCHTEC_FW_TYPE_MAP;
	case SWITCHTEC_FW_PART_ID_G6_MAP1: return SWITCHTEC_FW_TYPE_MAP;
	case SWITCHTEC_FW_PART_ID_G6_KEY0: return SWITCHTEC_FW_TYPE_KEY;
	case SWITCHTEC_FW_PART_ID_G6_KEY1: return SWITCHTEC_FW_TYPE_KEY;
	case SWITCHTEC_FW_PART_ID_G6_BL20: return SWITCHTEC_FW_TYPE_BL2;
	case SWITCHTEC_FW_PART_ID_G6_BL21: return SWITCHTEC_FW_TYPE_BL2;
	case SWITCHTEC_FW_PART_ID_G6_CFG0: return SWITCHTEC_FW_TYPE_CFG;
	case SWITCHTEC_FW_PART_ID_G6_CFG1: return SWITCHTEC_FW_TYPE_CFG;
	case SWITCHTEC_FW_PART_ID_G6_IMG0: return SWITCHTEC_FW_TYPE_IMG;
	case SWITCHTEC_FW_PART_ID_G6_IMG1: return SWITCHTEC_FW_TYPE_IMG;
	case SWITCHTEC_FW_PART_ID_G6_NVLOG: return SWITCHTEC_FW_TYPE_NVLOG;
	case SWITCHTEC_FW_PART_ID_G6_SEEPROM: return SWITCHTEC_FW_TYPE_SEEPROM;
	default: return SWITCHTEC_FW_TYPE_UNKNOWN;
	}
}

static long multicfg_subcmd(struct switchtec_dev *dev, uint32_t subcmd,
			    uint8_t index)
{
	int ret;
	uint32_t result;

	subcmd |= index << 8;
	subcmd = htole32(subcmd);

	ret = switchtec_cmd(dev, MRPC_MULTI_CFG, &subcmd, sizeof(subcmd),
			    &result, sizeof(result));
	if (ret)
		return -1;

	return result;
}

static inline int get_multicfg(struct switchtec_dev *dev,
		 struct switchtec_fw_image_info *info,
		 int *nr_mult)
{
	int ret;
	int i;

	ret = multicfg_subcmd(dev, MRPC_MULTI_CFG_SUPPORTED, 0);
	if (ret < 0)
		return ret;

	if (!ret) {
		*nr_mult = 0;
		return 0;
	}

	ret = multicfg_subcmd(dev, MRPC_MULTI_CFG_COUNT, 0);
	if (ret < 0)
		return ret;

	if (*nr_mult > ret)
		*nr_mult = ret;

	for (i = 0; i < *nr_mult; i++) {
		info[i].part_addr = multicfg_subcmd(dev,
						    MRPC_MULTI_CFG_START_ADDR,
						    i);
		info[i].part_len = multicfg_subcmd(dev,
						   MRPC_MULTI_CFG_LENGTH, i);
		strcpy(info[i].version, "");
		info[i].image_crc = 0;
		info[i].active = 0;
	}

	ret = multicfg_subcmd(dev, MRPC_MULTI_CFG_ACTIVE, 0);
	if (ret < 0)
		return ret;

	if (ret < *nr_mult)
		info[ret].active = 1;

	return 0;
}

static inline uint32_t get_fw_tx_id(struct switchtec_dev *dev)
{
	if (switchtec_is_gen6(dev))
		return MRPC_FW_TX_GEN6;
	if (switchtec_is_gen5(dev))
		return MRPC_FW_TX_GEN5;

	return MRPC_FW_TX;
}

#endif