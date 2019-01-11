# nullmodem95
DRAFT!!!

## Abstract
*nullmodem95* is a virtual COM port driver for Windows 95.
The idea is to install pairs of virtual COM ports, which are virtually interconnected with a nullmodem cable.
Data written into one port is received on the respective other port.

*nullmodem95* can also be considered as an example how to implement Windows 95 drivers using the C language.

## Further Information
Further information can be found (in german language) in file *doc/readme.txt*.
File *src/wrapper.h* contains some information about the usage of x64 inline assembly and the invocation of
VxD services from C language.

## Build and Installation
The driver can be build using **make.bat**.
To do that, **start.bat** must be executed first. This file sets a couple of environment variable, which must
probably adapted to your build environment. After that **make.bat** compiles the sources to *result/mxvcp.vxd*, that can
be installed using the *result/mxvcp.inf* and the hardware assistent of Windows 95 control panel (see *doc/readme.txt*
for more information). Otherwise, it can be installed using **result/install.bat**.

## References
The idea of this project was inspired by the COM2COM project found on sourceforge:
http://com0com.sourceforge.net/
