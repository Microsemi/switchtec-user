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

#define SWITCHTEC_MB_MAX_ENTRIES		16
#define SWITCHTEC_ACTV_IDX_MAX_ENTRIES		32
#define SWITCHTEC_ACTV_IDX_SET_ENTRIES		4

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
	uint8_t public_key[SWITCHTEC_KMSK_NUM][SWITCHTEC_KMSK_LEN];
	uint8_t rsvd4[32];
};

static int get_configs(struct switchtec_dev *dev, struct get_cfgs_reply *cfgs)
{
	uint8_t subcmd = 0;
	int ret;

	if (switchtec_gen(dev) == SWITCHTEC_GEN5) {
		subcmd = 1;
		ret = switchtec_mfg_cmd(dev,
					MRPC_SECURITY_CONFIG_GET_GEN5,
					&subcmd, sizeof(subcmd),
					cfgs, sizeof(struct get_cfgs_reply));
	} else {
		ret = switchtec_mfg_cmd(dev, MRPC_SECURITY_CONFIG_GET,
					NULL, 0, cfgs,
					sizeof(struct get_cfgs_reply));
	}

	return ret;
}

static int secure_config_get(struct switchtec_dev *dev,
			     struct switchtec_security_cfg_state *state,
			     struct switchtec_security_cfg_otp_region *otp,
			     bool *otp_valid)
{
	int ret;
	uint8_t subcmd = 0;
	uint32_t addr_shift;
	uint32_t map_shift;
	uint32_t map_mask;
	int spi_clk;
	struct get_cfgs_reply reply;

	if (otp_valid)
		*otp_valid = false;

	ret = switchtec_mfg_cmd(dev, MRPC_SECURITY_CONFIG_GET_EXT,
				&subcmd, sizeof(subcmd),
				&reply, sizeof(reply));
	if (ret && ERRNO_MRPC(errno) != ERR_CMD_INVALID)
		return ret;

	if (!ret) {
		if (otp) {
			otp->basic_valid = !!(reply.valid & BIT(5));
			otp->basic = !!(reply.valid & BIT(6));
			otp->mixed_ver_valid = !!(reply.valid & BIT(7));
			otp->mixed_ver = !!(reply.valid & BIT(8));
			otp->main_fw_ver_valid = !!(reply.valid & BIT(9));
			otp->main_fw_ver = !!(reply.valid & BIT(10));
			otp->sec_unlock_ver_valid = !!(reply.valid & BIT(11));
			otp->sec_unlock_ver = !!(reply.valid & BIT(12));
			otp->kmsk_valid[0] = !!(reply.valid & BIT(13));
			otp->kmsk[0] = !!(reply.valid & BIT(14));
			otp->kmsk_valid[1] = !!(reply.valid & BIT(15));
			otp->kmsk[1] = !!(reply.valid & BIT(16));
			otp->kmsk_valid[2] = !!(reply.valid & BIT(17));
			otp->kmsk[2] = !!(reply.valid & BIT(18));
			otp->kmsk_valid[3] = !!(reply.valid & BIT(19));
			otp->kmsk[3] = !!(reply.valid & BIT(20));

			if (otp_valid)
				*otp_valid = true;
		}
	} else {
		ret = get_configs(dev, &reply);
		if (ret)
			return ret;
	}

	reply.valid = le32toh(reply.valid);
	reply.cfg = le64toh(reply.cfg);
	reply.public_key_exponent = le32toh(reply.public_key_exponent);

	state->basic_setting_valid = !!(reply.valid & 0x01);
	state->public_key_exp_valid = !!(reply.valid & 0x02);
	state->public_key_num_valid = !!(reply.valid & 0x04);
	state->public_key_ver_valid = !!(reply.valid & 0x08);
	state->public_key_valid = !!(reply.valid & 0x10);

	state->debug_mode = reply.cfg & 0x03;
	state->secure_state = (reply.cfg>>2) & 0x03;

	state->jtag_lock_after_reset = !!(reply.cfg & 0x40);
	state->jtag_lock_after_bl1 = !!(reply.cfg & 0x80);
	state->jtag_bl1_unlock_allowed = !!(reply.cfg & 0x0100);
	state->jtag_post_bl1_unlock_allowed = !!(reply.cfg & 0x0200);

	spi_clk = (reply.cfg >> SWITCHTEC_CLK_RATE_BITSHIFT) & 0x0f;
	if (spi_clk == 0)
		spi_clk = 7;

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
	       SWITCHTEC_KMSK_NUM * SWITCHTEC_KMSK_LEN);

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
	return secure_config_get(dev, state, NULL, NULL);
}

/**
 * @brief Get secure boot extended configurations
 * @param[in]  dev	Switchtec device handle
 * @param[out] ext	Current extended secure boot settings
 * @return 0 on success, error code on failure
 */
int switchtec_security_config_get_ext(struct switchtec_dev *dev,
		struct switchtec_security_cfg_state_ext *ext)
{
	return secure_config_get(dev, &ext->state, &ext->otp, &ext->otp_valid);
}

