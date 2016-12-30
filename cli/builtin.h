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
	ENTRY("status", "Display status information", status)
	ENTRY("bw", "Measure the bandwidth for each port", bw)
	ENTRY("events", "Display events that have occurred", events)
	ENTRY("log-dump", "Dump firmware log to a file", log_dump)
	ENTRY("test", "Test if switchtec interface is working", test)
	ENTRY("temp", "Return the switchtec die temperature", temp)
	ENTRY("hard-reset", "Perform a hard reset of the switch", hard_reset)
	ENTRY("fw-update", "Upload a new firmware image", fw_update)
	ENTRY("fw-info", "Return information on currently flashed firmware", fw_info)
	ENTRY("fw-toggle", "Toggle the active and inactive firmware partition", fw_toggle)
	ENTRY("fw-read", "Read back firmware image from hardware", fw_read)
	ENTRY("fw-img-info", "Display information for a firmware image", fw_image_info)
	ENTRY("evcntr", "Display event counters", evcntr)
	ENTRY("evcntr-setup", "Setup an event counter", evcntr_setup)
	ENTRY("evcntr-show", "Show an event counters setup info", evcntr_show)
	ENTRY("evcntr-del", "Deconfigure an event counter", evcntr_del)
	ENTRY("evcntr-wait", "Wait for an event counter", evcntr_wait)
);

#endif

#include "define_cmd.h"
