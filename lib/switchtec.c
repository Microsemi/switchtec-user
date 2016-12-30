/*
 * Microsemi Switchtec(tm) PCIe Management Library
 * Copyright (c) 2016, Microsemi Corporation
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include "switchtec_priv.h"

#include "switchtec/switchtec.h"
#include "switchtec/mrpc.h"
#include "switchtec/errors.h"
#include "switchtec/log.h"

#include <linux/switchtec_ioctl.h>

#include <fcntl.h>
#include <unistd.h>
#include <endian.h>
#include <dirent.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *sys_path = "/sys/class/switchtec";

static int dev_to_sysfs_path(struct switchtec_dev *dev, const char *suffix,
			     char *buf, size_t buflen)
{
	int ret;
	struct stat stat;

	ret = fstat(dev->fd, &stat);
	if (ret < 0)
		return ret;

	snprintf(buf, buflen,
		 "/sys/dev/char/%d:%d/%s",
		 major(stat.st_rdev), minor(stat.st_rdev), suffix);

	return 0;
}

static int sysfs_read_str(const char *path, char *buf, size_t buflen)
{
	int ret;
	int fd;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		return -1;

	ret = read(fd, buf, buflen);

	close(fd);

	return ret;
}

static int sysfs_read_int(const char *path, int base)
{
	int ret;
	char buf[64];

	ret = sysfs_read_str(path, buf, sizeof(buf));
	if (ret < 0)
		return ret;

	return strtol(buf, NULL, base);
}

static int check_switchtec_device(struct switchtec_dev *dev)
{
	int ret;
	char syspath[PATH_MAX];

	ret = dev_to_sysfs_path(dev, "device/switchtec", syspath,
				sizeof(syspath));
	if (ret)
		return ret;

	ret = access(syspath, F_OK);
	if (ret)
		errno = ENOTTY;

	return ret;
}

static int get_partition(struct switchtec_dev *dev)
{
	int ret;
	char syspath[PATH_MAX];

	ret = dev_to_sysfs_path(dev, "partition", syspath,
				sizeof(syspath));
	if (ret)
		return ret;

	dev->partition = sysfs_read_int(syspath, 10);
	if (dev->partition < 0)
		return dev->partition;

	return 0;
}

struct switchtec_dev *switchtec_open(const char * path)
{
	struct switchtec_dev *dev;

	dev = malloc(sizeof(*dev));
	if (dev == NULL)
		return dev;

	dev->fd = open(path, O_RDWR | O_CLOEXEC);
	if (dev->fd < 0)
		goto err_free;

	if (check_switchtec_device(dev))
		goto err_close_free;

	if (get_partition(dev))
		goto err_close_free;

	snprintf(dev->name, sizeof(dev->name), "%s", path);

	return dev;

err_close_free:
	close(dev->fd);
err_free:
	free(dev);
	return NULL;
}

void switchtec_close(struct switchtec_dev *dev)
{
	if (dev == NULL)
		return;

	close(dev->fd);
	free(dev);
}

__attribute__ ((pure))
const char *switchtec_name(struct switchtec_dev *dev)
{
	return dev->name;
}

__attribute__ ((pure))
int switchtec_fd(struct switchtec_dev *dev)
{
	return dev->fd;
}

__attribute__ ((pure))
int switchtec_partition(struct switchtec_dev *dev)
{
	return dev->partition;
}

static int scan_dev_filter(const struct dirent *d)
{
	if (d->d_name[0] == '.')
		return 0;

	return 1;
}

static void get_device_str(const char *path, const char *file,
			   char *buf, size_t buflen)
{
	char sysfs_path[PATH_MAX];
	int ret;

	snprintf(sysfs_path, sizeof(sysfs_path), "%s/%s",
		 path, file);

	ret = sysfs_read_str(sysfs_path, buf, buflen);
	if (ret < 0)
		snprintf(buf, buflen, "unknown");

	buf[strcspn(buf, "\n")] = 0;
}

static void get_fw_version(const char *path, char *buf, size_t buflen)
{
	char sysfs_path[PATH_MAX];
	int fw_ver;

	snprintf(sysfs_path, sizeof(sysfs_path), "%s/fw_version",
		 path);

	fw_ver = sysfs_read_int(sysfs_path, 16);

	if (fw_ver < 0)
		snprintf(buf, buflen, "unknown");
	else
		version_to_string(fw_ver, buf, buflen);
}

int switchtec_list(struct switchtec_device_info **devlist)
{
	struct dirent **devices;
	int i, n;
	char link_path[PATH_MAX];
	char pci_path[PATH_MAX];
	struct switchtec_device_info *dl;

	n = scandir(sys_path, &devices, scan_dev_filter, alphasort);
	if (n <= 0)
		return n;

	dl = *devlist = calloc(n, sizeof(struct switchtec_device_info));

	if (dl == NULL) {
		for (i = 0; i < n; i++)
			free(devices[n]);
		free(devices);
		errno=ENOMEM;
		return -errno;
	}

	for (i = 0; i < n; i++) {
		snprintf(dl[i].name, sizeof(dl[i].name),
			 "%s", devices[i]->d_name);
		snprintf(dl[i].path, sizeof(dl[i].path),
			 "/dev/%s", devices[i]->d_name);

		snprintf(link_path, sizeof(link_path), "%s/%s/device",
			 sys_path, devices[i]->d_name);

		if (readlink(link_path, pci_path, sizeof(pci_path)) > 0)
			snprintf(dl[i].pci_dev, sizeof(dl[i].pci_dev),
				 "%s", basename(pci_path));
		else
			snprintf(dl[i].pci_dev, sizeof(dl[i].pci_dev),
				 "unknown pci device");


		snprintf(link_path, sizeof(link_path), "%s/%s",
			 sys_path, devices[i]->d_name);

		get_device_str(link_path, "product_id", dl[i].product_id,
			       sizeof(dl[i].product_id));
		get_device_str(link_path, "product_revision",
			       dl[i].product_rev, sizeof(dl[i].product_rev));
		get_fw_version(link_path, dl[i].fw_version,
			       sizeof(dl[i].fw_version));

		free(devices[n]);
	}

	free(devices);
	return n;
}

int switchtec_get_fw_version(struct switchtec_dev *dev, char *buf,
			     size_t buflen)
{
	int ret;
	uint32_t version;
	char syspath[PATH_MAX];

	ret = dev_to_sysfs_path(dev, "fw_version", syspath, sizeof(syspath));
	if (ret)
		return ret;

	version = sysfs_read_int(syspath, 16);
	if (version < 0)
		return version;

	version_to_string(version, buf, buflen);

	return 0;
}

int switchtec_submit_cmd(struct switchtec_dev *dev, uint32_t cmd,
			 const void *payload, size_t payload_len)
{
	int ret;
	char buf[payload_len + sizeof(cmd)];

	cmd = htole32(cmd);
	memcpy(buf, &cmd, sizeof(cmd));
	memcpy(&buf[sizeof(cmd)], payload, payload_len);

	ret = write(dev->fd, buf, sizeof(buf));

	if (ret < 0)
		return ret;

	if (ret != sizeof(buf)) {
		errno = EIO;
		return -errno;
	}

	return 0;
}

int switchtec_read_resp(struct switchtec_dev *dev, void *resp,
			size_t resp_len)
{
	int32_t ret;
	char buf[sizeof(uint32_t) + resp_len];

	ret = read(dev->fd, buf, sizeof(buf));

	if (ret < 0)
		return ret;

	if (ret != sizeof(buf)) {
		errno = EIO;
		return -errno;
	}

	memcpy(&ret, buf, sizeof(ret));
	if (ret)
		errno = ret;

	if (!resp)
		return ret;

	memcpy(resp, &buf[sizeof(ret)], resp_len);

	return ret;
}

int switchtec_cmd(struct switchtec_dev *dev,  uint32_t cmd,
		  const void *payload, size_t payload_len, void *resp,
		  size_t resp_len)
{
	int ret;

	ret = switchtec_submit_cmd(dev, cmd, payload, payload_len);
	if (ret < 0)
		return ret;

	return switchtec_read_resp(dev, resp, resp_len);
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

static const char *ltssm_str(int ltssm)
{
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
	case 0x0001: return "Polling (INACTIVE)";
	case 0x0101: return "Polling (ACTIVE_ENTRY)";
	case 0x0201: return "Polling (ACTIVE)";
	case 0x0301: return "Polling (CFG)";
	case 0x0401: return "Polling (COMP)";
	case 0x0501: return "Polling (COMP_ENTRY)";
	case 0x0601: return "Polling (COMP_EIOS)";
	case 0x0701: return "Polling (COMP_EIOS_ACK)";
	case 0x0801: return "Polling (COMP_IDLE)";
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
	case 0x0003: return "L0 (INACTIVE)";
	case 0x0103: return "L0 (L0)";
	case 0x0203: return "L0 (TX_EL_IDLE)";
	case 0x0303: return "L0 (TX_IDLE_MIN)";
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
	case 0x0005: return "Disable (INACTIVE)";
	case 0x0105: return "Disable (DISABLE0)";
	case 0x0205: return "Disable (DISABLE1)";
	case 0x0305: return "Disable (DISABLE2)";
	case 0x0405: return "Disable (DISABLE3)";
	case 0x0006: return "Loop Back (INACTIVE)";
	case 0x0106: return "Loop Back (ENTRY)";
	case 0x0206: return "Loop Back (ENTRY_EXIT)";
	case 0x0306: return "Loop Back (EIOS)";
	case 0x0406: return "Loop Back (EIOS_ACK)";
	case 0x0506: return "Loop Back (IDLE)";
	case 0x0606: return "Loop Back (ACTIVE)";
	case 0x0706: return "Loop Back (EXIT0)";
	case 0x0806: return "Loop Back (EXIT1)";
	case 0x0007: return "Hot Reset (INACTIVE)";
	case 0x0107: return "Hot Reset (HOT_RESET)";
	case 0x0207: return "Hot Reset (MASTER_UP)";
	case 0x0307: return "Hot Reset (MASTER_DOWN)";
	case 0x0008: return "TxL0s (INACTIVE)";
	case 0x0108: return "TxL0s (IDLE)";
	case 0x0208: return "TxL0s (T0_L0)";
	case 0x0308: return "TxL0s (FTS0)";
	case 0x0408: return "TxL0s (FTS1)";
	case 0x0009: return "L1 (INACTIVE)";
	case 0x0109: return "L1 (IDLE)";
	case 0x0209: return "L1 (SUBSTATE)";
	case 0x0309: return "L1 (SPD_CHG1)";
	case 0x0409: return "L1 (T0_L0)";
	case 0x000A: return "L2 (INACTIVE)";
	case 0x010A: return "L2 (IDLE)";
	case 0x020A: return "L2 (TX_WAKE0)";
	case 0x030A: return "L2 (TX_WAKE1)";
	case 0x040A: return "L2 (EXIT)";
	case 0x050A: return "L2 (SPEED)";
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
	if (a->stack != b->stack)
		return a->stack - b->stack;
	return a->stk_id;
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
	int i;
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
		if (ports[i].par_id > SWITCHTEC_MAX_PORTS)
			continue;
		nr_ports++;
	}

	s = *status = calloc(nr_ports, sizeof(*s));
	if (!s)
		return -ENOMEM;

	for (i = 0; i < SWITCHTEC_MAX_PORTS; i++) {
		if (ports[i].par_id > SWITCHTEC_MAX_PORTS)
			continue;

		s[i].port.partition = ports[i].par_id;
		s[i].port.stack = ports[i].stk_id >> 4;
		s[i].port.upstream = ports[i].usp_flag;
		s[i].port.stk_id = ports[i].stk_id & 0xF;
		s[i].port.phys_id = ports[i].phys_port_id;
		s[i].port.log_id = ports[i].log_port_id;

		s[i].cfg_lnk_width = ports[i].cfg_lnk_width;
		s[i].neg_lnk_width = ports[i].neg_lnk_width;
		s[i].link_up = ports[i].linkup_linkrate >> 7;
		s[i].link_rate = ports[i].linkup_linkrate & 0x7F;
		s[i].ltssm = le16toh(ports[i].LTSSM);
		s[i].ltssm_str = ltssm_str(s[i].ltssm);
	}

	qsort(s, nr_ports, sizeof(*s), compare_status);

	return nr_ports;
}

void switchtec_perror(const char *str)
{
	const char *msg;

	switch (errno) {

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

int switchtec_pff_to_port(struct switchtec_dev *dev, int pff,
			  int *partition, int *port)
{
	int ret;
	struct switchtec_ioctl_pff_port p;

	p.pff = pff;
	ret = ioctl(dev->fd, SWITCHTEC_IOCTL_PFF_TO_PORT, &p);
	if (ret)
		return ret;

	if (partition)
		*partition = p.partition;
	if (port)
		*port = p.port;

	return 0;
}

int switchtec_port_to_pff(struct switchtec_dev *dev, int partition,
			  int port, int *pff)
{
	int ret;
	struct switchtec_ioctl_pff_port p;

	p.port = port;
	p.partition = partition;

	ret = ioctl(dev->fd, SWITCHTEC_IOCTL_PORT_TO_PFF, &p);
	if (ret)
		return ret;

	if (pff)
		*pff = p.pff;

	return 0;
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
