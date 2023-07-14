/*
 * Microsemi Switchtec(tm) PCIe Management Library
 * Copyright (c) 2017, Microsemi Corporation
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

#include "gasops.h"
#include "switchtec/gas.h"
#include "../switchtec_priv.h"
#include "switchtec/utils.h"

#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#define gas_reg_read8(dev, reg)  __gas_read8(dev, &dev->gas_map->reg)
#define gas_reg_read16(dev, reg) __gas_read16(dev, &dev->gas_map->reg)
#define gas_reg_read32(dev, reg) __gas_read32(dev, &dev->gas_map->reg)
#define gas_reg_read64(dev, reg) __gas_read64(dev, &dev->gas_map->reg)

#define gas_reg_write8(dev, val, reg)  __gas_write8(dev, val, \
						    &dev->gas_map->reg)
#define gas_reg_write16(dev, val, reg) __gas_write16(dev, val, \
						     &dev->gas_map->reg)
#define gas_reg_write32(dev, val, reg) __gas_write32(dev, val, \
						     &dev->gas_map->reg)
#define gas_reg_write64(dev, val, reg) __gas_write64(dev, val, \
						     &dev->gas_map->reg)

struct no_retry_struct {
	int no_retry;
	int num_subcmd;
	int *subcmds;
};
static int fw_toggle_noretry_subcmds[] = {
	MRPC_FW_TX_TOGGLE,
};
static const struct no_retry_struct gasop_noretry_cmds[] = {
	[MRPC_SECURITY_CONFIG_SET] = {1, 0, NULL},
	[MRPC_KMSK_ENTRY_SET] = {1, 0, NULL},
	[MRPC_SECURE_STATE_SET] = {1, 0, NULL},
	[MRPC_BOOTUP_RESUME] = {1, 0, NULL},
	[MRPC_DBG_UNLOCK] = {1, 0, NULL},
	[MRPC_FW_TX] = {1, 1, fw_toggle_noretry_subcmds},
	[MRPC_SECURITY_CONFIG_SET_GEN5] = {1, 0, NULL},
	[MRPC_KMSK_ENTRY_SET_GEN5] = {1, 0, NULL},
	[MRPC_SECURE_STATE_SET_GEN5] = {1, 0, NULL},
	[MRPC_BOOTUP_RESUME_GEN5] = {1, 0, NULL},
	[MRPC_DBG_UNLOCK_GEN5] = {1, 0, NULL},
	[MRPC_FW_TX_GEN5] = {1, 1, fw_toggle_noretry_subcmds},
};
static const int gasop_noretry_cmds_count = sizeof(gasop_noretry_cmds) /
					    sizeof(struct no_retry_struct);

static inline bool gasop_is_no_retry_cmd(uint32_t cmd, int subcmd)
{
	int i;

	cmd &= SWITCHTEC_CMD_MASK;

	if (cmd >= gasop_noretry_cmds_count)
		return 0;
	if (gasop_noretry_cmds[cmd].no_retry == 0)
		return 0;
	if (gasop_noretry_cmds[cmd].num_subcmd == 0)
		return 1;
	for (i = 0; i < gasop_noretry_cmds[cmd].num_subcmd; i++) {
		if (subcmd == gasop_noretry_cmds[cmd].subcmds[i])
			return 1;
	}

	return 0;
}

int gasop_access_check(struct switchtec_dev *dev)
{
	uint32_t device_id;

	device_id = gas_reg_read32(dev, sys_info.device_id);
	if (device_id == -1)
		return -1;
	return 0;
}

void gasop_set_partition_info(struct switchtec_dev *dev)
{
	dev->partition = gas_reg_read8(dev, top.partition_id);
	dev->partition_count = gas_reg_read8(dev, top.partition_count);
}

int gasop_cmd(struct switchtec_dev *dev, uint32_t cmd,
	      const void *payload, size_t payload_len, void *resp,
	      size_t resp_len)
{
	struct mrpc_regs __gas *mrpc = &dev->gas_map->mrpc;
	int status;
	int ret;
	uint8_t subcmd = 0xff;

	__memcpy_to_gas(dev, &mrpc->input_data, payload, payload_len);

       /* Due to the possible unreliable nature of hardware
        * communication, function __gas_write32() is implemented
        * with automatic retry.
        *
        * This poses a potential issue when a command is critical
        * and is expected to be sent only once (e.g., command that
        * adds a KMSK entry to chip OTP memory). Retrying could
        * cause the command be sent multiple times (and multiple
        * KMSK entry being added, if unlucky).
        *
        * Here we filter out the specific commands and use 'no retry'
        * version of gas_write32 for these commands.
        */
	if (payload)
		subcmd = *(uint8_t*)payload;
	if (gasop_is_no_retry_cmd(cmd, subcmd))
		__gas_write32_no_retry(dev, cmd, &mrpc->cmd);
	else
		__gas_write32(dev, cmd, &mrpc->cmd);

	while (1) {
		usleep(5000);

		status = __gas_read32(dev, &mrpc->status);
		if (status != SWITCHTEC_MRPC_STATUS_INPROGRESS)
			break;
	}

	if (status == SWITCHTEC_MRPC_STATUS_INTERRUPTED) {
		errno = ENXIO;
		return -errno;
	}

	if(status == SWITCHTEC_MRPC_STATUS_ERROR) {
		errno = __gas_read32(dev, &mrpc->ret_value);
		return errno;
	}

	if (status != SWITCHTEC_MRPC_STATUS_DONE) {
		errno = ENXIO;
		return -errno;
	}

	ret = __gas_read32(dev, &mrpc->ret_value);
	if (ret)
		errno = ret;

	if(resp)
		__memcpy_from_gas(dev, resp, &mrpc->output_data, resp_len);

	return ret;
}

