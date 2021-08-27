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

/**
 * @file
 * @brief Switchtec core library functions for basic device operations
 */

#define SWITCHTEC_LIB_CORE

#include "switchtec_priv.h"

#include "switchtec/switchtec.h"
#include "switchtec/mrpc.h"
#include "switchtec/errors.h"
#include "switchtec/log.h"
#include "switchtec/endian.h"
#include "switchtec/utils.h"

#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

/**
 * @defgroup Device Switchtec Management
 * @brief Functions to list, open and perform basic operations on Switchtec devices
 *
 * switchtec_list() may be used to list all the devices in the system. The
 * devices may then be opened using switchtec_open(). There are a number of
 * other functions to open devices by more specific information but
 * switchtec_open() is prefered and covers all cases.
 *
 * MRPC commands may be submitted to an open switch handle with switchtec_cmd() and
 * port status information may be retrieved with switchtec_status().
 * @{
 */

/**
 * @brief Module-specific log definitions
 */
struct module_log_defs {
	char *mod_name;		//!< module name
	char **entries;		//!< log entry array
	int num_entries;	//!< number of log entries
};

/**
 * @brief Log definitions for all modules
 */
struct log_defs {
	struct module_log_defs *module_defs;	//!< per-module log definitions
	int num_alloc;				//!< number of modules allocated
};

/**
 * @brief Switchtec device id to generation/variant mapping
 */
struct switchtec_device_id {
	unsigned short device_id;
	enum switchtec_gen gen;
	enum switchtec_variant var;
};

/**
 * @brief Supported Switchtec device id table
 */
static const struct switchtec_device_id switchtec_device_id_tbl[] = {
	{0x8531, SWITCHTEC_GEN3, SWITCHTEC_PFX},   //PFX 24xG3
	{0x8532, SWITCHTEC_GEN3, SWITCHTEC_PFX},   //PFX 32xG3
	{0x8533, SWITCHTEC_GEN3, SWITCHTEC_PFX},   //PFX 48xG3
	{0x8534, SWITCHTEC_GEN3, SWITCHTEC_PFX},   //PFX 64xG3
	{0x8535, SWITCHTEC_GEN3, SWITCHTEC_PFX},   //PFX 80xG3
	{0x8536, SWITCHTEC_GEN3, SWITCHTEC_PFX},   //PFX 96xG3
	{0x8541, SWITCHTEC_GEN3, SWITCHTEC_PSX},   //PSX 24xG3
	{0x8542, SWITCHTEC_GEN3, SWITCHTEC_PSX},   //PSX 32xG3
	{0x8543, SWITCHTEC_GEN3, SWITCHTEC_PSX},   //PSX 48xG3
	{0x8544, SWITCHTEC_GEN3, SWITCHTEC_PSX},   //PSX 64xG3
	{0x8545, SWITCHTEC_GEN3, SWITCHTEC_PSX},   //PSX 80xG3
	{0x8546, SWITCHTEC_GEN3, SWITCHTEC_PSX},   //PSX 96xG3
	{0x8551, SWITCHTEC_GEN3, SWITCHTEC_PAX},   //PAX 24XG3
	{0x8552, SWITCHTEC_GEN3, SWITCHTEC_PAX},   //PAX 32XG3
	{0x8553, SWITCHTEC_GEN3, SWITCHTEC_PAX},   //PAX 48XG3
	{0x8554, SWITCHTEC_GEN3, SWITCHTEC_PAX},   //PAX 64XG3
	{0x8555, SWITCHTEC_GEN3, SWITCHTEC_PAX},   //PAX 80XG3
	{0x8556, SWITCHTEC_GEN3, SWITCHTEC_PAX},   //PAX 96XG3
	{0x8561, SWITCHTEC_GEN3, SWITCHTEC_PFXL},  //PFXL 24XG3
	{0x8562, SWITCHTEC_GEN3, SWITCHTEC_PFXL},  //PFXL 32XG3
	{0x8563, SWITCHTEC_GEN3, SWITCHTEC_PFXL},  //PFXL 48XG3
	{0x8564, SWITCHTEC_GEN3, SWITCHTEC_PFXL},  //PFXL 64XG3
	{0x8565, SWITCHTEC_GEN3, SWITCHTEC_PFXL},  //PFXL 80XG3
	{0x8566, SWITCHTEC_GEN3, SWITCHTEC_PFXL},  //PFXL 96XG3
	{0x8571, SWITCHTEC_GEN3, SWITCHTEC_PFXI},  //PFXI 24XG3
	{0x8572, SWITCHTEC_GEN3, SWITCHTEC_PFXI},  //PFXI 32XG3
	{0x8573, SWITCHTEC_GEN3, SWITCHTEC_PFXI},  //PFXI 48XG3
	{0x8574, SWITCHTEC_GEN3, SWITCHTEC_PFXI},  //PFXI 64XG3
	{0x8575, SWITCHTEC_GEN3, SWITCHTEC_PFXI},  //PFXI 80XG3
	{0x8576, SWITCHTEC_GEN3, SWITCHTEC_PFXI},  //PFXI 96XG3
	{0x4000, SWITCHTEC_GEN4, SWITCHTEC_PFX},   //PFX 100XG4
	{0x4084, SWITCHTEC_GEN4, SWITCHTEC_PFX},   //PFX 84XG4
	{0x4068, SWITCHTEC_GEN4, SWITCHTEC_PFX},   //PFX 68XG4
	{0x4052, SWITCHTEC_GEN4, SWITCHTEC_PFX},   //PFX 52XG4
	{0x4036, SWITCHTEC_GEN4, SWITCHTEC_PFX},   //PFX 36XG4
	{0x4028, SWITCHTEC_GEN4, SWITCHTEC_PFX},   //PFX 28XG4
	{0x4100, SWITCHTEC_GEN4, SWITCHTEC_PSX},   //PSX 100XG4
	{0x4184, SWITCHTEC_GEN4, SWITCHTEC_PSX},   //PSX 84XG4
	{0x4168, SWITCHTEC_GEN4, SWITCHTEC_PSX},   //PSX 68XG4
	{0x4152, SWITCHTEC_GEN4, SWITCHTEC_PSX},   //PSX 52XG4
	{0x4136, SWITCHTEC_GEN4, SWITCHTEC_PSX},   //PSX 36XG4
	{0x4128, SWITCHTEC_GEN4, SWITCHTEC_PSX},   //PSX 28XG4
	{0x4200, SWITCHTEC_GEN4, SWITCHTEC_PAX},   //PAX 100XG4
	{0x4284, SWITCHTEC_GEN4, SWITCHTEC_PAX},   //PAX 84XG4
	{0x4268, SWITCHTEC_GEN4, SWITCHTEC_PAX},   //PAX 68XG4
	{0x4252, SWITCHTEC_GEN4, SWITCHTEC_PAX},   //PAX 52XG4
	{0x4236, SWITCHTEC_GEN4, SWITCHTEC_PAX},   //PAX 36XG4
	{0x4352, SWITCHTEC_GEN4, SWITCHTEC_PFXA},  //PFXA 52XG4
	{0x4336, SWITCHTEC_GEN4, SWITCHTEC_PFXA},  //PFXA 36XG4
	{0x4328, SWITCHTEC_GEN4, SWITCHTEC_PFXA},  //PFXA 28XG4
	{0x4452, SWITCHTEC_GEN4, SWITCHTEC_PSXA},  //PSXA 52XG4
	{0x4436, SWITCHTEC_GEN4, SWITCHTEC_PSXA},  //PSXA 36XG4
	{0x4428, SWITCHTEC_GEN4, SWITCHTEC_PSXA},  //PSXA 28XG4
	{0x4552, SWITCHTEC_GEN4, SWITCHTEC_PAXA},  //PAXA 52XG4
	{0x4536, SWITCHTEC_GEN4, SWITCHTEC_PAXA},  //PAXA 36XG4
	{0x4528, SWITCHTEC_GEN4, SWITCHTEC_PAXA},  //PAXA 28XG4
	{0x4228, SWITCHTEC_GEN4, SWITCHTEC_PAX},   //PAX 28XG4
	{0x5000, SWITCHTEC_GEN5, SWITCHTEC_PFX},   //PFX 100XG5
	{0x5084, SWITCHTEC_GEN5, SWITCHTEC_PFX},   //PFX 84XG5
	{0x5068, SWITCHTEC_GEN5, SWITCHTEC_PFX},   //PFX 68XG5
	{0x5052, SWITCHTEC_GEN5, SWITCHTEC_PFX},   //PFX 52XG5
	{0x5036, SWITCHTEC_GEN5, SWITCHTEC_PFX},   //PFX 36XG5
	{0x5028, SWITCHTEC_GEN5, SWITCHTEC_PFX},   //PFX 28XG5
	{0x5100, SWITCHTEC_GEN5, SWITCHTEC_PSX},   //PSX 100XG5
	{0x5184, SWITCHTEC_GEN5, SWITCHTEC_PSX},   //PSX 84XG5
	{0x5168, SWITCHTEC_GEN5, SWITCHTEC_PSX},   //PSX 68XG5
	{0x5152, SWITCHTEC_GEN5, SWITCHTEC_PSX},   //PSX 52XG5
	{0x5136, SWITCHTEC_GEN5, SWITCHTEC_PSX},   //PSX 36XG5
	{0x5128, SWITCHTEC_GEN5, SWITCHTEC_PSX},   //PSX 28XG5
	{0x5200, SWITCHTEC_GEN5, SWITCHTEC_PAX},   //PAX 100XG5
	{0x5284, SWITCHTEC_GEN5, SWITCHTEC_PAX},   //PAX 84XG5
	{0x5268, SWITCHTEC_GEN5, SWITCHTEC_PAX},   //PAX 68XG5
	{0x5252, SWITCHTEC_GEN5, SWITCHTEC_PAX},   //PAX 52XG5
	{0x5236, SWITCHTEC_GEN5, SWITCHTEC_PAX},   //PAX 36XG5
	{0x5228, SWITCHTEC_GEN5, SWITCHTEC_PAX},   //PAX 28XG5
	{0x5300, SWITCHTEC_GEN5, SWITCHTEC_PAXA},  //PAX-A 100XG5
	{0x5384, SWITCHTEC_GEN5, SWITCHTEC_PAXA},  //PAX-A 84XG5
	{0x5368, SWITCHTEC_GEN5, SWITCHTEC_PAXA},  //PAX-A 68XG5
	{0x5352, SWITCHTEC_GEN5, SWITCHTEC_PAXA},  //PAX-A 52XG5
	{0x5336, SWITCHTEC_GEN5, SWITCHTEC_PAXA},  //PAX-A 36XG5
	{0x5328, SWITCHTEC_GEN5, SWITCHTEC_PAXA},  //PAX-A 28XG5
	{0x5400, SWITCHTEC_GEN5, SWITCHTEC_PFXA},  //PFX-A 100XG5
	{0x5484, SWITCHTEC_GEN5, SWITCHTEC_PFXA},  //PFX-A 84XG5
	{0x5468, SWITCHTEC_GEN5, SWITCHTEC_PFXA},  //PFX-A 68XG5
	{0x5452, SWITCHTEC_GEN5, SWITCHTEC_PFXA},  //PFX-A 52XG5
	{0x5436, SWITCHTEC_GEN5, SWITCHTEC_PFXA},  //PFX-A 36XG5
	{0x5428, SWITCHTEC_GEN5, SWITCHTEC_PFXA},  //PFX-A 28XG5
	{0},
};

