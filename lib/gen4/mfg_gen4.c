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

#include "../switchtec_priv.h"
#include "switchtec/switchtec.h"
#include "switchtec/mfg.h"
#include "switchtec/errors.h"
#include "switchtec/endian.h"
#include "switchtec/mrpc.h"
#include "switchtec/errors.h"
#include <unistd.h>
#include "../mfg_common.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "lib/crc.h"
#include "config.h"

#ifdef __linux__

#if HAVE_LIBCRYPTO
#include <openssl/pem.h>
#endif

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

int switchtec_security_config_get_gen4(struct switchtec_dev *dev,
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

int switchtec_security_config_set_gen4(struct switchtec_dev *dev,
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

int switchtec_mailbox_to_file_gen4(struct switchtec_dev *dev, int fd)
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

int switchtec_active_image_index_get_gen4(struct switchtec_dev *dev,
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

int switchtec_active_image_index_set_gen4(struct switchtec_dev *dev,
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

int switchtec_fw_exec_gen4(struct switchtec_dev *dev, enum switchtec_bl2_recovery_mode recovery_mode)
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

	return switchtec_mfg_cmd(dev, cmd_id, &cmd, sizeof(cmd), NULL, 0);
}

int switchtec_boot_resume_gen4(struct switchtec_dev *dev)
{
	return switchtec_mfg_cmd(dev, MRPC_BOOTUP_RESUME,
				NULL, 0, NULL, 0);
}

int switchtec_secure_state_set_gen4(struct switchtec_dev *dev, int state)
{
	uint32_t data;

	data = htole32(state);

	return switchtec_mfg_cmd(dev, MRPC_SECURE_STATE_SET,
				 &data, sizeof(data), NULL, 0);
}

int switchtec_kmsk_set_gen4(struct switchtec_dev *dev, void *public_key,
			    void *signature, struct switchtec_kmsk *kmsk)
{
	int ret;
	uint32_t cmd_id = MRPC_KMSK_ENTRY_SET;

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

int switchtec_debug_unlock_gen4(struct switchtec_dev *dev, uint32_t serial,
				uint32_t ver_sec_unlock, void *public_key,
				struct switchtec_signature *signature,
				struct switchtec_gen6_token *token)
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

	cmd_id = get_dbg_unlock_id(dev);

	ret = dbg_unlock_send_pubkey(dev, public_key, cmd_id);
	if (ret)
		return ret;

	cmd.subcmd = MRPC_DBG_UNLOCK_DATA;
	cmd.serial = htole32(serial);
	cmd.unlock_ver = htole32(ver_sec_unlock);
	memcpy(cmd.signature, signature->signature, SWITCHTEC_SIG_LEN);

	return switchtec_mfg_cmd(dev, cmd_id, &cmd, sizeof(cmd), NULL, 0);
}

int switchtec_debug_lock_update_gen4(struct switchtec_dev *dev,
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

	cmd_id = get_dbg_unlock_id(dev);

	ret = dbg_unlock_send_pubkey(dev, public_key, cmd_id);
	if (ret)
		return ret;

	cmd.subcmd = MRPC_DBG_UNLOCK_UPDATE;
	cmd.serial = htole32(serial);
	cmd.unlock_ver = htole32(ver_sec_unlock);
	memcpy(cmd.signature, signature->signature, SWITCHTEC_SIG_LEN);

	return switchtec_mfg_cmd(dev, cmd_id, &cmd, sizeof(cmd), NULL, 0);	
}

int switchtec_dbg_unlock_version_update_gen4(struct switchtec_dev *dev,
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

	cmd_id = get_dbg_unlock_id(dev);

	ret = dbg_unlock_send_pubkey(dev, public_key, cmd_id);
	if (ret)
		return ret;

	cmd.subcmd = MRPC_DBG_UNLOCK_UPDATE;
	cmd.serial = htole32(serial);
	cmd.unlock_ver = htole32(ver_sec_unlock);
	memcpy(cmd.signature, signature->signature, SWITCHTEC_SIG_LEN);

	return switchtec_mfg_cmd(dev, cmd_id, &cmd, sizeof(cmd), NULL, 0);
}

int switchtec_read_sec_cfg_file_gen4(struct switchtec_dev *dev,
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

#if HAVE_LIBCRYPTO

#ifdef HAVE_DECL_PEM_READ_PUBKEY

#include <openssl/core_names.h>

int switchtec_read_pubk_file_gen4(FILE *pubk_file, struct switchtec_pubkey *pubk)
{
	BIGNUM *bn_priv = NULL;
	uint32_t exponent_tmp;
	EVP_PKEY *pkey;

	pkey = PEM_read_PUBKEY(pubk_file, NULL, NULL, NULL);
	if (!pkey) {
		fseek(pubk_file, 0L, SEEK_SET);
		pkey = PEM_read_PrivateKey(pubk_file, NULL, NULL, NULL);
		if (!pkey)
			return -1;
	}

	EVP_PKEY_get_bn_param(pkey, OSSL_PKEY_PARAM_RSA_N, &bn_priv);
	BN_bn2bin(bn_priv, pubk->pubkey);
	EVP_PKEY_get_bn_param(pkey, OSSL_PKEY_PARAM_RSA_E, &bn_priv);
	BN_bn2bin(bn_priv, (uint8_t *)&exponent_tmp);
	pubk->pubkey_exp = be32toh(exponent_tmp);

	EVP_PKEY_free(pkey);
	return 0;
}

#else /* ! HAVE_DECL_PEM_READ_PUBKEY */

#if !HAVE_DECL_RSA_GET0_KEY
/**
 *  openssl1.0 or older versions don't have this function, so copy
 *  the code from openssl1.1 here
 */
static void RSA_get0_key(const RSA *r, const BIGNUM **n,
			 const BIGNUM **e, const BIGNUM **d)
{
	if (n)
		*n = r->n;
	if (e)
		*e = r->e;
	if (d)
		*d = r->d;
}
#endif

/**
 * @brief Read public key from public key file
 * @param[in]  pubk_file Public key file
 * @param[out] pubk	 Public key
 * @return 0 on success, error code on failure
 */
int switchtec_read_pubk_file_gen4(FILE *pubk_file, struct switchtec_pubkey *pubk)
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

#endif /* HAVE_DECL_PEM_READ_PUBKEY */

#endif

int switchtec_read_kmsk_file_gen4(FILE *kmsk_file, struct switchtec_kmsk *kmsk)
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

int switchtec_read_signature_file_gen4(FILE *sig_file,
				  struct switchtec_signature *signature)
{
	ssize_t rlen;

	rlen = fread(signature->signature, 1, SWITCHTEC_SIG_LEN, sig_file);

	if (rlen < SWITCHTEC_SIG_LEN)
		return -EBADF;

	return 0;
}

int switchtec_read_uds_file_gen4(FILE *uds_file, struct switchtec_uds *uds)
{
	ssize_t rlen;

	rlen = fread(uds->uds, 1, SWITCHTEC_UDS_LEN, uds_file);

	if (rlen < SWITCHTEC_UDS_LEN)
		return -EBADF;

	return 0;
}

int switchtec_security_state_has_kmsk_gen4(struct switchtec_security_cfg_state *state,
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

#endif

int switchtec_sn_ver_get_gen4(struct switchtec_dev *dev, struct switchtec_sn_ver_info *info)
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