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
 * Based partially on the ntb_hw_amd driver
 * Copyright (C) 2016 Advanced Micro Devices, Inc. All Rights Reserved.
 *
 */

#ifndef LIBSWITCHTEC_LOG_H
#define LIBSWITCHTEC_LOG_H

#include "mrpc.h"
#include <stdint.h>
#include <stddef.h>

struct log_a_retr {
	uint8_t sub_cmd_id;
	uint8_t from_end;
	uint8_t reserved1[6];
	uint32_t count;
	uint32_t reserved2;
	uint32_t start;
};

struct log_a_data {
	uint32_t data[8];
};

struct log_a_retr_result {
	struct log_a_retr_hdr {
		uint8_t sub_cmd_id;
		uint8_t from_end;
		uint8_t reserved1[2];
		uint32_t total;
		uint32_t count;
		uint32_t remain;
		uint32_t next_start;
		uint32_t reserved2[3];
	} hdr;

	struct log_a_data data[(MRPC_MAX_DATA_LEN -
				sizeof(struct log_a_retr_hdr)) /
			       sizeof(struct log_a_data)];
};

struct log_b_retr {
	uint8_t sub_cmd_id;
	uint8_t reserved[3];
	uint32_t offset;
	uint32_t length;
};

struct log_b_retr_result {
	struct log_b_retr_hdr {
		uint8_t sub_cmd_id;
		uint8_t reserved[3];
		uint32_t length;
		uint32_t remain;
	} hdr;
	uint8_t data[MRPC_MAX_DATA_LEN - sizeof(struct log_b_retr_hdr)];
};

#endif
