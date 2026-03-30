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

struct get_cfgs_reply_gen6 {
	uint16_t twi_recov_info;
	uint16_t twi_recov_addr;
	uint16_t mrpc_cmd_map;
	uint16_t rsrvd2;
	uint32_t ap_offset;
	uint64_t i3c_pid_reserved;
	uint8_t i3c_recovery_address_reserved;
	uint8_t i3c_bus_reserved;
	uint16_t rsvrd3;
	uint8_t algo_engine_disable_reserved;
	uint8_t bootrom_key_revoke_reserved;
	uint8_t boot_recovery_failover_disable;
	uint8_t token_disable_reserved;
	uint8_t puf_ac_status_reserved;
	uint8_t puf_ac_read_mask_reserved;
	uint8_t puf_ac_read_mask_request_reserved;
	uint8_t otp_key_hash_status;
	uint8_t otp_hash_key_read_mask;
	uint8_t otp_hash_key_read_mask_request;
	uint8_t hash_table_disable_reserved;
	uint8_t reserved2;
	uint32_t otp_key_hash[SWITCHTEC_KMSK_NUM_GEN6][SWITCHTEC_KMSK_LEN_DWORDS];
};

static int get_configs_gen6(struct switchtec_dev *dev,
			    struct get_cfgs_reply_gen6 *cfgs)
{
	uint32_t subcmd = 0;

	return switchtec_mfg_cmd(dev,
				 MRPC_SECURE_CONFIG_GET_GEN6,
				 &subcmd, sizeof(subcmd),
				 cfgs, sizeof(struct get_cfgs_reply_gen6));

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

int switchtec_security_config_get_gen6(struct switchtec_dev *dev,
				       struct switchtec_security_cfg_state *state)
{
	int ret;
	struct get_cfgs_reply_gen6 reply;

	ret = get_configs_gen6(dev, &reply);
	if (ret)
		return ret;
	state->i2c_port = (reply.twi_recov_info >> SECURE_CFG_GET_I2C_PORT_LSB) 
			   & SECURE_CFG_GET_I2C_PORT_MSK;
	state->i2c_addr = ((reply.twi_recov_info & SECURE_CFG_GET_I2C_ADDR_MSK) == 0) ? 
			   (SECIRE_CFG_GET_I2C) : (reply.twi_recov_info & SECURE_CFG_GET_I2C_ADDR_MSK);
	state->i2c_cmd_map = reply.mrpc_cmd_map & SECURE_CFG_GET_I2C_CMD_MAP_MSK;
	state->secsc = (reply.mrpc_cmd_map >> SECURE_CFG_GET_I2C_CMD_MAP_LSB) & 0x1;
	state->i2c_rcvry_address_ocp = ((reply.twi_recov_info & SECURE_CFG_GET_I2C_RCVRY_INF_MSK) | 
					(reply.twi_recov_addr & SECURE_CFG_GET_I2C_RCVRY_ADDR_MSK) ? (SECURE_CFG_GET_OCP) : 
					((reply.twi_recov_info & SECURE_CFG_GET_I2C_RCVRY_INF_MSK) | 
					(reply.twi_recov_addr & SECURE_CFG_GET_I2C_RCVRY_ADDR_MSK)));
	memcpy(state->otp_key_hash, reply.otp_key_hash, 
		SWITCHTEC_KMSK_NUM_GEN6 * SWITCHTEC_KMSK_LEN);

