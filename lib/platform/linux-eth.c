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

#include <linux/switchtec_ioctl.h>

#include <unistd.h>
#include <fcntl.h>
#include <endian.h>
#include <dirent.h>
#include <libgen.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/sysmacros.h>
#include <arpa/inet.h>
#include <glob.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>

#include <errno.h>
#include <string.h>
#include <stddef.h>

#define ETH_SERVER_PORT 54545

#define ETH_CHAN_TYPE_COMMAND 0x1
#define ETH_CHAN_TYPE_EVENT   0x2

#define ETH_PROT_SIGNATURE 0x6d6c7373
#define ETH_PROT_VERSION   0x1

#define ETH_PACKET_TYPE_OPEN 0xB1
#define ETH_PACKET_TYPE_CMD  0xB2

#define ETH_FUNC_TYPE_OPEN_REQUEST 0x1
#define ETH_FUNC_TYPE_OPEN_ACCEPT  0x2
#define ETH_FUNC_TYPE_OPEN_REJECT  0x3
#define ETH_FUNC_TYPE_OPEN_CLOSE   0x4

#define ETH_FUNC_TYPE_MRPC_CMD  0x1
#define ETH_FUNC_TYPE_MOE_CMD   0x2
#define ETH_FUNC_TYPE_MRPC_RESP 0x3
#define ETH_FUNC_TYPE_EVENT     0x4
#define ETH_FUNC_TYPE_MOE_RESP  0x5

#define ETH_INST_ID_0 0x0
#define ETH_INST_ID_1 0x1

#define ETH_GAS_READ_CMD_ID  0x1001
#define ETH_GAS_WRITE_CMD_ID 0x1002

#define ETH_MAX_READ 512

struct switchtec_eth {
	struct switchtec_dev dev;
	int cmd_fd;
	int evt_fd;
};

#define to_switchtec_eth(d)  \
	((struct switchtec_eth *) \
	((char *)d - offsetof(struct switchtec_eth, dev)))

struct eth_header {
	uint32_t signature;
	uint8_t version_id;
	uint8_t rsvd0;
	uint8_t function_type;
	uint8_t packet_type;
	union {
		uint8_t service_inst;
		uint8_t rsvd1;
	};
	union {
		uint8_t service_type;
		uint8_t rsvd2;
	};
	uint16_t payload_bytes;
	union {
		uint16_t mrpc_output_bytes;
		uint16_t return_code;
	};
	uint16_t rsvd3;
};

struct eth_packet {
	struct eth_header hdr;
	uint8_t body[MRPC_MAX_DATA_LEN + 4];
};

static int send_eth_command(int cmd_fd, int func_type, uint8_t *data,
			    uint32_t data_len, uint32_t mrpc_output_len)
{
	size_t packet_len;
	struct eth_packet *command_p;

	packet_len = offsetof(struct eth_packet, body) + data_len;
	command_p = malloc(packet_len);

	command_p->hdr.signature = htonl(ETH_PROT_SIGNATURE);
	command_p->hdr.version_id = ETH_PROT_VERSION;
	command_p->hdr.function_type = func_type;
	command_p->hdr.packet_type = ETH_PACKET_TYPE_CMD;
	command_p->hdr.payload_bytes = htons(data_len);
	command_p->hdr.mrpc_output_bytes = htons(mrpc_output_len);

	memcpy(command_p->body, data, data_len);

	if (send(cmd_fd, command_p, packet_len, 0) < 0) {
		free(command_p);
		return -1;
	}

	free(command_p);
	return 0;
}

static int recv_eth_response(int cmd_fd, uint32_t *result,
			     uint8_t *output, uint32_t *output_len)
{
	struct eth_packet recvd_p;
	void *p;
	uint32_t len;
	uint16_t func_type;
	uint16_t packet_type;

	len = sizeof(struct eth_header);

	if (recv(cmd_fd, &recvd_p.hdr, len, 0) < 0)
		return -1;

	func_type = recvd_p.hdr.function_type;
	packet_type = recvd_p.hdr.packet_type;

	if ((func_type == ETH_FUNC_TYPE_OPEN_CLOSE)
	&& (packet_type == ETH_PACKET_TYPE_OPEN))
		return -2;

	len = ntohs(recvd_p.hdr.payload_bytes);
	p = recvd_p.body;

	if (!len)
		return 0;

	if (recv(cmd_fd, p, len, 0) < 0)
		return -3;

	if (packet_type == ETH_PACKET_TYPE_CMD) {
		*result = le32toh(*(uint32_t *)p);
		p += sizeof(uint32_t);
		len -= sizeof(uint32_t);
		if (output)
			memcpy(output, p, len);
		if (output_len)
			*output_len = len;
	}

	return 0;
}

