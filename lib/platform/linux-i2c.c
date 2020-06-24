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
#include "../crc.h"
#include "switchtec/switchtec.h"
#include "gasops.h"

#include <endian.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/sysmacros.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

struct switchtec_i2c {
	struct switchtec_dev dev;
	int fd;
	int i2c_addr;
	uint8_t tag;
};

#define CMD_GET_CAP  0xE0
#define CMD_GAS_WRITE  0xEA
#define CMD_GET_WRITE_STATUS  0xE2
#define CMD_GAS_WRITE_WITH_STATUS  0xE8
#define CMD_GAS_READ  0xE9

#define MAX_RETRY_COUNT  100
#define MAX_STATUS_GET_RETRY  50
#define PEC_BYTE_COUNT  1
#define TWI_ENHANCED_MODE  0x80
#define GAS_TWI_MRPC_ERR  0x20
#define DATA_TAIL_BYTE_COUNT  2

#define to_switchtec_i2c(d)  \
	((struct switchtec_i2c *) \
	 ((char *)d - offsetof(struct switchtec_i2c, dev)))

static uint8_t get_tag(struct switchtec_i2c *idev)
{
	/* Valid tag is 0x01 ~ 0xff */
	idev->tag++;
	if (!idev->tag)
		idev->tag = 1;
	return idev->tag;
}

static uint8_t i2c_msg_pec(struct i2c_msg *msg, uint8_t byte_count,
                           uint8_t oldchksum, bool init)
{
	/* This function just supports 7-bits I2C address */
	uint8_t addr = (msg->addr << 1) | msg->flags;
	uint8_t pec = crc8(&addr, 1, oldchksum, init);
	return crc8(msg->buf, byte_count, pec, false);
}

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

static int i2c_set_timeout(struct switchtec_i2c *idev, int time)
{
	return ioctl(idev->fd, I2C_TIMEOUT, time);
}

#ifdef __CHECKER__
#define __force __attribute__((force))
#else
#define __force
#endif

