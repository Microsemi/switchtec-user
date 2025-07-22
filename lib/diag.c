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
#define SWITCHTEC_LTSSM_MAX_LOGS 61

#include "switchtec_priv.h"
#include "switchtec/diag.h"
#include "switchtec/endian.h"
#include "switchtec/switchtec.h"
#include "switchtec/utils.h"

#include <errno.h>
#include <math.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

/**
 * @brief Enable cross hair on specified lane
 * @param[in]  dev	Switchtec device handle
 * @param[in]  lane_id	Lane to enable, or SWITCHTEC_DIAG_CROSS_HAIR_ALL_LANES
 *			for all lanes.
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_cross_hair_enable(struct switchtec_dev *dev, int lane_id)
{
	struct switchtec_diag_cross_hair_in in = {
		.sub_cmd = MRPC_CROSS_HAIR_ENABLE,
		.lane_id = lane_id,
		.all_lanes = lane_id == SWITCHTEC_DIAG_CROSS_HAIR_ALL_LANES,
	};

	return switchtec_cmd(dev, MRPC_CROSS_HAIR, &in, sizeof(in), NULL, 0);
}

/**
 * @brief Disable active cross hair
 * @param[in]  dev	Switchtec device handle
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_cross_hair_disable(struct switchtec_dev *dev)
{
	struct switchtec_diag_cross_hair_in in = {
		.sub_cmd = MRPC_CROSS_HAIR_DISABLE,
	};

	return switchtec_cmd(dev, MRPC_CROSS_HAIR, &in, sizeof(in), NULL, 0);
}

/**
 * @brief Disable active cross hair
 * @param[in]  dev		Switchtec device handle
 * @param[in]  start_lane_id	Start lane ID to get
 * @param[in]  num_lanes	Number of lanes to get
 * @param[out] res		Resulting cross hair data
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_cross_hair_get(struct switchtec_dev *dev, int start_lane_id,
		int num_lanes, struct switchtec_diag_cross_hair *res)
{
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
		memset(&res[i], 0, sizeof(res[i]));
		res[i].state = out[i].state;
		res[i].lane_id = out[i].lane_id;

		if (out[i].state <= SWITCHTEC_DIAG_CROSS_HAIR_WAITING) {
			continue;
		} else if (out[i].state < SWITCHTEC_DIAG_CROSS_HAIR_DONE) {
			res[i].x_pos = out[i].x_pos;
			res[i].y_pos = out[i].y_pos;
		} else if (out[i].state == SWITCHTEC_DIAG_CROSS_HAIR_DONE) {
			res[i].eye_left_lim = out[i].eye_left_lim;
			res[i].eye_right_lim = out[i].eye_right_lim;
			res[i].eye_bot_left_lim = out[i].eye_bot_left_lim;
			res[i].eye_bot_right_lim = out[i].eye_bot_right_lim;
			res[i].eye_top_left_lim = out[i].eye_top_left_lim;
			res[i].eye_top_right_lim = out[i].eye_top_right_lim;
		} else if (out[i].state == SWITCHTEC_DIAG_CROSS_HAIR_ERROR) {
			res[i].x_pos = out[i].x_pos;
			res[i].y_pos = out[i].y_pos;
			res[i].prev_state = out[i].prev_state;
		}
	}

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

static int switchtec_diag_eye_cmd_gen5(struct switchtec_dev *dev, void *in,
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

/**
 * @brief Set the data mode for the next Eye Capture
 * @param[in]  dev	       Switchtec device handle
 * @param[in]  mode	       Mode to use (raw or ratio)
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_eye_set_mode(struct switchtec_dev *dev,
				enum switchtec_diag_eye_data_mode mode)
{
	struct switchtec_diag_port_eye_cmd in = {
		.sub_cmd = MRPC_EYE_OBSERVE_SET_DATA_MODE,
		.data_mode = mode,
	};

	return switchtec_diag_eye_cmd_gen4(dev, &in, sizeof(in));
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
int switchtec_diag_eye_read(struct switchtec_dev *dev, int lane_id,
		      	    int bin, int* num_phases, double* ber_data)
{
	if (dev) {
		fprintf(stderr, "Eye read not supported on Gen 4 switches.\n");
		return -1;
	}
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

/**
 * @brief Start a PCIe Eye Capture
 * @param[in]  dev	       Switchtec device handle
 * @param[in]  lane_mask       Bitmap of the lanes to capture
 * @param[in]  x_range         Time range: start should be between 0 and 63,
 *			       end between start and 63.
 * @param[in]  y_range         Voltage range: start should be between -255 and 255,
 *			       end between start and 255.
 * @param[in]  step_interval   Sampling time in milliseconds for each step
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_eye_start(struct switchtec_dev *dev, int lane_mask[4],
			     struct range *x_range, struct range *y_range,
			     int step_interval, int capture_depth)
{
	int err, ret;
	if (switchtec_is_gen5(dev)) {
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
	} else {
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
	return -1;
}

static uint64_t hi_lo_to_uint64(uint32_t lo, uint32_t hi)
{
	uint64_t ret;

	ret = le32toh(hi);
	ret <<= 32;
	ret |= le32toh(lo);

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
int switchtec_diag_eye_fetch(struct switchtec_dev *dev, double *pixels,
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
int switchtec_diag_eye_cancel(struct switchtec_dev *dev)
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

static int switchtec_diag_loopback_set_gen5(struct switchtec_dev *dev,
					    int port_id, int enable_parallel,
					    int enable_external,
					    int enable_ltssm,
					    enum switchtec_diag_ltssm_speed
					    ltssm_speed)
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

		ltssm_in.enable = enable_ltssm;
		ret = switchtec_cmd(dev, MRPC_INT_LOOPBACK, &ltssm_in,
				    sizeof(ltssm_in), NULL, 0);
      		if (ret)
	       		return ret;
	}
	return 0;
}

static int switchtec_diag_loopback_set_gen4(struct switchtec_dev *dev,
					    int port_id, int enable,
					    enum switchtec_diag_ltssm_speed
					    ltssm_speed)
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

/**
 * @brief Setup Loopback Mode
 * @param[in]  dev	    Switchtec device handle
 * @param[in]  port_id	    Physical port ID
 * @param[in]  enable		Enable bitmap - Gen 4
 * @param[in]  enable_parallel	Enable the parallel SERDES loopback - Gen 5
 * @param[in]  enable_external	Enable the external physical loopback - Gen 5
 * @param[in]  enable_ltssm	Enable the ltssm loopback
 * @param[in]  ltssm_speed  LTSSM loopback max speed
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_loopback_set(struct switchtec_dev *dev, int port_id,
				int enable, int enable_parallel,
				int enable_external, int enable_ltssm,
				enum switchtec_diag_ltssm_speed ltssm_speed)
{
	int ret = 0;
	if (switchtec_is_gen5(dev)) {
		ret = switchtec_diag_loopback_set_gen5(dev, port_id,
						       enable_parallel,
						       enable_external,
						       enable_ltssm,
						       ltssm_speed);
		if (ret)
			return ret;
	}
	else {
		ret = switchtec_diag_loopback_set_gen4(dev, port_id, enable,
						       ltssm_speed);
		if (ret)
			return ret;
	}
	return 0;
}

/**
 * @brief Setup Loopback Mode
 * @param[in]  dev	     Switchtec device handle
 * @param[in]  port_id	     Physical port ID
 * @param[out] enabled       Set of enum switchtec_diag_loopback_enable
 *			     indicating which loopback modes are enabled
 * @param[out] ltssm_speed   LTSSM loopback max speed
 *
 * @return 0 on succes, error code on failure
 */
