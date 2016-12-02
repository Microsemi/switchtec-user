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

#include "switchtec/switchtec.h"
#include "switchtec/mrpc.h"

#include <fcntl.h>
#include <unistd.h>

#include <errno.h>
#include <string.h>

int switchtec_open(const char * path)
{
	return open(path, O_RDWR | O_CLOEXEC);
}

void switchtec_close(int fd)
{
	close(fd);
}

int switchtec_submit_cmd(int fd, uint32_t cmd, const void *payload,
			 size_t payload_len)
{
	int ret;
	char buf[payload_len + sizeof(cmd)];

	memcpy(buf, &cmd, sizeof(cmd));
	memcpy(&buf[sizeof(cmd)], payload, payload_len);

	ret = write(fd, buf, sizeof(buf));

	if (ret < 0)
		return ret;

	if (ret != sizeof(buf))
		return -EIO;

	return 0;
}

int switchtec_read_resp(int fd, void *resp, size_t resp_len)
{
	int32_t ret;
	char buf[sizeof(uint32_t) + resp_len];

	ret = read(fd, buf, sizeof(buf));

	if (ret < 0)
		return ret;

	if (ret != sizeof(buf))
		return -EIO;

	memcpy(&ret, buf, sizeof(ret));
	memcpy(resp, &buf[sizeof(ret)], resp_len);

	return ret;
}

int switchtec_cmd(int fd,  uint32_t cmd, const void *payload,
		  size_t payload_len, void *resp, size_t resp_len)
{
	int ret;

	ret = switchtec_submit_cmd(fd, cmd, payload, payload_len);
	if (ret < 0)
		return ret;

	return switchtec_read_resp(fd, resp, resp_len);
}

int switchtec_echo(int fd, uint32_t input, uint32_t *output)
{
	return switchtec_cmd(fd, MRPC_ECHO, &input, sizeof(input),
			     output, sizeof(output));
}
