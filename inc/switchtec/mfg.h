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

#define SWITCHTEC_MB_LOG_LEN	64
#define SWITCHTEC_KMSK_LEN	64
#define SWITCHTEC_KMSK_NUM	4

struct switchtec_sn_ver_info {
	uint32_t chip_serial;
	uint32_t ver_km;
	uint32_t ver_bl2;
	uint32_t ver_main;
	uint32_t ver_sec_unlock;
};
enum switchtec_debug_mode {
	SWITCHTEC_DEBUG_MODE_ENABLED,
	SWITCHTEC_DEBUG_MODE_DISABLED_BUT_ENABLE_ALLOWED,
	SWITCHTEC_DEBUG_MODE_DISABLED
};

enum switchtec_secure_state {
	SWITCHTEC_UNINITIALIZED_UNSECURED,
	SWITCHTEC_INITIALIZED_UNSECURED,
	SWITCHTEC_INITIALIZED_SECURED,
	SWITCHTEC_SECURE_STATE_UNKNOWN = 0xff,
};

enum switchtec_spi_clk_rate {
	SWITCHTEC_SPI_RATE_100M = 1,
	SWITCHTEC_SPI_RATE_67M,
	SWITCHTEC_SPI_RATE_50M,
	SWITCHTEC_SPI_RATE_40M,
	SWITCHTEC_SPI_RATE_33_33M,
	SWITCHTEC_SPI_RATE_28_57M,
	SWITCHTEC_SPI_RATE_25M,
	SWITCHTEC_SPI_RATE_22_22M,
	SWITCHTEC_SPI_RATE_20M,
	SWITCHTEC_SPI_RATE_18_18M
};

struct switchtec_security_cfg_stat {
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

	enum switchtec_spi_clk_rate spi_clk_rate;
	uint32_t i2c_recovery_tmo;
	uint32_t i2c_port;
	uint32_t i2c_addr;
	uint32_t i2c_cmd_map;
	uint32_t public_key_exponent;
	uint32_t public_key_num;
	uint32_t public_key_ver;

	uint8_t public_key[SWITCHTEC_KMSK_NUM][SWITCHTEC_KMSK_LEN];
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
};

enum switchtec_bl2_recovery_mode {
	SWITCHTEC_BL2_RECOVERY_I2C = 1,
	SWITCHTEC_BL2_RECOVERY_XMODEM = 2,
	SWITCHTEC_BL2_RECOVERY_I2C_AND_XMODEM = 3
};

int switchtec_sn_ver_get(struct switchtec_dev *dev,
			 struct switchtec_sn_ver_info *info);
int switchtec_security_config_get(struct switchtec_dev *dev,
			          struct switchtec_security_cfg_stat *state);
int switchtec_mailbox_to_file(struct switchtec_dev *dev, int fd);
int switchtec_active_image_index_get(struct switchtec_dev *dev,
				     struct switchtec_active_index *index);
int switchtec_active_image_index_set(struct switchtec_dev *dev,
				     struct switchtec_active_index *index);
int switchtec_fw_exec(struct switchtec_dev *dev,
		      enum switchtec_bl2_recovery_mode recovery_mode);
int switchtec_boot_resume(struct switchtec_dev *dev);
int switchtec_secure_state_set(struct switchtec_dev *dev,
			       enum switchtec_secure_state state);

#endif // LIBSWITCHTEC_MFG_H
