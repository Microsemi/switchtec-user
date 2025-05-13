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
#include "suffix.h"
#include "progress.h"
#include "gui.h"
#include "common.h"

#include <switchtec/switchtec.h>
#include <switchtec/errors.h>
#include <switchtec/utils.h>
#include <switchtec/pci.h>

#include <locale.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>

static struct switchtec_dev *global_dev = NULL;
static int global_pax_id = SWITCHTEC_PAX_ID_LOCAL;

enum output_format {
	FMT_NORMAL,
	FMT_TABLE,
};

static const struct argconfig_choice output_fmt_choices[] = {
	{"normal", FMT_NORMAL, "Human Readable Output"},
	{"table",  FMT_TABLE,  "Tabular Output"},
	{}
};

static const struct argconfig_choice bandwidth_types[] = {
	{"RAW", SWITCHTEC_BW_TYPE_RAW, "get the raw bandwidth"},
	{"PAYLOAD", SWITCHTEC_BW_TYPE_PAYLOAD, "get the payload bandwidth"},
	{}
};

static int set_global_pax_id(void)
{
	if (global_dev)
		return switchtec_set_pax_id(global_dev, global_pax_id);

	return 0;
}

int switchtec_handler(const char *optarg, void *value_addr,
		      const struct argconfig_options *opt)
{
	struct switchtec_dev *dev;

	global_dev = dev = switchtec_open(optarg);

	if (dev == NULL) {
		switchtec_perror(optarg);
		return 1;
	}

	if (switchtec_is_gen3(dev) && switchtec_is_pax(dev)) {
		fprintf(stderr, "%s: Gen3 PAX is not supported.\n", optarg);
		return 2;
	}
#if 0
	if (switchtec_is_gen4(dev)) {
		fprintf(stderr, "%s: Gen4 is not supported.\n", optarg);
		return 3;
	}
#endif
	*((struct switchtec_dev  **) value_addr) = dev;

	if (set_global_pax_id()) {
		fprintf(stderr, "%s: Setting PAX ID is not supported.\n", optarg);
		return 4;
	}

	return 0;
}

/*
 * The 'mfg' submenu commands are only available in Linux builds.
 *
 * Due to the difference in driver architecture, supporting Windows
 * build is non-trivial. After evaluating the development effort
 * and the resources available, we have decided to remove Windows
 * build support from release roadmap.
 *
 */
int mfg_handler(const char *optarg, void *value_addr,
		const struct argconfig_options *opt)
{
#ifndef __linux__
	printf("WARNING: MFG COMMANDS ARE NOT SUPPORTED ON YOUR CURRENT OPERATING SYSTEM!\n"
	       "Use this command at your own risk!!!\n\n\n");
#endif
	return switchtec_handler(optarg, value_addr, opt);
}

int pax_handler(const char *optarg, void *value_addr,
		const struct argconfig_options *opt)
{
	char *end;
	long num;

	errno = 0;
	num = strtol(optarg, &end, 0);
	global_pax_id = num;

	if ((end == optarg) || errno || num < 0 ||
	    (global_pax_id & ~SWITCHTEC_PAX_ID_MASK)) {
		fprintf(stderr, "Invalid PAX ID specified: %s\n", optarg);
		return 1;
	}

	if (set_global_pax_id()) {
		fprintf(stderr, "%s: Setting PAX ID is not supported.\n", optarg);
		return 4;
	}

	return 0;
}

#define CMD_DESC_LIST "list all Switchtec devices on this machine"

static int list(int argc, char **argv)
{
	struct switchtec_device_info *devices = NULL;
	int i, n;

	static struct {
		int verbose;
	} cfg = {};

	const struct argconfig_options opts[] = {
		{"verbose", 'v', "", CFG_NONE, &cfg.verbose, no_argument,
		 "print additional device information"},
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_LIST, opts, &cfg, sizeof(cfg));

	n = switchtec_list(&devices);
	if (n <= 0)
		return n;

	for (i = 0; i < n; i++) {
		printf("%-20s\t%-16s%-5s\t%-10s\t%s", devices[i].name,
		       devices[i].product_id, devices[i].product_rev,
		       devices[i].fw_version, devices[i].pci_dev);
		if (cfg.verbose) {
			if (strlen(devices[i].desc))
				printf("\t%s", devices[i].desc);
			if (strlen(devices[i].path))
				printf("\t%s", devices[i].path);
		}
		printf("\n");
	}

	free(devices);
	return 0;
}

static int print_dev_info(struct switchtec_dev *dev)
{
	int ret;
	int device_id;
	char version[32];
	enum switchtec_boot_phase phase;
	enum switchtec_rev hw_rev;

	device_id = switchtec_device_id(dev);

	ret = switchtec_get_fw_version(dev, version, sizeof(version));
	if (ret < 0)
		strcpy(version, "N/A");

	ret = switchtec_get_device_info(dev, &phase, NULL, &hw_rev);
	if (ret) {
		switchtec_perror("dev info");
		return ret;
	}

	printf("%s (%s):\n", switchtec_name(dev),
	       switchtec_phase_id_str(phase));
	printf("    Generation:  %s\n", switchtec_gen_str(dev));
	printf("    HW Revision: %s\n", switchtec_rev_str(hw_rev));
	printf("    Variant:     %s\n",
	       device_id ? switchtec_variant_str(dev) : "N/A");
	if (device_id)
		printf("    Device ID:   0x%04x\n", device_id);
	else
		printf("    Device ID:   %s\n", "N/A");
	printf("    FW Version:  %s\n", version);

	return 0;
}

#define CMD_DESC_INFO "display switch information (BL1, BL2, Main Firmware)"

static int info(int argc, char **argv)
{
	static struct {
		struct switchtec_dev *dev;
	} cfg = {};

	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_INFO, opts, &cfg, sizeof(cfg));

	return print_dev_info(cfg.dev);
}

static void print_port_title(struct switchtec_dev *dev,
			     struct switchtec_port_id *p)
{
	static int last_partition = -1;
	const char *local = "";

	if (p->partition != last_partition) {
		if (p->partition == SWITCHTEC_UNBOUND_PORT) {
			printf("Unbound Ports:\n");
		} else {
			if (p->partition == switchtec_partition(dev))
				local = "    (LOCAL)";
			printf("Partition %d:%s\n", p->partition, local);
		}
	}
	last_partition = p->partition;

	if (p->partition == SWITCHTEC_UNBOUND_PORT) {
		printf("    Phys Port ID %d  (Stack %d, Port %d)\n",
		       p->phys_id, p->stack, p->stk_id);
	} else {
		printf("    Logical Port ID %d (%s):\n", p->log_id,
		       p->upstream ? "USP" : "DSP");
	}
}

#define CMD_DESC_GUI "display a simple ncurses GUI"

static int gui(int argc, char **argv)
{
	int ret;

	static struct {
		struct switchtec_dev *dev;
		unsigned all_ports;
		unsigned reset_bytes;
		unsigned refresh;
		int duration;
		enum switchtec_bw_type bw_type;
	} cfg = {
	    .refresh  = 1,
	    .duration = -1,
	    .bw_type  = SWITCHTEC_BW_TYPE_RAW,
	};

	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"all_ports", 'a', "", CFG_NONE, &cfg.all_ports, no_argument,
		 "show all ports (including downed links)"},
		{"reset", 'r', "", CFG_NONE, &cfg.reset_bytes, no_argument,
		 "reset byte counters"},
		{"refresh", 'f', "", CFG_POSITIVE, &cfg.refresh, required_argument,
		 "GUI refresh period in seconds (default: 1 second)"},
		{"duration", 'd', "", CFG_INT, &cfg.duration, required_argument,
		 "GUI duration in seconds (-1 = forever)"},
		{"bw_type", 'b', "TYPE", CFG_CHOICES, &cfg.bw_type,
		 required_argument, "GUI bandwidth type", .choices=bandwidth_types},
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_GUI, opts, &cfg, sizeof(cfg));

	ret = gui_main(cfg.dev, cfg.all_ports, cfg.reset_bytes, cfg.refresh,
		       cfg.duration, cfg.bw_type);

	return ret;
}

#define PCI_ACS_P2P_MASK (PCI_ACS_CTRL_REQ_RED | PCI_ACS_CTRL_CMPLT_RED | \
			  PCI_ACS_EGRESS_CTRL)

static const char * const pci_acs_strings[] = {
	"SrcValid",
	"TransBlk",
	"ReqRedir",
	"CmpltRedir",
	"UpstreamFwd",
	"EgressCtrl",
	"DirectTrans",
	NULL,
};

static char *pci_acs_to_string(char *buf, size_t buflen, int acs_ctrl,
			       int verbose)
{
	int ptr = 0;
	int wr;
	int i;

	if (acs_ctrl == -1) {
		snprintf(buf, buflen, "Unknown");
		return buf;
	}

	if (!verbose) {
		if (acs_ctrl & PCI_ACS_P2P_MASK)
			snprintf(buf, buflen, "P2P Redirected");
		else
			snprintf(buf, buflen, "Direct P2P Supported");
		return buf;
	}

	for (i = 0; pci_acs_strings[i]; i++) {
		wr = snprintf(buf + ptr, buflen - ptr, "%s%c ",
			      pci_acs_strings[i],
			      (acs_ctrl & (1 << i)) ? '+' : '-');

		if (wr <= 0 || wr >= buflen - ptr)
			break;

		ptr += wr;
	}

	if (ptr)
		buf[ptr - 1] = 0;

	return buf;
}

