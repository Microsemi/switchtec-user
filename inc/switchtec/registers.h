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

#pragma once

#include <stdint.h>

#define SWITCHTEC_MRPC_PAYLOAD_SIZE 1024
#define SWITCHTEC_MAX_PFF_CSR 48
#define SWITCHTEC_MAX_PARTITIONS 48

#define MICROSEMI_VENDOR_ID 0x11f8

#define BIT(x) (1 << x)
#define SWITCHTEC_EVENT_OCCURRED BIT(0)
#define SWITCHTEC_EVENT_CLEAR    BIT(0)
#define SWITCHTEC_EVENT_EN_LOG   BIT(1)
#define SWITCHTEC_EVENT_EN_CLI   BIT(2)
#define SWITCHTEC_EVENT_EN_IRQ   BIT(3)
#define SWITCHTEC_EVENT_FATAL    BIT(4)

#ifdef MSVC
#pragma warning(disable: 4201)
#endif

#pragma pack(push, 1)

enum {
	SWITCHTEC_GAS_MRPC_OFFSET = 0x0000,
	SWITCHTEC_GAS_TOP_CFG_OFFSET = 0x1000,
	SWITCHTEC_GAS_SW_EVENT_OFFSET = 0x1800,
	SWITCHTEC_GAS_SYS_INFO_OFFSET = 0x2000,
	SWITCHTEC_GAS_FLASH_INFO_OFFSET = 0x2200,
	SWITCHTEC_GAS_PART_CFG_OFFSET = 0x4000,
	SWITCHTEC_GAS_NTB_OFFSET = 0x10000,
	SWITCHTEC_GAS_PFF_CSR_OFFSET = 0x134000,
};

struct mrpc_regs {
	uint8_t input_data[SWITCHTEC_MRPC_PAYLOAD_SIZE];
	uint8_t output_data[SWITCHTEC_MRPC_PAYLOAD_SIZE];
	uint32_t cmd;
	uint32_t status;
	uint32_t ret_value;
};

enum mrpc_status {
	SWITCHTEC_MRPC_STATUS_INPROGRESS = 1,
	SWITCHTEC_MRPC_STATUS_DONE = 2,
	SWITCHTEC_MRPC_STATUS_ERROR = 0xFF,
	SWITCHTEC_MRPC_STATUS_INTERRUPTED = 0x100,
};

struct top_regs {
	uint8_t bifur_valid;
	uint8_t stack_valid[6];
	uint8_t partition_count;
	uint8_t partition_id;
	uint8_t pff_count;
	uint8_t pff_port[255];
};

struct event {
	uint32_t hdr;
	uint32_t data[5];
};

struct sw_event_regs {
	uint64_t event_report_ctrl;
	uint64_t reserved1;
	uint64_t part_event_bitmap;
	uint64_t reserved2;
	uint32_t global_summary;
	uint32_t reserved3[3];
	uint32_t stack_error_event_hdr;
	uint32_t stack_error_event_data;
	uint32_t reserved4[4];
	uint32_t ppu_error_event_hdr;
	uint32_t ppu_error_event_data;
	uint32_t reserved5[4];
	uint32_t isp_error_event_hdr;
	uint32_t isp_error_event_data;
	uint32_t reserved6[4];
	uint32_t sys_reset_event_hdr;
	uint32_t reserved7[5];
	uint32_t fw_exception_hdr;
	uint32_t reserved8[5];
	uint32_t fw_nmi_hdr;
	uint32_t reserved9[5];
	uint32_t fw_non_fatal_hdr;
	uint32_t reserved10[5];
	uint32_t fw_fatal_hdr;
	uint32_t reserved11[5];
	uint32_t twi_mrpc_comp_hdr;
	uint32_t twi_mrpc_comp_data;
	uint32_t reserved12[4];
	uint32_t twi_mrpc_comp_async_hdr;
	uint32_t twi_mrpc_comp_async_data;
	uint32_t reserved13[4];
	uint32_t cli_mrpc_comp_hdr;
	uint32_t cli_mrpc_comp_data;
	uint32_t reserved14[4];
	uint32_t cli_mrpc_comp_async_hdr;
	uint32_t cli_mrpc_comp_async_data;
	uint32_t reserved15[4];
	uint32_t gpio_interrupt_hdr;
	uint32_t gpio_interrupt_data;
	uint32_t reserved16[4];
	uint32_t gfms_event_hdr;		//!< Event specific for PAX
	uint32_t gfms_event_data;
	uint32_t reserved17[4];
	uint32_t reserved18[60];
	struct event customer_events[6];
	uint32_t reserved19[320];
};

enum {
	SWITCHTEC_CFG0_RUNNING = 0x04,
	SWITCHTEC_CFG1_RUNNING = 0x05,
	SWITCHTEC_IMG0_RUNNING = 0x03,
	SWITCHTEC_IMG1_RUNNING = 0x07,
};

