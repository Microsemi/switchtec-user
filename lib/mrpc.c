/*
 * Microsemi Switchtec(tm) PCIe Management Library
 * Copyright (c) 2021, Microsemi Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "switchtec_priv.h"
#include "switchtec/mrpc.h"

#define M(k, d) [MRPC_ ## k] = {#k, d}
#define R(k, d) [MRPC_ ## k] = {#k, d, .reserved = true}

const struct switchtec_mrpc switchtec_mrpc_table[MRPC_MAX_ID] = {
	M(TWI,				"TWI Access"),
	M(VGPIO,			"GPIO"),
	M(PWM,				"Pulse Width Modulator"),
	M(DIETEMP,			"Die Temperature"),
	M(FWDNLD,			"Firmware Download"),
	M(FWLOGRD,			"Firmware Log Retrieval"),
	M(PMON,				"Performance Monitor"),
	M(PORTARB,			"Port Arbitration Set"),
	M(MCOVRLY,			"MC Overlay Setting"),
	M(STACKBIF,			"Dynamic Port Bifurcation"),
	M(PORTPARTP2P,			"Port Partition P2P Binding"),
	M(DIAG_TLP_INJECT,		"TLP Injection"),
	R(RESERVED1,			"Internal MRPC"),
	M(DIAG_PORT_EYE,		"2D Eye Capture"),
	M(DIAG_POT_VHIST,		"Real Time Eye Capture"),
	M(DIAG_PORT_LTSSM_LOG,		"LTSSM Monitor"),
	M(DIAG_PORT_TLP_ANL,		"PCIe Analyzer"),
	M(DIAG_PORT_LN_ADPT,		"Port Adaptation Objects"),
	M(SRDS_PCIE_PEAK,		"Receiver Peaking Control"),
	M(SRDS_EQ_CTRL,			"Port Equalization Control"),
	M(SRDS_LN_TUNING_MODE,		"Port Tuning Mode"),
	M(NT_MCG_CAPABLE_CONFIG,	"NT MCG Capable Configuration"),
	M(TCH,				"Tachometer"),
	M(ARB,				"Port Arbitration"),
	M(SMBUS,			"SMBus"),
	M(RESET,			"Reset"),
	M(LNKSTAT,			"Link Status Retrieve"),
	M(MULTI_CFG,			"Multi-Configuration"),
	M(RD_FLASH,			"Read Flash"),
	M(SPI_ECC,			"SPI Single Bit ECC"),
	M(PAT_GEN,			"Pattern Generator and Monitor"),
	M(INT_LOOPBACK,			"Internal Loopback"),
	R(RESERVED2,			"Internal MRPC"),
	M(ROUTE_TO_SELF,		"Route-To-Self"),
	M(REFCLK_S,			"REFCLK_S Control"),
	M(SYNTH_EP,			"Synthetic EP"),
	M(EVENTS_QUERY,			"Events Query"),
	M(GAS_READ,			"GAS Read"),
	M(AER_GEN,			"AER Events Generator"),
	M(PART_INFO,			"Get Partition Info"),
	M(PCIE_GEN_1_2_DUMP,		"PCIe Gen1/2 Port Tuning Dump"),
	M(PCIE_GEN_1_2_TUNE,		"PCIe Gen1/2 Port Tuning"),
	M(EYE_OBSERVE,			"Eye Observation Monitor"),
	M(RCVR_OBJ_DUMP,		"Receiver Object Dump"),
	R(RESERVED3,			"Internal MRPC"),
	M(PORT_EQ_STATUS,		"Port Equalization Status"),
	M(PORT_EQ_CTRL,			"Port Equalization Control"),
	M(GAS_WRITE,			"GAS Write"),
	M(MRPC_ERR_INJ,			"MRPC Link Error Injection"),
	M(DEV_INFO_GET,			"Device Info Get"),
	M(MRPC_PERM_TABLE_GET,		"MRPC Permission Table Get"),
	M(CROSS_HAIR,			"Cross Hair"),
	M(RECV_DETECT_STATUS,		"Receiver Detect Status Get"),
	M(EXT_RCVR_OBJ_DUMP,		"Extended Receiver Object Dump"),
	M(LOG_DEF_GET,			"Read Application Log"),
	M(SECURITY_CONFIG_GET_EXT,	"Secure Configuration Get Extended"),
	M(ECHO,				"Echo"),
	M(GET_PAX_ID,			"Local Fabric Switch Index"),
	M(TOPO_INFO_DUMP,		"Fabric Switch Topology Info"),
	M(GFMS_DB_DUMP,			"GFMS Database Info"),
	M(GFMS_BIND_UNBIND,		"Bind/Unbind EP Function"),
	M(DEVICE_MANAGE_CMD,		"Send EP Management"),
	M(PORT_CONFIG,			"Configure Fabric Physical Port"),
	M(GFMS_EVENT,			"GFMS Event Data Registers"),
	M(PORT_CONTROL,			"Port Link Control"),
	M(EP_RESOURCE_ACCESS,		"Endpoint Device CSR and MS Raw Access"),
	M(EP_TUNNEL_CFG,		"Endpoint Device Tunnel Configuration"),
	M(NVME_ADMIN_PASSTHRU,		"NVMe Admin Passthrough"),
	M(I2C_TWI_PING,			"I2C/TWI Ping"),
	M(SECURITY_CONFIG_GET,		"Secure Configuration Get"),
	M(SECURITY_CONFIG_SET,		"Secure Configuration Set"),
	M(KMSK_ENTRY_SET,		"Public Key Entry Hash Key Set"),
	M(SECURE_STATE_SET,		"Secure State Set"),
	M(ACT_IMG_IDX_GET,		"Firmware Active Image Index Get"),
	M(ACT_IMG_IDX_SET,		"Firmware Active Image Index Select"),
	M(FW_TX,			"Image Transfer and Execution"),
	M(MAILBOX_GET,			"Mailbox Log Get"),
	M(SN_VER_GET,			"Chip Serial Number and Secure Versions"),
	M(DBG_UNLOCK,			"Resource Unlock"),
	M(BOOTUP_RESUME,		"Bootup Resume"),
	M(SECURITY_CONFIG_GET_GEN5,	"Secure Configuration Get (Gen5)"),
	M(SECURITY_CONFIG_SET_GEN5,	"Secure Configuration Set (Gen5)"),
};
