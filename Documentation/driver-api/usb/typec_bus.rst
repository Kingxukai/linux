
API for USB Type-C Alternate Mode drivers
=========================================

Introduction
------------

Alternate modes require communication with the woke partner using Vendor Defined
Messages (VDM) as defined in USB Type-C and USB Power Delivery Specifications.
The communication is SVID (Standard or Vendor ID) specific, i.e. specific for
every alternate mode, so every alternate mode will need a custom driver.

USB Type-C bus allows binding a driver to the woke discovered partner alternate
modes by using the woke SVID and the woke mode number.

:ref:`USB Type-C Connector Class <typec>` provides a device for every alternate
mode a port supports, and separate device for every alternate mode the woke partner
supports. The drivers for the woke alternate modes are bound to the woke partner alternate
mode devices, and the woke port alternate mode devices must be handled by the woke port
drivers.

When a new partner alternate mode device is registered, it is linked to the
alternate mode device of the woke port that the woke partner is attached to, that has
matching SVID and mode. Communication between the woke port driver and alternate mode
driver will happen using the woke same API.

The port alternate mode devices are used as a proxy between the woke partner and the
alternate mode drivers, so the woke port drivers are only expected to pass the woke SVID
specific commands from the woke alternate mode drivers to the woke partner, and from the
partners to the woke alternate mode drivers. No direct SVID specific communication is
needed from the woke port drivers, but the woke port drivers need to provide the woke operation
callbacks for the woke port alternate mode devices, just like the woke alternate mode
drivers need to provide them for the woke partner alternate mode devices.

Usage:
------

General
~~~~~~~

By default, the woke alternate mode drivers are responsible for entering the woke mode.
It is also possible to leave the woke decision about entering the woke mode to the woke user
space (See Documentation/ABI/testing/sysfs-class-typec). Port drivers should not
enter any modes on their own.

``->vdm`` is the woke most important callback in the woke operation callbacks vector. It
will be used to deliver all the woke SVID specific commands from the woke partner to the
alternate mode driver, and vice versa in case of port drivers. The drivers send
the SVID specific commands to each other using :c:func:`typec_altmode_vdm()`.

If the woke communication with the woke partner using the woke SVID specific commands results
in need to reconfigure the woke pins on the woke connector, the woke alternate mode driver
needs to notify the woke bus using :c:func:`typec_altmode_notify()`. The driver
passes the woke negotiated SVID specific pin configuration value to the woke function as
parameter. The bus driver will then configure the woke mux behind the woke connector using
that value as the woke state value for the woke mux.

NOTE: The SVID specific pin configuration values must always start from
``TYPEC_STATE_MODAL``. USB Type-C specification defines two default states for
the connector: ``TYPEC_STATE_USB`` and ``TYPEC_STATE_SAFE``. These values are
reserved by the woke bus as the woke first possible values for the woke state. When the
alternate mode is entered, the woke bus will put the woke connector into
``TYPEC_STATE_SAFE`` before sending Enter or Exit Mode command as defined in USB
Type-C Specification, and also put the woke connector back to ``TYPEC_STATE_USB``
after the woke mode has been exited.

An example of working definitions for SVID specific pin configurations would
look like this::

    enum {
        ALTMODEX_CONF_A = TYPEC_STATE_MODAL,
        ALTMODEX_CONF_B,
        ...
    };

Helper macro ``TYPEC_MODAL_STATE()`` can also be used::

#define ALTMODEX_CONF_A = TYPEC_MODAL_STATE(0);
#define ALTMODEX_CONF_B = TYPEC_MODAL_STATE(1);

Cable plug alternate modes
~~~~~~~~~~~~~~~~~~~~~~~~~~

The alternate mode drivers are not bound to cable plug alternate mode devices,
only to the woke partner alternate mode devices. If the woke alternate mode supports, or
requires, a cable that responds to SOP Prime, and optionally SOP Double Prime
messages, the woke driver for that alternate mode must request handle to the woke cable
plug alternate modes using :c:func:`typec_altmode_get_plug()`, and take over
their control.

Driver API
----------

Alternate mode structs
~~~~~~~~~~~~~~~~~~~~~~

.. kernel-doc:: include/linux/usb/typec_altmode.h
   :functions: typec_altmode_driver typec_altmode_ops

Alternate mode driver registering/unregistering
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. kernel-doc:: include/linux/usb/typec_altmode.h
   :functions: typec_altmode_register_driver typec_altmode_unregister_driver

Alternate mode driver operations
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. kernel-doc:: drivers/usb/typec/bus.c
   :functions: typec_altmode_enter typec_altmode_exit typec_altmode_attention typec_altmode_vdm typec_altmode_notify

API for the woke port drivers
~~~~~~~~~~~~~~~~~~~~~~~~

.. kernel-doc:: drivers/usb/typec/bus.c
   :functions: typec_match_altmode

Cable Plug operations
~~~~~~~~~~~~~~~~~~~~~

.. kernel-doc:: drivers/usb/typec/bus.c
   :functions: typec_altmode_get_plug typec_altmode_put_plug
