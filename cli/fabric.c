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
		{"disable", 0, "disable port"},
		{"enable", 1, "enable port"},
		{"retrain", 2, "link retrain"},
		{"hot_reset", 3, "link hot reset"},
		{0}
	};
	struct argconfig_choice hot_reset_flag_choices[3] = {
		{"status_clear", 0, "hot reset status clear"},
		{"status_set", 1, "hot reset status set"},
		{0}
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

static int portcfg_set(int argc, char **argv)
{
	const char *desc = "Set the port config";
	int ret;

	struct argconfig_choice port_type_choices[4] = {
		{"unused", 0,
		 port_type_strs[SWITCHTEC_FAB_PORT_TYPE_UNUSED]},
		{"fabric_ep", 2,
		 port_type_strs[SWITCHTEC_FAB_PORT_TYPE_FABRIC_EP]},
		{"fabric_host", 3,
		 port_type_strs[SWITCHTEC_FAB_PORT_TYPE_FABRIC_HOST]},
		{0}
	};
	struct argconfig_choice clock_mode_choices[5] = {
		{"common", 0,
		 clock_mode_strs[SWITCHTEC_FAB_PORT_CLOCK_COMMON_WO_SSC]},
		{"srns", 1,
		 clock_mode_strs[SWITCHTEC_FAB_PORT_CLOCK_NON_COMMON_WO_SSC]},
		{"common_ssc", 2,
		 clock_mode_strs[SWITCHTEC_FAB_PORT_CLOCK_COMMON_W_SSC]},
		{"sris", 3,
		 clock_mode_strs[SWITCHTEC_FAB_PORT_CLOCK_NON_COMMON_W_SSC]},
		{0}
	};

	static struct {
		struct switchtec_dev *dev;
		uint8_t phys_port_id;
		struct switchtec_fab_port_config port_cfg;
	} cfg;

	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"phys_port_id", 'p', "NUM", CFG_INT,
		 &cfg.phys_port_id, required_argument,
		 "physical port id", .require_in_usage = 1},
		{"port_type", 't', "TYPE", CFG_MULT_CHOICES,
		 &cfg.port_cfg.port_type, required_argument,
		.choices=port_type_choices, .require_in_usage = 1,
		.help="Port type"},
		{"clock_source", 'c', "NUM", CFG_INT,
		 &cfg.port_cfg.clock_source, required_argument,
		 "CSU channel index for port clock source",
		 .require_in_usage = 1},
		{"clock_mode", 'm', "TYPE", CFG_MULT_CHOICES,
		 &cfg.port_cfg.clock_mode, required_argument,
		 .choices=clock_mode_choices, .require_in_usage = 1,
		 .help="Clock mode"},
		{"hvd_id", 'd', "NUM", CFG_INT, &cfg.port_cfg.hvd_inst,
		 required_argument, "HVM domain index for USP",
		 .require_in_usage = 1},
		{NULL}
	};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));

	ret = switchtec_fab_port_config_set(cfg.dev, cfg.phys_port_id, &cfg.port_cfg);
	if (ret) {
		switchtec_perror("port_config");
		return ret;
	}

	return 0;
}

static int portcfg_show(int argc, char **argv)
{
	const char *desc = "Get the port config info";
	int ret;
	struct switchtec_fab_port_config port_info;
	int port_type, clock_mode;

	static struct {
		struct switchtec_dev *dev;
		uint8_t phys_port_id;
	} cfg ;

	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"phys_port_id", 'p', "NUM", CFG_INT, &cfg.phys_port_id,
		 required_argument,"physical port id", .require_in_usage = 1},
		{NULL}};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));

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

static const char * const fabric_port_link_rate[] = {
	"NONE",
	"2.5 GT/s",
	"5 GT/s",
	"8 GT/s",
	"16 GT/s",
	"Unknown",
};

static const char * const fabric_port_ltssm_major_state[] = {
	"DETECT",
	"POLLING",
	"CONFIG",
	"L0",
	"RECOVERY",
	"DISABLED",
	"LOOPBK",
	"HOTRST",
	"L0S",
	"L1",
	"L2",
	"INVALID",
};

