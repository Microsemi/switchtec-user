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

static void switchtec_diag_ltssm_set_log_data_gen5(struct switchtec_diag_ltssm_log
					*log_data,
					struct switchtec_diag_ltssm_log_dmp_out
					*log_dump_out_ptr,
					int curr_idx, uint16_t num_of_logs)
{
	uint32_t dw0;
	uint32_t timestamp;

	int major;
	int minor;
	int rate;

	for (int j = 0; j < num_of_logs; j++) {
		dw0 = log_dump_out_ptr[j].dw0;
		timestamp = log_dump_out_ptr[j].ram_timestamp;

		rate = (dw0 >> 13) & 0x7;
		major = (dw0 >> 7) & 0x3f;
		minor = (dw0 >> 3) & 0xf;

		log_data[curr_idx + j].timestamp = timestamp;
		log_data[curr_idx + j].link_rate = switchtec_gen_transfers[rate+1];
		log_data[curr_idx + j].link_state = major | (minor << 8);
	}
}

/**
 * @brief Get the LTSSM log of a port on a gen5 switchtec device
 * @param[in]	dev    Switchtec device handle
 * @param[in]	port   Switchtec Port
 * @param[inout] log_count number of log entries
 * @param[out] log    A pointer to an array containing the log
 */
int switchtec_diag_ltssm_log_gen5(struct switchtec_dev *dev,
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
		uint16_t log_count;
		uint16_t w0_trigger_count;
		uint16_t w1_trigger_count;
	} status_output;

	struct {
		uint8_t sub_cmd;
		uint8_t port;
		uint16_t log_index;
		uint16_t no_of_logs;
	} log_dump;

	uint8_t log_buffer[1024];

	struct switchtec_diag_ltssm_log_dmp_out *log_dump_out_ptr = NULL;

	int ret;
	int log_dmp_size = sizeof(struct switchtec_diag_ltssm_log_dmp_out);

	/* freeze logs */
	ltssm_freeze.sub_cmd = MRPC_LTMON_FREEZE;
	ltssm_freeze.port = port;
	ltssm_freeze.freeze = 1;

	ret = switchtec_cmd(dev, MRPC_DIAG_PORT_LTSSM_LOG, &ltssm_freeze,
			    sizeof(ltssm_freeze), NULL, 0);
	if (ret)
		return ret;

	/* get number of entries */
	status.sub_cmd = MRPC_LTMON_GET_STATUS_GEN5;
	status.port = port;
	ret = switchtec_cmd(dev, MRPC_DIAG_PORT_LTSSM_LOG, &status,
			    sizeof(status), &status_output,
			    sizeof(status_output));
	if (ret)
		return ret;

	*log_count = status_output.log_count;

	/* get log data */
	log_dump.sub_cmd = MRPC_LTMON_LOG_DUMP_GEN5;
	log_dump.port = port;
	log_dump.log_index = 0;
	log_dump.no_of_logs = *log_count;

	if(log_dump.no_of_logs <= SWITCHTEC_LTSSM_MAX_LOGS) {
		/* Single buffer log case */
		ret = switchtec_cmd(dev, MRPC_DIAG_PORT_LTSSM_LOG, &log_dump,
				    sizeof(log_dump), &log_buffer[0],
				    log_dump.no_of_logs * log_dmp_size + 4);
		if (ret)
			return ret;
		log_dump_out_ptr =
			(struct switchtec_diag_ltssm_log_dmp_out *)
			&(log_buffer[4]);

		switchtec_diag_ltssm_set_log_data_gen5(log_data,
						  log_dump_out_ptr,
						  0, log_dump.no_of_logs);
	} else {
		/* Multiple buffer log case */
		int buff_count = log_dump.no_of_logs / SWITCHTEC_LTSSM_MAX_LOGS;
		int curr_idx = 0;
		int buffer_size = SWITCHTEC_LTSSM_MAX_LOGS * log_dmp_size + 4;

		for (int i = 0; i < buff_count; i++) {
			log_dump.no_of_logs = SWITCHTEC_LTSSM_MAX_LOGS;
			ret = switchtec_cmd(dev, MRPC_DIAG_PORT_LTSSM_LOG,
					    &log_dump, sizeof(log_dump),
					    &log_buffer[0], buffer_size);
			if (ret)
				return ret;
			log_dump_out_ptr =
				(struct switchtec_diag_ltssm_log_dmp_out *)
				&(log_buffer[4]);

			switchtec_diag_ltssm_set_log_data_gen5(log_data,
							  log_dump_out_ptr,
							  curr_idx,
							  log_dump.no_of_logs);
			curr_idx += SWITCHTEC_LTSSM_MAX_LOGS;
			log_dump.log_index = curr_idx;
		}
		if (*log_count % SWITCHTEC_LTSSM_MAX_LOGS) {
			log_dump.no_of_logs = *log_count - curr_idx;
			buffer_size = log_dump.no_of_logs * log_dmp_size + 4;
			ret = switchtec_cmd(dev, MRPC_DIAG_PORT_LTSSM_LOG,
					    &log_dump, sizeof(log_dump),
					    &log_buffer[0], buffer_size);
			if (ret)
				return ret;
			log_dump_out_ptr =
				(struct switchtec_diag_ltssm_log_dmp_out *)
				&(log_buffer[4]);

			switchtec_diag_ltssm_set_log_data_gen5(log_data,
							  log_dump_out_ptr,
							  curr_idx,
							  log_dump.no_of_logs);
		}
	}

	/* unfreeze logs */
	ltssm_freeze.sub_cmd = MRPC_LTMON_FREEZE;
	ltssm_freeze.port = port;
	ltssm_freeze.freeze = 0;

	ret = switchtec_cmd(dev, MRPC_DIAG_PORT_LTSSM_LOG, &ltssm_freeze,
			    sizeof(ltssm_freeze), NULL, 0);

	return ret;
}

