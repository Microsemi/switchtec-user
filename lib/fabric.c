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

	if (switchtec_is_gen5(dev))
		cmd.subcmd = MRPC_TOPO_INFO_DUMP_DATA_GET_GEN5;

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

static int topo_info_dump_gen4(struct switchtec_dev *dev,
			       struct switchtec_fab_topo_info *topo_info)
{
	int ret;
	int status;
	uint16_t total_info_len, offset, buf_len;
	struct topo_info_reply_gen4 {
		uint8_t sw_idx;
		uint8_t rsvd[3];
		uint32_t stack_bif[7];
		uint8_t route_port[16];
		uint64_t port_bitmap;

		struct switchtec_fab_port_info list[SWITCHTEC_MAX_PORTS];
	} reply = {};

	char *buf = (char *)&reply;

	ret = topo_info_dump_start(dev);
	if (ret)
		return ret;

	do {
		ret = topo_info_dump_status_get(dev, &status, &total_info_len);
		if (ret)
			return ret;
	} while (status == SWITCHTEC_FAB_TOPO_INFO_DUMP_WAIT);

	if (status != SWITCHTEC_FAB_TOPO_INFO_DUMP_READY)
		return -1;

	if (total_info_len > sizeof(reply))
		return -1;

	offset = 0;
	buf_len = sizeof(reply);

	while (offset < total_info_len) {
		ret = topo_info_dump_data_get(dev, offset,
					      buf + offset, &buf_len);
		if (ret)
			return ret;

		offset += buf_len;
		buf_len = sizeof(reply) - offset;
	}

	ret = topo_info_dump_finish(dev);
	if (ret)
		return ret;

	topo_info->sw_idx = reply.sw_idx;
	topo_info->num_stack_bif = 7;
	memcpy(topo_info->stack_bif, reply.stack_bif, 7 * sizeof(uint32_t));
	memcpy(topo_info->route_port, reply.route_port, 16 * sizeof(uint8_t));
	topo_info->port_bitmap = reply.port_bitmap;
	memcpy(topo_info->port_info_list, reply.list,
	       total_info_len - (sizeof(reply) - sizeof(reply.list)));

	return 0;
}

static int topo_info_dump_gen5(struct switchtec_dev *dev,
			       struct switchtec_fab_topo_info *topo_info)
{
	int ret;
	int status;
	uint16_t total_info_len, offset, buf_len;
	struct topo_info_reply_gen5 {
		uint8_t sw_idx;
		uint8_t rsvd[3];
		uint32_t stack_bif[8];
		uint8_t route_port[16];
		uint64_t port_bitmap;

		struct switchtec_fab_port_info list[SWITCHTEC_MAX_PORTS];
	} reply = {};

	char *buf = (char *)&reply;

	ret = topo_info_dump_start(dev);
	if (ret)
		return ret;

	do {
		ret = topo_info_dump_status_get(dev, &status, &total_info_len);
		if (ret)
			return ret;
	} while (status == SWITCHTEC_FAB_TOPO_INFO_DUMP_WAIT);

	if (status != SWITCHTEC_FAB_TOPO_INFO_DUMP_READY)
		return -1;

	if (total_info_len > sizeof(reply))
		return -1;

	offset = 0;
	buf_len = sizeof(reply);

	while (offset < total_info_len) {
		ret = topo_info_dump_data_get(dev, offset,
					      buf + offset, &buf_len);
		if (ret)
			return ret;

		offset += buf_len;
		buf_len = sizeof(reply) - offset;
	}

	ret = topo_info_dump_finish(dev);
	if (ret)
		return ret;

	topo_info->sw_idx = reply.sw_idx;
	topo_info->num_stack_bif = 8;
	memcpy(topo_info->stack_bif, reply.stack_bif, 8 * sizeof(uint32_t));
	memcpy(topo_info->route_port, reply.route_port, 16 * sizeof(uint8_t));
	topo_info->port_bitmap = reply.port_bitmap;
	memcpy(topo_info->port_info_list, reply.list,
	       total_info_len - (sizeof(reply) - sizeof(reply.list)));