static void status_print_normal(struct switchtec_dev *dev, int ports,
				struct switchtec_status *status,
				struct switchtec_bwcntr_res *bw_data,
				bool verbose)
{
	struct switchtec_status *s;
	const char *bw_suf;
	double bw_val;
	char buf[100];
	int p;

	for (p = 0; p < ports; p++) {
		s = &status[p];
		print_port_title(dev, &s->port);

		if (s->port.partition == SWITCHTEC_UNBOUND_PORT)
			continue;

		printf("\tPhys Port ID:    \t%d (Stack %d, Port %d)\n",
		       s->port.phys_id, s->port.stack, s->port.stk_id);
		if (s->pci_bdf)
			printf("\tBus-Dev-Func:    \t%s\n", s->pci_bdf);
		if (verbose && s->pci_bdf_path)
			printf("\tBus-Dev-Func Path:\t%s\n", s->pci_bdf_path);

		printf("\tStatus:          \t%s\n",
		       s->link_up ? "UP" : "DOWN");
		printf("\tLTSSM:           \t%s\n", s->ltssm_str);
		printf("\tMax-Width:       \tx%d\n", s->cfg_lnk_width);

		if (!s->link_up) continue;

		printf("\tNeg Width:       \tx%d\n", s->neg_lnk_width);
		printf("\tLane Reversal:   \t%s\n", s->lane_reversal_str);
		printf("\tFirst Act Lane:  \t%d\n", s->first_act_lane);
		printf("\tLanes:           \t%s\n", s->lanes);
		printf("\tRate:            \tGen%d - %g GT/s  %g GB/s\n",
		       s->link_rate, switchtec_gen_transfers[s->link_rate],
		       switchtec_gen_datarate[s->link_rate]*s->neg_lnk_width/1000.);

		bw_val = switchtec_bwcntr_tot(&bw_data[p].egress);
		bw_suf = suffix_si_get(&bw_val);
		printf("\tOut Bytes:       \t%-.3g %sB\n", bw_val, bw_suf);

		bw_val = switchtec_bwcntr_tot(&bw_data[p].ingress);
		bw_suf = suffix_si_get(&bw_val);
		printf("\tIn Bytes:        \t%-.3g %sB\n", bw_val, bw_suf);

		if (s->acs_ctrl != -1) {
			pci_acs_to_string(buf, sizeof(buf), s->acs_ctrl,
					  verbose);
			printf("\tACS:             \t%s\n", buf);
		}

		if (!s->vendor_id || !s->device_id || !s->pci_dev)
			continue;

		printf("\tDevice:          \t%04x:%04x (%s)\n",
		       s->vendor_id, s->device_id, s->pci_dev);
		if (s->class_devices)
			printf("\t                 \t%s\n", s->class_devices);
	}
}

static void status_print_table(int ports, struct switchtec_status *status)
{
	struct switchtec_status *s, *map[SWITCHTEC_MAX_PORTS] = {};
	int p;

	for (p = 0; p < ports; p++) {
		s = &status[p];
		map[s->port.phys_id] = s;
	}

	for (p = 0; p < SWITCHTEC_MAX_PORTS; p++) {
		if (!map[p])
			continue;
		s = map[p];

		printf("[%02d] ", s->port.phys_id);
		if (s->port.partition == SWITCHTEC_UNBOUND_PORT)
			printf("part:      ");
		else
			printf("part:%02d.%02d ", s->port.partition,
			       s->port.log_id);
		printf("w:cfg[x%02d]-neg[x%02d] ", s->cfg_lnk_width,
		       s->neg_lnk_width);
		printf("stk:%d.%d ", s->port.stack, s->port.stk_id);
		printf("lanes:%-16s ", s->lanes);
		printf("rev:%d ", s->lane_reversal);
		printf(s->port.upstream ? "usp " : "dsp ");
		printf("link:%d ", s->link_up);
		printf("rate:G%d ", s->link_rate);
		printf("LTSSM:%s\n", s->ltssm_str);
	}
}

#define CMD_DESC_STATUS "display switch port status information"

static int status(int argc, char **argv)
{
	int ret;
	int ports;
	struct switchtec_status *status;
	int port_ids[SWITCHTEC_MAX_PORTS];
	struct switchtec_bwcntr_res bw_data[SWITCHTEC_MAX_PORTS];
	int p;

	static struct {
		struct switchtec_dev *dev;
		int reset_bytes;
		int verbose;
		int fmt;
	} cfg = {};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"format", 'f', "FMT", CFG_CHOICES, &cfg.fmt, required_argument,
		 "output format (default: normal)",
		 .choices=output_fmt_choices},
		{"reset", 'r', "", CFG_NONE, &cfg.reset_bytes, no_argument,
		 "reset byte counters"},
		{"verbose", 'v', "", CFG_NONE, &cfg.verbose, no_argument,
		 "print additional information (only with 'normal' format)"},
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_STATUS, opts, &cfg, sizeof(cfg));

	ports = switchtec_status(cfg.dev, &status);
	if (ports < 0) {
		switchtec_perror("status");
		return ports;
	}

	ret = switchtec_get_devices(cfg.dev, status, ports);
	if (ret < 0) {
		switchtec_perror("get_devices");
		goto free_and_return;
	}

	for (p = 0; p < ports; p++)
		port_ids[p] = status[p].port.phys_id;

	ret = switchtec_bwcntr_many(cfg.dev, ports, port_ids, cfg.reset_bytes,
				    bw_data);
	if (ret < 0) {
		switchtec_perror("bwcntr");
		goto free_and_return;
	}

	if (cfg.reset_bytes)
		memset(bw_data, 0, sizeof(bw_data));

	switch (cfg.fmt) {
	case FMT_NORMAL:
		status_print_normal(cfg.dev, ports, status, bw_data, cfg.verbose);
		break;
	case FMT_TABLE:
		status_print_table(ports, status);
		break;
	}

	ret = 0;

free_and_return:
	switchtec_status_free(status, ports);

	return ret;
}

static void print_bw(const char *msg, uint64_t time_us, uint64_t bytes)
{
	double rate = bytes / (time_us * 1e-6);
	const char *suf = suffix_si_get(&rate);

	printf("\t%-8s\t%5.3g %sB/s\n", msg, rate, suf);
}

#define CMD_DESC_BW "measure the traffic bandwidth through each port"

static int bw(int argc, char **argv)
{
	struct switchtec_bwcntr_res *before, *after;
	struct switchtec_port_id *port_ids;
	int ret;
	int i;
	uint64_t ingress_tot, egress_tot;

	static struct {
		struct switchtec_dev *dev;
		unsigned meas_time;
		int verbose;
		enum switchtec_bw_type bw_type;
	} cfg = {
		.meas_time = 5,
		.bw_type = SWITCHTEC_BW_TYPE_RAW,
	};

	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"time", 't', "NUM", CFG_POSITIVE, &cfg.meas_time,
		  required_argument,
		 "measurement time in seconds"},
		{"verbose", 'v', "", CFG_NONE, &cfg.verbose, no_argument,
		 "print posted, non-posted and completion results"},
		{"bw_type", 'b', "TYPE", CFG_CHOICES, &cfg.bw_type,
		 required_argument, "bandwidth type", .choices=bandwidth_types},
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_BW, opts, &cfg, sizeof(cfg));

	ret = switchtec_bwcntr_set_all(cfg.dev, cfg.bw_type);
	if (ret < 0) {
		switchtec_perror("bw type");
		return ret;
	}
	/* switchtec_bwcntr_set_all will reset bandwidth counter and it needs
	 * about 1s */
	sleep(1);

	ret = switchtec_bwcntr_all(cfg.dev, 0, &port_ids, &before);
	if (ret < 0) {
		switchtec_perror("bw");
		return ret;
	}

	sleep(cfg.meas_time);

	ret = switchtec_bwcntr_all(cfg.dev, 0, NULL, &after);
	if (ret < 0) {
		free(before);
		free(port_ids);
		switchtec_perror("bw");
		return ret;
	}

	for (i = 0; i < ret; i++) {
		print_port_title(cfg.dev, &port_ids[i]);

		switchtec_bwcntr_sub(&after[i], &before[i]);

		egress_tot = switchtec_bwcntr_tot(&after[i].egress);
		ingress_tot = switchtec_bwcntr_tot(&after[i].ingress);

		if (!cfg.verbose) {
			print_bw("Out:", after[i].time_us, egress_tot);
			print_bw("In:", after[i].time_us, ingress_tot);
		} else {
			printf("\tOut:\n");
			print_bw("  Posted:", after[i].time_us,
				 after[i].egress.posted);
			print_bw("  Non-Posted:", after[i].time_us,
				 after[i].egress.nonposted);
			print_bw("  Completion:", after[i].time_us,
				 after[i].egress.comp);
			print_bw("  Total:", after[i].time_us, egress_tot);

			printf("\tIn:\n");
			print_bw("  Posted:", after[i].time_us,
				 after[i].ingress.posted);
			print_bw("  Non-Posted:", after[i].time_us,
				 after[i].ingress.nonposted);
			print_bw("  Completion:", after[i].time_us,
				 after[i].ingress.comp);
			print_bw("  Total:", after[i].time_us, ingress_tot);
		}
	}

	free(before);
	free(after);
	free(port_ids);
	return 0;
}

#define CMD_DESC_LATENCY "measure the latency of a port"

static int latency(int argc, char **argv)
{
	int ret;
	int cur_ns, max_ns;

	static struct {
		struct switchtec_dev *dev;
		unsigned meas_time;
		int egress;
		int ingress;
	} cfg = {
		.meas_time = 5,
		.egress = -1,
		.ingress = SWITCHTEC_LAT_ALL_INGRESS,
	};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"time", 't', "NUM", CFG_POSITIVE, &cfg.meas_time,
		  required_argument,
		 "measurement time in seconds"},
		{"egress", 'e', "NUM", CFG_NONNEGATIVE, &cfg.egress,
		  required_argument,
		 "physical port ID for the egress side",
		 .require_in_usage=1},
		{"ingress", 'i', "NUM", CFG_NONNEGATIVE, &cfg.ingress,
		  required_argument,
		 "physical port ID for the ingress side (default: use all ports)"},
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_LATENCY, opts, &cfg, sizeof(cfg));

	if (cfg.egress < 0) {
		argconfig_print_usage(opts);
		fprintf(stderr, "The --egress argument is required!\n");
		return 1;
	}

	ret = switchtec_lat_setup(cfg.dev, cfg.egress, cfg.ingress, 1);
	if (ret != 1) {
		switchtec_perror("latency");
		return -1;
	}

	sleep(cfg.meas_time);

	ret = switchtec_lat_get(cfg.dev, 0, cfg.egress, &cur_ns, &max_ns);
	if (ret != 1) {
		switchtec_perror("latency");
		return -1;
	}

	if (switchtec_is_gen3(cfg.dev))
		printf("Current: %d ns\n", cur_ns);
	else
		printf("Minimum: %d ns\n", cur_ns);

	printf("Maximum: %d ns\n", max_ns);

	return 0;
}

struct event_list {
	enum switchtec_event_id eid;
	int partition;
	int port;
	unsigned count;
};

static int compare_event_list(const void *aa, const void *bb)
{
	const struct event_list *a = aa, *b = bb;

	if (a->partition != b->partition)
		return a->partition - b->partition;
	if (a->port != b->port)
		return a->port - b->port;

	return a->eid - b->eid;
}

