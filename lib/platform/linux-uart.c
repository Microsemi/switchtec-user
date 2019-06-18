/*
 * Microsemi Switchtec(tm) PCIe Management Library
 * Copyright (c) 2018, Microsemi Corporation
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
#include "../crc8.h"
#include "switchtec/switchtec.h"
#include "gasops.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>

#include <sys/file.h>
#include <termios.h>

/*
 * Example of uart operations
 *
 * GAS Write:
 *
 * command: gaswr -c -s <offset> 0x<byte str> <crc>
 *
 * case 1: success
 * input:    gaswr -c -s 0x5 0xaabbccddeeff 0x84
 * output:   gas_reg_write() success
 * 	     CRC: [0x84/0x84]
 *	     0x00000000:1212>
 *
 * case 2: success
 * input:    gaswr -c -s 0x135c10 0x00000008 0xbc
 * output:   [PFF] cs addr: 0x0304, not hit
 *           gas_reg_write() success
 * 	     CRC: [0xbc/0xbc]
 *	     0x00000000:2172>
 *

 * case 3: crc error
 * input:    gaswr -c -s 0x5 0xaabbccddeeff 0xb
 * output:   gas_reg_write() CRC Error
 * 	     CRC: [0x84/0x0b]
 *	     0x00000000:0000>
 *
 * case 4: out of range
 * input:    gaswr -c -s 0x5135c00 0x00000000 0xe9
 * output:   Error with gas_reg_write(): 0x63006, Offset:0x5135c00
 *           CRC:[0xe9/0xe9]
 *           0x00000000:084d>
 *
 * GAS Read:
 *
 * command: gasrd -c -s <offset> <byte count>
 *
 * case 1: success
 * input:    gasrd -c -s 0x3 5
 * output:   gas_reg_read <0x3> [5 Byte]
 * 	     00 58 00 00 00
 * 	     CRC: 0x37
 *           0x00000000:1204>
 *
 * case 2: success
 * input:    gasrd -c -s 0x135c00 4
 * output:   gas_reg_read <0x135c00> [4 Byte]
 *           [PFF] cs addr: 0x0300,not hit
 * 	     00 00 00 00
 * 	     CRC: 0xb6
 * 	     0x00000000:0d93>
 *
 * case 3: out of range
 * input:    gasrd -c -s 0x5135c00 4
 * output:   gas_reg_read <0x5135c00> [4 Byte]
 *           No access beyond the Total GAS Section
 * 	     ...
 * 	     ...
 * 	     0x00000000:0d93>
 */

struct switchtec_uart{
	struct switchtec_dev dev;
	int fd;
};

#define to_switchtec_uart(d) \
	((struct switchtec_uart *) \
	 ((char *)(d) - offsetof(struct switchtec_uart, dev)))

#define UART_MAX_WRITE_BYTES			100
#define UART_MAX_READ_BYTES			1024
#define RETRY_NUM				3
#define SWITCHTEC_UART_BAUDRATE			(B230400)

static int send_cmd(int fd, const char *fmt, int write_bytes, ...)
{
	int ret;
	int i;
	int cnt;
	char cmd[1024];
	uint8_t *write_data;
	uint32_t write_crc;
	va_list argp;

	va_start(argp, write_bytes);

	if (write_bytes) {
		write_data = va_arg(argp, uint8_t *);
		write_crc = va_arg(argp, uint32_t);
	}

	cnt = vsnprintf(cmd, sizeof(cmd), fmt, argp);

	if (write_bytes) {
		for (i = 0; i< write_bytes; i++) {
			cnt += snprintf(cmd + cnt, sizeof(cmd) - cnt,
				       "%02x", write_data[write_bytes - 1 - i]);
		}

		cnt += snprintf(cmd + cnt, sizeof(cmd) - cnt,
				" 0x%x\r", write_crc);
	}

	va_end(argp);

	ret = write(fd, cmd, cnt);
	if (ret < 0)
		return ret;

	if (ret != cnt) {
		errno = EIO;
		return -errno;
	}

	return 0;
}

static int read_resp_line(int fd, char *str)
{
	int ret;
	int cnt = 0;

	while(1) {
		ret = read(fd, str + cnt, sizeof(str));
		if (ret <= 0)
			return ret;

		cnt += ret;
		str[cnt] = '\0';

		/* Prompt "0x12345678:1234>" */
		if (strrchr(str, ':') + 5 == strrchr(str, '>'))
			return 0;
	}

	return -1;
}

static int cli_control(struct switchtec_dev *dev, const char *str)
{
	int ret;
	char rtn[1024];
	struct switchtec_uart *udev = to_switchtec_uart(dev);

	ret = send_cmd(udev->fd, str, 0);
	if (ret)
		return ret;

	ret =  read_resp_line(udev->fd, rtn);
	if (ret)
		return ret;

	return 0;
}

#ifdef __CHECKER__
#define __force __attribute__((force))
#else
#define __force
#endif

