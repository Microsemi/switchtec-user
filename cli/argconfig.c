////////////////////////////////////////////////////////////////////////
//
// Copyright 2014 PMC-Sierra, Inc.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
//
//   Author: Logan Gunthorpe
//
//   Date:   Oct 23 2014
//
//   Description:
//     Functions for parsing command line options.
//
////////////////////////////////////////////////////////////////////////

#include "argconfig.h"
#include "suffix.h"

#include <fcntl.h>
#include <string.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>
#include <inttypes.h>
#include <unistd.h>

static argconfig_help_func *help_funcs[MAX_HELP_FUNC] = { NULL };

static char append_usage_str[100] = "";

void argconfig_append_usage(const char *str)
{
	strncat(append_usage_str, str, sizeof(append_usage_str) -
		strlen(append_usage_str) - 1);
}

void print_word_wrapped(const char *s, int indent, int start)
{
	const int width = 76;
	const char *c, *t;
	int next_space = -1;
	int last_line = indent;

	while (start < indent) {
		putc(' ', stderr);
		start++;
	}

	for (c = s; *c != 0; c++) {
		if (*c == '\n')
			goto new_line;

		if (*c == ' ' || next_space < 0) {
			next_space = 0;
			for (t = c + 1; *t != 0 && *t != ' '; t++)
				next_space++;

			if (((int)(c - s) + start + next_space) > (last_line - indent + width)) {
				int i;
new_line:
				last_line = (int) (c-s) + start;
				putc('\n', stderr);
				for (i = 0; i < indent; i++)
					putc(' ', stderr);
				start = indent;
				continue;
			}
		}
		putc(*c, stderr);
	}
}

static void show_option(const struct argconfig_options *option)
{
	char buffer[0x1000];
	char *b = buffer;

	b += sprintf(b, "  [ ");
	if (option->option) {
		b += sprintf(b, " --%s", option->option);
		if (option->argument_type == optional_argument)
			b += sprintf(b, "[=<%s>]", option->meta ? option->meta : "arg");
		if (option->argument_type == required_argument)
			b += sprintf(b, "=<%s>", option->meta ? option->meta : "arg");
		if (option->short_option)
			b += sprintf(b, ",");
	}
	if (option->short_option) {
		b += sprintf(b, " -%c", option->short_option);
		if (option->argument_type == optional_argument)
			b += sprintf(b, " [<%s>]", option->meta ? option->meta : "arg");
		if (option->argument_type == required_argument)
			b += sprintf(b, " <%s>", option->meta ? option->meta : "arg");
	}
	b += sprintf(b, " ] ");

	fprintf(stderr, "%s", buffer);
	if (option->help) {
		print_word_wrapped("--- ", 40, b - buffer);
		print_word_wrapped(option->help, 44, 44);
	}
	fprintf(stderr, "\n");
}

void argconfig_print_usage(void)
{
	printf("Usage: %s\n", append_usage_str);
}

void argconfig_print_help(const char *program_desc,
			  const struct argconfig_options *options)
{
	const struct argconfig_options *s;
	const char *optstring = "";

	if (options->option != NULL)
		optstring = " [OPTIONS]";

	printf("\033[1mUsage: %s%s\033[0m\n\n",
	       append_usage_str, optstring);

	print_word_wrapped(program_desc, 0, 0);

	if (options->option == NULL) {
		printf("\n\n");
		return;
	}

	printf("\n\n\033[1mOptions:\033[0m\n");

	for (s = options; (s->option != NULL) && (s != NULL); s++)
		show_option(s);
}

static int cfg_none_handler(const char *optarg, void *value_addr,
			    const struct argconfig_options *opt)
{
	*((int *)value_addr) = 1;
	return 0;
}

static int cfg_string_handler(const char *optarg, void *value_addr,
			      const struct argconfig_options *opt)
{
	*((const char **)value_addr) = optarg;
	return 0;
}

