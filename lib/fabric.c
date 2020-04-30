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

#include <sys/mman.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>

#include "switchtec/fabric.h"
#include "switchtec_priv.h"
#include "switchtec/switchtec.h"
#include "switchtec/errors.h"
#include "switchtec/endian.h"

static int topo_info_dump_start(struct switchtec_dev *dev)
{
	uint8_t subcmd = MRPC_TOPO_INFO_DUMP_START;
	uint8_t status;

	return switchtec_cmd(dev, MRPC_TOPO_INFO_DUMP, &subcmd, sizeof(subcmd),
			     &status, sizeof(status));
}

static int topo_info_dump_status_get(struct switchtec_dev *dev,
				     int *status, uint16_t *info_len)
{
	int ret;

	uint8_t subcmd = MRPC_TOPO_INFO_DUMP_STATUS_GET;

	struct {
		uint8_t status;
		uint8_t reserved;
		uint16_t data_len_dw;
	} result;

	ret = switchtec_cmd(dev, MRPC_TOPO_INFO_DUMP, &subcmd, sizeof(subcmd),
			    &result, sizeof(result));

	*status = result.status;
	*info_len = result.data_len_dw * 4;

	return ret;
}

#define SWITCHTEC_TOPO_INFO_DUMP_DATA_LENGTH_MAX 1000
static int topo_info_dump_data_get(struct switchtec_dev *dev, uint16_t offset,
				   char *buf, uint16_t *len)
{
	int ret;
	size_t buf_len;

	struct {
		uint8_t subcmd;
		uint8_t reserved;
		uint16_t offset;
	} cmd = {
		.subcmd = MRPC_TOPO_INFO_DUMP_DATA_GET,
	};

	struct	{
		uint8_t status;
		uint8_t reserved;
		uint16_t data_len_dw;
		uint8_t data[SWITCHTEC_TOPO_INFO_DUMP_DATA_LENGTH_MAX];
	} result;


	buf_len = sizeof(result);

	if(*len < SWITCHTEC_TOPO_INFO_DUMP_DATA_LENGTH_MAX)
		buf_len = *len + sizeof(result)
			  - SWITCHTEC_TOPO_INFO_DUMP_DATA_LENGTH_MAX;

	cmd.offset = offset;

	ret = switchtec_cmd(dev, MRPC_TOPO_INFO_DUMP, &cmd,
			    sizeof(cmd), &result, buf_len);

	*len = result.data_len_dw * 4;

	memcpy(buf, &(result.data), *len);

	return ret;
}

static int topo_info_dump_finish(struct switchtec_dev *dev)
{
	uint8_t subcmd = MRPC_TOPO_INFO_DUMP_FINISH;
	uint8_t status;

	return switchtec_cmd(dev, MRPC_TOPO_INFO_DUMP, &subcmd, sizeof(subcmd),
			     &status, sizeof(status));
}

enum switchtec_fab_topo_info_dump_status {
	SWITCHTEC_FAB_TOPO_INFO_DUMP_NOT_START = 1,
	SWITCHTEC_FAB_TOPO_INFO_DUMP_WAIT = 2,
	SWITCHTEC_FAB_TOPO_INFO_DUMP_READY = 3,
	SWITCHTEC_FAB_TOPO_INFO_DUMP_FAILED = 4,
	SWITCHTEC_FAB_TOPO_INFO_DUMP_WRONG_SUB_CMD = 5,
};

/**
 * @brief Get the topology of the specified switch
 * @param[in]  dev		Switchtec device handle
 * @param[out] topo_info	The topology info
 * @return 0 on success, error code on failure
 */
int switchtec_topo_info_dump(struct switchtec_dev *dev,
			     struct switchtec_fab_topo_info *topo_info)
{
	int ret;
	int status;
	uint16_t total_info_len, offset, buf_len;
	char *buf = (char *)topo_info;

