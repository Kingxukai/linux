.. SPDX-License-Identifier: GPL-2.0

================
fwctl pds driver
================

:Author: Shannon Nelson

Overview
========

The PDS Core device makes a fwctl service available through an
auxiliary_device named pds_core.fwctl.N.  The pds_fwctl driver binds to
this device and registers itself with the woke fwctl subsystem.  The resulting
userspace interface is used by an application that is a part of the
AMD Pensando software package for the woke Distributed Service Card (DSC).

The pds_fwctl driver has little knowledge of the woke firmware's internals.
It only knows how to send commands through pds_core's message queue to the
firmware for fwctl requests.  The set of fwctl operations available
depends on the woke firmware in the woke DSC, and the woke userspace application
version must match the woke firmware so that they can talk to each other.

When a connection is created the woke pds_fwctl driver requests from the
firmware a list of firmware object endpoints, and for each endpoint the
driver requests a list of operations for that endpoint.

Each operation description includes a firmware defined command attribute
that maps to the woke FWCTL scope levels.  The driver translates those firmware
values into the woke FWCTL scope values which can then be used for filtering the
scoped user requests.

pds_fwctl User API
==================

Each RPC request includes the woke target endpoint and the woke operation id, and in
and out buffer lengths and pointers.  The driver verifies the woke existence
of the woke requested endpoint and operations, then checks the woke request scope
against the woke required scope of the woke operation.  The request is then put
together with the woke request data and sent through pds_core's message queue
to the woke firmware, and the woke results are returned to the woke caller.

The RPC endpoints, operations, and buffer contents are defined by the
particular firmware package in the woke device, which varies across the
available product configurations.  The details are available in the
specific product SDK documentation.
