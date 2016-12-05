/*
 * Microsemi Switchtec(tm) PCIe Management Command Line Interface
 * Copyright (c) 2016, Microsemi Corporation
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

#undef CMD_INC_FILE
#define CMD_INC_FILE builtin

#if !defined(BUILTIN) || defined(CMD_HEADER_MULTI_READ)
#define BUILTIN

#include "cmd.h"

COMMAND_LIST(
	ENTRY("list", "List all switchtec devices on this machine", list)
	ENTRY("test", "Test if switchtec interface is working", test)
	ENTRY("hard-reset", "Perform a hard reset of the switch", hard_reset)
);

#endif

#include "define_cmd.h"
