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

#ifndef LIBSWITCHTEC_SWITCHTEC_H
#define LIBSWITCHTEC_SWITCHTEC_H

/**
 * @file
 * @brief Main Switchtec header
 */

#include "mrpc.h"
#include "bind.h"
#include "portable.h"
#include "registers.h"
#include "utils.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

struct switchtec_dev;

#define SWITCHTEC_MAX_PARTS  48
#define SWITCHTEC_MAX_PORTS  60
#define SWITCHTEC_MAX_LANES  100
#define SWITCHTEC_MAX_STACKS 8
#define SWITCHTEC_MAX_EVENT_COUNTERS 64
#define SWITCHTEC_UNBOUND_PORT 255
#define SWITCHTEC_PFF_PORT_VEP 100

#define SWITCHTEC_FLASH_BOOT_PART_START 0xa8000000
#define SWITCHTEC_FLASH_MAP0_PART_START 0xa8020000
#define SWITCHTEC_FLASH_MAP1_PART_START 0xa8040000
#define SWITCHTEC_FLASH_PART_LEN 0x10000

#define SWITCHTEC_CMD_MASK 0xffff
#define SWITCHTEC_PAX_ID_SHIFT 18
#define SWITCHTEC_PAX_ID_MASK 0x1f
#define SWITCHTEC_PAX_ID_LOCAL SWITCHTEC_PAX_ID_MASK

#ifdef __CHECKER__
#define __gas __attribute__((noderef, address_space(1)))
#else
#define __gas
#endif

#define _PURE __attribute__ ((pure))

/**
 * @brief Shortform for a pointer to the GAS register space
 */
typedef __gas struct switchtec_gas *gasptr_t;
#define SWITCHTEC_MAP_FAILED ((gasptr_t) -1)

/**
 * @brief The PCIe generations
 */
enum switchtec_gen {
	SWITCHTEC_GEN3,
	SWITCHTEC_GEN4,
	SWITCHTEC_GEN5,
	SWITCHTEC_GEN_UNKNOWN,
};

/**
 * @brief Device hardware revision
 */
enum switchtec_rev {
	SWITCHTEC_REVA = 0x0f,
	SWITCHTEC_REVB = 0x00,
	SWITCHTEC_REVC = 0x01,
	SWITCHTEC_REV_UNKNOWN = 0xff
};

/**
 * @brief Device boot phase
 */
enum switchtec_boot_phase {
	SWITCHTEC_BOOT_PHASE_BL1 = 1,
	SWITCHTEC_BOOT_PHASE_BL2,
	SWITCHTEC_BOOT_PHASE_FW,
	SWITCHTEC_BOOT_PHASE_UNKNOWN
};

/**
 * @brief The variant types of Switchtec device
 */
enum switchtec_variant {
	SWITCHTEC_PFX,
	SWITCHTEC_PFXL,
	SWITCHTEC_PFXI,
	SWITCHTEC_PSX,
	SWITCHTEC_PAX,
	SWITCHTEC_PAXA,
	SWITCHTEC_PFXA,
	SWITCHTEC_PSXA,
	SWITCHTEC_VAR_UNKNOWN,
};

/**
 * @brief Represents a Switchtec device in the switchtec_list() function
 */
struct switchtec_device_info {
	char name[256];		//!< Device name, eg. switchtec0
	char desc[256];		//!< Device description, if available
	char pci_dev[256];	//!< PCI BDF string
	char product_id[32];	//!< Product ID
	char product_rev[8];	//!< Product revision
	char fw_version[32];	//!< Firmware version
	char path[PATH_MAX];	//!< Path to the device
};

/**
 * @brief Port identification
 */
struct switchtec_port_id {
	unsigned char partition;	//!< Partition the port is in.
					/*!< May be SWITCHTEC_UNBOUND_PORT. */
	unsigned char stack;		//!< Stack number
	unsigned char upstream;		//!< 1 if this is an upstream port
	unsigned char stk_id;		//!< Port number within the stack
	unsigned char phys_id;		//!< Physical port number
	unsigned char log_id;		//!< Logical port number
};

/**
 * @brief Port status structure
 *
 * \p pci_dev, \p vendor_id, \p device_id and \p class_devices are populated by
 * switchtec_get_devices(). These are only available in Linux.
 */
struct switchtec_status {
	struct switchtec_port_id port;	//!< Port ID
	unsigned char cfg_lnk_width;	//!< Configured link width
	unsigned char neg_lnk_width;	//!< Negotiated link width
	unsigned char link_up;		//!< 1 if the link is up
	unsigned char link_rate;	//!< Link rate/gen
	uint16_t ltssm;			//!< Link state
	const char *ltssm_str;		//!< Link state as a string
	unsigned char lane_reversal;	//!< Lane reversal
	const char *lane_reversal_str;	//!< Lane reversal as a string
	unsigned char first_act_lane;	//!< First active lane
	char lanes[17];

	char *pci_bdf;			//!< PCI BDF of the port
	char *pci_bdf_path;		//!< PCI BDF path of the port

	char *pci_dev;			//!< PCI BDF of the device on the port
	int vendor_id;			//!< Vendor ID
	int device_id;			//!< Device ID
	char *class_devices;		//!< Comma seperated list of classes
	unsigned int acs_ctrl;		//!< ACS Setting of the Port
};

/**
 * @brief The types of bandwidth
 */
enum switchtec_bw_type {
	SWITCHTEC_BW_TYPE_RAW = 0x0,
	SWITCHTEC_BW_TYPE_PAYLOAD = 0x1,
};

/**
 * @brief Describe the type of logs too dump
 * @see switchtec_log_to_file()
 */
enum switchtec_log_type {
	SWITCHTEC_LOG_RAM,
	SWITCHTEC_LOG_FLASH,
	SWITCHTEC_LOG_MEMLOG,
	SWITCHTEC_LOG_REGS,
	SWITCHTEC_LOG_SYS_STACK,
	SWITCHTEC_LOG_THRD_STACK,
	SWITCHTEC_LOG_THRD,
	SWITCHTEC_LOG_NVHDR,
};

/**
 * @brief Log types to parse
 */
enum switchtec_log_parse_type {
	SWITCHTEC_LOG_PARSE_TYPE_APP,
	SWITCHTEC_LOG_PARSE_TYPE_MAILBOX
};

/**
 * @brief Information about log file and log definition file
 */