static int cfg_int_handler(const char *optarg, void *value_addr,
			   const struct argconfig_options *opt)
{
	char *endptr;

	*((int *)value_addr) = strtol(optarg, &endptr, 0);

	if (errno || optarg == endptr) {
		fprintf(stderr,
			"Expected integer argument for '--%s/-%c' "
			"but got '%s'!\n",
			opt->option, opt->short_option, optarg);
		return 1;
	}

	return 0;
}

static int cfg_size_handler(const char *optarg, void *value_addr,
			    const struct argconfig_options *opt)
{
	char *endptr;

	*((size_t *) value_addr) = strtol(optarg, &endptr, 0);
	if (errno || optarg == endptr) {
		fprintf(stderr,
			"Expected integer argument for '--%s/-%c' "
			"but got '%s'!\n",
			opt->option, opt->short_option, optarg);
		return 1;
	}

	return 0;
}

static int cfg_long_handler(const char *optarg, void *value_addr,
			    const struct argconfig_options *opt)
{
	char *endptr;

	*((unsigned long *)value_addr) = strtoul(optarg, &endptr, 0);
	if (errno || optarg == endptr) {
		fprintf(stderr,
			"Expected long integer argument for '--%s/-%c' "
			"but got '%s'!\n",
			opt->option, opt->short_option, optarg);
		return 1;
	}

	return 0;
}

static int cfg_long_suffix_handler(const char *optarg, void *value_addr,
				   const struct argconfig_options *opt)
{
	*((long *)value_addr) = suffix_binary_parse(optarg);
	if (errno) {
		fprintf(stderr,
			"Expected suffixed integer argument for '--%s/-%c' "
			"but got '%s'!\n",
			opt->option, opt->short_option, optarg);
		return 1;
	}

	return 0;
}

static int cfg_double_handler(const char *optarg, void *value_addr,
			      const struct argconfig_options *opt)
{
	char *endptr;

	*((double *)value_addr) = strtod(optarg, &endptr);
	if (errno || optarg == endptr) {
		fprintf(stderr,
			"Expected float argument for '--%s/-%c' "
			"but got '%s'!\n",
			opt->option, opt->short_option, optarg);
		return 1;
	}
	return 0;
}

static int cfg_bool_handler(const char *optarg, void *value_addr,
			    const struct argconfig_options *opt)
{
	char *endptr;

	int tmp = strtol(optarg, &endptr, 0);
	if (errno || tmp < 0 || tmp > 1 || optarg == endptr) {
		fprintf(stderr,
			"Expected 0 or 1 argument for '--%s/-%c' "
			"but got '%s'!\n",
			opt->option, opt->short_option, optarg);
		return 1;
	}
	*((int *)value_addr) = tmp;

	return 0;
}

static int cfg_byte_handler(const char *optarg, void *value_addr,
			    const struct argconfig_options *opt)
{
	char *endptr;

	unsigned long tmp = strtoul(optarg, &endptr, 0);
	if (errno || tmp >= (1 << 8)  || optarg == endptr) {
		fprintf(stderr,
			"Expected byte argument for '--%s/-%c' "
			"but got '%s'!\n",
			opt->option, opt->short_option, optarg);
		return 1;
	}
	*((uint8_t *) value_addr) = tmp;
	return 0;
}

static int cfg_short_handler(const char *optarg, void *value_addr,
			     const struct argconfig_options *opt)
{
	char *endptr;

	unsigned long tmp = strtoul(optarg, &endptr, 0);
	if (errno || tmp >= (1 << 16)  || optarg == endptr) {
		fprintf(stderr,
			"Expected short argument for '--%s/-%c' "
			"but got '%s'!\n",
			opt->option, opt->short_option, optarg);
		return 1;
	}
	*((uint16_t *) value_addr) = tmp;
	return 0;
}

static int cfg_positive_handler(const char *optarg, void *value_addr,
				const struct argconfig_options *opt)
{
	char *endptr;

	uint32_t tmp = strtoul(optarg, &endptr, 0);
	if (errno || optarg == endptr) {
		fprintf(stderr,
			"Expected positive argument for '--%s/-%c' "
			"but got '%s'!\n",
			opt->option, opt->short_option, optarg);
		return 1;
	}
	*((unsigned *) value_addr) = tmp;
	return 0;
}

