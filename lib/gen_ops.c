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
 * @brief Generation-specific operations registration
 */

#include "switchtec_priv.h"
#include "gen_ops.h"

/**
 * @brief Generation ops table indexed by switchtec_gen enum
 *
 * This table allows O(1) lookup of generation-specific operations
 * based on the device's generation.
 */
const struct switchtec_gen_ops *switchtec_gen_ops[] = {
	[SWITCHTEC_GEN_UNKNOWN] = NULL,
	[SWITCHTEC_GEN3] = &switchtec_gen3_ops,
	[SWITCHTEC_GEN4] = &switchtec_gen4_ops,
	[SWITCHTEC_GEN5] = &switchtec_gen5_ops,
	[SWITCHTEC_GEN6] = &switchtec_gen6_ops,
};