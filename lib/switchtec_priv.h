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

#ifndef LIBSWITCHTEC_SWITCHTEC_PRIV_H
#define LIBSWITCHTEC_SWITCHTEC_PRIV_H

#include "switchtec/switchtec.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

struct switchtec_dev;

struct switchtec_ops {
	void (*close)(struct switchtec_dev *dev);
	int (*get_fw_version)(struct switchtec_dev *dev, char *buf,
			      size_t buflen);
	int (*cmd)(struct switchtec_dev *dev,  uint32_t cmd,
		   const void *payload, size_t payload_len, void *resp,
		   size_t resp_len);
	int (*get_devices)(struct switchtec_dev *dev,
			   struct switchtec_status *status,
			   int ports);
	int (*pff_to_port)(struct switchtec_dev *dev, int pff,
			   int *partition, int *port);
	int (*port_to_pff)(struct switchtec_dev *dev, int partition,
			   int port, int *pff);
	gasptr_t (*gas_map)(struct switchtec_dev *dev, int writeable,
			    size_t *map_size);
	void (*gas_unmap)(struct switchtec_dev *dev, gasptr_t map);
	int (*flash_part)(struct switchtec_dev *dev,
			  struct switchtec_fw_image_info *info,
			  enum switchtec_fw_image_type part);
	int (*event_summary)(struct switchtec_dev *dev,
			     struct switchtec_event_summary *sum);
	int (*event_ctl)(struct switchtec_dev *dev,
			 enum switchtec_event_id e,
			 int index, int flags,
			 uint32_t data[5]);
	int (*event_wait)(struct switchtec_dev *dev, int timeout_ms);
	int (*event_wait_for)(struct switchtec_dev *dev,
			      enum switchtec_event_id e, int index,
			      struct switchtec_event_summary *res,
			      int timeout_ms);

	uint8_t (*gas_read8)(struct switchtec_dev *dev, uint8_t __gas *addr);
	uint16_t (*gas_read16)(struct switchtec_dev *dev, uint16_t __gas *addr);
	uint32_t (*gas_read32)(struct switchtec_dev *dev, uint32_t __gas *addr);
	uint64_t (*gas_read64)(struct switchtec_dev *dev, uint64_t __gas *addr);

	void (*gas_write8)(struct switchtec_dev *dev, uint8_t val,
			   uint8_t __gas *addr);
	void (*gas_write16)(struct switchtec_dev *dev, uint16_t val,
			    uint16_t __gas *addr);
	void (*gas_write32)(struct switchtec_dev *dev, uint32_t val,
			    uint32_t __gas *addr);
	void (*gas_write64)(struct switchtec_dev *dev, uint64_t val,
			    uint64_t __gas *addr);

	void (*memcpy_to_gas)(struct switchtec_dev *dev, void __gas *dest,
			      const void *src, size_t n);
	void (*memcpy_from_gas)(struct switchtec_dev *dev, void *dest,
				const void __gas *src, size_t n);
	ssize_t (*write_from_gas)(struct switchtec_dev *dev, int fd,
				  const void __gas *src, size_t n);
};

struct switchtec_dev {
	int partition, partition_count;
	char name[PATH_MAX];

	gasptr_t gas_map;
	size_t gas_map_size;

	const struct switchtec_ops *ops;
};

static inline void version_to_string(uint32_t version, char *buf, size_t buflen)
{
	int major = version >> 24;
	int minor = (version >> 16) & 0xFF;
	int build = version & 0xFFFF;

	snprintf(buf, buflen, "%x.%02x B%03X", major, minor, build);
}

void platform_perror(const char *str);

#endif
