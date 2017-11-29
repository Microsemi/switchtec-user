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

/**
 * @file
 * @brief Switchtec core library functions for performance monitoring
 */

#include "switchtec/pmon.h"
#include "switchtec_priv.h"
#include "switchtec/switchtec.h"
#include "switchtec/endian.h"

#include <stddef.h>
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

/**
 * @brief Get the number of event counter types
 * @return The number of types
 */
int switchtec_evcntr_type_count(void)
{
	const struct switchtec_evcntr_type_list *t;
	int i = 0;

	for (t = switchtec_evcntr_type_list; t->name; t++, i++);

	return i;
}

/**
 * @brief Get a string for the event indicated by
 *	lowest bit set in the type_mask
 * @param[in,out] type_mask	Bitmask of types
 * @return Name string
 *
 * Clears the lowest bit in \p type_mask and returns the string
 * corresponding to that type. Thus, this function can be used in
 * a loop to print a list of strings representing a bit mask of
 * types.
 */
const char *switchtec_evcntr_type_str(int *type_mask)
{
	const struct switchtec_evcntr_type_list *t;

	for (t = switchtec_evcntr_type_list; t->name; t++) {
		if ((t->mask & *type_mask) != t->mask)
			continue;

		*type_mask &= ~t->mask;
		return t->name;
	}

	return NULL;
}

/**
 * @brief Setup an event counter performance monitor
 * @param[in] dev	Switchtec device handle
 * @param[in] stack_id	Stack to setup this counter in
 * @param[in] cntr_id   Counter ID to setup
 * @param[in] setup	Event counter setup structure
 * @return 0 on success, error code on failure
 */
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
				.mask = htole32((setup->type_mask << 8) |
						(setup->port_mask & 0xFF)),
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
		      void *res, size_t res_size, int clear)
{
	int ret;

	struct pmon_event_counter_get cmd =  {
		.sub_cmd_id = sub_cmd,
		.stack_id = stack_id,
		.counter_id = cntr_id,
		.num_counters = nr_cntrs,
		.read_clear = clear,
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

/**
 * @brief Retrieve the setup information for one or more event counters
 * @param[in]  dev	Switchtec device handle
 * @param[in]  stack_id	Stack to setup this counter in
 * @param[in]  cntr_id  First counter ID to retrieve
 * @param[in]  nr_cntrs	Number of counters to retrieve
 * @param[out] res	List of event counter setup structures
 *	(at least \p nr_cntrs elements)
 * @return 0 on success, error code on failure
 *
 * Retrieves \p nr_cntrs setup structures sequentially starting at \p cntr_id
 */
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
			 sizeof(*data) * nr_cntrs, 0);
	if (ret)
		return ret;

	for (i = 0; i < nr_cntrs; i++) {
		res[i].port_mask = le32toh(data[i].mask) & 0xFF;
		res[i].type_mask = le32toh(data[i].mask) >> 8;
		res[i].egress = data[i].ieg;
		res[i].threshold = le32toh(data[i].thresh);
	}

	return nr_cntrs;
}

/**
 * @brief Retrieve the current counts for one or more event counters
 * @param[in]  dev	Switchtec device handle
 * @param[in]  stack_id	Stack to setup this counter in
 * @param[in]  cntr_id   Counter ID to setup
 * @param[in]  nr_cntrs	Number of counters to retrieve
 * @param[out] res	List of unsigned values to return the
 *	current counts in (at least \p nr_cntrs elements)
 * @param[in]  clear	If non-zero, clear all the retrieved counters
 *	to zero
 * @return 0 on success, error code on failure
 *
 * Retrieves \p nr_cntrs counter values sequentially starting at \p cntr_id
 */
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
			 sizeof(*data) * nr_cntrs, clear);
	if (ret)
		return ret;

	for (i = 0; i < nr_cntrs; i++)
		res[i] = le32toh(data[i].value);

	return nr_cntrs;
}

/**
 * @brief Retrieve the current counts and setup information for one
 * 	or more event counters
 * @param[in]  dev	Switchtec device handle
 * @param[in]  stack_id	Stack to setup this counter in
 * @param[in]  cntr_id   Counter ID to setup
 * @param[in]  nr_cntrs	Number of counters to retrieve
 * @param[out] setup	List of event counter setup structures
 *	(at least \p nr_cntrs elements)
 * @param[out] counts	List of unsigned values to return the
 *	current counts in (at least \p nr_cntrs elements)
 * @param[in]  clear	If non-zero, clear all the retrieved counters
 *	to zero
 * @return 0 on success, error code on failure
 *
 * Retrieves \p nr_cntrs counter values and setup structures sequentially
 * starting at \p cntr_id. This is equivalent to calling switchtec_evcntr_get()
 * and switchtec_evcntr_get_setup().
 */
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

