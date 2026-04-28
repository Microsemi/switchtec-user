/*
 * Microsemi Switchtec(tm) PCIe Management Command Line Interface
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

#ifdef __linux__

#include "commands.h"
#include "argconfig.h"
#include "suffix.h"
#include "progress.h"
#include "gui.h"
#include "common.h"
#include "progress.h"

#include "config.h"

#include <switchtec/switchtec.h>
#include <switchtec/utils.h>
#include <switchtec/mfg.h>
#include <switchtec/endian.h>

#include <locale.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

static const struct argconfig_choice recovery_mode_choices[] = {
	{"I2C", SWITCHTEC_BL2_RECOVERY_I2C, "I2C"},
	{"XMODEM", SWITCHTEC_BL2_RECOVERY_XMODEM, "XModem"},
	{"BOTH", SWITCHTEC_BL2_RECOVERY_I2C_AND_XMODEM,
		"both I2C and XModem (default)"},
	{}
};

static const struct argconfig_choice secure_state_choices[] = {
	{"INITIALIZED_UNSECURED", SWITCHTEC_INITIALIZED_UNSECURED,
		"unsecured state"},
	{"INITIALIZED_SECURED", SWITCHTEC_INITIALIZED_SECURED,
		"secured state"},
	{}
};

#define CMD_DESC_PING "ping device and get current boot phase"

static int ping(int argc, char **argv)
{
	int ret;
	static struct {
		struct switchtec_dev *dev;
	} cfg = {};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION_MFG_PCI,
		{NULL}
	};

	argconfig_parse(argc, argv, CMD_DESC_PING, opts, &cfg, sizeof(cfg));

	ret = switchtec_get_device_info(cfg.dev, NULL, NULL, NULL);
	if (ret) {
		switchtec_perror("mfg ping");
		return ret;
	}

	printf("Mfg Ping: \t\tSUCCESS\n");

	return 0;
}

static const char* program_status_to_string(enum switchtec_otp_program_status s)
{
	switch (s) {
	case SWITCHTEC_OTP_PROGRAMMABLE:
		return "R/W (Programmable)";
	case SWITCHTEC_OTP_UNPROGRAMMABLE:
		return "R/O (Unprogrammable)";
	default:
		return "Unknown";
	}
}

static const char* program_mask_to_string(enum switchtec_otp_program_mask m)
{
	switch (m) {
	case SWITCHTEC_OTP_UNMASKED:
		return "Accessible";
	case SWITCHTEC_OTP_MASKED:
		return "Inaccessible)";
	default:
		return "Unknown";
	}
}

static void print_otp_info(struct switchtec_security_cfg_otp_region *otp)
{
	int i;

	printf("\nOTP Region Program Status\n");
	printf("\tBasic Secure Settings %s%s\n",
	       otp->basic_valid? "(Valid):  ": "(Invalid):",
	program_status_to_string(otp->basic));
	printf("\tMixed Version %s\t%s\n",
	       otp->mixed_ver_valid? "(Valid):  ": "(Invalid):",
	program_status_to_string(otp->mixed_ver));
	printf("\tMain FW Version %s\t%s\n",
	       otp->main_fw_ver_valid? "(Valid):  ": "(Invalid):",
	       program_status_to_string(otp->main_fw_ver));
	printf("\tSecure Unlock Version %s%s\n",
	       otp->sec_unlock_ver_valid? "(Valid):  ": "(Invalid):",
	       program_status_to_string(otp->sec_unlock_ver));
	for (i = 0; i < 4; i++) {
		printf("\tKMSK%d %s\t\t%s\n", i + 1,
		       otp->kmsk_valid[i]? "(Valid):  ": "(Invalid):",
		       program_status_to_string(otp->kmsk[i]));
	}
}

static void print_otp_ext_info(
	struct switchtec_security_cfg_otp_region_ext *otp)
{
	int i;

	printf("\nOTP Region Program Status\n");
	printf("\tBasic Secure Settings %s%s\n",
	       otp->basic_valid? "(Valid):  ": "(Invalid):",
	       program_status_to_string(otp->basic));

	printf("\tDebug Mode %s\t\t%s\n",
	       otp->debug_mode_valid? "(Valid):  ": "(Invalid):",
	       program_status_to_string(otp->debug_mode));
	printf("\tKey Version %s\t\t%s\n",
	       otp->key_ver_valid? "(Valid):  ": "(Invalid):",
	       program_status_to_string(otp->key_ver));
	printf("\tRIOT Core Version %s\t%s\n",
	       otp->rc_ver_valid? "(Valid):  ": "(Invalid):",
	       program_status_to_string(otp->rc_ver));
	printf("\tBL2 Version %s\t\t%s\n",
	       otp->bl2_ver_valid? "(Valid):  ": "(Invalid):",
	       program_status_to_string(otp->bl2_ver));
	printf("\tMain FW Version %s\t%s\n",
	       otp->main_fw_ver_valid? "(Valid):  ": "(Invalid):",
	       program_status_to_string(otp->main_fw_ver));
	printf("\tSecure Unlock Version %s%s\n",
	       otp->sec_unlock_ver_valid? "(Valid):  ": "(Invalid):",
	       program_status_to_string(otp->sec_unlock_ver));
	for (i = 0; i < 10; i++) {
		printf("\tKMSK%d %s\t\t%s\n", i + 1,
		       otp->kmsk_valid[i]? "(Valid):  ": "(Invalid):",
		       program_status_to_string(otp->kmsk[i]));
	}
	printf("\tCDI eFuse Include Mask %s%s\n",
	       otp->cdi_efuse_inc_mask_valid? "(Valid): ": "(Invalid):",
	       program_status_to_string(otp->cdi_efuse_inc_mask));
	printf("\tUDS %s\t\t\t%s - %s\n",
	       otp->uds_valid? "(Valid):  ": "(Invalid):",
	       program_status_to_string(otp->uds),
	       program_mask_to_string(otp->uds_mask));

	printf("\tMCHP UDS %s\t\t%s - %s\n",
	       otp->mchp_uds_valid? "(Valid):  ": "(Invalid):",
	       program_status_to_string(otp->mchp_uds),
	       program_mask_to_string(otp->mchp_uds_mask));

	printf("\tDID CERT0 %s\t\t%s\n",
	       otp->did_cert0_valid? "(Valid):  ": "(Invalid):",
	       program_status_to_string(otp->did_cert0));

	printf("\tDID CERT1 %s\t\t%s\n",
	       otp->did_cert1_valid? "(Valid):  ": "(Invalid):",
	       program_status_to_string(otp->did_cert1));
}

static void print_security_config(struct switchtec_security_cfg_state *state,
				  bool print_otp)
{
	int key_idx;
	int i;

	printf("\nDebug Mode Settings %s\n",
	       state->debug_mode_valid? "(Valid)":"(Invalid)");

	printf("\tJTAG/EJTAG Debug State: \t");
	switch (state->debug_mode) {
	case SWITCHTEC_DEBUG_MODE_ENABLED:
		printf("Always Enabled\n");
		break;
	case SWITCHTEC_DEBUG_MODE_DISABLED_BUT_ENABLE_ALLOWED:
		printf("Disabled by Default But Can Be Enabled\n");
		break;
	case SWITCHTEC_DEBUG_MODE_DISABLED:
	case SWITCHTEC_DEBUG_MODE_DISABLED_EXT:
		printf("Always Disabled\n");
		break;
	default:
		printf("Unsupported State\n");
		break;
	}

	printf("\nBasic Secure Settings %s\n",
		state->basic_setting_valid? "(Valid)":"(Invalid)");

	printf("\tSecure State: \t\t\t");
	switch (state->secure_state) {
	case SWITCHTEC_UNINITIALIZED_UNSECURED:
		printf("UNINITIALIZED_UNSECURED\n");
		break;
	case SWITCHTEC_INITIALIZED_UNSECURED:
		printf("INITIALIZED_UNSECURED\n");
		break;
	case SWITCHTEC_INITIALIZED_SECURED:
		printf("INITIALIZED_SECURED\n");
		break;
	default:
		printf("Unsupported State\n");
		break;
	}

	printf("\tJTAG/EJTAG State After Reset: \t%d\n",
		state->jtag_lock_after_reset);

	printf("\tJTAG/EJTAG State After BL1: \t%d\n",
		state->jtag_lock_after_bl1);

	printf("\tJTAG/EJTAG Unlock IN BL1: \t%d\n",
		state->jtag_bl1_unlock_allowed);

	printf("\tJTAG/EJTAG Unlock AFTER BL1: \t%d\n",
		state->jtag_post_bl1_unlock_allowed);

	printf("\tSPI Clock Rate: \t\t%.2f MHz\n", state->spi_clk_rate);

	printf("\tI2C Recovery TMO: \t\t%d Second(s)\n",
		state->i2c_recovery_tmo);
	printf("\tI2C Port: \t\t\t%d\n", state->i2c_port);
	printf("\tI2C Address (7-bits): \t\t0x%02x\n", state->i2c_addr);
	printf("\tI2C Command Map: \t\t0x%08x\n", state->i2c_cmd_map);

	if (state->attn_state.attestation_mode !=
	    SWITCHTEC_ATTESTATION_MODE_NOT_SUPPORTED) {
		if (state->attn_state.attestation_mode ==
		    SWITCHTEC_ATTESTATION_MODE_DICE) {
			printf("\tAttestation:\t\t\tEnabled, with UDS ");

			if (state->attn_state.uds_selfgen)
				printf("Self-Generated by Device\n");
			else
				printf("Provided by User\n");
		} else {
			printf("\tAttestation: \t\t\tDisabled\n");
		}
	}

	printf("\nExponent Hex Data %s: \t\t0x%08x\n",
		state->public_key_exp_valid? "(Valid)":"(Invalid)",
		state->public_key_exponent);

	printf("KMSK Entry Number %s: \t\t%d\n",
		state->public_key_num_valid? "(Valid)":"(Invalid)",
		state->public_key_num);

	if (state->public_key_ver)
		printf("Current KMSK Index %s: \t\t%d\n",
			state->public_key_ver_valid? "(Valid)":"(Invalid)",
			state->public_key_ver);
	else
		printf("Current KMSK Index %s: \t\tNot Set\n",
			state->public_key_ver_valid? "(Valid)":"(Invalid)");

	for (key_idx = 0; key_idx < state->public_key_num; key_idx++) {
		printf("KMSK Entry %d:  \t\t\t\t", key_idx + 1);
		for (i = 0; i < SWITCHTEC_KMSK_LEN; i++) {
			if (i && (i % 16) == 0)
				printf("\n\t\t\t\t\t");
			printf("%02x", state->public_key[key_idx][i]);
		}
		printf("\n\n");
	}

	if (state->attn_state.attestation_mode !=
	    SWITCHTEC_ATTESTATION_MODE_NOT_SUPPORTED) {
		printf("CDI eFuse Include Mask %s: \t0x%08x\n",
			state->attn_state.cdi_efuse_inc_mask_valid?
			"(Valid)":"(Invalid)",
			state->attn_state.cdi_efuse_inc_mask);

		printf("UDS Data: \t\t\t\t");
		if (state->attn_state.uds_visible) {
			for (i = 0; i < 32; i++) {
				printf("%02x", state->attn_state.uds_data[i]);
				if (i==15)
					printf("\n\t\t\t\t\t");
			}

			printf("\n");
		} else {
			printf("not visible with current security settings\n");
		}
	}

	if (print_otp) {
		if (state->use_otp_ext)
			print_otp_ext_info(&state->otp_ext);
		else
			print_otp_info(&state->otp);
	}
}

static void print_security_config_gen6(struct switchtec_security_cfg_state_gen6 *state)
{
	printf("----------------- Security Configuration ------------------\n");
	printf("I2C Bus: \t\t\t\t%d\n", state->twi_rcvry_bus);
	printf("I2C Address (7-bits) MRPC: \t\t0x%02X\n",
	       (state->twi_rcvry_address_mrpc == 0x0) ?
	       (0xD4 >> 1) : state->twi_rcvry_address_mrpc);
	printf("I2C Address (7-bits) OCP: \t\t0x%02X\n",
	       (state->twi_rcvry_address_ocp == 0x0) ?
	       (0xD2 >> 1) : state->twi_rcvry_address_ocp);
	printf("I2C Command Map: \t\t\t0x%03X\n",
	       state->mrpc_command_map & 0xFFF);
	printf("Device SECSC: \t\t\t\t0x%01x\n", state->secsc);

	/* Print key hashes */
	uint32_t *state_ptr = (uint32_t *)state;
	uint32_t dw7 = state_ptr[7];
	for (int key = 0; key < SWITCHTEC_KMSK_NUM_GEN6; key++) {
		int shift = 4 + key * 2;
		uint32_t status = (dw7 >> shift) & 0x3;

		printf("OTP Key%d Hash:\t\t\t\t", key + 1);

		switch (status) {
		case PROGRAMMED:
			for (int j = 0; j < SWITCHTEC_KMSK_LEN_DWORDS; j++) {
				if (j && j % 8 == 0)
					printf("\n\t\t\t\t\t");
				printf("%08x", be32toh(state->otp_key_hash[key][j]));
			}
			printf("\n");
			break;

		case REVOKED:
			printf("Revoked\n");
			break;

		case UNPROGRAMMED:
			printf("Available\n");
			break;

		default:
			printf("Invalid\n");
			break;
		}
	}

	/* Print MHCP inbuilt key hash status */
	if (state->rom_key_1_disable)
		printf("MHCP Inbuilt Key0\t\t\tRevoked\n");
	else
		printf("MHCP Inbuilt Key0\t\t\tActive\n");
	if (state->rom_key_2_disable)
		printf("MHCP Inbuilt Key1\t\t\tRevoked\n");
	else
		printf("MHCP Inbuilt Key1\t\t\tActive\n");
	if (state->rom_key_3_disable)
		printf("MHCP Inbuilt Key2\t\t\tRevoked\n");
	else
		printf("MHCP Inbuilt Key2\t\t\tActive\n");
	if (state->rom_key_4_disable)
		printf("MHCP Inbuilt Key3\t\t\tRevoked\n");
	else
		printf("MHCP Inbuilt Key3\t\t\tActive\n");
}

