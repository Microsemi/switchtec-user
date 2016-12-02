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

#ifndef LIBSWITCHTEC_SWITCHTEC_H
#define LIBSWITCHTEC_SWITCHTEC_H

#include <linux/limits.h>

#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct switchtec_device {
	char name[256];
	char pci_dev[256];
	char path[PATH_MAX];
};

int switchtec_open(const char * path);
void switchtec_close(int fd);
int switchtec_list(struct switchtec_device **devlist);

int switchtec_submit_cmd(int fd, uint32_t cmd, const void *payload,
			 size_t payload_len);

int switchtec_read_resp(int fd, void *resp, size_t resp_len);

int switchtec_cmd(int fd,  uint32_t cmd, const void *payload,
		  size_t payload_len, void *resp, size_t resp_len);

int switchtec_echo(int fd, uint32_t input, uint32_t *output);

#ifdef __cplusplus
}
#endif

#endif