struct switchtec_log_file_info {
	unsigned int log_fw_version;
	unsigned int log_sdk_version;
	unsigned int def_fw_version;
	unsigned int def_sdk_version;
	bool version_mismatch;
	bool overflow;
};

/**
 * @brief Log definition data types
 */
enum switchtec_log_def_type {
	SWITCHTEC_LOG_DEF_TYPE_APP,
	SWITCHTEC_LOG_DEF_TYPE_MAILBOX
};

enum switchtec_fw_type {
	SWITCHTEC_FW_TYPE_UNKNOWN = 0,
	SWITCHTEC_FW_TYPE_BOOT,
	SWITCHTEC_FW_TYPE_MAP,
	SWITCHTEC_FW_TYPE_IMG,
	SWITCHTEC_FW_TYPE_CFG,
	SWITCHTEC_FW_TYPE_NVLOG,
	SWITCHTEC_FW_TYPE_SEEPROM,
	SWITCHTEC_FW_TYPE_KEY,
	SWITCHTEC_FW_TYPE_BL2,
};

/**
 * @brief Information about a firmware image or partition
 */
struct switchtec_fw_image_info {
	enum switchtec_gen gen;			//!< Image generation
	unsigned long part_id;			//!< Image partition ID
	enum switchtec_fw_type type;		//!< Image partition type
	char version[32];			//!< Firmware/Config version
	size_t part_addr;			//!< Address of the partition
	size_t part_len;			//!< Length of the partition
	size_t part_body_offset;		//!< Partition image body offset
	size_t image_len;			//!< Length of the image
	unsigned long image_crc;		//!< CRC checksum of the image

	bool valid;
	bool active;
	bool running;
	bool read_only;
	bool redundant;

	struct switchtec_fw_image_info *next;
	void *metadata;

	unsigned long secure_version;
	bool signed_image;
};

struct switchtec_fw_part_summary {
	struct switchtec_fw_part_type {
		struct switchtec_fw_image_info *active, *inactive;
	} boot, map, img, cfg, nvlog, seeprom, key, bl2;

	struct switchtec_fw_image_info *mult_cfg;

	int nr_info;
	struct switchtec_fw_image_info all[];
};

/**
 * @brief Event summary bitmaps
 */
struct switchtec_event_summary {
	uint64_t global;	//!< Bitmap of global events
	uint64_t part_bitmap;	//!< Bitmap of partitions with active events
	unsigned local_part;	//!< Bitmap of events in the local partition

	/** @brief Bitmap of events in each partition */
	unsigned part[SWITCHTEC_MAX_PARTS];

	/** @brief Bitmap of events in each port function */
	unsigned pff[SWITCHTEC_MAX_PFF_CSR];
};

/**
 * @brief Enumeration of all possible events
 */
enum switchtec_event_id {
	SWITCHTEC_EVT_INVALID = -1,
	SWITCHTEC_GLOBAL_EVT_STACK_ERROR,
	SWITCHTEC_GLOBAL_EVT_PPU_ERROR,
	SWITCHTEC_GLOBAL_EVT_ISP_ERROR,
	SWITCHTEC_GLOBAL_EVT_SYS_RESET,
	SWITCHTEC_GLOBAL_EVT_FW_EXC,
	SWITCHTEC_GLOBAL_EVT_FW_NMI,
	SWITCHTEC_GLOBAL_EVT_FW_NON_FATAL,
	SWITCHTEC_GLOBAL_EVT_FW_FATAL,
	SWITCHTEC_GLOBAL_EVT_TWI_MRPC_COMP,
	SWITCHTEC_GLOBAL_EVT_TWI_MRPC_COMP_ASYNC,
	SWITCHTEC_GLOBAL_EVT_CLI_MRPC_COMP,
	SWITCHTEC_GLOBAL_EVT_CLI_MRPC_COMP_ASYNC,
	SWITCHTEC_GLOBAL_EVT_GPIO_INT,
	SWITCHTEC_GLOBAL_EVT_GFMS,
	SWITCHTEC_PART_EVT_PART_RESET,
	SWITCHTEC_PART_EVT_MRPC_COMP,
	SWITCHTEC_PART_EVT_MRPC_COMP_ASYNC,
	SWITCHTEC_PART_EVT_DYN_PART_BIND_COMP,
	SWITCHTEC_PFF_EVT_AER_IN_P2P,
	SWITCHTEC_PFF_EVT_AER_IN_VEP,
	SWITCHTEC_PFF_EVT_DPC,
	SWITCHTEC_PFF_EVT_CTS,
	SWITCHTEC_PFF_EVT_UEC,
	SWITCHTEC_PFF_EVT_HOTPLUG,
	SWITCHTEC_PFF_EVT_IER,
	SWITCHTEC_PFF_EVT_THRESH,
	SWITCHTEC_PFF_EVT_POWER_MGMT,
	SWITCHTEC_PFF_EVT_TLP_THROTTLING,
	SWITCHTEC_PFF_EVT_FORCE_SPEED,
	SWITCHTEC_PFF_EVT_CREDIT_TIMEOUT,
	SWITCHTEC_PFF_EVT_LINK_STATE,
	SWITCHTEC_MAX_EVENTS,
};

/*********** Platform Functions ***********/

struct switchtec_dev *switchtec_open(const char *device);
struct switchtec_dev *switchtec_open_by_path(const char *path);
struct switchtec_dev *switchtec_open_by_index(int index);
struct switchtec_dev *switchtec_open_by_pci_addr(int domain, int bus,
						 int device, int func);
struct switchtec_dev *switchtec_open_i2c(const char *path, int i2c_addr);
struct switchtec_dev *switchtec_open_i2c_by_adapter(int adapter, int i2c_addr);
struct switchtec_dev *switchtec_open_uart(int fd);
struct switchtec_dev *switchtec_open_eth(const char *ip, const int inst);

void switchtec_close(struct switchtec_dev *dev);
int switchtec_list(struct switchtec_device_info **devlist);
void switchtec_list_free(struct switchtec_device_info *devlist);
int switchtec_get_fw_version(struct switchtec_dev *dev, char *buf,
			     size_t buflen);
int switchtec_cmd(struct switchtec_dev *dev, uint32_t cmd,
		  const void *payload, size_t payload_len, void *resp,
		  size_t resp_len);
int switchtec_get_devices(struct switchtec_dev *dev,
			  struct switchtec_status *status,
			  int ports);
