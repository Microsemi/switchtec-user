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

struct switchtec_ioctl_event_summary {
	__u64 global_summary;
	__u64 part_event_bitmap;
	__u32 local_part_event_summary;
	__u32 part_event_summary[48];
	__u32 port_event_summary[255];
};

enum switchtec_ioctl_event {
	SWITCHTEC_IOCTL_EVENT_STACK_ERROR,
	SWITCHTEC_IOCTL_EVENT_PPU_ERROR,
	SWITCHTEC_IOCTL_EVENT_ISP_ERROR,
	SWITCHTEC_IOCTL_EVENT_TWI_MRPC_COMP,
	SWITCHTEC_IOCTL_EVENT_TWI_MRPC_COMP_ASYNC,
	SWITCHTEC_IOCTL_EVENT_CLI_MRPC_COMP,
	SWITCHTEC_IOCTL_EVENT_CLI_MRPC_COMP_ASYNC,
	SWITCHTEC_IOCTL_EVENT_GPIO_INT,
	SWITCHTEC_IOCTL_EVENT_PART_RESET,
	SWITCHTEC_IOCTL_EVENT_MRPC_COMP,
	SWITCHTEC_IOCTL_EVENT_MRPC_COMP_ASYNC,
	SWITCHTEC_IOCTL_EVENT_DYN_PART_BIND_COMP,
	SWITCHTEC_IOCTL_EVENT_AER_IN_P2P,
	SWITCHTEC_IOCTL_EVENT_AER_IN_VEP,
	SWITCHTEC_IOCTL_EVENT_DPC,
	SWITCHTEC_IOCTL_EVENT_CTS,
	SWITCHTEC_IOCTL_EVENT_HOTPLUG,
	SWITCHTEC_IOCTL_EVENT_IER,
	SWITCHTEC_IOCTL_EVENT_THRESH,
	SWITCHTEC_IOCTL_EVENT_POWER_MGMT,
	SWITCHTEC_IOCTL_EVENT_TLP_THROTTLING,
	SWITCHTEC_IOCTL_EVENT_FORCE_SPEED,
	SWITCHTEC_IOCTL_EVENT_CREDIT_TIMEOUT,
	SWITCHTEC_IOCTL_EVENT_LINK_STATE,
};

#define SWITCHTEC_IOCTL_EVENT_LOCAL_PART_IDX -1

struct switchtec_ioctl_event_info {
	__u32 event_id;
	__s32 index;
	__u32 clear;
	__u32 header;
	__u32 data[5];
};

#define SWITCHTEC_IOCTL_FW_INFO \
	_IOR('W', 0x40, struct switchtec_ioctl_fw_info)
#define SWITCHTEC_IOCTL_EVENT_SUMMARY \
	_IOR('W', 0x41, struct switchtec_ioctl_event_summary)
#define SWITCHTEC_IOCTL_EVENT_INFO \
	_IOWR('W', 0x42, struct switchtec_ioctl_event_info)

#endif