int switchtec_diag_port_eq_tx_coeff_gen5(struct switchtec_dev *dev,
					 int port_id, int prev_speed,
					 int end, int link, void *res)
{
	struct switchtec_port_eq_coeff *coeff = res;
	struct switchtec_port_eq_coeff *loc_out;
	struct switchtec_rem_port_eq_coeff *rem_out;
	struct switchtec_port_eq_coeff_in *in;
	uint8_t *buf;
	uint32_t buf_size;
	uint32_t in_size = sizeof(struct switchtec_port_eq_coeff_in);
	uint32_t out_size = 0;
	int ret = 0;
	int i;

	if (!coeff) {
		fprintf(stderr, "Error inval output buffer\n");
		errno = -EINVAL;
		return -1;
	}

	buf_size = in_size;
	if (end == SWITCHTEC_DIAG_LOCAL) {
		buf_size += sizeof(struct switchtec_port_eq_coeff);
		out_size = sizeof(struct switchtec_port_eq_coeff);
	} else if (end == SWITCHTEC_DIAG_FAR_END) {
		buf_size += sizeof(struct switchtec_rem_port_eq_coeff);
		out_size = sizeof(struct switchtec_rem_port_eq_coeff);
	} else {
		fprintf(stderr, "Error inval end option\n");
		errno = -EINVAL;
	}
	buf = (uint8_t *)malloc(buf_size);
	if (!buf) {
		fprintf(stderr, "Error in buffer alloc\n");
		errno = -ENOMEM;
		return -1;
	}

	in = (struct switchtec_port_eq_coeff_in *)buf;
	in->op_type = DIAG_PORT_EQ_STATUS_OP_PER_PORT;
	in->phys_port_id = port_id;
	in->lane_id = 0;
	in->dump_type = LANE_EQ_DUMP_TYPE_CURR;

	if (link == SWITCHTEC_DIAG_LINK_PREVIOUS) {
		in->dump_type = LANE_EQ_DUMP_TYPE_PREV;
		in->prev_rate = prev_speed;
	}

	if (end == SWITCHTEC_DIAG_LOCAL) {
		in->cmd = MRPC_GEN5_PORT_EQ_LOCAL_TX_COEFF_DUMP;
		loc_out = (struct switchtec_port_eq_coeff *)&buf[in_size];
		ret = switchtec_cmd(dev, MRPC_PORT_EQ_STATUS, in, in_size,
				    loc_out, out_size);
		if (ret) {
			fprintf(stderr, "Error in switchtec cmd:%d\n", ret);
			goto end;
		}
	} else if (end == SWITCHTEC_DIAG_FAR_END) {
		in->cmd = MRPC_GEN5_PORT_EQ_FAR_END_TX_COEFF_DUMP;
		rem_out = (struct switchtec_rem_port_eq_coeff *)&buf[in_size];
		ret = switchtec_cmd(dev, MRPC_PORT_EQ_STATUS, in, in_size,
				    rem_out, out_size);
		if (ret) {
			fprintf(stderr, "Error in switchtec cmd:%d\n", ret);
			goto end;
		}
	} else {
		fprintf(stderr, "Error inval end request\n");
		errno = -EINVAL;
		goto end;
	}

	if (end == SWITCHTEC_DIAG_LOCAL) {
		coeff->lane_cnt = loc_out->lane_cnt + 1;
		for (i = 0; i < coeff->lane_cnt; i++) {
			coeff->cursors[i].pre = loc_out->cursors[i].pre;
			coeff->cursors[i].post = loc_out->cursors[i].post;
		}
	} else {
		coeff->lane_cnt = rem_out->lane_cnt + 1;
		for (i = 0; i < coeff->lane_cnt; i++) {
			coeff->cursors[i].pre = rem_out->cursors[i].pre;
			coeff->cursors[i].post = rem_out->cursors[i].post;
		}
	}

end:
	if (buf)
		free(buf);

	return ret;
}

