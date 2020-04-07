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
#include <switchtec/utils.h>

#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <errno.h>
#include <ctype.h>

#define CMD_DESC_GFMS_BIND "bind the EP (function) to the specified host"

static int gfms_bind(int argc, char **argv)
{
	int ret;
	int count;
	char pdfid[9][16];
	int i;
	char *endptr;

	static struct {
		struct switchtec_dev *dev;
		struct switchtec_gfms_bind_req bind_req;
		char *pdfid_str;
	} cfg ;

	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"host_sw_idx", 's', "NUM", CFG_INT, &cfg.bind_req.host_sw_idx,
		 required_argument,"host switch index", .require_in_usage = 1},
		{"phys_port_id", 'p', "NUM", CFG_INT,
		 &cfg.bind_req.host_phys_port_id,
		 required_argument,"host physical port ID",
		 .require_in_usage = 1},
		{"log_port_id", 'l', "NUM", CFG_INT,
		 &cfg.bind_req.host_log_port_id, required_argument,
		 "host logical port ID", .require_in_usage = 1},
		{"pdfid", 'f', "STR", CFG_STRING, &cfg.pdfid_str,
		 required_argument,"EP function's PDFID",
		 .require_in_usage = 1},
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_GFMS_BIND, opts, &cfg, sizeof(cfg));

	count = sscanf(cfg.pdfid_str, "%[^','], %[^','], %[^','], %[^','],"
		     "%[^','], %[^','], %[^','], %[^','], %[^'.']",
		     pdfid[0], pdfid[1], pdfid[2], pdfid[3], pdfid[4],
		     pdfid[5], pdfid[6], pdfid[7], pdfid[8]);

	if (count == EOF)  {
		fprintf(stderr, "Must specify pdfid.\n");
		return -1;
	}

	if (count > SWITCHTEC_FABRIC_MULTI_FUNC_NUM) {
		fprintf(stderr, "Too many pdfids specified (Max: %d).\n",
			SWITCHTEC_FABRIC_MULTI_FUNC_NUM);
		return -2;
	}

	cfg.bind_req.ep_number = 0;
	for (i = 0; i < count; i++) {
		unsigned long value = strtoul(pdfid[i], &endptr, 0);
		if (errno || value >= 0xffff  || optarg == endptr) {
			fprintf(stderr, "Invalid pdfid %s.\n", pdfid[i]);
			return -3;
		}
		cfg.bind_req.ep_pdfid[i] = value;
		cfg.bind_req.ep_number++;
	}

	ret = switchtec_gfms_bind(cfg.dev, &cfg.bind_req);
	if (ret) {
		switchtec_perror("gfms_bind");
		return ret;
	}

	return 0;
}

#define CMD_DESC_GFMS_UNBIND "unbind the EP (function) from the specified host"

static int gfms_unbind(int argc, char **argv)
{
	int ret;

	static struct {
		struct switchtec_dev *dev;
		struct switchtec_gfms_unbind_req unbind_req;
	} cfg;

	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"host_sw_idx", 's', "NUM", CFG_INT,
		 &cfg.unbind_req.host_sw_idx, required_argument,
		 "host switch index", .require_in_usage = 1,},
		{"phys_port_id", 'p', "NUM", CFG_INT,
		 &cfg.unbind_req.host_phys_port_id, required_argument,
		 .require_in_usage = 1, .help = "host physical port ID"},
		{"log_port_id", 'l', "NUM", CFG_INT,
		 &cfg.unbind_req.host_log_port_id, required_argument,
		 .require_in_usage = 1,.help = "host logical port ID"},
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_GFMS_UNBIND, opts, &cfg, sizeof(cfg));

	ret = switchtec_gfms_unbind(cfg.dev, &cfg.unbind_req);
	if (ret) {
		switchtec_perror("gfms_unbind");
		return ret;
	}

	return 0;
}

#define CMD_DESC_PORT_CONTROL "control a port"

