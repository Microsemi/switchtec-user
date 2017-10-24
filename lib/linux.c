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

#include "switchtec_priv.h"
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

#include <errno.h>
#include <string.h>

static const char *sys_path = "/sys/class/switchtec";

static int dev_to_sysfs_path(struct switchtec_dev *dev, const char *suffix,
			     char *buf, size_t buflen)
{
	int ret;
	struct stat stat;

	ret = fstat(dev->fd, &stat);
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

static int sysfs_read_int(const char *path, int base)
{
	int ret;
	char buf[64];

	ret = sysfs_read_str(path, buf, sizeof(buf));
	if (ret < 0)
		return ret;

	return strtol(buf, NULL, base);
}

static int check_switchtec_device(struct switchtec_dev *dev)
{
	int ret;
	char syspath[PATH_MAX];

	ret = dev_to_sysfs_path(dev, "device/switchtec", syspath,
				sizeof(syspath));
	if (ret)
		return ret;

	ret = access(syspath, F_OK);
	if (ret)
		errno = ENOTTY;

	return ret;
}

static int get_partition(struct switchtec_dev *dev)
{
	int ret;
	char syspath[PATH_MAX];

	ret = dev_to_sysfs_path(dev, "partition", syspath,
				sizeof(syspath));
	if (ret)
		return ret;

	dev->partition = sysfs_read_int(syspath, 10);
	if (dev->partition < 0)
		return dev->partition;

	return 0;
}

struct switchtec_dev *switchtec_open(const char *path)
{
	struct switchtec_dev *dev;

	dev = malloc(sizeof(*dev));
	if (!dev)
		return dev;

	dev->fd = open(path, O_RDWR | O_CLOEXEC);
	if (dev->fd < 0)
		goto err_free;

	if (check_switchtec_device(dev))
		goto err_close_free;

	if (get_partition(dev))
		goto err_close_free;

	snprintf(dev->name, sizeof(dev->name), "%s", path);

	return dev;

err_close_free:
	close(dev->fd);
err_free:
	free(dev);
	return NULL;
}

void switchtec_close(struct switchtec_dev *dev)
{
	if (!dev)
		return;

	close(dev->fd);
	free(dev);
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
	uint32_t version;
	char syspath[PATH_MAX];

	ret = dev_to_sysfs_path(dev, "fw_version", syspath, sizeof(syspath));
	if (ret)
		return ret;

	version = sysfs_read_int(syspath, 16);
	if (version < 0)
		return version;

	version_to_string(version, buf, buflen);

	return 0;
}

static int submit_cmd(struct switchtec_dev *dev, uint32_t cmd,
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

	if (ret != sizeof(buf)) {
		errno = EIO;
		return -errno;
	}

	return 0;
}

static int read_resp(struct switchtec_dev *dev, void *resp,
		     size_t resp_len)
{
	int32_t ret;
	char buf[sizeof(uint32_t) + resp_len];

	ret = read(dev->fd, buf, sizeof(buf));

	if (ret < 0)
		return ret;

	if (ret != sizeof(buf)) {
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

retry:
	ret = submit_cmd(dev, cmd, payload, payload_len);
	if (errno == EBADE) {
		read_resp(dev, NULL, 0);
		errno = 0;
		goto retry;
	}

	if (ret < 0)
		return ret;

	return read_resp(dev, resp, resp_len);
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

	ret = dev_to_sysfs_path(dev, "device", syspath,
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

	p.pff = pff;
	ret = ioctl(dev->fd, SWITCHTEC_IOCTL_PFF_TO_PORT, &p);
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

	p.port = port;
	p.partition = partition;

	ret = ioctl(dev->fd, SWITCHTEC_IOCTL_PORT_TO_PFF, &p);
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

	ret = dev_to_sysfs_path(dev, "device/resource0_wc", respath,
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

#endif