static void print_event_list(struct event_list *e, size_t cnt)
{
	const char *name, *desc;
	int last_part = -2, last_port = -2;

	while (cnt--) {
		if (e->partition != last_part) {
			if (e->partition == -1)
				printf("Global Events:\n");
			else
				printf("Partition %d Events:\n", e->partition);
		}

		if (e->port != last_port && e->port != -1) {
			if (e->port == SWITCHTEC_PFF_PORT_VEP)
				printf("    Port VEP:\n");
			else
				printf("    Port %d:\n", e->port);
		}

		last_part = e->partition;
		last_port = e->port;

		switchtec_event_info(e->eid, &name, &desc);
		printf("\t%-22s\t%-4u\t%s\n", name, e->count, desc);

		e++;
	}
}

static void populate_event_choices(struct argconfig_choice *c, int mask)
{
	int i;

	for (i = 0; i < SWITCHTEC_MAX_EVENTS; i++) {
		if (mask)
			c->value = 1 << i;
		else
			c->value = i;
		switchtec_event_info(i, &c->name, &c->help);
		c++;
	}
}

static int get_events(struct switchtec_dev *dev,
		      struct switchtec_event_summary *sum,
		      struct event_list *elist, size_t elist_len,
		      int event_id, int show_all, int clear_all,
		      int index)
{
	struct event_list *e = elist;
	enum switchtec_event_type type;
	int flags;
	int idx;
	int ret;
	int local_part;

	local_part = switchtec_partition(dev);

	while (switchtec_event_summary_iter(sum, &e->eid, &idx)) {
		if (e->eid == SWITCHTEC_EVT_INVALID)
			continue;

		type = switchtec_event_info(e->eid, NULL, NULL);

		if (index >= 0 && index != idx)
			continue;

		switch (type)  {
		case SWITCHTEC_EVT_GLOBAL:
			e->partition = -1;
			e->port = -1;
			break;
		case SWITCHTEC_EVT_PART:
			e->partition = idx;
			e->port = -1;
			break;
		case SWITCHTEC_EVT_PFF:
			ret = switchtec_pff_to_port(dev, idx, &e->partition,
						    &e->port);
			if (ret < 0) {
				perror("pff_to_port");
				return -1;
			}
			break;
		}

		if (clear_all || event_id & (1 << e->eid))
			flags = SWITCHTEC_EVT_FLAG_CLEAR;
		else
			flags = 0;

		ret = switchtec_event_ctl(dev, e->eid, idx, flags, NULL);
		if (ret < 0) {
			perror("event_ctl");
			return -1;
		}

		if (!show_all && e->partition != local_part)
			continue;

		e->count = ret;
		e++;
		if (e - elist > elist_len)
			break;
	}

	return e - elist;
}

#define CMD_DESC_EVENTS "display events that have occurred"

static int events(int argc, char **argv)
{
	struct event_list elist[256];
	struct switchtec_event_summary sum;
	struct argconfig_choice event_choices[SWITCHTEC_MAX_EVENTS + 1] = {};
	int ret;

	static struct {
		struct switchtec_dev *dev;
		int show_all;
		int clear_all;
		unsigned event_id;
	} cfg = {};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"all", 'a', "", CFG_NONE, &cfg.show_all, no_argument,
		 "show events in all partitions"},
		{"reset", 'r', "", CFG_NONE, &cfg.clear_all, no_argument,
		 "clear all events"},
		{"event", 'e', "EVENT", CFG_MULT_CHOICES, &cfg.event_id,
		  required_argument, .choices=event_choices,
		  .help="clear all events of a specified type"},
		{NULL}};

	populate_event_choices(event_choices, 1);
	argconfig_parse(argc, argv, CMD_DESC_EVENTS, opts, &cfg, sizeof(cfg));

	ret = switchtec_event_summary(cfg.dev, &sum);
	if (ret < 0) {
		perror("event_summary");
		return ret;
	}

	ret = get_events(cfg.dev, &sum, elist, ARRAY_SIZE(elist), cfg.event_id,
			 cfg.show_all, cfg.clear_all, -1);
	if (ret < 0)
		return ret;

	qsort(elist, ret, sizeof(*elist), compare_event_list);
	print_event_list(elist, ret);

	return 0;
}

#define CMD_DESC_EVENT_WAIT "wait for an event to occur"

static int event_wait(int argc, char **argv)
{
	struct event_list elist[256];
	struct switchtec_event_summary sum = {0};
	struct argconfig_choice event_choices[SWITCHTEC_MAX_EVENTS + 1] = {};
	int index = 0;
	int ret;

	static struct {
		struct switchtec_dev *dev;
		int show_all;
		int partition;
		int port;
		int timeout;
		unsigned event_id;
	} cfg = {
		.partition = -1,
		.port = -1,
		.timeout = -1,
		.event_id = -1,
	};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"event", 'e', "EVENT", CFG_CHOICES, &cfg.event_id,
		  required_argument, .choices=event_choices,
		  .help="event to wait on"},
		{"partition", 'p', "NUM", CFG_NONNEGATIVE, &cfg.partition,
		  required_argument,
		  .help="partition ID for the event"},
		{"port", 'q', "NUM", CFG_NONNEGATIVE, &cfg.port,
		  required_argument,
		  .help="logical port ID for the event"},
		{"timeout", 't', "MS", CFG_INT, &cfg.timeout,
		  required_argument,
		  "timeout in milliseconds (-1 = forever)"},
		{NULL}};

	populate_event_choices(event_choices, 0);
	argconfig_parse(argc, argv, CMD_DESC_EVENT_WAIT, opts, &cfg, sizeof(cfg));

	switch (switchtec_event_info(cfg.event_id, NULL, NULL)) {
	case SWITCHTEC_EVT_GLOBAL:
		break;
	case SWITCHTEC_EVT_PART:
		if (cfg.port >= 0) {
			fprintf(stderr, "Port cannot be specified for this event type.\n");
			return -1;
		}

		if (cfg.partition < 0)
			index = SWITCHTEC_EVT_IDX_ALL;
		else
			index = cfg.partition;

		break;
	case SWITCHTEC_EVT_PFF:
		if (cfg.partition < 0 && cfg.port < 0) {
			index = SWITCHTEC_EVT_IDX_ALL;
		} else if (cfg.partition < 0 || cfg.port < 0) {
			fprintf(stderr, "Must specify partition and port for this event type.\n");
			return -1;
		} else {
			ret = switchtec_port_to_pff(cfg.dev, cfg.partition,
						    cfg.port, &index);
			if (ret) {
				perror("port");
				return ret;
			}
		}
		break;
	default:
		fprintf(stderr, "Must specify event type.\n");
		return -1;
	}

	ret = switchtec_event_wait_for(cfg.dev, cfg.event_id, index, &sum,
				       cfg.timeout);
	if (ret < 0) {
		switchtec_perror("event-wait");
		return ret;
	}

	ret = get_events(cfg.dev, &sum, elist, ARRAY_SIZE(elist),
			 0, 1, 0, index);
	if (ret < 0)
		return ret;

	qsort(elist, ret, sizeof(*elist), compare_event_list);
	print_event_list(elist, ret);

	return 0;
}

#define CMD_DESC_LOG_DUMP "dump the firmware log to a file"

#define LOG_FMT_TXT 0
#define LOG_FMT_BIN 1

static FILE *get_log_def_file(struct switchtec_dev *dev, unsigned type,
			      int format)
{
	int ret;
	FILE *file;

	file = tmpfile();
	if (file == NULL) {
		fprintf(stderr,
			"Cannot open temporary file for log definition data!\n");
		return NULL;
	}

	ret = switchtec_log_def_to_file(dev, SWITCHTEC_LOG_DEF_TYPE_APP,
					file);
	if (ret) {
		switchtec_perror("log_dump");
		return NULL;
	}

	rewind(file);

	return file;
}

