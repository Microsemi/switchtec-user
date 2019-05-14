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

/********** TOPO INFO *********/

#define SWITCHTEC_TOPO_INFO_DUMP_DATA_LENGTH_MAX 1000

enum switchtec_fab_topo_info_dump_status {
	SWITCHTEC_FAB_TOPO_INFO_DUMP_NOT_START = 1,
	SWITCHTEC_FAB_TOPO_INFO_DUMP_WAIT = 2,
	SWITCHTEC_FAB_TOPO_INFO_DUMP_READY = 3,
	SWITCHTEC_FAB_TOPO_INFO_DUMP_FAILED = 4,
	SWITCHTEC_FAB_TOPO_INFO_DUMP_WRONG_SUB_CMD = 5,
};

enum switchtec_fab_topo_info_dump_link_rate {
	SWITCHTEC_FAB_PORT_LINK_RATE_NONE,
	SWITCHTEC_FAB_PORT_LINK_RATE_GEN1,
	SWITCHTEC_FAB_PORT_LINK_RATE_GEN2,
	SWITCHTEC_FAB_PORT_LINK_RATE_GEN3,
	SWITCHTEC_FAB_PORT_LINK_RATE_GEN4,
	SWITCHTEC_FAB_PORT_LINK_RATE_INVALID,
};

enum switchtec_fab_topo_info_dump_LTSSM_STATE {
	SWITCHTEC_FAB_PORT_LTSSM_MAJOR_STATE_POLLING = 1,
	SWITCHTEC_FAB_PORT_LTSSM_MAJOR_STATE_CONFIG,
	SWITCHTEC_FAB_PORT_LTSSM_MAJOR_STATE_L0,
	SWITCHTEC_FAB_PORT_LTSSM_MAJOR_STATE_RECOVERY,
	SWITCHTEC_FAB_PORT_LTSSM_MAJOR_STATE_DISABLED,
	SWITCHTEC_FAB_PORT_LTSSM_MAJOR_STATE_LOOPBK,
	SWITCHTEC_FAB_PORT_LTSSM_MAJOR_STATE_HOTRST,
	SWITCHTEC_FAB_PORT_LTSSM_MAJOR_STATE_L0S,
	SWITCHTEC_FAB_PORT_LTSSM_MAJOR_STATE_L1,
	SWITCHTEC_FAB_PORT_LTSSM_MAJOR_STATE_L2,
	SWITCHTEC_FAB_PORT_LTSSM_MAJOR_STATE_INVALID,
};

#define SWITCHTEC_FAB_PORT_LTSSM_MINOR_STATE_MAX 12

struct switchtec_fab_port_info {
	uint8_t phys_port_id;			//!< Physical port id
	uint8_t port_type;			//!< Port type
	uint8_t port_clock_channel;		//!< Clock channel
	uint8_t port_connector_id;		//!< Connector index

	struct gpio_idx_val {
		uint16_t gpio_idx;		//!< GPIO index
		uint8_t value;			//!< GPIO value
		uint8_t rsvd;
	} conn_sig_pwrctrl;			//!< Power controller GPIO pin

	struct gpio_idx_val conn_sig_dsp_perst;	//!< DSP PERST# GPIO pin
	struct gpio_idx_val conn_sig_usp_perst;	//!< USP PERST# GPIO pin
	struct gpio_idx_val conn_sig_presence;	//!< Presence GPIO pin
	struct gpio_idx_val conn_sig_8639;	//!< SFF-8639 IFDET GPIO pin

	/* link status */
	uint8_t port_cfg_width;			//!< link width in config file
	uint8_t port_neg_width;			//!< link width negotiated
	uint8_t port_cfg_rate;			//!< link rate in config file
	uint8_t port_neg_rate;			//!< link rate negotiated
	uint8_t port_major_ltssm;		//!< Major LTSSM state
	uint8_t port_minor_ltssm;		//!< Minor LTSSM state
	uint8_t rsvd[2];
};

struct switchtec_fab_topo_info {
	uint8_t sw_idx;			//!< Switch index
	uint8_t rsvd[3];
	uint32_t stack_bif[6]; 		//!< port bifurcation
	uint8_t route_port[16];		//!< Route port
	uint32_t rsvd1;
	uint64_t port_bitmap;		//!< Physical port enabled bitmap

	/**
	 * @brief Port info list.
	 *
	 * The total port count is decided by enabled port number, which
	 * is reflected by port_bitmap. Only enabled physical port will be
	 * reported in the port_info_list[].
	 */
	struct switchtec_fab_port_info port_info_list[SWITCHTEC_MAX_PORTS];
};

int switchtec_topo_info_dump(struct switchtec_dev *dev,
			     struct switchtec_fab_topo_info *topo_info);

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

/********** DEVICE MANAGE *********/

#define SWITCHTEC_DEVICE_MANAGE_MAX_RESP 1016

struct switchtec_device_manage_req_hdr {
        uint16_t pdfid;
        uint16_t expected_rsp_len;
};

struct switchtec_device_manage_rsp_hdr {
        uint16_t rsp_len;
        uint16_t rsvd;
};

struct switchtec_device_manage_req {
        struct switchtec_device_manage_req_hdr hdr;
        uint8_t cmd_data[MRPC_MAX_DATA_LEN -
                        sizeof(struct switchtec_device_manage_req_hdr)];
};

struct switchtec_device_manage_rsp {
        struct switchtec_device_manage_rsp_hdr hdr;
        uint8_t rsp_data[SWITCHTEC_DEVICE_MANAGE_MAX_RESP];
};

int switchtec_device_manage(struct switchtec_dev *dev,
                            struct switchtec_device_manage_req *req,
                            struct switchtec_device_manage_rsp *rsp);

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