int gasop_get_device_id(struct switchtec_dev *dev)
{
	return gas_reg_read32(dev, sys_info.device_id);
}

int gasop_get_fw_version(struct switchtec_dev *dev, char *buf,
			 size_t buflen)
{
	long long ver;

	ver = gas_reg_read32(dev, sys_info.firmware_version);
	version_to_string(ver, buf, buflen);

	return 0;
}

int gasop_pff_to_port(struct switchtec_dev *dev, int pff,
		      int *partition, int *port)
{
	int i, part;
	uint32_t reg;
	struct part_cfg_regs __gas *pcfg;

	*port = -1;

	for (part = 0; part < dev->partition_count; part++) {
		pcfg = &dev->gas_map->part_cfg[part];
		*partition = part;

		reg = __gas_read32(dev, &pcfg->usp_pff_inst_id);
		if (reg == pff) {
			*port = 0;
			return 0;
		}

		reg = __gas_read32(dev, &pcfg->vep_pff_inst_id);
		if (reg == pff) {
			*port = SWITCHTEC_PFF_PORT_VEP;
			return 0;
		}

		for (i = 0; i < ARRAY_SIZE(pcfg->dsp_pff_inst_id); i++) {
			reg = __gas_read32(dev, &pcfg->dsp_pff_inst_id[i]);
			if (reg != pff)
				continue;

			*port = i + 1;
			break;
		}

		if (*port != -1)
			return 0;
	}

	errno = EINVAL;
	return -EINVAL;
}

int gasop_port_to_pff(struct switchtec_dev *dev, int partition,
		      int port, int *pff)
{
	struct part_cfg_regs __gas *pcfg;

	if (partition < 0) {
		partition = dev->partition;
	} else if (partition >= dev->partition_count) {
		errno = EINVAL;
		return -errno;
	}

	pcfg = &dev->gas_map->part_cfg[partition];

	switch (port) {
	case 0:
		*pff = __gas_read32(dev, &pcfg->usp_pff_inst_id);
		break;
	case SWITCHTEC_PFF_PORT_VEP:
		*pff = __gas_read32(dev, &pcfg->vep_pff_inst_id);
		break;
	default:
		if (port > ARRAY_SIZE(pcfg->dsp_pff_inst_id)) {
			errno = EINVAL;
			return -errno;
		}

		*pff = __gas_read32(dev, &pcfg->dsp_pff_inst_id[port - 1]);
		break;
	}

	return 0;
}

