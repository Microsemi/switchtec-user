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
 * Based partially on the ntb_hw_amd driver
 * Copyright (C) 2016 Advanced Micro Devices, Inc. All Rights Reserved.
 *
 */

#ifndef LIBSWITCHTEC_MRPC_H
#define LIBSWITCHTEC_MRPC_H

#define MRPC_MAX_DATA_LEN   1024

enum mrpc_cmd {
	MRPC_DIAG_PMC_START = 0,
	MRPC_TWI = 1,
	MRPC_VGPIO = 2,
	MRPC_PWM = 3,
	MRPC_DIETEMP = 4,
	MRPC_FWDNLD = 5,
	MRPC_FWLOGRD = 6,
	MRPC_PMON = 7,
	MRPC_PORTLN = 8,
	MRPC_PORTARB = 9,
	MRPC_MCOVRLY = 10,
	MRPC_STACKBIF = 11,
	MRPC_PORTPARTP2P = 12,
	MRPC_DIAG_TLP_INJECT = 13,
	MRPC_DIAG_TLP_GEN = 14,
	MRPC_DIAG_PORT_EYE = 15,
	MRPC_DIAG_POT_VHIST = 16,
	MRPC_DIAG_PORT_LTSSM_LOG = 17,
	MRPC_DIAG_PORT_TLP_ANL = 18,
	MRPC_DIAG_PORT_LN_ADPT = 19,
	MRPC_SRDS_PCIE_PEAK = 20,
	MRPC_SRDS_EQ_CTRL = 21,
	MRPC_SRDS_LN_TUNING_MODE = 22,
	MRPC_NT_MCG_CAPABLE_CONFIG = 23,
	MRPC_TCH = 24,
	MRPC_ARB = 25,
	MRPC_SMBUS = 26,
	MRPC_RESET = 27,
	MRPC_FWREAD = 31,
	MRPC_ECHO = 65,
};

enum mrpc_bg_status {
	MRPC_BG_STAT_IDLE = 0,
	MRPC_BG_STAT_INPROGRESS = 1,
	MRPC_BG_STAT_DONE = 2,
	MRPC_BG_STAT_ERROR = 0xFF,
};

enum mrpc_sub_cmd {
	MRPC_FWDNLD_GET_STATUS = 0,
	MRPC_FWDNLD_DOWNLOAD = 1,
	MRPC_FWDNLD_TOGGLE = 2,
};

#endif
