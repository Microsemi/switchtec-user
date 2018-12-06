/*
 * Microsemi Switchtec(tm) PCIe Management Library
 * Copyright (c) 2018, Microsemi Corporation
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

#ifndef LIBSWITCHTEC_PCI_H
#define LIBSWITCHTEC_PCI_H

#include <string.h>

#define PCI_EXT_CAP_OFFSET 0x100
#define PCI_EXT_CAP_ID(cap)((cap) & 0x0000ffff)
#define PCI_EXT_CAP_VER(cap)(((cap) >> 16) & 0xf)
#define PCI_EXT_CAP_NEXT(cap)(((cap) >> 20) & 0xffc)

#define PCI_EXT_CAP_ID_ACS      0x0d

#define PCI_ACS_CTRL            0x06    //!< ACS Control Register
#define PCI_ACS_CTRL_VALID      0x0001  //!< ACS Source Validation Enable
#define PCI_ACS_CTRL_BLOCK      0x0002  //!< ACS Translation Blocking Enable
#define PCI_ACS_CTRL_REQ_RED    0x0004  //!< ACS P2P Request Redirect Enable
#define PCI_ACS_CTRL_CMPLT_RED  0x0008  //!< ACS P2P Completion Redirect Enable
#define PCI_ACS_CTRL_FORWARD    0x0010  //!< ACS Upstream Forwarding Enable
#define PCI_ACS_CTRL_EGRESS     0x0020  //!< ACS P2P Egress Control Enable
#define PCI_ACS_CTRL_TRANS      0x0040  //!< ACS Direct Translated P2P Enable
#define PCI_ACS_EGRESS_CTRL     0x08    //!< Egress Control Vector

#endif
