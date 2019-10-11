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
#include "switchtec/recovery.h"

#include <string.h>
#include <unistd.h>
#include <errno.h>

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
	{0x4228, SWITCHTEC_GEN4, SWITCHTEC_PAX},   //PAX 28XG4
	{0},
};

static int set_gen_variant(struct switchtec_dev * dev)
{
	const struct switchtec_device_id *id = switchtec_device_id_tbl;
	int ret;
	enum switchtec_boot_phase phase_id;
	
	ret = switchtec_get_boot_phase(dev, &phase_id);
	if (ret == 0 &&
	    ((phase_id == SWITCHTEC_BOOT_PHASE_BL1) ||
	     (phase_id == SWITCHTEC_BOOT_PHASE_BL2))) {
		dev->gen = SWITCHTEC_GEN4;
		dev->var = SWITCHTEC_PSX;
		return 0;
	}

	dev->device_id = dev->ops->get_device_id(dev);
	if (dev->device_id < 0) {
		errno = ENOTSUP;
		return -1;
	}

	dev->gen = SWITCHTEC_GEN_UNKNOWN;
	while (id->device_id) {
		if (id->device_id == dev->device_id) {
			dev->gen = id->gen;
			dev->var = id->var;
		}

		id++;
	}

	if (dev->gen == SWITCHTEC_GEN_UNKNOWN) {
		errno = ENOTSUP;
		return -1;
	}

	return 0;
}

static int set_local_pax_id(struct switchtec_dev *dev)
{
	unsigned char local_pax_id;
	int ret;

	dev->local_pax_id = -1;

	if (!switchtec_is_pax(dev))
		return 0;

	ret = switchtec_cmd(dev, MRPC_GET_PAX_ID, NULL, 0,
			    &local_pax_id, sizeof(local_pax_id));
	if (ret)
		return -1;

	dev->local_pax_id = local_pax_id;
	return 0;
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
	char *endptr;
	struct switchtec_dev *ret;

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

	if (sscanf(device, "%i@%i", &bus, &dev) == 2) {
		ret = switchtec_open_i2c_by_adapter(bus, dev);
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
	if (!(switchtec_is_gen4(dev) && switchtec_is_pax(dev)) &&
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
		uint16_t reserved;
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

		s[p].acs_ctrl = -1;

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

	if ((errno & SWITCHTEC_ERRNO_MRPC_FLAG_BIT) == 0) {
		if (errno)
			return strerror(errno);
		else
			return platform_strerror();
	}

	err = errno & ~SWITCHTEC_ERRNO_MRPC_FLAG_BIT;

	switch (err) {
	case ERR_NO_AVAIL_MRPC_THREAD:
		msg = "No available MRPC handler thread"; break;
	case ERR_HANDLER_THREAD_NOT_IDLE:
		msg = "The handler thread is not idle"; break;
	case ERR_NO_BG_THREAD:
		msg = "No background thread run for the command"; break;

	case ERR_SUBCMD_INVALID: 	msg = "Invalid subcommand"; break;
	case ERR_CMD_INVALID: 		msg = "Invalid command"; break;
	case ERR_PARAM_INVALID:		msg = "Invalid parameter"; break;
	case ERR_BAD_FW_STATE:		msg = "Bad firmware state"; break;
	case ERR_STACK_INVALID: 	msg = "Invalid Stack"; break;
	case ERR_PORT_INVALID: 		msg = "Invalid Port"; break;
	case ERR_EVENT_INVALID: 	msg = "Invalid Event"; break;
	case ERR_RST_RULE_FAILED: 	msg = "Reset rule search failed"; break;
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

static int log_a_to_file(struct switchtec_dev *dev, int sub_cmd_id, int fd)
{
	int ret;
	int read = 0;
	struct log_a_retr_result res;
	struct log_a_retr cmd = {
		.sub_cmd_id = sub_cmd_id,
		.start = -1,
	};

	res.hdr.remain = 1;

	while (res.hdr.remain) {
		ret = switchtec_cmd(dev, MRPC_FWLOGRD, &cmd, sizeof(cmd),
				    &res, sizeof(res));
		if (ret)
			return -1;

		ret = write(fd, res.data, sizeof(*res.data) * res.hdr.count);
		if (ret < 0)
			return ret;

		read += le32toh(res.hdr.count);
		cmd.start = res.hdr.next_start;
	}

	return read;
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

	return read;
}

/**
 * @brief Dump the Switchtec log data to a file
 * @param[in]  dev    Switchtec device handle
 * @param[in]  type   Type of log data to dump
 * @param[in]  fd     File descriptor to dump the data to
 * @return 0 on success, error code on failure
 */
int switchtec_log_to_file(struct switchtec_dev *dev,
			  enum switchtec_log_type type,
			  int fd)
{
	switch (type) {
	case SWITCHTEC_LOG_RAM:
		return log_a_to_file(dev, MRPC_FWLOGRD_RAM, fd);
	case SWITCHTEC_LOG_FLASH:
		return log_a_to_file(dev, MRPC_FWLOGRD_FLASH, fd);
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
	};

	errno = EINVAL;
	return -errno;
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

	if (!switchtec_is_gen3(dev) && !switchtec_is_gen4(dev)) {
		errno = ENOTSUP;
		return -100.0;
	}

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

	return temp / 100.;
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
/**@}*/
