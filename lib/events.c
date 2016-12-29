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

#include "switchtec_priv.h"

#include "switchtec/switchtec.h"

#include <linux/switchtec_ioctl.h>

#include <poll.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <errno.h>
#include <string.h>

int switchtec_event_wait(struct switchtec_dev *dev, int timeout_ms)
{
	int ret;
	struct pollfd fds = {
		.fd = dev->fd,
		.events = POLLPRI,
	};

	ret = poll(&fds, 1, timeout_ms);
	if (ret <= 0)
		return ret;

	if (fds.revents & POLLPRI)
		return 1;

	return 0;
}

static void event_summary_copy(struct switchtec_event_summary *dst,
			       struct switchtec_ioctl_event_summary *src)
{
	int i;

	dst->global_summary = src->global_summary;
	dst->part_event_bitmap = src->part_event_bitmap;
	dst->local_part_event_summary = src->local_part_event_summary;

	for (i = 0; i < SWITCHTEC_MAX_PARTS; i++)
		dst->part_event_summary[i] = src->part_event_summary[i];

	for (i = 0; i < SWITCHTEC_MAX_PORTS; i++)
		dst->port_event_summary[i] = src->port_event_summary[i];
}

int switchtec_event_summary(struct switchtec_dev *dev,
			    struct switchtec_event_summary *sum)
{
	int ret;
	struct switchtec_ioctl_event_summary isum;

	if (!sum)
		return -EINVAL;

	ret = ioctl(dev->fd, SWITCHTEC_IOCTL_EVENT_SUMMARY, &isum);
	if (ret < 0)
		return ret;

	event_summary_copy(sum, &isum);

	return 0;
}

int switchtec_event_check(struct switchtec_dev *dev,
			  struct switchtec_event_summary *check,
			  struct switchtec_event_summary *res)
{
	int ret, i;
	struct switchtec_ioctl_event_summary isum;

	if (!check)
		return -EINVAL;

	ret = ioctl(dev->fd, SWITCHTEC_IOCTL_EVENT_SUMMARY, &isum);
	if (ret < 0)
		return ret;

	ret = 0;

	if (isum.global_summary & check->global_summary)
		ret = 1;

	if (isum.part_event_bitmap & check->part_event_bitmap)
		ret = 1;

	if (isum.local_part_event_summary &
	    check->local_part_event_summary)
		ret = 1;

	for (i = 0; i < SWITCHTEC_MAX_PARTS; i++)
		if (isum.part_event_summary[i] & check->part_event_summary[i])
			ret = 1;

	for (i = 0; i < SWITCHTEC_MAX_PORTS; i++)
		if (isum.port_event_summary[i] & check->port_event_summary[i])
			ret = 1;

	if (res)
		event_summary_copy(res, &isum);

	return ret;
}

int switchtec_event_wait_for(struct switchtec_dev *dev,
			     struct switchtec_event_summary *wait_for,
			     struct switchtec_event_summary *res,
			     int timeout_ms)
{
	struct timeval tv;
	long long start, now;
	int ret;

	ret = switchtec_event_check(dev, wait_for, res);
	if (ret < 0)
		return ret;

	if (ret)
		return 1;

	ret = gettimeofday(&tv, NULL);
	if (ret)
		return ret;

	now = start = ((tv.tv_sec) * 1000 + tv.tv_usec / 1000);

	while (1) {
		ret = switchtec_event_wait(dev, timeout_ms > 0 ?
					   now - start + timeout_ms : -1);
		if (ret < 0)
			return ret;

		if (ret == 0)
			goto next;

		ret = switchtec_event_check(dev, wait_for, res);
		if (ret < 0)
			return ret;

		if (ret)
			return 1;

next:
		ret = gettimeofday(&tv, NULL);
		if (ret)
			return ret;

		now = ((tv.tv_sec) * 1000 + tv.tv_usec / 1000);

		if (timeout_ms > 0 && now - start >= timeout_ms)
			return 0;
	}
}

