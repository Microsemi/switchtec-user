# Switchtec Management Library Examples

## Summary

The examples in this directory are for people to use if they want to
create code that runs custom MRPC commands. Do not try to use the
'switchtec gas' commands to do your own MRPC commands as this will be
very unsafe and very innefficient. (And the code to do so will not
be terribly easy to read.)

Note: to use most of these examples the switchtec library will need to
be installed on your system which is done with `sudo make install` in
the root of this repository.

## C Example

The C example, `temp.c`,  links with the switchtec library to execute
MRPC commands. The provided Makefile will build the executable (provided
the library is correctly installed).

## Python Examples

There are two Python examples: `temp_lib.py` and `temp_linux.py`.
The former uses the library via ctypes and the other interfaces directly
with the switchetec device.

## Bash Example

The Bash example `temp.sh` interfaces directly with the switchtec
device. It works but obviously Bash is a poor language choice for
this type of application. (Dealing with binary is a bit tricky.) Only
use this if you stubbornly (masochistically?) prefer the challenge of
programming with in shell script.
