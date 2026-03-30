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

#ifndef LIBSWITCHTEC_DIAG_GEN5_H
#define LIBSWITCHTEC_DIAG_GEN5_H

#include "../switchtec_priv.h"

int switchtec_diag_ltssm_log_gen5(struct switchtec_dev *dev,
				 int port, int *log_count, void *log_data);

int switchtec_diag_port_eq_tx_coeff_gen5(struct switchtec_dev *dev,
					 int port_id, int prev_speed,
					 int end, int link, void *res);

int switchtec_diag_port_eq_tx_table_gen5(struct switchtec_dev *dev,
					 int port_id, int prev_speed,
					 int link, void *res);

int switchtec_diag_port_eq_tx_fslf_gen5(struct switchtec_dev *dev,
					int port_id, int prev_speed,
					int lane_id, int end, int link,
					void *res);

int switchtec_diag_loopback_set_gen5(struct switchtec_dev *dev,
				     int port_id, int enable,
				     int enable_parallel,
				     int enable_external,
				     int enable_ltssm, int enable_pipe,
				     int ltssm_speed);

int switchtec_diag_loopback_get_gen5(struct switchtec_dev *dev,
				     int port_id, int *enabled,
				     int *ltssm_speed);

int switchtec_diag_eye_start_gen5(struct switchtec_dev *dev, int lane_mask[4],
				  void *x_range, void *y_range,
				  int step_interval, int capture_depth,
				  int sar_sel, int intleav_sel, int hstep,
				  int data_mode, int eye_mode,
				  uint64_t refclk, int vstep);

int switchtec_diag_eye_read_gen5(struct switchtec_dev *dev, int lane_id,
				 int bin, int *num_phases, double *ber_data);

int switchtec_diag_pattern_gen_set_gen5(struct switchtec_dev *dev,
					int port_id, int type,
					int link_speed);

int switchtec_inject_err_tlp_lcrc_gen5(struct switchtec_dev *dev,
				       int phys_port_id, int enable,
				       uint8_t rate);

int switchtec_inject_err_cto_gen5(struct switchtec_dev *dev,
				  int phys_port_id);

int switchtec_osa_capture_data_gen5(struct switchtec_dev *dev, int stack_id, 
   					int lane, int direction, void *data);

int switchtec_osa_capture_control_gen5(struct switchtec_dev *dev, int stack_id,
					int lane_mask, int direction,
					int drop_single_os, int stop_mode,
					int snapshot_mode, int post_trigger,
					int os_types);

int switchtec_osa_config_misc_gen5(struct switchtec_dev *dev, int stack_id,
				int trigger_en);

int switchtec_osa_config_pattern_gen5(struct switchtec_dev *dev, int stack_id,
				 int direction, int lane_mask, int link_rate,
				 uint32_t *value_data, uint32_t *mask_data);

int switchtec_osa_config_type_gen5(struct switchtec_dev *dev, int stack_id,
				int direction, int lane_mask, int link_rate, int os_types);

int switchtec_osa_dump_conf_gen5(struct switchtec_dev *dev, int stack_id, void *config);

int switchtec_osa_gen5(struct switchtec_dev *dev, int stack_id, int operation, void *status);

#endif