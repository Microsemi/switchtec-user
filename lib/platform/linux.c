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

#include "../switchtec_priv.h"
#include "switchtec/switchtec.h"

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

	return 0;
}

struct switchtec_dev *switchtec_open_by_path(const char *path)
{
	struct switchtec_linux *ldev;

	ldev = malloc(sizeof(*ldev));
	if (!ldev)
		return NULL;

	ldev->fd = open(path, O_RDWR | O_CLOEXEC);
	if (ldev->fd < 0)
		goto err_free;

	if (check_switchtec_device(ldev))
		goto err_close_free;

	if (get_partition(ldev))
		goto err_close_free;

	snprintf(ldev->dev.name, sizeof(ldev->dev.name), "%s", path);

	return &ldev->dev;

err_close_free:
	close(ldev->fd);
err_free:
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

void switchtec_close(struct switchtec_dev *dev)
{
	struct switchtec_linux *ldev = to_switchtec_linux(dev);

	if (!dev)
		return;

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

	if (!dl) {
		for (i = 0; i < n; i++)
			free(devices[n]);
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

		free(devices[n]);
	}

	free(devices);
	return n;
}

int switchtec_get_fw_version(struct switchtec_dev *dev, char *buf,
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

static void get_port_info(const char *searchpath, int port,
			  struct switchtec_status *status)
{
	int i;
	char syspath[PATH_MAX];
	glob_t paths;

	snprintf(syspath, sizeof(syspath), "%s/*:*:%02d.*/*:*:*/",
		 searchpath, port);

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

int switchtec_get_devices(struct switchtec_dev *dev,
			  struct switchtec_status *status,
			  int ports)
{
	int ret;
	int i;
	int local_part;
	int port;
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
	port = 0;

	for (i = 0; i < ports; i++) {
		if (status[i].port.upstream ||
		    status[i].port.partition != local_part)
			continue;

		get_port_info(searchpath, port, &status[i]);
		port++;
	}

	return 0;
}

int switchtec_pff_to_port(struct switchtec_dev *dev, int pff,
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

int switchtec_port_to_pff(struct switchtec_dev *dev, int partition,
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

/*
 * GAS map maps the hardware registers into user memory space.
 * Needless to say, this can be very dangerous and should only
 * be done if you know what you are doing. Any register accesses
 * that use this will remain unsupported by Microsemi unless it's
 * done within the switchtec user project or otherwise specified.
 */
void *switchtec_gas_map(struct switchtec_dev *dev, int writeable,
			size_t *map_size)
{
	int ret;
	int fd;
	void *map;
	char respath[PATH_MAX];
	struct stat stat;
	struct switchtec_linux *ldev = to_switchtec_linux(dev);

	ret = dev_to_sysfs_path(ldev, "device/resource0_wc", respath,
				sizeof(respath));

	if (ret) {
		errno = ret;
		return MAP_FAILED;
	}

	fd = open(respath, writeable ? O_RDWR : O_RDONLY);
	if (fd < 0)
		return MAP_FAILED;

	ret = fstat(fd, &stat);
	if (ret < 0)
		return MAP_FAILED;

	map = mmap(NULL, stat.st_size,
		   (writeable ? PROT_WRITE : 0) | PROT_READ,
		   MAP_SHARED, fd, 0);
	close(fd);

	if (map_size)
		*map_size = stat.st_size;

	dev->gas_map = map;
	dev->gas_map_size = stat.st_size;

	return map;
}

void switchtec_gas_unmap(struct switchtec_dev *dev, void *map)
{
	munmap(map, dev->gas_map_size);
}

int switchtec_flash_part(struct switchtec_dev *dev,
			 struct switchtec_fw_image_info *info,
			 enum switchtec_fw_image_type part)
{
	struct switchtec_linux *ldev = to_switchtec_linux(dev);
	struct switchtec_ioctl_flash_part_info ioctl_info = {0};
	int ret;

	switch (part) {
	case SWITCHTEC_FW_TYPE_IMG0:
		ioctl_info.flash_partition = SWITCHTEC_IOCTL_PART_IMG0;
		break;
	case SWITCHTEC_FW_TYPE_IMG1:
		ioctl_info.flash_partition = SWITCHTEC_IOCTL_PART_IMG1;
		break;
	case SWITCHTEC_FW_TYPE_DAT0:
		ioctl_info.flash_partition = SWITCHTEC_IOCTL_PART_CFG0;
		break;
	case SWITCHTEC_FW_TYPE_DAT1:
		ioctl_info.flash_partition = SWITCHTEC_IOCTL_PART_CFG1;
		break;
	case SWITCHTEC_FW_TYPE_NVLOG:
		ioctl_info.flash_partition = SWITCHTEC_IOCTL_PART_NVLOG;
		break;
	default:
		return -EINVAL;
	}

	ret = ioctl(ldev->fd, SWITCHTEC_IOCTL_FLASH_PART_INFO, &ioctl_info);
	if (ret)
		return ret;

	info->image_addr = ioctl_info.address;
	info->image_len = ioctl_info.length;
	info->active = ioctl_info.active;
	return 0;
}

static void event_summary_copy(struct switchtec_event_summary *dst,
			       struct switchtec_ioctl_event_summary *src)
{
	int i;

	dst->global = src->global;
	dst->part_bitmap = src->part_bitmap;
	dst->local_part = src->local_part;

	for (i = 0; i < SWITCHTEC_MAX_PARTS; i++)
		dst->part[i] = src->part[i];

	for (i = 0; i < SWITCHTEC_MAX_PORTS; i++)
		dst->pff[i] = src->pff[i];
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
	EV(PFF, HOTPLUG),
	EV(PFF, IER),
	EV(PFF, THRESH),
	EV(PFF, POWER_MGMT),
	EV(PFF, TLP_THROTTLING),
	EV(PFF, FORCE_SPEED),
	EV(PFF, CREDIT_TIMEOUT),
	EV(PFF, LINK_STATE),
};

int switchtec_event_summary(struct switchtec_dev *dev,
			    struct switchtec_event_summary *sum)
{
	int ret;
	struct switchtec_ioctl_event_summary isum;
	struct switchtec_linux *ldev = to_switchtec_linux(dev);

	if (!sum)
		return -EINVAL;

	ret = ioctl(ldev->fd, SWITCHTEC_IOCTL_EVENT_SUMMARY, &isum);
	if (ret < 0)
		return ret;

	event_summary_copy(sum, &isum);

	return 0;
}

int switchtec_event_check(struct switchtec_dev *dev,
			  struct switchtec_event_summary *check,
			  struct switchtec_event_summary *res)
{
	int ret, i;
	struct switchtec_ioctl_event_summary isum;
	struct switchtec_linux *ldev = to_switchtec_linux(dev);

	if (!check)
		return -EINVAL;

	ret = ioctl(ldev->fd, SWITCHTEC_IOCTL_EVENT_SUMMARY, &isum);
	if (ret < 0)
		return ret;

	ret = 0;

	if (isum.global & check->global)
		ret = 1;

	if (isum.part_bitmap & check->part_bitmap)
		ret = 1;

	if (isum.local_part & check->local_part)
		ret = 1;

	for (i = 0; i < SWITCHTEC_MAX_PARTS; i++)
		if (isum.part[i] & check->part[i])
			ret = 1;

	for (i = 0; i < SWITCHTEC_MAX_PORTS; i++)
		if (isum.pff[i] & check->pff[i])
			ret = 1;

	if (res)
		event_summary_copy(res, &isum);

	return ret;
}

int switchtec_event_ctl(struct switchtec_dev *dev,
			enum switchtec_event_id e,
			int index, int flags,
			uint32_t data[5])
{
	int ret;
	struct switchtec_ioctl_event_ctl ctl;
	struct switchtec_linux *ldev = to_switchtec_linux(dev);

	if (e > SWITCHTEC_MAX_EVENTS)
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

int switchtec_event_wait(struct switchtec_dev *dev, int timeout_ms)
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

#endif