struct sys_info_regs {
	uint32_t device_id;
	uint32_t device_version;
	uint32_t firmware_version;
	uint32_t reserved1;
	uint32_t vendor_table_revision;
	uint32_t table_format_version;
	uint32_t partition_id;
	uint32_t cfg_file_fmt_version;
	uint16_t cfg_running;
	uint16_t img_running;
	uint32_t reserved2[57];
	char vendor_id[8];
	char product_id[16];
	char product_revision[4];
	char component_vendor[8];
	uint16_t component_id;
	uint8_t component_revision;
};

struct flash_info_regs {
	uint32_t flash_part_map_upd_idx;

	struct active_partition_info {
		uint32_t address;
		uint32_t build_version;
		uint32_t build_string;
	} active_img;

	struct active_partition_info active_cfg;
	struct active_partition_info inactive_img;
	struct active_partition_info inactive_cfg;

	uint32_t flash_length;

	struct partition_info {
		uint32_t address;
		uint32_t length;
	} cfg0;

	struct partition_info cfg1;
	struct partition_info img0;
	struct partition_info img1;
	struct partition_info nvlog;
	struct partition_info vendor[8];
};

struct part_cfg_regs {
	uint32_t status;
	uint32_t state;
	uint32_t port_cnt;
	uint32_t usp_port_mode;
	uint32_t usp_pff_inst_id;
	uint32_t vep_pff_inst_id;
	uint32_t dsp_pff_inst_id[47];
	uint32_t reserved1[11];
	uint16_t vep_vector_number;
	uint16_t usp_vector_number;
	uint32_t port_event_bitmap;
	uint32_t reserved2[3];
	uint32_t part_event_summary;
	uint32_t reserved3[3];
	uint32_t part_reset_hdr;
	uint32_t part_reset_data[5];
	uint32_t mrpc_comp_hdr;
	uint32_t mrpc_comp_data[5];
	uint32_t mrpc_comp_async_hdr;
	uint32_t mrpc_comp_async_data[5];
	uint32_t dyn_binding_hdr;
	uint32_t dyn_binding_data[5];
	uint32_t reserved4[120];
	struct event customer_events[6];
	uint32_t reserved5[3];
};

enum {
	SWITCHTEC_NTB_REG_INFO_OFFSET = 0x0000,
	SWITCHTEC_NTB_REG_CTRL_OFFSET = 0x4000,
	SWITCHTEC_NTB_REG_DBMSG_OFFSET = 0x64000,
};

struct ntb_info_regs {
	uint8_t  partition_count;
	uint8_t  partition_id;
	uint16_t reserved1;
	uint64_t ep_map;
	uint16_t requester_id;
};

enum {
	NTB_CTRL_PART_OP_LOCK = 0x1,
	NTB_CTRL_PART_OP_CFG = 0x2,
	NTB_CTRL_PART_OP_RESET = 0x3,

	NTB_CTRL_PART_STATUS_NORMAL = 0x1,
	NTB_CTRL_PART_STATUS_LOCKED = 0x2,
	NTB_CTRL_PART_STATUS_LOCKING = 0x3,
	NTB_CTRL_PART_STATUS_CONFIGURING = 0x4,
	NTB_CTRL_PART_STATUS_RESETTING = 0x5,

	NTB_CTRL_BAR_VALID = 1 << 0,
	NTB_CTRL_BAR_DIR_WIN_EN = 1 << 4,
	NTB_CTRL_BAR_LUT_WIN_EN = 1 << 5,

	NTB_CTRL_REQ_ID_EN = 1 << 0,

	NTB_CTRL_LUT_EN = 1 << 0,

	NTB_PART_CTRL_ID_PROT_DIS = 1 << 0,
};

struct ntb_ctrl_regs {
	uint32_t partition_status;
	uint32_t partition_op;
	uint32_t partition_ctrl;
	uint32_t bar_setup;
	uint32_t bar_error;
	uint16_t lut_table_entries;
	uint16_t lut_table_offset;
	uint32_t lut_error;
	uint16_t req_id_table_size;
	uint16_t req_id_table_offset;
	uint32_t req_id_error;
	uint32_t reserved1[7];
	struct {
		uint32_t ctl;
		uint32_t win_size;
		uint64_t xlate_addr;
	} bar_entry[6];
	uint32_t reserved2[216];
	uint32_t req_id_table[256];
	uint32_t reserved3[512];
	uint64_t lut_entry[512];
};

#define NTB_DBMSG_IMSG_STATUS BIT_ULL(32)
#define NTB_DBMSG_IMSG_MASK   BIT_ULL(40)

struct ntb_dbmsg_regs {
	uint32_t reserved1[1024];
	uint64_t odb;
	uint64_t odb_mask;
	uint64_t idb;
	uint64_t idb_mask;
	uint8_t  idb_vec_map[64];
	uint32_t msg_map;
	uint32_t reserved2;
	struct {
		uint32_t msg;
		uint32_t status;
	} omsg[4];