static int switchtec_submit_cmd_eth(struct switchtec_dev *dev, uint32_t cmd,
				    const void *payload, size_t payload_len,
				    size_t resp_len)
{
	struct switchtec_eth *edev = to_switchtec_eth(dev);
	uint32_t body_len;
	int ret;

	struct eth_mrpc_body{
		uint32_t command_id;
		uint8_t data[];
	} __attribute__(( packed )) *mrpc_body;

	body_len = offsetof(struct eth_mrpc_body, data) + payload_len;
	mrpc_body = malloc(body_len);
	memset(mrpc_body, 0, body_len);

	mrpc_body->command_id = htole32(cmd);
	memcpy(mrpc_body->data, payload, payload_len);

	ret = send_eth_command(edev->cmd_fd, ETH_FUNC_TYPE_MRPC_CMD,
			       (uint8_t *)mrpc_body, body_len, resp_len);
	free(mrpc_body);

	return ret;
}

static int switchtec_read_resp_eth(struct switchtec_dev *dev, void *resp,
				   size_t resp_len)
{
	struct switchtec_eth *edev = to_switchtec_eth(dev);
	uint32_t ret;
	uint32_t result;
	uint8_t buf[resp_len];
	uint32_t received_len;

	ret = recv_eth_response(edev->cmd_fd, &result, buf, &received_len);
	if (ret)
		return ret;

	if (received_len != resp_len) {
		errno = EIO;
		return -errno;
	}

	if (result)
		errno = result;

	if (!resp)
		return result;

	memcpy(resp, buf, resp_len);

	return result;
}

static int eth_cmd(struct switchtec_dev *dev, uint32_t cmd,
		   const void *payload, size_t payload_len,
		   void *resp, size_t resp_len)
{
	int ret;

	ret = switchtec_submit_cmd_eth(dev, cmd, payload,
				       payload_len, resp_len);

	if (ret < 0)
		return ret;

	return switchtec_read_resp_eth(dev, resp, resp_len);
}

#ifdef __CHECKER__
#define __force __attribute__((force))
#else
#define __force
#endif

static int eth_gas_write_exec(int fd, uint32_t offset,
			      const void *data, uint16_t bytes)
{
	uint32_t result;
	uint32_t body_len;
	int ret;

	struct eth_gas_write_body{
		uint32_t command_id;
		uint32_t offset;
		uint16_t bytes;
		uint16_t reserved;
		uint8_t data[];
	} __attribute__(( packed )) *gas_write_body;

	body_len = offsetof(struct eth_gas_write_body, data) + bytes;
	gas_write_body = malloc(body_len);
	memset(gas_write_body, 0, body_len);

	gas_write_body->command_id = htole32(ETH_GAS_WRITE_CMD_ID);
	gas_write_body->offset = htole32(offset);
	gas_write_body->bytes = htole16(bytes);

	memcpy(gas_write_body->data, data, bytes);

	ret = send_eth_command(fd, ETH_FUNC_TYPE_MOE_CMD,
			       (uint8_t *)gas_write_body, body_len, 0);

	free(gas_write_body);

	if (ret)
		return ret;

	ret = recv_eth_response(fd, &result, NULL, NULL);

	return ret;
}

static int eth_gas_read_exec(struct switchtec_dev *dev, uint32_t offset,
			     uint8_t *data, size_t bytes)
{
	struct switchtec_eth *edev = to_switchtec_eth(dev);
	uint32_t result;
	uint32_t data_len;
	size_t body_len;
	int ret;

	struct eth_gas_write_body{
		uint32_t command_id;
		uint32_t offset;
		uint16_t bytes;
		uint16_t reserved;
	} __attribute__(( packed )) gas_read_body;

	gas_read_body.command_id = htole32(ETH_GAS_READ_CMD_ID);
	gas_read_body.offset = htole32(offset);
	gas_read_body.bytes = htole16(bytes);

	body_len = sizeof(gas_read_body);

	ret = send_eth_command(edev->cmd_fd, ETH_FUNC_TYPE_MOE_CMD,
			       (uint8_t *)&gas_read_body, body_len, 0);

	if (ret)
		return ret;

	ret = recv_eth_response(edev->cmd_fd, &result, data, &data_len);

	return ret;
}

