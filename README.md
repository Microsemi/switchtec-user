# Switchtec Management CLI and Library

## Summary

The code hosted here provides an easy to use CLI and C library for
communicating with Microsemi's Switchtec management interface. It
utilizes the switchtec kernel module which, with luck, will be upstream
for 4.11. Otherwise it can be obtained from the
[switchtec-kernel](https://github.com/sbates130272/switchtec-kernel)
github repository and compiled for other supported kernels.

Currently the following features are supported:

* Show status for each port on all partitions
* Measure bandwidth and latency for each port
* Display and wait on event occurrances
* Setup and show event counters for various types of events
* Dump firmware logs
* Send a hard reset command to the switch
* Update and readback firmware as well as display image version and CRC info
* A simple ncurses GUI that shows salient information for the switch

## Dependencies

This program has an optional build dependencies on the following libraries:

* libncurses5-dev (without it 'switchtec gui' will not work)

## Installation

Installation is simple, with:

~~~
./configure
make
sudo make install
~~~~

## Building on Windows

MSYS2 should be used to build switchtec-user on windows seeing it
provides a reasonably compatible compiler. (Visual C++ still is
missing full support of C99.) To setup an environment:

1. Install MSYS2 by following the instructions at: http://www.msys2.org/
2. In an MSYS2 shell, run the following:

~~~
pacman -S --needed base-devel mingw-w64-i686-toolchain mingw-w64-x86_64-toolchain git
~~~

3. Close the MSYS2 shell, open an MINGW64 shell, checkout the project
and follow the above installation instructions as usual.

## TODO List

* Documention: Library documentation needs to be written as well as
man pages for the commands.
* Optional JSON output for various data based commands

## Demos

### CLI Demo
[![asciicast](https://asciinema.org/a/98042.png)](https://asciinema.org/a/98042)

### Firmware Demo
[![asciicast](https://asciinema.org/a/96442.png)](https://asciinema.org/a/96442)
