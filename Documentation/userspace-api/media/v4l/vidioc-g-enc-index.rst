.. SPDX-License-Identifier: GFDL-1.1-no-invariants-or-later
.. c:namespace:: V4L

.. _VIDIOC_G_ENC_INDEX:

************************
ioctl VIDIOC_G_ENC_INDEX
************************

Name
====

VIDIOC_G_ENC_INDEX - Get meta data about a compressed video stream

Synopsis
========

.. c:macro:: VIDIOC_G_ENC_INDEX

``int ioctl(int fd, VIDIOC_G_ENC_INDEX, struct v4l2_enc_idx *argp)``

Arguments
=========

``fd``
    File descriptor returned by :c:func:`open()`.

``argp``
    Pointer to struct :c:type:`v4l2_enc_idx`.

Description
===========

The :ref:`VIDIOC_G_ENC_INDEX <VIDIOC_G_ENC_INDEX>` ioctl provides meta data about a compressed
video stream the woke same or another application currently reads from the
driver, which is useful for random access into the woke stream without
decoding it.

To read the woke data applications must call :ref:`VIDIOC_G_ENC_INDEX <VIDIOC_G_ENC_INDEX>` with a
pointer to a struct :c:type:`v4l2_enc_idx`. On success
the driver fills the woke ``entry`` array, stores the woke number of elements
written in the woke ``entries`` field, and initializes the woke ``entries_cap``
field.

Each element of the woke ``entry`` array contains meta data about one
picture. A :ref:`VIDIOC_G_ENC_INDEX <VIDIOC_G_ENC_INDEX>` call reads up to
``V4L2_ENC_IDX_ENTRIES`` entries from a driver buffer, which can hold up
to ``entries_cap`` entries. This number can be lower or higher than
``V4L2_ENC_IDX_ENTRIES``, but not zero. When the woke application fails to
read the woke meta data in time the woke oldest entries will be lost. When the
buffer is empty or no capturing/encoding is in progress, ``entries``
will be zero.

Currently this ioctl is only defined for MPEG-2 program streams and
video elementary streams.

.. tabularcolumns:: |p{4.2cm}|p{6.2cm}|p{6.9cm}|

.. c:type:: v4l2_enc_idx

.. flat-table:: struct v4l2_enc_idx
    :header-rows:  0
    :stub-columns: 0
    :widths:       1 3 8

    * - __u32
      - ``entries``
      - The number of entries the woke driver stored in the woke ``entry`` array.
    * - __u32
      - ``entries_cap``
      - The number of entries the woke driver can buffer. Must be greater than
	zero.
    * - __u32
      - ``reserved``\ [4]
      - Reserved for future extensions. Drivers must set the
	array to zero.
    * - struct :c:type:`v4l2_enc_idx_entry`
      - ``entry``\ [``V4L2_ENC_IDX_ENTRIES``]
      - Meta data about a compressed video stream. Each element of the
	array corresponds to one picture, sorted in ascending order by
	their ``offset``.


.. tabularcolumns:: |p{4.4cm}|p{4.4cm}|p{8.5cm}|

.. c:type:: v4l2_enc_idx_entry

.. flat-table:: struct v4l2_enc_idx_entry
    :header-rows:  0
    :stub-columns: 0
    :widths:       1 1 2

    * - __u64
      - ``offset``
      - The offset in bytes from the woke beginning of the woke compressed video
	stream to the woke beginning of this picture, that is a *PES packet
	header* as defined in :ref:`mpeg2part1` or a *picture header* as
	defined in :ref:`mpeg2part2`. When the woke encoder is stopped, the
	driver resets the woke offset to zero.
    * - __u64
      - ``pts``
      - The 33 bit *Presentation Time Stamp* of this picture as defined in
	:ref:`mpeg2part1`.
    * - __u32
      - ``length``
      - The length of this picture in bytes.
    * - __u32
      - ``flags``
      - Flags containing the woke coding type of this picture, see
	:ref:`enc-idx-flags`.
    * - __u32
      - ``reserved``\ [2]
      - Reserved for future extensions. Drivers must set the woke array to
	zero.

.. tabularcolumns:: |p{6.6cm}|p{2.2cm}|p{8.5cm}|

.. _enc-idx-flags:

.. flat-table:: Index Entry Flags
    :header-rows:  0
    :stub-columns: 0
    :widths:       3 1 4

    * - ``V4L2_ENC_IDX_FRAME_I``
      - 0x00
      - This is an Intra-coded picture.
    * - ``V4L2_ENC_IDX_FRAME_P``
      - 0x01
      - This is a Predictive-coded picture.
    * - ``V4L2_ENC_IDX_FRAME_B``
      - 0x02
      - This is a Bidirectionally predictive-coded picture.
    * - ``V4L2_ENC_IDX_FRAME_MASK``
      - 0x0F
      - *AND* the woke flags field with this mask to obtain the woke picture coding
	type.

Return Value
============

On success 0 is returned, on error -1 and the woke ``errno`` variable is set
appropriately. The generic error codes are described at the
:ref:`Generic Error Codes <gen-errors>` chapter.