int switchtec_pff_to_port(struct switchtec_dev *dev, int pff,
			  int *partition, int *port);
int switchtec_port_to_pff(struct switchtec_dev *dev, int partition,
			  int port, int *pff);
int switchtec_event_summary(struct switchtec_dev *dev,
			    struct switchtec_event_summary *sum);
int switchtec_event_check(struct switchtec_dev *dev,
			  struct switchtec_event_summary *check,
			  struct switchtec_event_summary *res);
int switchtec_event_ctl(struct switchtec_dev *dev,
			enum switchtec_event_id e,
			int index, int flags,
			uint32_t data[5]);
int switchtec_event_wait(struct switchtec_dev *dev, int timeout_ms);

/*********** Generic Accessors ***********/

_PURE const char *switchtec_name(struct switchtec_dev *dev);
_PURE int switchtec_partition(struct switchtec_dev *dev);
_PURE int switchtec_device_id(struct switchtec_dev *dev);
_PURE enum switchtec_gen switchtec_gen(struct switchtec_dev *dev);
_PURE enum switchtec_variant switchtec_variant(struct switchtec_dev *dev);
_PURE enum switchtec_boot_phase
switchtec_boot_phase(struct switchtec_dev *dev);
int switchtec_set_pax_id(struct switchtec_dev *dev, int pax_id);
int switchtec_echo(struct switchtec_dev *dev, uint32_t input, uint32_t *output);
int switchtec_hard_reset(struct switchtec_dev *dev);
int switchtec_status(struct switchtec_dev *dev,
		     struct switchtec_status **status);
void switchtec_status_free(struct switchtec_status *status, int ports);
int switchtec_get_device_info(struct switchtec_dev *dev,
			      enum switchtec_boot_phase *phase,
			      enum switchtec_gen *gen,
			      enum switchtec_rev *rev);
const char *switchtec_strerror(void);
void switchtec_perror(const char *str);
int switchtec_log_to_file(struct switchtec_dev *dev,
		enum switchtec_log_type type, int fd, FILE *log_def_file,
		struct switchtec_log_file_info *info);
int switchtec_parse_log(FILE *bin_log_file, FILE *log_def_file,
			FILE *parsed_log_file,
			enum switchtec_log_parse_type log_type,
			struct switchtec_log_file_info *info);
int switchtec_log_def_to_file(struct switchtec_dev *dev,
			      enum switchtec_log_def_type type,
			      FILE* file);
float switchtec_die_temp(struct switchtec_dev *dev);
int switchtec_calc_lane_id(struct switchtec_dev *dev, int phys_port_id,
			   int lane_id, struct switchtec_status *port);
int switchtec_calc_port_lane(struct switchtec_dev *dev, int lane_id,
			     int *phys_port_id, int *port_lane_id,
			     struct switchtec_status *port);
int switchtec_calc_lane_mask(struct switchtec_dev *dev, int phys_port_id,
		int lane_id, int num_lanes, int *lane_mask,
		struct switchtec_status *port);

/**
 * @brief Return whether a Switchtec device is a Gen 3 device.
 */
static inline int switchtec_is_gen3(struct switchtec_dev *dev)
{
	return switchtec_gen(dev) == SWITCHTEC_GEN3;
}

/**
 * @brief Return whether a Switchtec device is a Gen 4 device.
 */
static inline int switchtec_is_gen4(struct switchtec_dev *dev)
{
	return switchtec_gen(dev) == SWITCHTEC_GEN4;
}

/**
 * @brief Return whether a Switchtec device is a Gen 5 device.
 */
static inline int switchtec_is_gen5(struct switchtec_dev *dev)
{
	return switchtec_gen(dev) == SWITCHTEC_GEN5;
}

/**
 * @brief Return whether a Switchtec device is PFX.
 */
static inline int switchtec_is_pfx(struct switchtec_dev *dev)
{
	return switchtec_variant(dev) == SWITCHTEC_PFX;
}

/**
 * @brief Return whether a Switchtec device is PFX-L.
 */
static inline int switchtec_is_pfxl(struct switchtec_dev *dev)
{
	return switchtec_variant(dev) == SWITCHTEC_PFXL;
}

/**
 * @brief Return whether a Switchtec device is PFX-I.
 */
static inline int switchtec_is_pfxi(struct switchtec_dev *dev)
{
	return switchtec_variant(dev) == SWITCHTEC_PFXI;
}

/**
 * @brief Return whether a Switchtec device is PFX-A.
 */
static inline int switchtec_is_pfxa(struct switchtec_dev *dev)
{
	return switchtec_variant(dev) == SWITCHTEC_PFXA;
}

/**
 * @brief Return whether a Switchtec device is PFX(L/I/A).
 */
static inline int switchtec_is_pfx_all(struct switchtec_dev *dev)
{
	return switchtec_is_pfx(dev) ||
	       switchtec_is_pfxl(dev) ||
	       switchtec_is_pfxi(dev) ||
	       switchtec_is_pfxa(dev);
}

/**
 * @brief Return whether a Switchtec device is PSX.
 */
static inline int switchtec_is_psx(struct switchtec_dev *dev)
{
	return switchtec_variant(dev) == SWITCHTEC_PSX;
}

/**
 * @brief Return whether a Switchtec device is PSX-A.
 */
static inline int switchtec_is_psxa(struct switchtec_dev *dev)
{
	return switchtec_variant(dev) == SWITCHTEC_PSXA;
}

/**
 * @brief Return whether a Switchtec device is PSX(A).
 */
static inline int switchtec_is_psx_all(struct switchtec_dev *dev)
{
	return switchtec_is_psx(dev) ||
	       switchtec_is_psxa(dev);
}

/**
 * @brief Return whether a Switchtec device is PFX or PSX.
 */
static inline int switchtec_is_psx_pfx_all(struct switchtec_dev *dev)
{
	return switchtec_is_psx_all(dev) || switchtec_is_pfx_all(dev);
}

/**
 * @brief Return whether a Switchtec device is PAX.
 */
static inline int switchtec_is_pax(struct switchtec_dev *dev)
{
	return switchtec_variant(dev) == SWITCHTEC_PAX;
}

/**
 * @brief Return whether a Switchtec device is PAX-A.
 */
static inline int switchtec_is_paxa(struct switchtec_dev *dev)
{
	return switchtec_variant(dev) == SWITCHTEC_PAXA;
}

