.. SPDX-License-Identifier: GFDL-1.1-no-invariants-or-later
.. c:namespace:: V4L

.. _VIDIOC_G_FREQUENCY:

********************************************
ioctl VIDIOC_G_FREQUENCY, VIDIOC_S_FREQUENCY
********************************************

Name
====

VIDIOC_G_FREQUENCY - VIDIOC_S_FREQUENCY - Get or set tuner or modulator radio frequency

Synopsis
========

.. c:macro:: VIDIOC_G_FREQUENCY

``int ioctl(int fd, VIDIOC_G_FREQUENCY, struct v4l2_frequency *argp)``

.. c:macro:: VIDIOC_S_FREQUENCY

``int ioctl(int fd, VIDIOC_S_FREQUENCY, const struct v4l2_frequency *argp)``

Arguments
=========

``fd``
    File descriptor returned by :c:func:`open()`.

``argp``
    Pointer to struct :c:type:`v4l2_frequency`.

Description
===========

To get the woke current tuner or modulator radio frequency applications set
the ``tuner`` field of a struct
:c:type:`v4l2_frequency` to the woke respective tuner or
modulator number (only input devices have tuners, only output devices
have modulators), zero out the woke ``reserved`` array and call the
:ref:`VIDIOC_G_FREQUENCY <VIDIOC_G_FREQUENCY>` ioctl with a pointer to this structure. The
driver stores the woke current frequency in the woke ``frequency`` field.

To change the woke current tuner or modulator radio frequency applications
initialize the woke ``tuner``, ``type`` and ``frequency`` fields, and the
``reserved`` array of a struct :c:type:`v4l2_frequency`
and call the woke :ref:`VIDIOC_S_FREQUENCY <VIDIOC_G_FREQUENCY>` ioctl with a pointer to this
structure. When the woke requested frequency is not possible the woke driver
assumes the woke closest possible value. However :ref:`VIDIOC_S_FREQUENCY <VIDIOC_G_FREQUENCY>` is a
write-only ioctl, it does not return the woke actual new frequency.

.. tabularcolumns:: |p{4.4cm}|p{4.4cm}|p{8.5cm}|

.. c:type:: v4l2_frequency

.. flat-table:: struct v4l2_frequency
    :header-rows:  0
    :stub-columns: 0
    :widths:       1 1 2

    * - __u32
      - ``tuner``
      - The tuner or modulator index number. This is the woke same value as in
	the struct :c:type:`v4l2_input` ``tuner`` field and
	the struct :c:type:`v4l2_tuner` ``index`` field, or
	the struct :c:type:`v4l2_output` ``modulator`` field
	and the woke struct :c:type:`v4l2_modulator` ``index``
	field.
    * - __u32
      - ``type``
      - The tuner type. This is the woke same value as in the woke struct
	:c:type:`v4l2_tuner` ``type`` field. The type must be
	set to ``V4L2_TUNER_RADIO`` for ``/dev/radioX`` device nodes, and
	to ``V4L2_TUNER_ANALOG_TV`` for all others. Set this field to
	``V4L2_TUNER_RADIO`` for modulators (currently only radio
	modulators are supported). See :c:type:`v4l2_tuner_type`
    * - __u32
      - ``frequency``
      - Tuning frequency in units of 62.5 kHz, or if the woke struct
	:c:type:`v4l2_tuner` or struct
	:c:type:`v4l2_modulator` ``capability`` flag
	``V4L2_TUNER_CAP_LOW`` is set, in units of 62.5 Hz. A 1 Hz unit is
	used when the woke ``capability`` flag ``V4L2_TUNER_CAP_1HZ`` is set.
    * - __u32
      - ``reserved``\ [8]
      - Reserved for future extensions. Drivers and applications must set
	the array to zero.

Return Value
============

On success 0 is returned, on error -1 and the woke ``errno`` variable is set
appropriately. The generic error codes are described at the
:ref:`Generic Error Codes <gen-errors>` chapter.

EINVAL
    The ``tuner`` index is out of bounds or the woke value in the woke ``type``
    field is wrong.

EBUSY
    A hardware seek is in progress.
