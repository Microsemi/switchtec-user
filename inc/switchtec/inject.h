/*
 * Microsemi Switchtec(tm) PCIe Management Library
 * Copyright (c) 2025, Microsemi Corporation
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

#include <switchtec/switchtec.h>
#include <stdint.h>

struct switchtec_lnkerr_dllp_in {
	uint8_t subcmd;
	uint8_t phys_port_id;
	uint8_t resvd[2];
	uint32_t data;
};

struct switchtec_lnkerr_dllp_crc_in {
	uint8_t subcmd;
	uint8_t phys_port_id;
	uint8_t enable;
	uint8_t resvd1;
	uint16_t rate;
	uint8_t resvd2[2];
};

struct switchtec_lnkerr_tlp_lcrc_gen5_in {
	uint8_t subcmd;
	uint8_t phys_port_id;
	uint8_t enable;
	uint8_t resvd1;
	uint8_t rate;
	uint8_t resvd[3];
};

struct switchtec_lnkerr_tlp_lcrc_gen4_in {
	uint8_t subcmd;
	uint8_t phys_port_id;
	uint8_t enable;
	uint8_t rate;
};

struct switchtec_lnkerr_tlp_seqn_in {
	uint8_t subcmd;
	uint8_t phys_port_id;
	uint8_t resvd[2];
};

struct switchtec_lnkerr_ack_nack_in {
	uint8_t subcmd;
	uint8_t phys_port_id;
	uint8_t resvd1[2];
	uint16_t seq_num;
	uint8_t count;
	uint8_t resvd2;
};

struct switchtec_lnkerr_cto_in {
	uint8_t subcmd;
	uint8_t phys_port_id;
	uint8_t resvd[2];
};
