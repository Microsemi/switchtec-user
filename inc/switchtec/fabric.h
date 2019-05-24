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

/********** GFMS DUMP *********/

#define SWITCHTEC_FABRIC_MAX_SWITCH_NUM 16
#define SWITCHTEC_FABRIC_MAX_HOST_PER_SWITCH 16
#define SWITCHTEC_FABRIC_MAX_DEV_PER_SWITCH 32
#define SWITCHTEC_FABRIC_MAX_FUNC_PER_DEV 32
#define SWITCHTEC_FABRIC_MAX_BAR_NUM 6
#define SWITCHTEC_FABRIC_MAX_DSP_PER_HOST 32
#define SWITCHTEC_FABRIC_MAX_BINDING_NUM 512

enum switchtec_gfms_db_ep_attached_device_type {
	SWITCHTEC_GFMS_DB_TYPE_EP,
	SWITCHTEC_GFMS_DB_TYPE_SWITCH,
	SWITCHTEC_GFMS_DB_TYPE_NON,
};

enum switchtec_gfms_db_reach_type {
	SWITCHTEC_GFMS_DB_REACH_UC,
	SWITCHTEC_GFMS_DB_REACH_BC,
	SWITCHTEC_GFMS_DB_REACH_UR,
};

enum switchtec_gfms_db_hvd_usp_link_state {
	SWITCHTEC_GFMS_DB_HVD_USP_LINK_DOWN,
	SWITCHTEC_GFMS_DB_HVD_USP_LINK_UP,
};

enum switchtec_gfms_db_vep_type {
	SWITCHTEC_GFMS_DB_VEP_TYPE_MGMT = 8,
};

struct switchtec_gfms_db_dump_section_hdr {
	uint8_t section_class;
	uint8_t pax_idx;
	uint16_t swfid;
	uint32_t resp_size_dw;
	uint32_t rsvd;
};

struct switchtec_gfms_db_fabric_general_body {
	uint32_t rsvd[3];
	struct pax_idx_info {
		uint8_t pax_idx;
		uint8_t reachable_type;
		uint16_t rsvd;
	} pax_idx[16];
};

struct switchtec_gfms_db_fabric_general {
	struct switchtec_gfms_db_dump_section_hdr hdr;
	struct switchtec_gfms_db_fabric_general_body body;
};

struct switchtec_gfms_db_pax_general_body {
	uint8_t phy_port_count;
	uint8_t hvd_count;
	uint16_t ep_count;
	uint16_t ep_function_count;
	uint16_t rsvd0;
	uint32_t rsvd1[3];
	uint16_t fid_start;
	uint16_t fid_end;
	uint16_t hfid_start;
	uint16_t hfid_end;
	uint16_t vdfid_start;
	uint16_t vdfid_end;
	uint16_t pdfid_start;
	uint16_t pdfid_end;
	uint32_t rc_port_map_low;
	uint32_t rc_port_map_high;
	uint32_t ep_port_map_low;
	uint32_t ep_port_map_high;
	uint32_t fab_port_map_low;
	uint32_t fab_port_map_high;
	uint32_t free_port_map_low;
	uint32_t free_port_map_high;
};

struct switchtec_gfms_db_pax_general {
	struct switchtec_gfms_db_dump_section_hdr hdr;
	struct switchtec_gfms_db_pax_general_body body;
};

struct switchtec_gfms_db_hvd_port_bound {
	uint8_t log_pid;
	uint8_t bound;
	uint16_t bound_pdfid;
};

struct switchtec_gfms_db_hvd_body {
	uint8_t hvd_inst_id;
	uint8_t phy_pid;
	uint16_t hfid;
	uint16_t logical_port_count;
	uint16_t rsvd;
	struct switchtec_gfms_db_hvd_port_bound bound[(MRPC_MAX_DATA_LEN -
			sizeof(struct switchtec_gfms_db_dump_section_hdr) - 8) /
			sizeof(struct switchtec_gfms_db_hvd_port_bound)];
};

struct switchtec_gfms_db_hvd {
	struct switchtec_gfms_db_dump_section_hdr hdr;
	struct switchtec_gfms_db_hvd_body body;
};

struct switchtec_gfms_db_hvd_all {
	int hvd_count;
	struct switchtec_gfms_db_dump_section_hdr hdr;
	struct switchtec_gfms_db_hvd_body bodies[
		SWITCHTEC_FABRIC_MAX_HOST_PER_SWITCH];
};

struct switchtec_gfms_db_hvd_vep {
	uint8_t type;
	uint8_t rsvd;
	uint16_t bdf;
};

struct switchtec_gfms_db_hvd_log_port {
	uint8_t log_pid;
	uint8_t bound;
	uint16_t dsp_bdf;
	uint16_t bound_pdfid;
	uint16_t bound_hvd_bdf;
};

struct switchtec_gfms_db_hvd_detail_body {
	uint8_t hvd_inst_id;
	uint8_t phy_pid;
	uint16_t hfid;
	uint8_t vep_count;
	uint8_t usp_status;
	uint8_t rsvd[2];
	struct switchtec_gfms_db_hvd_vep vep_region[7];
	uint16_t log_dsp_count;
	uint16_t usp_bdf;
	struct switchtec_gfms_db_hvd_log_port log_port_region[48];
	uint32_t log_port_p2p_enable_bitmap_low;
	uint32_t log_port_p2p_enable_bitmap_high;
	uint8_t log_port_count;
	struct switchtec_cfg_act_bitmap {
		uint32_t config_bitmap_low;
		uint32_t config_bitmap_high;
		uint32_t active_bitmap_low;
		uint32_t active_bitmap_high;
	} log_port_p2p_bitmap[64];
};

