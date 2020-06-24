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

#ifndef LIBSWITCHTEC_PORTABLE_H
#define LIBSWITCHTEC_PORTABLE_H

#if (defined(_WIN16) || defined(_WIN32) || defined(_WIN64)) \
	&& !defined(__WINDOWS__)
# define __WINDOWS__
#endif

#if defined(__linux__)
# include <linux/limits.h>
#elif defined(__WINDOWS__)
# include <winsock2.h>
# include <windows.h>
# ifndef PATH_MAX
#  define PATH_MAX MAX_PATH
# endif
#endif

#ifdef __WINDOWS__
#ifndef MAP_FAILED
#define MAP_FAILED NULL
#endif
#endif

#ifdef __WINDOWS__
#ifndef SIGBUS
#define SIGBUS SIGABRT
#endif
#endif

#ifdef __MINGW32__
#define ffs __builtin_ffs
#endif

#endif
