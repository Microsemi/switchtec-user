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

#ifndef LIBSWITCHTEC_MFG_H
#define LIBSWITCHTEC_MFG_H

#define SWITCHTEC_MB_LOG_LEN	32

#define SWITCHTEC_PUB_KEY_LEN	512
#define SWITCHTEC_SIG_LEN	512
#define SWITCHTEC_UDS_LEN	32
#define SWITCHTEC_KMSK_LEN	64
#define SWITCHTEC_KMSK_NUM_MAX	10

#define SWITCHTEC_SECURITY_SPI_RATE_MAX_NUM	16

struct switchtec_sn_ver_info {
	uint32_t chip_serial;
	uint32_t ver_km;
	uint32_t ver_bl2;
	uint32_t ver_main;
	uint32_t ver_sec_unlock;
	bool riot_ver_valid;
	uint32_t ver_riot;
};
enum switchtec_debug_mode {
	SWITCHTEC_DEBUG_MODE_ENABLED,
	SWITCHTEC_DEBUG_MODE_DISABLED_BUT_ENABLE_ALLOWED,
	SWITCHTEC_DEBUG_MODE_DISABLED,
	SWITCHTEC_DEBUG_MODE_DISABLED_EXT
};

enum switchtec_secure_state {
	SWITCHTEC_UNINITIALIZED_UNSECURED,
	SWITCHTEC_INITIALIZED_UNSECURED,
	SWITCHTEC_INITIALIZED_SECURED,
	SWITCHTEC_SECURE_STATE_UNKNOWN = 0xff,
};

enum switchtec_attestation_mode {
	SWITCHTEC_ATTESTATION_MODE_NOT_SUPPORTED,
	SWITCHTEC_ATTESTATION_MODE_NONE,
	SWITCHTEC_ATTESTATION_MODE_DICE
};

/**
 * @brief Flag which indicates if an OTP region is programmable or not
 */
enum switchtec_otp_program_status {
	SWITCHTEC_OTP_PROGRAMMABLE = 0,
	SWITCHTEC_OTP_UNPROGRAMMABLE = 1,
};

enum switchtec_otp_program_mask {
	SWITCHTEC_OTP_UNMASKED = 0,
	SWITCHTEC_OTP_MASKED = 1,
};

struct switchtec_security_cfg_otp_region {
	bool basic_valid;
	bool mixed_ver_valid;
	bool main_fw_ver_valid;
	bool sec_unlock_ver_valid;
	bool kmsk_valid[4];
	enum switchtec_otp_program_status basic;
	enum switchtec_otp_program_status mixed_ver;
	enum switchtec_otp_program_status main_fw_ver;
	enum switchtec_otp_program_status sec_unlock_ver;
	enum switchtec_otp_program_status kmsk[4];
};

struct switchtec_security_cfg_otp_region_ext {
	bool basic_valid;
	bool debug_mode_valid;
	bool key_ver_valid;
	bool rc_ver_valid;
	bool bl2_ver_valid;
	bool main_fw_ver_valid;
	bool sec_unlock_ver_valid;
	bool kmsk_valid[10];
	bool cdi_efuse_inc_mask_valid;
	bool uds_valid;
	bool uds_mask_valid;
	bool mchp_uds_valid;
	bool mchp_uds_mask_valid;
	bool did_cert0_valid;
	bool did_cert1_valid;
	enum switchtec_otp_program_status basic;
	enum switchtec_otp_program_status debug_mode;
	enum switchtec_otp_program_status key_ver;
	enum switchtec_otp_program_status rc_ver;
	enum switchtec_otp_program_status bl2_ver;
	enum switchtec_otp_program_status main_fw_ver;
	enum switchtec_otp_program_status sec_unlock_ver;
	enum switchtec_otp_program_status kmsk[10];
	enum switchtec_otp_program_status cdi_efuse_inc_mask;
	enum switchtec_otp_program_status uds;
	enum switchtec_otp_program_mask   uds_mask;
	enum switchtec_otp_program_status mchp_uds;
	enum switchtec_otp_program_mask   mchp_uds_mask;
	enum switchtec_otp_program_status did_cert0;
	enum switchtec_otp_program_status did_cert1;
};

struct switchtec_attestation_state {
	enum switchtec_attestation_mode attestation_mode;
	bool cdi_efuse_inc_mask_valid;
	unsigned int cdi_efuse_inc_mask;
	bool uds_selfgen;
	bool uds_visible;
	unsigned char uds_data[32];
};

struct switchtec_security_cfg_state {
	bool debug_mode_valid;
	uint8_t basic_setting_valid;
	uint8_t public_key_exp_valid;
	uint8_t public_key_num_valid;
	uint8_t public_key_ver_valid;
	uint8_t public_key_valid;

	enum switchtec_debug_mode debug_mode;
	enum switchtec_secure_state secure_state;

	uint8_t jtag_lock_after_reset;
	uint8_t jtag_lock_after_bl1;
	uint8_t jtag_bl1_unlock_allowed;
	uint8_t jtag_post_bl1_unlock_allowed;

	float spi_clk_rate;
	uint32_t i2c_recovery_tmo;
	uint32_t i2c_port;
	uint32_t i2c_addr;
	uint32_t i2c_cmd_map;
	uint32_t public_key_exponent;
	uint32_t public_key_num;
	uint32_t public_key_ver;

