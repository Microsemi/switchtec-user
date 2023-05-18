/*
 * Microsemi Switchtec(tm) PCIe Management Command Line Interface
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

#ifndef COMMON_H
#define COMMON_H

int ask_if_sure(int always_yes);
int switchtec_handler(const char *optarg, void *value_addr,
		      const struct argconfig_options *opt);
int mfg_handler(const char *optarg, void *value_addr,
		const struct argconfig_options *opt);
int pax_handler(const char *optarg, void *value_addr,
		const struct argconfig_options *opt);
enum switchtec_fw_type check_and_print_fw_image(int img_fd,
						const char *img_filename);

#define BOOT_PHASE_HELP_TEXT \
	"NOTE - A device can be in one of these three boot phases: \n" \
	"BOOTLOADER1 (BL1): in this phase, a device runs " \
	"a BL1 image that resides on the device's on-chip boot ROM. " \
	"The BL1 image is implemented to facilitate device recovery -- it " \
	"supports transferring and executing a BOOTLOADER2 image. " \
	"To enter the BL1 boot phase, set the device's BOOT_RECOVERY " \
	"PIN 0 to LOW and reset the device.\n\n" \
	"BOOTLOADER2 (BL2): in this phase, a device runs " \
	"the BL2 image stored in flash or transferred during the BL1 boot phase. " \
	"BL2 is the phase for device recovery -- it provides commands " \
	"to update and activate device partitions. " \
	"To enter the BL2 boot phase, set the device's BOOT_RECOVERY PIN[0] to HIGH " \
	"and PIN[1] to LOW and reset the device.\n\n" \
	"MAIN FIRMWARE (MAIN): this is the full-featured firmware that runs " \
	"on your device during normal operation.\n\n"

#define UART_HELP_TEXT " * a UART path (/dev/ttyUSB0)\n"

#define PCI_HELP_TEXT " * a device path (/dev/switchtec0)\n" \
		       " * an index (0, 1, 2)\n" \
		       " * a PCI address (3:00.1)\n" \

#define DEVICE_OPTION_I2C(extra_text1, extra_text2, handler, type) \
	{ \
			"device", .cfg_type=CFG_CUSTOM, .value_addr=&cfg.dev, \
			.argument_type=(type), \
			.custom_handler=handler, \
			.complete="/dev/switchtec*", \
			.env="SWITCHTEC_DEV", \
			.help="Switchtec device to operate on. Can be any of:\n" \
			extra_text1 \
			" * an I2C path with slave address (/dev/i2c-1@0x20)\n" \
			extra_text2 \
	}

#define DEVICE_OPTION_BASIC DEVICE_OPTION_I2C(PCI_HELP_TEXT, "", mfg_handler, \
					      required_positional)

#define DEVICE_OPTION_MFG DEVICE_OPTION_I2C("", "", mfg_handler, \
					    required_positional)

#define DEVICE_OPTION_MFG_PCI DEVICE_OPTION_I2C(PCI_HELP_TEXT, "", mfg_handler, \
						required_positional)

#define __DEVICE_OPTION(type) \
	DEVICE_OPTION_I2C(PCI_HELP_TEXT, UART_HELP_TEXT, switchtec_handler, \
			  (type)), \
	{ \
			"pax", 'x', .cfg_type=CFG_CUSTOM, \
			.value_addr=&cfg.dev, \
			.argument_type=required_argument, \
			.custom_handler=pax_handler, \
			.env="SWITCHTEC_PAX", \
			.help="PAX ID within a PAX fabric. Only valid on " \
			"Switchtec PAX devices" \
	}

#define DEVICE_OPTION		__DEVICE_OPTION(required_positional)
#define DEVICE_OPTION_OPTIONAL	__DEVICE_OPTION(optional_positional)

#endif
