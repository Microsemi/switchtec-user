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
 * @brief Switchtec core library functions for event management
 */

#define SWITCHTEC_LIB_CORE

#include "switchtec_priv.h"

#include "switchtec/switchtec.h"
#include "switchtec/utils.h"

#include <sys/time.h>

#include <errno.h>
#include <string.h>
#include <strings.h>

/**
 * @defgroup Event Event Management
 * @brief List and wait for switch events
 *
 * switchtec_event_info() provides an interface to list all possible switch
 * events. switchtec_event_summary() gives a bitmask of events that have
 * occured since they were last cleared. switchtec_event_ctl() can be used
 * to clear and event or manage what happens when an event occurs.
 * switchtec_event_wait_for() may be used to block until a specific event
 * occurs.
 *
 * @{
 */

#define EV(t, n, s, d)[SWITCHTEC_ ## t ## _EVT_ ## n] = {\
	.type = t, \
	.summary_bit = (1 << (s)), \
	.short_name = #n, \
	.desc = d, \
}

#define GLOBAL SWITCHTEC_EVT_GLOBAL
#define PART SWITCHTEC_EVT_PART
#define PFF SWITCHTEC_EVT_PFF

static const struct {
	enum switchtec_event_id id;
	enum switchtec_event_type type;
	uint64_t summary_bit;
	const char *short_name;
	const char *desc;
} events[] = {
	EV(GLOBAL, STACK_ERROR, 0, "Stack Error"),
	EV(GLOBAL, PPU_ERROR, 1, "PPU Error"),
	EV(GLOBAL, ISP_ERROR, 2, "ISP Error"),
	EV(GLOBAL, SYS_RESET, 3, "System Reset"),
	EV(GLOBAL, FW_EXC, 4, "Firmware Exception"),
	EV(GLOBAL, FW_NMI, 5, "Firmware Non-Maskable Interrupt"),
	EV(GLOBAL, FW_NON_FATAL, 6, "Firmware Non-Fatal Error"),
	EV(GLOBAL, FW_FATAL, 7, "Firmware Fatal Error"),
	EV(GLOBAL, TWI_MRPC_COMP, 8, "TWI MRPC Completion"),
	EV(GLOBAL, TWI_MRPC_COMP_ASYNC, 9, "TWI MRPC Async Completion"),
	EV(GLOBAL, CLI_MRPC_COMP, 10, "CLI MRPC Completion"),
	EV(GLOBAL, CLI_MRPC_COMP_ASYNC, 11, "CLI MRPC Async Completion"),
	EV(GLOBAL, GPIO_INT, 12, "GPIO Interrupt"),
	EV(GLOBAL, GFMS, 13, "Global Fabric Management Server Event"),
	EV(PART, PART_RESET, 0, "Partition Reset"),
	EV(PART, MRPC_COMP, 1, "MRPC Completion"),
	EV(PART, MRPC_COMP_ASYNC, 2, "MRPC Async Completion"),
	EV(PART, DYN_PART_BIND_COMP, 3,
	   "Dynamic Partition Binding Completion"),
	EV(PFF, AER_IN_P2P, 0, "Advanced Error Reporting in P2P Port"),
	EV(PFF, AER_IN_VEP, 1, "Advancde Error Reporting in vEP"),
	EV(PFF, DPC, 2, "Downstream Port Containment Event"),
	EV(PFF, CTS, 3, "Completion Timeout Synthesis Event"),
	EV(PFF, HOTPLUG, 5, "Hotplug Event"),
	EV(PFF, IER, 6, "Internal Error Reporting Event"),
	EV(PFF, THRESH, 7, "Event Counter Threshold Reached"),
	EV(PFF, POWER_MGMT, 8, "Power Management Event"),
	EV(PFF, TLP_THROTTLING, 9, "TLP Throttling Event"),
	EV(PFF, FORCE_SPEED, 10, "Force Speed Error"),
	EV(PFF, CREDIT_TIMEOUT, 11, "Credit Timeout"),
	EV(PFF, LINK_STATE, 12, "Link State Change Event"),
};

