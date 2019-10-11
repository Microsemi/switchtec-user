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
#include "switchtec/recovery.h"
#include "switchtec/errors.h"
#include "switchtec/endian.h"
#include "switchtec/mrpc.h"
#include <unistd.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

//#include <openssl/err.h>
//#include <openssl/evp.h>
//#include <openssl/rsa.h>
#include <openssl/pem.h>

#include "lib/crc32.h"

#define SWITCHTEC_ACTV_IMG_ID_KMAN		1
#define SWITCHTEC_ACTV_IMG_ID_BL2		2
#define SWITCHTEC_ACTV_IMG_ID_CFG		3
#define SWITCHTEC_ACTV_IMG_ID_FW		4

#define SWITCHTEC_MB_MAX_ENTRIES		16

#define SWITCHTEC_ACTV_IDX_MAX_ENTRIES		32
#define SWITCHTEC_ACTV_IDX_SET_ENTRIES		4

#define SWITCHTEC_KMSK_MAX_ALLOWED_ENTRIES	1

#define SWITCHTEC_CLK_RATE_BITSHIFT		10
#define SWITCHTEC_RC_TMO_BITSHIFT		14
#define SWITCHTEC_I2C_PORT_BITSHIFT		18
#define SWITCHTEC_I2C_ADDR_BITSHIFT		22
#define SWITCHTEC_CMD_MAP_BITSHIFT		29

#if OPENSSL_VERSION_NUMBER < 0x10100000L
/**
*  openssl1.0 or older versions don't have this function, so copy
*  the code from openssl1.1 here
*/
static void RSA_get0_key(const RSA *r, const BIGNUM **n,
			 const BIGNUM **e, const BIGNUM **d)
{
	if(n != NULL)
		*n = r->n;
	if(e != NULL)
		*e = r->e;
	if(d != NULL)
		*d = r->d;
}
#endif

/**
 * @brief Ping a device with a 32-bit data.
 * @param[in]  dev	Switchtec device handle
 * @param[in]  ping_dw	Ping data
 * @param[out] reply_dw	Ping reply data (2's complement of ping data)
 * @param[out] image_id	The current running image
 * @return 0 on success, error code on failure
 */
int switchtec_ping(struct switchtec_dev *dev,
		   uint32_t ping_dw,
		   uint32_t *reply_dw)
{
	int ret;
	struct ping_reply {
		uint32_t phase;
		uint32_t reply;
	} r;

	ret = switchtec_cmd(dev, MRPC_PING, &ping_dw,
			sizeof(ping_dw),
			&r, sizeof(r));
	if(ret != 0)
		return ret;

	*reply_dw = r.reply;

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
	uint32_t t = 0;
	struct ping_reply {
		uint32_t phase;
		uint32_t reply;
	} r;

	ret = switchtec_cmd(dev, MRPC_PING, &t,
			sizeof(t),
			&r, sizeof(r));
	if(ret != 0)
		return ret;

	*phase_id = le32toh(r.phase);

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
	int ret;

	ret = switchtec_cmd(dev, MRPC_SN_VER_GET, NULL, 0,
			info,
			sizeof(struct switchtec_sn_ver_info));
	if(ret)
		return ret;

	info->chip_serial = le32toh(info->chip_serial);
	info->ver_bl2 = le32toh(info->ver_bl2);
	info->ver_km = le32toh(info->ver_km);
	info->ver_main = le32toh(info->ver_main);
	info->ver_sec_unlock = le32toh(info->ver_sec_unlock);

	return 0;
}

/**
 * @brief Get secure boot configurations
 * @param[in]  dev	Switchtec device handle
 * @param[out] state	Current secure boot settings
 * @return 0 on success, error code on failure
 */
int switchtec_security_config_get(struct switchtec_dev *dev,
				  struct switchtec_security_cfg_stat *state)
{
	int ret;
	struct cfg_reply {
		uint32_t valid;
		uint32_t rsvd1;
		uint64_t cfg;
		uint32_t  public_key_exponent;
		uint8_t  rsvd2;
		uint8_t  public_key_num;
		uint8_t  public_key_ver;
		uint8_t  rsvd3;
		uint8_t  public_key[SWITCHTEC_KMSK_NUM]\
					 [SWITCHTEC_KMSK_LEN];
		uint8_t  rsvd4[32];
	} r;

