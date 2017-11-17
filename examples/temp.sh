#!/bin/bash
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
# with Bash by directly interfacing with the device. This will only work
# on Linux. Obviously Bash is a terrible language for this type of
# application so I do not recommend using this unless you absolutely must
# program in bash.
#
# This script requires the head, tail, od and base64 utilities which
# are available in the coreutils package

set -e

declare -r MRPC_ECHO="\x41\x00\x00\x00"
declare -r MRPC_DIETEMP="\x04\x00\x00\x00"
declare -r MRPC_DIETEMP_SET_MEAS="\x01\x00\x00\x00"
declare -r MRPC_DIETEMP_GET="\x02\x00\x00\x00"

function read_int_size()
{
	local bytes=$1
	local skip

	let skip=bytes+1

	VAL=$(echo "$DATA" | base64 -d | head -c $bytes | od -An -tu$bytes |
		tr -d '[:space:]')
	DATA=$(echo "$DATA" | base64 -d | tail -c +$skip | base64)

}

function read_uint32()
{
	read_int_size 4
}

function read_uint16()
{
	read_int_size 2
}

function switchtec_cmd()
{
	local incmd=$1
	local indata=$2
	local outlen=4

	if [ "$#" -eq 3 ]; then
		let outlen=outlen+$3
	fi

	echo -en "$incmd$indata" >&3
	DATA=$(head -c $outlen <&3 | base64)

	read_uint32
	if [[ $VAL -ne  0 ]]; then
		echo "Error in switchtec command: $VAL" >&2
		return 1
	fi

	return 0
}

function echo_cmd()
{
	local indata="\x55\xAA\x00\x00\x12\x34\x78\x56"

	switchtec_cmd ${MRPC_ECHO} $indata 8

	read_uint32
	if [[ 0xAA55 -ne $((~$VAL & 0xFFFFFFFF)) ]]; then
		printf "Echo data did not match: 0xAA55 0x%x\n" $VAL >&2
		return 1
	fi

	read_uint16 #VAL=param1
	read_uint16 #VAL=param2
}

function die_temp()
{
	local temp_deg
	local temp_dec

	switchtec_cmd ${MRPC_DIETEMP} ${MRPC_DIETEMP_SET_MEAS}
	switchtec_cmd ${MRPC_DIETEMP} ${MRPC_DIETEMP_GET} 4

	read_uint32
	let temp_deg="$VAL / 100"
	let temp_dec="($VAL % 100) / 10"

	echo "$temp_deg.$temp_dec°C"
}

DEVPATH=${DEVPATH:-/dev/switchtec0}
if [ "$#" -gt 1 ]; then
	echo "USAGE: $0 <device>" >&2
elif [ "$#" -eq 1 ]; then
	DEVPATH=$1
fi

if [ ! -c $DEVPATH ]; then
	echo "$DEVPATH: Invalid device" >&2
	exit 1;
fi

exec 3<> $DEVPATH

echo_cmd
die_temp

exec 3>&-