static int port_control(int argc, char **argv)
{
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
		{"phys_port_id", 'p', "NUM", CFG_INT, &cfg.phys_port_id, required_argument,"physical port ID",
		.require_in_usage = 1,},
		{"hot_reset_flag", 'f', "FLAG", CFG_MULT_CHOICES, &cfg.hot_reset_flag, required_argument,
		.choices=hot_reset_flag_choices,
		.require_in_usage = 1,
		.help="hot reset flag option"},
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_PORT_CONTROL, opts, &cfg, sizeof(cfg));

	ret = switchtec_port_control(cfg.dev, cfg.control_type, cfg.phys_port_id, cfg.hot_reset_flag);
	if (ret) {
		switchtec_perror("port_control");
		return ret;
	}

	return 0;
}

static const char * const port_type_strs[] = {
	"unused",
	"fabric link",
	"fabric EP",
	"fabric host",
	"invalid",
};

static const char * const clock_sris_strs[] = {
	"disable",
	"enable",
	"invalid",
};

#define CMD_DESC_PORTCFG_SET "configure a port"

static int portcfg_set(int argc, char **argv)
{
	struct switchtec_fab_port_config port_cfg;
	int ret;

	struct argconfig_choice port_type_choices[4] = {
		{"UNUSED", SWITCHTEC_FAB_PORT_TYPE_UNUSED,
		 port_type_strs[SWITCHTEC_FAB_PORT_TYPE_UNUSED]},
		{"FABRIC_EP", SWITCHTEC_FAB_PORT_TYPE_FABRIC_EP,
		 port_type_strs[SWITCHTEC_FAB_PORT_TYPE_FABRIC_EP]},
		{"FABRIC_HOST", SWITCHTEC_FAB_PORT_TYPE_FABRIC_HOST,
		 port_type_strs[SWITCHTEC_FAB_PORT_TYPE_FABRIC_HOST]},
		{}
	};

	struct argconfig_choice clock_sris_choices[3] = {
		{"DISABLE", SWITCHTEC_FAB_PORT_CLOCK_SRIS_DISABLE,
		 clock_sris_strs[SWITCHTEC_FAB_PORT_CLOCK_SRIS_DISABLE]},
		{"ENABLE", SWITCHTEC_FAB_PORT_CLOCK_SRIS_ENABLE,
		 clock_sris_strs[SWITCHTEC_FAB_PORT_CLOCK_SRIS_ENABLE]},
		{}
	};

	static struct {
		struct switchtec_dev *dev;
		int phys_port_id;
		int port_type;
		int clock_source;
		int clock_sris;
		int hvd_inst;
	} cfg = {
		.phys_port_id = -1,
		.port_type = -1,
		.clock_source = -1,
		.clock_sris = -1,
		.hvd_inst = -1,
	};

	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"phys_port_id", 'p', "NUM", CFG_INT,
		 &cfg.phys_port_id, required_argument,
		 "physical port ID", .require_in_usage = 1},
		{"port_type", 't', "TYPE", CFG_CHOICES,
		 &cfg.port_type, required_argument,
		.choices=port_type_choices, .require_in_usage = 1,
		.help="port type"},
		{"clock_source", 'c', "NUM", CFG_INT,
		 &cfg.clock_source, required_argument,
		 "CSU channel index for port clock source",
		 .require_in_usage = 1},
		{"clock_sris", 's', "SRIS", CFG_CHOICES,
		 &cfg.clock_sris, required_argument,
		 .choices=clock_sris_choices, .require_in_usage = 1,
		 .help="clock sris"},
		{"hvd_id", 'd', "NUM", CFG_INT, &cfg.hvd_inst,
		 required_argument, "HVM domain index for USP",
		 .require_in_usage = 1},
		{NULL}
	};

	argconfig_parse(argc, argv, CMD_DESC_PORTCFG_SET, opts, &cfg, sizeof(cfg));
	if (cfg.phys_port_id == -1) {
		fprintf(stderr, "The --phys_port_id|-p argument is required!\n");
		return 1;
	}

	if (cfg.port_type == -1) {
		fprintf(stderr, "The --port_type|-t argument is required!\n");
		return 1;
	}

	if (cfg.port_type != SWITCHTEC_FAB_PORT_TYPE_UNUSED &&
	    cfg.clock_source == -1) {
		fprintf(stderr, "The --clock_source|-c argument is required for port type FABRIC_EP and FABRIC_HOST!\n");
		return 2;
	}

	if (cfg.port_type != SWITCHTEC_FAB_PORT_TYPE_UNUSED &&
	    cfg.clock_sris == -1) {
		fprintf(stderr, "The --clock_sris|-s argument is required for port type FABRIC_EP and FABRIC_HOST!\n");
		return 3;
	}

	if (cfg.port_type == SWITCHTEC_FAB_PORT_TYPE_FABRIC_HOST &&
	    cfg.hvd_inst == -1) {
		fprintf(stderr, "The --hvd_id|-d argument is required for port type FABRIC_HOST!\n");
		return 4;
	}

	port_cfg.clock_source = cfg.clock_source;
	port_cfg.clock_sris = cfg.clock_sris;
	port_cfg.port_type = cfg.port_type;
	port_cfg.hvd_inst = cfg.hvd_inst;
	ret = switchtec_fab_port_config_set(cfg.dev, cfg.phys_port_id, &port_cfg);
	if (ret) {
		switchtec_perror("port_config");
		return ret;
	}

	return 0;
}

