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

/**
 * @file
 * @brief Switchtec diagnostic functions
 */

#define SWITCHTEC_LIB_CORE

#include "switchtec_priv.h"
#include "switchtec/diag.h"
#include "switchtec/switchtec.h"
#include "switchtec/utils.h"

#include <errno.h>

/**
 * @brief Get the receiver object
 * @param[in]  dev	Switchtec device handle
 * @param[in]  port_id	Physical port ID
 * @param[in]  lane_id  Lane ID
 * @param[in]  link     Current or previous link-up
 * @param[out] res      Resulting receiver object
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_rcvr_obj(struct switchtec_dev *dev, int port_id,
		int lane_id, enum switchtec_diag_link link,
		struct switchtec_rcvr_obj *res)
{
	struct switchtec_diag_rcvr_obj_dump_out out = {};
	struct switchtec_diag_rcvr_obj_dump_in in = {
		.port_id = port_id,
		.lane_id = lane_id,
	};
	struct switchtec_diag_ext_recv_obj_dump_in ext_in = {
		.sub_cmd = MRPC_EXT_RCVR_OBJ_DUMP_PREV,
		.port_id = port_id,
		.lane_id = lane_id,
	};
	int i, ret;

	if (!res) {
		errno = -EINVAL;
		return -1;
	}

	if (link == SWITCHTEC_DIAG_LINK_CURRENT) {
		ret = switchtec_cmd(dev, MRPC_RCVR_OBJ_DUMP, &in, sizeof(in),
				    &out, sizeof(out));
	} else if (link == SWITCHTEC_DIAG_LINK_PREVIOUS) {
		ret = switchtec_cmd(dev, MRPC_EXT_RCVR_OBJ_DUMP, &ext_in,
				    sizeof(ext_in), &out, sizeof(out));
	} else {
		errno = -EINVAL;
		return -1;
	}

	if (ret)
		return -1;

	res->port_id = out.port_id;
	res->lane_id = out.lane_id;
	res->ctle = out.ctle;
	res->target_amplitude = out.target_amplitude;
	res->speculative_dfe = out.speculative_dfe;
	for (i = 0; i < ARRAY_SIZE(res->dynamic_dfe); i++)
		res->dynamic_dfe[i] = out.dynamic_dfe[i];

	return 0;
}

/**
 * @brief Get the port equalization TX coefficients
 * @param[in]  dev	Switchtec device handle
 * @param[in]  port_id	Physical port ID
 * @param[in]  end      Get coefficents for the Local or the Far End
 * @param[out] res      Resulting port equalization coefficients
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_port_eq_tx_coeff(struct switchtec_dev *dev, int port_id,
		enum switchtec_diag_end end, enum switchtec_diag_link link,
		struct switchtec_port_eq_coeff *res)
{
	struct switchtec_diag_port_eq_status_out out = {};
	struct switchtec_diag_port_eq_status_in in = {
		.op_type = DIAG_PORT_EQ_STATUS_OP_PER_PORT,
		.port_id = port_id,
	};
	struct switchtec_diag_ext_dump_coeff_prev_in in_prev = {
		.op_type = DIAG_PORT_EQ_STATUS_OP_PER_PORT,
		.port_id = port_id,
	};
	int ret, i;

	if (!res) {
		errno = -EINVAL;
		return -1;
	}

	if (end == SWITCHTEC_DIAG_LOCAL) {
		in.sub_cmd = MRPC_PORT_EQ_LOCAL_TX_COEFF_DUMP;
		in_prev.sub_cmd = MRPC_EXT_RCVR_OBJ_DUMP_LOCAL_TX_COEFF_PREV;
	} else if (end == SWITCHTEC_DIAG_FAR_END) {
		in.sub_cmd = MRPC_PORT_EQ_FAR_END_TX_COEFF_DUMP;
		in_prev.sub_cmd = MRPC_EXT_RCVR_OBJ_DUMP_FAR_END_TX_COEFF_PREV;
	} else {
		errno = -EINVAL;
		return -1;
	}

	if (link == SWITCHTEC_DIAG_LINK_CURRENT) {
		ret = switchtec_cmd(dev, MRPC_PORT_EQ_STATUS, &in, sizeof(in),
				    &out, sizeof(out));
	} else if (link == SWITCHTEC_DIAG_LINK_PREVIOUS) {
		ret = switchtec_cmd(dev, MRPC_EXT_RCVR_OBJ_DUMP, &in_prev,
				    sizeof(in_prev), &out, sizeof(out));
	} else {
		errno = -EINVAL;
		return -1;
	}

	if (ret)
		return -1;

	res->lane_cnt = out.lane_id;
	for (i = 0; i < res->lane_cnt; i++) {
		res->cursors[i].pre = out.cursors[i].pre;
		res->cursors[i].post = out.cursors[i].post;
	}

	return 0;
}

/**
 * @brief Get the far end TX equalization table
 * @param[in]  dev	Switchtec device handle
 * @param[in]  port_id	Physical port ID
 * @param[out] res      Resulting port equalization table
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_port_eq_tx_table(struct switchtec_dev *dev, int port_id,
				    enum switchtec_diag_link link,
				    struct switchtec_port_eq_table *res)
{
	struct switchtec_diag_port_eq_table_out out = {};
	struct switchtec_diag_port_eq_status_in2 in = {
		.sub_cmd = MRPC_PORT_EQ_FAR_END_TX_EQ_TABLE_DUMP,
		.port_id = port_id,
	};
	struct switchtec_diag_port_eq_status_in2 in_prev = {
		.sub_cmd = MRPC_EXT_RCVR_OBJ_DUMP_EQ_TX_TABLE_PREV,
		.port_id = port_id,
	};
	int ret, i;

	if (!res) {
		errno = -EINVAL;
		return -1;
	}

	if (link == SWITCHTEC_DIAG_LINK_CURRENT) {
		ret = switchtec_cmd(dev, MRPC_PORT_EQ_STATUS, &in, sizeof(in),
				    &out, sizeof(out));
	} else if (link == SWITCHTEC_DIAG_LINK_PREVIOUS) {
		ret = switchtec_cmd(dev, MRPC_EXT_RCVR_OBJ_DUMP, &in_prev,
				    sizeof(in_prev), &out, sizeof(out));
	} else {
		errno = -EINVAL;
		return -1;
	}

	if (ret)
		return -1;

	res->lane_id = out.lane_id;
	res->step_cnt = out.step_cnt;
	for (i = 0; i < res->step_cnt; i++) {
		res->steps[i].pre_cursor     = out.steps[i].pre_cursor;
		res->steps[i].post_cursor    = out.steps[i].post_cursor;
		res->steps[i].fom            = out.steps[i].fom;
		res->steps[i].pre_cursor_up  = out.steps[i].pre_cursor_up;
		res->steps[i].post_cursor_up = out.steps[i].post_cursor_up;
		res->steps[i].error_status   = out.steps[i].error_status;
		res->steps[i].active_status  = out.steps[i].active_status;
		res->steps[i].speed          = out.steps[i].speed;
	}

	return 0;
}

/**
 * @brief Get the equalization FS/LF
 * @param[in]  dev	Switchtec device handle
 * @param[in]  port_id	Physical port ID
 * @param[in]  lane_id	Physical port ID
 * @param[in]  end      Get coefficents for the Local or the Far End
 * @param[out] res      Resulting FS/LF values
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_port_eq_tx_fslf(struct switchtec_dev *dev, int port_id,
				   int lane_id, enum switchtec_diag_end end,
				   enum switchtec_diag_link link,
				   struct switchtec_port_eq_tx_fslf *res)
{
	struct switchtec_diag_port_eq_tx_fslf_out out = {};
	struct switchtec_diag_port_eq_status_in2 in = {
		.port_id = port_id,
		.lane_id = lane_id,
	};
	struct switchtec_diag_ext_recv_obj_dump_in in_prev = {
		.port_id = port_id,
		.lane_id = lane_id,
	};
	int ret;

	if (!res) {
		errno = -EINVAL;
		return -1;
	}

	if (end == SWITCHTEC_DIAG_LOCAL) {
		in.sub_cmd = MRPC_PORT_EQ_LOCAL_TX_FSLF_DUMP;
		in_prev.sub_cmd = MRPC_EXT_RCVR_OBJ_DUMP_LOCAL_TX_FSLF_PREV;
	} else if (end == SWITCHTEC_DIAG_FAR_END) {
		in.sub_cmd = MRPC_PORT_EQ_FAR_END_TX_FSLF_DUMP;
		in_prev.sub_cmd = MRPC_EXT_RCVR_OBJ_DUMP_FAR_END_TX_FSLF_PREV;
	} else {
		errno = -EINVAL;
		return -1;
	}

	if (link == SWITCHTEC_DIAG_LINK_CURRENT) {
		ret = switchtec_cmd(dev, MRPC_PORT_EQ_STATUS, &in, sizeof(in),
				    &out, sizeof(out));
	} else if (link == SWITCHTEC_DIAG_LINK_PREVIOUS) {
		ret = switchtec_cmd(dev, MRPC_EXT_RCVR_OBJ_DUMP, &in_prev,
				    sizeof(in_prev), &out, sizeof(out));
	} else {
		errno = -EINVAL;
		return -1;
	}

	if (ret)
		return -1;

	res->fs = out.fs;
	res->lf = out.lf;

	return 0;
}

/**
 * @brief Get the Extended Receiver Object
 * @param[in]  dev	Switchtec device handle
 * @param[in]  port_id	Physical port ID
 * @param[in]  lane_id  Lane ID
 * @param[in]  link     Current or previous link-up
 * @param[out] res      Resulting receiver object
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_rcvr_ext(struct switchtec_dev *dev, int port_id,
			    int lane_id, enum switchtec_diag_link link,
			    struct switchtec_rcvr_ext *res)
{
	struct switchtec_diag_rcvr_ext_out out = {};
	struct switchtec_diag_ext_recv_obj_dump_in in = {
		.port_id = port_id,
		.lane_id = lane_id,
	};
	int ret;

	if (!res) {
		errno = -EINVAL;
		return -1;
	}

	if (link == SWITCHTEC_DIAG_LINK_CURRENT) {
		in.sub_cmd = MRPC_EXT_RCVR_OBJ_DUMP_RCVR_EXT;
	} else if (link == SWITCHTEC_DIAG_LINK_PREVIOUS) {
		in.sub_cmd = MRPC_EXT_RCVR_OBJ_DUMP_RCVR_EXT_PREV;
	} else {
		errno = -EINVAL;
		return -1;
	}

	ret = switchtec_cmd(dev, MRPC_EXT_RCVR_OBJ_DUMP, &in, sizeof(in),
			    &out, sizeof(out));
	if (ret)
		return -1;

	res->ctle2_rx_mode = out.ctle2_rx_mode;
	res->dtclk_9 = out.dtclk_9;
	res->dtclk_8_6 = out.dtclk_8_6;
	res->dtclk_5 = out.dtclk_5;

	return 0;
}

/**
 * @brief Get the permission table
 * @param[in]  dev	Switchtec device handle
 * @param[out] table    Resulting MRPC permission table
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_perm_table(struct switchtec_dev *dev,
			      struct switchtec_mrpc table[MRPC_MAX_ID])
{
	uint32_t perms[MRPC_MAX_ID / 32];
	int i, ret;

	ret = switchtec_cmd(dev, MRPC_MRPC_PERM_TABLE_GET, NULL, 0,
			    perms, sizeof(perms));
	if (ret)
		return -1;

	for (i = 0; i < MRPC_MAX_ID; i++) {
		if (perms[i >> 5] & (1 << (i & 0x1f))) {
			if (switchtec_mrpc_table[i].tag) {
				table[i] = switchtec_mrpc_table[i];
			} else {
				table[i].tag = "UNKNOWN";
				table[i].desc = "Unknown MRPC Command";
				table[i].reserved = true;
			}
		} else {
			table[i].tag = NULL;
			table[i].desc = NULL;
		}
	}

	return 0;
}

/**@}*/