int switchtec_diag_loopback_get(struct switchtec_dev *dev,
				int port_id, int *enabled,
				enum switchtec_diag_ltssm_speed *ltssm_speed)
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

	if (switchtec_is_gen5(dev))
		int_in.type = DIAG_LOOPBACK_PARALEL_DATAPATH;
	else
		int_in.type = DIAG_LOOPBACK_RX_TO_TX;

	ret = switchtec_cmd(dev, MRPC_INT_LOOPBACK, &int_in, sizeof(int_in),
			    &int_out, sizeof(int_out));
	if (ret)
		return ret;

	if (int_out.enabled)
		en |= SWITCHTEC_DIAG_LOOPBACK_RX_TO_TX;

	if (switchtec_is_gen5(dev))
		int_in.type = DIAG_LOOPBACK_EXTERNAL_DATAPATH;
	else
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

/**
 * @brief Setup Pattern Generator
 * @param[in]  dev	 Switchtec device handle
 * @param[in]  port_id	 Physical port ID
 * @param[in]  type      Pattern type to enable
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_pattern_gen_set(struct switchtec_dev *dev, int port_id,
				   enum switchtec_diag_pattern type,
				   enum switchtec_diag_pattern_link_rate link_speed)
{
	struct switchtec_diag_pat_gen_in in = {
		.sub_cmd = MRPC_PAT_GEN_SET_GEN,
		.port_id = port_id,
		.pattern_type = type,
		.lane_id = link_speed
	};
	if (switchtec_is_gen5(dev))
		in.sub_cmd = MRPC_PAT_GEN_SET_GEN_GEN5;

	return switchtec_cmd(dev, MRPC_PAT_GEN, &in, sizeof(in), NULL, 0);
}

/**
 * @brief Get Pattern Generator set on port
 * @param[in]  dev	 Switchtec device handle
 * @param[in]  port_id	 Physical port ID
 * @param[out] type      Pattern type to enable
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_pattern_gen_get(struct switchtec_dev *dev, int port_id,
				   enum switchtec_diag_pattern *type)
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

/**
 * @brief Setup Pattern Monitor
 * @param[in]  dev	 Switchtec device handle
 * @param[in]  port_id	 Physical port ID
 * @param[in]  type      Pattern type to enable
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_pattern_mon_set(struct switchtec_dev *dev, int port_id,
				   enum switchtec_diag_pattern type)
{
	struct switchtec_diag_pat_gen_in in = {
		.sub_cmd = MRPC_PAT_GEN_SET_MON,
		.port_id = port_id,
		.pattern_type = type,
	};

	return switchtec_cmd(dev, MRPC_PAT_GEN, &in, sizeof(in), NULL, 0);
}

/**
 * @brief Get Pattern Monitor
 * @param[in]  dev	 Switchtec device handle
 * @param[in]  port_id	 Physical port ID
 * @param[out] type      Pattern type to enable
 * @param[out] err_cnt   Number of errors seen
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_pattern_mon_get(struct switchtec_dev *dev, int port_id,
				   int lane_id, enum switchtec_diag_pattern *type,
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

/**
 * @brief Inject error into pattern generator
 * @param[in]  dev	 Switchtec device handle
 * @param[in]  port_id	 Physical port ID
 * @param[in] err_cnt   Number of errors seen
 *
 * Injects up to err_cnt errors into each lane of the TX port. It's
 * recommended that the err_cnt be less than 1000, otherwise the
 * firmware runs the risk of consuming too many resources and crashing.
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_pattern_inject(struct switchtec_dev *dev, int port_id,
				  unsigned int err_cnt)
{
	struct switchtec_diag_pat_gen_inject in = {
		.sub_cmd = MRPC_PAT_GEN_INJ_ERR,
		.port_id = port_id,
		.err_cnt = err_cnt,
	};
	int ret;

	ret = switchtec_cmd(dev, MRPC_PAT_GEN, &in, sizeof(in), NULL, 0);
	if (ret)
		return ret;

	return 0;
}

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
 * @brief Get the Gen5 port equalization TX coefficients
 * @param[in]  dev	Switchtec device handle
 * @param[in]  port_id	Physical port ID
 * @param[in]  end      Get coefficents for the Local or the Far End
 * @param[out] res      Resulting port equalization coefficients
 *
 * @return 0 on success, error code on failure
 */
