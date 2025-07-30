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

#ifdef __linux__

#define SWITCHTEC_LIB_LINUX

#include "../switchtec_priv.h"
#include "switchtec/switchtec.h"
#include "switchtec/pci.h"
#include "switchtec/utils.h"
#include "mmap_gas.h"
#include "gasops.h"

#include <linux/switchtec_ioctl.h>

#include <unistd.h>
#include <fcntl.h>
#include <endian.h>
#include <dirent.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/sysmacros.h>
#include <glob.h>
#include <poll.h>

#include <errno.h>
#include <string.h>
#include <stddef.h>

static const char *sys_path = "/sys/class/switchtec";

struct switchtec_linux {
	struct switchtec_dev dev;
	int fd;
};

#define to_switchtec_linux(d)  \
	((struct switchtec_linux *) \
	 ((char *)d - offsetof(struct switchtec_linux, dev)))

const char *platform_strerror(void)
{
	return "Success";
}

static int dev_to_sysfs_path(struct switchtec_linux *ldev, const char *suffix,
			     char *buf, size_t buflen)
{
	int ret;
	struct stat stat;

	ret = fstat(ldev->fd, &stat);
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

static long long sysfs_read_int(const char *path, int base)
{
	int ret;
	char buf[64];

	ret = sysfs_read_str(path, buf, sizeof(buf));
	if (ret < 0)
		return ret;

	return strtoll(buf, NULL, base);
}

static int check_switchtec_device(struct switchtec_linux *ldev)
{
	int ret;
	char syspath[PATH_MAX];

	ret = dev_to_sysfs_path(ldev, "device/switchtec", syspath,
				sizeof(syspath));
	if (ret)
		return ret;

	ret = access(syspath, F_OK);
	if (ret)
		errno = ENOTTY;

	return ret;
}

static int get_partition(struct switchtec_linux *ldev)
{
	int ret;
	char syspath[PATH_MAX];

	ret = dev_to_sysfs_path(ldev, "partition", syspath,
				sizeof(syspath));
	if (ret)
		return ret;

	ldev->dev.partition = sysfs_read_int(syspath, 10);
	if (ldev->dev.partition < 0)
		return ldev->dev.partition;

	ret = dev_to_sysfs_path(ldev, "partition_count", syspath,
				sizeof(syspath));
	if (ret)
		return ret;

	ldev->dev.partition_count = sysfs_read_int(syspath, 10);
	if (ldev->dev.partition_count < 1)
		return -1;

	return 0;
}

static void linux_close(struct switchtec_dev *dev)
{
	struct switchtec_linux *ldev = to_switchtec_linux(dev);

	close(ldev->fd);
	free(ldev);
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
	if (ret < 0 || buf[0] == -1)
		snprintf(buf, buflen, "unknown");

	buf[strcspn(buf, "\n")] = 0;
}

static void get_fw_version(const char *path, char *buf, size_t buflen)
{
	char sysfs_path[PATH_MAX];
	int fw_ver;
	int ret;

	ret = snprintf(sysfs_path, sizeof(sysfs_path), "%s/fw_version",
		       path);
	if (ret >= sizeof(sysfs_path))
		goto unknown_version;

	fw_ver = sysfs_read_int(sysfs_path, 16);

	if (fw_ver < 0)
		goto unknown_version;

	version_to_string(fw_ver, buf, buflen);
	return;

unknown_version:
	snprintf(buf, buflen, "unknown");
}

int switchtec_list(struct switchtec_device_info **devlist)
{
	struct dirent **devices;
	int i, n;
	char link_path[PATH_MAX];
	char pci_path[PATH_MAX] = "";
	struct switchtec_device_info *dl;

	n = scandir(sys_path, &devices, scan_dev_filter, alphasort);
	if (n <= 0)
		return n;

	dl = *devlist = calloc(n, sizeof(struct switchtec_device_info));

	if (!dl) {
		for (i = 0; i < n; i++)
			free(devices[i]);
		free(devices);
		errno = ENOMEM;
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

		free(devices[i]);
	}

	free(devices);
	return n;
}

static int linux_get_device_id(struct switchtec_dev *dev)
{
	int ret;
	char link_path[PATH_MAX];
	struct switchtec_linux *ldev = to_switchtec_linux(dev);

	ret = dev_to_sysfs_path(ldev, "device/device", link_path,
				sizeof(link_path));
	if (ret)
		return ret;

	return sysfs_read_int(link_path, 16);
}

static int linux_get_fw_version(struct switchtec_dev *dev, char *buf,
				size_t buflen)
{
	int ret;
	long long version;
	char syspath[PATH_MAX];
	struct switchtec_linux *ldev = to_switchtec_linux(dev);

	ret = dev_to_sysfs_path(ldev, "fw_version", syspath, sizeof(syspath));
	if (ret)
		return ret;

	version = sysfs_read_int(syspath, 16);
	if (version < 0)
		return version;

	version_to_string(version, buf, buflen);

	return 0;
}

static int linux_get_device_version(struct switchtec_dev *dev, int *version_res)
{
	int ret;
	int version;
	char syspath[PATH_MAX];
	struct switchtec_linux *ldev = to_switchtec_linux(dev);

	ret = dev_to_sysfs_path(ldev, "device_version", syspath, sizeof(syspath));
	if (ret)
		return ret;

	version = sysfs_read_int(syspath, 16);
	if (version < 0)
		return version;
	
	memcpy(version_res, &version, sizeof(int));

	return 0;
}

static int submit_cmd(struct switchtec_linux *ldev, uint32_t cmd,
		      const void *payload, size_t payload_len)
{
	int ret;
	size_t bufsize = payload_len + sizeof(cmd);
	char buf[bufsize];

	cmd = htole32(cmd);
	memcpy(buf, &cmd, sizeof(cmd));
	memcpy(&buf[sizeof(cmd)], payload, payload_len);

	ret = write(ldev->fd, buf, bufsize);

	if (ret < 0)
		return ret;

	if (ret != bufsize) {
		errno = EIO;
		return -errno;
	}

	return 0;
}

static int read_resp(struct switchtec_linux *ldev, void *resp,
		     size_t resp_len)
{
	int32_t ret;
	size_t bufsize = sizeof(uint32_t) + resp_len;
	char buf[bufsize];

	ret = read(ldev->fd, buf, bufsize);

	if (ret < 0)
		return ret;

	if (ret != bufsize) {
		errno = EIO;
		return -errno;
	}

	memcpy(&ret, buf, sizeof(ret));

	if (ret) {
		errno = ENODATA;
		return -errno;
	}

	if (!resp)
		return ret;

	memcpy(resp, &buf[sizeof(ret)], resp_len);

	return ret;
}

static int linux_cmd(struct switchtec_dev *dev,  uint32_t cmd,
		     const void *payload, size_t payload_len, void *resp,
		     size_t resp_len)
{
	int ret;
	struct switchtec_linux *ldev = to_switchtec_linux(dev);

retry:
	ret = submit_cmd(ldev, cmd, payload, payload_len);
	if (errno == EBADE) {
		read_resp(ldev, NULL, 0);
		errno = 0;
		goto retry;
	}

	if (ret < 0)
		return ret;

	return read_resp(ldev, resp, resp_len);
}

static int get_class_devices(const char *searchpath,
			     struct switchtec_status *status)
{
	int i;
	ssize_t len;
	char syspath[PATH_MAX];
	glob_t paths;
	int found = 0;
	const size_t MAX_LEN = 256;

	snprintf(syspath, sizeof(syspath), "%s*/*/device", searchpath);
	glob(syspath, 0, NULL, &paths);

	for (i = 0; i < paths.gl_pathc; i++) {
		char *p = paths.gl_pathv[i];

		len = readlink(p, syspath, sizeof(syspath));
		if (len <= 0)
			continue;

		p = dirname(p);

		if (!status->class_devices) {
			status->class_devices = calloc(MAX_LEN, 1);
			strcpy(status->class_devices, basename(p));
		} else {
			len = strlen(status->class_devices);
			snprintf(&status->class_devices[len], MAX_LEN - len,
				 ", %s", basename(p));
		}

		found = 1;
	}

	globfree(&paths);
	return found;
}

static void get_port_bdf(const char *searchpath, int port,
			 struct switchtec_status *status)
{
	char syspath[PATH_MAX];
	glob_t paths;
	int ret;

	ret = snprintf(syspath, sizeof(syspath), "%s/*:*:%02x.?",
		       searchpath, port);
	if (ret >= sizeof(syspath))
		return;

	glob(syspath, 0, NULL, &paths);

	if (paths.gl_pathc == 1)
		status->pci_bdf = strdup(basename(paths.gl_pathv[0]));

	globfree(&paths);
}

static void get_port_bdf_path(struct switchtec_status *status)
{
	char path[PATH_MAX];
	char rpath[PATH_MAX];
	int domain, bus, dev, fn;
	int ptr = 0;
	char *subpath;
	int ret;

	if (!status->pci_bdf)
		return;

	snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s",
		 status->pci_bdf);

	if (!realpath(path, rpath))
		return;

	subpath = strtok(rpath, "/");
	while (subpath) {
		ret = sscanf(subpath, "%x:%x:%x.%x", &domain, &bus, &dev, &fn);
		if (ret == 4) {
			if (ptr == 0)
				ret = snprintf(path + ptr, sizeof(path) - ptr,
					       "%04x:%02x:%02x:%x/",
					       domain, bus, dev, fn);
			else
				ret = snprintf(path + ptr, sizeof(path) - ptr,
					       "%02x.%x/", dev, fn);

			if (ret <= 0 || ret >= sizeof(path) - ptr)
				break;

			ptr += ret;
		}
		subpath = strtok(NULL, "/");
	}

	if (ptr)
		path[ptr - 1] = 0;

	status->pci_bdf_path = strdup(path);
}