	if (!switchtec_is_gen4(dev) || !switchtec_is_pax(dev)) {
		errno = ENOTSUP;
		return -1;
	}

	ret = topo_info_dump_start(dev);
	if(ret)
		return ret;

	do {
		ret = topo_info_dump_status_get(dev, &status, &total_info_len);
		if(ret)
			return ret;
	} while(status == SWITCHTEC_FAB_TOPO_INFO_DUMP_WAIT);

	if (status != SWITCHTEC_FAB_TOPO_INFO_DUMP_READY)
		return -1;

	if (total_info_len > sizeof(struct switchtec_fab_topo_info))
		return -1;

	offset = 0;
	buf_len = sizeof(struct switchtec_fab_topo_info);

	while (offset < total_info_len) {
		ret = topo_info_dump_data_get(dev, offset,
					      buf+offset, &buf_len);
		if(ret)
			return ret;

		offset += buf_len;
		buf_len = sizeof(struct switchtec_fab_topo_info) - offset;
	}

	return topo_info_dump_finish(dev);
}

int switchtec_gfms_bind(struct switchtec_dev *dev,
			struct switchtec_gfms_bind_req *req)
{
	int i;

	struct {
		uint8_t subcmd;
		uint8_t host_sw_idx;
		uint8_t host_phys_port_id;
		uint8_t host_log_port_id;
		struct {
			uint16_t pdfid;
			uint8_t next_valid;
			uint8_t reserved;
		} function[SWITCHTEC_FABRIC_MULTI_FUNC_NUM];
	} cmd;

	struct {
		uint8_t status;
		uint8_t reserved[3];
	} result;

	cmd.subcmd = MRPC_GFMS_BIND;
	cmd.host_sw_idx = req->host_sw_idx;
	cmd.host_phys_port_id = req->host_phys_port_id;
	cmd.host_log_port_id = req->host_log_port_id;

	for (i = 0; i < req->ep_number; i++) {
		cmd.function[i].pdfid = req->ep_pdfid[i];
		cmd.function[i].next_valid = 0;
		if (i)
			cmd.function[i - 1].next_valid = 1;
	}

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
		uint8_t clock_sris;
		uint8_t hvd_inst;
		uint8_t reserved[2];
	} cmd;

	cmd.subcmd = MRPC_PORT_CONFIG_SET;
	cmd.phys_port_id = phys_port_id;
	cmd.port_type = info->port_type;
	cmd.clock_source = info->clock_source;
	cmd.clock_sris = info->clock_sris;
	cmd.hvd_inst = info->hvd_inst;

	ret = switchtec_cmd(dev, MRPC_PORT_CONFIG, &cmd, sizeof(cmd),
			    info, sizeof(struct switchtec_fab_port_config));

	return ret;
}

int switchtec_fab_gfms_db_dump_fabric_general(
		struct switchtec_dev *dev,
		struct switchtec_gfms_db_fabric_general *fabric_general)
{
	uint8_t subcmd = MRPC_GFMS_DB_DUMP_FABRIC;

	return switchtec_cmd(dev, MRPC_GFMS_DB_DUMP, &subcmd, sizeof(subcmd),
			     fabric_general, sizeof(*fabric_general));
}

static size_t gfms_hvd_all_section_parse(
		struct switchtec_dev *dev,
		uint8_t *data,
		struct switchtec_gfms_db_hvd_all *hvd_all)
{
	uint8_t *p;
	int i;
	size_t len;
	size_t parsed_len;
	size_t remaining_len;
	struct switchtec_gfms_db_hvd_body *hvd_body;

	p = data;

	len = sizeof(hvd_all->hdr);
	memcpy(&hvd_all->hdr, data, len);
	p += len;
	parsed_len = len;
	remaining_len = hvd_all->hdr.resp_size_dw * 4 - len;