static int set_gen_variant(struct switchtec_dev * dev)
{
	const struct switchtec_device_id *id = switchtec_device_id_tbl;
	int ret;

	dev->boot_phase = SWITCHTEC_BOOT_PHASE_FW;
	dev->gen = SWITCHTEC_GEN_UNKNOWN;
	dev->var = SWITCHTEC_VAR_UNKNOWN;
	dev->device_id = dev->ops->get_device_id(dev);

	while (id->device_id) {
		if (id->device_id == dev->device_id) {
			dev->gen = id->gen;
			dev->var = id->var;

			return 0;
		}

		id++;
	}

	ret = switchtec_get_device_info(dev, &dev->boot_phase, &dev->gen, NULL);
	if (ret)
		return -1;

	return 0;
}

static int set_local_pax_id(struct switchtec_dev *dev)
{
	unsigned char local_pax_id;
	int ret;

	dev->local_pax_id = -1;

	if (!switchtec_is_pax_all(dev))
		return 0;

	ret = switchtec_cmd(dev, MRPC_GET_PAX_ID, NULL, 0,
			    &local_pax_id, sizeof(local_pax_id));
	if (ret)
		return -1;

	dev->local_pax_id = local_pax_id;
	return 0;
}

/**
 * @brief Free a list of device info structures allocated by switchtec_list()
 * @param[in] devlist switchtec_device_info structure list as returned by switchtec_list()
 */
void switchtec_list_free(struct switchtec_device_info *devlist)
{
	free(devlist);
}

/**
 * @brief Open a Switchtec device by string
 * @param[in] device A string representing the device to open
 * @return A switchtec_dev structure for use in other library functions
 *	or NULL if an error occurred.
 *
 * The string can be specified as:
 *   * A path to the device (/dev/switchtec0)
 *   * An index (0, 1, etc)
 *   * An index with a 'switchtec' prefix (switchtec0)
 *   * A BDF (bus, device function) string (3:00.1)
 *   * An I2C device with slave number (/dev/i2c-1@0x20)
 *   * An I2C adapter number and slave number (0@0x20)
 *   * An I2C device delimited with a colon (/dev/i2c-1:0x20)
 *     (must start with a / so that it is distinguishable from a BDF)
 *   * A UART device (/dev/ttyUSB0)
 */
struct switchtec_dev *switchtec_open(const char *device)
{
	int idx;
	int domain = 0;
	int bus, dev, func;
	char path[PATH_MAX];
	int inst;
	char *endptr;
	struct switchtec_dev *ret;

	if (sscanf(device, "%i@%i", &bus, &dev) == 2) {
		ret = switchtec_open_i2c_by_adapter(bus, dev);
		goto found;
	}

	if (sscanf(device, "%2049[^@]@%i", path, &dev) == 2) {
		ret = switchtec_open_i2c(path, dev);
		goto found;
	}

	if (device[0] == '/' &&
	    sscanf(device, "%2049[^:]:%i", path, &dev) == 2) {
		ret = switchtec_open_i2c(path, dev);
		goto found;
	}

	if (strchr(device, '/') || strchr(device, '\\')) {
		ret = switchtec_open_by_path(device);
		goto found;
	}

	if (sscanf(device, "%x:%x.%x", &bus, &dev, &func) == 3) {
		ret = switchtec_open_by_pci_addr(domain, bus, dev, func);
		goto found;
	}

	if (sscanf(device, "%x:%x:%x.%x", &domain, &bus, &dev, &func) == 4) {
		ret = switchtec_open_by_pci_addr(domain, bus, dev, func);
		goto found;
	}

	if (sscanf(device, "%2049[^:]:%i", path, &inst) == 2) {
		ret = switchtec_open_eth(path, inst);
		goto found;
	}

	errno = 0;
	idx = strtol(device, &endptr, 0);
	if (!errno && endptr != device) {
		ret = switchtec_open_by_index(idx);
		goto found;
	}

	if (sscanf(device, "switchtec%d", &idx) == 1) {
		ret = switchtec_open_by_index(idx);
		goto found;
	}

	errno = ENODEV;
	return NULL;

found:
	if (!ret) {
		errno = ENODEV;
		return NULL;
	}

	snprintf(ret->name, sizeof(ret->name), "%s", device);

	if (set_gen_variant(ret))
		return NULL;

	if (set_local_pax_id(ret))
		return NULL;

	return ret;
}

/**
 * @brief Get the device id of the device
 * @param[in] dev Switchtec device handle
 * @return The device id of the device
 *
 * This is only valid if the device was opend with switchtec_open().
 */
_PURE int switchtec_device_id(struct switchtec_dev *dev)
{
	return dev->device_id;
}

/**
 * @brief Get the generation of the device
 * @param[in] dev Switchtec device handle
 * @return The generation of the device
 *
 * This is only valid if the device was opend with switchtec_open().
 */
_PURE enum switchtec_gen switchtec_gen(struct switchtec_dev *dev)
{
	return dev->gen;
}

/**
 * @brief Get the variant type of the device
 * @param[in] dev Switchtec device handle
 * @return The variant type of the device
 *
 * This is only valid if the device was opend with switchtec_open().
 */