static void print_security_cfg_set(struct switchtec_security_cfg_set *set)
{
	int i;

	printf("\nBasic Secure Settings\n");

	printf("\tJTAG/EJTAG State After Reset: \t%d\n",
		set->jtag_lock_after_reset);

	printf("\tJTAG/EJTAG State After BL1: \t%d\n",
		set->jtag_lock_after_bl1);

	printf("\tJTAG/EJTAG Unlock IN BL1: \t%d\n",
		set->jtag_bl1_unlock_allowed);

	printf("\tJTAG/EJTAG Unlock AFTER BL1: \t%d\n",
		set->jtag_post_bl1_unlock_allowed);

	printf("\tSPI Clock Rate: \t\t%.2f MHz\n", set->spi_clk_rate);

	printf("\tI2C Recovery TMO: \t\t%d Second(s)\n",
		set->i2c_recovery_tmo);

	printf("\tI2C Port: \t\t\t%d\n", set->i2c_port);
	printf("\tI2C Address (7-bits): \t\t0x%02x\n", set->i2c_addr);
	printf("\tI2C Command Map: \t\t0x%08x\n", set->i2c_cmd_map);
	if (set->attn_set.attestation_mode !=
	    SWITCHTEC_ATTESTATION_MODE_NOT_SUPPORTED) {
		if (set->attn_set.attestation_mode ==
		    SWITCHTEC_ATTESTATION_MODE_DICE) {
			printf("\tAttestation:\t\t\tEnabled, with UDS ");
			if (set->attn_set.uds_selfgen)
				printf("Self-Generated by Device\n");
			else
				printf("Provided by User\n");
		} else {
			printf("\tAttestation: \t\t\tDisabled\n");
		}
	}

	printf("Exponent Hex Data: \t\t\t0x%08x\n", set->public_key_exponent);

	if (set->attn_set.attestation_mode ==
	    SWITCHTEC_ATTESTATION_MODE_DICE) {
		printf("CDI eFuse Include Mask: \t\t0x%08x\n",
			set->attn_set.cdi_efuse_inc_mask);

		printf("UDS Data: \t\t\t\t");
		if (set->attn_set.uds_valid) {
			for (i = 0; i < 32; i++) {
				printf("%02x",
				       set->attn_set.uds_data[i]);
				if (i == 15)
					printf("\n\t\t\t\t\t");
			}

			printf("\n");
		} else {
			printf("not set\n");
		}
	}
}

#define CMD_DESC_INFO "display security settings"

static int info(int argc, char **argv)
{
	int ret;
	enum switchtec_boot_phase phase_id;

	struct switchtec_sn_ver_info sn_info = {};

	static struct {
		struct switchtec_dev *dev;
		int verbose;
	} cfg = {};

	const struct argconfig_options opts[] = {
		DEVICE_OPTION_MFG_PCI,
		{"verbose", 'v', "", CFG_NONE, &cfg.verbose, no_argument,
		 "print additional chip information"},
		{NULL}};

	union {
		struct switchtec_security_cfg_state state;
		struct switchtec_security_cfg_state_gen6 state_gen6;
	} u = {};
	struct switchtec_security_cfg_state *state = &u.state;
	struct switchtec_security_cfg_state_gen6 *state_gen6 = &u.state_gen6;

	argconfig_parse(argc, argv, CMD_DESC_INFO, opts, &cfg, sizeof(cfg));

	phase_id = switchtec_boot_phase(cfg.dev);
	printf("Current Boot Phase: \t\t\t%s\n",
	       switchtec_phase_id_str(phase_id));

	if (switchtec_is_gen6(cfg.dev)) {
		enum switchtec_secure_state_gen6 device_state;
		ret = switchtec_secure_state_get_gen6(cfg.dev, &device_state);
		if (ret == 0) {
			printf("Device State: \t\t\t\t");
			switch (device_state) {
			case SWITCHTEC_GEN6_UNINITIALIZED_SECURE_CAPABLE:
				printf("SECURE-CAPABLE\n");
				break;
			case SWITCHTEC_GEN6_UNPROVISIONED_SECURED:
				printf("ALWAYS-SECURED\n");
				break;
			case SWITCHTEC_GEN6_INITIALIZED_SECURED:
				printf("INITIALIZED-SECURED\n");
				break;
			case SWITCHTEC_GEN6_INITIALIZED_UNSECURED:
				printf("INITIALIZED-UNSECURED\n");
				break;
			default:
				printf("UNKNOWN (0x%x)\n", device_state);
			}
		}
		uint32_t jtag_status;
		ret = switchtec_dbg_unlock_status_get_gen6(cfg.dev, &jtag_status);
		if (ret == 0)
			printf("JTAG Port: \t\t\t\t%s\n",
			       (jtag_status & 1) ? "Enabled" : "Disabled");
		else
			printf("JTAG Port: \t\t\t\tUnknown\n");
	}

	ret = switchtec_sn_ver_get(cfg.dev, &sn_info);
	if (ret) {
		switchtec_perror("mfg info");
		return ret;
	}

	if (switchtec_is_gen6(cfg.dev)) {

		char *status[4] = {"Unprogrammed", "Programmed", "Locked", "Revoked"};

		printf("----------------- UID info --------------------------------\n");
		printf("Device Unique ID: \t\t\t0x");
		printf("%08X\n", *sn_info.UID);
		sn_info.UID++;
		for (int i = 1; i < SWITCHTEC_UID_DWORD_S; i++) {
			printf("\t\t\t\t\t  %08X\n", *sn_info.UID);
			sn_info.UID++;
		}
		printf("Status: \t\t\t\t%s\n", status[(sn_info.PSID_UID_valid_flags >> 4) & 0x3]);
		printf("Mask Read Mask Enable: \t\t\t0x%0x\n", (sn_info.PSID_UID_valid_flags >> 6) & 0x1);
		printf("Read Mask Request Enable: \t\t0x%0x\n", (sn_info.PSID_UID_valid_flags >> 7) & 0x1);
		printf("----------------- PSID info -------------------------------\n");
		printf("Device PSID: \t\t\t\t0x");
		printf("%08X\n", *sn_info.PSID0);
		sn_info.PSID0++;
		for (int i = 1; i < SWITCHTEC_PSID_DWORD_S; i++) {
			printf("\t\t\t\t\t  %08X\n", *sn_info.PSID0);
			sn_info.PSID0++;
		}
		printf("Status: \t\t\t\t%s\n", status[(sn_info.PSID_UID_valid_flags) & 0x3]);
		printf("Read Mask Enable: \t\t\t0x%0x\n", (sn_info.PSID_UID_valid_flags >> 2) & 0x1);
		printf("Read Mask Request Enable: \t\t0x%0x\n", (sn_info.PSID_UID_valid_flags >> 3) & 0x1);
		printf("----------------- Image info ------------------------------\n");
		printf("BL2 Secure Version: \t\t\t0x%08x\n", sn_info.ver_bl2);
		printf("Main Secure Version: \t\t\t0x%08x\n", sn_info.ver_main);
		printf("Debug Token Secure Version: \t\t0x%08x\n", sn_info.dbg_tok_sec_ver_rsvrd);
		printf("KMT Secure Version: \t\t\t0x%08x\n", sn_info.kmt_sec_ver_rsvrd);
	} else {
		printf("Chip Serial: \t\t\t\t0x%08x\n", sn_info.chip_serial);
		printf("Key Manifest Secure Version: \t\t0x%08x\n", sn_info.ver_km);
		if (sn_info.riot_ver_valid)
			printf("RIOT Secure Version: \t\t\t0x%08x\n",
				sn_info.ver_riot);
		printf("BL2 Secure Version: \t\t\t0x%08x\n", sn_info.ver_bl2);
		printf("Main Secure Version: \t\t\t0x%08x\n", sn_info.ver_main);
		printf("Secure Unlock Version: \t\t\t0x%08x\n", sn_info.ver_sec_unlock);
	}

	if (!switchtec_is_gen6(cfg.dev) && phase_id == SWITCHTEC_BOOT_PHASE_BL2) {
		printf("\nOther secure settings are only shown in the BL1 or Main Firmware phase.\n\n");
		return 0;
	}

	if (switchtec_is_gen6(cfg.dev) && (phase_id == SWITCHTEC_BOOT_PHASE_BL1)) {
		ret = security_settings_get_gen6(cfg.dev, state_gen6);
		if (ret) {
			switchtec_perror("mfg info");
			return ret;
		}
	} else {
		ret = switchtec_security_config_get(cfg.dev, &u);
		if (ret) {
			switchtec_perror("mfg info");
			return ret;
		}
	}

	if (cfg.verbose) {
		if (switchtec_is_gen6(cfg.dev))
			print_security_config_gen6(state_gen6);
		else {
			if (!state->otp_valid) {
				print_security_config(state, false);
				fprintf(stderr,
					"\nAdditional (verbose) chip info is not available on this chip!\n\n");
			} else if (switchtec_gen(cfg.dev) == SWITCHTEC_GEN4 &&
				phase_id != SWITCHTEC_BOOT_PHASE_FW) {
				print_security_config(state, false);
				fprintf(stderr,
					"\nAdditional (verbose) chip info is only available in the Main Firmware phase!\n\n");
			} else {
				print_security_config(state, true);
			}
		}
		return 0;
	}

	if (switchtec_is_gen6(cfg.dev))
		print_security_config_gen6(state_gen6);
	else
		print_security_config(state, false);

	return 0;
}

#define CMD_DESC_MAILBOX "retrieve mailbox logs"

static int mailbox(int argc, char **argv)
{
	int ret;

	static struct {
		struct switchtec_dev *dev;
		int out_fd;
		const char *out_filename;
	} cfg = {};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION_MFG_PCI,
		{"filename", .cfg_type=CFG_FD_WR, .value_addr=&cfg.out_fd,
		  .argument_type=optional_positional,
		  .force_default="switchtec_mailbox.log",
		  .help="file to log mailbox data"},
		{NULL}
	};

	argconfig_parse(argc, argv, CMD_DESC_MAILBOX, opts, &cfg, sizeof(cfg));

	ret = switchtec_mailbox_to_file(cfg.dev, cfg.out_fd);
	if (ret) {
		switchtec_perror("mfg mailbox");
		close(cfg.out_fd);
		return ret;
	}

	close(cfg.out_fd);

	fprintf(stderr, "\nLog saved to %s.\n", cfg.out_filename);

	return 0;
}

static void print_image_list(struct switchtec_active_index *idx)
{
	printf("IMAGE\t\tINDEX\n");
	printf("Key Manifest\t%d\n", idx->keyman);
	if (idx->riot != SWITCHTEC_ACTIVE_INDEX_NOT_SET)
		printf("RIOT\t\t%d\n", idx->riot);
	printf("BL2\t\t%d\n", idx->bl2);
	printf("Config\t\t%d\n", idx->config);
	printf("Firmware\t%d\n", idx->firmware);
}

#define CMD_DESC_IMAGE_LIST "display active image list (BL1 only)"

static int image_list(int argc, char **argv)
{
	int ret;
	struct switchtec_active_index index;

	static struct {
		struct switchtec_dev *dev;
	} cfg = {};

	const struct argconfig_options opts[] = {
		DEVICE_OPTION_MFG,
		{NULL}
	};

	argconfig_parse(argc, argv, CMD_DESC_IMAGE_LIST, opts, &cfg, sizeof(cfg));

	if (switchtec_is_gen6(cfg.dev)) {
		fprintf(stderr, "This command is not supported on Gen6 switches\n");
		return -1;
	}

	if (switchtec_boot_phase(cfg.dev) != SWITCHTEC_BOOT_PHASE_BL1) {
		fprintf(stderr, "This command is only available in BL1!\n");
		return -1;
	}

	ret = switchtec_active_image_index_get(cfg.dev, &index);
	if (ret) {
		switchtec_perror("image list");
		return ret;
	}

	print_image_list(&index);

	return 0;
}

#define CMD_DESC_IMAGE_SELECT "select active image index (BL1 only)"

static int image_select(int argc, char **argv)
{
	int ret;
	struct switchtec_active_index index;

	static struct {
		struct switchtec_dev *dev;
		unsigned char bl2;
		unsigned char firmware;
		unsigned char config;
		unsigned char keyman;
		unsigned char riot;
	} cfg = {
		.bl2 = SWITCHTEC_ACTIVE_INDEX_NOT_SET,
		.firmware = SWITCHTEC_ACTIVE_INDEX_NOT_SET,
		.config = SWITCHTEC_ACTIVE_INDEX_NOT_SET,
		.keyman = SWITCHTEC_ACTIVE_INDEX_NOT_SET,
		.riot = SWITCHTEC_ACTIVE_INDEX_NOT_SET
	};

	const struct argconfig_options opts[] = {
		DEVICE_OPTION_MFG,
		{"bl2", 'b', "", CFG_BYTE, &cfg.bl2,
			required_argument, "active image index for BL2"},
		{"firmware", 'm', "", CFG_BYTE, &cfg.firmware,
			required_argument, "active image index for FIRMWARE"},
		{"config", 'c', "", CFG_BYTE, &cfg.config,
			required_argument, "active image index for CONFIG"},
		{"keyman", 'k', "", CFG_BYTE, &cfg.keyman, required_argument,
			"active image index for KEY MANIFEST"},
		{"riot", 'r', "", CFG_BYTE, &cfg.riot, required_argument,
			"active image index for RIOT (Gen5 device only)"},
		{NULL}
	};

	argconfig_parse(argc, argv, CMD_DESC_IMAGE_SELECT, opts, &cfg, sizeof(cfg));

	if (switchtec_is_gen6(cfg.dev)) {
		fprintf(stderr, "This command is not supported on Gen6 switches\n");
		return -1;
	}

	if (cfg.bl2 == SWITCHTEC_ACTIVE_INDEX_NOT_SET &&
	    cfg.firmware == SWITCHTEC_ACTIVE_INDEX_NOT_SET &&
	    cfg.config == SWITCHTEC_ACTIVE_INDEX_NOT_SET &&
	    cfg.keyman == SWITCHTEC_ACTIVE_INDEX_NOT_SET &&
	    cfg.riot == SWITCHTEC_ACTIVE_INDEX_NOT_SET) {
		fprintf(stderr,
			"One of BL2, Config, Key Manifest, RIOT or Firmware indices must be set in this command!\n");
		return -1;
	}

	if (switchtec_boot_phase(cfg.dev) != SWITCHTEC_BOOT_PHASE_BL1) {
		fprintf(stderr,
			"This command is only available in BL1!\n");
		return -2;
	}

	if (cfg.bl2 > 1 && cfg.bl2 != SWITCHTEC_ACTIVE_INDEX_NOT_SET) {
		fprintf(stderr, "Active index of BL2 must be within 0-1!\n");
		return -3;
	}
	index.bl2 = cfg.bl2;

	if (cfg.firmware > 1 &&
	    cfg.firmware != SWITCHTEC_ACTIVE_INDEX_NOT_SET) {
		fprintf(stderr,
			"Active index of FIRMWARE must be within 0-1!\n");
		return -4;
	}
	index.firmware = cfg.firmware;

	if (cfg.config > 1 && cfg.config != SWITCHTEC_ACTIVE_INDEX_NOT_SET) {
		fprintf(stderr,
			"Active index of CONFIG must be within 0-1!\n");
		return -5;
	}
	index.config = cfg.config;

	if (cfg.keyman > 1 && cfg.keyman != SWITCHTEC_ACTIVE_INDEX_NOT_SET) {
		fprintf(stderr,
			"Active index of KEY MANIFEST must be within 0-1!\n");
		return -6;
	}
	index.keyman = cfg.keyman;

	if (!switchtec_is_gen5(cfg.dev) &&
	    cfg.riot != SWITCHTEC_ACTIVE_INDEX_NOT_SET) {
		fprintf(stderr,
			"RIOT image is only available on Gen5 devices!\n");
		return -7;
	}

	if (switchtec_is_gen5(cfg.dev)) {
		if (cfg.riot > 1 &&
		    cfg.riot != SWITCHTEC_ACTIVE_INDEX_NOT_SET) {
			fprintf(stderr,
				"Active index of RIOT must be within 0-1!\n");
			return -8;
		}

		index.riot = cfg.riot;
	}

	ret = switchtec_active_image_index_set(cfg.dev, &index);
	if (ret) {
		switchtec_perror("image select");
		return ret;
	}

	return ret;
}

