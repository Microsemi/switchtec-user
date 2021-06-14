/*
 * Microsemi Switchtec(tm) PCIe Management Command Line Interface
 * Copyright (c) 2021, Microsemi Corporation
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
#include "common.h"

#include <switchtec/switchtec.h>
#include <switchtec/utils.h>

#include <stdio.h>

struct diag_common_cfg {
	struct switchtec_dev *dev;
	struct switchtec_status port;
	enum switchtec_diag_end end;
	enum switchtec_diag_link link;
	int port_id;
	int far_end;
	int prev;
};

#define DEFAULT_DIAG_COMMON_CFG {	\
	.port_id = -1,			\
}

#define PORT_OPTION {							\
	"port", 'p', "PORT_ID", CFG_NONNEGATIVE, &cfg.port_id,		\
	required_argument, "physical port ID to dump data for",		\
}
#define FAR_END_OPTION {						\
	"far-end", 'f', "", CFG_NONE, &cfg.far_end, no_argument,	\
	"get the far-end coefficients instead of the local ones",	\
}
#define PREV_OPTION {							\
	"prev", 'P', "", CFG_NONE, &cfg.prev, no_argument,		\
	"return the data for the previous link",			\
}

static int get_port(struct switchtec_dev *dev, int port_id,
		    struct switchtec_status *port)
{
	struct switchtec_status *status;
	int i, ports;

	ports = switchtec_status(dev, &status);
	if (ports < 0) {
		switchtec_perror("status");
		return ports;
	}

	for (i = 0; i < ports; i++) {
		if (status[i].port.phys_id == port_id ||
		    (port_id == -1 && status[i].port.upstream)) {
			*port = status[i];
			switchtec_status_free(status, ports);
			return 0;
		}
	}

	fprintf(stderr, "Invalid physical port id: %d\n", port_id);
	switchtec_status_free(status, ports);
	return -1;
}

static int diag_parse_common_cfg(int argc, char **argv, const char *desc,
				 struct diag_common_cfg *cfg,
				 const struct argconfig_options *opts)
{
	int ret;

	argconfig_parse(argc, argv, desc, opts, cfg, sizeof(*cfg));

	ret = get_port(cfg->dev, cfg->port_id, &cfg->port);
	if (ret)
		return ret;

	cfg->port_id = cfg->port.port.phys_id;

	if (cfg->far_end)
		cfg->end = SWITCHTEC_DIAG_FAR_END;
	else
		cfg->end = SWITCHTEC_DIAG_LOCAL;

	if (cfg->prev)
		cfg->link = SWITCHTEC_DIAG_LINK_PREVIOUS;
	else
		cfg->link = SWITCHTEC_DIAG_LINK_CURRENT;

	return 0;
}

#define CMD_DESC_LIST_MRPC "List permissible MRPC commands"

static int list_mrpc(int argc, char **argv)
{
	struct switchtec_mrpc table[MRPC_MAX_ID];
	int i, ret;

	static struct {
		struct switchtec_dev *dev;
		int all;
	} cfg = {};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"all", 'a', "", CFG_NONE, &cfg.all, no_argument,
		 "print all MRPC commands, including ones that are unknown"},
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_LIST_MRPC, opts, &cfg,
			sizeof(cfg));
	ret = switchtec_diag_perm_table(cfg.dev, table);
	if (ret) {
		switchtec_perror("perm_table");
		return -1;
	}

	for (i = 0; i < MRPC_MAX_ID; i++) {
		if (!table[i].tag)
			continue;
		if (!cfg.all && table[i].reserved)
			continue;

		printf("  0x%03x  %-25s  %s\n", i, table[i].tag,
		       table[i].desc);
	}

	return 0;
}

#define CMD_DESC_PORT_EQ_TXCOEFF "Dump port equalization coefficients"

static int port_eq_txcoeff(int argc, char **argv)
{
	struct diag_common_cfg cfg = DEFAULT_DIAG_COMMON_CFG;
	struct switchtec_port_eq_coeff coeff;
	int i, ret;

	const struct argconfig_options opts[] = {
		DEVICE_OPTION, FAR_END_OPTION, PORT_OPTION, PREV_OPTION, {}
	};

	ret = diag_parse_common_cfg(argc, argv, CMD_DESC_PORT_EQ_TXCOEFF,
				    &cfg, opts);
	if (ret)
		return ret;

	ret = switchtec_diag_port_eq_tx_coeff(cfg.dev, cfg.port_id, cfg.end,
					      cfg.link, &coeff);
	if (ret) {
		switchtec_perror("port_eq_coeff");
		return -1;
	}

	printf("%s TX Coefficients for physical port %d %s\n\n",
	       cfg.far_end ? "Far End" : "Local", cfg.port_id,
	       cfg.prev ? "(Previous Link-Up)" : "");
	printf("Lane  Pre-Cursor  Post-Cursor\n");

	for (i = 0; i < coeff.lane_cnt; i++) {
		printf("%4d  %7d      %8d\n", i, coeff.cursors[i].pre,
		       coeff.cursors[i].post);
	}

	return 0;
}

#define CMD_DESC_PORT_EQ_TXFSLF "Dump FS/LF output data"

static int port_eq_txfslf(int argc, char **argv)
{
	struct diag_common_cfg cfg = DEFAULT_DIAG_COMMON_CFG;
	struct switchtec_port_eq_tx_fslf data;
	int i, ret;

	const struct argconfig_options opts[] = {
		DEVICE_OPTION, FAR_END_OPTION, PORT_OPTION, PREV_OPTION, {}
	};

	ret = diag_parse_common_cfg(argc, argv, CMD_DESC_PORT_EQ_TXFSLF,
				    &cfg, opts);
	if (ret)
		return ret;

	printf("%s Equalization FS/LF data for physical port %d %s\n\n",
	       cfg.far_end ? "Far End" : "Local", cfg.port_id,
	       cfg.prev ? "(Previous Link-Up)" : "");
	printf("Lane    FS    LF\n");

	for (i = 0; i < cfg.port.neg_lnk_width; i++) {
		ret = switchtec_diag_port_eq_tx_fslf(cfg.dev, cfg.port_id, i,
				cfg.end, cfg.link, &data);
		if (ret) {
			switchtec_perror("port_eq_fs_ls");
			return -1;
		}

		printf("%4d  %4d  %4d\n", i, data.fs, data.lf);
	}

	return 0;
}

#define CMD_DESC_PORT_EQ_TXTABLE "Dump far end port equalization table"

static int port_eq_txtable(int argc, char **argv)
{
	struct diag_common_cfg cfg = DEFAULT_DIAG_COMMON_CFG;
	struct switchtec_port_eq_table table;
	int i, ret;

	const struct argconfig_options opts[] = {
		DEVICE_OPTION, PORT_OPTION, PREV_OPTION, {}
	};

	ret = diag_parse_common_cfg(argc, argv, CMD_DESC_PORT_EQ_TXTABLE,
				    &cfg, opts);
	if (ret)
		return ret;

	ret = switchtec_diag_port_eq_tx_table(cfg.dev, cfg.port_id,
					      cfg.link, &table);
	if (ret) {
		switchtec_perror("port_eq_table");
		return -1;
	}

	printf("Far End TX Equalization Table for physical port %d, lane %d %s\n\n",
	       cfg.port_id, table.lane_id, cfg.prev ? "(Previous Link-Up)" : "");
	printf("Step  Pre-Cursor  Post-Cursor  FOM  Pre-Up  Post-Up  Error  Active  Speed\n");

	for (i = 0; i < table.step_cnt; i++) {
		printf("%4d  %10d  %11d  %3d  %6d  %7d  %5d  %6d  %5d\n",
		       i, table.steps[i].pre_cursor, table.steps[i].post_cursor,
		       table.steps[i].fom, table.steps[i].pre_cursor_up,
		       table.steps[i].post_cursor_up, table.steps[i].error_status,
		       table.steps[i].active_status, table.steps[i].speed);
	}

	return 0;
}

#define CMD_DESC_RCVR_OBJ "Dump analog RX coefficients/adaptation objects"

static int rcvr_obj(int argc, char **argv)
{
	struct diag_common_cfg cfg = DEFAULT_DIAG_COMMON_CFG;
	struct switchtec_rcvr_obj obj;
	int i, j, ret;

	const struct argconfig_options opts[] = {
		DEVICE_OPTION, PORT_OPTION, PREV_OPTION, {}
	};

	ret = diag_parse_common_cfg(argc, argv, CMD_DESC_RCVR_OBJ,
				    &cfg, opts);
	if (ret)
		return ret;

	printf("Coefficients for physical port %d %s\n\n", cfg.port_id,
	       cfg.prev ? "(Previous Link-Up)" : "");
	printf("Lane  CTLE  Tgt-Amp  Spec-DFE  DFE0 DFE1 DFE2 DFE3 DFE4 DFE5 DFE6\n");

	for (i = 0; i < cfg.port.neg_lnk_width; i++) {
		ret = switchtec_diag_rcvr_obj(cfg.dev, cfg.port_id, i,
					      cfg.link, &obj);
		if (ret) {
			switchtec_perror("rcvr_obj");
			return -1;
		}

		printf("%4d  %4d  %6d   %7d   ", i, obj.ctle,
		       obj.target_amplitude, obj.speculative_dfe);
		for (j = 0; j < ARRAY_SIZE(obj.dynamic_dfe); j++)
			printf("%4d ", obj.dynamic_dfe[j]);
		printf("\n");
	}

	return 0;
}

#define CMD_DESC_RCVR_EXTENDED "Dump RX mode and DTCLK"

static int rcvr_extended(int argc, char **argv)
{
	struct diag_common_cfg cfg = DEFAULT_DIAG_COMMON_CFG;
	struct switchtec_rcvr_ext ext;
	int i, ret;

	const struct argconfig_options opts[] = {
		DEVICE_OPTION, PORT_OPTION, PREV_OPTION, {}
	};

	ret = diag_parse_common_cfg(argc, argv, CMD_DESC_RCVR_EXTENDED,
				    &cfg, opts);
	if (ret)
		return ret;

	printf("Mode and DTCLCK for physical port %d %s\n\n",
	       cfg.port_id, cfg.prev ? "(Previous Link-Up)" : "");
	printf("Lane      MODE   DTCLK_5  DTCLK_8_6  DTCLK_9\n");

	for (i = 0; i < cfg.port.neg_lnk_width; i++) {
		ret = switchtec_diag_rcvr_ext(cfg.dev, cfg.port_id, i,
					      cfg.link, &ext);
		if (ret) {
			switchtec_perror("rx_mode");
			return -1;
		}

		printf("%4d  %#8x  %7d  %9d  %7d\n", i, ext.ctle2_rx_mode,
		       ext.dtclk_5, ext.dtclk_8_6, ext.dtclk_9);
	}

	return 0;
}

#define CMD_DESC_REF_CLK "Enable or disable the output reference clock of a stack"

static int refclk(int argc, char **argv)
{
	int ret;

	static struct {
		struct switchtec_dev *dev;
		int stack_id;
		int enable;
		int disable;
	} cfg = {};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"disable", 'd', "", CFG_NONE, &cfg.disable, no_argument,
		 "disable the rfclk output"},
		{"enable", 'e', "", CFG_NONE, &cfg.enable, no_argument,
		 "enable the rfclk output"},
		{"stack", 's', "NUM", CFG_POSITIVE, &cfg.stack_id,
		required_argument, "stack to operate on"},
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_REF_CLK, opts, &cfg,
			sizeof(cfg));

	if (!cfg.enable && !cfg.disable) {
		fprintf(stderr, "Must set either --enable or --disable");
		return -1;
	}

	if (cfg.enable && cfg.disable) {
		fprintf(stderr, "Must not set both --enable and --disable");
		return -1;
	}

	ret = switchtec_diag_refclk_ctl(cfg.dev, cfg.stack_id, cfg.enable);
	if (ret) {
		switchtec_perror("refclk_ctl");
		return -1;
	}

	printf("REFCLK Output %s for Stack %d\n",
	       cfg.enable ? "Enabled" : "Disabled", cfg.stack_id);

	return 0;
}

static const struct cmd commands[] = {
	CMD(list_mrpc,		CMD_DESC_LIST_MRPC),
	CMD(port_eq_txcoeff,	CMD_DESC_PORT_EQ_TXCOEFF),
	CMD(port_eq_txfslf,	CMD_DESC_PORT_EQ_TXFSLF),
	CMD(port_eq_txtable,	CMD_DESC_PORT_EQ_TXTABLE),
	CMD(rcvr_extended,	CMD_DESC_RCVR_EXTENDED),
	CMD(rcvr_obj,		CMD_DESC_RCVR_OBJ),
	CMD(refclk,		CMD_DESC_REF_CLK),
	{}
};

static struct subcommand subcmd = {
	.name = "diag",
	.cmds = commands,
	.desc = "Diagnostic Information",
	.long_desc = "These functions provide diagnostic information from "
		"the switch",
};

REGISTER_SUBCMD(subcmd);