static void uart_close(struct switchtec_dev *dev)
{
	struct switchtec_uart *udev =  to_switchtec_uart(dev);
	cli_control(dev, "echo 1\r");

	if (dev->gas_map)
		munmap((void __force *)dev->gas_map,
		      dev->gas_map_size);

	flock(udev->fd, LOCK_UN);
	close(udev->fd);
	free(udev);
}

static int map_gas(struct switchtec_dev *dev)
{
	void *addr;
	dev->gas_map_size = 4 << 20;

	addr = mmap(NULL, dev->gas_map_size,
		   PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (addr == MAP_FAILED)
		return -1;

	dev->gas_map = (gasptr_t __force)addr;

	return 0;
}

#undef __force

static gasptr_t uart_gas_map(struct switchtec_dev *dev, int writeable,
			    size_t *map_size)
{
	if (map_size)
		*map_size = dev->gas_map_size;

	return dev->gas_map;
}

static void uart_gas_read(struct switchtec_dev *dev, void *dest,
				const void __gas *src, size_t n)
{
	int ret;
	int raddr, rnum, rcrc;
	int i, j;
	char *pos;
	struct switchtec_uart *udev = to_switchtec_uart(dev);
	uint32_t addr = (uint32_t)(src - (void __gas *)dev->gas_map);
	uint8_t *ptr = dest;
	uint8_t cal;
	char gas_rd_rtn[4096];

	for (i = 0; i < RETRY_NUM; i++) {
		ret =  send_cmd(udev->fd, "gasrd -c -s 0x%x %zu\r", 0, addr, n);
		if (ret)
			continue;

		ret = read_resp_line(udev->fd, gas_rd_rtn);
		if (ret)
			continue;

		/* case 3 */
		if (strstr(gas_rd_rtn, "No access beyond the Total GAS Section")){
			memset(dest, 0xff, n);
			break;
		}
		/* case 2 */
		if (strchr(gas_rd_rtn, ',')) {
			if (sscanf(gas_rd_rtn,
				 "%*[^<]<0x%x> [%d Byte]%*[^,],%*[^:]: 0x"
				 "%x%*[^:]:",
				  &raddr, &rnum, &rcrc) != 3)
			continue;
		} else {
			/* case 1 */
			if (sscanf(gas_rd_rtn,
				 "%*[^<]<0x%x> [%d Byte]%*[^:]: 0x%x%*[^:]:",
				  &raddr, &rnum, &rcrc) != 3)
			continue;
		}

		if ((raddr != addr) || (rnum != n))
			continue;

		pos = strchr(gas_rd_rtn, ']');
		if (strchr(gas_rd_rtn, ','))
			pos = strchr(gas_rd_rtn, ',') + strlen("not hit");
		else
			pos += 2;

		for (j = 0; j < n; j++)
			*ptr++ = strtol(pos, &pos, 16);

		addr = htobe32(addr);
		cal = crc8((uint8_t *)&addr, sizeof(addr), 0, true);
		cal = crc8(dest, n, cal, false);
		if (cal == rcrc)
			break;
	}

	if (i == RETRY_NUM)
		raise(SIGBUS);
}

static void uart_memcpy_from_gas(struct switchtec_dev *dev, void *dest,
				const void __gas *src, size_t n)
{
	ssize_t cnt;

	while(n) {
		cnt = n > UART_MAX_READ_BYTES? UART_MAX_READ_BYTES : n;
		uart_gas_read(dev, dest, src, cnt);
		dest += cnt;
		src += cnt;
		n -= cnt;
	}
}

#define create_gas_read(type, suffix) \
	static type uart_gas_read ## suffix(struct switchtec_dev *dev, \
		type __gas *addr) \
	{ \
		type ret; \
		uart_gas_read(dev, &ret, addr, sizeof(ret)); \
		return ret; \
	}
create_gas_read(uint8_t, 8);
create_gas_read(uint16_t, 16);
create_gas_read(uint32_t, 32);
create_gas_read(uint64_t, 64);

static void uart_gas_write(struct switchtec_dev *dev, void __gas *dest,
			   const void *src, size_t n)
{
	int ret;
	int i;
	char gas_wr_rtn[4096];
	uint32_t crc;
	uint32_t cal, exp;
	struct switchtec_uart *udev =  to_switchtec_uart(dev);
	uint32_t addr = (uint32_t)(dest - (void __gas *)dev->gas_map);

	addr = htobe32(addr);
	crc = crc8((uint8_t *)&addr, sizeof(addr), 0, true);
	for (i = n; i > 0; i--)
		crc = crc8((uint8_t *)src + i - 1, sizeof(uint8_t), crc, false);

	addr = htobe32(addr);
	for (i = 0; i < RETRY_NUM; i++) {
		ret =  send_cmd(udev->fd, "gaswr -c -s 0x%x 0x",
			        n, src, crc, addr);
		if (ret)
			continue;

		ret = read_resp_line(udev->fd, gas_wr_rtn);
		if (ret)
			continue;

		/* case 4 */
		if (strstr(gas_wr_rtn, "Error with gas_reg_write()"))
			break;
		/* case 2 */
		if (strchr(gas_wr_rtn, ',')) {
			if (sscanf(gas_wr_rtn, "%*[^,],%*[^:]: [0x%x/0x%x]%*[^:]:",
				   &cal, &exp) != 2)
				continue;
		} else {
			/* case 1 and case 3 */
			if (sscanf(gas_wr_rtn, "%*[^:]: [0x%x/0x%x]%*[^:]:",
				   &cal, &exp) != 2)
				continue;
		}
		if ((exp == cal) && (cal == crc))
			break;
	}

	if (i == RETRY_NUM)
		raise(SIGBUS);
}

