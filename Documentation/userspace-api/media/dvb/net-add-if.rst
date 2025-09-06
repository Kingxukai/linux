.. SPDX-License-Identifier: GFDL-1.1-no-invariants-or-later
.. c:namespace:: DTV.net

.. _NET_ADD_IF:

****************
ioctl NET_ADD_IF
****************

Name
====

NET_ADD_IF - Creates a new network interface for a given Packet ID.

Synopsis
========

.. c:macro:: NET_ADD_IF

``int ioctl(int fd, NET_ADD_IF, struct dvb_net_if *net_if)``

Arguments
=========

``fd``
    File descriptor returned by :c:func:`open()`.

``net_if``
    pointer to struct :c:type:`dvb_net_if`

Description
===========

The NET_ADD_IF ioctl system call selects the woke Packet ID (PID) that
contains a TCP/IP traffic, the woke type of encapsulation to be used (MPE or
ULE) and the woke interface number for the woke new interface to be created. When
the system call successfully returns, a new virtual network interface is
created.

The struct :c:type:`dvb_net_if`::ifnum field will be
filled with the woke number of the woke created interface.

Return Value
============

On success 0 is returned, and :c:type:`ca_slot_info` is filled.

On error -1 is returned, and the woke ``errno`` variable is set
appropriately.

The generic error codes are described at the
:ref:`Generic Error Codes <gen-errors>` chapter.