	uint8_t public_key[SWITCHTEC_KMSK_NUM_MAX][SWITCHTEC_KMSK_LEN];

	bool otp_valid;
	bool use_otp_ext;
	struct switchtec_security_cfg_otp_region otp;
	struct switchtec_security_cfg_otp_region_ext otp_ext;

	struct switchtec_attestation_state attn_state;
};

struct switchtec_attestation_set {
	enum switchtec_attestation_mode attestation_mode;
	unsigned int cdi_efuse_inc_mask;
	bool uds_selfgen;
	bool uds_valid;
	unsigned char uds_data[32];
};

struct switchtec_security_cfg_set {
	uint8_t jtag_lock_after_reset;
	uint8_t jtag_lock_after_bl1;
	uint8_t jtag_bl1_unlock_allowed;
	uint8_t jtag_post_bl1_unlock_allowed;

	float spi_clk_rate;
	uint32_t i2c_recovery_tmo;
	uint32_t i2c_port;
	uint32_t i2c_addr;
	uint32_t i2c_cmd_map;
	uint32_t public_key_exponent;

	struct switchtec_attestation_set attn_set;
};

enum switchtec_active_index_id {
	SWITCHTEC_ACTIVE_INDEX_0 = 0,
	SWITCHTEC_ACTIVE_INDEX_1 = 1,
	SWITCHTEC_ACTIVE_INDEX_NOT_SET = 0xfe
};

struct switchtec_active_index {
	enum switchtec_active_index_id bl2;
	enum switchtec_active_index_id firmware;
	enum switchtec_active_index_id config;
	enum switchtec_active_index_id keyman;
	enum switchtec_active_index_id riot;
};

enum switchtec_bl2_recovery_mode {
	SWITCHTEC_BL2_RECOVERY_I2C = 1,
	SWITCHTEC_BL2_RECOVERY_XMODEM = 2,
	SWITCHTEC_BL2_RECOVERY_I2C_AND_XMODEM = 3
};

struct switchtec_kmsk {
	uint8_t kmsk[SWITCHTEC_KMSK_LEN];
};

struct switchtec_pubkey {
	uint8_t pubkey[SWITCHTEC_PUB_KEY_LEN];
	uint32_t pubkey_exp;
};

struct switchtec_signature{
	uint8_t signature[SWITCHTEC_SIG_LEN];
};

struct switchtec_uds {
	unsigned char uds[SWITCHTEC_UDS_LEN];
};

struct switchtec_security_spi_avail_rate {
	int num_rates;
	float rates[SWITCHTEC_SECURITY_SPI_RATE_MAX_NUM];
};

int switchtec_sn_ver_get(struct switchtec_dev *dev,
			 struct switchtec_sn_ver_info *info);
int switchtec_security_config_get(struct switchtec_dev *dev,
			          struct switchtec_security_cfg_state *state);
int switchtec_security_spi_avail_rate_get(struct switchtec_dev *dev,
		struct switchtec_security_spi_avail_rate *rates);
int switchtec_security_config_set(struct switchtec_dev *dev,
				  struct switchtec_security_cfg_set *setting);
int switchtec_mailbox_to_file(struct switchtec_dev *dev, int fd);
int switchtec_active_image_index_get(struct switchtec_dev *dev,
				     struct switchtec_active_index *index);
int switchtec_active_image_index_set(struct switchtec_dev *dev,
				     struct switchtec_active_index *index);
int switchtec_fw_exec(struct switchtec_dev *dev,
		      enum switchtec_bl2_recovery_mode recovery_mode);
int switchtec_boot_resume(struct switchtec_dev *dev);
int switchtec_kmsk_set(struct switchtec_dev *dev,
		       struct switchtec_pubkey *public_key,
		       struct switchtec_signature *signature,
		       struct switchtec_kmsk *kmsk);
int switchtec_secure_state_set(struct switchtec_dev *dev,
			       enum switchtec_secure_state state);
int switchtec_dbg_unlock(struct switchtec_dev *dev, uint32_t serial,
			 uint32_t ver_sec_unlock,
			 struct switchtec_pubkey *public_key,
			 struct switchtec_signature *signature);
int switchtec_dbg_unlock_version_update(struct switchtec_dev *dev,
					uint32_t serial,
					uint32_t ver_sec_unlock,
					struct switchtec_pubkey *public_key,
			 		struct switchtec_signature *signature);
int switchtec_read_sec_cfg_file(struct switchtec_dev *dev,
				FILE *setting_file,
				struct switchtec_security_cfg_set *set);
int switchtec_read_pubk_file(FILE *pubk_file, struct switchtec_pubkey *pubk);
int switchtec_read_kmsk_file(FILE *kmsk_file, struct switchtec_kmsk *kmsk);
int switchtec_read_signature_file(FILE *sig_file,
				  struct switchtec_signature *sigature);
int switchtec_read_uds_file(FILE *uds_file, struct switchtec_uds *uds);
int
switchtec_security_state_has_kmsk(struct switchtec_security_cfg_state *state,
				  struct switchtec_kmsk *kmsk);

#endif // LIBSWITCHTEC_MFG_H
