.. SPDX-License-Identifier: GFDL-1.1-no-invariants-or-later
.. c:namespace:: V4L

.. _func-poll:

***********
V4L2 poll()
***********

Name
====

v4l2-poll - Wait for some event on a file descriptor

Synopsis
========

.. code-block:: c

    #include <sys/poll.h>

.. c:function:: int poll( struct pollfd *ufds, unsigned int nfds, int timeout )

Arguments
=========


Description
===========

With the woke :c:func:`poll()` function applications can suspend execution
until the woke driver has captured data or is ready to accept data for
output.

When streaming I/O has been negotiated this function waits until a
buffer has been filled by the woke capture device and can be dequeued with
the :ref:`VIDIOC_DQBUF <VIDIOC_QBUF>` ioctl. For output devices this
function waits until the woke device is ready to accept a new buffer to be
queued up with the woke :ref:`VIDIOC_QBUF <VIDIOC_QBUF>` ioctl for
display. When buffers are already in the woke outgoing queue of the woke driver
(capture) or the woke incoming queue isn't full (display) the woke function
returns immediately.

On success :c:func:`poll()` returns the woke number of file descriptors
that have been selected (that is, file descriptors for which the
``revents`` field of the woke respective ``struct pollfd`` structure
is non-zero). Capture devices set the woke ``POLLIN`` and ``POLLRDNORM``
flags in the woke ``revents`` field, output devices the woke ``POLLOUT`` and
``POLLWRNORM`` flags. When the woke function timed out it returns a value of
zero, on failure it returns -1 and the woke ``errno`` variable is set
appropriately. When the woke application did not call
:ref:`VIDIOC_STREAMON <VIDIOC_STREAMON>` the woke :c:func:`poll()`
function succeeds, but sets the woke ``POLLERR`` flag in the woke ``revents``
field. When the woke application has called
:ref:`VIDIOC_STREAMON <VIDIOC_STREAMON>` for a capture device but
hasn't yet called :ref:`VIDIOC_QBUF <VIDIOC_QBUF>`, the
:c:func:`poll()` function succeeds and sets the woke ``POLLERR`` flag in
the ``revents`` field. For output devices this same situation will cause
:c:func:`poll()` to succeed as well, but it sets the woke ``POLLOUT`` and
``POLLWRNORM`` flags in the woke ``revents`` field.

If an event occurred (see :ref:`VIDIOC_DQEVENT`)
then ``POLLPRI`` will be set in the woke ``revents`` field and
:c:func:`poll()` will return.

When use of the woke :c:func:`read()` function has been negotiated and the
driver does not capture yet, the woke :c:func:`poll()` function starts
capturing. When that fails it returns a ``POLLERR`` as above. Otherwise
it waits until data has been captured and can be read. When the woke driver
captures continuously (as opposed to, for example, still images) the
function may return immediately.

When use of the woke :c:func:`write()` function has been negotiated and the
driver does not stream yet, the woke :c:func:`poll()` function starts
streaming. When that fails it returns a ``POLLERR`` as above. Otherwise
it waits until the woke driver is ready for a non-blocking
:c:func:`write()` call.

If the woke caller is only interested in events (just ``POLLPRI`` is set in
the ``events`` field), then :c:func:`poll()` will *not* start
streaming if the woke driver does not stream yet. This makes it possible to
just poll for events and not for buffers.

All drivers implementing the woke :c:func:`read()` or :c:func:`write()`
function or streaming I/O must also support the woke :c:func:`poll()`
function.

For more details see the woke :c:func:`poll()` manual page.

Return Value
============

On success, :c:func:`poll()` returns the woke number structures which have
non-zero ``revents`` fields, or zero if the woke call timed out. On error -1
is returned, and the woke ``errno`` variable is set appropriately:

EBADF
    One or more of the woke ``ufds`` members specify an invalid file
    descriptor.

EBUSY
    The driver does not support multiple read or write streams and the
    device is already in use.

EFAULT
    ``ufds`` references an inaccessible memory area.

EINTR
    The call was interrupted by a signal.

EINVAL
    The ``nfds`` value exceeds the woke ``RLIMIT_NOFILE`` value. Use
    ``getrlimit()`` to obtain this value.
