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

#include <switchtec/switchtec.h>

#include <dirent.h>
#include <locale.h>

#include <errno.h>
#include <stdio.h>

#define CREATE_CMD
#include "builtin.h"

static const char *sys_path = "/sys/class/switchtec";

static const char version_string[] = "TODO-VERESION";

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