	i = 0;
	while (remaining_len) {
		hvd_body = &hvd_all->bodies[i];

		len = 8;
		memcpy(hvd_body, p, len);
		p += len;
		remaining_len -= len;
		parsed_len += len;

		len = hvd_body->logical_port_count *
			SWITCHTEC_FABRIC_MULTI_FUNC_NUM * 4;
		memcpy(&hvd_body->bound[0], p, len);
		p += len;
		remaining_len -= len;
		parsed_len += len;

		i++;
		hvd_all->hvd_count = i;
	}

	return parsed_len;
}

static size_t gfms_pax_general_section_parse(
		struct switchtec_dev *dev,
		uint8_t *data,
		struct switchtec_gfms_db_pax_general *pax_general)
{
	size_t parsed_len;

	parsed_len = sizeof(*pax_general);

	memcpy(pax_general, data, parsed_len);

	return parsed_len;
}

int switchtec_fab_gfms_db_dump_pax_general(
		struct switchtec_dev *dev,
		struct switchtec_gfms_db_pax_general *pax_general)
{
	uint8_t subcmd = MRPC_GFMS_DB_DUMP_PAX;

	return switchtec_cmd(dev, MRPC_GFMS_DB_DUMP, &subcmd, sizeof(subcmd),
			     pax_general, sizeof(*pax_general));
}

static int gfms_dump_start(struct switchtec_dev *dev, uint8_t subcmd,
			   uint8_t param, uint32_t *total_len_dw)
{
	int ret;

	struct {
		uint8_t subcmd;
		uint8_t param;
		uint8_t reserved[2];
		uint32_t type;
	} cmd = {
		.subcmd = subcmd,
		.param = param,
		.type = 1,
	};

	struct {
		uint32_t dw_len;
		uint32_t num_of_switch;
	} rsp;

	ret = switchtec_cmd(dev, MRPC_GFMS_DB_DUMP, &cmd, sizeof(cmd),
			    &rsp, sizeof(rsp));
	*total_len_dw = rsp.dw_len;

	return ret;
}

static int gfms_dump_get(struct switchtec_dev *dev, uint8_t subcmd,
			 uint32_t total_len_dw, uint8_t *data)
{
	int ret;

	struct {
		uint8_t subcmd;
		uint8_t reserved[3];
		uint32_t type;
		uint32_t offset_dw;
	} cmd = {
		.subcmd = subcmd,
		.type = 2,
		.offset_dw = 0,
	};

	struct {
		uint32_t offset_dw;
		uint32_t size_dw;
		uint32_t reserved;
		uint8_t data[MRPC_MAX_DATA_LEN - 12];
	} rsp = {
		.offset_dw = 0,
		.size_dw = 0,
	};
	do {
		ret = switchtec_cmd(dev, MRPC_GFMS_DB_DUMP, &cmd, sizeof(cmd),
				    &rsp, MRPC_MAX_DATA_LEN);

		if (ret)
			break;

		rsp.size_dw -= 3;

		memcpy(data + (cmd.offset_dw * 4), rsp.data, rsp.size_dw * 4);

		cmd.offset_dw += rsp.size_dw;

	} while(total_len_dw > rsp.offset_dw + rsp.size_dw);

	return ret;
}

static int gfms_dump_finish(struct switchtec_dev *dev, uint8_t subcmd)
{
	struct {
		uint8_t subcmd;
		uint8_t reserved[3];
		uint32_t type;
	} cmd = {
		.subcmd = subcmd,
		.type = 3,
	};

	return switchtec_cmd(dev, MRPC_GFMS_DB_DUMP, &cmd, sizeof(cmd),
			     NULL, 0);
}

int switchtec_fab_gfms_db_dump_hvd(struct switchtec_dev *dev,
				   uint8_t hvd_idx,
				   struct switchtec_gfms_db_hvd *hvd)
{
	uint32_t total_len_dw;
	int ret;

	ret = gfms_dump_start(dev, MRPC_GFMS_DB_DUMP_HVD,
			      hvd_idx, &total_len_dw);
	if (ret)
		return ret;

