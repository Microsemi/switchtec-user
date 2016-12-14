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

#include <fcntl.h>
#include <unistd.h>
#include <endian.h>
#include <dirent.h>
#include <libgen.h>

#include <errno.h>
#include <string.h>
#include <stdio.h>

static const char *sys_path = "/sys/class/switchtec";

struct switchtec_dev *switchtec_open(const char * path)
{
	struct switchtec_dev * dev;

	dev = malloc(sizeof(*dev));
	if (dev == NULL)
		return dev;

	dev->fd = open(path, O_RDWR | O_CLOEXEC);
	if (dev->fd < 0)
		goto err_free;

	return dev;

err_free:
	free(dev);
	return NULL;
}

void switchtec_close(struct switchtec_dev *dev)
{
	close(dev->fd);
	free(dev);
}

static int scan_dev_filter(const struct dirent *d)
{
	if (d->d_name[0] == '.')
		return 0;

	return 1;
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

		free(devices[n]);
	}

	free(devices);
	return n;
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

	if (ret != sizeof(buf))
		return -EIO;

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

	if (ret != sizeof(buf))
		return -EIO;

	memcpy(&ret, buf, sizeof(ret));

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
