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

enum switchtec_ioctl_partition {
	SWITCHTEC_IOCTL_PART_CFG0,
	SWITCHTEC_IOCTL_PART_CFG1,
	SWITCHTEC_IOCTL_PART_IMG0,
	SWITCHTEC_IOCTL_PART_IMG1,
	SWITCHTEC_IOCTL_PART_NVLOG,
	SWITCHTEC_IOCTL_PART_VENDOR0,
	SWITCHTEC_IOCTL_PART_VENDOR1,
	SWITCHTEC_IOCTL_PART_VENDOR2,
	SWITCHTEC_IOCTL_PART_VENDOR3,
	SWITCHTEC_IOCTL_PART_VENDOR4,
	SWITCHTEC_IOCTL_PART_VENDOR5,
	SWITCHTEC_IOCTL_PART_VENDOR6,
	SWITCHTEC_IOCTL_PART_VENDOR7,
	SWITCHTEC_IOCTL_NUM_PARTITIONS,
};

struct switchtec_ioctl_fw_info {
	__u32 flash_length;

	struct {
		__u32 address;
		__u32 length;
		__u32 active;
	} partition[SWITCHTEC_IOCTL_NUM_PARTITIONS];
};

#define SWITCHTEC_IOCTL_FW_INFO _IOR('W', 0x40, struct switchtec_ioctl_fw_info)

#endif