	return 0;
}

int switchtec_active_image_index_get_gen6(struct switchtec_dev *dev,
					  void *index)
{
	/* Gen6 doesn't support active image index - return error */
	errno = ENOTSUP;
	return -1;
}

int switchtec_active_image_index_set_gen6(struct switchtec_dev *dev,
					  void *index)
{
	/* Gen6 doesn't support active image index - return error */
	errno = ENOTSUP;
	return -1;
}

int switchtec_fw_exec_gen6(struct switchtec_dev *dev, enum switchtec_bl2_recovery_mode recovery_mode)
{
	uint32_t cmd_id = MRPC_FW_TX_GEN6;
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

int switchtec_boot_resume_gen6(struct switchtec_dev *dev)
{
	/* Gen6 doesn't support boot resume - return error */
	errno = ENOTSUP;
	return -1;
}

int switchtec_secure_state_set_gen6(struct switchtec_dev *dev, int state)
{
	/* Gen6 doesn't support - return error */
	errno = ENOTSUP;
	return -1;
}

int switchtec_kmsk_set_gen6(struct switchtec_dev *dev, void *public_key,
			    void *signature, struct switchtec_kmsk *kmsk)
{
	/* Gen6 doesn't support - return error */
	errno = ENOTSUP;
	return -1;
}

int switchtec_debug_unlock_gen6(struct switchtec_dev *dev, uint32_t serial,
				uint32_t ver_sec_unlock, void *public_key,
				struct switchtec_signature *signature,
				struct switchtec_gen6_token *token)
{
	int ret;

	struct unlock_cmd_gen6 {
		uint8_t subcmd;
		uint8_t rsvd[3];
		uint8_t token[SWITCHTEC_GEN6_TOKEN_LEN];
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
	memcpy(cmd.token, token->token, SWITCHTEC_GEN6_TOKEN_LEN);
	
	return switchtec_mfg_cmd(dev, cmd_id, &cmd, sizeof(cmd), NULL, 0);
}

int switchtec_dbg_unlock_get_token_gen6(struct switchtec_dev *dev,
			struct switchtec_gen6_token *token,
			int token_type)
{
	int ret;

	struct get_unlock_token_cmd_gen6 {
		uint8_t subcmd;
		uint8_t token_type;
		uint8_t rsvd[2];
	} cmd = {};
	
	uint32_t cmd_id;
	cmd_id = MRPC_DBG_UNLOCK_GEN6;

	struct get_unlock_token_reply_gen6 {
		uint8_t token[SWITCHTEC_GEN6_TOKEN_LEN];
	} reply;

	cmd.subcmd = MRPC_GEN6_DBG_UNLOCK_TOKEN_GET;
	
	if(token_type == GEN6_TOKEN_STATIC)
		cmd.token_type = SECURE_TOKEN_GET_TYPE_STATIC;
	else
		cmd.token_type = SECURE_TOKEN_GET_TYPE_EPHEMERAL;
	
	ret = switchtec_mfg_cmd(dev, cmd_id, &cmd, sizeof(cmd), &reply, 
				sizeof(reply));
	if (ret)
		return ret;

	memcpy(&token->token, &reply, SWITCHTEC_GEN6_TOKEN_LEN);
	return 0;
}

int switchtec_security_settings_get_gen6(struct switchtec_dev *dev,
				struct switchtec_security_cfg_state *state)
{
	int ret;

	struct sec_cfg_get_struct cmd = {};
	uint32_t reply_otp[MRPC_MAX_DATA_LEN / sizeof(uint32_t)] = {};

	/* get first 60dwords of OTP content */
	cmd.subcmd = MRPC_GET_SECURE_OTP;
	cmd.OTP_dword_offset = 0;
	cmd.read_dwords = 60;

	cmd.subcmd = MRPC_GET_SECURE_OTP;
	ret = switchtec_mfg_cmd(dev,MRPC_SECURITY_CONFIG_GET_GEN6, &cmd, sizeof(cmd),
				 			&reply_otp, cmd.read_dwords * sizeof(uint32_t));
	if (ret)
		return ret;
	
	state->i2c_cmd_map = (reply_otp[OTP_DWORD_10] & OTP_DWORD_10_SMBUS_SMBRMRPCADDR_MSK) 
						  >> OTP_DWORD_10_SMBUS_SMBRMRPCADDR_LSB;
	state->i2c_port = (reply_otp[OTP_DWORD_10] & OTP_DWORD_10_SMBUS_SMBRIF_MSK) 
					   >> OTP_DWORD_10_SMBUS_SMBRIF_LSB;
	state->i2c_addr = (reply_otp[OTP_DWORD_10] & OTP_DWORD_10_SMBUS_SMBRATYPE_MSK) 
					   >> OTP_DWORD_10_SMBUS_SMBRATYPE_LSB;
	state->i2c_rcvry_address_ocp = (reply_otp[OTP_DWORD_10] & OTP_DWORD_10_SMBUS_SMBROCPADDR_MSK) 
									>> OTP_DWORD_10_SMBUS_SMBROCPADDR_LSB;
	state->secsc = (reply_otp[OTP_DWORD_0] & OTP_DWORD_0_PRODUCT_SECSC_MSK) 
					>> OTP_DWORD_0_PRODUCT_SECSC_LSB;

	/* get 192 dwords of OTP content from offset 656 for keys*/
	cmd.subcmd = MRPC_GET_SECURE_OTP;
	cmd.OTP_dword_offset = OTP_MULTI_DWORD_IMAGE_BIAK0;
	cmd.read_dwords = (SWITCHTEC_KMSK_NUM_GEN6 * SWITCHTEC_KMSK_LEN_DWORDS);

	cmd.subcmd = MRPC_GET_SECURE_OTP;
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

int switchtec_read_token_file_gen6(FILE *tkn_file, struct switchtec_gen6_token *token)
{
	ssize_t rlen;

	rlen = fread(token->token, 1, SWITCHTEC_GEN6_TOKEN_LEN, tkn_file);
	if (rlen < SWITCHTEC_GEN6_TOKEN_LEN)
		return -EBADF;

	return 0;
}

#endif /* __linux__ */

int switchtec_sn_ver_get_gen6(struct switchtec_dev *dev, struct switchtec_sn_ver_info *info)
{
	int ret;
	uint32_t subcmd = 0;
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
	if (ret)
		return ret;

	info->UID = malloc(sizeof(reply.UID));
	info->PSID0 = malloc(sizeof(reply.PSID0));
	memcpy(info->UID, reply.UID, sizeof(reply.UID));
	memcpy(info->PSID0, reply.PSID0, sizeof(reply.PSID0));
	info->PSID_UID_valid_flags = reply.PSID_UID_valid_flags;
	info->ver_bl2 = reply.bl2_sec_ver;
	info->ver_main = reply.mainfw_sec_ver;
	info->dbg_tok_sec_ver_rsvrd = reply.dbg_tok_sec_ver_rsvrd;
	info->kmt_sec_ver_rsvrd = reply.kmt_sec_ver_rsvrd;

	return 0;
}