	ret = gfms_dump_get(dev, MRPC_GFMS_DB_DUMP_HVD, total_len_dw,
			    (uint8_t *)hvd);
	if (ret)
		return ret;

	ret = gfms_dump_finish(dev, MRPC_GFMS_DB_DUMP_HVD);
	if (ret)
		return ret;

	return 0;
}

int switchtec_fab_gfms_db_dump_hvd_detail(
		struct switchtec_dev *dev,
		uint8_t hvd_idx,
		struct switchtec_gfms_db_hvd_detail *hvd_detail)
{
	uint32_t total_len_dw;
	int ret;
	uint8_t *data;
	struct switchtec_gfms_db_hvd_detail_body *body;
	void *p;
	int len;
	uint64_t bitmap;
	int i;

	ret = gfms_dump_start(dev, MRPC_GFMS_DB_DUMP_HVD_DETAIL,
			      hvd_idx, &total_len_dw);
	if (ret)
		return ret;

	data = malloc(total_len_dw * 4);
	ret = gfms_dump_get(dev, MRPC_GFMS_DB_DUMP_HVD_DETAIL, total_len_dw,
			    (uint8_t *)data);
	if (ret) {
		free(data);
		return ret;
	}

	ret = gfms_dump_finish(dev, MRPC_GFMS_DB_DUMP_HVD_DETAIL);
	if (ret) {
		free(data);
		return ret;
	}

	memcpy(&hvd_detail->hdr, data, sizeof(hvd_detail->hdr));

	body = (struct switchtec_gfms_db_hvd_detail_body *)(data + sizeof(hvd_detail->hdr));

	p = (void *)body;
	hvd_detail->body.hvd_inst_id = body->hvd_inst_id;
	hvd_detail->body.phy_pid = body->phy_pid;
	hvd_detail->body.hfid = body->hfid;
	hvd_detail->body.vep_count = body->vep_count;
	hvd_detail->body.usp_status = body->usp_status;

	p += offsetof(struct switchtec_gfms_db_hvd_detail_body, vep_region);
	len = sizeof(body->vep_region[0]) * body->vep_count;
	memcpy(hvd_detail->body.vep_region, body->vep_region, len);
	p += len;

	len = sizeof(hvd_detail->body.log_dsp_count);
	memcpy(&hvd_detail->body.log_dsp_count, p, len);
	p += len;

	len = sizeof(hvd_detail->body.usp_bdf);
	memcpy(&hvd_detail->body.usp_bdf, p, len);
	p += len;

	len = sizeof(hvd_detail->body.log_port_region[0]) *
		     le16toh(hvd_detail->body.log_dsp_count) *
		     SWITCHTEC_FABRIC_MULTI_FUNC_NUM;
	memcpy(hvd_detail->body.log_port_region, p, len);
	p += len;

	len = sizeof(hvd_detail->body.log_port_p2p_enable_bitmap_low);
	memcpy(&hvd_detail->body.log_port_p2p_enable_bitmap_low, p, len);
	p += len;

	len = sizeof(hvd_detail->body.log_port_p2p_enable_bitmap_high);
	memcpy(&hvd_detail->body.log_port_p2p_enable_bitmap_high, p, len);
	p += len;

	bitmap = le32toh(hvd_detail->body.log_port_p2p_enable_bitmap_high);
	bitmap <<= 32;
	bitmap |= le32toh(hvd_detail->body.log_port_p2p_enable_bitmap_low);

	hvd_detail->body.log_port_count = 0;
	for (i = 0; i < (sizeof(bitmap) * 8); i++)
		if (bitmap >> i && 0x1)
			hvd_detail->body.log_port_count++;

	len = sizeof(hvd_detail->body.log_port_p2p_bitmap[0]) *
		     hvd_detail->body.log_port_count;
	memcpy(hvd_detail->body.log_port_p2p_bitmap, p, len);

	free(data);
	return 0;
}