/**
 * @brief Return whether a Switchtec device is PAX(A).
 */
static inline int switchtec_is_pax_all(struct switchtec_dev *dev)
{
	return switchtec_is_pax(dev) || switchtec_is_paxa(dev);
}

/**
 * @brief Return the generation string of a Switchtec device.
 */
static inline const char *switchtec_gen_str(struct switchtec_dev *dev)
{
	const char *str;

	str =  switchtec_is_gen3(dev) ? "GEN3" :
	       switchtec_is_gen4(dev) ? "GEN4" :
	       switchtec_is_gen5(dev) ? "GEN5" : "Unknown";

	return str;
}

/**
 * @brief Return the revision string
 */
static inline const char *switchtec_rev_str(enum switchtec_rev rev)
{
	const char *str;

	str =  (rev == SWITCHTEC_REVA) ? "REVA" :
	       (rev == SWITCHTEC_REVB) ? "REVB" :
	       (rev == SWITCHTEC_REVC) ? "REVC" : "Unknown";

	return str;
}

/**
 * @brief Return the generation string of a Switchtec fw image.
 */
static inline const char *
switchtec_fw_image_gen_str(struct switchtec_fw_image_info *inf)
{
	switch (inf->gen) {
	case SWITCHTEC_GEN3: return "GEN3";
	case SWITCHTEC_GEN4: return "GEN4";
	case SWITCHTEC_GEN5: return "GEN5";
	default:	     return "UNKNOWN";
	}
}

/**
 * @brief Return the variant string of a Switchtec device.
 */
static inline const char *switchtec_variant_str(struct switchtec_dev *dev)
{
	const char *str;

	str = switchtec_is_pfx(dev) ? "PFX" :
	      switchtec_is_pfxl(dev) ? "PFX-L" :
	      switchtec_is_pfxi(dev) ? "PFX-I" :
	      switchtec_is_psx(dev) ? "PSX" :
	      switchtec_is_pax(dev) ? "PAX" :
	      switchtec_is_pfxa(dev) ? "PFX-A" :
	      switchtec_is_psxa(dev) ? "PSX-A" :
	      switchtec_is_paxa(dev) ? "PAX-A" : "Unknown";

	return str;
}

/** @brief Number of GT/s capable for each PCI generation or \p link_rate */
static const float switchtec_gen_transfers[] = {0, 2.5, 5, 8, 16};
/** @brief Number of GB/s capable for each PCI generation or \p link_rate */
static const float switchtec_gen_datarate[] = {0, 250, 500, 985, 1969};

static inline const char *switchtec_ltssm_str(int ltssm, int show_minor)
{
	if (!show_minor)
		ltssm |= 0xFF00;

	switch(ltssm) {
	case 0x0000: return "Detect (INACTIVE)";
	case 0x0100: return "Detect (QUIET)";
	case 0x0200: return "Detect (SPD_CHD0)";
	case 0x0300: return "Detect (SPD_CHD1)";
	case 0x0400: return "Detect (ACTIVE0)";
	case 0x0500: return "Detect (ACTIVE1)";
	case 0x0600: return "Detect (P1_TO_P0)";
	case 0x0700: return "Detect (P0_TO_P1_0)";
	case 0x0800: return "Detect (P0_TO_P1_1)";
	case 0x0900: return "Detect (P0_TO_P1_2)";
	case 0xFF00: return "Detect";
	case 0x0001: return "Polling (INACTIVE)";
	case 0x0101: return "Polling (ACTIVE_ENTRY)";
	case 0x0201: return "Polling (ACTIVE)";
	case 0x0301: return "Polling (CFG)";
	case 0x0401: return "Polling (COMP)";
	case 0x0501: return "Polling (COMP_ENTRY)";
	case 0x0601: return "Polling (COMP_EIOS)";
	case 0x0701: return "Polling (COMP_EIOS_ACK)";
	case 0x0801: return "Polling (COMP_IDLE)";
	case 0xFF01: return "Polling";
	case 0x0002: return "Config (INACTIVE)";
	case 0x0102: return "Config (US_LW_START)";
	case 0x0202: return "Config (US_LW_ACCEPT)";
	case 0x0302: return "Config (US_LN_WAIT)";
	case 0x0402: return "Config (US_LN_ACCEPT)";
	case 0x0502: return "Config (DS_LW_START)";
	case 0x0602: return "Config (DS_LW_ACCEPT)";
	case 0x0702: return "Config (DS_LN_WAIT)";
	case 0x0802: return "Config (DS_LN_ACCEPT)";
	case 0x0902: return "Config (COMPLETE)";
	case 0x0A02: return "Config (IDLE)";
	case 0xFF02: return "Config";
	case 0x0003: return "L0 (INACTIVE)";
	case 0x0103: return "L0 (L0)";
	case 0x0203: return "L0 (TX_EL_IDLE)";
	case 0x0303: return "L0 (TX_IDLE_MIN)";
	case 0xFF03: return "L0";
	case 0x0004: return "Recovery (INACTIVE)";
	case 0x0104: return "Recovery (RCVR_LOCK)";
	case 0x0204: return "Recovery (RCVR_CFG)";
	case 0x0304: return "Recovery (IDLE)";
	case 0x0404: return "Recovery (SPEED0)";
	case 0x0504: return "Recovery (SPEED1)";
	case 0x0604: return "Recovery (SPEED2)";
	case 0x0704: return "Recovery (SPEED3)";
	case 0x0804: return "Recovery (EQ_PH0)";
	case 0x0904: return "Recovery (EQ_PH1)";
	case 0x0A04: return "Recovery (EQ_PH2)";
	case 0x0B04: return "Recovery (EQ_PH3)";
	case 0xFF04: return "Recovery";
	case 0x0005: return "Disable (INACTIVE)";
	case 0x0105: return "Disable (DISABLE0)";
	case 0x0205: return "Disable (DISABLE1)";
	case 0x0305: return "Disable (DISABLE2)";
	case 0x0405: return "Disable (DISABLE3)";
	case 0xFF05: return "Disable";
	case 0x0006: return "Loop Back (INACTIVE)";
	case 0x0106: return "Loop Back (ENTRY)";
	case 0x0206: return "Loop Back (ENTRY_EXIT)";
	case 0x0306: return "Loop Back (EIOS)";
	case 0x0406: return "Loop Back (EIOS_ACK)";
	case 0x0506: return "Loop Back (IDLE)";
	case 0x0606: return "Loop Back (ACTIVE)";
	case 0x0706: return "Loop Back (EXIT0)";
	case 0x0806: return "Loop Back (EXIT1)";
	case 0xFF06: return "Loop Back";
	case 0x0007: return "Hot Reset (INACTIVE)";
	case 0x0107: return "Hot Reset (HOT_RESET)";
	case 0x0207: return "Hot Reset (MASTER_UP)";
	case 0x0307: return "Hot Reset (MASTER_DOWN)";
	case 0xFF07: return "Hot Reset";
	case 0x0008: return "TxL0s (INACTIVE)";
	case 0x0108: return "TxL0s (IDLE)";
	case 0x0208: return "TxL0s (T0_L0)";
	case 0x0308: return "TxL0s (FTS0)";
	case 0x0408: return "TxL0s (FTS1)";
	case 0xFF08: return "TxL0s";
	case 0x0009: return "L1 (INACTIVE)";
	case 0x0109: return "L1 (IDLE)";
	case 0x0209: return "L1 (SUBSTATE)";
	case 0x0309: return "L1 (SPD_CHG1)";
	case 0x0409: return "L1 (T0_L0)";
	case 0xFF09: return "L1";
	case 0x000A: return "L2 (INACTIVE)";
	case 0x010A: return "L2 (IDLE)";
	case 0x020A: return "L2 (TX_WAKE0)";
	case 0x030A: return "L2 (TX_WAKE1)";
	case 0x040A: return "L2 (EXIT)";
	case 0x050A: return "L2 (SPEED)";
	case 0xFF0A: return "L2";
	default: return "UNKNOWN";
	}

}

