/*
 * Microsemi Switchtec(tm) PCIe Management Library
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

#ifndef LIBSWITCHTEC_ERRORS_H
#define LIBSWITCHTEC_ERRORS_H


enum {
	ERR_NO_AVAIL_MRPC_THREAD = 0x64001,
	ERR_HANDLER_THREAD_NOT_IDLE = 0x64002,
	ERR_NO_BG_THREAD = 0x64003,
	ERR_SUBCMD_INVALID = 0x64004,
	ERR_CMD_INVALID = 0x64005,
	ERR_PARAM_INVALID = 0x64006,
	ERR_BAD_FW_STATE = 0x64007,

	ERR_STACK_INVALID = 0x100001,
	ERR_PORT_INVALID = 0x100002,
	ERR_EVENT_INVALID = 0x100003,
	ERR_RST_RULE_FAILED = 0x100005,
	ERR_ACCESS_REFUSED = 0xFFFF0001,
};

#endif
