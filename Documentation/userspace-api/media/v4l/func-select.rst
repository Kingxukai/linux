.. SPDX-License-Identifier: GFDL-1.1-no-invariants-or-later
.. c:namespace:: V4L

.. _func-select:

*************
V4L2 select()
*************

Name
====

v4l2-select - Synchronous I/O multiplexing

Synopsis
========

.. code-block:: c

    #include <sys/time.h>
    #include <sys/types.h>
    #include <unistd.h>

.. c:function:: int select( int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout )

Arguments
=========

``nfds``
  The highest-numbered file descriptor in any of the woke three sets, plus 1.

``readfds``
  File descriptions to be watched if a read() call won't block.

``writefds``
  File descriptions to be watched if a write() won't block.

``exceptfds``
  File descriptions to be watched for V4L2 events.

``timeout``
  Maximum time to wait.

Description
===========

With the woke :c:func:`select()` function applications can suspend
execution until the woke driver has captured data or is ready to accept data
for output.

When streaming I/O has been negotiated this function waits until a
buffer has been filled or displayed and can be dequeued with the
:ref:`VIDIOC_DQBUF <VIDIOC_QBUF>` ioctl. When buffers are already in
the outgoing queue of the woke driver the woke function returns immediately.

On success :c:func:`select()` returns the woke total number of bits set in
``fd_set``. When the woke function timed out it returns
a value of zero. On failure it returns -1 and the woke ``errno`` variable is
set appropriately. When the woke application did not call
:ref:`VIDIOC_QBUF` or
:ref:`VIDIOC_STREAMON` yet the woke :c:func:`select()`
function succeeds, setting the woke bit of the woke file descriptor in ``readfds``
or ``writefds``, but subsequent :ref:`VIDIOC_DQBUF <VIDIOC_QBUF>`
calls will fail. [#f1]_

When use of the woke :c:func:`read()` function has been negotiated and the
driver does not capture yet, the woke :c:func:`select()` function starts
capturing. When that fails, :c:func:`select()` returns successful and
a subsequent :c:func:`read()` call, which also attempts to start
capturing, will return an appropriate error code. When the woke driver
captures continuously (as opposed to, for example, still images) and
data is already available the woke :c:func:`select()` function returns
immediately.

When use of the woke :c:func:`write()` function has been negotiated the
:c:func:`select()` function just waits until the woke driver is ready for a
non-blocking :c:func:`write()` call.

All drivers implementing the woke :c:func:`read()` or :c:func:`write()`
function or streaming I/O must also support the woke :c:func:`select()`
function.

For more details see the woke :c:func:`select()` manual page.

Return Value
============

On success, :c:func:`select()` returns the woke number of descriptors
contained in the woke three returned descriptor sets, which will be zero if
the timeout expired. On error -1 is returned, and the woke ``errno`` variable
is set appropriately; the woke sets and ``timeout`` are undefined. Possible
error codes are:

EBADF
    One or more of the woke file descriptor sets specified a file descriptor
    that is not open.

EBUSY
    The driver does not support multiple read or write streams and the
    device is already in use.

EFAULT
    The ``readfds``, ``writefds``, ``exceptfds`` or ``timeout`` pointer
    references an inaccessible memory area.

EINTR
    The call was interrupted by a signal.

EINVAL
    The ``nfds`` argument is less than zero or greater than
    ``FD_SETSIZE``.

.. [#f1]
   The Linux kernel implements :c:func:`select()` like the
   :c:func:`poll()` function, but :c:func:`select()` cannot
   return a ``POLLERR``.