int switchtec_event_ctl(struct switchtec_dev *dev,
			enum switchtec_event_type t,
			enum switchtec_event e,
			int index,
			uint32_t data[5])
{
	int ret;
	struct switchtec_ioctl_event_ctl ctl;

	ctl.index = index;

	switch (t) {
	case SWITCHTEC_GLOBAL_EVT:
		switch (e) {
		case SWITCHTEC_GLOBAL_EVT_STACK_ERR:
			ctl.event_id = SWITCHTEC_IOCTL_EVENT_STACK_ERROR;
			break;
		case SWITCHTEC_GLOBAL_EVT_PPU_ERR:
			ctl.event_id = SWITCHTEC_IOCTL_EVENT_PPU_ERROR;
			break;
		case SWITCHTEC_GLOBAL_EVT_ISP_ERROR:
			ctl.event_id = SWITCHTEC_IOCTL_EVENT_ISP_ERROR;
			break;
		case SWITCHTEC_GLOBAL_EVT_TWI_MRPC_COMP:
			ctl.event_id = SWITCHTEC_IOCTL_EVENT_TWI_MRPC_COMP;
			break;
		case SWITCHTEC_GLOBAL_EVT_TWI_MRPC_COMP_ASYNC:
			ctl.event_id = SWITCHTEC_IOCTL_EVENT_TWI_MRPC_COMP_ASYNC;
			break;
		case SWITCHTEC_GLOBAL_EVT_CLI_MRPC_COMP:
			ctl.event_id = SWITCHTEC_IOCTL_EVENT_CLI_MRPC_COMP;
			break;
		case SWITCHTEC_GLOBAL_EVT_CLI_MRPC_COMP_ASYNC:
			ctl.event_id = SWITCHTEC_IOCTL_EVENT_CLI_MRPC_COMP_ASYNC;
			break;
		case SWITCHTEC_GLOBAL_EVT_GPIO_INT:
			ctl.event_id = SWITCHTEC_IOCTL_EVENT_GPIO_INT;
			break;

		default:
			return -EINVAL;
		}
	case SWITCHTEC_PART_EVT:
		switch (e) {
		case SWITCHTEC_PART_EVT_RESET:
			ctl.event_id = SWITCHTEC_IOCTL_EVENT_PART_RESET;
			break;
		case SWITCHTEC_PART_EVT_MRPC_COMP_ASYNC:
			ctl.event_id = SWITCHTEC_IOCTL_EVENT_MRPC_COMP_ASYNC;
			break;
		case SWITCHTEC_PART_EVT_DYN_PART_BIND:
			ctl.event_id = SWITCHTEC_IOCTL_EVENT_DYN_PART_BIND_COMP;
			break;
		default:
			return -EINVAL;
		}
	case SWITCHTEC_PORT_EVT:
		switch (e) {
		case SWITCHTEC_PORT_EVT_AER_IN_P2P:
			ctl.event_id = SWITCHTEC_IOCTL_EVENT_AER_IN_P2P;
			break;
		case SWITCHTEC_PORT_EVT_AER_INVEP:
			ctl.event_id = SWITCHTEC_IOCTL_EVENT_AER_IN_VEP;
			break;
		case SWITCHTEC_PORT_EVT_DPC:
			ctl.event_id = SWITCHTEC_IOCTL_EVENT_DPC;
			break;
		case SWITCHTEC_PORT_EVT_CTS:
			ctl.event_id = SWITCHTEC_IOCTL_EVENT_CTS;
			break;
		case SWITCHTEC_PORT_EVT_HOTPLUG:
			ctl.event_id = SWITCHTEC_IOCTL_EVENT_HOTPLUG;
			break;
		case SWITCHTEC_PORT_EVT_IER:
			ctl.event_id = SWITCHTEC_IOCTL_EVENT_IER;
			break;
		case SWITCHTEC_PORT_EVT_THRESHOLD:
			ctl.event_id = SWITCHTEC_IOCTL_EVENT_THRESH;
			break;
		case SWITCHTEC_PORT_EVT_PWR_MGMT:
			ctl.event_id = SWITCHTEC_IOCTL_EVENT_POWER_MGMT;
			break;
		case SWITCHTEC_PORT_EVT_TLP_THROTTLING:
			ctl.event_id = SWITCHTEC_IOCTL_EVENT_TLP_THROTTLING;
			break;
		case SWITCHTEC_PORT_EVT_FORCE_SPEED:
			ctl.event_id = SWITCHTEC_IOCTL_EVENT_FORCE_SPEED;
			break;
		case SWITCHTEC_PORT_EVT_CREDIT_TIMEOUT:
			ctl.event_id = SWITCHTEC_IOCTL_EVENT_CREDIT_TIMEOUT;
			break;
		case SWITCHTEC_PORT_EVT_LINK_STATE:
			ctl.event_id = SWITCHTEC_IOCTL_EVENT_LINK_STATE;
			break;
		default:
			return -EINVAL;
		}
	}

	ret = ioctl(dev->fd, SWITCHTEC_IOCTL_EVENT_CTL, &ctl);
	if (ret)
		return ret;

	if (data)
		memcpy(data, ctl.data, sizeof(ctl.data));

	return 0;
}