	struct {
		uint32_t msg;
		uint8_t  status;
		uint8_t  mask;
		uint8_t  src;
		uint8_t  reserved;
	} imsg[4];

	uint8_t reserved3[3928];
	uint8_t msix_table[1024];
	uint8_t reserved4[3072];
	uint8_t pba[24];
	uint8_t reserved5[4072];
};

struct ntb_regs {
	union {
		struct ntb_info_regs info;
		uint8_t __pad_info[SWITCHTEC_NTB_REG_CTRL_OFFSET -
				   SWITCHTEC_NTB_REG_INFO_OFFSET];
	};

	union {
		struct ntb_ctrl_regs ctrl[SWITCHTEC_MAX_PARTITIONS];
		uint8_t __pad_ctrl[SWITCHTEC_NTB_REG_DBMSG_OFFSET -
				   SWITCHTEC_NTB_REG_CTRL_OFFSET];
	};

	struct ntb_dbmsg_regs dbmsg[SWITCHTEC_MAX_PARTITIONS];
};

enum {
	SWITCHTEC_PART_CFG_EVENT_RESET = 1 << 0,
	SWITCHTEC_PART_CFG_EVENT_MRPC_CMP = 1 << 1,
	SWITCHTEC_PART_CFG_EVENT_MRPC_ASYNC_CMP = 1 << 2,
	SWITCHTEC_PART_CFG_EVENT_DYN_PART_CMP = 1 << 3,
};

struct pff_csr_regs {
	uint16_t vendor_id;
	uint16_t device_id;
	uint32_t pci_cfg_header[15];
	uint32_t pci_cap_region[48];
	uint32_t pcie_cap_region[448];
	uint32_t indirect_gas_window[128];
	uint32_t indirect_gas_window_off;
	uint32_t reserved[127];
	uint32_t pff_event_summary;
	uint32_t reserved2[3];
	uint32_t aer_in_p2p_hdr;
	uint32_t aer_in_p2p_data[5];
	uint32_t aer_in_vep_hdr;
	uint32_t aer_in_vep_data[5];
	uint32_t dpc_hdr;
	uint32_t dpc_data[5];
	uint32_t cts_hdr;
	uint32_t cts_data[5];
	uint32_t reserved3[6];
	uint32_t hotplug_hdr;
	uint32_t hotplug_data[5];
	uint32_t ier_hdr;
	uint32_t ier_data[5];
	uint32_t threshold_hdr;
	uint32_t threshold_data[5];
	uint32_t power_mgmt_hdr;
	uint32_t power_mgmt_data[5];
	uint32_t tlp_throttling_hdr;
	uint32_t tlp_throttling_data[5];
	uint32_t force_speed_hdr;
	uint32_t force_speed_data[5];
	uint32_t credit_timeout_hdr;
	uint32_t credit_timeout_data[5];
	uint32_t link_state_hdr;
	uint32_t link_state_data[5];
	uint32_t reserved4[66];
	struct event customer_events[6];
	uint32_t reserved5[72];
};

struct switchtec_gas {
	union {
		struct mrpc_regs mrpc;
		uint8_t __pad_mrpc[SWITCHTEC_GAS_TOP_CFG_OFFSET];
	};

	union {
		struct top_regs top;
		uint8_t __pad_top_cfg[SWITCHTEC_GAS_SW_EVENT_OFFSET -
				      SWITCHTEC_GAS_TOP_CFG_OFFSET];
	};

	union {
		struct sw_event_regs sw_event;
		uint8_t __pad_sw_event[SWITCHTEC_GAS_SYS_INFO_OFFSET -
				       SWITCHTEC_GAS_SW_EVENT_OFFSET];
	};

	union {
		struct sys_info_regs sys_info;
		uint8_t __pad_sys_info[SWITCHTEC_GAS_FLASH_INFO_OFFSET -
				       SWITCHTEC_GAS_SYS_INFO_OFFSET];
	};

	union {
		struct flash_info_regs flash_info;
		uint8_t __pad_flash_info[SWITCHTEC_GAS_PART_CFG_OFFSET -
					 SWITCHTEC_GAS_FLASH_INFO_OFFSET];
	};

	union {
		struct part_cfg_regs part_cfg[SWITCHTEC_MAX_PARTITIONS];
		uint8_t __pad_part_cfg[SWITCHTEC_GAS_NTB_OFFSET -
				       SWITCHTEC_GAS_PART_CFG_OFFSET];
	};

	union {
		struct ntb_regs ntb;
		uint8_t __pad_ntb[SWITCHTEC_GAS_PFF_CSR_OFFSET -
				  SWITCHTEC_GAS_NTB_OFFSET];
	};

	struct pff_csr_regs pff_csr[SWITCHTEC_MAX_PFF_CSR];
};

#pragma pack(pop)
