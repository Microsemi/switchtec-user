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

int section_hdr_print(struct switchtec_dev *dev,
		      struct switchtec_gfms_db_dump_section_hdr *hdr)
{
	printf("PAX ID: %d (SWFID: 0x%04hx)\n",
		hdr->pax_idx, hdr->swfid);

	return 0;
}

int fabric_general_print(
		struct switchtec_dev *dev,
		struct switchtec_gfms_db_fabric_general *fabric_general)
{
	int i;
	uint8_t r_type;
	char * r_type_str = NULL;
	struct switchtec_gfms_db_fabric_general *fg;

	section_hdr_print(dev, &fabric_general->hdr);

	fg = fabric_general;

	for (i = 0; i < SWITCHTEC_FABRIC_MAX_SWITCH_NUM; i++) {
		if (fg->hdr.pax_idx == i)
			continue;

		r_type = fg->body.pax_idx[i].reachable_type;
		if (r_type == SWITCHTEC_GFMS_DB_REACH_UC)
			r_type_str = "Unicast";
		else if (r_type == SWITCHTEC_GFMS_DB_REACH_BC)
			r_type_str = "Broadcast";
		else
			r_type_str = NULL;

		if (r_type_str)
			printf("    To PAX_IDX %d: %s\n", i, r_type_str);
	}

	return 0;
}

int pax_general_body_print(struct switchtec_dev *dev,
			   struct switchtec_gfms_db_pax_general_body *body)
{
	uint64_t rc_port_map;
	uint64_t ep_port_map;
	uint64_t fab_port_map;
	uint64_t free_port_map;

	rc_port_map = body->rc_port_map_high;
	rc_port_map = rc_port_map << 32 | body->rc_port_map_low;
	ep_port_map = body->ep_port_map_high;
	ep_port_map = ep_port_map << 32 | body->ep_port_map_low;
	fab_port_map = body->fab_port_map_high;
	fab_port_map = fab_port_map << 32 | body->fab_port_map_low;
	free_port_map = body->free_port_map_high;
	free_port_map = free_port_map << 32 | body->free_port_map_low;

	printf("    Physical Port Count:\t%hhu\n", body->phy_port_count);
	printf("    HVD Count:          \t%hhd\n", body->hvd_count);
	printf("    EP Count:           \t%hd\n", body->ep_count);
	printf("    FID range:          \t0x%04hx - 0x%04hx\n",
	       body->fid_start, body->fid_end);
	printf("    HFID range:         \t0x%04hx - 0x%04hx\n",
	       body->hfid_start, body->hfid_end);
	printf("    VDFID range:        \t0x%04hx - 0x%04hx\n",
	       body->vdfid_start, body->vdfid_end);
	printf("    PDFID range:        \t0x%04hx - 0x%04hx\n",
	       body->pdfid_start, body->pdfid_end);
	printf("    RC Port Map:        \t0x%016lx\n", rc_port_map);
	printf("    EP Port Map:        \t0x%016lx\n", ep_port_map);
	printf("    Fabric Port Map:    \t0x%016lx\n", fab_port_map);
	printf("    Free Port Map:      \t0x%016lx\n", free_port_map);
	printf("\n");

	return 0;
}

int pax_general_print(struct switchtec_dev *dev,
		      struct switchtec_gfms_db_pax_general *pax_general)
{
	section_hdr_print(dev, &pax_general->hdr);
	pax_general_body_print(dev, &pax_general->body);

	return 0;
}

int hvd_body_print(struct switchtec_dev *dev,
		   struct switchtec_gfms_db_hvd_body *body)
{
	int i;
	int log_port_count;

	printf("    HVD %hhx (Physical Port ID: %hhu, HFID: 0x%04hx):\n",
	       body->hvd_inst_id, body->phy_pid,
	       body->hfid);
	log_port_count = body->logical_port_count;

