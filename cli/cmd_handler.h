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

/*
 * Stage 1
 *
 * Define function prototypes.
 */

#undef NAME
#define NAME(n, d)

#undef ENTRY
#define ENTRY(n, h, f) \
static int f(int argc, char **argv, struct command *command, struct plugin *plugin);

#undef COMMAND_LIST
#define COMMAND_LIST(args...) args

#undef PLUGIN
#define PLUGIN(name, cmds) cmds

#include CMD_INCLUDE(CMD_INC_FILE)

/*
 * Stage 2
 *
 * Define command structures.
 */

#undef NAME
#define NAME(n, d)

#undef ENTRY
#define ENTRY(n, h, f)			\
static struct command f ## _cmd = {	\
	.name = n, 			\
	.help = h, 			\
	.fn = f, 			\
};

#undef COMMAND_LIST
#define COMMAND_LIST(args...) args

#undef PLUGIN
#define PLUGIN(name, cmds) cmds

#include CMD_INCLUDE(CMD_INC_FILE)

/*
 * Stage 3
 *
 * Generate list of commands for the plugin.
 */

#undef NAME
#define NAME(n, d)

#undef ENTRY
#define ENTRY(n, h, f) &f ## _cmd,

#undef COMMAND_LIST
#define COMMAND_LIST(args...)	\
static struct command *commands[] = {	\
	args				\
	NULL,				\
};

#undef PLUGIN
#define PLUGIN(name, cmds) cmds

#include CMD_INCLUDE(CMD_INC_FILE)

/*
 * Stage 4
 *
 * Define and register plugin
 */

#undef NAME
#define NAME(n, d) .name = n, .desc = d,

#undef COMMAND_LIST
#define COMMAND_LIST(args...)

#undef PLUGIN
#define PLUGIN(name, cmds)				\
static struct plugin plugin = {				\
	name						\
	.commands = commands				\
}; 							\
							\
static void init(void) __attribute__((constructor)); 	\
static void init(void)					\
{							\
	register_extension(&plugin);			\
}

#include CMD_INCLUDE(CMD_INC_FILE)