static void uart_memcpy_to_gas(struct switchtec_dev *dev, void __gas *dest,
			      const void *src, size_t n)
{
	size_t cnt;

	while(n){
		cnt = n > UART_MAX_WRITE_BYTES ? UART_MAX_WRITE_BYTES : n;
		uart_gas_write(dev, dest, src, cnt);
		dest += cnt;
		src += cnt;
		n -= cnt;
	}
}

#define create_gas_write(type, suffix) \
	static void uart_gas_write ## suffix(struct switchtec_dev *dev, \
					type val, type __gas *addr) \
	{ \
		uart_gas_write(dev, addr, &val, sizeof(type)); \
	}

create_gas_write(uint8_t, 8);
create_gas_write(uint16_t, 16);
create_gas_write(uint32_t, 32);
create_gas_write(uint64_t, 64);

static ssize_t uart_write_from_gas(struct switchtec_dev *dev, int fd,
				  const void __gas *src, size_t n)
{
	ssize_t ret;
	void *buf;

	buf = malloc(n);

	uart_memcpy_from_gas(dev, buf, src, n);
	ret = write(fd, buf, n);

	free(buf);

	return ret;
}

static const struct switchtec_ops uart_ops = {
	.close = uart_close,
	.gas_map = uart_gas_map,

	.cmd = gasop_cmd,
	.get_device_id = gasop_get_device_id,
	.get_fw_version = gasop_get_fw_version,
	.pff_to_port = gasop_pff_to_port,
	.port_to_pff = gasop_port_to_pff,
	.flash_part = gasop_flash_part,
	.event_summary = gasop_event_summary,
	.event_ctl = gasop_event_ctl,
	.event_wait_for = gasop_event_wait_for,

	.gas_read8 = uart_gas_read8,
	.gas_read16 = uart_gas_read16,
	.gas_read32 = uart_gas_read32,
	.gas_read64 = uart_gas_read64,
	.gas_write8 = uart_gas_write8,
	.gas_write16 = uart_gas_write16,
	.gas_write32 = uart_gas_write32,
	.gas_write64 = uart_gas_write64,

	.memcpy_to_gas = uart_memcpy_to_gas,
	.memcpy_from_gas = uart_memcpy_from_gas,
	.write_from_gas = uart_write_from_gas,
};

static int set_uart_attribs(int fd, int speed, int parity)
{
	int ret;
	struct termios uart_attribs;
	memset(&uart_attribs, 0, sizeof(uart_attribs));

	ret = tcgetattr(fd, &uart_attribs);
	if (ret)
		return -1;

	cfsetospeed(&uart_attribs, speed);
	cfsetispeed(&uart_attribs, speed);

	uart_attribs.c_iflag &= ~IGNBRK;
	uart_attribs.c_iflag &= ~(IXON | IXOFF | IXANY);
	uart_attribs.c_lflag = 0;
	uart_attribs.c_oflag = 0;
	uart_attribs.c_cflag = (uart_attribs.c_cflag & ~CSIZE) | CS8;
	uart_attribs.c_cflag |= (CLOCAL | CREAD);
	uart_attribs.c_cflag &= ~(PARENB | PARODD);
	uart_attribs.c_cflag |= parity;
	uart_attribs.c_cflag &= ~CSTOPB;
	uart_attribs.c_cflag &= ~CRTSCTS;
	uart_attribs.c_cc[VMIN] = 0;
	uart_attribs.c_cc[VTIME] = 50;

	ret = tcsetattr(fd, TCSANOW, &uart_attribs);
	if (ret)
		return -1;

	return 0;
}

struct switchtec_dev *switchtec_open_uart(int fd)
{
	int ret;
	struct switchtec_uart *udev;

	udev = malloc(sizeof(*udev));
	if (!udev)
		return NULL;

	udev->fd = fd;
	if (udev->fd < 0)
		goto err_free;

	ret = flock(udev->fd, LOCK_EX | LOCK_NB);
	if (ret)
		goto err_close_free;

	ret = set_uart_attribs(udev->fd, SWITCHTEC_UART_BAUDRATE, 0);
	if (ret)
		goto err_close_free;

	ret = cli_control(&udev->dev, "pscdbg 0 all\r");
	if (ret)
		goto err_close_free;

	ret = cli_control(&udev->dev, "echo 0\r");
	if (ret)
		goto err_close_free;

	if (map_gas(&udev->dev))
		goto err_close_free;

	udev->dev.ops = &uart_ops;
	gasop_set_partition_info(&udev->dev);
	return &udev->dev;

err_close_free:
	close(udev->fd);

err_free:
	free(udev);
	return NULL;
}

#endif

