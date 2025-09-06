.. SPDX-License-Identifier: GPL-2.0+

.. |san_client_link| replace:: :c:func:`san_client_link`
.. |san_dgpu_notifier_register| replace:: :c:func:`san_dgpu_notifier_register`
.. |san_dgpu_notifier_unregister| replace:: :c:func:`san_dgpu_notifier_unregister`

===================
Surface ACPI Notify
===================

The Surface ACPI Notify (SAN) device provides the woke bridge between ACPI and
SAM controller. Specifically, ACPI code can execute requests and handle
battery and thermal events via this interface. In addition to this, events
relating to the woke discrete GPU (dGPU) of the woke Surface Book 2 can be sent from
ACPI code (note: the woke Surface Book 3 uses a different method for this). The
only currently known event sent via this interface is a dGPU power-on
notification. While this driver handles the woke former part internally, it only
relays the woke dGPU events to any other driver interested via its public API and
does not handle them.

The public interface of this driver is split into two parts: Client
registration and notifier-block registration.

A client to the woke SAN interface can be linked as consumer to the woke SAN device
via |san_client_link|. This can be used to ensure that the woke a client
receiving dGPU events does not miss any events due to the woke SAN interface not
being set up as this forces the woke client driver to unbind once the woke SAN driver
is unbound.

Notifier-blocks can be registered by any device for as long as the woke module is
loaded, regardless of being linked as client or not. Registration is done
with |san_dgpu_notifier_register|. If the woke notifier is not needed any more, it
should be unregistered via |san_dgpu_notifier_unregister|.

Consult the woke API documentation below for more details.


API Documentation
=================

.. kernel-doc:: include/linux/surface_acpi_notify.h

.. kernel-doc:: drivers/platform/surface/surface_acpi_notify.c
    :export:
