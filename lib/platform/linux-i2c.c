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
#include "gasops.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

struct switchtec_i2c {
	struct switchtec_dev dev;
	int fd;
	int i2c_addr;
};

#define to_switchtec_i2c(d)  \
	((struct switchtec_i2c *) \
	 ((char *)d - offsetof(struct switchtec_i2c, dev)))

static int dev_to_sysfs_path(struct switchtec_i2c *idev, const char *suffix,
			     char *buf, size_t buflen)
{
	int ret;
	struct stat stat;

	ret = fstat(idev->fd, &stat);
	if (ret < 0)
		return ret;

	snprintf(buf, buflen,
		 "/sys/dev/char/%d:%d/%s",
		 major(stat.st_rdev), minor(stat.st_rdev), suffix);

	return 0;
}

static int check_i2c_device_supported(struct switchtec_i2c *idev)
{
	unsigned long funcs;
	int ret;

	ret = ioctl(idev->fd, I2C_FUNCS, &funcs);
	if (ret < 0)
		return ret;

	if (!(funcs & I2C_FUNC_I2C)) {
		errno = ENOPROTOOPT;
		return -errno;
	}

	return 0;
}

static int check_i2c_device(struct switchtec_i2c *idev)
{
	int ret;
	char syspath[PATH_MAX];

	ret = dev_to_sysfs_path(idev, "device/i2c-dev", syspath,
				sizeof(syspath));
	if (ret)
		return ret;

	ret = access(syspath, F_OK);
	if (ret)
		errno = ENOTTY;

	return check_i2c_device_supported(idev);
}

static int i2c_set_addr(struct switchtec_i2c *idev, int i2c_addr)
{
	idev->i2c_addr = i2c_addr;

	return ioctl(idev->fd, I2C_SLAVE, i2c_addr);
}

#ifdef __CHECKER__
#define __force __attribute__((force))
#else
#define __force
#endif

static void i2c_close(struct switchtec_dev *dev)
{
	struct switchtec_i2c *idev = to_switchtec_i2c(dev);

	if (!dev)
		return;

	if (dev->gas_map)
		munmap((void __force *)dev->gas_map, dev->gas_map_size);

	close(idev->fd);
	free(idev);
}

static int map_gas(struct switchtec_dev *dev)
{
	void *addr;
	dev->gas_map_size = 4 << 20;

	/*
	 * Ensure that if someone tries to do something stupid,
	 * like dereference the GAS directly we fail without
	 * trashing random memory somewhere. We do this by
	 * allocating an innaccessible range in the virtual
	 * address space and use that as the GAS address which
	 * will be subtracted by subsequent operations
	 */

	addr = mmap(NULL, dev->gas_map_size, PROT_NONE,
		    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (addr == MAP_FAILED)
		return -1;

	dev->gas_map = (gasptr_t __force)addr;

	return 0;
}

#undef __force

static gasptr_t i2c_gas_map(struct switchtec_dev *dev, int writeable,
			    size_t *map_size)
{
	if (map_size)
		*map_size = dev->gas_map_size;

	return dev->gas_map;
}

/* One I2C transaction can write a maximum of 28 bytes */
#define I2C_MAX_WRITE 28

static void i2c_gas_write(struct switchtec_dev *dev, void __gas *dest,
			  const void *src, size_t n)
{
	int ret;
	struct switchtec_i2c *idev = to_switchtec_i2c(dev);

	struct {
		uint32_t gas_addr;
		uint8_t data[];
	} *msg;

	msg = malloc(sizeof(*msg) + n);

	msg->gas_addr = (uint32_t)(dest - (void __gas *)dev->gas_map);
	assert(n <= I2C_MAX_WRITE);
	assert((msg->gas_addr & 0x3) == 0);

	memcpy(&msg->data, src, n);
	msg->gas_addr = htobe32(msg->gas_addr);

	ret = write(idev->fd, msg, sizeof(*msg) + n);
	if (ret != sizeof(*msg) + n) {
		/*
		 * For now, this is all we can do. If it's a problem we'll
		 * have to refactor all GAS access functions to return an
		 * error code.
		 */
		perror("Could not communicate with I2C device");
		exit(1);
	}

	free(msg);
}

static void i2c_memcpy_to_gas(struct switchtec_dev *dev, void __gas *dest,
			      const void *src, size_t n)
{
	size_t cnt;

	while (n) {
		cnt = n > I2C_MAX_WRITE ? I2C_MAX_WRITE : n;
		i2c_gas_write(dev, dest, src, cnt);

		dest += cnt;
		src += cnt;
		n -= cnt;
	}
}

static void i2c_memcpy_from_gas(struct switchtec_dev *dev, void *dest,
				const void __gas *src, size_t n)
{
	int ret;
	struct switchtec_i2c *idev = to_switchtec_i2c(dev);
	uint32_t gas_addr = (uint32_t)(src - (void __gas *)dev->gas_map);
	struct i2c_msg msgs[2];
	struct i2c_rdwr_ioctl_data rwdata = {
		.msgs = msgs,
		.nmsgs = 2,
	};

	gas_addr = htobe32(gas_addr);

	msgs[0].addr = msgs[1].addr = idev->i2c_addr;

	msgs[0].flags = 0;
	msgs[0].len = sizeof(gas_addr);
	msgs[0].buf = (__u8 *)&gas_addr;

	msgs[1].flags = I2C_M_RD;
	msgs[1].len = n;
	msgs[1].buf = dest;

	ret = ioctl(idev->fd, I2C_RDWR, &rwdata);
	if (ret < 0) {
		/*
		 * For now, this is all we can do. If it's a problem we'll
		 * have to refactor all GAS access functions to return an
		 * error code.
		 */
		perror("Could not communicate with I2C device");
		exit(1);
	}
}

static ssize_t i2c_write_from_gas(struct switchtec_dev *dev, int fd,
				  const void __gas *src, size_t n)
{
	ssize_t ret;
	void *buf;

	buf = malloc(n);

	i2c_memcpy_from_gas(dev, buf, src, n);

	ret = write(fd, buf, n);

	free(buf);

	return ret;
}

#define create_gas_read(type, suffix) \
	static type i2c_gas_read ## suffix(struct switchtec_dev *dev, \
					   type __gas *addr) \
	{ \
		type ret; \
		i2c_memcpy_from_gas(dev, &ret, addr, sizeof(ret)); \
		return ret; \
	}