#define CMD_DESC_PORTCFG_SHOW "display a port's configuration information"

static int portcfg_show(int argc, char **argv)
{
	int ret;
	struct switchtec_fab_port_config port_info;
	int port_type, clock_sris;

	static struct {
		struct switchtec_dev *dev;
		int phys_port_id;
	} cfg = {
		.phys_port_id = -1,
	};

	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"phys_port_id", 'p', "NUM", CFG_NONNEGATIVE, &cfg.phys_port_id,
		 required_argument,"physical port ID", .require_in_usage = 1},
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_PORTCFG_SHOW, opts, &cfg, sizeof(cfg));

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

	clock_sris = port_info.clock_sris;
	if(clock_sris >= SWITCHTEC_FAB_PORT_CLOCK_SRIS_INVALID)
		clock_sris = SWITCHTEC_FAB_PORT_CLOCK_SRIS_INVALID;

	printf("Clock SRIS:   %s\n", clock_sris_strs[clock_sris]);
	printf("HVD Instance: %d\n", port_info.hvd_inst);

	return 0;
}

#define CMD_DESC_TOPO_INFO "show topology information"

static int topo_info(int argc, char **argv)
{
	struct switchtec_fab_topo_info topo_info;
	int i;
	int port_type, port_rate, ltssm;
	struct switchtec_fab_port_info *port_info;
        int ret;

	static struct {
		struct switchtec_dev *dev;
	} cfg = {
	};

	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_TOPO_INFO, opts, &cfg, sizeof(cfg));

	for(i = 0; i < SWITCHTEC_MAX_PORTS; i++)
		topo_info.port_info_list[i].phys_port_id = 0xff;

	ret = switchtec_topo_info_dump(cfg.dev, &topo_info);
	if (ret) {
		switchtec_perror("topo_info_get");
		return ret;
	}

	printf("Active Physical Ports:\n");
	for(i = 0; i < SWITCHTEC_MAX_PORTS; i++) {
		port_info = &topo_info.port_info_list[i];
		if(port_info->phys_port_id == 0xff)
			break;

		port_type = port_info->port_type;
		if(port_type >= SWITCHTEC_FAB_PORT_TYPE_INVALID)
			port_type = SWITCHTEC_FAB_PORT_TYPE_INVALID;

		printf("    Physical Port ID %d (%s):\n",
		       port_info->phys_port_id,
		       port_type_strs[port_type]);

		printf("        Stack-Port:			Stack %d, Port %d\n",
		       port_info->phys_port_id / 8,
		       port_info->phys_port_id % 8);

		printf("        Cfg Width (Bifurcation):	x%d\n",
		       port_info->port_cfg_width);
		printf("        Neg Width:			x%d\n",
		       port_info->port_neg_width);

		port_rate= port_info->port_cfg_rate;
		printf("        Cfg Rate:			%g GT/s\n",
		       switchtec_gen_transfers[port_rate]);

		port_rate= port_info->port_neg_rate;
		printf("        Neg Rate:			%g GT/s\n",
		       switchtec_gen_transfers[port_rate]);

		ltssm = port_info->port_minor_ltssm;
		ltssm <<= 8;
		ltssm |= port_info->port_major_ltssm;

		printf("        LTSSM:			        %s\n",
		       switchtec_ltssm_str(ltssm, 1));
		printf("        Clock Channel:			%d\n",
		       port_info->port_clock_channel);
		printf("        Connector Index:		%d\n",
		       port_info->port_connector_id);
		if (port_info->conn_sig_pwrctrl.gpio_idx == 0xffff)
			printf("        Power Controller GPIO:		Unused\n");
		else
			printf("        Power Controller GPIO:		Index: 0x%04x, Value: 0x%02x\n",
			       port_info->conn_sig_pwrctrl.gpio_idx,
			       port_info->conn_sig_pwrctrl.value);
		if (port_info->conn_sig_dsp_perst.gpio_idx == 0xffff)
			printf("        DSP PERST GPIO:			Unused\n");
		else
			printf("        DSP PERST GPIO:			Index: 0x%04x, Value: 0x%02x\n",
			       port_info->conn_sig_dsp_perst.gpio_idx,
			       port_info->conn_sig_dsp_perst.value);
		if (port_info->conn_sig_usp_perst.gpio_idx == 0xffff)
			printf("        USP PERST GPIO:			Unused\n");
		else
			printf("        USP PERST GPIO:			Index: 0x%04x, Value: 0x%02x\n",
			       port_info->conn_sig_usp_perst.gpio_idx,
			       port_info->conn_sig_usp_perst.value);
		if (port_info->conn_sig_presence.gpio_idx == 0xffff)
			printf("        PRESENCE GPIO:			Unused\n");
		else
			printf("        PRESENCE GPIO:			Index: 0x%04x, Value: 0x%02x\n",
			       port_info->conn_sig_presence.gpio_idx,
			       port_info->conn_sig_presence.value);
		if (port_info->conn_sig_8639.gpio_idx == 0xffff)
			printf("        SFF8639 IFDET GPIO:		Unused\n");
		else
			printf("        SFF8639 IFDET GPIO:		Index: 0x%04x, Value: 0x%02x\n",
			       port_info->conn_sig_8639.gpio_idx,
			       port_info->conn_sig_8639.value);
	}

	return 0;
}

