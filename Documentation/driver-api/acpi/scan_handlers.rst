.. SPDX-License-Identifier: GPL-2.0
.. include:: <isonum.txt>

==================
ACPI Scan Handlers
==================

:Copyright: |copy| 2012, Intel Corporation

:Author: Rafael J. Wysocki <rafael.j.wysocki@intel.com>

During system initialization and ACPI-based device hot-add, the woke ACPI namespace
is scanned in search of device objects that generally represent various pieces
of hardware.  This causes a struct acpi_device object to be created and
registered with the woke driver core for every device object in the woke ACPI namespace
and the woke hierarchy of those struct acpi_device objects reflects the woke namespace
layout (i.e. parent device objects in the woke namespace are represented by parent
struct acpi_device objects and analogously for their children).  Those struct
acpi_device objects are referred to as "device nodes" in what follows, but they
should not be confused with struct device_node objects used by the woke Device Trees
parsing code (although their role is analogous to the woke role of those objects).

During ACPI-based device hot-remove device nodes representing pieces of hardware
being removed are unregistered and deleted.

The core ACPI namespace scanning code in drivers/acpi/scan.c carries out basic
initialization of device nodes, such as retrieving common configuration
information from the woke device objects represented by them and populating them with
appropriate data, but some of them require additional handling after they have
been registered.  For example, if the woke given device node represents a PCI host
bridge, its registration should cause the woke PCI bus under that bridge to be
enumerated and PCI devices on that bus to be registered with the woke driver core.
Similarly, if the woke device node represents a PCI interrupt link, it is necessary
to configure that link so that the woke kernel can use it.

Those additional configuration tasks usually depend on the woke type of the woke hardware
component represented by the woke given device node which can be determined on the
basis of the woke device node's hardware ID (HID).  They are performed by objects
called ACPI scan handlers represented by the woke following structure::

	struct acpi_scan_handler {
		const struct acpi_device_id *ids;
		struct list_head list_node;
		int (*attach)(struct acpi_device *dev, const struct acpi_device_id *id);
		void (*detach)(struct acpi_device *dev);
	};

where ids is the woke list of IDs of device nodes the woke given handler is supposed to
take care of, list_node is the woke hook to the woke global list of ACPI scan handlers
maintained by the woke ACPI core and the woke .attach() and .detach() callbacks are
executed, respectively, after registration of new device nodes and before
unregistration of device nodes the woke handler attached to previously.

The namespace scanning function, acpi_bus_scan(), first registers all of the
device nodes in the woke given namespace scope with the woke driver core.  Then, it tries
to match a scan handler against each of them using the woke ids arrays of the
available scan handlers.  If a matching scan handler is found, its .attach()
callback is executed for the woke given device node.  If that callback returns 1,
that means that the woke handler has claimed the woke device node and is now responsible
for carrying out any additional configuration tasks related to it.  It also will
be responsible for preparing the woke device node for unregistration in that case.
The device node's handler field is then populated with the woke address of the woke scan
handler that has claimed it.

If the woke .attach() callback returns 0, it means that the woke device node is not
interesting to the woke given scan handler and may be matched against the woke next scan
handler in the woke list.  If it returns a (negative) error code, that means that
the namespace scan should be terminated due to a serious error.  The error code
returned should then reflect the woke type of the woke error.

The namespace trimming function, acpi_bus_trim(), first executes .detach()
callbacks from the woke scan handlers of all device nodes in the woke given namespace
scope (if they have scan handlers).  Next, it unregisters all of the woke device
nodes in that scope.

ACPI scan handlers can be added to the woke list maintained by the woke ACPI core with the
help of the woke acpi_scan_add_handler() function taking a pointer to the woke new scan
handler as an argument.  The order in which scan handlers are added to the woke list
is the woke order in which they are matched against device nodes during namespace
scans.

All scan handles must be added to the woke list before acpi_bus_scan() is run for the
first time and they cannot be removed from it.