int switchtec_fab_gfms_db_dump_fab_port(
		struct switchtec_dev *dev,
		uint8_t phy_pid,
		struct switchtec_gfms_db_fab_port *fab_port)
{
	struct {
		uint8_t subcmd;
		uint8_t phy_pid;
	} cmd = {
		.subcmd = MRPC_GFMS_DB_DUMP_FAB_PORT,
		.phy_pid = phy_pid,
	};

	return switchtec_cmd(dev, MRPC_GFMS_DB_DUMP, &cmd, sizeof(cmd),
			     fab_port, sizeof(*fab_port));
}

static int gfms_ep_port_start(struct switchtec_dev *dev,
				      uint8_t fab_ep_pid,
				      uint32_t *total_len_dw)
{
	int ret;

	struct {
		uint8_t subcmd;
		uint8_t fab_ep_pid;
		uint16_t reserved;
		uint32_t type;
	} cmd = {
		.subcmd = MRPC_GFMS_DB_DUMP_EP_PORT,
		.fab_ep_pid = fab_ep_pid,
		.type = 1,
	};

	struct {
		uint32_t dw_len;
		uint32_t num_of_switch;
	} rsp;

	ret = switchtec_cmd(dev, MRPC_GFMS_DB_DUMP, &cmd, sizeof(cmd),
			    &rsp, sizeof(rsp));
	*total_len_dw = rsp.dw_len;

	return ret;
}

static int gfms_ep_port_get(struct switchtec_dev *dev,
				    uint8_t fab_ep_pid,
				    uint32_t total_len_dw,
				    uint8_t *data)
{
	int ret;

	struct {
		uint8_t subcmd;
		uint8_t fab_ep_pid;
		uint16_t reserved;
		uint32_t type;
		uint32_t offset_dw;
	} cmd = {
		.subcmd = MRPC_GFMS_DB_DUMP_EP_PORT,
		.fab_ep_pid = fab_ep_pid,
		.type = 2,
		.offset_dw = 0,
	};

	struct {
		uint32_t offset_dw;
		uint32_t size_dw;
		uint32_t reserved;
		uint8_t data[MRPC_MAX_DATA_LEN - 12];
	} rsp = {
		.offset_dw = 0,
		.size_dw = 0,
	};

	do {
		ret = switchtec_cmd(dev, MRPC_GFMS_DB_DUMP, &cmd, sizeof(cmd),
				    &rsp, MRPC_MAX_DATA_LEN);

		if (ret)
			break;

		if (rsp.size_dw > 0xf0)
			rsp.size_dw = 0xf0;

		rsp.size_dw -= 3;

		memcpy(data + (cmd.offset_dw * 4), rsp.data, rsp.size_dw * 4);

		cmd.offset_dw += rsp.size_dw;

	} while(total_len_dw > rsp.offset_dw + rsp.size_dw);

	return ret;
}

static int gfms_ep_port_finish(struct switchtec_dev *dev)
{
	struct {
		uint8_t subcmd;
		uint8_t reserved[3];
		uint32_t type;
	} cmd = {
		.subcmd = MRPC_GFMS_DB_DUMP_EP_PORT,
		.type = 3,
	};

	return switchtec_cmd(dev, MRPC_GFMS_DB_DUMP, &cmd, sizeof(cmd),
			     NULL, 0);
}

static size_t gfms_ep_port_attached_ep_parse(
		struct switchtec_dev *dev,
		uint8_t *data,
		struct switchtec_gfms_db_ep_port_ep *ep_port_ep)
{
	size_t len;
	size_t parsed_len;
	uint8_t *p = data;

	len = sizeof(ep_port_ep->ep_hdr);
	memcpy(&ep_port_ep->ep_hdr, p, len);
	p += len;
	parsed_len = len;

	len = ep_port_ep->ep_hdr.size_dw * 4 - sizeof(ep_port_ep->ep_hdr);
	memcpy(ep_port_ep->functions, p, len);
	parsed_len += len;