struct switchtec_gfms_db_hvd_detail {
	struct switchtec_gfms_db_dump_section_hdr hdr;
	struct switchtec_gfms_db_hvd_detail_body body;
};

struct switchtec_gfms_db_fab_port_body {
	uint8_t phy_pid;
	uint8_t rsvd0[3];
	uint8_t attached_phy_pid;
	uint8_t attached_sw_idx;
	uint16_t attached_swfid;
	uint32_t attached_fw_version;
	uint32_t rsvd1[2];
};

struct switchtec_gfms_db_fab_port {
	struct switchtec_gfms_db_dump_section_hdr hdr;
	struct switchtec_gfms_db_fab_port_body body;
};

struct switchtec_gfms_db_ep_port_attached_device_function {
	uint16_t func_id;
	uint16_t pdfid;
	uint8_t sriov_cap_pf;
	uint8_t vf_num;
	uint16_t rsvd;
	uint8_t bound;
	uint8_t bound_pax_id;
	uint8_t bound_hvd_phy_pid;
	uint8_t bound_hvd_log_pid;
	uint16_t vid;
	uint16_t did;
	uint16_t sub_sys_vid;
	uint16_t sub_sys_did;
	uint32_t device_class: 24;
	uint32_t bar_number: 8;
	struct bar {
		uint8_t type;
		uint8_t size;
	} bars[6];
};

struct switchtec_gfms_db_ep_port_attached_ds_function {
	uint16_t func_id;
	uint16_t enumid;
	uint32_t rsvd0[2];
	uint16_t vid;
	uint16_t did;
	uint16_t rsvd1[2];
	uint32_t device_class: 24;
	uint32_t bar_num: 8;
	struct {
		uint8_t type;
		uint8_t size;
	} bar[6];
};

struct switchtec_gfms_db_ep_port_attachement_hdr {
	uint16_t function_number;
	uint16_t attached_dsp_enumid;
	uint32_t size_dw: 24;
	uint32_t rsvd: 8;
};

struct switchtec_gfms_db_ep_port_ep {
	struct switchtec_gfms_db_ep_port_attachement_hdr ep_hdr;
	struct switchtec_gfms_db_ep_port_attached_device_function
		functions[SWITCHTEC_FABRIC_MAX_FUNC_PER_DEV];
};

struct switchtec_gfms_db_ep_port_switch {
	struct switchtec_gfms_db_ep_port_attachement_hdr sw_hdr;
	struct attached_switch {
		struct switchtec_gfms_db_ep_port_attached_ds_function
			internal_functions[
			SWITCHTEC_FABRIC_MAX_HOST_PER_SWITCH +
			SWITCHTEC_FABRIC_MAX_DEV_PER_SWITCH];
	} ds_switch;

	struct switchtec_gfms_db_ep_port_ep
		switch_eps[SWITCHTEC_FABRIC_MAX_DEV_PER_SWITCH];
};

struct switchtec_gfms_db_ep_port_hdr {
	uint8_t type;
	uint8_t phy_pid;
	uint16_t ep_count;
	uint32_t size_dw: 24;
	uint32_t rsvd: 8;
};

struct switchtec_gfms_db_ep_port {
	struct switchtec_gfms_db_ep_port_hdr port_hdr;
	union {
		struct switchtec_gfms_db_ep_port_switch ep_switch;
		struct switchtec_gfms_db_ep_port_ep ep_ep;
	};
};

struct switchtec_gfms_db_ep_port_section {
	struct switchtec_gfms_db_dump_section_hdr hdr;
	struct switchtec_gfms_db_ep_port ep_port;
};

struct switchtec_gfms_db_ep_port_all_section {
	int ep_port_count;
	struct switchtec_gfms_db_dump_section_hdr hdr;
	struct switchtec_gfms_db_ep_port ep_ports[
		SWITCHTEC_FABRIC_MAX_DEV_PER_SWITCH];
};

struct switchtec_gfms_db_pax_all {
	struct switchtec_gfms_db_fabric_general fabric_general;
	struct switchtec_gfms_db_hvd_all hvd_all;
	struct switchtec_gfms_db_pax_general pax_general;
	struct switchtec_gfms_db_ep_port_all_section ep_port_all;
};

int switchtec_fab_gfms_db_dump_fabric_general(
		struct switchtec_dev *dev,
		struct switchtec_gfms_db_fabric_general *fabric_general);
int switchtec_fab_gfms_db_dump_pax_all(
		struct switchtec_dev *dev,
		struct switchtec_gfms_db_pax_all *pax_all);
int switchtec_fab_gfms_db_dump_pax_general(
		struct switchtec_dev *dev,
		struct switchtec_gfms_db_pax_general *pax_general);
int switchtec_fab_gfms_db_dump_hvd(struct switchtec_dev *dev,
				   uint8_t hvd_idx,
				   struct switchtec_gfms_db_hvd *hvd);
int switchtec_fab_gfms_db_dump_fab_port(
		struct switchtec_dev *dev,
		uint8_t phy_pid,
		struct switchtec_gfms_db_fab_port *fab_port);
int switchtec_fab_gfms_db_dump_ep_port(
		struct switchtec_dev *dev,
		uint8_t phy_pid,
		struct switchtec_gfms_db_ep_port_section *ep_port_section);
int switchtec_fab_gfms_db_dump_hvd_detail(
		struct switchtec_dev *dev,
		uint8_t hvd_idx,
		struct switchtec_gfms_db_hvd_detail *hvd_detail);
#ifdef __cplusplus
}
#endif

#endif
