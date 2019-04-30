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

int switchtec_device_manage(struct switchtec_dev *dev,
                            struct switchtec_device_manage_req *req,
                            struct switchtec_device_manage_rsp *rsp)
{
        int ret;

        ret = switchtec_cmd(dev, MRPC_DEVICE_MANAGE_CMD,
			    req, sizeof(*req), rsp, sizeof(*rsp));

        return ret;
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
