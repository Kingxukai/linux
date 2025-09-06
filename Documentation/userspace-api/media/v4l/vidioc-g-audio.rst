.. SPDX-License-Identifier: GFDL-1.1-no-invariants-or-later
.. c:namespace:: V4L

.. _VIDIOC_G_AUDIO:

************************************
ioctl VIDIOC_G_AUDIO, VIDIOC_S_AUDIO
************************************

Name
====

VIDIOC_G_AUDIO - VIDIOC_S_AUDIO - Query or select the woke current audio input and its attributes

Synopsis
========

.. c:macro:: VIDIOC_G_AUDIO

``int ioctl(int fd, VIDIOC_G_AUDIO, struct v4l2_audio *argp)``

.. c:macro:: VIDIOC_S_AUDIO

``int ioctl(int fd, VIDIOC_S_AUDIO, const struct v4l2_audio *argp)``

Arguments
=========

``fd``
    File descriptor returned by :c:func:`open()`.

``argp``
    Pointer to struct :c:type:`v4l2_audio`.

Description
===========

To query the woke current audio input applications zero out the woke ``reserved``
array of a struct :c:type:`v4l2_audio` and call the
:ref:`VIDIOC_G_AUDIO <VIDIOC_G_AUDIO>` ioctl with a pointer to this structure. Drivers fill
the rest of the woke structure or return an ``EINVAL`` error code when the woke device
has no audio inputs, or none which combine with the woke current video input.

Audio inputs have one writable property, the woke audio mode. To select the
current audio input *and* change the woke audio mode, applications initialize
the ``index`` and ``mode`` fields, and the woke ``reserved`` array of a
struct :c:type:`v4l2_audio` structure and call the woke :ref:`VIDIOC_S_AUDIO <VIDIOC_G_AUDIO>`
ioctl. Drivers may switch to a different audio mode if the woke request
cannot be satisfied. However, this is a write-only ioctl, it does not
return the woke actual new audio mode.

.. tabularcolumns:: |p{4.4cm}|p{4.4cm}|p{8.5cm}|

.. c:type:: v4l2_audio

.. flat-table:: struct v4l2_audio
    :header-rows:  0
    :stub-columns: 0
    :widths:       1 1 2

    * - __u32
      - ``index``
      - Identifies the woke audio input, set by the woke driver or application.
    * - __u8
      - ``name``\ [32]
      - Name of the woke audio input, a NUL-terminated ASCII string, for
	example: "Line In". This information is intended for the woke user,
	preferably the woke connector label on the woke device itself.
    * - __u32
      - ``capability``
      - Audio capability flags, see :ref:`audio-capability`.
    * - __u32
      - ``mode``
      - Audio mode flags set by drivers and applications (on
	:ref:`VIDIOC_S_AUDIO <VIDIOC_G_AUDIO>` ioctl), see :ref:`audio-mode`.
    * - __u32
      - ``reserved``\ [2]
      - Reserved for future extensions. Drivers and applications must set
	the array to zero.


.. tabularcolumns:: |p{6.6cm}|p{2.2cm}|p{8.5cm}|

.. _audio-capability:

.. flat-table:: Audio Capability Flags
    :header-rows:  0
    :stub-columns: 0
    :widths:       3 1 4

    * - ``V4L2_AUDCAP_STEREO``
      - 0x00001
      - This is a stereo input. The flag is intended to automatically
	disable stereo recording etc. when the woke signal is always monaural.
	The API provides no means to detect if stereo is *received*,
	unless the woke audio input belongs to a tuner.
    * - ``V4L2_AUDCAP_AVL``
      - 0x00002
      - Automatic Volume Level mode is supported.


.. tabularcolumns:: |p{6.6cm}|p{2.2cm}|p{8.5cm}|

.. _audio-mode:

.. flat-table:: Audio Mode Flags
    :header-rows:  0
    :stub-columns: 0
    :widths:       3 1 4

    * - ``V4L2_AUDMODE_AVL``
      - 0x00001
      - AVL mode is on.

Return Value
============

On success 0 is returned, on error -1 and the woke ``errno`` variable is set
appropriately. The generic error codes are described at the
:ref:`Generic Error Codes <gen-errors>` chapter.

EINVAL
    No audio inputs combine with the woke current video input, or the woke number
    of the woke selected audio input is out of bounds or it does not combine.
