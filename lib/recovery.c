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

/**
 * @file
 * @brief Switchtec core library functions for secure boot operations
 */

#include "switchtec_priv.h"
#include "switchtec/switchtec.h"
#include "switchtec/errors.h"
#include "switchtec/endian.h"
#include "switchtec/mrpc.h"
#include "switchtec/errors.h"
#include <unistd.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>


static enum switchtec_gen map_to_gen(uint32_t gen)
{
	enum switchtec_gen ret = SWITCHTEC_GEN_UNKNOWN;

	switch (gen) {
	case 0:
		ret = SWITCHTEC_GEN4;
		break;
	default:
		ret = SWITCHTEC_GEN_UNKNOWN;
		break;
	}

	return ret;
}

/**
 * @brief Ping a device with a 32-bit data.
 * @param[in]  dev	Switchtec device handle
 * @param[in]  ping_dw	Ping data
 * @param[out] reply_dw	Ping reply data (2's complement of ping data)
 * @param[out] phase	The current boot phase
 * @param[out] gen	Device generation
 * @param[out] rev	Device revision
 * @return 0 on success, error code on failure
 */
int switchtec_ping(struct switchtec_dev *dev, uint32_t ping_dw,
		   uint32_t *reply_dw, enum switchtec_boot_phase *phase,
		   enum switchtec_gen *gen, enum switchtec_rev *rev)
{
	int ret;
	struct ping_reply {
		uint32_t stat;
		uint32_t reply;
	} r;
	uint32_t hw_stat;
	uint32_t ping_dw_le = htole32(ping_dw);

	ret = switchtec_cmd(dev, MRPC_PING, &ping_dw_le,
			sizeof(ping_dw_le),
			&r, sizeof(r));
	if (ret == 0) {
		*reply_dw = le32toh(r.reply);
		hw_stat = le32toh(r.stat);
		*phase = hw_stat & 0xff;
		*rev = (hw_stat >> 8) & 0x0f;
		*gen = map_to_gen((hw_stat >> 12) & 0x0f);
	} else {
		if ((errno & (~SWITCHTEC_ERRNO_MRPC_FLAG_BIT))
		    == ERR_MPRC_UNSUPPORTED) {
			*reply_dw = ~ping_dw;
			*phase = SWITCHTEC_BOOT_PHASE_FW;
			*gen = SWITCHTEC_GEN3;
			*rev = SWITCHTEC_REVA;

			errno = 0;
		} else
			return -1;
	}
	return 0;
}

/**
 * @brief Get current boot phase
 * @param[in]  dev	Switchtec device handle
 * @param[out] phase	Current boot phase
 * @return 0 on success, error code on failure
 */
int switchtec_get_boot_phase(struct switchtec_dev *dev,
			     enum switchtec_boot_phase *phase_id)
{
	int ret;
	enum switchtec_gen gen;
	uint32_t t = 0;
	uint32_t r = 0;
	uint32_t rev;

	ret = switchtec_ping(dev, t, &r, phase_id, &gen, &rev);
	if (ret)
		return -1;
	return 0;
}