static int log_dump(int argc, char **argv)
{
	int ret;
	enum switchtec_boot_phase boot_phase;
	FILE *log_def_to_use;
	struct switchtec_log_file_info info;

	const struct argconfig_choice types[] = {
		{"RAM", SWITCHTEC_LOG_RAM, "dump the app log from RAM"},
		{"FLASH", SWITCHTEC_LOG_FLASH, "dump the app log from flash"},
		{"FTDC", SWITCHTEC_LOG_FTDC, "dump the FTDC firmware log"},
		{"MEMLOG", SWITCHTEC_LOG_MEMLOG,
		 "dump the Memlog info from flash in the last fatal error handling dump"},
		{"REGS", SWITCHTEC_LOG_REGS,
		 "dump the Generic Registers context from flash in the last fatal error handling dump"},
		{"THRD_STACK", SWITCHTEC_LOG_THRD_STACK,
		 "dump the thread stack info from flash in the last fatal error handling dump"},
		{"SYS_STACK", SWITCHTEC_LOG_SYS_STACK,
		 "dump the system stack info from flash in the last fatal error handling dump"},
		{"THRDS", SWITCHTEC_LOG_THRD,
		 "dump all thread info from flash in the last fatal error handling dump"},
		{"NVHDR", SWITCHTEC_LOG_NVHDR,
		 "dump NVLog header information in the last fatal error handling dump"},
		{}
	};
	const struct argconfig_choice format[] = {
		{"BIN", LOG_FMT_BIN, "output binary log data (default)"},
		{"TXT", LOG_FMT_TXT, "output text log data"},
		{}
	};

	static struct {
		struct switchtec_dev *dev;
		int out_fd;
		const char *out_filename;
		unsigned type;
		FILE *log_def_file;
		const char *log_def_filename;
		int format;
	} cfg = {
		.type = SWITCHTEC_LOG_RAM,
		.out_fd = 0,
		.log_def_file = NULL,
		.format = LOG_FMT_BIN
	};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"output_file", .cfg_type=CFG_FD_WR, .value_addr=&cfg.out_fd,
		  .argument_type=optional_positional,
		  .force_default="switchtec.log",
		  .help="log output file"},
		{"log_def", 'd', "DEF_FILE", CFG_FILE_R, &cfg.log_def_file,
		  required_argument,
		  "parse log output using specified log definition file (app log only)"},
		{"type", 't', "TYPE", CFG_CHOICES, &cfg.type,
		  required_argument,
		 "log type to dump", .choices=types},
		{"format", 'f', "FORMAT", CFG_CHOICES, &cfg.format,
		  required_argument,
		 "output log file format", .choices=format},
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_LOG_DUMP, opts, &cfg, sizeof(cfg));

	ret = switchtec_get_device_info(cfg.dev, &boot_phase, NULL, NULL);
	if (ret) {
		switchtec_perror("log_dump");
		return ret;
	}

	if (boot_phase != SWITCHTEC_BOOT_PHASE_FW &&
	    (cfg.type == SWITCHTEC_LOG_RAM ||
	     cfg.type == SWITCHTEC_LOG_FLASH) &&
	    cfg.format == LOG_FMT_TXT &&
	    cfg.log_def_file == NULL) {
		fprintf(stderr, "Cannot generate text format log file in BL1/2 boot phase without\n"
				"a log defintion file. Please provide log definiton file with '-d',\n"
				"or specify binary log format with '-f BIN' instead\n");
		return -1;
	}

	if (cfg.format == LOG_FMT_TXT &&
	    (cfg.type != SWITCHTEC_LOG_RAM &&
	     cfg.type != SWITCHTEC_LOG_FLASH)) {
		fprintf(stderr,
			"INFO: Only BIN format is supported for the given log type,\n"
			"dumping logs in binary format instead.\n");

		cfg.format = LOG_FMT_BIN;
	}

	if (cfg.format == LOG_FMT_BIN) {
		log_def_to_use = NULL;
	} else if (cfg.log_def_file) {
		log_def_to_use = cfg.log_def_file;
	} else {
		log_def_to_use = get_log_def_file(cfg.dev, cfg.type,
						  cfg.format);
		if (!log_def_to_use)
			return ret;
	}

	ret = switchtec_log_to_file(cfg.dev, cfg.type, cfg.out_fd,
				    log_def_to_use, &info);
	if (ret < 0)
		switchtec_perror("log_dump");
	else
		fprintf(stderr, "\nLog saved to %s.\n", cfg.out_filename);

	if (info.version_mismatch) {
		fprintf(stderr, "\nWARNING: The binary log file have different version numbers\n"
				"         from those of the log definition file:\n");
		fprintf(stderr, "\t\tFW Version\tSDK Version\n");
		fprintf(stderr, "Log file:\t0x%08x\t0x%08x\n",
			info.log_fw_version, info.log_sdk_version);
		fprintf(stderr, "Log def file:\t0x%08x\t0x%08x\n\n",
			info.def_fw_version, info.def_sdk_version);
		fprintf(stderr,	"The log file is parsed but the output file might contain errors.\n");
	}

	if (info.overflow)
		fprintf(stderr, "\nWARNING: The log buffer pointer has wrapped. The log data may be incomplete!\n");

	if (cfg.out_fd > 0)
		close(cfg.out_fd);

	if (log_def_to_use)
		fclose(log_def_to_use);

	return ret;
}

#define CMD_DESC_LOG_PARSE "parse a binary app log or mailbox log to a text file"

static int log_parse(int argc, char **argv)
{
	int ret;
	struct switchtec_log_file_info info;
	const struct argconfig_choice log_types[] = {
		{"APP", SWITCHTEC_LOG_PARSE_TYPE_APP, "app log"},
		{"MAILBOX", SWITCHTEC_LOG_PARSE_TYPE_MAILBOX, "mailbox log"},
		{"FTDC", SWITCHTEC_LOG_PARSE_TYPE_FTDC, "ftdc"},
		{}
	};
	const struct argconfig_choice device_gen[] = {
		{"GEN3", SWITCHTEC_GEN3, "GEN3"},
		{"GEN4", SWITCHTEC_GEN4, "GEN4"},
		{"GEN5", SWITCHTEC_GEN5, "GEN5"},
		{"UNKNOWN", SWITCHTEC_GEN_UNKNOWN, "UNKNOWN"},
		{}
	};

	static struct {
		enum switchtec_log_parse_type log_type;
		FILE *bin_log_file;
		const char *bin_log_filename;
		FILE *log_def_file;
		const char *log_def_filename;
		FILE *parsed_log_file;
		const char *parsed_log_filename;
		enum switchtec_gen gen;
	} cfg = {
		.log_type = SWITCHTEC_LOG_PARSE_TYPE_APP,
		.bin_log_file = NULL,
		.log_def_file = NULL,
		.parsed_log_file = NULL,
		.gen = SWITCHTEC_GEN_UNKNOWN
	};
	const struct argconfig_options opts[] = {
		{"type", 't',
		 .meta = "TYPE", .cfg_type = CFG_CHOICES,
		 .value_addr = &cfg.log_type,
		 .argument_type = required_argument,
		 .help = "log type to parse (default: APP)",
		 .choices = log_types},
		{"device_gen", 'g',
		 .meta = "GEN", .cfg_type = CFG_CHOICES,
		 .value_addr = &cfg.gen,
		 .argument_type = required_argument,
		 .help = "device generation (Only needed when parsing "
			 "earlier log files which do not contain device "
			 "generation information. Default: UNKNOWN)",
		 .choices = device_gen},
		{"log_input", .cfg_type = CFG_FILE_R,
		 .value_addr = &cfg.bin_log_file,
		 .argument_type = required_positional,
		 .help = "binary app log input file"},
		{"log_def", .cfg_type = CFG_FILE_R,
		 .value_addr = &cfg.log_def_file,
		 .argument_type = required_positional,
		 .help = "log definition file"},
		{"parsed_output", .cfg_type = CFG_FILE_W,
		 .value_addr = &cfg.parsed_log_file,
		 .argument_type = optional_positional,
		 .force_default = "log.txt",
		 .help = "parsed output file"},
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_LOG_PARSE, opts,
			&cfg, sizeof(cfg));

	fseek(cfg.bin_log_file, 0, SEEK_END);
	if (ftell(cfg.bin_log_file) == 0) {
		fprintf(stderr, "\nLog file %s is empty!\n",
			cfg.bin_log_filename);

		ret = -1;
		goto done;
	}
	fseek(cfg.bin_log_file, 0, SEEK_SET);

	ret = switchtec_parse_log(cfg.bin_log_file, cfg.log_def_file,
				  cfg.parsed_log_file, cfg.log_type,
				  cfg.gen, &info);
	if (ret < 0)
		switchtec_perror("log_parse");
	else
		fprintf(stderr, "\nParsed log saved to %s.\n",
			cfg.parsed_log_filename);

	if (info.version_mismatch && cfg.log_type != SWITCHTEC_LOG_PARSE_TYPE_FTDC) {
		fprintf(stderr, "\nWARNING: The two input files have different version numbers.\n");
		fprintf(stderr, "\t\tFW Version\tSDK Version\n");
		fprintf(stderr, "Log file:\t0x%08x\t0x%08x\n",
			info.log_fw_version, info.log_sdk_version);
		fprintf(stderr, "Log def file:\t0x%08x\t0x%08x\n\n",
			info.def_fw_version, info.def_sdk_version);
		fprintf(stderr,	"The log file is parsed but the output file might contain errors.\n");
	}

	if (info.gen_unknown) {
		fprintf(stderr, "\nWARNING: There is no device Generation information in the log file.\n");
		fprintf(stderr, "           The log file is parsed but the output file contains errors.\n");
		fprintf(stderr, "Hint: Use '-g' option to specify device generation.\n");
	}

	if (info.gen_ignored) {
		fprintf(stderr, "\nNOTE: The input log file contains device generation information,\n");
		fprintf(stderr, "        therefore the generation option in the command line is ignored.\n");
	}

done:
	if (cfg.bin_log_file != NULL)
		fclose(cfg.bin_log_file);

	if (cfg.log_def_file != NULL)
		fclose(cfg.log_def_file);

	if (cfg.parsed_log_file != NULL)
		fclose(cfg.parsed_log_file);

	return ret;
}

#define CMD_DESC_TEST "test if the Switchtec interface is working"

static int test(int argc, char **argv)
{
	int ret;
	uint32_t in, out;

	static struct {
		struct switchtec_dev *dev;
	} cfg = {};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_TEST, opts, &cfg, sizeof(cfg));

	in = time(NULL);

	ret = switchtec_echo(cfg.dev, in, &out);

	if (ret) {
		switchtec_perror(argv[0]);
		return ret;
	}

	if (in != ~out) {
		fprintf(stderr, "%s: echo command returned the "
			"wrong result; got %x, expected %x\n",
			argv[0], out, ~in);
		return 1;
	}

	fprintf(stderr, "%s: success\n", argv[0]);

	return 0;
}

#define CMD_DESC_TEMP "display the die temperature"

static int temp(int argc, char **argv)
{
	float ret;
	int nr_reading;
	float temps[4];

	static struct {
		struct switchtec_dev *dev;
		int verbose;
	} cfg = {};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"verbose", 'v', "", CFG_NONE, &cfg.verbose, no_argument,
		 "print individual die temperature sensor reading"},
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_TEMP, opts, &cfg, sizeof(cfg));

	if (!cfg.verbose) {
		ret = switchtec_die_temp(cfg.dev);
		if (ret < 0) {
			switchtec_perror("die_temp");
			return 1;
		}

		if (have_decent_term())
			printf("%.3g °C\n", ret);
		else
			printf("%.3g degC\n", ret);
	} else {
		int i;
		nr_reading = switchtec_die_temps(cfg.dev, 4, temps);
		if (nr_reading < 0) {
			switchtec_perror("die_temp");
			return 1;
		}

		for (i = 0; i < nr_reading; i++) {
			if (have_decent_term())
				printf("Sensor %d: %.3g °C\n", i, temps[i]);
			else
				printf("Sensor %d: %.3g degC\n", i, temps[i]);
		}
	}

	return 0;
}

static void print_bind_info(struct switchtec_bind_status_out status)
{
	int i;

	for (i = 0; i < status.inf_cnt; i++) {
		enum switchtec_bind_info_result result = status.port_info[i].bind_state & 0x0F;
		int state = (status.port_info[i].bind_state & 0xF0) >> 4;

		switch (result) {
		case BIND_INFO_SUCCESS:
			printf("bind state: %s\n", state ? "Bound" : "Unbound");
			if (state)
				printf("physical port %u bound to %u, partition %u\n",
				       status.port_info[i].phys_port_id,
				       status.port_info[i].log_port_id,
				       status.port_info[i].par_id);
			else
				printf("physical port %u\n",
				       status.port_info[i].phys_port_id);
			break;
		case BIND_INFO_FAIL:
			printf("bind_info: Fail\n");
			printf("physical port %u\n",
			       status.port_info[i].phys_port_id);
			break;
		case BIND_INFO_IN_PROGRESS:
			printf("bind_info: In Progress\n");
			printf("physical port %u\n",
			       status.port_info[i].phys_port_id);
			break;
		}
	}
}

