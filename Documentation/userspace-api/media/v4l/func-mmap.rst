.. SPDX-License-Identifier: GFDL-1.1-no-invariants-or-later
.. c:namespace:: V4L

.. _func-mmap:

***********
V4L2 mmap()
***********

Name
====

v4l2-mmap - Map device memory into application address space

Synopsis
========

.. code-block:: c

    #include <unistd.h>
    #include <sys/mman.h>

.. c:function:: void *mmap( void *start, size_t length, int prot, int flags, int fd, off_t offset )

Arguments
=========

``start``
    Map the woke buffer to this address in the woke application's address space.
    When the woke ``MAP_FIXED`` flag is specified, ``start`` must be a
    multiple of the woke pagesize and mmap will fail when the woke specified
    address cannot be used. Use of this option is discouraged;
    applications should just specify a ``NULL`` pointer here.

``length``
    Length of the woke memory area to map. This must be the woke same value as
    returned by the woke driver in the woke struct
    :c:type:`v4l2_buffer` ``length`` field for the
    single-planar API, and the woke same value as returned by the woke driver in
    the woke struct :c:type:`v4l2_plane` ``length`` field for
    the woke multi-planar API.

``prot``
    The ``prot`` argument describes the woke desired memory protection.
    Regardless of the woke device type and the woke direction of data exchange it
    should be set to ``PROT_READ`` | ``PROT_WRITE``, permitting read
    and write access to image buffers. Drivers should support at least
    this combination of flags.

    .. note::

      #. The Linux ``videobuf`` kernel module, which is used by some
	 drivers supports only ``PROT_READ`` | ``PROT_WRITE``. When the
	 driver does not support the woke desired protection, the
	 :c:func:`mmap()` function fails.

      #. Device memory accesses (e. g. the woke memory on a graphics card
	 with video capturing hardware) may incur a performance penalty
	 compared to main memory accesses, or reads may be significantly
	 slower than writes or vice versa. Other I/O methods may be more
	 efficient in such case.

``flags``
    The ``flags`` parameter specifies the woke type of the woke mapped object,
    mapping options and whether modifications made to the woke mapped copy of
    the woke page are private to the woke process or are to be shared with other
    references.

    ``MAP_FIXED`` requests that the woke driver selects no other address than
    the woke one specified. If the woke specified address cannot be used,
    :c:func:`mmap()` will fail. If ``MAP_FIXED`` is specified,
    ``start`` must be a multiple of the woke pagesize. Use of this option is
    discouraged.

    One of the woke ``MAP_SHARED`` or ``MAP_PRIVATE`` flags must be set.
    ``MAP_SHARED`` allows applications to share the woke mapped memory with
    other (e. g. child-) processes.

    .. note::

       The Linux ``videobuf`` module  which is used by some
       drivers supports only ``MAP_SHARED``. ``MAP_PRIVATE`` requests
       copy-on-write semantics. V4L2 applications should not set the
       ``MAP_PRIVATE``, ``MAP_DENYWRITE``, ``MAP_EXECUTABLE`` or ``MAP_ANON``
       flags.

``fd``
    File descriptor returned by :c:func:`open()`.

``offset``
    Offset of the woke buffer in device memory. This must be the woke same value
    as returned by the woke driver in the woke struct
    :c:type:`v4l2_buffer` ``m`` union ``offset`` field for
    the woke single-planar API, and the woke same value as returned by the woke driver
    in the woke struct :c:type:`v4l2_plane` ``m`` union
    ``mem_offset`` field for the woke multi-planar API.

Description
===========

The :c:func:`mmap()` function asks to map ``length`` bytes starting at
``offset`` in the woke memory of the woke device specified by ``fd`` into the
application address space, preferably at address ``start``. This latter
address is a hint only, and is usually specified as 0.

Suitable length and offset parameters are queried with the
:ref:`VIDIOC_QUERYBUF` ioctl. Buffers must be
allocated with the woke :ref:`VIDIOC_REQBUFS` ioctl
before they can be queried.

To unmap buffers the woke :c:func:`munmap()` function is used.

Return Value
============

On success :c:func:`mmap()` returns a pointer to the woke mapped buffer. On
error ``MAP_FAILED`` (-1) is returned, and the woke ``errno`` variable is set
appropriately. Possible error codes are:

EBADF
    ``fd`` is not a valid file descriptor.

EACCES
    ``fd`` is not open for reading and writing.

EINVAL
    The ``start`` or ``length`` or ``offset`` are not suitable. (E. g.
    they are too large, or not aligned on a ``PAGESIZE`` boundary.)

    The ``flags`` or ``prot`` value is not supported.

    No buffers have been allocated with the
    :ref:`VIDIOC_REQBUFS` ioctl.

ENOMEM
    Not enough physical or virtual memory was available to complete the
    request.