static void set_fw_info_part(struct switchtec_dev *dev,
			     struct switchtec_fw_image_info *info,
			     struct partition_info __gas *pi)
{
	info->part_addr = __gas_read32(dev, &pi->address);
	info->part_len = __gas_read32(dev, &pi->length);
}

int gasop_flash_part(struct switchtec_dev *dev,
		     struct switchtec_fw_image_info *info,
		     enum switchtec_fw_image_part_id_gen3 part)
{
	struct flash_info_regs __gas *fi = &dev->gas_map->flash_info;
	struct sys_info_regs __gas *si = &dev->gas_map->sys_info;
	uint32_t active_addr = -1;
	int val;

	info->running = false;
	info->active = false;

	switch (part) {
	case SWITCHTEC_FW_PART_ID_G3_IMG0:
		active_addr = __gas_read32(dev, &fi->active_img.address);
		set_fw_info_part(dev, info, &fi->img0);

		val = __gas_read16(dev, &si->img_running);
		if (val == SWITCHTEC_IMG0_RUNNING)
			info->running = true;
		break;

	case SWITCHTEC_FW_PART_ID_G3_IMG1:
		active_addr = __gas_read32(dev, &fi->active_img.address);
		set_fw_info_part(dev, info, &fi->img1);

		val = __gas_read16(dev, &si->img_running);
		if (val == SWITCHTEC_IMG1_RUNNING)
			info->running = true;
		break;

	case SWITCHTEC_FW_PART_ID_G3_DAT0:
		active_addr = __gas_read32(dev, &fi->active_cfg.address);
		set_fw_info_part(dev, info, &fi->cfg0);

		val = __gas_read16(dev, &si->cfg_running);
		if (val == SWITCHTEC_CFG0_RUNNING)
			info->running = true;
		break;

	case SWITCHTEC_FW_PART_ID_G3_DAT1:
		active_addr = __gas_read32(dev, &fi->active_cfg.address);
		set_fw_info_part(dev, info, &fi->cfg1);

		val = __gas_read16(dev, &si->cfg_running);
		if (val == SWITCHTEC_CFG1_RUNNING)
			info->running = true;
		break;

	case SWITCHTEC_FW_PART_ID_G3_NVLOG:
		set_fw_info_part(dev, info, &fi->nvlog);
		break;

	default:
		return -EINVAL;
	}

	if (info->part_addr == active_addr)
		info->active = true;

	return 0;
}

int gasop_event_summary(struct switchtec_dev *dev,
			struct switchtec_event_summary *sum)
{
	int i;
	uint32_t reg;

	if (!sum)
		return 0;

	memset(sum, 0, sizeof(*sum));

	sum->global = gas_reg_read32(dev, sw_event.global_summary);
	sum->part_bitmap = gas_reg_read64(dev, sw_event.part_event_bitmap);

	for (i = 0; i < dev->partition_count; i++) {
		reg = gas_reg_read32(dev, part_cfg[i].part_event_summary);
		sum->part[i] = reg;
		if (i == dev->partition)
			sum->local_part = reg;
	}

	for (i = 0; i < SWITCHTEC_MAX_PFF_CSR; i++) {
		reg = gas_reg_read16(dev, pff_csr[i].vendor_id);
		if (reg != MICROSEMI_VENDOR_ID)
			break;

		sum->pff[i] = gas_reg_read32(dev, pff_csr[i].pff_event_summary);
	}

	return 0;
}

static uint32_t __gas *global_ev_reg(struct switchtec_dev *dev,
				     size_t offset, int index)
{
	return (void __gas *)&dev->gas_map->sw_event + offset;
}

static uint32_t __gas *part_ev_reg(struct switchtec_dev *dev,
				   size_t offset, int index)
{
	return (void __gas *)&dev->gas_map->part_cfg[index] + offset;
}

static uint32_t __gas *pff_ev_reg(struct switchtec_dev *dev,
				  size_t offset, int index)
{
	return (void __gas *)&dev->gas_map->pff_csr[index] + offset;
}

