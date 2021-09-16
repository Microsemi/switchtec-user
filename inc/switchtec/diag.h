/*
 * Microsemi Switchtec(tm) PCIe Management Library
 * Copyright (c) 2021, Microsemi Corporation
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
 *
 */

#ifndef LIBSWITCHTEC_DIAG_H
#define LIBSWITCHTEC_DIAG_H

#include <stdint.h>

/**
 * @file
 * @brief Diagnostic structures
 */

struct switchtec_diag_rcvr_obj_dump_in {
	uint8_t port_id;
	uint8_t lane_id;
	uint16_t resvd;
};

struct switchtec_diag_rcvr_obj_dump_out {
	uint8_t port_id;
	uint8_t lane_id;
	uint8_t ctle;
	uint8_t target_amplitude;
	uint8_t speculative_dfe;
	int8_t dynamic_dfe[7];
};

enum {
	DIAG_PORT_EQ_STATUS_OP_PER_PORT = 0,
	DIAG_PORT_EQ_STATUS_OP_PER_LANE = 1,
};

struct switchtec_diag_port_eq_status_in {
	uint8_t sub_cmd;
	uint8_t op_type;
	uint8_t port_id;
	uint8_t lane_id;
};

struct switchtec_diag_port_eq_status_in2 {
	uint8_t sub_cmd;
	uint8_t port_id;
	uint8_t lane_id;
	uint8_t resvd;
};

struct switchtec_diag_port_eq_status_out {
	uint8_t sub_cmd;
	uint8_t op_type;
	uint8_t port_id;
	uint8_t lane_id;

	struct {
		uint8_t pre;
		uint8_t post;
	} cursors[16];
};

struct switchtec_diag_port_eq_table_out {
	uint8_t sub_cmd;
	uint8_t port_id;
	uint8_t lane_id;
	uint8_t step_cnt;
	struct {
		uint8_t pre_cursor;
		uint8_t post_cursor;
		uint8_t fom;
		uint8_t pre_cursor_up;
		uint8_t post_cursor_up;
		uint8_t error_status;
		uint8_t active_status;
		uint8_t speed;
	} steps[126];
};

struct switchtec_diag_port_eq_tx_fslf_out {
	uint8_t sub_cmd;
	uint8_t port_id;
	uint8_t lane_id;
	uint8_t fs;
	uint8_t lf;
	uint8_t resvd[3];
};

struct switchtec_diag_ext_recv_obj_dump_in {
	uint8_t sub_cmd;
	uint8_t port_id;
	uint8_t lane_id;
	uint8_t resvd;
};

struct switchtec_diag_ext_dump_coeff_prev_in {
	uint8_t sub_cmd;
	uint8_t op_type;
	uint8_t port_id;
	uint8_t lane_id;
};

struct switchtec_diag_rcvr_ext_out {
	uint8_t port_id;
	uint8_t lane_id;
	uint16_t ctle2_rx_mode;
	uint8_t dtclk_9;
	uint8_t dtclk_8_6;
	uint8_t dtclk_5;
};

struct switchtec_diag_refclk_ctl_in {
	uint8_t sub_cmd;
	uint8_t stack_id;
};

enum switchtec_diag_loopback_type {
	DIAG_LOOPBACK_RX_TO_TX = 0,
	DIAG_LOOPBACK_TX_TO_RX = 1,
};

struct switchtec_diag_loopback_in {
	uint8_t sub_cmd;
	uint8_t port_id;
	uint8_t enable;
	uint8_t type;
};

struct switchtec_diag_loopback_out {
	uint8_t port_id;
	uint8_t enabled;
	uint8_t type;
	uint8_t resvdd;
};

struct switchtec_diag_loopback_ltssm_in {
	uint8_t sub_cmd;
	uint8_t port_id;
	uint8_t enable;
	uint8_t speed;
};

struct switchtec_diag_loopback_ltssm_out {
	uint8_t port_id;
	uint8_t enabled;
	uint8_t speed;
	uint8_t resvd;
};

struct switchtec_diag_pat_gen_in {
	uint8_t sub_cmd;
	uint8_t port_id;
	uint8_t pattern_type;
	uint8_t lane_id;
};

struct switchtec_diag_pat_gen_inject {
	uint8_t sub_cmd;
	uint8_t port_id;
	uint16_t resvd;
	uint32_t err_cnt;
};

struct switchtec_diag_pat_gen_out {
	uint8_t port_id;
	uint8_t pattern_type;
	uint16_t resvd;
	uint32_t err_cnt_lo;
	uint32_t err_cnt_hi;
};

struct switchtec_diag_port_eye_start {
	uint8_t sub_cmd;
	uint8_t resvd1[3];
	uint32_t resvd2;
	uint32_t lane_mask[4];
	int16_t x_start;
	int16_t y_start;
	int16_t x_end;
	int16_t y_end;
	uint16_t x_step;
	uint16_t y_step;
	uint32_t step_interval;
};

struct switchtec_diag_port_eye_cmd {
	uint8_t sub_cmd;
	uint8_t data_mode;
	uint8_t resvd;
	uint8_t status;
};

struct switchtec_diag_port_eye_fetch {
	uint8_t sub_cmd;
	uint8_t data_mode;
	uint8_t resvd1;
	uint8_t status;
	uint32_t time_remaining;
	uint32_t lane_mask[4];
	uint8_t x_start;
	uint8_t resvd2;
	int16_t y_start;
	uint8_t data_count_lo;
	uint8_t frame_status;
	uint8_t resvd3;
	uint8_t data_count_hi;
	union {
		struct {
			uint32_t error_cnt_lo;
			uint32_t error_cnt_hi;
			uint32_t sample_cnt_lo;
			uint32_t sample_cnt_hi;
		} raw[62];
		struct {
			uint16_t ratio;
		} ratio[496];
	};
};

struct switchtec_diag_cross_hair_in {
	uint8_t sub_cmd;
	uint8_t lane_id;
	uint8_t all_lanes;
	uint8_t num_lanes;
};

struct switchtec_diag_cross_hair_get {
	uint8_t lane_id;
	uint8_t state;

	union {
		struct {
			int8_t byte0;
			int8_t byte1;
			int16_t word0;
			int16_t word1;
			int16_t word2;
			int16_t word3;
		};
		struct {
			uint8_t prev_state;
			uint8_t _byte1;
			int16_t x_pos;
			int16_t y_pos;
		};
		struct {
			int8_t eye_left_lim;
			int8_t eye_right_lim;
			int16_t eye_bot_left_lim;
			int16_t eye_bot_right_lim;
			int16_t eye_top_left_lim;
			int16_t eye_top_right_lim;
		};
	};
};

#endif
/**@}*/