static int switchtec_gen5_diag_port_eq_tx_coeff(struct switchtec_dev *dev,
						int port_id,
						enum switchtec_diag_end end,
						enum switchtec_diag_link link,
						struct switchtec_port_eq_coeff
						*res)
{
	struct switchtec_port_eq_coeff *loc_out;
	struct switchtec_rem_port_eq_coeff *rem_out;
	struct switchtec_port_eq_coeff_in *in;
	uint8_t *buf;
	uint32_t buf_size;
	uint32_t in_size = sizeof(struct switchtec_port_eq_coeff_in);
	uint32_t out_size = 0;
	int ret = 0;
	int i;

	if (!res) {
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
		in->prev_rate = PCIE_LINK_RATE_GEN5;
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
		res->lane_cnt = loc_out->lane_cnt + 1;
		for (i = 0; i < res->lane_cnt; i++) {
			res->cursors[i].pre = loc_out->cursors[i].pre;
			res->cursors[i].post = loc_out->cursors[i].post;
		}
	} else {
		res->lane_cnt = rem_out->lane_cnt + 1;
		for (i = 0; i < res->lane_cnt; i++) {
			res->cursors[i].pre = rem_out->cursors[i].pre;
			res->cursors[i].post = rem_out->cursors[i].post;
		}
	}

end:
	if (buf)
		free(buf);

	return ret;
}

/**
 * @brief Get the Gen4 port equalization TX coefficients
 * @param[in]  dev	Switchtec device handle
 * @param[in]  port_id	Physical port ID
 * @param[in]  end      Get coefficents for the Local or the Far End
 * @param[out] res      Resulting port equalization coefficients
 *
 * @return 0 on success, error code on failure
 */
static int switchtec_gen4_diag_port_eq_tx_coeff(struct switchtec_dev *dev,
						int port_id,
						enum switchtec_diag_end end,
						enum switchtec_diag_link link,
						struct switchtec_port_eq_coeff
						*res)
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

	res->lane_cnt = out.lane_id + 1;
	for (i = 0; i < res->lane_cnt; i++) {
		res->cursors[i].pre = out.cursors[i].pre;
		res->cursors[i].post = out.cursors[i].post;
	}

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
				    enum switchtec_diag_link link,
				    struct switchtec_port_eq_coeff *res)
{
	int ret = -1;