static int section_hdr_print(struct switchtec_dev *dev,
			     struct switchtec_gfms_db_dump_section_hdr *hdr)
{
	printf("PAX ID: %d (SWFID: 0x%04hx)\n",
		hdr->pax_idx, hdr->swfid);

	return 0;
}

static int fabric_general_print(
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

static int pax_general_body_print(
		struct switchtec_dev *dev,
		struct switchtec_gfms_db_pax_general_body *body)
{
	unsigned long long rc_port_map;
	unsigned long long ep_port_map;
	unsigned long long fab_port_map;
	unsigned long long free_port_map;

	rc_port_map = body->rc_port_map_high;
	rc_port_map = rc_port_map << 32 | body->rc_port_map_low;
	ep_port_map = body->ep_port_map_high;
	ep_port_map = ep_port_map << 32 | body->ep_port_map_low;
	fab_port_map = body->fab_port_map_high;
	fab_port_map = fab_port_map << 32 | body->fab_port_map_low;
	free_port_map = body->free_port_map_high;
	free_port_map = free_port_map << 32 | body->free_port_map_low;

	printf("    Physical Port Count:\t%d\n", body->phy_port_count);
	printf("    HVD Count:          \t%d\n", body->hvd_count);
	printf("    EP Count:           \t%d\n", body->ep_count);
	printf("    FID range:          \t0x%04hx - 0x%04hx\n",
	       body->fid_start, body->fid_end);
	printf("    HFID range:         \t0x%04hx - 0x%04hx\n",
	       body->hfid_start, body->hfid_end);
	printf("    VDFID range:        \t0x%04hx - 0x%04hx\n",
	       body->vdfid_start, body->vdfid_end);
	printf("    PDFID range:        \t0x%04hx - 0x%04hx\n",
	       body->pdfid_start, body->pdfid_end);
	printf("    RC Port Map:        \t0x%016llx\n", rc_port_map);
	printf("    EP Port Map:        \t0x%016llx\n", ep_port_map);
	printf("    Fabric Port Map:    \t0x%016llx\n", fab_port_map);
	printf("    Free Port Map:      \t0x%016llx\n", free_port_map);
	printf("\n");

	return 0;
}

static int pax_general_print(struct switchtec_dev *dev,
			     struct switchtec_gfms_db_pax_general *pax_general)
{
	section_hdr_print(dev, &pax_general->hdr);
	pax_general_body_print(dev, &pax_general->body);

	return 0;
}

static int hvd_body_print(struct switchtec_dev *dev,
			  struct switchtec_gfms_db_hvd_body *body)
{
	int i, j;
	int index;
	int bound;
	int log_port_count;

	printf("    HVD %hhx (Physical Port ID: %hhu, HFID: 0x%04hx):\n",
	       body->hvd_inst_id, body->phy_pid,
	       body->hfid);
	log_port_count = body->logical_port_count;

	for (i = 0; i < log_port_count; i++) {
		bound = 0;
		for (j = 0; j < SWITCHTEC_FABRIC_MULTI_FUNC_NUM; j++) {
			index = j * log_port_count + i;
			if (body->bound[index].bound)
				bound = 1;
		}

		if (!bound) {
			printf("        Logical PID %hhu:\t\tUnbound\n", i);
			continue;
		}

		printf("        Logical PID %hhu:\n", i);
		for (j = 0; j < SWITCHTEC_FABRIC_MULTI_FUNC_NUM; j++) {
			index = j * log_port_count + i;

			if (body->bound[index].bound)
				printf("            Function %hhu:    \tPDFID 0x%04hx\n",
				       j, body->bound[index].bound_pdfid);
		}
	}

	return 0;
}

static int bdf_to_str(uint16_t bdf, char *str)
{
	sprintf(str, "%02x:%02x.%x",
		     (bdf & 0xff00) >> 8,
		     (bdf & 0x00f8) >> 3,
		     (bdf & 0x0007));

	return 0;
}

static int vep_type_to_str(uint8_t type, char *str)
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

static int hvd_detail_body_print(struct switchtec_dev *dev,
				 struct switchtec_gfms_db_hvd_detail_body *body)
{
	int i, j;
	int index;
	int vep_count;
	int log_port_count;
	struct switchtec_gfms_db_hvd_log_port *port;
	int bound;
	char bdf_str1[32];
	char bdf_str2[32];
	char vep_type_str[32];
	uint64_t enable_bitmap;
	uint64_t bitmap;
	int pos;

	bdf_to_str(body->usp_bdf, bdf_str1);
	printf("    HVD %hhx (Physical Port ID: %hhu, HFID: 0x%04hx):\n"
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
		bound = 0;
		for (j = 0; j < SWITCHTEC_FABRIC_MULTI_FUNC_NUM; j++) {
			index = j * log_port_count + i;
			port = &body->log_port_region[index];
			if (port->bound) {
				bound = 1;
			}
		}

		if (!bound) {
			printf("            Logical PID %hhu:\t\tUnbound\n", i);
			continue;
		}

		printf("            Logical PID %hhu:\n", i);
		for (j = 0; j < SWITCHTEC_FABRIC_MULTI_FUNC_NUM; j++) {
			index = j * log_port_count + i;
			port = &body->log_port_region[index];
			if (port->bound) {
				bdf_to_str(port->dsp_bdf, bdf_str1);
				bdf_to_str(port->bound_hvd_bdf, bdf_str2);
				printf("                Function %hhu:\t\tPDFID 0x%04hx (DSP BDF: %s, EP BDF: %s)\n",
				       j, port->bound_pdfid,
				       body->usp_status ? bdf_str1 : "N/A",
				       body->usp_status ? bdf_str2 : "N/A");
			}
		}
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
			printf("            Logical Port %hhu P2P config bitmap:    \t0x%016lx\n",
			       pos, bitmap);

			bitmap = body->log_port_p2p_bitmap[i].active_bitmap_high;
			bitmap <<= 32;
			bitmap |= body->log_port_p2p_bitmap[i].active_bitmap_low;
			printf("            Logical Port %hhu P2P active bitmap:    \t0x%016lx\n",
			       pos, bitmap);
		}
	}

	return 0;
}