/*********** EVENT Handling ***********/

/**
 * @brief Event control flags
 * @see switchtec_event_ctl()
 */
enum switchtec_event_flags {
	SWITCHTEC_EVT_FLAG_CLEAR = 1 << 0,
	SWITCHTEC_EVT_FLAG_EN_POLL = 1 << 1,
	SWITCHTEC_EVT_FLAG_EN_LOG = 1 << 2,
	SWITCHTEC_EVT_FLAG_EN_CLI = 1 << 3,
	SWITCHTEC_EVT_FLAG_EN_FATAL = 1 << 4,
	SWITCHTEC_EVT_FLAG_DIS_POLL = 1 << 5,
	SWITCHTEC_EVT_FLAG_DIS_LOG = 1 << 6,
	SWITCHTEC_EVT_FLAG_DIS_CLI = 1 << 7,
	SWITCHTEC_EVT_FLAG_DIS_FATAL = 1 << 8,
};

/**
 * @brief Special event indexes numbers.
 *
 * For specifying the local partition or all partitions/ports.
 *
 * @see switchtec_event_ctl()
 */
enum switchtec_event_special {
	SWITCHTEC_EVT_IDX_LOCAL = -1,
	SWITCHTEC_EVT_IDX_ALL = -2,
};

/**
 * @brief There are three event types indicated by this enumeration:
 * 	global, partition and port function
 */
enum switchtec_event_type {
	SWITCHTEC_EVT_GLOBAL,
	SWITCHTEC_EVT_PART,
	SWITCHTEC_EVT_PFF,
};

int switchtec_event_summary_set(struct switchtec_event_summary *sum,
				enum switchtec_event_id e,
				int index);
int switchtec_event_summary_test(struct switchtec_event_summary *sum,
				 enum switchtec_event_id e,
				 int index);
int switchtec_event_summary_iter(struct switchtec_event_summary *sum,
				 enum switchtec_event_id *e,
				 int *idx);
enum switchtec_event_type switchtec_event_info(enum switchtec_event_id e,
					       const char **name,
					       const char **desc);
int switchtec_event_wait_for(struct switchtec_dev *dev,
			     enum switchtec_event_id e, int index,
			     struct switchtec_event_summary *res,
			     int timeout_ms);

/******** FIRMWARE Management ********/

/**
 * @brief Firmware update status.
 * @see switchtec_fw_dlstatus()
 */
enum switchtec_fw_dlstatus {
	SWITCHTEC_DLSTAT_READY = 0x0,
	SWITCHTEC_DLSTAT_INPROGRESS = 0x1,
	SWITCHTEC_DLSTAT_HEADER_INCORRECT = 0x2,
	SWITCHTEC_DLSTAT_OFFSET_INCORRECT = 0x3,
	SWITCHTEC_DLSTAT_CRC_INCORRECT = 0x4,
	SWITCHTEC_DLSTAT_LENGTH_INCORRECT = 0x5,
	SWITCHTEC_DLSTAT_HARDWARE_ERR = 0x6,
	SWITCHTEC_DLSTAT_COMPLETES = 0x7,
	SWITCHTEC_DLSTAT_SUCCESS_FIRM_ACT = 0x8,
	SWITCHTEC_DLSTAT_SUCCESS_DATA_ACT = 0x9,
	SWITCHTEC_DLSTAT_PACKAGE_TOO_SMALL = 0xa,
	SWITCHTEC_DLSTAT_SIG_MEM_ALLOC = 0xb,
	SWITCHTEC_DLSTAT_SEEPROM = 0xc,
	SWITCHTEC_DLSTAT_READONLY_PARTITION = 0xd,
	SWITCHTEC_DLSTAT_DOWNLOAD_TIMEOUT = 0xe,
	SWITCHTEC_DLSTAT_SEEPROM_TWI_NOT_ENABLED = 0xf,
	SWITCHTEC_DLSTAT_PROGRAM_RUNNING = 0x10,
	SWITCHTEC_DLSTAT_NOT_ALLOWED = 0x11,
	SWITCHTEC_DLSTAT_XML_MISMATCH_ACT = 0x12,
	SWITCHTEC_DLSTAT_UNKNOWN_ACT = 0x13,

	SWITCHTEC_DLSTAT_ERROR_PROGRAM = 0x1000,
	SWITCHTEC_DLSTAT_ERROR_OFFSET = 0x1001,

	SWITCHTEC_DLSTAT_NO_FILE = 0x7d009,
};

/**
 * @brief Flag which indicates if a partition is read-only or not
 */
enum switchtec_fw_ro {
	SWITCHTEC_FW_RW = 0,
	SWITCHTEC_FW_RO = 1,
};

enum switchtec_fw_redundancy {
	SWITCHTEC_FW_REDUNDANCY_SET = 1,
	SWITCHTEC_FW_REDUNDANCY_CLEAR = 0,
};