static void get_port_info(struct switchtec_status *status)
{
	int i;
	char syspath[PATH_MAX];
	glob_t paths;

	if (!status->pci_bdf)
		return;

	snprintf(syspath, sizeof(syspath), "/sys/bus/pci/devices/%s/*:*:*/",
		 status->pci_bdf);

	glob(syspath, 0, NULL, &paths);

	for (i = 0; i < paths.gl_pathc; i++) {
		char *p = paths.gl_pathv[i];

		snprintf(syspath, sizeof(syspath), "%s/vendor", p);
		status->vendor_id = sysfs_read_int(syspath, 16);
		if (status->vendor_id < 0)
			continue;

		snprintf(syspath, sizeof(syspath), "%s/device", p);
		status->device_id = sysfs_read_int(syspath, 16);
		if (status->device_id < 0)
			continue;

		if (get_class_devices(p, status)) {
			if (status->pci_dev)
				free(status->pci_dev);
			status->pci_dev = strdup(basename(p));
		}

		if (!status->pci_dev)
			status->pci_dev = strdup(basename(p));
	}

	globfree(&paths);
}

static void get_config_info(struct switchtec_status *status)
{
	int ret;
	int fd;
	char syspath[PATH_MAX];
	uint32_t extcap;
	int pos = PCI_EXT_CAP_OFFSET;
	uint16_t acs;

	snprintf(syspath, sizeof(syspath), "/sys/bus/pci/devices/%s/config",
		 status->pci_bdf);

	fd = open(syspath, O_RDONLY);
	if (fd < -1)
		return;

	while (1) {
		ret = pread(fd, &extcap, sizeof(extcap), pos);
		if (ret != sizeof(extcap) || !extcap)
			goto close_and_exit;

		if (PCI_EXT_CAP_ID(extcap) == PCI_EXT_CAP_ID_ACS)
			break;

		pos = PCI_EXT_CAP_NEXT(extcap);
		if (pos < PCI_EXT_CAP_OFFSET)
			goto close_and_exit;
	}

	ret = pread(fd, &acs, sizeof(acs), pos + PCI_ACS_CTRL);
	if (ret != sizeof(acs))
		goto close_and_exit;

	status->acs_ctrl = acs;

close_and_exit:
	close(fd);
}

