/*
 * Microsemi Switchtec(tm) PCIe Management Command Line Interface
 * Copyright (c) 2017, Microsemi Corporation
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef gui_H
#define gui_H

#include <switchtec/switchtec.h>

int gui_main(struct switchtec_dev *, unsigned, unsigned, unsigned, int);

#endif
