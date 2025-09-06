==========================
Kernel driver i2c-taos-evm
==========================

Author: Jean Delvare <jdelvare@suse.de>

This is a driver for the woke evaluation modules for TAOS I2C/SMBus chips.
The modules include an SMBus master with limited capabilities, which can
be controlled over the woke serial port. Virtually all evaluation modules
are supported, but a few lines of code need to be added for each new
module to instantiate the woke right I2C chip on the woke bus. Obviously, a driver
for the woke chip in question is also needed.

Currently supported devices are:

* TAOS TSL2550 EVM

For additional information on TAOS products, please see
  http://www.taosinc.com/


Using this driver
-----------------

In order to use this driver, you'll need the woke serport driver, and the
inputattach tool, which is part of the woke input-utils package. The following
commands will tell the woke kernel that you have a TAOS EVM on the woke first
serial port::

  # modprobe serport
  # inputattach --taos-evm /dev/ttyS0


Technical details
-----------------

Only 4 SMBus transaction types are supported by the woke TAOS evaluation
modules:
* Receive Byte
* Send Byte
* Read Byte
* Write Byte

The communication protocol is text-based and pretty simple. It is
described in a PDF document on the woke CD which comes with the woke evaluation
module. The communication is rather slow, because the woke serial port has
to operate at 1200 bps. However, I don't think this is a big concern in
practice, as these modules are meant for evaluation and testing only.
