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

#ifndef LIBSWITCHTEC_BIND_H
#define LIBSWITCHTEC_BIND_H

#include <stdint.h>
#include <switchtec/switchtec.h>

#pragma pack(push, 1)

struct switchtec_bind_status_in {
	uint8_t sub_cmd;
	uint8_t phys_port_id;
	uint8_t reserved1;
	uint8_t reserved2;
};

struct switchtec_bind_status_out {
	uint8_t inf_cnt;
	uint8_t reserved1;
	uint8_t reserved2;
	uint8_t reserved3;
	uint8_t phys_port_id;
	uint8_t par_id;
	uint8_t log_port_id;
	uint8_t bind_state;
};

struct switchtec_bind_in {
	uint8_t sub_cmd;
	uint8_t par_id;
	uint8_t log_port_id;
	uint8_t phys_port_id;
};

struct switchtec_unbind_in {
	uint8_t sub_cmd;
	uint8_t par_id;
	uint8_t log_port_id;
	uint8_t opt;
};


#pragma pack(pop)

#endif
