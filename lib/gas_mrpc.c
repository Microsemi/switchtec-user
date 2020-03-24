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

#include "switchtec/gas_mrpc.h"
#include "switchtec/switchtec.h"
#include "switchtec_priv.h"

#include <string.h>
#include <signal.h>
#include <unistd.h>

/**
 * @defgroup GASMRPC Access through MRPC commands
 * @brief Access the GAS through MRPC commands
 *
 * Access the gas through MRPC commands. The linux kernel may reject
 * these commands if the process has insufficient permission.
 *
 * MRPC commands respect the PAX ID where as standard gas access mechanisms
 * may not.
 *
 * These functions should generally not be used unless you really know what
 * you are doing. The regular gas accessors (ie. gas_*()) will call these
 * functions when switchtec_set_pax_id() has been used.
 *
 * @{
 */

/**
 * @brief Copy data to the GAS using MRPC commands
 * @param[in]  dev	Switchtec device handle
 * @param[out] dest	Destination gas address
 * @param[in]  src	Source data buffer
 * @param[in]  n	Number of bytes to transfer
 */
void gas_mrpc_memcpy_to_gas(struct switchtec_dev *dev, void __gas *dest,
			    const void *src, size_t n)
{
	struct gas_mrpc_write cmd;
	int ret;
	uint32_t len;
	uint32_t offset = (uint32_t)(dest - (void __gas *)dev->gas_map);

	while (n) {
		len = n;
		if (len > sizeof(cmd.data))
			len = sizeof(cmd.data);
		cmd.len = htole32(len);
		cmd.gas_offset = htole32(offset);
		memcpy(&cmd.data, src, len);

		ret = switchtec_cmd(dev, MRPC_GAS_WRITE, &cmd,
				    len + sizeof(cmd) - sizeof(cmd.data),
				    NULL, 0);
		if (ret)
			raise(SIGBUS);

		n -= len;
		offset += len;
	}
}

/**
 * @brief Copy data from the GAS using MRPC commands
 * @param[in]  dev	Switchtec device handle
 * @param[out] dest	Destination buffer
 * @param[in]  src	Source gas address
 * @param[in]  n	Number of bytes to transfer
 */
void gas_mrpc_memcpy_from_gas(struct switchtec_dev *dev, void *dest,
			      const void __gas *src, size_t n)
{
	struct gas_mrpc_read cmd;
	int ret;
	uint32_t len;

	cmd.gas_offset = htole32((uint32_t)(src - (void __gas *)dev->gas_map));

	while (n) {
		len = n;
		if (len > MRPC_MAX_DATA_LEN)
			len = MRPC_MAX_DATA_LEN;
		cmd.len = htole32(len);

		ret = switchtec_cmd(dev, MRPC_GAS_READ, &cmd,
				    sizeof(cmd), dest, len);
		if (ret)
			raise(SIGBUS);

		n -= len;
		dest += len;
	}
}

/**
 * @brief Call write() with data from the GAS using an MRPC command
 * @param[in] dev	Switchtec device handle
 * @param[in] fd	Destination buffer
 * @param[in] src	Source gas address
 * @param[in] n		Number of bytes to transfer
 */
ssize_t gas_mrpc_write_from_gas(struct switchtec_dev *dev, int fd,
				const void __gas *src, size_t n)
{
	char buf[MRPC_MAX_DATA_LEN];
	ssize_t ret, total = 0;
	size_t txfr_sz;

	while (n) {
		txfr_sz = n;
		if (txfr_sz > sizeof(buf))
			txfr_sz = sizeof(buf);

		gas_mrpc_memcpy_from_gas(dev, buf, src, txfr_sz);

		ret = write(fd, buf, txfr_sz);
		if (ret < 0)
			return ret;

		n -= ret;
		src += ret;
		total += ret;
	}

	return total;
}

/**@}*/