_PURE enum switchtec_variant switchtec_variant(struct switchtec_dev *dev)
{
	return dev->var;
}

/**
 * @brief Get boot phase of the device
 * @param[in] dev Switchtec device handle
 * @return The boot phase of the device
 *
 * This is only valid if the device was opend with switchtec_open().
 */
_PURE enum switchtec_boot_phase switchtec_boot_phase(struct switchtec_dev *dev)
{
	return dev->boot_phase;
}

/**
 * @brief Get the string that was used to open the deviec
 * @param[in] dev Switchtec device handle
 * @return The name of the device as a string
 *
 * This is only valid if the device was opend with switchtec_open().
 */
_PURE const char *switchtec_name(struct switchtec_dev *dev)
{
	return dev->name;
}

/**
 * @brief Get the partiton number of the device that was opened
 * @param[in] dev Switchtec device handle
 * @return The partition number
 */
_PURE int switchtec_partition(struct switchtec_dev *dev)
{
	return dev->partition;
}

int switchtec_set_pax_id(struct switchtec_dev *dev, int pax_id)
{
	if (!(switchtec_is_gen4(dev) && switchtec_is_pax_all(dev)) &&
	    (pax_id != SWITCHTEC_PAX_ID_LOCAL))
		return -1;

	if (pax_id == SWITCHTEC_PAX_ID_LOCAL)
		dev->pax_id = dev->local_pax_id;
	else
		dev->pax_id = pax_id;

	return 0;
}

static int compare_port_id(const void *aa, const void *bb)
{
	const struct switchtec_port_id *a = aa, *b = bb;

	if (a->partition != b->partition)
		return a->partition - b->partition;
	if (a->upstream != b->upstream)
		return b->upstream - a->upstream;
	return a->log_id - b->log_id;
}

static int compare_status(const void *aa, const void *bb)
{
	const struct switchtec_status *a = aa, *b = bb;

	return compare_port_id(&a->port, &b->port);
}

static const char *lane_reversal_str(int link_up,
				     int lane_reversal)
{
	if (!link_up)
		return "N/A";

	switch(lane_reversal) {
	case 0: return "Normal Lane Ordering";
	case 1: return "x16 (Full) Lane Reversal";
	case 2: return "x2 Lane Reversal";
	case 4: return "x4 Lane Reversal";
	case 8: return "x8 Lane Reversal";
	default: return "Unknown Lane Ordering";
	}
}

static void generate_lane_str(struct switchtec_status *s)
{
	int i, l;

	for (i = 0; i < s->cfg_lnk_width; i++)
		s->lanes[i] = 'x';

	if (!s->link_up)
		return;

	l = s->first_act_lane;
	if (!l && s->lane_reversal)
		l += s->neg_lnk_width - 1;

	for (i = 0; i < s->neg_lnk_width; i++) {
		if (l < 0)
			break;

		if (i < 10)
			s->lanes[l] = '0' + i;
		else
			s->lanes[l] = 'a' + i - 10;

		l += s->lane_reversal ? -1 : 1;
	}
}

/**
 * @brief Get the status of all the ports on a switchtec device
 * @param[in]  dev    Switchtec device handle
 * @param[out] status A pointer to an allocated list of port statuses
 * @return The number of ports in the status list or a negative value
 *	on failure
 *
 * This function a allocates memory for the number of ports in the
 * system. The returned \p status structure should be freed with the
 * switchtec_status_free() function.
 */
int switchtec_status(struct switchtec_dev *dev,
		     struct switchtec_status **status)
{
	uint64_t port_bitmap = 0;
	int ret;
	int i, p;
	int nr_ports = 0;
	struct switchtec_status *s;

	if (!status) {
		errno = EINVAL;
		return -errno;
	}

	struct {
		uint8_t phys_port_id;
		uint8_t par_id;
		uint8_t log_port_id;
		uint8_t stk_id;
		uint8_t cfg_lnk_width;
		uint8_t neg_lnk_width;
		uint8_t usp_flag;
		uint8_t linkup_linkrate;
		uint16_t LTSSM;
		uint8_t lane_reversal;
		uint8_t first_act_lane;
	} ports[SWITCHTEC_MAX_PORTS];

	ret = switchtec_cmd(dev, MRPC_LNKSTAT, &port_bitmap, sizeof(port_bitmap),
			    ports, sizeof(ports));
	if (ret)
		return ret;


	for (i = 0; i < SWITCHTEC_MAX_PORTS; i++) {
		if ((ports[i].stk_id >> 4) > SWITCHTEC_MAX_STACKS)
			continue;
		nr_ports++;
	}

	s = *status = calloc(nr_ports, sizeof(*s));
	if (!s)
		return -ENOMEM;

	for (i = 0, p = 0; i < SWITCHTEC_MAX_PORTS && p < nr_ports; i++) {
		if ((ports[i].stk_id >> 4) > SWITCHTEC_MAX_STACKS)
			continue;

		s[p].port.partition = ports[i].par_id;
		s[p].port.stack = ports[i].stk_id >> 4;
		s[p].port.upstream = ports[i].usp_flag;
		s[p].port.stk_id = ports[i].stk_id & 0xF;
		s[p].port.phys_id = ports[i].phys_port_id;
		s[p].port.log_id = ports[i].log_port_id;

		s[p].cfg_lnk_width = ports[i].cfg_lnk_width;
		s[p].neg_lnk_width = ports[i].neg_lnk_width;
		s[p].link_up = ports[i].linkup_linkrate >> 7;
		s[p].link_rate = ports[i].linkup_linkrate & 0x7F;
		s[p].ltssm = le16toh(ports[i].LTSSM);
		s[p].ltssm_str = switchtec_ltssm_str(s[i].ltssm, 1);
		s[p].lane_reversal = ports[i].lane_reversal;
		s[p].lane_reversal_str = lane_reversal_str(s[p].link_up,
							   s[p].lane_reversal);
		s[p].first_act_lane = ports[i].first_act_lane & 0xF;
		s[p].acs_ctrl = -1;
		generate_lane_str(&s[p]);

		p++;
	}

	qsort(s, nr_ports, sizeof(*s), compare_status);

	return nr_ports;
}

/**
 * @brief Free a list of status structures allocated by switchtec_status()
 * @param[in] status Status structure list
 * @param[in] ports Number of ports in the list (as returned by
 *	switchtec_status())
 */
void switchtec_status_free(struct switchtec_status *status, int ports)
{
	int i;

	for (i = 0; i < ports; i++) {
		if (status[i].pci_bdf)
			free(status[i].pci_bdf);

		if (status[i].pci_bdf_path)
			free(status[i].pci_bdf_path);

		if (status[i].pci_dev)
			free(status[i].pci_dev);

		if (status[i].class_devices)
			free(status[i].class_devices);
	}

	free(status);
}

/**
 * @brief The MRPC command ID when errno is set.
 *
 * If errno is for MRPC (with the SWITCHTEC_ERRNO_MRPC_FLAG_BIT set), this
 * variable will be set to the corresponding MRPC command ID.
 */
int mrpc_error_cmd;

/**
 * @brief Return a message coresponding to the last error
 *
 * This can be called after another switchtec function returned an error
 * to find out what caused the problem.
 *
 * For MRPC errors (mrpc_error_cmd is not -1) that are unknown to this function,
 * the string "Unknown MRPC error" are returned. Otherwise, either proper
 * system error string or MRPC error string is returned.
 */