	return 0;
}

/**
 * @brief Get the topology of the specified switch
 * @param[in]  dev		Switchtec device handle
 * @param[out] topo_info	The topology info
 * @return 0 on success, error code on failure
 */
int switchtec_topo_info_dump(struct switchtec_dev *dev,
			     struct switchtec_fab_topo_info *topo_info)
{
	if (!switchtec_is_pax_all(dev)) {
		errno = ENOTSUP;
		return -1;
	}

	if (switchtec_is_gen4(dev))
		return topo_info_dump_gen4(dev, topo_info);
	else
		return topo_info_dump_gen5(dev, topo_info);
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

int switchtec_device_manage(struct switchtec_dev *dev,
			    struct switchtec_device_manage_req *req,
			    struct switchtec_device_manage_rsp *rsp)
{
	int ret;

	req->hdr.expected_rsp_len = htole16(req->hdr.expected_rsp_len);
	req->hdr.pdfid = htole16(req->hdr.pdfid);

	ret = switchtec_cmd(dev, MRPC_DEVICE_MANAGE_CMD,
			    req, sizeof(struct switchtec_device_manage_req),
			    rsp, sizeof(struct switchtec_device_manage_rsp));

	rsp->hdr.rsp_len = le16toh(rsp->hdr.rsp_len);