#define EV_GLB(i, r)[SWITCHTEC_GLOBAL_EVT_ ## i] = \
	{offsetof(struct sw_event_regs, r), global_ev_reg}
#define EV_PAR(i, r)[SWITCHTEC_PART_EVT_ ## i] = \
	{offsetof(struct part_cfg_regs, r), part_ev_reg}
#define EV_PFF(i, r)[SWITCHTEC_PFF_EVT_ ## i] = \
	{offsetof(struct pff_csr_regs, r), pff_ev_reg}

static const struct event_reg {
	size_t offset;
	uint32_t __gas *(*map_reg)(struct switchtec_dev *stdev,
				   size_t offset, int index);
} event_regs[] = {
	EV_GLB(STACK_ERROR, stack_error_event_hdr),
	EV_GLB(PPU_ERROR, ppu_error_event_hdr),
	EV_GLB(ISP_ERROR, isp_error_event_hdr),
	EV_GLB(SYS_RESET, sys_reset_event_hdr),
	EV_GLB(FW_EXC, fw_exception_hdr),
	EV_GLB(FW_NMI, fw_nmi_hdr),
	EV_GLB(FW_NON_FATAL, fw_non_fatal_hdr),
	EV_GLB(FW_FATAL, fw_fatal_hdr),
	EV_GLB(TWI_MRPC_COMP, twi_mrpc_comp_hdr),
	EV_GLB(TWI_MRPC_COMP_ASYNC, twi_mrpc_comp_async_hdr),
	EV_GLB(CLI_MRPC_COMP, cli_mrpc_comp_hdr),
	EV_GLB(CLI_MRPC_COMP_ASYNC, cli_mrpc_comp_async_hdr),
	EV_GLB(GPIO_INT, gpio_interrupt_hdr),
	EV_GLB(GFMS, gfms_event_hdr),
	EV_PAR(PART_RESET, part_reset_hdr),
	EV_PAR(MRPC_COMP, mrpc_comp_hdr),
	EV_PAR(MRPC_COMP_ASYNC, mrpc_comp_async_hdr),
	EV_PAR(DYN_PART_BIND_COMP, dyn_binding_hdr),
	EV_PFF(AER_IN_P2P, aer_in_p2p_hdr),
	EV_PFF(AER_IN_VEP, aer_in_vep_hdr),
	EV_PFF(DPC, dpc_hdr),
	EV_PFF(CTS, cts_hdr),
	EV_PFF(UEC, uec_hdr),
	EV_PFF(HOTPLUG, hotplug_hdr),
	EV_PFF(IER, ier_hdr),
	EV_PFF(THRESH, threshold_hdr),
	EV_PFF(POWER_MGMT, power_mgmt_hdr),
	EV_PFF(TLP_THROTTLING, tlp_throttling_hdr),
	EV_PFF(FORCE_SPEED, force_speed_hdr),
	EV_PFF(CREDIT_TIMEOUT, credit_timeout_hdr),
	EV_PFF(LINK_STATE, link_state_hdr),
};

static uint32_t __gas *event_hdr_addr(struct switchtec_dev *dev,
				      enum switchtec_event_id e,
				      int index)
{
	size_t off;

	if (e < 0 || e >= SWITCHTEC_MAX_EVENTS)
		return NULL;

	off = event_regs[e].offset;

	if (event_regs[e].map_reg == part_ev_reg) {
		if (index < 0)
			index = dev->partition;
		else if (index >= dev->partition_count)
			return NULL;
	} else if (event_regs[e].map_reg == pff_ev_reg) {
		if (index < 0 || index >= SWITCHTEC_MAX_PFF_CSR)
			return NULL;
	}

	return event_regs[e].map_reg(dev, off, index);
}

