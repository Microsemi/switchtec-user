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
#include <switchtec/cap.h>

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define CMD_DESC_MC_SHOW "show the multicast PCI capability"

static const char *port_type_str(enum switchtec_cap_port_type type)
{
	switch (type) {
	case SWITCHTEC_CAP_PORT_DSP:
		return "DSP";
	case SWITCHTEC_CAP_PORT_USP:
		return "USP";
	case SWITCHTEC_CAP_PORT_MGMT:
		return "MGMT";
	default:
		return "Unknown";
	}
}

static int multicast_show_one_port(struct switchtec_dev *dev,
				   const char *bdf_str)
{
	struct switchtec_multicast_cap mc;
	struct switchtec_port_info pinfo;
	int ret;

	ret = switchtec_find_port_by_bdf(dev, bdf_str, &pinfo);
	if (ret < 0) {
		switchtec_perror("find_port_by_bdf");
		return -1;
	}

	ret = switchtec_multicast_cap_get(dev, pinfo.gas_base, &mc);
	if (ret) {
		switchtec_perror("multicast_cap_get");
		return -1;
	}

	printf("Multicast Extended Capability @ 0x%06X (%s, BDF %s):\n",
	       pinfo.gas_base + SWITCHTEC_CAP_MULTICAST_OFFSET,
	       port_type_str(pinfo.port_type), bdf_str);
	printf("  Header (DW0):        0x%08X\n", mc.header);
	printf("    Capability ID:     0x%04X\n",
	       SWITCHTEC_MC_HDR_CAP_ID(mc.header));
	printf("    Capability Ver:    %u\n",
	       SWITCHTEC_MC_HDR_CAP_VER(mc.header));
	printf("    Next Cap Offset:   0x%03X\n",
	       SWITCHTEC_MC_HDR_NEXT_CAP(mc.header));
	printf("  Cap/Ctrl (DW1):      0x%08X\n", mc.cap_ctrl);
	printf("    MC Capability:     0x%04X\n",
	       SWITCHTEC_MC_CAP(mc.cap_ctrl));
	printf("      Max Group:       %u\n",
	       SWITCHTEC_MC_CAP_MAX_GROUP(mc.cap_ctrl));
	printf("    MC Control:        0x%04X\n",
	       SWITCHTEC_MC_CTRL(mc.cap_ctrl));
	/* Zero indicates 1 group (PCIe spec) add one to display proper number*/
	printf("      Num Group:       %u\n",
	       SWITCHTEC_MC_CTRL_NUM_GROUP(mc.cap_ctrl)+1);
	printf("      MC Enable:       %s\n",
	       SWITCHTEC_MC_CTRL_ENABLE(mc.cap_ctrl) ? "Enabled" : "Disabled");
	printf("  MC Base Addr (DW2-3):  0x%016llX\n",
	       (unsigned long long)SWITCHTEC_MC_BASE_ADDR(mc.mc_base_addr));
	printf("    Index Position:    %u\n",
	       (unsigned)SWITCHTEC_MC_BASE_INDEX_POS(mc.mc_base_addr));
	printf("  MC Receive (DW4-5):    0x%016llX\n",
	       (unsigned long long)mc.mc_receive);
	printf("  MC Block All (DW6-7):  0x%016llX\n",
	       (unsigned long long)mc.mc_block_all);
	printf("  MC Block Untrans (DW8-9): 0x%016llX\n",
	       (unsigned long long)mc.mc_block_untranslated);
	printf("  MC Overlay BAR (DW10-11): 0x%016llX\n",
	       (unsigned long long)SWITCHTEC_MC_OVERLAY_ADDR(mc.mc_overlay_bar));
	printf("    Overlay Size:      %u\n",
	       (unsigned)SWITCHTEC_MC_OVERLAY_SIZE(mc.mc_overlay_bar));

	return 0;
}

