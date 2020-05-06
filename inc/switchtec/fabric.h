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

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/********** TOPO INFO *********/

/**
 * @brief Represents each port in the in topology info.
 */
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

	uint8_t port_cfg_width;			//!< link width in config file
	uint8_t port_neg_width;			//!< link width negotiated
	uint8_t port_cfg_rate;			//!< link rate in config file
	uint8_t port_neg_rate;			//!< link rate negotiated
	uint8_t port_major_ltssm;		//!< Major LTSSM state
	uint8_t port_minor_ltssm;		//!< Minor LTSSM state
	uint8_t rsvd[2];
};

/**
 * @brief Represents the topology info.
 */
struct switchtec_fab_topo_info {
	uint8_t sw_idx;			//!< Switch index
	uint8_t rsvd[3];
	uint32_t stack_bif[7]; 		//!< Port bifurcation
	uint8_t route_port[16];		//!< Route port
	uint64_t port_bitmap;		//!< Enabled physical port bitmap

	/**
	 * @brief Port info list.
	 *
	 * The total port count is determined by the enabled port number, which
	 * is reported in the port_bitmap. Only enabled physical port will be
	 * populated in the port_info_list[].
	 */
	struct switchtec_fab_port_info port_info_list[SWITCHTEC_MAX_PORTS];
};

int switchtec_topo_info_dump(struct switchtec_dev *dev,
			     struct switchtec_fab_topo_info *topo_info);

/********** GFMS BIND *********/

#define SWITCHTEC_FABRIC_MULTI_FUNC_NUM 8

struct switchtec_gfms_bind_req {
	uint8_t host_sw_idx;
	uint8_t host_phys_port_id;
	uint8_t host_log_port_id;
	int ep_number;
	uint16_t ep_pdfid[SWITCHTEC_FABRIC_MULTI_FUNC_NUM];
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
 * @brief The port clock sris
 */
enum switchtec_fab_port_clock_sris {
	SWITCHTEC_FAB_PORT_CLOCK_SRIS_DISABLE,
	SWITCHTEC_FAB_PORT_CLOCK_SRIS_ENABLE,
	SWITCHTEC_FAB_PORT_CLOCK_SRIS_INVALID,
};

/**
 * @brief The port config
 */
struct switchtec_fab_port_config {
	uint8_t port_type;	//!< Port type
	uint8_t clock_source; 	//!< CSU channel index for port clock source(0-2)
	uint8_t clock_sris;	//!< Port clock sris, enable/disable
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
	SWITCHTEC_GFMS_DB_VEP_TYPE_MGMT = 6,
};

enum switchtec_gfms_db_ep_port_bar_type {
	SWITCHTEC_GFMS_DB_EP_BAR_TYPE_MEM_32_PREFETCH = 0x8,
	SWITCHTEC_GFMS_DB_EP_BAR_TYPE_MEM_64_PREFETCH = 0xc,
	SWITCHTEC_GFMS_DB_EP_BAR_TYPE_MEM_32_NON_PREFETCH = 0x0,
	SWITCHTEC_GFMS_DB_EP_BAR_TYPE_MEM_64_NON_PREFETCH = 0x4,
	SWITCHTEC_GFMS_DB_EP_BAR_TYPE_IO_32_PREFETCH = 0x9,
	SWITCHTEC_GFMS_DB_EP_BAR_TYPE_IO_64_PREFETCH = 0xd,
	SWITCHTEC_GFMS_DB_EP_BAR_TYPE_IO_32_NON_PREFETCH = 0x1,
	SWITCHTEC_GFMS_DB_EP_BAR_TYPE_IO_64_NON_PREFETCH = 0x5,
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

struct switchtec_gfms_db_hvd_body {
	uint8_t hvd_inst_id;
	uint8_t phy_pid;
	uint16_t hfid;
	uint16_t logical_port_count;
	uint16_t rsvd;
	struct port_bound {
		uint8_t log_pid;
		uint8_t bound;
		uint16_t bound_pdfid;
	} bound[SWITCHTEC_FABRIC_MULTI_FUNC_NUM *
		SWITCHTEC_FABRIC_MAX_DSP_PER_HOST];
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
	struct {
		uint8_t type;
		uint8_t rsvd;
		uint16_t bdf;
	} vep_region[7];

	uint16_t log_dsp_count;
	uint16_t usp_bdf;
	struct switchtec_gfms_db_hvd_log_port
		log_port_region[SWITCHTEC_FABRIC_MULTI_FUNC_NUM *
		SWITCHTEC_FABRIC_MAX_DSP_PER_HOST];