static int hvd_print(struct switchtec_dev *dev,
		     struct switchtec_gfms_db_hvd* hvd)
{
	section_hdr_print(dev, &hvd->hdr);
	hvd_body_print(dev, &hvd->body);

	return 0;
}

static int hvd_detail_print(struct switchtec_dev *dev,
			    struct switchtec_gfms_db_hvd_detail* hvd_detail)
{
	section_hdr_print(dev, &hvd_detail->hdr);
	hvd_detail_body_print(dev, &hvd_detail->body);

	return 0;
}

static int fab_port_print(struct switchtec_dev *dev,
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

static int ep_port_func_type(uint8_t sriov_cap_pf, char *func_type, size_t len)
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

static int exp2_to_string(int exp, char *str)
{
	char unit[7] = {'\0', 'K', 'M', 'G', 'T', 'P', '\0'};

	if (exp >= 50)
		return -1;

	sprintf(str, "%d%c", 1 << (exp % 10), unit[exp / 10]);

	return 0;
}

static int ep_port_function_print(
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
		printf("%s            Binding:    \tPAX ID: %hhd, HVD Physical PID: %hhd, HVD Logical PID: %hhd\n", lead, func->bound_pax_id, func->bound_hvd_phy_pid, func->bound_hvd_log_pid);

	} else
		printf("%s            Binding:    \tUnbound\n", lead);

	for (i = 0; i < 6; i++) {
		if (func->bars[i].size) {
			switchtec_ep_port_bar_type_str(func->bars[i].type,
						       bar_type,
						       sizeof(bar_type));

			if (exp2_to_string(func->bars[i].size, bar_size))
				sprintf(bar_size, "Invalid");

			printf("%s            BAR[%d]:     \t%s Bytes (%s)\n",
			       lead, i, bar_size, bar_type);
		}
	}

	return 0;
}

