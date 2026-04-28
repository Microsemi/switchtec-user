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
#define SWITCHTEC_KMSK_NUM_GEN6		12
#define SWITCHTEC_KMSK_LEN_DWORDS	(SWITCHTEC_KMSK_LEN / 4)
#define SWITCHTEC_GEN6_TOKEN_LEN	88

#define SWITCHTEC_UID_LEN_DWORDS	16
#define SWITCHTEC_PSID_LEN_DWORDS	4

#define OTP_MULTI_DWORD_UID_UNIQUEID_DWORDS		16
#define OTP_MULTI_DWORD_CUSTOMER_PSID0_DWORDS	4

#define SWITCHTEC_SECURITY_SPI_RATE_MAX_NUM	16

#define SWITCHTEC_UID_DWORD_S 	16
#define SWITCHTEC_PSID_DWORD_S 	4

#define OTP_MULTI_DWORD_IMAGE_BIAK0			   656
#define OTP_DWORD_0							   0
#define OTP_DWORD_10 						   10

#define OTP_DWORD_0_PRODUCT_SECSC_LSB          22
#define OTP_DWORD_0_PRODUCT_SECSC_MSK          0x00400000

#define OTP_DWORD_10_SMBUS_SMBRMRPCADDR_LSB    0
#define OTP_DWORD_10_SMBUS_SMBRMRPCADDR_MSK    0x000003FF
#define OTP_DWORD_10_SMBUS_SMBRIF_LSB          10
#define OTP_DWORD_10_SMBUS_SMBRIF_MSK          0x00000C00
#define OTP_DWORD_10_SMBUS_SMBRATYPE_LSB       12
#define OTP_DWORD_10_SMBUS_SMBRATYPE_MSK       0x00003000
#define OTP_DWORD_10_SMBUS_SMBROCPADDR_LSB     18
#define OTP_DWORD_10_SMBUS_SMBROCPADDR_MSK     0x0FFC0000

#define SECIRE_CFG_GET_I2C					   (0xD4>>1)
#define SECURE_CFG_GET_OCP					   (0xD2>>1)

#define SECURE_CFG_GET_I2C_PORT_MSK			   0x00000003
#define SECURE_CFG_GET_I2C_PORT_LSB			   0x0000000A
#define SECURE_CFG_GET_I2C_ADDR_MSK			   0x000003FF
#define SECURE_CFG_GET_I2C_CMD_MAP_MSK		   0x00000FFF
#define SECURE_CFG_GET_I2C_CMD_MAP_LSB		   0x0000000C
#define SECURE_CFG_GET_I2C_RCVRY_INF_MSK	   0x0000C000
#define SECURE_CFG_GET_I2C_RCVRY_ADDR_MSK	   0x000003FF

struct switchtec_sn_ver_info {
	uint32_t chip_serial;
	uint32_t ver_km;
	uint32_t ver_bl2;
	uint32_t ver_main;
	uint32_t ver_sec_unlock;
	bool riot_ver_valid;
	uint32_t ver_riot;
	uint32_t *UID;
	uint32_t *PSID0;
	uint32_t PSID_UID_valid_flags;
	uint32_t dbg_tok_sec_ver_rsvrd;
	uint32_t kmt_sec_ver_rsvrd;
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

enum switchtec_secure_state_gen6 {
	SWITCHTEC_GEN6_UNINITIALIZED_SECURE_CAPABLE = 0,
	SWITCHTEC_GEN6_UNPROVISIONED_SECURED = 1,
	SWITCHTEC_GEN6_INITIALIZED_SECURED = 2,
	SWITCHTEC_GEN6_INITIALIZED_UNSECURED = 3,
	SWITCHTEC_GEN6_SECURE_STATE_UNKNOWN = 0xff,
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

	uint8_t secsc;
	uint16_t i2c_rcvry_address_ocp;
	uint32_t otp_key_hash[SWITCHTEC_KMSK_NUM_GEN6][SWITCHTEC_KMSK_LEN_DWORDS];

	bool otp_valid;
	bool use_otp_ext;
	struct switchtec_security_cfg_otp_region otp;
	struct switchtec_security_cfg_otp_region_ext otp_ext;