static int cfg_increment_handler(const char *optarg, void *value_addr,
				 const struct argconfig_options *opt)
{
	(*((int *)value_addr))++;
	return 0;
}

static int cfg_file_handler(const char *optarg, void *value_addr,
			    const struct argconfig_options *opt)
{
	const char *fopts = "";
	switch(opt->config_type) {
	case CFG_FILE_A: fopts= "a"; break;
	case CFG_FILE_R: fopts= "r"; break;
	case CFG_FILE_W: fopts= "w"; break;
	case CFG_FILE_AP: fopts= "a+"; break;
	case CFG_FILE_RP: fopts= "r+"; break;
	case CFG_FILE_WP: fopts= "w+"; break;
	default: return 1;
	}

	FILE *f = fopen(optarg, fopts);
	if (f == NULL) {
		perror(optarg);
		return 1;
	}
	*((FILE **) value_addr) = f;

	return 0;
}

static void set_fd_path(void *value_addr, int fd, const char *path)
{
	*((int *) value_addr) = fd;
	value_addr += sizeof(int *);
	*((const char **) value_addr) = path;
}

static int cfg_fd_handler(const char *optarg, void *value_addr,
			  const struct argconfig_options *opt)
{
	int fd;
	int flags;

	if (strcmp(optarg, "-") == 0) {
		if (opt->config_type == CFG_FD_WR)
			set_fd_path(value_addr, STDOUT_FILENO, "stdout");
		else
			set_fd_path(value_addr, STDIN_FILENO, "stdin");

		return 0;
	}

	switch(opt->config_type) {
	case CFG_FD_WR: flags = O_CREAT | O_TRUNC | O_WRONLY; break;
	case CFG_FD_RD: flags = O_RDONLY; break;
	default: return 1;
	}

	fd = open(optarg, flags, 0666);
	if (fd < 0) {
		perror(optarg);
		return 1;
	}

	set_fd_path(value_addr, fd, optarg);

	return 0;
}

static int cfg_custom_handler(const char *optarg, void *value_addr,
			      const struct argconfig_options *opt)
{
	if (opt->custom_handler == NULL) {
		fprintf(stderr, "FATAL: custom handler not specified "
			"for CFG_CUSTOM!\n");
		return 1;
	}

	return opt->custom_handler(optarg, value_addr, opt);
}

typedef int (*type_handler)(const char *optarg, void *value_addr,
			    const struct argconfig_options *opt);

static type_handler cfg_type_handlers[_CFG_MAX_TYPES] = {
	[CFG_NONE] = cfg_none_handler,
	[CFG_STRING] = cfg_string_handler,
	[CFG_INT] = cfg_int_handler,
	[CFG_SIZE] = cfg_size_handler,
	[CFG_LONG] = cfg_long_handler,
	[CFG_LONG_SUFFIX] = cfg_long_suffix_handler,
	[CFG_DOUBLE] = cfg_double_handler,
	[CFG_BOOL] = cfg_bool_handler,
	[CFG_BYTE] = cfg_byte_handler,
	[CFG_SHORT] = cfg_short_handler,
	[CFG_POSITIVE] = cfg_positive_handler,
	[CFG_INCREMENT] = cfg_increment_handler,
	[CFG_FILE_A] = cfg_file_handler,
	[CFG_FILE_A] = cfg_file_handler,
	[CFG_FILE_W] = cfg_file_handler,
	[CFG_FILE_R] = cfg_file_handler,
	[CFG_FILE_AP] = cfg_file_handler,
	[CFG_FILE_WP] = cfg_file_handler,
	[CFG_FILE_RP] = cfg_file_handler,
	[CFG_FD_WR] = cfg_fd_handler,
	[CFG_FD_RD] = cfg_fd_handler,
	[CFG_CUSTOM] = cfg_custom_handler,
};

static int handle(const char *optarg, void *value_addr,
		  const struct argconfig_options *s)
{
	if (s->config_type >= _CFG_MAX_TYPES ||
	    cfg_type_handlers[s->config_type] == NULL) {
		fprintf(stderr, "FATAL: unknown config type: %d\n",
			s->config_type);
		return 1;
	}

	return cfg_type_handlers[s->config_type](optarg, value_addr, s);
}

