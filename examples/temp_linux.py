#!/usr/bin/env python3
########################################################################
##
## Microsemi Switchtec(tm) PCIe Management Library
## Copyright (c) 2017, Microsemi Corporation
##
## Permission is hereby granted, free of charge, to any person obtaining a
## copy of this software and associated documentation files (the "Software"),
## to deal in the Software without restriction, including without limitation
## the rights to use, copy, modify, merge, publish, distribute, sublicense,
## and/or sell copies of the Software, and to permit persons to whom the
## Software is furnished to do so, subject to the following conditions:
##
## The above copyright notice and this permission notice shall be included
## in all copies or substantial portions of the Software.
##
## THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
## OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
## FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
## THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
## OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
## ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
## OTHER DEALINGS IN THE SOFTWARE.
##
########################################################################

# This code serves as a simple example for sending custom MRPC commands
# with python by directly operating on the device. This script is exactly
# the same as temp_lib.py but does not require the Switchtec library. Thus it's
# a bit simpler and has fewer dependencies. However, as a consequence, it
# will only work in Linux.
#
# I'd love to take a pull request for a full python library implementation!

import os
import sys
import time
import struct

MRPC_ECHO = 65
MRPC_DIETEMP = 4
MRPC_DIETEMP_SET_MEAS = 1
MRPC_DIETEMP_GET = 2

class SwitchtecError(Exception):
    pass

class Switchtec(object):
    def __init__(self, devpath):
        self.fd = os.open(devpath, os.O_RDWR)

    def cmd(self, cmd, indata=b"", outdata_len=0):
        indata = struct.pack("<L", cmd) + indata
        os.write(self.fd, indata)
        outdata = os.read(self.fd, 4+outdata_len)

        retcode = outdata[:4]
        outdata = outdata[4:]

        retcode, = struct.unpack("<L", retcode)

        if retcode != 0:
            raise SwitchtecError("Error occurred during command: {:x}".
                                 format(retcode))

        return outdata

class CommandError(Exception):
    pass

def echo_cmd(dev):
    my_cmd = struct.Struct("<LHHQ")

    sub_cmd_in = 0xAA55
    indata = my_cmd.pack(sub_cmd_in, 0x1234, 0x5678, int(time.time()))

    outdata = dev.cmd(MRPC_ECHO, indata, len(indata))

    sub_cmd_out, param1, param2, time_val = my_cmd.unpack(outdata)

    if sub_cmd_in != ~sub_cmd_out & 0xFFFFFFFF:
        raise CommandError("Echo data did not match: {:x} != ~{:x}".
                           format(sub_cmd_in, sub_cmd_out))

def die_temp(dev):
    dev.cmd(MRPC_DIETEMP, struct.pack("<L", MRPC_DIETEMP_SET_MEAS))
    temp_packed = dev.cmd(MRPC_DIETEMP, struct.pack("<L", MRPC_DIETEMP_GET), 4)

    temp, = struct.unpack("<L", temp_packed)

    print("Die Temp: {:.1f}°C".format(temp / 100.))

if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("device", nargs="?", default="/dev/switchtec0",
                        help="switchtec device to use")
    options = parser.parse_args()

    try:
        dev = Switchtec(options.device)
        echo_cmd(dev)
        die_temp(dev)

    except KeyboardInterrupt:
        print()
    except SwitchtecError as e:
        print(e)
    except CommandError as e:
        print(e)
    except OSError as e:
        print(e)