#define CMD_DESC_BOOT_RESUME "resume device boot process (BL1 and BL2 only)"

static int boot_resume(int argc, char **argv)
{
	const char *desc = CMD_DESC_BOOT_RESUME "\n\n"
			   "A normal device boot process includes the BL1, "
			   "BL2 and Main Firmware boot phases. In the case "
			   "when the boot process is paused at the BL1 or BL2 phase "
			   "(due to boot failure or BOOT_RECOVERY PIN[0:1] "
			   "being set to LOW), sending this command requests "
			   "the device to try resuming a normal boot process.\n\n"
			   "NOTE: if your system does not support hotplug, "
			   "your device might not be immediately accessible "
			   "after a normal boot process. In this case, be sure "
			   "to reboot your system after sending this command.";
	int ret;

	static struct {
		struct switchtec_dev *dev;
		int assume_yes;
	} cfg = {};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION_MFG,
		{"yes", 'y', "", CFG_NONE, &cfg.assume_yes, no_argument,
		 "assume yes when prompted"},
		{NULL}
	};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));
	if (switchtec_is_gen6(cfg.dev)) {
		fprintf(stderr, "Boot resume is not supported on Gen6 switches\n");
		return -1;
	}

	if (switchtec_boot_phase(cfg.dev) == SWITCHTEC_BOOT_PHASE_FW) {
		fprintf(stderr,
			"This command is only available in BL1 or BL2!\n");
		return -1;
	}

	if (!cfg.assume_yes)
		fprintf(stderr,
			"WARNING: if your system does not support hotplug,\n"
			"your device might not be immediately accessible\n"
			"after a normal boot process. In this case, be sure\n"
			"to reboot your system after sending this command.\n\n");

	ret = ask_if_sure(cfg.assume_yes);
	if (ret)
		return ret;

	ret = switchtec_boot_resume(cfg.dev);
	if (ret) {
		switchtec_perror("mfg boot-resume");
		return ret;
	}

	return 0;
}

#define CMD_DESC_FW_TRANSFER "transfer a firmware image to device (BL1 only)"

static int fw_transfer(int argc, char **argv)
{
	const char *desc = CMD_DESC_FW_TRANSFER "\n\n"
			   "This command only supports transferring a firmware "
			   "image when the device is in the BL1 boot phase. Use "
			   "'fw-execute' after this command to excute the "
			   "transferred image. Note that the image is stored "
			   "in device RAM and is lost after device reboot.\n\n"
			   "To update an image in the BL2 or MAIN boot phase, use "
			   "the 'fw-update' command instead.\n\n"
			   BOOT_PHASE_HELP_TEXT;
	int ret;
	enum switchtec_fw_type type;

	static struct {
		struct switchtec_dev *dev;
		FILE *fimg;
		const char *img_filename;
		int assume_yes;
		int force;
		int no_progress_bar;
	} cfg = {};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION_MFG,
		{"img_file", .cfg_type=CFG_FILE_R, .value_addr=&cfg.fimg,
			.argument_type=required_positional,
			.help="firmware image file to transfer"},
		{"yes", 'y', "", CFG_NONE, &cfg.assume_yes, no_argument,
			"assume yes when prompted"},
		{"force", 'f', "", CFG_NONE, &cfg.force, no_argument,
			"force interrupting an existing fw-update command "
			"in case firmware is stuck in a busy state"},
		{"no-progress", 'p', "", CFG_NONE, &cfg.no_progress_bar,
			no_argument, "don't print progress to stdout"},
		{NULL}
	};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));

	if (switchtec_boot_phase(cfg.dev) != SWITCHTEC_BOOT_PHASE_BL1) {
		fprintf(stderr,
			"This command is only available in the BL1 boot phase!\n");
		fprintf(stderr,
			"Use 'fw-update' instead to update an image in other boot phases.\n");
		return -1;
	}

	printf("Writing the following firmware image to %s:\n",
		switchtec_name(cfg.dev));

	type = check_and_print_fw_image(fileno(cfg.fimg), cfg.img_filename);

	if (type != SWITCHTEC_FW_TYPE_BL2) {
		fprintf(stderr,
			"This command only supports transferring a BL2 image.\n");
		return -2;
	}

	ret = ask_if_sure(cfg.assume_yes);
	if (ret) {
		fclose(cfg.fimg);
		return ret;
	}

	progress_start();
	if (cfg.no_progress_bar)
		ret = switchtec_fw_write_file(cfg.dev, cfg.fimg, 1, cfg.force,
					      NULL);
	else
		ret = switchtec_fw_write_file(cfg.dev, cfg.fimg, 1, cfg.force,
					      progress_update);
	fclose(cfg.fimg);

	if (ret) {
		printf("\n");
		switchtec_fw_perror("mfg fw-transfer", ret);
		return -3;
	}

	progress_finish(cfg.no_progress_bar);
	printf("\n");

	return 0;
}

#define CMD_DESC_FW_EXECUTE "execute previously transferred firmware image (BL1 only)"

static int fw_execute(int argc, char **argv)
{
	const char *desc = CMD_DESC_FW_EXECUTE "\n\n"
			   "This command is only supported when the device is "
			   "in the BL1 boot phase. The firmware image must have "
			   "been transferred using the 'fw-transfer' command. "
			   "After firmware initializes, it listens for activity from "
			   "I2C, UART (XModem), or both interfaces for input. "
			   "Once activity is detected from an interface, "
			   "firmware falls into recovery mode on that interface. "
			   "The interface to listen on can be specified using "
			   "the 'bl2_recovery_mode' option. \n\n"
			   "To activate an image in the BL2 or MAIN boot "
			   "phase, use the 'fw-toggle' command instead.\n\n"
			   BOOT_PHASE_HELP_TEXT;
	int ret;

	static struct {
		struct switchtec_dev *dev;
		int assume_yes;
		enum switchtec_bl2_recovery_mode bl2_rec_mode;
	} cfg = {
		.bl2_rec_mode = SWITCHTEC_BL2_RECOVERY_I2C_AND_XMODEM
	};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION_MFG,
		{"yes", 'y', "", CFG_NONE, &cfg.assume_yes, no_argument,
			"assume yes when prompted"},
		{"bl2_recovery_mode", 'm', "MODE",
			CFG_CHOICES, &cfg.bl2_rec_mode,
			required_argument, "BL2 recovery mode",
			.choices = recovery_mode_choices},
		{NULL}
	};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));

	if (switchtec_boot_phase(cfg.dev) != SWITCHTEC_BOOT_PHASE_BL1) {
		fprintf(stderr,
			"This command is only available in the BL1 phase!\n");
		return -2;
	}

	if (!cfg.assume_yes)
		printf("This command will execute the previously transferred image.\n");
	ret = ask_if_sure(cfg.assume_yes);
	if (ret) {
		return ret;
	}

	ret = switchtec_fw_exec(cfg.dev, cfg.bl2_rec_mode);
	if (ret) {
		switchtec_fw_perror("mfg fw-execute", ret);
		return ret;
	}

	return 0;
}

#define CMD_DESC_STATE_SET "set device secure state (BL1 and Main Firmware only)"

static int state_set(int argc, char **argv)
{
	int ret;
	struct switchtec_security_cfg_state state = {};

	const char *desc = CMD_DESC_STATE_SET "\n\n"
			   "This command can only be used when the device "
			   "secure state is UNINITIALIZED_UNSECURED.\n\n"
			   "NOTE - A device can be in one of these "
			   "three secure states: \n"
			   "UNINITIALIZED_UNSECURED: this is the default state "
			   "when the chip is shipped. All security-related settings "
			   "are 'uninitialized', and the chip is in the 'unsecured' "
			   "state. \n"
			   "INITIALIZED_UNSECURED: this is the state when "
			   "security-related settings are 'initialized', and "
			   "the chip is set to the 'unsecured' state. \n"
			   "INITIALIZED_SECURED: this is the state when "
			   "security-related settings are 'initialized', and "
			   "the chip is set to the 'secured' state. \n\n"
			   "Use 'config-set' or other mfg commands to "
			   "initialize security settings when the chip is in "
			   "the UNINITIALIZED_UNSECURED state, then use this "
			   "command to switch the chip to the INITIALIZED_SECURED "
			   "or INITIALIZED_UNSECURED state. \n\n"
			   "WARNING: ONCE THE CHIP STATE IS SUCCESSFULLY SET, "
			   "IT CAN NO LONGER BE CHANGED. USE CAUTION WHEN ISSUING "
			   "THIS COMMAND.";

	static struct {
		struct switchtec_dev *dev;
		enum switchtec_secure_state state;
		int assume_yes;
	} cfg = {
		.state = SWITCHTEC_SECURE_STATE_UNKNOWN,
	};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION_MFG_PCI,
		{"state", 't', "state",
			CFG_CHOICES, &cfg.state,
			required_argument, "secure state",
			.choices=secure_state_choices},
		{"yes", 'y', "", CFG_NONE, &cfg.assume_yes, no_argument,
		 "assume yes when prompted"},
		{NULL}
	};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));

	if (cfg.state == SWITCHTEC_SECURE_STATE_UNKNOWN) {
		fprintf(stderr,
			"Secure state must be set in this command!\n");
		return -1;
	}

	if (switchtec_boot_phase(cfg.dev) == SWITCHTEC_BOOT_PHASE_BL2) {
		fprintf(stderr,
			"This command is only available in BL1 or Main Firmware!\n");
		return -2;
	}

	ret = switchtec_security_config_get(cfg.dev, &state);
	if (ret) {
		switchtec_perror("mfg state-set");
		return ret;
	}
	if (state.secure_state != SWITCHTEC_UNINITIALIZED_UNSECURED) {
		fprintf(stderr,
			"This command is only valid when secure state is UNINITIALIZED_UNSECURED!\n");
		return -3;
	}

	print_security_config(&state, false);

	if (!cfg.assume_yes) {
		fprintf(stderr,
			"\nWARNING: This operation makes changes to the device OTP memory and is IRREVERSIBLE!\n");

		ret = ask_if_sure(cfg.assume_yes);
		if (ret)
			return -4;
	}

	ret = switchtec_secure_state_set(cfg.dev, cfg.state);
	if (ret) {
		switchtec_perror("mfg state-set");
		return ret;
	}

	return 0;
}

#define CMD_DESC_CONFIG_SET "set device security settings (BL1 and Main Firmware only)"

static int config_set(int argc, char **argv)
{
	int ret;
	struct switchtec_security_cfg_state state = {};
	struct switchtec_security_cfg_set settings = {};
	struct switchtec_uds uds_data = {};

	const char *desc = CMD_DESC_CONFIG_SET "\n\n"
			   "The security settings programmed with this command "
			   "will not take effect until the chip is set to either "
			   "INITIALIZED_UNSECURED or INITIALIZED_SECURED state.";

	static struct {
		struct switchtec_dev *dev;
		FILE *setting_fimg;
		char *setting_file;
		FILE *uds_fimg;
		char *uds_file;
		int show_only;
		int assume_yes;
	} cfg = {};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION_MFG_PCI,
		{"setting_file", .cfg_type=CFG_FILE_R,
			.value_addr=&cfg.setting_fimg,
			.argument_type=required_positional,
			.help="security setting file"},
		{"uds_file", 'u', .cfg_type=CFG_FILE_R,
			.value_addr=&cfg.uds_fimg,
			.argument_type=required_argument,
			.help="UDS file"},
		{"show-settings-only", 's', "", CFG_NONE, &cfg.show_only, no_argument,
			"Show secure settings without programming"},
		{"yes", 'y', "", CFG_NONE, &cfg.assume_yes, no_argument,
			"assume yes when prompted"},
		{NULL}
	};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));

	if (switchtec_boot_phase(cfg.dev) == SWITCHTEC_BOOT_PHASE_BL2) {
		fprintf(stderr,
			"This command is only available in BL1 or Main Firmware!\n");
		return -1;
	}

	ret = switchtec_security_config_get(cfg.dev, &state);
	if (ret) {
		switchtec_perror("mfg config-set");
		return ret;
	}
	if (state.secure_state != SWITCHTEC_UNINITIALIZED_UNSECURED) {
		fprintf(stderr,
			"This command is only available when secure state is UNINITIALIZED_UNSECURED!\n");
		return -2;
	}

	ret = switchtec_read_sec_cfg_file(cfg.dev, cfg.setting_fimg,
					  &settings);
	fclose(cfg.setting_fimg);
	if (ret == -EBADF) {
		fprintf(stderr, "Invalid secure setting file: %s!\n",
			cfg.setting_file);
		return -3;
	} else if (ret == -ENODEV) {
		fprintf(stderr, "The security setting file is for a different generation of Switchtec device!\n");
		return -5;
	} else if (ret == -EINVAL) {
		fprintf(stderr, "Invalid SPI Clock Rate value specified in the security setting file!\n");
		return -6;
	} else if (ret) {
		switchtec_perror("mfg config-set");
	}

	if (cfg.uds_fimg) {
		if (settings.attn_set.attestation_mode !=
		    SWITCHTEC_ATTESTATION_MODE_DICE) {
			fprintf(stderr, "INFO: Attestation is not supported or not enabled. The given UDS file is ignored.\n");
		} else if (settings.attn_set.uds_selfgen) {
			fprintf(stderr, "INFO: Device uses self-generated UDS. The given UDS file is ignored.\n");
		} else {
			ret = switchtec_read_uds_file(cfg.uds_fimg, &uds_data);
			if (ret) {
				fprintf(stderr, "Error reading UDS file %s\n",
					cfg.uds_file);
				return -6;
			}
			memcpy(settings.attn_set.uds_data, uds_data.uds,
			       SWITCHTEC_UDS_LEN);
			settings.attn_set.uds_valid = true;
		}
	} else {
		if ((settings.attn_set.attestation_mode ==
		     SWITCHTEC_ATTESTATION_MODE_DICE) &&
		    !settings.attn_set.uds_selfgen) {
			fprintf(stderr, "ERROR: UDS file is required for the current configuration!\n");
			return -7;
		}
	}

	if (cfg.show_only)
		printf("Secure settings for device: \n");
	else
		printf("Writing the below settings to device: \n");
	print_security_cfg_set(&settings);

	if (cfg.show_only)
		return 0;

	if (!cfg.assume_yes)
		fprintf(stderr,
			"\nWARNING: This operation makes changes to the device OTP memory and is IRREVERSIBLE!\n");
	ret = ask_if_sure(cfg.assume_yes);
	if (ret)
		return -4;

	ret = switchtec_security_config_set(cfg.dev, &settings);
	if (ret) {
		switchtec_perror("mfg config-set");
		return ret;
	}

	return 0;
}

