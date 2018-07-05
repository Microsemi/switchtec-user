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

#include <sys/file.h>
#include <termios.h>
#include "../crc8.h"

static const char *dbginfo_off = "pscdbg 0 all\r";
static const char *echo_off = "echo 0\r";
static const char *echo_on = "echo 1\r";

struct switchtec_uart{
	struct switchtec_dev dev;
	int fd;
};

#define to_switchtec_uart(d) \
	((struct switchtec_uart *) \
	 ((char *)d - offsetof(struct switchtec_uart, dev)))


#define UART_MAX_WRITE_BYTES			(100)
#define RETRY_NUM				(50)

static int cli_control(struct switchtec_dev *dev, const char *str)
{
	int ret;
	int idx;
	int i;
	char rtn[1024];
	int cnt = 0;
	struct switchtec_uart *udev = to_switchtec_uart(dev);

	ret = write(udev->fd, str, strlen(str));
	if (ret != strlen(str))
		return -1;

	for (i = 0; i< RETRY_NUM; i++) {
		ret = read(udev->fd, rtn + cnt, sizeof(rtn));
		if (ret < 0)
			return ret;

		cnt =+ ret;
		rtn[cnt] = '\0';

		if (sscanf(rtn, "%*[^:]:%x>", &idx) == 1)
			break;
		else
			usleep(5000);
	}

	if (i == RETRY_NUM)
		return -1;

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
	cli_control(dev, echo_on);

	if (dev->gas_map)
		munmap((void __force *)dev->gas_map, dev->gas_map_size);

	flock(udev->fd, LOCK_UN);
	close(udev->fd);
	free(udev);
}

static int map_gas(struct switchtec_dev *dev)
{
	void *addr;
	dev->gas_map_size = 4 << 20;

	addr = mmap(NULL, dev->gas_map_size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (addr == MAP_FAILED)
		return -1;

	dev->gas_map = (gasptr_t __force)addr;

	return 0;
}

#undef __force

static gasptr_t uart_gas_map(struct switchtec_dev *dev, int writeable, size_t *map_size)
{
	if (map_size)
		*map_size = dev->gas_map_size;

	return dev->gas_map;
}


static void uart_memcpy_from_gas(struct switchtec_dev *dev, void *dest, const void __gas *src, size_t n)
{
	int ret, raddr, rnum, rcrc, idx;
	int i;
	int crc_rty_cnt = 0;
	int cnt = 0;
	char *tok;
	const char *delims = "]";
	char gas_rd[1024];
	char gas_rd_rtn[102400];
	char *b = gas_rd;
	uint8_t crc_cal;
	uint8_t crc_buf[2*n];
	uint8_t *crc_ptr = crc_buf;
	uint32_t crc_cnt;
	uint32_t crc_addr;
	struct switchtec_uart *udev = to_switchtec_uart(dev);
	uint32_t addr = (uint32_t)(src - (void __gas *)dev->gas_map);

	b += snprintf(b, sizeof(gas_rd), "gasrd -c -s 0x%x %zu\r", addr, n);

rd_crc_err:
	if (crc_rty_cnt > RETRY_NUM)
		exit(1);

	ret = write(udev->fd, gas_rd, strlen(gas_rd));
	if (ret != strlen(gas_rd))
		exit(1);

	for (i = 0; i< RETRY_NUM; i++) {
		ret = read(udev->fd, gas_rd_rtn + cnt, sizeof(gas_rd_rtn));
		if (ret < 0)
			exit(1);

		cnt += ret;
		gas_rd_rtn[cnt] = '\0';

		if(sscanf(gas_rd_rtn, "%*[^<]<0x%x> [%d Byte]%*[^:]: 0x%x%*[^:]:%x>",
			  &raddr, &rnum, &rcrc, &idx) == 4)
			break;
		else
			usleep(5000);
	}

	if (i == RETRY_NUM)
		exit(1);

	assert(raddr == addr);
	assert(rnum == n);

	crc_addr = htobe32(addr);
	memcpy(crc_ptr, &crc_addr, sizeof(crc_addr));
	crc_ptr += sizeof(addr);

	tok = strtok(gas_rd_rtn,delims);
	for (i = 0; i<rnum; i++){
		tok = strtok(NULL, " ");
		*crc_ptr = strtol(tok, &tok, 16);
		crc_ptr += sizeof(uint8_t);
	}

	crc_cnt = sizeof(crc_addr) + n;
	crc_cal = crc8(crc_buf, crc_cnt, 0, true);
	if (crc_cal != rcrc){
		usleep(5000);
		crc_rty_cnt++;
		goto rd_crc_err;
	}

	memcpy(dest, crc_buf+sizeof(addr), n);

}

#define create_gas_read(type, suffix) \
	static type uart_gas_read ## suffix(struct switchtec_dev *dev, \
		type __gas *addr) \
	{ \
		type ret; \
		uart_memcpy_from_gas(dev, &ret, addr, sizeof(ret)); \
		return ret; \
	}
