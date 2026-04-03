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

#include "../switchtec_priv.h"
#include "switchtec/switchtec.h"
#include "switchtec/errors.h"
#include "switchtec/endian.h"
#include "switchtec/utils.h"
#include "switchtec/mfg.h"
#include "../fw_common.h"

#include <unistd.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

struct switchtec_fw_footer_gen3 {
	char magic[4];
	uint32_t image_len;
	uint32_t load_addr;
	uint32_t version;
	uint32_t rsvd;
	uint32_t header_crc;
	uint32_t image_crc;
};

struct switchtec_fw_image_header_gen3 {
	char magic[4];
	uint32_t image_len;
	uint32_t type;
	uint32_t load_addr;
	uint32_t version;
	uint32_t rsvd[9];
	uint32_t header_crc;
	uint32_t image_crc;
};

static const enum switchtec_fw_image_part_id_gen3
switchtec_fw_partitions_gen3[] = {
	SWITCHTEC_FW_PART_ID_G3_BOOT,
	SWITCHTEC_FW_PART_ID_G3_MAP0,
	SWITCHTEC_FW_PART_ID_G3_MAP1,
	SWITCHTEC_FW_PART_ID_G3_IMG0,
	SWITCHTEC_FW_PART_ID_G3_DAT0,
	SWITCHTEC_FW_PART_ID_G3_DAT1,
	SWITCHTEC_FW_PART_ID_G3_NVLOG,
	SWITCHTEC_FW_PART_ID_G3_IMG1,
};

struct switchtec_boot_ro_gen3 {
	uint8_t subcmd;
	uint8_t set_get;
	uint8_t status;
	uint8_t reserved;
};

int switchtec_fw_read_gen3(struct switchtec_dev *dev, unsigned long addr, size_t len, void *buf)
{
	int ret;
	struct {
		uint32_t addr;
		uint32_t length;
	} cmd;
	unsigned char *cbuf = buf;
	size_t read = 0;

	while(len) {
		size_t chunk_len = len;
		if (chunk_len > MRPC_MAX_DATA_LEN-8)
			chunk_len = MRPC_MAX_DATA_LEN-8;

		cmd.addr = htole32(addr);
		cmd.length = htole32(chunk_len);

		ret = switchtec_cmd(dev, MRPC_RD_FLASH, &cmd, sizeof(cmd),
				    cbuf, chunk_len);
		if (ret)
			return -1;

		addr += chunk_len;
		len -= chunk_len;
		read += chunk_len;
		cbuf += chunk_len;
	}

	return read;
}

static int switchtec_fw_map_get_active(struct switchtec_dev *dev,
				       struct switchtec_fw_image_info *info)
{
	uint32_t map0_update_index;
	uint32_t map1_update_index;
	int ret;

	ret = switchtec_fw_read_gen3(dev, SWITCHTEC_FLASH_MAP0_PART_START,
				sizeof(uint32_t), &map0_update_index);
	if (ret < 0)
		return ret;

	ret = switchtec_fw_read_gen3(dev, SWITCHTEC_FLASH_MAP1_PART_START,
				sizeof(uint32_t), &map1_update_index);
	if (ret < 0)
		return ret;

	info->active = 0;
	if (map0_update_index > map1_update_index) {
		if (info->part_addr == SWITCHTEC_FLASH_MAP0_PART_START)
			info->active = 1;
	} else {
		if (info->part_addr == SWITCHTEC_FLASH_MAP1_PART_START)
			info->active = 1;
	}

	return 0;
}

int switchtec_fw_is_boot_ro_gen3(struct switchtec_dev *dev)
{
	struct switchtec_boot_ro_gen3 subcmd = {
		.subcmd = MRPC_FWDNLD_BOOT_RO,
		.set_get = 0,
	};

	struct {
		uint8_t status;
		uint8_t reserved[3];
	} result;

	int ret;

	ret = switchtec_cmd(dev, MRPC_FWDNLD, &subcmd, sizeof(subcmd),
			    &result, sizeof(result));

	if (ret == ERR_SUBCMD_INVALID) {
		errno = 0;
		return 0;
	}

	if (ret)
		return ret;

	return result.status;
}