int switchtec_diag_port_eq_tx_table_gen5(struct switchtec_dev *dev,
					 int port_id, int prev_speed,
					 int link, void *res)
{
	struct switchtec_port_eq_table *table = res;
	struct switchtec_gen5_port_eq_table out = {};
	struct switchtec_port_eq_table_in in = {
		.sub_cmd = MRPC_GEN5_PORT_EQ_FAR_END_TX_EQ_TABLE_DUMP,
		.port_id = port_id,
	};
	int ret, i;

	if (!table) {
		errno = -EINVAL;
		return -1;
	}

	in.dump_type = LANE_EQ_DUMP_TYPE_CURR;
	in.prev_rate = 0;

	if (link == SWITCHTEC_DIAG_LINK_PREVIOUS) {
		in.dump_type = LANE_EQ_DUMP_TYPE_PREV;
		in.prev_rate = prev_speed;
	}

	ret = switchtec_cmd(dev, MRPC_PORT_EQ_STATUS, &in,
			    sizeof(struct switchtec_port_eq_table_in),
			    &out, sizeof(struct switchtec_gen5_port_eq_table));
	if (ret)
		return -1;

	table->lane_id = out.lane_id;
	table->step_cnt = out.step_cnt;

	for (i = 0; i < table->step_cnt; i++) {
		table->steps[i].pre_cursor = out.steps[i].pre_cursor;
		table->steps[i].post_cursor = out.steps[i].post_cursor;
		table->steps[i].fom = 0;
		table->steps[i].pre_cursor_up = 0;
		table->steps[i].post_cursor_up = 0;
		table->steps[i].error_status = out.steps[i].error_status;
		table->steps[i].active_status = out.steps[i].active_status;
		table->steps[i].speed = out.steps[i].speed;
	}

	return 0;
}

int switchtec_diag_port_eq_tx_fslf_gen5(struct switchtec_dev *dev,
					int port_id, int prev_speed,
					int lane_id, int end, int link,
					void *res)
{
	struct switchtec_port_eq_tx_fslf *fslf = res;
	struct switchtec_port_eq_tx_fslf_in in = {};
	struct switchtec_port_eq_tx_fslf_out out = {};
	int ret;

	if (!fslf) {
		errno = -EINVAL;
		return -1;
	}

	in.port_id = port_id;
	in.lane_id = lane_id;

	if (end == SWITCHTEC_DIAG_LOCAL) {
		in.sub_cmd = MRPC_GEN5_PORT_EQ_LOCAL_TX_FSLF_DUMP;
	} else if (end == SWITCHTEC_DIAG_FAR_END) {
		in.sub_cmd = MRPC_GEN5_PORT_EQ_FAR_END_TX_FSLF_DUMP;
	} else {
		errno = -EINVAL;
		return -1;
	}

	if (link == SWITCHTEC_DIAG_LINK_CURRENT) {
		in.dump_type = LANE_EQ_DUMP_TYPE_CURR;
	} else {
		in.dump_type = LANE_EQ_DUMP_TYPE_PREV;
		in.prev_rate = prev_speed;
	}

	ret = switchtec_cmd(dev, MRPC_PORT_EQ_STATUS, &in,
			    sizeof(struct switchtec_port_eq_tx_fslf_in), &out,
			    sizeof(struct switchtec_port_eq_tx_fslf_out));
	if (ret)
		return -1;

	fslf->fs = out.fs;
	fslf->lf = out.lf;

	return 0;
}

int switchtec_diag_loopback_set_gen5(struct switchtec_dev *dev,
				     int port_id, int enable,
				     int enable_parallel, int enable_external,
				     int enable_ltssm, int enable_pipe,
				     int ltssm_speed)
{
	struct switchtec_diag_loopback_in int_in = {
		.sub_cmd = MRPC_LOOPBACK_SET_INT_LOOPBACK,
		.port_id = port_id,
		.enable = 1,
	};
	struct switchtec_diag_loopback_ltssm_in ltssm_in = {
		.sub_cmd = MRPC_LOOPBACK_SET_LTSSM_LOOPBACK,
		.port_id = port_id,
		.enable = enable_ltssm,
		.speed = ltssm_speed,
	};
	int ret;

	if (enable_ltssm && !(enable_external || enable_parallel)) {
		ret = switchtec_cmd(dev, MRPC_INT_LOOPBACK, &ltssm_in,
				    sizeof(ltssm_in), NULL, 0);
		if (ret)
			return ret;
	} else {
		int_in.type = DIAG_LOOPBACK_PARALEL_DATAPATH;
		int_in.enable = enable_parallel;
		ret = switchtec_cmd(dev, MRPC_INT_LOOPBACK, &int_in,
				    sizeof(int_in), NULL, 0);
		if (ret)
			return ret;
		if (!enable_parallel) {
			int_in.type = DIAG_LOOPBACK_EXTERNAL_DATAPATH;
			int_in.enable = enable_external;
			ret = switchtec_cmd(dev, MRPC_INT_LOOPBACK, &int_in,
					    sizeof(int_in), NULL, 0);
			if (ret)
				return ret;
		}
		int_in.type = DIAG_LOOPBACK_PIPE_DATAPATH;
		int_in.enable = enable_pipe;
		ret = switchtec_cmd(dev, MRPC_INT_LOOPBACK, &int_in,
				    sizeof(int_in), NULL, 0);
		if (ret)
			return ret;

		ltssm_in.enable = enable_ltssm;
		ret = switchtec_cmd(dev, MRPC_INT_LOOPBACK, &ltssm_in,
				    sizeof(ltssm_in), NULL, 0);
		if (ret)
			return ret;
	}
	return 0;
}