/**
 * @brief Retrieve mailbox entries
 * @param[in]  dev	Switchtec device handle
 * @param[in]  fd	File handle to write the log data
 * @return 0 on success, error code on failure
 */
int switchtec_mailbox_to_file(struct switchtec_dev *dev, int fd)
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

/**
 * @brief Set secure settings
 * @param[in]  dev	Switchtec device handle
 * @param[out] setting	Secure boot settings
 * @return 0 on success, error code on failure
 */
int switchtec_security_config_set(struct switchtec_dev *dev,
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

	ret = get_configs(dev, &reply);
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

	ret = switchtec_mfg_cmd(dev, MRPC_SECURITY_CONFIG_SET, &sd, sizeof(sd),
				NULL, 0);
	return ret;
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

	return 0;
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
	int ret;
	int i = 0;
	struct active_idx {
		uint32_t count;
		struct entry {
			uint8_t image_id;
			uint8_t index;
		} idx[SWITCHTEC_ACTV_IDX_SET_ENTRIES];
	} set;

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

/**
 * @brief Execute the transferred firmware
 * @param[in]  dev		Switchtec device handle
 * @param[in]  recovery_mode	Recovery mode in case of a boot failure
 * @return 0 on success, error code on failure
 */
int switchtec_fw_exec(struct switchtec_dev *dev,
		      enum switchtec_bl2_recovery_mode recovery_mode)
{
	struct fw_exec_struct {
		uint8_t subcmd;
		uint8_t recovery_mode;
		uint8_t rsvd[2];
	} cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.subcmd = MRPC_FW_TX_EXEC;
	cmd.recovery_mode = recovery_mode;

	return switchtec_mfg_cmd(dev, MRPC_FW_TX, &cmd, sizeof(cmd), NULL, 0);
}

/**
 * @brief Resume device boot.
 *        Note that after calling this function, the current
 *        'dev' pointer is no longer valid. Before making further
 *        calls to switchtec library functions, be sure to close
 *        this pointer and get a new one by calling switchtec_open().
 * 	  Also be sure to check the return value of switchtec_open()
 * 	  for error, as the device might not be immediately 
 * 	  accessible after normal boot process.
 * @param[in]  dev	Switchtec device handle
 * @return 0 on success, error code on failure
 */
int switchtec_boot_resume(struct switchtec_dev *dev)
{
	return switchtec_mfg_cmd(dev, MRPC_BOOTUP_RESUME, NULL, 0,
				 NULL, 0);
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
	uint32_t data;

	if ((state != SWITCHTEC_INITIALIZED_UNSECURED)
	   && (state != SWITCHTEC_INITIALIZED_SECURED)) {
		return ERR_PARAM_INVALID;
	}
	data = htole32(state);

	return switchtec_mfg_cmd(dev, MRPC_SECURE_STATE_SET, &data,
				 sizeof(data), NULL, 0);
}

static int dbg_unlock_send_pubkey(struct switchtec_dev *dev,
				  struct switchtec_pubkey *public_key)
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

	return switchtec_mfg_cmd(dev, MRPC_DBG_UNLOCK, &cmd,
				 sizeof(cmd), NULL, 0);
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

	ret = dbg_unlock_send_pubkey(dev, public_key);
	if (ret)
		return ret;

	cmd.subcmd = MRPC_DBG_UNLOCK_DATA;
	cmd.serial = htole32(serial);
	cmd.unlock_ver = htole32(ver_sec_unlock);
	memcpy(cmd.signature, signature->signature, SWITCHTEC_SIG_LEN);

	return switchtec_mfg_cmd(dev, MRPC_DBG_UNLOCK, &cmd,
				 sizeof(cmd), NULL, 0);
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

	ret = dbg_unlock_send_pubkey(dev, public_key);
	if (ret)
		return ret;

	cmd.subcmd = MRPC_DBG_UNLOCK_UPDATE;
	cmd.serial = htole32(serial);
	cmd.unlock_ver = htole32(ver_sec_unlock);
	memcpy(cmd.signature, signature->signature, SWITCHTEC_SIG_LEN);

	return switchtec_mfg_cmd(dev, MRPC_DBG_UNLOCK, &cmd, sizeof(cmd),
				 NULL, 0);
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
	ssize_t rlen;
	char magic[4] = {'S', 'S', 'F', 'F'};
	uint32_t crc;
	struct setting_file_header {
		uint8_t magic[4];
		uint32_t version;
		uint8_t hw_gen;
		uint8_t rsvd[3];
		uint32_t crc;
	};
	struct setting_file_data {
		uint64_t cfg;
		uint32_t pub_key_exponent;
		uint8_t rsvd[36];
	};
	struct setting_file {
		struct setting_file_header header;
		struct setting_file_data data;
	} file_data;
	struct get_cfgs_reply reply;
	uint32_t addr_shift;
	uint32_t map_shift;
	uint32_t map_mask;
	enum switchtec_gen gen;
	int spi_clk;
	int ret;

	ret = get_configs(dev, &reply);
	if (ret)
		return ret;

	rlen = fread(&file_data, 1, sizeof(file_data), setting_file);

	if (rlen < sizeof(file_data))
		return -EBADF;

	if (memcmp(file_data.header.magic, magic, sizeof(magic)))
		return -EBADF;

	crc = crc32((uint8_t*)&file_data.data,
			sizeof(file_data.data), 0, 1, 1);
	if (crc != le32toh(file_data.header.crc))
		return -EBADF;
	switch (file_data.header.hw_gen) {
	case 0:
		gen = SWITCHTEC_GEN4;
		break;
	case 1:
		gen = SWITCHTEC_GEN5;
		break;
	default:
		return -EBADF;
	}

	memset(set, 0, sizeof(struct switchtec_security_cfg_set));

	file_data.data.cfg = le64toh(file_data.data.cfg);

	set->jtag_lock_after_reset =
		!!(file_data.data.cfg & SWITCHTEC_JTAG_LOCK_AFT_RST_BITMASK);
	set->jtag_lock_after_bl1 =
		!!(file_data.data.cfg & SWITCHTEC_JTAG_LOCK_AFT_BL1_BITMASK);
	set->jtag_bl1_unlock_allowed =
		!!(file_data.data.cfg & SWITCHTEC_JTAG_UNLOCK_BL1_BITMASK);
	set->jtag_post_bl1_unlock_allowed =
		!!(file_data.data.cfg & SWITCHTEC_JTAG_UNLOCK_AFT_BL1_BITMASK);

	spi_clk = (file_data.data.cfg >> SWITCHTEC_CLK_RATE_BITSHIFT) &
		SWITCHTEC_CLK_RATE_BITMASK;
	if (reply.spi_core_clk_high)
		set->spi_clk_rate = spi_clk_hi_rate_float[spi_clk - 1];
	else
		set->spi_clk_rate = spi_clk_rate_float[spi_clk - 1];

	set->i2c_recovery_tmo =
		(file_data.data.cfg >> SWITCHTEC_RC_TMO_BITSHIFT) &
		SWITCHTEC_RC_TMO_BITMASK;
	set->i2c_port =
		(file_data.data.cfg >> SWITCHTEC_I2C_PORT_BITSHIFT) &
		SWITCHTEC_I2C_PORT_BITMASK;

	get_i2c_operands(gen, &addr_shift, &map_shift, &map_mask);
	set->i2c_addr =
		(file_data.data.cfg >> addr_shift) &
		SWITCHTEC_I2C_ADDR_BITMASK;
	set->i2c_cmd_map = (file_data.data.cfg >> map_shift) & map_mask;

	set->public_key_exponent = le32toh(file_data.data.pub_key_exponent);

	return 0;
}