#define CMD_DESC_PORT_BIND_INFO "display physical port binding information"

static int port_bind_info(int argc, char **argv)
{
	int ret;
	struct switchtec_bind_status_out bind_status;
	static struct {
		struct switchtec_dev *dev;
		int phy_port;
	} cfg = {
		.phy_port = 0xff
	};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"physical", 'f', "", CFG_NONNEGATIVE, &cfg.phy_port, required_argument,
			"physical port ID"},
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_PORT_BIND_INFO, opts, &cfg, sizeof(cfg));

	if (cfg.phy_port == 0xff)
		printf("physical port: all\n");

	ret = switchtec_bind_info(cfg.dev, &bind_status, cfg.phy_port);

	if (ret != 0) {
		switchtec_perror("port_bind_info");
		return 1;
	}

	print_bind_info(bind_status);
	return 0;
}

#define CMD_DESC_PORT_BIND "bind a logical port to a physical port"

static int port_bind(int argc, char **argv)
{
	int ret;
	static struct {
		struct switchtec_dev *dev;
		int par_id;
		int log_port;
		int phy_port;
	} cfg = {};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"partition", 'p', "", CFG_NONNEGATIVE, &cfg.par_id, required_argument,
			"partition ID"},
		{"logical", 'l', "", CFG_POSITIVE, &cfg.log_port, required_argument,
			"logical port ID"},
		{"physical", 'f', "", CFG_NONNEGATIVE, &cfg.phy_port, required_argument,
			"physical port ID"},
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_PORT_BIND, opts, &cfg, sizeof(cfg));

	ret = switchtec_bind(cfg.dev, cfg.par_id, cfg.log_port, cfg.phy_port);

	if (ret != 0) {
		switchtec_perror("port_bind");
		return 1;
	}

	return 0;
}

#define CMD_DESC_PORT_UNBIND "unbind a logical port from a physical port"

static int port_unbind(int argc, char **argv)
{
	int ret;
	static struct {
		struct switchtec_dev *dev;
		int par_id;
		int log_port;
	} cfg = {};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"partition", 'p', "", CFG_NONNEGATIVE, &cfg.par_id, required_argument,
			"partition ID"},
		{"logical", 'l', "", CFG_POSITIVE, &cfg.log_port, required_argument,
			"logical port ID"},
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_PORT_UNBIND, opts, &cfg, sizeof(cfg));

	ret = switchtec_unbind(cfg.dev, cfg.par_id, cfg.log_port);

	if (ret != 0) {
		switchtec_perror("port_unbind");
		return 1;
	}

	return 0;
}

int ask_if_sure(int always_yes)
{
	char buf[10];
	char *ret;

	if (always_yes)
		return 0;

	fprintf(stderr, "Do you want to continue? [y/N] ");
	fflush(stderr);
	ret = fgets(buf, sizeof(buf), stdin);

	if (!ret)
		goto abort;

	if (strcmp(buf, "y\n") == 0 || strcmp(buf, "Y\n") == 0)
		return 0;

abort:
	fprintf(stderr, "Abort.\n");
	errno = EINTR;
	return -errno;
}

static void stack_bif_print(struct switchtec_dev *dev, int stack_id,
			    int ports[SWITCHTEC_PORTS_PER_STACK])
{
	int i, p;

	for (i = 0; i < SWITCHTEC_PORTS_PER_STACK;
	     i += switchtec_stack_bif_width(dev, stack_id, ports[i])) {
		p = stack_id * SWITCHTEC_PORTS_PER_STACK + i;

		if (ports[i] < 0)
			break;
		else if (!switchtec_stack_bif_port_valid(dev, stack_id, i))
			break;
		else if (!ports[i])
			printf("  Port %2d:  disabled\n", p);
		else
			printf("  Port %2d:  x%d\n", p, ports[i]);
	}
}

static int stack_bif_get_print(struct switchtec_dev *dev, int stack_id,
			       bool skip_bad_stack)
{
	int ports[SWITCHTEC_PORTS_PER_STACK];
	int ret;

	ret = switchtec_get_stack_bif(dev, stack_id, ports);
	if (ret) {
		if (skip_bad_stack &&
		    errno == (SWITCHTEC_ERRNO_MRPC_FLAG_BIT |
			      ERR_STACKBIF_STACK_ID_INVALID)) {
			return 0;
		}

		switchtec_perror("get_stack_bifurcation");
		return 1;
	}

	printf("Stack %d:\n", stack_id);
	stack_bif_print(dev, stack_id, ports);

	return 0;
}

static int stack_bif_set(struct switchtec_dev *dev, int stack_id,
			 int ports[SWITCHTEC_PORTS_PER_STACK], int assume_yes)
{
	int p[SWITCHTEC_PORTS_PER_STACK] = {};
	int i, w, nports = 0;
	int ret;

	if (stack_id < 0) {
		fprintf(stderr,
			"Must specify --stack_id/-s when setting bifurcation\n");
		return 1;
	}

	for (i = 0; i < SWITCHTEC_PORTS_PER_STACK; i++) {
		if (ports[i] == -1)
			break;

		w = switchtec_stack_bif_width(dev, stack_id, ports[i]);
		if (w < 0) {
			fprintf(stderr, "Invalid bifurcation value: %d\n",
				ports[i]);
			return 1;
		}

		if (nports & (w - 1))
			nports += w - (nports & (w - 1));

		if (!switchtec_stack_bif_port_valid(dev, stack_id, nports))
			goto invalid_lanes;

		p[nports] = ports[i];

		nports += w;

		if (nports > SWITCHTEC_PORTS_PER_STACK) {
invalid_lanes:
			fprintf(stderr,
				"Too many lanes specified in the bifurcation\n");
			return 1;
		}
	}

	printf("Set Stack %d to:\n", stack_id);
	stack_bif_print(dev, stack_id, p);
	if (ask_if_sure(assume_yes))
		return 1;

	ret = switchtec_set_stack_bif(dev, stack_id, p);
	if (ret) {
		switchtec_perror("set_stack_bifurcation");
		return 1;
	}

	return 0;
}

#define CMD_DESC_STACK_BIF "dynamically get and set the bifurcation in a stack"
#define CMD_DESC_STACK_BIF_LONG "dynamically get and set the bifurcation in a stack\n\
\n\
To set the bifurcation of a stack specify the stack_id with -s and specify\n\
a series of bifurcation numbers (1, 2, 4, 8, 16) for each consecutive\n\
port. Specify 0 to disable a port. For example:\n\
\n\
  switchtec stack-bif <device> -s <stack_id> 16\n\
\n\
will bifurcate the entire stack into a x16 port.\n\
\n\
  switchtec stack-bif <device> -s <stack_id> 4 0 8\n\
\n\
will bifurcate the first port in the stack into a x4, disable the next\n\
4 lanes and the last port will be a x8."

static int stack_bif(int argc, char **argv)
{
	static struct {
		struct switchtec_dev *dev;
		int stack_id;
		int assume_yes;
		int ports[SWITCHTEC_PORTS_PER_STACK];
	} cfg = {
		.stack_id = -1,
		.ports[0 ... SWITCHTEC_PORTS_PER_STACK - 1] = -1,
	};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"stack_id", 's', "", CFG_NONNEGATIVE, &cfg.stack_id,
		 required_argument, "stack ID"},
		{"yes", 'y', "", CFG_NONE, &cfg.assume_yes, no_argument,
		 "assume yes when prompted"},
		{"port0", .cfg_type=CFG_NONNEGATIVE, .value_addr=&cfg.ports[0],
		  .argument_type=optional_positional,
		  .help="set first port bifurcation width"},
		{"port1", .cfg_type=CFG_NONNEGATIVE, .value_addr=&cfg.ports[1],
		  .argument_type=optional_positional,
		  .help="set second port bifurcation width"},
		{"port2", .cfg_type=CFG_NONNEGATIVE, .value_addr=&cfg.ports[2],
		  .argument_type=optional_positional,
		  .help="set third port bifurcation width"},
		{"port3", .cfg_type=CFG_NONNEGATIVE, .value_addr=&cfg.ports[3],
		  .argument_type=optional_positional,
		  .help="set forth port bifurcation width"},
		{"port4", .cfg_type=CFG_NONNEGATIVE, .value_addr=&cfg.ports[4],
		  .argument_type=optional_positional,
		  .help="set fifth port bifurcation width"},
		{"port5", .cfg_type=CFG_NONNEGATIVE, .value_addr=&cfg.ports[5],
		  .argument_type=optional_positional,
		  .help="set sixth port bifurcation width"},
		{"port6", .cfg_type=CFG_NONNEGATIVE, .value_addr=&cfg.ports[6],
		  .argument_type=optional_positional,
		  .help="set seventh port bifurcation width"},
		{"port7", .cfg_type=CFG_NONNEGATIVE, .value_addr=&cfg.ports[7],
		  .argument_type=optional_positional,
		  .help="set last port bifurcation width"},
		{NULL}};
	int ret, i;

	argconfig_parse(argc, argv, CMD_DESC_STACK_BIF_LONG, opts, &cfg,
			sizeof(cfg));

	if (cfg.ports[0] >= 0)
		return stack_bif_set(cfg.dev, cfg.stack_id, cfg.ports,
				     cfg.assume_yes);

	if (cfg.stack_id < 0) {
		for (i = 0; i < SWITCHTEC_MAX_STACKS; i++) {
			ret = stack_bif_get_print(cfg.dev, i, true);
			if (ret)
				return 1;
		}
	} else {
		ret = stack_bif_get_print(cfg.dev, cfg.stack_id, false);
		if (ret)
			return 1;
	}

	return 0;
}

#define CMD_DESC_HARD_RESET "perform a hard reset of the switch"