const char *switchtec_strerror(void)
{
	const char *msg = "Unknown MRPC error";
	int err;

	if ((errno & (SWITCHTEC_ERRNO_MRPC_FLAG_BIT |
		      SWITCHTEC_ERRNO_GENERAL_FLAG_BIT)) == 0) {
		if (errno)
			return strerror(errno);
		else
			return platform_strerror();
	}

	if (errno & SWITCHTEC_ERRNO_GENERAL_FLAG_BIT) {
		switch (errno) {
		case SWITCHTEC_ERR_LOG_DEF_READ_ERROR:
			msg = "Error reading log definition file"; break;
		case SWITCHTEC_ERR_BIN_LOG_READ_ERROR:
			msg = "Error reading binary log file"; break;
		case SWITCHTEC_ERR_PARSED_LOG_WRITE_ERROR:
			msg = "Error writing parsed log file"; break;
		case SWITCHTEC_ERR_LOG_DEF_DATA_INVAL:
			msg = "Invalid log definition data"; break;
		case SWITCHTEC_ERR_INVALID_PORT:
			msg = "Invalid port specified"; break;
		case SWITCHTEC_ERR_INVALID_LANE:
			msg = "Invalid lane specified"; break;
		default:
			msg = "Unknown Switchtec error"; break;
		}

		return msg;
	}

	err = errno & ~SWITCHTEC_ERRNO_MRPC_FLAG_BIT;

	switch (err) {
	case ERR_NO_AVAIL_MRPC_THREAD:
		msg = "No available MRPC handler thread"; break;
	case ERR_HANDLER_THREAD_NOT_IDLE:
		msg = "The handler thread is not idle"; break;
	case ERR_NO_BG_THREAD:
		msg = "No background thread run for the command"; break;

	case ERR_REFCLK_SUBCMD_INVALID:
	case ERR_SUBCMD_INVALID: 	msg = "Invalid subcommand"; break;
	case ERR_CMD_INVALID: 		msg = "Invalid command"; break;
	case ERR_PARAM_INVALID:		msg = "Invalid parameter"; break;
	case ERR_BAD_FW_STATE:		msg = "Bad firmware state"; break;
	case ERR_MRPC_DENIED:		msg = "MRPC request denied"; break;
	case ERR_MRPC_NO_PREV_DATA:
		msg = "No previous adaptation object data";
		break;
	case ERR_REFCLK_STACK_ID_INVALID:
	case ERR_STACK_INVALID: 	msg = "Invalid Stack"; break;
	case ERR_LOOPBACK_PORT_INVALID:
	case ERR_PORT_INVALID: 		msg = "Invalid Port"; break;
	case ERR_EVENT_INVALID: 	msg = "Invalid Event"; break;
	case ERR_RST_RULE_FAILED: 	msg = "Reset rule search failed"; break;
	case ERR_UART_NOT_SUPPORTED:
		msg = "UART interface not supported for this command"; break;
	case ERR_XML_VERSION_MISMATCH:
		msg = "XML version mismatch between MAIN and CFG partition";
		break;
	case ERR_ACCESS_REFUSED: 	msg = "Access Refused"; break;

	default: break;
	}

	switch (mrpc_error_cmd) {
	case MRPC_PORTPARTP2P:
		switch (err) {
		case ERR_PHYC_PORT_ARDY_BIND:
			msg = "Physical port already bound"; break;
		case ERR_LOGC_PORT_ARDY_BIND:
			msg = "Logical bridge instance already bound"; break;
		case ERR_BIND_PRTT_NOT_EXIST:
			msg = "Partition does not exist"; break;
		case ERR_PHYC_PORT_NOT_EXIST:
			msg = "Physical port does not exist"; break;
		case ERR_PHYC_PORT_DIS:
			msg = "Physical port disabled"; break;
		case ERR_NO_LOGC_PORT:
			msg = "No logical bridge instance"; break;
		case ERR_BIND_IN_PROGRESS:
			msg = "Bind/unbind in progress"; break;
		case ERR_BIND_TGT_IS_USP:
			msg = "Bind/unbind target is USP"; break;
		case ERR_BIND_SUBCMD_INVALID:
			msg = "Sub-command does not exist"; break;
		case ERR_PHYC_PORT_LINK_ACT:
			msg = "Physical port link active"; break;
		case ERR_LOGC_PORT_NOT_BIND_PHYC_PORT:
			msg = "Logical bridge not bind to physical port"; break;
		case ERR_UNBIND_OPT_INVALID:
			msg = "Invalid unbind option"; break;
		case ERR_BIND_CHECK_FAIL:
			msg = "Port bind checking failed"; break;
		default: break;
		}
		break;
	default: break;
	}

	return msg;
}

/**
 * @brief Print an error string to stdout
 * @param[in] str String that will be prefixed to the error message
 *
 * This can be called after another switchtec function returned an error
 * to find out what caused the problem.
 */
void switchtec_perror(const char *str)
{
	const char *msg = switchtec_strerror();
	int is_mrpc = errno & SWITCHTEC_ERRNO_MRPC_FLAG_BIT;
	int err = errno & ~SWITCHTEC_ERRNO_MRPC_FLAG_BIT;

	if (is_mrpc)
		fprintf(stderr, "%s: %s (MRPC: 0x%x, error: 0x%x)\n",
			str, msg, mrpc_error_cmd, err);
	else
		fprintf(stderr, "%s: %s\n", str, msg);
}

/**@}*/

/**
 * @defgroup Misc Miscellaneous Commands
 * @brief Various functions that fit don't fit into other categories
 * @{
 */

/**
 * @brief Perform an MRPC echo command
 * @param[in]  dev    Switchtec device handle
 * @param[in]  input  The input data for the echo command
 * @param[out] output The result of the echo command
 * @return 0 on success, error code on failure
 *
 * The echo command takes 4 bytes and returns the bitwise-not of those
 * bytes.
 */
int switchtec_echo(struct switchtec_dev *dev, uint32_t input,
		   uint32_t *output)
{
	return switchtec_cmd(dev, MRPC_ECHO, &input, sizeof(input),
			     output, sizeof(*output));
}

/**
 * @brief Perform an MRPC hard reset command
 * @param[in] dev Switchtec device handle
 * @return 0 on success, error code on failure
 *
 * Note: if your system does not support hotplug this may leave
 * the Switchtec device in an unusable state. A reboot would be
 * required to fix this.
 */
int switchtec_hard_reset(struct switchtec_dev *dev)
{
	uint32_t subcmd = 0;

	return switchtec_cmd(dev, MRPC_RESET, &subcmd, sizeof(subcmd),
			     NULL, 0);
}

/**
 * @brief Free log definition data
 * @param[in] defs - log definition data to free
 */
static void free_log_defs(struct log_defs *defs)
{
	int i, j;

	if (!defs->module_defs)
		return;

	for (i = 0; i < defs->num_alloc; i++) {
		free(defs->module_defs[i].mod_name);

		for (j = 0; j < defs->module_defs[i].num_entries; j++)
			free(defs->module_defs[i].entries[j]);

		free(defs->module_defs[i].entries);
	}

	free(defs->module_defs);
}

/**
 * @brief Allocate / reallocate log definition data
 * @param[in] defs 	  - log definition data
 * @param[in] num_modules - number of modules to allocate for
 * @return 0 on success, negative value on failure
 */
static int realloc_log_defs(struct log_defs *defs, int num_modules)
{
	int i;

	defs->module_defs = realloc(defs->module_defs,
				    (num_modules *
				     sizeof(struct module_log_defs)));
	if (!defs->module_defs) {
		free_log_defs(defs);
		return -1;
	}

	for (i = defs->num_alloc; i < num_modules; i++)
		memset(&defs->module_defs[i], 0,
		       sizeof(struct module_log_defs));

	defs->num_alloc = num_modules;

	return 0;
}

/**
 * @brief Parse an integer from a string
 * @param[in] str  - string to parse
 * @param[out] val - integer
 * @return true on success, false on failure
 */
static bool parse_int(char *str, int *val)
{
	char *endptr;

	errno = 0;
	*val = strtol(str, &endptr, 0);

	if ((endptr == str) || (*endptr != '\0') || (errno != 0))
	    return false;

	return true;
}

/**
 * @brief Read an app log definition file and store the definitions
 * @param[in] log_def_file - log definition file
 * @param[out] defs 	   - log definitions
 * @return 0 on success, negative value on failure
 */