#define CMD_DESC_KMSK_ENTRY_ADD "add a KSMK entry (BL1 and Main Firmware only)"

#if HAVE_LIBCRYPTO
static int kmsk_entry_add(int argc, char **argv)
{
	int i;
	int ret;
	struct switchtec_kmsk kmsk;
	struct switchtec_pubkey pubk;
	struct switchtec_signature sig;
	struct switchtec_security_cfg_state state = {};

	const char *desc = CMD_DESC_KMSK_ENTRY_ADD "\n\n"
			   "KMSK stands for Key Manifest Secure Key. It is a "
			   "key used to verify the Key Manifest partition, which "
			   "contains keys used to verify all other partitions.\n";
	static struct {
		struct switchtec_dev *dev;
		FILE *pubk_fimg;
		char *pubk_file;
		FILE *sig_fimg;
		char *sig_file;
		FILE *kmsk_fimg;
		char *kmsk_file;
		int assume_yes;
	} cfg = {};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION_MFG_PCI,
		{"pub_key_file", 'p', .cfg_type=CFG_FILE_R,
			.value_addr=&cfg.pubk_fimg,
			.argument_type=required_argument,
			.help="public key file"},
		{"signature_file", 's', .cfg_type=CFG_FILE_R,
			.value_addr=&cfg.sig_fimg,
			.argument_type=required_argument,
			.help="signature file"},
		{"kmsk_entry_file", 'k', .cfg_type=CFG_FILE_R,
			.value_addr=&cfg.kmsk_fimg,
			.argument_type=required_argument,
			.help="KMSK entry file"},
		{"yes", 'y', "", CFG_NONE, &cfg.assume_yes, no_argument,
		 "assume yes when prompted"},
		{NULL}
	};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));

	if (cfg.kmsk_file == NULL) {
		fprintf(stderr,
			"KSMK entry file must be set in this command!\n");
		return -1;
	}

	if (switchtec_boot_phase(cfg.dev) == SWITCHTEC_BOOT_PHASE_BL2) {
		fprintf(stderr,
			"This command is only available in BL1 or Main Firmware!\n");
		return -2;
	}

	ret = switchtec_security_config_get(cfg.dev, &state);
	if (ret) {
		switchtec_perror("mfg ksmk-entry-add");
		return ret;
	}
	if (state.secure_state == SWITCHTEC_INITIALIZED_UNSECURED) {
		fprintf(stderr,
			"This command is only valid when secure state is not INITIALIZED_UNSECURED!\n");
		return -3;
	}

	ret = switchtec_read_kmsk_file(cfg.kmsk_fimg, &kmsk);
	fclose(cfg.kmsk_fimg);
	if (ret) {
		fprintf(stderr, "Invalid KMSK file %s!\n", cfg.kmsk_file);
		return -4;
	}

	if (switchtec_security_state_has_kmsk(&state, &kmsk)) {
		fprintf(stderr,
			"REJECTED: the specified KMSK entry already exists on the device!\n");
		return -8;
	}

	if (state.secure_state == SWITCHTEC_INITIALIZED_SECURED &&
	   cfg.pubk_file == NULL) {
		fprintf(stderr,
			"Public key file must be specified when secure state is INITIALIZED_SECURED!\n");
		return -4;
	}

	if (cfg.pubk_file) {
		ret = switchtec_read_pubk_file(cfg.pubk_fimg, &pubk);
		fclose(cfg.pubk_fimg);

		if (ret) {
			fprintf(stderr, "Invalid public key file %s!\n",
				cfg.pubk_file);
			return -5;
		}
	}

	if (state.secure_state == SWITCHTEC_INITIALIZED_SECURED &&
	   cfg.sig_file == NULL) {
		fprintf(stderr,
			"Signature file must be specified when secure state is INITIALIZED_SECURED!\n");
		return -5;
	}

	if (cfg.sig_file) {
		ret = switchtec_read_signature_file(cfg.sig_fimg, &sig);
		fclose(cfg.sig_fimg);

		if (ret) {
			fprintf(stderr, "Invalid signature file %s!\n",
				cfg.sig_file);
			return -6;
		}
	}

	printf("Adding the following KMSK entry to device:\n");
	for (i = 0; i < SWITCHTEC_KMSK_LEN; i++)
		printf("%02x", kmsk.kmsk[i]);
	printf("\n");

	if (!cfg.assume_yes)
		fprintf(stderr,
			"\nWARNING: This operation makes changes to the device OTP memory and is IRREVERSIBLE!\n");
	ret = ask_if_sure(cfg.assume_yes);
	if (ret)
		return -7;

	if (state.secure_state == SWITCHTEC_INITIALIZED_SECURED) {
		ret = switchtec_kmsk_set(cfg.dev, &pubk, &sig, &kmsk);

	}
	else {
		ret = switchtec_kmsk_set(cfg.dev, NULL,	NULL, &kmsk);
	}

	if (ret)
		switchtec_perror("mfg kmsk-entry-add");

	return ret;
}
#endif

#define CMD_DESC_DEBUG_UNLOCK "unlock firmware debug features"

#if HAVE_LIBCRYPTO
static int debug_unlock(int argc, char **argv)
{
	int ret;
	struct switchtec_pubkey pubk;
	struct switchtec_signature sig;
	struct switchtec_gen6_token token;

	const char *desc = CMD_DESC_DEBUG_UNLOCK "\n\n"
			   "This command unlocks the EJTAG port, Command Line "
			   "Interface (CLI), MRPC command and Global Address "
			   "Space (GAS) registers.";
	static struct {
		struct switchtec_dev *dev;
		FILE *pubkey_fimg;
		char *pubkey_file;
		unsigned long unlock_version;
		unsigned long serial;
		FILE *sig_fimg;
		char *sig_file;
		FILE *tkn_fimg;
		char *tkn_file;
	} cfg = {
		.unlock_version = 0xffff,
	};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION_MFG_PCI,
		{"pub_key", 'p', .cfg_type=CFG_FILE_R,
			.value_addr=&cfg.pubkey_fimg,
			.argument_type=required_argument,
			.help="public key file"},
		{"serial_number", 'n', .cfg_type=CFG_LONG,
			.value_addr=&cfg.serial,
			.argument_type=required_argument,
			.help="device serial number"},
		{"unlock_version", 'v', .cfg_type=CFG_LONG,
			.value_addr=&cfg.unlock_version,
			.argument_type=required_argument,
			.help="unlock version"},
		{"signature_file", 's', .cfg_type=CFG_FILE_R,
			.value_addr=&cfg.sig_fimg,
			.argument_type=required_argument,
			.help="signature file"},
		{"token_file", 't', .cfg_type=CFG_FILE_R,
			.value_addr=&cfg.tkn_fimg,
			.argument_type=required_argument,
			.help="token file - Gen6 only"},
		{NULL}
	};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));

	if (cfg.serial == 0) {
		fprintf(stderr,
			"Serial number must be set for this command!\n");
		return -1;
	}

	if (cfg.unlock_version == 0xffff) {
		fprintf(stderr,
			"Unlock version must be set for this command!\n");
		return -1;
	}

	if (cfg.pubkey_file == NULL) {
		fprintf(stderr,
			"Public key file must be set for this command!\n");
		return -1;
	}

	if (cfg.sig_file == NULL) {
		fprintf(stderr,
			"Signature file must be set for this command!\n");
		return -1;
	}

	if (cfg.tkn_file == NULL && switchtec_is_gen6(cfg.dev)) {
		fprintf(stderr,
			"Token file must be set for Gen6 devices using this command!\n");
		return -1;
	}

	if(cfg.tkn_file != NULL && !switchtec_is_gen6(cfg.dev)) {
		fprintf(stderr,
			"Ignoring token file parameter, this device is not Gen6!\n");
		cfg.tkn_file = NULL;
	}

	ret = switchtec_read_pubk_file(cfg.pubkey_fimg, &pubk);
	fclose(cfg.pubkey_fimg);

	if (ret) {
		fprintf(stderr, "Invalid public key file %s!\n",
			cfg.pubkey_file);
		return -2;
	}

	ret = switchtec_read_signature_file(cfg.sig_fimg, &sig);
	fclose(cfg.sig_fimg);

	if (ret) {
		fprintf(stderr, "Invalid signature file %s!\n",
			cfg.sig_file);
		return -3;
	}

	if (switchtec_is_gen6(cfg.dev)) {
		ret = switchtec_read_token_file(cfg.tkn_fimg, &token);
		fclose(cfg.tkn_fimg);

		if (ret) {
			fprintf(stderr, "Invalid token file %s!\n",
				cfg.tkn_file);
			return -3;
		}
	}

	ret = switchtec_dbg_unlock(cfg.dev, cfg.serial, cfg.unlock_version,
				   &pubk, &sig, &token);
	if (ret)
		switchtec_perror("mfg dbg-unlock");

	return ret;
}
#endif

#define CMD_DESC_DEBUG_LOCK_UPDATE "update debug feature secure unlock version"

#if HAVE_LIBCRYPTO
static int debug_lock_update(int argc, char **argv)
{
	int ret;
	struct switchtec_pubkey pubk;
	struct switchtec_signature sig;

	static struct {
		struct switchtec_dev *dev;
		FILE *pubkey_fimg;
		char *pubkey_file;
		unsigned long unlock_version;
		unsigned long serial;
		FILE *sig_fimg;
		char *sig_file;
		unsigned int assume_yes;
	} cfg = {
		.unlock_version = 0xffff,
	};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION_MFG_PCI,
		{"pub_key", 'p', .cfg_type=CFG_FILE_R,
			.value_addr=&cfg.pubkey_fimg,
			.argument_type=required_argument,
			.help="public key file"},
		{"serial_number", 'n', .cfg_type=CFG_LONG,
			.value_addr=&cfg.serial,
			.argument_type=required_argument,
			.help="device serial number"},
		{"new_unlock_version", 'v', .cfg_type=CFG_LONG,
			.value_addr=&cfg.unlock_version,
			.argument_type=required_argument,
			.help="new unlock version"},
		{"signature_file", 's', .cfg_type=CFG_FILE_R,
			.value_addr=&cfg.sig_fimg,
			.argument_type=required_argument,
			.help="signature file"},
		{"yes", 'y', "", CFG_NONE, &cfg.assume_yes, no_argument,
			"assume yes when prompted"},
		{NULL}
	};

	argconfig_parse(argc, argv, CMD_DESC_DEBUG_LOCK_UPDATE, opts, &cfg, sizeof(cfg));

	if (cfg.serial == 0) {
		fprintf(stderr,
			"Serial number must be set for this command!\n");
		return -1;
	}

	if (cfg.unlock_version == 0xffff) {
		fprintf(stderr,
			"Unlock version must be set for this command!\n");
		return -1;
	}

	if (cfg.pubkey_file == NULL) {
		fprintf(stderr,
			"Public key file must be set for this command!\n");
		return -1;
	}

	if (cfg.sig_file == NULL) {
		fprintf(stderr,
			"Signature file must be set for this command!\n");
		return -1;
	}

	ret = switchtec_read_pubk_file(cfg.pubkey_fimg, &pubk);
	fclose(cfg.pubkey_fimg);
	if (ret) {
		printf("Invalid public key file %s!\n",
			cfg.pubkey_file);
		return -2;
	}

	ret = switchtec_read_signature_file(cfg.sig_fimg, &sig);
	fclose(cfg.sig_fimg);
	if (ret) {
		printf("Invalid signature file %s!\n",
			cfg.sig_file);
		return -3;
	}

	fprintf(stderr,
		"WARNING: This operation makes changes to the device OTP memory and is IRREVERSIBLE!\n");
	ret = ask_if_sure(cfg.assume_yes);
	if (ret)
		return -4;

	ret = switchtec_dbg_unlock_version_update(cfg.dev, cfg.serial,
						  cfg.unlock_version,
						  &pubk, &sig);
	if (ret)
		switchtec_perror("dbg-lock-update");

	return ret;
}
#endif

#if !HAVE_LIBCRYPTO
static int no_openssl(int argc, char **argv)
{
	fprintf(stderr,
		"This command is only available when the OpenSSL development library is installed.\n"
		"Try installing the OpenSSL development library in your system and rebuild this\n"
		"program by running 'configure' and then 'make'.\n");
	return -1;
}

#define kmsk_entry_add		no_openssl
#define debug_unlock		no_openssl
#define debug_lock_update	no_openssl

#endif


#define TOKEN_RESOURCE_UNLOCK 0
#define TOKEN_VERSION_UPDATE 1

