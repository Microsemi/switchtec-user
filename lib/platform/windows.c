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
#include "switchtec/gas.h"
#include "switchtec/utils.h"
#include "../switchtec_priv.h"

#ifdef __WINDOWS__
#include "windows/switchtec_public.h"

#include <setupapi.h>

#include <errno.h>
#include <stdio.h>

struct switchtec_windows {
	struct switchtec_dev dev;
	HANDLE hdl;
};

#define to_switchtec_windows(d)  \
	((struct switchtec_windows *) \
	 ((char *)d - offsetof(struct switchtec_windows, dev)))

void platform_perror(const char *msg)
{
	char errmsg[500] = "";
	int err = GetLastError();

	FormatMessage (FORMAT_MESSAGE_FROM_SYSTEM |
		       FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err,
		       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		       errmsg, sizeof (errmsg), NULL);

	if (!strlen(errmsg))
		sprintf(errmsg, "Error %d", err);

	fprintf(stderr, "%s: %s", msg, errmsg);
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

	return count - 1;
}

static BOOL get_path(HDEVINFO devinfo, SP_DEVICE_INTERFACE_DATA *deviface,
		     SP_DEVINFO_DATA *devdata, char *path, size_t path_size)
{
	DWORD size;
	SP_DEVICE_INTERFACE_DETAIL_DATA *devdetail;
	BOOL status = TRUE;
	char *hash;

	devdata->cbSize = sizeof(SP_DEVINFO_DATA);

	SetupDiGetDeviceInterfaceDetail(devinfo, deviface, NULL, 0, &size,
					NULL);

	devdetail = malloc(size);
	if (!devdetail) {
		perror("Enumeration");
		return FALSE;
	}

	devdetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

	status = SetupDiGetDeviceInterfaceDetail(devinfo, deviface, devdetail,
						 size, NULL, devdata);
	if (!status) {
		platform_perror("SetupDiGetDeviceInterfaceDetail");
		goto out;
	}

	strcpy_s(path, path_size, devdetail->DevicePath);

	/* Chop off the GUID */
	hash = strrchr(path, '#');
	if (hash)
		*hash = 0;

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
		platform_perror("SetupDiGetDeviceRegistryProperty (LOC)");
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

/*
 * Sigh... Mingw doesn't define this API yet in it's header and the library
 * only has the WCHAR version.
 */
WINSETUPAPI WINBOOL WINAPI SetupDiGetDevicePropertyW(HDEVINFO DeviceInfoSet,
	PSP_DEVINFO_DATA DeviceInfoData, const DEVPROPKEY *PropertyKey,
	DEVPROPTYPE *PropertyType, PBYTE PropertyBuffer,
	DWORD PropertyBufferSize, PDWORD RequiredSize,
	DWORD Flags);

static void get_property(HDEVINFO devinfo, SP_DEVINFO_DATA *devdata,
			 const DEVPROPKEY *propkey, char *res, size_t res_size)
{
	DEVPROPTYPE ptype;
	WCHAR buf[res_size];

	SetupDiGetDevicePropertyW(devinfo, devdata, propkey, &ptype,
				 (PBYTE)buf, sizeof(buf), NULL, 0);
	wcstombs(res, buf, res_size);
}

static void get_fw_property(HDEVINFO devinfo, SP_DEVINFO_DATA *devdata,
			    char *res, size_t res_size)
{
	char buf[16];
	long fw_ver;

	get_property(devinfo, devdata, &SWITCHTEC_PROP_FW_VERSION,
		     buf, sizeof(buf));

	fw_ver = strtol(buf, NULL, 16);

	if (fw_ver < 0)
		snprintf(res, res_size, "unknown");
	else
		version_to_string(fw_ver, res, res_size);
}

static void append_guid(const char *path, char *path_with_guid, size_t bufsize,
			const GUID *guid)
{
	snprintf(path_with_guid, bufsize,
		 "%s#{%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
		 path, guid->Data1, guid->Data2, guid->Data3,
		 guid->Data4[0], guid->Data4[1], guid->Data4[2],
		 guid->Data4[3], guid->Data4[4], guid->Data4[5],
		 guid->Data4[6], guid->Data4[7]);
}

#ifdef __CHECKER__
#define __force __attribute__((force))
#else
#define __force
#endif

static BOOL map_gas(struct switchtec_windows *wdev)
{
	BOOL status;
	struct switchtec_gas_map map;

	status = DeviceIoControl(wdev->hdl, IOCTL_SWITCHTEC_GAS_MAP, NULL, 0,
				 &map, sizeof(map), NULL, NULL);
	if (!status)
		return status;

	wdev->dev.gas_map = (gasptr_t __force)map.gas;
	wdev->dev.gas_map_size = map.length;
	return TRUE;
}

static void unmap_gas(struct switchtec_windows *wdev)
{
	struct switchtec_gas_map map = {
		.gas = (void * __force)wdev->dev.gas_map,
		.length = wdev->dev.gas_map_size,
	};

	DeviceIoControl(wdev->hdl, IOCTL_SWITCHTEC_GAS_UNMAP, &map, sizeof(map),
			NULL, 0, NULL, NULL);
}

static void set_partition_info(struct switchtec_dev *dev)
{
	dev->partition = gas_reg_read8(dev, top.partition_id);
	dev->partition_count = gas_reg_read8(dev, top.partition_count);
}

static void windows_close(struct switchtec_dev *dev)
{
	struct switchtec_windows *wdev = to_switchtec_windows(dev);

	if (!dev)
		return;

	unmap_gas(wdev);
	CloseHandle(wdev->hdl);
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
	if (devinfo == INVALID_HANDLE_VALUE)
		return 0;

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

		get_property(devinfo, &devdata, &SWITCHTEC_PROP_PRODUCT_ID,
			     dl[cnt].product_id, sizeof(dl[cnt].product_id));
		get_property(devinfo, &devdata, &SWITCHTEC_PROP_PRODUCT_REV,
			     dl[cnt].product_rev, sizeof(dl[cnt].product_rev));
		get_fw_property(devinfo, &devdata, dl[cnt].fw_version,
				sizeof(dl[cnt].fw_version));
		cnt++;
	}

	SetupDiDestroyDeviceInfoList(devinfo);

	return cnt;
}

