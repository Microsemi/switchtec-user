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
#include "gen4/mfg_gen4.h"
#include "gen6/mfg_gen6.h"
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

/**
 * @brief Get secure boot configurations
 * @param[in]  dev	Switchtec device handle
 * @param[out] state	Current secure boot settings
 * @return 0 on success, error code on failure
 */
int switchtec_security_config_get(struct switchtec_dev *dev,
				  struct switchtec_security_cfg_state *state)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->security_config_get)
		return GEN_OPS(dev)->security_config_get(dev, state);
	return -EOPNOTSUPP;
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

/**
 * @brief Retrieve mailbox entries
 * @param[in]  dev	Switchtec device handle
 * @param[in]  fd	File handle to write the log data
 * @return 0 on success, error code on failure
 */
int switchtec_mailbox_to_file(struct switchtec_dev *dev, int fd)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->mailbox_to_file)
		return GEN_OPS(dev)->mailbox_to_file(dev, fd);
	return -EOPNOTSUPP;
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
	if (GEN_OPS(dev) && GEN_OPS(dev)->security_config_set)
		return GEN_OPS(dev)->security_config_set(dev, setting);
	return -EOPNOTSUPP;
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
	if (GEN_OPS(dev) && GEN_OPS(dev)->active_image_index_get)
		return GEN_OPS(dev)->active_image_index_get(dev, index);
	return -EOPNOTSUPP;
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
	if (GEN_OPS(dev) && GEN_OPS(dev)->active_image_index_set)
		return GEN_OPS(dev)->active_image_index_set(dev, index);
	return -EOPNOTSUPP;
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
	if (GEN_OPS(dev) && GEN_OPS(dev)->fw_exec)
		return GEN_OPS(dev)->fw_exec(dev, recovery_mode);
	return -EOPNOTSUPP;
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
	if (GEN_OPS(dev) && GEN_OPS(dev)->boot_resume)
		return GEN_OPS(dev)->boot_resume(dev);
	return -EOPNOTSUPP;
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

	if (GEN_OPS(dev) && GEN_OPS(dev)->secure_state_set)
		return GEN_OPS(dev)->secure_state_set(dev, state);
	return -EOPNOTSUPP;
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
	if (GEN_OPS(dev) && GEN_OPS(dev)->debug_unlock)
		return GEN_OPS(dev)->debug_unlock(dev, serial, ver_sec_unlock,
						  public_key, signature, token);
	return -EOPNOTSUPP;
}

/**
 * @brief Get gen6 token
 * @param[in]  dev		Switchtec device handle
 * @param[in]  token	pointer to downloaded token
 * @param[in]  token_type	type of token to download
 *
 * @return 0 on success, error code on failure
 */
int switchtec_dbg_unlock_get_token(struct switchtec_dev *dev,
			struct switchtec_gen6_token *token,
			int token_type)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->debug_token_unlock_get_token)
		return GEN_OPS(dev)->debug_token_unlock_get_token(dev, token,
								   token_type);
	return -EOPNOTSUPP;
}

int switchtec_security_settings_get(struct switchtec_dev *dev, struct switchtec_security_cfg_state *state)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->security_settings_get)
		return GEN_OPS(dev)->security_settings_get(dev, state);
	return -EOPNOTSUPP;
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
	if (GEN_OPS(dev) && GEN_OPS(dev)->dbg_unlock_version_update)
		return GEN_OPS(dev)->dbg_unlock_version_update(dev, serial,
								ver_sec_unlock,
								public_key,
								signature);
	return -EOPNOTSUPP;
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

	if (GEN_OPS(dev) && GEN_OPS(dev)->read_sec_cfg_file)
		return GEN_OPS(dev)->read_sec_cfg_file(dev, setting_file, set);
	return -EOPNOTSUPP;
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
	if (GEN_OPS(dev) && GEN_OPS(dev)->kmsk_set)
		return GEN_OPS(dev)->kmsk_set(dev, public_key, signature, kmsk);
	return -EOPNOTSUPP;
}

/**
 * @brief Read public key from public key file
 * @param[in]  pubk_file Public key file
 * @param[out] pubk	 Public key
 * @return 0 on success, error code on failure
 */
int switchtec_read_pubk_file(FILE *pubk_file, struct switchtec_pubkey *pubk)
{
	return switchtec_read_pubk_file_gen4(pubk_file, pubk);
}

/**
 * @brief Read KMSK data from KMSK file
 * @param[in]  kmsk_file KMSK file
 * @param[out] kmsk   	 KMSK entry data
 * @return 0 on success, error code on failure
 */
int switchtec_read_kmsk_file(FILE *kmsk_file, struct switchtec_kmsk *kmsk)
{
	return switchtec_read_kmsk_file_gen4(kmsk_file, kmsk);
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
	return switchtec_read_signature_file_gen4(sig_file, signature);
}

/**
 * @brief Read token data from token file
 * @param[in]  tkn_file  Token file
 * @param[out] token Token data
 * @return 0 on success, error code on failure
 */
int switchtec_read_token_file(FILE *tkn_file, struct switchtec_gen6_token *token)
{
	return switchtec_read_token_file_gen6(tkn_file, token);
}

/**
 * @brief Read UDS data from UDS file
 * @param[in]  uds_file  UDS file
 * @param[out] uds       UDS data
 * @return 0 on success, error code on failure
 */
int switchtec_read_uds_file(FILE *uds_file, struct switchtec_uds *uds)
{
	return switchtec_read_uds_file_gen4(uds_file, uds);
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
	return switchtec_security_state_has_kmsk_gen4(state, kmsk);
}

#endif /* __linux__ */

/**
 * @brief Get serial number and security version
 * @param[in]  dev	Switchtec device handle
 * @param[out] info	Serial number and security version info
 * @return 0 on success, error code on failure
 */
int switchtec_sn_ver_get(struct switchtec_dev *dev,
			 struct switchtec_sn_ver_info *info)
{
	if (GEN_OPS(dev) && GEN_OPS(dev)->sn_ver_get)
		return GEN_OPS(dev)->sn_ver_get(dev, info);
	return -EOPNOTSUPP;
}

/**@}*/