static int read_app_log_defs(FILE *log_def_file, struct log_defs *defs)
{
	int ret;
	char line[512];
	char *tok;
	int mod_id;
	struct module_log_defs *mod_defs;
	int num_entries;
	int i;

	/* allocate some log definition entries */
	ret = realloc_log_defs(defs, 200);
	if (ret < 0)
		return ret;

	while (fgets(line, sizeof(line), log_def_file)) {

		/* ignore comments */
		if (line[0] == '#')
			continue;

		/* strip any newline characters */
		line[strcspn(line, "\r\n")] = '\0';

		/*
		 * Tokenize and parse the line. Module headings are of the form:
		 * mod_name    mod_id    num_entries
		 */
		tok = strtok(line, " \t");
		if (!tok)
			continue;

		tok = strtok(NULL, " \t");
		if (!tok)
			continue;

		if (!parse_int(tok, &mod_id)) {
			errno = SWITCHTEC_ERR_LOG_DEF_DATA_INVAL;
			goto err_free_log_defs;
		}

		/* reallocate more log definition entries if needed */
		if (mod_id > defs->num_alloc) {
			ret = realloc_log_defs(defs, mod_id * 2);
			if (ret < 0)
				return ret;
		}

		mod_defs = &defs->module_defs[mod_id];

		tok = strtok(NULL, " \t");
		if (!tok)
			continue;

		if (!parse_int(tok, &num_entries)) {
			errno = SWITCHTEC_ERR_LOG_DEF_DATA_INVAL;
			goto err_free_log_defs;
		}

		/*
		 * Skip this module if it has already been done. This can happen
		 * if the module is duplicated in the log definition file.
		 */
		if (mod_defs->mod_name != NULL) {
			for (i = 0; i < num_entries; i++) {
				if (!fgets(line, sizeof(line),
					  log_def_file))
					break;
			}
			continue;
		}

		mod_defs->mod_name = strdup(line);
		mod_defs->num_entries = num_entries;
		mod_defs->entries = calloc(mod_defs->num_entries,
					   sizeof(*mod_defs->entries));
		if (!mod_defs->entries)
			goto err_free_log_defs;

		for (i = 0; i < mod_defs->num_entries; i++) {
			if (fgets(line, sizeof(line), log_def_file) == NULL) {
				errno = SWITCHTEC_ERR_LOG_DEF_READ_ERROR;
				goto err_free_log_defs;
			}

			mod_defs->entries[i] = strdup(line);
			if (!mod_defs->entries[i])
				goto err_free_log_defs;
		}
	}

	if (ferror(log_def_file)) {
		errno = SWITCHTEC_ERR_LOG_DEF_READ_ERROR;
		goto err_free_log_defs;
	}

	return 0;

err_free_log_defs:
	free_log_defs(defs);
	return -1;
}

/**
 * @brief Read a mailbox log definition file and store the definitions
 * @param[in] log_def_file - log definition file
 * @param[out] defs 	   - log definitions
 * @return 0 on success, negative value on failure
 */
static int read_mailbox_log_defs(FILE *log_def_file, struct log_defs *defs)
{
	int ret;
	char line[512];
	struct module_log_defs *mod_defs;
	int num_entries_alloc;

	/*
	 * The mailbox log definitions don't keep track of modules. Allocate a
	 * single log definition entry for all definitions.
	 */
	ret = realloc_log_defs(defs, 1);
	if (ret < 0)
		return ret;

	mod_defs = &defs->module_defs[0];
	mod_defs->num_entries = 0;

	/* allocate some entries */
	num_entries_alloc = 100;
	mod_defs->entries = calloc(num_entries_alloc,
				   sizeof(*mod_defs->entries));
	if (!mod_defs->entries)
		goto err_free_log_defs;

	while (fgets(line, sizeof(line), log_def_file)) {
		if (mod_defs->num_entries >= num_entries_alloc) {
			/* allocate more entries */
			num_entries_alloc *= 2;
			mod_defs->entries = realloc(mod_defs->entries,
						    (num_entries_alloc *
						     sizeof(*mod_defs->entries)));
			if (!mod_defs->entries)
				goto err_free_log_defs;
		}

		mod_defs->entries[mod_defs->num_entries] = strdup(line);
		if (!mod_defs->entries[mod_defs->num_entries])
			goto err_free_log_defs;

		mod_defs->num_entries++;
	}

	if (ferror(log_def_file)) {
		errno = SWITCHTEC_ERR_LOG_DEF_READ_ERROR;
		goto err_free_log_defs;
	}

	return 0;

err_free_log_defs:
	free_log_defs(defs);
	return -1;
}

/**
 * @brief Parse an app log or mailbox log and write the results to a file
 * @param[in] log_data	     - logging data
 * @param[in] count	     - number of entries
 * @param[in] init_entry_idx - index of the initial entry
 * @param[in] defs           - log definitions
 * @param[in] log_type       - log type
 * @param[in] log_file	     - log output file
 * @return 0 on success, negative value on failure
 */
static int write_parsed_log(struct log_a_data log_data[],
			    size_t count, int init_entry_idx,
			    struct log_defs *defs,
			    enum switchtec_log_parse_type log_type,
			    FILE *log_file)
{
	int i;
	int entry_idx = init_entry_idx;
	unsigned long long time;
	unsigned int nanos, micros, millis, secs, mins, hours, days;
	unsigned int entry_num;
	unsigned int mod_id;
	unsigned int log_sev = 0;
	const char *log_sev_strs[] = {"DISABLED", "HIGHEST", "HIGH", "MEDIUM",
				      "LOW", "LOWEST"};
	bool is_bl1;
	struct module_log_defs *mod_defs;

	if (entry_idx == 0) {
		if (log_type == SWITCHTEC_LOG_PARSE_TYPE_APP)
			fputs("   #|Timestamp                |Module       |Severity |Event\n",
		      	      log_file);
		else
			fputs("   #|Timestamp                |Source |Event\n",
		      	      log_file);
	}

	for (i = 0; i < count; i ++) {
		/* timestamp is in the first 2 DWords */
		time = (((unsigned long long)log_data[i].data[0] << 32) |
			log_data[i].data[1]) * 10ULL;
		nanos = time % 1000;
		time /= 1000;
		micros = time % 1000;
		time /= 1000;
		millis = time % 1000;
		time /= 1000;
		secs = time % 60;
		time /= 60;
		mins = time % 60;
		time /= 60;
		hours = time % 24;
		days = time / 24;

		if (log_type == SWITCHTEC_LOG_PARSE_TYPE_APP) {
			/*
			 * app log: module ID and log severity are in the 3rd
			 * DWord
			 */
			mod_id = (log_data[i].data[2] >> 16) & 0xFFF;
			log_sev = (log_data[i].data[2] >> 28) & 0xF;

			if ((mod_id > defs->num_alloc) ||
			    (defs->module_defs[mod_id].mod_name == NULL) ||
			    (strlen(defs->module_defs[mod_id].mod_name) == 0)) {
				if (fprintf(log_file, "(Invalid module ID: 0x%x)\n",
					mod_id) < 0)
					goto ret_print_error;
				continue;
			}

			if (log_sev >= ARRAY_SIZE(log_sev_strs)) {
				if (fprintf(log_file, "(Invalid log severity: %d)\n",
					log_sev) < 0)
					goto ret_print_error;
				continue;
			}
		} else {
			/*
			 * mailbox log: BL1/BL2 indication is in the 3rd
			 * DWord
			 */
			is_bl1 = (((log_data[i].data[2] >> 27) & 1) == 0);

			/* mailbox log definitions are all in the first entry */
			mod_id = 0;
		}

		mod_defs = &defs->module_defs[mod_id];

		/* entry number is in the 3rd DWord */
		entry_num = log_data[i].data[2] & 0x0000FFFF;

		if (entry_num >= mod_defs->num_entries) {
			if (fprintf(log_file,
				    "(Invalid log entry number: %d (module 0x%x))\n",
				    entry_num, mod_id) < 0)
				goto ret_print_error;
			continue;
		}

		/* print the entry index and timestamp */
		if (fprintf(log_file,
			    "%04d|%03dd %02d:%02d:%02d.%03d,%03d,%03d|",
			    entry_idx, days, hours, mins, secs, millis,
			    micros, nanos) < 0)
			goto ret_print_error;

		if (log_type == SWITCHTEC_LOG_PARSE_TYPE_APP) {
			/* print the module name and log severity */
			if (fprintf(log_file, "%-12s |%-8s |",
			    mod_defs->mod_name, log_sev_strs[log_sev]) < 0)
				goto ret_print_error;
		} else {
			/* print the log source (BL1/BL2) */
			if (fprintf(log_file, "%-6s |",
			    (is_bl1 ? "BL1" : "BL2")) < 0)
				goto ret_print_error;
		}

		/* print the log entry */
	  	if (fprintf(log_file, mod_defs->entries[entry_num],
	  		    log_data[i].data[3], log_data[i].data[4],
			    log_data[i].data[5], log_data[i].data[6],
			    log_data[i].data[7]) < 0)
			goto ret_print_error;

		entry_idx++;
	}

	if (fflush(log_file) != 0)
		return -1;

	return 0;

ret_print_error:
	errno = SWITCHTEC_ERR_PARSED_LOG_WRITE_ERROR;
	return -1;
}

