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

static int get_configs_gen5(struct switchtec_dev *dev,
			    struct get_cfgs_reply_gen5 *cfgs)
{
	uint32_t subcmd = 0;

	return switchtec_mfg_cmd(dev,
				 switchtec_is_gen6(dev) 
				 ? MRPC_SECURITY_CONFIG_GET_GEN6 :
				 MRPC_SECURITY_CONFIG_GET_GEN5,
				 &subcmd, sizeof(subcmd),
				 cfgs, sizeof(struct get_cfgs_reply_gen5));
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

int switchtec_security_config_get_gen5(struct switchtec_dev *dev,
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

int switchtec_security_config_set_gen5(struct switchtec_dev *dev,
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

int switchtec_mailbox_to_file_gen5(struct switchtec_dev *dev, int fd)
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

	enum mrpc_cmd cmd_id = switchtec_is_gen6(dev) ?
				MRPC_MAILBOX_GET_GEN6 : MRPC_MAILBOX_GET_GEN5;

	do {
		ret = switchtec_mfg_cmd(dev, cmd_id,
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

int switchtec_active_image_index_get_gen5(struct switchtec_dev *dev,
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

int switchtec_active_image_index_set_gen5(struct switchtec_dev *dev,
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

int switchtec_fw_exec_gen5(struct switchtec_dev *dev, enum switchtec_bl2_recovery_mode recovery_mode)
{
	uint32_t cmd_id = MRPC_FW_TX_GEN5;
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

int switchtec_boot_resume_gen5(struct switchtec_dev *dev)
{
	uint32_t subcmd = 0;
	return switchtec_mfg_cmd(dev, MRPC_BOOTUP_RESUME_GEN5,
				&subcmd, sizeof(subcmd),
				NULL, 0);
}

int switchtec_secure_state_set_gen5(struct switchtec_dev *dev, int state)
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

int switchtec_kmsk_set_gen5(struct switchtec_dev *dev, void *public_key,
			    void *signature, struct switchtec_kmsk *kmsk)
{
	int ret;
	uint32_t cmd_id = MRPC_KMSK_ENTRY_SET_GEN5;

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

int switchtec_read_sec_cfg_file_gen5(struct switchtec_dev *dev, FILE *setting_file,
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

#endif

int switchtec_sn_ver_get_gen5(struct switchtec_dev *dev, struct switchtec_sn_ver_info *info)
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