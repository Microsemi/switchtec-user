/*
 * Microsemi Switchtec PCIe Driver
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

#ifndef _UAPI_LINUX_SWITCHTEC_IOCTL_H
#define _UAPI_LINUX_SWITCHTEC_IOCTL_H

#include <linux/types.h>

struct switchtec_ioctl_fw_info {
	__u32 flash_part_map_upd_idx;

	struct switchtec_flash_partition_info {
		__u32 address;
		__u32 build_version;
		__u32 build_string;
	} active_main_fw;

	struct switchtec_flash_partition_info active_cfg;
	struct switchtec_flash_partition_info inactive_main_fw;
	struct switchtec_flash_partition_info inactive_cfg;
};

#define SWITCHTEC_IOCTL_FW_INFO _IOR('W', 0x40, struct switchtec_ioctl_fw_info)

#endif
