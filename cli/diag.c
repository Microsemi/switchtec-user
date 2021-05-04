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
	"port", 'p', "PORT_ID", CFG_POSITIVE, &cfg.port_id,		\
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
		DEVICE_OPTION, FAR_END_OPTION, PORT_OPTION, {}
	};

	ret = diag_parse_common_cfg(argc, argv, CMD_DESC_PORT_EQ_TXFSLF,
				    &cfg, opts);
	if (ret)
		return ret;

	printf("%s Equalization FS/LF data for physical port %d\n\n",
	       cfg.far_end ? "Far End" : "Local", cfg.port_id);
	printf("Lane    FS    LF\n");

	for (i = 0; i < cfg.port.neg_lnk_width; i++) {
		ret = switchtec_diag_port_eq_tx_fslf(cfg.dev, cfg.port_id, i,
						     cfg.end, &data);
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
		DEVICE_OPTION, PORT_OPTION, {}
	};

	ret = diag_parse_common_cfg(argc, argv, CMD_DESC_PORT_EQ_TXTABLE,
				    &cfg, opts);
	if (ret)
		return ret;

	ret = switchtec_diag_port_eq_tx_table(cfg.dev, cfg.port_id,
				SWITCHTEC_DIAG_LINK_CURRENT, &table);
	if (ret) {
		switchtec_perror("port_eq_table");
		return -1;
	}

	printf("Far End TX Equalization Table for physical port %d, lane %d\n\n",
	       cfg.port_id, table.lane_id);
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

static const struct cmd commands[] = {
	CMD(port_eq_txcoeff,	CMD_DESC_PORT_EQ_TXCOEFF),
	CMD(port_eq_txfslf,	CMD_DESC_PORT_EQ_TXFSLF),
	CMD(port_eq_txtable,	CMD_DESC_PORT_EQ_TXTABLE),
	CMD(rcvr_extended,	CMD_DESC_RCVR_EXTENDED),
	CMD(rcvr_obj,		CMD_DESC_RCVR_OBJ),
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
