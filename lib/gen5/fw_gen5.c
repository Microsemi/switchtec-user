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
 */

#include "../fw_common.h"
#include "../switchtec_priv.h"
#include "switchtec/switchtec.h"
#include "switchtec/errors.h"
#include "switchtec/endian.h"
#include "switchtec/utils.h"
#include "switchtec/mfg.h"

#include <unistd.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

enum switchtec_fw_part_type_gen5 {
	SWITCHTEC_FW_IMG_TYPE_MAP_GEN5 = 0x0,
	SWITCHTEC_FW_IMG_TYPE_KEYMAN_GEN5 = 0x1,
	SWITCHTEC_FW_IMG_TYPE_RIOT_GEN5 = 0x2,
	SWITCHTEC_FW_IMG_TYPE_BL2_GEN5 = 0x3,
	SWITCHTEC_FW_IMG_TYPE_CFG_GEN5 = 0x4,
	SWITCHTEC_FW_IMG_TYPE_IMG_GEN5 = 0x5,
	SWITCHTEC_FW_IMG_TYPE_NVLOG_GEN5 = 0x6,
	SWITCHTEC_FW_IMG_TYPE_SEEPROM_GEN5 = 0xFE,
	SWITCHTEC_FW_IMG_TYPE_UNKNOWN_GEN5,
};

struct switchtec_fw_metadata_gen5 {
	char magic[4];
	char sub_magic[4];
	uint32_t hdr_version;
	uint32_t secure_version;
	uint32_t header_len;
	uint32_t metadata_len;
	uint32_t image_len;
	uint32_t type;
	uint8_t fw_id;
	uint8_t rsvd[3];
	uint32_t version;
	uint32_t sequence;
	uint32_t reserved1;
	uint8_t date_str[8];
	uint8_t time_str[8];
	uint8_t img_str[16];
	uint8_t rsvd1[4];
	uint32_t image_crc;
	uint8_t public_key_modulus[512];
	uint8_t public_key_exponent[4];
	uint8_t uart_port;
	uint8_t uart_rate;
	uint8_t bist_enable;
	uint8_t bist_gpio_pin_cfg;
	uint8_t bist_gpio_level_cfg;
	uint8_t rollback_enable;
	uint8_t rsvd2[2];
	uint32_t xml_version;
	uint32_t relocatable_img_len;
	uint32_t link_addr;
	uint32_t header_crc;
};

struct switchtec_flash_info_gen5 {
	uint32_t firmware_version;
	uint32_t flash_size;
	uint16_t device_id;
	uint8_t ecc_enable;
	uint8_t rsvd1;
	uint8_t running_riot_flag;
	uint8_t running_bl2_flag;
	uint8_t running_cfg_flag;
	uint8_t running_img_flag;
	uint8_t running_key_flag;
	uint8_t rsvd2[3];
	uint8_t key_redundant_flag;
	uint8_t riot_redundant_flag;
	uint8_t bl2_redundant_flag;
	uint8_t cfg_redundant_flag;
	uint8_t img_redundant_flag;
	uint8_t rsvd3[3];
	uint32_t rsvd4[9];
	struct switchtec_flash_part_info_gen5 {
		uint32_t image_crc;
		uint32_t image_len;
		uint16_t image_version;
		uint8_t valid;
		uint8_t active;
		uint32_t part_start;
		uint32_t part_end;
		uint32_t part_offset;
		uint32_t part_size_dw;
		uint8_t read_only;
		uint8_t is_using;
		uint8_t rsvd[2];
	} map0, map1, keyman0, keyman1, riot0, riot1, bl20, bl21, cfg0, cfg1,
	  img0, img1, nvlog, vendor[8];
};

int switchtec_fw_img_write_hdr_gen5(int fd, struct switchtec_fw_image_info *info)
{
	int ret;
	struct switchtec_fw_metadata_gen5 *hdr = info->metadata;

	ret = write(fd, hdr, sizeof(*hdr));
	if (ret < 0)
		return ret;

	return lseek(fd, info->part_body_offset, SEEK_SET);
}

