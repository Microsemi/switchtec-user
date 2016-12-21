/*
 * Microsemi Switchtec(tm) PCIe Management Command Line Interface
 * Copyright (c) 2016, Microsemi Corporation
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include "plugin.h"
#include "argconfig.h"
#include "version.h"

#include <switchtec/switchtec.h>

#include <locale.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>

#include <errno.h>
#include <stdio.h>

#define CREATE_CMD
#include "builtin.h"

static const char version_string[] = VERSION;

static struct plugin builtin = {
	.commands = commands,
	.tail = &builtin,
};

static struct program switchtec = {
	.name = "switchtec",
	.version = version_string,
	.usage = "<command> [<device>] [OPTIONS]",
	.desc = "The <device> must be a switchtec device "\
                "(ex: /dev/switchtec0)",
	.extensions = &builtin,
};

static struct {} empty_cfg;
const struct argconfig_options empty_opts[] = {{NULL}};

struct switchtec_dev *global_dev = NULL;

int switchtec_handler(const char *optarg, void *value_addr,
		      const struct argconfig_options *opt)
{
	struct switchtec_dev *dev;

	global_dev = dev = switchtec_open(optarg);

	if (dev == NULL) {
		perror(optarg);
		return 1;
	}

	*((struct switchtec_dev  **) value_addr) = dev;
	return 0;
}

#define DEVICE_OPTION {"device", .cfg_type=CFG_CUSTOM, .value_addr=&cfg.dev, \
		       .argument_type=required_positional, \
		       .custom_handler=switchtec_handler, \
		       .complete="/dev/switchtec*", \
		       .help="switchtec device to operate on"}


static int list(int argc, char **argv, struct command *cmd,
		struct plugin *plugin)
{
	struct switchtec_device_info *devices;
	int i, n;
	const char *desc = "List all the switchtec devices on this machine";

	argconfig_parse(argc, argv, desc, empty_opts, &empty_cfg,
			sizeof(empty_cfg));

	n = switchtec_list(&devices);
	if (n < 0)
		return n;

	for (i = 0; i < n; i++)
		printf("%-20s\t%-15s\t%-5s\t%-10s\t%s\n", devices[i].path,
		       devices[i].product_id, devices[i].product_rev,
		       devices[i].fw_version, devices[i].pci_dev);

	free(devices);
	return 0;
}

static int status(int argc, char **argv, struct command *cmd,
		  struct plugin *plugin)
{
	const char *desc = "Display status of the ports on the switch";
	int ret;
	struct switchtec_status *status;
	int p;
	int last_partition = -1;

	const float gen_transfers[] = {0, 2.5, 5, 8, 16};
	const float gen_datarate[] = {0, 250, 500, 985, 1969};

	static struct {
		struct switchtec_dev *dev;
	} cfg = {0};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{NULL}};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));

	ret = switchtec_status(cfg.dev, &status);
	if (ret < 0) {
		perror("status");
		return ret;
	}

	for (p = 0; p < ret; p++) {
		struct switchtec_status *s = &status[p];

		if (s->partition != last_partition)
			printf("Partition %d:\n", s->partition);
		last_partition = s->partition;

		printf("      Stack %d, Port %d (%s): \n", s->stack,
		       s->stk_port_id, s->upstream_port ? "USP" : "DSP");
		printf("         Status:          \t%s\n",
		       s->link_up ? "UP" : "DOWN");
		printf("         LTSSM:           \t%s\n", s->ltssm_str);
		printf("         Max-Width:       \tx%d\n", s->cfg_lnk_width);
		printf("         Phys Port ID:    \t%d\n", s->phys_port_id);
		printf("         Logical Port ID: \t%d\n", s->log_port_id);

		if (!s->link_up) continue;

		printf("         Width:           \tx%d\n", s->neg_lnk_width);
		printf("         Rate:            \tGen%d - %g GT/s  %g GB/s\n",
		       s->link_rate, gen_transfers[s->link_rate],
		       gen_datarate[s->link_rate]*s->neg_lnk_width/1000.);
	}

	free(status);

	return 0;

}

static int test(int argc, char **argv, struct command *cmd,
		struct plugin *plugin)
{
	const char *desc = "Test if switchtec interface is working";
	int ret;
	uint32_t in, out;

	static struct {
		struct switchtec_dev *dev;
	} cfg = {0};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{NULL}};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));

	in = time(NULL);

	ret = switchtec_echo(cfg.dev, in, &out);

	if (ret) {
		perror(argv[optind]);
		return ret;
	}

	if (in != ~out) {
		fprintf(stderr, "argv[optind]: echo command returned the "
			"wrong result; got %x, expected %x\n",
			out, ~in);
		return 1;
	}

	fprintf(stderr, "%s: success\n", argv[optind-1]);

	return 0;
}

static int ask_if_sure(int always_yes)
{
	char buf[10];

	if (always_yes)
		return 0;

	fprintf(stderr, "Do you want to continue? [y/N] ");
	fgets(buf, sizeof(buf), stdin);

	if (strcmp(buf, "y\n") == 0 || strcmp(buf, "Y\n") == 0)
		return 0;

	fprintf(stderr, "Abort.\n");
	errno = EINTR;
	return -errno;
}

static int hard_reset(int argc, char **argv, struct command *cmd,
		      struct plugin *plugin)
{
	const char *desc = "Perform a hard reset on the switch";
	int ret;

	static struct {
		struct switchtec_dev *dev;
		int assume_yes;
	} cfg = {0};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"yes", 'y', "", CFG_NONE, &cfg.assume_yes, no_argument,
		 "assume yes when prompted"},
		{NULL}};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));

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
		perror(argv[optind]);
		return ret;
	}

	fprintf(stderr, "%s: hard reset\n", argv[optind]);
	return 0;
}

static const char *get_basename(const char *buf)
{
	const char *slash = strrchr(buf, '/');

	if (slash)
		return slash+1;

	return buf;
}

static int check_and_print_fw_image(int img_fd, const char *img_filename)
{
	int ret;
	struct switchtec_fw_image_info info;
	ret = switchtec_fw_image_info(img_fd, &info);

	if (ret < 0) {
		fprintf(stderr, "%s: Invalid image file format\n",
			img_filename);
		return ret;
	}

	printf("File:     %s\n", get_basename(img_filename));
	printf("Type:     %s\n", switchtec_fw_image_type(&info));
	printf("Version:  %s\n", info.version);
	printf("Img Len:  0x%zx\n", info.image_len);
	printf("CRC:      0x%08lx\n", info.crc);

	return 0;
}

static int fw_image_info(int argc, char **argv, struct command *cmd,
		       struct plugin *plugin)
{
	const char *desc = "Display information for a firmware image";
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

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));

	ret = check_and_print_fw_image(cfg.img_fd, cfg.img_filename);
	if (ret < 0)
		return ret;

	close(cfg.img_fd);
	return 0;
}

static int print_fw_part_info(struct switchtec_dev *dev)
{
	struct switchtec_fw_image_info act_img, inact_img, act_cfg, inact_cfg;
	int ret;

	ret = switchtec_fw_part_act_info(dev, &act_img, &inact_img, &act_cfg,
					 &inact_cfg);
	if (ret < 0)
		return ret;

	printf("Active Partition:\n");
	printf("  IMG \tVersion: %-8s\tCRC: %08lx\n",
	       act_img.version, act_img.crc);
	printf("  CFG  \tVersion: %-8s\tCRC: %08lx\n",
	       act_cfg.version, act_cfg.crc);
	printf("Inactive Partition:\n");
	printf("  IMG  \tVersion: %-8s\tCRC: %08lx\n",
	       inact_img.version, inact_img.crc);
	printf("  CFG  \tVersion: %-8s\tCRC: %08lx\n",
	       inact_cfg.version, inact_cfg.crc);

	return 0;
}

static int fw_info(int argc, char **argv, struct command *cmd,
		struct plugin *plugin)
{
	const char *desc = "Test if switchtec interface is working";
	int ret;
	char version[64];

	static struct {
		struct switchtec_dev *dev;
	} cfg = {0};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{NULL}};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));

	ret = switchtec_get_fw_version(cfg.dev, version, sizeof(version));
	if (ret < 0) {
		perror("fw info");
		return ret;
	}

	printf("Currently Running:\n");
	printf("  IMG Version: %s\n", version);

	print_fw_part_info(cfg.dev);

	return 0;
}

static void fw_progress_callback(int cur, int total)
{
	const int bar_width = 60;

	int i;
	float progress = cur * 100.0 / total;
	int pos = bar_width * cur / total;

	printf(" [");
	for (i = 0; i < bar_width; i++) {
		if (i < pos) putchar('=');
		else if (i == pos) putchar('>');
		else putchar(' ');
	}
	printf("] %2.0f %%\r", progress);
	fflush(stdout);
}

static int fw_update(int argc, char **argv, struct command *cmd,
		     struct plugin *plugin)
{
	int ret;
	const char *desc = "Flash the firmware with a new image";

	static struct {
		struct switchtec_dev *dev;
		int img_fd;
		const char *img_filename;
		int assume_yes;
		int dont_activate;
	} cfg = {0};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"img_file", .cfg_type=CFG_FD_RD, .value_addr=&cfg.img_fd,
		  .argument_type=required_positional,
		  .help="image file to use as the new firmware"},
		{"yes", 'y', "", CFG_NONE, &cfg.assume_yes, no_argument,
		 "assume yes when prompted"},
		{"dont-activate", 'A', "", CFG_NONE, &cfg.dont_activate, no_argument,
		 "don't activate the new image, use fw-toggle to do so "
		 "when it is safe"},
		{NULL}};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));

	printf("Writing the following firmware image to %s.\n",
	       switchtec_name(cfg.dev));

	ret = check_and_print_fw_image(cfg.img_fd, cfg.img_filename);
	if (ret < 0)
		return ret;

	ret = ask_if_sure(cfg.assume_yes);
	if (ret) {
		close(cfg.img_fd);
		return ret;
	}

	ret = switchtec_fw_write_file(cfg.dev, cfg.img_fd, cfg.dont_activate,
				      fw_progress_callback);
	close(cfg.img_fd);
	printf("\n\n");

	print_fw_part_info(cfg.dev);
	printf("\n");

	switchtec_fw_perror("firmware update", ret);

	return ret;
}

static int fw_toggle(int argc, char **argv, struct command *cmd,
		     struct plugin *plugin)
{
	const char *desc = "Toggle active and inactive firmware partitions";
	int ret = 0;

	static struct {
		struct switchtec_dev *dev;
		int firmware;
		int config;
	} cfg = {0};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"firmware", 'f', "", CFG_NONE, &cfg.firmware, no_argument,
		 "toggle IMG firmware"},
		{"config", 'c', "", CFG_NONE, &cfg.config, no_argument,
		 "toggle CFG data"},
		{NULL}};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));

	if (!cfg.firmware && !cfg.config) {
		fprintf(stderr, "NOTE: Not toggling images seeing neither "
			"--firmware nor --config were specified\n\n");
	} else {
		ret = switchtec_fw_toggle_active_partition(cfg.dev,
							   cfg.firmware,
							   cfg.config);
	}

	print_fw_part_info(cfg.dev);
	printf("\n");

	perror("firmware toggle");

	return ret;
}

static int fw_read(int argc, char **argv, struct command *cmd,
		   struct plugin *plugin)
{
	const char *desc = "Flash the firmware with a new image";
	struct switchtec_fw_footer ftr;
	struct switchtec_fw_image_info act_img, inact_img, act_cfg, inact_cfg;
	int ret = 0;
	char version[16];
	unsigned long img_addr;
	size_t img_size;
	enum switchtec_fw_image_type type;

	static struct {
		struct switchtec_dev *dev;
		int out_fd;
		const char *out_filename;
		int inactive;
		int data;
	} cfg = {0};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"filename", .cfg_type=CFG_FD_WR, .value_addr=&cfg.out_fd,
		  .argument_type=optional_positional,
		  .force_default="image.pmc",
		  .help="image file to display information for"},
		{"inactive", 'i', "", CFG_NONE, &cfg.inactive, no_argument,
		 "read the inactive partition"},
		{"data", 'd', "", CFG_NONE, &cfg.data, no_argument,
		 "read the data/config partiton instead of the main firmware"},
		{"config", 'c', "", CFG_NONE, &cfg.data, no_argument,
		 "read the data/config partiton instead of the main firmware"},
		{NULL}};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));

	ret = switchtec_fw_part_act_info(cfg.dev, &act_img, &inact_img,
					 &act_cfg, &inact_cfg);
	if (ret < 0) {
		perror("fw_part_act_info");
		goto close_and_exit;
	}

	if (cfg.data) {
		img_addr = cfg.inactive ? inact_cfg.image_addr :
			act_cfg.image_addr;
		img_size = cfg.inactive ? inact_cfg.image_len :
			act_cfg.image_len;;
		type = SWITCHTEC_FW_TYPE_DAT0;
	} else {
		img_addr = cfg.inactive ? inact_img.image_addr :
			act_img.image_addr;
		img_size = cfg.inactive ? inact_img.image_len :
			act_img.image_len;
		type = SWITCHTEC_FW_TYPE_IMG0;
	}

	ret = switchtec_fw_read_footer(cfg.dev, img_addr, img_size, &ftr,
				       version, sizeof(version));
	if (ret < 0) {
		perror("fw_read_footer");
		goto close_and_exit;
	}

	fprintf(stderr, "Version:  %s\n", version);
	fprintf(stderr, "Type:     %s\n", cfg.data ? "DAT" : "IMG");
	fprintf(stderr, "Img Len:  0x%x\n", (int) ftr.image_len);
	fprintf(stderr, "CRC:      0x%x\n", (int) ftr.image_crc);

	ret = switchtec_fw_img_write_hdr(cfg.out_fd, &ftr, type);
	if (ret < 0) {
		perror(cfg.out_filename);
		goto close_and_exit;
	}

	ret = switchtec_fw_read_file(cfg.dev, cfg.out_fd, img_addr,
				     ftr.image_len, fw_progress_callback);
	if (ret < 0)
		perror("fw_read");

	fprintf(stderr, "\nFirmware read to %s.\n", cfg.out_filename);

close_and_exit:
	close(cfg.out_fd);

	return ret;
}

int main(int argc, char **argv)
{
	int ret;

	switchtec.extensions->parent = &switchtec;
	if (argc < 2) {
		general_help(&builtin);
		return 0;
	}

	setlocale(LC_ALL, "");

	ret = handle_plugin(argc - 1, &argv[1], switchtec.extensions);
	if (ret == -ENOTSUP)
		general_help(&builtin);

	switchtec_close(global_dev);

	return ret;
}