static int linux_get_devices(struct switchtec_dev *dev,
			     struct switchtec_status *status,
			     int ports)
{
	int ret;
	int i;
	int local_part;
	char syspath[PATH_MAX];
	char searchpath[PATH_MAX];
	struct switchtec_linux *ldev = to_switchtec_linux(dev);

	ret = dev_to_sysfs_path(ldev, "device", syspath,
				sizeof(syspath));
	if (ret)
		return ret;

	if (!realpath(syspath, searchpath)) {
		errno = ENXIO;
		return -errno;
	}

	//Replace eg "0000:03:00.1" into "0000:03:00.0"
	searchpath[strlen(searchpath) - 1] = '0';

	local_part = switchtec_partition(dev);

	for (i = 0; i < ports; i++) {
		if (status[i].port.partition != local_part)
			continue;

		if (status[i].port.upstream) {
			status[i].pci_bdf = strdup(basename(searchpath));
			get_port_bdf_path(&status[i]);
			continue;
		}

		get_port_bdf(searchpath, status[i].port.log_id - 1, &status[i]);
		get_port_bdf_path(&status[i]);
		get_port_info(&status[i]);
		get_config_info(&status[i]);
	}

	return 0;
}

static int linux_pff_to_port(struct switchtec_dev *dev, int pff,
			     int *partition, int *port)
{
	int ret;
	struct switchtec_ioctl_pff_port p;
	struct switchtec_linux *ldev = to_switchtec_linux(dev);

	p.pff = pff;
	ret = ioctl(ldev->fd, SWITCHTEC_IOCTL_PFF_TO_PORT, &p);
	if (ret)
		return ret;

	if (partition)
		*partition = p.partition;
	if (port)
		*port = p.port;

	return 0;
}