static void eth_gas_read(struct switchtec_dev *dev, void *dest,
			 const void __gas *src, size_t n)
{
	uint32_t gas_addr;
	int ret;

	gas_addr = (uint32_t)(src - (void __gas *)dev->gas_map);
	ret = eth_gas_read_exec(dev, gas_addr, dest, n);
	if (ret)
		raise(SIGBUS);
}

static void eth_gas_write(struct switchtec_dev *dev, void __gas *dest,
			  const void *src, size_t n)
{
	uint32_t gas_addr;
	struct switchtec_eth *edev = to_switchtec_eth(dev);
	int ret;

	gas_addr = (uint32_t)(dest - (void __gas *)dev->gas_map);
	ret = eth_gas_write_exec(edev->cmd_fd, gas_addr, src, n);
	if (ret)
		raise(SIGBUS);
}

static void eth_gas_write8(struct switchtec_dev *dev, uint8_t val,
			   uint8_t __gas *addr)
{
	eth_gas_write(dev, addr, &val, sizeof(uint8_t));
}

static void eth_gas_write16(struct switchtec_dev *dev, uint16_t val,
			    uint16_t __gas *addr)
{
	val = htole16(val);
	eth_gas_write(dev, addr, &val, sizeof(uint16_t));
}

static void eth_gas_write32(struct switchtec_dev *dev, uint32_t val,
			    uint32_t __gas *addr)
{
	val = htole32(val);
	eth_gas_write(dev, addr, &val, sizeof(uint32_t));
}

static void eth_gas_write64(struct switchtec_dev *dev, uint64_t val,
			    uint64_t __gas *addr)
{
	val = htole64(val);
	eth_gas_write(dev, addr, &val, sizeof(uint64_t));
}

static void eth_memcpy_from_gas(struct switchtec_dev *dev, void *dest,
			        const void __gas *src, size_t n)
{
	eth_gas_read(dev, dest, src, n);
}

static void eth_memcpy_to_gas(struct switchtec_dev *dev, void __gas *dest,
			      const void *src, size_t n)
{
	eth_gas_write(dev, dest, src, n);
}

static ssize_t eth_write_from_gas(struct switchtec_dev *dev, int fd,
				  const void __gas *src, size_t n)
{
	ssize_t ret = 0;
	uint8_t buf[ETH_MAX_READ];
	int cnt;

	while (n) {
		cnt = n > ETH_MAX_READ ? ETH_MAX_READ : n;
		eth_memcpy_from_gas(dev, buf, src, cnt);
		ret +=write(fd, buf, cnt);

		src += cnt;
		n -= cnt;
	}

	return ret;
}

static uint8_t eth_gas_read8(struct switchtec_dev *dev, uint8_t __gas *addr)
{
	uint8_t val;

	eth_gas_read(dev, &val, addr, sizeof(val));
	return val;
}

static uint16_t eth_gas_read16(struct switchtec_dev *dev, uint16_t __gas *addr)
{
	uint16_t val;

	eth_gas_read(dev, &val, addr, sizeof(val));
	return le16toh(val);
}

static uint32_t eth_gas_read32(struct switchtec_dev *dev, uint32_t __gas *addr)
{
	uint32_t val;

	eth_gas_read(dev, &val, addr, sizeof(val));
	return le32toh(val);
}

static uint64_t eth_gas_read64(struct switchtec_dev *dev, uint64_t __gas *addr)
{
	uint64_t val;

	eth_gas_read(dev, &val, addr, sizeof(val));
	return le64toh(val);
}