	for (i = 0; i < log_port_count; i++) {
		if (body->bound[i].bound)
			printf("        Logical Port ID %hhd:    \tBound to PDFID 0x%04hx\n",
			       body->bound[i].log_pid,
			       body->bound[i].bound_pdfid);
		else
			printf("        Logical Port ID %hhd:    \tUnbound\n",
			       body->bound[i].log_pid);
	}

	return 0;
}

int bdf_to_str(uint16_t bdf, char *str)
{
	sprintf(str, "%02x:%02x.%x",
		     (bdf & 0xff00) >> 8,
		     (bdf & 0x00f8) >> 3,
		     (bdf & 0x0007));

	return 0;
}

int vep_type_to_str(uint8_t type, char *str)
{
	switch (type) {
		case SWITCHTEC_GFMS_DB_VEP_TYPE_MGMT:
			sprintf(str, "EP_MGMT");
			break;
		default:
			sprintf(str, "Unknown(%hhu)", type);
			return -1;
	}

	return 0;
}

int hvd_detail_body_print(struct switchtec_dev *dev,
			  struct switchtec_gfms_db_hvd_detail_body *body)
{
	int i;
	int vep_count;
	int log_port_count;
	char bdf_str1[32];
	char bdf_str2[32];
	char vep_type_str[32];
	uint64_t enable_bitmap;
	uint64_t bitmap;
	int pos;

	bdf_to_str(body->usp_bdf, bdf_str1);
	printf("    HVD %hhx:\n"
	       "        Physical Port ID:  \t\t%hhu\n"
	       "        HFID:              \t\t0x%04hx\n"
	       "        USP Status:        \t\t%s\n"
	       "        USP BDF:           \t\t%s\n",
	       body->hvd_inst_id, body->phy_pid,
	       body->hfid, body->usp_status ? "LINK UP": "LINK DOWN",
	       body->usp_status ? bdf_str1 : "N/A");

	vep_count = body->vep_count;
	log_port_count = body->log_dsp_count;

	printf("        VEPs (%hhu):\n", body->vep_count);
	for (i = 0; i < vep_count; i++) {
		vep_type_to_str(body->vep_region[i].type, vep_type_str);
		bdf_to_str(body->vep_region[i].bdf, bdf_str1);
		printf("            VEP %hhu:\n"
		       "                Type:\t\t\t%s\n"
		       "                BDF: \t\t\t%s\n",
		       i, vep_type_str, body->usp_status ? bdf_str1 : "N/A");
	}

	printf("        Logical Ports (%hhu):\n", body->log_dsp_count);
	for (i = 0; i < log_port_count; i++) {
		if (body->log_port_region[i].bound) {
			bdf_to_str(body->log_port_region[i].dsp_bdf, bdf_str1);
			bdf_to_str(body->log_port_region[i].bound_hvd_bdf,
				   bdf_str2);
			printf("            Logical PID %hhu:\t\tBound to PDFID 0x%04hx (DSP BDF: %s, EP BDF: %s)\n",
			       body->log_port_region[i].log_pid,
			       body->log_port_region[i].bound_pdfid,
			       body->usp_status ? bdf_str1 : "N/A",
			       body->usp_status ? bdf_str2 : "N/A");
		} else
			printf("            Logical PID %hhu:\t\tUnbound\n",
			       body->log_port_region[i].log_pid);
	}

	enable_bitmap = body->log_port_p2p_enable_bitmap_high;
	enable_bitmap <<= 32;
	enable_bitmap |= body->log_port_p2p_enable_bitmap_low;