static int kmsk_set_send_pubkey(struct switchtec_dev *dev,
				struct switchtec_pubkey *public_key)
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

	return switchtec_mfg_cmd(dev, MRPC_KMSK_ENTRY_SET, &cmd,
				 sizeof(cmd), NULL, 0);
}

static int kmsk_set_send_signature(struct switchtec_dev *dev,
				   struct switchtec_signature *signature)
{
	struct kmsk_signature_cmd {
		uint8_t subcmd;
		uint8_t reserved[3];
		uint8_t signature[SWITCHTEC_SIG_LEN];
	} cmd = {};

	cmd.subcmd = MRPC_KMSK_ENTRY_SET_SIG;
	memcpy(cmd.signature, signature->signature,
	       SWITCHTEC_SIG_LEN);

	return switchtec_mfg_cmd(dev, MRPC_KMSK_ENTRY_SET, &cmd,
				 sizeof(cmd), NULL, 0);
}

static int kmsk_set_send_kmsk(struct switchtec_dev *dev,
			      struct switchtec_kmsk *kmsk)
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

	return switchtec_mfg_cmd(dev, MRPC_KMSK_ENTRY_SET, &cmd, sizeof(cmd),
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

	if (public_key) {
		ret = kmsk_set_send_pubkey(dev, public_key);
		if (ret)
			return ret;
	}

	if (signature) {
		ret = kmsk_set_send_signature(dev, signature);
		if (ret)
			return ret;
	}

	return kmsk_set_send_kmsk(dev, kmsk);
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

/**
 * @brief Get serial number and security version
 * @param[in]  dev	Switchtec device handle
 * @param[out] info	Serial number and security version info
 * @return 0 on success, error code on failure
 */
int switchtec_sn_ver_get(struct switchtec_dev *dev,
			 struct switchtec_sn_ver_info *info)
{
	int ret;

	ret = switchtec_mfg_cmd(dev, MRPC_SN_VER_GET, NULL, 0, info,
				sizeof(struct switchtec_sn_ver_info));
	if (ret)
		return ret;

	info->chip_serial = le32toh(info->chip_serial);
	info->ver_bl2 = le32toh(info->ver_bl2);
	info->ver_km = le32toh(info->ver_km);
	info->ver_main = le32toh(info->ver_main);
	info->ver_sec_unlock = le32toh(info->ver_sec_unlock);

	return 0;
}

/**@}*/