#define EVBIT(t, n, b)[b] = SWITCHTEC_ ## t ## _EVT_ ## n
static const enum switchtec_event_id global_event_bits[64] = {
	[0 ... 63] = -1,
	EVBIT(GLOBAL, STACK_ERROR, 0),
	EVBIT(GLOBAL, PPU_ERROR, 1),
	EVBIT(GLOBAL, ISP_ERROR, 2),
	EVBIT(GLOBAL, SYS_RESET, 3),
	EVBIT(GLOBAL, FW_EXC, 4),
	EVBIT(GLOBAL, FW_NMI, 5),
	EVBIT(GLOBAL, FW_NON_FATAL, 6),
	EVBIT(GLOBAL, FW_FATAL, 7),
	EVBIT(GLOBAL, TWI_MRPC_COMP, 8),
	EVBIT(GLOBAL, TWI_MRPC_COMP_ASYNC, 9),
	EVBIT(GLOBAL, CLI_MRPC_COMP, 10),
	EVBIT(GLOBAL, CLI_MRPC_COMP_ASYNC, 11),
	EVBIT(GLOBAL, GPIO_INT, 12),
};

static const enum switchtec_event_id part_event_bits[64] = {
	[0 ... 63] = -1,
	EVBIT(PART, PART_RESET, 0),
	EVBIT(PART, MRPC_COMP, 1),
	EVBIT(PART, MRPC_COMP_ASYNC, 2),
	EVBIT(PART, DYN_PART_BIND_COMP, 3),
};

static const enum switchtec_event_id pff_event_bits[64] = {
	[0 ... 63] = -1,
	EVBIT(PFF, AER_IN_P2P, 0),
	EVBIT(PFF, AER_IN_VEP, 1),
	EVBIT(PFF, DPC, 2),
	EVBIT(PFF, CTS, 3),
	EVBIT(PFF, HOTPLUG, 5),
	EVBIT(PFF, IER, 6),
	EVBIT(PFF, THRESH, 7),
	EVBIT(PFF, POWER_MGMT, 8),
	EVBIT(PFF, TLP_THROTTLING, 9),
	EVBIT(PFF, FORCE_SPEED, 10),
	EVBIT(PFF, CREDIT_TIMEOUT, 11),
	EVBIT(PFF, LINK_STATE, 12),
};

static void set_all_parts(struct switchtec_event_summary *sum, uint64_t bit)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sum->part); i++)
		sum->part[i] |= bit;
}

static void set_all_pffs(struct switchtec_event_summary *sum, uint64_t bit)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sum->pff); i++)
		sum->pff[i] |= bit;
}

/**
 * @brief Set a bit corresponding to an event in a summary structure
 * @param[in] sum	Summary structure to set the bit in
 * @param[in] e		Event ID to set
 * @param[in] index	Event index (partition or port, depending on event)
 * @return 0 on success, or -EINVAL if the index was invalid
 */
int switchtec_event_summary_set(struct switchtec_event_summary *sum,
				enum switchtec_event_id e,
				int index)
{
	uint64_t bit = events[e].summary_bit;

	switch (events[e].type) {
	case GLOBAL:
		sum->global |= bit;
		break;
	case PART:
		if (index == SWITCHTEC_EVT_IDX_LOCAL) {
			sum->local_part |= bit;
		} else if (index == SWITCHTEC_EVT_IDX_ALL) {
			set_all_parts(sum, bit);
		} else if (index < 0 || index >= ARRAY_SIZE(sum->part)) {
			errno = EINVAL;
			return -EINVAL;
		} else {
			sum->part[index] |= bit;
		}
		break;
	case PFF:
		if (index == SWITCHTEC_EVT_IDX_ALL) {
			set_all_pffs(sum, bit);
		} else if (index < 0 || index >= ARRAY_SIZE(sum->pff)) {
			errno = EINVAL;
			return -EINVAL;
		} else {
			sum->pff[index] |= bit;
		}
		break;
	}

	return 0;
}

/**
 * @brief Test if a bit corresponding to an event is set in a summary structure
 * @param[in] sum	Summary structure to set the bit in
 * @param[in] e		Event ID to test
 * @param[in] index	Event index (partition or port, depending on event)
 * @return 1 if the bit is set, 0 otherwise
 */
int switchtec_event_summary_test(struct switchtec_event_summary *sum,
				 enum switchtec_event_id e,
				 int index)
{
	uint64_t bit = events[e].summary_bit;

	switch (events[e].type) {
	case GLOBAL:
		return sum->global & bit;
	case PART:
		return sum->part[index] & bit;
	case PFF:
		return sum->pff[index] & bit;
	}

	return 0;
}

/**
 * @brief Iterate through all set bits in an event summary structure
 * @param[in]  sum	Summary structure to set the bit in
 * @param[out] e	Event ID which was set
 * @param[out] idx	Event index (partition or port, depending on event)
 * @return 1 if a bit is set, 0 otherwise
 *
 * This function is meant to be called in a loop. It finds the lowest
 * bit set and returns the corresponding event id and index. It then
 * clears that bit in the structure.
 */
