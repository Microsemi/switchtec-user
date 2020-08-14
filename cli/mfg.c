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

static char *spi_rate_str[] = {
	"100", "67", "50", "40", "33.33", "28.57",
	"25", "22.22", "20", "18.18"
};

static const char* phase_id_to_string(enum switchtec_boot_phase phase_id)
{
	switch(phase_id) {
	case SWITCHTEC_BOOT_PHASE_BL1:
		return "BL1";
	case SWITCHTEC_BOOT_PHASE_BL2:
		return "BL2";
	case SWITCHTEC_BOOT_PHASE_FW:
		return "Main Firmware";
	default:
		return "Unknown Phase";
	}
}

#define CMD_DESC_PING "ping device and get current boot phase"

static int ping(int argc, char **argv)
{
	int ret;
	enum switchtec_boot_phase phase_id;
	static struct {
		struct switchtec_dev *dev;
	} cfg = {};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION_MFG,
		{NULL}
	};

	argconfig_parse(argc, argv, CMD_DESC_PING, opts, &cfg, sizeof(cfg));

	ret = switchtec_get_device_info(cfg.dev, &phase_id, NULL, NULL);
	if (ret) {
		switchtec_perror("mfg ping");
		return ret;
	}

	printf("Mfg Ping: \t\tSUCCESS\n");
	printf("Current Boot Phase: \t%s\n", phase_id_to_string(phase_id));

	return 0;
}