	printf("        Logical Port P2P enable bitmap:\t0x%016lx\n",
	       enable_bitmap);
	for (i = 0; i < body->log_port_count; i++) {
		pos = ffs(enable_bitmap);
		if (pos) {
			pos -= 1;
			enable_bitmap &= ~(1UL << pos);

			bitmap = body->log_port_p2p_bitmap[i].config_bitmap_high;
			bitmap <<= 32;
			bitmap |= body->log_port_p2p_bitmap[i].config_bitmap_low;
			printf("        Logical Port %hhu P2P config bitmap:    \t0x%016lx\n",
			       pos, bitmap);

			bitmap = body->log_port_p2p_bitmap[i].active_bitmap_high;
			bitmap <<= 32;
			bitmap |= body->log_port_p2p_bitmap[i].active_bitmap_low;
			printf("        Logical Port %hhu P2P active bitmap:    \t0x%016lx\n",
			       pos, bitmap);
		}
	}

	return 0;
}

int hvd_print(struct switchtec_dev *dev, struct switchtec_gfms_db_hvd* hvd)
{
	section_hdr_print(dev, &hvd->hdr);
	hvd_body_print(dev, &hvd->body);

	return 0;
}

int hvd_detail_print(struct switchtec_dev *dev,
		     struct switchtec_gfms_db_hvd_detail* hvd_detail)
{
	section_hdr_print(dev, &hvd_detail->hdr);
	hvd_detail_body_print(dev, &hvd_detail->body);

	return 0;
}

int fab_port_print(struct switchtec_dev *dev,
		   struct switchtec_gfms_db_fab_port *fab_port)
{
	section_hdr_print(dev, &fab_port->hdr);

	if (fab_port->body.attached_swfid == 0xff) {
		printf("    Physical Port ID %hhd (Not attached)\n",
		       fab_port->body.phy_pid);
		return 0;
	}

	printf("    Physical PID %hhd:\n",
	       fab_port->body.phy_pid);
	printf("        Attached Physical PID:\t%hhd\n",
	       fab_port->body.attached_phy_pid);
	printf("        Attached Switch Index:\t%hhd\n",
	       fab_port->body.attached_sw_idx);
	printf("        Attached SWFID:       \t0x%04hx\n",
	       fab_port->body.attached_swfid);
	printf("        Attached FW Version:  \t0x%hx\n",
	       fab_port->body.attached_fw_version);

	return 0;
}

int ep_port_func_type(uint8_t sriov_cap_pf, char *func_type, size_t len)
{
	if (sriov_cap_pf == 0x3) {
		strncpy(func_type, "SRIOV-PF", len);
		func_type[len - 1] = '\0';
	} else if(sriov_cap_pf == 0x1) {
		strncpy(func_type, "PF", len);
		func_type[len - 1] = '\0';
	} else if(sriov_cap_pf == 0x0) {
		strncpy(func_type, "SRIOV-VF", len);
		func_type[len - 1] = '\0';
	} else {
		strncpy(func_type, "Unknown", len);
		func_type[len - 1] = '\0';
	}

	return 0;
}

int ep_port_bar_type(uint8_t bar_type, char *bar_type_str, size_t len)
{
	if (bar_type == 0x4) {
		strncpy(bar_type_str, "Memory, Non-prefechable, 64-bit", len);
		bar_type_str[len - 1] = '\0';
	} else if (bar_type == 0xc)  {
		strncpy(bar_type_str, "Memory, Prefechable, 64-bit", len);
		bar_type_str[len - 1] = '\0';
	} else {
		strncpy(bar_type_str, "Unknown", len);
		bar_type_str[len - 1] = '\0';
	}

	return 0;
}

int exp2_to_string(int exp, char *str)
{
	char unit[7] = {'\0', 'K', 'M', 'G', 'T', 'P', '\0'};

	if (exp >= 50)
		return -1;

	sprintf(str, "%d%c", 1 << (exp % 10), unit[exp / 10]);

	return 0;
}