	return ret;
}

int switchtec_ep_tunnel_config(struct switchtec_dev *dev, uint16_t subcmd,
			       uint16_t pdfid, uint16_t expected_rsp_len,
			       uint8_t *meta_data, uint16_t meta_data_len,
			       uint8_t *rsp_data)
{
	int ret;
	size_t payload_len;

	struct cfg_req {
		uint16_t subcmd;
		uint16_t pdfid;
		uint16_t expected_rsp_len;
		uint16_t meta_data_len;
		uint8_t meta_data[MRPC_MAX_DATA_LEN - 8];
	} req = {
		.subcmd = htole16(subcmd),
		.pdfid = htole16(pdfid),
		.expected_rsp_len = htole16(expected_rsp_len),
	};

	struct cfg_rsp {
		uint32_t len;
		uint8_t data[MRPC_MAX_DATA_LEN - 4];
	} rsp;

	if (meta_data_len > sizeof(req.meta_data))
		return -1;

	req.meta_data_len = htole16(meta_data_len);

	if (meta_data_len)
		memcpy(req.meta_data, meta_data, meta_data_len);

	payload_len = offsetof(struct cfg_req, meta_data) + meta_data_len;

	ret = switchtec_cmd(dev, MRPC_EP_TUNNEL_CFG, &req,
			    payload_len, &rsp, sizeof(rsp));

	if (ret)
		return -errno;

	rsp.len = le32toh(rsp.len);

	if (rsp_data && rsp.len)
		memcpy(rsp_data, rsp.data, rsp.len);

	return 0;
}

int switchtec_ep_tunnel_enable(struct switchtec_dev *dev, uint16_t pdfid)
{
	return switchtec_ep_tunnel_config(dev, MRPC_EP_TUNNEL_ENABLE,
					  pdfid, 0, NULL, 0, NULL);
}

int switchtec_ep_tunnel_disable(struct switchtec_dev *dev, uint16_t pdfid)
{
	return switchtec_ep_tunnel_config(dev, MRPC_EP_TUNNEL_DISABLE,
					  pdfid, 0, NULL, 0, NULL);
}

int switchtec_ep_tunnel_status(struct switchtec_dev *dev, uint16_t pdfid,
			       uint32_t *status)
{
	int ret;

	ret = switchtec_ep_tunnel_config(dev, MRPC_EP_TUNNEL_STATUS,
					 pdfid, sizeof(*status), NULL,
					 0, (uint8_t *)status);
	*status = le32toh(*status);

	return ret;
}

static int ep_csr_read(struct switchtec_dev *dev,
		       uint16_t pdfid, void *dest,
		       uint16_t src, size_t n)
{
	int ret;

	if (n > SWITCHTEC_EP_CSR_MAX_READ_LEN)
		n = SWITCHTEC_EP_CSR_MAX_READ_LEN;

	if (!n)
		return n;

	struct ep_cfg_read {
		uint8_t subcmd;
		uint8_t reserved0;
		uint16_t pdfid;
		uint16_t addr;
		uint8_t bytes;
		uint8_t reserved1;
	} cmd = {
		.subcmd = 0,
		.pdfid = htole16(pdfid),
		.addr = htole16(src),
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

int switchtec_ep_csr_read8(struct switchtec_dev *dev, uint16_t pdfid,
			   uint16_t addr, uint8_t *val)
{
	return ep_csr_read(dev, pdfid, val, addr, 1);
}

int switchtec_ep_csr_read16(struct switchtec_dev *dev, uint16_t pdfid,
			    uint16_t addr, uint16_t *val)
{
	int ret;

	ret = ep_csr_read(dev, pdfid, val, addr, 2);
	*val = le16toh(*val);

	return ret;
}

int switchtec_ep_csr_read32(struct switchtec_dev *dev, uint16_t pdfid,
			    uint16_t addr, uint32_t *val)
{
	int ret;

	ret = ep_csr_read(dev, pdfid, val, addr, 4);
	*val = le32toh(*val);

	return ret;
}

static int ep_csr_write(struct switchtec_dev *dev, uint16_t pdfid,
			uint16_t addr, const void *val, size_t n)
{
	if (n > SWITCHTEC_EP_CSR_MAX_WRITE_LEN)
		n = SWITCHTEC_EP_CSR_MAX_WRITE_LEN;

	if (!n)
		return n;

	struct ep_cfg_write {
		uint8_t subcmd;
		uint8_t reserved0;
		uint16_t pdfid;
		uint16_t addr;
		uint8_t bytes;
		uint8_t reserved1;
		uint32_t data;
	} cmd = {
		.subcmd = 1,
		.pdfid = htole16(pdfid),
		.addr = htole16(addr),
		.bytes= n,
	};

	memcpy(&cmd.data, val, n);

	return switchtec_cmd(dev, MRPC_EP_RESOURCE_ACCESS, &cmd,
			     sizeof(cmd), NULL, 0);
}

int switchtec_ep_csr_write8(struct switchtec_dev *dev, uint16_t pdfid,
			    uint8_t val, uint16_t addr)
{
	return ep_csr_write(dev, pdfid, addr, &val, 1);
}

int switchtec_ep_csr_write16(struct switchtec_dev *dev, uint16_t pdfid,
			     uint16_t val, uint16_t addr)
{
	val = htole16(val);
	return ep_csr_write(dev, pdfid, addr, &val, 2);
}

int switchtec_ep_csr_write32(struct switchtec_dev *dev, uint16_t pdfid,
			     uint32_t val, uint16_t addr)
{
	val = htole32(val);
	return ep_csr_write(dev, pdfid, addr, &val, 4);
}

static size_t ep_bar_read(struct switchtec_dev *dev, uint16_t pdfid,
			  uint8_t bar, void *dest,
			  uint64_t src, size_t n)
{
	if (n > SWITCHTEC_EP_BAR_MAX_READ_LEN)
		n = SWITCHTEC_EP_BAR_MAX_READ_LEN;

	if (!n)
		return n;

	src = htole64(src);

	struct ep_bar_read {
		uint8_t subcmd;
		uint8_t reserved0;
		uint16_t pdfid;
		uint8_t bar;
		uint8_t reserved1;
		uint16_t bytes;
		uint32_t addr_low;
		uint32_t addr_high;
	} cmd = {
		.subcmd = 2,
		.pdfid = htole16(pdfid),
		.bar = bar,
		.addr_low = (uint32_t)src,
		.addr_high = (uint32_t)(src >> 32),
		.bytes= htole16((uint16_t)n),
	};

	return switchtec_cmd(dev, MRPC_EP_RESOURCE_ACCESS, &cmd,
			     sizeof(cmd), dest, n);
}

int switchtec_ep_bar_read8(struct switchtec_dev *dev, uint16_t pdfid,
			   uint8_t bar, uint64_t addr, uint8_t *val)
{
	return ep_bar_read(dev, pdfid, bar, val, addr, 1);
}

int switchtec_ep_bar_read16(struct switchtec_dev *dev, uint16_t pdfid,
			    uint8_t bar, uint64_t addr, uint16_t *val)
{
	int ret;

	ret = ep_bar_read(dev, pdfid, bar, val, addr, 2);
	*val = le16toh(*val);

	return ret;
}

int switchtec_ep_bar_read32(struct switchtec_dev *dev, uint16_t pdfid,
			    uint8_t bar, uint64_t addr, uint32_t *val)
{
	int ret;

	ret = ep_bar_read(dev, pdfid, bar, val, addr, 4);
	*val = le32toh(*val);

	return ret;
}

int switchtec_ep_bar_read64(struct switchtec_dev *dev, uint16_t pdfid,
			    uint8_t bar, uint64_t addr, uint64_t *val)
{
	int ret;

	ret = ep_bar_read(dev, pdfid, bar, val, addr, 8);
	*val = le64toh(*val);

	return ret;
}

static int ep_bar_write(struct switchtec_dev *dev, uint16_t pdfid,
			uint8_t bar, uint64_t addr,
			const void *val, size_t n)
{
	if (n > SWITCHTEC_EP_BAR_MAX_WRITE_LEN)
		n = SWITCHTEC_EP_BAR_MAX_WRITE_LEN;

	if (!n)
		return n;

	addr = htole64(addr);

	struct ep_bar_write {
		uint8_t subcmd;
		uint8_t reserved0;
		uint16_t pdfid;
		uint8_t bar;
		uint8_t reserved1;
		uint16_t bytes;
		uint32_t addr_low;
		uint32_t addr_high;
		uint32_t data[128];
	} cmd = {
		.subcmd = 3,
		.pdfid = htole16(pdfid),
		.bar = bar,
		.bytes= htole16((uint16_t)n),
		.addr_low = (uint32_t)addr,
		.addr_high = (uint32_t)(addr >> 32),
	};

	memcpy(&cmd.data, val, n);

	return switchtec_cmd(dev, MRPC_EP_RESOURCE_ACCESS,
			     &cmd, sizeof(cmd), NULL, 0);
}

int switchtec_ep_bar_write8(struct switchtec_dev *dev, uint16_t pdfid,
			    uint8_t bar, uint8_t val, uint64_t addr)
{
	return ep_bar_write(dev, pdfid, bar, addr, &val, 1);
}

int switchtec_ep_bar_write16(struct switchtec_dev *dev, uint16_t pdfid,
			     uint8_t bar, uint16_t val, uint64_t addr)
{
	val = htole16(val);
	return ep_bar_write(dev, pdfid, bar, addr, &val, 2);
}

int switchtec_ep_bar_write32(struct switchtec_dev *dev, uint16_t pdfid,
			     uint8_t bar, uint32_t val, uint64_t addr)
{
	val = htole32(val);
	return ep_bar_write(dev, pdfid, bar, addr, &val, 4);
}

int switchtec_ep_bar_write64(struct switchtec_dev *dev, uint16_t pdfid,
			     uint8_t bar, uint64_t val, uint64_t addr)
{
	val = htole64(val);
	return ep_bar_write(dev, pdfid, bar, addr, &val, 8);
}

static int admin_passthru_start(struct switchtec_dev *dev, uint16_t pdfid,
				size_t data_len, void *data,
				size_t *rsp_len)
{
	int ret;
	uint16_t copy_len;
	uint16_t offset = 0;

	struct {
		uint8_t subcmd;
		uint8_t rsvd[3];
		uint16_t pdfid;
		uint16_t expected_rsp_len;
		uint8_t more_data;
		uint8_t rsvd1[3];
		uint16_t data_offset;
		uint16_t data_len;
		uint8_t data[MRPC_MAX_DATA_LEN - 16];
	} cmd = {
		.subcmd = MRPC_NVME_ADMIN_PASSTHRU_START,
		.pdfid = htole16(pdfid)
	};

	struct {
		uint16_t rsp_len;
		uint16_t rsvd1;
	} reply = {};

	if (data_len && data != NULL) {
		cmd.more_data = data_len > sizeof(cmd.data);
		while (cmd.more_data) {
			copy_len = sizeof(cmd.data);
			memcpy(cmd.data, data + offset, copy_len);

			cmd.data_offset = htole16(offset);
			cmd.data_len = htole16(copy_len);

			ret = switchtec_cmd(dev, MRPC_NVME_ADMIN_PASSTHRU,
					    &cmd, sizeof(cmd), NULL, 0);
			if (ret)
				return ret;

			offset += copy_len;
			data_len -= copy_len;
			cmd.more_data = data_len > sizeof(cmd.data);
		}

		if (data_len) {
			memcpy(cmd.data, data + offset, data_len);

			cmd.data_offset = htole16(offset);
			cmd.data_len = htole16(data_len);
		} else {
			cmd.data_len = 0;
			cmd.data_offset = 0;
		}
	}

	cmd.expected_rsp_len = htole16(*rsp_len);

	ret = switchtec_cmd(dev, MRPC_NVME_ADMIN_PASSTHRU,
			    &cmd, sizeof(cmd), &reply, sizeof(reply));
	if (ret) {
		*rsp_len = 0;
		return ret;
	}

	*rsp_len = le16toh(reply.rsp_len);
	return 0;
}

static int admin_passthru_data(struct switchtec_dev *dev, uint16_t pdfid,
			       size_t rsp_len, void *rsp)
{
	size_t offset = 0;
	int ret;
	struct {
		uint8_t subcmd;
		uint8_t rsvd[3];
		uint16_t pdfid;
		uint16_t offset;
	} cmd = {
		.subcmd = MRPC_NVME_ADMIN_PASSTHRU_DATA,
		.pdfid = htole16(pdfid),
	};

	struct {
		uint16_t offset;
		uint16_t len;
		uint8_t data[MRPC_MAX_DATA_LEN - 4];
	} reply = {};

	while (offset < rsp_len) {
		cmd.offset = htole16(offset);

		ret = switchtec_cmd(dev, MRPC_NVME_ADMIN_PASSTHRU,
				    &cmd, sizeof(cmd), &reply,
				    sizeof(reply));
		if (ret)
			return ret;

		memcpy((uint8_t*)rsp + offset, reply.data,
		       htole16(reply.len));
		offset += htole16(reply.len);
	}

	return 0;
}

static int admin_passthru_end(struct switchtec_dev *dev, uint16_t pdfid)
{
	struct {
		uint8_t subcmd;
		uint8_t rsvd[3];
		uint16_t pdfid;
		uint16_t rsvd1;
	} cmd = {};

	cmd.subcmd = MRPC_NVME_ADMIN_PASSTHRU_END;
	cmd.pdfid = htole16(pdfid);

	return switchtec_cmd(dev, MRPC_NVME_ADMIN_PASSTHRU,
			     &cmd, sizeof(cmd), NULL, 0);
}

/**
 * @brief Send an ADMIN PASSTHRU command to device and get reply
 * @param[in] dev	Switchtec device handle
 * @param[in] pdfid	The PDFID for the device
 * @param[in] data_len	Length of data for this command
 * @param[in] data	Data for this command
 * @param[in/out] rsp_len Expected reply length/actual response length
 * @param[out] rsp	Reply from device
 * @return 0 on success, error code on failure
 */
int switchtec_nvme_admin_passthru(struct switchtec_dev *dev, uint16_t pdfid,
				  size_t data_len, void *data,
				  size_t *rsp_len, void *rsp)
{
	int ret;

	ret = admin_passthru_start(dev, pdfid, data_len, data, rsp_len);
	if (ret)
		return ret;

	if (*rsp_len && rsp != NULL) {
		ret = admin_passthru_data(dev, pdfid, *rsp_len, rsp);
		if (ret) {
			*rsp_len = 0;
			return ret;
		}
	}

	ret = admin_passthru_end(dev, pdfid);

	return ret;
}
