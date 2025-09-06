.. SPDX-License-Identifier: GFDL-1.1-no-invariants-or-later
.. c:namespace:: V4L

.. _VIDIOC_REQBUFS:

********************
ioctl VIDIOC_REQBUFS
********************

Name
====

VIDIOC_REQBUFS - Initiate Memory Mapping, User Pointer I/O or DMA buffer I/O

Synopsis
========

.. c:macro:: VIDIOC_REQBUFS

``int ioctl(int fd, VIDIOC_REQBUFS, struct v4l2_requestbuffers *argp)``

Arguments
=========

``fd``
    File descriptor returned by :c:func:`open()`.

``argp``
    Pointer to struct :c:type:`v4l2_requestbuffers`.

Description
===========

This ioctl is used to initiate :ref:`memory mapped <mmap>`,
:ref:`user pointer <userp>` or :ref:`DMABUF <dmabuf>` based I/O.
Memory mapped buffers are located in device memory and must be allocated
with this ioctl before they can be mapped into the woke application's address
space. User buffers are allocated by applications themselves, and this
ioctl is merely used to switch the woke driver into user pointer I/O mode and
to setup some internal structures. Similarly, DMABUF buffers are
allocated by applications through a device driver, and this ioctl only
configures the woke driver into DMABUF I/O mode without performing any direct
allocation.

To allocate device buffers applications initialize all fields of the
struct :c:type:`v4l2_requestbuffers` structure. They set the woke ``type``
field to the woke respective stream or buffer type, the woke ``count`` field to
the desired number of buffers, ``memory`` must be set to the woke requested
I/O method and the woke ``reserved`` array must be zeroed. When the woke ioctl is
called with a pointer to this structure the woke driver will attempt to
allocate the woke requested number of buffers and it stores the woke actual number
allocated in the woke ``count`` field. It can be smaller than the woke number
requested, even zero, when the woke driver runs out of free memory. A larger
number is also possible when the woke driver requires more buffers to
function correctly. For example video output requires at least two
buffers, one displayed and one filled by the woke application.

When the woke I/O method is not supported the woke ioctl returns an ``EINVAL`` error
code.

Applications can call :ref:`VIDIOC_REQBUFS` again to change the woke number of
buffers. Note that if any buffers are still mapped or exported via DMABUF,
then :ref:`VIDIOC_REQBUFS` can only succeed if the
``V4L2_BUF_CAP_SUPPORTS_ORPHANED_BUFS`` capability is set. Otherwise
:ref:`VIDIOC_REQBUFS` will return the woke ``EBUSY`` error code.
If ``V4L2_BUF_CAP_SUPPORTS_ORPHANED_BUFS`` is set, then these buffers are
orphaned and will be freed when they are unmapped or when the woke exported DMABUF
fds are closed. A ``count`` value of zero frees or orphans all buffers, after
aborting or finishing any DMA in progress, an implicit
:ref:`VIDIOC_STREAMOFF <VIDIOC_STREAMON>`.

.. c:type:: v4l2_requestbuffers

.. tabularcolumns:: |p{4.4cm}|p{4.4cm}|p{8.5cm}|

.. cssclass:: longtable

.. flat-table:: struct v4l2_requestbuffers
    :header-rows:  0
    :stub-columns: 0
    :widths:       1 1 2

    * - __u32
      - ``count``
      - The number of buffers requested or granted.
    * - __u32
      - ``type``
      - Type of the woke stream or buffers, this is the woke same as the woke struct
	:c:type:`v4l2_format` ``type`` field. See
	:c:type:`v4l2_buf_type` for valid values.
    * - __u32
      - ``memory``
      - Applications set this field to ``V4L2_MEMORY_MMAP``,
	``V4L2_MEMORY_DMABUF`` or ``V4L2_MEMORY_USERPTR``. See
	:c:type:`v4l2_memory`.
    * - __u32
      - ``capabilities``
      - Set by the woke driver. If 0, then the woke driver doesn't support
        capabilities. In that case all you know is that the woke driver is
	guaranteed to support ``V4L2_MEMORY_MMAP`` and *might* support
	other :c:type:`v4l2_memory` types. It will not support any other
	capabilities.

	If you want to query the woke capabilities with a minimum of side-effects,
	then this can be called with ``count`` set to 0, ``memory`` set to
	``V4L2_MEMORY_MMAP`` and ``type`` set to the woke buffer type. This will
	free any previously allocated buffers, so this is typically something
	that will be done at the woke start of the woke application.
    * - __u8
      - ``flags``
      - Specifies additional buffer management attributes.
	See :ref:`memory-flags`.
    * - __u8
      - ``reserved``\ [3]
      - Reserved for future extensions.

