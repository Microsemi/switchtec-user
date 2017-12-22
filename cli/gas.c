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
#include <switchtec/gas.h>

#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <errno.h>
#include <ctype.h>

static void print_line(unsigned long addr, uint8_t *bytes, size_t n)
{
	int i;

	printf("%08lx ", addr);
	for (i = 0; i < n; i++) {
		if (i == 8)
			printf(" ");
		printf(" %02x", bytes[i]);
	}

	for (; i < 16; i++) {
		printf("   ");
	}

	printf("  |");

	for (i = 0; i < 16; i++) {
		if (isprint(bytes[i]))
			printf("%c", bytes[i]);
		else
			printf(".");
	}

	printf("|\n");
}

static void hexdump_data(void __gas *map, size_t map_size)
{
	uint8_t line[16];
	uint8_t last_line[16];
	unsigned long addr = 0;
	size_t bytes;
	int last_match = 0;

	while (map_size) {
		bytes = map_size > sizeof(line) ? sizeof(line) : map_size;
		memcpy_from_gas(line, map, bytes);

		if (bytes != sizeof(line) ||
		    memcmp(last_line, line, sizeof(last_line))) {
			print_line(addr, line, bytes);
			last_match = 0;
		} else if (!last_match) {
			printf("*\n");
			last_match = 1;
		}

		map += bytes;
		map_size -= bytes;
		addr += bytes;
		memcpy(last_line, line, sizeof(last_line));
	}

	printf("%08lx\n", addr);
}

#ifndef _WIN32
#include <sys/mman.h>
#include <sys/wait.h>

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

static int pipe_to_hd_less(gasptr_t map, size_t map_size)
{
	int hd_fds[2];
	int less_fds[2];
	int less_pid, hd_pid;
	int ret;

	ret = pipe(less_fds);
	if (ret) {
		perror("pipe");
		return -1;
	}

	less_pid = spawn_proc(less_fds[0], STDOUT_FILENO, less_fds[1], "less");
	if (less_pid < 0) {
		perror("less");
		return -1;
	}
	close(STDOUT_FILENO);
	close(less_fds[0]);

	ret = pipe(hd_fds);
	if (ret) {
		perror("pipe");
		return -1;
	}

	hd_pid = spawn_proc(hd_fds[0], less_fds[1], hd_fds[1], "hd");
	if (hd_pid < 0) {
		perror("hd");
		return -1;
	}

	close(hd_fds[0]);
	close(less_fds[1]);

	ret = write_from_gas(hd_fds[1], map, map_size);
	close(hd_fds[1]);
	waitpid(less_pid, NULL, 0);
	return ret;
}
#else /* _WIN32 defined */

#include <fcntl.h>
#include <signal.h>

static int spawn(char *exe, int fd_in, int fd_out, int fd_err,
		  PROCESS_INFORMATION *pi)
{
	BOOL status;

	STARTUPINFO si = {
		.cb = sizeof(si),
		.hStdInput = (HANDLE)_get_osfhandle(fd_in),
		.hStdOutput = (HANDLE)_get_osfhandle(fd_out),
		.hStdError = (HANDLE)_get_osfhandle(fd_err),
		.dwFlags = STARTF_USESTDHANDLES,
	};

	status = CreateProcess(NULL, exe, NULL, NULL, TRUE, 0, NULL,
			       NULL, &si, pi);
	if (!status)
		return -1;

	close(fd_in);
	close(fd_out);

	return 0;
}

static PROCESS_INFORMATION proc_info;

static void wait_for_less(void)
{
	if (!proc_info.hProcess)
		return;

	WaitForSingleObject(proc_info.hProcess, INFINITE);
	CloseHandle(proc_info.hProcess);
	CloseHandle(proc_info.hThread);
}

static void int_handler(int sig)
{
	wait_for_less();
	exit(0);
}

static int pipe_to_hd_less(gasptr_t map, size_t map_size)
{
	int ret;
	int fd_stdout;
	int more_fds[2];

	ret = _pipe(more_fds, 256, _O_TEXT);
	if (ret) {
		perror("pipe");
		return -1;
	}

	fd_stdout = _dup(STDOUT_FILENO);
	_dup2(more_fds[1], STDOUT_FILENO);
	close(more_fds[1]);

	signal(SIGINT, int_handler);

	/*
	 * Set the new stdout to not be inheritable. If it is, then the child
	 * process will inherit it and we will no longer be able to close it.
	 */
	ret = SetHandleInformation((HANDLE)_get_osfhandle(STDOUT_FILENO),
				   HANDLE_FLAG_INHERIT, 0);
	if (!ret) {
		fprintf(stderr, "SetHandleInformation Failed: %ld\n",
			GetLastError());
		return -1;
	}

	ret = spawn("less", more_fds[0], fd_stdout, STDERR_FILENO, &proc_info);
	if (ret) {
		_dup2(fd_stdout, STDOUT_FILENO);
		hexdump_data(map, map_size);
		return 0;
	}

	hexdump_data(map, map_size);
	fclose(stdout);
	close(STDOUT_FILENO);

	wait_for_less();

	return 0;
}

#endif