#define CMD_DESC_DEBUG_TOKEN "generate device token file for signature"
static int debug_unlock_token(int argc, char **argv)
{
	int ret;

	struct switchtec_sn_ver_info sn_info = {};
	struct {
		uint32_t id;
		uint32_t serial;
		uint32_t version;
	} token;

	const char *desc = CMD_DESC_DEBUG_TOKEN "\n\n"
			   "Use the generated token file on your security "
			   "management system to generate the signature file "
			   "required for either command 'mfg debug-unlock' or "
			   "'mfg debug-lock-update' ";

	const struct argconfig_choice type[] = {
		{"RESOURCE_UNLOCK", TOKEN_RESOURCE_UNLOCK,
		 "Generate token for signature file required for command 'mfg debug-unlock' (default)"},
		{"UNLOCK_VERSION_UPDATE", TOKEN_VERSION_UPDATE,
		 "Generate token for signature file required for command 'mfg debug-lock-update'"},
		{"GEN6_STATIC_TOKEN", GEN6_TOKEN_STATIC,
		 "Generate static token for signature file required for command 'mfg debug-unlock' on Gen6 devices"},
		{"GEN6_EPHEMERAL_TOKEN", GEN6_TOKEN_EPHEMERAL,
		 "Generate ephemeral token for signature file required for command 'mfg debug-unlock' on Gen6 devices"},
		{}
	};

	struct {
		struct switchtec_dev *dev;
		int out_fd;
		const char *out_filename;
		int unlock;
		int update;
		int type;
	} cfg = {
		.type = TOKEN_RESOURCE_UNLOCK,
	};

	const struct argconfig_options opts[] = {
		DEVICE_OPTION_MFG_PCI,
		{"type", 't', "TYPE", CFG_CHOICES, &cfg.type,
		  required_argument,
		 "output token file type", .choices=type},
		{"token_file", .cfg_type=CFG_FD_WR, .value_addr=&cfg.out_fd,
		  .argument_type=optional_positional,
		  .force_default="debug.tkn",
		  .help="debug unlock token file"},
		{NULL}};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));

	if (switchtec_is_gen6(cfg.dev))
	{
		if (cfg.type != GEN6_TOKEN_STATIC &&
		    cfg.type != GEN6_TOKEN_EPHEMERAL) {
			fprintf(stderr,
				"On Gen6 devices, only GEN6_STATIC_TOKEN \
				and GEN6_EPHEMERAL_TOKEN types are supported.\n");
			return -1;
		}

		struct switchtec_gen6_token token;

		ret = switchtec_dbg_unlock_get_token_gen6(cfg.dev, &token, cfg.type);
		if (ret) {
			switchtec_perror("mfg debug unlock token");
			return ret;
		}

		ret = write(cfg.out_fd, &token, sizeof(token));
		if(ret <= 0) {
			switchtec_perror("mfg debug gen6 unlock token");
			return ret;
		}

		fprintf(stderr, "\nToken data saved to %s\n", cfg.out_filename);
		close(cfg.out_fd);
	} else {
		if (cfg.type == GEN6_TOKEN_STATIC ||
		    cfg.type == GEN6_TOKEN_EPHEMERAL) {
			fprintf(stderr,
				"Gen6 types are not supported on this device.\n");
			return -1;
		}
		ret = switchtec_sn_ver_get(cfg.dev, &sn_info);
		if (ret) {
			switchtec_perror("mfg debug unlock token");
			return ret;
		}

		token.serial = htole32(sn_info.chip_serial);

		if (cfg.type == TOKEN_RESOURCE_UNLOCK) {
			token.id = htole32(1);
			token.version = htole32(sn_info.ver_sec_unlock);
		} else {
			token.id = htole32(2);
			token.version = htole32(sn_info.ver_sec_unlock) + 1;
		}

		ret = write(cfg.out_fd, &token, sizeof(token));
		if(ret <= 0) {
			switchtec_perror("mfg debug unlock token");
			return ret;
		}

		fprintf(stderr, "\nToken data saved to %s\n", cfg.out_filename);
		close(cfg.out_fd);
	}

	return 0;
}

static void print_device_settings_only(struct switchtec_device_config_dev_settings *settings)
{
	printf("\n--Device Settings--\n");
	printf("TWI OCP Address (7-bits):\t\t0x%02X\n",
	       settings->twi_ocp_addr);
	printf("TWI MRPC Address (7-bits):\t\t0x%02X\n",
	       settings->twi_mrpc_addr);
	printf("TWI Recovery Address Type:\t\t%d\n",
	       settings->twi_rcvry_addr_type);
	printf("TWI Recovery Bus:\t\t\t%d\n",
	       settings->twi_rcvry_bus);
	printf("I3C PID:\t\t\t\t0x%08x%04x\n",
	       settings->i3c_pid_hi,
	       settings->i3c_pid_lo);
	printf("I3C Address (7-bits):\t\t\t0x%02X\n",
	       settings->i3c_addr_7bit);
	printf("I3C Recovery Bus:\t\t\t%d\n",
	       settings->i3c_rcvry_bus);
}

static void print_customer_settings(struct switchtec_device_config_customer_settings *customer)
{
	int i;

	printf("\n--Customer Settings--\n");
	printf("Device ID:\t\t\t\t0x%04X\n", customer->device_id);
	printf("Vendor ID:\t\t\t\t0x%04X\n", customer->vendor_id);
	printf("Revision ID:\t\t\t\t0x%02X\n", customer->revision_id);
	printf("Subsystem ID:\t\t\t\t0x%04X\n", customer->subsystem_id);
	printf("Subsystem Vendor ID:\t\t\t0x%04X\n", customer->subsystem_vendor_id);

	for (i = 0; i < DEVICE_CONFIG_CUSTOMER_FIELD_NUM; i++) {
		printf("Customer Field %d:\t\t\t0x%08X\n",
		       i, customer->customer_fields[i]);
	}

	for (i = 0; i < DEVICE_CONFIG_CUSTOMER_ECC_FIELD_NUM; i++) {
		printf("Customer ECC Field %d:\t\t\t0x%08X%08X\n", i,
		       customer->customer_ecc_fields[i][1],
		       customer->customer_ecc_fields[i][0]);
	}
}

static const char *dok_status_to_str(uint32_t status)
{
	switch (status) {
	case 0:
		return "Empty";
	case 1:
		return "Valid";
	case 2:
		return "Revoked";
	default:
		return "Reserved";
	}
}

static void print_security_settings_only(struct switchtec_device_config_get_sec *sec_cfg)
{
	int i, j;
	uint32_t dok_status[DEVICE_CONFIG_MAX_KEY_SLOTS] = {
		sec_cfg->secure_settings_status.dok0_status,
		sec_cfg->secure_settings_status.dok1_status,
		sec_cfg->secure_settings_status.dok2_status,
		sec_cfg->secure_settings_status.dok3_status,
		sec_cfg->secure_settings_status.dok4_status,
		sec_cfg->secure_settings_status.dok5_status,
		sec_cfg->secure_settings_status.dok6_status,
		sec_cfg->secure_settings_status.dok7_status,
		sec_cfg->secure_settings_status.dok8_status,
		sec_cfg->secure_settings_status.dok9_status,
		sec_cfg->secure_settings_status.dok10_status,
		sec_cfg->secure_settings_status.dok11_status,
	};

	printf("\n--Security Settings--\n");
	printf("Command Map:\t\t\t\t0x%03X\n",
	       sec_cfg->secure_settings.command_map & 0xFFF);
	printf("Static Token Disable:\t\t\t%s\n",
	       sec_cfg->secure_settings.static_token_disable ? "Yes" : "No");
	printf("PSID Only Token Disable:\t\t%s\n",
	       sec_cfg->secure_settings.psid_only_token_disable ? "Yes" : "No");
	printf("UID Only Token Disable:\t\t\t%s\n",
	       sec_cfg->secure_settings.uid_only_token_disable ? "Yes" : "No");
	printf("PSID+UID Token Disable:\t\t\t%s\n",
	       sec_cfg->secure_settings.psid_uid_token_disable ? "Yes" : "No");
	printf("Boot from UART Disable:\t\t\t%s\n",
	       sec_cfg->secure_settings.boot_from_uart_disable ? "Yes" : "No");
	printf("Boot from SMBus Disable:\t\t%s\n",
	       sec_cfg->secure_settings.boot_from_smbus_disable ? "Yes" : "No");
	printf("Boot from I3C Disable:\t\t\t%s\n",
	       sec_cfg->secure_settings.boot_from_i3c_disable ? "Yes" : "No");
	printf("Failover to UART Disable:\t\t%s\n",
	       sec_cfg->secure_settings.failover_to_uart_disable ? "Yes" : "No");
	printf("Failover to SMBus Disable:\t\t%s\n",
	       sec_cfg->secure_settings.failover_to_smbus_disable ? "Yes" : "No");
	printf("Failover to I3C Disable:\t\t%s\n",
	       sec_cfg->secure_settings.failover_to_i3c_disable ? "Yes" : "No");
	printf("PSID0:\t\t\t\t\t");
	for (i = 0; i < SWITCHTEC_PSID_LEN_DWORDS; i++)
		printf("%08x", be32toh(sec_cfg->secure_settings.psid0[i]));
	printf("\n");

	for (i = 0; i < (int)sec_cfg->secure_settings.key_prog_num &&
		    i < DEVICE_CONFIG_MAX_KEY_SLOTS; i++) {
		printf("OTP Key%d Hash:\t\t\t\t", i + 1);
		for (j = 0; j < DEVICE_CONFIG_KEY_HASH_SIZE_DWORDS; j++) {
			if (j && (j % 8) == 0)
				printf("\n\t\t\t\t\t");
			printf("%08x", be32toh(sec_cfg->secure_settings.key_data[i].hash[j]));
		}
		printf("\n");
	}

	printf("\n--DOK Key Status--\n");
	for (i = 0; i < DEVICE_CONFIG_MAX_KEY_SLOTS; i++)
		printf("DOK%02d: %s (%u)\n", i,
		       dok_status_to_str(dok_status[i]), dok_status[i]);
}

#define CMD_DESC_DEVICE_CONFIG_GET "get device configuration (Gen6 only)"

static int device_config_get(int argc, char **argv)
{
	int ret;
	struct switchtec_device_config_dev_settings dev_settings = {};
	struct switchtec_device_config_get_sec sec_config = {};
	struct switchtec_device_config_customer_settings customer_settings = {};

	const char *desc = CMD_DESC_DEVICE_CONFIG_GET "\n\n"
			   "This command retrieves device configuration sections "
			   "from Gen6 devices. The device configuration includes:\n"
			   "  - Device settings (TWI/I3C addresses)\n"
			   "  - Customer settings (PSID, PCI IDs)\n"
			   "  - Security settings (token disable, boot modes)\n\n"
			   "Use -3 to retrieve device settings only (default).\n"
			   "Use -4 to retrieve security settings only.\n"
			   "Use -5 to retrieve customer settings only.\n";

	static struct {
		struct switchtec_dev *dev;
		int subcmd_get_device;
		int subcmd_get_security;
		int subcmd_get_customer;
	} cfg = {};

	const struct argconfig_options opts[] = {
		DEVICE_OPTION_MFG_PCI,
		{"device-get", '3', "", CFG_NONE, &cfg.subcmd_get_device,
			no_argument, "get device settings (sub-command 3, default)"},
		{"security-get", '4', "", CFG_NONE, &cfg.subcmd_get_security,
			no_argument, "get security settings (sub-command 4)"},
		{"customer-get", '5', "", CFG_NONE, &cfg.subcmd_get_customer,
			no_argument, "get customer settings (sub-command 5)"},
		{NULL}
	};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));

	if (!switchtec_is_gen6(cfg.dev)) {
		fprintf(stderr, "This command is only supported on Gen6 devices!\n");
		return -1;
	}

	if (cfg.subcmd_get_device && (cfg.subcmd_get_security || cfg.subcmd_get_customer)) {
		fprintf(stderr, "-3 cannot be combined with -4 or -5\n");
		return -1;
	}

	if (cfg.subcmd_get_security && cfg.subcmd_get_customer) {
		fprintf(stderr, "-4 and -5 cannot be used together\n");
		return -1;
	}

	if (cfg.subcmd_get_security) {
		ret = switchtec_device_config_get_security(cfg.dev, &sec_config);
		if (ret) {
			switchtec_perror("mfg device-config-get -4");
			return ret;
		}
		print_security_settings_only(&sec_config);
	} else if (cfg.subcmd_get_customer) {
		ret = switchtec_device_config_get_customer(cfg.dev, &customer_settings);
		if (ret) {
			switchtec_perror("mfg device-config-get -5");
			return ret;
		}
		print_customer_settings(&customer_settings);
	} else {
		ret = switchtec_device_config_get(cfg.dev, &dev_settings);
		if (ret) {
			switchtec_perror("mfg device-config-get -3");
			return ret;
		}
		print_device_settings_only(&dev_settings);
	}

	return 0;
}

#define CMD_DESC_DEVICE_CONFIG_SET_DEVICE "set device settings (Gen6 only)"

static int device_config_set_device(int argc, char **argv)
{
	int ret;
	struct switchtec_device_config_dev_settings settings = {};
	FILE *fp;
	size_t nread;

	const char *desc = CMD_DESC_DEVICE_CONFIG_SET_DEVICE "\n\n"
			   "Set device settings including TWI and I3C addresses.\n"
			   "Use -f to load settings from a binary file.\n\n"
			   "WARNING: This operation modifies OTP and may be IRREVERSIBLE!";

	static struct {
		struct switchtec_dev *dev;
		const char *input_file;
		unsigned long twi_ocp_addr;
		unsigned long twi_mrpc_addr;
		unsigned long twi_rcvry_addr_type;
		unsigned long twi_rcvry_bus;
		unsigned long i3c_pid_hi;
		unsigned long i3c_pid_lo;
		unsigned long i3c_addr_7bit;
		unsigned long i3c_rcvry_bus;
		int assume_yes;
	} cfg = {};

	const struct argconfig_options opts[] = {
		DEVICE_OPTION_MFG_PCI,
		{"file", 'f', "", CFG_STRING, &cfg.input_file,
			required_argument, "binary input file (mutually exclusive with other options)"},
		{"twi-ocp-addr", 'o', "", CFG_LONG, &cfg.twi_ocp_addr,
			required_argument, "TWI OCP address (10-bit)"},
		{"twi-mrpc-addr", 'm', "", CFG_LONG, &cfg.twi_mrpc_addr,
			required_argument, "TWI MRPC address (10-bit)"},
		{"twi-addr-type", 't', "", CFG_LONG, &cfg.twi_rcvry_addr_type,
			required_argument, "TWI recovery address type (0-3)"},
		{"twi-bus", 'b', "", CFG_LONG, &cfg.twi_rcvry_bus,
			required_argument, "TWI recovery bus (0-3)"},
		{"i3c-pid-hi", 'H', "", CFG_LONG, &cfg.i3c_pid_hi,
			required_argument, "I3C PID high bits [47:16]"},
		{"i3c-pid-lo", 'L', "", CFG_LONG, &cfg.i3c_pid_lo,
			required_argument, "I3C PID low bits [15:0]"},
		{"i3c-addr", 'a', "", CFG_LONG, &cfg.i3c_addr_7bit,
			required_argument, "I3C 7-bit address"},
		{"i3c-bus", 'i', "", CFG_LONG, &cfg.i3c_rcvry_bus,
			required_argument, "I3C recovery bus (0-3)"},
		{"yes", 'y', "", CFG_NONE, &cfg.assume_yes, no_argument,
			"assume yes when prompted"},
		{NULL}
	};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));

	if (!switchtec_is_gen6(cfg.dev)) {
		fprintf(stderr, "This command is only supported on Gen6 devices!\n");
		return -1;
	}

	if (cfg.input_file) {
		if (cfg.twi_ocp_addr || cfg.twi_mrpc_addr || cfg.twi_rcvry_addr_type ||
		    cfg.twi_rcvry_bus || cfg.i3c_pid_hi || cfg.i3c_pid_lo ||
		    cfg.i3c_addr_7bit || cfg.i3c_rcvry_bus) {
			fprintf(stderr, "Error: -f option is mutually exclusive with other setting options\n");
			return -1;
		}

		fp = fopen(cfg.input_file, "rb");
		if (!fp) {
			perror(cfg.input_file);
			return -1;
		}

		nread = fread(&settings, 1, sizeof(settings), fp);
		fclose(fp);

		if (nread != sizeof(settings)) {
			fprintf(stderr, "Error: expected %zu bytes, read %zu bytes\n",
				sizeof(settings), nread);
			return -1;
		}
		printf("Loaded settings from %s (%zu bytes)\n", cfg.input_file, nread);
	} else {
		settings.twi_ocp_addr = cfg.twi_ocp_addr & 0x3FF;
		settings.twi_mrpc_addr = cfg.twi_mrpc_addr & 0x3FF;
		settings.twi_rcvry_addr_type = cfg.twi_rcvry_addr_type & 0x3;
		settings.twi_rcvry_bus = cfg.twi_rcvry_bus & 0x3;
		settings.i3c_pid_hi = cfg.i3c_pid_hi;
		settings.i3c_pid_lo = cfg.i3c_pid_lo & 0xFFFF;
		settings.i3c_addr_7bit = cfg.i3c_addr_7bit & 0x7F;
		settings.i3c_rcvry_bus = cfg.i3c_rcvry_bus & 0x3;
	}

	printf("Setting device configuration:\n");
	printf("  TWI OCP Address:        0x%03x\n", settings.twi_ocp_addr);
	printf("  TWI MRPC Address:       0x%03x\n", settings.twi_mrpc_addr);
	printf("  TWI Recovery Addr Type: %d\n", settings.twi_rcvry_addr_type);
	printf("  TWI Recovery Bus:       %d\n", settings.twi_rcvry_bus);
	printf("  I3C PID:                0x%08x%04x\n",
	       settings.i3c_pid_hi, settings.i3c_pid_lo);
	printf("  I3C 7-bit Address:      0x%02x\n", settings.i3c_addr_7bit);
	printf("  I3C Recovery Bus:       %d\n", settings.i3c_rcvry_bus);

	if (!cfg.assume_yes) {
		fprintf(stderr,
			"\nWARNING: This operation modifies OTP and may be IRREVERSIBLE!\n");
		ret = ask_if_sure(cfg.assume_yes);
		if (ret)
			return -2;
	}

	ret = switchtec_device_config_set_dev(cfg.dev, &settings);
	if (ret) {
		switchtec_perror("mfg device-config-set-dev");
		return ret;
	}

	printf("Device settings programmed successfully.\n");
	return 0;
}

