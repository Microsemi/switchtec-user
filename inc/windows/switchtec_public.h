/*
* Microsemi Switchtec(tm) PCIe Windows Management Driver
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

#ifndef SWITCHTEC_PUBLIC
#define SWITCHTEC_PUBLIC

#if defined(__MINGW32__) || defined(__MINGW64__)
#include <stdint.h>
#include <initguid.h>
#include <ddk/wdmguid.h>
#include <devpropdef.h>
#else
#include "pstdint.h"
#include <initguid.h>
#include <wdmguid.h>
#include <devpropdef.h>

#pragma warning(disable: 4200)

#endif

DEFINE_GUID(SWITCHTEC_INTERFACE_GUID,
	    0xC94C2F2B, 0xF574, 0x4CFE, 0xA5, 0x5B, 0x5C, 0xEF, 0xEE, 0x68, 0xB4, 0x62);

DEFINE_DEVPROPKEY(SWITCHTEC_PROP_DEVICE_VERSION,
		  0xC26DF34B, 0x0A46, 0x4942, 0x8E, 0x6B, 0xF8, 0x92, 0x4E, 0xB7, 0x32, 0x84, 2);
DEFINE_DEVPROPKEY(SWITCHTEC_PROP_FW_VERSION,
		  0xC26DF34B, 0x0A46, 0x4942, 0x8E, 0x6B, 0xF8, 0x92, 0x4E, 0xB7, 0x32, 0x84, 3);
DEFINE_DEVPROPKEY(SWITCHTEC_PROP_VENDOR_ID,
		  0xC26DF34B, 0x0A46, 0x4942, 0x8E, 0x6B, 0xF8, 0x92, 0x4E, 0xB7, 0x32, 0x84, 4);
DEFINE_DEVPROPKEY(SWITCHTEC_PROP_PRODUCT_ID,
		  0xC26DF34B, 0x0A46, 0x4942, 0x8E, 0x6B, 0xF8, 0x92, 0x4E, 0xB7, 0x32, 0x84, 5);
DEFINE_DEVPROPKEY(SWITCHTEC_PROP_PRODUCT_REV,
		  0xC26DF34B, 0x0A46, 0x4942, 0x8E, 0x6B, 0xF8, 0x92, 0x4E, 0xB7, 0x32, 0x84, 6);
DEFINE_DEVPROPKEY(SWITCHTEC_PROP_COMPONENT_VENDOR,
		  0xC26DF34B, 0x0A46, 0x4942, 0x8E, 0x6B, 0xF8, 0x92, 0x4E, 0xB7, 0x32, 0x84, 7);
DEFINE_DEVPROPKEY(SWITCHTEC_PROP_COMPONENT_ID,
		  0xC26DF34B, 0x0A46, 0x4942, 0x8E, 0x6B, 0xF8, 0x92, 0x4E, 0xB7, 0x32, 0x84, 8);
DEFINE_DEVPROPKEY(SWITCHTEC_PROP_COMPONENT_REV,
		  0xC26DF34B, 0x0A46, 0x4942, 0x8E, 0x6B, 0xF8, 0x92, 0x4E, 0xB7, 0x32, 0x84, 9);
DEFINE_DEVPROPKEY(SWITCHTEC_PROP_PARTITION,
		  0xC26DF34B, 0x0A46, 0x4942, 0x8E, 0x6B, 0xF8, 0x92, 0x4E, 0xB7, 0x32, 0x84, 10);
DEFINE_DEVPROPKEY(SWITCHTEC_PROP_PARTITION_COUNT,
		  0xC26DF34B, 0x0A46, 0x4942, 0x8E, 0x6B, 0xF8, 0x92, 0x4E, 0xB7, 0x32, 0x84, 11);

struct switchtec_gas_map {
	union {
		struct switchtec_gas *gas;
		uint64_t gas_padded;
	};
	uint64_t length;
};

struct switchtec_mrpc_cmd {
	uint32_t cmd;
	uint8_t data[];
};

struct switchtec_mrpc_result {
	uint32_t status;
	uint8_t data[];
};

#define IOCTL_SWITCHTEC_GAS_MAP \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x0, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SWITCHTEC_GAS_UNMAP \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x1, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SWITCHTEC_MRPC \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x2, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SWITCHTEC_WAIT_FOR_EVENT \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x3, METHOD_BUFFERED, FILE_ANY_ACCESS)

#endif