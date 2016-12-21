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

#include "switchtec/pmon.h"
#include "switchtec_priv.h"
#include "switchtec/switchtec.h"

#include <errno.h>

#define ENTRY(x, h) {.mask=x, .name=#x, .help=h}

const struct switchtec_evcntr_type_list switchtec_evcntr_type_list[] = {
	ENTRY(ALL, "All Events"),
	ENTRY(ALL_TLPS, "All TLPs"),
	ENTRY(ALL_ERRORS, "All errors"),
	ENTRY(UNSUP_REQ_ERR, "Unsupported Request error"),
	ENTRY(ECRC_ERR, "ECRC error"),
	ENTRY(MALFORM_TLP_ERR, "Malformed TLP error"),
	ENTRY(RCVR_OFLOW_ERR, "Receiver overflow error"),
	ENTRY(CMPLTR_ABORT_ERR, "Completer Abort error"),
	ENTRY(POISONED_TLP_ERR, "Poisoned TLP error"),
	ENTRY(SURPRISE_DOWN_ERR, "Surprise down error"),
	ENTRY(DATA_LINK_PROTO_ERR, "Data Link protocol error"),
	ENTRY(HDR_LOG_OFLOW_ERR, "Header Log Overflow error"),
	ENTRY(UNCOR_INT_ERR, "Uncorrectable Internal error"),
	ENTRY(REPLAY_TMR_TIMEOUT, "Replay timer timeout"),
	ENTRY(REPLAY_NUM_ROLLOVER, "Replay number rollover"),
	ENTRY(BAD_DLPP, "Bad DLLP"),
	ENTRY(BAD_TLP, "Bad TLP"),
	ENTRY(RCVR_ERR, "Receiver error"),
	ENTRY(RCV_FATAL_MSG, "Receive FATAL error message"),
	ENTRY(RCV_NON_FATAL_MSG, "Receive Non-FATAL error message"),
	ENTRY(RCV_CORR_MSG, "Receive Correctable error message"),
	ENTRY(NAK_RCVD, "NAK received"),
	ENTRY(RULE_TABLE_HIT, "Rule Search Table Rule Hit"),
	ENTRY(POSTED_TLP, "Posted TLP"),
	ENTRY(COMP_TLP, "Completion TLP"),
	ENTRY(NON_POSTED_TLP, "Non-Posted TLP"),
	{0}};

int switchtec_evcntr_type_count(void)
{
	const struct switchtec_evcntr_type_list *t;
	int i = 0;

	for (t = switchtec_evcntr_type_list; t->name; t++, i++);

	return i;
}

int switchtec_evcntr_setup(struct switchtec_dev *dev, unsigned stack_id,
			   unsigned cntr_id,
			   struct switchtec_evcntr_setup *setup)
{
	struct pmon_event_counter_setup cmd = {
		.sub_cmd_id = MRPC_PMON_SETUP_EV_COUNTER,
		.stack_id = stack_id,
		.counter_id = cntr_id,
		.num_counters = 1,

		.counters = {
			[0] = {
				.port_mask = setup->port_mask,
				.type_mask = htole32(setup->type_mask),
				.ieg = setup->egress,
				.thresh = htole32(setup->threshold),
			},
		},
	};

	if (cntr_id >= SWITCHTEC_MAX_EVENT_COUNTERS) {
		errno = EINVAL;
		return -errno;
	}

	return switchtec_cmd(dev, MRPC_PMON, &cmd, sizeof(cmd),
			     NULL, 0);
}

static int evcntr_get(struct switchtec_dev *dev, int sub_cmd,
		      unsigned stack_id, unsigned cntr_id, unsigned nr_cntrs,
		      void *res, size_t res_size)
{
	int ret;

	struct pmon_event_counter_get cmd =  {
		.sub_cmd_id = sub_cmd,
		.stack_id = stack_id,
		.counter_id = cntr_id,
		.num_counters = nr_cntrs,
	};

	if (res_size > MRPC_MAX_DATA_LEN ||
	    cntr_id >= SWITCHTEC_MAX_EVENT_COUNTERS ||
	    nr_cntrs > SWITCHTEC_MAX_EVENT_COUNTERS ||
	    cntr_id + nr_cntrs > SWITCHTEC_MAX_EVENT_COUNTERS)
	{
		errno = EINVAL;
		return -errno;
	}

	ret = switchtec_cmd(dev, MRPC_PMON, &cmd, sizeof(cmd),
			    res, res_size);

	if (ret) {
		errno = ret;
		return -EINVAL;
	}

	return 0;
}

int switchtec_evcntr_get_setup(struct switchtec_dev *dev, unsigned stack_id,
			       unsigned cntr_id, unsigned nr_cntrs,
			       struct switchtec_evcntr_setup *res)
{
	int ret;
	int i;
	struct pmon_event_counter_get_setup_result data[nr_cntrs];

	if (res == NULL) {
		errno = EINVAL;
		return -errno;
	}

	ret = evcntr_get(dev, MRPC_PMON_GET_EV_COUNTER_SETUP,
			 stack_id, cntr_id, nr_cntrs, data,
			 sizeof(data));
	if (ret)
		return ret;

	for (i = 0; i < nr_cntrs; i++) {
		res[i].port_mask = data[i].port_mask;
		res[i].type_mask = le32toh(data[i].type_mask);
		res[i].egress = data[i].ieg;
		res[i].threshold = le32toh(data[i].thresh);
	}

	return nr_cntrs;
}

int switchtec_evcntr_get(struct switchtec_dev *dev, unsigned stack_id,
			 unsigned cntr_id, unsigned nr_cntrs, unsigned *res,
			 int clear)
{
	int ret;
	int i;
	struct pmon_event_counter_result data[nr_cntrs];

	if (res == NULL) {
		errno = EINVAL;
		return -errno;
	}

	ret = evcntr_get(dev, MRPC_PMON_GET_EV_COUNTER,
			 stack_id, cntr_id, nr_cntrs, data,
			 sizeof(data));
	if (ret)
		return ret;

	for (i = 0; i < nr_cntrs; i++)
		res[i] = le32toh(data[i].value);

	return nr_cntrs;
}

int switchtec_evcntr_get_both(struct switchtec_dev *dev, unsigned stack_id,
			      unsigned cntr_id, unsigned nr_cntrs,
			      struct switchtec_evcntr_setup *setup,
			      unsigned *counts, int clear)
{
	int ret = 0;

	ret = switchtec_evcntr_get_setup(dev, stack_id, cntr_id, nr_cntrs,
					 setup);
	if (ret < 0)
		return ret;

	return switchtec_evcntr_get(dev, stack_id, cntr_id, nr_cntrs,
				    counts, clear);
}

void switchtec_pmon_perror(const char *str)
{
	const char *msg;

	switch (errno) {
	case 0x100001: msg = "Invalid Stack"; break;
	case 0x100002: msg = "Invalid Port"; break;
	case 0x100003: msg = "Invalid Event"; break;
	case 0x100005: msg = "Reset rule search failed"; break;
	case 0xffff0001: msg = "Access Refused"; break;
	default:
		perror(str);
		return;
	}

	fprintf(stderr, "%s: %s\n", str, msg);
}