static int parse_def_header(FILE *log_def_file, uint32_t *fw_version,
			    uint32_t *sdk_version)
{
	char line[512];
	int i;

	*fw_version = 0;
	*sdk_version = 0;
	while (fgets(line, sizeof(line), log_def_file)) {
		if (line[0] != '#')
			continue;

		i = 0;
		while (line[i] == ' ' || line[i] == '#') i++;

		if (strncasecmp(line + i, "SDK Version:", 12) == 0) {
			i += 12;
			while (line[i] == ' ') i++;
			sscanf(line + i, "%i", (int*)sdk_version);
		}
		else if (strncasecmp(line + i, "FW Version:", 11) == 0) {
			i += 11;
			while (line[i] == ' ') i++;
			sscanf(line + i, "%i", (int*)fw_version);
		}
	}

	rewind(log_def_file);
	return 0;
}

static int append_log_header(int fd, uint32_t sdk_version,
			     uint32_t fw_version, int binary)
{
	int ret;
	struct log_header {
		uint8_t magic[8];
		uint32_t fw_version;
		uint32_t sdk_version;
		uint32_t flags;
		uint32_t rsvd[3];
	} header = {
		.magic = {'S', 'W', 'M', 'C', 'L', 'O', 'G', 'F'},
		.fw_version = fw_version,
		.sdk_version = sdk_version
	};
	char hdr_str_fmt[] = "#########################\n"
			     "## FW version %08x\n"
			     "## SDK version %08x\n"
			     "#########################\n\n";
	char hdr_str[512];

	if (binary) {
		ret = write(fd, &header, sizeof(header));
	} else {
		snprintf(hdr_str, 512, hdr_str_fmt, fw_version, sdk_version);
		ret = write(fd, hdr_str, strlen(hdr_str));
	}

	return ret;
}

static int log_a_to_file(struct switchtec_dev *dev, int sub_cmd_id,
			 int fd, FILE *log_def_file,
			 struct switchtec_log_file_info *info)
{
	int ret = -1;
	int read = 0;
	struct log_a_retr_result res;
	struct log_a_retr cmd = {
		.sub_cmd_id = sub_cmd_id,
		.start = -1,
	};
	struct log_defs defs = {
		.module_defs = NULL,
		.num_alloc = 0};
	FILE *log_file;
	int entry_idx = 0;
	uint32_t fw_version = 0;
	uint32_t sdk_version = 0;

	if (log_def_file != NULL) {
		ret = parse_def_header(log_def_file, &fw_version,
				       &sdk_version);
		if (ret)
			return ret;
		/* read the log definition file into defs */
		ret = read_app_log_defs(log_def_file, &defs);
		if (ret < 0)
			return ret;
	}

	res.hdr.remain = 1;

	while (res.hdr.remain) {
		ret = switchtec_cmd(dev, MRPC_FWLOGRD, &cmd, sizeof(cmd),
				    &res, sizeof(res));
		if (ret)
			goto ret_free_log_defs;
		if (res.hdr.overflow && info)
			info->overflow = 1;
		if (read == 0) {
			if (dev->gen < SWITCHTEC_GEN5) {
				res.hdr.sdk_version = 0;
				res.hdr.fw_version = 0;
			}

			if (info) {
				info->def_fw_version = fw_version;
				info->def_sdk_version = sdk_version;
				info->log_fw_version = res.hdr.fw_version;
				info->log_sdk_version = res.hdr.sdk_version;
			}

			if (res.hdr.sdk_version != sdk_version ||
			     res.hdr.fw_version != fw_version) {
				if (info && log_def_file)
					info->version_mismatch = true;

			}

			append_log_header(fd, res.hdr.sdk_version,
					  res.hdr.fw_version,
					  log_def_file == NULL? 1 : 0);
		}

		if (log_def_file == NULL) {
			/* write the binary log data to a file */
			ret = write(fd, res.data,
				    sizeof(*res.data) * res.hdr.count);
			if (ret < 0)
				return ret;
		} else {
			log_file = fdopen(fd, "w");
			if (!log_file)
				goto ret_free_log_defs;

			/* parse the log data and write it to a file */
			ret = write_parsed_log(res.data, res.hdr.count,
					       entry_idx, &defs,
					       SWITCHTEC_LOG_PARSE_TYPE_APP,
					       log_file);
			if (ret < 0)
				goto ret_free_log_defs;

			entry_idx += res.hdr.count;
		}

		read += le32toh(res.hdr.count);
		cmd.start = res.hdr.next_start;
	}

	ret = 0;

ret_free_log_defs:
	free_log_defs(&defs);
	return ret;
}

static int log_b_to_file(struct switchtec_dev *dev, int sub_cmd_id, int fd)
{
	int ret;
	int read = 0;
	struct log_b_retr_result res;
	struct log_b_retr cmd = {
		.sub_cmd_id = sub_cmd_id,
		.offset = 0,
		.length = htole32(sizeof(res.data)),
	};

	res.hdr.remain = sizeof(res.data);

	while (res.hdr.remain) {
		ret = switchtec_cmd(dev, MRPC_FWLOGRD, &cmd, sizeof(cmd),
				    &res, sizeof(res));
		if (ret)
			return -1;

		ret = write(fd, res.data, res.hdr.length);
		if (ret < 0)
			return ret;

		read += le32toh(res.hdr.length);
		cmd.offset = htole32(read);
	}

	return 0;
}

static int log_c_to_file(struct switchtec_dev *dev, int sub_cmd_id, int fd)
{
	int ret;
	struct log_cmd {
		uint8_t subcmd;
		uint8_t rsvd[3];
	} cmd = {};

	struct log_reply {
		uint8_t reason;
		uint8_t rsvd[3];
		uint32_t nvlog_version;
		uint32_t thread_handle;
		uint32_t fw_version;
		uint32_t timestamp1;
		uint32_t timestamp2;
	} reply;

	cmd.subcmd = sub_cmd_id;

	ret = switchtec_cmd(dev, MRPC_FWLOGRD, &cmd, sizeof(cmd),
			    &reply, sizeof(reply));
	if (ret)
		return -1;

	ret = write(fd, &reply, sizeof(reply));
	if (ret < 0)
		return ret;

	return 0;
}

static int log_ram_flash_to_file(struct switchtec_dev *dev,
				 int gen5_cmd, int gen4_cmd, int gen4_cmd_lgcy,
				 int fd, FILE *log_def_file,
				 struct switchtec_log_file_info *info)
{
	int ret;

	if (switchtec_is_gen5(dev)) {
		return log_a_to_file(dev, gen5_cmd, fd, log_def_file,
				     info);
	} else {
		ret = log_a_to_file(dev, gen4_cmd, fd, log_def_file,
				    info);

		/* somehow hardware returns ERR_LOGC_PORT_ARDY_BIND
		 * instead of ERR_SUBCMD_INVALID if this subcommand
		 * is not supported, so we fall back to legacy
		 * subcommand on ERR_LOGC_PORT_ARDY_BIND error as well
		 */
		if (ret > 0 &&
		    (ERRNO_MRPC(errno) == ERR_LOGC_PORT_ARDY_BIND ||
		     ERRNO_MRPC(errno) == ERR_SUBCMD_INVALID))
			ret = log_a_to_file(dev, gen4_cmd_lgcy, fd,
					    log_def_file, info);

		return ret;
	}
}

/**
 * @brief Dump the Switchtec log data to a file
 * @param[in]  dev          - Switchtec device handle
 * @param[in]  type         - Type of log data to dump
 * @param[in]  fd           - File descriptor to dump the data to
 * @param[in]  log_def_file - Log definition file
 * @param[out] info         - Log file information
 * @return 0 on success, error code on failure
 */
int switchtec_log_to_file(struct switchtec_dev *dev,
		enum switchtec_log_type type, int fd, FILE *log_def_file,
		struct switchtec_log_file_info *info)
{
	if (info)
		memset(info, 0, sizeof(*info));