static int ep_port_print(struct switchtec_dev *dev,
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
					switchtec_ep_port_bar_type_str(
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

static int pax_all_print(struct switchtec_dev *dev,
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
	printf("\n");

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

#define CMD_DESC_GFMS_DUMP "dump the GFMS database"

static int gfms_dump(int argc, char **argv)
{
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
		 "dump the fabric general information"},
		{"PAX_ALL", SWITCHTEC_GFMS_PAX_ALL,
		 "dump all topology information of one PAX"},
		{"PAX", SWITCHTEC_GFMS_PAX,
		 "dump specified PAX's general information"},
		{"HVD", SWITCHTEC_GFMS_HVD,
		 "dump specified HVD's information"},
		{"FAB_PORT", SWITCHTEC_GFMS_FAB_PORT,
		 "dump specified fabric port's information"},
		{"EP_PORT", SWITCHTEC_GFMS_EP_PORT,
		 "dump specified EP port's information"},
		{"HVD_DETAIL", SWITCHTEC_GFMS_HVD_DETAIL,
		 "dump specified HVD's detailed information"},
		{}
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
		  required_argument, .help="fabric port ID"},
		{"ep_pid", 'e', "PID", CFG_INT, &cfg.ep_pid,
		  required_argument, .help="EP port ID"},
		{NULL}
	};

	argconfig_parse(argc, argv, CMD_DESC_GFMS_DUMP, opts, &cfg, sizeof(cfg));
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

#define CMD_DESC_ROUTE "show routing information"

static int route(int argc, char **argv)
{
	struct switchtec_fab_topo_info topo_info;
	struct switchtec_gfms_db_fabric_general fg;
	uint8_t r_type;
	char * r_type_str = NULL;
	int i;
        int ret;

	static struct {
		struct switchtec_dev *dev;
	} cfg = {
	};

	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_ROUTE, opts, &cfg, sizeof(cfg));

	for(i = 0; i < SWITCHTEC_MAX_PORTS; i++) {
		topo_info.port_info_list[i].phys_port_id = 0xff;
	}

	ret = switchtec_topo_info_dump(cfg.dev, &topo_info);
	if (ret) {
		switchtec_perror("topo_info_dump");
		return ret;
	}

	ret = switchtec_fab_gfms_db_dump_fabric_general(cfg.dev, &fg);
	if (ret) {
		switchtec_perror("gfms_db_dump");
		return -1;
	}

	printf("%-15s\t%-15s\t%-15s\n", "Destination",
	       "Phys_port", "Reachable");
	for (i = 0; i < 16; i++) {
		if (fg.hdr.pax_idx == i)
			continue;

		if (topo_info.route_port[i] == 0xff)
			continue;

		r_type = fg.body.pax_idx[i].reachable_type;

		if (r_type == SWITCHTEC_GFMS_DB_REACH_UC)
			r_type_str = "Unicast";
		else if (r_type == SWITCHTEC_GFMS_DB_REACH_BC)
			r_type_str = "Broadcast";
		else if (r_type == SWITCHTEC_GFMS_DB_REACH_UR)
			r_type_str = "Unreachable";
		else
			r_type_str = "Unknown";

		printf("%-15d\t%-15d\t%s\n", i, topo_info.route_port[i],
		       r_type_str);
	}

	return 0;
}