int ep_port_function_print(
		struct switchtec_gfms_db_ep_port_attached_device_function *func,
		char *lead)
{
	int i;
	char bar_type[64];
	char bar_size[10];
	char func_type[16];

	if (!lead)
		lead = "";
	ep_port_func_type(func->sriov_cap_pf, func_type, 16);

	printf("%s        Function %d (%s): \n",
	       lead, func->func_id, func_type);
	printf("%s            PDFID:      \t0x%02hx\n", lead, func->pdfid);
	printf("%s            VID-DID:    \t0x%04hx-0x%04hx\n", lead, func->vid, func->did);
	if (func->bound) {
		printf("%s            Binding:    \tBound\n", lead);
		printf("%s                Bound PAX ID          : %hhd\n",
		       lead, func->bound_pax_id);
		printf("%s                Bound HVD Physical PID: %hhd\n",
		       lead, func->bound_hvd_phy_pid);
		printf("%s                Bound HVD Logical PID : %hhd\n",
		       lead, func->bound_hvd_log_pid);
	} else
		printf("%s            Binding:    \tUnbound\n", lead);

	for (i = 0; i < 6; i++) {
		if (func->bars[i].size) {
			ep_port_bar_type(func->bars[i].type, bar_type, 64);

			if (exp2_to_string(func->bars[i].size, bar_size))
				sprintf(bar_size, "Invalid");

			printf("%s            BAR[%d]:     \t%s Bytes (%s)\n",
			       lead, i, bar_size, bar_type);
		}
	}

	return 0;
}

int ep_port_print(struct switchtec_dev *dev,
		  struct switchtec_gfms_db_ep_port *ep_port)
{
	int i, j;
	char bar_type[64];
	char bar_size[10];

	struct switchtec_gfms_db_ep_port_attached_device_function
		*device_function;
	struct switchtec_gfms_db_ep_port_attached_ds_function
		*switch_function;

	if (ep_port->port_hdr.type == SWITCHTEC_GFMS_DB_TYPE_NON) {
		printf("    Physical Port ID %d (Not attached)\n",
		       ep_port->port_hdr.phy_pid);
	} else if (ep_port->port_hdr.type == SWITCHTEC_GFMS_DB_TYPE_EP) {
		printf("    Physical Port ID %d (EP attached):\n",
		       ep_port->port_hdr.phy_pid);

		for (i = 0; i < ep_port->ep_ep.ep_hdr.function_number; i++) {
			device_function = &ep_port->ep_ep.functions[i];
			ep_port_function_print(device_function, NULL);
		}
	} else if (ep_port->port_hdr.type == SWITCHTEC_GFMS_DB_TYPE_SWITCH) {
		printf("    Physical Port ID %d (Switch attached):\n",
		       ep_port->port_hdr.phy_pid);

		printf("        Switch Functions:\n");
		for (i = 0; i < ep_port->ep_switch.sw_hdr.function_number; i++) {
			switch_function =
				&ep_port->ep_switch.ds_switch.internal_functions[i];
			printf("            Function %d:\n"
			       "                ENUM_ID:      \t0x%04hx\n"
			       "                VID-DID:      \t0x%04hx-0x%04hx\n"
			       "                Class Code:   \t0x%06x\n",
			       switch_function->func_id, switch_function->enumid,
			       switch_function->vid, switch_function->did,
			       switch_function->device_class);

			for (j = 0; j < 6; j++)
				if (switch_function->bar[j].size) {
					ep_port_bar_type(
						switch_function->bar[j].type,
						bar_type, 64);

					if (exp2_to_string(
						switch_function->bar[j].size,
						bar_size))
						sprintf(bar_size, "Invalid");

					printf("                BAR[%d]:\t\t%s Bytes (%s)\n",
					       j, bar_size, bar_type);
				}
		}
		printf("        Switch attached EPs:\n");
		for (i = 0; i < ep_port->port_hdr.ep_count; i++) {
			printf("            Physical Port ID %d (DSP P2P ENUMID 0x%04x):\n",
			       ep_port->port_hdr.phy_pid,
			       ep_port->ep_switch.switch_eps[i].ep_hdr.attached_dsp_enumid);

			for (j = 0; j < ep_port->ep_switch.switch_eps[i].ep_hdr.function_number; j++) {
				device_function = &ep_port->ep_switch.switch_eps[i].functions[j];
				ep_port_function_print(device_function, "        ");
			}
		}
	}