	if (switchtec_is_gen5(dev))
		ret = switchtec_gen5_diag_port_eq_tx_coeff(dev, port_id, end,
							   link, res);
	else if (switchtec_is_gen4(dev))
		ret = switchtec_gen4_diag_port_eq_tx_coeff(dev, port_id, end,
							   link, res);

	return ret;
}

/**
 * @brief Get the Gen5 far end TX equalization table
 * @param[in]  dev	Switchtec device handle
 * @param[in]  port_id	Physical port ID
 * @param[out] res      Resulting port equalization table
 *
 * @return 0 on success, error code on failure
 */
static int switchtec_gen5_diag_port_eq_tx_table(struct switchtec_dev *dev,
						int port_id,
						enum switchtec_diag_link link,
						struct switchtec_port_eq_table
						*res)
{
	struct switchtec_gen5_port_eq_table out = {};
	struct switchtec_port_eq_table_in in = {
		.sub_cmd = MRPC_GEN5_PORT_EQ_FAR_END_TX_EQ_TABLE_DUMP,
		port_id = port_id,
	};
	int ret, i;

	if (!res) {
		errno = -EINVAL;
		return -1;
	}

	in.dump_type = LANE_EQ_DUMP_TYPE_CURR;
	in.prev_rate = 0;

	if (link == SWITCHTEC_DIAG_LINK_PREVIOUS) {
		in.dump_type = LANE_EQ_DUMP_TYPE_PREV;
		in.prev_rate = PCIE_LINK_RATE_GEN5;
	}

	ret = switchtec_cmd(dev, MRPC_PORT_EQ_STATUS, &in,
			    sizeof(struct switchtec_port_eq_table_in),
			    &out, sizeof(struct switchtec_gen5_port_eq_table));
	if (ret)
		return -1;

	res->lane_id = out.lane_id;
	res->step_cnt = out.step_cnt;

	for (i = 0; i < res->step_cnt; i++) {
		res->steps[i].pre_cursor = out.steps[i].pre_cursor;
		res->steps[i].post_cursor = out.steps[i].post_cursor;
		res->steps[i].fom = 0;
		res->steps[i].pre_cursor_up = 0;
		res->steps[i].post_cursor_up = 0;
		res->steps[i].error_status = out.steps[i].error_status;
		res->steps[i].active_status = out.steps[i].active_status;
		res->steps[i].speed = out.steps[i].speed;
	}

	return 0;
}

/**
 * @brief Get the Gen4 far end TX equalization table
 * @param[in]  dev	Switchtec device handle
 * @param[in]  port_id	Physical port ID
 * @param[out] res      Resulting port equalization table
 *
 * @return 0 on success, error code on failure
 */
static int switchtec_gen4_diag_port_eq_tx_table(struct switchtec_dev *dev,
						int port_id,
						enum switchtec_diag_link link,
						struct switchtec_port_eq_table
						*res)
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
		res->steps[i].pre_cursor = out.steps[i].pre_cursor;
		res->steps[i].post_cursor = out.steps[i].post_cursor;
		res->steps[i].fom = out.steps[i].fom;
		res->steps[i].pre_cursor_up = out.steps[i].pre_cursor_up;
		res->steps[i].post_cursor_up = out.steps[i].post_cursor_up;
		res->steps[i].error_status = out.steps[i].error_status;
		res->steps[i].active_status = out.steps[i].active_status;
		res->steps[i].speed = out.steps[i].speed;
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
	int ret = -1;

	if (switchtec_is_gen5(dev))
		ret = switchtec_gen5_diag_port_eq_tx_table(dev, port_id, link,
							   res);
	else if (switchtec_is_gen4(dev))
		ret = switchtec_gen4_diag_port_eq_tx_table(dev, port_id, link,
							   res);

	return ret;
}

/**
 * @brief Get the Gen5 equalization FS/LF
 * @param[in]  dev	Switchtec device handle
 * @param[in]  port_id	Physical port ID
 * @param[in]  lane_id	Physical port ID
 * @param[in]  end      Get coefficents for the Local or the Far End
 * @param[out] res      Resulting FS/LF values
 *
 * @return 0 on success, error code on failure
 */
