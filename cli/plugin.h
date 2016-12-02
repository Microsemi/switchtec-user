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

#ifndef PLUGIN_H
#define PLUGIN_H

#include <stdbool.h>

struct program {
	const char *name;
	const char *version;
	const char *usage;
	const char *desc;
	const char *more;
	struct command **commands;
	struct plugin *extensions;
};

struct plugin {
	const char *name;
	const char *desc;
	struct command **commands;
	struct program *parent;
	struct plugin *next;
	struct plugin *tail;
};

struct command {
	char *name;
	char *help;
	int (*fn)(int argc, char **argv, struct command *command, struct plugin *plugin);
};

void usage(struct plugin *plugin);
void general_help(struct plugin *plugin);
int handle_plugin(int argc, char **argv, struct plugin *plugin);

#endif