static void print_gfms_event_host_link(struct switchtec_gfms_event *e)
{
	printf("        Physical Port ID:           \t%d\n",
	       e->data.host.phys_port_id);
}

static void print_gfms_event_bind(struct switchtec_gfms_event *e)
{
	struct switchtec_gfms_event_bind *bind = &e->data.bind;

	printf("        Host Switch Index:          \t%d\n", bind->host_sw_idx);
	printf("        Host Physical Port ID:      \t%d\n",
	       bind->host_phys_port_id);
	printf("        Logical Port ID:            \t%d\n", bind->log_port_id);
	printf("        PDFID:                      \t0x%04x,\n", bind->pdfid);
}

static void print_gfms_event_hvd(struct switchtec_gfms_event *e)
{
	struct switchtec_gfms_event_hvd *hvd = &e->data.hvd;

	printf("        HVD Instance ID:            \t%d\n", hvd->hvd_inst_id);
	printf("        Physical Port ID:           \t%d\n", hvd->phys_port_id);
	printf("        Clock Channel:              \t%d\n", hvd->clock_chan);
}

static void print_gfms_event_ep(struct switchtec_gfms_event *e)
{
	printf("        Physical Port ID:           \t%d\n",
	       e->data.ep.phys_port_id);
}

static void print_gfms_event_aer(struct switchtec_gfms_event *e)
{
	struct switchtec_gfms_event_aer *aer = &e->data.aer;

	printf("        Physical Port ID:           \t%d\n", aer->phys_port_id);
	printf("        DPC Triggered:              \t%s\n",
			switchtec_gfms_aer_dpc(aer) ? "Yes": "No");
	printf("        CE/UE:                      \t%s\n",
			switchtec_gfms_aer_ce_ue(aer) ? "CE": "UE");
	printf("        CE/UE Error Status:         \t0x%08x\n",
			aer->ce_ue_err_sts);
	printf("        Time Stamp (In Clock Ticks):\t0x%08x%08x\n",
			aer->aer_err_log_time_stamp_high,
			aer->aer_err_log_time_stamp_low);
	if (switchtec_gfms_aer_log(aer))
		printf("        AER TLP Header Log:         \t0x%08x\n"
		       "                                    \t0x%08x\n"
		       "                                    \t0x%08x\n"
		       "                                    \t0x%08x\n",
		       aer->aer_header_log[0],
		       aer->aer_header_log[1],
		       aer->aer_header_log[2],
		       aer->aer_header_log[3]);
	else
		printf("        AER TLP Header Log:         \tN/A\n");
}

