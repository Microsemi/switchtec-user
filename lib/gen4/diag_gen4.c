/*
 * Microsemi Switchtec(tm) PCIe Management Library
 * Copyright (c) 2017, Microsemi Corporation
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

#include "../switchtec_priv.h"
#include "switchtec/diag.h"
#include "switchtec/switchtec.h"
#include "switchtec/endian.h"
#include "switchtec/utils.h"

#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

/**
 * @brief Get the LTSSM log of a port on a gen4 switchtec device
 * @param[in]	dev    Switchtec device handle
 * @param[in]	port   Switchtec Port
 * @param[inout] log_count number of log entries
 * @param[out] log    A pointer to an array containing the log
 */
int switchtec_diag_ltssm_log_gen4(struct switchtec_dev *dev,
				 int port, int *log_count,
				 struct switchtec_diag_ltssm_log *log_data)
{
	struct {
		uint8_t sub_cmd;
		uint8_t port;
		uint8_t freeze;
		uint8_t unused;
	} ltssm_freeze;

	struct {
		uint8_t sub_cmd;
		uint8_t port;
	} status;
	struct {
		uint32_t w0_trigger_count;
		uint32_t w1_trigger_count;
		uint8_t log_num;
	} status_output;

	struct {
		uint8_t sub_cmd;
		uint8_t port;
		uint8_t log_index;
		uint8_t no_of_logs;
	} log_dump;
	struct {
		uint32_t dw0;
		uint32_t dw1;
	} log_dump_out[256];

	uint32_t dw1;
	uint32_t dw0;
	int major;
	int minor;
	int rate;
	int ret;
	int i;

	/* freeze logs */
	ltssm_freeze.sub_cmd = MRPC_LTMON_FREEZE;
	ltssm_freeze.port = port;
	ltssm_freeze.freeze = 1;

	ret = switchtec_cmd(dev, MRPC_DIAG_PORT_LTSSM_LOG, &ltssm_freeze,
			    sizeof(ltssm_freeze), NULL, 0);
	if (ret)
		return ret;

	/* get number of entries */
	status.sub_cmd = MRPC_LTMON_GET_STATUS_GEN4;
	status.port = port;
	ret = switchtec_cmd(dev, MRPC_DIAG_PORT_LTSSM_LOG, &status,
			    sizeof(status), &status_output,
			    sizeof(status_output));
	if (ret)
		return ret;

	if (status_output.log_num < *log_count)
		*log_count = status_output.log_num;

	/* get log data */
	log_dump.sub_cmd = MRPC_LTMON_LOG_DUMP_GEN4;
	log_dump.port = port;
	log_dump.log_index = 0;
	log_dump.no_of_logs = *log_count;
	if(log_dump.no_of_logs <= 126) {
		ret = switchtec_cmd(dev, MRPC_DIAG_PORT_LTSSM_LOG, &log_dump,
				    sizeof(log_dump), log_dump_out,
				    8 * log_dump.no_of_logs);
		if (ret)
			return ret;
	} else {
		log_dump.no_of_logs = 126;
		ret = switchtec_cmd(dev, MRPC_DIAG_PORT_LTSSM_LOG, &log_dump,
				    sizeof(log_dump), log_dump_out,
				    8 * log_dump.no_of_logs);
		if (ret)
			return ret;

		log_dump.log_index = 126;
		log_dump.no_of_logs = *log_count - 126;

		ret = switchtec_cmd(dev, MRPC_DIAG_PORT_LTSSM_LOG, &log_dump,
				    sizeof(log_dump), log_dump_out + 126,
				    8 * log_dump.no_of_logs);
		if (ret)
			return ret;
	}
	for (i = 0; i < *log_count; i++) {
		dw1 = log_dump_out[i].dw1;
		dw0 = log_dump_out[i].dw0;
		rate = (dw0 >> 13) & 0x3;
		major = (dw0 >> 7) & 0xf;
		minor = (dw0 >> 3) & 0xf;

		log_data[i].timestamp = dw1 & 0x3ffffff;
		log_data[i].link_rate = switchtec_gen_transfers[rate + 1];
		log_data[i].link_state = major | (minor << 8);
	}