static int windows_get_fw_version(struct switchtec_dev *dev, char *buf,
				  size_t buflen)
{
	long long ver;

	ver = gas_reg_read32(dev, sys_info.firmware_version);
	version_to_string(ver, buf, buflen);

	return 0;
}

static int windows_cmd(struct switchtec_dev *dev, uint32_t cmd,
		       const void *payload, size_t payload_len, void *resp,
		       size_t resp_len)
{
	struct switchtec_windows *wdev = to_switchtec_windows(dev);
	BOOL status;
	int ret;

	struct switchtec_mrpc_cmd *mcmd;
	struct switchtec_mrpc_result *mres;
	size_t mcmd_len, mres_len;

	mcmd_len = offsetof(struct switchtec_mrpc_cmd, data) + payload_len;
	mres_len = offsetof(struct switchtec_mrpc_result, data) + resp_len;

	mcmd = calloc(1, mcmd_len);
	if (!mcmd)
		return -errno;

	mres = calloc(1, mres_len);
	if (!mres) {
		free(mcmd);
		return -errno;
	}

	mcmd->cmd = cmd;
	memcpy(mcmd->data, payload, payload_len);

	status = DeviceIoControl(wdev->hdl, IOCTL_SWITCHTEC_MRPC,
				 mcmd, (DWORD)mcmd_len,
				 mres, (DWORD)mres_len,
				 NULL, NULL);
	if (!status) {
		ret = -EIO;
		goto free_and_exit;
	}

	if (resp)
		memcpy(resp, mres->data, resp_len);

	ret = mres->status;

free_and_exit:
	free(mres);
	free(mcmd);
	return ret;
}

static int windows_pff_to_port(struct switchtec_dev *dev, int pff,
			       int *partition, int *port)
{
	int i, part;
	uint32_t reg;
	struct part_cfg_regs *pcfg;

	*port = -1;

	for (part = 0; part < dev->partition_count; part++) {
		pcfg = &dev->gas_map->part_cfg[part];
		*partition = part;

		reg = gas_read32(dev, &pcfg->usp_pff_inst_id);
		if (reg == pff) {
			*port = 0;
			return 0;
		}

		reg = gas_read32(dev, &pcfg->vep_pff_inst_id);
		if (reg == pff) {
			*port = SWITCHTEC_PFF_PORT_VEP;
			return 0;
		}

		for (i = 0; i < ARRAY_SIZE(pcfg->dsp_pff_inst_id); i++) {
			reg = gas_read32(dev, &pcfg->dsp_pff_inst_id[i]);
			if (reg != pff)
				continue;

			*port = i + 1;
			break;
		}

		if (*port != -1)
			return 0;
	}

	errno = EINVAL;
	return -EINVAL;
}