static int hard_reset(int argc, char **argv)
{
	int ret;

	static struct {
		struct switchtec_dev *dev;
		int assume_yes;
	} cfg = {};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"yes", 'y', "", CFG_NONE, &cfg.assume_yes, no_argument,
		 "assume yes when prompted"},
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_HARD_RESET, opts, &cfg, sizeof(cfg));

	if (!cfg.assume_yes)
		fprintf(stderr,
			"WARNING: if your system does not support hotplug,\n"
			"a hard reset can leave the system in a broken state.\n"
			"Make sure you reboot after issuing this command.\n\n");

	ret = ask_if_sure(cfg.assume_yes);
	if (ret)
		return ret;

	ret = switchtec_hard_reset(cfg.dev);
	if (ret) {
		switchtec_perror(argv[0]);
		return ret;
	}

	fprintf(stderr, "%s: hard reset\n", switchtec_name(cfg.dev));
	return 0;
}

static const char *get_basename(const char *buf)
{
	const char *slash = strrchr(buf, '/');

	if (slash)
		return slash+1;

	return buf;
}

enum switchtec_fw_type check_and_print_fw_image(int img_fd,
						const char *img_filename)
{
	int ret;
	struct switchtec_fw_image_info info;
	ret = switchtec_fw_file_info(img_fd, &info);

	if (ret < 0) {
		fprintf(stderr, "%s: Invalid image file format\n",
			img_filename);
		return ret;
	}

	printf("File:           %s\n", get_basename(img_filename));
	printf("Gen:            %s\n", switchtec_fw_image_gen_str(&info));
	printf("Type:           %s\n", switchtec_fw_image_type(&info));
	printf("Version:        %s\n", info.version);
	printf("Img Len:        0x%zx\n", info.image_len);
	printf("CRC:            0x%08lx\n", info.image_crc);
	if (info.gen != SWITCHTEC_GEN3)
		printf("Secure version: 0x%08lx\n", info.secure_version);

	return info.type;
}

#define CMD_DESC_FW_IMG_INFO "display information for a firmware image"

static int fw_img_info(int argc, char **argv)
{
	int ret;

	static struct {
		int img_fd;
		const char *img_filename;
	} cfg = {0};
	const struct argconfig_options opts[] = {
		{"img_file", .cfg_type=CFG_FD_RD, .value_addr=&cfg.img_fd,
		  .argument_type=required_positional,
		  .help="image file to display information for"},
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_FW_IMG_INFO, opts, &cfg, sizeof(cfg));

	ret = check_and_print_fw_image(cfg.img_fd, cfg.img_filename);
	if (ret < 0)
		return ret;

	close(cfg.img_fd);
	return 0;
}

static const char *fw_active_string(struct switchtec_fw_image_info *inf)
{
	return inf->active ? " - Active" : "";
}

static void print_fw_part_line(const char *tag,
			       struct switchtec_fw_image_info *inf)
{
	if (!inf)
		return;

	printf("  %-4s\tVersion: %-8s\tCRC: %08lx\t%4s%11s%s\n",
	       tag, inf->version, inf->image_crc,
	       inf->read_only ? "(RO)" : "",
	       inf->running ? "  (Running)" : "",
	       inf->valid ? "" : "  (Invalid)");
}

static int print_fw_part_info(struct switchtec_dev *dev)
{
	struct switchtec_fw_part_summary *sum;
	struct switchtec_fw_image_info *inf;
	int i;

	sum = switchtec_fw_part_summary(dev);
	if (!sum)
		return -1;

	printf("Active Partitions:\n");
	print_fw_part_line("BOOT", sum->boot.active);
	print_fw_part_line("MAP", sum->map.active);
	print_fw_part_line("KEY", sum->key.active);
	print_fw_part_line("BL2", sum->bl2.active);
	print_fw_part_line("IMG", sum->img.active);
	print_fw_part_line("CFG", sum->cfg.active);

	for (i = 0, inf = sum->mult_cfg; inf; i++, inf = inf->next)
		printf("   \tMulti Config %d%s\n", i, fw_active_string(inf));

	printf("Inactive Partitions:\n");
	print_fw_part_line("MAP", sum->map.inactive);
	print_fw_part_line("KEY", sum->key.inactive);
	print_fw_part_line("BL2", sum->bl2.inactive);
	print_fw_part_line("IMG", sum->img.inactive);
	print_fw_part_line("CFG", sum->cfg.inactive);

	printf("Other Partitions:\n");
	print_fw_part_line("SEE", sum->seeprom.active);

	switchtec_fw_part_summary_free(sum);

	return 0;
}

#define CMD_DESC_FW_INFO "return information on the currently flashed firmware (BL2, Main Firmware)"

static int fw_info(int argc, char **argv)
{
	int ret;
	char version[64];
	enum switchtec_boot_phase phase_id;

	static struct {
		struct switchtec_dev *dev;
	} cfg = {};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_FW_INFO, opts, &cfg, sizeof(cfg));

	phase_id = switchtec_boot_phase(cfg.dev);
	if (phase_id == SWITCHTEC_BOOT_PHASE_BL1) {
		fprintf(stderr,
			"This command is only available in BL2 or Main Firmware!\n");
		return -1;
	}
	if (phase_id == SWITCHTEC_BOOT_PHASE_FW) {
		ret = switchtec_get_fw_version(cfg.dev, version,
					       sizeof(version));
		if (ret < 0) {
			switchtec_perror("print fw info");
			return ret;
		}

		printf("Currently Running:\n");
		printf("  IMG\tVersion: %s\n", version);
	}
	ret = print_fw_part_info(cfg.dev);
	if (ret) {
		switchtec_perror("print fw info");
		return ret;
	}


	return 0;
}

#define CMD_DESC_FW_UPDATE "upload a new firmware image to flash (BL2, Main Firmware)"

static int fw_update(int argc, char **argv)
{
	int ret;
	int type;
	struct switchtec_fw_image_info info;
	const char *desc = CMD_DESC_FW_UPDATE "\n\n"
			   "This command only supports flashing firmware "
			   "when the device is in the BL2 or MAIN boot phase. To "
			   "transfer an image in the BL1 boot phase, use the "
			   "'mfg fw-transfer' command instead.\n\n"
			   BOOT_PHASE_HELP_TEXT;
	static struct {
		struct switchtec_dev *dev;
		FILE *fimg;
		const char *img_filename;
		int assume_yes;
		int dont_activate;
		int force;
		int set_boot_rw;
		int no_progress_bar;
	} cfg = {};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"img_file", .cfg_type=CFG_FILE_R, .value_addr=&cfg.fimg,
		  .argument_type=required_positional,
		  .help="image file to upload"},
		{"yes", 'y', "", CFG_NONE, &cfg.assume_yes, no_argument,
		 "assume yes when prompted"},
		{"dont-activate", 'A', "", CFG_NONE, &cfg.dont_activate, no_argument,
		 "don't activate the new image, use fw-toggle to do so "
		 "when it is safe"},
		{"force", 'f', "", CFG_NONE, &cfg.force, no_argument,
		 "force interrupting an existing fw-update command in case "
		 "firmware is stuck in a busy state"},
		{"set-boot-rw", 'W', "", CFG_NONE, &cfg.set_boot_rw, no_argument,
		 "set the bootloader and map partition as RW (only valid for BOOT and MAP images)"},
		{"no-progress", 'p', "", CFG_NONE, &cfg.no_progress_bar, no_argument,
		"don't print progress to stdout"},
		{NULL}};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));

	printf("Writing the following firmware image to %s.\n",
	       switchtec_name(cfg.dev));

	type = check_and_print_fw_image(fileno(cfg.fimg), cfg.img_filename);
	if (type < 0)
		return type;

	if (switchtec_boot_phase(cfg.dev) == SWITCHTEC_BOOT_PHASE_BL1) {
		fprintf(stderr,
			"This command is only available in BL2 or Main Firmware!\n");
		fprintf(stderr,
			"Use 'mfg fw-transfer' instead to transfer a BL2 image.\n");
		return -1;
	}

	switchtec_fw_file_info(fileno(cfg.fimg), &info);
	if (switchtec_gen(cfg.dev) != info.gen) {
		fprintf(stderr,
			"\nThe image is for %s devices and cannot be applied to this device!\n",
			switchtec_fw_image_gen_str(&info));
		return -1;
	}

	ret = ask_if_sure(cfg.assume_yes);
	if (ret) {
		fclose(cfg.fimg);
		return ret;
	}

	if (cfg.set_boot_rw && type != SWITCHTEC_FW_TYPE_BOOT &&
	    type != SWITCHTEC_FW_TYPE_MAP) {
		fprintf(stderr, "The --set-boot-rw option only applies for BOOT and MAP images\n");
		return -1;
	} else if (type == SWITCHTEC_FW_TYPE_BOOT ||
		   type == SWITCHTEC_FW_TYPE_MAP) {
		if (cfg.set_boot_rw)
			switchtec_fw_set_boot_ro(cfg.dev, SWITCHTEC_FW_RW);

		if (switchtec_fw_is_boot_ro(cfg.dev) == SWITCHTEC_FW_RO) {
			fprintf(stderr, "\nfirmware update: the BOOT and MAP partition are read-only. "
				"use --set-boot-rw to override\n");
			return -1;
		}
	}

	if(switchtec_fw_file_secure_version_newer(cfg.dev, fileno(cfg.fimg))) {
		fprintf(stderr, "\n\nWARNING:\n"
			"Updating this image will IRREVERSIBLY update device %s image\n"
			"secure version to 0x%08lx!\n\n",
			switchtec_fw_image_type(&info),
			info.secure_version);

		ret = ask_if_sure(cfg.assume_yes);
		if (ret) {
			fclose(cfg.fimg);
			return ret;
		}
	}

	progress_start();
	if (cfg.no_progress_bar)
		ret = switchtec_fw_write_file(cfg.dev, cfg.fimg, cfg.dont_activate,
					      cfg.force, NULL);
	else
		ret = switchtec_fw_write_file(cfg.dev, cfg.fimg, cfg.dont_activate,
					      cfg.force, progress_update);
	fclose(cfg.fimg);

	if (ret) {
		printf("\n");
		switchtec_fw_perror("firmware update", ret);
		goto set_boot_ro;
	}

	progress_finish(cfg.no_progress_bar);
	printf("\n");

	print_fw_part_info(cfg.dev);
	printf("\n");

	if (type == SWITCHTEC_FW_TYPE_MAP) {
		printf("\nNOTE: Device partition map has been updated! All other partitions\n"
		       "(BL2, Config and Main Image) MUST BE UPDATED to ensure your device can boot properly!\n");
	}

	if (switchtec_boot_phase(cfg.dev) == SWITCHTEC_BOOT_PHASE_BL2 &&
	    !cfg.dont_activate) {
		printf("\nNOTE: This command does not automatically activate the image when used in the BL2 boot phase.\n"
		       "Be sure to use 'fw-toggle' after this command to activate the updated image.\n");
	}