static const char * const fabric_port_ltssm_minor_state[11][13] = {
	{
		"INACTIVE",
		"QUIET",
		"SPD_CHG0",
		"SPD_CHG1",
		"ACTIVE0",
		"ACTIVE1",
		"ACTIVE2",
		"P1_TO_P0",
		"P0_TO_P1_0",
		"P0_TO_P1_1",
		"P0_TO_P1_2",
		"INVALID",
		"INVALID"
	},
	{
		"INACTIVE",
		"ACTIVE_ENTRY",
		"ACTIVE",
		"CFG",
		"COMP",
		"COMP_ENTRY",
		"COMP_EIOS",
		"COMP_EIOS_ACK",
		"COMP_IDLE",
		"INVALID",
		"INVALID",
		"INVALID",
		"INVALID"
	},
	{
		"INACTIVE",
		"US_LW_START",
		"US_LW_ACCEPT",
		"US_LN_WAIT",
		"US_LN_ACCEPT",
		"DS_LW_START",
		"DS_LW_ACCEPT",
		"DS_LN_WAIT",
		"DS_LN_ACCEPT",
		"COMPLETE",
		"IDLE",
		"INVALID",
		"INVALID"
	},
	{
		"INACTIVE",
		"L0",
		"TX_EL_IDLE",
		"TX_IDLE_MIN",
		"INVALID",
		"INVALID",
		"INVALID",
		"INVALID",
		"INVALID",
		"INVALID",
		"INVALID",
		"INVALID",
		"INVALID"
	},
	{
		"INACTIVE",
		"RCVR_LOCK",
		"RCVR_CFG",
		"IDLE",
		"SPEED0",
		"SPEED1",
		"SPEED2",
		"SPEED3",
		"EQ_PH0",
		"EQ_PH1",
		"EQ_PH2",
		"EQ_PH3",
		"INVALID"
	},
	{
		"INACTIVE",
		"DISABLE0",
		"DISABLE1",
		"DISABLE2",
		"DISABLE3",
		"INVALID",
		"INVALID",
		"INVALID",
		"INVALID",
		"INVALID",
		"INVALID",
		"INVALID",
		"INVALID"
	},
	{
		"INACTIVE",
		"ENTRY",
		"ENTRY_EXIT",
		"EIOS",
		"EIOS_ACK",
		"IDLE",
		"ACTIVE",
		"EXIT0",
		"EXIT1",
		"INVALID",
		"INVALID",
		"INVALID",
		"INVALID"
	},
	{
		"INACTIVE",
		"HOT_RESET",
		"MASTER_UP",
		"MASTER_DOWN",
		"INVALID",
		"INVALID",
		"INVALID",
		"INVALID",
		"INVALID",
		"INVALID",
		"INVALID",
		"INVALID",
		"INVALID"
	},
	{
		"INACTIVE",
		"IDLE",
		"TO_L0",
		"FTS0",
		"FTS1",
		"INVALID",
		"INVALID",
		"INVALID",
		"INVALID",
		"INVALID",
		"INVALID",
		"INVALID",
		"INVALID"
	},
	{
		"INACTIVE",
		"IDLE",
		"SUBSTATE",
		"TO_L0",
		"INVALID",
		"INVALID",
		"INVALID",
		"INVALID",
		"INVALID",
		"INVALID",
		"INVALID",
		"INVALID",
		"INVALID"
	},
	{
		"INACTIVE",
		"IDLE",
		"TX_WAKE0",
		"TX_WAKE1",
		"EXIT",
		"SPEED",
		"INVALID",
		"INVALID",
		"INVALID",
		"INVALID",
		"INVALID",
		"INVALID",
		"INVALID"
	}
};

