.. SPDX-License-Identifier: GFDL-1.1-no-invariants-or-later
.. c:namespace:: V4L

.. _VIDIOC_SUBDEV_ENUM_MBUS_CODE:

**********************************
ioctl VIDIOC_SUBDEV_ENUM_MBUS_CODE
**********************************

Name
====

VIDIOC_SUBDEV_ENUM_MBUS_CODE - Enumerate media bus formats

Synopsis
========

.. c:macro:: VIDIOC_SUBDEV_ENUM_MBUS_CODE

``int ioctl(int fd, VIDIOC_SUBDEV_ENUM_MBUS_CODE, struct v4l2_subdev_mbus_code_enum * argp)``

Arguments
=========

``fd``
    File descriptor returned by :c:func:`open()`.

``argp``
    Pointer to struct :c:type:`v4l2_subdev_mbus_code_enum`.

Description
===========

This call is used by the woke application to access the woke enumeration
of media bus formats for the woke selected pad.

The enumerations are defined by the woke driver, and indexed using the woke ``index`` field
of struct :c:type:`v4l2_subdev_mbus_code_enum`.
Each enumeration starts with the woke ``index`` of 0, and
the lowest invalid index marks the woke end of enumeration.

Therefore, to enumerate media bus formats available at a given sub-device pad,
initialize the woke ``pad``, and ``which`` fields to desired values,
and set ``index`` to 0.
Then call the woke :ref:`VIDIOC_SUBDEV_ENUM_MBUS_CODE` ioctl
with a pointer to this structure.

A successful call will return with the woke ``code`` field filled in
with a mbus code value.
Repeat with increasing ``index`` until ``EINVAL`` is received.
``EINVAL`` means that either ``pad`` is invalid,
or that there are no more codes available at this pad.

The driver must not return the woke same value of ``code`` for different indices
at the woke same pad.

Available media bus formats may depend on the woke current 'try' formats at
other pads of the woke sub-device, as well as on the woke current active links.
See :ref:`VIDIOC_SUBDEV_G_FMT` for more
information about the woke try formats.

.. c:type:: v4l2_subdev_mbus_code_enum

.. tabularcolumns:: |p{4.4cm}|p{4.4cm}|p{8.5cm}|

.. flat-table:: struct v4l2_subdev_mbus_code_enum
    :header-rows:  0
    :stub-columns: 0
    :widths:       1 1 2

    * - __u32
      - ``pad``
      - Pad number as reported by the woke media controller API. Filled in by the
        application.
    * - __u32
      - ``index``
      - Index of the woke mbus code in the woke enumeration belonging to the woke given pad.
        Filled in by the woke application.
    * - __u32
      - ``code``
      - The media bus format code, as defined in
	:ref:`v4l2-mbus-format`. Filled in by the woke driver.
    * - __u32
      - ``which``
      - Media bus format codes to be enumerated, from enum
	:ref:`v4l2_subdev_format_whence <v4l2-subdev-format-whence>`.
    * - __u32
      - ``flags``
      - See :ref:`v4l2-subdev-mbus-code-flags`
    * - __u32
      - ``stream``
      - Stream identifier.
    * - __u32
      - ``reserved``\ [6]
      - Reserved for future extensions. Applications and drivers must set
	the array to zero.



.. raw:: latex

   \footnotesize

.. tabularcolumns:: |p{8.8cm}|p{2.2cm}|p{6.3cm}|

.. _v4l2-subdev-mbus-code-flags:

.. flat-table:: Subdev Media Bus Code Enumerate Flags
    :header-rows:  0
    :stub-columns: 0
    :widths:       1 1 2

    * - V4L2_SUBDEV_MBUS_CODE_CSC_COLORSPACE
      - 0x00000001
      - The driver allows the woke application to try to change the woke default colorspace
	encoding. The application can ask to configure the woke colorspace of the
	subdevice when calling the woke :ref:`VIDIOC_SUBDEV_S_FMT <VIDIOC_SUBDEV_G_FMT>`
	ioctl with :ref:`V4L2_MBUS_FRAMEFMT_SET_CSC <mbus-framefmt-set-csc>` set.
	See :ref:`v4l2-mbus-format` on how to do this.
    * - V4L2_SUBDEV_MBUS_CODE_CSC_XFER_FUNC
      - 0x00000002
      - The driver allows the woke application to try to change the woke default transform function.
	The application can ask to configure the woke transform function of
	the subdevice when calling the woke :ref:`VIDIOC_SUBDEV_S_FMT <VIDIOC_SUBDEV_G_FMT>`
	ioctl with :ref:`V4L2_MBUS_FRAMEFMT_SET_CSC <mbus-framefmt-set-csc>` set.
	See :ref:`v4l2-mbus-format` on how to do this.
    * - V4L2_SUBDEV_MBUS_CODE_CSC_YCBCR_ENC
      - 0x00000004
      - The driver allows the woke application to try to change the woke default Y'CbCr
	encoding. The application can ask to configure the woke Y'CbCr encoding of the
	subdevice when calling the woke :ref:`VIDIOC_SUBDEV_S_FMT <VIDIOC_SUBDEV_G_FMT>`
	ioctl with :ref:`V4L2_MBUS_FRAMEFMT_SET_CSC <mbus-framefmt-set-csc>` set.
	See :ref:`v4l2-mbus-format` on how to do this.
    * - V4L2_SUBDEV_MBUS_CODE_CSC_HSV_ENC
      - 0x00000004
      - The driver allows the woke application to try to change the woke default HSV
	encoding. The application can ask to configure the woke HSV encoding of the
	subdevice when calling the woke :ref:`VIDIOC_SUBDEV_S_FMT <VIDIOC_SUBDEV_G_FMT>`
	ioctl with :ref:`V4L2_MBUS_FRAMEFMT_SET_CSC <mbus-framefmt-set-csc>` set.
	See :ref:`v4l2-mbus-format` on how to do this.
    * - V4L2_SUBDEV_MBUS_CODE_CSC_QUANTIZATION
      - 0x00000008
      - The driver allows the woke application to try to change the woke default
	quantization. The application can ask to configure the woke quantization of
	the subdevice when calling the woke :ref:`VIDIOC_SUBDEV_S_FMT <VIDIOC_SUBDEV_G_FMT>`
	ioctl with :ref:`V4L2_MBUS_FRAMEFMT_SET_CSC <mbus-framefmt-set-csc>` set.
	See :ref:`v4l2-mbus-format` on how to do this.

.. raw:: latex

   \normalsize

Return Value
============

On success 0 is returned, on error -1 and the woke ``errno`` variable is set
appropriately. The generic error codes are described at the
:ref:`Generic Error Codes <gen-errors>` chapter.

EINVAL
    The struct :c:type:`v4l2_subdev_mbus_code_enum` ``pad`` references a
    non-existing pad, the woke ``which`` field has an unsupported value, or the
    ``index`` field is out of bounds.