int switchtec_fw_set_boot_ro_gen3(struct switchtec_dev *dev,
			     enum switchtec_fw_ro ro)
{
	struct switchtec_boot_ro_gen3 subcmd = {
		.subcmd = MRPC_FWDNLD_BOOT_RO,
		.set_get = 1,
		.status = ro,
	};

	return switchtec_cmd(dev, MRPC_FWDNLD, &subcmd, sizeof(subcmd),
			     NULL, 0);
}

int switchtec_fw_img_write_hdr_gen3(int fd, struct switchtec_fw_image_info *info)
{
	struct switchtec_fw_footer_gen3 *ftr = info->metadata;
	struct switchtec_fw_image_header_gen3 hdr = {};

	memcpy(hdr.magic, ftr->magic, sizeof(hdr.magic));
	hdr.image_len = ftr->image_len;
	hdr.type = info->part_id;
	hdr.load_addr = ftr->load_addr;
	hdr.version = ftr->version;
	hdr.header_crc = ftr->header_crc;
	hdr.image_crc = ftr->image_crc;

	if (hdr.type == SWITCHTEC_FW_PART_ID_G3_MAP1)
		hdr.type = SWITCHTEC_FW_PART_ID_G3_MAP0;
	else if (hdr.type == SWITCHTEC_FW_PART_ID_G3_IMG1)
		hdr.type = SWITCHTEC_FW_PART_ID_G3_IMG0;
	else if (hdr.type == SWITCHTEC_FW_PART_ID_G3_DAT1)
		hdr.type = SWITCHTEC_FW_PART_ID_G3_DAT0;

	return write(fd, &hdr, sizeof(hdr));
}

static int switchtec_fw_info_metadata_gen3(struct switchtec_dev *dev,
					   struct switchtec_fw_image_info *inf)
{
	struct switchtec_fw_footer_gen3 *metadata;
	unsigned long addr;
	int ret = 0;

	if (inf->part_id == SWITCHTEC_FW_PART_ID_G3_NVLOG)
		return 1;

	metadata = malloc(sizeof(*metadata));
	if (!metadata)
		return -1;

	addr = inf->part_addr + inf->part_len - sizeof(*metadata);

	ret = switchtec_fw_read(dev, addr, sizeof(*metadata), metadata);
	if (ret < 0)
		goto err_out;

	if (strncmp(metadata->magic, "PMC", sizeof(metadata->magic)))
		goto err_out;

	version_to_string(metadata->version, inf->version,
			  sizeof(inf->version));
	inf->part_body_offset = 0;
	inf->image_crc = metadata->image_crc;
	inf->image_len = metadata->image_len;
	inf->metadata = metadata;

	return 0;

err_out:
	free(metadata);
	return 1;
}

static int switchtec_fw_part_info_gen3_single(struct switchtec_dev *dev,
					       struct switchtec_fw_image_info *inf)
{
	int ret = 0;

	inf->read_only = switchtec_fw_is_boot_ro(dev);

	switch (inf->part_id) {
		case SWITCHTEC_FW_PART_ID_G3_BOOT:
			inf->part_addr = SWITCHTEC_FLASH_BOOT_PART_START;
			inf->part_len = SWITCHTEC_FLASH_PART_LEN;
			inf->active = true;
			break;
		case SWITCHTEC_FW_PART_ID_G3_MAP0:
			inf->part_addr = SWITCHTEC_FLASH_MAP0_PART_START;
			inf->part_len = SWITCHTEC_FLASH_PART_LEN;
			ret = switchtec_fw_map_get_active(dev, inf);
			break;
		case SWITCHTEC_FW_PART_ID_G3_MAP1:
			inf->part_addr = SWITCHTEC_FLASH_MAP1_PART_START;
			inf->part_len = SWITCHTEC_FLASH_PART_LEN;
			ret = switchtec_fw_map_get_active(dev, inf);
			break;
		default:
			ret = switchtec_flash_part(dev, inf, inf->part_id);
			inf->read_only = false;
	}

	if (ret)
		return ret;

	inf->valid = true;

