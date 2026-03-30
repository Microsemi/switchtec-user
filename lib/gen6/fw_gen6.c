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

struct cmd_fwget_gen6 {
	uint8_t subcmd;
	uint8_t fw_type;
	uint8_t fw_slot;
	uint8_t f_from_start;
};

typedef enum
{
    MRPC_FW_IMG_GET_CMD_START = 0,
    MRPC_FW_IMG_GET_CMD_NEXT = 1,

    MRPC_FW_IMG_GET_CMD_MAX
} mrpc_fw_img_get_cmd_e;

struct switchtec_fw_metadata_gen6 {
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

struct switchtec_flash_info_gen6 {
	uint32_t firmware_version;
	uint32_t flash_size;
	uint16_t device_id;
	uint8_t ecc_enable;
	uint8_t rsvd1;
	uint8_t running_bl2_flag;
	uint8_t running_cfg_flag;
	uint8_t running_img_flag;
	uint8_t running_key_flag;
	uint8_t rsvd2[4];
	uint8_t key_redundant_flag;
	uint8_t bl2_redundant_flag;
	uint8_t cfg_redundant_flag;
	uint8_t img_redundant_flag;
	uint8_t rsvd3[2];
	uint32_t rsvd4[9];
	struct switchtec_flash_part_info_gen6 {
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
	} map0, map1, keyman0, keyman1, bl20, bl21, cfg0, cfg1, img0, img1, 
	  nvlog, vendor[8];
};

int switchtec_fw_img_write_hdr_gen6(int fd, struct switchtec_fw_image_info *info)
{
	int ret;
	struct switchtec_fw_metadata_gen6 *hdr = info->metadata;

	ret = write(fd, hdr, sizeof(*hdr));
	if (ret < 0)
		return ret;

	return lseek(fd, info->part_body_offset, SEEK_SET);
}

static const enum switchtec_fw_image_part_id_gen6
switchtec_fw_partitions_gen6[] = {
	SWITCHTEC_FW_PART_ID_G6_MAP0,
	SWITCHTEC_FW_PART_ID_G6_MAP1,
	SWITCHTEC_FW_PART_ID_G6_KEY0,
	SWITCHTEC_FW_PART_ID_G6_KEY1,
	SWITCHTEC_FW_PART_ID_G6_BL20,
	SWITCHTEC_FW_PART_ID_G6_BL21,
	SWITCHTEC_FW_PART_ID_G6_CFG0,
	SWITCHTEC_FW_PART_ID_G6_CFG1,
	SWITCHTEC_FW_PART_ID_G6_IMG0,
	SWITCHTEC_FW_PART_ID_G6_IMG1,
	SWITCHTEC_FW_PART_ID_G6_NVLOG,
	SWITCHTEC_FW_PART_ID_G6_SEEPROM,
};


static int switchtec_fw_info_metadata(struct switchtec_dev *dev,
					   struct switchtec_fw_image_info *inf)
{
	struct switchtec_fw_metadata_gen6 *metadata;
	struct {
		uint8_t subcmd;
		uint8_t part_id;
	} subcmd = {
		.subcmd = MRPC_PART_INFO_GET_METADATA_GEN6,
		.part_id = inf->part_id,
	};
	int ret;

	if (inf->part_id == SWITCHTEC_FW_PART_ID_G6_NVLOG)
		return 1;
	if (inf->part_id == SWITCHTEC_FW_PART_ID_G6_SEEPROM)
		subcmd.subcmd = MRPC_PART_INFO_GET_SEEPROM_GEN6;

	metadata = malloc(sizeof(*metadata));
	if (!metadata)
		return -1;

	ret = switchtec_cmd(dev, MRPC_PART_INFO, &subcmd, sizeof(subcmd),
			    metadata, sizeof(*metadata));
	if (ret)
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
				       struct switchtec_flash_info_gen6 *all)
{
	struct switchtec_flash_part_info_gen6 *part_info;
	int ret;