create_gas_read(uint8_t, 8);
create_gas_read(uint16_t, 16);
create_gas_read(uint32_t, 32);
create_gas_read(uint64_t, 64);

static void i2c_gas_write8(struct switchtec_dev *dev, uint8_t val,
			   uint8_t __gas *addr)
{
	fprintf(stderr, "gas_write8 is not supported for I2C accesses\n");
	exit(1);
}

static void i2c_gas_write16(struct switchtec_dev *dev, uint16_t val,
			    uint16_t __gas *addr)
{
	fprintf(stderr, "gas_write16 is not supported for I2C accesses\n");
	exit(1);
}

static void i2c_gas_write32(struct switchtec_dev *dev, uint32_t val,
			    uint32_t __gas *addr)
{
	i2c_gas_write(dev, addr, &val, sizeof(uint32_t));
}

static void i2c_gas_write64(struct switchtec_dev *dev, uint64_t val,
			    uint64_t __gas *addr)
{
	i2c_gas_write(dev, addr, &val, sizeof(uint64_t));
}

static const struct switchtec_ops i2c_ops = {
	.close = i2c_close,
	.gas_map = i2c_gas_map,

	.cmd = gasop_cmd,
	.get_fw_version = gasop_get_fw_version,
	.pff_to_port = gasop_pff_to_port,
	.port_to_pff = gasop_port_to_pff,
	.flash_part = gasop_flash_part,
	.event_summary = gasop_event_summary,
	.event_ctl = gasop_event_ctl,
	.event_wait_for = gasop_event_wait_for,

	.gas_read8 = i2c_gas_read8,
	.gas_read16 = i2c_gas_read16,
	.gas_read32 = i2c_gas_read32,
	.gas_read64 = i2c_gas_read64,
	.gas_write8 = i2c_gas_write8,
	.gas_write16 = i2c_gas_write16,
	.gas_write32 = i2c_gas_write32,
	.gas_write64 = i2c_gas_write64,
	.memcpy_to_gas = i2c_memcpy_to_gas,
	.memcpy_from_gas = i2c_memcpy_from_gas,
	.write_from_gas = i2c_write_from_gas,
};

struct switchtec_dev *switchtec_open_i2c(const char *path, int i2c_addr)
{
	struct switchtec_i2c *idev;

	idev = malloc(sizeof(*idev));
	if (!idev)
		return NULL;

	idev->fd = open(path, O_RDWR | O_CLOEXEC);
	if (idev->fd < 0)
		goto err_free;

	if (check_i2c_device(idev))
		goto err_close_free;

	if (i2c_set_addr(idev, i2c_addr))
		goto err_close_free;

	if (map_gas(&idev->dev))
		goto err_close_free;

	idev->dev.ops = &i2c_ops;

	gasop_set_partition_info(&idev->dev);

	return &idev->dev;

err_close_free:
	close(idev->fd);
err_free:
	free(idev);
	return NULL;
}

struct switchtec_dev *switchtec_open_i2c_by_adapter(int adapter, int i2c_addr)
{
	char path[PATH_MAX];

	sprintf(path, "/dev/i2c-%d", adapter);

	return switchtec_open_i2c(path, i2c_addr);
}

#endif
