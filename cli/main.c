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
	.usage = "<command> [<device>] [<args>]",
	.desc = "The <device> must be a switchtec device "\
                "(ex: /dev/switchtec0)",
	.extensions = &builtin,
};

static struct {} empty_cfg;
const struct argconfig_commandline_options empty_opts[] = {{NULL}};

static void check_arg_dev(int argc, char **argv)
{
	if (optind >= argc) {
		errno = EINVAL;
		argconfig_print_usage();
		exit(errno);
	}
}

struct switchtec_dev *parse_and_open(int argc, char **argv, const char *desc,
	const struct argconfig_commandline_options *clo,
	void *cfg, size_t size)
{
	struct switchtec_dev *dev;

	argconfig_parse(argc, argv, desc, clo, cfg, size);
	check_arg_dev(argc, argv);

	dev = switchtec_open(argv[optind]);

	if (dev == NULL)
		perror(argv[optind]);

	optind++;

	return dev;
}

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
		printf("%-20s %-15s %s\n", devices[i].path, devices[i].model,
		       devices[i].pci_dev);

	free(devices);
	return 0;
}

static int test(int argc, char **argv, struct command *cmd,
		struct plugin *plugin)
{
	struct switchtec_dev *dev;
	int ret;
	uint32_t in, out;
	const char *desc = "Test if switchtec interface is working";

	argconfig_append_usage(" <device>");
	dev = parse_and_open(argc, argv, desc, empty_opts, &empty_cfg,
			    sizeof(empty_cfg));

	if (dev == NULL)
		return -errno;

	in = time(NULL);

	ret = switchtec_echo(dev, in, &out);

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
        struct switchtec_dev *dev;
	int ret;
	const char *desc = "Perform a hard reset on the switch";

	static struct {
		int assume_yes;
	} cfg;
	const struct argconfig_commandline_options opts[] = {
		{"yes", 'y', "", CFG_NONE, &cfg.assume_yes, no_argument,
		 "assume yes when prompted"},
		{NULL}};

	argconfig_append_usage(" <device>");
	dev = parse_and_open(argc, argv, desc, opts, &cfg, sizeof(cfg));

	if (dev == NULL)
		return -errno;

	if (!cfg.assume_yes)
		fprintf(stderr,
			"WARNING: if your system does not support hotplug,\n"
			"a hard reset can leave the system in a broken state.\n"
			"Make sure you reboot after issuing this command.\n\n");

	ret = ask_if_sure(cfg.assume_yes);
	if (ret)
		return ret;

	ret = switchtec_hard_reset(dev);
	if (ret) {
		perror(argv[optind]);
		return ret;
	}

	fprintf(stderr, "%s: hard reset\n", argv[optind]);
	return 0;
}

static int open_and_print_fw_image(int argc, char **argv)
{
	int img_fd, ret;
	struct switchtec_fw_image_info info;

	if (optind >= argc) {
		argconfig_print_usage();
		exit(-EINVAL);
	}

	img_fd = open(argv[optind], O_RDONLY);
	if (img_fd < 0) {
		perror(argv[optind]);
		return img_fd;
	}

	ret = switchtec_fw_image_info(img_fd, &info);

	if (ret < 0) {
		fprintf(stderr, "%s: Invalid image file format\n",
			argv[optind]);
		return ret;
	}

	printf("File:     %s\n", strrchr(argv[optind], '/')+1);
	printf("Type:     %s\n", switchtec_fw_image_type(&info));
	printf("Version:  %s\n", info.version);
	printf("Img Len:  0x%zx\n", info.image_len);
	printf("CRC:      0x%lx\n", info.crc);

	return img_fd;
}

static int fw_image_info(int argc, char **argv, struct command *cmd,
		       struct plugin *plugin)
{
	int img_fd;
	const char *desc = "Display information for a firmware image";

	argconfig_append_usage(" <img_file>");
	argconfig_parse(argc, argv, desc, empty_opts, &empty_cfg,
			sizeof(empty_cfg));

	img_fd = open_and_print_fw_image(argc, argv);
	if (img_fd < 0)
		return img_fd;

	close(img_fd);
	return 0;
}

static void fw_update_callback(int cur, int total)
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
	struct switchtec_dev *dev;
	int img_fd;
	int ret;
	const char *desc = "Flash the firmware with a new image";

	static struct {
		int assume_yes;
	} cfg;
	const struct argconfig_commandline_options opts[] = {
		{"yes", 'y', "", CFG_NONE, &cfg.assume_yes, no_argument,
		 "assume yes when prompted"},
		{NULL}};

	argconfig_append_usage(" <device> <img_file>");
	dev = parse_and_open(argc, argv, desc, opts, &cfg, sizeof(cfg));

	if (dev == NULL)
		return -errno;

	printf("Writing the following firmware image to %s.\n",
	       argv[optind-1]);

	img_fd = open_and_print_fw_image(argc, argv);
	if (img_fd < 0)
		return img_fd;

	ret = ask_if_sure(cfg.assume_yes);
	if (ret) {
		close(img_fd);
		return ret;
	}

	ret = switchtec_fw_update(dev, img_fd, fw_update_callback);
	close(img_fd);
	printf("\n");

	switchtec_fw_perror("firmware update", ret);

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

	return ret;
}
