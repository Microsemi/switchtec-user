/*
 * Microsemi Switchtec(tm) PCIe Management Command Line Interface
 * Copyright (c) 2017, Microsemi Corporation
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

#include <sys/mman.h>
#include <unistd.h>

#define CREATE_CMD
#include "gas.h"

static int spawn_proc(int fd_in, int fd_out, int fd_close,
		      const char *cmd)
{
	pid_t pid;

	pid = fork();

	if (pid != 0)
		return 0;

	close(fd_close);

	if (fd_in != STDIN_FILENO) {
		dup2(fd_in, STDIN_FILENO);
		close(fd_in);
	}

	if (fd_out != STDOUT_FILENO) {
		dup2(fd_out, STDOUT_FILENO);
		close(fd_out);
	}

	return execlp(cmd, cmd, NULL);
}

static int pipe_to_hd_less(void *map)
{
	int hd_fds[2];
	int less_fds[2];
	int ret;

	pipe(less_fds);

	ret = spawn_proc(less_fds[0], STDOUT_FILENO, less_fds[1], "less");
	if (ret < 0) {
		perror("less");
		return ret;
	}
	close(STDOUT_FILENO);
	close(less_fds[0]);

	pipe(hd_fds);
	ret = spawn_proc(hd_fds[0], less_fds[1], hd_fds[1], "hd");
	if (ret < 0) {
		perror("hd");
		return ret;
	}

	close(hd_fds[0]);
	close(less_fds[1]);

	ret = write(hd_fds[1], map, SWITCHTEC_GAS_MAP_SIZE);
	close(hd_fds[1]);
	return ret;
}

static int gas_dump(int argc, char **argv, struct command *cmd,
		    struct plugin *plugin)
{
	const char *desc = "Dump all gas registers";
	void *map;
	int ret;

	static struct {
		struct switchtec_dev *dev;
	} cfg = {0};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{NULL}};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));

	map = switchtec_gas_map(cfg.dev, 0);
	if (map == MAP_FAILED) {
		switchtec_perror("gas_map");
		return 1;
	}

	if (!isatty(STDOUT_FILENO)) {
		ret = write(STDOUT_FILENO, map, SWITCHTEC_GAS_MAP_SIZE);
		return ret > 0;
	}

	return pipe_to_hd_less(map);
}
