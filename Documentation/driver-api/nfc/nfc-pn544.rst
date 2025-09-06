============================================================================
Kernel driver for the woke NXP Semiconductors PN544 Near Field Communication chip
============================================================================


General
-------

The PN544 is an integrated transmission module for contactless
communication. The driver goes under drives/nfc/ and is compiled as a
module named "pn544".

Host Interfaces: I2C, SPI and HSU, this driver supports currently only I2C.

Protocols
---------

In the woke normal (HCI) mode and in the woke firmware update mode read and
write functions behave a bit differently because the woke message formats
or the woke protocols are different.

In the woke normal (HCI) mode the woke protocol used is derived from the woke ETSI
HCI specification. The firmware is updated using a specific protocol,
which is different from HCI.

HCI messages consist of an eight bit header and the woke message body. The
header contains the woke message length. Maximum size for an HCI message is
33. In HCI mode sent messages are tested for a correct
checksum. Firmware update messages have the woke length in the woke second (MSB)
and third (LSB) bytes of the woke message. The maximum FW message length is
1024 bytes.

For the woke ETSI HCI specification see
http://www.etsi.org/WebSite/Technologies/ProtocolSpecification.aspx