	ret = switchtec_cmd(dev, MRPC_SECURITY_CONFIG_GET, NULL, 0,
			    &r, sizeof(r));
	if(ret)
		return ret;

	r.valid = le32toh(r.valid);
	r.cfg = le64toh(r.cfg);
	r.public_key_exponent = le32toh(r.public_key_exponent);

	state->basic_setting_valid = !!(r.valid & 0x01);
	state->public_key_exp_valid = !!(r.valid & 0x02);
	state->public_key_num_valid = !!(r.valid & 0x04);
	state->public_key_ver_valid = !!(r.valid & 0x08);
	state->public_key_valid = !!(r.valid & 0x10);

	state->debug_mode = r.cfg & 0x03;
	state->secure_state = (r.cfg>>2) & 0x03;

	state->jtag_lock_after_reset = !!(r.cfg & 0x40);
	state->jtag_lock_after_bl1 = !!(r.cfg & 0x80);
	state->jtag_bl1_unlock_allowed = !!(r.cfg & 0x0100);
	state->jtag_post_bl1_unlock_allowed = !!(r.cfg & 0x0200);

	state->spi_clk_rate = (r.cfg >> SWITCHTEC_CLK_RATE_BITSHIFT) & 0x0f;
	if(state->spi_clk_rate == 0)
		state->spi_clk_rate = SWITCHTEC_SPI_RATE_25M;

	state->i2c_recovery_tmo = (r.cfg >> SWITCHTEC_RC_TMO_BITSHIFT) & 0x0f;

	state->i2c_port = (r.cfg >> SWITCHTEC_I2C_PORT_BITSHIFT) & 0xf;

	state->i2c_addr = (r.cfg >> SWITCHTEC_I2C_ADDR_BITSHIFT) & 0x7f;

	state->i2c_cmd_map = (r.cfg >> SWITCHTEC_CMD_MAP_BITSHIFT) & 0xfff;

	state->public_key_exponent = r.public_key_exponent;

	state->public_key_num = r.public_key_num;
	state->public_key_ver = r.public_key_ver;
	memcpy(state->public_key, r.public_key,
		SWITCHTEC_KMSK_NUM * SWITCHTEC_KMSK_LEN);

	return 0;
}

/**
 * @brief Set secure boot settings
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
	uint64_t L = 0;

	memset(&sd, 0, sizeof(sd));

	sd.cfg |= setting->jtag_lock_after_reset? 0x40 : 0;
	sd.cfg |= setting->jtag_lock_after_bl1? 0x80 : 0;
	sd.cfg |= setting->jtag_bl1_unlock_allowed? 0x0100 : 0;
	sd.cfg |= (setting->jtag_post_bl1_unlock_allowed? 0x0200 : 0);

	sd.cfg |= (setting->spi_clk_rate & 0x0f)
			<< SWITCHTEC_CLK_RATE_BITSHIFT;

	sd.cfg |= (setting->i2c_recovery_tmo & 0x0f)
			<< SWITCHTEC_RC_TMO_BITSHIFT;
	sd.cfg |= (setting->i2c_port & 0x0f)
			<< SWITCHTEC_I2C_PORT_BITSHIFT;
	sd.cfg |= (setting->i2c_addr & 0x7f)
			<< SWITCHTEC_I2C_ADDR_BITSHIFT;

	L = setting->i2c_cmd_map & 0xfff;
	L <<= SWITCHTEC_CMD_MAP_BITSHIFT;
	sd.cfg |= L;

	sd.cfg = htole64(sd.cfg);

	sd.pub_key_exponent = htole32(setting->public_key_exponent);

	ret = switchtec_cmd(dev, MRPC_SECURITY_CONFIG_SET, &sd, sizeof(sd),
			NULL,
			0);
	return ret;
}

/**
 * @brief Retrieve mailbox entries
 * @param[in]  dev	Switchtec device handle
 * @param[in]  fd	File handle to write the log data
 * @return 0 on success, error code on failure
 */
