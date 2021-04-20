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

#include "switchtec/diag.h"
#include "switchtec/switchtec.h"
#include "switchtec/utils.h"

#include <errno.h>

/**
 * @brief Get the receiver object
 * @param[in]  dev	Switchtec device handle
 * @param[in]  port_id	Physical port ID
 * @param[in]  lane_id  Lane ID
 * @param[out] res      Resulting receiver object
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_rcvr_obj(struct switchtec_dev *dev, int port_id,
		int lane_id, struct switchtec_rcvr_obj *res)
{
	struct switchtec_diag_rcvr_obj_dump_out out = {};
	struct switchtec_diag_rcvr_obj_dump_in in = {
		.port_id = port_id,
		.lane_id = lane_id,
	};
	int i, ret;

	if (!res) {
		errno = -EINVAL;
		return -1;
	}

	ret = switchtec_cmd(dev, MRPC_RCVR_OBJ_DUMP, &in, sizeof(in),
			    &out, sizeof(out));
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
				    enum switchtec_diag_end end,
				    struct switchtec_port_eq_coeff *res)
{
	struct switchtec_diag_port_eq_status_out out = {};
	struct switchtec_diag_port_eq_status_in in = {
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
	} else if (end == SWITCHTEC_DIAG_FAR_END) {
		in.sub_cmd = MRPC_PORT_EQ_FAR_END_TX_COEFF_DUMP;
	} else {
		errno = -EINVAL;
		return -1;
	}

	ret = switchtec_cmd(dev, MRPC_PORT_EQ_STATUS, &in, sizeof(in),
			    &out, sizeof(out));
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
				    struct switchtec_port_eq_table *res)
{
	struct switchtec_diag_port_eq_table_out out = {};
	struct switchtec_diag_port_eq_status_in2 in = {
		.sub_cmd = MRPC_PORT_EQ_FAR_END_TX_EQ_TABLE_DUMP,
		.port_id = port_id,
	};
	int ret, i;

	if (!res) {
		errno = -EINVAL;
		return -1;
	}

	ret = switchtec_cmd(dev, MRPC_PORT_EQ_STATUS, &in, sizeof(in),
			    &out, sizeof(out));
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

/**@}*/