static const struct argconfig_options *
get_option(const struct argconfig_options * options,
	   int option_index)
{
	const struct argconfig_options *s;

	for (s = options; s->option; s++) {
		if (s->argument_type != optional_positional &&
		    s->argument_type != required_positional)
			continue;
		if (!option_index--)
			return s;
	}

	return s;
}

int argconfig_parse(int argc, char *argv[], const char *program_desc,
		    const struct argconfig_options *options,
		    void *config_out, size_t config_size)
{
	char *short_opts;
	struct option *long_opts;
	const struct argconfig_options *s;
	int c, option_index = 0, short_index = 0, options_count = 0;
	void *value_addr;

	errno = 0;
	for (s = options; s->option != NULL; s++) {
		if (s->argument_type == optional_positional ||
		    s->argument_type == required_positional)
			continue;
		options_count++;
	}

	long_opts = malloc(sizeof(struct option) * (options_count + 2));
	short_opts = malloc(sizeof(*short_opts) * (options_count * 3 + 4));

	for (s = options; (s->option != NULL) && (option_index < options_count);
	     s++) {

		if (s->argument_type == optional_positional ||
		    s->argument_type == required_positional)
			continue;

		if (s->short_option != 0) {
			short_opts[short_index++] = s->short_option;
			if (s->argument_type == required_argument ||
			    s->argument_type == optional_argument)
				short_opts[short_index++] = ':';
			if (s->argument_type == optional_argument)
				short_opts[short_index++] = ':';
		}
		if (s->option && strlen(s->option)) {
			long_opts[option_index].name = s->option;
			long_opts[option_index].has_arg = s->argument_type;

			if (s->argument_type == no_argument
			    && s->value_addr != NULL) {
				long_opts[option_index].flag = s->value_addr;
				long_opts[option_index].val = 1;
			} else {
				long_opts[option_index].flag = NULL;
				long_opts[option_index].val = 0;
			}
		}
		option_index++;
	}

	long_opts[option_index].name = "help";
	long_opts[option_index].flag = NULL;
	long_opts[option_index].val = 'h';
	option_index++;

	long_opts[option_index].name = NULL;
	long_opts[option_index].flag = NULL;
	long_opts[option_index].val = 0;

	short_opts[short_index++] = '?';
	short_opts[short_index++] = 'h';
	short_opts[short_index] = 0;

	optind = 0;
	while ((c = getopt_long_only(argc, argv, short_opts, long_opts,
				&option_index)) != -1) {
		if (c != 0) {
			if (c == '?' || c == 'h') {
				argconfig_print_help(program_desc, options);
				goto exit;
			}

			for (s = options; s->option; s++) {
				if (c == s->short_option)
					break;
			}

			if (s == NULL)
				continue;
		} else {
			s = get_option(options, option_index);
		}

		if (handle(optarg, s->value_addr, s))
		    goto exit;
	}

	for (s = options; (s->option != NULL); s++) {
		if (s->argument_type != optional_positional &&
		    s->argument_type != required_positional)
			continue;

		if (s->argument_type == required_positional &&
		    optind >= argc)
		{
			argconfig_print_usage();
			goto exit;
		}

		if (optind >= argc)
			continue;

		if (handle(argv[optind], s->value_addr, s))
		    goto exit;

		optind++;
	}

	for (s = options; (s->option != NULL); s++) {
		if (!s->force_default)
			continue;

		value_addr = (void *)(char *)s->value_addr;

		if (*(int *)value_addr)
			continue;

		if (handle(s->force_default, s->value_addr, s))
		    goto exit;
	}


	free(short_opts);
	free(long_opts);

	return 0;
 exit:
	free(short_opts);
	free(long_opts);
	exit(1);
}

void argconfig_register_help_func(argconfig_help_func * f)
{
	int i;
	for (i = 0; i < MAX_HELP_FUNC; i++) {
		if (help_funcs[i] == NULL) {
			help_funcs[i] = f;
			help_funcs[i + 1] = NULL;
			break;
		}
	}
}
