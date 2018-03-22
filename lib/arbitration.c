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
 * @brief Switchtec core library functions for port arbitration
 */

#define SWITCHTEC_LIB_CORE

#include "switchtec/arbitration.h"
#include "switchtec_priv.h"
#include "switchtec/switchtec.h"
#include "switchtec/endian.h"

#include <stddef.h>
#include <errno.h>

/**
 * @defgroup Port Arbitration
 * @brief Setup and query port arbitration in the switch.
 * Switchtec supports two types of Port Arbitration Modes:
 *   - Fixed Round Robin (FRR) and
 *   - Weighted Round Robin (WRR)
 * In Weighted Round Robin (WRR) mode, the arbitration initial count for all
 * ports needs to be specified.The MRPC Port Arbitration Command can be used to
 * set or get the port arbitration mode and initial count in WRR mode.
 *
 * switchtec_arbitration_get() may be used to get port arbitration mode and
 * the WRR arbitration initial count.
 *
 * switchtec_arbitration_set() may be used to set port arbitration mode and
 * the WRR arbitration initial count for WRR mode. If mode set to FRR,
 * then arbitration initial count fields will be ignored.
 *
 * @{
 */

/**
 * @brief Return a string describing the arbitration mode
 * @param[out] info Information structure to return the type string for
 * @return Type string
 */
const char *switchtec_arbitration_mode(enum switchtec_arbitration_mode mode)
{
	switch (mode) {
	case SWITCHTEC_ARBITRATION_FRR: return "Fixed Round Robin (FRR)";
	case SWITCHTEC_ARBITRATION_WRR: return "Weighted Round Robin (WRR)";

	default: return "UNKNOWN";
	}
}

/**
 * @brief Get all ports arbitration
 * @param[in]  dev	Switchtec device handle
 * @param[in]  port_id	The physical port number (0-47)
 * @param[out] mode	The arbitration mode
 * @param[out] weights	A list of current weight values for each physical port
 * @return Number of ports in arbitration list or a negative value on failure
 */
int switchtec_arbitration_get(struct switchtec_dev *dev, int port_id,
			      enum switchtec_arbitration_mode *mode,
			      int *weights)
{
	int i, ret;
	int nr_ports = 0;
	struct arbitration_out response;
	struct arbitration_in sub_cmd_id = {
		.sub_cmd_id = MRPC_ARB_GET,
		.port_id = port_id
	};

	if (!mode || !weights) {
		errno = EINVAL;
		return -errno;
	}

	ret = switchtec_cmd(dev, MRPC_ARB, &sub_cmd_id, sizeof(sub_cmd_id),
			    &response, sizeof(response));

	if (ret)
		return ret;

	*mode = response.mode;

	for (i = 0; i < SWITCHTEC_MAX_ARBITRATION_WEIGHTS; i++) {
		weights[i] = response.weights[i];
		nr_ports++;
	}

	return nr_ports;
}

/**
 * @brief Set port arbitration weights
 * @param[in]  dev	Switchtec device handle
 * @param[in]  port_id	The physical port number (0-47)
 * @param[in]  in_mode	The arbitration mode to set
 * @param[in]  in_weights	A list of weights for each physical port to set
 * @param[out] out_mode	The arbitration mode
 * @param[out] out_weights	A list of weight values for each physical port
 * @return Number of ports in arbitration list or a negative value on failure
 */
int switchtec_arbitration_set(struct switchtec_dev *dev, int port_id,
			      enum switchtec_arbitration_mode in_mode,
			      int *in_weights,
			      enum switchtec_arbitration_mode *out_mode,
			      int *out_weights)
{
	int i, ret;
	int nr_ports = 0;
	struct arbitration_out response;
	struct arbitration_in sub_cmd_id = {
		.sub_cmd_id = MRPC_ARB_SET,
		.port_id = port_id,
		.mode = in_mode
	};

	for (i = 0; i < SWITCHTEC_MAX_ARBITRATION_WEIGHTS; i++)
		sub_cmd_id.weights[i] = in_weights[i];

	if (!out_mode || !out_weights) {
		errno = EINVAL;
		return -errno;
	}

	ret = switchtec_cmd(dev, MRPC_ARB, &sub_cmd_id, sizeof(sub_cmd_id),
			    &response, sizeof(response));

	if (ret)
		return ret;

	*out_mode = response.mode;

	for (i = 0; i < SWITCHTEC_MAX_ARBITRATION_WEIGHTS; i++) {
		out_weights[i] = response.weights[i];
		nr_ports++;
	}

	return nr_ports;
}

/**@}*/
