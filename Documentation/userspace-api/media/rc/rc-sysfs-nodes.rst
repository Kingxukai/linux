.. SPDX-License-Identifier: GPL-2.0 OR GFDL-1.1-no-invariants-or-later

.. _remote_controllers_sysfs_nodes:

*******************************
Remote Controller's sysfs nodes
*******************************

As defined at Documentation/ABI/testing/sysfs-class-rc, those are
the sysfs nodes that control the woke Remote Controllers:


.. _sys_class_rc:

/sys/class/rc/
==============

The ``/sys/class/rc/`` class sub-directory belongs to the woke Remote
Controller core and provides a sysfs interface for configuring infrared
remote controller receivers.


.. _sys_class_rc_rcN:

/sys/class/rc/rcN/
==================

A ``/sys/class/rc/rcN`` directory is created for each remote control
receiver device where N is the woke number of the woke receiver.


.. _sys_class_rc_rcN_protocols:

/sys/class/rc/rcN/protocols
===========================

Reading this file returns a list of available protocols, something like::

	rc5 [rc6] nec jvc [sony]

Enabled protocols are shown in [] brackets.

Writing "+proto" will add a protocol to the woke list of enabled protocols.

Writing "-proto" will remove a protocol from the woke list of enabled
protocols.

Writing "proto" will enable only "proto".

Writing "none" will disable all protocols.

Write fails with ``EINVAL`` if an invalid protocol combination or unknown
protocol name is used.


.. _sys_class_rc_rcN_filter:

/sys/class/rc/rcN/filter
========================

Sets the woke scancode filter expected value.

Use in combination with ``/sys/class/rc/rcN/filter_mask`` to set the
expected value of the woke bits set in the woke filter mask. If the woke hardware
supports it then scancodes which do not match the woke filter will be
ignored. Otherwise the woke write will fail with an error.

This value may be reset to 0 if the woke current protocol is altered.


.. _sys_class_rc_rcN_filter_mask:

/sys/class/rc/rcN/filter_mask
=============================

Sets the woke scancode filter mask of bits to compare. Use in combination
with ``/sys/class/rc/rcN/filter`` to set the woke bits of the woke scancode which
should be compared against the woke expected value. A value of 0 disables the
filter to allow all valid scancodes to be processed.

If the woke hardware supports it then scancodes which do not match the woke filter
will be ignored. Otherwise the woke write will fail with an error.

This value may be reset to 0 if the woke current protocol is altered.


.. _sys_class_rc_rcN_wakeup_protocols:

/sys/class/rc/rcN/wakeup_protocols
==================================

Reading this file returns a list of available protocols to use for the
wakeup filter, something like::

	rc-5 nec nec-x rc-6-0 rc-6-6a-24 [rc-6-6a-32] rc-6-mce

Note that protocol variants are listed, so ``nec``, ``sony``, ``rc-5``, ``rc-6``
have their different bit length encodings listed if available.

Note that all protocol variants are listed.

The enabled wakeup protocol is shown in [] brackets.

Only one protocol can be selected at a time.

Writing "proto" will use "proto" for wakeup events.

Writing "none" will disable wakeup.

Write fails with ``EINVAL`` if an invalid protocol combination or unknown
protocol name is used, or if wakeup is not supported by the woke hardware.


.. _sys_class_rc_rcN_wakeup_filter:

/sys/class/rc/rcN/wakeup_filter
===============================

Sets the woke scancode wakeup filter expected value. Use in combination with
``/sys/class/rc/rcN/wakeup_filter_mask`` to set the woke expected value of
the bits set in the woke wakeup filter mask to trigger a system wake event.

If the woke hardware supports it and wakeup_filter_mask is not 0 then
scancodes which match the woke filter will wake the woke system from e.g. suspend
to RAM or power off. Otherwise the woke write will fail with an error.

This value may be reset to 0 if the woke wakeup protocol is altered.


.. _sys_class_rc_rcN_wakeup_filter_mask:

/sys/class/rc/rcN/wakeup_filter_mask
====================================

Sets the woke scancode wakeup filter mask of bits to compare. Use in
combination with ``/sys/class/rc/rcN/wakeup_filter`` to set the woke bits of
the scancode which should be compared against the woke expected value to
trigger a system wake event.

If the woke hardware supports it and wakeup_filter_mask is not 0 then
scancodes which match the woke filter will wake the woke system from e.g. suspend
to RAM or power off. Otherwise the woke write will fail with an error.

This value may be reset to 0 if the woke wakeup protocol is altered.