static int linux_port_to_pff(struct switchtec_dev *dev, int partition,
			     int port, int *pff)
{
	int ret;
	struct switchtec_ioctl_pff_port p;
	struct switchtec_linux *ldev = to_switchtec_linux(dev);

	p.port = port;
	p.partition = partition;

	ret = ioctl(ldev->fd, SWITCHTEC_IOCTL_PORT_TO_PFF, &p);
	if (ret)
		return ret;

	if (pff)
		*pff = p.pff;

	return 0;
}

#ifdef __CHECKER__
#define __force __attribute__((force))
#else
#define __force
#endif

static ssize_t resource_size(struct switchtec_linux *ldev, const char *fname)
{
	char respath[PATH_MAX];
	struct stat stat;
	int fd, ret;

	ret = dev_to_sysfs_path(ldev, fname, respath,
				sizeof(respath));
	if (ret) {
		errno = ret;
		return -1;
	}

	fd = open(respath, O_RDONLY);
	if (fd < 0)
		return -1;

	ret = fstat(fd, &stat);
	if (ret < 0) {
		close(fd);
		return -1;
	}

	close(fd);
	return stat.st_size;
}

static int mmap_resource(struct switchtec_linux *ldev, const char *fname,
			 void *addr, size_t offset, size_t size, int writeable)
{
	char respath[PATH_MAX];
	void *map;
	int fd, ret = 0;

	ret = dev_to_sysfs_path(ldev, fname, respath,
				sizeof(respath));
	if (ret) {
		errno = ret;
		return -1;
	}

	fd = open(respath, writeable ? O_RDWR : O_RDONLY);
	if (fd < 0)
		return -1;

	map = mmap(addr, size, (writeable ? PROT_WRITE : 0) | PROT_READ,
		   MAP_SHARED | MAP_FIXED, fd, offset);
	if (map == MAP_FAILED)
		ret = -1;

	close(fd);
	return ret;
}

/*
 * GAS map maps the hardware registers into user memory space.
 * Needless to say, this can be very dangerous and should only
 * be done if you know what you are doing. Any register accesses
 * that use this will remain unsupported by Microsemi unless it's
 * done within the switchtec user project or otherwise specified.
 */