static int switchtec_gen5_diag_port_eq_tx_fslf(struct switchtec_dev *dev,
					       int port_id, int lane_id,
					       enum switchtec_diag_end end,
				   	       enum switchtec_diag_link link,
					       struct switchtec_port_eq_tx_fslf
					       *res)
{
	struct switchtec_port_eq_tx_fslf_in in = {};
	struct switchtec_port_eq_tx_fslf_out out = {};
	int ret;

	if (!res) {
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
		in.prev_rate = PCIE_LINK_RATE_GEN5;
	}

	ret = switchtec_cmd(dev, MRPC_PORT_EQ_STATUS, &in,
			    sizeof(struct switchtec_port_eq_tx_fslf_in), &out,
			    sizeof(struct switchtec_port_eq_tx_fslf_out));
	if (ret)
		return -1;

	res->fs = out.fs;
	res->lf = out.lf;

	return 0;
}

/**
 * @brief Get the Gen4 equalization FS/LF
 * @param[in]  dev	Switchtec device handle
 * @param[in]  port_id	Physical port ID
 * @param[in]  lane_id	Physical port ID
 * @param[in]  end      Get coefficents for the Local or the Far End
 * @param[out] res      Resulting FS/LF values
 *
 * @return 0 on success, error code on failure
 */
static int switchtec_gen4_diag_port_eq_tx_fslf(struct switchtec_dev *dev,
					       int port_id, int lane_id,
					       enum switchtec_diag_end end,
					       enum switchtec_diag_link link,
					       struct switchtec_port_eq_tx_fslf
					       *res)
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
	int ret = -1;

	if (switchtec_is_gen5(dev))
		ret = switchtec_gen5_diag_port_eq_tx_fslf(dev, port_id,
							  lane_id, end,
							  link, res);
	else if (switchtec_is_gen4(dev))
		ret = switchtec_gen4_diag_port_eq_tx_fslf(dev, port_id,
							  lane_id, end,
							  link, res);

	return ret;
}

/**
 * @brief Get the Extended Receiver Object
 * @param[in]  dev 	Switchtec device handle
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
	uint32_t perms[(MRPC_MAX_ID + 31) / 32];
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

/**
 * @brief Control the refclk output for a stack
 * @param[in]  dev	Switchtec device handle
 * @param[in]  stack_id	Stack ID to control the refclk of
 * @param[in]  en	Set to true to enable, false to disable
 *
 * @return 0 on success, error code on failure
 */
int switchtec_diag_refclk_ctl(struct switchtec_dev *dev, int stack_id, bool en)
{
	struct switchtec_diag_refclk_ctl_in cmd = {
		.sub_cmd = en ? MRPC_REFCLK_S_ENABLE : MRPC_REFCLK_S_DISABLE,
		.stack_id = stack_id,
	};

	return switchtec_cmd(dev, MRPC_REFCLK_S, &cmd, sizeof(cmd), NULL, 0);
}