static int windows_port_to_pff(struct switchtec_dev *dev, int partition,
			       int port, int *pff)
{
	struct part_cfg_regs *pcfg;

	if (partition < 0) {
		partition = dev->partition;
	} else if (partition >= dev->partition_count) {
		errno = EINVAL;
		return -errno;
	}

	pcfg = &dev->gas_map->part_cfg[partition];

	switch(port) {
	case 0:
		*pff = gas_read32(dev, &pcfg->usp_pff_inst_id);
		break;
	case SWITCHTEC_PFF_PORT_VEP:
		*pff = gas_read32(dev, &pcfg->vep_pff_inst_id);
		break;
	default:
		if (port > ARRAY_SIZE(pcfg->dsp_pff_inst_id)) {
			errno = EINVAL;
			return -errno;
		}

		*pff = gas_read32(dev, &pcfg->dsp_pff_inst_id[port - 1]);
		break;
	}

	return 0;
}

static void set_fw_info_part(struct switchtec_dev *dev,
			     struct switchtec_fw_image_info *info,
			     struct partition_info __gas *pi)
{
	info->image_addr = gas_read32(dev, &pi->address);
	info->image_len = gas_read32(dev, &pi->length);
}

static int windows_flash_part(struct switchtec_dev *dev,
			      struct switchtec_fw_image_info *info,
			      enum switchtec_fw_image_type part)
{
	struct flash_info_regs __gas *fi = &dev->gas_map->flash_info;
	struct sys_info_regs __gas *si = &dev->gas_map->sys_info;
	uint32_t active_addr = -1;
	int val;

	memset(info, 0, sizeof(*info));

	switch (part) {
	case SWITCHTEC_FW_TYPE_IMG0:
		active_addr = gas_read32(dev, &fi->active_img.address);
		set_fw_info_part(dev, info, &fi->img0);

		val = gas_read16(dev, &si->img_running);
		if (val == SWITCHTEC_IMG0_RUNNING)
			info->active |= SWITCHTEC_FW_PART_RUNNING;
		break;

	case SWITCHTEC_FW_TYPE_IMG1:
		active_addr = gas_read32(dev, &fi->active_img.address);
		set_fw_info_part(dev, info, &fi->img1);

		val = gas_read16(dev, &si->img_running);
		if (val == SWITCHTEC_IMG1_RUNNING)
			info->active |= SWITCHTEC_FW_PART_RUNNING;
		break;

	case SWITCHTEC_FW_TYPE_DAT0:
		active_addr = gas_read32(dev, &fi->active_cfg.address);
		set_fw_info_part(dev, info, &fi->cfg0);

		val = gas_read16(dev, &si->cfg_running);
		if (val == SWITCHTEC_CFG0_RUNNING)
			info->active |= SWITCHTEC_FW_PART_RUNNING;
		break;


	case SWITCHTEC_FW_TYPE_DAT1:
		active_addr = gas_read32(dev, &fi->active_cfg.address);
		set_fw_info_part(dev, info, &fi->cfg1);

		val = gas_read16(dev, &si->cfg_running);
		if (val == SWITCHTEC_CFG1_RUNNING)
			info->active |= SWITCHTEC_FW_PART_RUNNING;
		break;

	case SWITCHTEC_FW_TYPE_NVLOG:
		set_fw_info_part(dev, info, &fi->nvlog);
		break;

	default:
		return -EINVAL;
	}

	if (info->image_addr == active_addr)
		info->active |= SWITCHTEC_FW_PART_ACTIVE;

	return 0;
}

static int windows_event_summary(struct switchtec_dev *dev,
				 struct switchtec_event_summary *sum)
{
	int i;
	uint32_t reg;

	memset(sum, 0, sizeof(*sum));

	sum->global = gas_reg_read32(dev, sw_event.global_summary);
	sum->part_bitmap = gas_reg_read64(dev, sw_event.part_event_bitmap);

	for (i = 0; i < dev->partition_count; i++) {
		reg = gas_reg_read32(dev, part_cfg[i].part_event_summary);
		sum->part[i] = reg;
		if (i == dev->partition)
			sum->local_part = reg;
	}