int switchtec_diag_loopback_get_gen5(struct switchtec_dev *dev,
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

	int_in.type = DIAG_LOOPBACK_PARALEL_DATAPATH;

	ret = switchtec_cmd(dev, MRPC_INT_LOOPBACK, &int_in, sizeof(int_in),
			    &int_out, sizeof(int_out));
	if (ret)
		return ret;

	if (int_out.enabled)
		en |= SWITCHTEC_DIAG_LOOPBACK_RX_TO_TX;

	int_in.type = DIAG_LOOPBACK_EXTERNAL_DATAPATH;

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

static int switchtec_diag_eye_status_gen5(struct switchtec_dev *dev)
{
	int ret;
	int eye_status;

	struct switchtec_gen5_diag_eye_status_in in = {
		.sub_cmd = MRPC_EYE_CAP_STATUS_GEN5,
	};
	struct switchtec_gen5_diag_eye_status_out out;

	do {
		ret = switchtec_cmd(dev, MRPC_GEN5_EYE_CAPTURE, &in, sizeof(in),
				    &out, sizeof(out));
		if (ret) {
			switchtec_perror("eye_status");
			return -1;
		}
		eye_status = out.status;
		usleep(200000);
	} while (eye_status == SWITCHTEC_GEN5_DIAG_EYE_STATUS_IN_PROGRESS ||
		 eye_status == SWITCHTEC_GEN5_DIAG_EYE_STATUS_PENDING);

	switch (eye_status) {
		case SWITCHTEC_GEN5_DIAG_EYE_STATUS_IDLE:
			switchtec_perror("Eye capture idle");
		case SWITCHTEC_GEN5_DIAG_EYE_STATUS_DONE:
			return 0;
		case SWITCHTEC_GEN5_DIAG_EYE_STATUS_TIMEOUT:
			switchtec_perror("Eye capture timeout");
		case SWITCHTEC_GEN5_DIAG_EYE_STATUS_ERROR:
			switchtec_perror("Eye capture error");
		return -1;
	}
	switchtec_perror("Unknown eye capture state");
	return -1;
}

int switchtec_diag_eye_cmd_gen5(struct switchtec_dev *dev, void *in,
				       size_t size)
{
	int ret;

	ret = switchtec_cmd(dev, MRPC_GEN5_EYE_CAPTURE, in, size,
			    NULL, 0);
	if (ret)
		return ret;

	usleep(200000);

	return switchtec_diag_eye_status_gen5(dev);
}

int switchtec_diag_eye_start_gen5(struct switchtec_dev *dev, int lane_mask[4],
				struct range *x_range, struct range *y_range,
				int step_interval, int capture_depth, int sar_sel,
				int intleav_sel, int hstep, int data_mode, 
				int eye_mode, uint64_t refclk, int vstep)
{
	int err, ret;
	struct switchtec_gen5_diag_eye_run_in in = {
		.sub_cmd = MRPC_EYE_CAP_RUN_GEN5,
		.capture_depth = capture_depth,
		.timeout_disable = 1,
		.lane_mask[0] = lane_mask[0],
		.lane_mask[1] = lane_mask[1],
		.lane_mask[2] = lane_mask[2],
		.lane_mask[3] = lane_mask[3],
	};

	ret = switchtec_diag_eye_cmd_gen5(dev, &in, sizeof(in));
	err = errno;
	errno = err;
	return ret;
}

/**
 * @brief Start a PCIe Eye Read Gen5
 * @param[in]  dev	       Switchtec device handle
 * @param[in]  lane_id         lane_id
 * @param[in]  bin             bin
 * @param[in]  num_phases      pointer to the number of phases
 * @param[in]  ber_data        pointer to the Ber data
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_eye_read_gen5(struct switchtec_dev *dev, int lane_id,
		      	    int bin, int* num_phases, double* ber_data)
{
	struct switchtec_gen5_diag_eye_read_in in = {
		.sub_cmd = MRPC_EYE_CAP_READ_GEN5,
		.lane_id = lane_id,
		.bin = bin,
	};
	struct switchtec_gen5_diag_eye_read_out out;
	int i, ret;

	ret = switchtec_cmd(dev, MRPC_GEN5_EYE_CAPTURE, &in, sizeof(in),
			    &out, sizeof(out));
	if (ret)
		return ret;

	*num_phases = out.num_phases;

	for(i = 0; i < out.num_phases; i++)
		ber_data[i] = le64toh(out.ber_data[i]) / 281474976710656.;

	return ret;
}

int switchtec_diag_pattern_gen_set_gen5(struct switchtec_dev *dev, int port_id,
					int type, int link_speed)
{
	struct switchtec_diag_pat_gen_in in = {
		.sub_cmd = MRPC_PAT_GEN_SET_GEN_GEN5,
		.port_id = port_id,
		.pattern_type = type,
		.lane_id = link_speed
	};

	return switchtec_cmd(dev, MRPC_PAT_GEN, &in, sizeof(in), NULL, 0);
}

int switchtec_inject_err_tlp_lcrc_gen5(struct switchtec_dev *dev,
				       int phys_port_id, int enable,
				       uint8_t rate)
{
	uint32_t output;

	struct switchtec_lnkerr_tlp_lcrc_gen5_in cmd = {
		.subcmd = MRPC_ERR_INJ_TLP_LCRC,
		.phys_port_id = phys_port_id,
		.enable = enable,
		.rate = rate,
	};

	return switchtec_cmd(dev, MRPC_MRPC_ERR_INJ, &cmd,
			     sizeof(cmd), &output, sizeof(output));
}

int switchtec_inject_err_cto_gen5(struct switchtec_dev *dev, int phys_port_id)
{
	uint32_t output;

	struct switchtec_lnkerr_cto_in cmd = {
		.subcmd = MRPC_ERR_INJ_CTO,
		.phys_port_id = phys_port_id,
	};

	return switchtec_cmd(dev, MRPC_MRPC_ERR_INJ, &cmd,
			     sizeof(cmd), &output, sizeof(output));
}

int switchtec_osa_capture_data_gen5(struct switchtec_dev *dev, int stack_id, 
                                    int lane, int direction, 
                                    struct switchtec_osa_capture_data *data)
{
	int ret = 0;
	struct {
		uint8_t sub_cmd;
		uint8_t stack_id;
		uint8_t lane;
		uint8_t direction;
		uint16_t start_entry;
		uint8_t num_entries;
		uint8_t reserved;
	} osa_data_read_in;

	struct {
		uint8_t entries_read;
		uint8_t stack_id;
		uint8_t lane;
		uint8_t direction;
		uint16_t next_entry;
		uint16_t entries_remaining;
		uint16_t wrap;
		uint16_t reserved;
	} osa_data_entries_out;

	osa_data_read_in.sub_cmd = MRPC_OSA_DATA_READ;
	osa_data_read_in.stack_id = stack_id;
	osa_data_read_in.lane = lane;
	osa_data_read_in.direction = direction;

	osa_data_read_in.start_entry = 0;
	osa_data_read_in.num_entries = 0;

	struct {
		uint8_t sub_cmd;
		uint8_t stack_id;
		uint16_t reserved;
	} osa_status_query_in;

	struct {
		uint8_t state;
		uint8_t trigger_lane;
		uint8_t trigger_dir;
		uint8_t reserved;
		uint16_t trigger_reason;
		uint16_t reserved2;
	} osa_status_query_out;

	osa_status_query_in.sub_cmd = MRPC_OSA_STATUS_QUERY;
	osa_status_query_in.stack_id = stack_id;

	ret = switchtec_cmd(dev, MRPC_ORDERED_SET_ANALYZER, &osa_status_query_in,
			    sizeof(osa_status_query_in), &osa_status_query_out,
			    sizeof(osa_status_query_out));

	ret = switchtec_cmd(dev, MRPC_ORDERED_SET_ANALYZER, &osa_data_read_in,
			    sizeof(osa_data_read_in), &osa_data_entries_out,
			    sizeof(osa_data_entries_out));
	if (ret) {
		switchtec_perror("OSA data dump");
		return ret;
	}

	struct {
		uint8_t entries_read;
		uint8_t stack_id;
		uint8_t lane;
		uint8_t direction;
		uint16_t next_entry;
		uint16_t entries_remaining;
		uint16_t wrap;
		uint16_t reserved;
		uint32_t entry_dwords[osa_data_entries_out.entries_remaining * 6];
	} osa_data_read_out;

	osa_data_read_out.entries_remaining = osa_data_entries_out.entries_remaining;
	osa_data_read_out.next_entry = osa_data_entries_out.next_entry;

	if (data) {
		data->stack_id = stack_id;
		data->lane = lane;
		data->direction = direction;
		data->total_entries = osa_data_entries_out.entries_remaining;
		data->wrap_occurred = osa_data_entries_out.wrap;
		data->entry_count = 0;
	}

	int total_dword = 0;
	int total_entries = 0;
	int curr_entry_dword = 0;
	uint64_t timestamp;
	uint32_t timestamp_lower, timestamp_upper, counter;
	uint8_t link_rate, trigger, os_droppped;
	uint32_t osa_data_dwords[4];
	int osa_data_idx = 0;

	while (osa_data_read_out.entries_remaining != 0) {
		if (data && total_entries >= SWITCHTEC_OSA_MAX_ENTRIES)
			break;

		osa_data_read_in.num_entries = osa_data_read_out.entries_remaining;
		osa_data_read_in.start_entry = osa_data_read_out.next_entry;

		ret = switchtec_cmd(dev, MRPC_ORDERED_SET_ANALYZER,
				    &osa_data_read_in, sizeof(osa_data_read_in),
				    &osa_data_read_out, sizeof(osa_data_read_out));
		if (ret)
			return -1;

		for (int i = total_dword; i < total_dword + (osa_data_read_out.entries_read * 6); i++) {
			if (curr_entry_dword < 4) {
				osa_data_dwords[osa_data_idx++] = osa_data_read_out.entry_dwords[i];
			} else if (curr_entry_dword == 4) {
				timestamp_lower = (osa_data_read_out.entry_dwords[i] >> 22) & 0x3FF;
				timestamp_upper = (osa_data_read_out.entry_dwords[i+1] & 0x1A);
				timestamp = timestamp_upper | timestamp_lower;

				link_rate = osa_data_read_out.entry_dwords[i] & 0x3;
				counter = (osa_data_read_out.entry_dwords[i] >> 3) & 0x12;
				trigger	= (osa_data_read_out.entry_dwords[i+1] >> 28) & 0x1;
				os_droppped = (osa_data_read_out.entry_dwords[i+1] >> 29) & 0x1;

				if (data && total_entries < SWITCHTEC_OSA_MAX_ENTRIES) {
					data->entries[total_entries].timestamp = timestamp;
					data->entries[total_entries].link_rate = link_rate;
					data->entries[total_entries].counter = counter;
					data->entries[total_entries].trigger_indication = trigger;
					data->entries[total_entries].os_dropped = os_droppped;
					data->entries[total_entries].osa_data[0] = osa_data_dwords[0];
					data->entries[total_entries].osa_data[1] = osa_data_dwords[1];
					data->entries[total_entries].osa_data[2] = osa_data_dwords[2];
					data->entries[total_entries].osa_data[3] = osa_data_dwords[3];
					data->entry_count++;
				}

				osa_data_idx = 0;
				total_entries++;
			}
			curr_entry_dword++;
			if (i != 0 && i % 5 == 0) {
				curr_entry_dword = 0;
			}
		}
		total_dword += osa_data_read_out.entries_read;
	}

	return ret;
}

int switchtec_osa_capture_control_gen5(struct switchtec_dev *dev, int stack_id,
				  int lane_mask, int direction,
				  int drop_single_os, int stop_mode,
				  int snapshot_mode, int post_trigger,
				  int os_types)
{
	int ret = 0;

	struct osa_capture_ctrl_in osa_capture_ctrl_in = {0};

	osa_capture_ctrl_in.sub_cmd = MRPC_OSA_CAPTURE_CTRL;
	osa_capture_ctrl_in.stack_id = stack_id;
	osa_capture_ctrl_in.lane_mask = lane_mask;
	osa_capture_ctrl_in.direction = direction;
	osa_capture_ctrl_in.drop_single_os = drop_single_os;
	osa_capture_ctrl_in.stop_mode = stop_mode;
	osa_capture_ctrl_in.snapshot_mode = snapshot_mode;
	osa_capture_ctrl_in.post_trig_entries = post_trigger;
	osa_capture_ctrl_in.os_types = os_types;

	ret = switchtec_cmd(dev, MRPC_ORDERED_SET_ANALYZER, &osa_capture_ctrl_in,
			    sizeof(osa_capture_ctrl_in), NULL, 0);
	if (ret)
		switchtec_perror("OSA capture control");

	return ret;
}

int switchtec_osa_config_misc_gen5(struct switchtec_dev *dev, int stack_id,
				int trigger_en)
{
	int ret = 0;
	struct {
		uint8_t sub_cmd;
		uint8_t stack_id;
		uint16_t reserved;
		uint8_t trigger_en;
		uint8_t reserved2;
		uint16_t reserved3;
	} osa_misc_config_in;

	osa_misc_config_in.sub_cmd = MRPC_OSA_MISC_TRIG_CONFIG;
	osa_misc_config_in.stack_id = stack_id;
	osa_misc_config_in.trigger_en = trigger_en;

	ret = switchtec_cmd(dev, MRPC_ORDERED_SET_ANALYZER, &osa_misc_config_in,
			    sizeof(osa_misc_config_in), NULL, 0);
	if (ret)
		switchtec_perror("OSA misc config");

	return ret;
}

int switchtec_osa_config_pattern_gen5(struct switchtec_dev *dev, int stack_id,
				 int direction, int lane_mask, int link_rate,
				 uint32_t *value_data, uint32_t *mask_data)
{
	int ret = 1;

	struct osa_pattern_config_in osa_pattern_config_in = {0};
	osa_pattern_config_in.sub_cmd = MRPC_OSA_PAT_TRIG_CONFIG;
	osa_pattern_config_in.stack_id = stack_id;
	osa_pattern_config_in.direction = direction;
	osa_pattern_config_in.lane_mask = lane_mask;
	osa_pattern_config_in.link_rate = link_rate;
	osa_pattern_config_in.pat_val_dword0 = value_data[0];
	osa_pattern_config_in.pat_val_dword1 = value_data[1];
	osa_pattern_config_in.pat_val_dword2 = value_data[2];
	osa_pattern_config_in.pat_val_dword3 = value_data[3];
	osa_pattern_config_in.pat_mask_dword0 = mask_data[0];
	osa_pattern_config_in.pat_mask_dword1 = mask_data[1];
	osa_pattern_config_in.pat_mask_dword2 = mask_data[2];
	osa_pattern_config_in.pat_mask_dword3 = mask_data[3];

	ret = switchtec_cmd(dev, MRPC_ORDERED_SET_ANALYZER,
			    &osa_pattern_config_in,
			    sizeof(osa_pattern_config_in), NULL, 0);
	if (ret)
		switchtec_perror("OSA pattern config");

	return ret;
}

int switchtec_osa_config_type_gen5(struct switchtec_dev *dev, int stack_id,
		int direction, int lane_mask, int link_rate, int os_types)
{
	int ret = 1;

	struct osa_type_config_in osa_type_config_in = {0};

	osa_type_config_in.sub_cmd = MRPC_OSA_TYPE_TRIG_CONFIG;
	osa_type_config_in.stack_id = stack_id;
	osa_type_config_in.lane_mask = lane_mask;
	osa_type_config_in.direction = direction;
	osa_type_config_in.link_rate = link_rate;
	osa_type_config_in.os_types = os_types;

	ret = switchtec_cmd(dev, MRPC_ORDERED_SET_ANALYZER, &osa_type_config_in,
			    sizeof(osa_type_config_in), NULL, 0);
	if (ret)
		switchtec_perror("OSA type config");

	return ret;
}

int switchtec_osa_dump_conf_gen5(struct switchtec_dev *dev, int stack_id,
			    struct switchtec_osa_config *config)
{
	int ret = 0;

	struct {
		uint8_t sub_cmd;
		uint8_t stack_id;
		uint16_t reserved;
	} osa_dmp_in;

	struct {
		int16_t os_type_trig_lane_mask;
		uint8_t os_type_trig_dir;
		uint8_t os_type_trig_link_rate;
		uint8_t os_type_trig_os_types;
		uint8_t reserved;
		uint16_t reserved2;
		uint16_t os_pat_trig_lane_mask;
		uint8_t os_pat_trig_dir;
		uint8_t os_pat_trig_link_rate;
		uint32_t os_pat_trig_val_dw0;
		uint32_t os_pat_trig_val_dw1;
		uint32_t os_pat_trig_val_dw2;
		uint32_t os_pat_trig_val_dw3;
		uint32_t os_pat_trig_mask_dw0;
		uint32_t os_pat_trig_mask_dw1;
		uint32_t os_pat_trig_mask_dw2;
		uint32_t os_pat_trig_mask_dw3;
		uint8_t misc_trig_en;
		uint8_t reserved3;
		uint16_t reserved4;
		uint16_t capture_lane_mask;
		uint8_t capture_dir;
		uint8_t capture_drop_os;
		uint8_t capture_stop_mode;
		uint8_t capture_snap_mode;
		uint16_t capture_post_trig_entries;
		uint8_t capture_os_types;
		uint8_t reserved5;
		uint16_t reserved6;
	} osa_dmp_out;

	osa_dmp_in.stack_id = stack_id;
	osa_dmp_in.sub_cmd = MRPC_OSA_CONFIG_DMP;

	ret = switchtec_cmd(dev, MRPC_ORDERED_SET_ANALYZER, &osa_dmp_in,
			    sizeof(osa_dmp_in), &osa_dmp_out,
			    sizeof(osa_dmp_out));
	if (ret) {
		switchtec_perror("OSA config dump");
		return ret;
	}

	if (config) {
		/* OS Type Trigger */
		config->os_type_lane_mask = osa_dmp_out.os_type_trig_lane_mask;
		config->os_type_direction = osa_dmp_out.os_type_trig_dir;
		config->os_type_link_rate = osa_dmp_out.os_type_trig_link_rate;
		config->os_type_os_types = osa_dmp_out.os_type_trig_os_types;

		/* OS Pattern Trigger */
		config->os_pat_lane_mask = osa_dmp_out.os_pat_trig_lane_mask;
		config->os_pat_direction = osa_dmp_out.os_pat_trig_dir;
		config->os_pat_link_rate = osa_dmp_out.os_pat_trig_link_rate;
		config->os_pat_value[0] = osa_dmp_out.os_pat_trig_val_dw0;
		config->os_pat_value[1] = osa_dmp_out.os_pat_trig_val_dw1;
		config->os_pat_value[2] = osa_dmp_out.os_pat_trig_val_dw2;
		config->os_pat_value[3] = osa_dmp_out.os_pat_trig_val_dw3;
		config->os_pat_mask[0] = osa_dmp_out.os_pat_trig_mask_dw0;
		config->os_pat_mask[1] = osa_dmp_out.os_pat_trig_mask_dw1;
		config->os_pat_mask[2] = osa_dmp_out.os_pat_trig_mask_dw2;
		config->os_pat_mask[3] = osa_dmp_out.os_pat_trig_mask_dw3;

		/* Misc Trigger */
		config->misc_trig_enable = osa_dmp_out.misc_trig_en;

		/* Capture Config */
		config->capture_lane_mask = osa_dmp_out.capture_lane_mask;
		config->capture_direction = osa_dmp_out.capture_dir;
		config->capture_drop_single_os = osa_dmp_out.capture_drop_os;
		config->capture_stop_mode = osa_dmp_out.capture_stop_mode;
		config->capture_snapshot_mode = osa_dmp_out.capture_snap_mode;
		config->capture_post_trig_entries = osa_dmp_out.capture_post_trig_entries;
		config->capture_os_types = osa_dmp_out.capture_os_types;
	}

	return ret;
}

