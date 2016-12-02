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

#include <dirent.h>
#include <locale.h>
#include <time.h>

#include <errno.h>
#include <stdio.h>

#define CREATE_CMD
#include "builtin.h"

static const char *sys_path = "/sys/class/switchtec";

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

static int scan_dev_filter(const struct dirent *d)
{
	if (d->d_name[0] == '.')
		return 0;

	return 1;
}

static int list(int argc, char **argv, struct command *cmd,
		struct plugin *plugin)
{
	const char *desc = "List all the switchtec devices on this machine";
	struct dirent **devices;
	unsigned int i, n;
	struct config {
	} cfg;

	const struct argconfig_commandline_options opts[] = {
		{NULL}
	};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));

	n = scandir(sys_path, &devices, scan_dev_filter, alphasort);
	if (n <= 0)
		return n;

	for (i = 0; i < n; i++)
		printf("/dev/%s\n", devices[i]->d_name);

	return 0;
}

static int test(int argc, char **argv, struct command *cmd,
		struct plugin *plugin)
{
	int fd;
	int ret;
	uint32_t in, out;
	const char *desc = "Test if switchtec interface is working";

	struct config {
	} cfg;

	const struct argconfig_commandline_options opts[] = {
		{NULL}
	};

	fd = parse_and_open(argc, argv, desc, opts, &cfg, sizeof(cfg));

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