static const enum switchtec_fw_image_part_id_gen5
switchtec_fw_partitions_gen5[] = {
	SWITCHTEC_FW_PART_ID_G5_MAP0,
	SWITCHTEC_FW_PART_ID_G5_MAP1,
	SWITCHTEC_FW_PART_ID_G5_KEY0,
	SWITCHTEC_FW_PART_ID_G5_KEY1,
	SWITCHTEC_FW_PART_ID_G5_RIOT0,
	SWITCHTEC_FW_PART_ID_G5_RIOT1,
	SWITCHTEC_FW_PART_ID_G5_BL20,
	SWITCHTEC_FW_PART_ID_G5_BL21,
	SWITCHTEC_FW_PART_ID_G5_CFG0,
	SWITCHTEC_FW_PART_ID_G5_CFG1,
	SWITCHTEC_FW_PART_ID_G5_IMG0,
	SWITCHTEC_FW_PART_ID_G5_IMG1,
	SWITCHTEC_FW_PART_ID_G5_NVLOG,
	SWITCHTEC_FW_PART_ID_G5_SEEPROM,
};

static enum switchtec_fw_type
switchtec_fw_id_to_type_gen5(const struct switchtec_fw_image_info *info)
{
	switch (info->part_id) {
	case SWITCHTEC_FW_PART_ID_G5_MAP0: return SWITCHTEC_FW_TYPE_MAP;
	case SWITCHTEC_FW_PART_ID_G5_MAP1: return SWITCHTEC_FW_TYPE_MAP;
	case SWITCHTEC_FW_PART_ID_G5_KEY0: return SWITCHTEC_FW_TYPE_KEY;
	case SWITCHTEC_FW_PART_ID_G5_KEY1: return SWITCHTEC_FW_TYPE_KEY;
	case SWITCHTEC_FW_PART_ID_G5_RIOT0: return SWITCHTEC_FW_TYPE_RIOT;
	case SWITCHTEC_FW_PART_ID_G5_RIOT1: return SWITCHTEC_FW_TYPE_RIOT;
	case SWITCHTEC_FW_PART_ID_G5_BL20: return SWITCHTEC_FW_TYPE_BL2;
	case SWITCHTEC_FW_PART_ID_G5_BL21: return SWITCHTEC_FW_TYPE_BL2;
	case SWITCHTEC_FW_PART_ID_G5_CFG0: return SWITCHTEC_FW_TYPE_CFG;
	case SWITCHTEC_FW_PART_ID_G5_CFG1: return SWITCHTEC_FW_TYPE_CFG;
	case SWITCHTEC_FW_PART_ID_G5_IMG0: return SWITCHTEC_FW_TYPE_IMG;
	case SWITCHTEC_FW_PART_ID_G5_IMG1: return SWITCHTEC_FW_TYPE_IMG;
	case SWITCHTEC_FW_PART_ID_G5_NVLOG: return SWITCHTEC_FW_TYPE_NVLOG;
	case SWITCHTEC_FW_PART_ID_G5_SEEPROM: return SWITCHTEC_FW_TYPE_SEEPROM;
	default: return SWITCHTEC_FW_TYPE_UNKNOWN;
	}
}

static int switchtec_fw_info_metadata(struct switchtec_dev *dev,
					   struct switchtec_fw_image_info *inf)
{
	struct switchtec_fw_metadata_gen5 *metadata;
	struct {
		uint8_t subcmd;
		uint8_t part_id;
	} subcmd = {
		.subcmd = MRPC_PART_INFO_GET_METADATA_GEN5,
		.part_id = inf->part_id,
	};
	int ret;

	if (inf->part_id == SWITCHTEC_FW_PART_ID_G5_NVLOG)
		return 1;
	if (inf->part_id == SWITCHTEC_FW_PART_ID_G5_SEEPROM)
		subcmd.subcmd = MRPC_PART_INFO_GET_SEEPROM;

	metadata = malloc(sizeof(*metadata));
	if (!metadata)
		return -1;

	ret = switchtec_cmd(dev, MRPC_PART_INFO, &subcmd, sizeof(subcmd),
			    metadata, sizeof(*metadata));
	if (ret)
		goto err_out;

	if (strncmp(metadata->magic, "MSCC", sizeof(metadata->magic)))
		goto err_out;

	if (strncmp(metadata->sub_magic, "_MD ", sizeof(metadata->sub_magic)))
		goto err_out;

	version_to_string(le32toh(metadata->version), inf->version,
			  sizeof(inf->version));
	inf->part_body_offset = le32toh(metadata->header_len);
	inf->image_crc = le32toh(metadata->image_crc);
	inf->image_len = le32toh(metadata->image_len);
	inf->metadata = metadata;