/**
 * @brief Block until any event counter has reached its threshold
 * @param[in] dev		Switchtec device handle
 * @param[in] timeout_ms	Stop waiting after the specified number
 * 	of milliseconds
 * @return 1 if the event occurred, 0 if a timeout occurred, a negative
 * 	value if an error occurred
 */
int switchtec_evcntr_wait(struct switchtec_dev *dev, int timeout_ms)
{
	return switchtec_event_wait_for(dev, SWITCHTEC_PFF_EVT_THRESH,
					SWITCHTEC_EVT_IDX_ALL,
					NULL, timeout_ms);
}

/**
 * @brief Subtract all the values between two bwcntr result structures
 * @param[in,out] new
 * @param[in] old
 *
 * \p new will have it's original values minus all the values in \p old
 */
void switchtec_bwcntr_sub(struct switchtec_bwcntr_res *new,
			  struct switchtec_bwcntr_res *old)
{
	new->time_us -= old->time_us;
	new->egress.posted -= old->egress.posted;
	new->egress.nonposted -= old->egress.nonposted;
	new->egress.comp -= old->egress.comp;
	new->ingress.posted -= old->ingress.posted;
	new->ingress.nonposted -= old->ingress.nonposted;
	new->ingress.comp -= old->ingress.comp;
}

/**
 * @brief Retrieve the bandwidth counter results for a number of ports
 * @param[in]  dev		Switchtec device handle
 * @param[in]  nr_ports		Number of ports to retrieve
 * @param[in]  phys_port_ids	The physical ids for each port to retrieve
 * @param[in]  clear		If non-zero, clear all the retrieved
 *	counters o zero
 * @param[out] res	List of bandwidth counter results structures
 *	(at least \p nr_ports elements)
 * @return number of ports retrieved on success, negative error
 *	code on failure
 */
int switchtec_bwcntr_many(struct switchtec_dev *dev, int nr_ports,
			  int *phys_port_ids, int clear,
			  struct switchtec_bwcntr_res *res)
{
	int i;
	int ret;
	int remain = nr_ports;
	size_t cmd_size;
	struct pmon_bw_get cmd = {
		.sub_cmd_id = MRPC_PMON_GET_BW_COUNTER,
	};

	while (remain) {
		cmd.count = remain;
		if (cmd.count > MRPC_MAX_DATA_LEN / sizeof(*res))
			cmd.count = MRPC_MAX_DATA_LEN / sizeof(*res);

		for (i = 0; i < cmd.count; i++) {
			cmd.ports[i].id = phys_port_ids[i];
			cmd.ports[i].clear = clear;
		}

		cmd_size = offsetof(struct pmon_bw_get, ports) +
			sizeof(cmd.ports[0]) * cmd.count;

		ret = switchtec_cmd(dev, MRPC_PMON, &cmd, cmd_size, res,
				    sizeof(*res) * cmd.count);
		if (ret)
			return -1;

		remain -= cmd.count;
		phys_port_ids += cmd.count;
		res += cmd.count;
	}

	return nr_ports;
}

/**
 * @brief Retrieve the bandwidth counter results for all the
 *	ports in the system
 * @param[in]  dev	Switchtec device handle
 * @param[in]  clear	If non-zero, clear all the retrieved counters
 * @param[out] ports	Allocated array of ports retrieved
 * @param[out] res	Allocated array of bandwidth counter results
 * @return 0 on success, error code on failure
 *
 * \p ports and \p res should be freed with free() once they are finished
 * with.
 */
int switchtec_bwcntr_all(struct switchtec_dev *dev, int clear,
			 struct switchtec_port_id **ports,
			 struct switchtec_bwcntr_res **res)
{
	int ret, i;
	struct switchtec_status *status;
	int ids[SWITCHTEC_MAX_PORTS];

	ret = switchtec_status(dev, &status);
	if (ret < 0)
		return ret;

	if (ports)
		*ports = calloc(ret, sizeof(**ports));
	if (res)
		*res = calloc(ret, sizeof(**res));

	for (i = 0; i < ret; i++) {
		ids[i] = status[i].port.phys_id;
		if (ports)
			(*ports)[i] = status[i].port;
	}

	ret = switchtec_bwcntr_many(dev, ret, ids, clear, *res);
	if (ret < 0) {
		if (ports)
			free(*ports);
		if (res)
			free(*res);
	}

	free(status);
	return ret;
}

/**
 * @brief Get the total
 * @param[in] d Bandwidth counter direction result
 * @return The total number for posted, non-posted and completions
 */