int switchtec_fw_toggle_active_partition(struct switchtec_dev *dev,
					 int toggle_bl2, int toggle_key,
					 int toggle_fw, int toggle_cfg);
int switchtec_fw_setup_redundancy(struct switchtec_dev *dev,
				  enum switchtec_fw_redundancy redund,
				  enum switchtec_fw_type type);
int switchtec_fw_write_fd(struct switchtec_dev *dev, int img_fd,
			  int dont_activate, int force,
			  void (*progress_callback)(int cur, int tot));
int switchtec_fw_write_file(struct switchtec_dev *dev, FILE *fimg,
			    int dont_activate, int force,
			    void (*progress_callback)(int cur, int tot));
int switchtec_fw_read_fd(struct switchtec_dev *dev, int fd,
			 unsigned long addr, size_t len,
			 void (*progress_callback)(int cur, int tot));
int switchtec_fw_body_read_fd(struct switchtec_dev *dev, int fd,
			      struct switchtec_fw_image_info *info,
			      void (*progress_callback)(int cur, int tot));
int switchtec_fw_read(struct switchtec_dev *dev, unsigned long addr,
		      size_t len, void *buf);
void switchtec_fw_perror(const char *s, int ret);
int switchtec_fw_file_info(int fd, struct switchtec_fw_image_info *info);
int switchtec_fw_file_secure_version_newer(struct switchtec_dev *dev,
					   int img_fd);
const char *switchtec_fw_image_type(const struct switchtec_fw_image_info *info);
struct switchtec_fw_part_summary *
switchtec_fw_part_summary(struct switchtec_dev *dev);
void switchtec_fw_part_summary_free(struct switchtec_fw_part_summary *summary);
int switchtec_fw_img_write_hdr(int fd, struct switchtec_fw_image_info *info);
int switchtec_fw_is_boot_ro(struct switchtec_dev *dev);
int switchtec_fw_set_boot_ro(struct switchtec_dev *dev,
			     enum switchtec_fw_ro ro);
enum switchtec_gen switchtec_fw_version_to_gen(unsigned int version);
int switchtec_bind_info(struct switchtec_dev *dev,
			struct switchtec_bind_status_out *bind_status,
			int phy_port);
int switchtec_bind(struct switchtec_dev *dev, int par_id,
		   int log_port, int phy_port);
int switchtec_unbind(struct switchtec_dev *dev, int par_id, int log_port);
/********** EVENT COUNTER *********/

/**
 * @brief Event counter type mask (may be or-d together)
 */
enum switchtec_evcntr_type_mask {
	UNSUP_REQ_ERR = 1 << 0,		//!< Unsupported Request Error
	ECRC_ERR = 1 << 1,		//!< ECRC Error
	MALFORM_TLP_ERR = 1 << 2,	//!< Malformed TLP Error
	RCVR_OFLOW_ERR = 1 << 3,	//!< Receiver Overflow Error
	CMPLTR_ABORT_ERR = 1 << 4,	//!< Completer Abort Error
	POISONED_TLP_ERR = 1 << 5,	//!< Poisoned TLP Error
	SURPRISE_DOWN_ERR = 1 << 6,	//!< Surprise Down Error
	DATA_LINK_PROTO_ERR = 1 << 7,	//!< Data Link Protocol Error
	HDR_LOG_OFLOW_ERR = 1 << 8,	//!< Header Log Overflow Error
	UNCOR_INT_ERR = 1 << 9,		//!< Uncorrectable Internal Error
	REPLAY_TMR_TIMEOUT = 1 << 10,	//!< Replay Timer Timeout
	REPLAY_NUM_ROLLOVER = 1 << 11,	//!< Replay Number Rollover
	BAD_DLLP = 1 << 12,		//!< Bad DLLP
	BAD_TLP = 1 << 13,		//!< Bad TLP
	RCVR_ERR = 1 << 14,		//!< Receiver Error
	RCV_FATAL_MSG = 1 << 15,	//!< Receive FATAL Error Message
	RCV_NON_FATAL_MSG = 1 << 16,	//!< Receive Non-FATAL Error Message
	RCV_CORR_MSG = 1 << 17,		//!< Receive Correctable Error Message
	NAK_RCVD = 1 << 18,		//!< NAK Received
	RULE_TABLE_HIT = 1 << 19,	//!< Rule Search Table Rule Hit
	POSTED_TLP = 1 << 20,		//!< Posted TLP
	COMP_TLP = 1 << 21,		//!< Completion TLP
	NON_POSTED_TLP = 1 << 22,	//!< Non-Posted TLP

	/**
	 * @brief Mask indicating all possible errors
	 */
	ALL_ERRORS = (UNSUP_REQ_ERR | ECRC_ERR | MALFORM_TLP_ERR |
		      RCVR_OFLOW_ERR | CMPLTR_ABORT_ERR | POISONED_TLP_ERR |
		      SURPRISE_DOWN_ERR | DATA_LINK_PROTO_ERR |
		      HDR_LOG_OFLOW_ERR | UNCOR_INT_ERR |
		      REPLAY_TMR_TIMEOUT | REPLAY_NUM_ROLLOVER |
		      BAD_DLLP | BAD_TLP | RCVR_ERR | RCV_FATAL_MSG |
		      RCV_NON_FATAL_MSG | RCV_CORR_MSG | NAK_RCVD),
	/**
	 * @brief Mask indicating all TLP types
	 */
	ALL_TLPS = (POSTED_TLP | COMP_TLP | NON_POSTED_TLP),

	/**
	 * @brief Mask indicating all event types
	 */
	ALL = (1 << 23) - 1,
};

/**
 * @brief Null-terminated list of all event counter types with a
 *	name and help text.
 */
extern const struct switchtec_evcntr_type_list {
	enum switchtec_evcntr_type_mask mask;
	const char *name;
	const char *help;
} switchtec_evcntr_type_list[];

/**
 * @brief Structure used to setup an event counter
 */
struct switchtec_evcntr_setup {
	unsigned port_mask;	//<! Mask of ports this counter counts

	/** @brief Event counter types to count */
	enum switchtec_evcntr_type_mask type_mask;
	int egress;		//<! If 1, count egress, otherwise on ingress

	/**
	 * @brief Threshold to count to before generating an interrupt
	 * @see switchtec_evcntr_wait()
	 */
	unsigned threshold;
};

