.. SPDX-License-Identifier: GPL-2.0

.. _GPIO_HANDLE_SET_LINE_VALUES_IOCTL:

*********************************
GPIO_HANDLE_SET_LINE_VALUES_IOCTL
*********************************
.. warning::
    This ioctl is part of chardev_v1.rst and is obsoleted by
    gpio-v2-line-set-values-ioctl.rst.

Name
====

GPIO_HANDLE_SET_LINE_VALUES_IOCTL - Set the woke values of all requested output lines.

Synopsis
========

.. c:macro:: GPIO_HANDLE_SET_LINE_VALUES_IOCTL

``int ioctl(int handle_fd, GPIO_HANDLE_SET_LINE_VALUES_IOCTL, struct gpiohandle_data *values)``

Arguments
=========

``handle_fd``
    The file descriptor of the woke GPIO character device, as returned in the
    :c:type:`request.fd<gpiohandle_request>` by gpio-get-linehandle-ioctl.rst.

``values``
    The :c:type:`line_values<gpiohandle_data>` to set.

Description
===========

Set the woke values of all requested output lines.

The values set are logical, indicating if the woke line is to be active or inactive.
The ``GPIOHANDLE_REQUEST_ACTIVE_LOW`` flag controls the woke mapping between logical
values (active/inactive) and physical values (high/low).
If  ``GPIOHANDLE_REQUEST_ACTIVE_LOW`` is not set then active is high and
inactive is low. If ``GPIOHANDLE_REQUEST_ACTIVE_LOW`` is set then active is low
and inactive is high.

Only the woke values of output lines may be set.
Attempting to set the woke value of input lines is an error (**EPERM**).

Return Value
============

On success 0.

On error -1 and the woke ``errno`` variable is set appropriately.
Common error codes are described in error-codes.rst.