	return 0;
}

int pax_all_print(struct switchtec_dev *dev,
		  struct switchtec_gfms_db_pax_all *pax_all)
{
	int i;

	section_hdr_print(dev, &pax_all->pax_general.hdr);

	printf("General:\n");
	pax_general_body_print(dev, &pax_all->pax_general.body);


	printf("Fabric EPs:\n");
	for (i = 0; i < pax_all->ep_port_all.ep_port_count; i++) {
		ep_port_print(dev, &pax_all->ep_port_all.ep_ports[i]);
	}

	printf("HVDs:\n");
	for (i = 0; i < pax_all->hvd_all.hvd_count; i++) {
		hvd_body_print(dev, &pax_all->hvd_all.bodies[i]);
	}

	return 0;
}

enum switchtec_gfms_db_dump_type {
	SWITCHTEC_GFMS_FABRIC = 0,
	SWITCHTEC_GFMS_PAX_ALL,
	SWITCHTEC_GFMS_PAX,
	SWITCHTEC_GFMS_HVD,
	SWITCHTEC_GFMS_FAB_PORT,
	SWITCHTEC_GFMS_EP_PORT,
	SWITCHTEC_GFMS_HVD_DETAIL,
};

static int gfms_dump(int argc, char **argv)
{
	const char *desc = "PAX only, dump the GFMS database";

	int ret = 0;

	struct switchtec_gfms_db_fabric_general fabric_general;
	struct switchtec_gfms_db_ep_port_section ep_port_section;
	struct switchtec_gfms_db_fab_port fab_port;
	struct switchtec_gfms_db_pax_general pax_general;
	struct switchtec_gfms_db_pax_all pax_all;
	struct switchtec_gfms_db_hvd hvd;
	struct switchtec_gfms_db_hvd_detail hvd_detail;

	const struct argconfig_choice types[] = {
		{"FABRIC", SWITCHTEC_GFMS_FABRIC,
		 "Dump the fabric general information"},
		{"PAX_ALL", SWITCHTEC_GFMS_PAX_ALL,
		 "Dump all topology information of one PAX"},
		{"PAX", SWITCHTEC_GFMS_PAX,
		 "Dump specific PAX's general information"},
		{"HVD", SWITCHTEC_GFMS_HVD,
		 "Dump specific HVD's information"},
		{"FAB_PORT", SWITCHTEC_GFMS_FAB_PORT,
		 "Dump specific Fabric port's information"},
		{"EP_PORT", SWITCHTEC_GFMS_EP_PORT,
		 "Dump specific ep port's information"},
		{"HVD_DETAIL", SWITCHTEC_GFMS_HVD_DETAIL,
		 "Dump specific HVD's detail information"},
		{0}
	};

	static struct {
		struct switchtec_dev *dev;
		unsigned type;
		signed hvd_idx;
		signed fab_pid;
		signed ep_pid;
	} cfg = {
		.type = SWITCHTEC_GFMS_PAX_ALL,
		.hvd_idx = -1,
		.fab_pid = -1,
		.ep_pid = -1,
	};

	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"type", 't', "TYPE", CFG_CHOICES, &cfg.type,
		  required_argument, "GFMS type to dump", .choices=types},
		{"hvd_id", 'd', "ID", CFG_INT, &cfg.hvd_idx,
		  required_argument, .help="HVM domain index for USP"},
		{"fab_pid", 'f', "PID", CFG_INT, &cfg.fab_pid,
		  required_argument, .help="Fabric port id"},
		{"ep_pid", 'e', "PID", CFG_INT, &cfg.ep_pid,
		  required_argument, .help="EP port id"},
		{NULL}
	};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));
	switch (cfg.type) {
		case SWITCHTEC_GFMS_FABRIC:
			ret = switchtec_fab_gfms_db_dump_fabric_general(
					cfg.dev, &fabric_general);
			if (ret) {
				switchtec_perror("gfms_db_dump");
				return -1;
			}
			fabric_general_print(cfg.dev, &fabric_general);
			break;
		case SWITCHTEC_GFMS_PAX_ALL:
			ret = switchtec_fab_gfms_db_dump_pax_all(cfg.dev,
								 &pax_all);
			if (ret) {
				switchtec_perror("gfms_db_dump");
				return -1;
			}
			pax_all_print(cfg.dev, &pax_all);
			break;
		case SWITCHTEC_GFMS_PAX:
			ret = switchtec_fab_gfms_db_dump_pax_general(
					cfg.dev,
					&pax_general);
			if (ret) {
				switchtec_perror("gfms_db_dump");
				return -1;
			}
			pax_general_print(cfg.dev, &pax_general);
			break;
		case SWITCHTEC_GFMS_HVD:
			if (cfg.hvd_idx < 0) {
				argconfig_print_usage(opts);
				fprintf(stderr, "The --hvd_id|-d argument is required for -t HVD!\n");
				return 1;
			}
			ret = switchtec_fab_gfms_db_dump_hvd(cfg.dev,
							     cfg.hvd_idx,
							     &hvd);
			if (ret) {
				switchtec_perror("gfms_db_dump");
				return -1;
			}
			hvd_print(cfg.dev, &hvd);
			break;
		case SWITCHTEC_GFMS_FAB_PORT:
			if (cfg.fab_pid < 0) {
				argconfig_print_usage(opts);
				fprintf(stderr, "The --fab_pid|-f argument is required for -t FAB_PORT!\n");
				return 1;
			}
			ret = switchtec_fab_gfms_db_dump_fab_port(cfg.dev,
								  cfg.fab_pid,
								  &fab_port);
			if (ret) {
				switchtec_perror("gfms_db_dump");
				return -1;
			}
			fab_port_print(cfg.dev, &fab_port);
			break;
		case SWITCHTEC_GFMS_EP_PORT:
			if (cfg.ep_pid < 0) {
				argconfig_print_usage(opts);
				fprintf(stderr, "The --ep_pid|-e argument is required for -t EP_PORT!\n");
				return 1;
			}
			ret = switchtec_fab_gfms_db_dump_ep_port(
					cfg.dev, cfg.ep_pid, &ep_port_section);
			if (ret) {
				switchtec_perror("gfms_db_dump");
				return -1;
			}
			section_hdr_print(cfg.dev, &ep_port_section.hdr);
			ep_port_print(cfg.dev, &ep_port_section.ep_port);
			break;
		case SWITCHTEC_GFMS_HVD_DETAIL:
			if (cfg.hvd_idx < 0) {
				argconfig_print_usage(opts);
				fprintf(stderr, "The --hvd_id|-d argument is required for -t HVD!\n");
				return 1;
			}
			ret = switchtec_fab_gfms_db_dump_hvd_detail(
					cfg.dev, cfg.hvd_idx, &hvd_detail);
			if (ret) {
				switchtec_perror("gfms_db_dump");
				return -1;
			}
			hvd_detail_print(cfg.dev, &hvd_detail);
			break;
		default:
			fprintf(stderr, "Invalid type\n");
			return -1;
	}

	return ret;
}

static const struct cmd commands[] = {
	{"topo_info", topo_info, "Show topology info of the specific switch"},
	{"gfms_bind", gfms_bind, "Bind the EP(function) to the specified host"},
	{"gfms_unbind", gfms_unbind, "Unbind the EP(function) from the specified host"},
	{"gfms_dump", gfms_dump, "PAX only, dump the GFMS database"},
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