#define CMD_DESC_DEVICE_CONFIG_SET_CUSTOMER "set customer settings (Gen6 only)"

static int device_config_set_customer(int argc, char **argv)
{
	int ret;
	struct switchtec_device_config_customer_settings settings = {};
	FILE *fp;
	size_t nread;

	const char *desc = CMD_DESC_DEVICE_CONFIG_SET_CUSTOMER "\n\n"
			   "Set customer settings including PSID and PCI IDs.\n"
			   "Use -f to load settings from a binary file.\n\n"
			   "WARNING: This operation modifies OTP and may be IRREVERSIBLE!";

	static struct {
		struct switchtec_dev *dev;
		const char *input_file;
		unsigned long device_id;
		unsigned long vendor_id;
		unsigned long revision_id;
		unsigned long subsystem_id;
		unsigned long subsystem_vendor_id;
		int assume_yes;
	} cfg = {};

	const struct argconfig_options opts[] = {
		DEVICE_OPTION_MFG_PCI,
		{"file", 'f', "", CFG_STRING, &cfg.input_file,
			required_argument, "binary input file (mutually exclusive with other options)"},
		{"device-id", 'd', "", CFG_LONG, &cfg.device_id,
			required_argument, "PCI Device ID"},
		{"vendor-id", 'v', "", CFG_LONG, &cfg.vendor_id,
			required_argument, "PCI Vendor ID"},
		{"revision-id", 'r', "", CFG_LONG, &cfg.revision_id,
			required_argument, "PCI Revision ID"},
		{"subsystem-id", 's', "", CFG_LONG, &cfg.subsystem_id,
			required_argument, "PCI Subsystem ID"},
		{"subsystem-vendor-id", 'S', "", CFG_LONG, &cfg.subsystem_vendor_id,
			required_argument, "PCI Subsystem Vendor ID"},
		{"yes", 'y', "", CFG_NONE, &cfg.assume_yes, no_argument,
			"assume yes when prompted"},
		{NULL}
	};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));

	if (!switchtec_is_gen6(cfg.dev)) {
		fprintf(stderr, "This command is only supported on Gen6 devices!\n");
		return -1;
	}

	if (cfg.input_file) {
		if (cfg.device_id || cfg.vendor_id ||
		    cfg.revision_id || cfg.subsystem_id || cfg.subsystem_vendor_id) {
			fprintf(stderr, "Error: -f option is mutually exclusive with other setting options\n");
			return -1;
		}

		fp = fopen(cfg.input_file, "rb");
		if (!fp) {
			perror(cfg.input_file);
			return -1;
		}

		nread = fread(&settings, 1, sizeof(settings), fp);
		fclose(fp);

		if (nread != sizeof(settings)) {
			fprintf(stderr, "Error: expected %zu bytes, read %zu bytes\n",
				sizeof(settings), nread);
			return -1;
		}
		printf("Loaded settings from %s (%zu bytes)\n", cfg.input_file, nread);
	} else {
		settings.device_id = cfg.device_id & 0xFFFF;
		settings.vendor_id = cfg.vendor_id & 0xFFFF;
		settings.revision_id = cfg.revision_id & 0xFFFF;
		settings.subsystem_id = cfg.subsystem_id & 0xFFFF;
		settings.subsystem_vendor_id = cfg.subsystem_vendor_id & 0xFFFF;
	}

	printf("Setting customer configuration:\n");
	printf("  Device ID:            0x%04x\n", settings.device_id);
	printf("  Vendor ID:            0x%04x\n", settings.vendor_id);
	printf("  Revision ID:          0x%04x\n", settings.revision_id);
	printf("  Subsystem ID:         0x%04x\n", settings.subsystem_id);
	printf("  Subsystem Vendor ID:  0x%04x\n", settings.subsystem_vendor_id);

	if (!cfg.assume_yes) {
		fprintf(stderr,
			"\nWARNING: This operation modifies OTP and may be IRREVERSIBLE!\n");
		ret = ask_if_sure(cfg.assume_yes);
		if (ret)
			return -2;
	}

	ret = switchtec_device_config_set_customer(cfg.dev, &settings);
	if (ret) {
		switchtec_perror("mfg device-config-set-customer");
		return ret;
	}

	printf("Customer settings programmed successfully.\n");
	return 0;
}

#define CMD_DESC_DEVICE_CONFIG_SET_SECURITY "set security settings (Gen6 only)"

static int parse_key_hash(const char *hex, uint32_t *out)
{
	int i;
	char buf[9];
	int len;

	if (!hex || !out)
		return -1;

	len = strlen(hex);
	if (len != 128) {
		fprintf(stderr, "Key hash must be 128 hex characters (512 bits), got %d\n", len);
		return -1;
	}

	for (i = 0; i < DEVICE_CONFIG_KEY_HASH_SIZE_DWORDS; i++) {
		memcpy(buf, &hex[i * 8], 8);
		buf[8] = '\0';
		out[i] = strtoul(buf, NULL, 16);
	}
	return 0;
}

static int device_config_set_security(int argc, char **argv)
{
	int ret, i, j;
	int key_count = 0;
	struct switchtec_device_config_secure_settings settings = {};
	FILE *fp;
	size_t nread;

	const char *desc = CMD_DESC_DEVICE_CONFIG_SET_SECURITY "\n\n"
			   "Set security settings including command map, token disable flags,\n"
			   "boot/failover disable flags, PSID0, and OTP key hashes.\n"
			   "Use -f to load settings from a binary file.\n\n"
			   "Key hash format: 128 hex characters (512-bit SHA2-512 hash)\n\n"
			   "WARNING: This operation modifies OTP and may be IRREVERSIBLE!";

	static struct {
		struct switchtec_dev *dev;
		const char *input_file;
		unsigned long command_map;
		int static_token_disable;
		int psid_only_token_disable;
		int uid_only_token_disable;
		int psid_uid_token_disable;
		int boot_from_uart_disable;
		int boot_from_smbus_disable;
		int boot_from_i3c_disable;
		int failover_to_uart_disable;
		int failover_to_smbus_disable;
		int failover_to_i3c_disable;
		unsigned long psid0_0;
		unsigned long psid0_1;
		unsigned long psid0_2;
		unsigned long psid0_3;
		const char *key1_hash;
		const char *key2_hash;
		const char *key3_hash;
		const char *key4_hash;
		const char *key5_hash;
		const char *key6_hash;
		const char *key7_hash;
		const char *key8_hash;
		const char *key9_hash;
		const char *key10_hash;
		const char *key11_hash;
		const char *key12_hash;
		int assume_yes;
	} cfg = {};

	const struct argconfig_options opts[] = {
		DEVICE_OPTION_MFG_PCI,
		{"file", 'f', "", CFG_STRING, &cfg.input_file,
			required_argument, "binary input file (mutually exclusive with other options)"},
		{"command-map", 'c', "", CFG_LONG, &cfg.command_map,
			required_argument, "MRPC command map (12-bit)"},
		{"static-token-disable", .cfg_type=CFG_NONE,
			.value_addr=&cfg.static_token_disable, .argument_type=no_argument,
			.help="disable static debug token"},
		{"psid-only-token-disable", .cfg_type=CFG_NONE,
			.value_addr=&cfg.psid_only_token_disable, .argument_type=no_argument,
			.help="disable PSID-only token"},
		{"uid-only-token-disable", .cfg_type=CFG_NONE,
			.value_addr=&cfg.uid_only_token_disable, .argument_type=no_argument,
			.help="disable UID-only token"},
		{"psid-uid-token-disable", .cfg_type=CFG_NONE,
			.value_addr=&cfg.psid_uid_token_disable, .argument_type=no_argument,
			.help="disable PSID+UID token"},
		{"boot-uart-disable", .cfg_type=CFG_NONE,
			.value_addr=&cfg.boot_from_uart_disable, .argument_type=no_argument,
			.help="disable boot from UART"},
		{"boot-smbus-disable", .cfg_type=CFG_NONE,
			.value_addr=&cfg.boot_from_smbus_disable, .argument_type=no_argument,
			.help="disable boot from SMBus"},
		{"boot-i3c-disable", .cfg_type=CFG_NONE,
			.value_addr=&cfg.boot_from_i3c_disable, .argument_type=no_argument,
			.help="disable boot from I3C"},
		{"failover-uart-disable", .cfg_type=CFG_NONE,
			.value_addr=&cfg.failover_to_uart_disable, .argument_type=no_argument,
			.help="disable failover to UART"},
		{"failover-smbus-disable", .cfg_type=CFG_NONE,
			.value_addr=&cfg.failover_to_smbus_disable, .argument_type=no_argument,
			.help="disable failover to SMBus"},
		{"failover-i3c-disable", .cfg_type=CFG_NONE,
			.value_addr=&cfg.failover_to_i3c_disable, .argument_type=no_argument,
			.help="disable failover to I3C"},
		{"psid0-0", .cfg_type=CFG_LONG, .value_addr=&cfg.psid0_0,
			.argument_type=required_argument, .help="PSID0 DWORD 0 (32-bit hex/dec)"},
		{"psid0-1", .cfg_type=CFG_LONG, .value_addr=&cfg.psid0_1,
			.argument_type=required_argument, .help="PSID0 DWORD 1 (32-bit hex/dec)"},
		{"psid0-2", .cfg_type=CFG_LONG, .value_addr=&cfg.psid0_2,
			.argument_type=required_argument, .help="PSID0 DWORD 2 (32-bit hex/dec)"},
		{"psid0-3", .cfg_type=CFG_LONG, .value_addr=&cfg.psid0_3,
			.argument_type=required_argument, .help="PSID0 DWORD 3 (32-bit hex/dec)"},
		{"key1-hash", .cfg_type=CFG_STRING, .value_addr=&cfg.key1_hash,
			.argument_type=required_argument, .help="Key 1 hash (128 hex chars)"},
		{"key2-hash", .cfg_type=CFG_STRING, .value_addr=&cfg.key2_hash,
			.argument_type=required_argument, .help="Key 2 hash (128 hex chars)"},
		{"key3-hash", .cfg_type=CFG_STRING, .value_addr=&cfg.key3_hash,
			.argument_type=required_argument, .help="Key 3 hash (128 hex chars)"},
		{"key4-hash", .cfg_type=CFG_STRING, .value_addr=&cfg.key4_hash,
			.argument_type=required_argument, .help="Key 4 hash (128 hex chars)"},
		{"key5-hash", .cfg_type=CFG_STRING, .value_addr=&cfg.key5_hash,
			.argument_type=required_argument, .help="Key 5 hash (128 hex chars)"},
		{"key6-hash", .cfg_type=CFG_STRING, .value_addr=&cfg.key6_hash,
			.argument_type=required_argument, .help="Key 6 hash (128 hex chars)"},
		{"key7-hash", .cfg_type=CFG_STRING, .value_addr=&cfg.key7_hash,
			.argument_type=required_argument, .help="Key 7 hash (128 hex chars)"},
		{"key8-hash", .cfg_type=CFG_STRING, .value_addr=&cfg.key8_hash,
			.argument_type=required_argument, .help="Key 8 hash (128 hex chars)"},
		{"key9-hash", .cfg_type=CFG_STRING, .value_addr=&cfg.key9_hash,
			.argument_type=required_argument, .help="Key 9 hash (128 hex chars)"},
		{"key10-hash", .cfg_type=CFG_STRING, .value_addr=&cfg.key10_hash,
			.argument_type=required_argument, .help="Key 10 hash (128 hex chars)"},
		{"key11-hash", .cfg_type=CFG_STRING, .value_addr=&cfg.key11_hash,
			.argument_type=required_argument, .help="Key 11 hash (128 hex chars)"},
		{"key12-hash", .cfg_type=CFG_STRING, .value_addr=&cfg.key12_hash,
			.argument_type=required_argument, .help="Key 12 hash (128 hex chars)"},
		{"yes", 'y', "", CFG_NONE, &cfg.assume_yes, no_argument,
			"assume yes when prompted"},
		{NULL}
	};

	const char **key_hashes[12] = {
		&cfg.key1_hash, &cfg.key2_hash, &cfg.key3_hash, &cfg.key4_hash,
		&cfg.key5_hash, &cfg.key6_hash, &cfg.key7_hash, &cfg.key8_hash,
		&cfg.key9_hash, &cfg.key10_hash, &cfg.key11_hash, &cfg.key12_hash
	};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));

	if (!switchtec_is_gen6(cfg.dev)) {
		fprintf(stderr, "This command is only supported on Gen6 devices!\n");
		return -1;
	}

	if (cfg.input_file) {
		fp = fopen(cfg.input_file, "rb");
		if (!fp) {
			perror(cfg.input_file);
			return -1;
		}

		nread = fread(&settings, 1, sizeof(settings), fp);
		fclose(fp);

		if (nread != sizeof(settings)) {
			fprintf(stderr, "Error: expected %zu bytes, read %zu bytes\n",
				sizeof(settings), nread);
			return -1;
		}
		printf("Loaded settings from %s (%zu bytes)\n", cfg.input_file, nread);
		key_count = settings.key_prog_num;
	} else {
		settings.command_map = cfg.command_map & 0xFFF;
		settings.static_token_disable = cfg.static_token_disable ? 1 : 0;
		settings.psid_only_token_disable = cfg.psid_only_token_disable ? 1 : 0;
		settings.uid_only_token_disable = cfg.uid_only_token_disable ? 1 : 0;
		settings.psid_uid_token_disable = cfg.psid_uid_token_disable ? 1 : 0;
		settings.boot_from_uart_disable = cfg.boot_from_uart_disable ? 1 : 0;
		settings.boot_from_smbus_disable = cfg.boot_from_smbus_disable ? 1 : 0;
		settings.boot_from_i3c_disable = cfg.boot_from_i3c_disable ? 1 : 0;
		settings.failover_to_uart_disable = cfg.failover_to_uart_disable ? 1 : 0;
		settings.failover_to_smbus_disable = cfg.failover_to_smbus_disable ? 1 : 0;
		settings.failover_to_i3c_disable = cfg.failover_to_i3c_disable ? 1 : 0;
		settings.psid0[0] = cfg.psid0_0;
		settings.psid0[1] = cfg.psid0_1;
		settings.psid0[2] = cfg.psid0_2;
		settings.psid0[3] = cfg.psid0_3;

		for (i = 0; i < 12; i++) {
			if (*key_hashes[i]) {
				ret = parse_key_hash(*key_hashes[i], settings.key_data[key_count].hash);
				if (ret) {
					fprintf(stderr, "Invalid key%d hash\n", i + 1);
					return -1;
				}
				settings.key_data[key_count].index = i;
				key_count++;
			}
		}
		settings.key_prog_num = key_count;
	}

	printf("Setting security configuration:\n");
	printf("  Command Map:              0x%03x\n", settings.command_map);
	printf("  Static Token Disable:     %d\n", settings.static_token_disable);
	printf("  PSID Only Token Disable:  %d\n", settings.psid_only_token_disable);
	printf("  UID Only Token Disable:   %d\n", settings.uid_only_token_disable);
	printf("  PSID+UID Token Disable:   %d\n", settings.psid_uid_token_disable);
	printf("  Boot from UART Disable:   %d\n", settings.boot_from_uart_disable);
	printf("  Boot from SMBus Disable:  %d\n", settings.boot_from_smbus_disable);
	printf("  Boot from I3C Disable:    %d\n", settings.boot_from_i3c_disable);
	printf("  Failover to UART Disable: %d\n", settings.failover_to_uart_disable);
	printf("  Failover to SMBus Disable: %d\n", settings.failover_to_smbus_disable);
	printf("  Failover to I3C Disable:  %d\n", settings.failover_to_i3c_disable);
	printf("  PSID0:                    %08lx%08lx%08lx%08lx\n",
	       cfg.psid0_0, cfg.psid0_1, cfg.psid0_2, cfg.psid0_3);
	printf("  Keys to program:          %d\n", key_count);
	for (i = 0; i < key_count; i++) {
		printf("  Key%d:                     ", settings.key_data[i].index + 1);
		for (j = 0; j < DEVICE_CONFIG_KEY_HASH_SIZE_DWORDS; j++) {
			if (j == 8)
				printf("\n                            ");
			printf("%08x", settings.key_data[i].hash[j]);
		}
		printf("\n");
	}

	if (!cfg.assume_yes) {
		fprintf(stderr,
			"\nWARNING: This operation modifies OTP and may be IRREVERSIBLE!\n");
		ret = ask_if_sure(cfg.assume_yes);
		if (ret)
			return -2;
	}

	ret = switchtec_device_config_set_security(cfg.dev, &settings);
	if (ret) {
		switchtec_perror("mfg device-config-set-security");
		return ret;
	}

	printf("Security settings programmed successfully.\n");
	return 0;
}

