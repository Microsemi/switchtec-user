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

#define GEN6_SN_VER_GET_SUBCMD_BL1		0x0
#define GEN6_SN_VER_GET_SUBCMD_POST_BL1		0x1
#define GEN6_SECURE_STATUS_DBG_PORT_BIT		16
#define GEN6_SECURE_STATUS_DBG_PORT_MASK	0x1

static int switchtec_mfg_cmd(struct switchtec_dev *dev, uint32_t cmd,
			     const void *payload, size_t payload_len,
			     void *resp, size_t resp_len);

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

typedef struct switchtec_security_cfg_state_gen6 get_cfgs_reply_gen6;

typedef struct {
	uint32_t devsel                       :6;
	uint32_t devrev                       :2;
	uint32_t devsubrev                    :2;
	uint32_t did_bstp_ovrd                :1;
	uint32_t spieccmode                   :1;
	uint32_t reserved_dw_0_0              :4;
	uint32_t bickidv0                     :16;
	uint32_t kmtidv;
	uint32_t svl0;
	uint32_t svl1;
	uint32_t svl2;
	uint32_t svl3;
	uint32_t algo_crc_disable             :1;
	uint32_t algo_ecdsa_p384_disable      :1;
	uint32_t algo_ecdsa_p521_disable      :1;
	uint32_t algo_rsa3ksha2_disable       :1;
	uint32_t algo_rsa4ksha2_disable       :1;
	uint32_t algo_dilithium5_disable      :1;
	uint32_t rom_key_1_revoke             :1;
	uint32_t rom_key_2_revoke             :1;
	uint32_t rom_key_3_revoke             :1;
	uint32_t rom_key_4_revoke             :1;
	uint32_t secsc                        :1;
	uint32_t hash_table_sha2_384_disable  :1;
	uint32_t hash_table_sha2_512_disable  :1;
	uint32_t hash_table_sha3_512_disable  :1;
	uint32_t hash_table_crc32_disable     :1;
	uint32_t reserved_dw_6                :17;
	uint32_t puf_ac_status                :2;
	uint32_t puf_ac_read_mask             :1;
	uint32_t puf_ac_read_mask_req         :1;
	uint32_t otp_key0_hash_status         :2;
	uint32_t otp_key1_hash_status         :2;
	uint32_t otp_key2_hash_status         :2;
	uint32_t otp_key3_hash_status         :2;
	uint32_t otp_key4_hash_status         :2;
	uint32_t otp_key5_hash_status         :2;
	uint32_t otp_key6_hash_status         :2;
	uint32_t otp_key7_hash_status         :2;
	uint32_t otp_key8_hash_status         :2;
	uint32_t otp_key9_hash_status         :2;
	uint32_t otp_key10_hash_status        :2;
	uint32_t otp_key11_hash_status        :2;
	uint32_t reserved_dw_7                :4;
	uint32_t mfgmv                        :8;
	uint32_t mfgmstt                      :8;
	uint32_t reserved_dw_8                :16;
	uint32_t pm1off                       :20;
	uint32_t reserved_dw_9                :12;
	uint32_t pufacaddr0;
	uint32_t pufacaddr1;
	uint32_t peserial[5];
	uint32_t uniqueid[SWITCHTEC_UID_LEN_DWORDS];
} sec_security_diag_s;

static uint32_t get_dbg_unlock_id(struct switchtec_dev *dev)
{
	if (switchtec_is_gen6(dev))
		return MRPC_DBG_UNLOCK_GEN6;
	else if (switchtec_is_gen5(dev))
		return MRPC_DBG_UNLOCK_GEN5;
	else
		return MRPC_DBG_UNLOCK;
}

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
				 switchtec_is_gen6(dev)
				 ? MRPC_SECURITY_CONFIG_GET_GEN6 :
				 MRPC_SECURITY_CONFIG_GET_GEN5,
				 &subcmd, sizeof(subcmd),
				 cfgs, sizeof(struct get_cfgs_reply_gen5));
}