int switchtec_mailbox_get(struct switchtec_dev *dev, int fd)
{
	int ret;
	int num_to_read = htole32(SWITCHTEC_MB_MAX_ENTRIES);
	struct mb_reply {
		uint8_t num_returned;
		uint8_t num_remaining;
		uint8_t rsvd[2];
		uint8_t data[SWITCHTEC_MB_MAX_ENTRIES*
				   SWITCHTEC_MB_LOG_LEN];
	} r;

	do {
		ret = switchtec_cmd(dev, MRPC_MAILBOX_GET, &num_to_read,
					sizeof(int), &r,  sizeof(r));
		if(ret)
			return ret;

		r.num_remaining = le32toh(r.num_remaining);
		r.num_returned = le32toh(r.num_returned);

		ret = write(fd, r.data, (r.num_returned) * SWITCHTEC_MB_LOG_LEN);
		if (ret < 0)
			return ret;
	}
	while(r.num_remaining > 0);

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
	struct active_indices {
		uint8_t index[SWITCHTEC_ACTV_IDX_MAX_ENTRIES];
	} idx;

	int ret;

	ret = switchtec_cmd(dev, MRPC_ACT_IMG_IDX_GET, NULL, 0, &idx, sizeof(idx));
	if(ret)
		return ret;

	index->keyman = idx.index[SWITCHTEC_ACTV_IMG_ID_KMAN];
	index->bl2 = idx.index[SWITCHTEC_ACTV_IMG_ID_BL2];
	index->config = idx.index[SWITCHTEC_ACTV_IMG_ID_CFG];
	index->firmware = idx.index[SWITCHTEC_ACTV_IMG_ID_FW];

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
	} t;

	memset(&t, 0, sizeof(t));

	if(index->keyman != SWITCHTEC_ACTIVE_INDEX_NOT_SET) {
		t.idx[i].image_id = SWITCHTEC_ACTV_IMG_ID_KMAN;
		t.idx[i].index = index->keyman;
		i++;
	}

	if(index->bl2 != SWITCHTEC_ACTIVE_INDEX_NOT_SET) {
		t.idx[i].image_id = SWITCHTEC_ACTV_IMG_ID_BL2;
		t.idx[i].index = index->bl2;
		i++;
	}

	if(index->config != SWITCHTEC_ACTIVE_INDEX_NOT_SET) {
		t.idx[i].image_id =  SWITCHTEC_ACTV_IMG_ID_CFG;
		t.idx[i].index = index->config;
		i++;
	}

	if(index->firmware != SWITCHTEC_ACTIVE_INDEX_NOT_SET) {
		t.idx[i].image_id = SWITCHTEC_ACTV_IMG_ID_FW;
		t.idx[i].index = index->firmware;
		i++;
	}
	t.count = htole32(i);

	if(i == 0)
		return 0;

	ret = switchtec_cmd(dev, MRPC_ACT_IMG_IDX_SET, &t,
			    sizeof(t), NULL, 0);
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
	int ret;
	struct fw_exec_struct {
		uint8_t subcmd;
		uint8_t recovery_mode;
		uint8_t rsvd[2];
	} s;

	memset(&s, 0, sizeof(s));
	s.subcmd = MRPC_FW_TX_EXEC;
	s.recovery_mode = recovery_mode;

	ret = switchtec_cmd(dev, MRPC_FW_TX, &s, sizeof(s), NULL, 0);
	return ret;
}

/**
 * @brief Set KMSK entry
 * @param[in]  dev		Switchtec device handle
 * @param[in]  public_key	Public key
 * @param[in]  public_key_exp	Public key exponent
 * @param[in]  signature	Signature
 * @param[in]  kmsk_entry_data	KMSK entry data
 * @return 0 on success, error code on failure
 */
