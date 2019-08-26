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

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

struct switchtec_dev;

#define SWITCHTEC_MAX_PARTS  48
#define SWITCHTEC_MAX_PORTS  48
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
	SWITCHTEC_GEN_UNKNOWN,
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
};

/**
 * @brief Raw Gen3 Flash partition id.
 *
 * Avoid using this directly
 */

enum switchtec_fw_partition_id_gen3 {
	SWITCHTEC_FW_PART_ID_BOOT_GEN3 = 0x0,
	SWITCHTEC_FW_PART_ID_MAP0_GEN3 = 0x1,
	SWITCHTEC_FW_PART_ID_MAP1_GEN3 = 0x2,
	SWITCHTEC_FW_PART_ID_IMG0_GEN3 = 0x3,
	SWITCHTEC_FW_PART_ID_CFG0_GEN3 = 0x4,
	SWITCHTEC_FW_PART_ID_CFG1_GEN3 = 0x5,
	SWITCHTEC_FW_PART_ID_NVLOG_GEN3 = 0x6,
	SWITCHTEC_FW_PART_ID_IMG1_GEN3 = 0x7,
};

/**
 * @brief Raw Gen4 Flash partition id.
 *
 * Avoid using this directly
 */
enum switchtec_fw_partition_id_gen4 {
	SWITCHTEC_FW_PART_ID_MAP0_GEN4 = 0x0,
	SWITCHTEC_FW_PART_ID_MAP1_GEN4 = 0x1,
	SWITCHTEC_FW_PART_ID_KEYMAN0_GEN4 = 0x2,
	SWITCHTEC_FW_PART_ID_KEYMAN1_GEN4 = 0x3,
	SWITCHTEC_FW_PART_ID_BL20_GEN4 = 0x4,
	SWITCHTEC_FW_PART_ID_BL21_GEN4 = 0x5,
	SWITCHTEC_FW_PART_ID_CFG0_GEN4 = 0x6,
	SWITCHTEC_FW_PART_ID_CFG1_GEN4 = 0x7,
	SWITCHTEC_FW_PART_ID_IMG0_GEN4 = 0x8,
	SWITCHTEC_FW_PART_ID_IMG1_GEN4 = 0x9,
	SWITCHTEC_FW_PART_ID_NVLOG_GEN4 = 0xa,
};

/**
 * @brief Flash partition id.
 *
 */
enum switchtec_fw_partition_id {
	SWITCHTEC_FW_PART_ID_BOOT = SWITCHTEC_FW_PART_ID_BOOT_GEN3,
	SWITCHTEC_FW_PART_ID_MAP0 = SWITCHTEC_FW_PART_ID_MAP0_GEN4 << 8 |
				    SWITCHTEC_FW_PART_ID_MAP0_GEN3,
	SWITCHTEC_FW_PART_ID_MAP1 = SWITCHTEC_FW_PART_ID_MAP1_GEN4 << 8 |
				    SWITCHTEC_FW_PART_ID_MAP1_GEN3,
	SWITCHTEC_FW_PART_ID_KEYMAN0 = SWITCHTEC_FW_PART_ID_KEYMAN0_GEN4 << 8,
	SWITCHTEC_FW_PART_ID_KEYMAN1 = SWITCHTEC_FW_PART_ID_KEYMAN1_GEN4 << 8,
	SWITCHTEC_FW_PART_ID_BL20 = SWITCHTEC_FW_PART_ID_BL20_GEN4 << 8,
	SWITCHTEC_FW_PART_ID_BL21 = SWITCHTEC_FW_PART_ID_BL21_GEN4 << 8,
	SWITCHTEC_FW_PART_ID_CFG0 = SWITCHTEC_FW_PART_ID_CFG0_GEN4 << 8 |
				    SWITCHTEC_FW_PART_ID_CFG0_GEN3,
	SWITCHTEC_FW_PART_ID_CFG1 = SWITCHTEC_FW_PART_ID_CFG1_GEN4 << 8 |
				    SWITCHTEC_FW_PART_ID_CFG1_GEN3,
	SWITCHTEC_FW_PART_ID_IMG0 = SWITCHTEC_FW_PART_ID_IMG0_GEN4 << 8 |
				    SWITCHTEC_FW_PART_ID_IMG0_GEN3,
	SWITCHTEC_FW_PART_ID_IMG1 = SWITCHTEC_FW_PART_ID_IMG1_GEN4 << 8 |
				    SWITCHTEC_FW_PART_ID_IMG1_GEN3,
	SWITCHTEC_FW_PART_ID_NVLOG = SWITCHTEC_FW_PART_ID_NVLOG_GEN4 << 8 |
				     SWITCHTEC_FW_PART_ID_NVLOG_GEN3,
};