static void print_gfms_event_list(struct switchtec_gfms_event *e, size_t cnt,
				  int overflow, int remain_num)
{
	int i = 0;

	printf("GFMS Events (%sRemaining: %d)\n",
	       overflow ? "Overflowed, ": "", remain_num);
	while (cnt--) {
		printf("    %d) ", ++i);
		switch (e->event_code) {
		case SWITCHTEC_GFMS_EVENT_HOST_LINK_UP:
			printf("HOST_LINK_UP (PAX ID %d):\n", e->src_sw_id);
			print_gfms_event_host_link(e);
			break;
		case SWITCHTEC_GFMS_EVENT_HOST_LINK_DOWN:
			printf("HOST_LINK_DOWN (PAX ID %d):\n", e->src_sw_id);
			print_gfms_event_host_link(e);
			break;
		case SWITCHTEC_GFMS_EVENT_DEV_ADD:
			printf("DEVICE_ADD (PAX ID %d)\n", e->src_sw_id);
			break;
		case SWITCHTEC_GFMS_EVENT_DEV_DEL:
			printf("DEVICE_DELETE (PAX ID %d)\n", e->src_sw_id);
			break;
		case SWITCHTEC_GFMS_EVENT_FAB_LINK_UP:
			printf("FABRIC_LINK_UP (PAX ID %d)\n", e->src_sw_id);
			break;
		case SWITCHTEC_GFMS_EVENT_FAB_LINK_DOWN:
			printf("FABRIC_LINK_DOWN (PAX ID %d)\n", e->src_sw_id);
			break;
		case SWITCHTEC_GFMS_EVENT_BIND:
			printf("BIND (PAX ID %d)\n", e->src_sw_id);
			print_gfms_event_bind(e);
			break;
		case SWITCHTEC_GFMS_EVENT_UNBIND:
			printf("UNBIND (PAX ID %d):\n", e->src_sw_id);
			print_gfms_event_bind(e);
			break;
		case SWITCHTEC_GFMS_EVENT_DATABASE_CHANGED:
			printf("DATABASE_CHANGED (PAX ID %d):\n", e->src_sw_id);
			break;
		case SWITCHTEC_GFMS_EVENT_HVD_INST_ENABLE:
			printf("HVD_INSTANCE_ENABLE (PAX ID %d):\n", e->src_sw_id);
			print_gfms_event_hvd(e);
			break;
		case SWITCHTEC_GFMS_EVENT_HVD_INST_DISABLE:
			printf("HVD_INSTANCE_DISABLE (PAX ID %d):\n", e->src_sw_id);
			print_gfms_event_hvd(e);
			break;
		case SWITCHTEC_GFMS_EVENT_EP_PORT_ADD:
			printf("EP_PORT_ADD (PAX ID %d):\n", e->src_sw_id);
			print_gfms_event_ep(e);
			break;
		case SWITCHTEC_GFMS_EVENT_EP_PORT_REMOVE:
			printf("EP_PORT_REMOVE (PAX ID %d):\n", e->src_sw_id);
			print_gfms_event_ep(e);
			break;
		case SWITCHTEC_GFMS_EVENT_AER:
			printf("AER (PAX ID %d):\n", e->src_sw_id);
			print_gfms_event_aer(e);
			break;
		}
		e++;
	}
}

#define CMD_DESC_GFMS_EVENTS "display GFMS event information"

static int gfms_events(int argc, char **argv)
{
	struct switchtec_gfms_event elist[128];
	int overflow;
	size_t num;
	size_t remain_num;
	int ret;

	static struct {
		struct switchtec_dev *dev;
		int status;
		int clear;
	} cfg = {
	};

	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"status", 's', "", CFG_NONE, &cfg.status, no_argument,
		 "show events in all partitions"},
		{"reset", 'r', "", CFG_NONE, &cfg.clear, no_argument,
		 "clear all GFMS events"},
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_GFMS_EVENTS, opts, &cfg, sizeof(cfg));

	num = ARRAY_SIZE(elist);

	if (cfg.clear && cfg.status) {
		fprintf(stderr, "--clear and --reset cannot be specified at the same time.\n");
		return -1;
	}

	if (cfg.clear) {
		ret = switchtec_clear_gfms_events(cfg.dev);
		if (ret < 0)  {
			switchtec_perror("get_gfms_events");
			return ret;
		}
	}

	if (cfg.status)
		num = 0;

	ret = switchtec_get_gfms_events(cfg.dev, elist, num,
					&overflow, &remain_num);
	if (ret < 0)  {
		switchtec_perror("get_gfms_events");
		return ret;
	}

	print_gfms_event_list(elist, ret, overflow, remain_num);

	return ret;
}

static const struct cmd commands[] = {
	{"topo_info", topo_info, CMD_DESC_TOPO_INFO},
	{"gfms_bind", gfms_bind, CMD_DESC_GFMS_BIND},
	{"gfms_unbind", gfms_unbind, CMD_DESC_GFMS_UNBIND},
	{"gfms_dump", gfms_dump, CMD_DESC_GFMS_DUMP},
	{"route", route, CMD_DESC_ROUTE},
	{"port_control", port_control, CMD_DESC_PORT_CONTROL},
	{"portcfg_show", portcfg_show, CMD_DESC_PORTCFG_SHOW},
	{"portcfg_set", portcfg_set, CMD_DESC_PORTCFG_SET},
	{"gfms_events", gfms_events, CMD_DESC_GFMS_EVENTS},
	{}
};

static struct subcommand subcmd = {
	.name = "fabric",
	.cmds = commands,
	.desc = "Switchtec Fabric Management (PAX only)",
	.long_desc = "",
};

REGISTER_SUBCMD(subcmd);
