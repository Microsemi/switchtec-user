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

#ifndef LIBSWITCHTEC_SWITCHTEC_PRIV_H
#define LIBSWITCHTEC_SWITCHTEC_PRIV_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

struct switchtec_dev {
	int fd;
	int partition;
	char name[PATH_MAX];

	void *gas_map;
	size_t gas_map_size;
};

static inline void version_to_string(uint32_t version, char *buf, size_t buflen)
{
	int major = version >> 24;
	int minor = (version >> 16) & 0xFF;
	int build = version & 0xFFFF;

	snprintf(buf, buflen, "%x.%02x B%03X", major, minor, build);
}

#endif