#define GEN3_FW_PART_ID(t) (t & 0xff)
#define GEN4_FW_PART_ID(t) ((t & 0xff00) >> 8)

/**
 * @brief Gen3 FW partition type.
 *
 * Avoid using this directly
 */
enum switchtec_fw_part_type_gen3 {
	SWITCHTEC_FW_PART_TYPE_BOOT_GEN3 = SWITCHTEC_FW_PART_ID_BOOT_GEN3,
	SWITCHTEC_FW_PART_TYPE_MAP_GEN3 = SWITCHTEC_FW_PART_ID_MAP0_GEN3,
	SWITCHTEC_FW_PART_TYPE_CFG_GEN3 = SWITCHTEC_FW_PART_ID_CFG0_GEN3,
	SWITCHTEC_FW_PART_TYPE_IMG_GEN3 = SWITCHTEC_FW_PART_ID_IMG0_GEN3,
	SWITCHTEC_FW_PART_TYPE_NVLOG_GEN3 = SWITCHTEC_FW_PART_ID_NVLOG_GEN3,
	SWITCHTEC_FW_PART_TYPE_SEEPROM_GEN3 = 0xFE,
};

/**
 * @brief Gen4 FW partition type.
 *
 * Avoid using this directly
 */
enum switchtec_fw_part_type_gen4 {
	SWITCHTEC_FW_PART_TYPE_MAP_GEN4 = 0x0,
	SWITCHTEC_FW_PART_TYPE_KEYMAN_GEN4 = 0x1,
	SWITCHTEC_FW_PART_TYPE_BL2_GEN4 = 0x2,
	SWITCHTEC_FW_PART_TYPE_CFG_GEN4 = 0x3,
	SWITCHTEC_FW_PART_TYPE_IMG_GEN4 = 0x4,
	SWITCHTEC_FW_PART_TYPE_NVLOG_GEN4 = 0x5,
	SWITCHTEC_FW_PART_TYPE_SEEPROM_GEN4 = 0xFE,
};

/**
 * @brief FW partition type.
 *
 */
enum switchtec_fw_partition_type {
	SWITCHTEC_FW_PART_TYPE_MAP = (SWITCHTEC_FW_PART_TYPE_MAP_GEN4 << 8 |
				     SWITCHTEC_FW_PART_TYPE_MAP_GEN3),
	SWITCHTEC_FW_PART_TYPE_BOOT = SWITCHTEC_FW_PART_TYPE_BOOT_GEN3,
	SWITCHTEC_FW_PART_TYPE_KEYMAN = SWITCHTEC_FW_PART_TYPE_KEYMAN_GEN4 << 8,
	SWITCHTEC_FW_PART_TYPE_BL2 = SWITCHTEC_FW_PART_TYPE_BL2_GEN4 << 8,
	SWITCHTEC_FW_PART_TYPE_CFG = (SWITCHTEC_FW_PART_TYPE_CFG_GEN4 << 8 |
				     SWITCHTEC_FW_PART_TYPE_CFG_GEN3),
	SWITCHTEC_FW_PART_TYPE_IMG = (SWITCHTEC_FW_PART_TYPE_IMG_GEN4 << 8 |
				     SWITCHTEC_FW_PART_TYPE_IMG_GEN3),
	SWITCHTEC_FW_PART_TYPE_NVLOG = (SWITCHTEC_FW_PART_TYPE_NVLOG_GEN4 << 8 |
				       SWITCHTEC_FW_PART_TYPE_NVLOG_GEN3),
	SWITCHTEC_FW_PART_TYPE_SEEPROM =
		(SWITCHTEC_FW_PART_TYPE_SEEPROM_GEN4 << 8 |
		SWITCHTEC_FW_PART_TYPE_SEEPROM_GEN3),
	SWITCHTEC_FW_PART_TYPE_UNKNOWN,
};

#define GEN3_FW_PART_TYPE(t) (t & 0xff)
#define GEN4_FW_PART_TYPE(t) ((t & 0xff00) >> 8)