int switchtec_evcntr_type_count(void);
const char *switchtec_evcntr_type_str(int *type_mask);
int switchtec_evcntr_setup(struct switchtec_dev *dev, unsigned stack_id,
			   unsigned cntr_id,
			   struct switchtec_evcntr_setup *setup);
int switchtec_evcntr_get_setup(struct switchtec_dev *dev, unsigned stack_id,
			       unsigned cntr_id, unsigned nr_cntrs,
			       struct switchtec_evcntr_setup *res);
int switchtec_evcntr_get(struct switchtec_dev *dev, unsigned stack_id,
			 unsigned cntr_id, unsigned nr_cntrs, unsigned *res,
			 int clear);
int switchtec_evcntr_get_both(struct switchtec_dev *dev, unsigned stack_id,
			      unsigned cntr_id, unsigned nr_cntrs,
			      struct switchtec_evcntr_setup *setup,
			      unsigned *counts, int clear);
int switchtec_evcntr_wait(struct switchtec_dev *dev, int timeout_ms);

/********** BANDWIDTH COUNTER *********/

/**
 * @brief Bandwidth counter result struct
 */
struct switchtec_bwcntr_res {
	uint64_t time_us;		//!< Time (in microseconds)
	struct switchtec_bwcntr_dir {
		uint64_t posted;	//!< Posted TLP bytes
		uint64_t comp;		//!< Completion TLP bytes
		uint64_t nonposted;	//!< Non-Posted TLP bytes
	} egress, 			//!< Bandwidth out of the port
	  ingress;			//!< Bandwidth into the port
};

void switchtec_bwcntr_sub(struct switchtec_bwcntr_res *new_cntr,
			  struct switchtec_bwcntr_res *old_cntr);
int switchtec_bwcntr_set_many(struct switchtec_dev *dev, int nr_ports,
			      int * phys_port_ids,
			      enum switchtec_bw_type bw_type);
int switchtec_bwcntr_set_all(struct switchtec_dev *dev,
			     enum switchtec_bw_type bw_type);
int switchtec_bwcntr_many(struct switchtec_dev *dev, int nr_ports,
			  int *phys_port_ids, int clear,
			  struct switchtec_bwcntr_res *res);
int switchtec_bwcntr_all(struct switchtec_dev *dev, int clear,
			 struct switchtec_port_id **ports,
			 struct switchtec_bwcntr_res **res);
uint64_t switchtec_bwcntr_tot(struct switchtec_bwcntr_dir *d);

/********** LATENCY COUNTER *********/

#define SWITCHTEC_LAT_ALL_INGRESS 63

int switchtec_lat_setup_many(struct switchtec_dev *dev, int nr_ports,
			     int *egress_port_ids, int *ingress_port_ids);
int switchtec_lat_setup(struct switchtec_dev *dev, int egress_port_id,
			int ingress_port_id, int clear);
int switchtec_lat_get_many(struct switchtec_dev *dev, int nr_ports,
			   int clear, int *egress_port_ids,
			   int *cur_ns, int *max_ns);
int switchtec_lat_get(struct switchtec_dev *dev, int clear,
		      int egress_port_ids, int *cur_ns,
		      int *max_ns);

/********** GLOBAL ADDRESS SPACE ACCESS *********/

/*
 * GAS map maps the hardware registers into user memory space.
 * Needless to say, this can be very dangerous and should only
 * be done if you know what you are doing. Any register accesses
 * that use this will remain unsupported by Microsemi unless it's
 * done within the switchtec user project or otherwise specified.
 */

gasptr_t switchtec_gas_map(struct switchtec_dev *dev, int writeable,
                           size_t *map_size);
void switchtec_gas_unmap(struct switchtec_dev *dev, gasptr_t map);

/********** DIAGNOSTIC FUNCTIONS *********/

#define SWITCHTEC_DIAG_CROSS_HAIR_ALL_LANES -1
#define SWITCHTEC_DIAG_CROSS_HAIR_MAX_LANES 64

enum switchtec_diag_cross_hair_state {
	SWITCHTEC_DIAG_CROSS_HAIR_DISABLED = 0,
	SWITCHTEC_DIAG_CROSS_HAIR_RESVD,
	SWITCHTEC_DIAG_CROSS_HAIR_WAITING,
	SWITCHTEC_DIAG_CROSS_HAIR_FIRST_ERROR_RIGHT,
	SWITCHTEC_DIAG_CROSS_HAIR_ERROR_FREE_RIGHT,
	SWITCHTEC_DIAG_CROSS_HAIR_FINAL_RIGHT,
	SWITCHTEC_DIAG_CROSS_HAIR_FIRST_ERROR_LEFT,
	SWITCHTEC_DIAG_CROSS_HAIR_ERROR_FREE_LEFT,
	SWITCHTEC_DIAG_CROSS_HAIR_FINAL_LEFT,
	SWITCHTEC_DIAG_CROSS_HAIR_FIRST_ERROR_TOP_RIGHT,
	SWITCHTEC_DIAG_CROSS_HAIR_ERROR_FREE_TOP_RIGHT,
	SWITCHTEC_DIAG_CROSS_HAIR_FINAL_TOP_RIGHT,
	SWITCHTEC_DIAG_CROSS_HAIR_FIRST_ERROR_BOT_RIGHT,
	SWITCHTEC_DIAG_CROSS_HAIR_ERROR_FREE_BOT_RIGHT,
	SWITCHTEC_DIAG_CROSS_HAIR_FINAL_BOT_RIGHT,
	SWITCHTEC_DIAG_CROSS_HAIR_FIRST_ERROR_TOP_LEFT,
	SWITCHTEC_DIAG_CROSS_HAIR_ERROR_FREE_TOP_LEFT,
	SWITCHTEC_DIAG_CROSS_HAIR_FINAL_TOP_LEFT,
	SWITCHTEC_DIAG_CROSS_HAIR_FIRST_ERROR_BOT_LEFT,
	SWITCHTEC_DIAG_CROSS_HAIR_ERROR_FREE_BOT_LEFT,
	SWITCHTEC_DIAG_CROSS_HAIR_FINAL_BOT_LEFT,
	SWITCHTEC_DIAG_CROSS_HAIR_DONE,
	SWITCHTEC_DIAG_CROSS_HAIR_ERROR,
};

struct switchtec_diag_cross_hair {
	enum switchtec_diag_cross_hair_state state;
	int lane_id;

