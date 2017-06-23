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

#undef CMD_INC_FILE
#define CMD_INC_FILE gas

#if !defined(GAS_PLUGIN) || defined(CMD_HEADER_MULTI_READ)
#define GAS_PLUGIN

#include "cmd.h"

PLUGIN(NAME("gas", "Global Address Space Access (dangerous)",
	    "These functions should be used with extreme caution only "
	    "if you know what you are doing. Any register accesses through "
	    "this interface is unsupported by Microsemi unless specifically "
	    "otherwise specified."),
	COMMAND_LIST(
		     ENTRY("dump", "dump the global address space", gas_dump)
		     ENTRY("read", "read a register from the global address space", gas_read)
		     ENTRY("write", "write a register in the global address space", gas_write)
		    )
);

#endif

#include "define_cmd.h"
