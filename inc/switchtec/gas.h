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

/**
 * @file
 * @brief GAS Accessor functions
 *
 * Note: these functions should _only_ be used in client code between
 * calls of switchtec_gas_map() and switchtec_gas_unmap(). This implies
 * the code will require full root priviliges in Linux. The functions may
 * also be used in platform specific code on platforms that have full
 * access to the GAS.
 */

#include <switchtec/switchtec.h>

#ifdef __CHECKER__
#define __force __attribute__((force))
#else
#define __force
#endif

#include <unistd.h>
#include <stdint.h>
#include <string.h>

/**
 * @brief Copy data to the GAS
 * @param[out] dest	Destination gas address
 * @param[in]  src	Source data buffer
 * @param[in]  n	Number of bytes to transfer
 */
static inline void memcpy_to_gas(void __gas *dest, const void *src, size_t n)
{
	memcpy((void __force *)dest, src, n);
}

/**
 * @brief Copy data from the GAS
 * @param[out] dest	Destination buffer
 * @param[in]  src	Source gas address
 * @param[in]  n	Number of bytes to transfer
 */
static inline void memcpy_from_gas(void *dest, const void __gas *src, size_t n)
{
	memcpy(dest, (void __force *)src, n);
}

/**
 * @brief Call write() with data from the GAS
 * @param[in] fd	Destination buffer
 * @param[in] src	Source gas address
 * @param[in] n		Number of bytes to transfer
 */
static inline ssize_t write_from_gas(int fd, const void __gas *src, size_t n)
{
	return write(fd, (void __force *)src, n);
}

/**
 * @brief Read a uint8_t from the GAS
 * @param[in] addr Address to read the value
 * @return The read value
 */
static inline uint8_t gas_read8(uint8_t __gas *addr);

/**
 * @brief Read a uint8_t from the GAS
 * @param[in] addr Address to read the value
 * @return The read value
 */
static inline uint16_t gas_read16(uint16_t __gas *addr);

/**
 * @brief Read a uint8_t from the GAS
 * @param[in] addr Address to read the value
 * @return The read value
 */
static inline uint32_t gas_read32(uint32_t __gas *addr);

/**
 * @brief Read a uint8_t from the GAS
 * @param[in] addr Address to read the value
 * @return The read value
 */
static inline uint64_t gas_read64(uint64_t __gas *addr);

/**
 * @brief Write a uint8_t to the GAS
 * @param[in]  val  Value to write
 * @param[out] addr Address to write the value
 */
static inline void gas_write8(uint8_t val, uint8_t __gas *addr);

/**
 * @brief Write a uint16_t to the GAS
 * @param[in]  val  Value to write
 * @param[out] addr Address to write the value
 */
static inline void gas_write16(uint16_t val, uint16_t __gas *addr);

/**
 * @brief Write a uint32_t to the GAS
 * @param[in]  val  Value to write
 * @param[out] addr Address to write the value
 */
static inline void gas_write32(uint32_t val, uint32_t __gas *addr);

/**
 * @brief Write a uint64_t to the GAS
 * @param[in]  val  Value to write
 * @param[out] addr Address to write the value
 */
static inline void gas_write64(uint64_t val, uint64_t __gas *addr);


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

/*
 * These helpers only work in platform code that have access to the
 * switchtec_dev private structure. They should probably move out of here
 * at some point.
 */
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