create_gas_read(uint8_t, 8);
create_gas_read(uint16_t, 16);
create_gas_read(uint32_t, 32);
create_gas_read(uint64_t, 64);

static void uart_gas_write(struct switchtec_dev *dev, void __gas *dest, const void *src, size_t n)
{
	int ret, idx;
	int cnt = 0;
	int i;
	int crc_rty_cnt = 0;
	char gas_wr[1024];
	char gas_wr_rtn[102400];
	char *b = gas_wr;
	uint32_t crc;
	uint32_t crc_addr;
	uint32_t crc_cal, crc_exp;
	uint8_t crc_buf[2*n];
	struct switchtec_uart *udev =  to_switchtec_uart(dev);
	uint32_t addr = (uint32_t)(dest - (void __gas *)dev->gas_map);

	crc_addr = htobe32(addr);
	memcpy(crc_buf, &crc_addr, sizeof(crc_addr));
	for (i = 0; i< n; i++){
		memcpy(crc_buf + sizeof(crc_addr) + i, (uint8_t *)src + n - 1 - i, sizeof(uint8_t));
	}
	crc = crc8(crc_buf, sizeof(crc_addr)+n, 0, true);

	b += snprintf(b, sizeof(gas_wr), "gaswr -c -s 0x%x 0x", addr);
	for (i = 0; i < n; i++ ){
		b += snprintf(b, sizeof(gas_wr), "%02x", *((uint8_t *)src + n - 1 - i));
	}
	b += snprintf(b, sizeof(gas_wr), " 0x%x\r", crc);

wr_crc_err:
	if (crc_rty_cnt > RETRY_NUM)
		exit(1);

	ret = write(udev->fd, gas_wr, strlen(gas_wr));
	if (ret != strlen(gas_wr))
		exit(1);

	for (i = 0; i< RETRY_NUM; i++) {
		ret = read(udev->fd, gas_wr_rtn + cnt, sizeof(gas_wr_rtn));
		if (ret < 0)
			exit(1);

		cnt += ret;
		gas_wr_rtn[cnt] = '\0';

		if (sscanf(gas_wr_rtn, "%*[^:]: [0x%x/0x%x]%*[^:]:%x>",
			   &crc_cal, &crc_exp, &idx) == 3)
			break;
		else
			usleep(5000);
	}

	if (i == RETRY_NUM)
		exit(1);

	if ((crc_exp != crc_cal) && (crc_cal != crc)){
		usleep(5000);
		crc_rty_cnt++;
		goto wr_crc_err;
	}
}

static void uart_memcpy_to_gas(struct switchtec_dev *dev, void __gas *dest, const void *src, size_t n)
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
	uart_attribs.c_cc[VTIME] = 5;

	ret = tcsetattr(fd, TCSANOW, &uart_attribs);
	if (ret)
		return -1;

	return 0;
}

struct switchtec_dev *switchtec_open_uart(const char *path)
{
	int ret;
	struct switchtec_uart *udev;

	udev = malloc(sizeof(*udev));
	if (!udev)
		return NULL;

	udev->fd = open(path, O_RDWR | O_NOCTTY);
	if (udev->fd < 0)
		goto err_free;

	ret = isatty(udev->fd);
	if (!ret)
		goto err_close_free;

	ret = flock(udev->fd, LOCK_EX);
	if (ret)
		goto err_close_free;

	ret = set_uart_attribs(udev->fd, B230400, 0);
	if (ret)
		goto err_close_free;

	ret = cli_control(&udev->dev, dbginfo_off);
	if (ret)
		goto err_close_free;

	ret = cli_control(&udev->dev, echo_off);
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

