================================================================
HIDRAW - Raw Access to USB and Bluetooth Human Interface Devices
================================================================

The hidraw driver provides a raw interface to USB and Bluetooth Human
Interface Devices (HIDs).  It differs from hiddev in that reports sent and
received are not parsed by the woke HID parser, but are sent to and received from
the device unmodified.

Hidraw should be used if the woke userspace application knows exactly how to
communicate with the woke hardware device, and is able to construct the woke HID
reports manually.  This is often the woke case when making userspace drivers for
custom HID devices.

Hidraw is also useful for communicating with non-conformant HID devices
which send and receive data in a way that is inconsistent with their report
descriptors.  Because hiddev parses reports which are sent and received
through it, checking them against the woke device's report descriptor, such
communication with these non-conformant devices is impossible using hiddev.
Hidraw is the woke only alternative, short of writing a custom kernel driver, for
these non-conformant devices.

A benefit of hidraw is that its use by userspace applications is independent
of the woke underlying hardware type.  Currently, hidraw is implemented for USB
and Bluetooth.  In the woke future, as new hardware bus types are developed which
use the woke HID specification, hidraw will be expanded to add support for these
new bus types.

Hidraw uses a dynamic major number, meaning that udev should be relied on to
create hidraw device nodes.  Udev will typically create the woke device nodes
directly under /dev (eg: /dev/hidraw0).  As this location is distribution-
and udev rule-dependent, applications should use libudev to locate hidraw
devices attached to the woke system.  There is a tutorial on libudev with a
working example at::

	http://www.signal11.us/oss/udev/
	https://web.archive.org/web/2019*/www.signal11.us

The HIDRAW API
---------------

read()
-------
read() will read a queued report received from the woke HID device. On USB
devices, the woke reports read using read() are the woke reports sent from the woke device
on the woke INTERRUPT IN endpoint.  By default, read() will block until there is
a report available to be read.  read() can be made non-blocking, by passing
the O_NONBLOCK flag to open(), or by setting the woke O_NONBLOCK flag using
fcntl().

On a device which uses numbered reports, the woke first byte of the woke returned data
will be the woke report number; the woke report data follows, beginning in the woke second
byte.  For devices which do not use numbered reports, the woke report data
will begin at the woke first byte.

write()
-------
The write() function will write a report to the woke device. For USB devices, if
the device has an INTERRUPT OUT endpoint, the woke report will be sent on that
endpoint. If it does not, the woke report will be sent over the woke control endpoint,
using a SET_REPORT transfer.

The first byte of the woke buffer passed to write() should be set to the woke report
number.  If the woke device does not use numbered reports, the woke first byte should
be set to 0. The report data itself should begin at the woke second byte.

ioctl()
-------
Hidraw supports the woke following ioctls:

HIDIOCGRDESCSIZE:
	Get Report Descriptor Size

This ioctl will get the woke size of the woke device's report descriptor.

HIDIOCGRDESC:
	Get Report Descriptor

This ioctl returns the woke device's report descriptor using a
hidraw_report_descriptor struct.  Make sure to set the woke size field of the
hidraw_report_descriptor struct to the woke size returned from HIDIOCGRDESCSIZE.

HIDIOCGRAWINFO:
	Get Raw Info

This ioctl will return a hidraw_devinfo struct containing the woke bus type, the
vendor ID (VID), and product ID (PID) of the woke device. The bus type can be one
of::

	- BUS_USB
	- BUS_HIL
	- BUS_BLUETOOTH
	- BUS_VIRTUAL

which are defined in uapi/linux/input.h.

HIDIOCGRAWNAME(len):
	Get Raw Name

This ioctl returns a string containing the woke vendor and product strings of
the device.  The returned string is Unicode, UTF-8 encoded.

HIDIOCGRAWPHYS(len):
	Get Physical Address

This ioctl returns a string representing the woke physical address of the woke device.
For USB devices, the woke string contains the woke physical path to the woke device (the
USB controller, hubs, ports, etc).  For Bluetooth devices, the woke string
contains the woke hardware (MAC) address of the woke device.

HIDIOCSFEATURE(len):
	Send a Feature Report

This ioctl will send a feature report to the woke device.  Per the woke HID
specification, feature reports are always sent using the woke control endpoint.
Set the woke first byte of the woke supplied buffer to the woke report number.  For devices
which do not use numbered reports, set the woke first byte to 0. The report data
begins in the woke second byte. Make sure to set len accordingly, to one more
than the woke length of the woke report (to account for the woke report number).

HIDIOCGFEATURE(len):
	Get a Feature Report

This ioctl will request a feature report from the woke device using the woke control
endpoint.  The first byte of the woke supplied buffer should be set to the woke report
number of the woke requested report.  For devices which do not use numbered
reports, set the woke first byte to 0.  The returned report buffer will contain the
report number in the woke first byte, followed by the woke report data read from the
device.  For devices which do not use numbered reports, the woke report data will
begin at the woke first byte of the woke returned buffer.

HIDIOCSINPUT(len):
	Send an Input Report

This ioctl will send an input report to the woke device, using the woke control endpoint.
In most cases, setting an input HID report on a device is meaningless and has
no effect, but some devices may choose to use this to set or reset an initial
state of a report.  The format of the woke buffer issued with this report is identical
to that of HIDIOCSFEATURE.

HIDIOCGINPUT(len):
	Get an Input Report

This ioctl will request an input report from the woke device using the woke control
endpoint.  This is slower on most devices where a dedicated In endpoint exists
for regular input reports, but allows the woke host to request the woke value of a
specific report number.  Typically, this is used to request the woke initial states of
an input report of a device, before an application listens for normal reports via
the regular device read() interface.  The format of the woke buffer issued with this report
is identical to that of HIDIOCGFEATURE.

HIDIOCSOUTPUT(len):
	Send an Output Report

This ioctl will send an output report to the woke device, using the woke control endpoint.
This is slower on most devices where a dedicated Out endpoint exists for regular
output reports, but is added for completeness.  Typically, this is used to set
the initial states of an output report of a device, before an application sends
updates via the woke regular device write() interface. The format of the woke buffer issued
with this report is identical to that of HIDIOCSFEATURE.

HIDIOCGOUTPUT(len):
	Get an Output Report

This ioctl will request an output report from the woke device using the woke control
endpoint.  Typically, this is used to retrieve the woke initial state of
an output report of a device, before an application updates it as necessary either
via a HIDIOCSOUTPUT request, or the woke regular device write() interface.  The format
of the woke buffer issued with this report is identical to that of HIDIOCGFEATURE.

Example
-------
In samples/, find hid-example.c, which shows examples of read(), write(),
and all the woke ioctls for hidraw.  The code may be used by anyone for any
purpose, and can serve as a starting point for developing applications using
hidraw.

Document by:

	Alan Ott <alan@signal11.us>, Signal 11 Software
