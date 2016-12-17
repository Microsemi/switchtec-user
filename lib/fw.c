/*
 * Microsemi Switchtec(tm) PCIe Management Library
 * Copyright (c) 2016, Microsemi Corporation
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include "switchtec_priv.h"
#include "switchtec/switchtec.h"

#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/switchtec_ioctl.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

int switchtec_fw_dlstatus(struct switchtec_dev *dev,
			  enum switchtec_fw_dlstatus *status,
			  enum mrpc_bg_status *bgstatus)
{
	uint32_t subcmd = MRPC_FWDNLD_GET_STATUS;
	struct {
		uint8_t dlstatus;
		uint8_t bgstatus;
		uint16_t reserved;
	} result;
	int ret;

	ret = switchtec_cmd(dev, MRPC_FWDNLD, &subcmd, sizeof(subcmd),
			    &result, sizeof(result));

	if (ret < 0)
		return ret;

	if (status != NULL)
		*status = result.dlstatus;

	if (bgstatus != NULL)
		*bgstatus = result.bgstatus;

	return 0;
}

int switchtec_fw_wait(struct switchtec_dev *dev,
		      enum switchtec_fw_dlstatus *status)
{
	enum mrpc_bg_status bgstatus;
	int ret;

	do {
		ret = switchtec_fw_dlstatus(dev, status, &bgstatus);
		if (ret < 0)
			return ret;
		if (bgstatus == MRPC_BG_STAT_ERROR)
			return SWITCHTEC_DLSTAT_HARDWARE_ERR;

	} while (bgstatus == MRPC_BG_STAT_INPROGRESS);

	return 0;
}

int switchtec_fw_toggle_active_partition(struct switchtec_dev *dev,
					 int toggle_fw, int toggle_cfg)
{
	struct {
		uint8_t subcmd;
		uint8_t toggle_fw;
		uint8_t toggle_cfg;
	} cmd;

	cmd.subcmd = MRPC_FWDNLD_TOGGLE;
	cmd.toggle_fw = !!toggle_fw;
	cmd.toggle_cfg = !!toggle_cfg;

	return switchtec_cmd(dev, MRPC_FWDNLD, &cmd, sizeof(cmd),
			     NULL, 0);
}

int switchtec_fw_write_file(struct switchtec_dev *dev, int img_fd,
			    void (*progress_callback)(int cur, int tot))
{
	enum switchtec_fw_dlstatus status;
	enum mrpc_bg_status bgstatus;
	ssize_t image_size, offset = 0;
	int ret;

	struct {
		struct cmd_fwdl_hdr {
			uint8_t subcmd;
			uint8_t reserved[3];
			uint32_t offset;
			uint32_t img_length;
			uint32_t blk_length;
		} hdr;
		uint8_t data[MRPC_MAX_DATA_LEN - sizeof(struct cmd_fwdl_hdr)];
	} cmd = {};

	image_size = lseek(img_fd, 0, SEEK_END);
	if (image_size < 0)
		return -errno;
	lseek(img_fd, 0, SEEK_SET);

	switchtec_fw_dlstatus(dev, &status, &bgstatus);

	if (status == SWITCHTEC_DLSTAT_INPROGRESS)
		return -EBUSY;

	if (bgstatus == MRPC_BG_STAT_INPROGRESS)
		return -EBUSY;

	cmd.hdr.subcmd = MRPC_FWDNLD_DOWNLOAD;
	cmd.hdr.img_length = htole32(image_size);

	while (offset < image_size) {
		ssize_t blklen = read(img_fd, &cmd.data,
				      sizeof(cmd.data));

		if (blklen == -EAGAIN || blklen == -EWOULDBLOCK)
			continue;

		if (blklen < 0)
			return -errno;

		if (blklen == 0)
			break;

		cmd.hdr.offset = htole32(offset);
		cmd.hdr.blk_length = htole32(blklen);

		ret = switchtec_cmd(dev, MRPC_FWDNLD, &cmd, sizeof(cmd),
				    NULL, 0);

		if (ret < 0)
			return ret;

		ret = switchtec_fw_wait(dev, &status);
		if (ret < 0)
		    return ret;

		offset += cmd.hdr.blk_length;

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

void switchtec_fw_perror(const char *s, int ret)
{
	const char *msg;

	if (ret <= 0) {
		perror(s);
		return;
	}

	switch(ret) {
	case SWITCHTEC_DLSTAT_HEADER_INCORRECT:
		msg = "Header incorrect";  break;
	case SWITCHTEC_DLSTAT_OFFSET_INCORRECT:
		msg = "Offset incorrect";  break;
	case SWITCHTEC_DLSTAT_CRC_INCORRECT:
		msg = "CRC incorrect";  break;
	case SWITCHTEC_DLSTAT_LENGTH_INCORRECT:
		msg = "Length incorrect";  break;
	case SWITCHTEC_DLSTAT_HARDWARE_ERR:
		msg = "Hardware Error";  break;
	default:
		fprintf(stderr, "%s: Unknown Error (%d)\n", s, ret);
		return;
	}

	fprintf(stderr, "%s: %s\n", s, msg);
}

int switchtec_fw_image_info(int fd, struct switchtec_fw_image_info *info)
{
	int ret;
	struct {
		char magic[4];
		uint32_t image_len;
		uint32_t type;
		uint32_t rsvd1;
		uint32_t version;
		uint32_t rsvd2[10];
		uint32_t crc;
	} hdr;

	ret = read(fd, &hdr, sizeof(hdr));
	lseek(fd, 0, SEEK_SET);

	if (ret != sizeof(hdr))
		goto invalid_file;

	if (strcmp(hdr.magic, "PMC") != 0)
		goto invalid_file;

	if (info == NULL)
		return 0;

	info->type = hdr.type;
	info->crc = le32toh(hdr.crc);
	version_to_string(hdr.version, info->version, sizeof(info->version));
	info->image_len = le32toh(hdr.image_len);

	return 0;

invalid_file:
	errno = ENOEXEC;
	return -errno;
}

const char *switchtec_fw_image_type(const struct switchtec_fw_image_info *info)
{
	switch(info->type) {
	case SWITCHTEC_FW_TYPE_BOOT: return "BOOT";
	case SWITCHTEC_FW_TYPE_MAP0: return "MAP";
	case SWITCHTEC_FW_TYPE_MAP1: return "MAP";
	case SWITCHTEC_FW_TYPE_IMG0: return "IMG";
	case SWITCHTEC_FW_TYPE_IMG1: return "IMG";
	case SWITCHTEC_FW_TYPE_DAT0: return "DAT";
	case SWITCHTEC_FW_TYPE_DAT1: return "DAT";
	case SWITCHTEC_FW_TYPE_NVLOG: return "NVLOG";
	default: return "UNKNOWN";
	}
}

int switchtec_fw_part_info(struct switchtec_dev *dev,
			  struct switchtec_fw_part_info *info)
{
	int ret;
	struct switchtec_ioctl_fw_info ioctl_info;

	ret = ioctl(dev->fd, SWITCHTEC_IOCTL_FW_INFO, &ioctl_info);
	if (ret)
		return ret;

	#define fw_info_set(field) \
                info->field = ioctl_info.field

	fw_info_set(flash_part_map_upd_idx);
	fw_info_set(active_main_fw.address);
	fw_info_set(active_cfg.address);
	fw_info_set(inactive_main_fw.address);
	fw_info_set(inactive_cfg.address);

	#define fw_version_set(field)\
		version_to_string(ioctl_info.field.build_version, \
				  info->field.version, \
				  sizeof(info->field.version));

	fw_version_set(active_main_fw);
	fw_version_set(active_cfg);
	fw_version_set(inactive_main_fw);
	fw_version_set(inactive_cfg);

	return 0;
}
