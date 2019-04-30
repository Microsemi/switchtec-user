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

#ifndef LIBSWITCHTEC_FABRIC_H
#define LIBSWITCHTEC_FABRIC_H

#include "mrpc.h"
#include "portable.h"
#include "registers.h"

#include <switchtec/switchtec.h>

#include <linux/limits.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/********** GFMS BIND *********/

struct switchtec_gfms_bind_req {
	uint8_t host_sw_idx;
	uint8_t host_phys_port_id;
	uint8_t host_log_port_id;
	uint16_t pdfid;
};

struct switchtec_gfms_unbind_req {
	uint8_t host_sw_idx;
	uint8_t host_phys_port_id;
	uint8_t host_log_port_id;
	uint16_t pdfid;
	uint8_t option;
};

int switchtec_gfms_bind(struct switchtec_dev *dev,
		        struct switchtec_gfms_bind_req *req);
int switchtec_gfms_unbind(struct switchtec_dev *dev,
			  struct switchtec_gfms_unbind_req *req);
#ifdef __cplusplus
}
#endif

#endif