int switchtec_event_summary_iter(struct switchtec_event_summary *sum,
				 enum switchtec_event_id *e,
				 int *idx)
{
	int bit;

	if (!idx || !e)
		return -EINVAL;

	*idx = 0;

	bit = ffs(sum->global) - 1;
	if (bit >= 0) {
		*e = global_event_bits[bit];
		sum->global &= ~(1 << bit);
		return 1;
	}

	for (*idx = 0; *idx < ARRAY_SIZE(sum->part); (*idx)++) {
		bit = ffs(sum->part[*idx]) - 1;
		if (bit < 0)
			continue;

		*e = part_event_bits[bit];
		sum->part[*idx] &= ~(1 << bit);
		return 1;
	}

	for (*idx = 0; *idx < ARRAY_SIZE(sum->pff); (*idx)++) {
		bit = ffs(sum->pff[*idx]) - 1;
		if (bit < 0)
			continue;

		*e = pff_event_bits[bit];
		sum->pff[*idx] &= ~(1 << bit);
		return 1;
	}

	return 0;
}

/**
 * @brief Check if one or more events have occurred
 * @param[in]  dev	Switchtec device handle
 * @param[in]  chk	Summary structure with events to check
 * @param[out] res	Returned current events summary, (may be NULL)
 * @return 1 if one of the events in chk occurred, 0 otherwise or a
 * 	negative value if an error occurred.
 */
int switchtec_event_check(struct switchtec_dev *dev,
			  struct switchtec_event_summary *chk,
			  struct switchtec_event_summary *res)
{
	struct switchtec_event_summary res_tmp;
	int i;
	int ret;

	if (!chk)
		return -EINVAL;

	if (!res)
		res = &res_tmp;

	ret = switchtec_event_summary(dev, res);
	if (ret)
		return ret;

	if (chk->global & res->global)
		return 1;

	if (chk->part_bitmap & res->part_bitmap)
		return 1;

	if (chk->local_part & res->local_part)
		return 1;

	for (i = 0; i < SWITCHTEC_MAX_PARTS; i++)
		if (chk->part[i] & res->part[i])
			return 1;

	for (i = 0; i < SWITCHTEC_MAX_PFF_CSR; i++)
		if (chk->pff[i] & res->pff[i])
			return 1;

	return 0;
}

/**
 * @brief Get the name and description strings as well as the type (global,
 *     partition or pff) for a specific event ID.
 * @param[in]  e	Event ID to get the strings for
 * @param[out] name	Name string of the event
 * @param[out] desc	Description string of the event
 * @return The event type
 */
enum switchtec_event_type switchtec_event_info(enum switchtec_event_id e,
					       const char **name,
					       const char **desc)
{
	if (name)
		*name = events[e].short_name;

	if (desc)
		*desc = events[e].desc;

	return events[e].type;
}

/**
 * @brief Block until a specific event occurs
 * @param[in]  dev		Switchtec device handle
 * @param[in]  e		Event ID to wait for
 * @param[in]  index		Event index (partition or port)
 * @param[out] res		Current event summary set, after waiting
 * @param[in]  timeout_ms	Timeout of this many milliseconds
 * @return 1 if the event occurred, 0 on a timeout and a negative number
 *	an error.
 */
int switchtec_event_wait_for(struct switchtec_dev *dev,
			     enum switchtec_event_id e, int index,
			     struct switchtec_event_summary *res,
			     int timeout_ms)
{
	struct timeval tv;
	long long start, now;
	struct switchtec_event_summary wait_for = {0};
	int ret;

	if (dev->ops->event_wait_for)
		return dev->ops->event_wait_for(dev, e, index, res, timeout_ms);

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
		ret = switchtec_event_wait(dev, timeout_ms > 0 ?
					   now - start + timeout_ms : -1);
		if (ret < 0)
			return ret;

		if (ret == 0)
			goto next;

		ret = switchtec_event_check(dev, &wait_for, res);
		if (ret < 0)
			return ret;

		if (ret)
			return 1;

next:
		ret = gettimeofday(&tv, NULL);
		if (ret)
			return ret;

		now = ((tv.tv_sec) * 1000 + tv.tv_usec / 1000);

		if (timeout_ms > 0 && now - start >= timeout_ms) {
			ret = switchtec_event_summary(dev, res);
			return ret;
		}
	}
}

/**@}*/