int switchtec_kmsk_set(struct switchtec_dev *dev,
		       uint8_t *public_key,
		       uint32_t public_key_exp,
		       uint8_t *signature,
		       uint8_t *kmsk_entry_data)
{
	int ret;
	struct kmsk_cmd1 {
		uint8_t subcmd;
		uint8_t reserved[3];
		uint8_t pub_key[SWITCHTEC_PUB_KEY_LEN];
		uint32_t pub_key_exponent;
	} cmd1;

	struct kmsk_cmd2 {
		uint8_t subcmd;
		uint8_t reserved[3];
		uint8_t signature[SWITCHTEC_SIG_LEN];
	} cmd2;

	struct kmsk_cmd3 {
		uint8_t subcmd;
		uint8_t num_entries;
		uint8_t reserved[2];
		uint8_t kmsk[SWITCHTEC_KMSK_LEN];
	} cmd3;

	if(public_key) {
		memset(&cmd1, 0, sizeof(cmd1));
		cmd1.subcmd = MRPC_KMSK_ENTRY_SET_PKEY;
		memcpy(cmd1.pub_key, public_key, SWITCHTEC_PUB_KEY_LEN);
		cmd1.pub_key_exponent = htole32(public_key_exp);

		ret = switchtec_cmd(dev, MRPC_KMSK_ENTRY_SET, &cmd1,
				    sizeof(cmd1), NULL, 0);
		if(ret) {
			return ret;
		}
	}

	if(signature) {
		memset(&cmd2, 0, sizeof(cmd2));
		cmd2.subcmd = MRPC_KMSK_ENTRY_SET_SIG;
		memcpy(cmd2.signature, signature, SWITCHTEC_SIG_LEN);

		ret = switchtec_cmd(dev, MRPC_KMSK_ENTRY_SET, &cmd2,
			    	    sizeof(cmd2), NULL, 0);
		if(ret) {
			return ret;
		}
	}

	memset(&cmd3, 0, sizeof(cmd3));
	cmd3.subcmd = MRPC_KMSK_ENTRY_SET_KMSK;
	cmd3.num_entries = 1;
	memcpy(cmd3.kmsk, kmsk_entry_data, SWITCHTEC_KMSK_LEN);

	ret = switchtec_cmd(dev, MRPC_KMSK_ENTRY_SET, &cmd3, sizeof(cmd3),
			    NULL, 0);
	return ret;
}

/**
 * @brief Set chip secure state
 * @param[in]  dev	Switchtec device handle
 * @param[in]  state	Secure state
 * @return 0 on success, error code on failure
 */
int switchtec_secure_state_set(struct switchtec_dev *dev,
			       enum switchtec_secure_state state)
{
	int ret;
	uint32_t d;

	if((state != SWITCHTEC_INITIALIZED_UNSECURED)
	   && (state != SWITCHTEC_INITIALIZED_SECURED)) {

		return ERR_PARAM_INVALID;
	}
	d = htole32(state);

	ret = switchtec_cmd(dev, MRPC_SECURE_STATE_SET, &d, sizeof(d),
			    NULL, 0);
	return ret;
}

/**
 * @brief Resume device boot
 * @param[in]  dev	Switchtec device handle
 * @return 0 on success, error code on failure
 */
int switchtec_boot_resume(struct switchtec_dev *dev)
{
	int ret;

	ret = switchtec_cmd(dev, MRPC_BOOTUP_RESUME, NULL, 0,
			    NULL, 0);
	return ret;
}

/**
 * @brief Unlock debug port
 * @param[in]  dev		Switchtec device handle
 * @param[in]  serial		Device serial number
 * @param[in]  ver_sec_unlock	Secure unlock version
 * @param[in]  public_key	public key data
 * @param[in]  public_key_exp	public key exponent
 * @param[in]  signature	Signature of data sent
 * @return 0 on success, error code on failure
 */