static void switchtec_diag_ltssm_set_log_data(struct switchtec_diag_ltssm_log
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
static int switchtec_diag_ltssm_log_gen5(struct switchtec_dev *dev,
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

		switchtec_diag_ltssm_set_log_data(log_data,
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

			switchtec_diag_ltssm_set_log_data(log_data,
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

			switchtec_diag_ltssm_set_log_data(log_data,
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

/**
 * @brief Get the LTSSM log of a port on a gen4 switchtec device
 * @param[in]	dev    Switchtec device handle
 * @param[in]	port   Switchtec Port
 * @param[inout] log_count number of log entries
 * @param[out] log    A pointer to an array containing the log
 */
static int switchtec_diag_ltssm_log_gen4(struct switchtec_dev *dev,
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

/**
 * @brief Determine the generation and call the related LTSSM log func
 * @param[in]	dev    Switchtec device handle
 * @param[in]	port   Switchtec Port
 * @param[inout] log_count number of log entries
 * @param[out] log    A pointer to an array containing the log
 */
int switchtec_diag_ltssm_log(struct switchtec_dev *dev,
			    int port, int *log_count,
			    struct switchtec_diag_ltssm_log *log_data)
{
	int ret;
	if (switchtec_is_gen5(dev))
		ret = switchtec_diag_ltssm_log_gen5(dev, port, log_count, log_data);
	else
		ret = switchtec_diag_ltssm_log_gen4(dev, port, log_count, log_data);
	return ret;
}

int switchtec_tlp_inject(struct switchtec_dev *dev, int port_id, int tlp_type,
			 int tlp_length, int ecrc, uint32_t *raw_tlp_data)
{
	uint32_t tlp_out;
	int ret = 1;
	struct switchtec_tlp_inject_in tlp_in = {
		.dest_port = port_id,
		.tlp_type = tlp_type,
		.tlp_length = tlp_length,
		.ecrc = ecrc
	};
	for (int i = 0; i < tlp_in.tlp_length; i++) {
		tlp_in.raw_tlp_data[i] = htole32(*(raw_tlp_data + i));
	}
	free(raw_tlp_data);

	ret = switchtec_cmd(dev, MRPC_DIAG_TLP_INJECT, &tlp_in, sizeof(tlp_in),
			    &tlp_out, sizeof(tlp_out));
	return ret;
}

/**
 * @brief Call the aer event gen function to generate AER events
 * @param[in]   dev    Switchtec device handle
 * @param[in]   port   Switchtec Port
 * @param[in]   aer_error_id aer error bit
 * @param[out]  trigger_event One of the trigger events
 */
int switchtec_aer_event_gen(struct switchtec_dev *dev, int port_id,
			    int aer_error_id, int trigger_event)
{
	uint32_t output;
	int ret_val;

	struct switchtec_aer_event_gen_in sub_cmd_id = {
		.sub_cmd = trigger_event,
		.phys_port_id = port_id,
		.err_mask = (1 << aer_error_id),
		.hdr_log[0] = 0,
		.hdr_log[1] = 0,
		.hdr_log[2] = 0,
		.hdr_log[3] = 0
	};

	ret_val = switchtec_cmd(dev, MRPC_AER_GEN, &sub_cmd_id,
				sizeof(sub_cmd_id), &output, sizeof(output));
	return ret_val;
}

/**
 * @brief Inject a DLLP into a physical port
 * @param[in] dev	Switchtec device handle
 * @param[in] phys_port_id Physical port id
 * @param[in] data	DLLP data
 * @return 0 on success, or a negative value on failure
 */
int switchtec_inject_err_dllp(struct switchtec_dev *dev, int phys_port_id,
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

/**
 * @brief Inject a DLLP CRC error into a physical port
 * @param[in] dev	Switchtec device handle
 * @param[in] phys_port_id Physical port id
 * @param[in] enable	Enable DLLP CRC error injection
 * @param[in] rate 	Rate of the error injection
 * @return 0 on success, or a negative value on failure
 */
int switchtec_inject_err_dllp_crc(struct switchtec_dev *dev,
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

static int switchtec_inject_err_tlp_lcrc_gen4(struct switchtec_dev *dev,
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
	printf("enable: %d\n", enable);

	return switchtec_cmd(dev, MRPC_MRPC_ERR_INJ, &cmd,
			     sizeof(cmd), &output, sizeof(output));
}

static int switchtec_inject_err_tlp_lcrc_gen5(struct switchtec_dev *dev,
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

/**
 * @brief Inject a TLP LCRC error into a physical port
 * @param[in] dev	Switchtec device handle
 * @param[in] phy_port Physical port id
 * @param[in] rate	Rate of the error injection
 * @return 0 on success, or a negative value on failure
 */
int switchtec_inject_err_tlp_lcrc(struct switchtec_dev *dev, int phy_port,
				  int enable, uint8_t rate)
{
	int ret;
	if (switchtec_is_gen4(dev)) {
		ret = switchtec_inject_err_tlp_lcrc_gen4(dev, phy_port, enable, rate);
		return ret;
	} else if (switchtec_is_gen5(dev)) {
		ret = switchtec_inject_err_tlp_lcrc_gen5(dev, phy_port, enable, rate);
		return ret;
	}
	fprintf(stderr, "The TLP LCRC is not supported for Gen3 switches.\n");
	return -1;
}

/**
 * @brief Inject a TLP Sequence Number error into a physical port
 * @param[in] dev	Switchtec device handle
 * @param[in] phys_port_id Physical port id
 * @return 0 on success, or a negative value on failure
 */
int switchtec_inject_err_tlp_seq_num(struct switchtec_dev *dev, int phys_port_id)
{
	uint32_t output;

	struct switchtec_lnkerr_tlp_seqn_in cmd = {
		.subcmd = MRPC_ERR_INJ_TLP_SEQ,
		.phys_port_id = phys_port_id,
	};

	return switchtec_cmd(dev, MRPC_MRPC_ERR_INJ, &cmd,
			     sizeof(cmd), &output, sizeof(output));
}

/**
 * @brief Inject an ACK to NACK error into a physical port
 * @param[in] dev	Switchtec device handle
 * @param[in] phys_port_id Physical port id
 * @param[in] seq_num	Sequence Number of ACK to be changed to a NACK (0-4095)
 * @param[in] count		Number of times to replace ACK with NACK (0-255)
 * @return 0 on success, or a negative value on failure
 */
int switchtec_inject_err_ack_nack(struct switchtec_dev *dev, int phys_port_id,
				  uint16_t seq_num, uint8_t count)
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

/**
 * @brief Inject Credit Timeout error into a physical port
 * @param[in] dev	Switchtec device handle
 * @param[in] phys_port_id Physical port id
 * @return 0 on success, or a negative value on failure
 */
int switchtec_inject_err_cto(struct switchtec_dev *dev, int phys_port_id)
{
	uint32_t output;

	struct switchtec_lnkerr_cto_in cmd = {
		.subcmd = MRPC_ERR_INJ_CTO,
		.phys_port_id = phys_port_id,
	};

	return switchtec_cmd(dev, MRPC_MRPC_ERR_INJ, &cmd,
			     sizeof(cmd), &output, sizeof(output));
}

static void osa_dword_data_helper(const uint32_t dwords[4], char *buffer) {
	char *ptr = buffer;
	for (int i = 3; i >= 0; --i) {
		int tmp = sprintf(ptr, "0x%08X", dwords[i]);
		ptr += tmp;
		*ptr = ' ';
		ptr++;
	}
	*ptr = '\0';
}

static void print_osa_capture_data(uint32_t* entry_dwords, uint8_t entries_read)
{
	int curr_idx = 0;
	uint32_t timestamp_upper = 0;
	uint32_t timestamp_lower = 0;
	uint64_t timestamp = 0;
	char data_string[45];
	uint32_t osa_dword_data[4];

	printf("IDX\tTIMESTAMP\tCNT\tRATE\tDRP\tTRIG\tDATA\n");
	for (int i = 0; i < entries_read; i++) {
		printf("%d\t", i);
		curr_idx = (i * 6);
		for (int j = 0; j < 6; j++) {
			if (j >= 0 && j <= 3) {
				osa_dword_data[j] = entry_dwords[curr_idx];
			}
			else if (j == 4) {
				osa_dword_data_helper(osa_dword_data, data_string);
				timestamp_lower = (entry_dwords[curr_idx] >> 22) & 0x3FF;
				timestamp_upper = (entry_dwords[curr_idx+1] & 0x7FFFFFF);
				printf("time_upper: %d\n", timestamp_upper);
				printf("time_lower: %d\n", timestamp_lower);
				timestamp = (uint64_t)timestamp_upper << 12 | timestamp_lower;
				printf("0x%08lx\t", timestamp);
				printf("%d\t", (entry_dwords[curr_idx] >> 3) & 0x7FFFF);
				printf("%d\t", entry_dwords[curr_idx] & 0x7);
				printf("%d\t", (entry_dwords[curr_idx+1] >> 28) & 0x1);
				printf("%d\t", (entry_dwords[curr_idx+1] >> 27) & 0x1);
				printf("%s\n", data_string);
			}
			curr_idx++;
		}
		printf("\n");
	}
}

int switchtec_osa_capture_data(struct switchtec_dev *dev, int stack_id,
			       int lane, int direction)
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

	printf("Current status of stack %d\n", stack_id);
	printf("state: %d\n", osa_status_query_out.state);
	printf("trigger_lane: %d\n", osa_status_query_out.trigger_lane);
	printf("trigger_dir: %d\n", osa_status_query_out.trigger_dir);
	printf("trigger_reason: %d\n", osa_status_query_out.trigger_reason);

	ret = switchtec_cmd(dev, MRPC_ORDERED_SET_ANALYZER, &osa_data_read_in,
			    sizeof(osa_data_read_in), &osa_data_entries_out,
			    sizeof(osa_data_entries_out));
	if (ret) {
		switchtec_perror("OSA data dump");
		return ret;
	}
	printf("OSA: Captured Data \n");

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

	while (osa_data_read_out.entries_remaining != 0) {
		osa_data_read_in.num_entries = osa_data_read_out.entries_remaining;
		osa_data_read_in.start_entry = osa_data_read_out.next_entry;

		ret = switchtec_cmd(dev, MRPC_ORDERED_SET_ANALYZER,
				    &osa_data_read_in, sizeof(osa_data_read_in),
				    &osa_data_read_out, sizeof(osa_data_read_out));

		if (ret) {
			return -1;
		}
		print_osa_capture_data(osa_data_read_out.entry_dwords,
				       osa_data_read_out.entries_read);
	}

	return ret;
}

int switchtec_osa_capture_control(struct switchtec_dev *dev, int stack_id,
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
	if (ret) {
		switchtec_perror("OSA capture control");
		return ret;
	}
	printf("OSA: Configuring capture control on stack %d\n", stack_id);
	return ret;
}

int switchtec_osa_config_misc(struct switchtec_dev *dev, int stack_id,
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
	if (ret) {
		switchtec_perror("OSA misc config");
		return ret;
	}
	printf("OSA: Enabled misc triggering config on stack %d\n", stack_id);
	return ret;
}

int switchtec_osa_config_pattern(struct switchtec_dev *dev, int stack_id,
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
	if (ret) {
		switchtec_perror("OSA pattern config");
		return ret;
	}
	printf("OSA: Enabled pattern triggering config on stack %d\n", stack_id);
	return ret;
}

int switchtec_osa_config_type(struct switchtec_dev *dev, int stack_id,
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

	printf("%d : %d : %d : %d : %d\n", stack_id, lane_mask, direction,
	       link_rate, os_types);
	ret = switchtec_cmd(dev, MRPC_ORDERED_SET_ANALYZER, &osa_type_config_in,
			    sizeof(osa_type_config_in), NULL, 0);
	if (ret) {
		switchtec_perror("OSA type config");
		return ret;
	}
	printf("OSA: Enabled type triggering config on stack %d\n", stack_id);
	return ret;
}

int switchtec_osa_dump_conf(struct switchtec_dev *dev, int stack_id)
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
	printf("Config dump \n");
	printf("---- OS Type ---------------\n");
	printf("lane mask: \t\t%d\n", osa_dmp_out.os_type_trig_lane_mask);
	printf("direciton: \t\t%d\n", osa_dmp_out.os_type_trig_dir);
	printf("link rate: \t\t%d\n", osa_dmp_out.os_type_trig_link_rate);
	printf("os types: \t\t%d\n", osa_dmp_out.os_type_trig_os_types);
	printf("---- OS Pattern ------------\n");
	printf("lane mask: \t\t%d\n", osa_dmp_out.os_pat_trig_lane_mask);
	printf("direciton: \t\t%d\n", osa_dmp_out.os_pat_trig_dir);
	printf("link rate: \t\t%d\n", osa_dmp_out.os_pat_trig_link_rate);
	printf("patttern: \t\t%d %d %d %d\n", osa_dmp_out.os_pat_trig_val_dw0,
	       osa_dmp_out.os_pat_trig_val_dw1, osa_dmp_out.os_pat_trig_val_dw2,
	       osa_dmp_out.os_pat_trig_val_dw3);
	printf("mask: \t\t\t%d %d %d %d\n", osa_dmp_out.os_pat_trig_mask_dw0,
	       osa_dmp_out.os_pat_trig_mask_dw1, osa_dmp_out.os_pat_trig_mask_dw2,
	       osa_dmp_out.os_pat_trig_mask_dw3);
	printf("---- Misc ------------------\n");
	printf("Misc trigger enabled: \t%d\n", osa_dmp_out.misc_trig_en);
	printf("---- Capture ---------------\n");
	printf("lane mask: \t\t%d\n", osa_dmp_out.capture_lane_mask);
	printf("direciton: \t\t%d\n", osa_dmp_out.capture_dir);
	printf("drop single os: \t%d\n", osa_dmp_out.capture_drop_os);
	printf("stop mode: \t\t%d\n", osa_dmp_out.capture_stop_mode);
	printf("snaphot mode: \t\t%d\n", osa_dmp_out.capture_snap_mode);
	printf("post-trigger entries: \t%d\n", osa_dmp_out.capture_post_trig_entries);
	printf("os types: \t\t%d\n", osa_dmp_out.capture_os_types);
	return ret;
}

int switchtec_osa(struct switchtec_dev *dev, int stack_id, int operation)
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

	char *valid_ops[6] = {"stop", "start", "trigger", "reset", "release",
			       "status"};
	char *states[5] = {"Deactivated (not armed)", "Started (armed), not triggered",
			    "Started (armted), triggered", "Stopped, not triggered",
			    "Stopped, triggered"};
	char *directions[2] = {"TX", "RX"};
	printf("Attempting %s operation...\n", valid_ops[operation]);
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
		printf("Status of stack %d\n", stack_id);
		printf("STATE: %s\n", states[osa_status_query_out.state]);
		printf("TRIGGER_LANE: %d\n", osa_status_query_out.trigger_lane);
		printf("TRIGGER_DIR: %s\n", directions[osa_status_query_out.trigger_dir]);
		printf("REASON_BITMASK: %d\n", osa_status_query_out.trigger_reason);
	}
	else {
		osa_op_in.sub_cmd = MRPC_OSA_ANALYZER_OP;
		osa_op_in.stack_id = stack_id;
		osa_op_in.operation = operation;

		ret = switchtec_cmd(dev, MRPC_ORDERED_SET_ANALYZER, &osa_op_in,
				    sizeof(osa_op_in), NULL, 0);
	}
	if (ret) {
		switchtec_perror("OSA operation");
		return ret;
	}
	printf("Successful %s operation!\n", valid_ops[operation]);

	return ret;
}

/**@}*/
