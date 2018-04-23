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
 * @brief Open a Switchtec device by string
 * @param[in] device A string representing the device to open
 * @return A switchtec_dev structure for use in other library functions
 *	or NULL if an error occurred.
 *
 * The string can be specified as a path to the device (/dev/switchtec0),
 * an index (0, 1, etc), an index with a 'switchtec' prefix (switchtec0)
 * or a BDF (bus, device function) string (3:00.1).
 */
struct switchtec_dev *switchtec_open(const char *device)
{
	int idx;
	int domain = 0;
	int bus, dev, func;
	char *endptr;
	struct switchtec_dev *ret;

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
	if (ret)
		snprintf(ret->name, sizeof(ret->name), "%s", device);

	return ret;
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

static const char *ltssm_str(int ltssm, int show_minor)
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
		s[p].ltssm_str = ltssm_str(s[i].ltssm, 0);
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
		if (status[i].pci_dev)
			free(status[i].pci_dev);

		if (status[i].class_devices)
			free(status[i].class_devices);
	}

	free(status);
}

/**
 * @brief Return a message coresponding to the last error
 *
 * This can be called after another switchtec function returned an error
 * to find out what caused the problem.
 */
const char *switchtec_strerror(void)
{
	const char *msg;

	switch (errno) {
	case 0: msg = platform_strerror(); break;

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

	default: msg = strerror(errno); break;
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
			     output, sizeof(output));
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
	uint32_t sub_cmd_id = MRPC_DIETEMP_SET_MEAS;
	uint32_t temp;

	ret = switchtec_cmd(dev, MRPC_DIETEMP, &sub_cmd_id,
			    sizeof(sub_cmd_id), NULL, 0);
	if (ret)
		return -100.0;

	sub_cmd_id = MRPC_DIETEMP_GET;
	ret = switchtec_cmd(dev, MRPC_DIETEMP, &sub_cmd_id,
			    sizeof(sub_cmd_id), &temp, sizeof(temp));
	if (ret)
		return -100.0;

	return temp / 100.;
}

int switchtec_bind_info(struct switchtec_dev *dev,
			struct switchtec_bind_status_out *status, int phy_port)
{
	int ret;

	struct switchtec_bind_status_in sub_cmd_id = {
		.sub_cmd = MRPC_PORT_INFO,
		.phys_port_id = phy_port
	};

	ret = switchtec_cmd(dev, MRPC_PORTPARTP2P, &sub_cmd_id,
			    sizeof(sub_cmd_id), status, sizeof(*status));

	if (ret)
		return ret;

	return 0;
}

int switchtec_bind(struct switchtec_dev *dev, int par_id, int log_port,
		   int phy_port)
{
	int ret;
	uint32_t output;

	struct switchtec_bind_in sub_cmd_id = {
		.sub_cmd = MRPC_PORT_BIND,
		.par_id = par_id,
		.log_port_id = log_port,
		.phys_port_id = phy_port
	};

	ret = switchtec_cmd(dev, MRPC_PORTPARTP2P, &sub_cmd_id,
			    sizeof(sub_cmd_id), &output, sizeof(output));

	if (ret)
		return ret;

	return 0;
}

int switchtec_unbind(struct switchtec_dev *dev, int par_id, int log_port)
{
	int ret;
	uint32_t output;

	struct switchtec_unbind_in sub_cmd_id = {
		.sub_cmd = MRPC_PORT_UNBIND,
		.par_id = par_id,
		.log_port_id = log_port,
		.opt = 2
	};

	ret = switchtec_cmd(dev, MRPC_PORTPARTP2P, &sub_cmd_id,
			    sizeof(sub_cmd_id), &output, sizeof(output));

	if (ret)
		return ret;

	return 0;
}
/**@}*/