int switchtec_dport_unlock(struct switchtec_dev *dev,
			   uint32_t serial,
			   uint32_t ver_sec_unlock,
			   uint8_t *public_key,
			   uint32_t public_key_exp,
			   uint8_t *signatue)
{
	int ret;
	struct unlock_cmd1 {
		uint32_t subcmd;
		uint8_t pub_key[SWITCHTEC_PUB_KEY_LEN];
		uint32_t pub_key_exp;
	} cmd1;

	struct unlock_cmd2 {
		uint32_t subcmd;
		uint32_t serial;
		uint32_t unlock_ver;
		uint8_t signature[SWITCHTEC_SIG_LEN];
	} cmd2;

	memset(&cmd1, 0, sizeof(cmd1));
	cmd1.subcmd = htole32(MRPC_DPORT_UNLOCK_PKEY);
	memcpy(cmd1.pub_key, public_key, SWITCHTEC_PUB_KEY_LEN);
	cmd1.pub_key_exp = htole32(public_key_exp);

	ret = switchtec_cmd(dev, MRPC_DPORT_UNLOCK, &cmd1,
			    sizeof(cmd1), NULL, 0);
	if(ret)
		return ret;

	memset(&cmd2, 0, sizeof(cmd2));
	cmd2.subcmd = htole32(MRPC_DPORT_UNLOCK_DATA);
	cmd2.serial = htole32(serial);
	cmd2.unlock_ver = htole32(ver_sec_unlock);
	memcpy(cmd2.signature, signatue, SWITCHTEC_SIG_LEN);

	ret = switchtec_cmd(dev, MRPC_DPORT_UNLOCK, &cmd2,
			    sizeof(cmd2), NULL, 0);
	return ret;
}

/**
 * @brief Update secure unlock version
 * @param[in]  dev		Switchtec device handle
 * @param[in]  serial		Device serial number
 * @param[in]  ver_sec_unlock	New secure unlock version
 * @param[in]  public_key	public key data
 * @param[in]  public_key_exp	public key exponent
 * @param[in]  signature	Signature of data sent
 * @return 0 on success, error code on failure
 */
int switchtec_secure_unlock_version_update(struct switchtec_dev *dev,
					   uint32_t serial,
					   uint32_t ver_sec_unlock,
					   uint8_t *public_key,
					   uint32_t public_key_exp,
					   uint8_t *signatue)
{
	int ret;
	struct update_cmd1 {
		uint32_t subcmd;
		uint8_t pub_key[SWITCHTEC_PUB_KEY_LEN];
		uint32_t pub_key_exponent;
	} cmd1;

	struct update_cmd2 {
		uint32_t subcmd;
		uint32_t serial;
		uint32_t unlock_ver;
		uint8_t signature[SWITCHTEC_SIG_LEN];
	} cmd2;

	memset(&cmd1, 0, sizeof(cmd1));
	cmd1.subcmd = htole32(MRPC_DPORT_UNLOCK_PKEY);
	memcpy(cmd1.pub_key, public_key, SWITCHTEC_PUB_KEY_LEN);
	cmd1.pub_key_exponent = htole32(public_key_exp);

	ret = switchtec_cmd(dev, MRPC_DPORT_UNLOCK, &cmd1, sizeof(cmd1),
			    NULL, 0);
	if(ret)
		return ret;

	memset(&cmd2, 0, sizeof(cmd2));
	cmd2.subcmd = htole32(MRPC_DPORT_UNLOCK_UPDATE);
	cmd2.serial = htole32(serial);
	cmd2.unlock_ver = htole32(ver_sec_unlock);
	memcpy(cmd2.signature, signatue, SWITCHTEC_SIG_LEN);

	ret = switchtec_cmd(dev, MRPC_DPORT_UNLOCK, &cmd2, sizeof(cmd2),
			    NULL, 0);
	return ret;
}

/**
 * @brief Read public key from public key file
 * @param[in]  pubk_file Public key file
 * @param[out] pubk	 Public key
 * @param[out] exp	 Public key exponent
 * @return 0 on success, error code on failure
 */
int switchtec_read_pubk_file(FILE *pubk_file, uint8_t *pubk,
			     uint32_t *exp)
{
	RSA	     *RSAKey = NULL;
	const BIGNUM    *modulus_bn;
	const BIGNUM    *exponent_bn;
	uint32_t    exponent_tmp = 0;

	RSAKey = PEM_read_RSA_PUBKEY(pubk_file, NULL, NULL, NULL);
	if(RSAKey == NULL) {
		return -1;
	}

	RSA_get0_key(RSAKey, &modulus_bn, &exponent_bn, NULL);

	BN_bn2bin(modulus_bn, pubk);
	BN_bn2bin(exponent_bn, (uint8_t *)&exponent_tmp);

	*exp = be32toh(exponent_tmp);
	RSA_free(RSAKey);

	return 0;
}