	if (inf->part_id == SWITCHTEC_FW_PART_ID_G3_NVLOG)
		return 1;

	return switchtec_fw_info_metadata_gen3(dev, inf);
}

int switchtec_fw_part_info_gen3(struct switchtec_dev *dev, int nr_info,
				struct switchtec_fw_image_info *info)
{
	int i;
	int ret;

	for (i = 0; i < nr_info; i++) {
		ret = switchtec_fw_part_info_gen3_single(dev, &info[i]);
		if (ret < 0)
			return ret;
	}

	return nr_info;
}

struct switchtec_fw_part_summary *switchtec_fw_part_summary_gen3(struct switchtec_dev *dev)
{
	struct switchtec_fw_part_summary *summary;
	struct switchtec_fw_image_info **infp;
	struct switchtec_fw_part_type *type;
	int nr_info, nr_mcfg = 16;
	size_t st_sz;
	int ret, i;

	nr_info = ARRAY_SIZE(switchtec_fw_partitions_gen3);

	st_sz = sizeof(*summary) + sizeof(*summary->all) * (nr_info + nr_mcfg);

	summary = malloc(st_sz);
	if (!summary)
		return NULL;

	memset(summary, 0, st_sz);
	summary->nr_info = nr_info;

        for (i = 0; i < nr_info; i++)
                summary->all[i].part_id =
                        switchtec_fw_partitions_gen3[i];

	ret = switchtec_fw_part_info_gen3(dev, nr_info, summary->all);
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


static enum switchtec_fw_type
switchtec_fw_id_to_type(const struct switchtec_fw_image_info *info)
{
	switch ((unsigned long)info->part_id) {
	case SWITCHTEC_FW_PART_ID_G3_BOOT: return SWITCHTEC_FW_TYPE_BOOT;
	case SWITCHTEC_FW_PART_ID_G3_MAP0: return SWITCHTEC_FW_TYPE_MAP;
	case SWITCHTEC_FW_PART_ID_G3_MAP1: return SWITCHTEC_FW_TYPE_MAP;
	case SWITCHTEC_FW_PART_ID_G3_IMG0: return SWITCHTEC_FW_TYPE_IMG;
	case SWITCHTEC_FW_PART_ID_G3_IMG1: return SWITCHTEC_FW_TYPE_IMG;
	case SWITCHTEC_FW_PART_ID_G3_DAT0: return SWITCHTEC_FW_TYPE_CFG;
	case SWITCHTEC_FW_PART_ID_G3_DAT1: return SWITCHTEC_FW_TYPE_CFG;
	case SWITCHTEC_FW_PART_ID_G3_NVLOG: return SWITCHTEC_FW_TYPE_NVLOG;
	case SWITCHTEC_FW_PART_ID_G3_SEEPROM: return SWITCHTEC_FW_TYPE_SEEPROM;

	//Legacy
	case 0xa8000000: return SWITCHTEC_FW_TYPE_BOOT;
	case 0xa8020000: return SWITCHTEC_FW_TYPE_MAP;
	case 0xa8060000: return SWITCHTEC_FW_TYPE_IMG;
	case 0xa8210000: return SWITCHTEC_FW_TYPE_CFG;

	default: return SWITCHTEC_FW_TYPE_UNKNOWN;
	}
}

int switchtec_fw_file_info_gen3(int fd,
				struct switchtec_fw_image_info *info)
{
	struct switchtec_fw_image_header_gen3 hdr = {};
	int ret;

	ret = read(fd, &hdr, sizeof(hdr));
	lseek(fd, 0, SEEK_SET);

	if (ret != sizeof(hdr))
		goto invalid_file;

	info->gen = SWITCHTEC_GEN3;
	info->part_id = hdr.type;
	info->image_crc = le32toh(hdr.image_crc);
	version_to_string(hdr.version, info->version, sizeof(info->version));
	info->image_len = le32toh(hdr.image_len);

	info->type = switchtec_fw_id_to_type(info);

	info->secure_version = 0;
	info->signed_image = 0;

	return 0;

invalid_file:
	errno = ENOEXEC;
	return -errno;
}