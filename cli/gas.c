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
#include <stdint.h>
#include <sys/types.h>
#include <sys/wait.h>

#define CREATE_CMD
#include "gas.h"

static int spawn_proc(int fd_in, int fd_out, int fd_close,
		      const char *cmd)
{
	pid_t pid;

	pid = fork();

	if (pid != 0)
		return pid;

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
	int less_pid, hd_pid;
	int ret;

	pipe(less_fds);

	less_pid = spawn_proc(less_fds[0], STDOUT_FILENO, less_fds[1], "less");
	if (less_pid < 0) {
		perror("less");
		return -1;
	}
	close(STDOUT_FILENO);
	close(less_fds[0]);

	pipe(hd_fds);
	hd_pid = spawn_proc(hd_fds[0], less_fds[1], hd_fds[1], "hd");
	if (hd_pid < 0) {
		perror("hd");
		return -1;
	}

	close(hd_fds[0]);
	close(less_fds[1]);

	ret = write(hd_fds[1], map, SWITCHTEC_GAS_MAP_SIZE);
	close(hd_fds[1]);
	waitpid(less_pid, NULL, 0);
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

static int print_hex(void *addr, int offset, int bytes)
{
	unsigned long long x;

	offset = offset & ~(bytes - 1);

	switch (bytes) {
	case 1: x = *((uint8_t *)(addr + offset));  break;
	case 2: x = *((uint16_t *)(addr + offset)); break;
	case 4: x = *((uint32_t *)(addr + offset)); break;
	case 8: x = *((uint64_t *)(addr + offset)); break;
	default:
		fprintf(stderr, "invalid access width\n");
		return -1;
	}

	printf("%06X - 0x%0*llX\n", offset, bytes * 2, x);
	return 0;
}

static int print_dec(void *addr, int offset, int bytes)
{
	unsigned long long x;

	offset = offset & ~(bytes - 1);

	switch (bytes) {
	case 1: x = *((uint8_t *)(addr + offset));  break;
	case 2: x = *((uint16_t *)(addr + offset)); break;
	case 4: x = *((uint32_t *)(addr + offset)); break;
	case 8: x = *((uint64_t *)(addr + offset)); break;
	default:
		fprintf(stderr, "invalid access width\n");
		return -1;
	}

	printf("%06X - %lld\n", offset, x);
	return 0;
}

static int print_str(void *addr, int offset, int bytes)
{
	char buf[bytes + 1];

	memset(buf, 0, bytes + 1);
	memcpy(buf, addr + offset, bytes);

	printf("%06X - %s\n", offset, buf);
	return 0;
}

enum {
	HEX,
	DEC,
	STR,
};

int (*print_funcs[])(void *addr, int offset, int bytes) = {
	[HEX] = print_hex,
	[DEC] = print_dec,
	[STR] = print_str,
};

static int gas_read(int argc, char **argv, struct command *cmd,
		    struct plugin *plugin)
{
	const char *desc = "Read a gas register";
	void *map;
	int i;
	int ret = 0;

	struct argconfig_choice print_choices[] = {
		{"hex", HEX, "print in hexadecimal"},
		{"dec", DEC, "print in decimal"},
		{"str", STR, "print as an ascii string"},
		{0},
	};

	static struct {
		struct switchtec_dev *dev;
		unsigned long addr;
		unsigned long count;
		unsigned bytes;
		unsigned print_style;
	} cfg = {
		.bytes=4,
		.count=1,
		.print_style=HEX,
	};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"addr", 'a', "ADDR", CFG_LONG_SUFFIX, &cfg.addr, required_argument,
		 "address to read"},
		{"bytes", 'b', "NUM", CFG_POSITIVE, &cfg.bytes, required_argument,
		 "number of bytes to read per access (default 4)"},
		{"count", 'n', "NUM", CFG_LONG_SUFFIX, &cfg.count, required_argument,
		 "number of accesses to performe (default 1)"},
		{"print", 'p', "STYLE", CFG_CHOICES, &cfg.print_style, required_argument,
		 "printing style", .choices=print_choices},
		{NULL}};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));

	map = switchtec_gas_map(cfg.dev, 0);
	if (map == MAP_FAILED) {
		switchtec_perror("gas_map");
		return 1;
	}

	for (i = 0; i < cfg.count; i++) {
		ret = print_funcs[cfg.print_style](map, cfg.addr, cfg.bytes);
		cfg.addr += cfg.bytes;
		if (ret)
			break;
	}

	switchtec_gas_unmap(cfg.dev, map);
	return ret;
}

static int gas_write(int argc, char **argv, struct command *cmd,
		    struct plugin *plugin)
{
	const char *desc = "Write a gas register";
	void *map;
	int ret = 0;

	static struct {
		struct switchtec_dev *dev;
		unsigned long addr;
		unsigned bytes;
		unsigned long value;
		int assume_yes;
	} cfg = {
		.bytes=4,
	};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"addr", 'a', "ADDR", CFG_LONG_SUFFIX, &cfg.addr, required_argument,
		 "address to read"},
		{"bytes", 'b', "NUM", CFG_POSITIVE, &cfg.bytes, required_argument,
		 "number of bytes to read per access (default 4)"},
		{"value", 'v', "ADDR", CFG_LONG_SUFFIX, &cfg.value, required_argument,
		 "value to write"},
		{"yes", 'y', "", CFG_NONE, &cfg.assume_yes, no_argument,
		 "assume yes when prompted"},
		{NULL}};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));

	map = switchtec_gas_map(cfg.dev, 1);
	if (map == MAP_FAILED) {
		switchtec_perror("gas_map");
		return 1;
	}

	if (!cfg.assume_yes)
		fprintf(stderr,
			"Writing 0x%lx to %06lx (%d bytes).\n",
			cfg.value, cfg.addr, cfg.bytes);

	ret = ask_if_sure(cfg.assume_yes);
	if (ret)
		return ret;

	switch (cfg.bytes) {
	case 1: *((uint8_t *)(map + cfg.addr)) = cfg.value;  break;
	case 2: *((uint16_t *)(map + cfg.addr)) = cfg.value; break;
	case 4: *((uint32_t *)(map + cfg.addr)) = cfg.value; break;
	case 8: *((uint64_t *)(map + cfg.addr)) = cfg.value; break;
	default:
		fprintf(stderr, "invalid access width\n");
		return -1;
	}

	switchtec_gas_unmap(cfg.dev, map);
	return ret;
}