	for (i = 0; i < SWITCHTEC_MAX_PFF_CSR; i++) {
		reg = gas_reg_read16(dev, pff_csr[i].vendor_id);
		if (reg != MICROSEMI_VENDOR_ID)
			break;

		sum->pff[i] = gas_reg_read32(dev, pff_csr[i].pff_event_summary);
	}

	return 0;
}

static uint32_t __gas *global_ev_reg(struct switchtec_dev *dev,
				  size_t offset, int index)
{
	return (void __gas *)&dev->gas_map->sw_event + offset;
}

static uint32_t __gas *part_ev_reg(struct switchtec_dev *dev,
				size_t offset, int index)
{
	return (void __gas *)&dev->gas_map->part_cfg[index] + offset;
}

static uint32_t __gas *pff_ev_reg(struct switchtec_dev *dev,
			       size_t offset, int index)
{
	return (void __gas *)&dev->gas_map->pff_csr[index] + offset;
}

#define EV_GLB(i, r)[SWITCHTEC_GLOBAL_EVT_ ## i] = \
	{offsetof(struct sw_event_regs, r), global_ev_reg}
#define EV_PAR(i, r)[SWITCHTEC_PART_EVT_ ## i] = \
	{offsetof(struct part_cfg_regs, r), part_ev_reg}
#define EV_PFF(i, r)[SWITCHTEC_PFF_EVT_ ## i] = \
	{offsetof(struct pff_csr_regs, r), pff_ev_reg}

static const struct event_reg {
	size_t offset;
	uint32_t __gas *(*map_reg)(struct switchtec_dev *stdev,
				size_t offset, int index);
} event_regs[] = {
	EV_GLB(STACK_ERROR, stack_error_event_hdr),
	EV_GLB(PPU_ERROR, ppu_error_event_hdr),
	EV_GLB(ISP_ERROR, isp_error_event_hdr),
	EV_GLB(SYS_RESET, sys_reset_event_hdr),
	EV_GLB(FW_EXC, fw_exception_hdr),
	EV_GLB(FW_NMI, fw_nmi_hdr),
	EV_GLB(FW_NON_FATAL, fw_non_fatal_hdr),
	EV_GLB(FW_FATAL, fw_fatal_hdr),
	EV_GLB(TWI_MRPC_COMP, twi_mrpc_comp_hdr),
	EV_GLB(TWI_MRPC_COMP_ASYNC, twi_mrpc_comp_async_hdr),
	EV_GLB(CLI_MRPC_COMP, cli_mrpc_comp_hdr),
	EV_GLB(CLI_MRPC_COMP_ASYNC, cli_mrpc_comp_async_hdr),
	EV_GLB(GPIO_INT, gpio_interrupt_hdr),
	EV_GLB(GFMS, gfms_event_hdr),
	EV_PAR(PART_RESET, part_reset_hdr),
	EV_PAR(MRPC_COMP, mrpc_comp_hdr),
	EV_PAR(MRPC_COMP_ASYNC, mrpc_comp_async_hdr),
	EV_PAR(DYN_PART_BIND_COMP, dyn_binding_hdr),
	EV_PFF(AER_IN_P2P, aer_in_p2p_hdr),
	EV_PFF(AER_IN_VEP, aer_in_vep_hdr),
	EV_PFF(DPC, dpc_hdr),
	EV_PFF(CTS, cts_hdr),
	EV_PFF(HOTPLUG, hotplug_hdr),
	EV_PFF(IER, ier_hdr),
	EV_PFF(THRESH, threshold_hdr),
	EV_PFF(POWER_MGMT, power_mgmt_hdr),
	EV_PFF(TLP_THROTTLING, tlp_throttling_hdr),
	EV_PFF(FORCE_SPEED, force_speed_hdr),
	EV_PFF(CREDIT_TIMEOUT, credit_timeout_hdr),
	EV_PFF(LINK_STATE, link_state_hdr),
};

static uint32_t __gas *event_hdr_addr(struct switchtec_dev *dev,
				      enum switchtec_event_id e,
				      int index)
{
	size_t off;

	if (e < 0 || e >= SWITCHTEC_MAX_EVENTS)
		return NULL;

	off = event_regs[e].offset;

	if (event_regs[e].map_reg == part_ev_reg) {
		if (index < 0)
			index = dev->partition;
		else if (index >= dev->partition_count)
			return NULL;
	} else if (event_regs[e].map_reg == pff_ev_reg) {
		if (index < 0 || index >= SWITCHTEC_MAX_PFF_CSR)
			return NULL;
	}

	return event_regs[e].map_reg(dev, off, index);
}

