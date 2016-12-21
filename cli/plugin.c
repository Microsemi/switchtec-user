////////////////////////////////////////////////////////////////////////
//
// Copyright 2016 Keith Busch
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
// Forked off of nvme-cli:
//   https://github.com/linux-nvme/nvme-cli/
//
////////////////////////////////////////////////////////////////////////

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "plugin.h"
#include "argconfig.h"

static int version(struct plugin *plugin)
{
	struct program *prog = plugin->parent;

	if (plugin->name)
		printf("%s %s version %s\n", prog->name, plugin->name, prog->version);
	else
		printf("%s version %s\n", prog->name, prog->version);
	return 0;
}

static int help(int argc, char **argv, struct plugin *plugin)
{
	char man[0x100];
	struct program *prog = plugin->parent;

	if (argc == 1) {
		general_help(plugin);
		return 0;
	}

	if (plugin->name)
		sprintf(man, "%s-%s-%s", prog->name, plugin->name, argv[1]);
	else
		sprintf(man, "%s-%s", prog->name, argv[1]);
	if (execlp("man", "man", man, (char *)NULL)) {
		perror(argv[1]);
		exit(errno);
	}
	return 0;
}

void usage(struct plugin *plugin)
{
	struct program *prog = plugin->parent;

	if (plugin->name)
		printf("usage: %s %s %s\n", prog->name, plugin->name, prog->usage);
	else
		printf("usage: %s %s\n", prog->name, prog->usage);
}

static void print_completions(struct plugin *plugin)
{
	int i;

	if (!getenv("SWITCHTEC_COMPLETE"))
		return;

	for (i = 0; plugin->commands[i]; i++)
		printf(" %s", plugin->commands[i]->name);
	printf("\n");
	exit(0);
}

void general_help(struct plugin *plugin)
{
	struct program *prog = plugin->parent;
	struct plugin *extension;
	unsigned i = 0;

	print_completions(plugin);

	printf("%s-%s\n", prog->name, prog->version);

	usage(plugin);

	printf("\n");
	print_word_wrapped(prog->desc, 0, 0);
	printf("\n");

	if (plugin->desc) {
		printf("\n");
		print_word_wrapped(plugin->desc, 0, 0);
		printf("\n");
	}

	printf("\nThe following are all implemented sub-commands:\n");

	for (; plugin->commands[i]; i++)
		printf("  %-*s %s\n", 15, plugin->commands[i]->name,
					plugin->commands[i]->help);

	printf("  %-*s %s\n", 15, "version", "Shows the program version");
	printf("  %-*s %s\n", 15, "help", "Display this help");
	printf("\n");

	if (plugin->name)
		printf("See '%s %s help <command>' for more information on a specific command\n",
			prog->name, plugin->name);
	else
		printf("See '%s help <command>' for more information on a specific command\n",
			prog->name);

	/* The first plugin is the built-in. If we're not showing help for the
	 * built-in, don't show the program's other extensions */
	if (plugin->name)
		return;

	extension = prog->extensions->next;
	if (!extension)
		return;

	printf("\nThe following are all installed plugin extensions:\n");
	while (extension) {
		printf("  %-*s %s\n", 15, extension->name, extension->desc);
		extension = extension->next;
	}
	printf("\nSee '%s <plugin> help' for more information on a plugin\n",
			prog->name);
}

int handle_plugin(int argc, char **argv, struct plugin *plugin)
{
	unsigned i = 0;
	char *str = argv[0];
	char use[0x100];

	struct plugin *extension;
	struct program *prog = plugin->parent;

	if (!argc) {
		general_help(plugin);
		return 0;
	}

	if (!plugin->name)
		sprintf(use, "%s %s", prog->name, str);
	else
		sprintf(use, "%s %s %s", prog->name, plugin->name, str);
	argconfig_append_usage(use);

	/* translate --help and --version into commands */
	while (*str == '-')
		str++;

	for (; plugin->commands[i]; i++) {
		struct command *cmd = plugin->commands[i];

		if (!strcmp(str, "help"))
			return help(argc, argv, plugin);
		if (!strcmp(str, "version"))
			return version(plugin);
		if (strcmp(str, cmd->name))
			continue;

		return (cmd->fn(argc, argv, cmd, plugin));
	}

	/* Check extensions only if this is running the built-in plugin */
	if (plugin->name) {
		printf("ERROR: Invalid sub-command '%s' for plugin %s\n", str, plugin->name);
		return -ENOTSUP;
        }

	extension = plugin->next;
	while (extension) {
		if (!strcmp(str, extension->name))
			return handle_plugin(argc - 1, &argv[1], extension);

		/* If the command is executed with the extension name and
		 * command together ("plugin-command"), run the plug in */
		if (!strncmp(str, extension->name, strlen(extension->name))) {
			argv[0] += strlen(extension->name);
			while (*argv[0] == '-')
				argv[0]++;
			return handle_plugin(argc, &argv[0], extension);
		}
		extension = extension->next;
	}

	print_completions(plugin);
	printf("ERROR: Invalid sub-command '%s'\n", str);
	return -ENOTSUP;
}
