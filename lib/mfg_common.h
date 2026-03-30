/*
 * Microsemi Switchtec(tm) PCIe Management Library
 * Copyright (c) 2019, Microsemi Corporation
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

#ifndef LIBSWITCHTEC_MFG_COMMON_H
#define LIBSWITCHTEC_MFG_COMMON_H

#include "switchtec_priv.h"
#include "switchtec/switchtec.h"

#include <errno.h>
#include <string.h>

#define SWITCHTEC_ACTV_IMG_ID_KMAN		1
#define SWITCHTEC_ACTV_IMG_ID_BL2		2
#define SWITCHTEC_ACTV_IMG_ID_CFG		3
#define SWITCHTEC_ACTV_IMG_ID_FW		4

#define SWITCHTEC_MB_MAX_ENTRIES		16
#define SWITCHTEC_ACTV_IDX_MAX_ENTRIES		32
#define SWITCHTEC_ACTV_IDX_SET_ENTRIES		5

#define SWITCHTEC_ATTEST_BITSHIFT		4
#define SWITCHTEC_ATTEST_BITMASK		0x03
#define SWITCHTEC_CLK_RATE_BITSHIFT		10
#define SWITCHTEC_CLK_RATE_BITMASK		0x0f
#define SWITCHTEC_RC_TMO_BITSHIFT		14
#define SWITCHTEC_RC_TMO_BITMASK		0x0f
#define SWITCHTEC_I2C_PORT_BITSHIFT		18
#define SWITCHTEC_I2C_PORT_BITMASK		0x0f
#define SWITCHTEC_I2C_ADDR_BITSHIFT		22
#define SWITCHTEC_I2C_ADDR_BITMASK		0x7f
#define SWITCHTEC_CMD_MAP_BITSHIFT		29
#define SWITCHTEC_CMD_MAP_BITMASK		0xfff
#define SWITCHTEC_UDS_SELFGEN_BITSHIFT		44
#define SWITCHTEC_UDS_SELFGEN_BITMASK		0x01

#define SWITCHTEC_JTAG_LOCK_AFT_RST_BITMASK	0x40
#define SWITCHTEC_JTAG_LOCK_AFT_BL1_BITMASK	0x80
#define SWITCHTEC_JTAG_UNLOCK_BL1_BITMASK	0x0100
#define SWITCHTEC_JTAG_UNLOCK_AFT_BL1_BITMASK	0x0200

#define SWITCHTEC_ACTV_IMG_ID_KMAN_GEN5		1
#define SWITCHTEC_ACTV_IMG_ID_RC_GEN5		2
#define SWITCHTEC_ACTV_IMG_ID_BL2_GEN5		3
#define SWITCHTEC_ACTV_IMG_ID_CFG_GEN5		4
#define SWITCHTEC_ACTV_IMG_ID_FW_GEN5		5
#define SWITCHTEC_I2C_ADDR_BITSHIFT_GEN5	23
#define SWITCHTEC_CMD_MAP_BITSHIFT_GEN5		30
#define SWITCHTEC_CMD_MAP_BITMASK_GEN5		0x3fff

static float spi_clk_rate_float[] = {
	100, 67, 50, 40, 33.33, 28.57, 25, 22.22, 20, 18.18
};

static float spi_clk_hi_rate_float[] = {
	120, 80, 60, 48, 40, 34, 30, 26.67, 24, 21.82
};

static inline void get_i2c_operands(enum switchtec_gen gen, uint32_t *addr_shift,
				    uint32_t *map_shift, uint32_t *map_mask)
{
	if (gen > SWITCHTEC_GEN4) {
		*addr_shift = SWITCHTEC_I2C_ADDR_BITSHIFT_GEN5;
		*map_shift = SWITCHTEC_CMD_MAP_BITSHIFT_GEN5;
		*map_mask = SWITCHTEC_CMD_MAP_BITMASK_GEN5;
	} else {
		*addr_shift = SWITCHTEC_I2C_ADDR_BITSHIFT;
		*map_shift = SWITCHTEC_CMD_MAP_BITSHIFT;
		*map_mask = SWITCHTEC_CMD_MAP_BITMASK;
	}
}

static inline int switchtec_mfg_cmd(struct switchtec_dev *dev, uint32_t cmd,
				    const void *payload, size_t payload_len,
				    void *resp, size_t resp_len)
{
	if (dev->ops->flags & SWITCHTEC_OPS_FLAG_NO_MFG) {
		errno = ERR_UART_NOT_SUPPORTED | SWITCHTEC_ERRNO_MRPC_FLAG_BIT;
		return -1;
	}

	return switchtec_cmd(dev, cmd, payload, payload_len,
			     resp, resp_len);
}

static inline int convert_spi_clk_rate(float clk_float, int hi_rate)
{
	int i;
	float *p;

	if (hi_rate)
		p = spi_clk_hi_rate_float;
	else
		p = spi_clk_rate_float;

	for (i = 0; i < 10; i++)
		if ((clk_float < p[i] + 0.1) && (clk_float > p[i] - 0.1))
			return i + 1;

	return -1;
}

static inline int kmsk_set_send_pubkey(struct switchtec_dev *dev,
				       struct switchtec_pubkey *public_key,
				       uint32_t cmd_id)
{
	struct kmsk_pubk_cmd {
		uint8_t subcmd;
		uint8_t reserved[3];
		uint8_t pub_key[SWITCHTEC_PUB_KEY_LEN];
		uint32_t pub_key_exponent;
	} cmd = {};

	cmd.subcmd = MRPC_KMSK_ENTRY_SET_PKEY;
	memcpy(cmd.pub_key, public_key->pubkey,
	       SWITCHTEC_PUB_KEY_LEN);
	cmd.pub_key_exponent = htole32(public_key->pubkey_exp);

	return switchtec_mfg_cmd(dev, cmd_id, &cmd,
				 sizeof(cmd), NULL, 0);
}

static inline int kmsk_set_send_signature(struct switchtec_dev *dev,
					  struct switchtec_signature *signature,
					  uint32_t cmd_id)
{
	struct kmsk_signature_cmd {
		uint8_t subcmd;
		uint8_t reserved[3];
		uint8_t signature[SWITCHTEC_SIG_LEN];
	} cmd = {};

	cmd.subcmd = MRPC_KMSK_ENTRY_SET_SIG;
	memcpy(cmd.signature, signature->signature,
	       SWITCHTEC_SIG_LEN);

	return switchtec_mfg_cmd(dev, cmd_id, &cmd,
				 sizeof(cmd), NULL, 0);
}

static inline int kmsk_set_send_kmsk(struct switchtec_dev *dev,
				     struct switchtec_kmsk *kmsk, uint32_t cmd_id)
{
	struct kmsk_kmsk_cmd {
		uint8_t subcmd;
		uint8_t num_entries;
		uint8_t reserved[2];
		uint8_t kmsk[SWITCHTEC_KMSK_LEN];
	} cmd = {};

	cmd.subcmd = MRPC_KMSK_ENTRY_SET_KMSK;
	cmd.num_entries = 1;
	memcpy(cmd.kmsk, kmsk->kmsk, SWITCHTEC_KMSK_LEN);

	return switchtec_mfg_cmd(dev, cmd_id, &cmd, sizeof(cmd),
				 NULL, 0);
}

static inline uint32_t get_dbg_unlock_id(struct switchtec_dev *dev)
{
	if(switchtec_is_gen6(dev))
		return MRPC_DBG_UNLOCK_GEN6;
	else if (switchtec_is_gen5(dev))
		return MRPC_DBG_UNLOCK_GEN5;
	else
		return MRPC_DBG_UNLOCK;
}

static inline int dbg_unlock_send_pubkey(struct switchtec_dev *dev,
					 struct switchtec_pubkey *public_key,
				  	 uint32_t cmd_id)
{
	struct public_key_cmd {
		uint8_t subcmd;
		uint8_t rsvd[3];
		uint8_t pub_key[SWITCHTEC_PUB_KEY_LEN];
		uint32_t pub_key_exp;
	} cmd = {};

	cmd.subcmd = MRPC_DBG_UNLOCK_PKEY;
	memcpy(cmd.pub_key, public_key->pubkey, SWITCHTEC_PUB_KEY_LEN);
	cmd.pub_key_exp = htole32(public_key->pubkey_exp);

	return switchtec_mfg_cmd(dev, cmd_id, &cmd, sizeof(cmd), NULL, 0);
}

#endif