static int event_ctl(struct switchtec_dev *dev, enum switchtec_event_id e,
		     int index, int flags, uint32_t data[5])
{
	int i;
	uint32_t __gas *reg;
	uint32_t hdr;

	reg = event_hdr_addr(dev, e, index);
	if (!reg) {
		errno = EINVAL;
		return -errno;
	}

	hdr = gas_read32(dev, reg);
	if (data)
		for (i = 0; i < 5; i++)
			data[i] = gas_read32(dev, &reg[i + 1]);

	if (!(flags & SWITCHTEC_EVT_FLAG_CLEAR))
		hdr &= ~SWITCHTEC_EVENT_CLEAR;
	if (flags & SWITCHTEC_EVT_FLAG_EN_POLL)
		hdr |= SWITCHTEC_EVENT_EN_IRQ;
	if (flags & SWITCHTEC_EVT_FLAG_EN_LOG)
		hdr |= SWITCHTEC_EVENT_EN_IRQ;
	if (flags & SWITCHTEC_EVT_FLAG_EN_CLI)
		hdr |= SWITCHTEC_EVENT_EN_CLI;
	if (flags & SWITCHTEC_EVT_FLAG_EN_FATAL)
		hdr |= SWITCHTEC_EVENT_FATAL;
	if (flags & SWITCHTEC_EVT_FLAG_DIS_POLL)
		hdr &= ~SWITCHTEC_EVENT_EN_IRQ;
	if (flags & SWITCHTEC_EVT_FLAG_DIS_LOG)
		hdr &= ~SWITCHTEC_EVENT_EN_LOG;
	if (flags & SWITCHTEC_EVT_FLAG_DIS_CLI)
		hdr &= ~SWITCHTEC_EVENT_EN_CLI;
	if (flags & SWITCHTEC_EVT_FLAG_DIS_FATAL)
		hdr &= ~SWITCHTEC_EVENT_FATAL;

	if (flags)
		gas_write32(dev, hdr, reg);

	return (hdr >> 5) & 0xFF;
}

static int windows_event_ctl(struct switchtec_dev *dev,
			     enum switchtec_event_id e,
			     int index, int flags,
			     uint32_t data[5])
{
	int nr_idxs;
	int ret = 0;

	if (e >= SWITCHTEC_MAX_EVENTS)
		goto einval;

	if (index == SWITCHTEC_EVT_IDX_ALL) {
		if (event_regs[e].map_reg == global_ev_reg)
			nr_idxs = 1;
		else if (event_regs[e].map_reg == part_ev_reg)
			nr_idxs = dev->partition_count;
		else if (event_regs[e].map_reg == pff_ev_reg)
			nr_idxs = gas_reg_read8(dev, top.pff_count);
		else
			goto einval;

		for (index = 0; index < nr_idxs; index++) {
			ret = event_ctl(dev, e, index, flags, data);
			if (ret < 0)
				return ret;
		}
	} else {
		ret = event_ctl(dev, e, index, flags, data);
	}

	return ret;

einval:
	errno = EINVAL;
	return -errno;
}

static int windows_event_wait(struct switchtec_dev *dev, int timeout_ms)
{
	struct switchtec_windows *wdev = to_switchtec_windows(dev);
	OVERLAPPED overlap = {
		.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL),
	};
	DWORD ret;
	DWORD transferred;
	BOOL error;

	errno = 0;

	if (!overlap.hEvent)
		return -1;

	DeviceIoControl(wdev->hdl, IOCTL_SWITCHTEC_WAIT_FOR_EVENT, NULL, 0,
			NULL, 0, NULL, &overlap);
	if (GetLastError() != ERROR_IO_PENDING)
		return -1;

	ret = WaitForSingleObject(overlap.hEvent, timeout_ms);
	if (ret == WAIT_TIMEOUT) {
		CancelIoEx(wdev->hdl, &overlap);
		return 0;
	} else if (ret) {
		return -1;
	}

	error = GetOverlappedResult(wdev->hdl, &overlap, &transferred, FALSE);
	if (!error)
		return -1;

	return 1;
}