	struct switchtec_attestation_state attn_state;
};

struct switchtec_security_cfg_state_gen6 {
	/* DWORD 0 */
	uint32_t twi_rcvry_address_mrpc   :10;
	uint32_t twi_rcvry_bus            :2;
	uint32_t twi_address_type         :2;
	uint32_t twi_rcvry_address_ocp    :10;
	uint32_t reserved_dw_0_1          :8;

	/* DWORD 1 */
	uint32_t mrpc_command_map     :12;
	uint32_t secsc                :1;
	uint32_t reserved_dw_1_1      :19;

	/* DWORD 2 */
	uint32_t ap_offset            :20;
	uint32_t reserved_dw_2_1      :12;

	/* DWORD 3 */
	uint32_t i3c_pid_high         :32;

	/* DWORD 4 */
	uint32_t i3c_pid_low          :32;

	/* DWORD 5 */
	uint32_t i3c_rcvry_address    :7;
	uint32_t i3c_rcvry_bus        :2;
	uint32_t reserved_dw_5_1      :23;

	/* DWORD 6 */
	uint32_t algo_crc_disable             :1;
	uint32_t algo_ecdsa_p384_disable      :1;
	uint32_t algo_ecdsa_p521_disable      :1;
	uint32_t algo_rsa3ksha2_disable       :1;
	uint32_t algo_rsa4ksha2_disable       :1;
	uint32_t algo_dilithium5_disable      :1;
	uint32_t reserved_dw_6_1              :2;
	uint32_t rom_key_1_disable            :1;
	uint32_t rom_key_2_disable            :1;
	uint32_t rom_key_3_disable            :1;
	uint32_t rom_key_4_disable            :1;
	uint32_t reserved_dw_6_2              :4;
	uint32_t boot_from_uart_disable       :1;
	uint32_t boot_from_smbus_disable      :1;
	uint32_t boot_from_i3c_disable        :1;
	uint32_t failover_to_uart_disable     :1;
	uint32_t failover_to_smbus_disable    :1;
	uint32_t failover_to_i3c_disable      :1;
	uint32_t reserved_dw_6_3              :2;
	uint32_t static_token_disable         :1;
	uint32_t psid_only_token_disable      :1;
	uint32_t uid_only_token_disable       :1;
	uint32_t psid_uid_token_disable       :1;
	uint32_t reserved_dw_6_4              :4;

	/* DWORD 7 */
	uint32_t puf_ac_status            :2;
	uint32_t rsvd_dw_7_0              :2;
	uint32_t otp_key0_hash_status     :2;
	uint32_t otp_key1_hash_status     :2;
	uint32_t otp_key2_hash_status     :2;
	uint32_t otp_key3_hash_status     :2;
	uint32_t otp_key4_hash_status     :2;
	uint32_t otp_key5_hash_status     :2;
	uint32_t otp_key6_hash_status     :2;
	uint32_t otp_key7_hash_status     :2;
	uint32_t otp_key8_hash_status     :2;
	uint32_t otp_key9_hash_status     :2;
	uint32_t otp_key10_hash_status    :2;
	uint32_t otp_key11_hash_status    :2;
	uint32_t rsvd_dw_7_1              :4;

	/* DWORD 8 */
	uint32_t rsvd_dw_8_0                  :24;
	uint32_t has_table_sha2_384_disable   :1;
	uint32_t has_table_sha2_512_disable   :1;
	uint32_t has_table_sha3_512_disable   :1;
	uint32_t has_table_crc32_disable      :1;
	uint32_t reserved_dw_8_1              :4;