	union {
		struct {
			/* Valid when state is Error */
			int prev_state;
			int x_pos;
			int y_pos;
		};
		/* Valid when state is DONE */
		struct {
			int eye_left_lim;
			int eye_right_lim;
			int eye_bot_left_lim;
			int eye_bot_right_lim;
			int eye_top_left_lim;
			int eye_top_right_lim;
		};
	};
};

struct switchtec_rcvr_obj {
	int port_id;
	int lane_id;
	int ctle;
	int target_amplitude;
	int speculative_dfe;
	int dynamic_dfe[7];
};

struct switchtec_port_eq_coeff {
	int lane_cnt;
	struct {
		int pre;
		int post;
	} cursors[16];
};

struct switchtec_port_eq_table {
	int lane_id;
	int step_cnt;

	struct {
		int pre_cursor;
		int post_cursor;
		int fom;
		int pre_cursor_up;
		int post_cursor_up;
		int error_status;
		int active_status;
		int speed;
	} steps[126];
};

struct switchtec_port_eq_tx_fslf {
	int fs;
	int lf;
};

struct switchtec_rcvr_ext {
	int ctle2_rx_mode;
	int dtclk_5;
	int dtclk_8_6;
	int dtclk_9;
};

struct switchtec_mrpc {
	const char *tag;
	const char *desc;
	bool reserved;
};

enum switchtec_diag_eye_data_mode {
	SWITCHTEC_DIAG_EYE_RAW,
	SWITCHTEC_DIAG_EYE_RATIO,
};

enum switchtec_diag_loopback_enable {
	SWITCHTEC_DIAG_LOOPBACK_RX_TO_TX = 1 << 0,
	SWITCHTEC_DIAG_LOOPBACK_TX_TO_RX = 1 << 1,
	SWITCHTEC_DIAG_LOOPBACK_LTSSM = 1 << 2,
};

enum switchtec_diag_pattern {
	SWITCHTEC_DIAG_PATTERN_PRBS_7,
	SWITCHTEC_DIAG_PATTERN_PRBS_11,
	SWITCHTEC_DIAG_PATTERN_PRBS_23,
	SWITCHTEC_DIAG_PATTERN_PRBS_31,
	SWITCHTEC_DIAG_PATTERN_PRBS_9,
	SWITCHTEC_DIAG_PATTERN_PRBS_15,
	SWITCHTEC_DIAG_PATTERN_PRBS_DISABLED,
};

enum switchtec_diag_ltssm_speed {
	SWITCHTEC_DIAG_LTSSM_GEN1 = 0,
	SWITCHTEC_DIAG_LTSSM_GEN2 = 1,
	SWITCHTEC_DIAG_LTSSM_GEN3 = 2,
	SWITCHTEC_DIAG_LTSSM_GEN4 = 3,
};

enum switchtec_diag_end {
	SWITCHTEC_DIAG_LOCAL,
	SWITCHTEC_DIAG_FAR_END,
};

enum switchtec_diag_link {
	SWITCHTEC_DIAG_LINK_CURRENT,
	SWITCHTEC_DIAG_LINK_PREVIOUS,
};

int switchtec_diag_cross_hair_enable(struct switchtec_dev *dev, int lane_id);
int switchtec_diag_cross_hair_disable(struct switchtec_dev *dev);
int switchtec_diag_cross_hair_get(struct switchtec_dev *dev, int start_lane_id,
		int num_lanes, struct switchtec_diag_cross_hair *res);

int switchtec_diag_eye_set_mode(struct switchtec_dev *dev,
				enum switchtec_diag_eye_data_mode mode);
int switchtec_diag_eye_start(struct switchtec_dev *dev, int lane_mask[4],
			     struct range *x_range, struct range *y_range,
			     int step_interval);
int switchtec_diag_eye_fetch(struct switchtec_dev *dev, double *pixels,
			     size_t pixel_cnt, int *lane_id);
int switchtec_diag_eye_cancel(struct switchtec_dev *dev);

int switchtec_diag_loopback_set(struct switchtec_dev *dev, int port_id,
		int enable, enum switchtec_diag_ltssm_speed ltssm_speed);
int switchtec_diag_loopback_get(struct switchtec_dev *dev, int port_id,
		int *enabled, enum switchtec_diag_ltssm_speed *ltssm_speed);
int switchtec_diag_pattern_gen_set(struct switchtec_dev *dev, int port_id,
		enum switchtec_diag_pattern type);
int switchtec_diag_pattern_gen_get(struct switchtec_dev *dev, int port_id,
		enum switchtec_diag_pattern *type);
int switchtec_diag_pattern_mon_set(struct switchtec_dev *dev, int port_id,
		enum switchtec_diag_pattern type);
int switchtec_diag_pattern_mon_get(struct switchtec_dev *dev, int port_id,
		int lane_id, enum switchtec_diag_pattern *type,
		unsigned long long *err_cnt);
int switchtec_diag_pattern_inject(struct switchtec_dev *dev, int port_id,
				  unsigned int err_cnt);

int switchtec_diag_rcvr_obj(struct switchtec_dev *dev, int port_id,
		int lane_id, enum switchtec_diag_link link,
		struct switchtec_rcvr_obj *res);
int switchtec_diag_rcvr_ext(struct switchtec_dev *dev, int port_id,
			    int lane_id, enum switchtec_diag_link link,
			    struct switchtec_rcvr_ext *res);

int switchtec_diag_port_eq_tx_coeff(struct switchtec_dev *dev, int port_id,
		enum switchtec_diag_end end, enum switchtec_diag_link link,
		struct switchtec_port_eq_coeff *res);
int switchtec_diag_port_eq_tx_table(struct switchtec_dev *dev, int port_id,
				    enum switchtec_diag_link link,
				    struct switchtec_port_eq_table *res);
int switchtec_diag_port_eq_tx_fslf(struct switchtec_dev *dev, int port_id,
				 int lane_id, enum switchtec_diag_end end,
				 enum switchtec_diag_link link,
				 struct switchtec_port_eq_tx_fslf *res);

int switchtec_diag_perm_table(struct switchtec_dev *dev,
			      struct switchtec_mrpc table[MRPC_MAX_ID]);
int switchtec_diag_refclk_ctl(struct switchtec_dev *dev, int stack_id, bool en);

#ifdef __cplusplus
}
#endif

#endif