static gasptr_t windows_gas_map(struct switchtec_dev *dev, int writeable,
				size_t *map_size)
{
	if (map_size)
		*map_size = dev->gas_map_size;

	return dev->gas_map;
}

static const struct switchtec_ops windows_ops = {
	.close = windows_close,
	.get_fw_version = windows_get_fw_version,
	.cmd = windows_cmd,
	.pff_to_port = windows_pff_to_port,
	.port_to_pff = windows_port_to_pff,
	.gas_map = windows_gas_map,
	.flash_part = windows_flash_part,
	.event_summary = windows_event_summary,
	.event_ctl = windows_event_ctl,
	.event_wait = windows_event_wait,
};

struct switchtec_dev *switchtec_open_by_path(const char *path)
{
	struct switchtec_windows *wdev;
	char path_with_guid[MAX_PATH];
	int idx;

	if (sscanf(path, "/dev/switchtec%d", &idx) == 1)
		return switchtec_open_by_index(idx);

	wdev = malloc(sizeof(*wdev));
	if (!wdev)
		return NULL;

	append_guid(path, path_with_guid, sizeof(path_with_guid),
		    &SWITCHTEC_INTERFACE_GUID);

	wdev->hdl = CreateFile(path_with_guid, GENERIC_READ | GENERIC_WRITE,
			       FILE_SHARE_READ | FILE_SHARE_WRITE,
			       NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);

	if (wdev->hdl == INVALID_HANDLE_VALUE)
		goto err_free;

	if (!map_gas(wdev))
		goto err_close;

	wdev->dev.ops = &windows_ops;

	set_partition_info(&wdev->dev);

	return &wdev->dev;

err_close:
	CloseHandle(wdev->hdl);
err_free:
	free(wdev);
	return NULL;
}

struct switchtec_dev *switchtec_open_by_index(int index)
{
	HDEVINFO devinfo;
	SP_DEVICE_INTERFACE_DATA deviface;
	SP_DEVINFO_DATA devdata;
	char path[MAX_PATH];
	struct switchtec_dev *dev = NULL;
	BOOL status;

	devinfo = SetupDiGetClassDevs(&SWITCHTEC_INTERFACE_GUID,
				      NULL, NULL, DIGCF_DEVICEINTERFACE |
				      DIGCF_PRESENT);
	if (devinfo == INVALID_HANDLE_VALUE)
		return NULL;

	deviface.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	status = SetupDiEnumDeviceInterfaces(devinfo, NULL,
					     &SWITCHTEC_INTERFACE_GUID,
					     index, &deviface);
	if (!status) {
		errno = ENODEV;
		goto out;
	}

	status = get_path(devinfo, &deviface,  &devdata,
			  path, sizeof(path));
	if (!status)
		goto out;

	dev = switchtec_open_by_path(path);

out:
	SetupDiDestroyDeviceInfoList(devinfo);
	return dev;
}

struct switchtec_dev *switchtec_open_by_pci_addr(int domain, int bus,
						 int device, int func)
{
	HDEVINFO devinfo;
	SP_DEVICE_INTERFACE_DATA deviface;
	SP_DEVINFO_DATA devdata;
	char path[MAX_PATH];
	struct switchtec_dev *dev = NULL;
	BOOL status;
	int dbus, ddevice, dfunc;
	int idx = 0;

	devinfo = SetupDiGetClassDevs(&SWITCHTEC_INTERFACE_GUID,
				      NULL, NULL, DIGCF_DEVICEINTERFACE |
				      DIGCF_PRESENT);
	if (devinfo == INVALID_HANDLE_VALUE)
		return NULL;

	deviface.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	while (SetupDiEnumDeviceInterfaces(devinfo, NULL,
					   &SWITCHTEC_INTERFACE_GUID,
					   idx++, &deviface))
	{
		status = get_path(devinfo, &deviface,  &devdata,
				  path, sizeof(path));
		if (!status)
			continue;

		get_pci_address(devinfo, &devdata, &dbus, &ddevice, &dfunc);
		if (dbus == bus && ddevice == device && dfunc == func) {
			dev = switchtec_open_by_path(path);
			break;
		}
	}

	if (!dev)
		errno = ENODEV;

	SetupDiDestroyDeviceInfoList(devinfo);
	return dev;
}

#endif
