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

#include "switchtec/switchtec.h"
#include "switchtec/portable.h"

#ifdef __WINDOWS__
#include "windows/switchtec_public.h"

#include <setupapi.h>

#include <errno.h>
#include <stdio.h>

static int count_devices(void)
{
	HDEVINFO devinfo;
	DWORD count = 0;
	SP_DEVICE_INTERFACE_DATA deviface;

	devinfo = SetupDiGetClassDevs(&SWITCHTEC_INTERFACE_GUID,
				      NULL, NULL, DIGCF_DEVICEINTERFACE |
				      DIGCF_PRESENT);

	deviface.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	while (SetupDiEnumDeviceInterfaces(devinfo, NULL,
					   &SWITCHTEC_INTERFACE_GUID,
					   count++, &deviface));

	return count-1;
}

struct switchtec_dev *switchtec_open(const char *path)
{
	errno = ENOSYS;
	return NULL;
}

void switchtec_close(struct switchtec_dev *dev)
{
}

int switchtec_list(struct switchtec_device_info **devlist)
{
	errno = ENOSYS;
	printf("count: %d\n", count_devices());
	return -errno;
}

int switchtec_get_fw_version(struct switchtec_dev *dev, char *buf,
			     size_t buflen)
{
	errno = ENOSYS;
	return -errno;
}

int switchtec_cmd(struct switchtec_dev *dev, uint32_t cmd,
		  const void *payload, size_t payload_len, void *resp,
		  size_t resp_len)
{
	errno = ENOSYS;
	return -errno;
}

int switchtec_get_devices(struct switchtec_dev *dev,
			  struct switchtec_status *status,
			  int ports)
{
	errno = ENOSYS;
	return -errno;
}

int switchtec_pff_to_port(struct switchtec_dev *dev, int pff,
			  int *partition, int *port)
{
	errno = ENOSYS;
	return -errno;
}

int switchtec_port_to_pff(struct switchtec_dev *dev, int partition,
			  int port, int *pff)
{
	errno = ENOSYS;
	return -errno;
}

int switchtec_flash_part(struct switchtec_dev *dev,
			 struct switchtec_fw_image_info *info,
			 enum switchtec_fw_image_type part)
{
	errno = ENOSYS;
	return -errno;
}

int switchtec_event_summary(struct switchtec_dev *dev,
			    struct switchtec_event_summary *sum)
{
	errno = ENOSYS;
	return -errno;
}

int switchtec_event_check(struct switchtec_dev *dev,
			  struct switchtec_event_summary *check,
			  struct switchtec_event_summary *res)
{
	errno = ENOSYS;
	return -errno;
}

int switchtec_event_ctl(struct switchtec_dev *dev,
			enum switchtec_event_id e,
			int index, int flags,
			uint32_t data[5])
{
	errno = ENOSYS;
	return -errno;
}

int switchtec_event_wait(struct switchtec_dev *dev, int timeout_ms)
{
	errno = ENOSYS;
	return -errno;
}

void *switchtec_gas_map(struct switchtec_dev *dev, int writeable,
			size_t *map_size)
{
	errno = ENOSYS;
	return MAP_FAILED;
}

void switchtec_gas_unmap(struct switchtec_dev *dev, void *map)
{
}

#endif