struct switchtec_fw_partition_info {
	enum switchtec_fw_partition_id part_id;
	enum switchtec_fw_partition_type type;
	uint32_t version;
	char ver_str[16];
	size_t part_addr;
	size_t part_len;
	size_t image_len;
	unsigned long image_crc;
	int valid;
	int active;
	int running;
	int readonly;
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

struct switchtec_fw_partition_info;

struct switchtec_dev *switchtec_open(const char *device);
struct switchtec_dev *switchtec_open_by_path(const char *path);
struct switchtec_dev *switchtec_open_by_index(int index);
struct switchtec_dev *switchtec_open_by_pci_addr(int domain, int bus,
						 int device, int func);
struct switchtec_dev *switchtec_open_i2c(const char *path, int i2c_addr);
struct switchtec_dev *switchtec_open_i2c_by_adapter(int adapter, int i2c_addr);
struct switchtec_dev *switchtec_open_uart(int fd);

void switchtec_close(struct switchtec_dev *dev);
int switchtec_list(struct switchtec_device_info **devlist);
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
int switchtec_flash_part(struct switchtec_dev *dev,
			 struct switchtec_fw_partition_info *info,
			 enum switchtec_fw_partition_id part);
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
int switchtec_set_pax_id(struct switchtec_dev *dev, int pax_id);
int switchtec_echo(struct switchtec_dev *dev, uint32_t input, uint32_t *output);
int switchtec_hard_reset(struct switchtec_dev *dev);
int switchtec_status(struct switchtec_dev *dev,
		     struct switchtec_status **status);
void switchtec_status_free(struct switchtec_status *status, int ports);

const char *switchtec_strerror(void);
void switchtec_perror(const char *str);
int switchtec_log_to_file(struct switchtec_dev *dev,
			  enum switchtec_log_type type,
			  int fd);
float switchtec_die_temp(struct switchtec_dev *dev);

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
 * @brief Return whether a Switchtec device is PFX(L/I).
 */
static inline int switchtec_is_pfx_all(struct switchtec_dev *dev)
{
	return switchtec_is_pfx(dev) ||
	       switchtec_is_pfxl(dev) ||
	       switchtec_is_pfxi(dev);
}

/**
 * @brief Return whether a Switchtec device is PSX.
 */
static inline int switchtec_is_psx(struct switchtec_dev *dev)
{
	return switchtec_variant(dev) == SWITCHTEC_PSX;
}

/**
 * @brief Return whether a Switchtec device is PFX or PSX.
 */
static inline int switchtec_is_psx_pfx_all(struct switchtec_dev *dev)
{
	return switchtec_is_psx(dev) || switchtec_is_pfx_all(dev);
}

/**
 * @brief Return whether a Switchtec device is PAX.
 */
static inline int switchtec_is_pax(struct switchtec_dev *dev)
{
	return switchtec_variant(dev) == SWITCHTEC_PAX;
}

/**
 * @brief Return the generation string of a Switchtec device.
 */
static inline const char *switchtec_gen_str(struct switchtec_dev *dev)
{
	const char *str;

	str =  switchtec_is_gen3(dev) ? "GEN3" :
	       switchtec_is_gen4(dev) ? "GEN4" : "Unknown";

	return str;
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
	      switchtec_is_pax(dev) ? "PAX" : "Unknown";

	return str;
}

/** @brief Number of GT/s capable for each PCI generation or \p link_rate */
static const float switchtec_gen_transfers[] = {0, 2.5, 5, 8, 16};
/** @brief Number of GB/s capable for each PCI generation or \p link_rate */
static const float switchtec_gen_datarate[] = {0, 250, 500, 985, 1969};

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
	SWITCHTEC_DLSTAT_READY = 0,
	SWITCHTEC_DLSTAT_INPROGRESS = 1,
	SWITCHTEC_DLSTAT_HEADER_INCORRECT = 2,
	SWITCHTEC_DLSTAT_OFFSET_INCORRECT = 3,
	SWITCHTEC_DLSTAT_CRC_INCORRECT = 4,
	SWITCHTEC_DLSTAT_LENGTH_INCORRECT = 5,
	SWITCHTEC_DLSTAT_HARDWARE_ERR = 6,
	SWITCHTEC_DLSTAT_COMPLETES = 7,
	SWITCHTEC_DLSTAT_SUCCESS_FIRM_ACT = 8,
	SWITCHTEC_DLSTAT_SUCCESS_DATA_ACT = 9,
	SWITCHTEC_DLSTAT_DOWNLOAD_TIMEOUT = 14,
};

