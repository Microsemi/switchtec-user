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

#ifndef LIBSWITCHTEC_GASOPS_H
#define LIBSWITCHTEC_GASOPS_H

#include "switchtec/switchtec.h"

int gasop_access_check(struct switchtec_dev *dev);
void gasop_set_partition_info(struct switchtec_dev *dev);
int gasop_cmd(struct switchtec_dev *dev, uint32_t cmd,
	      const void *payload, size_t payload_len, void *resp,
	      size_t resp_len);
int gasop_get_fw_version(struct switchtec_dev *dev, char *buf,
			 size_t buflen);
int gasop_pff_to_port(struct switchtec_dev *dev, int pff,
		      int *partition, int *port);
int gasop_port_to_pff(struct switchtec_dev *dev, int partition,
		      int port, int *pff);
int gasop_flash_part(struct switchtec_dev *dev,
		     struct switchtec_fw_image_info *info,
		     enum switchtec_fw_image_type part);
int gasop_event_summary(struct switchtec_dev *dev,
			struct switchtec_event_summary *sum);
int gasop_event_ctl(struct switchtec_dev *dev, enum switchtec_event_id e,
		    int index, int flags, uint32_t data[5]);
int gasop_event_wait_for(struct switchtec_dev *dev,
			 enum switchtec_event_id e, int index,
			 struct switchtec_event_summary *res,
			 int timeout_ms);

#endif