	/* DWORD 9 to ... */
	uint32_t otp_key_hash[SWITCHTEC_KMSK_NUM_GEN6][SWITCHTEC_KMSK_LEN_DWORDS];
};

/**
 * @brief Supported KMT Signature Formats. Value stored in KMT Prefix in 4 bit field.
 */
enum kmt_signature_types_e {
	KMT_SIG_FORMAT_CRC            = 0,
	KMT_SIG_FORMAT_RSA3KSHA2      = 1,
	KMT_SIG_FORMAT_RSA4KSHA2      = 2,
	KMT_SIG_FORMAT_ECDSAP384SHA2  = 3,
	KMT_SIG_FORMAT_ECDSAP521SHA2  = 4,
	KMT_SIG_FORMAT_DILITHIUM5     = 5,
	KMT_SIG_FORMAT_MAX
};

enum switchtec_otp_key_status {
	UNPROGRAMMED = 0x00,
	PROGRAMMED = 0x01,
	REVOKED = 0x02,
	INVALID = 0x03
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

#define TOKEN_RESOURCE_UNLOCK 0
#define TOKEN_VERSION_UPDATE 1
#define GEN6_TOKEN_STATIC    2
#define GEN6_TOKEN_EPHEMERAL 3

enum secure_token_get_types_e {
	SECURE_TOKEN_GET_TYPE_STATIC    = 0,
	SECURE_TOKEN_GET_TYPE_EPHEMERAL = 1,
	SECURE_TOKEN_GET_TYPE_MAX
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

struct switchtec_gen6_token{
	uint8_t token[SWITCHTEC_GEN6_TOKEN_LEN];
};

struct switchtec_uds {
	unsigned char uds[SWITCHTEC_UDS_LEN];
};

struct switchtec_security_spi_avail_rate {
	int num_rates;
	float rates[SWITCHTEC_SECURITY_SPI_RATE_MAX_NUM];
};

struct sec_cfg_get_struct {
	uint32_t subcmd;
	uint32_t OTP_dword_offset;
	uint32_t read_dwords;
};

int switchtec_sn_ver_get(struct switchtec_dev *dev,
			 struct switchtec_sn_ver_info *info);
int switchtec_security_config_get(struct switchtec_dev *dev, void *state);
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
			 struct switchtec_signature *signature,
			 struct switchtec_gen6_token *token);
int switchtec_dbg_unlock_version_update(struct switchtec_dev *dev,
					uint32_t serial,
					uint32_t ver_sec_unlock,
					struct switchtec_pubkey *public_key,
					struct switchtec_signature *signature);
int switchtec_dbg_unlock_get_token_gen6(struct switchtec_dev *dev,
					struct switchtec_gen6_token *token,
					int token_type);
int switchtec_dbg_unlock_status_get_gen6(struct switchtec_dev *dev,
					 uint32_t *jtag_status);
int switchtec_secure_state_get_gen6(struct switchtec_dev *dev,
				    enum switchtec_secure_state_gen6 *state);
int switchtec_read_sec_cfg_file(struct switchtec_dev *dev,
				FILE *setting_file,
				struct switchtec_security_cfg_set *set);
int switchtec_read_pubk_file(FILE *pubk_file, struct switchtec_pubkey *pubk);
int switchtec_read_kmsk_file(FILE *kmsk_file, struct switchtec_kmsk *kmsk);
int switchtec_read_signature_file(FILE *sig_file,
				  struct switchtec_signature *sigature);
int switchtec_read_token_file(FILE *tkn_file, struct switchtec_gen6_token *token);
int switchtec_read_uds_file(FILE *uds_file, struct switchtec_uds *uds);
int
switchtec_security_state_has_kmsk(struct switchtec_security_cfg_state *state,
				  struct switchtec_kmsk *kmsk);
int security_settings_get_gen6(struct switchtec_dev *dev,
					struct switchtec_security_cfg_state_gen6 *state);

/*
 * Device Configuration MRPC (MRPC_DEVICE_CONFIG = 0x127)
 * Structures and constants for Gen6 device configuration
 */

/* Sub-commands for MRPC_DEVICE_CONFIG */
#define DEVICE_CONFIG_SUB_CMD_SET_DEVICE    0x0
#define DEVICE_CONFIG_SUB_CMD_SET_SECURITY  0x1
#define DEVICE_CONFIG_SUB_CMD_SET_CUSTOMER  0x2
#define DEVICE_CONFIG_SUB_CMD_GET           0x3
#define DEVICE_CONFIG_SUB_CMD_GET_SECURITY  0x4
#define DEVICE_CONFIG_SUB_CMD_GET_CUSTOMER  0x5

/* Constants for device configuration structures */
#define DEVICE_CONFIG_CUSTOMER_FIELD_NUM        4
#define DEVICE_CONFIG_CUSTOMER_ECC_FIELD_NUM    4
#define DEVICE_CONFIG_CUSTOMER_ECC_FIELD_SIZE   2
#define DEVICE_CONFIG_KEY_HASH_SIZE_DWORDS      16
#define DEVICE_CONFIG_MAX_KEY_SLOTS             12

struct switchtec_device_config_dev_settings {
	/* DWORD 0 */
	uint32_t twi_ocp_addr        :10;
	uint32_t twi_mrpc_addr       :10;
	uint32_t twi_rcvry_addr_type :2;
	uint32_t twi_rcvry_bus       :2;
	uint32_t rsvd_0              :8;

