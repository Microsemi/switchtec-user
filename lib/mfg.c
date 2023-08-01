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

/**
 * @file
 * @brief Switchtec core library functions for mfg operations
 */

/**
 * @defgroup mfg Manufacturing Functions
 * @brief Manufacturing-related API functions
 *
 * These are functions used during manufacturing process. These
 * includes functions that configure device security settings and
 * recover device from boot failures.
 *
 * Some of these functions modify device One-Time-Programming (OTP) memory,
 * so they should be used with great caution, and you should really
 * know what you are doing when calling these functions. FAILURE TO DO SO
 * COULD MAKE YOUR DEVICE UNBOOTABLE!!
 *
 * @{
 */

#include "switchtec_priv.h"
#include "switchtec/switchtec.h"
#include "switchtec/mfg.h"
#include "switchtec/errors.h"
#include "switchtec/endian.h"
#include "switchtec/mrpc.h"
#include "switchtec/errors.h"
#include <unistd.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "lib/crc.h"
#include "config.h"

#ifdef __linux__

#if HAVE_LIBCRYPTO
#include <openssl/pem.h>
#endif

#define SWITCHTEC_ACTV_IMG_ID_KMAN		1
#define SWITCHTEC_ACTV_IMG_ID_BL2		2
#define SWITCHTEC_ACTV_IMG_ID_CFG		3
#define SWITCHTEC_ACTV_IMG_ID_FW		4

#define SWITCHTEC_ACTV_IMG_ID_KMAN_GEN5		1
#define SWITCHTEC_ACTV_IMG_ID_RC_GEN5		2
#define SWITCHTEC_ACTV_IMG_ID_BL2_GEN5		3
#define SWITCHTEC_ACTV_IMG_ID_CFG_GEN5		4
#define SWITCHTEC_ACTV_IMG_ID_FW_GEN5		5

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
#define SWITCHTEC_I2C_ADDR_BITSHIFT_GEN5	23
#define SWITCHTEC_I2C_ADDR_BITMASK		0x7f
#define SWITCHTEC_CMD_MAP_BITSHIFT		29
#define SWITCHTEC_CMD_MAP_BITSHIFT_GEN5		30
#define SWITCHTEC_CMD_MAP_BITMASK		0xfff
#define SWITCHTEC_CMD_MAP_BITMASK_GEN5		0x3fff
#define SWITCHTEC_UDS_SELFGEN_BITSHIFT		44
#define SWITCHTEC_UDS_SELFGEN_BITMASK		0x01

#define SWITCHTEC_JTAG_LOCK_AFT_RST_BITMASK	0x40
#define SWITCHTEC_JTAG_LOCK_AFT_BL1_BITMASK	0x80
#define SWITCHTEC_JTAG_UNLOCK_BL1_BITMASK	0x0100
#define SWITCHTEC_JTAG_UNLOCK_AFT_BL1_BITMASK	0x0200

static int switchtec_mfg_cmd(struct switchtec_dev *dev, uint32_t cmd,
			     const void *payload, size_t payload_len,
			     void *resp, size_t resp_len);

#if (HAVE_LIBCRYPTO && !HAVE_DECL_RSA_GET0_KEY)
/**
*  openssl1.0 or older versions don't have this function, so copy
*  the code from openssl1.1 here
*/
static void RSA_get0_key(const RSA *r, const BIGNUM **n,
			 const BIGNUM **e, const BIGNUM **d)
{
	if (n != NULL)
		*n = r->n;
	if (e != NULL)
		*e = r->e;
	if (d != NULL)
		*d = r->d;
}
#endif

