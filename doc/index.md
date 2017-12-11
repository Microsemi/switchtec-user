Introduction {#mainpage}
=================================

This is the documentation for the Switchtec Userspace project. The userspace
project consists of a command line interface (CLI) program which provides a
simple way to perform the most common maintenance operations. The CLI makes
use of the userspace library (libswitchtec) which is also included in
this project and provides an API interface for all the commands the CLI can do
as well as interfaces for writing custom or other unimplemented commands.
The library communicates with the kernel space drivers ([switchtec-kernel] for
Linux and switchtec-kmdf for windows) which are documented in their respective
packages. A block diagram showing the relationships between the components is
given below.

![Block Diagram][blockdiagram]

It's important to note that, on Linux, the driver does not provide direct access
to the GAS registers and this accesses is obtained when necessary through the
PCI resource file in SYSFS. Because of this, GAS accesss requires full root
privileges where as all normal operations only requires permissions access
to the device file. This is a good thing seeing direct GAS access has many
security implications and can be dangerous for applications to directly
access.

The Windows kernel driver, on the other hand, was designed as a thin driver
with all GAS accesses being done in the userspace library. This was mostly
done to ease future development on I2C/TWI and Ethernet interfaces which
will always have direct access to the GAS.


[blockdiagram]: switchtec.svg
[switchtec-kernel]: https://github.com/Microsemi/switchtec-kernel


Switchtec Library
===================

To get started using the library you may refer to the examples provided
in the examples folder of the switchtec-user repository. The APIs are also
documented herein. Basic functions to open a handle to a device and perform
custom MRPC commands are documented in the [Device API]. Additional APIs
for other functionality can be browsed in the [Modules] section.


[device api]: @ref Device
[modules]: modules.html