	return parsed_len;
}

static size_t gfms_ep_port_attache_switch_parse(
		struct switchtec_dev *dev,
		uint8_t *data,
		struct switchtec_gfms_db_ep_port_switch *ep_port_switch)
{
	size_t len;
	size_t parsed_len;
	uint8_t *p = data;

	len = sizeof(ep_port_switch->sw_hdr);
	memcpy(&ep_port_switch->sw_hdr, p, len);
	p += len;
	parsed_len = len;

	len = sizeof(ep_port_switch->ds_switch.internal_functions[0]);
	len = ep_port_switch->sw_hdr.function_number * len;
	memcpy(ep_port_switch->ds_switch.internal_functions, p, len);
	p += len;
	parsed_len += len;

	return parsed_len;
}

static size_t gfms_ep_port_sub_section_parse(
		struct switchtec_dev *dev,
		uint8_t *data,
		struct switchtec_gfms_db_ep_port *ep_port)
{
	int i;
	size_t parsed_len;
	size_t remaining_len;
	size_t len;
	void *p = data;

	len = sizeof(ep_port->port_hdr);
	memcpy(&ep_port->port_hdr, p, len);
	remaining_len = ep_port->port_hdr.size_dw * 4;
	p += len;
	parsed_len = len;
	remaining_len -= len;

	if (ep_port->port_hdr.type == SWITCHTEC_GFMS_DB_TYPE_SWITCH) {
		len = gfms_ep_port_attache_switch_parse(dev, p,
							&ep_port->ep_switch);
		p += len;
		parsed_len += len;
		remaining_len -= len;

		i = 0;
		while (remaining_len) {
			len = gfms_ep_port_attached_ep_parse(
					dev, p,
					&ep_port->ep_switch.switch_eps[i++]);
			p += len;
			parsed_len += len;
			remaining_len -= len;
		}
	} else if (ep_port->port_hdr.type == SWITCHTEC_GFMS_DB_TYPE_EP) {
		len = gfms_ep_port_attached_ep_parse(dev, p, &ep_port->ep_ep);
		p += len;
		parsed_len += len;
	} else if (ep_port->port_hdr.type == SWITCHTEC_GFMS_DB_TYPE_NON) {
	}

	return parsed_len;
}

static size_t gfms_ep_port_section_parse(
		struct switchtec_dev *dev,
		uint8_t *data,
		struct switchtec_gfms_db_ep_port_section *ep_port_section)
{
	size_t len;
	size_t parsed_len;
	void *p = data;

	len = sizeof(ep_port_section->hdr);
	memcpy(&ep_port_section->hdr, p, len);
	p += len;
	parsed_len = len;

	len = ep_port_section->hdr.resp_size_dw * 4 - len;
	len = gfms_ep_port_sub_section_parse(dev, p, &ep_port_section->ep_port);
	parsed_len += len;

	return parsed_len;
}

int switchtec_fab_gfms_db_dump_ep_port(
		struct switchtec_dev *dev,
		uint8_t phy_pid,
		struct switchtec_gfms_db_ep_port_section *ep_port_section)
{
	uint32_t total_len_dw;
	size_t parsed_len;
	uint8_t *data;
	int ret = 0;

	ret = gfms_ep_port_start(dev, phy_pid, &total_len_dw);
	if (ret)
		goto exit;

	data = malloc(total_len_dw * 4);
	ret = gfms_ep_port_get(dev, phy_pid, total_len_dw, data);
	if (ret)
		goto free_and_exit;

	ret = gfms_ep_port_finish(dev);
	if (ret)
		goto free_and_exit;

	parsed_len = gfms_ep_port_section_parse(dev, data, ep_port_section);
	if (parsed_len != total_len_dw * 4)
		ret = -1;

free_and_exit:
	free(data);
exit:
	return ret;
}

