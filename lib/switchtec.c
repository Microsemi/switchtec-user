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

#include "switchtec_priv.h"

#include "switchtec/switchtec.h"
#include "switchtec/mrpc.h"
#include "switchtec/errors.h"
#include "switchtec/log.h"
#include "switchtec/endian.h"

#include <string.h>
#include <unistd.h>
#include <errno.h>

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

__attribute__ ((pure))
const char *switchtec_name(struct switchtec_dev *dev)
{
	return dev->name;
}

__attribute__ ((pure))
int switchtec_partition(struct switchtec_dev *dev)
{
	return dev->partition;
}

int switchtec_echo(struct switchtec_dev *dev, uint32_t input,
		   uint32_t *output)
{
	return switchtec_cmd(dev, MRPC_ECHO, &input, sizeof(input),
			     output, sizeof(output));
}

int switchtec_hard_reset(struct switchtec_dev *dev)
{
	uint32_t subcmd = 0;

	return switchtec_cmd(dev, MRPC_RESET, &subcmd, sizeof(subcmd),
			     NULL, 0);
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

void switchtec_perror(const char *str)
{
	const char *msg;

	switch (errno) {
	case 0:
		platform_perror(str);
		return;

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

	default:
		perror(str);
		return;
	}

	fprintf(stderr, "%s: %s\n", str, msg);
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

float switchtec_die_temp(struct switchtec_dev *dev)
{
	int ret;
	uint32_t sub_cmd_id = MRPC_DIETEMP_SET_MEAS;
	uint32_t temp;

	ret = switchtec_cmd(dev, MRPC_DIETEMP, &sub_cmd_id,
			    sizeof(sub_cmd_id), NULL, 0);
	if (ret)
		return -1.0;

	sub_cmd_id = MRPC_DIETEMP_GET;
	ret = switchtec_cmd(dev, MRPC_DIETEMP, &sub_cmd_id,
			    sizeof(sub_cmd_id), &temp, sizeof(temp));
	if (ret)
		return -1.0;

	return temp / 100.;
}
