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

#define SWITCHTEC_MAX_PARTS  48
#define SWITCHTEC_MAX_PORTS  48
#define SWITCHTEC_MAX_STACKS 8
#define SWITCHTEC_MAX_EVENT_COUNTERS 64

struct switchtec_device_info {
	char name[256];
	char pci_dev[256];
	char product_id[32];
	char product_rev[8];
	char fw_version[32];
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

enum switchtec_fw_image_type {
	SWITCHTEC_FW_TYPE_BOOT = 0x0,
	SWITCHTEC_FW_TYPE_MAP0 = 0x1,
	SWITCHTEC_FW_TYPE_MAP1 = 0x2,
	SWITCHTEC_FW_TYPE_IMG0 = 0x3,
	SWITCHTEC_FW_TYPE_DAT0 = 0x4,
	SWITCHTEC_FW_TYPE_DAT1 = 0x5,
	SWITCHTEC_FW_TYPE_NVLOG = 0x6,
	SWITCHTEC_FW_TYPE_IMG1 = 0x7,
};

struct switchtec_fw_image_info {
	enum switchtec_fw_image_type type;
	char version[32];
	size_t image_addr;
	size_t image_len;
	unsigned long crc;
	int active;

};

struct switchtec_fw_footer {
	char magic[4];
	uint32_t image_len;
	uint32_t load_addr;
	uint32_t version;
	uint32_t rsvd;
	uint32_t header_crc;
	uint32_t image_crc;
};

struct switchtec_status {
	unsigned char partition;
	unsigned char stack;
	unsigned char upstream_port;
	unsigned char stk_port_id;
	unsigned char phys_port_id;
	unsigned char log_port_id;

	unsigned char cfg_lnk_width;
	unsigned char neg_lnk_width;
	unsigned char link_up;
	unsigned char link_rate;
	unsigned char ltssm;
	const char *ltssm_str;
};

enum switchtec_event {
	SWITCHTEC_GLOBAL_EVT_STACK_ERR = 1 << 0,
	SWITCHTEC_GLOBAL_EVT_PPU_ERR = 1 << 1,
	SWITCHTEC_GLOBAL_EVT_ISP_ERROR = 1 << 2,
	SWITCHTEC_GLOBAL_EVT_SYS_RESET = 1 << 3,
	SWITCHTEC_GLOBAL_EVT_FIRMWARE_ERR = 1 << 4,
	SWITCHTEC_GLOBAL_EVT_FIRMWARE_NMI = 1 << 5,
	SWITCHTEC_GLOBAL_EVT_FIRMWARE_NON_FATAL_ERR = 1 << 6,
	SWITCHTEC_GLOBAL_EVT_FIRMWARE_FATAL_ERR = 1 << 7,
	SWITCHTEC_GLOBAL_EVT_TWI_MRPC_COMP = 1 << 8,
	SWITCHTEC_GLOBAL_EVT_TWI_MRPC_COMP_ASYNC = 1 << 9,
	SWITCHTEC_GLOBAL_EVT_CLI_MRPC_COMP = 1 << 10,
	SWITCHTEC_GLOBAL_EVT_CLI_MRPC_COMP_ASYNC = 1 << 11,
	SWITCHTEC_GLOBAL_EVT_GPIO_INT = 1 << 12,

	SWITCHTEC_PART_EVT_RESET = 1 << 0,
	SWITCHTEC_PART_EVT_MRPC_COMP_ASYNC = 1 << 2,
	SWITCHTEC_PART_EVT_DYN_PART_BIND = 1 << 3,

