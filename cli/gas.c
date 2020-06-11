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

static void print_line(unsigned long addr, uint8_t *bytes, size_t n, int error)
{
	int i;

	printf("%08lx ", addr);
	for (i = 0; i < n; i++) {
		if (i == 8)
			printf(" ");

		if (error)
			printf(" XX");
		else
			printf(" %02x", bytes[i]);
	}

	for (; i < 16; i++) {
		printf("   ");
	}

	printf("  |");

	for (i = 0; i < 16; i++) {
		if (error)
			printf("X");
		else if (isprint(bytes[i]))
			printf("%c", bytes[i]);
		else
			printf(".");
	}

	printf("|\n");
}

static void hexdump_data(struct switchtec_dev *dev, void __gas *map,
			 size_t map_size, int (*is_alive)(void))
{
	uint8_t line[16];
	uint8_t last_line[16];
	unsigned long addr = 0;
	size_t bytes;
	int last_match = 0;
	int err;

	while (map_size) {
		if (is_alive && !is_alive())
			return;

		bytes = map_size > sizeof(line) ? sizeof(line) : map_size;
		err = memcpy_from_gas(dev, line, map, bytes);
		if (err && errno == EPERM) {
			fprintf(stderr, "GAS dump: permission denied\n");
			return;
		}

		if (bytes != sizeof(line) ||
		    memcmp(last_line, line, sizeof(last_line))) {
			print_line(addr, line, bytes, err);
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

static int pipe_to_hd_less(struct switchtec_dev *dev, gasptr_t map,
			   size_t map_size)
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

	ret = write_from_gas(dev, hd_fds[1], map, map_size);
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

static int is_less_alive(void)
{
	DWORD exit_code;

	GetExitCodeProcess(proc_info.hProcess, &exit_code);

	return exit_code == STILL_ACTIVE;
}

static void int_handler(int sig)
{
	wait_for_less();
	exit(0);
}

static int pipe_to_hd_less(struct switchtec_dev *dev, gasptr_t map,
			   size_t map_size)
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
		hexdump_data(dev, map, map_size, NULL);
		return 0;
	}

	hexdump_data(dev, map, map_size, is_less_alive);
	fclose(stdout);
	close(STDOUT_FILENO);

	wait_for_less();

	return 0;
}

#endif

#define CMD_DESC_DUMP "dump all Global Address Space registers"

static int gas_dump(int argc, char **argv)
{
	gasptr_t map;
	size_t map_size;
	int ret;

	static struct {
		struct switchtec_dev *dev;
		size_t count;
		int text;
	} cfg = {};
	const struct argconfig_options opts[] = {
		DEVICE_OPTION,
		{"count", 'n', "NUM", CFG_SIZE_SUFFIX, &cfg.count, required_argument,
		 "number of bytes to dump (default is the entire GAS space)"},
		{"text", 't', "", CFG_NONE, &cfg.text, no_argument,
		 "force outputting data in text format, default is to output in "
		 "text unless the output is a pipe, in which case binary is "
		 "output"},
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_DUMP, opts, &cfg, sizeof(cfg));

	map = switchtec_gas_map(cfg.dev, 0, &map_size);
	if (map == SWITCHTEC_MAP_FAILED) {
		switchtec_perror("gas_map");
		return 1;
	}

	if (!cfg.count || cfg.count > map_size)
		cfg.count = map_size;

	if (cfg.text) {
		hexdump_data(cfg.dev, map, cfg.count, NULL);
		return 0;
	}

	if (!isatty(STDOUT_FILENO)) {
		ret = write_from_gas(cfg.dev, STDOUT_FILENO, map, cfg.count);
		return ret > 0;
	}

	return pipe_to_hd_less(cfg.dev, map, cfg.count);
}

static int read_gas(struct switchtec_dev *dev, void __gas *addr,
		    int bytes, unsigned long long *val)
{
	int ret = 0;

	uint8_t u8;
	uint16_t u16;
	uint32_t u32;
	uint64_t u64;

	switch (bytes) {
	case 1:
		ret = gas_read8(dev, addr, &u8);
		*val = u8;
		break;
	case 2:
		ret = gas_read16(dev, addr, &u16);
		*val = u16;
		break;
	case 4:
		ret = gas_read32(dev, addr, &u32);
		*val = u32;
		break;
	case 8:
		ret = gas_read64(dev, addr, &u64);
		*val = u64;
		break;
	default:
		errno = EINVAL;
		return -1;
	}

	return ret;
}

static int print_hex(struct switchtec_dev *dev, void __gas *addr,
		     int offset, int bytes)
{
	unsigned long long x;
	int ret;

	offset = offset & ~(bytes - 1);

	if (bytes != 1 && bytes != 2 && bytes != 4 && bytes != 8) {
		fprintf(stderr, "invalid access width\n");
		return -1;
	}

	ret = read_gas(dev, addr + offset, bytes, &x);
	if (ret) {
		switchtec_perror("gas read");
		return -1;
	}

	printf("%06X - 0x%0*llX\n", offset, bytes * 2, x);
	return 0;
}

static int print_dec(struct switchtec_dev *dev, void __gas *addr,
		     int offset, int bytes)
{
	unsigned long long x;
	int ret;

	offset = offset & ~(bytes - 1);

	if (bytes != 1 && bytes != 2 && bytes != 4 && bytes != 8) {
		fprintf(stderr, "invalid access width\n");
		return -1;
	}

	ret = read_gas(dev, addr + offset, bytes, &x);
	if (ret) {
		switchtec_perror("gas read");
		return -1;
	}

	printf("%06X - %lld\n", offset, x);
	return 0;
}

static int print_str(struct switchtec_dev *dev, void __gas *addr,
		     int offset, int bytes)
{
	char buf[bytes + 1];
	int ret;

	memset(buf, 0, bytes + 1);

	ret = memcpy_from_gas(dev, buf, addr + offset, bytes);
	if (ret) {
		switchtec_perror("gas read");
		return -1;
	}

	printf("%06X - %s\n", offset, buf);
	return 0;
}

enum {
	HEX,
	DEC,
	STR,
};

static int (*print_funcs[])(struct switchtec_dev *dev, void __gas *addr,
			    int offset, int bytes) = {
	[HEX] = print_hex,
	[DEC] = print_dec,
	[STR] = print_str,
};

#define CMD_DESC_READ "read a register from the Global Address Space"

static int gas_read(int argc, char **argv)
{
	gasptr_t map;
	size_t map_size;
	int i;
	int ret = 0;

	struct argconfig_choice print_choices[] = {
		{"hex", HEX, "print in hexadecimal"},
		{"dec", DEC, "print in decimal"},
		{"str", STR, "print as an ASCII string"},
		{},
	};

	static struct {
		struct switchtec_dev *dev;
		unsigned long addr;
		size_t count;
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
		{"count", 'n', "NUM", CFG_SIZE_SUFFIX, &cfg.count, required_argument,
		 "number of accesses to perform (default 1)"},
		{"print", 'p', "STYLE", CFG_CHOICES, &cfg.print_style, required_argument,
		 "printing style", .choices=print_choices},
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_READ, opts, &cfg, sizeof(cfg));

	map = switchtec_gas_map(cfg.dev, 0, &map_size);
	if (map == SWITCHTEC_MAP_FAILED) {
		switchtec_perror("gas_map");
		return 1;
	}


	for (i = 0; i < cfg.count; i++) {
		if ((cfg.addr + cfg.bytes) > map_size) {
			fprintf(stderr, "Out of range for Global Address Space\n");
			return -1;
		}

		ret = print_funcs[cfg.print_style](cfg.dev, map, cfg.addr,
						   cfg.bytes);
		cfg.addr += cfg.bytes;
		if (ret)
			break;
	}

	switchtec_gas_unmap(cfg.dev, map);
	return ret;
}

#define CMD_DESC_WRITE "write a register in the Global Address Space"

static int gas_write(int argc, char **argv)
{
	void __gas *map;
	size_t map_size;
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
		 "address to write"},
		{"bytes", 'b', "NUM", CFG_POSITIVE, &cfg.bytes, required_argument,
		 "number of bytes to write (default 4)"},
		{"value", 'v', "VAL", CFG_LONG_SUFFIX, &cfg.value, required_argument,
		 "value to write"},
		{"yes", 'y', "", CFG_NONE, &cfg.assume_yes, no_argument,
		 "assume yes when prompted"},
		{NULL}};

	argconfig_parse(argc, argv, CMD_DESC_WRITE, opts, &cfg, sizeof(cfg));

	map = switchtec_gas_map(cfg.dev, 1, &map_size);
	if (map == SWITCHTEC_MAP_FAILED) {
		switchtec_perror("gas_map");
		return 1;
	}

	if ((cfg.addr + cfg.bytes) > map_size) {
		fprintf(stderr, "Out of range for Global Address Space\n");
		return -1;
	}

	if (!cfg.assume_yes)
		fprintf(stderr,
			"Writing 0x%lx to %06lx (%d bytes).\n",
			cfg.value, cfg.addr, cfg.bytes);

	ret = ask_if_sure(cfg.assume_yes);
	if (ret)
		return ret;

	switch (cfg.bytes) {
	case 1: gas_write8(cfg.dev, cfg.value, map + cfg.addr);  break;
	case 2: gas_write16(cfg.dev, cfg.value, map + cfg.addr); break;
	case 4: gas_write32(cfg.dev, cfg.value, map + cfg.addr); break;
	case 8: gas_write64(cfg.dev, cfg.value, map + cfg.addr); break;
	default:
		fprintf(stderr, "invalid access width\n");
		return -1;
	}

	switchtec_gas_unmap(cfg.dev, map);
	return ret;
}

static const struct cmd commands[] = {
	{"dump", gas_dump, CMD_DESC_DUMP},
	{"read", gas_read, CMD_DESC_READ},
	{"write", gas_write, CMD_DESC_WRITE},
	{}
};

static struct subcommand subcmd = {
	.name = "gas",
	.cmds = commands,
	.desc = "Global Address Space Access (dangerous)",
	.long_desc = "These functions should be used with extreme caution only "
	      "if you know what you are doing. Any register accesses through "
	      "this interface are unsupported by Microsemi unless specifically "
	      "otherwise specified.",
};

REGISTER_SUBCMD(subcmd);