	return 0;

err_out:
	free(metadata);
	return -1;
}

static int switchtec_fw_part_info_helper(struct switchtec_dev *dev,
				       struct switchtec_fw_image_info *inf,
				       struct switchtec_flash_info_gen5 *all)
{
	struct switchtec_flash_part_info_gen5 *part_info;
	int ret;

	switch(inf->part_id) {
	case SWITCHTEC_FW_PART_ID_G5_MAP0:
		part_info = &all->map0;
		break;
	case SWITCHTEC_FW_PART_ID_G5_MAP1:
		part_info = &all->map1;
		break;
	case SWITCHTEC_FW_PART_ID_G5_RIOT0:
		inf->redundant = all->riot_redundant_flag;
		part_info = &all->riot0;
		break;
	case SWITCHTEC_FW_PART_ID_G5_RIOT1:
		inf->redundant = all->riot_redundant_flag;
		part_info = &all->riot1;
		break;
	case SWITCHTEC_FW_PART_ID_G5_KEY0:
		inf->redundant = all->key_redundant_flag;
		part_info = &all->keyman0;
		break;
	case SWITCHTEC_FW_PART_ID_G5_KEY1:
		inf->redundant = all->key_redundant_flag;
		part_info = &all->keyman1;
		break;
	case SWITCHTEC_FW_PART_ID_G5_BL20:
		inf->redundant = all->bl2_redundant_flag;
		part_info = &all->bl20;
		break;
	case SWITCHTEC_FW_PART_ID_G5_BL21:
		inf->redundant = all->bl2_redundant_flag;
		part_info = &all->bl21;
		break;
	case SWITCHTEC_FW_PART_ID_G5_IMG0:
		inf->redundant = all->img_redundant_flag;
		part_info = &all->img0;
		break;
	case SWITCHTEC_FW_PART_ID_G5_IMG1:
		inf->redundant = all->img_redundant_flag;
		part_info = &all->img1;
		break;
	case SWITCHTEC_FW_PART_ID_G5_CFG0:
		inf->redundant = all->cfg_redundant_flag;
		part_info = &all->cfg0;
		break;
	case SWITCHTEC_FW_PART_ID_G5_CFG1:
		inf->redundant = all->cfg_redundant_flag;
		part_info = &all->cfg1;
		break;
	case SWITCHTEC_FW_PART_ID_G5_NVLOG:
		part_info = &all->nvlog;
		break;
	case SWITCHTEC_FW_PART_ID_G5_SEEPROM:
		inf->active = true;
		/* length is not applicable for SEEPROM image */
		inf->part_len = 0xffffffff;

		ret = switchtec_fw_info_metadata(dev, inf);
		if (!ret) {
			inf->running = true;
			inf->valid = true;
		}

		return 0;
	default:
		errno = EINVAL;
		return -1;
	}

	inf->part_addr = le32toh(part_info->part_start);
	inf->part_len = le32toh(part_info->part_size_dw) * 4;
	inf->active = part_info->active;
	inf->running = part_info->is_using;
	inf->read_only = part_info->read_only;
	inf->valid = part_info->valid;
	if (!inf->valid)
		return 0;

	return switchtec_fw_info_metadata(dev, inf);
}

int switchtec_fw_part_info_gen5(struct switchtec_dev *dev, int nr_info, struct switchtec_fw_image_info *info)
{
	int ret;
	int i;
	uint8_t subcmd = MRPC_PART_INFO_GET_ALL_INFO;
	struct switchtec_flash_info_gen5 all_info_gen5;

	if (info == NULL || nr_info == 0)
		return -EINVAL;

	subcmd = MRPC_PART_INFO_GET_ALL_INFO_GEN5;
	ret = switchtec_cmd(dev, MRPC_PART_INFO, &subcmd,
				sizeof(subcmd), &all_info_gen5,
				sizeof(all_info_gen5));
	if (ret)
		return ret;
	all_info_gen5.firmware_version =
		le32toh(all_info_gen5.firmware_version);
	all_info_gen5.flash_size = le32toh(all_info_gen5.flash_size);
	all_info_gen5.device_id = le16toh(all_info_gen5.device_id);

	for (i = 0; i < nr_info; i++) {
		struct switchtec_fw_image_info *inf = &info[i];
		ret = 0;

		inf->gen = dev->gen;
		inf->type = switchtec_fw_id_to_type_gen5(inf);
		inf->active = false;
		inf->running = false;
		inf->valid = false;
		ret = switchtec_fw_part_info_helper(dev, inf, &all_info_gen5);

		if (ret < 0)
			return ret;

		if (ret) {
			inf->version[0] = 0;
			inf->image_crc = 0xFFFFFFFF;
			inf->metadata = NULL;
		}
	}

	return nr_info;
}