static int topo_info(int argc, char **argv)
{
	const char *desc = "Show topology info of the specific switch";
	struct switchtec_fab_topo_info topo_info;
	int i, j;
	int port_type, port_rate, ltssm_major, ltssm_minor;
	const char *ltssm_major_str;
	const char *ltssm_minor_str;
        int ret;

	static struct {
		struct switchtec_dev *dev;
	} cfg = {
	};

	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{NULL}};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));

	for(i = 0; i < SWITCHTEC_MAX_PORTS; i++) {
		topo_info.port_info_list[i].phys_port_id = 0xFF;
	}

	ret = switchtec_topo_info_dump(cfg.dev, &topo_info);
	if (ret) {
		switchtec_perror("topo_info_get");
		return ret;
	}

	printf("Bifurcation:\n");
	for(i = 0; i < 6; i++) {
		for (j = 0; j < 8; j++) {
			int bif = (topo_info.stack_bif[i] >> (j * 4)) & 0x0f;
			const char *bif_str;
			switch(bif) {
			case 0x1:
				bif_str = "x2";
				break;
			case 0x2:
				bif_str = "x4";
				break;
			case 0x4:
				bif_str = "x8";
				break;
			case 0x8:
				bif_str = "x16";
				break;
			case 0xf:
				bif_str = "x1";
				break;
			default:
				continue;
			}
			printf("    Physical Port %d (Stack %d Port %d): %s\n",
					i * 8 + j, i, j, bif_str);
		}
	}

	printf("\nRouting Table:\n");
	for(i = 0; i < 16; i++)
		if (topo_info.route_port[i] != 0xff)
			printf("    To Switch %d via Physical Port ID %d\n",
					i, topo_info.route_port[i]);

	printf("\nActive Physical Ports:\n");
	for(i = 0; i < SWITCHTEC_MAX_PORTS; i++) {
		if(topo_info.port_info_list[i].phys_port_id == 0xFF)
			break;

		port_type =   topo_info.port_info_list[i].port_type;
		if(port_type >= SWITCHTEC_FAB_PORT_TYPE_INVALID)
			port_type = SWITCHTEC_FAB_PORT_TYPE_INVALID;
		printf("    Physical Port ID %d (%s):\n",
				topo_info.port_info_list[i].phys_port_id,
				port_type_strs[port_type]);

		printf("        Cfg Width:			x%d\n",
				topo_info.port_info_list[i].port_cfg_width);
		printf("        Neg Width:			x%d\n",
				topo_info.port_info_list[i].port_neg_width);

		port_rate= topo_info.port_info_list[i].port_cfg_rate;
		if(port_rate >= SWITCHTEC_FAB_PORT_LINK_RATE_INVALID)
			port_rate = SWITCHTEC_FAB_PORT_LINK_RATE_INVALID;

		printf("        Cfg Rate:			%s\n",
				fabric_port_link_rate[port_rate]);

		port_rate= topo_info.port_info_list[i].port_neg_rate;

		if(port_rate >= SWITCHTEC_FAB_PORT_LINK_RATE_INVALID)
			port_rate = SWITCHTEC_FAB_PORT_LINK_RATE_INVALID;

		printf("        Neg Rate:			%s\n",
				fabric_port_link_rate[port_rate]);

		ltssm_major = topo_info.port_info_list[i].port_major_ltssm;
		ltssm_minor = topo_info.port_info_list[i].port_minor_ltssm;

		if(ltssm_major >= SWITCHTEC_FAB_PORT_LTSSM_MAJOR_STATE_INVALID)
			ltssm_major =
				SWITCHTEC_FAB_PORT_LTSSM_MAJOR_STATE_INVALID;
		if(ltssm_minor >= SWITCHTEC_FAB_PORT_LTSSM_MINOR_STATE_MAX)
			ltssm_minor =
				SWITCHTEC_FAB_PORT_LTSSM_MINOR_STATE_MAX + 1;

		ltssm_major_str = fabric_port_ltssm_major_state[ltssm_major];
		ltssm_minor_str =
			fabric_port_ltssm_minor_state[ltssm_major][ltssm_minor];
		printf("        LTSSM:			        %s (%s)\n",
				ltssm_major_str, ltssm_minor_str);
		printf("        Clock Channel:			%d\n",
				topo_info.port_info_list[i].port_clock_channel);
		printf("        Connector Index:		%d\n",
				topo_info.port_info_list[i].port_connector_id);
		if (topo_info.port_info_list[i].conn_sig_pwrctrl.gpio_idx == 0xffff)
			printf("        Power Controller GPIO:		Unused\n");
		else
			printf("        Power Controller GPIO:		Index: 0x%04x, Value: 0x%02x\n",
					topo_info.port_info_list[i].conn_sig_pwrctrl.gpio_idx,
					topo_info.port_info_list[i].conn_sig_pwrctrl.value);
		if (topo_info.port_info_list[i].conn_sig_dsp_perst.gpio_idx == 0xffff)
			printf("        DSP PERST GPIO:			Unused\n");
		else
			printf("        DSP PERST GPIO:			Index: 0x%04x, Value: 0x%02x\n",
					topo_info.port_info_list[i].conn_sig_dsp_perst.gpio_idx,
					topo_info.port_info_list[i].conn_sig_dsp_perst.value);
		if (topo_info.port_info_list[i].conn_sig_usp_perst.gpio_idx == 0xffff)
			printf("        USP PERST GPIO:			Unused\n");
		else
			printf("        USP PERST GPIO:			Index: 0x%04x, Value: 0x%02x\n",
					topo_info.port_info_list[i].conn_sig_usp_perst.gpio_idx,
					topo_info.port_info_list[i].conn_sig_usp_perst.value);
		if (topo_info.port_info_list[i].conn_sig_presence.gpio_idx == 0xffff)
			printf("        PRESENCE GPIO:			Unused\n");
		else
			printf("        PRESENCE GPIO:			Index: 0x%04x, Value: 0x%02x\n",
					topo_info.port_info_list[i].conn_sig_presence.gpio_idx,
					topo_info.port_info_list[i].conn_sig_presence.value);
		if (topo_info.port_info_list[i].conn_sig_8639.gpio_idx == 0xffff)
			printf("        SFF8639 IFDET GPIO:		Unused\n");
		else
			printf("        SFF8639 IFDET GPIO:		Index: 0x%04x, Value: 0x%02x\n",
					topo_info.port_info_list[i].conn_sig_8639.gpio_idx,
					topo_info.port_info_list[i].conn_sig_8639.value);
	}

	return 0;
}

static const struct cmd commands[] = {
	{"topo_info", topo_info, "Show topology info of the specific switch"},
	{"gfms_bind", gfms_bind, "Bind the EP(function) to the specified host"},
	{"gfms_unbind", gfms_unbind, "Unbind the EP(function) from the specified host"},
	{"device_manage", device_manage, "Initiate device specific manage command"},
	{"port_control", port_control, "Initiate port control command"},
	{"portcfg_show", portcfg_show, "Get the port config info"},
	{"portcfg_set", portcfg_set, "Set the port config"},
	{}
};

static struct subcommand subcmd = {
	.name = "fabric",
	.cmds = commands,
	.desc = "Switchtec Fabric Management (PAX only)",
	.long_desc = "",
};

REGISTER_SUBCMD(subcmd);
