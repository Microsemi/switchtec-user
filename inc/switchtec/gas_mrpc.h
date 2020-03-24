/*
 * Microsemi Switchtec(tm) PCIe Management Library
 * Copyright (c) 2019, Microsemi Corporation
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

#ifndef LIBSWITCHTEC_GAS_MRPC_H
#define LIBSWITCHTEC_GAS_MRPC_H

#include <switchtec/mrpc.h>
#include <switchtec/switchtec.h>
#include <stdint.h>

struct gas_mrpc_write {
	uint32_t gas_offset;
	uint32_t len;
	uint8_t data[MRPC_MAX_DATA_LEN - 2 * sizeof(uint32_t)];
};

struct gas_mrpc_read {
	uint32_t gas_offset;
	uint32_t len;
};

void gas_mrpc_memcpy_to_gas(struct switchtec_dev *dev, void __gas *dest,
			    const void *src, size_t n);
void gas_mrpc_memcpy_from_gas(struct switchtec_dev *dev, void *dest,
			      const void __gas *src, size_t n);
ssize_t gas_mrpc_write_from_gas(struct switchtec_dev *dev, int fd,
				const void __gas *src, size_t n);

// noop conversion functions to make macros below work
static inline uint8_t le8toh(uint8_t x) { return x; }
static inline uint8_t htole8(uint8_t x) { return x; }

#define create_mrpc_gas_read(type, suffix) \
	static inline type gas_mrpc_read ## suffix(struct switchtec_dev *dev, \
						   type __gas *addr) \
	{ \
		type ret; \
		gas_mrpc_memcpy_from_gas(dev, &ret, addr, sizeof(ret)); \
		return le##suffix##toh(ret);                            \
	}

#define create_mrpc_gas_write(type, suffix) \
	static inline void gas_mrpc_write ## suffix(struct switchtec_dev *dev, \
			type val, type __gas *addr) \
	{ \
		val = htole##suffix (val); \
		gas_mrpc_memcpy_to_gas(dev, addr, &val, sizeof(val)); \
	}

create_mrpc_gas_read(uint8_t, 8);
create_mrpc_gas_read(uint16_t, 16);
create_mrpc_gas_read(uint32_t, 32);
create_mrpc_gas_read(uint64_t, 64);

create_mrpc_gas_write(uint8_t, 8);
create_mrpc_gas_write(uint16_t, 16);
create_mrpc_gas_write(uint32_t, 32);
create_mrpc_gas_write(uint64_t, 64);

#undef create_mrpc_gas_read
#undef create_mrpc_gas_write

#endif
