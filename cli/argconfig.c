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

#include <string.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>
#include <inttypes.h>

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

static void show_option(const struct argconfig_commandline_options *option)
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
			  const struct argconfig_commandline_options *options)
{
	const struct argconfig_commandline_options *s;
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

int argconfig_parse(int argc, char *argv[], const char *program_desc,
		    const struct argconfig_commandline_options *options,
		    void *config_out, size_t config_size)
{
	char *short_opts;
	char *endptr;
	struct option *long_opts;
	const struct argconfig_commandline_options *s;
	int c, option_index = 0, short_index = 0, options_count = 0;
	void *value_addr;

	errno = 0;
	for (s = options; s->option != NULL; s++)
		options_count++;

	long_opts = malloc(sizeof(struct option) * (options_count + 2));
	short_opts = malloc(sizeof(*short_opts) * (options_count * 3 + 4));

	for (s = options; (s->option != NULL) && (option_index < options_count);
	     s++) {
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
			    && s->default_value != NULL) {
				value_addr = (void *)(char *)s->default_value;

				long_opts[option_index].flag = value_addr;
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
			for (option_index = 0; option_index < options_count;
			     option_index++) {
				if (c == options[option_index].short_option)
					break;
			}
			if (option_index == options_count)
				continue;
			if (long_opts[option_index].flag) {
				*(uint8_t *)(long_opts[option_index].flag) = 1;
				continue;
			}
		}

		s = &options[option_index];
		value_addr = (void *)(char *)s->default_value;
		if (s->config_type == CFG_STRING) {
			*((char **)value_addr) = optarg;
		} else if (s->config_type == CFG_SIZE) {
			*((size_t *) value_addr) = strtol(optarg, &endptr, 0);
			if (errno || optarg == endptr) {
				fprintf(stderr,
					"Expected integer argument for '%s' but got '%s'!\n",
					long_opts[option_index].name, optarg);
				goto exit;
			}
		} else if (s->config_type == CFG_INT) {
			*((int *)value_addr) = strtol(optarg, &endptr, 0);
			if (errno || optarg == endptr) {
				fprintf(stderr,
					"Expected integer argument for '%s' but got '%s'!\n",
					long_opts[option_index].name, optarg);
				goto exit;
			}
		} else if (s->config_type == CFG_BOOL) {
			int tmp = strtol(optarg, &endptr, 0);
			if (errno || tmp < 0 || tmp > 1 || optarg == endptr) {
				fprintf(stderr,
					"Expected 0 or 1 argument for '%s' but got '%s'!\n",
					long_opts[option_index].name, optarg);
				goto exit;
			}
			*((int *)value_addr) = tmp;
		} else if (s->config_type == CFG_BYTE) {
			unsigned long tmp = strtoul(optarg, &endptr, 0);
			if (errno || tmp >= (1 << 8)  || optarg == endptr) {
				fprintf(stderr,
					"Expected byte argument for '%s' but got '%s'!\n",
					long_opts[option_index].name, optarg);
				goto exit;
			}
			*((uint8_t *) value_addr) = tmp;
		} else if (s->config_type == CFG_SHORT) {
			unsigned long tmp = strtoul(optarg, &endptr, 0);
			if (errno || tmp >= (1 << 16) || optarg == endptr) {
				fprintf(stderr,
					"Expected short argument for '%s' but got '%s'!\n",
					long_opts[option_index].name, optarg);
				goto exit;
			}
			*((uint16_t *) value_addr) = tmp;
		} else if (s->config_type == CFG_POSITIVE) {
			uint32_t tmp = strtoul(optarg, &endptr, 0);
			if (errno || optarg == endptr) {
				fprintf(stderr,
					"Expected word argument for '%s' but got '%s'!\n",
					long_opts[option_index].name, optarg);
				goto exit;
			}
			*((uint32_t *) value_addr) = tmp;
		} else if (s->config_type == CFG_INCREMENT) {
			(*((int *)value_addr))++;
		} else if (s->config_type == CFG_LONG) {
			*((unsigned long *)value_addr) = strtoul(optarg, &endptr, 0);
			if (errno || optarg == endptr) {
				fprintf(stderr,
					"Expected long integer argument for '%s' but got '%s'!\n",
					long_opts[option_index].name, optarg);
				goto exit;
			}
		} else if (s->config_type == CFG_LONG_SUFFIX) {
			*((long *)value_addr) = suffix_binary_parse(optarg);
			if (errno) {
				fprintf(stderr,
					"Expected long suffixed integer argument for '%s' but got '%s'!\n",
					long_opts[option_index].name, optarg);
				goto exit;
			}
		} else if (s->config_type == CFG_DOUBLE) {
			*((double *)value_addr) = strtod(optarg, &endptr);
			if (errno || optarg == endptr) {
				fprintf(stderr,
					"Expected float argument for '%s' but got '%s'!\n",
					long_opts[option_index].name, optarg);
				goto exit;
			}
		} else if (s->config_type == CFG_FILE_A ||
			   s->config_type == CFG_FILE_R ||
			   s->config_type == CFG_FILE_W ||
			   s->config_type == CFG_FILE_AP ||
			   s->config_type == CFG_FILE_RP ||
			   s->config_type == CFG_FILE_WP) {
			const char *fopts = "";
			if (s->config_type == CFG_FILE_A)
				fopts = "a";
			else if (s->config_type == CFG_FILE_R)
				fopts = "r";
			else if (s->config_type == CFG_FILE_W)
				fopts = "w";
			else if (s->config_type == CFG_FILE_AP)
				fopts = "a+";
			else if (s->config_type == CFG_FILE_RP)
				fopts = "r+";
			else if (s->config_type == CFG_FILE_WP)
				fopts = "w+";

			FILE *f = fopen(optarg, fopts);
			if (f == NULL) {
				fprintf(stderr, "Unable to open %s file: %s\n",
					s->option, optarg);
				goto exit;
			}
			*((FILE **) value_addr) = f;
		}
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
