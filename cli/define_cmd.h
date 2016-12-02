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


#ifdef CREATE_CMD
#undef CREATE_CMD


#define __stringify_1(x...) #x
#define __stringify(x...)  __stringify_1(x)
#define __CMD_INCLUDE(cmd) __stringify(cmd.h)
#define CMD_INCLUDE(cmd) __CMD_INCLUDE(cmd)

#define CMD_HEADER_MULTI_READ

#include CMD_INCLUDE(CMD_INC_FILE)

#include "cmd_handler.h"

#undef CMD_HEADER_MULTI_READ

#define CREATE_CMD
#endif