static gasptr_t linux_gas_map(struct switchtec_dev *dev, int writeable,
			      size_t *map_size)
{
	int ret;
	void *map;
	ssize_t msize;
	struct switchtec_linux *ldev = to_switchtec_linux(dev);

	msize = resource_size(ldev, "device/resource0");
	if (msize <= 0)
		return SWITCHTEC_MAP_FAILED;

	/*
	 * Reserve virtual address space for the entire GAS mapping.
	 */
	map = mmap(NULL, msize, PROT_NONE,
		   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (map == MAP_FAILED)
		return SWITCHTEC_MAP_FAILED;

	ret = mmap_resource(ldev, "device/resource0_wc", map, 0,
			    SWITCHTEC_GAS_TOP_CFG_OFFSET, writeable);
	if (ret) {
		ret = mmap_resource(ldev, "device/resource0", map, 0,
				    SWITCHTEC_GAS_TOP_CFG_OFFSET,
				    writeable);
		if (ret)
			goto unmap_and_exit;
	}

	ret = mmap_resource(ldev, "device/resource0",
			    map + SWITCHTEC_GAS_TOP_CFG_OFFSET,
			    SWITCHTEC_GAS_TOP_CFG_OFFSET,
			    msize - SWITCHTEC_GAS_TOP_CFG_OFFSET,
			    writeable);
	if (ret)
		goto unmap_and_exit;

	if (map_size)
		*map_size = msize;

	dev->gas_map = (gasptr_t __force)map;
	dev->gas_map_size = msize;

	ret = gasop_access_check(dev);
	if (ret) {
		errno = ENODEV;
		goto unmap_and_exit;
	}
	return (gasptr_t __force)map;

unmap_and_exit:
	munmap(map, msize);
	return SWITCHTEC_MAP_FAILED;
}

static void linux_gas_unmap(struct switchtec_dev *dev, gasptr_t map)
{
	munmap((void __force *)map, dev->gas_map_size);
}

static int linux_flash_part(struct switchtec_dev *dev,
			    struct switchtec_fw_image_info *info,
			    enum switchtec_fw_image_part_id_gen3 part)
{
	struct switchtec_linux *ldev = to_switchtec_linux(dev);
	struct switchtec_ioctl_flash_part_info ioctl_info = {0};
	int ret;

	switch (part) {
	case SWITCHTEC_FW_PART_ID_G3_IMG0:
		ioctl_info.flash_partition = SWITCHTEC_IOCTL_PART_IMG0;
		break;
	case SWITCHTEC_FW_PART_ID_G3_IMG1:
		ioctl_info.flash_partition = SWITCHTEC_IOCTL_PART_IMG1;
		break;
	case SWITCHTEC_FW_PART_ID_G3_DAT0:
		ioctl_info.flash_partition = SWITCHTEC_IOCTL_PART_CFG0;
		break;
	case SWITCHTEC_FW_PART_ID_G3_DAT1:
		ioctl_info.flash_partition = SWITCHTEC_IOCTL_PART_CFG1;
		break;
	case SWITCHTEC_FW_PART_ID_G3_NVLOG:
		ioctl_info.flash_partition = SWITCHTEC_IOCTL_PART_NVLOG;
		break;
	default:
		return -EINVAL;
	}

	ret = ioctl(ldev->fd, SWITCHTEC_IOCTL_FLASH_PART_INFO, &ioctl_info);
	if (ret)
		return ret;

	info->part_addr = ioctl_info.address;
	info->part_len = ioctl_info.length;
	info->active = false;
	info->running = false;

	if (ioctl_info.active & SWITCHTEC_IOCTL_PART_ACTIVE)
		info->active = true;

	if (ioctl_info.active & SWITCHTEC_IOCTL_PART_RUNNING)
		info->running = true;

	return 0;
}

static void event_summary_copy(struct switchtec_event_summary *dst,
			       struct switchtec_ioctl_event_summary *src,
			       int size)
{
	int i;

	dst->global = src->global;
	dst->part_bitmap = src->part_bitmap;
	dst->local_part = src->local_part;

	for (i = 0; i < SWITCHTEC_MAX_PARTS; i++)
		dst->part[i] = src->part[i];

	for (i = 0; i < SWITCHTEC_MAX_PFF_CSR && i < size; i++)
		dst->pff[i] = src->pff[i];

	for (; i < SWITCHTEC_MAX_PFF_CSR; i++)
		dst->pff[i] = 0;
}

#define EV(t, n)[SWITCHTEC_ ## t ## _EVT_ ## n] = \
	SWITCHTEC_IOCTL_EVENT_ ## n

static const int event_map[] = {
	EV(GLOBAL, STACK_ERROR),
	EV(GLOBAL, PPU_ERROR),
	EV(GLOBAL, ISP_ERROR),
	EV(GLOBAL, SYS_RESET),
	EV(GLOBAL, FW_EXC),
	EV(GLOBAL, FW_NMI),
	EV(GLOBAL, FW_NON_FATAL),
	EV(GLOBAL, FW_FATAL),
	EV(GLOBAL, TWI_MRPC_COMP),
	EV(GLOBAL, TWI_MRPC_COMP_ASYNC),
	EV(GLOBAL, CLI_MRPC_COMP),
	EV(GLOBAL, CLI_MRPC_COMP_ASYNC),
	EV(GLOBAL, GPIO_INT),
	EV(GLOBAL, GFMS),
	EV(PART, PART_RESET),
	EV(PART, MRPC_COMP),
	EV(PART, MRPC_COMP_ASYNC),
	EV(PART, DYN_PART_BIND_COMP),
	EV(PFF, AER_IN_P2P),
	EV(PFF, AER_IN_VEP),
	EV(PFF, DPC),
	EV(PFF, CTS),
	EV(PFF, UEC),
	EV(PFF, HOTPLUG),
	EV(PFF, IER),
	EV(PFF, THRESH),
	EV(PFF, POWER_MGMT),
	EV(PFF, TLP_THROTTLING),
	EV(PFF, FORCE_SPEED),
	EV(PFF, CREDIT_TIMEOUT),
	EV(PFF, LINK_STATE),
};

static int linux_event_summary(struct switchtec_dev *dev,
			       struct switchtec_event_summary *sum)
{
	int ret;
	struct switchtec_ioctl_event_summary isum;
	struct switchtec_ioctl_event_summary_legacy isum_legacy;
	struct switchtec_linux *ldev = to_switchtec_linux(dev);

	if (!sum)
		return 0;

	ret = ioctl(ldev->fd, SWITCHTEC_IOCTL_EVENT_SUMMARY, &isum);
	if (!ret) {
		event_summary_copy(sum, &isum, ARRAY_SIZE(isum.pff));
		return ret;
	}

	ret = ioctl(ldev->fd, SWITCHTEC_IOCTL_EVENT_SUMMARY_LEGACY, &isum);
	if (ret < 0)
		return ret;

	event_summary_copy(sum, &isum, ARRAY_SIZE(isum_legacy.pff));

	return 0;
}

static int linux_event_ctl(struct switchtec_dev *dev,
			   enum switchtec_event_id e,
			   int index, int flags,
			   uint32_t data[5])
{
	int ret;
	struct switchtec_ioctl_event_ctl ctl;
	struct switchtec_linux *ldev = to_switchtec_linux(dev);

	if (e >= SWITCHTEC_MAX_EVENTS)
		return -EINVAL;

	ctl.event_id = event_map[e];
	ctl.flags = 0;

	if (flags & SWITCHTEC_EVT_FLAG_CLEAR)
		ctl.flags |= SWITCHTEC_IOCTL_EVENT_FLAG_CLEAR;
	if (flags & SWITCHTEC_EVT_FLAG_EN_POLL)
		ctl.flags |= SWITCHTEC_IOCTL_EVENT_FLAG_EN_POLL;
	if (flags & SWITCHTEC_EVT_FLAG_EN_LOG)
		ctl.flags |= SWITCHTEC_IOCTL_EVENT_FLAG_EN_LOG;
	if (flags & SWITCHTEC_EVT_FLAG_EN_CLI)
		ctl.flags |= SWITCHTEC_IOCTL_EVENT_FLAG_EN_CLI;
	if (flags & SWITCHTEC_EVT_FLAG_EN_FATAL)
		ctl.flags |= SWITCHTEC_IOCTL_EVENT_FLAG_EN_FATAL;
	if (flags & SWITCHTEC_EVT_FLAG_DIS_POLL)
		ctl.flags |= SWITCHTEC_IOCTL_EVENT_FLAG_DIS_POLL;
	if (flags & SWITCHTEC_EVT_FLAG_DIS_LOG)
		ctl.flags |= SWITCHTEC_IOCTL_EVENT_FLAG_DIS_LOG;
	if (flags & SWITCHTEC_EVT_FLAG_DIS_CLI)
		ctl.flags |= SWITCHTEC_IOCTL_EVENT_FLAG_DIS_CLI;
	if (flags & SWITCHTEC_EVT_FLAG_DIS_FATAL)
		ctl.flags |= SWITCHTEC_IOCTL_EVENT_FLAG_DIS_FATAL;

	ctl.index = index;
	ret = ioctl(ldev->fd, SWITCHTEC_IOCTL_EVENT_CTL, &ctl);

	if (ret)
		return ret;

	if (data)
		memcpy(data, ctl.data, sizeof(ctl.data));

	return ctl.count;
}

static int linux_event_wait(struct switchtec_dev *dev, int timeout_ms)
{
	int ret;
	struct switchtec_linux *ldev = to_switchtec_linux(dev);
	struct pollfd fds = {
		.fd = ldev->fd,
		.events = POLLPRI,
	};

	ret = poll(&fds, 1, timeout_ms);
	if (ret <= 0)
		return ret;

	if (fds.revents & POLLERR) {
		errno = ENODEV;
		return -1;
	}

	if (fds.revents & POLLPRI)
		return 1;

	return 0;
}

static const struct switchtec_ops linux_ops = {
	.close = linux_close,
	.get_device_id = linux_get_device_id,
	.get_fw_version = linux_get_fw_version,
	.get_device_version = linux_get_device_version,
	.cmd = linux_cmd,
	.get_devices = linux_get_devices,
	.pff_to_port = linux_pff_to_port,
	.port_to_pff = linux_port_to_pff,
	.gas_map = linux_gas_map,
	.gas_unmap = linux_gas_unmap,
	.flash_part = linux_flash_part,
	.event_summary = linux_event_summary,
	.event_ctl = linux_event_ctl,
	.event_wait = linux_event_wait,

	.gas_read8 = mmap_gas_read8,
	.gas_read16 = mmap_gas_read16,
	.gas_read32 = mmap_gas_read32,
	.gas_read64 = mmap_gas_read64,
	.gas_write8 = mmap_gas_write8,
	.gas_write16 = mmap_gas_write16,
	.gas_write32 = mmap_gas_write32,
	.gas_write32_no_retry = mmap_gas_write32,
	.gas_write64 = mmap_gas_write64,
	.memcpy_to_gas = mmap_memcpy_to_gas,
	.memcpy_from_gas = mmap_memcpy_from_gas,
	.write_from_gas = mmap_write_from_gas,
};

struct switchtec_dev *switchtec_open_by_path(const char *path)
{
	struct switchtec_linux *ldev;
	int fd;

	fd = open(path, O_RDWR | O_CLOEXEC);
	if (fd < 0)
		return NULL;

	if (isatty(fd))
		return switchtec_open_uart(fd);
	else
		errno = 0;

	ldev = malloc(sizeof(*ldev));
	if (!ldev)
		return NULL;

	ldev->fd = fd;

	if (check_switchtec_device(ldev))
		goto err_close_free;

	if (get_partition(ldev))
		goto err_close_free;

	ldev->dev.ops = &linux_ops;

	return &ldev->dev;

err_close_free:
	close(ldev->fd);
	free(ldev);
	return NULL;
}

struct switchtec_dev *switchtec_open_by_index(int index)
{
	char path[PATH_MAX];
	struct switchtec_dev *dev;

	snprintf(path, sizeof(path), "/dev/switchtec%d", index);

	dev = switchtec_open_by_path(path);

	if (errno == ENOENT)
		errno = ENODEV;

	return dev;
}

struct switchtec_dev *switchtec_open_by_pci_addr(int domain, int bus,
						 int device, int func)
{
	char path[PATH_MAX];
	struct switchtec_dev *dev;
	struct dirent *dirent;
	DIR *dir;

	snprintf(path, sizeof(path),
		 "/sys/bus/pci/devices/%04x:%02x:%02x.%x/switchtec",
		 domain, bus, device, func);

	dir = opendir(path);
	if (!dir)
		goto err_out;

	while ((dirent = readdir(dir))) {
		if (dirent->d_name[0] != '.')
			break;
	}

	if (!dirent)
		goto err_close;

	/*
	 * Should only be one switchtec device, if there are
	 * more then something is wrong
	 */
	if (readdir(dir))
		goto err_close;

	snprintf(path, sizeof(path), "/dev/%s", dirent->d_name);
	printf("%s\n", path);
	dev = switchtec_open(path);

	closedir(dir);
	return dev;

err_close:
	closedir(dir);
err_out:
	errno = ENODEV;
	return NULL;
}

#endif