static void print_security_config(struct switchtec_security_cfg_state *state)
{
	int key_idx;
	int i;

	printf("\nBasic Secure Settings %s\n",
		state->basic_setting_valid? "(Valid)":"(Invalid)");

	printf("\tSecure State: \t\t\t");
	switch(state->secure_state) {
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

	printf("\tJTAG/EJTAG Debug State: \t");
	switch(state->debug_mode) {
	case SWITCHTEC_DEBUG_MODE_ENABLED:
		printf("Always Enabled\n");
		break;
	case SWITCHTEC_DEBUG_MODE_DISABLED_BUT_ENABLE_ALLOWED:
		printf("Disabled by Default But Can Be Enabled\n");
		break;
	case SWITCHTEC_DEBUG_MODE_DISABLED:
		printf("Always Disabled\n");
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

	printf("\tSPI Clock Rate: \t\t%s MHz\n",
		spi_rate_str[state->spi_clk_rate-1]);

	printf("\tI2C Recovery TMO: \t\t%d Second(s)\n",
		state->i2c_recovery_tmo);
	printf("\tI2C Port: \t\t\t%d\n", state->i2c_port);
	printf("\tI2C Address (7-bits): \t\t0x%02x\n", state->i2c_addr);
	printf("\tI2C Command Map: \t\t0x%08x\n\n", state->i2c_cmd_map);

	printf("Exponent Hex Data %s: \t\t0x%08x\n",
		state->public_key_exp_valid? "(Valid)":"(Invalid)",
		state->public_key_exponent);

	printf("KMSK Entry Number %s: \t\t%d\n",
		state->public_key_num_valid? "(Valid)":"(Invalid)",
		state->public_key_num);

	if (state->public_key_ver)
		printf("Current KMSK index %s: \t\t%d\n",
			state->public_key_ver_valid? "(Valid)":"(Invalid)",
			state->public_key_ver);
	else
		printf("Current KMSK index %s: \t\tNot Set\n",
			state->public_key_ver_valid? "(Valid)":"(Invalid)");

	for(key_idx = 0; key_idx < state->public_key_num; key_idx++) {
		printf("KMSK Entry %d:  ", key_idx + 1);
		for(i = 0; i < SWITCHTEC_KMSK_LEN; i++)
				printf("%02x", state->public_key[key_idx][i]);
		printf("\n");
	}
}

static void print_security_cfg_set(struct switchtec_security_cfg_set *set)
{
	printf("\nBasic Secure Settings\n");

	printf("\tJTAG/EJTAG State After Reset: \t%d\n",
		set->jtag_lock_after_reset);

	printf("\tJTAG/EJTAG State After BL1: \t%d\n",
		set->jtag_lock_after_bl1);

	printf("\tJTAG/EJTAG Unlock IN BL1: \t%d\n",
		set->jtag_bl1_unlock_allowed);

	printf("\tJTAG/EJTAG Unlock AFTER BL1: \t%d\n",
		set->jtag_post_bl1_unlock_allowed);

	printf("\tSPI Clock Rate: \t\t%s MHz\n",
		spi_rate_str[set->spi_clk_rate-1]);

	printf("\tI2C Recovery TMO: \t\t%d Second(s)\n",
		set->i2c_recovery_tmo);

	printf("\tI2C Port: \t\t\t%d\n", set->i2c_port);
	printf("\tI2C Address (7-bits): \t\t0x%02x\n", set->i2c_addr);
	printf("\tI2C Command Map: \t\t0x%08x\n", set->i2c_cmd_map);

	printf("Exponent Hex Data: \t\t\t0x%08x\n", set->public_key_exponent);
}

#define CMD_DESC_INFO "display security settings"

static int info(int argc, char **argv)
{
	int ret;
	enum switchtec_boot_phase phase_id;

	struct switchtec_sn_ver_info sn_info = {};

	static struct {
		struct switchtec_dev *dev;
	} cfg = {};

	const struct argconfig_options opts[] = {
		DEVICE_OPTION_MFG,
		{NULL}
	};

	struct switchtec_security_cfg_state state = {};

	argconfig_parse(argc, argv, CMD_DESC_INFO, opts, &cfg, sizeof(cfg));

	phase_id = switchtec_boot_phase(cfg.dev);
	printf("Current Boot Phase: \t\t\t%s\n", phase_id_to_string(phase_id));

	ret = switchtec_sn_ver_get(cfg.dev, &sn_info);
	if (ret) {
		switchtec_perror("mfg info");
		return ret;
	}
	printf("Chip Serial: \t\t\t\t0x%08x\n", sn_info.chip_serial);
	printf("Key Manifest Secure Version: \t\t0x%08x\n", sn_info.ver_km);
	printf("BL2 Secure Version: \t\t\t0x%08x\n", sn_info.ver_bl2);
	printf("Main Secure Version: \t\t\t0x%08x\n", sn_info.ver_main);
	printf("Secure Unlock Version: \t\t\t0x%08x\n", sn_info.ver_sec_unlock);

	if (phase_id == SWITCHTEC_BOOT_PHASE_BL2) {
		printf("\nOther secure settings are only shown in the BL1 or Main Firmware phase.\n\n");
		return 0;
	}

	ret = switchtec_security_config_get(cfg.dev, &state);
	if (ret) {
		switchtec_perror("mfg info");
		return ret;
	}

	print_security_config(&state);

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
		DEVICE_OPTION_MFG,
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
	} cfg = {
		.bl2 = SWITCHTEC_ACTIVE_INDEX_NOT_SET,
		.firmware = SWITCHTEC_ACTIVE_INDEX_NOT_SET,
		.config = SWITCHTEC_ACTIVE_INDEX_NOT_SET,
		.keyman = SWITCHTEC_ACTIVE_INDEX_NOT_SET
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
		{NULL}
	};

	argconfig_parse(argc, argv, CMD_DESC_IMAGE_SELECT, opts, &cfg, sizeof(cfg));

	if (cfg.bl2 == SWITCHTEC_ACTIVE_INDEX_NOT_SET &&
	    cfg.firmware == SWITCHTEC_ACTIVE_INDEX_NOT_SET &&
	    cfg.config == SWITCHTEC_ACTIVE_INDEX_NOT_SET &&
	    cfg.keyman == SWITCHTEC_ACTIVE_INDEX_NOT_SET) {
		fprintf(stderr,
			"One of BL2, Config, Key Manifest or Firmware indices must be set in this command!\n");
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
	ret = switchtec_fw_write_file(cfg.dev, cfg.fimg, 1, cfg.force,
				      progress_update);
	fclose(cfg.fimg);

	if (ret) {
		printf("\n");
		switchtec_fw_perror("mfg fw-transfer", ret);
		return -3;
	}

	progress_finish(0);
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
		DEVICE_OPTION_MFG,
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

	print_security_config(&state);

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

	static struct {
		struct switchtec_dev *dev;
		FILE *setting_fimg;
		char *setting_file;
		int assume_yes;
	} cfg = {};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION_MFG,
		{"setting_file", .cfg_type=CFG_FILE_R,
			.value_addr=&cfg.setting_fimg,
			.argument_type=required_positional,
			.help="security setting file"},
		{"yes", 'y', "", CFG_NONE, &cfg.assume_yes, no_argument,
			"assume yes when prompted"},
		{NULL}
	};

	argconfig_parse(argc, argv, CMD_DESC_CONFIG_SET, opts, &cfg, sizeof(cfg));

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

	ret = switchtec_read_sec_cfg_file(cfg.setting_fimg, &settings);
	fclose(cfg.setting_fimg);
	if (ret) {
		fprintf(stderr, "Invalid secure setting file: %s!\n",
			cfg.setting_file);
		return -3;
	}

	printf("Writing the below settings to device: \n");
	print_security_cfg_set(&settings);

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
		DEVICE_OPTION_MFG,
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
		if (!cfg.assume_yes)
			fprintf(stderr,
				"WARNING: the specified KMSK entry already exists on the device.\n"
				"Writing duplicate KMSK entries could make your device unbootable!\n");
		ret = ask_if_sure(cfg.assume_yes);
		if (ret)
			return ret;
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
	for(i = 0; i < SWITCHTEC_KMSK_LEN; i++)
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
	} cfg = {
		.unlock_version = 0xffff,
	};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION_MFG,
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

	ret = switchtec_dbg_unlock(cfg.dev, cfg.serial, cfg.unlock_version,
				   &pubk, &sig);
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
		DEVICE_OPTION_MFG,
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

#define CMD_DESC_DEBUG_TOKEN "generate debug unlock token file for the device"
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
			   "required for command 'mfg debug-unlock'";

	struct {
		struct switchtec_dev *dev;
		int out_fd;
		const char *out_filename;
	} cfg = {};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION_MFG,
		{"token_file", .cfg_type=CFG_FD_WR, .value_addr=&cfg.out_fd,
		  .argument_type=optional_positional,
		  .force_default="debug.tkn",
		  .help="debug unlock token file"},
		{NULL}};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));

	ret = switchtec_sn_ver_get(cfg.dev, &sn_info);
	if (ret) {
		switchtec_perror("mfg debug unlock token");
		return ret;
	}

	token.id = htole32(1);
	token.serial = htole32(sn_info.chip_serial);
	token.version = htole32(sn_info.ver_sec_unlock);

	ret = write(cfg.out_fd, &token, sizeof(token));
	if(ret <= 0) {
		switchtec_perror("mfg debug unlock token");
		return ret;
	}

	fprintf(stderr, "\nToken data saved to %s\n", cfg.out_filename);
	close(cfg.out_fd);

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
	CMD(debug_unlock, CMD_DESC_DEBUG_UNLOCK),
	CMD(debug_lock_update, CMD_DESC_DEBUG_LOCK_UPDATE),
	CMD(debug_unlock_token, CMD_DESC_DEBUG_TOKEN),
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