static int gas_dump(int argc, char **argv)
{
	const char *desc = "Dump all gas registers";
	gasptr_t map;
	size_t map_size;
	int ret;

	static struct {
		struct switchtec_dev *dev;
		int count;
		int text;
	} cfg = {};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"count", 'n', "NUM", CFG_LONG_SUFFIX, &cfg.count, required_argument,
		 "number of bytes to dump (default is the entire gas space)"},
		{"text", 't', "", CFG_NONE, &cfg.text, no_argument,
		 "force outputing data in text format, default is to output in "
		 "text unless the output is a pipe, in which case binary is "
		 "output"},
		{NULL}};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));

	map = switchtec_gas_map(cfg.dev, 0, &map_size);
	if (map == SWITCHTEC_MAP_FAILED) {
		switchtec_perror("gas_map");
		return 1;
	}

	if (!cfg.count)
		cfg.count = map_size;

	if (cfg.text) {
		hexdump_data(map, cfg.count);
		return 0;
	}

	if (!isatty(STDOUT_FILENO)) {
		ret = write_from_gas(STDOUT_FILENO, map, cfg.count);
		return ret > 0;
	}

	return pipe_to_hd_less(map, cfg.count);
}

static int print_hex(void __gas *addr, int offset, int bytes)
{
	unsigned long long x;

	offset = offset & ~(bytes - 1);

	switch (bytes) {
	case 1: x = gas_read8(addr + offset);  break;
	case 2: x = gas_read16(addr + offset); break;
	case 4: x = gas_read32(addr + offset); break;
	case 8: x = gas_read64(addr + offset); break;
	default:
		fprintf(stderr, "invalid access width\n");
		return -1;
	}

	printf("%06X - 0x%0*" FMT_llX "\n", offset, bytes * 2, x);
	return 0;
}

static int print_dec(void __gas *addr, int offset, int bytes)
{
	unsigned long long x;

	offset = offset & ~(bytes - 1);

	switch (bytes) {
	case 1: x = gas_read8(addr + offset);  break;
	case 2: x = gas_read16(addr + offset); break;
	case 4: x = gas_read32(addr + offset); break;
	case 8: x = gas_read64(addr + offset); break;
	default:
		fprintf(stderr, "invalid access width\n");
		return -1;
	}

	printf("%06X - %" FMT_lld "\n", offset, x);
	return 0;
}

static int print_str(void __gas *addr, int offset, int bytes)
{
	char buf[bytes + 1];

	memset(buf, 0, bytes + 1);
	memcpy_from_gas(buf, addr + offset, bytes);

	printf("%06X - %s\n", offset, buf);
	return 0;
}

enum {
	HEX,
	DEC,
	STR,
};

static int (*print_funcs[])(void __gas *addr, int offset, int bytes) = {
	[HEX] = print_hex,
	[DEC] = print_dec,
	[STR] = print_str,
};

static int gas_read(int argc, char **argv)
{
	const char *desc = "Read a gas register";
	gasptr_t map;
	int i;
	int ret = 0;

	struct argconfig_choice print_choices[] = {
		{"hex", HEX, "print in hexadecimal"},
		{"dec", DEC, "print in decimal"},
		{"str", STR, "print as an ascii string"},
		{},
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
		 "number of accesses to perform (default 1)"},
		{"print", 'p', "STYLE", CFG_CHOICES, &cfg.print_style, required_argument,
		 "printing style", .choices=print_choices},
		{NULL}};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));

	map = switchtec_gas_map(cfg.dev, 0, NULL);
	if (map == SWITCHTEC_MAP_FAILED) {
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

static int gas_write(int argc, char **argv)
{
	const char *desc = "Write a gas register";
	void __gas *map;
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
		 "number of bytes to write (default 4)"},
		{"value", 'v', "ADDR", CFG_LONG_SUFFIX, &cfg.value, required_argument,
		 "value to write"},
		{"yes", 'y', "", CFG_NONE, &cfg.assume_yes, no_argument,
		 "assume yes when prompted"},
		{NULL}};

	argconfig_parse(argc, argv, desc, opts, &cfg, sizeof(cfg));

	map = switchtec_gas_map(cfg.dev, 1, NULL);
	if (map == SWITCHTEC_MAP_FAILED) {
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
	case 1: gas_write8(cfg.value, map + cfg.addr);  break;
	case 2: gas_write16(cfg.value, map + cfg.addr); break;
	case 4: gas_write32(cfg.value, map + cfg.addr); break;
	case 8: gas_write64(cfg.value, map + cfg.addr); break;
	default:
		fprintf(stderr, "invalid access width\n");
		return -1;
	}

	switchtec_gas_unmap(cfg.dev, map);
	return ret;
}

static const struct cmd commands[] = {
	{"dump", gas_dump, "dump the global address space"},
	{"read", gas_read, "read a register from the global address space"},
	{"write", gas_write, "write a register in the global address space"},
	{}
};

static struct subcommand subcmd = {
	.name = "gas",
	.cmds = commands,
	.desc = "Global Address Space Access (dangerous)",
	.long_desc = "These functions should be used with extreme caution only "
	      "if you know what you are doing. Any register accesses through "
	      "this interface is unsupported by Microsemi unless specifically "
	      "otherwise specified.",
};

REGISTER_SUBCMD(subcmd);
