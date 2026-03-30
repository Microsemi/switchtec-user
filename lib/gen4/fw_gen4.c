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

enum switchtec_fw_part_type_gen4 {
	SWITCHTEC_FW_IMG_TYPE_MAP_GEN4 = 0x0,
	SWITCHTEC_FW_IMG_TYPE_KEYMAN_GEN4 = 0x1,
	SWITCHTEC_FW_IMG_TYPE_BL2_GEN4 = 0x2,
	SWITCHTEC_FW_IMG_TYPE_CFG_GEN4 = 0x3,
	SWITCHTEC_FW_IMG_TYPE_IMG_GEN4 = 0x4,
	SWITCHTEC_FW_IMG_TYPE_NVLOG_GEN4 = 0x5,
	SWITCHTEC_FW_IMG_TYPE_SEEPROM_GEN4 = 0xFE,
	SWITCHTEC_FW_IMG_TYPE_UNKNOWN_GEN4,
};

struct switchtec_fw_metadata_gen4 {
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
	uint8_t rsvd2[3];
	uint32_t xml_version;
	uint32_t relocatable_img_len;
	uint32_t link_addr;
	uint32_t header_crc;
};

struct switchtec_flash_info_gen4 {
	uint32_t firmware_version;
	uint32_t flash_size;
	uint16_t device_id;
	uint8_t ecc_enable;
	uint8_t rsvd1;
	uint8_t running_bl2_flag;
	uint8_t running_cfg_flag;
	uint8_t running_img_flag;
	uint8_t running_key_flag;
	uint32_t rsvd2[12];
	struct switchtec_flash_part_info_gen4 {
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
	} map0, map1, keyman0, keyman1, bl20, bl21, cfg0, cfg1,
	  img0, img1, nvlog, vendor[8];
};

int switchtec_fw_img_write_hdr_gen4(int fd, struct switchtec_fw_image_info *info)
{
	int ret;
	struct switchtec_fw_metadata_gen4 *hdr = info->metadata;

	ret = write(fd, hdr, sizeof(*hdr));
	if (ret < 0)
		return ret;

	return lseek(fd, info->part_body_offset, SEEK_SET);
}

static const enum switchtec_fw_image_part_id_gen4
switchtec_fw_partitions_gen4[] = {
	SWITCHTEC_FW_PART_ID_G4_MAP0,
	SWITCHTEC_FW_PART_ID_G4_MAP1,
	SWITCHTEC_FW_PART_ID_G4_KEY0,
	SWITCHTEC_FW_PART_ID_G4_KEY1,
	SWITCHTEC_FW_PART_ID_G4_BL20,
	SWITCHTEC_FW_PART_ID_G4_BL21,
	SWITCHTEC_FW_PART_ID_G4_CFG0,
	SWITCHTEC_FW_PART_ID_G4_CFG1,
	SWITCHTEC_FW_PART_ID_G4_IMG0,
	SWITCHTEC_FW_PART_ID_G4_IMG1,
	SWITCHTEC_FW_PART_ID_G4_NVLOG,
	SWITCHTEC_FW_PART_ID_G4_SEEPROM,
};

static enum switchtec_fw_type
switchtec_fw_id_to_type(const struct switchtec_fw_image_info *info)
{
	switch (info->part_id) {
	case SWITCHTEC_FW_PART_ID_G4_MAP0: return SWITCHTEC_FW_TYPE_MAP;
	case SWITCHTEC_FW_PART_ID_G4_MAP1: return SWITCHTEC_FW_TYPE_MAP;
	case SWITCHTEC_FW_PART_ID_G4_KEY0: return SWITCHTEC_FW_TYPE_KEY;
	case SWITCHTEC_FW_PART_ID_G4_KEY1: return SWITCHTEC_FW_TYPE_KEY;
	case SWITCHTEC_FW_PART_ID_G4_BL20: return SWITCHTEC_FW_TYPE_BL2;
	case SWITCHTEC_FW_PART_ID_G4_BL21: return SWITCHTEC_FW_TYPE_BL2;
	case SWITCHTEC_FW_PART_ID_G4_CFG0: return SWITCHTEC_FW_TYPE_CFG;
	case SWITCHTEC_FW_PART_ID_G4_CFG1: return SWITCHTEC_FW_TYPE_CFG;
	case SWITCHTEC_FW_PART_ID_G4_IMG0: return SWITCHTEC_FW_TYPE_IMG;
	case SWITCHTEC_FW_PART_ID_G4_IMG1: return SWITCHTEC_FW_TYPE_IMG;
	case SWITCHTEC_FW_PART_ID_G4_NVLOG: return SWITCHTEC_FW_TYPE_NVLOG;
	case SWITCHTEC_FW_PART_ID_G4_SEEPROM: return SWITCHTEC_FW_TYPE_SEEPROM;
	default: return SWITCHTEC_FW_TYPE_UNKNOWN;
	}
}