set_boot_ro:
	if (cfg.set_boot_rw)
		switchtec_fw_set_boot_ro(cfg.dev, SWITCHTEC_FW_RO);

	return ret;
}

#define CMD_DESC_FW_TOGGLE "toggle the active and inactive firmware partitions (BL2, Main Firmware)"

static int fw_toggle(int argc, char **argv)
{
	int ret = 0;
	int err = 0;

	static struct {
		struct switchtec_dev *dev;
		int bl2;
		int key;
		int firmware;
		int config;
	} cfg = {};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"bl2", 'b', "", CFG_NONE, &cfg.bl2, no_argument,
			"toggle BL2 firmware"},
		{"key", 'k', "", CFG_NONE, &cfg.key, no_argument,
			"toggle Key manifest"},
		{"firmware", 'f', "", CFG_NONE, &cfg.firmware, no_argument,
		 "toggle IMG firmware"},
		{"config", 'c', "", CFG_NONE, &cfg.config, no_argument,
		 "toggle CFG data"},
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_FW_TOGGLE, opts, &cfg, sizeof(cfg));

	if (!cfg.bl2 && !cfg.key && !cfg.firmware && !cfg.config) {
		fprintf(stderr, "NOTE: Not toggling images as no "
			"partition type options were specified\n\n");
	} else if ((cfg.bl2 || cfg.key) && switchtec_is_gen3(cfg.dev)) {
		fprintf(stderr, "Firmware type BL2 and Key manifest"
			"are not supported by Gen3 switches\n");
		return 1;
	} else {
		ret = switchtec_fw_toggle_active_partition(cfg.dev,
							   cfg.bl2,
							   cfg.key,
							   cfg.firmware,
							   cfg.config);
		if (ret)
			err = errno;
	}

	ret = print_fw_part_info(cfg.dev);
	if (ret)
		switchtec_perror("print fw info");

	printf("\n");

	errno = err;
	if (errno)
		switchtec_perror("firmware toggle");
	else
		printf("firmware toggle: Success\n");

	return ret;
}

#define CMD_DESC_FW_READ "read a firmware image from flash"

static int fw_read(int argc, char **argv)
{
	struct switchtec_fw_part_summary *sum;
	struct switchtec_fw_image_info *inf;
	int ret = 0;

	static struct {
		struct switchtec_dev *dev;
		int out_fd;
		char *out_filename;
		int assume_yes;
		int inactive;
		int data;
		int bl2;
		int key;
		int no_progress_bar;
	} cfg = {
		.out_fd = -1
	};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"filename", .cfg_type=CFG_FD_WR, .value_addr=&cfg.out_fd,
		  .argument_type=optional_positional,
		  .help="image output file"},
		{"yes", 'y', "", CFG_NONE, &cfg.assume_yes, no_argument,
		 "assume yes when prompted"},
		{"inactive", 'i', "", CFG_NONE, &cfg.inactive, no_argument,
		 "read the inactive partition"},
		{"data", 'd', "", CFG_NONE, &cfg.data, no_argument,
		 "read the data/config partiton instead of the main firmware"},
		{"config", 'c', "", CFG_NONE, &cfg.data, no_argument,
		 "read the data/config partiton instead of the main firmware"},
		{"bl2", 'b', "", CFG_NONE, &cfg.bl2, no_argument,
		 "read the BL2 partiton instead of the main firmware"},
		{"key", 'k', "", CFG_NONE, &cfg.key, no_argument,
		 "read the key manifest partiton instead of the main firmware"},
		{"no-progress", 'p', "", CFG_NONE, &cfg.no_progress_bar, no_argument,
		"don't print progress to stdout"},
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_FW_READ, opts, &cfg, sizeof(cfg));

	if(cfg.out_fd == -1) {
		if (switchtec_is_gen3(cfg.dev))
			cfg.out_filename = "image.pmc";
		else
			cfg.out_filename = "image.fwimg";

		cfg.out_fd = fileno(fopen(cfg.out_filename, "w"));
	}

	sum = switchtec_fw_part_summary(cfg.dev);
	if (!sum) {
		switchtec_perror("fw_part_summary");
		goto close_and_exit;
	}

	if (cfg.data)
		inf = cfg.inactive ? sum->cfg.inactive : sum->cfg.active;
	else if (cfg.bl2)
		inf = cfg.inactive ? sum->bl2.inactive : sum->bl2.active;
	else if (cfg.key)
		inf = cfg.inactive ? sum->key.inactive : sum->key.active;
	else
		inf = cfg.inactive ? sum->img.inactive : sum->img.active;

	if (!inf) {
		fprintf(stderr,
			"The specified partition on the flash is empty!\n");
		ret = -1;
		goto close_and_exit;
	}

	if (inf->valid) {
		fprintf(stderr, "Version:  %s\n", inf->version);
		fprintf(stderr, "Type:     %s\n",
			cfg.data ? "DAT" : cfg.bl2? "BL2" :
			cfg.key? "KEY" : "IMG");
		fprintf(stderr, "Img Len:  0x%x\n", (int)inf->image_len);
		fprintf(stderr, "CRC:      0x%x\n", (int)inf->image_crc);
	}

	if (!inf->valid && !cfg.assume_yes) {
		fprintf(stderr,
			"\nWARNING: The firmware image for this partition is INVALID!\n");

		ret = ask_if_sure(cfg.assume_yes);
		if (ret) {
			close(cfg.out_fd);
			return ret;
		}
	}

	ret = switchtec_fw_img_write_hdr(cfg.out_fd, inf);
	if (ret < 0) {
		switchtec_perror(cfg.out_filename);
		goto close_and_exit;
	}

	progress_start();
	if (cfg.no_progress_bar)
		ret = switchtec_fw_body_read_fd(cfg.dev, cfg.out_fd,
						inf, NULL);
	else
		ret = switchtec_fw_body_read_fd(cfg.dev, cfg.out_fd,
						inf, progress_update);
	progress_finish(cfg.no_progress_bar);

	if (ret < 0)
		switchtec_perror("fw_read");
	else
		ret = 0;

	fprintf(stderr, "\nFirmware read to %s.\n", cfg.out_filename);

	switchtec_fw_part_summary_free(sum);

close_and_exit:
	close(cfg.out_fd);

	return ret;
}

static void create_type_choices(struct argconfig_choice *c)
{
	const struct switchtec_evcntr_type_list *t;

	for (t = switchtec_evcntr_type_list; t->name; t++, c++) {
		c->name = t->name;
		c->value = t->mask;
		c->help = t->help;
	}

	c->name = NULL;
	c->value = 0;
	c->help = NULL;
}

static char *type_mask_to_string(int type_mask, char *buf, ssize_t buflen)
{
	int w;
	char *ret = buf;

	while (type_mask) {
		const char *str = switchtec_evcntr_type_str(&type_mask);
		if (str == NULL)
			break;

		w = snprintf(buf, buflen, "%s,", str);
		buf += w;
		buflen -= w;
		if (buflen < 0)
			return ret;
	}

	buf[-1] = 0;

	return ret;
}

static char *port_mask_to_string(unsigned port_mask, char *buf, size_t buflen)
{
	int i, range=-1;
	int w;
	char *ret = buf;

	port_mask &= 0xFF;

	if (port_mask == 0xFF) {
		snprintf(buf, buflen, "ALL");
		return buf;
	}

	for (i = 0; port_mask; port_mask = port_mask >> 1, i++) {
		if (port_mask & 1 && range < 0) {
			w = snprintf(buf, buflen, "%d,", i);
			buf += w;
			buflen -=w;
			range = i;
		} else if (!(port_mask & 1)) {
			if (range >= 0 && range < i-1) {
				buf--;
				buflen++;
				w = snprintf(buf, buflen, "-%d,", i-1);
				buf += w;
				buflen -=w;
			}
			range = -1;
		}
	}

	if (range >= 0 && range < i-1) {
		buf--;
		buflen++;
		w = snprintf(buf, buflen, "-%d,", i-1);
		buf += w;
		buflen -=w;
	}


	buf[-1] = 0;

	return ret;
}


static int display_event_counters(struct switchtec_dev *dev, int stack,
				  int reset)
{
	int ret, i;
	struct switchtec_evcntr_setup setups[SWITCHTEC_MAX_EVENT_COUNTERS];
	unsigned counts[SWITCHTEC_MAX_EVENT_COUNTERS];
	char buf[1024];
	int count = 0;

	ret = switchtec_evcntr_get_both(dev, stack, 0, ARRAY_SIZE(setups),
					setups, counts, reset);

	if (ret < 0)
		return ret;

	printf("Stack %d:\n", stack);

	for (i = 0; i < ret; i ++) {
		if (!setups[i].port_mask || !setups[i].type_mask)
			continue;

		port_mask_to_string(setups[i].port_mask, buf, sizeof(buf));
		printf("   %2d - %-11s", i, buf);

		type_mask_to_string(setups[i].type_mask, buf, sizeof(buf));
		if (strlen(buf) > 39)
			strcpy(buf, "MANY");

		printf("%-40s   %10u\n", buf, counts[i]);
		count++;
	}

	if (!count)
		printf("  No event counters enabled.\n");

	return 0;
}

static int get_free_counter(struct switchtec_dev *dev, int stack)
{
	struct switchtec_evcntr_setup setups[SWITCHTEC_MAX_EVENT_COUNTERS];
	int ret;
	int i;

	ret = switchtec_evcntr_get_setup(dev, stack, 0, ARRAY_SIZE(setups),
					 setups);
	if (ret < 0) {
		switchtec_perror("evcntr_get_setup");
		return ret;
	}

	for (i = 0; i < ret; i ++) {
		if (!setups[i].port_mask || !setups[i].type_mask)
			return i;
	}

	errno = EBUSY;
	return -errno;
}

static void show_event_counter(int stack, int counter,
			       struct switchtec_evcntr_setup *setup)
{
	char buf[200];

	printf("Stack:     %d\n", stack);
	printf("Counter:   %d\n", counter);

	if (!setup->port_mask || !setup->type_mask) {
		printf("Not Configured.\n");
		return;
	}

