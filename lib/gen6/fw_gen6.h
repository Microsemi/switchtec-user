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

#ifndef LIBSWITCHTEC_FW_GEN6_H
#define LIBSWITCHTEC_FW_GEN6_H

#include "../switchtec_priv.h"

int switchtec_fw_img_write_hdr_gen6(int fd, struct switchtec_fw_image_info *info);
int switchtec_fw_part_info_gen6(struct switchtec_dev *dev, int nr_info,
				struct switchtec_fw_image_info *info);
struct switchtec_fw_part_summary *switchtec_fw_part_summary_gen6(struct switchtec_dev *dev);
struct switchtec_fw_image_info *switchtec_fw_part_data_bl2_gen6(struct switchtec_dev *dev);
int switchtec_fw_set_redundant_flag_gen6(struct switchtec_dev *dev, int keyman,
					 int riot, int bl2, int cfg, int fw,
					 int set);
int switchtec_fw_img_get_gen6(struct switchtec_dev *dev, int fd,
			      enum switchtec_fw_type_gen6 fw_type, int fw_slot,
			      void (*progress_callback)(int cur, int tot));
int switchtec_fw_toggle_active_partition_gen6(struct switchtec_dev *dev,
					      int toggle_bl2, int toggle_key,
					      int toggle_fw, int toggle_cfg,
					      int toggle_riotcore);

#endif