/**
 * @brief Read KMSK data from KMSK file
 * @param[in]  kmsk_file KMSK file
 * @param[out] kmsk   	 KMSK entry data
 * @return 0 on success, error code on failure
 */
int switchtec_read_kmsk_file(FILE *kmsk_file, uint8_t *kmsk)
{
	ssize_t rlen;
	struct kmsk_struct {
		uint8_t magic[4];
		uint32_t version;
		uint32_t reserved;
		uint32_t crc32;
		uint8_t kmsk[SWITCHTEC_KMSK_LEN];
	} s;

	char magic[4] = {'K', 'M', 'S', 'K'};
	uint32_t crc;

	rlen = fread(&s, 1, sizeof(s), kmsk_file);

	if(rlen < sizeof(s))
		return SWITCHTEC_KMSK_FILE_ERROR_LEN;

	if(memcmp(s.magic, magic, sizeof(magic)))
		return SWITCHTEC_KMSK_FILE_ERROR_SIG;

	crc = pmc_crc32(s.kmsk, SWITCHTEC_KMSK_LEN, 0, 1, 1);
	if(crc != le32toh(s.crc32))
		return SWITCHTEC_KMSK_FILE_ERROR_CRC;

	memcpy(kmsk, s.kmsk, SWITCHTEC_KMSK_LEN);

	return 0;
}

/**
 * @brief Read security settings from config file
 * @param[in]  setting_file	Security setting file
 * @param[out] s		Security settings
 * @return 0 on success, error code on failure
 */
int switchtec_read_sec_cfg_file(FILE *setting_file,
			       	struct switchtec_security_cfg_set *s)
{
	ssize_t rlen;
	char magic[4] = {'S', 'S', 'F', 'F'};
	uint32_t crc;
	struct setting_file_header {
		uint8_t magic[4];
		uint32_t version;
		uint32_t rsvd;
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
	} t;

	rlen = fread(&t, 1, sizeof(t), setting_file);

	if(rlen < sizeof(t))
		return SWITCHTEC_SETTING_FILE_ERROR_LEN;

	if(memcmp(t.header.magic, magic, sizeof(magic)))
		return SWITCHTEC_SETTING_FILE_ERROR_SIG;

	crc = pmc_crc32((uint8_t*)&t.data, sizeof(t.data), 0, 1, 1);
	if(crc != le32toh(t.header.crc))
		return SWITCHTEC_SETTING_FILE_ERROR_CRC;

	memset(s, 0, sizeof(struct switchtec_security_cfg_set));

	t.data.cfg = le64toh(t.data.cfg);

	s->jtag_lock_after_reset = !!(t.data.cfg & 0x40);
	s->jtag_lock_after_bl1 = !!(t.data.cfg & 0x80);
	s->jtag_bl1_unlock_allowed = !!(t.data.cfg & 0x0100);
	s->jtag_post_bl1_unlock_allowed = !!(t.data.cfg & 0x0200);

	s->spi_clk_rate = (t.data.cfg >> SWITCHTEC_CLK_RATE_BITSHIFT) & 0x0f;
	s->i2c_recovery_tmo = (t.data.cfg >> SWITCHTEC_RC_TMO_BITSHIFT) & 0x0f;
	s->i2c_port = (t.data.cfg >> SWITCHTEC_I2C_PORT_BITSHIFT) & 0x0f;
	s->i2c_addr = (t.data.cfg >> SWITCHTEC_I2C_ADDR_BITSHIFT) & 0x7f;
	s->i2c_cmd_map = (t.data.cfg >> SWITCHTEC_CMD_MAP_BITSHIFT) & 0x0fff;

	s->public_key_exponent = le32toh(t.data.pub_key_exponent);

	return 0;
}