/* PMC CRC32 lookup table - polynomial 0x04C11DB7 (MSB first) */
static const uint32_t crc32_tab[256] = {
	0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9,
	0x130476dc, 0x17c56b6b, 0x1a864db2, 0x1e475005,
	0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
	0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd,
	0x4c11db70, 0x48d0c6c7, 0x4593e01e, 0x4152fda9,
	0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
	0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011,
	0x791d4014, 0x7ddc5da3, 0x709f7b7a, 0x745e66cd,
	0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
	0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5,
	0xbe2b5b58, 0xbaea46ef, 0xb7a96036, 0xb3687d81,
	0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
	0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49,
	0xc7361b4c, 0xc3f706fb, 0xceb42022, 0xca753d95,
	0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
	0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d,
	0x34867077, 0x30476dc0, 0x3d044b19, 0x39c556ae,
	0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
	0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16,
	0x018aeb13, 0x054bf6a4, 0x0808d07d, 0x0cc9cdca,
	0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
	0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02,
	0x5e9f46bf, 0x5a5e5b08, 0x571d7dd1, 0x53dc6066,
	0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
	0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e,
	0xbfa1b04b, 0xbb60adfc, 0xb6238b25, 0xb2e29692,
	0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
	0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a,
	0xe0b41de7, 0xe4750050, 0xe9362689, 0xedf73b3e,
	0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
	0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686,
	0xd5b88683, 0xd1799b34, 0xdc3abded, 0xd8fba05a,
	0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
	0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb,
	0x4f040d56, 0x4bc510e1, 0x46863638, 0x42472b8f,
	0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
	0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47,
	0x36194d42, 0x32d850f5, 0x3f9b762c, 0x3b5a6b9b,
	0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
	0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623,
	0xf12f560e, 0xf5ee4bb9, 0xf8ad6d60, 0xfc6c70d7,
	0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
	0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f,
	0xc423cd6a, 0xc0e2d0dd, 0xcda1f604, 0xc960ebb3,
	0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
	0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b,
	0x9b3660c6, 0x9ff77d71, 0x92b45ba8, 0x9675461f,
	0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
	0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640,
	0x4e8ee645, 0x4a4ffbf2, 0x470cdd2b, 0x43cdc09c,
	0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
	0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24,
	0x119b4be9, 0x155a565e, 0x18197087, 0x1cd86d30,
	0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
	0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088,
	0x2497d08d, 0x2056cd3a, 0x2d15ebe3, 0x29d4f654,
	0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
	0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c,
	0xe3a1cbc1, 0xe760d676, 0xea23f0af, 0xeee2ed18,
	0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
	0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0,
	0x9abc8bd5, 0x9e7d9662, 0x933eb0bb, 0x97ffad0c,
	0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
	0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4,
};

static uint32_t compute_crc32(const uint8_t *data, size_t len)
{
	uint32_t crc = 0xFFFFFFFF;
	size_t i;
	uint8_t byte;

	for (i = 0; i < len; i++) {
		byte = (crc >> 24) ^ data[i];
		crc = crc32_tab[byte] ^ (crc << 8);
	}

	return crc ^ 0xFFFFFFFF;
}

static int parse_hex_string(const char *hex, uint32_t *out, int dwords)
{
	int i;
	char buf[9];
	int len = strlen(hex);

	for (i = 0; i < dwords; i++) {
		int offset = i * 8;
		if (offset < len) {
			int copy_len = (offset + 8 <= len) ? 8 : (len - offset);
			memset(buf, '0', 8);
			memcpy(buf, hex + offset, copy_len);
			buf[8] = '\0';
			out[i] = strtoul(buf, NULL, 16);
		} else {
			out[i] = 0;
		}
	}
	return 0;
}

#if HAVE_LIBCRYPTO
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>

static int compute_rsa4k_hash(FILE *key_file, uint32_t *hash)
{
	EVP_PKEY *pkey = NULL;
	uint8_t pubkey_data[512] = {0};
	uint8_t hash_bytes[64];
	size_t pubkey_len = 0;
	int ret = -1;
	BIGNUM *n = NULL;

	pkey = PEM_read_PUBKEY(key_file, NULL, NULL, NULL);
	if (!pkey) {
		fseek(key_file, 0L, SEEK_SET);
		pkey = PEM_read_PrivateKey(key_file, NULL, NULL, NULL);
		if (!pkey) {
			fprintf(stderr, "Failed to read PEM key file!\n");
			return -1;
		}
	}

	if (EVP_PKEY_base_id(pkey) != EVP_PKEY_RSA) {
		fprintf(stderr, "Key must be RSA! Got key type: %d\n",
			EVP_PKEY_base_id(pkey));
		goto cleanup;
	}

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	EVP_PKEY_get_bn_param(pkey, "n", &n);
#else
	{
		const RSA *rsa = EVP_PKEY_get0_RSA(pkey);
		if (rsa) {
			const BIGNUM *n_tmp;
			RSA_get0_key(rsa, &n_tmp, NULL, NULL);
			n = BN_dup(n_tmp);
		}
	}
#endif
	if (!n) {
		fprintf(stderr, "Failed to extract RSA modulus!\n");
		goto cleanup;
	}

	pubkey_len = BN_num_bytes(n);

	if (pubkey_len != 512) {
		fprintf(stderr, "Key must be RSA 4096-bit! Got %zu-bit key.\n",
			pubkey_len * 8);
		BN_free(n);
		goto cleanup;
	}

	BN_bn2bin(n, pubkey_data);
	BN_free(n);

	SHA512(pubkey_data, 512, hash_bytes);

	for (int i = 0; i < 16; i++) {
		hash[i] = ((uint32_t)hash_bytes[i*4 + 3] << 24) |
			  ((uint32_t)hash_bytes[i*4 + 2] << 16) |
			  ((uint32_t)hash_bytes[i*4 + 1] << 8) |
			  ((uint32_t)hash_bytes[i*4 + 0]);
	}

	ret = 0;

cleanup:
	EVP_PKEY_free(pkey);
	return ret;
}

static void compute_sha512_dwords(const void *data, size_t len,
				  uint32_t hash_out[DEVICE_CONFIG_KEY_HASH_SIZE_DWORDS])
{
	uint8_t hash_bytes[64];
	int i;

	SHA512((const unsigned char *)data, len, hash_bytes);

	for (i = 0; i < DEVICE_CONFIG_KEY_HASH_SIZE_DWORDS; i++) {
		hash_out[i] = ((uint32_t)hash_bytes[i*4 + 3] << 24) |
			      ((uint32_t)hash_bytes[i*4 + 2] << 16) |
			      ((uint32_t)hash_bytes[i*4 + 1] << 8) |
			      ((uint32_t)hash_bytes[i*4 + 0]);
	}
}

#define CMD_DESC_DOK_KEY_ADD "add DOK key entry (Gen6 only)"

static int dok_key_add(int argc, char **argv)
{
	int ret;
	size_t sig_len;
	struct switchtec_dok_signature sig = {};
	struct switchtec_dok_key_add key_add = {};
	uint32_t computed_hash[DEVICE_CONFIG_KEY_HASH_SIZE_DWORDS];
	char hash_str[129];

	const char *desc = CMD_DESC_DOK_KEY_ADD "\n\n"
			   "Add a Device Owner Key (DOK) entry to an OTP BIAK slot.\n"
			   "The key hash is SHA2-512 over the 512-byte RSA-4096 modulus.\n\n"
			   "Authorization Flag (--auth-type):\n"
			   "  0 = UID only        (Initialized-Secure)\n"
			   "  1 = PSID only       (Initialized-Secure)\n"
			   "  2 = UID + PSID      (Initialized-Secure)\n"
			   "  3 = NONE            (Uninitialized state, no signature)\n\n"
			   "WARNING: This operation modifies OTP and is IRREVERSIBLE!";

	static struct {
		struct switchtec_dev *dev;
		unsigned long key_slot;
		unsigned long auth_type;
		char *uid_hex;
		char *psid_hex;
		FILE *key_fimg;
		FILE *sig_fimg;
		unsigned long sig_type;
		int assume_yes;
	} cfg = {};

	const struct argconfig_options opts[] = {
		DEVICE_OPTION_MFG_PCI,
		{"key-slot", 'k', "", CFG_LONG, &cfg.key_slot,
			required_argument, "key slot index (0-11)"},
		{"auth-type", 't', "", CFG_LONG, &cfg.auth_type,
			required_argument, "Authorization flag (0=UID, 1=PSID, 2=UID+PSID, 3=NONE)"},
		{"uid", 'u', "", CFG_STRING, &cfg.uid_hex,
			required_argument, "UID hex string (512-bit, 128 hex chars)"},
		{"psid", 'p', "", CFG_STRING, &cfg.psid_hex,
			required_argument, "PSID hex string (128-bit, 32 hex chars)"},
		{"key", 'K', .cfg_type=CFG_FILE_R,
			.value_addr=&cfg.key_fimg,
			.argument_type=required_argument,
			.help="RSA 4096-bit key file (PEM format, public or private)"},
		{"signature", 's', .cfg_type=CFG_FILE_R,
			.value_addr=&cfg.sig_fimg,
			.argument_type=required_argument,
			.help="signature file (required unless --auth-type 3)"},
		{"sig-type", 'T', "", CFG_LONG, &cfg.sig_type,
			required_argument, "signature type (default 0)"},
		{"yes", 'y', "", CFG_NONE, &cfg.assume_yes, no_argument,
			"assume yes when prompted"},
		{NULL}
	};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));

	if (!switchtec_is_gen6(cfg.dev)) {
		fprintf(stderr, "This command is only supported on Gen6 devices!\n");
		return -1;
	}

	if (cfg.key_slot > 11) {
		fprintf(stderr, "Key slot must be 0-11!\n");
		return -1;
	}

	if (cfg.auth_type > DOK_AUTH_FLAG_NONE) {
		fprintf(stderr, "Authorization flag must be 0-3!\n");
		return -1;
	}

	if (cfg.key_fimg == NULL) {
		fprintf(stderr, "RSA 4096-bit key file is required (--key)!\n");
		return -1;
	}

	if (cfg.auth_type != DOK_AUTH_FLAG_NONE && cfg.sig_fimg == NULL) {
		fprintf(stderr, "Signature file is required for auth flag %lu!\n",
			cfg.auth_type);
		return -1;
	}

	ret = compute_rsa4k_hash(cfg.key_fimg, computed_hash);
	fclose(cfg.key_fimg);
	if (ret)
		return -2;

	memcpy(key_add.key_hash, computed_hash, sizeof(computed_hash));

	for (int i = 0; i < DEVICE_CONFIG_KEY_HASH_SIZE_DWORDS; i++)
		sprintf(hash_str + i * 8, "%08x", be32toh(computed_hash[i]));
	hash_str[128] = '\0';

	sig_len = 0;
	if (cfg.sig_fimg != NULL) {
		sig_len = fread(sig.sig_data, 1, sizeof(sig.sig_data), cfg.sig_fimg);
		fclose(cfg.sig_fimg);
		if (sig_len == 0) {
			fprintf(stderr, "Failed to read signature file!\n");
			return -2;
		}
	}

	key_add.sub_cmd = DOK_CONFIG_SUB_CMD_PROVISION;
	key_add.key_slot = cfg.key_slot;
	key_add.auth_type = cfg.auth_type;

	if (cfg.uid_hex)
		parse_hex_string(cfg.uid_hex, key_add.uid, SWITCHTEC_UID_LEN_DWORDS);
	if (cfg.psid_hex)
		parse_hex_string(cfg.psid_hex, key_add.psid, SWITCHTEC_PSID_LEN_DWORDS);

	if (cfg.auth_type == DOK_AUTH_FLAG_NONE) {
		uint8_t integrity_input[4 + 64 + 64 + 16];

		integrity_input[0] = (uint8_t)cfg.key_slot;
		integrity_input[1] = (uint8_t)cfg.auth_type;
		integrity_input[2] = 0;
		integrity_input[3] = 0;
		memcpy(&integrity_input[4], key_add.key_hash, 64);
		memcpy(&integrity_input[4 + 64], key_add.uid, 64);
		memcpy(&integrity_input[4 + 64 + 64], key_add.psid, 16);
		compute_sha512_dwords(integrity_input, sizeof(integrity_input),
				      key_add.integrity_hash);
	}

	printf("Adding DOK key entry:\n");
	printf("  Key Slot:        %lu\n", cfg.key_slot);
	printf("  Auth Flag:       %lu%s\n", cfg.auth_type,
		cfg.auth_type == DOK_AUTH_FLAG_NONE ?
			" (NONE - Uninitialized state)" : "");
	printf("  Key Type:        RSA4K\n");
	printf("  Key Hash:        %s\n", hash_str);
	if (sig_len)
		printf("  Signature Size:  %zu bytes\n", sig_len);
	else
		printf("  Auth:            integrity hash (no signature)\n");

	if (!cfg.assume_yes) {
		fprintf(stderr,
			"\nWARNING: This operation modifies OTP and is IRREVERSIBLE!\n");
		ret = ask_if_sure(cfg.assume_yes);
		if (ret)
			return -3;
	}

	if (cfg.auth_type != DOK_AUTH_FLAG_NONE) {
		sig.sub_cmd = DOK_CONFIG_SUB_CMD_SIGNATURE;
		sig.sig_type = cfg.sig_type;
		sig.total_len = sig_len;
		sig.total_crc = compute_crc32(sig.sig_data, sig_len);
		sig.data_len = sig_len;
		sig.offset = 0;

		ret = switchtec_dok_config_signature(cfg.dev, &sig);
		if (ret) {
			switchtec_perror("dok-key-add (signature)");
			return ret;
		}
	}

	ret = switchtec_dok_config_key_add(cfg.dev, &key_add);
	if (ret) {
		switchtec_perror("dok-key-add");
		return ret;
	}

	printf("DOK key entry added successfully.\n");
	return 0;
}
#endif /* HAVE_LIBCRYPTO */

