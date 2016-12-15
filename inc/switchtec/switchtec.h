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

#ifndef LIBSWITCHTEC_SWITCHTEC_H
#define LIBSWITCHTEC_SWITCHTEC_H

#include "mrpc.h"

#include <linux/limits.h>

#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct switchtec_dev;

struct switchtec_device_info {
	char name[256];
	char pci_dev[256];
	char model[256];
	char path[PATH_MAX];
};

enum switchtec_fw_dlstatus {
	SWITCHTEC_DLSTAT_READY = 0,
	SWITCHTEC_DLSTAT_INPROGRESS = 1,
	SWITCHTEC_DLSTAT_HEADER_INCORRECT = 2,
	SWITCHTEC_DLSTAT_OFFSET_INCORRECT = 3,
	SWITCHTEC_DLSTAT_CRC_INCORRECT = 4,
	SWITCHTEC_DLSTAT_LENGTH_INCORRECT = 5,
	SWITCHTEC_DLSTAT_HARDWARE_ERR = 6,
	SWITCHTEC_DLSTAT_COMPLETES = 7,
	SWITCHTEC_DLSTAT_SUCCESS_FIRM_ACT = 8,
	SWITCHTEC_DLSTAT_SUCCESS_DATA_ACT = 9,
};

struct switchtec_fw_image_info {
	enum {
		SWITCHTEC_FW_TYPE_BOOT = 0x0,
		SWITCHTEC_FW_TYPE_MAP = 0x2,
		SWITCHTEC_FW_TYPE_IMG = 0x6,
		SWITCHTEC_FW_TYPE_CFG = 0x21,
	} type;

	char version[32];
	size_t image_len;
	unsigned long crc;
};

struct switchtec_fw_part_info {
	uint32_t flash_part_map_upd_idx;

	struct switchtec_fw_part_info_sec {
		uint32_t address;
		char version[32];
	} active_main_fw;

	struct switchtec_fw_part_info_sec active_cfg;
	struct switchtec_fw_part_info_sec inactive_main_fw;
	struct switchtec_fw_part_info_sec inactive_cfg;
};

struct switchtec_dev *switchtec_open(const char * path);
void switchtec_close(struct switchtec_dev *dev);
int switchtec_list(struct switchtec_device_info **devlist);
int switchtec_get_fw_version(struct switchtec_dev *dev, char *buf,
			     size_t buflen);

int switchtec_submit_cmd(struct switchtec_dev *dev, uint32_t cmd,
			 const void *payload, size_t payload_len);

int switchtec_read_resp(struct switchtec_dev *dev, void *resp,
			size_t resp_len);

int switchtec_cmd(struct switchtec_dev *dev, uint32_t cmd,
		  const void *payload, size_t payload_len, void *resp,
		  size_t resp_len);

int switchtec_echo(struct switchtec_dev *dev, uint32_t input, uint32_t *output);
int switchtec_hard_reset(struct switchtec_dev *dev);


int switchtec_fw_dlstatus(struct switchtec_dev *dev,
			  enum switchtec_fw_dlstatus *status,
			  enum mrpc_bg_status *bgstatus);
int switchtec_fw_wait(struct switchtec_dev *dev,
		      enum switchtec_fw_dlstatus *status);
int switchtec_fw_toggle_active_partition(struct switchtec_dev *dev,
					 int toggle_fw, int toggle_cfg);
int switchtec_fw_update(struct switchtec_dev *dev, int img_fd,
			void (*progress_callback)(int cur, int tot));
void switchtec_fw_perror(const char *s, int ret);
int switchtec_fw_image_info(int fd, struct switchtec_fw_image_info *info);
const char *switchtec_fw_image_type(const struct switchtec_fw_image_info *info);
int switchtec_fw_part_info(struct switchtec_dev *dev,
			   struct switchtec_fw_part_info *info);


#ifdef __cplusplus
}
#endif

#endif