	/* unfreeze logs */
	ltssm_freeze.sub_cmd = MRPC_LTMON_FREEZE;
	ltssm_freeze.port = port;
	ltssm_freeze.freeze = 0;

	ret = switchtec_cmd(dev, MRPC_DIAG_PORT_LTSSM_LOG, &ltssm_freeze,
			    sizeof(ltssm_freeze), NULL, 0);

	return ret;
}

int switchtec_diag_port_eq_tx_coeff_gen4(struct switchtec_dev *dev,
					 int port_id, int prev_speed,
					 int end, int link, void *res)
{
	struct switchtec_port_eq_coeff *coeff = res;
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

	if (!coeff) {
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

	coeff->lane_cnt = out.lane_id + 1;
	for (i = 0; i < coeff->lane_cnt; i++) {
		coeff->cursors[i].pre = out.cursors[i].pre;
		coeff->cursors[i].post = out.cursors[i].post;
	}

	return 0;
}

int switchtec_diag_port_eq_tx_table_gen4(struct switchtec_dev *dev,
					 int port_id, int prev_speed,
					 int link, void *res)
{
	struct switchtec_port_eq_table *table = res;
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

	if (!table) {
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

	table->lane_id = out.lane_id;
	table->step_cnt = out.step_cnt;
	for (i = 0; i < table->step_cnt; i++) {
		table->steps[i].pre_cursor = out.steps[i].pre_cursor;
		table->steps[i].post_cursor = out.steps[i].post_cursor;
		table->steps[i].fom = out.steps[i].fom;
		table->steps[i].pre_cursor_up = out.steps[i].pre_cursor_up;
		table->steps[i].post_cursor_up = out.steps[i].post_cursor_up;
		table->steps[i].error_status = out.steps[i].error_status;
		table->steps[i].active_status = out.steps[i].active_status;
		table->steps[i].speed = out.steps[i].speed;
	}

	return 0;
}

int switchtec_diag_port_eq_tx_fslf_gen4(struct switchtec_dev *dev,
					int port_id, int prev_speed,
					int lane_id, int end, int link,
					void *res)
{
	struct switchtec_port_eq_tx_fslf *fslf = res;
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

	if (!fslf) {
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

	fslf->fs = out.fs;
	fslf->lf = out.lf;

	return 0;
}

int switchtec_diag_loopback_set_gen4(struct switchtec_dev *dev,
				     int port_id, int enable,
				     int enable_parallel, int enable_external,
				     int enable_ltssm, int enable_pipe,
				     int ltssm_speed)
{
	struct switchtec_diag_loopback_in int_in = {
		.sub_cmd = MRPC_LOOPBACK_SET_INT_LOOPBACK,
		.port_id = port_id,
		.enable = enable,
	};
	struct switchtec_diag_loopback_ltssm_in ltssm_in = {
		.sub_cmd = MRPC_LOOPBACK_SET_LTSSM_LOOPBACK,
		.port_id = port_id,
		.enable = !!(enable & SWITCHTEC_DIAG_LOOPBACK_LTSSM),
		.speed = ltssm_speed,
	};
	int ret;

	int_in.type = DIAG_LOOPBACK_RX_TO_TX;
	int_in.enable = !!(enable & SWITCHTEC_DIAG_LOOPBACK_RX_TO_TX);

	ret = switchtec_cmd(dev, MRPC_INT_LOOPBACK, &int_in,
			    sizeof(int_in), NULL, 0);
	if (ret)
		return ret;

	int_in.type = DIAG_LOOPBACK_TX_TO_RX;
	int_in.enable = !!(enable & SWITCHTEC_DIAG_LOOPBACK_TX_TO_RX);

	ret = switchtec_cmd(dev, MRPC_INT_LOOPBACK, &int_in,
			    sizeof(int_in), NULL, 0);
	if (ret)
		return ret;

	ret = switchtec_cmd(dev, MRPC_INT_LOOPBACK, &ltssm_in,
			    sizeof(ltssm_in), NULL, 0);
	if (ret)
		return ret;

	return 0;
}

int switchtec_diag_loopback_get_gen4(struct switchtec_dev *dev,
				     int port_id, int *enabled,
				     int *ltssm_speed)
{
	struct switchtec_diag_loopback_in int_in = {
		.sub_cmd = MRPC_LOOPBACK_GET_INT_LOOPBACK,
		.port_id = port_id,
	};
	struct switchtec_diag_loopback_ltssm_in lt_in = {
		.sub_cmd = MRPC_LOOPBACK_GET_LTSSM_LOOPBACK,
		.port_id = port_id,
	};
	struct switchtec_diag_loopback_out int_out;
	struct switchtec_diag_loopback_ltssm_out lt_out;
	int ret, en = 0;

	int_in.type = DIAG_LOOPBACK_RX_TO_TX;

	ret = switchtec_cmd(dev, MRPC_INT_LOOPBACK, &int_in, sizeof(int_in),
			    &int_out, sizeof(int_out));
	if (ret)
		return ret;

	if (int_out.enabled)
		en |= SWITCHTEC_DIAG_LOOPBACK_RX_TO_TX;

	int_in.type = DIAG_LOOPBACK_TX_TO_RX;

	ret = switchtec_cmd(dev, MRPC_INT_LOOPBACK, &int_in, sizeof(int_in),
			    &int_out, sizeof(int_out));
	if (ret)
		return ret;

	if (int_out.enabled)
		en |= SWITCHTEC_DIAG_LOOPBACK_TX_TO_RX;

	ret = switchtec_cmd(dev, MRPC_INT_LOOPBACK, &lt_in, sizeof(lt_in),
			    &lt_out, sizeof(lt_out));
	if (ret)
		return ret;

	if (lt_out.enabled)
		en |= SWITCHTEC_DIAG_LOOPBACK_LTSSM;

	if (enabled)
		*enabled = en;

	if (ltssm_speed)
		*ltssm_speed = lt_out.speed;

	return 0;
}

static uint64_t hi_lo_to_uint64(uint32_t lo, uint32_t hi)
{
	uint64_t ret;

	ret = le32toh(hi);
	ret <<= 32;
	ret |= le32toh(lo);

	return ret;
}

static int switchtec_diag_eye_status(int status)
{
	switch (status) {
	case 0: return 0;
	case 2:
		errno = EINVAL;
		return -1;
	case 3:
		errno = EBUSY;
		return -1;
	default:
		errno = EPROTO;
		return -1;
	}
}

static int switchtec_diag_eye_cmd_gen4(struct switchtec_dev *dev, void *in,
				       size_t size)
{
	struct switchtec_diag_port_eye_cmd out;
	int ret;

	ret = switchtec_cmd(dev, MRPC_EYE_OBSERVE, in, size, &out,
			    sizeof(out));

	if (ret)
		return ret;

	return switchtec_diag_eye_status(out.status);
}

int switchtec_diag_eye_start_gen4(struct switchtec_dev *dev, int lane_mask[4],
				struct range *x_range, struct range *y_range,
				int step_interval, int capture_depth, int sar_sel,
				int intleav_sel, int hstep, int data_mode, 
				int eye_mode, uint64_t refclk, int vstep)
{
	int err, ret;
	struct switchtec_diag_port_eye_start in = {
		.sub_cmd = MRPC_EYE_OBSERVE_START,
		.lane_mask[0] = lane_mask[0],
		.lane_mask[1] = lane_mask[1],
		.lane_mask[2] = lane_mask[2],
		.lane_mask[3] = lane_mask[3],
		.x_start = x_range->start,
		.y_start = y_range->start,
		.x_end = x_range->end,
		.y_end = y_range->end,
		.x_step = x_range->step,
		.y_step = y_range->step,
		.step_interval = step_interval,
	};

	ret = switchtec_diag_eye_cmd_gen4(dev, &in, sizeof(in));
	/* Add delay so hardware has enough time to start */
	err = errno;
	usleep(200000);
	errno = err;
	return ret;
}

/**
 * @brief Start a PCIe Eye Capture
 * @param[in]  dev	       Switchtec device handle
 * @param[out] pixels          Resulting pixel data
 * @param[in]  pixel_cnt       Space in pixel array
 * @param[out] lane_id         The lane for the resulting pixels
 *
 * @return number of pixels fetched on success, error code on failure
 *
 * pixel_cnt needs to be greater than 62 in raw mode or 496 in ratio
 * mode, otherwise data will be lost and the number of pixels fetched
 * will be greater than the space in the pixel buffer.
 */
int switchtec_diag_eye_fetch_gen4(struct switchtec_dev *dev, double *pixels,
			     size_t pixel_cnt, int *lane_id)
{
	struct switchtec_diag_port_eye_cmd in = {
		.sub_cmd = MRPC_EYE_OBSERVE_FETCH,
	};
	struct switchtec_diag_port_eye_fetch out;
	uint64_t samples, errors;
	int i, ret, data_count;

retry:
	ret = switchtec_cmd(dev, MRPC_EYE_OBSERVE, &in, sizeof(in), &out,
			    sizeof(out));
	if (ret)
		return ret;

	if (out.status == 1) {
		usleep(5000);
		goto retry;
	}

	ret = switchtec_diag_eye_status(out.status);
	if (ret)
		return ret;

	for (i = 0; i < 4; i++) {
		*lane_id = ffs(out.lane_mask[i]);
		if (*lane_id)
			break;
	}

	data_count = out.data_count_lo | ((int)out.data_count_hi << 8);

	for (i = 0; i < data_count && i < pixel_cnt; i++) {
		switch (out.data_mode) {
		case SWITCHTEC_DIAG_EYE_RAW:
			errors = hi_lo_to_uint64(out.raw[i].error_cnt_lo,
						 out.raw[i].error_cnt_hi);
			samples = hi_lo_to_uint64(out.raw[i].sample_cnt_lo,
						  out.raw[i].sample_cnt_hi);
			if (samples)
				pixels[i] = (double)errors / samples;
			else
				pixels[i] = nan("");
			break;
		case SWITCHTEC_DIAG_EYE_RATIO:
			pixels[i] = le32toh(out.ratio[i].ratio) / 65536.;
			break;
		}
	}

	return data_count;
}

/**
 * @brief Cancel in-progress eye capture
 * @param[in]  dev	       Switchtec device handle
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_eye_cancel_gen4(struct switchtec_dev *dev)
{
	int ret;
	int err;
	struct switchtec_diag_port_eye_cmd in = {
		.sub_cmd = MRPC_EYE_OBSERVE_CANCEL,
	};

	ret = switchtec_diag_eye_cmd_gen4(dev, &in, sizeof(in));

	/* Add delay so hardware can stop completely */
	err = errno;
	usleep(200000);
	errno = err;

	return ret;
}

/**
 * @brief Set the data mode for the next Eye Capture
 * @param[in]  dev	       Switchtec device handle
 * @param[in]  mode	       Mode to use (raw or ratio)
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_eye_set_mode_gen4(struct switchtec_dev *dev, enum switchtec_diag_eye_data_mode mode)
{
	struct switchtec_diag_port_eye_cmd in = {
		.sub_cmd = MRPC_EYE_OBSERVE_SET_DATA_MODE,
		.data_mode = mode,
	};

	return switchtec_diag_eye_cmd_gen4(dev, &in, sizeof(in));
}

int switchtec_diag_cross_hair_enable_gen4(struct switchtec_dev *dev,
					  int lane_id)
{
	struct switchtec_diag_cross_hair_in in = {
		.sub_cmd = MRPC_CROSS_HAIR_ENABLE,
		.lane_id = lane_id,
		.all_lanes = lane_id == SWITCHTEC_DIAG_CROSS_HAIR_ALL_LANES,
	};

	return switchtec_cmd(dev, MRPC_CROSS_HAIR, &in, sizeof(in), NULL, 0);
}

int switchtec_diag_cross_hair_disable_gen4(struct switchtec_dev *dev)
{
	struct switchtec_diag_cross_hair_in in = {
		.sub_cmd = MRPC_CROSS_HAIR_DISABLE,
	};

	return switchtec_cmd(dev, MRPC_CROSS_HAIR, &in, sizeof(in), NULL, 0);
}

int switchtec_diag_cross_hair_get_gen4(struct switchtec_dev *dev,
				       int start_lane_id, int num_lanes,
				       void *res)
{
	struct switchtec_diag_cross_hair *result = res;
	struct switchtec_diag_cross_hair_in in = {
		.sub_cmd = MRPC_CROSS_HAIR_GET,
		.lane_id = start_lane_id,
		.num_lanes = num_lanes,
	};
	struct switchtec_diag_cross_hair_get out[num_lanes];
	int i, ret;

	ret = switchtec_cmd(dev, MRPC_CROSS_HAIR, &in, sizeof(in), &out,
			    sizeof(out));
	if (ret)
		return ret;

	for (i = 0; i < num_lanes; i++) {
		memset(&result[i], 0, sizeof(result[i]));
		result[i].state = out[i].state;
		result[i].lane_id = out[i].lane_id;

		if (out[i].state <= SWITCHTEC_DIAG_CROSS_HAIR_WAITING) {
			continue;
		} else if (out[i].state < SWITCHTEC_DIAG_CROSS_HAIR_DONE) {
			result[i].x_pos = out[i].x_pos;
			result[i].y_pos = out[i].y_pos;
		} else if (out[i].state == SWITCHTEC_DIAG_CROSS_HAIR_DONE) {
			result[i].eye_left_lim = out[i].eye_left_lim;
			result[i].eye_right_lim = out[i].eye_right_lim;
			result[i].eye_bot_left_lim = out[i].eye_bot_left_lim;
			result[i].eye_bot_right_lim = out[i].eye_bot_right_lim;
			result[i].eye_top_left_lim = out[i].eye_top_left_lim;
			result[i].eye_top_right_lim = out[i].eye_top_right_lim;
		} else if (out[i].state == SWITCHTEC_DIAG_CROSS_HAIR_ERROR) {
			result[i].x_pos = out[i].x_pos;
			result[i].y_pos = out[i].y_pos;
			result[i].prev_state = out[i].prev_state;
		}
	}

	return 0;
}

int switchtec_diag_pattern_gen_set_gen4(struct switchtec_dev *dev, int port_id,
					int type, int link_speed)
{
	struct switchtec_diag_pat_gen_in in = {
		.sub_cmd = MRPC_PAT_GEN_SET_GEN,
		.port_id = port_id,
		.pattern_type = type,
		.lane_id = link_speed
	};

	return switchtec_cmd(dev, MRPC_PAT_GEN, &in, sizeof(in), NULL, 0);
}

int switchtec_diag_pattern_gen_get_gen4(struct switchtec_dev *dev, int port_id,
					int *type)
{
	struct switchtec_diag_pat_gen_in in = {
		.sub_cmd = MRPC_PAT_GEN_GET_GEN,
		.port_id = port_id,
	};
	struct switchtec_diag_pat_gen_out out;
	int ret;

	ret = switchtec_cmd(dev, MRPC_PAT_GEN, &in, sizeof(in), &out,
			    sizeof(out));
	if (ret)
		return ret;

	if (type)
		*type = out.pattern_type;

	return 0;
}

int switchtec_diag_pattern_mon_set_gen4(struct switchtec_dev *dev, int port_id,
					int type)
{
	struct switchtec_diag_pat_gen_in in = {
		.sub_cmd = MRPC_PAT_GEN_SET_MON,
		.port_id = port_id,
		.pattern_type = type,
	};

	return switchtec_cmd(dev, MRPC_PAT_GEN, &in, sizeof(in), NULL, 0);
}

int switchtec_diag_pattern_mon_get_gen4(struct switchtec_dev *dev, int port_id,
					int lane_id, int *type,
					unsigned long long *err_cnt)
{
	struct switchtec_diag_pat_gen_in in = {
		.sub_cmd = MRPC_PAT_GEN_GET_MON,
		.port_id = port_id,
		.lane_id = lane_id,
	};
	struct switchtec_diag_pat_gen_out out;
	int ret;

	ret = switchtec_cmd(dev, MRPC_PAT_GEN, &in, sizeof(in), &out,
			    sizeof(out));
	if (ret)
		return ret;

	if (type)
		*type = out.pattern_type;

	if (err_cnt)
		*err_cnt = (htole32(out.err_cnt_lo) |
			    ((uint64_t)htole32(out.err_cnt_hi) << 32));

	return 0;
}

int switchtec_diag_pattern_inject_gen4(struct switchtec_dev *dev, int port_id,
				       int err_cnt)
{
	struct switchtec_diag_pat_gen_inject in = {
		.sub_cmd = MRPC_PAT_GEN_INJ_ERR,
		.port_id = port_id,
		.err_cnt = err_cnt,
	};

	return switchtec_cmd(dev, MRPC_PAT_GEN, &in, sizeof(in), NULL, 0);
}

int switchtec_diag_rcvr_obj_gen4(struct switchtec_dev *dev, int port_id,
				 int lane_id, int link,
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

int switchtec_inject_err_tlp_lcrc_gen4(struct switchtec_dev *dev,
				       int phys_port_id, int enable,
				       uint8_t rate)
{
	uint32_t output;

	struct switchtec_lnkerr_tlp_lcrc_gen4_in cmd = {
		.subcmd = MRPC_ERR_INJ_TLP_LCRC,
		.phys_port_id = phys_port_id,
		.enable = enable,
		.rate = rate,
	};

	return switchtec_cmd(dev, MRPC_MRPC_ERR_INJ, &cmd,
			     sizeof(cmd), &output, sizeof(output));
}

int switchtec_inject_err_tlp_seqnum_gen4(struct switchtec_dev *dev,
					 int phys_port_id)
{
	uint32_t output;

	struct switchtec_lnkerr_tlp_seqn_in cmd = {
		.subcmd = MRPC_ERR_INJ_TLP_SEQ,
		.phys_port_id = phys_port_id,
	};

	return switchtec_cmd(dev, MRPC_MRPC_ERR_INJ, &cmd,
			     sizeof(cmd), &output, sizeof(output));
}

int switchtec_inject_err_dllp_gen4(struct switchtec_dev *dev, int phys_port_id,
				   int data)
{
	uint32_t output;

	struct switchtec_lnkerr_dllp_in cmd = {
		.subcmd = MRPC_ERR_INJ_DLLP,
		.phys_port_id = phys_port_id,
		.data = data,
	};

	return switchtec_cmd(dev, MRPC_MRPC_ERR_INJ, &cmd,
			     sizeof(cmd), &output, sizeof(output));
}

int switchtec_inject_err_dllp_crc_gen4(struct switchtec_dev *dev,
				       int phys_port_id, int enable,
				       uint16_t rate)
{
	uint32_t output;

	struct switchtec_lnkerr_dllp_crc_in cmd = {
		.subcmd = MRPC_ERR_INJ_DLLP_CRC,
		.phys_port_id = phys_port_id,
		.enable = enable,
		.rate = rate,
	};

	return switchtec_cmd(dev, MRPC_MRPC_ERR_INJ, &cmd,
			     sizeof(cmd), &output, sizeof(output));
}

int switchtec_inject_err_ack_nack_gen4(struct switchtec_dev *dev,
				       int phys_port_id, uint16_t seq_num,
				       uint8_t count)
{
	uint32_t output;

	struct switchtec_lnkerr_ack_nack_in cmd = {
		.subcmd = MRPC_ERR_INJ_ACK_NACK,
		.phys_port_id = phys_port_id,
		.seq_num = seq_num,
		.count = count,
	};

	return switchtec_cmd(dev, MRPC_MRPC_ERR_INJ, &cmd,
			     sizeof(cmd), &output, sizeof(output));
}