#define CMD_DESC_DOK_KEY_REVOKE "revoke DOK key entry (Gen6 only)"

static int dok_key_revoke(int argc, char **argv)
{
	int ret;
	size_t sig_len;
	struct switchtec_dok_signature sig = {};
	struct switchtec_dok_key_revoke key_revoke = {};

	const char *desc = CMD_DESC_DOK_KEY_REVOKE "\n\n"
			   "Revoke a Device Owner Key (DOK) entry in an OTP BIAK slot.\n\n"
			   "Authorization Flag (--auth-type):\n"
			   "  0 = UID only        (Initialized-Secure)\n"
			   "  1 = PSID only       (Initialized-Secure)\n"
			   "  2 = UID + PSID      (Initialized-Secure)\n"
			   "  3 = NONE            (Uninitialized state, no signature)\n\n"
			   "WARNING: This operation modifies OTP and is IRREVERSIBLE!";

	static struct {
		struct switchtec_dev *dev;
		unsigned long key_slot;
		unsigned long auth_type;
		char *uid_hex;
		char *psid_hex;
		FILE *sig_fimg;
		unsigned long sig_type;
		int assume_yes;
	} cfg = {};

	const struct argconfig_options opts[] = {
		DEVICE_OPTION_MFG_PCI,
		{"key-slot", 'k', "", CFG_LONG, &cfg.key_slot,
			required_argument, "key slot index (0-11)"},
		{"auth-type", 't', "", CFG_LONG, &cfg.auth_type,
			required_argument, "Authorization flag (0=UID, 1=PSID, 2=UID+PSID, 3=NONE)"},
		{"uid", 'u', "", CFG_STRING, &cfg.uid_hex,
			required_argument, "UID hex string (512-bit, 128 hex chars)"},
		{"psid", 'p', "", CFG_STRING, &cfg.psid_hex,
			required_argument, "PSID hex string (128-bit, 32 hex chars)"},
		{"signature", 's', .cfg_type=CFG_FILE_R,
			.value_addr=&cfg.sig_fimg,
			.argument_type=required_argument,
			.help="signature file (required unless --auth-type 3)"},
		{"sig-type", 'T', "", CFG_LONG, &cfg.sig_type,
			required_argument, "signature type (default 0)"},
		{"yes", 'y', "", CFG_NONE, &cfg.assume_yes, no_argument,
			"assume yes when prompted"},
		{NULL}
	};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));

	if (!switchtec_is_gen6(cfg.dev)) {
		fprintf(stderr, "This command is only supported on Gen6 devices!\n");
		return -1;
	}

	if (cfg.key_slot > 11) {
		fprintf(stderr, "Key slot must be 0-11!\n");
		return -1;
	}

	if (cfg.auth_type > DOK_AUTH_FLAG_NONE) {
		fprintf(stderr, "Authorization flag must be 0-3!\n");
		return -1;
	}

	if (cfg.auth_type != DOK_AUTH_FLAG_NONE && cfg.sig_fimg == NULL) {
		fprintf(stderr, "Signature file is required for auth flag %lu!\n",
			cfg.auth_type);
		return -1;
	}

	sig_len = 0;
	if (cfg.sig_fimg != NULL) {
		sig_len = fread(sig.sig_data, 1, sizeof(sig.sig_data), cfg.sig_fimg);
		fclose(cfg.sig_fimg);
		if (sig_len == 0) {
			fprintf(stderr, "Failed to read signature file!\n");
			return -2;
		}
	}

	key_revoke.sub_cmd = DOK_CONFIG_SUB_CMD_REVOKE;
	key_revoke.key_slot = cfg.key_slot;
	key_revoke.auth_type = cfg.auth_type;

	if (cfg.uid_hex)
		parse_hex_string(cfg.uid_hex, key_revoke.uid, SWITCHTEC_UID_LEN_DWORDS);
	if (cfg.psid_hex)
		parse_hex_string(cfg.psid_hex, key_revoke.psid, SWITCHTEC_PSID_LEN_DWORDS);

	if (cfg.auth_type == DOK_AUTH_FLAG_NONE) {
		uint8_t integrity_input[4 + 64 + 16];

		integrity_input[0] = (uint8_t)cfg.key_slot;
		integrity_input[1] = (uint8_t)cfg.auth_type;
		integrity_input[2] = 0;
		integrity_input[3] = 0;
		memcpy(&integrity_input[4], key_revoke.uid, 64);
		memcpy(&integrity_input[4 + 64], key_revoke.psid, 16);
		compute_sha512_dwords(integrity_input, sizeof(integrity_input),
				      key_revoke.integrity_hash);
	}

	printf("Revoking DOK key entry:\n");
	printf("  Key Slot:        %lu\n", cfg.key_slot);
	printf("  Auth Flag:       %lu%s\n", cfg.auth_type,
		cfg.auth_type == DOK_AUTH_FLAG_NONE ?
			" (NONE - Uninitialized state)" : "");
	if (sig_len)
		printf("  Signature Size:  %zu bytes\n", sig_len);
	else
		printf("  Auth:            integrity hash (no signature)\n");

	if (!cfg.assume_yes) {
		fprintf(stderr,
			"\nWARNING: This operation modifies OTP and is IRREVERSIBLE!\n");
		ret = ask_if_sure(cfg.assume_yes);
		if (ret)
			return -3;
	}

	if (cfg.auth_type != DOK_AUTH_FLAG_NONE) {
		sig.sub_cmd = DOK_CONFIG_SUB_CMD_SIGNATURE;
		sig.sig_type = cfg.sig_type;
		sig.total_len = sig_len;
		sig.total_crc = compute_crc32(sig.sig_data, sig_len);
		sig.data_len = sig_len;
		sig.offset = 0;

		ret = switchtec_dok_config_signature(cfg.dev, &sig);
		if (ret) {
			switchtec_perror("dok-key-revoke (signature)");
			return ret;
		}
	}

	ret = switchtec_dok_config_key_revoke(cfg.dev, &key_revoke);
	if (ret) {
		switchtec_perror("dok-key-revoke");
		return ret;
	}

	printf("DOK key entry revoked successfully.\n");
	return 0;
}

#define CMD_DESC_JTAG_STATUS_GET "get JTAG lock status (Gen6 only)"

static int jtag_status_get(int argc, char **argv)
{
	int ret;
	uint32_t jtag_status;

	const char *desc = CMD_DESC_JTAG_STATUS_GET "\n\n"
			   "Retrieve the current JTAG lock status from the device.\n"
			   "Status values:\n"
			   "  0 = JTAG Locked\n"
			   "  1 = JTAG Unlocked";

	static struct {
		struct switchtec_dev *dev;
	} cfg = {};

	const struct argconfig_options opts[] = {
		DEVICE_OPTION_MFG_PCI,
		{NULL}
	};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));

	if (!switchtec_is_gen6(cfg.dev)) {
		fprintf(stderr, "This command is only supported on Gen6 devices!\n");
		return -1;
	}

	ret = switchtec_dbg_unlock_status_get_gen6(cfg.dev, &jtag_status);
	if (ret) {
		switchtec_perror("mfg jtag-status-get");
		return ret;
	}

	printf("JTAG Status: %s\n", (jtag_status & 1) ? "Unlocked" : "Locked");

	return 0;
}

#define CMD_DESC_SECURE_STATE_GET "get current device secure state (Gen6 only)"

static int secure_state_get(int argc, char **argv)
{
	int ret;
	enum switchtec_secure_state_gen6 state;

	const char *desc = CMD_DESC_SECURE_STATE_GET "\n\n"
			   "Retrieve the current device secure state.\n"
			   "State values:\n"
			   "  0 = UNINITIALIZED_SECURE_CAPABLE\n"
			   "  1 = UNPROVISIONED_SECURED\n"
			   "  2 = INITIALIZED_SECURED\n"
			   "  3 = INITIALIZED_UNSECURED";

	static struct {
		struct switchtec_dev *dev;
	} cfg = {};

	const struct argconfig_options opts[] = {
		DEVICE_OPTION_MFG_PCI,
		{NULL}
	};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));

	if (!switchtec_is_gen6(cfg.dev)) {
		fprintf(stderr, "This command is only supported on Gen6 devices!\n");
		return -1;
	}

	ret = switchtec_secure_state_get_gen6(cfg.dev, &state);
	if (ret) {
		switchtec_perror("mfg secure-state-get");
		return ret;
	}

	printf("Secure State: ");
	switch (state) {
	case SWITCHTEC_GEN6_UNINITIALIZED_SECURE_CAPABLE:
		printf("UNINITIALIZED_SECURE_CAPABLE (0x%x)\n", state);
		break;
	case SWITCHTEC_GEN6_UNPROVISIONED_SECURED:
		printf("UNPROVISIONED_SECURED (0x%x)\n", state);
		break;
	case SWITCHTEC_GEN6_INITIALIZED_SECURED:
		printf("INITIALIZED_SECURED (0x%x)\n", state);
		break;
	case SWITCHTEC_GEN6_INITIALIZED_UNSECURED:
		printf("INITIALIZED_UNSECURED (0x%x)\n", state);
		break;
	default:
		printf("Unknown (0x%x)\n", state);
	}

	return 0;
}

static const struct cmd commands[] = {
	CMD(ping, CMD_DESC_PING),
	CMD(info, CMD_DESC_INFO),
	CMD(mailbox, CMD_DESC_MAILBOX),
	CMD(image_list, CMD_DESC_IMAGE_LIST),
	CMD(image_select, CMD_DESC_IMAGE_SELECT),
	CMD(fw_transfer, CMD_DESC_FW_TRANSFER),
	CMD(fw_execute, CMD_DESC_FW_EXECUTE),
	CMD(boot_resume, CMD_DESC_BOOT_RESUME),
	CMD(state_set, CMD_DESC_STATE_SET),
	CMD(config_set, CMD_DESC_CONFIG_SET),
	CMD(kmsk_entry_add, CMD_DESC_KMSK_ENTRY_ADD),
	CMD(debug_unlock_token, CMD_DESC_DEBUG_TOKEN),
	CMD(device_config_get, CMD_DESC_DEVICE_CONFIG_GET),
	CMD(device_config_set_device, CMD_DESC_DEVICE_CONFIG_SET_DEVICE),
	CMD(device_config_set_customer, CMD_DESC_DEVICE_CONFIG_SET_CUSTOMER),
	CMD(device_config_set_security, CMD_DESC_DEVICE_CONFIG_SET_SECURITY),
#if HAVE_LIBCRYPTO
	CMD(dok_key_add, CMD_DESC_DOK_KEY_ADD),
#endif
	CMD(dok_key_revoke, CMD_DESC_DOK_KEY_REVOKE),
	CMD(debug_unlock, CMD_DESC_DEBUG_UNLOCK),
	CMD(debug_lock_update, CMD_DESC_DEBUG_LOCK_UPDATE),
	CMD(jtag_status_get, CMD_DESC_JTAG_STATUS_GET),
	CMD(secure_state_get, CMD_DESC_SECURE_STATE_GET),
	{}
};

static struct subcommand subcmd = {
	.name = "mfg",
	.cmds = commands,
	.desc = "Manufacturing Process Commands",
	.long_desc = "These commands control and manage"
		  " mfg settings.",
};

REGISTER_SUBCMD(subcmd);

#endif /* __linux__ */