static size_t gfms_ep_port_all_section_parse(
		struct switchtec_dev *dev,
		uint8_t *data,
		struct switchtec_gfms_db_ep_port_all_section *ep_port_all)
{
	uint8_t *p = data;
	size_t parsed_len;
	size_t remaining_len;
	struct switchtec_gfms_db_ep_port *ep_port;
	size_t len;
	int i;

	len = sizeof(ep_port_all->hdr);
	memcpy(&ep_port_all->hdr, data, len);
	parsed_len = len;
	p += len;

	remaining_len = ep_port_all->hdr.resp_size_dw * 4 -
			sizeof(ep_port_all->hdr);

	i = 0;
	while (remaining_len) {
		ep_port = &ep_port_all->ep_ports[i];

		len = gfms_ep_port_sub_section_parse(dev, p, ep_port);
		p += len;
		parsed_len += len;
		remaining_len -= len;

		i++;
		ep_port_all->ep_port_count = i;
	}

	return parsed_len;
}

static size_t gfms_pax_all_parse(struct switchtec_dev *dev,
				 uint8_t *data,
				 uint32_t data_len,
				 struct switchtec_gfms_db_pax_all *pax_all)
{
	uint8_t *p = data;
	size_t len;
	size_t parsed_len;

	parsed_len = 0;

	len = gfms_pax_general_section_parse(dev, data, &pax_all->pax_general);
	p += len;
	parsed_len += len;

	len = gfms_hvd_all_section_parse(dev, p, &pax_all->hvd_all);
	p += len;
	parsed_len += len;

	len = gfms_ep_port_all_section_parse(dev, p, &pax_all->ep_port_all);
	parsed_len += len;

	return parsed_len;
}

int switchtec_fab_gfms_db_dump_pax_all(
		struct switchtec_dev *dev,
		struct switchtec_gfms_db_pax_all *pax_all)
{
	uint32_t total_len_dw;
	size_t parsed_len;
	uint8_t *data;
	int ret;

	ret = gfms_dump_start(dev, MRPC_GFMS_DB_DUMP_PAX_ALL, 0, &total_len_dw);
	if (ret)
		return ret;

	data = malloc(total_len_dw * 4);
	ret = gfms_dump_get(dev, MRPC_GFMS_DB_DUMP_PAX_ALL, total_len_dw, data);
	if (ret) {
		free(data);
		return ret;
	}

	ret = gfms_dump_finish(dev, MRPC_GFMS_DB_DUMP_PAX_ALL);
	if (ret) {
		free(data);
		return ret;
	}

	parsed_len = gfms_pax_all_parse(dev, data, total_len_dw * 4, pax_all);

	if (parsed_len != total_len_dw * 4)
		ret = -1;

	free(data);
	return ret;
}

int switchtec_get_gfms_events(struct switchtec_dev *dev,
			      struct switchtec_gfms_event *elist,
			      size_t elist_len, int *overflow,
			      size_t *remain_number)
{
	int ret;
	int event_cnt = 0;
	uint16_t req_num = elist_len;
	uint16_t remain_num;
	struct switchtec_gfms_event *e = elist;
	size_t d_len;
	uint8_t *p;
	int i;

	struct {
		uint8_t subcmd;
		uint8_t reserved;
		uint16_t req_num;
	} req = {
		.subcmd = 1,
		.req_num = req_num,
	};

	struct {
		uint16_t num;
		uint16_t remain_num_flag;
		uint8_t data[MRPC_MAX_DATA_LEN - 4];
	} resp;

	struct entry {
		uint16_t entry_len;
		uint8_t event_code;
		uint8_t src_sw_id;
		uint8_t data[];
	} *hdr;

	do {
		ret = switchtec_cmd(dev, MRPC_GFMS_EVENT, &req,
				    sizeof(req), &resp, sizeof(resp));
		if (ret)
			return -1;

		if ((resp.remain_num_flag & 0x8000) && overflow)
			*overflow = 1;

		p = resp.data;
		for (i = 0; i < resp.num; i++) {
			hdr = (struct entry *)p;
			e->event_code = hdr->event_code;
			e->src_sw_id = hdr->src_sw_id;
			d_len = le32toh(hdr->entry_len) -
				offsetof(struct entry, data);
			memcpy(e->data.byte, hdr->data, d_len);
			p += hdr->entry_len;
			e++;
		};
		event_cnt += resp.num;
		remain_num = resp.remain_num_flag & 0x7fff;
		req_num -= resp.num;
	} while (req_num && remain_num);

	if (remain_number)
		*remain_number = remain_num;

	return event_cnt;
}

