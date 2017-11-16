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

/*
 * This code serves as a simple example for sending custom MRPC commands.
 * For the purposes of the example, we retrieve the die temperature
 * and do an echo command.
 *
 * More example MRPC command implementations can be found in the library
 * source code.
 */

#include <switchtec/switchtec.h>
#include <switchtec/mrpc.h>

#include <time.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

static int echo_cmd(struct switchtec_dev *dev)
{
	int ret;

	/*
	 * This is just some example command packet. Your structure
	 * could resemble whatever data you are passing for the custom
	 * MRPC command.
	 */
	struct my_cmd {
		uint32_t sub_cmd_id;
		uint16_t param1;
		uint16_t param2;
		uint64_t time_val;
	} __attribute__((packed)) incmd = {
		.sub_cmd_id = 0xAA55,
		.param1 = 0x1234,
		.param2 = 0x5678,
		.time_val = time(NULL),
	};
	struct my_cmd outdata = {};

	ret = switchtec_cmd(dev, MRPC_ECHO, &incmd, sizeof(incmd),
			    &outdata, sizeof(outdata));
	if (ret) {
		switchtec_perror("echo_cmd");
		return 2;
	}

	if (incmd.sub_cmd_id != ~outdata.sub_cmd_id) {
		fprintf(stderr, "Echo data did not match!\n");
		return 3;
	}

	return 0;
}

static int die_temp(struct switchtec_dev *dev)
{
	uint32_t sub_cmd_id = MRPC_DIETEMP_SET_MEAS;
	uint32_t temp;
	int ret;

	ret = switchtec_cmd(dev, MRPC_DIETEMP, &sub_cmd_id,
			    sizeof(sub_cmd_id), NULL, 0);
	if (ret) {
		switchtec_perror("dietemp_set_meas");
		return 4;
	}

	sub_cmd_id = MRPC_DIETEMP_GET;
	ret = switchtec_cmd(dev, MRPC_DIETEMP, &sub_cmd_id,
			    sizeof(sub_cmd_id), &temp, sizeof(temp));
	if (ret) {
		switchtec_perror("dietemp_get");
		return 5;
	}

	printf("Die Temp: %.1f°C\n", temp / 100.);
	return 0;
}

int main(int argc, char *argv[])
{
	struct switchtec_dev *dev;
	int ret = 0;
	const char *devpath;

	if (argc > 2) {
		fprintf(stderr, "USAGE: %s <device>\n", argv[0]);
		return 1;
	} else if (argc == 2) {
		devpath = argv[1];
	} else {
		devpath = "/dev/switchtec0";
	}

	dev = switchtec_open(devpath);
	if (!dev) {
		switchtec_perror(devpath);
		return 1;
	}

	ret = echo_cmd(dev);
	if (ret)
		goto out;

	ret = die_temp(dev);

out:
	switchtec_close(dev);
	return ret;
}