static int event_ctl(struct switchtec_dev *dev, enum switchtec_event_id e,
		     int index, int flags, uint32_t data[5])
{
	int i;
	uint32_t __gas *reg;
	uint32_t hdr;

	reg = event_hdr_addr(dev, e, index);
	if (!reg) {
		errno = EINVAL;
		return -errno;
	}

	hdr = __gas_read32(dev, reg);
	if (data)
		for (i = 0; i < 5; i++)
			data[i] = __gas_read32(dev, &reg[i + 1]);

	if (!(flags & SWITCHTEC_EVT_FLAG_CLEAR))
		hdr &= ~SWITCHTEC_EVENT_CLEAR;
	if (flags & SWITCHTEC_EVT_FLAG_EN_POLL)
		hdr |= SWITCHTEC_EVENT_EN_IRQ;
	if (flags & SWITCHTEC_EVT_FLAG_EN_LOG)
		hdr |= SWITCHTEC_EVENT_EN_LOG;
	if (flags & SWITCHTEC_EVT_FLAG_EN_CLI)
		hdr |= SWITCHTEC_EVENT_EN_CLI;
	if (flags & SWITCHTEC_EVT_FLAG_EN_FATAL)
		hdr |= SWITCHTEC_EVENT_FATAL;
	if (flags & SWITCHTEC_EVT_FLAG_DIS_POLL)
		hdr &= ~SWITCHTEC_EVENT_EN_IRQ;
	if (flags & SWITCHTEC_EVT_FLAG_DIS_LOG)
		hdr &= ~SWITCHTEC_EVENT_EN_LOG;
	if (flags & SWITCHTEC_EVT_FLAG_DIS_CLI)
		hdr &= ~SWITCHTEC_EVENT_EN_CLI;
	if (flags & SWITCHTEC_EVT_FLAG_DIS_FATAL)
		hdr &= ~SWITCHTEC_EVENT_FATAL;

	if (flags)
		__gas_write32(dev, hdr, reg);

	return (hdr >> 5) & 0xFF;
}

int gasop_event_ctl(struct switchtec_dev *dev, enum switchtec_event_id e,
		    int index, int flags, uint32_t data[5])
{
	int nr_idxs;
	int ret = 0;

	if (e >= SWITCHTEC_MAX_EVENTS)
		goto einval;

	if (index == SWITCHTEC_EVT_IDX_ALL) {
		if (event_regs[e].map_reg == global_ev_reg)
			nr_idxs = 1;
		else if (event_regs[e].map_reg == part_ev_reg)
			nr_idxs = dev->partition_count;
		else if (event_regs[e].map_reg == pff_ev_reg)
			nr_idxs = gas_reg_read8(dev, top.pff_count);
		else
			goto einval;

		for (index = 0; index < nr_idxs; index++) {
			ret = event_ctl(dev, e, index, flags, data);
			if (ret < 0)
				return ret;
		}
	} else {
		ret = event_ctl(dev, e, index, flags, data);
	}

	return ret;

einval:
	errno = EINVAL;
	return -errno;
}

int gasop_event_wait_for(struct switchtec_dev *dev,
			 enum switchtec_event_id e, int index,
			 struct switchtec_event_summary *res,
			 int timeout_ms)
{
	struct timeval tv;
	long long start, now;
	struct switchtec_event_summary wait_for = {0};
	int ret;

	ret = switchtec_event_summary_set(&wait_for, e, index);
	if (ret)
		return ret;

	ret = switchtec_event_ctl(dev, e, index,
				  SWITCHTEC_EVT_FLAG_CLEAR |
				  SWITCHTEC_EVT_FLAG_EN_POLL,
				  NULL);
	if (ret < 0)
		return ret;

	ret = gettimeofday(&tv, NULL);
	if (ret)
		return ret;

	now = start = ((tv.tv_sec) * 1000 + tv.tv_usec / 1000);

	while (1) {
		ret = switchtec_event_check(dev, &wait_for, res);
		if (ret < 0)
			return ret;

		if (ret)
			return 1;

		ret = gettimeofday(&tv, NULL);
		if (ret)
			return ret;

		now = ((tv.tv_sec) * 1000 + tv.tv_usec / 1000);

		if (timeout_ms > 0 && now - start >= timeout_ms)
			return 0;

		usleep(5000);
	}
}