struct switchtec_fw_part_summary *switchtec_fw_part_summary_gen5(struct switchtec_dev *dev)
{
	struct switchtec_fw_part_summary *summary;
	struct switchtec_fw_image_info **infp;
	struct switchtec_fw_part_type *type;
	int nr_info, nr_mcfg = 16;
	size_t st_sz;
	int ret, i;

	nr_info = ARRAY_SIZE(switchtec_fw_partitions_gen5);

	st_sz = sizeof(*summary) + sizeof(*summary->all) * (nr_info + nr_mcfg);

	summary = malloc(st_sz);
	if (!summary)
		return NULL;

	memset(summary, 0, st_sz);
	summary->nr_info = nr_info;

	for (i = 0; i < nr_info; i++)
		summary->all[i].part_id =
			switchtec_fw_partitions_gen5[i];

	ret = switchtec_fw_part_info_gen5(dev, nr_info, summary->all);
	if (ret != nr_info) {
		free(summary);
		return NULL;
	}

	ret = get_multicfg(dev, &summary->all[nr_info], &nr_mcfg);
	if (ret) {
		nr_mcfg = 0;
		errno = 0;
	}

	for (i = 0; i < nr_info; i++) {
		type = switchtec_fw_type_ptr(summary, &summary->all[i]);
		if (type == NULL) {
			free(summary);
			return NULL;
		}
		if (summary->all[i].active)
			type->active = &summary->all[i];
		else
			type->inactive = &summary->all[i];
	}

	infp = &summary->mult_cfg;
	for (; i < nr_info + nr_mcfg; i++) {
		*infp = &summary->all[i];
		infp = &summary->all[i].next;
	}

	return summary;
}

static enum switchtec_fw_image_part_id_gen5 hdr_type2_id_gen5(uint32_t type)
{
	switch (type) {
	case SWITCHTEC_FW_IMG_TYPE_MAP_GEN5:
		return SWITCHTEC_FW_PART_ID_G5_MAP0;

	case SWITCHTEC_FW_IMG_TYPE_KEYMAN_GEN5:
		return SWITCHTEC_FW_PART_ID_G5_KEY0;

	case SWITCHTEC_FW_IMG_TYPE_RIOT_GEN5:
		return SWITCHTEC_FW_PART_ID_G5_RIOT0;

	case SWITCHTEC_FW_IMG_TYPE_BL2_GEN5:
		return SWITCHTEC_FW_PART_ID_G5_BL20;

	case SWITCHTEC_FW_IMG_TYPE_CFG_GEN5:
		return SWITCHTEC_FW_PART_ID_G5_CFG0;

	case SWITCHTEC_FW_IMG_TYPE_IMG_GEN5:
		return SWITCHTEC_FW_PART_ID_G5_IMG0;

	case SWITCHTEC_FW_IMG_TYPE_NVLOG_GEN5:
		return SWITCHTEC_FW_PART_ID_G5_NVLOG;

	case SWITCHTEC_FW_IMG_TYPE_SEEPROM_GEN5:
		return SWITCHTEC_FW_PART_ID_G5_SEEPROM;

	default:
		return -1;
	}
}

int switchtec_fw_file_info_gen5(int fd, struct switchtec_fw_image_info *info)
{
	int ret;
	struct switchtec_fw_metadata_gen5 hdr = {};
	uint8_t exp_zero[4] = {};
	uint32_t version;
	int part_id;

	ret = read(fd, &hdr, sizeof(hdr));
	lseek(fd, 0, SEEK_SET);

	if (ret != sizeof(hdr))
		goto invalid_file;

	part_id = hdr_type2_id_gen5(le32toh(hdr.type));

	if (part_id < 0)
		goto invalid_file;

	info->part_id = part_id;

	info->image_crc = le32toh(hdr.image_crc);
	version = le32toh(hdr.version);
	version_to_string(version, info->version, sizeof(info->version));
	info->image_len = le32toh(hdr.image_len);
	info->gen = switchtec_fw_version_to_gen(version);

	if (info->gen == SWITCHTEC_GEN5)
		info->type = switchtec_fw_id_to_type_gen5(info);
	else if (info->gen == SWITCHTEC_GEN6)
		info->type = switchtec_fw_id_to_type_gen6(info);

	info->secure_version = le32toh(hdr.secure_version);
	info->signed_image = !!memcmp(hdr.public_key_exponent, exp_zero, 4);

	return 0;

invalid_file:
	errno = ENOEXEC;
	return -errno;
}

