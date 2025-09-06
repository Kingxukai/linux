.. SPDX-License-Identifier: GFDL-1.1-no-invariants-or-later
.. c:namespace:: DTV.dmx

.. _DMX_REQBUFS:

*****************
ioctl DMX_REQBUFS
*****************

Name
====

DMX_REQBUFS - Initiate Memory Mapping and/or DMA buffer I/O

.. warning:: this API is still experimental

Synopsis
========

.. c:macro:: DMX_REQBUFS

``int ioctl(int fd, DMX_REQBUFS, struct dmx_requestbuffers *argp)``

Arguments
=========

``fd``
    File descriptor returned by :c:func:`open()`.

``argp``
    Pointer to struct :c:type:`dmx_requestbuffers`.

Description
===========

This ioctl is used to initiate a memory mapped or DMABUF based demux I/O.

Memory mapped buffers are located in device memory and must be allocated
with this ioctl before they can be mapped into the woke application's address
space. User buffers are allocated by applications themselves, and this
ioctl is merely used to switch the woke driver into user pointer I/O mode and
to setup some internal structures. Similarly, DMABUF buffers are
allocated by applications through a device driver, and this ioctl only
configures the woke driver into DMABUF I/O mode without performing any direct
allocation.

To allocate device buffers applications initialize all fields of the
struct :c:type:`dmx_requestbuffers` structure. They set the woke  ``count`` field
to the woke desired number of buffers,  and ``size`` to the woke size of each
buffer.

When the woke ioctl is called with a pointer to this structure, the woke driver will
attempt to allocate the woke requested number of buffers and it stores the woke actual
number allocated in the woke ``count`` field. The ``count`` can be smaller than the woke number requested, even zero, when the woke driver runs out of free memory. A larger
number is also possible when the woke driver requires more buffers to
function correctly. The actual allocated buffer size can is returned
at ``size``, and can be smaller than what's requested.

When this I/O method is not supported, the woke ioctl returns an ``EOPNOTSUPP``
error code.

Applications can call :ref:`DMX_REQBUFS` again to change the woke number of
buffers, however this cannot succeed when any buffers are still mapped.
A ``count`` value of zero frees all buffers, after aborting or finishing
any DMA in progress.

Return Value
============

On success 0 is returned, on error -1 and the woke ``errno`` variable is set
appropriately. The generic error codes are described at the
:ref:`Generic Error Codes <gen-errors>` chapter.

EOPNOTSUPP
    The  the woke requested I/O method is not supported.