	switch(inf->part_id) {
	case SWITCHTEC_FW_PART_ID_G6_MAP0:
		part_info = &all->map0;
		break;
	case SWITCHTEC_FW_PART_ID_G6_MAP1:
		part_info = &all->map1;
		break;
	case SWITCHTEC_FW_PART_ID_G6_KEY0:
		inf->redundant = all->key_redundant_flag;
		part_info = &all->keyman0;
		break;
	case SWITCHTEC_FW_PART_ID_G6_KEY1:
		inf->redundant = all->key_redundant_flag;
		part_info = &all->keyman1;
		break;
	case SWITCHTEC_FW_PART_ID_G6_BL20:
		inf->redundant = all->bl2_redundant_flag;
		part_info = &all->bl20;
		break;
	case SWITCHTEC_FW_PART_ID_G6_BL21:
		inf->redundant = all->bl2_redundant_flag;
		part_info = &all->bl21;
		break;
	case SWITCHTEC_FW_PART_ID_G6_IMG0:
		inf->redundant = all->img_redundant_flag;
		part_info = &all->img0;
		break;
	case SWITCHTEC_FW_PART_ID_G6_IMG1:
		inf->redundant = all->img_redundant_flag;
		part_info = &all->img1;
		break;
	case SWITCHTEC_FW_PART_ID_G6_CFG0:
		inf->redundant = all->cfg_redundant_flag;
		part_info = &all->cfg0;
		break;
	case SWITCHTEC_FW_PART_ID_G6_CFG1:
		inf->redundant = all->cfg_redundant_flag;
		part_info = &all->cfg1;
		break;
	case SWITCHTEC_FW_PART_ID_G6_NVLOG:
		part_info = &all->nvlog;
		break;
	case SWITCHTEC_FW_PART_ID_G6_SEEPROM:
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

int switchtec_fw_part_info_gen6(struct switchtec_dev *dev, int nr_info, struct switchtec_fw_image_info *info)
{
	int ret;
	int i;
	uint8_t subcmd = MRPC_PART_INFO_GET_ALL_INFO;
	struct switchtec_flash_info_gen6 all_info_gen6;

	if (info == NULL || nr_info == 0)
		return -EINVAL;

	subcmd = MRPC_PART_INFO_GET_ALL_INFO_GEN6;
	ret = switchtec_cmd(dev, MRPC_PART_INFO, &subcmd,
				sizeof(subcmd), &all_info_gen6,
				sizeof(all_info_gen6));
	if (ret)
		return ret;
	all_info_gen6.firmware_version =
		le32toh(all_info_gen6.firmware_version);
	all_info_gen6.flash_size = le32toh(all_info_gen6.flash_size);
	all_info_gen6.device_id = le16toh(all_info_gen6.device_id);

	for (i = 0; i < nr_info; i++) {
		struct switchtec_fw_image_info *inf = &info[i];
		ret = 0;

		inf->gen = dev->gen;
		inf->type = switchtec_fw_id_to_type_gen6(inf);
		inf->active = false;
		inf->running = false;
		inf->valid = false;
		ret = switchtec_fw_part_info_helper(dev, inf, &all_info_gen6);

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

struct switchtec_fw_part_summary *switchtec_fw_part_summary_gen6(struct switchtec_dev *dev)
{
	struct switchtec_fw_part_summary *summary;
	struct switchtec_fw_image_info **infp;
	struct switchtec_fw_part_type *type;
	int nr_info, nr_mcfg = 16;
	size_t st_sz;
	int ret, i;

	nr_info = ARRAY_SIZE(switchtec_fw_partitions_gen6);

	st_sz = sizeof(*summary) + sizeof(*summary->all) * (nr_info + nr_mcfg);

	summary = malloc(st_sz);
	if (!summary)
		return NULL;

	memset(summary, 0, st_sz);
	summary->nr_info = nr_info;

	for (i = 0; i < nr_info; i++)
		summary->all[i].part_id =
			switchtec_fw_partitions_gen6[i];

	ret = switchtec_fw_part_info_gen6(dev, nr_info, summary->all);
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

struct switchtec_fw_image_info *switchtec_fw_part_data_bl2_gen6(struct switchtec_dev *dev)
{
	struct switchtec_fw_image_info *inf;
	struct switchtec_fw_image_info *tmp;
	int ret;
	
	struct switchtec_fw_metadata_gen6 *metadata;
	
	struct {
		uint8_t subcmd;
		uint8_t part_id;
	} subcmd = {
		.subcmd = MRPC_PART_INFO_GET_METADATA_GEN6,
	};
	inf = malloc(sizeof(*inf));
	if (!inf)
		return NULL;
	tmp = inf;

	for (int i = 0; i <= 9; i++)
	{
		metadata = malloc(sizeof(*metadata));
		if (!metadata)
			return NULL;
		subcmd.part_id = i;
		ret = switchtec_cmd(dev, MRPC_PART_INFO, &subcmd, sizeof(subcmd),
				metadata, sizeof(*metadata));
		if (ret)
			return NULL;
		version_to_string(le32toh(metadata->version), inf->version,
				sizeof(inf->version));
		inf->metadata = metadata;
		inf->next = malloc(sizeof(*inf));
		if (!inf->next)
			return NULL;
		inf = inf->next;
	}
	inf = tmp;
	return inf;
}

static int set_redundant(struct switchtec_dev *dev, int type, int set)
{
	int ret;
	char *part_types[] = {
		"KEYMAN",
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
	if(type != 1)
		cmd.part_type = type - 1;

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

int switchtec_fw_set_redundant_flag_gen6(struct switchtec_dev *dev, int keyman, 
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

int switchtec_fw_img_get_gen6(struct switchtec_dev *dev, int fd, 
			 enum switchtec_fw_type_gen6 fw_type, int fw_slot, 
			 void (*progress_callback)(int cur, int tot))
{
  	struct cmd_fwget_gen6 cmd = {0};

	struct fw_img_get_resp_t {
		uint8_t status;
		uint8_t fw_type;
		uint8_t fw_slot;
		uint8_t reserved;
		uint32_t total_len;
		uint32_t offset;
		uint32_t chunk_len;
		uint32_t *data;
	};

	uint8_t resp[MRPC_MAX_DATA_LEN] ={};
	struct fw_img_get_resp_t *fw_img_get_resp = (struct fw_img_get_resp_t *)resp;
	int ret;

	/* erase existing file */
	if (ftruncate(fd, 0) == -1) {
		perror("ftruncate");
		return 1;
	}

	cmd.subcmd = 0;
	cmd.fw_type = fw_type;
	cmd.fw_slot = fw_slot;
	cmd.f_from_start = MRPC_FW_IMG_GET_CMD_START;

	ret = switchtec_cmd(dev, MRPC_FW_IMG_GET, &cmd, sizeof(cmd),
		resp, sizeof(resp));

	if(ret){
		printf("Error during FW image get\n");
		return ret;
	}

	size_t written = write(fd, (void*)(&fw_img_get_resp->data), fw_img_get_resp->chunk_len);
	if (written != fw_img_get_resp->chunk_len) {
		perror("Error writing to file");
		return 1;
	}

	if (progress_callback)
		progress_callback(fw_img_get_resp->offset, fw_img_get_resp->total_len);

	do {
		cmd.subcmd = 0;
		cmd.fw_type = fw_type;
		cmd.fw_slot = fw_slot;
		cmd.f_from_start = MRPC_FW_IMG_GET_CMD_NEXT;

		ret = switchtec_cmd(dev, MRPC_FW_IMG_GET, &cmd, sizeof(cmd),
			resp, sizeof(resp));

		if(ret){
			printf("Error during FW image get\n");
			return ret;
		}

		size_t written = write(fd, (void*)(&fw_img_get_resp->data), fw_img_get_resp->chunk_len);
		if (written != fw_img_get_resp->chunk_len){
			perror("Error writing to file");
			return 1;
		}

		if (progress_callback)
			progress_callback(fw_img_get_resp->offset, fw_img_get_resp->total_len);

	} while((fw_img_get_resp->offset + fw_img_get_resp->chunk_len) < fw_img_get_resp->total_len);

	return 0;
}

int switchtec_fw_toggle_active_partition_gen6(struct switchtec_dev *dev,
					 int toggle_bl2, int toggle_key,
					 int toggle_fw, int toggle_cfg,
					 int toggle_riotcore)
{
	uint32_t cmd_id;
	size_t cmd_size;
	int ret;
	struct {
		uint8_t subcmd;
		uint8_t toggle_fw;
		uint8_t toggle_cfg;
		uint8_t toggle_bl2;
		uint8_t toggle_key;
		uint16_t reserved;
	} cmd;

	if (switchtec_boot_phase(dev) == SWITCHTEC_BOOT_PHASE_BL2) {
		cmd_size = sizeof(cmd);
		cmd_id = MRPC_FW_TX_GEN6;
		cmd.subcmd = MRPC_FW_TX_TOGGLE;
		cmd.toggle_bl2 = !!toggle_bl2;
		ret = switchtec_cmd(dev, cmd_id, &cmd, cmd_size, NULL, 0);
		if (ret)
			return ret;
		cmd.toggle_bl2 = 0;
		cmd.toggle_fw = !!toggle_fw;
		ret = switchtec_cmd(dev, cmd_id, &cmd, cmd_size, NULL, 0);
		if (ret)
			return ret;
		cmd.toggle_fw = 0;
		cmd.toggle_cfg = !!toggle_cfg;
		ret = switchtec_cmd(dev, cmd_id, &cmd, cmd_size, NULL, 0);
		if (ret)
			return ret;
		
		return 0;
	}
	
	cmd_id = MRPC_FWDNLD;
	cmd.subcmd = MRPC_FWDNLD_TOGGLE;
	cmd.toggle_bl2 = !!toggle_bl2;
	cmd.toggle_key = !!toggle_key;
	cmd.toggle_fw = !!toggle_fw;
	cmd.toggle_cfg = !!toggle_cfg;
	cmd_size = sizeof(cmd);

	return switchtec_cmd(dev, cmd_id, &cmd, cmd_size,
			     NULL, 0);
}