int switchtec_get_device_id_bl2_gen5(struct switchtec_dev *dev,
			        unsigned short *device_id)
{
	int ret;
	uint8_t subcmd = MRPC_PART_INFO_GET_ALL_INFO_GEN5;
	struct switchtec_flash_info_gen5 all_info;

	ret = switchtec_cmd(dev, MRPC_PART_INFO, &subcmd,
				sizeof(subcmd), &all_info,
				sizeof(all_info));
	if (!ret)
		*device_id = le16toh(all_info.device_id);

	return ret;
}



static int set_redundant(struct switchtec_dev *dev, int type, int set)
{
	int ret;
	char *part_types[] = {
		"KEYMAN",
		"RIOT",
		"BL2",
		"CFG",
		"MAIN-FW"
	};

	struct {
		uint8_t subcmd;
		uint8_t part_type;
		uint8_t redundant_val;
		uint8_t reserved;
	} cmd;

	cmd.subcmd = MRPC_FWDNLD_SET_RDNDNT;
	cmd.redundant_val = set;
	cmd.part_type = type;

	printf("%s redundant flag \t(%s)\n", set ? "Checking" : "Un-checking", 
	       part_types[type-1]);
	ret = switchtec_cmd(dev, MRPC_FWDNLD, &cmd, sizeof(cmd), NULL, 0);
	if (ret) {
		fprintf(stderr, "Error: setting redudant flag \t(%s)\n", 
			part_types[type-1]);
		return 1;
	}
	else {
		printf("Success: set redundant flag \t(%s)\n", 
		       part_types[type-1]);
	}
	return 0;
}

int switchtec_fw_set_redundant_flag_gen5 (struct switchtec_dev *dev, int keyman, 
				     int riot, int bl2, int cfg, int fw, 
				     int set)
{
	int ret = 0;
	if(keyman)
		ret += set_redundant(dev, SWITCHTEC_PART_TYPE_KEYMAN, set);
	if (riot)
		ret += set_redundant(dev, SWITCHTEC_PART_TYPE_RC, set);
	if (bl2)
		ret += set_redundant(dev, SWITCHTEC_PART_TYPE_BL2, set);
	if (cfg)
		ret += set_redundant(dev, SWITCHTEC_PART_TYPE_CFG, set);
	if (fw)
		ret += set_redundant(dev, SWITCHTEC_PART_TYPE_FW, set);

	return ret;
}

int switchtec_fw_toggle_active_partition_gen5(struct switchtec_dev *dev,
					 int toggle_bl2, int toggle_key,
					 int toggle_fw, int toggle_cfg,
					 int toggle_riotcore)
{
	uint32_t cmd_id;
	size_t cmd_size;
	struct {
		uint8_t subcmd;
		uint8_t toggle_fw;
		uint8_t toggle_cfg;
		uint8_t toggle_bl2;
		uint8_t toggle_key;
		uint8_t toggle_riotcore;
	} cmd;

	if (switchtec_boot_phase(dev) == SWITCHTEC_BOOT_PHASE_BL2) {
		cmd_id = MRPC_FW_TX_GEN5;
		cmd.subcmd = MRPC_FW_TX_TOGGLE;
	} else {
		cmd_id = MRPC_FWDNLD;
		cmd.subcmd = MRPC_FWDNLD_TOGGLE;
	}

	cmd.toggle_bl2 = !!toggle_bl2;
	cmd.toggle_key = !!toggle_key;
	cmd.toggle_fw = !!toggle_fw;
	cmd.toggle_cfg = !!toggle_cfg;
	cmd.toggle_riotcore = !!toggle_riotcore;
	cmd_size = sizeof(cmd);

	return switchtec_cmd(dev, cmd_id, &cmd, cmd_size, NULL, 0);
}