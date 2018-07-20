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
 */

/**
 * @defgroup GAS Global Address Space
 * @brief Functions to access the Global Address Space (GAS)
 *
 * switchtec_gas_map() and switchtec_gas_unmap() map and unmap the GAS
 * into the processes address space. Once mapped, the various gas_read and
 * gas_write functions may be used to access the addresspace.
 *
 * Although on Linux and Windows, switchtec_gas_map() returns an addressable
 * pointer, the data should not be accessed directly. Instead the accessor
 * functions should be used. This will allow for support on systems that
 * do not have direct access to the address space (ie. I2C or Ethernet).
 *
 * Note: these functions should _only_ be used in client code between
 * calls of switchtec_gas_map() and switchtec_gas_unmap(). This implies
 * the code will require full root priviliges in Linux. The functions may
 * also be used in platform specific code on platforms that have full
 * access to the GAS.
 * @{
 */

#include <switchtec/switchtec.h>

#ifdef SWITCHTEC_LIB_CORE
#error "You should not be using GAS access functions in the core library."
#endif

#ifdef SWITCHTEC_LIB_LINUX
#error "GAS Access functions should not be used on the Linux platform " \
	"as they require full root access."
#endif

#include <stdint.h>

void memcpy_to_gas(struct switchtec_dev *dev, void __gas *dest,
		   const void *src, size_t n);

void memcpy_from_gas(struct switchtec_dev *dev, void *dest,
		     const void __gas *src, size_t n);

ssize_t write_from_gas(struct switchtec_dev *dev, int fd,
		       const void __gas *src, size_t n);

uint8_t gas_read8(struct switchtec_dev *dev, uint8_t __gas *addr);
uint16_t gas_read16(struct switchtec_dev *dev, uint16_t __gas *addr);
uint32_t gas_read32(struct switchtec_dev *dev, uint32_t __gas *addr);
uint64_t gas_read64(struct switchtec_dev *dev, uint64_t __gas *addr);

void gas_write8(struct switchtec_dev *dev, uint8_t val, uint8_t __gas *addr);
void gas_write16(struct switchtec_dev *dev, uint16_t val,
		 uint16_t __gas *addr);
void gas_write32(struct switchtec_dev *dev, uint32_t val,
		 uint32_t __gas *addr);
void gas_write64(struct switchtec_dev *dev, uint64_t val,
		 uint64_t __gas *addr);

/**@}*/

#endif