static int get_configs_gen6(struct switchtec_dev *dev,
			    get_cfgs_reply_gen6 *cfgs)
{
	int ret;
	uint32_t subcmd = 0;
	sec_security_diag_s diag = {};
	struct switchtec_device_config_dev_settings dev_settings = {};
	struct switchtec_device_config_get_sec dev_cfg_sec = {};

	ret = switchtec_mfg_cmd(dev,
				MRPC_SECURE_CONFIG_GET_GEN6,
				&subcmd, sizeof(subcmd),
				&diag, sizeof(diag));
	if (ret)
		return ret;

	memset(cfgs, 0, sizeof(*cfgs));

	cfgs->algo_crc_disable = diag.algo_crc_disable;
	cfgs->algo_ecdsa_p384_disable = diag.algo_ecdsa_p384_disable;
	cfgs->algo_ecdsa_p521_disable = diag.algo_ecdsa_p521_disable;
	cfgs->algo_rsa3ksha2_disable = diag.algo_rsa3ksha2_disable;
	cfgs->algo_rsa4ksha2_disable = diag.algo_rsa4ksha2_disable;
	cfgs->algo_dilithium5_disable = diag.algo_dilithium5_disable;
	cfgs->rom_key_1_disable = diag.rom_key_1_revoke;
	cfgs->rom_key_2_disable = diag.rom_key_2_revoke;
	cfgs->rom_key_3_disable = diag.rom_key_3_revoke;
	cfgs->rom_key_4_disable = diag.rom_key_4_revoke;
	cfgs->secsc = diag.secsc;
	cfgs->has_table_sha2_384_disable = diag.hash_table_sha2_384_disable;
	cfgs->has_table_sha2_512_disable = diag.hash_table_sha2_512_disable;
	cfgs->has_table_sha3_512_disable = diag.hash_table_sha3_512_disable;
	cfgs->has_table_crc32_disable = diag.hash_table_crc32_disable;
	cfgs->puf_ac_status = diag.puf_ac_status;
	cfgs->otp_key0_hash_status = diag.otp_key0_hash_status;
	cfgs->otp_key1_hash_status = diag.otp_key1_hash_status;
	cfgs->otp_key2_hash_status = diag.otp_key2_hash_status;
	cfgs->otp_key3_hash_status = diag.otp_key3_hash_status;
	cfgs->otp_key4_hash_status = diag.otp_key4_hash_status;
	cfgs->otp_key5_hash_status = diag.otp_key5_hash_status;
	cfgs->otp_key6_hash_status = diag.otp_key6_hash_status;
	cfgs->otp_key7_hash_status = diag.otp_key7_hash_status;
	cfgs->otp_key8_hash_status = diag.otp_key8_hash_status;
	cfgs->otp_key9_hash_status = diag.otp_key9_hash_status;
	cfgs->otp_key10_hash_status = diag.otp_key10_hash_status;
	cfgs->otp_key11_hash_status = diag.otp_key11_hash_status;

	ret = switchtec_device_config_get(dev, &dev_settings);
	if (ret == 0) {
		cfgs->twi_rcvry_address_mrpc = dev_settings.twi_mrpc_addr;
		cfgs->twi_rcvry_address_ocp = dev_settings.twi_ocp_addr;
		cfgs->twi_rcvry_bus = dev_settings.twi_rcvry_bus;
		cfgs->twi_address_type = dev_settings.twi_rcvry_addr_type;
		cfgs->i3c_pid_high = dev_settings.i3c_pid_hi;
		cfgs->i3c_pid_low = dev_settings.i3c_pid_lo;
		cfgs->i3c_rcvry_address = dev_settings.i3c_addr_7bit;
		cfgs->i3c_rcvry_bus = dev_settings.i3c_rcvry_bus;
	}

	ret = switchtec_device_config_get_security(dev, &dev_cfg_sec);
	if (ret)
		return ret;

	cfgs->mrpc_command_map = dev_cfg_sec.secure_settings.command_map;
	cfgs->static_token_disable = dev_cfg_sec.secure_settings.static_token_disable;
	cfgs->psid_only_token_disable = dev_cfg_sec.secure_settings.psid_only_token_disable;
	cfgs->uid_only_token_disable = dev_cfg_sec.secure_settings.uid_only_token_disable;
	cfgs->psid_uid_token_disable = dev_cfg_sec.secure_settings.psid_uid_token_disable;
	cfgs->boot_from_uart_disable = dev_cfg_sec.secure_settings.boot_from_uart_disable;
	cfgs->boot_from_smbus_disable = dev_cfg_sec.secure_settings.boot_from_smbus_disable;
	cfgs->boot_from_i3c_disable = dev_cfg_sec.secure_settings.boot_from_i3c_disable;
	cfgs->failover_to_uart_disable = dev_cfg_sec.secure_settings.failover_to_uart_disable;
	cfgs->failover_to_smbus_disable = dev_cfg_sec.secure_settings.failover_to_smbus_disable;
	cfgs->failover_to_i3c_disable = dev_cfg_sec.secure_settings.failover_to_i3c_disable;

	int num_keys = dev_cfg_sec.secure_settings.key_prog_num;

	if (num_keys > SWITCHTEC_KMSK_NUM_GEN6)
		num_keys = SWITCHTEC_KMSK_NUM_GEN6;

	for (int i = 0; i < num_keys; i++) {
		for (int j = 0; j < SWITCHTEC_KMSK_LEN_DWORDS; j++) {
			cfgs->otp_key_hash[i][j] =
				dev_cfg_sec.secure_settings.key_data[i].hash[j];
		}
	}

	return 0;
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

static int security_config_get_gen6(struct switchtec_dev *dev,
				    struct switchtec_security_cfg_state_gen6 *state)
{
	int ret;
	get_cfgs_reply_gen6 reply;

	ret = get_configs_gen6(dev, &reply);
	if (ret)
		return ret;

	memcpy(state, &reply, sizeof(reply));

	return 0;
}

/**
 * @brief Get secure boot configurations
 * @param[in]  dev	Switchtec device handle
 * @param[out] state	Current secure boot settings
 * @return 0 on success, error code on failure
 */
int switchtec_security_config_get(struct switchtec_dev *dev,
				  void *state)
{
	if (switchtec_is_gen6(dev))
		return security_config_get_gen6(dev,
			(struct switchtec_security_cfg_state_gen6 *)state);
	if (switchtec_is_gen5(dev))
		return security_config_get_gen5(dev,
			(struct switchtec_security_cfg_state *)state);
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

/*
 * @brief Get secure boot settings in BL1 for gen6.
 * Outside of
 * @param[in]  dev	Switchtec device handle
 * @param[out] state	Current secure boot settings
 * @return 0 on success, error code on failure
 *
 * Command ID cannot be accessed outside of BL1 phase. Use security_config_get_gen6
 * to access the secure settings during MainFW phase.
*/
int security_settings_get_gen6(struct switchtec_dev *dev,
				struct switchtec_security_cfg_state_gen6 *state)
{
	int ret;

	struct sec_cfg_get_struct cmd = {};
	uint32_t reply_otp[MRPC_MAX_DATA_LEN / sizeof(uint32_t)] = {};

	/* get first 60dwords of OTP content */
	cmd.subcmd = MRPC_GET_SECURE_OTP;
	cmd.OTP_dword_offset = 0;
	cmd.read_dwords = 62;

	ret = switchtec_mfg_cmd(dev, MRPC_SECURITY_CONFIG_GET_GEN6, &cmd, sizeof(cmd),
				&reply_otp, cmd.read_dwords * sizeof(uint32_t));
	if (ret)
		return ret;

	memset(state, 0, sizeof(*state));

	/* DWORD 10 — TWI RECOVERY */
	state->twi_rcvry_address_mrpc = (reply_otp[OTP_DWORD_10] & OTP_DWORD_10_SMBUS_SMBRMRPCADDR_MSK)
					>> OTP_DWORD_10_SMBUS_SMBRMRPCADDR_LSB;
	state->twi_rcvry_bus = (reply_otp[OTP_DWORD_10] & OTP_DWORD_10_SMBUS_SMBRIF_MSK)
			       >> OTP_DWORD_10_SMBUS_SMBRIF_LSB;
	state->twi_address_type = (reply_otp[OTP_DWORD_10] & OTP_DWORD_10_SMBUS_SMBRATYPE_MSK)
				  >> OTP_DWORD_10_SMBUS_SMBRATYPE_LSB;
	state->twi_rcvry_address_ocp = (reply_otp[OTP_DWORD_10] & OTP_DWORD_10_SMBUS_SMBROCPADDR_MSK)
				       >> OTP_DWORD_10_SMBUS_SMBROCPADDR_LSB;

	/* DWORD 25 / DWORD 0 / DWORD 9 */
	state->mrpc_command_map     = (reply_otp[OTP_DWORD_25] & OTP_DWORD_25_CONTROL_MRPCCMD_MSK) 
				       >> OTP_DWORD_25_CONTROL_MRPCCMD_LSB;
	state->secsc                = (reply_otp[OTP_DWORD_0] & OTP_DWORD_0_PRODUCT_SECSC_MSK) 
				       >> OTP_DWORD_0_PRODUCT_SECSC_LSB;
	state->ap_offset            = (reply_otp[OTP_DWORD_9] & OTP_DWORD_9_ADDITIONAL_APOFF_MSK) 
				       >> OTP_DWORD_9_ADDITIONAL_APOFF_LSB;
	state->puf_ac_status        = (reply_otp[OTP_DWORD_25] & OTP_DWORD_25_CONTROL_PUFACS_MSK) 
				       >> OTP_DWORD_25_CONTROL_PUFACS_LSB;

	/* DWORD 25 - inbuilt ROM key revoke flags */
	state->rom_key_1_disable = (reply_otp[OTP_DWORD_25] & OTP_DWORD_25_CONTROL_IRK1R_MSK) 
				    >> OTP_DWORD_25_CONTROL_IRK1R_LSB;
	state->rom_key_2_disable = (reply_otp[OTP_DWORD_25] & OTP_DWORD_25_CONTROL_IRK2R_MSK) 
				    >> OTP_DWORD_25_CONTROL_IRK2R_LSB;
	state->rom_key_3_disable = (reply_otp[OTP_DWORD_25] & OTP_DWORD_25_CONTROL_IRK3R_MSK) 
				    >> OTP_DWORD_25_CONTROL_IRK3R_LSB;
	state->rom_key_4_disable = (reply_otp[OTP_DWORD_25] & OTP_DWORD_25_CONTROL_IRK4R_MSK) 
				    >> OTP_DWORD_25_CONTROL_IRK4R_LSB;
	
	/* DWORD 23 — KEY HASH STATUS */
	state->otp_key0_hash_status = (reply_otp[OTP_DWORD_23] & OTP_DWORD_23_CONTROL_BIAK0S_MSK)
				      >> OTP_DWORD_23_CONTROL_BIAK0S_LSB;
	state->otp_key1_hash_status = (reply_otp[OTP_DWORD_23] & OTP_DWORD_23_CONTROL_BIAK1S_MSK)
				      >> OTP_DWORD_23_CONTROL_BIAK1S_LSB;
	state->otp_key2_hash_status = (reply_otp[OTP_DWORD_23] & OTP_DWORD_23_CONTROL_BIAK2S_MSK)
				      >> OTP_DWORD_23_CONTROL_BIAK2S_LSB;
	state->otp_key3_hash_status = (reply_otp[OTP_DWORD_23] & OTP_DWORD_23_CONTROL_BIAK3S_MSK)
				      >> OTP_DWORD_23_CONTROL_BIAK3S_LSB;
	state->otp_key4_hash_status = (reply_otp[OTP_DWORD_23] & OTP_DWORD_23_CONTROL_BIAK4S_MSK)
				      >> OTP_DWORD_23_CONTROL_BIAK4S_LSB;
	state->otp_key5_hash_status = (reply_otp[OTP_DWORD_23] & OTP_DWORD_23_CONTROL_BIAK5S_MSK)
				      >> OTP_DWORD_23_CONTROL_BIAK5S_LSB;
	state->otp_key6_hash_status = (reply_otp[OTP_DWORD_23] & OTP_DWORD_23_CONTROL_BIAK6S_MSK)
				      >> OTP_DWORD_23_CONTROL_BIAK6S_LSB;
	state->otp_key7_hash_status = (reply_otp[OTP_DWORD_23] & OTP_DWORD_23_CONTROL_BIAK7S_MSK)
				      >> OTP_DWORD_23_CONTROL_BIAK7S_LSB;
	state->otp_key8_hash_status = (reply_otp[OTP_DWORD_23] & OTP_DWORD_23_CONTROL_BIAK8S_MSK)
				      >> OTP_DWORD_23_CONTROL_BIAK8S_LSB;
	state->otp_key9_hash_status = (reply_otp[OTP_DWORD_23] & OTP_DWORD_23_CONTROL_BIAK9S_MSK)
				      >> OTP_DWORD_23_CONTROL_BIAK9S_LSB;
	state->otp_key10_hash_status = (reply_otp[OTP_DWORD_23] & OTP_DWORD_23_CONTROL_BIAK10S_MSK)
				       >> OTP_DWORD_23_CONTROL_BIAK10S_LSB;
	state->otp_key11_hash_status = (reply_otp[OTP_DWORD_23] & OTP_DWORD_23_CONTROL_BIAK11S_MSK)
				       >> OTP_DWORD_23_CONTROL_BIAK11S_LSB;

	/* DWORD 20 - hash table disable bits */
	state->has_table_sha2_384_disable = (reply_otp[OTP_DWORD_20] & OTP_DWORD_20_CONTROL_HTSHA2384D_MSK) >> 
					     OTP_DWORD_20_CONTROL_HTSHA2384D_LSB;
	state->has_table_sha2_512_disable = (reply_otp[OTP_DWORD_20] & OTP_DWORD_20_CONTROL_HTSHA2512D_MSK) >> 
					     OTP_DWORD_20_CONTROL_HTSHA2512D_LSB;
	state->has_table_sha3_512_disable = (reply_otp[OTP_DWORD_20] & OTP_DWORD_20_CONTROL_HTSHA3512D_MSK) >> 
					     OTP_DWORD_20_CONTROL_HTSHA3512D_LSB;
	state->has_table_crc32_disable    = (reply_otp[OTP_DWORD_20] & OTP_DWORD_20_CONTROL_HTCRC32D_MSK) >> 
					     OTP_DWORD_20_CONTROL_HTCRC32D_LSB;

	/* DWORD 11-13 — I3C IDENTIFIERS */
	state->i3c_pid_high = (reply_otp[OTP_DWORD_11] & OTP_DWORD_11_I3C_I3CPID_MSK)
			      >> OTP_DWORD_11_I3C_I3CPID_LSB;
	state->i3c_pid_low = (reply_otp[OTP_DWORD_12] & OTP_DWORD_12_I3C_I3CPID2_MSK)
			     >> OTP_DWORD_12_I3C_I3CPID2_LSB;
	state->i3c_rcvry_address = (reply_otp[OTP_DWORD_13] & OTP_DWORD_13_I3C_I3CADDR_MSK)
				   >> OTP_DWORD_13_I3C_I3CADDR_LSB;
	state->i3c_rcvry_bus = (reply_otp[OTP_DWORD_13] & OTP_DWORD_13_I3C_I3CINST_MSK)
			       >> OTP_DWORD_13_I3C_I3CINST_LSB;

	/* DWORD 19 — ALGO/BOOT/FAILOVER/TOKEN DISABLE FLAGS */
	state->algo_crc_disable = (reply_otp[OTP_DWORD_19] & OTP_DWORD_19_CONTROL_CRC32D_MSK)
				  >> OTP_DWORD_19_CONTROL_CRC32D_LSB;
	state->algo_ecdsa_p384_disable = (reply_otp[OTP_DWORD_19] & OTP_DWORD_19_CONTROL_ECDSAP384D_MSK)
					 >> OTP_DWORD_19_CONTROL_ECDSAP384D_LSB;
	state->algo_ecdsa_p521_disable = (reply_otp[OTP_DWORD_19] & OTP_DWORD_19_CONTROL_ECDSAP521D_MSK)
					 >> OTP_DWORD_19_CONTROL_ECDSAP521D_LSB;
	state->algo_rsa3ksha2_disable = (reply_otp[OTP_DWORD_19] & OTP_DWORD_19_CONTROL_RSA3KSHA2D_MSK)
					>> OTP_DWORD_19_CONTROL_RSA3KSHA2D_LSB;
	state->algo_rsa4ksha2_disable = (reply_otp[OTP_DWORD_19] & OTP_DWORD_19_CONTROL_RSA4KSHA2D_MSK)
					>> OTP_DWORD_19_CONTROL_RSA4KSHA2D_LSB;
	state->algo_dilithium5_disable = (reply_otp[OTP_DWORD_19] & OTP_DWORD_19_CONTROL_DILITHIUM5D_MSK)
					 >> OTP_DWORD_19_CONTROL_DILITHIUM5D_LSB;
	state->boot_from_uart_disable = (reply_otp[OTP_DWORD_19] & OTP_DWORD_19_CONTROL_BFUD_MSK)
					>> OTP_DWORD_19_CONTROL_BFUD_LSB;
	state->boot_from_smbus_disable = (reply_otp[OTP_DWORD_19] & OTP_DWORD_19_CONTROL_BFSMBUSD_MSK)
					 >> OTP_DWORD_19_CONTROL_BFSMBUSD_LSB;
	state->boot_from_i3c_disable = (reply_otp[OTP_DWORD_19] & OTP_DWORD_19_CONTROL_BFI3CD_MSK)
				       >> OTP_DWORD_19_CONTROL_BFI3CD_LSB;
	state->failover_to_uart_disable = (reply_otp[OTP_DWORD_19] & OTP_DWORD_19_CONTROL_BF2UD_MSK)
					  >> OTP_DWORD_19_CONTROL_BF2UD_LSB;
	state->failover_to_smbus_disable = (reply_otp[OTP_DWORD_19] & OTP_DWORD_19_CONTROL_BF2SMBUSD_MSK)
					   >> OTP_DWORD_19_CONTROL_BF2SMBUSD_LSB;
	state->failover_to_i3c_disable = (reply_otp[OTP_DWORD_19] & OTP_DWORD_19_CONTROL_BF2I3CD_MSK)
					 >> OTP_DWORD_19_CONTROL_BF2I3CD_LSB;
	state->static_token_disable    = (reply_otp[OTP_DWORD_25] >> OTP_DWORD_25_CONTROL_STTKND_LSB) & 0x1;
	state->psid_only_token_disable = (reply_otp[OTP_DWORD_25] >> OTP_DWORD_25_CONTROL_PSIDTKND_LSB) & 0x1;
	state->uid_only_token_disable  = (reply_otp[OTP_DWORD_25] >> OTP_DWORD_25_CONTROL_UIDTKND_LSB) & 0x1;
	state->psid_uid_token_disable  = (reply_otp[OTP_DWORD_25] >> OTP_DWORD_25_CONTROL_UIDPSIDTKND_LSB) & 0x1;

	/* get 192 dwords of OTP content from offset 656 for keys*/
	cmd.subcmd = MRPC_GET_SECURE_OTP;
	cmd.OTP_dword_offset = OTP_MULTI_DWORD_IMAGE_BIAK0;
	cmd.read_dwords = (SWITCHTEC_KMSK_NUM_GEN6 * SWITCHTEC_KMSK_LEN_DWORDS);

	ret = switchtec_mfg_cmd(dev, MRPC_SECURITY_CONFIG_GET_GEN6, &cmd, sizeof(cmd),
				&reply_otp, cmd.read_dwords * sizeof(uint32_t));
	if (ret)
		return ret;

	for (int i = 0; i < SWITCHTEC_KMSK_NUM_GEN6; i++) {
		memcpy(state->otp_key_hash[i],
			&reply_otp[i * SWITCHTEC_KMSK_LEN_DWORDS],
			SWITCHTEC_KMSK_LEN_DWORDS * sizeof(uint32_t));
	}

	return 0;
}

static int mailbox_to_file_gen56(struct switchtec_dev *dev, int fd)
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

/**
 * @brief Retrieve mailbox entries
 * @param[in]  dev	Switchtec device handle
 * @param[in]  fd	File handle to write the log data
 * @return 0 on success, error code on failure
 */
int switchtec_mailbox_to_file(struct switchtec_dev *dev, int fd)
{
	if (switchtec_is_gen5(dev) || switchtec_is_gen6(dev))
		return mailbox_to_file_gen56(dev, fd);
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
	if (switchtec_is_gen6(dev))
		cmd_id = MRPC_FW_TX_GEN6;
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

#define GEN6_STATE_SET_SUBCMD_DEBUG_PROTECT  0x0
#define GEN6_STATE_SET_SUBCMD_STATE_TRANS    0x1
#define GEN6_STATE_SET_FLAG_SKIP_CUST_CFG    0x10000

int switchtec_secure_state_set_debug_protect(struct switchtec_dev *dev)
{
	uint32_t data;

	if (!switchtec_is_gen6(dev))
		return ERR_SUBCMD_INVALID;

	data = htole32(GEN6_STATE_SET_SUBCMD_DEBUG_PROTECT);

	return switchtec_mfg_cmd(dev, MRPC_SECURE_STATE_SET_GEN6,
				 &data, sizeof(data), NULL, 0);
}

int switchtec_secure_state_set_transition(struct switchtec_dev *dev,
					  enum switchtec_secure_state state)
{
	return switchtec_secure_state_set_transition_ex(dev, state, 0);
}

int switchtec_secure_state_set_transition_ex(struct switchtec_dev *dev,
					     enum switchtec_secure_state state,
					     int skip_customer_config)
{
	uint32_t data;
	uint32_t flags = 0;

	if (!switchtec_is_gen6(dev))
		return ERR_SUBCMD_INVALID;

	if ((state != SWITCHTEC_INITIALIZED_UNSECURED)
	   && (state != SWITCHTEC_INITIALIZED_SECURED)) {
		return ERR_PARAM_INVALID;
	}

	if (skip_customer_config)
		flags |= GEN6_STATE_SET_FLAG_SKIP_CUST_CFG;

	data = htole32(GEN6_STATE_SET_SUBCMD_STATE_TRANS | (state << 8) | flags);

	return switchtec_mfg_cmd(dev, MRPC_SECURE_STATE_SET_GEN6,
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

	if (switchtec_is_gen6(dev))
		return switchtec_secure_state_set_transition(dev, state);
	else if (switchtec_is_gen5(dev))
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

static int dbg_unlock_send_pubkey_gen6(struct switchtec_dev *dev,
					struct switchtec_pubkey *public_key,
					uint32_t cmd_id)
{
	struct public_key_cmd_gen6 {
		uint8_t subcmd;
		uint8_t reserved[3];
		uint32_t  total_len;
		uint32_t  total_crc;
		uint32_t  data_len;
		uint32_t  offset;
		uint8_t  pub_key[SWITCHTEC_PUB_KEY_LEN];
	} cmd = {};

	cmd.subcmd = MRPC_GEN6_DBG_UNLOCK_PKEY;
	memcpy(cmd.pub_key, public_key->pubkey, SWITCHTEC_PUB_KEY_LEN);
	cmd.total_len = htole32(SWITCHTEC_PUB_KEY_LEN);
	cmd.total_crc = htole32(crc32(cmd.pub_key, SWITCHTEC_PUB_KEY_LEN, 0, 1, 1));
	cmd.data_len = htole32(SWITCHTEC_PUB_KEY_LEN);
	cmd.offset = htole32(0);

	return switchtec_mfg_cmd(dev, cmd_id, &cmd, sizeof(cmd), NULL, 0);
}

static int dbg_unlock_send_sig_gen6(struct switchtec_dev *dev,
				struct switchtec_signature *signature,
				uint32_t cmd_id)
{
	struct sig_cmd_gen6 {
		uint8_t subcmd;
		uint8_t sig_type;
		uint8_t reserved[2];
		uint32_t  total_len;
		uint32_t  total_crc;
		uint32_t  data_len;
		uint32_t  offset;
		uint8_t  signature[SWITCHTEC_SIG_LEN];
	} cmd = {};

	cmd.subcmd = MRPC_GEN6_DBG_UNLOCK_SIG;
	cmd.sig_type = KMT_SIG_FORMAT_RSA4KSHA2;
	memcpy(cmd.signature, signature->signature, SWITCHTEC_SIG_LEN);
	cmd.total_len = htole32(SWITCHTEC_SIG_LEN);
	cmd.total_crc = htole32(crc32(cmd.signature, SWITCHTEC_SIG_LEN, 0, 1, 1));
	cmd.data_len = htole32(SWITCHTEC_SIG_LEN);
	cmd.offset = htole32(0);

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
			 struct switchtec_signature *signature,
			 struct switchtec_gen6_token *token)
{
	int ret;

	if (switchtec_is_gen6(dev))
	{
		struct unlock_cmd_gen6 {
			uint8_t subcmd;
			uint8_t rsvd[3];
			uint8_t token[SWITCHTEC_GEN6_TOKEN_STATIC_LEN];
		} cmd = {};

		uint32_t cmd_id;
		cmd_id = MRPC_DBG_UNLOCK_GEN6;

		ret = dbg_unlock_send_pubkey_gen6(dev, public_key, cmd_id);
		if (ret)
			return ret;

		ret = dbg_unlock_send_sig_gen6(dev, signature, cmd_id);
		if (ret)
			return ret;

		cmd.subcmd = MRPC_GEN6_DBG_UNLOCK_STATIC;
		memcpy(cmd.token, token->token, SWITCHTEC_GEN6_TOKEN_STATIC_LEN);

		return switchtec_mfg_cmd(dev, cmd_id, &cmd, sizeof(cmd), NULL, 0);
	}

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

/**
 * @brief Get gen6 token
 * @param[in]  dev		Switchtec device handle
 * @param[in]  token	pointer to downloaded token
 * @param[in]  token_type	type of token to download
 *
 * @return 0 on success, error code on failure
 */
int gen6_token_len_by_type(int token_type)
{
	if (token_type == GEN6_TOKEN_STATIC)
		return SWITCHTEC_GEN6_TOKEN_STATIC_LEN;
	if (token_type == GEN6_TOKEN_EPHEMERAL)
		return SWITCHTEC_GEN6_TOKEN_EPHEMERAL_LEN;
	if (token_type == GEN6_TOKEN_VER_UPDATE)
		return SWITCHTEC_GEN6_TOKEN_VER_UPDATE_LEN;

	return SWITCHTEC_GEN6_TOKEN_DISABLE_STATIC_LEN;
}

int switchtec_dbg_unlock_get_token_gen6(struct switchtec_dev *dev,
			struct switchtec_gen6_token *token,
			int token_type,
			int auth_type)
{
	int ret;
	int token_len;

	struct get_unlock_token_cmd_gen6 {
		uint8_t subcmd;
		uint8_t token_type;
		uint8_t auth_type;
		uint8_t rsvd;
	} cmd = {};

	uint32_t cmd_id;
	cmd_id = MRPC_DBG_UNLOCK_GEN6;

	struct get_unlock_token_reply_gen6 {
		uint8_t token[SWITCHTEC_GEN6_TOKEN_MAX_LEN];
	} reply;

	cmd.subcmd = MRPC_GEN6_DBG_UNLOCK_TOKEN_GET;

	if (token_type == GEN6_TOKEN_STATIC)
		cmd.token_type = SECURE_TOKEN_GET_TYPE_STATIC;
	else if (token_type == GEN6_TOKEN_EPHEMERAL)
		cmd.token_type = SECURE_TOKEN_GET_TYPE_EPHEMERAL;
	else if (token_type == GEN6_TOKEN_VER_UPDATE)
		cmd.token_type = SECURE_TOKEN_GET_TYPE_VER_UPDATE;
	else
		cmd.token_type = SECURE_TOKEN_GET_TYPE_DISABLE_STATIC;

	cmd.auth_type = (uint8_t)auth_type;
	token_len = gen6_token_len_by_type(token_type);
	ret = switchtec_mfg_cmd(dev, cmd_id, &cmd, sizeof(cmd), &reply,
			       token_len);
	if (ret)
		return ret;

	memcpy(token->token, reply.token, token_len);
	return 0;
}

int switchtec_dbg_unlock_status_get_gen6(struct switchtec_dev *dev,
					 uint32_t *jtag_status)
{
	int ret;
	enum switchtec_boot_phase phase = switchtec_boot_phase(dev);
	struct {
		uint8_t subcmd;
		uint8_t rsvd[3];
	} cmd = {};
	struct {
		uint32_t jtag_status;
	} reply;
	struct sec_cfg_get_struct sec_status_cmd = {};
	uint64_t secure_status = 0;

	if (phase == SWITCHTEC_BOOT_PHASE_BL1) {
		sec_status_cmd.subcmd = MRPC_GET_SECURE_STATUS;
		ret = switchtec_mfg_cmd(dev, MRPC_SECURITY_CONFIG_GET_GEN6,
					&sec_status_cmd, sizeof(sec_status_cmd),
					&secure_status, sizeof(secure_status));
		if (ret)
			return ret;

		*jtag_status = (le64toh(secure_status) >>
				GEN6_SECURE_STATUS_DBG_PORT_BIT) &
			GEN6_SECURE_STATUS_DBG_PORT_MASK;
		return 0;
	}

	cmd.subcmd = MRPC_GEN6_DBG_UNLOCK_STATUS_GET;
	ret = switchtec_mfg_cmd(dev, MRPC_DBG_UNLOCK_GEN6,
				&cmd, sizeof(cmd), &reply, sizeof(reply));
	if (ret)
		return ret;

	*jtag_status = le32toh(reply.jtag_status);
	return 0;
}

#define GEN6_STATE_SET_SUBCMD_STATE_GET      0x2

static enum switchtec_secure_state_gen6
bl1_derive_state_from_otp(uint32_t *reply_otp)
{
	uint32_t devsel = (reply_otp[OTP_DWORD_0] &
			   OTP_DWORD_0_PRODUCT_DEVSEL_MSK) >>
			  OTP_DWORD_0_PRODUCT_DEVSEL_LSB;
	uint32_t crc_disabled = (reply_otp[OTP_DWORD_19] &
				 OTP_DWORD_19_CONTROL_BIICFCRCD_MSK) >>
				OTP_DWORD_19_CONTROL_BIICFCRCD_LSB;
	uint32_t rsa4k_disabled = (reply_otp[OTP_DWORD_20] &
				   OTP_DWORD_20_CONTROL_BIICFRS2D_MSK) >>
				  OTP_DWORD_20_CONTROL_BIICFRS2D_LSB;
	uint32_t secstateset = (reply_otp[OTP_DWORD_61] &
				OTP_DWORD_61_MFGMSTT_SECSTATESET_MSK) >>
			       OTP_DWORD_61_MFGMSTT_SECSTATESET_LSB;

	if (devsel & OTP_DWORD_0_PRODUCT_DEVSEL_FAMILY_BIT) {
		if (crc_disabled)
			return SWITCHTEC_GEN6_INITIALIZED_SECURED;
		else if (rsa4k_disabled)
			return SWITCHTEC_GEN6_INITIALIZED_UNSECURED;
		else
			return SWITCHTEC_GEN6_UNINITIALIZED_SECURE_CAPABLE;
	} else {
		if (secstateset && crc_disabled)
			return SWITCHTEC_GEN6_INITIALIZED_SECURED;
		else
			return SWITCHTEC_GEN6_INITIALIZED_UNSECURED;
	}
}

int switchtec_secure_state_get_gen6(struct switchtec_dev *dev,
				    enum switchtec_secure_state_gen6 *state)
{
	uint32_t data;
	uint32_t reply;
	int ret;

	if (!switchtec_is_gen6(dev))
		return -ENOTSUP;

	if (!state)
		return -EINVAL;

	if (switchtec_boot_phase(dev) == SWITCHTEC_BOOT_PHASE_BL1) {
		struct sec_cfg_get_struct cmd = {};
		uint32_t reply_otp[OTP_DWORD_61 + 1] = {};

		cmd.subcmd = MRPC_GET_SECURE_OTP;
		cmd.OTP_dword_offset = 0;
		cmd.read_dwords = OTP_DWORD_61 + 1;

		ret = switchtec_mfg_cmd(dev, MRPC_SECURITY_CONFIG_GET_GEN6,
					&cmd, sizeof(cmd),
					reply_otp, sizeof(reply_otp));
		if (ret)
			return ret;

		*state = bl1_derive_state_from_otp(reply_otp);
		return 0;
	}

	data = htole32(GEN6_STATE_SET_SUBCMD_STATE_GET);

	ret = switchtec_mfg_cmd(dev, MRPC_SECURE_STATE_SET_GEN6,
				&data, sizeof(data), &reply, sizeof(reply));
	if (ret)
		return ret;

	*state = (enum switchtec_secure_state_gen6)(le32toh(reply) & 0xFF);
	return 0;
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

int switchtec_dbg_sec_ver_update_gen6(struct switchtec_dev *dev,
				      struct switchtec_pubkey *public_key,
				      struct switchtec_signature *signature,
				      struct switchtec_gen6_token *token)
{
	int ret;
	struct ver_update_cmd_gen6 {
		uint8_t subcmd;
		uint8_t rsvd[3];
		uint8_t token[SWITCHTEC_GEN6_TOKEN_VER_UPDATE_LEN];
	} cmd = {};
	uint32_t cmd_id = MRPC_DBG_UNLOCK_GEN6;

	if (!switchtec_is_gen6(dev))
		return ERR_SUBCMD_INVALID;

	ret = dbg_unlock_send_pubkey_gen6(dev, public_key, cmd_id);
	if (ret)
		return ret;

	ret = dbg_unlock_send_sig_gen6(dev, signature, cmd_id);
	if (ret)
		return ret;

	cmd.subcmd = MRPC_GEN6_DBG_SEC_VER_UPDATE;
	memcpy(cmd.token, token->token, SWITCHTEC_GEN6_TOKEN_VER_UPDATE_LEN);
	return switchtec_mfg_cmd(dev, cmd_id, &cmd, sizeof(cmd), NULL, 0);
}

/**
 * @brief Gen6 static debug token disable.
 *	  Sends pubkey + signature, then submits the signed
 *	  GEN6_DISABLE_STATIC_TOKEN to the device. Permanently disables
 *	  static debug tokens via OTP.
 */
int switchtec_dbg_sec_static_disable_gen6(struct switchtec_dev *dev,
					  struct switchtec_pubkey *public_key,
					  struct switchtec_signature *signature,
					  struct switchtec_gen6_token *token)
{
	int ret;
	struct static_disable_cmd_gen6 {
		uint8_t subcmd;
		uint8_t rsvd[3];
		uint8_t token[SWITCHTEC_GEN6_TOKEN_VER_UPDATE_LEN];
	} cmd = {};
	uint32_t cmd_id = MRPC_DBG_UNLOCK_GEN6;

	if (!switchtec_is_gen6(dev))
		return ERR_SUBCMD_INVALID;

	if (token->token[0] != SECURE_TOKEN_GET_TYPE_DISABLE_STATIC)
		return ERR_SUBCMD_INVALID;

	ret = dbg_unlock_send_pubkey_gen6(dev, public_key, cmd_id);
	if (ret)
		return ret;

	ret = dbg_unlock_send_sig_gen6(dev, signature, cmd_id);
	if (ret)
		return ret;

	cmd.subcmd = MRPC_GEN6_DBG_UNLOCK_STATIC_DISABLE;
	memcpy(cmd.token, token->token, SWITCHTEC_GEN6_TOKEN_VER_UPDATE_LEN);
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

#ifdef HAVE_DECL_PEM_READ_PUBKEY

#include <openssl/core_names.h>

/**
 * @brief Read public key from public key file
 * @param[in]  pubk_file Public key file
 * @param[out] pubk	 Public key
 * @return 0 on success, error code on failure
 */
int switchtec_read_pubk_file(FILE *pubk_file, struct switchtec_pubkey *pubk)
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

#endif /* HAVE_DECL_PEM_READ_PUBKEY */

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
 * @brief Read token data from token file
 * @param[in]  tkn_file  Token file
 * @param[out] token Token data
 * @return 0 on success, error code on failure
 */
int switchtec_read_token_file(FILE *tkn_file, struct switchtec_gen6_token *token)
{
	ssize_t rlen;

	rlen = fread(token->token, 1, SWITCHTEC_GEN6_TOKEN_MAX_LEN, tkn_file);
	if (rlen < SWITCHTEC_GEN6_TOKEN_MAX_LEN)
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

	for (key_idx = 0; key_idx < state->public_key_num; key_idx++) {
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

static int sn_ver_get_gen6(struct switchtec_dev *dev,
			   struct switchtec_sn_ver_info *info,
			   enum switchtec_boot_phase phase_id)
{
	int ret;
	uint32_t subcmd;

	subcmd = (phase_id == SWITCHTEC_BOOT_PHASE_BL1) ?
		  GEN6_SN_VER_GET_SUBCMD_BL1 : GEN6_SN_VER_GET_SUBCMD_POST_BL1;
	struct reply_t {
		uint32_t UID[16];
		uint32_t PSID0[4];
		uint32_t PSID_UID_valid_flags;
		uint32_t SVNSV_rsvrd;
		uint32_t bl2_sec_ver;
		uint32_t rsvrd;
		uint32_t mainfw_sec_ver;
		uint32_t svl1_rsvrd;
		uint32_t svl2_rsvrd;
		uint32_t dbg_tok_sec_ver_rsvrd;
		uint32_t kmt_sec_ver_rsvrd;
	} reply;

	ret = switchtec_mfg_cmd(dev, MRPC_SN_VER_GET_GEN6, &subcmd, 4,
						    &reply, sizeof(reply));
	if (ret && subcmd == GEN6_SN_VER_GET_SUBCMD_POST_BL1) {
		subcmd = GEN6_SN_VER_GET_SUBCMD_BL1;
		ret = switchtec_mfg_cmd(dev, MRPC_SN_VER_GET_GEN6, &subcmd, 4,
					&reply, sizeof(reply));
	}
	if (ret)
		return ret;

	info->UID = malloc(sizeof(reply.UID));
	if (!info->UID)
		return -1;
	info->PSID0 = malloc(sizeof(reply.PSID0));
	if (!info->PSID0) {
		free(info->UID);
		info->UID = NULL;
		return -1;
	}
	memcpy(info->UID, reply.UID, sizeof(reply.UID));
	memcpy(info->PSID0, reply.PSID0, sizeof(reply.PSID0));
	info->PSID_UID_valid_flags = reply.PSID_UID_valid_flags;
	info->ver_bl2 = reply.bl2_sec_ver;
	info->ver_main = reply.mainfw_sec_ver;
	info->dbg_tok_sec_ver_rsvrd = reply.dbg_tok_sec_ver_rsvrd;
	info->kmt_sec_ver_rsvrd = reply.kmt_sec_ver_rsvrd;

	return 0;
}

int switchtec_sn_ver_get_with_phase(struct switchtec_dev *dev,
				    struct switchtec_sn_ver_info *info,
				    enum switchtec_boot_phase phase_id)
{
	if (switchtec_is_gen6(dev))
		return sn_ver_get_gen6(dev, info, phase_id);
	else if (switchtec_is_gen5(dev))
		return sn_ver_get_gen5(dev, info);
	else
		return sn_ver_get_gen4(dev, info);
}

int switchtec_sn_ver_get(struct switchtec_dev *dev,
			 struct switchtec_sn_ver_info *info)
{
	return switchtec_sn_ver_get_with_phase(dev, info,
					       switchtec_boot_phase(dev));
}

int switchtec_device_config_get(struct switchtec_dev *dev,
				struct switchtec_device_config_dev_settings *settings)
{
	int ret;
	uint32_t subcmd = DEVICE_CONFIG_SUB_CMD_GET;

	if (!switchtec_is_gen6(dev)) {
		errno = ENOTSUP;
		return -ENOTSUP;
	}

	ret = switchtec_mfg_cmd(dev, MRPC_DEVICE_CONFIG, &subcmd, sizeof(subcmd),
				settings, sizeof(*settings));
	return ret;
}

int switchtec_device_config_get_security(struct switchtec_dev *dev,
					 struct switchtec_device_config_get_sec *config)
{
	int ret;
	uint32_t subcmd = DEVICE_CONFIG_SUB_CMD_GET_SECURITY;

	if (!switchtec_is_gen6(dev)) {
		errno = ENOTSUP;
		return -ENOTSUP;
	}

	ret = switchtec_mfg_cmd(dev, MRPC_DEVICE_CONFIG, &subcmd, sizeof(subcmd),
				config, sizeof(*config));
	return ret;
}

int switchtec_device_config_get_customer(struct switchtec_dev *dev,
					 struct switchtec_device_config_customer_settings *settings)
{
	int ret;
	uint32_t subcmd = DEVICE_CONFIG_SUB_CMD_GET_CUSTOMER;

	if (!switchtec_is_gen6(dev)) {
		errno = ENOTSUP;
		return -ENOTSUP;
	}

	ret = switchtec_mfg_cmd(dev, MRPC_DEVICE_CONFIG, &subcmd, sizeof(subcmd),
				settings, sizeof(*settings));
	return ret;
}

static int check_dev_cfg_file_header(FILE *fp, const char *expected_magic)
{
	struct dev_cfg_file_header {
		uint8_t  magic[4];
		uint32_t version;
		uint8_t  hw_gen;
		uint8_t  rsvd[3];
		uint32_t crc;
	} hdr;
	uint8_t *data;
	int data_len;
	ssize_t rlen;
	uint32_t file_crc;

	rewind(fp);
	rlen = fread(&hdr, sizeof(hdr), 1, fp);
	if (rlen != 1)
		return -EBADF;

	if (memcmp(hdr.magic, expected_magic, 4))
		return -EBADF;

	if (le32toh(hdr.version) != DEVICE_CONFIG_FILE_VERSION)
		return -EBADF;

	if (hdr.hw_gen != DEVICE_CONFIG_FILE_HW_GEN_GEN6)
		return -ENODEV;

	fseek(fp, 0, SEEK_END);
	data_len = ftell(fp);
	if (data_len < 0)
		return -EBADF;
	data_len -= (int)sizeof(hdr);

	if (fseek(fp, sizeof(hdr), SEEK_SET))
		return -EBADF;

	if (data_len <= 0)
		return -EBADF;

	if (data_len > (int)(MRPC_MAX_DATA_LEN - sizeof(uint32_t)))
		return -EBADF;

	data = malloc(data_len);
	if (!data)
		return -ENOMEM;

	rlen = fread(data, 1, data_len, fp);
	if (rlen < data_len) {
		free(data);
		return -EBADF;
	}

	file_crc = crc32(data, data_len, 0, 1, 1);
	free(data);

	if (file_crc != le32toh(hdr.crc))
		return -EBADF;

	if (fseek(fp, sizeof(hdr), SEEK_SET))
		return -EBADF;

	return 0;
}

int switchtec_read_dev_cfg_file_dev(FILE *fp,
				    struct switchtec_device_config_dev_settings *settings)
{
	int ret = check_dev_cfg_file_header(fp, DEVICE_CONFIG_FILE_MAGIC_DEV);

	if (ret)
		return ret;

	if (fread(settings, sizeof(*settings), 1, fp) != 1)
		return -EBADF;

	return 0;
}

int switchtec_read_dev_cfg_file_customer(FILE *fp,
					 struct switchtec_device_config_customer_settings *settings)
{
	int ret = check_dev_cfg_file_header(fp, DEVICE_CONFIG_FILE_MAGIC_CUSTOMER);

	if (ret)
		return ret;

	if (fread(settings, sizeof(*settings), 1, fp) != 1)
		return -EBADF;

	return 0;
}

int switchtec_read_dev_cfg_file_security(FILE *fp,
					 struct switchtec_device_config_secure_settings *settings)
{
	int ret = check_dev_cfg_file_header(fp, DEVICE_CONFIG_FILE_MAGIC_SECURITY);

	if (ret)
		return ret;

	if (fread(settings, sizeof(*settings), 1, fp) != 1)
		return -EBADF;

	return 0;
}

int switchtec_device_config_set_dev(struct switchtec_dev *dev,
				    struct switchtec_device_config_dev_settings *settings)
{
	int ret;
	struct {
		uint8_t subcmd;
		uint8_t reserved[3];
		struct switchtec_device_config_dev_settings settings;
	} cmd;

	if (!switchtec_is_gen6(dev)) {
		errno = ENOTSUP;
		return -ENOTSUP;
	}

	memset(&cmd, 0, sizeof(cmd));
	cmd.subcmd = DEVICE_CONFIG_SUB_CMD_SET_DEVICE;
	memcpy(&cmd.settings, settings, sizeof(*settings));

	ret = switchtec_mfg_cmd(dev, MRPC_DEVICE_CONFIG, &cmd, sizeof(cmd),
				NULL, 0);
	return ret;
}

int switchtec_device_config_set_customer(struct switchtec_dev *dev,
					 struct switchtec_device_config_customer_settings *settings)
{
	int ret;
	struct {
		uint8_t subcmd;
		uint8_t reserved[3];
		struct switchtec_device_config_customer_settings settings;
	} cmd;

	if (!switchtec_is_gen6(dev)) {
		errno = ENOTSUP;
		return -ENOTSUP;
	}

	memset(&cmd, 0, sizeof(cmd));
	cmd.subcmd = DEVICE_CONFIG_SUB_CMD_SET_CUSTOMER;
	memcpy(&cmd.settings, settings, sizeof(*settings));

	ret = switchtec_mfg_cmd(dev, MRPC_DEVICE_CONFIG, &cmd, sizeof(cmd),
				NULL, 0);
	return ret;
}

int switchtec_device_config_set_security(struct switchtec_dev *dev,
					 struct switchtec_device_config_secure_settings *settings)
{
	int ret;
	struct {
		uint8_t subcmd;
		uint8_t reserved[3];
		struct switchtec_device_config_secure_settings settings;
	} cmd;

	if (!switchtec_is_gen6(dev)) {
		errno = ENOTSUP;
		return -ENOTSUP;
	}

	memset(&cmd, 0, sizeof(cmd));
	cmd.subcmd = DEVICE_CONFIG_SUB_CMD_SET_SECURITY;
	memcpy(&cmd.settings, settings, sizeof(*settings));

	ret = switchtec_mfg_cmd(dev, MRPC_DEVICE_CONFIG, &cmd, sizeof(cmd),
				NULL, 0);
	return ret;
}

int switchtec_dok_config_signature(struct switchtec_dev *dev,
				   struct switchtec_dok_signature *sig)
{
	int ret;

	if (!switchtec_is_gen6(dev)) {
		errno = ENOTSUP;
		return -ENOTSUP;
	}

	ret = switchtec_mfg_cmd(dev, MRPC_DOK_CONFIG, sig,
				20 + sig->data_len, NULL, 0);
	return ret;
}

int switchtec_dok_config_key_add(struct switchtec_dev *dev,
				 struct switchtec_dok_key_add *key_add)
{
	int ret;

	if (!switchtec_is_gen6(dev)) {
		errno = ENOTSUP;
		return -ENOTSUP;
	}

	ret = switchtec_mfg_cmd(dev, MRPC_DOK_CONFIG, key_add, sizeof(*key_add),
				NULL, 0);
	return ret;
}

int switchtec_dok_config_key_revoke(struct switchtec_dev *dev,
				    struct switchtec_dok_key_revoke *key_revoke)
{
	int ret;

	if (!switchtec_is_gen6(dev)) {
		errno = ENOTSUP;
		return -ENOTSUP;
	}

	ret = switchtec_mfg_cmd(dev, MRPC_DOK_CONFIG, key_revoke,
				sizeof(*key_revoke), NULL, 0);
	return ret;
}

/**@}*/
