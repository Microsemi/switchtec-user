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

#include <stddef.h>
#include <errno.h>
#include <string.h>

#include "switchtec/fabric.h"
#include "switchtec_priv.h"
#include "switchtec/switchtec.h"
#include "switchtec/errors.h"

int switchtec_gfms_bind(struct switchtec_dev *dev,
			struct switchtec_gfms_bind_req *req)
{
	struct {
		uint8_t subcmd;
		uint8_t host_sw_idx;
		uint8_t host_phys_port_id;
		uint8_t host_log_port_id;
		uint16_t pdfid;
		uint8_t reserved[2];
	} cmd;

	struct {
		uint8_t status;
		uint8_t reserved[3];
	} result;

	cmd.subcmd = MRPC_GFMS_BIND;
	cmd.host_sw_idx = req->host_sw_idx;
	cmd.host_phys_port_id = req->host_phys_port_id;
	cmd.host_log_port_id = req->host_log_port_id;
	cmd.pdfid = req->pdfid;

	return switchtec_cmd(dev, MRPC_GFMS_BIND_UNBIND, &cmd, sizeof(cmd),
			     &result, sizeof(result));
}

int switchtec_gfms_unbind(struct switchtec_dev *dev,
			  struct switchtec_gfms_unbind_req *req)
{
	struct {
		uint8_t subcmd;
		uint8_t host_sw_idx;
		uint8_t host_phys_port_id;
		uint8_t host_log_port_id;
		uint16_t pdfid;
		uint8_t option;
		uint8_t reserved;
	} cmd;

	struct {
		uint8_t status;
	} result;

	cmd.subcmd = MRPC_GFMS_UNBIND;
	cmd.host_sw_idx = req->host_sw_idx;
	cmd.host_phys_port_id = req->host_phys_port_id;
	cmd.host_log_port_id = req->host_log_port_id;
	cmd.pdfid = req->pdfid;
	cmd.option = req->option;

	return switchtec_cmd(dev, MRPC_GFMS_BIND_UNBIND, &cmd, sizeof(cmd),
			     &result, sizeof(result));
}

int switchtec_port_control(struct switchtec_dev *dev, uint8_t control_type,
			   uint8_t phys_port_id, uint8_t hot_reset_flag)
{
        int ret;

	struct {
		uint8_t control_type;
		uint8_t phys_port_id;
		uint8_t hot_reset_flag;
		uint8_t rsvd;
	} cmd;

	cmd.control_type = control_type;
	cmd.phys_port_id = phys_port_id;
	cmd.hot_reset_flag = hot_reset_flag;

        ret = switchtec_cmd(dev, MRPC_PORT_CONTROL, &cmd, sizeof(cmd), NULL, 0);

        return ret;
}

/**
 * @brief Get the port config of the specified physical port
 * @param[in]  dev		Switchtec device handle
 * @param[in]  phys_port_id	The physical port id
 * @param[out] info		The port config info
 * @return 0 on success, error code on failure
 */
int switchtec_fab_port_config_get(struct switchtec_dev *dev,
				  uint8_t phys_port_id,
				  struct switchtec_fab_port_config *info)
{
	int ret;

	struct {
		uint8_t subcmd;
		uint8_t phys_port_id;
		uint8_t reserved[2];
	} cmd;

	cmd.subcmd = MRPC_PORT_CONFIG_GET;
	cmd.phys_port_id = phys_port_id;

	ret = switchtec_cmd(dev, MRPC_PORT_CONFIG, &cmd, sizeof(cmd),
			    info, sizeof(struct switchtec_fab_port_config));

	return ret;
}

/**
 * @brief Set the port config of the specified physical port
 * @param[in]  dev		Switchtec device handle
 * @param[in]  phys_port_id	The physical port id
 * @param[in]  info		The port config info
 * @return 0 on success, error code on failure
 */
int switchtec_fab_port_config_set(struct switchtec_dev *dev,
				  uint8_t phys_port_id,
				  struct switchtec_fab_port_config *info)
{
	int ret;

	struct {
		uint8_t subcmd;
		uint8_t phys_port_id;
		uint8_t port_type;
		uint8_t clock_source;
		uint8_t clock_mode;
		uint8_t hvd_inst;
		uint8_t reserved[2];
	} cmd;

	cmd.subcmd = MRPC_PORT_CONFIG_SET;
	cmd.phys_port_id = phys_port_id;
	cmd.port_type = info->port_type;
	cmd.clock_source = info->clock_source;
	cmd.clock_mode = info->clock_mode;
	cmd.hvd_inst = info->hvd_inst;

	ret = switchtec_cmd(dev, MRPC_PORT_CONFIG, &cmd, sizeof(cmd),
			    info, sizeof(struct switchtec_fab_port_config));

	return ret;
}
