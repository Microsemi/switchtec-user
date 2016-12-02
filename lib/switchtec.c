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

#include <fcntl.h>
#include <unistd.h>

int switchtec_open(const char * path)
{
	return open(path, O_RDWR | O_CLOEXEC);
}

void switchtec_close(int fd)
{
	close(fd);
}