int switchtec_osa_gen5(struct switchtec_dev *dev, int stack_id, int operation,
		  struct switchtec_osa_status *status)
{
	int ret = 0;
	struct {
		uint8_t sub_cmd;
		uint8_t stack_id;
		uint16_t reserved;
	} osa_rel_access_perm_in;

	struct {
		uint8_t sub_cmd;
		uint8_t stack_id;
		uint16_t reserved;
	} osa_status_query_in;

	struct {
		uint8_t state;
		uint8_t trigger_lane;
		uint8_t trigger_dir;
		uint8_t reserved;
		uint16_t trigger_reason;
		uint16_t reserved2;
	} osa_status_query_out;

	struct {
		uint8_t sub_cmd;
		uint8_t stack_id;
		uint8_t operation;
		uint8_t reserved;
	} osa_op_in;

	if (operation == 4) {
		osa_rel_access_perm_in.sub_cmd = MRPC_OSA_REL_ACCESS_PERM;
		osa_rel_access_perm_in.stack_id = stack_id;

		ret = switchtec_cmd(dev, MRPC_ORDERED_SET_ANALYZER,
				    &osa_rel_access_perm_in,
				    sizeof(osa_rel_access_perm_in), NULL, 0);
	}
	else if (operation == 5) {
		osa_status_query_in.sub_cmd = MRPC_OSA_STATUS_QUERY;
		osa_status_query_in.stack_id = stack_id;

		ret = switchtec_cmd(dev, MRPC_ORDERED_SET_ANALYZER,
			&osa_status_query_in, sizeof(osa_status_query_in),
			&osa_status_query_out, sizeof(osa_status_query_out));
		if (ret) {
			switchtec_perror("OSA operation");
			return ret;
		}

		if (status) {
			status->state = osa_status_query_out.state;
			status->trigger_lane = osa_status_query_out.trigger_lane;
			status->trigger_dir = osa_status_query_out.trigger_dir;
			status->trigger_reason = osa_status_query_out.trigger_reason;
		}
	}
	else {
		osa_op_in.sub_cmd = MRPC_OSA_ANALYZER_OP;
		osa_op_in.stack_id = stack_id;
		osa_op_in.operation = operation;

		ret = switchtec_cmd(dev, MRPC_ORDERED_SET_ANALYZER, &osa_op_in,
				    sizeof(osa_op_in), NULL, 0);
	}

	if (ret)
		switchtec_perror("OSA operation");

	return ret;
}