	switch (type) {
	case SWITCHTEC_LOG_RAM:
		return log_ram_flash_to_file(dev,
					     MRPC_FWLOGRD_RAM_GEN5,
					     MRPC_FWLOGRD_RAM_WITH_FLAG,
					     MRPC_FWLOGRD_RAM,
					     fd, log_def_file, info);
	case SWITCHTEC_LOG_FLASH:
		return log_ram_flash_to_file(dev,
					     MRPC_FWLOGRD_FLASH_GEN5,
					     MRPC_FWLOGRD_FLASH_WITH_FLAG,
					     MRPC_FWLOGRD_FLASH,
					     fd, log_def_file, info);
	case SWITCHTEC_LOG_MEMLOG:
		return log_b_to_file(dev, MRPC_FWLOGRD_MEMLOG, fd);
	case SWITCHTEC_LOG_REGS:
		return log_b_to_file(dev, MRPC_FWLOGRD_REGS, fd);
	case SWITCHTEC_LOG_THRD_STACK:
		return log_b_to_file(dev, MRPC_FWLOGRD_THRD_STACK, fd);
	case SWITCHTEC_LOG_SYS_STACK:
		return log_b_to_file(dev, MRPC_FWLOGRD_SYS_STACK, fd);
	case SWITCHTEC_LOG_THRD:
		return log_b_to_file(dev, MRPC_FWLOGRD_THRD, fd);
	case SWITCHTEC_LOG_NVHDR:
		return log_c_to_file(dev, MRPC_FWLOGRD_NVHDR, fd);
	};

	errno = EINVAL;
	return -errno;
}

static int parse_log_header(FILE *bin_log_file, uint32_t *fw_version,
			    uint32_t *sdk_version)
{
	struct log_header {
		uint8_t magic[8];
		uint32_t fw_version;
		uint32_t sdk_version;
		uint32_t flags;
		uint32_t rsvd[3];
	} header;

	char sig[8] = {'S', 'W', 'M', 'C', 'L', 'O', 'G', 'F'};
	int ret;

	ret = fread(&header, sizeof(header), 1, bin_log_file);
	if (ret <= 0) {
		errno = EBADF;
		return -EBADF;
	}

	if (memcmp(sig, header.magic, 8)) {
		rewind(bin_log_file);
		*fw_version = 0;
		*sdk_version = 0;
		return 0;
	}

	*fw_version = header.fw_version;
	*sdk_version = header.sdk_version;

	return 0;
}

/**
 * @brief Parse a binary app log or mailbox log to a text file
 * @param[in] bin_log_file    - Binary log input file
 * @param[in] log_def_file    - Log definition file
 * @param[in] parsed_log_file - Parsed output file
 * @param[in] log_type        - log type
 * @param[out] info           - log file information
 * @return 0 on success, error code on failure
 */
int switchtec_parse_log(FILE *bin_log_file, FILE *log_def_file,
			FILE *parsed_log_file,
			enum switchtec_log_parse_type log_type,
			struct switchtec_log_file_info *info)
{
	int ret;
	struct log_a_data log_data;
	struct log_defs defs = {
		.module_defs = NULL,
		.num_alloc = 0};
	int entry_idx = 0;
	uint32_t fw_version_log;
	uint32_t sdk_version_log;
	uint32_t fw_version_def;
	uint32_t sdk_version_def;

	if (info)
		memset(info, 0, sizeof(*info));

	if ((log_type != SWITCHTEC_LOG_PARSE_TYPE_APP) &&
	    (log_type != SWITCHTEC_LOG_PARSE_TYPE_MAILBOX)) {
		errno = EINVAL;
		return -errno;
	}

	ret = parse_log_header(bin_log_file, &fw_version_log,
			       &sdk_version_log);
	if (ret)
		return ret;
	ret = parse_def_header(log_def_file, &fw_version_def,
			       &sdk_version_def);
	if (ret)
		return ret;

	if (info) {
		info->def_fw_version = fw_version_def;
		info->def_sdk_version = sdk_version_def;

		info->log_fw_version = fw_version_log;
		info->log_sdk_version = sdk_version_log;
	}
	/* read the log definition file into defs */
	if (log_type == SWITCHTEC_LOG_PARSE_TYPE_APP)
		ret = read_app_log_defs(log_def_file, &defs);
	else
		ret = read_mailbox_log_defs(log_def_file, &defs);

	ret = append_log_header(fileno(parsed_log_file), sdk_version_log,
				fw_version_log, 0);
	if (ret < 0)
		return ret;

	/* parse each log entry */
	while (fread(&log_data, sizeof(struct log_a_data), 1,
		     bin_log_file) == 1) {
		ret = write_parsed_log(&log_data, 1, entry_idx, &defs,
				       log_type, parsed_log_file);
		if (ret < 0)
			goto ret_free_log_defs;

		entry_idx++;
	}

	if (ferror(bin_log_file)) {
		errno = SWITCHTEC_ERR_BIN_LOG_READ_ERROR;
		ret = -1;
	}

	if (fw_version_def != fw_version_log ||
	    sdk_version_def != sdk_version_log)
		ret = ENOEXEC;

ret_free_log_defs:
	free_log_defs(&defs);
	return ret;
}

/**
 * @brief Dump the Switchtec log definition data to a file
 * @param[in]  dev          - Switchtec device handle
 * @param[in]  type         - Type of log definition data to dump
 * @param[in]  file           - File descriptor to dump the data to
 * @return 0 on success, error code on failure
 */
int switchtec_log_def_to_file(struct switchtec_dev *dev,
			      enum switchtec_log_def_type type,
			      FILE* file)
{
	int ret;
	struct log_cmd {
		uint8_t subcmd;
		uint8_t rsvd[3];
		uint16_t idx;
		uint16_t mod_id;
	} cmd = {};

	struct log_reply {
		uint16_t end_of_data;
		uint16_t data_len;
		uint16_t next_idx;
		uint16_t next_mod_id;
		uint8_t data[MRPC_MAX_DATA_LEN - 16];
	} reply = {};

	switch (type) {
	case SWITCHTEC_LOG_DEF_TYPE_APP:
		cmd.subcmd = MRPC_LOG_DEF_APP;
		break;

	case SWITCHTEC_LOG_DEF_TYPE_MAILBOX:
		cmd.subcmd = MRPC_LOG_DEF_MAILBOX;
		break;

	default:
		errno = EINVAL;
		return -errno;
	}

	do {
		ret = switchtec_cmd(dev, MRPC_LOG_DEF_GET, &cmd, sizeof(cmd),
				    &reply, sizeof(reply));
		if (ret)
			return -1;

		ret = fwrite(reply.data, reply.data_len, 1, file);
		if (ret < 0)
			return ret;

		cmd.idx = reply.next_idx;
		cmd.mod_id = reply.next_mod_id;
	} while (!reply.end_of_data);

	return 0;
}

static enum switchtec_gen map_to_gen(uint32_t gen)
{
	enum switchtec_gen ret = SWITCHTEC_GEN_UNKNOWN;

	switch (gen) {
	case 0:
		ret = SWITCHTEC_GEN4;
		break;
	case 1:
		ret = SWITCHTEC_GEN5;
		break;
	default:
		ret = SWITCHTEC_GEN_UNKNOWN;
		break;
	}

	return ret;
}

/**
 * @brief Get device generation, revision, and boot phase info
 * @param[in]  dev	Switchtec device handle
 * @param[out] phase	The current boot phase
 * @param[out] gen	Device generation
 * @param[out] rev	Device revision
 * @return 0 on success, error code on failure
 */
int switchtec_get_device_info(struct switchtec_dev *dev,
			      enum switchtec_boot_phase *phase,
			      enum switchtec_gen *gen,
			      enum switchtec_rev *rev)
{
	int ret;
	uint32_t ping_dw = 0;
	uint32_t dev_info;
	struct get_dev_info_reply {
		uint32_t dev_info;
		uint32_t ping_reply;
	} reply;

	ping_dw = time(NULL);

	/*
	 * The I2C TWI Ping command also dumps information about the
	 * revision and image phase.
	 */
	ret = switchtec_cmd(dev, MRPC_I2C_TWI_PING, &ping_dw,
			    sizeof(ping_dw),
			    &reply, sizeof(reply));
	if (ret == 0) {
		if (ping_dw != ~reply.ping_reply)
			return -1;

		dev_info = le32toh(reply.dev_info);
		if (phase)
			*phase = dev_info & 0xff;
		if (rev)
			*rev = (dev_info >> 8) & 0x0f;
		if (gen)
			*gen = map_to_gen((dev_info >> 12) & 0x0f);
	} else if (ERRNO_MRPC(errno) == ERR_CMD_INVALID) {
		if (phase)
			*phase = SWITCHTEC_BOOT_PHASE_FW;
		if (gen)
			*gen = SWITCHTEC_GEN3;
		if (rev)
			*rev = SWITCHTEC_REV_UNKNOWN;

		errno = 0;
	} else {
		return -1;
	}

