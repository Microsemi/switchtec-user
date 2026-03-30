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

#ifndef LIBSWITCHTEC_DIAG_GEN4_H
#define LIBSWITCHTEC_DIAG_GEN4_H

#include "../switchtec_priv.h"

int switchtec_diag_ltssm_log_gen4(struct switchtec_dev *dev,
				 int port, int *log_count, void *log_data);

int switchtec_diag_port_eq_tx_coeff_gen4(struct switchtec_dev *dev,
					 int port_id, int prev_speed,
					 int end, int link, void *res);

int switchtec_diag_port_eq_tx_table_gen4(struct switchtec_dev *dev,
					 int port_id, int prev_speed,
					 int link, void *res);

int switchtec_diag_port_eq_tx_fslf_gen4(struct switchtec_dev *dev,
					int port_id, int prev_speed,
					int lane_id, int end, int link,
					void *res);

int switchtec_diag_loopback_set_gen4(struct switchtec_dev *dev,
				     int port_id, int enable,
				     int enable_parallel,
				     int enable_external,
				     int enable_ltssm, int enable_pipe,
				     int ltssm_speed);

int switchtec_diag_loopback_get_gen4(struct switchtec_dev *dev,
				     int port_id, int *enabled,
				     int *ltssm_speed);

int switchtec_diag_eye_cancel_gen4(struct switchtec_dev *dev);

int switchtec_diag_eye_set_mode_gen4(struct switchtec_dev *dev, int mode);

int switchtec_diag_eye_fetch_gen4(struct switchtec_dev *dev, double *pixels,
				  size_t pixel_cnt, int *lane_id);

int switchtec_diag_eye_start_gen4(struct switchtec_dev *dev, int lane_mask[4],
				  void *x_range, void *y_range,
				  int step_interval, int capture_depth,
				  int sar_sel, int intleav_sel, int hstep,
				  int data_mode, int eye_mode,
				  uint64_t refclk, int vstep);

int switchtec_diag_cross_hair_enable_gen4(struct switchtec_dev *dev,
					  int lane_id);

int switchtec_diag_cross_hair_disable_gen4(struct switchtec_dev *dev);

int switchtec_diag_cross_hair_get_gen4(struct switchtec_dev *dev,
				       int start_lane_id, int num_lanes,
				       void *res);

int switchtec_diag_pattern_gen_set_gen4(struct switchtec_dev *dev,
					int port_id, int type,
					int link_speed);

int switchtec_diag_pattern_gen_get_gen4(struct switchtec_dev *dev,
					int port_id, int *type);

int switchtec_diag_pattern_mon_set_gen4(struct switchtec_dev *dev,
					int port_id, int type);

int switchtec_diag_pattern_mon_get_gen4(struct switchtec_dev *dev,
					int port_id, int lane_id,
					int *type,
					unsigned long long *err_cnt);

int switchtec_diag_pattern_inject_gen4(struct switchtec_dev *dev,
				       int port_id, int err_cnt);

int switchtec_diag_rcvr_obj_gen4(struct switchtec_dev *dev, int port_id,
				 int lane_id, int link, void *res);

int switchtec_inject_err_tlp_lcrc_gen4(struct switchtec_dev *dev,
				       int phys_port_id, int enable,
				       uint8_t rate);

int switchtec_inject_err_tlp_seqnum_gen4(struct switchtec_dev *dev,
					 int phys_port_id);

int switchtec_inject_err_dllp_gen4(struct switchtec_dev *dev,
				   int phys_port_id, int data);

int switchtec_inject_err_dllp_crc_gen4(struct switchtec_dev *dev,
				       int phys_port_id, int enable,
				       uint16_t rate);

int switchtec_inject_err_ack_nack_gen4(struct switchtec_dev *dev,
				       int phys_port_id, uint16_t seq_num,
				       uint8_t count);

#endif