static int switchtec_fw_info_metadata(struct switchtec_dev *dev,
					   struct switchtec_fw_image_info *inf)
{
	struct switchtec_fw_metadata_gen4 *metadata;
	struct {
		uint8_t subcmd;
		uint8_t part_id;
	} subcmd = {
		.subcmd = MRPC_PART_INFO_GET_METADATA,
		.part_id = inf->part_id,
	};
	int ret;

	if (inf->part_id == SWITCHTEC_FW_PART_ID_G4_NVLOG)
		return 1;
	if (inf->part_id == SWITCHTEC_FW_PART_ID_G4_SEEPROM)
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
				       struct switchtec_flash_info_gen4 *all)
{
	struct switchtec_flash_part_info_gen4 *part_info;
	int ret;

	switch(inf->part_id) {
	case SWITCHTEC_FW_PART_ID_G4_MAP0:
		part_info = &all->map0;
		break;
	case SWITCHTEC_FW_PART_ID_G4_MAP1:
		part_info = &all->map1;
		break;
	case SWITCHTEC_FW_PART_ID_G4_KEY0:
		part_info = &all->keyman0;
		break;
	case SWITCHTEC_FW_PART_ID_G4_KEY1:
		part_info = &all->keyman1;
		break;
	case SWITCHTEC_FW_PART_ID_G4_BL20:
		part_info = &all->bl20;
		break;
	case SWITCHTEC_FW_PART_ID_G4_BL21:
		part_info = &all->bl21;
		break;
	case SWITCHTEC_FW_PART_ID_G4_IMG0:
		part_info = &all->img0;
		break;
	case SWITCHTEC_FW_PART_ID_G4_IMG1:
		part_info = &all->img1;
		break;
	case SWITCHTEC_FW_PART_ID_G4_CFG0:
		part_info = &all->cfg0;
		break;
	case SWITCHTEC_FW_PART_ID_G4_CFG1:
		part_info = &all->cfg1;
		break;
	case SWITCHTEC_FW_PART_ID_G4_NVLOG:
		part_info = &all->nvlog;
		break;
	case SWITCHTEC_FW_PART_ID_G4_SEEPROM:
		if (switchtec_gen(dev) < SWITCHTEC_GEN5)
			return 0;

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

int switchtec_fw_part_info_gen4(struct switchtec_dev *dev, int nr_info, struct switchtec_fw_image_info *info)
{
	int ret;
	int i;
	uint8_t subcmd = MRPC_PART_INFO_GET_ALL_INFO;
	struct switchtec_flash_info_gen4 all_info_gen4;

	if (info == NULL || nr_info == 0)
		return -EINVAL;

	ret = switchtec_cmd(dev, MRPC_PART_INFO, &subcmd,
				sizeof(subcmd), &all_info_gen4,
				sizeof(all_info_gen4));
	if (ret)
		return ret;
	all_info_gen4.firmware_version =
		le32toh(all_info_gen4.firmware_version);
	all_info_gen4.flash_size = le32toh(all_info_gen4.flash_size);
	all_info_gen4.device_id = le16toh(all_info_gen4.device_id);

	for (i = 0; i < nr_info; i++) {
		struct switchtec_fw_image_info *inf = &info[i];
		ret = 0;

		inf->gen = dev->gen;
		inf->type = switchtec_fw_id_to_type(inf);
		inf->active = false;
		inf->running = false;
		inf->valid = false;
		ret = switchtec_fw_part_info_helper(dev, inf, &all_info_gen4);
		
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

struct switchtec_fw_part_summary *switchtec_fw_part_summary_gen4(struct switchtec_dev *dev)
{
	struct switchtec_fw_part_summary *summary;
	struct switchtec_fw_image_info **infp;
	struct switchtec_fw_part_type *type;
	int nr_info, nr_mcfg = 16;
	size_t st_sz;
	int ret, i;

	nr_info = ARRAY_SIZE(switchtec_fw_partitions_gen4);

	st_sz = sizeof(*summary) + sizeof(*summary->all) * (nr_info + nr_mcfg);

	summary = malloc(st_sz);
	if (!summary)
		return NULL;

	memset(summary, 0, st_sz);
	summary->nr_info = nr_info;

	for (i = 0; i < nr_info; i++)
		summary->all[i].part_id =
			switchtec_fw_partitions_gen4[i];

	ret = switchtec_fw_part_info_gen4(dev, nr_info, summary->all);
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

static enum switchtec_fw_image_part_id_gen4 hdr_type2_id_gen4(uint32_t type)
{
	switch (type) {
	case SWITCHTEC_FW_IMG_TYPE_MAP_GEN4:
		return SWITCHTEC_FW_PART_ID_G4_MAP0;

	case SWITCHTEC_FW_IMG_TYPE_KEYMAN_GEN4:
		return SWITCHTEC_FW_PART_ID_G4_KEY0;

	case SWITCHTEC_FW_IMG_TYPE_BL2_GEN4:
		return SWITCHTEC_FW_PART_ID_G4_BL20;

	case SWITCHTEC_FW_IMG_TYPE_CFG_GEN4:
		return SWITCHTEC_FW_PART_ID_G4_CFG0;

	case SWITCHTEC_FW_IMG_TYPE_IMG_GEN4:
		return SWITCHTEC_FW_PART_ID_G4_IMG0;

	case SWITCHTEC_FW_IMG_TYPE_NVLOG_GEN4:
		return SWITCHTEC_FW_PART_ID_G4_NVLOG;

	case SWITCHTEC_FW_IMG_TYPE_SEEPROM_GEN4:
		return SWITCHTEC_FW_PART_ID_G4_SEEPROM;

	default:
		return -1;
	}
}

int switchtec_fw_file_info_gen4(int fd, struct switchtec_fw_image_info *info)
{
	int ret;
	struct switchtec_fw_metadata_gen4 hdr = {};
	uint8_t exp_zero[4] = {};
	uint32_t version;
	int part_id;

	ret = read(fd, &hdr, sizeof(hdr));
	lseek(fd, 0, SEEK_SET);

	if (ret != sizeof(hdr))
		goto invalid_file;

	part_id = hdr_type2_id_gen4(le32toh(hdr.type));

	if (part_id < 0)
		goto invalid_file;

	info->part_id = part_id;

	info->image_crc = le32toh(hdr.image_crc);
	version = le32toh(hdr.version);
	version_to_string(version, info->version, sizeof(info->version));
	info->image_len = le32toh(hdr.image_len);
	info->gen = switchtec_fw_version_to_gen(version);

	info->type = switchtec_fw_id_to_type(info);

	info->secure_version = le32toh(hdr.secure_version);
	info->signed_image = !!memcmp(hdr.public_key_exponent, exp_zero, 4);

	return 0;

invalid_file:
	errno = ENOEXEC;
	return -errno;
}

int switchtec_get_device_id_bl2_gen4(struct switchtec_dev *dev,
			        unsigned short *device_id)
{
	int ret;
	uint8_t subcmd = MRPC_PART_INFO_GET_ALL_INFO;
	struct switchtec_flash_info_gen4 all_info;

	ret = switchtec_cmd(dev, MRPC_PART_INFO, &subcmd,
			    sizeof(subcmd), &all_info,
			    sizeof(all_info));
	
	*device_id = le16toh(all_info.device_id);

	return ret;
}

int switchtec_fw_toggle_active_partition_gen4(struct switchtec_dev *dev,
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
	} cmd;

	if (switchtec_boot_phase(dev) == SWITCHTEC_BOOT_PHASE_BL2) {
		cmd_id = MRPC_FW_TX;
		cmd.subcmd = MRPC_FW_TX_TOGGLE;
	} else {
		cmd_id = MRPC_FWDNLD;
		cmd.subcmd = MRPC_FWDNLD_TOGGLE;
	}

	cmd.toggle_bl2 = !!toggle_bl2;
	cmd.toggle_key = !!toggle_key;
	cmd.toggle_fw = !!toggle_fw;
	cmd.toggle_cfg = !!toggle_cfg;
	cmd_size = sizeof(cmd);

	return switchtec_cmd(dev, cmd_id, &cmd, cmd_size, NULL, 0);
}

struct cmd_fwdl {
	struct cmd_fwdl_hdr {
		uint8_t subcmd;
		uint8_t dont_activate;
		uint8_t reserved[2];
		uint32_t offset;
		uint32_t img_length;
		uint32_t blk_length;
	} hdr;
	uint8_t data[MRPC_MAX_DATA_LEN - sizeof(struct cmd_fwdl_hdr)];
};

static int switchtec_fw_dlstatus(struct switchtec_dev *dev,
				 enum switchtec_fw_dlstatus *status,
				 enum mrpc_bg_status *bgstatus)
{
	uint32_t cmd = MRPC_FWDNLD;
	uint32_t subcmd = MRPC_FWDNLD_GET_STATUS;
	struct {
		uint8_t dlstatus;
		uint8_t bgstatus;
		uint16_t reserved;
	} result;
	int ret;

	if (switchtec_boot_phase(dev) != SWITCHTEC_BOOT_PHASE_FW)
		cmd = get_fw_tx_id(dev);

	ret = switchtec_cmd(dev, cmd, &subcmd, sizeof(subcmd),
			    &result, sizeof(result));

	if (ret)
		return ret;

	if (status != NULL)
		*status = result.dlstatus;

	if (bgstatus != NULL)
		*bgstatus = result.bgstatus;

	return 0;
}

static int switchtec_fw_wait(struct switchtec_dev *dev,
			     enum switchtec_fw_dlstatus *status)
{
	enum mrpc_bg_status bgstatus;
	int ret;

	do {
		// Delay slightly to avoid interrupting the firmware too much
		usleep(5000);

		ret = switchtec_fw_dlstatus(dev, status, &bgstatus);
		if (ret < 0)
			return ret;

		if (bgstatus == MRPC_BG_STAT_OFFSET)
			return SWITCHTEC_DLSTAT_ERROR_OFFSET;

		if (bgstatus == MRPC_BG_STAT_ERROR) {
			if (*status != SWITCHTEC_DLSTAT_INPROGRESS &&
			    *status != SWITCHTEC_DLSTAT_COMPLETES &&
			    *status != SWITCHTEC_DLSTAT_SUCCESS_FIRM_ACT &&
			    *status != SWITCHTEC_DLSTAT_SUCCESS_DATA_ACT)
				return *status;
			else
				return SWITCHTEC_DLSTAT_ERROR_PROGRAM;
		}

	} while (bgstatus == MRPC_BG_STAT_INPROGRESS);

	return 0;
}


int switchtec_fw_write_file_gen4(struct switchtec_dev *dev, FILE *fimg,
			    int dont_activate, int force,
			    void (*progress_callback)(int cur, int tot))
{
	enum switchtec_fw_dlstatus status;
	enum mrpc_bg_status bgstatus;
	ssize_t image_size, offset = 0;
	int ret;
	struct cmd_fwdl cmd = {};
	uint32_t cmd_id = MRPC_FWDNLD;

	if (switchtec_boot_phase(dev) != SWITCHTEC_BOOT_PHASE_FW)
		cmd_id = get_fw_tx_id(dev);

	ret = fseek(fimg, 0, SEEK_END);
	if (ret)
		return -errno;
	image_size = ftell(fimg);
	if (image_size < 0)
		return -errno;
	ret = fseek(fimg, 0, SEEK_SET);
	if (ret)
		return -errno;

	switchtec_fw_dlstatus(dev, &status, &bgstatus);

	if (!force && status == SWITCHTEC_DLSTAT_INPROGRESS) {
		errno = EBUSY;
		return -EBUSY;
	}

	if (bgstatus == MRPC_BG_STAT_INPROGRESS) {
		errno = EBUSY;
		return -EBUSY;
	}

	if (switchtec_boot_phase(dev) == SWITCHTEC_BOOT_PHASE_BL2)
		cmd.hdr.subcmd = MRPC_FW_TX_FLASH;
	else
		cmd.hdr.subcmd = MRPC_FWDNLD_DOWNLOAD;

	cmd.hdr.dont_activate = !!dont_activate;
	cmd.hdr.img_length = htole32(image_size);

	while (offset < image_size) {
		ssize_t blklen = fread(&cmd.data, 1, sizeof(cmd.data), fimg);

		if (blklen == 0) {
			ret = ferror(fimg);
			if (ret)
				return ret;
			break;
		}

		cmd.hdr.offset = htole32(offset);
		cmd.hdr.blk_length = htole32(blklen);

		ret = switchtec_cmd(dev, cmd_id, &cmd, sizeof(cmd),
				    NULL, 0);

		if (ret)
			return ret;

		ret = switchtec_fw_wait(dev, &status);
		if (ret != 0)
			return ret;

		offset += le32toh(cmd.hdr.blk_length);

		if (progress_callback)
			progress_callback(offset, image_size);
	}

	if (status == SWITCHTEC_DLSTAT_COMPLETES)
		return 0;

	if (status == SWITCHTEC_DLSTAT_SUCCESS_FIRM_ACT)
		return 0;

	if (status == SWITCHTEC_DLSTAT_SUCCESS_DATA_ACT)
		return 0;

	if (status == 0)
		return SWITCHTEC_DLSTAT_HARDWARE_ERR;

	return status;
}

int switchtec_fw_read_gen4(struct switchtec_dev *dev, unsigned long addr, size_t len, void *buf)
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

int switchtec_fw_read_fd_gen4(struct switchtec_dev *dev, int fd, unsigned long addr, 
			      size_t len, void (*progress_callback)(int cur, int tot))
{
	int ret;
	unsigned char buf[(MRPC_MAX_DATA_LEN-8)*4];
	size_t read = 0;
	size_t total_len = len;
	size_t total_wrote;
	ssize_t wrote;

	while(len) {
		size_t chunk_len = len;
		if (chunk_len > sizeof(buf))
			chunk_len = sizeof(buf);

		ret = switchtec_fw_read(dev, addr, chunk_len, buf);
		if (ret < 0)
			return ret;

		total_wrote = 0;
		while (total_wrote < ret) {
			wrote = write(fd, &buf[total_wrote],
				      ret - total_wrote);
			if (wrote < 0)
				return -1;
			total_wrote += wrote;
		}

		read += ret;
		addr += ret;
		len -= ret;

		if (progress_callback)
			progress_callback(read, total_len);
	}

	return read;
}

int switchtec_fw_map_get_active_gen4(struct switchtec_dev *dev, struct switchtec_fw_image_info *info)
{
	uint32_t map0_update_index;
	uint32_t map1_update_index;
	int ret;

	ret = switchtec_fw_read(dev, SWITCHTEC_FLASH_MAP0_PART_START,
				sizeof(uint32_t), &map0_update_index);
	if (ret < 0)
		return ret;

	ret = switchtec_fw_read(dev, SWITCHTEC_FLASH_MAP1_PART_START,
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

int switchtec_fw_body_read_fd_gen4(struct switchtec_dev *dev, int fd,
			      struct switchtec_fw_image_info *info,
			      void (*progress_callback)(int cur, int tot))
{
	return switchtec_fw_read_fd_gen4(dev, fd, info->part_addr + info->part_body_offset,
					 info->image_len, progress_callback);
}