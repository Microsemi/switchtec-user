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
	.desc = "The '<device> must be a switchtec device "\
                "(ex: /dev/switchtec0)",
	.extensions = &builtin,
};

static struct {} empty_cfg;
const struct argconfig_commandline_options empty_opts[] = {{NULL}};

static void check_arg_dev(int argc, char **argv)
{
	if (optind >= argc) {
		errno = EINVAL;
		perror(argv[0]);
		exit(errno);
	}
}

int parse_and_open(int argc, char **argv, const char *desc,
		   const struct argconfig_commandline_options *clo,
		   void *cfg, size_t size)
{
	argconfig_parse(argc, argv, desc, clo, cfg, size);
	check_arg_dev(argc, argv);
	return switchtec_open(argv[optind]);
}


static int list(int argc, char **argv, struct command *cmd,
		struct plugin *plugin)
{
	struct switchtec_device *devices;
	int i, n;
	const char *desc = "List all the switchtec devices on this machine";

	argconfig_parse(argc, argv, desc, empty_opts, &empty_cfg,
			sizeof(empty_cfg));

	n = switchtec_list(&devices);
	if (n < 0)
		return n;

	for (i = 0; i < n; i++)
		printf("%-20s %s\n", devices[i].path, devices[i].pci_dev);

	free(devices);
	return 0;
}

static int test(int argc, char **argv, struct command *cmd,
		struct plugin *plugin)
{
	int fd;
	int ret;
	uint32_t in, out;
	const char *desc = "Test if switchtec interface is working";

	fd = parse_and_open(argc, argv, desc, empty_opts, &empty_cfg,
			    sizeof(empty_cfg));

	in = time(NULL);

	ret = switchtec_echo(fd, in, &out);

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

	fprintf(stderr, "%s: success\n", argv[optind]);

	return 0;
}

static int hard_reset(int argc, char **argv, struct command *cmd,
		struct plugin *plugin)
{
	int fd;
	int ret;
	const char *desc = "Perform a hard reset on the switch";

	fd = parse_and_open(argc, argv, desc, empty_opts, &empty_cfg,
			    sizeof(empty_cfg));

	ret = switchtec_hard_reset(fd);
	if (ret) {
		perror(argv[optind]);
		return ret;
	}

	fprintf(stderr, "%s: hard reset\n", argv[optind]);
	return 0;
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
	if (ret == -ENOTTY)
		general_help(&builtin);

	return ret;
}