static int multicast_show(int argc, char **argv)
{
	struct switchtec_status *status = NULL;
	int nr_ports, p;
	int ret;

	static struct {
		struct switchtec_dev *dev;
		const char *bdf;
		int all;
	} cfg = {
		.bdf = NULL,
		.all = 0,
	};

	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"bdf", 'b', "BB:DD.F", CFG_STRING, &cfg.bdf, required_argument,
		 "BDF of port to access"},
		{"all", 'a', "", CFG_NONE, &cfg.all, no_argument,
		 "show all USPs and DSPs"},
		{NULL}
	};

	argconfig_parse(argc, argv, CMD_DESC_MC_SHOW, opts, &cfg, sizeof(cfg));

	if (!cfg.bdf && !cfg.all) {
		fprintf(stderr, "Either --bdf or --all must be specified\n");
		return -1;
	}

	if (cfg.bdf && cfg.all) {
		fprintf(stderr, "--bdf and --all are mutually exclusive\n");
		return -1;
	}

	if (!cfg.all)
		return multicast_show_one_port(cfg.dev, cfg.bdf);

	nr_ports = switchtec_status(cfg.dev, &status);
	if (nr_ports < 0) {
		switchtec_perror("status");
		return -1;
	}

	ret = switchtec_get_devices(cfg.dev, status, nr_ports);
	if (ret < 0) {
		switchtec_perror("get_devices");
		switchtec_status_free(status, nr_ports);
		return -1;
	}

	for (p = 0; p < nr_ports; p++) {
		struct switchtec_status *s = &status[p];

		if (s->port.partition == SWITCHTEC_UNBOUND_PORT)
			continue;

		if (!s->pci_bdf)
			continue;

		ret = multicast_show_one_port(cfg.dev, s->pci_bdf);
		if (ret) {
			switchtec_status_free(status, nr_ports);
			return -1;
		}
		printf("\n");
	}

	switchtec_status_free(status, nr_ports);
	return 0;
}

#define ADDR_NOT_SET	((unsigned long long)-1)

#define CMD_DESC_MC_SET "set fields in the multicast PCI capability"

static int multicast_set_one_port(struct switchtec_dev *dev,
				  const char *bdf_str,
				  struct switchtec_multicast_set *set,
				  int assume_yes, int quiet)
{
	struct switchtec_multicast_cap mc;
	struct switchtec_port_info pinfo;
	int ret;

	ret = switchtec_find_port_by_bdf(dev, bdf_str, &pinfo);
	if (ret < 0) {
		switchtec_perror("find_port_by_bdf");
		return -1;
	}

	ret = switchtec_multicast_cap_get(dev, pinfo.gas_base, &mc);
	if (ret) {
		switchtec_perror("multicast_cap_get");
		return -1;
	}

	if (!quiet) {
		printf("Proposed changes to Multicast Capability (%s, BDF %s):\n",
		       port_type_str(pinfo.port_type), bdf_str);
		if (set->enable && !set->disable)
			printf("  MC Enable: %s -> Enabled\n",
			       SWITCHTEC_MC_CTRL_ENABLE(mc.cap_ctrl) ?
			       "Enabled" : "Disabled");
		else if (set->disable)
			printf("  MC Enable: %s -> Disabled\n",
			       SWITCHTEC_MC_CTRL_ENABLE(mc.cap_ctrl) ?
			       "Enabled" : "Disabled");
		/* Zero indicates 1 group (PCIe spec) add one to display proper number*/
		if (set->num_group >= 0 && set->num_group <= 63)
			printf("  Num Group: %u -> %d\n",
			       SWITCHTEC_MC_CTRL_NUM_GROUP(mc.cap_ctrl)+1,
			       set->num_group+1);
		if (set->index_pos >= 0 && set->index_pos <= 63)
			printf("  Index Position: %u -> %d\n",
			       (unsigned)SWITCHTEC_MC_BASE_INDEX_POS(mc.mc_base_addr),
			       set->index_pos);
		if (set->set_base_addr)
			printf("  MC Base Address: 0x%016llX -> 0x%016llX\n",
			       (unsigned long long)SWITCHTEC_MC_BASE_ADDR(mc.mc_base_addr),
			       (unsigned long long)SWITCHTEC_MC_BASE_ADDR(set->base_addr));
		if (set->set_receive)
			printf("  MC Receive: 0x%016llX -> 0x%016llX\n",
			       (unsigned long long)mc.mc_receive,
			       (unsigned long long)set->receive);
		if (set->set_block_all)
			printf("  MC Block All: 0x%016llX -> 0x%016llX\n",
			       (unsigned long long)mc.mc_block_all,
			       (unsigned long long)set->block_all);
		if (set->set_block_untranslated)
			printf("  MC Block Untrans: 0x%016llX -> 0x%016llX\n",
			       (unsigned long long)mc.mc_block_untranslated,
			       (unsigned long long)set->block_untranslated);
		if (set->set_overlay_bar)
			printf("  MC Overlay BAR: 0x%016llX -> 0x%016llX\n",
			       (unsigned long long)SWITCHTEC_MC_OVERLAY_ADDR(mc.mc_overlay_bar),
			       (unsigned long long)SWITCHTEC_MC_OVERLAY_ADDR(set->overlay_bar));
		if (set->overlay_size >= 0 && set->overlay_size <= 63)
			printf("  Overlay Size: %u -> %d\n",
			       (unsigned)SWITCHTEC_MC_OVERLAY_SIZE(mc.mc_overlay_bar),
			       set->overlay_size);

		ret = ask_if_sure(assume_yes);
		if (ret)
			return ret;
	}

	ret = switchtec_multicast_cap_set(dev, pinfo.gas_base, set);
	if (ret) {
		switchtec_perror("multicast_cap_set");
		return -1;
	}

	if (!quiet)
		printf("Multicast capability updated.\n");

	return 0;
}