	uint32_t log_port_p2p_enable_bitmap_low;
	uint32_t log_port_p2p_enable_bitmap_high;
	uint8_t log_port_count;
	struct switchtec_cfg_act_bitmap {
		uint32_t config_bitmap_low;
		uint32_t config_bitmap_high;
		uint32_t active_bitmap_low;
		uint32_t active_bitmap_high;
	} log_port_p2p_bitmap[SWITCHTEC_FABRIC_MAX_DSP_PER_HOST];
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

static inline int switchtec_ep_port_bar_type_str(uint8_t bar_type,
						 char *bar_type_str,
						 size_t len)
{
	switch (bar_type) {
	case SWITCHTEC_GFMS_DB_EP_BAR_TYPE_MEM_32_PREFETCH:
		strncpy(bar_type_str, "Memory, Prefetchable, 32-bit", len);
		break;
	case SWITCHTEC_GFMS_DB_EP_BAR_TYPE_MEM_64_PREFETCH:
		strncpy(bar_type_str, "Memory, Prefetchable, 64-bit", len);
		break;
	case SWITCHTEC_GFMS_DB_EP_BAR_TYPE_MEM_32_NON_PREFETCH:
		strncpy(bar_type_str, "Memory, Non-prefetchable, 32-bit", len);
		break;
	case SWITCHTEC_GFMS_DB_EP_BAR_TYPE_MEM_64_NON_PREFETCH:
		strncpy(bar_type_str, "Memory, Non-prefetchable, 64-bit", len);
		break;
	case SWITCHTEC_GFMS_DB_EP_BAR_TYPE_IO_32_PREFETCH:
		strncpy(bar_type_str, "IO, Prefetchable, 32-bit", len);
		break;
	case SWITCHTEC_GFMS_DB_EP_BAR_TYPE_IO_64_PREFETCH:
		strncpy(bar_type_str, "IO, Prefetchable, 64-bit", len);
		break;
	case SWITCHTEC_GFMS_DB_EP_BAR_TYPE_IO_32_NON_PREFETCH:
		strncpy(bar_type_str, "IO, Non-prefetchable, 32-bit", len);
		break;
	case SWITCHTEC_GFMS_DB_EP_BAR_TYPE_IO_64_NON_PREFETCH:
		strncpy(bar_type_str, "IO, Non-prefetchable, 64-bit", len);
		break;
	default:
		strncpy(bar_type_str, "Unknown", len);
	}

	bar_type_str[len - 1] = '\0';
	return 0;
}

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

/********** GFMS Event *********/

/**
 * @brief The GFMS event types
 */
enum switchtec_gfms_event_type {
	SWITCHTEC_GFMS_EVENT_HOST_LINK_UP = 0,
	SWITCHTEC_GFMS_EVENT_HOST_LINK_DOWN = 1,
	SWITCHTEC_GFMS_EVENT_DEV_ADD = 2,
	SWITCHTEC_GFMS_EVENT_DEV_DEL = 3,
	SWITCHTEC_GFMS_EVENT_FAB_LINK_UP = 4,
	SWITCHTEC_GFMS_EVENT_FAB_LINK_DOWN = 5,
	SWITCHTEC_GFMS_EVENT_BIND = 6,
	SWITCHTEC_GFMS_EVENT_UNBIND = 7,
	SWITCHTEC_GFMS_EVENT_DATABASE_CHANGED = 8,
	SWITCHTEC_GFMS_EVENT_HVD_INST_ENABLE = 9,
	SWITCHTEC_GFMS_EVENT_HVD_INST_DISABLE = 10,
	SWITCHTEC_GFMS_EVENT_EP_PORT_REMOVE = 11,
	SWITCHTEC_GFMS_EVENT_EP_PORT_ADD = 12,
	SWITCHTEC_GFMS_EVENT_AER = 13,
	SWITCHTEC_GFMS_EVENT_MAX = 14
};

/**
 * @brief The event data for SWITCHTEC_GFMS_EVENT_HOST_LINK_UP/DOWN
 */
struct switchtec_gfms_event_host {
	uint16_t phys_port_id;
};

/**
 * @brief The event data for SWITCHTEC_GFMS_EVENT_DEV_ADD/DEL
 */
struct switchtec_gfms_event_dev {
	uint16_t phys_port_id;
	uint16_t function_count;
};

/**
 * @brief The event data for SWITCHTEC_GFMS_EVENT_BIND/UNBIND
 */
struct switchtec_gfms_event_bind {
	uint8_t host_sw_idx;
	uint8_t host_phys_port_id;
	uint8_t log_port_id;
	uint8_t reserved1;
	uint16_t pdfid;
};

/**
 * @brief The event data for SWITCHTEC_GFMS_EVENT_HVD_INST_ENABLE/DISABLE
 */
struct switchtec_gfms_event_hvd {
	uint8_t hvd_inst_id;
	uint8_t phys_port_id;
	uint8_t clock_chan;
};

/**
 * @brief The event data for SWITCHTEC_GFMS_EVENT_EP_PORT_ADD/REMOVE
 */
struct switchtec_gfms_event_ep_port {
	uint8_t phys_port_id;
};

/**
 * @brief The event data for SWITCHTEC_GFMS_EVENT_AER
 */
struct switchtec_gfms_event_aer {
	uint16_t phys_port_id;
	uint8_t handle;
	uint8_t reserved1;
	uint32_t ce_ue_err_sts;
	uint32_t aer_err_log_time_stamp_high;
	uint32_t aer_err_log_time_stamp_low;
	uint32_t aer_header_log[4];
};
/** @brief Check if log saved in the AER */
#define switchtec_gfms_aer_log(aer) (((aer)->handle) & 0x01)
/** @brief Check if DPC triggered */
#define switchtec_gfms_aer_dpc(aer) (((aer)->handle) & 0x02)
/** @brief Return the CE/UE flag */
#define switchtec_gfms_aer_ce_ue(aer) (((aer)->handle) & 0x04)

/**
 * @brief Represents the GFMS event
 */
struct switchtec_gfms_event {
	int event_code;
	int src_sw_id;
	union {
		struct switchtec_gfms_event_host host;
		struct switchtec_gfms_event_dev dev;
		struct switchtec_gfms_event_bind bind;
		struct switchtec_gfms_event_hvd hvd;
		struct switchtec_gfms_event_ep_port ep;
		struct switchtec_gfms_event_aer aer;
		uint32_t byte[8];
	} data;
};

int switchtec_get_gfms_events(struct switchtec_dev *dev,
                              struct switchtec_gfms_event *elist,
                              size_t elist_len, int *overflow,
                              size_t *remain_number);

int switchtec_clear_gfms_events(struct switchtec_dev *dev);

/********** EP TUNNEL MANAGEMENT *********/
enum switchtec_ep_tunnel_status{
	SWITCHTEC_EP_TUNNEL_DISABLED = 0,
	SWITCHTEC_EP_TUNNEL_ENABLED = 1,
};

int switchtec_ep_tunnel_config(struct switchtec_dev *dev, uint16_t subcmd,
			       uint16_t pdfid, uint16_t expected_rsp_len,
			       uint8_t *meta_data, uint16_t meta_data_len,
			       uint8_t *rsp_data);
int switchtec_ep_tunnel_enable(struct switchtec_dev *dev, uint16_t pdfid);
int switchtec_ep_tunnel_disable(struct switchtec_dev *dev, uint16_t pdfid);
int switchtec_ep_tunnel_status(struct switchtec_dev *dev, uint16_t pdfid,
			       uint32_t *status);

/********** EP RESOURCE MANAGEMENT *********/
#define SWITCHTEC_EP_CSR_MAX_READ_LEN  4
#define SWITCHTEC_EP_CSR_MAX_WRITE_LEN 4
#define SWITCHTEC_EP_BAR_MAX_READ_LEN  SWITCHTEC_MRPC_PAYLOAD_SIZE

int switchtec_ep_csr_read8(struct switchtec_dev *dev, uint16_t pdfid,
			   uint16_t addr, uint8_t *val);
int switchtec_ep_csr_read16(struct switchtec_dev *dev, uint16_t pdfid,
			    uint16_t addr, uint16_t *val);
int switchtec_ep_csr_read32(struct switchtec_dev *dev, uint16_t pdfid,
			    uint16_t addr, uint32_t *val);

int switchtec_ep_csr_write8(struct switchtec_dev *dev, uint16_t pdfid,
			    uint8_t val, uint16_t addr);
int switchtec_ep_csr_write16(struct switchtec_dev *dev, uint16_t pdfid,
			     uint16_t val, uint16_t addr);
int switchtec_ep_csr_write32(struct switchtec_dev *dev, uint16_t pdfid,
			     uint32_t val, uint16_t addr);

int switchtec_ep_bar_read8(struct switchtec_dev *dev, uint16_t pdfid,
			   uint8_t bar_index, uint64_t addr, uint8_t *val);
int switchtec_ep_bar_read16(struct switchtec_dev *dev, uint16_t pdfid,
			    uint8_t bar_index, uint64_t addr, uint16_t *val);
int switchtec_ep_bar_read32(struct switchtec_dev *dev, uint16_t pdfid,
			    uint8_t bar_index, uint64_t addr, uint32_t *val);

#ifdef __cplusplus
}
#endif

#endif
