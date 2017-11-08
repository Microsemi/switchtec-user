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

static void win_perror(const char *msg)
{
	char errmsg[500] = "";
	int err = GetLastError();

	FormatMessage (FORMAT_MESSAGE_FROM_SYSTEM |
		       FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err,
		       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		       errmsg, sizeof (errmsg), NULL);

	if (!strlen(errmsg))
		sprintf(errmsg, "Error %d", err);

	fprintf(stderr, "%s: %s\n", msg, errmsg);
}

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

static BOOL get_path(HDEVINFO devinfo, SP_DEVICE_INTERFACE_DATA *deviface,
		     SP_DEVINFO_DATA *devdata, char *path, size_t path_size)
{
	DWORD size;
	SP_DEVICE_INTERFACE_DETAIL_DATA *devdetail;
	BOOL status = TRUE;

	devdata->cbSize = sizeof(SP_DEVINFO_DATA);

	SetupDiGetDeviceInterfaceDetail(devinfo, deviface, NULL, 0, &size,
					NULL);

	devdetail = malloc(size);
	if (!devdetail) {
		win_perror("Enumeration");
		return FALSE;
	}

	devdetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

	status = SetupDiGetDeviceInterfaceDetail(devinfo, deviface, devdetail,
						 size, NULL, devdata);
	if (!status) {
		win_perror("SetupDiGetDeviceInterfaceDetail");
		goto out;
	}

	strcpy_s(path, path_size, devdetail->DevicePath);

out:
	free(devdetail);
	return status;
}

static BOOL get_pci_address(HDEVINFO devinfo, SP_DEVINFO_DATA *devdata,
			    int *bus, int *dev, int *func)
{
	BOOL status;
	int ret;
	char loc[256];

	status = SetupDiGetDeviceRegistryProperty(devinfo, devdata,
			SPDRP_LOCATION_INFORMATION, NULL,
			(BYTE *)loc, sizeof(loc), NULL);
	if (!status) {
		win_perror("SetupDiGetDeviceRegistryProperty (LOC)");
		return FALSE;
	}

	ret = sscanf(loc, "PCI bus %d, device %d, function %d", bus, dev, func);
	if (ret != 3) {
		fprintf(stderr, "Error parsing PCI BUS: '%s'\n", loc);
		return FALSE;
	}

	return TRUE;
}

static void get_pci_address_str(HDEVINFO devinfo, SP_DEVINFO_DATA *devdata,
				char *res, size_t res_size)
{
	BOOL status;
	int bus, dev, func;

	status = get_pci_address(devinfo, devdata, &bus, &dev, &func);
	if (!status)
		snprintf(res, res_size, "??:??.?");
	else
		snprintf(res, res_size, "%02x:%02x.%x", bus, dev, func);
}

static void get_description(HDEVINFO devinfo, SP_DEVINFO_DATA *devdata,
			    char *res, size_t res_size)
{
	SetupDiGetDeviceRegistryProperty(devinfo, devdata,
			SPDRP_DEVICEDESC, NULL,(BYTE *)res, res_size, NULL);
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
	HDEVINFO devinfo;
	SP_DEVICE_INTERFACE_DATA deviface;
	SP_DEVINFO_DATA devdata;
	struct switchtec_device_info *dl;

	BOOL status;
	DWORD idx = 0;
	DWORD cnt = 0;

	dl = *devlist = calloc(count_devices(),
			  sizeof(struct switchtec_device_info));
	if (!dl) {
		errno = ENOMEM;
		return -errno;
	}

	devinfo = SetupDiGetClassDevs(&SWITCHTEC_INTERFACE_GUID,
				      NULL, NULL, DIGCF_DEVICEINTERFACE |
				      DIGCF_PRESENT);

	deviface.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	while (SetupDiEnumDeviceInterfaces(devinfo, NULL,
					   &SWITCHTEC_INTERFACE_GUID,
					   idx, &deviface))
	{
		snprintf(dl[cnt].name, sizeof(dl[cnt].name),
			 "switchtec%ld", idx++);

		status = get_path(devinfo, &deviface,  &devdata,
				  dl[cnt].path, sizeof(dl[cnt].path));
		if (!status)
			continue;

		get_pci_address_str(devinfo, &devdata, dl[cnt].pci_dev,
				    sizeof(dl[cnt].pci_dev));
		get_description(devinfo, &devdata, dl[cnt].desc,
				sizeof(dl[cnt].desc));

		cnt++;
	}

	SetupDiDestroyDeviceInfoList(devinfo);

	return cnt;
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
