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

#ifndef LIBSWITCHTEC_GAS_H
#define LIBSWITCHTEC_GAS_H

#include <switchtec/switchtec.h>

#ifdef __CHECKER__
#define __force __attribute__((force))
#else
#define __force
#endif

#include <unistd.h>
#include <stdint.h>

static inline void memcpy_to_gas(void __gas *dest, const void *src, size_t n)
{
	memcpy((void __force *)dest, src, n);
}

static inline void memcpy_from_gas(void *dest, const void __gas *src, size_t n)
{
	memcpy(dest, (void __force *)src, n);
}

static inline ssize_t write_from_gas(int fd, const void __gas *buf, size_t n)
{
	return write(fd, (void __force *)buf, n);
}

#define create_gas_read(type, suffix) \
	static inline type gas_read ## suffix(type __gas *addr) \
	{ \
		type *safe_addr = (type __force *)addr; \
		asm volatile("": : :"memory"); \
		return *safe_addr; \
	}

#define create_gas_write(type, suffix) \
	static inline void gas_write ## suffix(type val, type __gas *addr) \
	{ \
		type *safe_addr = (type __force *)addr; \
		asm volatile("": : :"memory"); \
		*safe_addr = val; \
	}

create_gas_read(uint8_t, 8);
create_gas_read(uint16_t, 16);
create_gas_read(uint32_t, 32);
create_gas_read(uint64_t, 64);

create_gas_write(uint8_t, 8);
create_gas_write(uint16_t, 16);
create_gas_write(uint32_t, 32);
create_gas_write(uint64_t, 64);

#define gas_reg_read8(dev, reg)  gas_read8(&dev->gas_map->reg)
#define gas_reg_read16(dev, reg) gas_read16(&dev->gas_map->reg)
#define gas_reg_read32(dev, reg) gas_read32(&dev->gas_map->reg)
#define gas_reg_read64(dev, reg) gas_read64(&dev->gas_map->reg)

#define gas_reg_write8(dev, val, reg)  gas_write8(val, &dev->gas_map->reg)
#define gas_reg_write16(dev, val, reg) gas_write16(val, &dev->gas_map->reg)
#define gas_reg_write32(dev, val, reg) gas_write32(val, &dev->gas_map->reg)
#define gas_reg_write64(dev, val, reg) gas_write64(val, &dev->gas_map->reg)

#undef __force

#endif