static void i2c_close(struct switchtec_dev *dev)
{
	struct switchtec_i2c *idev = to_switchtec_i2c(dev);

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

static uint8_t i2c_gas_cap_get(struct switchtec_dev *dev)
{
	int ret;
	struct switchtec_i2c *idev = to_switchtec_i2c(dev);

	struct i2c_msg msgs[2];
	struct i2c_rdwr_ioctl_data rwdata = {
		.msgs = msgs,
		.nmsgs = 2,
	};

	uint8_t command_code = CMD_GET_CAP;
	uint8_t rx_buf[2];
	uint8_t msg_0_pec, pec;
	uint8_t retry_count = 0;

	msgs[0].addr = msgs[1].addr = idev->i2c_addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &command_code;

	msgs[1].flags = I2C_M_RD;
	msgs[1].len = 2;
	msgs[1].buf = rx_buf;

	do {
		ret = ioctl(idev->fd, I2C_RDWR, &rwdata);
		if (ret < 0)
			goto i2c_ioctl_fail;

		msg_0_pec = i2c_msg_pec(&msgs[0], msgs[0].len, 0, true);
		pec = i2c_msg_pec(&msgs[1], msgs[1].len - PEC_BYTE_COUNT,
				   msg_0_pec, false);
		if (rx_buf[1] == pec)
			break;

		retry_count++;
	} while(retry_count < MAX_RETRY_COUNT);

	/* return capability */
	if (retry_count == MAX_RETRY_COUNT)
		return -1;
	else
		return (rx_buf[0] & TWI_ENHANCED_MODE);

i2c_ioctl_fail:
	return -1;
}

/*
 * One I2C transaction can write a maximum of 26 bytes, but it is better to
 * write the GAS with dword.
 */
#define I2C_MAX_WRITE 24
/*
 * One I2C transaction can read a maximum of 27 bytes, but it is better to
 * read GAS with dword.
 */
#define I2C_MAX_READ 24

static uint8_t i2c_gas_data_write(struct switchtec_dev *dev, void __gas *dest,
				  const void *src, size_t n, uint8_t tag)
{
	int ret;
	struct switchtec_i2c *idev = to_switchtec_i2c(dev);

	struct i2c_msg msg;
	struct i2c_rdwr_ioctl_data wdata = {
		.msgs = &msg,
		.nmsgs = 1,
	};

	struct {
		uint8_t command_code;
		uint8_t byte_count;
		uint8_t tag;
		uint32_t offset;
		uint8_t data[];
	} __attribute__((packed)) *i2c_data;

	uint32_t gas_addr = (uint32_t)(dest - (void __gas *)dev->gas_map);
	assert(n <= I2C_MAX_WRITE);

	/* PEC is the last byte */
	i2c_data = malloc(sizeof(*i2c_data) + n + PEC_BYTE_COUNT);

	i2c_data->command_code = CMD_GAS_WRITE;
	i2c_data->byte_count = (sizeof(i2c_data->tag)
				+ sizeof(i2c_data->offset)
			        + n);
	i2c_data->tag = tag;

	gas_addr = htobe32(gas_addr);
	i2c_data->offset = gas_addr;
	memcpy(&i2c_data->data, src, n);
	msg.addr = idev->i2c_addr;
	msg.flags = 0;
	msg.len = sizeof(*i2c_data) + n + PEC_BYTE_COUNT;
	msg.buf = (uint8_t *)i2c_data;

	i2c_data->data[n] = i2c_msg_pec(&msg, msg.len - PEC_BYTE_COUNT, 0,
					 true);

	ret = ioctl(idev->fd, I2C_RDWR, &wdata);
	if (ret < 0)
		goto i2c_write_fail;

	free(i2c_data);
	return 0;

i2c_write_fail:
	free(i2c_data);
	return -1;
}

static uint8_t i2c_gas_write_status_get(struct switchtec_dev *dev,
					 uint8_t tag)
{
	int ret;
	struct switchtec_i2c *idev = to_switchtec_i2c(dev);
	struct i2c_msg msgs[2];
	struct i2c_rdwr_ioctl_data rwdata = {
		.msgs = msgs,
		.nmsgs = 2,
	};

	uint8_t command_code = CMD_GET_WRITE_STATUS;
	uint8_t rx_buf[3];

	uint8_t msg_0_pec, pec;
	uint8_t retry_count = 0;

	msgs[0].addr = msgs[1].addr = idev->i2c_addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &command_code;

	msgs[1].flags = I2C_M_RD;
	msgs[1].len = 3;
	msgs[1].buf = rx_buf;

	do {
		ret = ioctl(idev->fd, I2C_RDWR, &rwdata);
		if (ret < 0) {
			retry_count++;
			/* Delay is typically only needed for BL1/2 phase */
			usleep(2000);
			continue;
		}

		msg_0_pec = i2c_msg_pec(&msgs[0], msgs[0].len, 0, true);
		pec = i2c_msg_pec(&msgs[1], msgs[1].len - PEC_BYTE_COUNT,
				  msg_0_pec, false);
		if (rx_buf[0] == tag && rx_buf[2] == pec &&
		    (rx_buf[1] == 0 || rx_buf[1] == GAS_TWI_MRPC_ERR))
			return rx_buf[1];

		/* Extra delay is typically only needed for BL1/2 phase */
		usleep(2000);
		retry_count++;
	} while(retry_count < MAX_STATUS_GET_RETRY);

	return -1;
}

static void i2c_gas_write(struct switchtec_dev *dev, void __gas *dest,
			  const void *src, size_t n)
{
	struct switchtec_i2c *idev = to_switchtec_i2c(dev);
	uint8_t tag;
	uint8_t status;
	uint8_t retry_count = 0;

	do {
		tag = get_tag(idev);
		i2c_gas_data_write(dev, dest, src, n, tag);
		status = i2c_gas_write_status_get(dev, tag);
		if (status == 0 || status == GAS_TWI_MRPC_ERR)
			break;

		/* Extra delay is typically only needed for BL1/2 phase */
		usleep(1000);
		retry_count++;
	} while (retry_count < MAX_RETRY_COUNT);

	if (retry_count == MAX_RETRY_COUNT)
		raise(SIGBUS);
}

static void i2c_gas_write_no_retry(struct switchtec_dev *dev, void __gas *dest,
				   const void *src, size_t n)
{
	struct switchtec_i2c *idev = to_switchtec_i2c(dev);
	uint8_t tag;
	uint8_t status;

	tag = get_tag(idev);
	i2c_gas_data_write(dev, dest, src, n, tag);
	status = i2c_gas_write_status_get(dev, tag);
	if (status == 0 || status == GAS_TWI_MRPC_ERR)
		return;

	raise(SIGBUS);
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

static uint8_t i2c_gas_data_read(struct switchtec_dev *dev, void *dest,
				 const void __gas *src, size_t n)
{
	int ret;
	int pec_index, status_index;
	uint8_t msg_0_pec, pec;
	uint8_t retry_count = 0;

	struct switchtec_i2c *idev = to_switchtec_i2c(dev);
	uint32_t gas_addr = (uint32_t)(src - (void __gas *)dev->gas_map);
	uint8_t status;

	struct i2c_msg msgs[2];
	struct i2c_rdwr_ioctl_data rwdata = {
		.msgs = msgs,
		.nmsgs = 2,
	};

	struct {
		uint8_t command_code;
		uint8_t byte_count;
		uint32_t offset;
		uint8_t data_length;
	} __attribute__((packed)) *read_command;

	struct {
		uint8_t byte_count;
		/* tail is one byte status and one byte pec */
		uint8_t data_and_tail[];
	}*read_response;

	read_command = malloc(sizeof(*read_command));
	read_response = malloc(sizeof(*read_response) + n \
			       + DATA_TAIL_BYTE_COUNT);

	msgs[0].addr = msgs[1].addr = idev->i2c_addr;
	msgs[0].flags = 0;
	msgs[0].len = sizeof(*read_command);

	read_command->command_code = CMD_GAS_READ;
	read_command->byte_count = sizeof(read_command->offset) \
				   + sizeof(read_command->data_length);
	gas_addr = htobe32(gas_addr);
	read_command->offset = gas_addr;
	read_command->data_length = n;
	msgs[0].buf = (uint8_t *)read_command;

	msgs[1].flags = I2C_M_RD;
	msgs[1].len = sizeof(read_response->byte_count) + n + \
		      DATA_TAIL_BYTE_COUNT;
	msgs[1].buf = (uint8_t *)read_response;

	do {
		ret = ioctl(idev->fd, I2C_RDWR, &rwdata);
		if (ret < 0)
			goto i2c_read_fail;

		msg_0_pec = i2c_msg_pec(&msgs[0], msgs[0].len, 0, true);
		pec = i2c_msg_pec(&msgs[1], msgs[1].len - PEC_BYTE_COUNT, \
				   msg_0_pec, false);
		pec_index = msgs[1].len - sizeof(read_response->byte_count) \
			    - PEC_BYTE_COUNT;
		if (read_response->data_and_tail[ pec_index ] == pec)
			break;

		retry_count++;
	} while(retry_count < MAX_RETRY_COUNT);

	if (retry_count == MAX_RETRY_COUNT)
		goto i2c_read_fail;

	memcpy(dest, read_response->data_and_tail, n);
	status_index = msgs[1].len - sizeof(read_response->byte_count) \
		       - DATA_TAIL_BYTE_COUNT;
	status = read_response->data_and_tail[ status_index ];

	free(read_command);
	free(read_response);
	return status;

i2c_read_fail:
	free(read_command);
	free(read_response);
	return -1;
}

static void i2c_gas_read(struct switchtec_dev *dev, void *dest,
			 const void __gas *src, size_t n)
{
	uint8_t status;
	uint8_t retry_count = 0;

	do {
		status = i2c_gas_data_read(dev, dest, src, n);
		if (status == 0 || status == GAS_TWI_MRPC_ERR)
			break;
		retry_count++;
	}while(retry_count < MAX_RETRY_COUNT);

	if (retry_count == MAX_RETRY_COUNT)
		raise(SIGBUS);
}

static void i2c_memcpy_from_gas(struct switchtec_dev *dev, void *dest,
			        const void __gas *src, size_t n)
{
	size_t cnt;

	while (n) {
		cnt = n > I2C_MAX_READ ? I2C_MAX_READ : n;
		i2c_gas_read(dev, dest, src, cnt);

		dest += cnt;
		src += cnt;
		n -= cnt;
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

// noop conversion functions to make macros below work
static inline uint8_t le8toh(uint8_t x) { return x; }

#define create_gas_read(type, suffix) \
	static type i2c_gas_read ## suffix(struct switchtec_dev *dev, \
					   type __gas *addr) \
	{ \
		type ret; \
		i2c_memcpy_from_gas(dev, &ret, addr, sizeof(ret)); \
		return le##suffix##toh(ret);                       \
	}

create_gas_read(uint8_t, 8);
create_gas_read(uint16_t, 16);
create_gas_read(uint32_t, 32);
create_gas_read(uint64_t, 64);

static void i2c_gas_write8(struct switchtec_dev *dev, uint8_t val,
			   uint8_t __gas *addr)
{
	i2c_gas_write(dev, addr, &val, sizeof(uint8_t));
}

static void i2c_gas_write16(struct switchtec_dev *dev, uint16_t val,
			    uint16_t __gas *addr)
{
	val = htole16(val);
	i2c_gas_write(dev, addr, &val, sizeof(uint16_t));
}

static void i2c_gas_write32(struct switchtec_dev *dev, uint32_t val,
			    uint32_t __gas *addr)
{
	val = htole32(val);
	i2c_gas_write(dev, addr, &val, sizeof(uint32_t));
}

static void i2c_gas_write32_no_retry(struct switchtec_dev *dev, uint32_t val,
				     uint32_t __gas *addr)
{
	val = htole32(val);
	i2c_gas_write_no_retry(dev, addr, &val, sizeof(uint32_t));
}

static void i2c_gas_write64(struct switchtec_dev *dev, uint64_t val,
			    uint64_t __gas *addr)
{
	val = htole64(val);
	i2c_gas_write(dev, addr, &val, sizeof(uint64_t));
}

static const struct switchtec_ops i2c_ops = {
	.close = i2c_close,
	.gas_map = i2c_gas_map,

	.cmd = gasop_cmd,
	.get_device_id = gasop_get_device_id,
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
	.gas_write32_no_retry = i2c_gas_write32_no_retry,
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

	if (i2c_set_timeout(idev, 10))
		goto err_close_free;

	if (i2c_gas_cap_get(&idev->dev) != TWI_ENHANCED_MODE)
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