	SWITCHTEC_PORT_EVT_AER_IN_P2P = 1 << 0,
	SWITCHTEC_PORT_EVT_AER_INVEP = 1 << 1,
	SWITCHTEC_PORT_EVT_DPC = 1 << 2,
	SWITCHTEC_PORT_EVT_CTS = 1 << 3,
	SWITCHTEC_PORT_EVT_HOTPLUG = 1 << 5,
	SWITCHTEC_PORT_EVT_IER = 1 << 6,
	SWITCHTEC_PORT_EVT_THRESHOLD = 1 << 7,
	SWITCHTEC_PORT_EVT_PWR_MGMT = 1 << 8,
	SWITCHTEC_PORT_EVT_TLP_THROTTLING = 1 << 9,
	SWITCHTEC_PORT_EVT_FORCE_SPEED = 1 << 10,
	SWITCHTEC_PORT_EVT_CREDIT_TIMEOUT = 1 << 11,
	SWITCHTEC_PORT_EVT_LINK_STATE = 1 << 12,
};

enum switchtec_event_type {
	SWITCHTEC_GLOBAL_EVT,
	SWITCHTEC_PART_EVT,
	SWITCHTEC_PORT_EVT,
};

struct switchtec_event_summary {
	uint64_t global_summary;
	uint64_t part_event_bitmap;
	unsigned local_part_event_summary;
	unsigned part_event_summary[SWITCHTEC_MAX_PARTS];
	unsigned port_event_summary[SWITCHTEC_MAX_PORTS];
};

enum switchtec_evcntr_type_mask {
	UNSUP_REQ_ERR = 1 << 0,
	ECRC_ERR = 1 << 1,
	MALFORM_TLP_ERR = 1 << 2,
	RCVR_OFLOW_ERR = 1 << 3,
	CMPLTR_ABORT_ERR = 1 << 4,
	POISONED_TLP_ERR = 1 << 5,
	SURPRISE_DOWN_ERR = 1 << 6,
	DATA_LINK_PROTO_ERR = 1 << 7,
	HDR_LOG_OFLOW_ERR = 1 << 8,
	UNCOR_INT_ERR = 1 << 9,
	REPLAY_TMR_TIMEOUT = 1 << 10,
	REPLAY_NUM_ROLLOVER = 1 << 11,
	BAD_DLPP = 1 << 12,
	BAD_TLP = 1 << 13,
	RCVR_ERR = 1 << 14,
	RCV_FATAL_MSG = 1 << 15,
	RCV_NON_FATAL_MSG = 1 << 16,
	RCV_CORR_MSG = 1 << 17,
	NAK_RCVD = 1 << 18,
	RULE_TABLE_HIT = 1 << 19,
        POSTED_TLP = 1 << 20,
	COMP_TLP = 1 << 21,
	NON_POSTED_TLP = 1 << 22,
	ALL_ERRORS = (UNSUP_REQ_ERR | ECRC_ERR | MALFORM_TLP_ERR |
		      RCVR_OFLOW_ERR | CMPLTR_ABORT_ERR | POISONED_TLP_ERR |
		      SURPRISE_DOWN_ERR | DATA_LINK_PROTO_ERR |
		      HDR_LOG_OFLOW_ERR | UNCOR_INT_ERR |
		      REPLAY_TMR_TIMEOUT | REPLAY_NUM_ROLLOVER |
		      BAD_DLPP | BAD_TLP | RCVR_ERR | RCV_FATAL_MSG |
		      RCV_NON_FATAL_MSG | RCV_CORR_MSG | NAK_RCVD),
	ALL_TLPS = (POSTED_TLP | COMP_TLP | NON_POSTED_TLP),
	ALL = (1 << 23) - 1,
};

extern const struct switchtec_evcntr_type_list {
	enum switchtec_evcntr_type_mask mask;
	const char *name;
	const char *help;
} switchtec_evcntr_type_list[];

struct switchtec_evcntr_setup {
	unsigned port_mask;
	enum switchtec_evcntr_type_mask type_mask;
	int egress;
	unsigned threshold;
};

struct switchtec_dev *switchtec_open(const char * path);
void switchtec_close(struct switchtec_dev *dev);
const char *switchtec_name(struct switchtec_dev *dev);
int switchtec_fd(struct switchtec_dev *dev);
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
int switchtec_status(struct switchtec_dev *dev,
		     struct switchtec_status **status);
void switchtec_perror(const char *str);

int switchtec_event_wait(struct switchtec_dev *dev, int timeout_ms);
int switchtec_event_summary(struct switchtec_dev *dev,
			    struct switchtec_event_summary *sum);
int switchtec_event_check(struct switchtec_dev *dev,
			  struct switchtec_event_summary *check);
int switchtec_event_wait_for(struct switchtec_dev *dev,
			     struct switchtec_event_summary *wait_for,
			     int timeout_ms);
int switchtec_event_get(struct switchtec_dev *dev,
			enum switchtec_event_type t,
			enum switchtec_event e,
			int index, int clear,
			uint32_t *hdr,
			uint32_t data[5]);

int switchtec_fw_dlstatus(struct switchtec_dev *dev,
			  enum switchtec_fw_dlstatus *status,
			  enum mrpc_bg_status *bgstatus);
int switchtec_fw_wait(struct switchtec_dev *dev,
		      enum switchtec_fw_dlstatus *status);
int switchtec_fw_toggle_active_partition(struct switchtec_dev *dev,
					 int toggle_fw, int toggle_cfg);
int switchtec_fw_write_file(struct switchtec_dev *dev, int img_fd,
			    int dont_activate,
			    void (*progress_callback)(int cur, int tot));
int switchtec_fw_read_file(struct switchtec_dev *dev, int fd,
			   unsigned long addr, size_t len,
			   void (*progress_callback)(int cur, int tot));
int switchtec_fw_read(struct switchtec_dev *dev, unsigned long addr,
		      size_t len, void *buf);
int switchtec_fw_read_footer(struct switchtec_dev *dev,
			     unsigned long partition_start,
			     size_t partition_len,
			     struct switchtec_fw_footer *ftr,
			     char *version, size_t version_len);
void switchtec_fw_perror(const char *s, int ret);
int switchtec_fw_image_info(int fd, struct switchtec_fw_image_info *info);
const char *switchtec_fw_image_type(const struct switchtec_fw_image_info *info);
int switchtec_fw_part_info(struct switchtec_dev *dev, int nr_info,
			   struct switchtec_fw_image_info *info);
int switchtec_fw_part_act_info(struct switchtec_dev *dev,
			       struct switchtec_fw_image_info *act_img,
			       struct switchtec_fw_image_info *inact_img,
			       struct switchtec_fw_image_info *act_cfg,
			       struct switchtec_fw_image_info *inact_cfg);
int switchtec_fw_img_write_hdr(int fd, struct switchtec_fw_footer *ftr,
			       enum switchtec_fw_image_type type);

int switchtec_evcntr_type_count(void);
const char *switchtec_evcntr_type_str(int *type_mask);
int switchtec_evcntr_setup(struct switchtec_dev *dev, unsigned stack_id,
			   unsigned cntr_id,
			   struct switchtec_evcntr_setup *setup);
int switchtec_evcntr_get_setup(struct switchtec_dev *dev, unsigned stack_id,
			       unsigned cntr_id, unsigned nr_cntrs,
			       struct switchtec_evcntr_setup *res);
int switchtec_evcntr_get(struct switchtec_dev *dev, unsigned stack_id,
			 unsigned cntr_id, unsigned nr_cntrs, unsigned *res,
			 int clear);
int switchtec_evcntr_get_both(struct switchtec_dev *dev, unsigned stack_id,
			      unsigned cntr_id, unsigned nr_cntrs,
			      struct switchtec_evcntr_setup *setup,
			      unsigned *counts, int clear);
int switchtec_evcntr_wait(struct switchtec_dev *dev, int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif
