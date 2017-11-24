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

#ifndef LIBSWITCHTEC_ENDIAN_H
#define LIBSWITCHTEC_ENDIAN_H

#include "portable.h"

#if defined(__linux__)
# include <endian.h>

#elif defined(__WINDOWS__)
# include <winsock2.h>
# include <sys/param.h>

# if BYTE_ORDER == LITTLE_ENDIAN

#  define htobe16(x) htons(x)
#  define htole16(x) (x)
#  define be16toh(x) ntohs(x)
#  define le16toh(x) (x)

#  define htobe32(x) htonl(x)
#  define htole32(x) (x)
#  define be32toh(x) ntohl(x)
#  define le32toh(x) (x)

#  define htobe64(x) htonll(x)
#  define htole64(x) (x)
#  define be64toh(x) ntohll(x)
#  define le64toh(x) (x)

# elif BYTE_ORDER == BIG_ENDIAN

#  define htobe16(x) (x)
#  define htole16(x) __builtin_bswap16(x)
#  define be16toh(x) (x)
#  define le16toh(x) __builtin_bswap16(x)

#  define htobe32(x) (x)
#  define htole32(x) __builtin_bswap32(x)
#  define be32toh(x) (x)
#  define le32toh(x) __builtin_bswap32(x)

#  define htobe64(x) (x)
#  define htole64(x) __builtin_bswap64(x)
#  define be64toh(x) (x)
#  define le64toh(x) __builtin_bswap64(x)

# endif
#endif

#endif