static int multicast_set(int argc, char **argv)
{
	struct switchtec_multicast_set set = {0};
	struct switchtec_status *status = NULL;
	int nr_ports, p;
	int ret = 0;
	int ports_updated = 0;

	static struct {
		struct switchtec_dev *dev;
		const char *bdf;
		int all;
		int enable;
		int disable;
		int num_group;
		int index_pos;
		unsigned long long base_addr;
		unsigned long long receive;
		unsigned long long block_all;
		unsigned long long block_untrans;
		unsigned long long overlay_bar;
		int overlay_size;
		int assume_yes;
	} cfg = {
		.bdf = NULL,
		.all = 0,
		.enable = 0,
		.disable = 0,
		.num_group = -1,
		.index_pos = -1,
		.base_addr = ADDR_NOT_SET,
		.receive = ADDR_NOT_SET,
		.block_all = ADDR_NOT_SET,
		.block_untrans = ADDR_NOT_SET,
		.overlay_bar = ADDR_NOT_SET,
		.overlay_size = -1,
	};

	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"bdf", 'b', "BB:DD.F", CFG_STRING, &cfg.bdf, required_argument,
		 "BDF of port to access"},
		{"all", 'a', "", CFG_NONE, &cfg.all, no_argument,
		 "set all USPs and DSPs"},
		{"enable", 'e', "", CFG_NONE, &cfg.enable, no_argument,
		 "enable multicast"},
		{"disable", 'd', "", CFG_NONE, &cfg.disable, no_argument,
		 "disable multicast"},
		{"num-group", 'n', "NUM", CFG_INT, &cfg.num_group,
		 required_argument, "number of multicast groups (0-63)"},
		{"index-pos", 'i', "POS", CFG_INT, &cfg.index_pos,
		 required_argument, "index position in base address (0-63)"},
		{"base-addr", 'B', "ADDR", CFG_LONG_LONG, &cfg.base_addr,
		 required_argument, "multicast base address"},
		{"receive", 'r', "MASK", CFG_LONG_LONG, &cfg.receive,
		 required_argument, "multicast receive mask"},
		{"block-all", 'A', "MASK", CFG_LONG_LONG, &cfg.block_all,
		 required_argument, "multicast block all mask"},
		{"block-untrans", 'u', "MASK", CFG_LONG_LONG, &cfg.block_untrans,
		 required_argument, "multicast block untranslated mask"},
		{"overlay-bar", 'o', "ADDR", CFG_LONG_LONG, &cfg.overlay_bar,
		 required_argument, "multicast overlay BAR address"},
		{"overlay-size", 's', "SIZE", CFG_INT, &cfg.overlay_size,
		 required_argument, "multicast overlay size (0-63)"},
		{"yes", 'y', "", CFG_NONE, &cfg.assume_yes, no_argument,
		 "assume yes when prompted"},
		{NULL}
	};

	argconfig_parse(argc, argv, CMD_DESC_MC_SET, opts, &cfg, sizeof(cfg));

	if (!cfg.bdf && !cfg.all) {
		fprintf(stderr, "Either --bdf or --all must be specified\n");
		return -1;
	}

	if (cfg.bdf && cfg.all) {
		fprintf(stderr, "--bdf and --all are mutually exclusive\n");
		return -1;
	}

	if (cfg.num_group != -1 && (cfg.num_group < 0 || cfg.num_group > 63)) {
		fprintf(stderr, "--num-group must be between 0 and 63\n");
		return -1;
	}

	if (cfg.index_pos != -1 && (cfg.index_pos < 0 || cfg.index_pos > 63)) {
		fprintf(stderr, "--index-pos must be between 0 and 63\n");
		return -1;
	}

	if (cfg.overlay_size != -1 &&
	    (cfg.overlay_size < 0 || cfg.overlay_size > 30)) {
		fprintf(stderr, "--overlay-size must be between 0 and 30\n");
		return -1;
	}

	set.enable = cfg.enable;
	set.disable = cfg.disable;
	set.num_group = cfg.num_group;
	set.index_pos = cfg.index_pos;
	set.base_addr = cfg.base_addr;
	set.receive = cfg.receive;
	set.block_all = cfg.block_all;
	set.block_untranslated = cfg.block_untrans;
	set.set_base_addr = (cfg.base_addr != ADDR_NOT_SET);
	set.set_receive = (cfg.receive != ADDR_NOT_SET);
	set.set_block_all = (cfg.block_all != ADDR_NOT_SET);
	set.set_block_untranslated = (cfg.block_untrans != ADDR_NOT_SET);
	set.overlay_bar = cfg.overlay_bar;
	set.overlay_size = cfg.overlay_size;
	set.set_overlay_bar = (cfg.overlay_bar != ADDR_NOT_SET);

	if (!set.enable && !set.disable && set.num_group < 0 &&
	    set.index_pos < 0 && !set.set_base_addr && !set.set_receive &&
	    !set.set_block_all && !set.set_block_untranslated &&
	    !set.set_overlay_bar && set.overlay_size < 0) {
		fprintf(stderr, "No changes specified.\n");
		return 0;
	}

	if (!cfg.all) {
		ret = multicast_set_one_port(cfg.dev, cfg.bdf, &set, cfg.assume_yes, 0);
		return -1;
	}

	printf("Setting multicast on all USPs and DSPs:\n");
	if (set.enable && !set.disable)
		printf("  MC Enable -> Enabled\n");
	else if (set.disable)
		printf("  MC Enable -> Disabled\n");
	if (set.num_group >= 0 && set.num_group <= 63)
		printf("  Num Group -> %d\n", set.num_group);
	if (set.set_base_addr)
		printf("  MC Base Address -> 0x%016llX\n",
		       (unsigned long long)SWITCHTEC_MC_BASE_ADDR(set.base_addr));
	if (set.set_receive)
		printf("  MC Receive -> 0x%016llX\n",
		       (unsigned long long)set.receive);
	if (set.set_block_all)
		printf("  MC Block All -> 0x%016llX\n",
		       (unsigned long long)set.block_all);
	if (set.set_block_untranslated)
		printf("  MC Block Untrans -> 0x%016llX\n",
		       (unsigned long long)set.block_untranslated);
	if (set.set_overlay_bar)
		printf("  MC Overlay BAR -> 0x%016llX\n",
		       (unsigned long long)SWITCHTEC_MC_OVERLAY_ADDR(set.overlay_bar));
	if (set.overlay_size >= 0 && set.overlay_size <= 63)
		printf("  Overlay Size -> %d\n", set.overlay_size);

	ret = ask_if_sure(cfg.assume_yes);
	if (ret)
		return -1;

	nr_ports = switchtec_status(cfg.dev, &status);
	if (nr_ports < 0) {
		switchtec_perror("status");
		return -1;
	}

	ret = switchtec_get_devices(cfg.dev, status, nr_ports);
	if (ret < 0) {
		switchtec_perror("get_devices");
		goto free_status;
	}

	for (p = 0; p < nr_ports; p++) {
		struct switchtec_status *s = &status[p];

		if (s->port.partition == SWITCHTEC_UNBOUND_PORT)
			continue;

		if (!s->pci_bdf)
			continue;

		printf("Setting %s %s... ",
		       s->port.upstream ? "USP" : "DSP", s->pci_bdf);
		fflush(stdout);

		ret = multicast_set_one_port(cfg.dev, s->pci_bdf, &set, 1, 1);
		if (ret) {
			printf("FAILED\n");
			return -1;
		}
		printf("OK\n");
		ports_updated++;
	}

	printf("Multicast capability updated on %d ports.\n", ports_updated);
	return 0;

free_status:
	switchtec_status_free(status, nr_ports);
	return -1;
}

static const struct cmd commands[] = {
	{"multicast_show", multicast_show, CMD_DESC_MC_SHOW},
	{"multicast_set", multicast_set, CMD_DESC_MC_SET},
	{}
};

static struct subcommand subcmd = {
	.name = "cap",
	.cmds = commands,
	.desc = "PCI Capability Access",
	.long_desc = "These commands allow reading and writing PCI capabilities "
		"through the Global Address Space. Use with caution as improper "
		"configuration may affect device operation.\n\n"
		"Port type (USP vs DSP) is determined from switch status.\n"
		"BDF format: [DDDD:]BB:DD.F (domain optional, defaults to 0000)\n"
		"Multicast capability offset: 0x170",
};

REGISTER_SUBCMD(subcmd);
