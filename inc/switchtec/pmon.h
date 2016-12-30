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

#ifndef LIBSWITCHTEC_PMON_H
#define LIBSWITCHTEC_PMON_H

#include <stdint.h>
#include <switchtec/switchtec.h>

struct pmon_event_counter_setup {
	uint8_t sub_cmd_id;
	uint8_t stack_id;
	uint8_t counter_id;
	uint8_t num_counters;

	struct {
		uint8_t  port_mask;
		uint32_t type_mask:24;
		uint8_t  ieg;
		uint32_t thresh;
	} __attribute__(( packed )) counters[63];
};

struct pmon_event_counter_get_setup_result {
	uint8_t  port_mask;
	uint32_t type_mask:24;
	uint8_t  ieg;
	uint32_t thresh;
} __attribute__(( packed ));

struct pmon_event_counter_get {
	uint8_t sub_cmd_id;
	uint8_t stack_id;
	uint8_t counter_id;
	uint8_t num_counters;
	uint8_t read_clear;
};

struct pmon_event_counter_result {
	uint32_t value;
	uint32_t threshold;
};

struct pmon_bw_get {
	uint8_t sub_cmd_id;
	uint8_t count;
	struct {
		uint8_t id;
		uint8_t clear;
	} ports[SWITCHTEC_MAX_PORTS];
};

struct pmon_lat_setup {
	uint8_t sub_cmd_id;
	uint8_t count;
	struct {
		uint8_t egress;
		uint8_t ingress;
	} ports[SWITCHTEC_MAX_PORTS];
};

struct pmon_lat_get {
	uint8_t sub_cmd_id;
	uint8_t count;
	uint8_t clear;
	uint8_t port_ids[SWITCHTEC_MAX_PORTS];
};

struct pmon_lat_data {
	uint16_t cur_ns;
	uint16_t max_ns;
};

#endif