	if (setup->threshold)
		printf("Threshold: %d\n", setup->threshold);
	printf("Ports:     %s\n", port_mask_to_string(setup->port_mask,
						      buf, sizeof(buf)));
	printf("Events:    %s\n", type_mask_to_string(setup->type_mask,
						      buf, sizeof(buf)));
	if (setup->type_mask & ALL_TLPS)
		printf("Direction: %s\n", setup->egress ?
		       "EGRESS" : "INGRESS");
}

#define CMD_DESC_EVCNTR_SETUP "configure an event counter"

static int evcntr_setup(int argc, char **argv)
{
	int nr_type_choices = switchtec_evcntr_type_count();
	struct argconfig_choice type_choices[nr_type_choices+1];
	int ret;

	static struct {
		struct switchtec_dev *dev;
		int stack;
		int counter;
		struct switchtec_evcntr_setup setup;
	} cfg = {
		.stack = -1,
		.counter = -1,
	};

	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"stack", 's', "NUM", CFG_NONNEGATIVE, &cfg.stack, required_argument,
		 "stack to create the counter in",
		 .require_in_usage=1},
		{"event", 'e', "EVENT", CFG_MULT_CHOICES, &cfg.setup.type_mask,
		  required_argument,
		 "event to count on, may specify this argument multiple times "
		 "to count on multiple events",
		 .choices=type_choices, .require_in_usage=1},

		{"counter", 'c', "NUM", CFG_NONNEGATIVE, &cfg.counter, required_argument,
		 "counter index, default is to use the next unused index"},
		{"egress", 'g', "", CFG_NONE, &cfg.setup.egress, no_argument,
		 "measure egress TLPs instead of ingress -- only meaningful for "
		 "POSTED_TLP, COMP_TLP and NON_POSTED_TLP counts"},
		{"port_mask", 'p', "0xXX|#,#,#-#,#", CFG_MASK_8, &cfg.setup.port_mask,
		  required_argument,
		 "ports to capture events on, default is all ports"},
		{"thresh", 't', "NUM", CFG_POSITIVE, &cfg.setup.threshold,
		 required_argument,
		 "threshold to trigger an event notification"},
		{NULL}};

	create_type_choices(type_choices);
	argconfig_parse(argc, argv, CMD_DESC_EVCNTR_SETUP, opts, &cfg, sizeof(cfg));

	if (cfg.stack < 0) {
		argconfig_print_usage(opts);
		fprintf(stderr, "The --stack argument is required!\n");
		return 1;
	}

	if (!cfg.setup.type_mask) {
		argconfig_print_usage(opts);
		fprintf(stderr, "Must specify at least one event!\n");
		return 1;
	}

	if (!cfg.setup.port_mask)
		cfg.setup.port_mask = -1;

	if (cfg.counter < 0) {
		cfg.counter = get_free_counter(cfg.dev, cfg.stack);
		if (cfg.counter < 0)
			return cfg.counter;
	}

	if (cfg.setup.threshold &&
	    (__builtin_popcount(cfg.setup.port_mask) > 1 ||
	    __builtin_popcount(cfg.setup.type_mask) > 1))
	{
		fprintf(stderr, "A threshold can only be used with a counter "
			"that has a single port and single event\n");
		return 1;
	}

	show_event_counter(cfg.stack, cfg.counter, &cfg.setup);

	ret = switchtec_evcntr_setup(cfg.dev, cfg.stack, cfg.counter,
				     &cfg.setup);

	switchtec_perror("evcntr-setup");

	return ret;
}

#define CMD_DESC_EVCNTR "display event counters"

static int evcntr(int argc, char **argv)
{
	int ret, i;

	static struct {
		struct switchtec_dev *dev;
		int stack;
		int reset;
	} cfg = {
		.stack = -1,
	};

	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"reset", 'r', "", CFG_NONE, &cfg.reset, no_argument,
		 "reset counters back to zero"},
		{"stack", 's', "NUM", CFG_NONNEGATIVE, &cfg.stack, required_argument,
		 "stack to show the counters for"},
		{}};

	argconfig_parse(argc, argv, CMD_DESC_EVCNTR, opts, &cfg, sizeof(cfg));

	if (cfg.stack < 0) {
		for (i = 0; i < SWITCHTEC_MAX_STACKS; i++)
			display_event_counters(cfg.dev, i, cfg.reset);
		return 0;
	}

	ret = display_event_counters(cfg.dev, cfg.stack, cfg.reset);
	if (ret)
		switchtec_perror("display events");

	return ret;
}

#define CMD_DESC_EVCNTR_SHOW "display an event counter's configuration"

static int evcntr_show(int argc, char **argv)
{
	struct switchtec_evcntr_setup setup;
	int ret;

	static struct {
		struct switchtec_dev *dev;
		int stack;
		int counter;
	} cfg = {
		.stack = -1,
		.counter = -1,
	};

	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"stack", 's', "NUM", CFG_NONNEGATIVE, &cfg.stack, required_argument,
		 "stack to show the configuration for",
		 .require_in_usage=1},
		{"counter", 'c', "NUM", CFG_NONNEGATIVE, &cfg.counter, required_argument,
		 "counter index",
		 .require_in_usage=1},
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_EVCNTR_SHOW, opts, &cfg, sizeof(cfg));

	if (cfg.stack < 0) {
		argconfig_print_usage(opts);
		fprintf(stderr, "The --stack argument is required!\n");
		return 1;
	}

	if (cfg.counter < 0) {
		argconfig_print_usage(opts);
		fprintf(stderr, "The --counter argument is required!\n");
		return 1;
	}

	ret = switchtec_evcntr_get_setup(cfg.dev, cfg.stack, cfg.counter, 1,
					 &setup);
	if (ret < 0) {
		switchtec_perror("evcntr_show");
		return ret;
	}

	show_event_counter(cfg.stack, cfg.counter, &setup);

	return 0;
}

#define CMD_DESC_EVCNTR_DEL "deconfigure an event counter"

static int evcntr_del(int argc, char **argv)
{
	struct switchtec_evcntr_setup setup = {};
	int ret;

	static struct {
		struct switchtec_dev *dev;
		int stack;
		int counter;
	} cfg = {
		.stack = -1,
		.counter = -1,
	};

	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"stack", 's', "NUM", CFG_NONNEGATIVE, &cfg.stack, required_argument,
		 "stack to deconfigure the counter in",
		 .require_in_usage=1},
		{"counter", 'c', "NUM", CFG_NONNEGATIVE, &cfg.counter, required_argument,
		 "counter index",
		 .require_in_usage=1},
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_EVCNTR_DEL, opts, &cfg, sizeof(cfg));

	if (cfg.stack < 0) {
		argconfig_print_usage(opts);
		fprintf(stderr, "The --stack argument is required!\n");
		return 1;
	}

	if (cfg.counter < 0) {
		argconfig_print_usage(opts);
		fprintf(stderr, "The --counter argument is required!\n");
		return 1;
	}

	ret = switchtec_evcntr_setup(cfg.dev, cfg.stack, cfg.counter, &setup);
	if (ret < 0) {
		switchtec_perror("evcntr_del");
		return ret;
	}


	return 0;
}

#define CMD_DESC_EVCNTR_WAIT "wait for an event counter to exceed its threshold"

static int evcntr_wait(int argc, char **argv)
{
	int ret, i;

	static struct {
		struct switchtec_dev *dev;
		int timeout;
	} cfg = {
		.timeout = -1,
	};

	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"timeout", 't', "MS", CFG_INT, &cfg.timeout, required_argument,
		 "timeout in milliseconds (-1 = forever)"},
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_EVCNTR_WAIT, opts, &cfg, sizeof(cfg));

	ret = switchtec_evcntr_wait(cfg.dev, cfg.timeout);
	if (ret < 0) {
		perror("evcntr_wait");
		return -1;
	}

	if (!ret) {
		fprintf(stderr, "timeout\n");
		return 1;
	}

	for (i = 0; i < SWITCHTEC_MAX_STACKS; i++)
		display_event_counters(cfg.dev, i, 0);

	return 0;
}

static const struct cmd commands[] = {
	CMD(list, CMD_DESC_LIST),
	CMD(info, CMD_DESC_INFO),
	CMD(gui, CMD_DESC_GUI),
	CMD(status, CMD_DESC_STATUS),
	CMD(bw, CMD_DESC_BW),
	CMD(latency, CMD_DESC_LATENCY),
	CMD(events, CMD_DESC_EVENTS),
	CMD(event_wait, CMD_DESC_EVENT_WAIT),
	CMD(log_dump, CMD_DESC_LOG_DUMP),
	CMD(log_parse, CMD_DESC_LOG_PARSE),
	CMD(test, CMD_DESC_TEST),
	CMD(temp, CMD_DESC_TEMP),
	CMD(port_bind_info, CMD_DESC_PORT_BIND_INFO),
	CMD(port_bind, CMD_DESC_PORT_BIND),
	CMD(port_unbind, CMD_DESC_PORT_UNBIND),
	CMD(stack_bif, CMD_DESC_STACK_BIF),
	CMD(hard_reset, CMD_DESC_HARD_RESET),
	CMD(fw_update, CMD_DESC_FW_UPDATE),
	CMD(fw_info, CMD_DESC_FW_INFO),
	CMD(fw_toggle, CMD_DESC_FW_TOGGLE),
	CMD(fw_read, CMD_DESC_FW_READ),
	CMD(fw_img_info, CMD_DESC_FW_IMG_INFO),
	CMD(evcntr, CMD_DESC_EVCNTR),
	CMD(evcntr_setup, CMD_DESC_EVCNTR_SETUP),
	CMD(evcntr_show, CMD_DESC_EVCNTR_SHOW),
	CMD(evcntr_del, CMD_DESC_EVCNTR_DEL),
	CMD(evcntr_wait, CMD_DESC_EVCNTR_WAIT),
	{},
};

static struct subcommand subcmd = {
	.cmds = commands,
};

REGISTER_SUBCMD(subcmd);

static struct prog_info prog_info = {
	.usage = "<command> [<device>] [OPTIONS]",
	.desc = "The <device> must be a Switchtec device "
		"(ex: /dev/switchtec0)",
};

static void sig_handler(int signum)
{
	if (signum == SIGBUS) {
		fprintf(stderr, "Error communicating with the device. "
			"Please check your setup.\n");
		exit(1);
	}
}

static void setup_sigbus(void)
{
	signal(SIGBUS, sig_handler);
}

int main(int argc, char **argv)
{
	int ret;

	setup_sigbus();

	ret = commands_handle(argc, argv, &prog_info);

	switchtec_close(global_dev);

	return ret;
}
