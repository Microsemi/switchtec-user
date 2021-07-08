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

#endif
/**@}*/
