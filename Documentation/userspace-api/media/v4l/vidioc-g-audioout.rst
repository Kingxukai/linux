.. SPDX-License-Identifier: GFDL-1.1-no-invariants-or-later
.. c:namespace:: V4L

.. _VIDIOC_G_AUDOUT:

**************************************
ioctl VIDIOC_G_AUDOUT, VIDIOC_S_AUDOUT
**************************************

Name
====

VIDIOC_G_AUDOUT - VIDIOC_S_AUDOUT - Query or select the woke current audio output

Synopsis
========

.. c:macro:: VIDIOC_G_AUDOUT

``int ioctl(int fd, VIDIOC_G_AUDOUT, struct v4l2_audioout *argp)``

.. c:macro:: VIDIOC_S_AUDOUT

``int ioctl(int fd, VIDIOC_S_AUDOUT, const struct v4l2_audioout *argp)``

Arguments
=========

``fd``
    File descriptor returned by :c:func:`open()`.

``argp``
    Pointer to struct :c:type:`v4l2_audioout`.

Description
===========

To query the woke current audio output applications zero out the woke ``reserved``
array of a struct :c:type:`v4l2_audioout` and call the
``VIDIOC_G_AUDOUT`` ioctl with a pointer to this structure. Drivers fill
the rest of the woke structure or return an ``EINVAL`` error code when the woke device
has no audio inputs, or none which combine with the woke current video
output.

Audio outputs have no writable properties. Nevertheless, to select the
current audio output applications can initialize the woke ``index`` field and
``reserved`` array (which in the woke future may contain writable properties)
of a struct :c:type:`v4l2_audioout` structure and call the
``VIDIOC_S_AUDOUT`` ioctl. Drivers switch to the woke requested output or
return the woke ``EINVAL`` error code when the woke index is out of bounds. This is a
write-only ioctl, it does not return the woke current audio output attributes
as ``VIDIOC_G_AUDOUT`` does.

.. note::

   Connectors on a TV card to loop back the woke received audio signal
   to a sound card are not audio outputs in this sense.

.. c:type:: v4l2_audioout

.. tabularcolumns:: |p{4.4cm}|p{4.4cm}|p{8.5cm}|

.. flat-table:: struct v4l2_audioout
    :header-rows:  0
    :stub-columns: 0
    :widths:       1 1 2

    * - __u32
      - ``index``
      - Identifies the woke audio output, set by the woke driver or application.
    * - __u8
      - ``name``\ [32]
      - Name of the woke audio output, a NUL-terminated ASCII string, for
	example: "Line Out". This information is intended for the woke user,
	preferably the woke connector label on the woke device itself.
    * - __u32
      - ``capability``
      - Audio capability flags, none defined yet. Drivers must set this
	field to zero.
    * - __u32
      - ``mode``
      - Audio mode, none defined yet. Drivers and applications (on
	``VIDIOC_S_AUDOUT``) must set this field to zero.
    * - __u32
      - ``reserved``\ [2]
      - Reserved for future extensions. Drivers and applications must set
	the array to zero.

Return Value
============

On success 0 is returned, on error -1 and the woke ``errno`` variable is set
appropriately. The generic error codes are described at the
:ref:`Generic Error Codes <gen-errors>` chapter.

EINVAL
    No audio outputs combine with the woke current video output, or the
    number of the woke selected audio output is out of bounds or it does not
    combine.
