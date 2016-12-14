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

#include <errno.h>
#include <stdio.h>

int switchtec_fw_dlstatus(struct switchtec_dev * dev,
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

int switchtec_fw_wait(struct switchtec_dev * dev,
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

int switchtec_fw_update(struct switchtec_dev * dev, int img_fd,
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
		ssize_t blklen =read(img_fd, &cmd.data,
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
