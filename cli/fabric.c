/*
 * Microsemi Switchtec(tm) PCIe Management Command Line Interface
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

#include "commands.h"
#include "argconfig.h"
#include "common.h"

#include <switchtec/switchtec.h>
#include <switchtec/portable.h>
#include <switchtec/fabric.h>

#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <errno.h>
#include <ctype.h>

static int gfms_bind(int argc, char **argv)
{
	const char *desc = "Unbind the EP(function) to the specified host";
	int ret;

	static struct {
		struct switchtec_dev *dev;
		struct switchtec_gfms_bind_req bind_req;
	} cfg ;

	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"host_sw_idx", 's', "NUM", CFG_INT, &cfg.bind_req.host_sw_idx,
		 required_argument,"Host switch index", .require_in_usage = 1},
		{"phys_port_id", 'p', "NUM", CFG_INT,
		 &cfg.bind_req.host_phys_port_id,
		 required_argument,"Host physical port id",
		 .require_in_usage = 1},
		{"log_port_id", 'l', "NUM", CFG_INT,
		 &cfg.bind_req.host_log_port_id, required_argument,
		 "Host logical port id", .require_in_usage = 1},
		{"pdfid", 'f', "NUM", CFG_INT, &cfg.bind_req.pdfid,
		 required_argument,"Endpoint function's PDFID",
		 .require_in_usage = 1},
		{NULL}};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));

	ret = switchtec_gfms_bind(cfg.dev, &cfg.bind_req);
	if (ret) {
		switchtec_perror("gfms_bind");
		return ret;
	}

	return 0;
}

static int gfms_unbind(int argc, char **argv)
{
	const char *desc = "Unbind the EP(function) from the specified host";
	int ret;

	static struct {
		struct switchtec_dev *dev;
		struct switchtec_gfms_unbind_req unbind_req;
	} cfg;

	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"host_sw_idx", 's', "NUM", CFG_INT,
		 &cfg.unbind_req.host_sw_idx, required_argument,
		 "Host switch index", .require_in_usage = 1,},
		{"phys_port_id", 'p', "NUM", CFG_INT,
		 &cfg.unbind_req.host_phys_port_id, required_argument,
		 .require_in_usage = 1, .help = "Host physical port id"},
		{"log_port_id", 'l', "NUM", CFG_INT,
		 &cfg.unbind_req.host_log_port_id, required_argument,
		 .require_in_usage = 1,.help = "Host logical port id"},
		{NULL}};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));

	ret = switchtec_gfms_unbind(cfg.dev, &cfg.unbind_req);
	if (ret) {
		switchtec_perror("gfms_unbind");
		return ret;
	}

	return 0;
}

static int string_to_dword_data(char *str, unsigned int *dw_data, int *data_len)
{
	char *tmp;
	uint32_t num;
	char *p;
	uint32_t max_len;
	uint32_t raw_data_len = 0;

	max_len = *data_len;
	memset(dw_data, 0, max_len);

	p = strtok((char *)str, " ");
	while(p) {
		num = strtoul(p, &tmp, 0);

		if (*tmp != '\0')
			return -1;

		dw_data[raw_data_len] = num;

		raw_data_len++;
		if(raw_data_len >= max_len)
			return -1;

		p = strtok(NULL, " ");
	}

	*data_len = raw_data_len;
	return 0;
}

static int device_manage(int argc, char **argv)
{
	const char *desc = "Initiate device specific manage command";
	int ret;
	struct switchtec_device_manage_rsp rsp;
	char *cmd_string = NULL;
	int data_len;
	int i;

	static struct {
		struct switchtec_dev *dev;
		struct switchtec_device_manage_req req;
	} cfg = {
		.req.hdr.pdfid = 0xffff,
	};

	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"pdfid", 'f', "NUM", CFG_INT, &cfg.req.hdr.pdfid,
		 required_argument, "Endpoint function's FID",
		 .require_in_usage = 1},
		{"cmd_data", 'c', "String", CFG_STRING, &cmd_string,
		 required_argument, .require_in_usage = 1,
		 .help= "Command raw data in dword, "
		 "format example: \"0x040b0006 0x00000001\""},
		{NULL}};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));

	if (cmd_string == NULL) {
		argconfig_print_usage(opts);
		fprintf(stderr, "The --cmd_data|-c argument is required!\n");
		return 1;
	}
	if (cfg.req.hdr.pdfid == 0xffff) {
		argconfig_print_usage(opts);
		fprintf(stderr, "The --pdfid|-f argument is required!\n");
		return 1;
	}

	data_len = sizeof(cfg.req.cmd_data);
	string_to_dword_data(cmd_string,
			     (unsigned int *)cfg.req.cmd_data,
			     &data_len);
	cfg.req.hdr.expected_rsp_len = sizeof(rsp.rsp_data);

	ret = switchtec_device_manage(cfg.dev, &(cfg.req), &rsp);
	if (ret) {
		switchtec_perror("device_manage");
		return ret;
	}

	for(i = 0; i < rsp.hdr.rsp_len / 4; i++) {
		printf("0x%08x ", *((int *)rsp.rsp_data + i));
		if(i % 8 == 7)
			printf("\n");
	}
	printf("\n");

	return 0;
}

static int port_control(int argc, char **argv)
{
	const char *desc = "Initiate switchtec port control command";
	int ret;
	struct argconfig_choice control_type_choices[5] = {
		{"DISABLE", 0, "disable port"},
		{"ENABLE", 1, "enable port"},
		{"RETRAIN", 2, "link retrain"},
		{"HOT_RESET", 3, "link hot reset"},
		{}
	};
	struct argconfig_choice hot_reset_flag_choices[3] = {
		{"CLEAR", 0, "hot reset status clear"},
		{"SET", 1, "hot reset status set"},
		{}
	};

	static struct {
		struct switchtec_dev *dev;
		uint8_t control_type;
		uint8_t phys_port_id;
		uint8_t hot_reset_flag;
	} cfg;

	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"control_type", 't', "TYPE", CFG_MULT_CHOICES, &cfg.control_type, required_argument,
		.choices=control_type_choices,
		.require_in_usage = 1,
		.help="Port control type"},
		{"phys_port_id", 'p', "NUM", CFG_INT, &cfg.phys_port_id, required_argument,"Physical port ID",
		.require_in_usage = 1,},
		{"hot_reset_flag", 'f', "FLAG", CFG_MULT_CHOICES, &cfg.hot_reset_flag, required_argument,
		.choices=hot_reset_flag_choices,
		.require_in_usage = 1,
		.help="Hot reset flag option"},
		{NULL}};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));

	ret = switchtec_port_control(cfg.dev, cfg.control_type, cfg.phys_port_id, cfg.hot_reset_flag);
	if (ret) {
		switchtec_perror("port_control");
		return ret;
	}

	return 0;
}

static const char * const port_type_strs[] = {
	"Unused",
	"Fabric Link",
	"Fabric EP",
	"Fabric Host",
	"Invalid",
};

static const char * const clock_mode_strs[] = {
	"Common clock without SSC",
	"Non-common clock without SSC (SRNS)",
	"Common clock with SSC",
	"Non-common clock with SSC (SRIS)",
	"Invalid",
};

static int portcfg_show(int argc, char **argv)
{
	const char *desc = "Get the port config info";
	int ret;
	struct switchtec_fab_port_config port_info;
	int port_type, clock_mode;

	static struct {
		struct switchtec_dev *dev;
		int phys_port_id;
	} cfg = {
		.phys_port_id = -1,
	};

	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"phys_port_id", 'p', "NUM", CFG_NONNEGATIVE, &cfg.phys_port_id,
		 required_argument,"physical port id", .require_in_usage = 1},
		{NULL}};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));

	if (cfg.phys_port_id == -1) {
		argconfig_print_usage(opts);
		return 1;
	}

	ret = switchtec_fab_port_config_get(cfg.dev, cfg.phys_port_id, &port_info);
	if (ret) {
		switchtec_perror("port_info");
		return ret;
	}

	port_type = port_info.port_type;
	if(port_type >= SWITCHTEC_FAB_PORT_TYPE_INVALID)
		port_type = SWITCHTEC_FAB_PORT_TYPE_INVALID;

	printf("Port Type:    %s \n", port_type_strs[port_type]);
	printf("Clock Source: %d\n", port_info.clock_source);

	clock_mode = port_info.clock_mode;
	if(clock_mode >= SWITCHTEC_FAB_PORT_CLOCK_INVALID)
		clock_mode = SWITCHTEC_FAB_PORT_CLOCK_INVALID;

	printf("Clock Mode:   %s\n", clock_mode_strs[clock_mode]);
	printf("Hvd Instance: %d\n", port_info.hvd_inst);

	return 0;
}

static const struct cmd commands[] = {
	{"gfms_bind", gfms_bind, "Bind the EP(function) to the specified host"},
	{"gfms_unbind", gfms_unbind, "Unbind the EP(function) from the specified host"},
	{"device_manage", device_manage, "Initiate device specific manage command"},
	{"port_control", port_control, "Initiate port control command"},
	{"portcfg_show", portcfg_show, "Get the port config info"},
	{}
};

static struct subcommand subcmd = {
	.name = "fabric",
	.cmds = commands,
	.desc = "Switchtec Fabric Management (PAX only)",
	.long_desc = "",
};

REGISTER_SUBCMD(subcmd);
