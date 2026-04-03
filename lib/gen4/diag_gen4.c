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