static void get_i2c_operands(enum switchtec_gen gen, uint32_t *addr_shift,
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

static float spi_clk_rate_float[] = {
	100, 67, 50, 40, 33.33, 28.57, 25, 22.22, 20, 18.18
};

static float spi_clk_hi_rate_float[] = {
	120, 80, 60, 48, 40, 34, 30, 26.67, 24, 21.82
};

struct get_cfgs_reply {
	uint32_t valid;
	uint32_t rsvd1;
	uint64_t cfg;
	uint32_t public_key_exponent;
	uint8_t rsvd2;
	uint8_t public_key_num;
	uint8_t public_key_ver;
	uint8_t spi_core_clk_high;
	uint8_t public_key[4][SWITCHTEC_KMSK_LEN];
	uint8_t rsvd4[32];
};

struct get_cfgs_reply_gen5 {
	uint32_t valid0;
	uint32_t valid1;
	uint64_t cfg;
	uint32_t public_key_exponent;
	uint8_t rsvd2;
	uint8_t public_key_num;
	uint8_t public_key_ver;
	uint8_t spi_core_clk_high;
	uint8_t public_key[10][SWITCHTEC_KMSK_LEN];
	uint32_t cdi_efuse_inc_mask;
	uint8_t uds_data[32];
};

static int get_configs(struct switchtec_dev *dev,
		       struct get_cfgs_reply *cfgs,
		       int *otp_valid)
{
	uint8_t subcmd = 0;
	int ret;

	ret = switchtec_mfg_cmd(dev, MRPC_SECURITY_CONFIG_GET_EXT,
				&subcmd, sizeof(subcmd),
				cfgs, sizeof(struct get_cfgs_reply));
	if (ret && ERRNO_MRPC(errno) != ERR_CMD_INVALID)
		return ret;

	if (!ret) {
		*otp_valid = true;
		return ret;
	}

	*otp_valid = false;
	ret = switchtec_mfg_cmd(dev, MRPC_SECURITY_CONFIG_GET,
				NULL, 0, cfgs,
				sizeof(struct get_cfgs_reply));

	return ret;
}

static int get_configs_gen5(struct switchtec_dev *dev,
			    struct get_cfgs_reply_gen5 *cfgs)
{
	uint32_t subcmd = 0;

	return switchtec_mfg_cmd(dev,
				 MRPC_SECURITY_CONFIG_GET_GEN5,
				 &subcmd, sizeof(subcmd),
				 cfgs, sizeof(struct get_cfgs_reply_gen5));
}

int switchtec_security_spi_avail_rate_get(struct switchtec_dev *dev,
		struct switchtec_security_spi_avail_rate *rates)
{
	int ret;
	struct get_cfgs_reply reply;
	int otp_valid;

	ret = get_configs(dev, &reply, &otp_valid);
	if (ret)
		return ret;

	rates->num_rates = 10;
	if (reply.spi_core_clk_high)
		memcpy(rates->rates, spi_clk_hi_rate_float,
		       sizeof(spi_clk_hi_rate_float));
	else
		memcpy(rates->rates, spi_clk_rate_float,
		       sizeof(spi_clk_rate_float));

	return 0;
}

static void parse_otp_settings(struct switchtec_security_cfg_otp_region *otp,
			       uint32_t flags)
{
	otp->basic_valid = !!(flags & BIT(5));
	otp->basic = !!(flags & BIT(6));
	otp->mixed_ver_valid = !!(flags & BIT(7));
	otp->mixed_ver = !!(flags & BIT(8));
	otp->main_fw_ver_valid = !!(flags & BIT(9));
	otp->main_fw_ver = !!(flags & BIT(10));
	otp->sec_unlock_ver_valid = !!(flags & BIT(11));
	otp->sec_unlock_ver = !!(flags & BIT(12));
	otp->kmsk_valid[0] = !!(flags & BIT(13));
	otp->kmsk[0] = !!(flags & BIT(14));
	otp->kmsk_valid[1] = !!(flags & BIT(15));
	otp->kmsk[1] = !!(flags & BIT(16));
	otp->kmsk_valid[2] = !!(flags & BIT(17));
	otp->kmsk[2] = !!(flags & BIT(18));
	otp->kmsk_valid[3] = !!(flags & BIT(19));
	otp->kmsk[3] = !!(flags & BIT(20));
}

static void parse_otp_settings_gen5(
	struct switchtec_security_cfg_otp_region_ext *otp,
	uint32_t flags0, uint32_t flags1)
{
	otp->basic_valid = !!(flags0 & BIT(8));
	otp->basic = !!(flags0 & BIT(9));
	otp->debug_mode_valid = !!(flags0 & BIT(10));
	otp->debug_mode = !!(flags0 & BIT(11));
	otp->key_ver_valid = !!(flags0 & BIT(12));
	otp->key_ver = !!(flags0 & BIT(13));
	otp->rc_ver_valid = !!(flags0 & BIT(14));
	otp->rc_ver = !!(flags0 & BIT(15));
	otp->bl2_ver_valid = !!(flags0 & BIT(16));
	otp->bl2_ver = !!(flags0 & BIT(17));
	otp->main_fw_ver_valid = !!(flags0 & BIT(18));
	otp->main_fw_ver = !!(flags0 & BIT(19));
	otp->sec_unlock_ver_valid = !!(flags0 & BIT(20));
	otp->sec_unlock_ver = !!(flags0 & BIT(21));
	otp->kmsk_valid[0] = !!(flags0 & BIT(22));
	otp->kmsk[0] = !!(flags0 & BIT(23));
	otp->kmsk_valid[1] = !!(flags0 & BIT(24));
	otp->kmsk[1] = !!(flags0 & BIT(25));
	otp->kmsk_valid[2] = !!(flags0 & BIT(26));
	otp->kmsk[2] = !!(flags0 & BIT(27));
	otp->kmsk_valid[3] = !!(flags0 & BIT(28));
	otp->kmsk[3] = !!(flags0 & BIT(29));
	otp->kmsk_valid[4] = !!(flags0 & BIT(30));
	otp->kmsk[4] = !!(flags0 & BIT(31));
	otp->kmsk_valid[5] = !!(flags1 & BIT(0));
	otp->kmsk[5] = !!(flags1 & BIT(1));
	otp->kmsk_valid[6] = !!(flags1 & BIT(2));
	otp->kmsk[6] = !!(flags1 & BIT(3));
	otp->kmsk_valid[7] = !!(flags1 & BIT(4));
	otp->kmsk[7] = !!(flags1 & BIT(5));
	otp->kmsk_valid[8] = !!(flags1 & BIT(6));
	otp->kmsk[8] = !!(flags1 & BIT(7));
	otp->kmsk_valid[9] = !!(flags1 & BIT(8));
	otp->kmsk[9] = !!(flags1 & BIT(9));
	otp->cdi_efuse_inc_mask_valid =  !!(flags1 & BIT(10));
	otp->cdi_efuse_inc_mask =  !!(flags1 & BIT(11));
	otp->uds_valid = !!(flags1 & BIT(12));
	otp->uds = !!(flags1 & BIT(13));
	otp->uds_mask_valid = !!(flags1 & BIT(14));
	otp->uds_mask = !!(flags1 & BIT(15));
	otp->mchp_uds_valid = !!(flags1 & BIT(16));
	otp->mchp_uds = !!(flags1 & BIT(17));
	otp->mchp_uds_mask_valid = !!(flags1 & BIT(18));
	otp->mchp_uds_mask = !!(flags1 & BIT(19));
	otp->did_cert0_valid = !!(flags1 & BIT(20));
	otp->did_cert0 = !!(flags1 & BIT(21));
	otp->did_cert1_valid = !!(flags1 & BIT(22));
	otp->did_cert1 = !!(flags1 & BIT(23));
}

static int security_config_get(struct switchtec_dev *dev,
			       struct switchtec_security_cfg_state *state)
{
	int ret;
	uint32_t addr_shift;
	uint32_t map_shift;
	uint32_t map_mask;
	int spi_clk;
	struct get_cfgs_reply reply;
	int otp_valid;

	ret = get_configs(dev, &reply, &otp_valid);
	if (ret)
		return ret;

	reply.valid = le32toh(reply.valid);
	reply.cfg = le64toh(reply.cfg);
	reply.public_key_exponent = le32toh(reply.public_key_exponent);

	state->basic_setting_valid = !!(reply.valid & 0x01);
	state->public_key_exp_valid = !!(reply.valid & 0x02);
	state->public_key_num_valid = !!(reply.valid & 0x04);
	state->public_key_ver_valid = !!(reply.valid & 0x08);
	state->public_key_valid = !!(reply.valid & 0x10);

	state->debug_mode_valid = state->basic_setting_valid;

	state->otp_valid = otp_valid;
	if (otp_valid)
		parse_otp_settings(&state->otp, reply.valid);

	state->use_otp_ext = false;

	state->debug_mode = reply.cfg & 0x03;
	state->secure_state = (reply.cfg>>2) & 0x03;

	state->jtag_lock_after_reset = !!(reply.cfg & 0x40);
	state->jtag_lock_after_bl1 = !!(reply.cfg & 0x80);
	state->jtag_bl1_unlock_allowed = !!(reply.cfg & 0x0100);
	state->jtag_post_bl1_unlock_allowed = !!(reply.cfg & 0x0200);

	spi_clk = (reply.cfg >> SWITCHTEC_CLK_RATE_BITSHIFT) & 0x0f;
	if (spi_clk == 0) {
		if (switchtec_gen(dev) == SWITCHTEC_GEN5)
			spi_clk = 9;
		else
			spi_clk = 7;
	}

	if (reply.spi_core_clk_high)
		state->spi_clk_rate = spi_clk_hi_rate_float[spi_clk - 1];
	else
		state->spi_clk_rate = spi_clk_rate_float[spi_clk - 1];

	state->i2c_recovery_tmo =
		(reply.cfg >> SWITCHTEC_RC_TMO_BITSHIFT) & 0x0f;
	state->i2c_port = (reply.cfg >> SWITCHTEC_I2C_PORT_BITSHIFT) & 0xf;

	get_i2c_operands(switchtec_gen(dev), &addr_shift, &map_shift,
			 &map_mask);
	state->i2c_addr =
		(reply.cfg >> addr_shift) & SWITCHTEC_I2C_ADDR_BITMASK;
	state->i2c_cmd_map = (reply.cfg >> map_shift) & map_mask;

	state->public_key_exponent = reply.public_key_exponent;
	state->public_key_num = reply.public_key_num;
	state->public_key_ver = reply.public_key_ver;

	if (state->public_key_num)
		memcpy(state->public_key, reply.public_key,
		       state->public_key_num * SWITCHTEC_KMSK_LEN);

	state->attn_state.attestation_mode =
		SWITCHTEC_ATTESTATION_MODE_NOT_SUPPORTED;

	return 0;
}

static int security_config_get_gen5(struct switchtec_dev *dev,
				    struct switchtec_security_cfg_state *state)
{
	int ret;
	uint32_t addr_shift;
	uint32_t map_shift;
	uint32_t map_mask;
	int spi_clk;
	struct get_cfgs_reply_gen5 reply;
	int attn_mode;

	ret = get_configs_gen5(dev, &reply);
	if (ret)
		return ret;

	reply.valid0 = le32toh(reply.valid0);
	reply.valid1 = le32toh(reply.valid1);

	reply.cfg = le64toh(reply.cfg);
	reply.public_key_exponent = le32toh(reply.public_key_exponent);

	state->basic_setting_valid = !!(reply.valid0 & 0x01);
	state->public_key_exp_valid = !!(reply.valid0 & 0x04);
	state->public_key_num_valid = !!(reply.valid0 & 0x08);
	state->public_key_ver_valid = !!(reply.valid0 & 0x10);
	state->public_key_valid = !!(reply.valid0 & 0x20);

	state->debug_mode_valid = !!(reply.valid0 & 0x02);
	state->attn_state.cdi_efuse_inc_mask_valid = !!(reply.valid0 & 0x40);

	state->otp_valid = true;
	parse_otp_settings_gen5(&state->otp_ext, reply.valid0,
				reply.valid1);

	state->use_otp_ext = true;

	state->debug_mode = reply.cfg & 0x03;
	state->secure_state = (reply.cfg>>2) & 0x03;

	state->jtag_lock_after_reset = !!(reply.cfg & 0x40);
	state->jtag_lock_after_bl1 = !!(reply.cfg & 0x80);
	state->jtag_bl1_unlock_allowed = !!(reply.cfg & 0x0100);
	state->jtag_post_bl1_unlock_allowed = !!(reply.cfg & 0x0200);

	spi_clk = (reply.cfg >> SWITCHTEC_CLK_RATE_BITSHIFT) & 0x0f;
	if (spi_clk == 0) {
		if (switchtec_gen(dev) == SWITCHTEC_GEN5)
			spi_clk = 9;
		else
			spi_clk = 7;
	}

	if (reply.spi_core_clk_high)
		state->spi_clk_rate = spi_clk_hi_rate_float[spi_clk - 1];
	else
		state->spi_clk_rate = spi_clk_rate_float[spi_clk - 1];

	state->i2c_recovery_tmo =
		(reply.cfg >> SWITCHTEC_RC_TMO_BITSHIFT) & 0x0f;
	state->i2c_port = (reply.cfg >> SWITCHTEC_I2C_PORT_BITSHIFT) & 0xf;

	get_i2c_operands(switchtec_gen(dev), &addr_shift, &map_shift,
			 &map_mask);
	state->i2c_addr =
		(reply.cfg >> addr_shift) & SWITCHTEC_I2C_ADDR_BITMASK;
	state->i2c_cmd_map = (reply.cfg >> map_shift) & map_mask;

	state->public_key_exponent = reply.public_key_exponent;
	state->public_key_num = reply.public_key_num;
	state->public_key_ver = reply.public_key_ver;
	memcpy(state->public_key, reply.public_key,
	       state->public_key_num * SWITCHTEC_KMSK_LEN);

	attn_mode = (reply.cfg >> SWITCHTEC_ATTEST_BITSHIFT) &
		SWITCHTEC_ATTEST_BITMASK;
	if (attn_mode == 1)
		state->attn_state.attestation_mode =
			SWITCHTEC_ATTESTATION_MODE_DICE;
	else
		state->attn_state.attestation_mode =
			SWITCHTEC_ATTESTATION_MODE_NONE;

	state->attn_state.uds_selfgen =
		(reply.cfg >> SWITCHTEC_UDS_SELFGEN_BITSHIFT) &
		SWITCHTEC_UDS_SELFGEN_BITMASK;
	state->attn_state.cdi_efuse_inc_mask =
		le32toh(reply.cdi_efuse_inc_mask);

	if (state->secure_state == SWITCHTEC_UNINITIALIZED_UNSECURED &&
	    state->attn_state.attestation_mode ==
		SWITCHTEC_ATTESTATION_MODE_DICE &&
	    !state->attn_state.uds_selfgen)
		state->attn_state.uds_visible = true;
	else
		state->attn_state.uds_visible = false;

	if (state->attn_state.uds_visible)
		memcpy(state->attn_state.uds_data, reply.uds_data, 32);

	return 0;
}

/**
 * @brief Get secure boot configurations
 * @param[in]  dev	Switchtec device handle
 * @param[out] state	Current secure boot settings
 * @return 0 on success, error code on failure
 */
int switchtec_security_config_get(struct switchtec_dev *dev,
				  struct switchtec_security_cfg_state *state)
{
	if (switchtec_is_gen5(dev))
		return security_config_get_gen5(dev, state);
	else
		return security_config_get(dev, state);
}

static int mailbox_to_file(struct switchtec_dev *dev, int fd)
{
	int ret;
	int num_to_read = htole32(SWITCHTEC_MB_MAX_ENTRIES);
	struct mb_reply {
		uint8_t num_returned;
		uint8_t num_remaining;
		uint8_t rsvd[2];
		uint8_t data[SWITCHTEC_MB_MAX_ENTRIES *
			     SWITCHTEC_MB_LOG_LEN];
	} reply;

	do {
		ret = switchtec_mfg_cmd(dev, MRPC_MAILBOX_GET, &num_to_read,
					sizeof(int), &reply,  sizeof(reply));
		if (ret)
			return ret;

		reply.num_remaining = le32toh(reply.num_remaining);
		reply.num_returned = le32toh(reply.num_returned);

		ret = write(fd, reply.data,
			    (reply.num_returned) * SWITCHTEC_MB_LOG_LEN);
		if (ret < 0)
			return ret;
	} while (reply.num_remaining > 0);

	return 0;
}

static int mailbox_to_file_gen5(struct switchtec_dev *dev, int fd)
{
	int ret;
	struct mb_read {
		uint32_t subcmd;
		uint32_t num_to_read;
	} read;
	struct mb_reply {
		uint8_t num_returned;
		uint8_t num_remaining;
		uint8_t rsvd[2];
		uint8_t data[SWITCHTEC_MB_MAX_ENTRIES *
			     SWITCHTEC_MB_LOG_LEN];
	} reply;

	read.subcmd = 0;
	read.num_to_read = htole32(SWITCHTEC_MB_MAX_ENTRIES);

	do {
		ret = switchtec_mfg_cmd(dev, MRPC_MAILBOX_GET_GEN5,
					&read, sizeof(read),
					&reply,  sizeof(reply));
		if (ret)
			return ret;

		reply.num_remaining = le32toh(reply.num_remaining);
		reply.num_returned = le32toh(reply.num_returned);

		ret = write(fd, reply.data,
			    (reply.num_returned) * SWITCHTEC_MB_LOG_LEN);
		if (ret < 0)
			return ret;
	} while (reply.num_remaining > 0);

	return 0;
}

/**
 * @brief Retrieve mailbox entries
 * @param[in]  dev	Switchtec device handle
 * @param[in]  fd	File handle to write the log data
 * @return 0 on success, error code on failure
 */
int switchtec_mailbox_to_file(struct switchtec_dev *dev, int fd)
{
	if (switchtec_is_gen5(dev))
		return mailbox_to_file_gen5(dev, fd);
	else
		return mailbox_to_file(dev, fd);
}

static int convert_spi_clk_rate(float clk_float, int hi_rate)
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

static int security_config_set_gen4(struct switchtec_dev *dev,
				    struct switchtec_security_cfg_set *setting)
{
	int ret;
	struct setting_data {
		uint64_t cfg;
		uint32_t pub_key_exponent;
		uint8_t rsvd[4];
	} sd;
	struct get_cfgs_reply reply;
	uint64_t ldata = 0;
	uint32_t addr_shift;
	uint32_t map_shift;
	uint32_t map_mask;
	int spi_clk;
	int otp_valid;

	/* Gen4 device does not support attestation feature */
	if (setting->attn_set.attestation_mode !=
	    SWITCHTEC_ATTESTATION_MODE_NOT_SUPPORTED)
		return -EINVAL;

	ret = get_configs(dev, &reply, &otp_valid);
	if (ret)
		return ret;

	memset(&sd, 0, sizeof(sd));

	sd.cfg |= setting->jtag_lock_after_reset?
			SWITCHTEC_JTAG_LOCK_AFT_RST_BITMASK : 0;
	sd.cfg |= setting->jtag_lock_after_bl1?
			SWITCHTEC_JTAG_LOCK_AFT_BL1_BITMASK : 0;
	sd.cfg |= setting->jtag_bl1_unlock_allowed?
			SWITCHTEC_JTAG_UNLOCK_BL1_BITMASK : 0;
	sd.cfg |= setting->jtag_post_bl1_unlock_allowed?
			SWITCHTEC_JTAG_UNLOCK_AFT_BL1_BITMASK : 0;

	spi_clk = convert_spi_clk_rate(setting->spi_clk_rate,
				       reply.spi_core_clk_high);
	if (spi_clk < 0) {
		errno = EINVAL;
		return -1;
	}

	sd.cfg |= (spi_clk & SWITCHTEC_CLK_RATE_BITMASK) <<
			SWITCHTEC_CLK_RATE_BITSHIFT;

	sd.cfg |= (setting->i2c_recovery_tmo & SWITCHTEC_RC_TMO_BITMASK) <<
			SWITCHTEC_RC_TMO_BITSHIFT;
	sd.cfg |= (setting->i2c_port & SWITCHTEC_I2C_PORT_BITMASK) <<
			SWITCHTEC_I2C_PORT_BITSHIFT;

	get_i2c_operands(switchtec_gen(dev), &addr_shift, &map_shift,
			 &map_mask);
	sd.cfg |= (setting->i2c_addr & SWITCHTEC_I2C_ADDR_BITMASK) <<
			addr_shift;

	ldata = setting->i2c_cmd_map & map_mask;
	ldata <<= map_shift;
	sd.cfg |= ldata;

	sd.cfg = htole64(sd.cfg);

	sd.pub_key_exponent = htole32(setting->public_key_exponent);

	return switchtec_mfg_cmd(dev, MRPC_SECURITY_CONFIG_SET,
				 &sd, sizeof(sd), NULL, 0);
}

static int security_config_set_gen5(struct switchtec_dev *dev,
				    struct switchtec_security_cfg_set *setting)
{
	int ret;
	struct setting_data {
		uint64_t cfg;
		uint32_t pub_key_exponent;
		uint8_t uds_valid;
		uint8_t rsvd[3];
		uint32_t cdi_efuse_inc_mask;
		uint8_t uds[32];
	} sd;
	struct get_cfgs_reply_gen5 reply;
	uint64_t ldata = 0;
	uint32_t addr_shift;
	uint32_t map_shift;
	uint32_t map_mask;
	int spi_clk;
	uint8_t cmd_buf[64]={};

	ret = get_configs_gen5(dev, &reply);
	if (ret)
		return ret;

	memset(&sd, 0, sizeof(sd));

	sd.cfg = setting->jtag_lock_after_reset?
			SWITCHTEC_JTAG_LOCK_AFT_RST_BITMASK : 0;
	sd.cfg |= setting->jtag_lock_after_bl1?
			SWITCHTEC_JTAG_LOCK_AFT_BL1_BITMASK : 0;
	sd.cfg |= setting->jtag_bl1_unlock_allowed?
			SWITCHTEC_JTAG_UNLOCK_BL1_BITMASK : 0;
	sd.cfg |= setting->jtag_post_bl1_unlock_allowed?
			SWITCHTEC_JTAG_UNLOCK_AFT_BL1_BITMASK : 0;

	spi_clk = convert_spi_clk_rate(setting->spi_clk_rate,
				       reply.spi_core_clk_high);
	if (spi_clk < 0) {
		errno = EINVAL;
		return -1;
	}

	sd.cfg |= (spi_clk & SWITCHTEC_CLK_RATE_BITMASK) <<
			SWITCHTEC_CLK_RATE_BITSHIFT;

	sd.cfg |= (setting->i2c_recovery_tmo & SWITCHTEC_RC_TMO_BITMASK) <<
			SWITCHTEC_RC_TMO_BITSHIFT;
	sd.cfg |= (setting->i2c_port & SWITCHTEC_I2C_PORT_BITMASK) <<
			SWITCHTEC_I2C_PORT_BITSHIFT;

	get_i2c_operands(switchtec_gen(dev), &addr_shift, &map_shift,
			 &map_mask);
	sd.cfg |= (setting->i2c_addr & SWITCHTEC_I2C_ADDR_BITMASK) <<
			addr_shift;

	ldata = setting->i2c_cmd_map & map_mask;
	ldata <<= map_shift;
	sd.cfg |= ldata;

	sd.cfg = htole64(sd.cfg);

	sd.pub_key_exponent = htole32(setting->public_key_exponent);

	if (setting->attn_set.attestation_mode ==
	    SWITCHTEC_ATTESTATION_MODE_DICE) {
		sd.cfg |= 0x10;
		sd.cdi_efuse_inc_mask = setting->attn_set.cdi_efuse_inc_mask;

		ldata = setting->attn_set.uds_selfgen? 1 : 0;
		ldata <<= 44;
		sd.cfg |= ldata;

		sd.uds_valid = setting->attn_set.uds_valid;
		if (sd.uds_valid)
			memcpy(sd.uds, setting->attn_set.uds_data, 32);
	}

	memcpy(cmd_buf + 4, &sd, sizeof(sd));
	return switchtec_mfg_cmd(dev, MRPC_SECURITY_CONFIG_SET_GEN5,
				 cmd_buf, sizeof(cmd_buf), NULL, 0);
}

/**
 * @brief Set secure settings
 * @param[in]  dev	Switchtec device handle
 * @param[out] setting	Secure boot settings
 * @return 0 on success, error code on failure
 */
int switchtec_security_config_set(struct switchtec_dev *dev,
				  struct switchtec_security_cfg_set *setting)
{
	if (switchtec_is_gen5(dev))
		return security_config_set_gen5(dev, setting);
	else
		return security_config_set_gen4(dev, setting);
}

static int active_image_index_get(struct switchtec_dev *dev,
				  struct switchtec_active_index *index)
{
	int ret;
	struct active_indices {
		uint8_t index[SWITCHTEC_ACTV_IDX_MAX_ENTRIES];
	} reply;

	ret = switchtec_mfg_cmd(dev, MRPC_ACT_IMG_IDX_GET, NULL,
				0, &reply, sizeof(reply));
	if (ret)
		return ret;

	index->keyman = reply.index[SWITCHTEC_ACTV_IMG_ID_KMAN];
	index->bl2 = reply.index[SWITCHTEC_ACTV_IMG_ID_BL2];
	index->config = reply.index[SWITCHTEC_ACTV_IMG_ID_CFG];
	index->firmware = reply.index[SWITCHTEC_ACTV_IMG_ID_FW];
	index->riot = SWITCHTEC_ACTIVE_INDEX_NOT_SET;

	return 0;
}

static int active_image_index_get_gen5(struct switchtec_dev *dev,
				       struct switchtec_active_index *index)
{
	int ret;
	uint32_t subcmd = 0;
	struct active_indices {
		uint8_t index[SWITCHTEC_ACTV_IDX_MAX_ENTRIES];
	} reply;

	ret = switchtec_mfg_cmd(dev, MRPC_ACT_IMG_IDX_GET_GEN5, &subcmd,
				sizeof(subcmd), &reply, sizeof(reply));
	if (ret)
		return ret;

	index->keyman = reply.index[SWITCHTEC_ACTV_IMG_ID_KMAN_GEN5];
	index->bl2 = reply.index[SWITCHTEC_ACTV_IMG_ID_BL2_GEN5];
	index->config = reply.index[SWITCHTEC_ACTV_IMG_ID_CFG_GEN5];
	index->firmware = reply.index[SWITCHTEC_ACTV_IMG_ID_FW_GEN5];
	index->riot = reply.index[SWITCHTEC_ACTV_IMG_ID_RC_GEN5];

	return 0;
}

/**
 * @brief Get active image index
 * @param[in]  dev	Switchtec device handle
 * @param[out] index	Active images indices
 * @return 0 on success, error code on failure
 */
int switchtec_active_image_index_get(struct switchtec_dev *dev,
				     struct switchtec_active_index *index)
{
	if (switchtec_is_gen5(dev))
		return active_image_index_get_gen5(dev, index);
	else
		return active_image_index_get(dev, index);
}

static int active_image_index_set(struct switchtec_dev *dev,
				  struct switchtec_active_index *index)
{
	int ret;
	int i = 0;
	struct active_idx {
		uint32_t count;
		struct entry {
			uint8_t image_id;
			uint8_t index;
		} idx[SWITCHTEC_ACTV_IDX_SET_ENTRIES];
	} set;

	/* RIOT image is not available on Gen4 device */
	if (index->riot != SWITCHTEC_ACTIVE_INDEX_NOT_SET) {
		errno = EINVAL;
		return -EINVAL;
	}

	memset(&set, 0, sizeof(set));

	if (index->keyman != SWITCHTEC_ACTIVE_INDEX_NOT_SET) {
		set.idx[i].image_id = SWITCHTEC_ACTV_IMG_ID_KMAN;
		set.idx[i].index = index->keyman;
		i++;
	}

	if (index->bl2 != SWITCHTEC_ACTIVE_INDEX_NOT_SET) {
		set.idx[i].image_id = SWITCHTEC_ACTV_IMG_ID_BL2;
		set.idx[i].index = index->bl2;
		i++;
	}

	if (index->config != SWITCHTEC_ACTIVE_INDEX_NOT_SET) {
		set.idx[i].image_id =  SWITCHTEC_ACTV_IMG_ID_CFG;
		set.idx[i].index = index->config;
		i++;
	}

	if (index->firmware != SWITCHTEC_ACTIVE_INDEX_NOT_SET) {
		set.idx[i].image_id = SWITCHTEC_ACTV_IMG_ID_FW;
		set.idx[i].index = index->firmware;
		i++;
	}

	if (i == 0)
		return 0;

	set.count = htole32(i);

	ret = switchtec_mfg_cmd(dev, MRPC_ACT_IMG_IDX_SET, &set,
				sizeof(set), NULL, 0);
	return ret;
}

static int active_image_index_set_gen5(struct switchtec_dev *dev,
				       struct switchtec_active_index *index)
{
	int ret;
	int i = 0;
	struct active_idx {
		uint32_t subcmd;
		uint32_t count;
		struct entry {
			uint8_t image_id;
			uint8_t index;
		} idx[SWITCHTEC_ACTV_IDX_SET_ENTRIES];
	} set = {};

	if (index->keyman != SWITCHTEC_ACTIVE_INDEX_NOT_SET) {
		set.idx[i].image_id = SWITCHTEC_ACTV_IMG_ID_KMAN_GEN5;
		set.idx[i].index = index->keyman;
		i++;
	}

	if (index->riot != SWITCHTEC_ACTIVE_INDEX_NOT_SET) {
		set.idx[i].image_id = SWITCHTEC_ACTV_IMG_ID_RC_GEN5;
		set.idx[i].index = index->riot;
		i++;
	}

	if (index->bl2 != SWITCHTEC_ACTIVE_INDEX_NOT_SET) {
		set.idx[i].image_id = SWITCHTEC_ACTV_IMG_ID_BL2_GEN5;
		set.idx[i].index = index->bl2;
		i++;
	}

	if (index->config != SWITCHTEC_ACTIVE_INDEX_NOT_SET) {
		set.idx[i].image_id =  SWITCHTEC_ACTV_IMG_ID_CFG_GEN5;
		set.idx[i].index = index->config;
		i++;
	}

	if (index->firmware != SWITCHTEC_ACTIVE_INDEX_NOT_SET) {
		set.idx[i].image_id = SWITCHTEC_ACTV_IMG_ID_FW_GEN5;
		set.idx[i].index = index->firmware;
		i++;
	}

	if (i == 0)
		return 0;

	set.count = htole32(i);

	ret = switchtec_mfg_cmd(dev, MRPC_ACT_IMG_IDX_SET_GEN5, &set,
				sizeof(set), NULL, 0);
	return ret;
}

/**
 * @brief Set active image index
 * @param[in]  dev	Switchtec device handle
 * @param[in] index	Active image indices
 * @return 0 on success, error code on failure
 */
int switchtec_active_image_index_set(struct switchtec_dev *dev,
				     struct switchtec_active_index *index)
{
	if (switchtec_is_gen5(dev))
		return active_image_index_set_gen5(dev, index);
	else
		return active_image_index_set(dev, index);
}

/**
 * @brief Execute the transferred firmware
 * @param[in]  dev		Switchtec device handle
 * @param[in]  recovery_mode	Recovery mode in case of a boot failure
 * @return 0 on success, error code on failure
 */
int switchtec_fw_exec(struct switchtec_dev *dev,
		      enum switchtec_bl2_recovery_mode recovery_mode)
{
	uint32_t cmd_id = MRPC_FW_TX;
	struct fw_exec_struct {
		uint8_t subcmd;
		uint8_t recovery_mode;
		uint8_t rsvd[2];
	} cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.subcmd = MRPC_FW_TX_EXEC;
	cmd.recovery_mode = recovery_mode;

	if (switchtec_is_gen5(dev))
		cmd_id = MRPC_FW_TX_GEN5;

	return switchtec_mfg_cmd(dev, cmd_id, &cmd, sizeof(cmd), NULL, 0);
}

/**
 * @brief Resume device boot.
 *	  Note that after calling this function, the current
 *	  'dev' pointer is no longer valid. Before making further
 *	  calls to switchtec library functions, be sure to close
 *	  this pointer and get a new one by calling switchtec_open().
 *	  Also be sure to check the return value of switchtec_open()
 *	  for error, as the device might not be immediately
 *	  accessible after normal boot process.
 * @param[in]  dev	Switchtec device handle
 * @return 0 on success, error code on failure
 */
int switchtec_boot_resume(struct switchtec_dev *dev)
{
	uint32_t subcmd = 0;

	if (switchtec_is_gen5(dev))
		return switchtec_mfg_cmd(dev, MRPC_BOOTUP_RESUME_GEN5,
					 &subcmd, sizeof(subcmd),
					 NULL, 0);
	else
		return switchtec_mfg_cmd(dev, MRPC_BOOTUP_RESUME,
					 NULL, 0, NULL, 0);
}

static int secure_state_set(struct switchtec_dev *dev,
			    enum switchtec_secure_state state)
{
	uint32_t data;

	data = htole32(state);

	return switchtec_mfg_cmd(dev, MRPC_SECURE_STATE_SET,
				 &data, sizeof(data), NULL, 0);
}

static int secure_state_set_gen5(struct switchtec_dev *dev,
				 enum switchtec_secure_state state)
{
	struct state_set {
		uint32_t subcmd;
		uint32_t state;
	} data;

	data.subcmd = 0;
	data.state = htole32(state);

	return switchtec_mfg_cmd(dev, MRPC_SECURE_STATE_SET_GEN5,
				 &data, sizeof(data), NULL, 0);
}

/**
 * @brief Set device secure state
 * @param[in]  dev	Switchtec device handle
 * @param[in]  state	Secure state
 * @return 0 on success, error code on failure
 */
int switchtec_secure_state_set(struct switchtec_dev *dev,
			       enum switchtec_secure_state state)
{
	if ((state != SWITCHTEC_INITIALIZED_UNSECURED)
	   && (state != SWITCHTEC_INITIALIZED_SECURED)) {
		return ERR_PARAM_INVALID;
	}

	if (switchtec_is_gen5(dev))
		return secure_state_set_gen5(dev, state);
	else
		return secure_state_set(dev, state);
}

static int dbg_unlock_send_pubkey(struct switchtec_dev *dev,
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

/**
 * @brief Unlock firmware debug features
 * @param[in]  dev		Switchtec device handle
 * @param[in]  serial		Device serial number
 * @param[in]  ver_sec_unlock	Secure unlock version
 * @param[in]  public_key	public key data
 * @param[in]  signature	Signature of data sent
 * @return 0 on success, error code on failure
 */
int switchtec_dbg_unlock(struct switchtec_dev *dev, uint32_t serial,
			 uint32_t ver_sec_unlock,
			 struct switchtec_pubkey *public_key,
			 struct switchtec_signature *signature)
{
	int ret;
	struct unlock_cmd {
		uint8_t subcmd;
		uint8_t rsvd[3];
		uint32_t serial;
		uint32_t unlock_ver;
		uint8_t signature[SWITCHTEC_SIG_LEN];
	} cmd = {};
	uint32_t cmd_id;

	if (switchtec_is_gen5(dev))
		cmd_id = MRPC_DBG_UNLOCK_GEN5;
	else
		cmd_id = MRPC_DBG_UNLOCK;

	ret = dbg_unlock_send_pubkey(dev, public_key, cmd_id);
	if (ret)
		return ret;

	cmd.subcmd = MRPC_DBG_UNLOCK_DATA;
	cmd.serial = htole32(serial);
	cmd.unlock_ver = htole32(ver_sec_unlock);
	memcpy(cmd.signature, signature->signature, SWITCHTEC_SIG_LEN);

	return switchtec_mfg_cmd(dev, cmd_id, &cmd, sizeof(cmd), NULL, 0);
}

/**
 * @brief Update firmware debug secure unlock version number
 * @param[in]  dev		Switchtec device handle
 * @param[in]  serial		Device serial number
 * @param[in]  ver_sec_unlock	New secure unlock version
 * @param[in]  public_key	public key data
 * @param[in]  signature	Signature of data sent
 * @return 0 on success, error code on failure
 */
int switchtec_dbg_unlock_version_update(struct switchtec_dev *dev,
					uint32_t serial,
					uint32_t ver_sec_unlock,
					struct switchtec_pubkey *public_key,
			 		struct switchtec_signature *signature)
{
	int ret;
	struct update_cmd {
		uint8_t subcmd;
		uint8_t rsvd[3];
		uint32_t serial;
		uint32_t unlock_ver;
		uint8_t signature[SWITCHTEC_SIG_LEN];
	} cmd = {};
	uint32_t cmd_id;

	if (switchtec_is_gen5(dev))
		cmd_id = MRPC_DBG_UNLOCK_GEN5;
	else
		cmd_id = MRPC_DBG_UNLOCK;

	ret = dbg_unlock_send_pubkey(dev, public_key, cmd_id);
	if (ret)
		return ret;

	cmd.subcmd = MRPC_DBG_UNLOCK_UPDATE;
	cmd.serial = htole32(serial);
	cmd.unlock_ver = htole32(ver_sec_unlock);
	memcpy(cmd.signature, signature->signature, SWITCHTEC_SIG_LEN);

	return switchtec_mfg_cmd(dev, cmd_id, &cmd, sizeof(cmd), NULL, 0);
}

static int check_sec_cfg_header(struct switchtec_dev *dev,
				FILE *setting_file)
{
	ssize_t rlen;
	enum switchtec_gen gen;
	char magic[4] = {'S', 'S', 'F', 'F'};
	uint32_t crc;
	struct setting_file_header {
		uint8_t magic[4];
		uint32_t version;
		uint8_t hw_gen;
		uint8_t rsvd[3];
		uint32_t crc;
	} hdr;
	int data_len;
	uint8_t data[64];

	rlen = fread(&hdr, sizeof(hdr), 1, setting_file);

	if (rlen != 1)
		return -EBADF;

	if (memcmp(hdr.magic, magic, sizeof(magic)))
		return -EBADF;

	switch (hdr.hw_gen) {
	case 0:
		gen = SWITCHTEC_GEN4;
		break;
	case 1:
		gen = SWITCHTEC_GEN5;
		break;
	default:
		return -EBADF;
	}

	if (gen != switchtec_gen(dev))
		return -ENODEV;

	fseek(setting_file, 0, SEEK_END);
	data_len = ftell(setting_file) - sizeof(hdr);
	fseek(setting_file, sizeof(hdr), SEEK_SET);

	rlen = fread(data, 1, data_len, setting_file);
	if (rlen < data_len)
		return -EBADF;

	crc = crc32(data, data_len, 0, 1, 1);
	if (crc != le32toh(hdr.crc))
		return -EBADF;

	fseek(setting_file, sizeof(hdr), SEEK_SET);
	return 0;
}

static int read_sec_cfg_file(struct switchtec_dev *dev,
			     FILE *setting_file,
			     struct switchtec_security_cfg_set *set)
{
	struct setting_file_data {
		uint64_t cfg;
		uint32_t pub_key_exponent;
		uint8_t rsvd[36];
	} data;
	struct get_cfgs_reply reply;
	uint32_t addr_shift;
	uint32_t map_shift;
	uint32_t map_mask;
	int spi_clk;
	int ret;
	int otp_valid;

	ret = get_configs(dev, &reply, &otp_valid);
	if (ret)
		return ret;

	memset(set, 0, sizeof(struct switchtec_security_cfg_set));

	ret = fread(&data, sizeof(data), 1, setting_file);

	if (ret != 1)
		return -EBADF;

	data.cfg = le64toh(data.cfg);

	set->jtag_lock_after_reset =
		!!(data.cfg & SWITCHTEC_JTAG_LOCK_AFT_RST_BITMASK);
	set->jtag_lock_after_bl1 =
		!!(data.cfg & SWITCHTEC_JTAG_LOCK_AFT_BL1_BITMASK);
	set->jtag_bl1_unlock_allowed =
		!!(data.cfg & SWITCHTEC_JTAG_UNLOCK_BL1_BITMASK);
	set->jtag_post_bl1_unlock_allowed =
		!!(data.cfg & SWITCHTEC_JTAG_UNLOCK_AFT_BL1_BITMASK);

	spi_clk = (data.cfg >> SWITCHTEC_CLK_RATE_BITSHIFT) &
		SWITCHTEC_CLK_RATE_BITMASK;

	if (spi_clk == 0)
		spi_clk = 7;

	if (spi_clk > 10)
		return -EINVAL;

	if (reply.spi_core_clk_high)
		set->spi_clk_rate = spi_clk_hi_rate_float[spi_clk - 1];
	else
		set->spi_clk_rate = spi_clk_rate_float[spi_clk - 1];

	set->i2c_recovery_tmo =
		(data.cfg >> SWITCHTEC_RC_TMO_BITSHIFT) &
		SWITCHTEC_RC_TMO_BITMASK;
	set->i2c_port =
		(data.cfg >> SWITCHTEC_I2C_PORT_BITSHIFT) &
		SWITCHTEC_I2C_PORT_BITMASK;

	get_i2c_operands(switchtec_gen(dev), &addr_shift, &map_shift,
			 &map_mask);
	set->i2c_addr =
		(data.cfg >> addr_shift) &
		SWITCHTEC_I2C_ADDR_BITMASK;
	set->i2c_cmd_map = (data.cfg >> map_shift) & map_mask;

	set->public_key_exponent = le32toh(data.pub_key_exponent);

	set->attn_set.attestation_mode =
		SWITCHTEC_ATTESTATION_MODE_NOT_SUPPORTED;

	return 0;
}

static int read_sec_cfg_file_gen5(struct switchtec_dev *dev,
				  FILE *setting_file,
				  struct switchtec_security_cfg_set *set)
{
	struct setting_data {
		uint64_t cfg;
		uint32_t pub_key_exponent;
		uint8_t rsvd[4];
		uint32_t cdi_efuse_inc_mask;
	} data;
	struct get_cfgs_reply_gen5 reply;
	uint32_t addr_shift;
	uint32_t map_shift;
	uint32_t map_mask;
	int spi_clk;
	int ret;
	int attest_mode;

	ret = get_configs_gen5(dev, &reply);
	if (ret)
		return ret;

	memset(set, 0, sizeof(struct switchtec_security_cfg_set));

	ret = fread(&data, sizeof(data), 1, setting_file);

	if (ret != 1)
		return -EBADF;

	data.cfg = le64toh(data.cfg);

	set->jtag_lock_after_reset =
		!!(data.cfg & SWITCHTEC_JTAG_LOCK_AFT_RST_BITMASK);
	set->jtag_lock_after_bl1 =
		!!(data.cfg & SWITCHTEC_JTAG_LOCK_AFT_BL1_BITMASK);
	set->jtag_bl1_unlock_allowed =
		!!(data.cfg & SWITCHTEC_JTAG_UNLOCK_BL1_BITMASK);
	set->jtag_post_bl1_unlock_allowed =
		!!(data.cfg & SWITCHTEC_JTAG_UNLOCK_AFT_BL1_BITMASK);

	spi_clk = (data.cfg >> SWITCHTEC_CLK_RATE_BITSHIFT) &
		SWITCHTEC_CLK_RATE_BITMASK;

	if (spi_clk == 0)
		spi_clk = 9;

	if (spi_clk > 10)
		return -EINVAL;

	if (reply.spi_core_clk_high)
		set->spi_clk_rate = spi_clk_hi_rate_float[spi_clk - 1];
	else
		set->spi_clk_rate = spi_clk_rate_float[spi_clk - 1];

	set->i2c_recovery_tmo =
		(data.cfg >> SWITCHTEC_RC_TMO_BITSHIFT) &
		SWITCHTEC_RC_TMO_BITMASK;
	set->i2c_port =
		(data.cfg >> SWITCHTEC_I2C_PORT_BITSHIFT) &
		SWITCHTEC_I2C_PORT_BITMASK;

	get_i2c_operands(switchtec_gen(dev), &addr_shift, &map_shift,
			 &map_mask);
	set->i2c_addr =
		(data.cfg >> addr_shift) &
		SWITCHTEC_I2C_ADDR_BITMASK;
	set->i2c_cmd_map = (data.cfg >> map_shift) & map_mask;

	set->public_key_exponent = le32toh(data.pub_key_exponent);

	attest_mode = (data.cfg >> SWITCHTEC_ATTEST_BITSHIFT) &
		SWITCHTEC_ATTEST_BITMASK;
	if (attest_mode == 1) {
		set->attn_set.attestation_mode =
			SWITCHTEC_ATTESTATION_MODE_DICE;
		set->attn_set.cdi_efuse_inc_mask = data.cdi_efuse_inc_mask;
		set->attn_set.uds_selfgen = (data.cfg >> 44) & 0x1;
	} else {
		set->attn_set.attestation_mode =
			SWITCHTEC_ATTESTATION_MODE_NONE;
	}

	return 0;
}

/**
 * @brief Read security settings from config file
 * @param[in]  dev		Switchtec device handle
 * @param[in]  setting_file	Security setting file
 * @param[out] set		Security settings
 * @return 0 on success, error code on failure
 */
int switchtec_read_sec_cfg_file(struct switchtec_dev *dev,
				FILE *setting_file,
				struct switchtec_security_cfg_set *set)
{
	int ret;

	ret = check_sec_cfg_header(dev, setting_file);
	if (ret)
		return ret;

	if (switchtec_is_gen4(dev))
		return read_sec_cfg_file(dev, setting_file, set);
	else
		return read_sec_cfg_file_gen5(dev, setting_file, set);
}

static int kmsk_set_send_pubkey(struct switchtec_dev *dev,
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

static int kmsk_set_send_signature(struct switchtec_dev *dev,
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

static int kmsk_set_send_kmsk(struct switchtec_dev *dev,
			      struct switchtec_kmsk *kmsk,
			      uint32_t cmd_id)
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

/**
 * @brief Set KMSK entry
 * 	  KMSK stands for Key Manifest Secure Key.
 * 	  It is a key used to verify Key Manifest
 * 	  partition, which contains keys to verify
 * 	  all other partitions.
 * @param[in]  dev		Switchtec device handle
 * @param[in]  public_key	Public key
 * @param[in]  signature	Signature
 * @param[in]  kmsk		KMSK entry data
 * @return 0 on success, error code on failure
 */
int switchtec_kmsk_set(struct switchtec_dev *dev,
		       struct switchtec_pubkey *public_key,
		       struct switchtec_signature *signature,
		       struct switchtec_kmsk *kmsk)
{
	int ret;
	uint32_t cmd_id;

	if (switchtec_is_gen5(dev))
		cmd_id = MRPC_KMSK_ENTRY_SET_GEN5;
	else
		cmd_id = MRPC_KMSK_ENTRY_SET;

	if (public_key) {
		ret = kmsk_set_send_pubkey(dev, public_key, cmd_id);
		if (ret)
			return ret;
	}

	if (signature) {
		ret = kmsk_set_send_signature(dev, signature, cmd_id);
		if (ret)
			return ret;
	}

	return kmsk_set_send_kmsk(dev, kmsk, cmd_id);
}

#if HAVE_LIBCRYPTO
/**
 * @brief Read public key from public key file
 * @param[in]  pubk_file Public key file
 * @param[out] pubk	 Public key
 * @return 0 on success, error code on failure
 */
int switchtec_read_pubk_file(FILE *pubk_file, struct switchtec_pubkey *pubk)
{
	RSA *RSAKey = NULL;
	const BIGNUM *modulus_bn;
	const BIGNUM *exponent_bn;
	uint32_t exponent_tmp = 0;

	RSAKey = PEM_read_RSA_PUBKEY(pubk_file, NULL, NULL, NULL);
	if (RSAKey == NULL) {
		fseek(pubk_file, 0L, SEEK_SET);
		RSAKey = PEM_read_RSAPrivateKey(pubk_file, NULL, NULL, NULL);
		if (RSAKey == NULL)
			return -1;
	}

	RSA_get0_key(RSAKey, &modulus_bn, &exponent_bn, NULL);

	BN_bn2bin(modulus_bn, pubk->pubkey);
	BN_bn2bin(exponent_bn, (uint8_t *)&exponent_tmp);

	pubk->pubkey_exp = be32toh(exponent_tmp);
	RSA_free(RSAKey);

	return 0;
}
#endif

/**
 * @brief Read KMSK data from KMSK file
 * @param[in]  kmsk_file KMSK file
 * @param[out] kmsk   	 KMSK entry data
 * @return 0 on success, error code on failure
 */
int switchtec_read_kmsk_file(FILE *kmsk_file, struct switchtec_kmsk *kmsk)
{
	ssize_t rlen;
	struct kmsk_struct {
		uint8_t magic[4];
		uint32_t version;
		uint32_t reserved;
		uint32_t crc32;
		uint8_t kmsk[SWITCHTEC_KMSK_LEN];
	} data;

	char magic[4] = {'K', 'M', 'S', 'K'};
	uint32_t crc;

	rlen = fread(&data, 1, sizeof(data), kmsk_file);

	if (rlen < sizeof(data))
		return -EBADF;

	if (memcmp(data.magic, magic, sizeof(magic)))
		return -EBADF;

	crc = crc32(data.kmsk, SWITCHTEC_KMSK_LEN, 0, 1, 1);
	if (crc != le32toh(data.crc32))
		return -EBADF;

	memcpy(kmsk->kmsk, data.kmsk, SWITCHTEC_KMSK_LEN);

	return 0;
}

/**
 * @brief Read signature data from signature file
 * @param[in]  sig_file  Signature file
 * @param[out] signature Signature data
 * @return 0 on success, error code on failure
 */
int switchtec_read_signature_file(FILE *sig_file,
				  struct switchtec_signature *signature)
{
	ssize_t rlen;

	rlen = fread(signature->signature, 1, SWITCHTEC_SIG_LEN, sig_file);

	if (rlen < SWITCHTEC_SIG_LEN)
		return -EBADF;

	return 0;
}

/**
 * @brief Read UDS data from UDS file
 * @param[in]  uds_file  UDS file
 * @param[out] uds       UDS data
 * @return 0 on success, error code on failure
 */
int switchtec_read_uds_file(FILE *uds_file, struct switchtec_uds *uds)
{
	ssize_t rlen;

	rlen = fread(uds->uds, 1, SWITCHTEC_UDS_LEN, uds_file);

	if (rlen < SWITCHTEC_UDS_LEN)
		return -EBADF;

	return 0;
}

/**
 * @brief Check if secure config already has a KMSK entry
 * 	  KMSK stands for Key Manifest Secure Key.
 * 	  It is a key used to verify Key Manifest
 * 	  partition, which contains keys used to
 * 	  verify all other partitions.
 * @param[in]  state  Secure config
 * @param[out] kmsk   KMSK entry to check for
 * @return 0 on success, error code on failure
 */
int
switchtec_security_state_has_kmsk(struct switchtec_security_cfg_state *state,
				  struct switchtec_kmsk *kmsk)
{
	int key_idx;

	for(key_idx = 0; key_idx < state->public_key_num; key_idx++) {
		if (memcmp(state->public_key[key_idx], kmsk->kmsk,
			   SWITCHTEC_KMSK_LEN) == 0)
			return 1;
	}

	return 0;
}

#endif /* __linux__ */

static int switchtec_mfg_cmd(struct switchtec_dev *dev, uint32_t cmd,
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

static int sn_ver_get_gen4(struct switchtec_dev *dev,
			   struct switchtec_sn_ver_info *info)
{
	int ret;
	struct reply_t {
		uint32_t chip_serial;
		uint32_t ver_km;
		uint32_t ver_bl2;
		uint32_t ver_main;
		uint32_t ver_sec_unlock;
	} reply;

	ret = switchtec_mfg_cmd(dev, MRPC_SN_VER_GET, NULL, 0,
				&reply, sizeof(reply));
	if (ret)
		return ret;

	info->chip_serial = reply.chip_serial;
	info->ver_bl2 = reply.ver_bl2;
	info->ver_km = reply.ver_km;
	info->riot_ver_valid = false;
	info->ver_sec_unlock = reply.ver_sec_unlock;
	info->ver_main = reply.ver_main;

	return 0;
}

static int sn_ver_get_gen5(struct switchtec_dev *dev,
			   struct switchtec_sn_ver_info *info)
{
	int ret;
	uint32_t subcmd = 0;
	struct reply_t {
		uint32_t chip_serial;
		uint32_t ver_km;
		uint16_t ver_riot;
		uint16_t ver_bl2;
		uint32_t ver_main;
		uint32_t ver_sec_unlock;
	} reply;

	ret = switchtec_mfg_cmd(dev, MRPC_SN_VER_GET_GEN5, &subcmd, 4,
				&reply, sizeof(reply));
	if (ret)
		return ret;

	info->chip_serial = reply.chip_serial;
	info->ver_bl2 = reply.ver_bl2;
	info->ver_km = reply.ver_km;
	info->riot_ver_valid = true;
	info->ver_riot = reply.ver_riot;
	info->ver_sec_unlock = reply.ver_sec_unlock;
	info->ver_main = reply.ver_main;

	return 0;
}

/**
 * @brief Get serial number and security version
 * @param[in]  dev	Switchtec device handle
 * @param[out] info	Serial number and security version info
 * @return 0 on success, error code on failure
 */
int switchtec_sn_ver_get(struct switchtec_dev *dev,
			 struct switchtec_sn_ver_info *info)
{
	if (switchtec_is_gen5(dev))
		return sn_ver_get_gen5(dev, info);
	else
		return sn_ver_get_gen4(dev, info);
}

/**@}*/
