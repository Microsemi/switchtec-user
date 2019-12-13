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

#include "commands.h"
#include "argconfig.h"
#include "suffix.h"
#include "progress.h"
#include "gui.h"
#include "common.h"
#include "progress.h"

#include <switchtec/switchtec.h>
#include <switchtec/utils.h>
#include <switchtec/endian.h>

#include <locale.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

static int ping(int argc, char **argv)
{
	const char *desc = "Ping firmware and get current boot phase";
	int ret;
	enum switchtec_boot_phase phase_id;
	static struct {
		struct switchtec_dev *dev;
	} cfg = {};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION_NO_PAX,
		{NULL}
	};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));

	ret = switchtec_get_device_info(cfg.dev, &phase_id, NULL, NULL);
	if (ret < 0) {
		switchtec_perror("mfg ping");
		return ret;
	}

	printf("Mfg Ping: \t\tSUCCESS\n");
	printf("Current Boot Phase: \t");
	switch(phase_id) {
	case SWITCHTEC_BOOT_PHASE_BL1:
		printf("BL1\n");
		break;
	case SWITCHTEC_BOOT_PHASE_BL2:
		printf("BL2\n");
		break;
	case SWITCHTEC_BOOT_PHASE_FW:
		printf("Main Firmware\n");
		break;
	default:
		printf("Unknown Phase\n");
		return -2;
	}

	return 0;
}

static const struct cmd commands[] = {
	{"ping", ping, "Ping firmware and get current boot phase"},
};

static struct subcommand subcmd = {
	.name = "mfg",
	.cmds = commands,
	.desc = "Manufacturing Process Commands",
	.long_desc = "These commands control and manage"
		  " mfg settings.",
};

REGISTER_SUBCMD(subcmd);
