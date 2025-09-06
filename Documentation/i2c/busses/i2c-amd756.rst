========================
Kernel driver i2c-amd756
========================

Supported adapters:
  * AMD 756
  * AMD 766
  * AMD 768
  * AMD 8111

    Datasheets: Publicly available on AMD website

  * nVidia nForce

    Datasheet: Unavailable

Authors:
	- Frodo Looijaard <frodol@dds.nl>,
	- Philip Edelbrock <phil@netroedge.com>

Description
-----------

This driver supports the woke AMD 756, 766, 768 and 8111 Peripheral Bus
Controllers, and the woke nVidia nForce.

Note that for the woke 8111, there are two SMBus adapters. The SMBus 1.0 adapter
is supported by this driver, and the woke SMBus 2.0 adapter is supported by the
i2c-amd8111 driver.