	/* DWORD 1 */
	uint32_t i3c_pid_hi;

	/* DWORD 2 */
	uint32_t i3c_pid_lo          :16;
	uint32_t i3c_addr_7bit       :7;
	uint32_t i3c_rcvry_bus       :2;
	uint32_t rsvd_1              :7;
};

struct switchtec_device_config_customer_settings {
	/* DWORD 0 */
	uint32_t device_id                    :16;
	uint32_t vendor_id                    :16;

	/* DWORD 1 */
	uint32_t revision_id                  :16;
	uint32_t subsystem_id                 :16;

	/* DWORD 2 */
	uint32_t subsystem_vendor_id          :16;
	uint32_t rsvd_0                       :16;

	/* DWORD 3-6: customer fields */
	uint32_t customer_fields[DEVICE_CONFIG_CUSTOMER_FIELD_NUM];

	/* DWORD 7-14: customer ECC fields */
	uint32_t customer_ecc_fields[DEVICE_CONFIG_CUSTOMER_ECC_FIELD_NUM]
				    [DEVICE_CONFIG_CUSTOMER_ECC_FIELD_SIZE];
};

struct switchtec_device_config_key_data {
	/* DWORD 0 */
	uint32_t index         :8;
	uint32_t rsvd          :24;

	/* DWORD 1-16: key hash (SHA2-512) */
	uint32_t hash[DEVICE_CONFIG_KEY_HASH_SIZE_DWORDS];
};

struct switchtec_device_config_secure_settings {
	/* DWORD 0 */
	uint32_t command_map                  :12;
	uint32_t rsvd_0                       :4;
	uint32_t static_token_disable         :1;
	uint32_t psid_only_token_disable      :1;
	uint32_t uid_only_token_disable       :1;
	uint32_t psid_uid_token_disable       :1;
	uint32_t rsvd_1                       :4;
	uint32_t boot_from_uart_disable       :1;
	uint32_t boot_from_smbus_disable      :1;
	uint32_t boot_from_i3c_disable        :1;
	uint32_t failover_to_uart_disable     :1;
	uint32_t failover_to_smbus_disable    :1;
	uint32_t failover_to_i3c_disable      :1;
	uint32_t rsvd_2                       :2;

	/* DWORD 1-4: PSID0 */
	uint32_t psid0[SWITCHTEC_PSID_LEN_DWORDS];

	/* DWORD 5: number of keys to program */
	uint32_t key_prog_num;