/**
 * @brief Flag which indicates if a partition is read-only or not
 */
enum switchtec_fw_ro {
	SWITCHTEC_FW_RW = 0,
	SWITCHTEC_FW_RO = 1,
};

/**
 * @brief Flags which indicates if a partition is active or running.
 */
enum switchtec_fw_active_flags {
	SWITCHTEC_FW_PART_ACTIVE = 1,
	SWITCHTEC_FW_PART_RUNNING = 2,
};

/**
 * @brief Get whether a firmware partition is active.
 *
 * An active partition implies that it will be used the next
 * time the switch is rebooted.
 */
static inline int switchtec_fw_active(struct switchtec_fw_partition_info *inf)
{
	return inf->active;
}

/**
 * @brief Get whether a firmware partition is active.
 *
 * An active partition implies that it will be used the next
 * time the switch is rebooted.
 */
static inline int switchtec_fw_running(struct switchtec_fw_partition_info *inf)
{
	return inf->running;
}

struct switchtec_fw_metadata {
	char magic[4];
	char sub_magic[4];
	uint32_t image_len;
	uint32_t type;
	uint32_t load_addr;
	uint32_t version;
	uint32_t secure_version;
	uint32_t sequence;
	char date_str[8];
	char time_str[8];
	char img_str[16];
	uint8_t public_key_modulus[512];
	uint8_t public_key_exponent[4];
	uint8_t uart_port;
	uint8_t uart_rate;
	uint8_t bist_enable;
	uint8_t bist_gpio_pin_cfg;
	uint8_t bist_gpio_level_cfg;
	uint32_t xml_version;
	uint32_t relocatable_img_len;
	uint32_t link_addr;
	uint32_t header_crc;
	uint32_t image_crc;
};

int switchtec_fw_dlstatus(struct switchtec_dev *dev,
			  enum switchtec_fw_dlstatus *status,
			  enum mrpc_bg_status *bgstatus);
int switchtec_fw_wait(struct switchtec_dev *dev,
		      enum switchtec_fw_dlstatus *status);
int switchtec_fw_toggle_active_partition(struct switchtec_dev *dev,
					 int toggle_bl2, int toggle_keyman,
					 int toggle_fw, int toggle_cfg);
int switchtec_fw_write_fd(struct switchtec_dev *dev, int img_fd,
			  int dont_activate, int force,
			  void (*progress_callback)(int cur, int tot));
int switchtec_fw_write_file(struct switchtec_dev *dev, FILE *fimg,
			    int dont_activate, int force,
			    void (*progress_callback)(int cur, int tot));
int switchtec_fw_read_fd(struct switchtec_dev *dev, int fd,
			 unsigned long addr, size_t len,
			 void (*progress_callback)(int cur, int tot));
int switchtec_fw_read(struct switchtec_dev *dev, unsigned long addr,
		      size_t len, void *buf);
int switchtec_fw_partition_info(struct switchtec_dev *dev,
				enum switchtec_fw_partition_id id,
				struct switchtec_fw_partition_info *info);
int switchtec_fw_get_multicfg(struct switchtec_dev *dev,
			      struct switchtec_fw_partition_info *info,
			      int *nr_mult);
int switchtec_fw_read_metadata(struct switchtec_dev *dev,
			       enum switchtec_fw_partition_id part_id,
			       struct switchtec_fw_metadata *meta,
			       char *version, size_t version_len);
void switchtec_fw_perror(const char *s, int ret);
int switchtec_fw_image_file_info(int fd,
				 struct switchtec_fw_partition_info *info);
const char *switchtec_fw_part_type(
		const struct switchtec_fw_partition_info *info);
int switchtec_fw_img_file_write_hdr(int fd, struct switchtec_fw_metadata *meta,
				    enum switchtec_fw_partition_type type);
int switchtec_fw_is_boot_ro(struct switchtec_dev *dev);
int switchtec_fw_set_boot_ro(struct switchtec_dev *dev,
			     enum switchtec_fw_ro ro);
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

#ifdef __cplusplus
}
#endif

#endif