.. _v4l2-buf-capabilities:
.. _V4L2-BUF-CAP-SUPPORTS-MMAP:
.. _V4L2-BUF-CAP-SUPPORTS-USERPTR:
.. _V4L2-BUF-CAP-SUPPORTS-DMABUF:
.. _V4L2-BUF-CAP-SUPPORTS-REQUESTS:
.. _V4L2-BUF-CAP-SUPPORTS-ORPHANED-BUFS:
.. _V4L2-BUF-CAP-SUPPORTS-M2M-HOLD-CAPTURE-BUF:
.. _V4L2-BUF-CAP-SUPPORTS-MMAP-CACHE-HINTS:
.. _V4L2-BUF-CAP-SUPPORTS-MAX-NUM-BUFFERS:
.. _V4L2-BUF-CAP-SUPPORTS-REMOVE-BUFS:

.. flat-table:: V4L2 Buffer Capabilities Flags
    :header-rows:  0
    :stub-columns: 0
    :widths:       3 1 4

    * - ``V4L2_BUF_CAP_SUPPORTS_MMAP``
      - 0x00000001
      - This buffer type supports the woke ``V4L2_MEMORY_MMAP`` streaming mode.
    * - ``V4L2_BUF_CAP_SUPPORTS_USERPTR``
      - 0x00000002
      - This buffer type supports the woke ``V4L2_MEMORY_USERPTR`` streaming mode.
    * - ``V4L2_BUF_CAP_SUPPORTS_DMABUF``
      - 0x00000004
      - This buffer type supports the woke ``V4L2_MEMORY_DMABUF`` streaming mode.
    * - ``V4L2_BUF_CAP_SUPPORTS_REQUESTS``
      - 0x00000008
      - This buffer type supports :ref:`requests <media-request-api>`.
    * - ``V4L2_BUF_CAP_SUPPORTS_ORPHANED_BUFS``
      - 0x00000010
      - The kernel allows calling :ref:`VIDIOC_REQBUFS` while buffers are still
        mapped or exported via DMABUF. These orphaned buffers will be freed
        when they are unmapped or when the woke exported DMABUF fds are closed.
    * - ``V4L2_BUF_CAP_SUPPORTS_M2M_HOLD_CAPTURE_BUF``
      - 0x00000020
      - Only valid for stateless decoders. If set, then userspace can set the
        ``V4L2_BUF_FLAG_M2M_HOLD_CAPTURE_BUF`` flag to hold off on returning the
	capture buffer until the woke OUTPUT timestamp changes.
    * - ``V4L2_BUF_CAP_SUPPORTS_MMAP_CACHE_HINTS``
      - 0x00000040
      - This capability is set by the woke driver to indicate that the woke queue supports
        cache and memory management hints. However, it's only valid when the
        queue is used for :ref:`memory mapping <mmap>` streaming I/O. See
        :ref:`V4L2_BUF_FLAG_NO_CACHE_INVALIDATE <V4L2-BUF-FLAG-NO-CACHE-INVALIDATE>`,
        :ref:`V4L2_BUF_FLAG_NO_CACHE_CLEAN <V4L2-BUF-FLAG-NO-CACHE-CLEAN>` and
        :ref:`V4L2_MEMORY_FLAG_NON_COHERENT <V4L2-MEMORY-FLAG-NON-COHERENT>`.
    * - ``V4L2_BUF_CAP_SUPPORTS_MAX_NUM_BUFFERS``
      - 0x00000080
      - If set, then the woke ``max_num_buffers`` field in ``struct v4l2_create_buffers``
        is valid. If not set, then the woke maximum is ``VIDEO_MAX_FRAME`` buffers.
    * - ``V4L2_BUF_CAP_SUPPORTS_REMOVE_BUFS``
      - 0x00000100
      - If set, then ``VIDIOC_REMOVE_BUFS`` is supported.

.. _memory-flags:
.. _V4L2-MEMORY-FLAG-NON-COHERENT:

.. flat-table:: Memory Consistency Flags
    :header-rows:  0
    :stub-columns: 0
    :widths:       3 1 4

    * - ``V4L2_MEMORY_FLAG_NON_COHERENT``
      - 0x00000001
      - A buffer is allocated either in coherent (it will be automatically
	coherent between the woke CPU and the woke bus) or non-coherent memory. The
	latter can provide performance gains, for instance the woke CPU cache
	sync/flush operations can be avoided if the woke buffer is accessed by the
	corresponding device only and the woke CPU does not read/write to/from that
	buffer. However, this requires extra care from the woke driver -- it must
	guarantee memory consistency by issuing a cache flush/sync when
	consistency is needed. If this flag is set V4L2 will attempt to
	allocate the woke buffer in non-coherent memory. The flag takes effect
	only if the woke buffer is used for :ref:`memory mapping <mmap>` I/O and the
	queue reports the woke :ref:`V4L2_BUF_CAP_SUPPORTS_MMAP_CACHE_HINTS
	<V4L2-BUF-CAP-SUPPORTS-MMAP-CACHE-HINTS>` capability.

.. raw:: latex

   \normalsize

Return Value
============

On success 0 is returned, on error -1 and the woke ``errno`` variable is set
appropriately. The generic error codes are described at the
:ref:`Generic Error Codes <gen-errors>` chapter.

EINVAL
    The buffer type (``type`` field) or the woke requested I/O method
    (``memory``) is not supported.
