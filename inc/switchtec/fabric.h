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

/********** PORT CONTROL *********/

enum switchtec_fabric_port_control_type {
	SWITCTEC_PORT_CONTROL_DISABLE,
	SWITCTEC_PORT_CONTROL_ENABLE,
	SWITCTEC_PORT_CONTROL_LINK_RETRAIN,
	SWITCTEC_PORT_CONTROL_LINK_HOT_RESET,
};

enum switchtec_fabric_hot_reset_flag {
	SWITCTEC_PORT_CONTROL_HOT_RESET_STATUS_CLEAR,
	SWITCTEC_PORT_CONTROL_HOT_RESET_STATUS_SET,
};

int switchtec_port_control(struct switchtec_dev *dev, uint8_t control_type,
			   uint8_t phys_port_id, uint8_t hot_reset_flag);

/********** PORT MANAGEMENT *********/

/**
 * @brief The port type
 */
enum switchtec_fab_port_type {
	SWITCHTEC_FAB_PORT_TYPE_UNUSED,
	SWITCHTEC_FAB_PORT_TYPE_FABRIC,
	SWITCHTEC_FAB_PORT_TYPE_FABRIC_EP,
	SWITCHTEC_FAB_PORT_TYPE_FABRIC_HOST,
	SWITCHTEC_FAB_PORT_TYPE_INVALID,
};

/**
 * @brief The port clock mode
 */
enum switchtec_fab_port_clock_mode {
	SWITCHTEC_FAB_PORT_CLOCK_COMMON_WO_SSC,
	SWITCHTEC_FAB_PORT_CLOCK_NON_COMMON_WO_SSC,
	SWITCHTEC_FAB_PORT_CLOCK_COMMON_W_SSC,
	SWITCHTEC_FAB_PORT_CLOCK_NON_COMMON_W_SSC,
	SWITCHTEC_FAB_PORT_CLOCK_INVALID,
};

/**
 * @brief The port config
 */
struct switchtec_fab_port_config {
	uint8_t port_type;	//!< Port type
	uint8_t clock_source; 	//!< CSU channel index for port clock source(0-2)
	uint8_t clock_mode;	//!< Port clock mode option
	uint8_t hvd_inst;	//!< HVM domain instance index for USP
};

int switchtec_fab_port_config_get(struct switchtec_dev *dev,
				  uint8_t phys_port_id,
				  struct switchtec_fab_port_config *info);
int switchtec_fab_port_config_set(struct switchtec_dev *dev,
				  uint8_t phys_port_id,
				  struct switchtec_fab_port_config *info);

#ifdef __cplusplus
}
#endif

#endif