int switchtec_clear_gfms_events(struct switchtec_dev *dev)
{
	int ret;
	uint32_t subcmd = 0;

	ret = switchtec_cmd(dev, MRPC_GFMS_EVENT, &subcmd, sizeof(subcmd),
			    NULL, 0);
	if (ret)
		return -1;

	return 0;
}

static int ep_csr_read(struct switchtec_dev *dev,
		       uint16_t pdfid, void *dest,
		       const void __csr *src, size_t n)
{
	int ret;
	uint16_t csr_addr;

	if (n > SWITCHTEC_EP_CSR_MAX_READ_LEN)
		n = SWITCHTEC_EP_CSR_MAX_READ_LEN;

	if (!n)
		return n;

	csr_addr = (uint16_t)(src - (void __csr *)dev->ep_csr_map);

	struct ep_cfg_read {
		uint8_t subcmd;
		uint8_t reserved0;
		uint16_t pdfid;
		uint16_t addr;
		uint8_t bytes;
		uint8_t reserved1;
	} cmd = {
		.subcmd = 0,
		.pdfid = pdfid,
		.addr = htole16(csr_addr),
		.bytes= n,
	};

	struct {
		uint32_t data;
	} rsp;

	ret = switchtec_cmd(dev, MRPC_EP_RESOURCE_ACCESS, &cmd,
			    sizeof(cmd), &rsp, 4);
	if (ret)
		return -1;

	memcpy(dest, &rsp.data, n);
	return 0;
}

void __csr * switchtec_ep_csr_map(struct switchtec_dev *dev)
{
	void *addr;
	dev->ep_csr_map_size = 4 << 20;

	/*
	 * Ensure that if someone tries to do something stupid,
	 * like dereference the EP CSR directly we fail without
	 * trashing random memory somewhere. We do this by
	 * allocating an innaccessible range in the virtual
	 * address space and use that as the EP CSR address which
	 * will be subtracted by subsequent operations
	 */

	addr = mmap(NULL, dev->ep_csr_map_size, PROT_NONE,
		    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (addr == MAP_FAILED)
		return (void __csr __force *)-1;

	dev->ep_csr_map = (void __csr __force *)addr;

	return dev->ep_csr_map;
}

void switchtec_ep_csr_unmap(struct switchtec_dev *dev,
			    void __csr __force *map)
{
	munmap((void __force *)map, dev->ep_csr_map_size);
}

int switchtec_ep_csr_read8(struct switchtec_dev *dev, uint16_t pdfid,
			   uint8_t __csr *addr, uint8_t *val)
{
	return ep_csr_read(dev, htole16(pdfid), val, addr, 1);
}

int switchtec_ep_csr_read16(struct switchtec_dev *dev, uint16_t pdfid,
			    uint16_t __csr *addr, uint16_t *val)
{
	int ret;

	ret = ep_csr_read(dev, htole16(pdfid), val, addr, 2);
	*val = le16toh(*val);

	return ret;
}

int switchtec_ep_csr_read32(struct switchtec_dev *dev, uint16_t pdfid,
			    uint32_t __csr *addr, uint32_t *val)
{
	int ret;

	ret = ep_csr_read(dev, htole16(pdfid), val, addr, 4);
	*val = le32toh(*val);

	return ret;
}