	return 0;
}

/**
 * @brief Get the die temperature of the switchtec device
 * @param[in]  dev    Switchtec device handle
 * @return The die temperature (in degrees celsius) or a negative value
 *	on failure
 */
float switchtec_die_temp(struct switchtec_dev *dev)
{
	int ret;
	uint32_t sub_cmd_id;
	uint32_t temp;

	if (switchtec_is_gen3(dev)) {
		sub_cmd_id = MRPC_DIETEMP_SET_MEAS;
		ret = switchtec_cmd(dev, MRPC_DIETEMP, &sub_cmd_id,
				    sizeof(sub_cmd_id), NULL, 0);
		if (ret)
			return -100.0;

		sub_cmd_id = MRPC_DIETEMP_GET;
		ret = switchtec_cmd(dev, MRPC_DIETEMP, &sub_cmd_id,
				    sizeof(sub_cmd_id), &temp, sizeof(temp));
		if (ret)
			return -100.0;
	} else {
		sub_cmd_id = MRPC_DIETEMP_GET_GEN4;
		ret = switchtec_cmd(dev, MRPC_DIETEMP, &sub_cmd_id,
				    sizeof(sub_cmd_id), &temp, sizeof(temp));
		if (ret)
			return -100.0;
	}

	return le32toh(temp) / 100.;
}

int switchtec_bind_info(struct switchtec_dev *dev,
			struct switchtec_bind_status_out *status, int phy_port)
{
	struct switchtec_bind_status_in sub_cmd_id = {
		.sub_cmd = MRPC_PORT_INFO,
		.phys_port_id = phy_port
	};

	return switchtec_cmd(dev, MRPC_PORTPARTP2P, &sub_cmd_id,
			    sizeof(sub_cmd_id), status, sizeof(*status));
}

int switchtec_bind(struct switchtec_dev *dev, int par_id, int log_port,
		   int phy_port)
{
	uint32_t output;

	struct switchtec_bind_in sub_cmd_id = {
		.sub_cmd = MRPC_PORT_BIND,
		.par_id = par_id,
		.log_port_id = log_port,
		.phys_port_id = phy_port
	};

	return switchtec_cmd(dev, MRPC_PORTPARTP2P, &sub_cmd_id,
			    sizeof(sub_cmd_id), &output, sizeof(output));
}

int switchtec_unbind(struct switchtec_dev *dev, int par_id, int log_port)
{
	uint32_t output;

	struct switchtec_unbind_in sub_cmd_id = {
		.sub_cmd = MRPC_PORT_UNBIND,
		.par_id = par_id,
		.log_port_id = log_port,
		.opt = 2
	};

	return switchtec_cmd(dev, MRPC_PORTPARTP2P, &sub_cmd_id,
			    sizeof(sub_cmd_id), &output, sizeof(output));
}

static int __switchtec_calc_lane_id(struct switchtec_status *port, int lane_id)
{
	int lane;

	if (lane_id >= port->neg_lnk_width) {
		errno = SWITCHTEC_ERR_INVALID_LANE;
		return -1;
	}

	lane = port->port.phys_id * 2;
	if (!port->lane_reversal)
		lane += lane_id;
	else
		lane += port->cfg_lnk_width - 1 - lane_id;

	switch (port->port.phys_id) {
	/* Trident (Gen4) - Ports 48 to 51 maps to 96 to 99 */
	case 48: return 96;
	case 49: return 97;
	case 50: return 98;
	case 51: return 99;
	/* Hrapoon (Gen5) - Ports 56 to 59 maps to 96 to 99 */
	case 56: return 96;
	case 57: return 97;
	case 58: return 98;
	case 59: return 99;
	default: return lane;
	}
}

/**
 * @brief Calculate the global lane ID for a lane within a physical port
 * @param[in] dev               Switchtec device handle
 * @param[in] phys_port_id      Physical port id
 * @param[in] lane_id           Lane number within the port
 * @param[out] status           Optionally return the status of the port
 * @return The lane id or -1 on error (with errno set appropriately)
 */
int switchtec_calc_lane_id(struct switchtec_dev *dev, int phys_port_id,
			   int lane_id, struct switchtec_status *port)
{
	struct switchtec_status *status;
	int ports, i;
	int rc = 0;

	ports = switchtec_status(dev, &status);
	if (ports < 0)
		return ports;

	for (i = 0; i < ports; i++)
		if (status[i].port.phys_id == phys_port_id)
			break;

	if (i == ports) {
		errno = SWITCHTEC_ERR_INVALID_PORT;
		rc = -1;
		goto out;
	}

	if (port)
		*port = status[i];

	rc = __switchtec_calc_lane_id(&status[i], lane_id);

out:
	switchtec_status_free(status, ports);
	return rc;
}

/**
 * @brief Calculate the port and lane within the port from a global lane ID
 * @param[in] dev               Switchtec device handle
 * @param[in] lane_id           Global Lane Number
 * @param[out] phys_port_id     Physical port id
 * @param[out] port_lane        Lane number within the port
 * @param[out] status           Optionally return the status of the port
 * @return The 0 on success or -1 on error (with errno set appropriately)
 */
int switchtec_calc_port_lane(struct switchtec_dev *dev, int lane_id,
			     int *phys_port_id, int *port_lane_id,
			     struct switchtec_status *port)
{
	struct switchtec_status *status;
	int ports, i, p, lane;
	int rc = 0;

	ports = switchtec_status(dev, &status);
	if (ports < 0)
		return ports;

	if (lane_id >= 96) {
		if (dev->gen < SWITCHTEC_GEN5)
			p = lane_id - 96 + 48;
		else
			p = lane_id - 96 + 56;

		for (i = 0; i < ports; i++)
			if (status[i].port.phys_id == p)
				break;
	} else {
		for (i = 0; i < ports; i++) {
			p = status[i].port.phys_id * 2;
			if (lane_id >= p && lane_id < p + status[i].cfg_lnk_width)
				break;
		}
	}

	if (i == ports) {
		errno = SWITCHTEC_ERR_INVALID_PORT;
		rc = -1;
		goto out;
	}

	if (port)
		*port = status[i];

	if (phys_port_id)
		*phys_port_id = status[i].port.phys_id;

	lane = lane_id - status[i].port.phys_id * 2;
	if (port->lane_reversal)
		lane = status[i].cfg_lnk_width - 1 - lane;

	if (port_lane_id)
		*port_lane_id = lane;

out:
	switchtec_status_free(status, ports);
	return rc;
}

/**
 * @brief Calculate the lane mask for lanes within a physical port
 * @param[in] dev		Switchtec device handle
 * @param[in] phys_port_id	Physical port id
 * @param[in] lane_id		Lane number within the port
 * @param[in] num_lanes		Number of consecutive lanes to set
 * @param[out] lane_mask	Pointer to array of 4 integers to set the
 *				bits of the lanes to
 * @param[out] status		Optionally, return the status of the port
 * @return The 0 or -1 on error (with errno set appropriately)
 */
int switchtec_calc_lane_mask(struct switchtec_dev *dev, int phys_port_id,
		int lane_id, int num_lanes, int *lane_mask,
		struct switchtec_status *port)
{
	struct switchtec_status *status;
	int ports, i, l, lane;
	int rc = 0;

	ports = switchtec_status(dev, &status);
	if (ports < 0)
		return ports;

	for (i = 0; i < ports; i++)
		if (status[i].port.phys_id == phys_port_id)
			break;

	if (i == ports) {
		errno = SWITCHTEC_ERR_INVALID_PORT;
		rc = -1;
		goto out;
	}

	if (port)
		*port = status[i];

	for (l = lane_id; l < lane_id + num_lanes; l++) {
		lane = __switchtec_calc_lane_id(&status[i], l);
		if (lane < 0) {
			rc = -1;
			goto out;
		}

		lane_mask[lane >> 5] |= 1 << (lane & 0x1F);
	}

out:
	switchtec_status_free(status, ports);
	return rc;
}

/**@}*/