uint64_t switchtec_bwcntr_tot(struct switchtec_bwcntr_dir *d)
{
	return d->posted + d->nonposted + d->comp;
}

/**
 * @brief Setup a number of latency counters
 * @param[in]  dev		Switchtec device handle
 * @param[in]  nr_ports		Number of latency counters to setup
 * @param[in]  egress_port_ids	A list of port ids for the egress of the TLP
 * @param[in]  ingress_port_ids	A list of port ids for the ingress of the TLP
 *	(may be SWITCHTEC_LAT_ALL_INGRESS for all ports)
 * @return 0 on success, error code on failure
 */
int switchtec_lat_setup_many(struct switchtec_dev *dev, int nr_ports,
			     int *egress_port_ids, int *ingress_port_ids)
{
	int i;
	size_t cmd_size;
	struct pmon_lat_setup cmd = {
		.sub_cmd_id = MRPC_PMON_SETUP_LAT_COUNTER,
		.count = nr_ports,
	};

	for (i = 0; i < nr_ports; i++) {
		cmd.ports[i].egress = egress_port_ids[i];
		cmd.ports[i].ingress = ingress_port_ids[i];
	}

	cmd_size = offsetof(struct pmon_lat_setup, ports) +
		sizeof(cmd.ports[0]) * nr_ports;

	return switchtec_cmd(dev, MRPC_PMON, &cmd, cmd_size, NULL, 0);
}

/**
 * @brief Setup a latency counter
 * @param[in]  dev		Switchtec device handle
 * @param[in]  egress_port_id	The port id for the egress of the TLP
 * @param[in]  ingress_port_id	The port id for the ingress of the TLP
 *	(may be SWITCHTEC_LAT_ALL_INGRESS for all ports)
 * @param[in]  clear		If non-zero, clear the latency counter
 * @return 0 on success, error code on failure
 */
int switchtec_lat_setup(struct switchtec_dev *dev, int egress_port_id,
			int ingress_port_id, int clear)
{
	int ret;

	ret = switchtec_lat_setup_many(dev, 1, &egress_port_id,
				       &ingress_port_id);
	if (ret)
		return ret;

	if (!clear)
		return ret;

	return switchtec_lat_get(dev, 1, egress_port_id, NULL, NULL);
}

/**
 * @brief Get a number of latency counter results
 * @param[in]  dev		Switchtec device handle
 * @param[in]  nr_ports		Number of latency counters to retrieve
 * @param[in]  clear		If non-zero, clear the latency counters
 * @param[in]  egress_port_ids	A list of port ids for the counters to return
 * @param[out] cur_ns		A list of current latency values
 * @param[out] max_ns		A list of maximum latency values
 * @return 0 on success, error code on failure
 *
 * Results are reported in nanoseconds.
 */
int switchtec_lat_get_many(struct switchtec_dev *dev, int nr_ports,
			   int clear, int *egress_port_ids,
			   int *cur_ns, int *max_ns)
{
	int ret, i;
	size_t cmd_size;
	struct pmon_lat_data resp[nr_ports];
	struct pmon_lat_get cmd = {
		.sub_cmd_id = MRPC_PMON_GET_LAT_COUNTER,
		.count = nr_ports,
		.clear = clear,
	};

	for (i = 0; i < nr_ports; i++)
		cmd.port_ids[i] = egress_port_ids[i];

	cmd_size = offsetof(struct pmon_lat_get, port_ids) +
		sizeof(cmd.port_ids[0]) * nr_ports;

	ret = switchtec_cmd(dev, MRPC_PMON, &cmd, cmd_size, resp,
			    sizeof(*resp) * nr_ports);

	if (ret)
		return -1;

	if (cur_ns)
		for (i = 0; i < nr_ports; i++)
			cur_ns[i] = resp[i].cur_ns;

	if (max_ns)
		for (i = 0; i < nr_ports; i++)
			max_ns[i] = resp[i].max_ns;

	return nr_ports;
}

/**
 * @brief Get a single latency counter result
 * @param[in]  dev		Switchtec device handle
 * @param[in]  clear		If non-zero, clear the latency counter
 * @param[in]  egress_port_ids	The egress port of the latency counter
 *	to retrieve
 * @param[out] cur_ns		The current latency value
 * @param[out] max_ns		The maximum latency value
 * @return 0 on success, error code on failure
 *
 * Results are reported in nanoseconds.
 */
int switchtec_lat_get(struct switchtec_dev *dev, int clear,
		      int egress_port_ids, int *cur_ns,
		      int *max_ns)
{
	return switchtec_lat_get_many(dev, 1, clear, &egress_port_ids,
				      cur_ns, max_ns);
}