	/* DWORD 6-...: key data (up to 12 keys, 17 DWORDs each) */
	struct switchtec_device_config_key_data key_data[DEVICE_CONFIG_MAX_KEY_SLOTS];
};

struct switchtec_device_config_security_settings_status {
	/* DWORD 0 */
	uint32_t dok0_status      :2;
	uint32_t dok1_status      :2;
	uint32_t dok2_status      :2;
	uint32_t dok3_status      :2;
	uint32_t dok4_status      :2;
	uint32_t dok5_status      :2;
	uint32_t dok6_status      :2;
	uint32_t dok7_status      :2;
	uint32_t dok8_status      :2;
	uint32_t dok9_status      :2;
	uint32_t dok10_status     :2;
	uint32_t dok11_status     :2;
	uint32_t rsvd             :8;
};

struct switchtec_device_config_get_sec {
	struct switchtec_device_config_secure_settings secure_settings;
	struct switchtec_device_config_security_settings_status secure_settings_status;
};

int switchtec_device_config_get(struct switchtec_dev *dev,
				struct switchtec_device_config_dev_settings *settings);
int switchtec_device_config_get_security(struct switchtec_dev *dev,
					 struct switchtec_device_config_get_sec *config);
int switchtec_device_config_get_customer(struct switchtec_dev *dev,
					 struct switchtec_device_config_customer_settings *settings);
int switchtec_device_config_set_dev(struct switchtec_dev *dev,
				    struct switchtec_device_config_dev_settings *settings);
int switchtec_device_config_set_customer(struct switchtec_dev *dev,
					 struct switchtec_device_config_customer_settings *settings);
int switchtec_device_config_set_security(struct switchtec_dev *dev,
					 struct switchtec_device_config_secure_settings *settings);

/*
 * DOK Config MRPC (MRPC_DOK_CONFIG = 0x128)
 * Device Owner Key configuration for Gen6 devices
 */

/* Sub-commands for MRPC_DOK_CONFIG */
#define DOK_CONFIG_SUB_CMD_SIGNATURE    0x0
#define DOK_CONFIG_SUB_CMD_PROVISION    0x1
#define DOK_CONFIG_SUB_CMD_REVOKE       0x2

/* Authorization Flag values (auth_type field) */
#define DOK_AUTH_FLAG_UID_ONLY          0x0
#define DOK_AUTH_FLAG_PSID_ONLY         0x1
#define DOK_AUTH_FLAG_UID_AND_PSID      0x2
#define DOK_AUTH_FLAG_NONE              0x3

struct switchtec_dok_signature {
	uint8_t sub_cmd;
	uint8_t sig_type;
	uint8_t reserved[2];
	uint32_t total_len;
	uint32_t total_crc;
	uint32_t data_len;
	uint32_t offset;
	uint8_t sig_data[512];
};

struct switchtec_dok_key_add {
	/* DWORD 0 */
	uint32_t sub_cmd       :8;
	uint32_t key_slot      :8;
	uint32_t auth_type     :8;
	uint32_t reserved      :8;

	/* DWORD 1-16: UID (512 bits) */
	uint32_t uid[SWITCHTEC_UID_LEN_DWORDS];

	/* DWORD 17-20: PSID (128 bits) */
	uint32_t psid[SWITCHTEC_PSID_LEN_DWORDS];

	/* DWORD 21-36: key hash (SHA2-512, 512 bits) */
	uint32_t key_hash[DEVICE_CONFIG_KEY_HASH_SIZE_DWORDS];

	/* DWORD 37-52: integrity hash (SHA2-512, 512 bits)
	 * Required when auth_type == DOK_AUTH_FLAG_NONE */
	uint32_t integrity_hash[DEVICE_CONFIG_KEY_HASH_SIZE_DWORDS];
};

struct switchtec_dok_key_revoke {
	/* DWORD 0 */
	uint32_t sub_cmd       :8;
	uint32_t key_slot      :8;
	uint32_t auth_type     :8;
	uint32_t reserved      :8;

	/* DWORD 1-16: UID (512 bits) */
	uint32_t uid[SWITCHTEC_UID_LEN_DWORDS];

	/* DWORD 17-20: PSID (128 bits) */
	uint32_t psid[SWITCHTEC_PSID_LEN_DWORDS];

	/* DWORD 21-36: integrity hash (SHA2-512, 512 bits)
	 * Required when auth_type == DOK_AUTH_FLAG_NONE */
	uint32_t integrity_hash[DEVICE_CONFIG_KEY_HASH_SIZE_DWORDS];
};

int switchtec_dok_config_signature(struct switchtec_dev *dev,
				   struct switchtec_dok_signature *sig);
int switchtec_dok_config_key_add(struct switchtec_dev *dev,
				 struct switchtec_dok_key_add *key_add);
int switchtec_dok_config_key_revoke(struct switchtec_dev *dev,
				    struct switchtec_dok_key_revoke *key_revoke);

#endif // LIBSWITCHTEC_MFG_H