static void eth_close(struct switchtec_dev *dev)
{
	struct switchtec_eth *edev = to_switchtec_eth(dev);

	if (dev->gas_map)
		munmap((void __force *)dev->gas_map, dev->gas_map_size);

	close(edev->cmd_fd);
	free(edev);
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

static gasptr_t eth_gas_map(struct switchtec_dev *dev, int writeable,
			    size_t *map_size)
{
	if (map_size)
		*map_size = dev->gas_map_size;

	return dev->gas_map;
}

static int eth_event_wait(struct switchtec_dev *dev, int timeout_ms)
{
	int ret;
	struct eth_packet recvd_p;
	struct switchtec_eth *edev = to_switchtec_eth(dev);
	uint32_t len;

	len = sizeof(struct eth_header);

	if (timeout_ms != -1)
		setsockopt(edev->evt_fd, SOL_SOCKET, SO_RCVTIMEO,
			   (char *)&timeout_ms, sizeof(int));

	ret = recv(edev->evt_fd, &recvd_p.hdr, len, 0);
	if (ret <= 0)
		return ret;

	if ((recvd_p.hdr.packet_type == ETH_PACKET_TYPE_CMD)
	    && (recvd_p.hdr.function_type == ETH_FUNC_TYPE_EVENT))
		return 1;

	return 0;
}

static const struct switchtec_ops eth_ops = {
	.close = eth_close,
	.gas_map = eth_gas_map,
	.cmd = eth_cmd,
	.get_device_id = gasop_get_device_id,
	.get_fw_version = gasop_get_fw_version,
	.pff_to_port = gasop_pff_to_port,
	.port_to_pff = gasop_port_to_pff,
	.flash_part = gasop_flash_part,
	.event_summary = gasop_event_summary,
	.event_ctl = gasop_event_ctl,
	.event_wait = eth_event_wait,

	.gas_read8 = eth_gas_read8,
	.gas_read16 = eth_gas_read16,
	.gas_read32 = eth_gas_read32,
	.gas_read64 = eth_gas_read64,
	.gas_write8 = eth_gas_write8,
	.gas_write16 = eth_gas_write16,
	.gas_write32 = eth_gas_write32,
	.gas_write32_no_retry = eth_gas_write32,
	.gas_write64 = eth_gas_write64,
	.memcpy_to_gas = eth_memcpy_to_gas,
	.memcpy_from_gas = eth_memcpy_from_gas,
	.write_from_gas = eth_write_from_gas,
};

static int open_eth_chan(const char *server_ip, int server_port,
			 int chan_type, int moe_inst_id)
{
	int fd;
	struct eth_packet *open_p;

	struct sockaddr_in server;
	uint32_t len;
	int ret;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1)
		return -1;
	ret = fd;

	server.sin_addr.s_addr = inet_addr(server_ip);
	server.sin_family = AF_INET;
	server.sin_port = htons(server_port);

	if (connect(fd, (struct sockaddr *)&server , sizeof(server)) < 0) {
		close(fd);
		return -2;
	}

	len = sizeof(struct eth_header);

	open_p = malloc(sizeof(struct eth_packet));

	open_p->hdr.signature = htonl(ETH_PROT_SIGNATURE);
	open_p->hdr.version_id = ETH_PROT_VERSION;
	open_p->hdr.function_type = ETH_FUNC_TYPE_OPEN_REQUEST;
	open_p->hdr.packet_type = ETH_PACKET_TYPE_OPEN;
	open_p->hdr.service_inst = moe_inst_id;
	open_p->hdr.service_type = chan_type;

	if (send(fd, open_p, len, 0) < 0) {
		ret = -3;
		goto out_free;
	}

	len = sizeof(struct eth_header);
	if (recv(fd, open_p, len, 0) < 0) {
		ret = -4;
		goto out_free;
	}

	if (!((open_p->hdr.function_type == ETH_FUNC_TYPE_OPEN_ACCEPT)
	    && (open_p->hdr.return_code == 0)))
		ret = -5;
out_free:
	free(open_p);
	return ret;

}

struct switchtec_dev *switchtec_open_eth(const char *ip, const int inst)
{
	struct switchtec_eth *edev;

	edev = malloc(sizeof(*edev));
	if (!edev)
		return NULL;

	edev->cmd_fd = open_eth_chan(ip, ETH_SERVER_PORT,
				     ETH_CHAN_TYPE_COMMAND, inst);
	if (edev->cmd_fd < 0)
		goto err_close_cmd_free;

	edev->evt_fd = open_eth_chan(ip, ETH_SERVER_PORT,
				     ETH_CHAN_TYPE_EVENT, inst);
	if (edev->evt_fd < 0)
		goto err_close_free;

	if (map_gas(&edev->dev))
		goto err_close_free;

	edev->dev.ops = &eth_ops;

	gasop_set_partition_info(&edev->dev);

	return &edev->dev;

err_close_free:
	close(edev->evt_fd);
err_close_cmd_free:
	close(edev->cmd_fd);

	free(edev);
	return NULL;
}

#endif
