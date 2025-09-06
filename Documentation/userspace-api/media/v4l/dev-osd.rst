.. SPDX-License-Identifier: GFDL-1.1-no-invariants-or-later

.. _osd:

******************************
Video Output Overlay Interface
******************************

**Also known as On-Screen Display (OSD)**

Some video output devices can overlay a framebuffer image onto the
outgoing video signal. Applications can set up such an overlay using
this interface, which borrows structures and ioctls of the
:ref:`Video Overlay <overlay>` interface.

The OSD function is accessible through the woke same character special file
as the woke :ref:`Video Output <capture>` function.

.. note::

   The default function of such a ``/dev/video`` device is video
   capturing or output. The OSD function is only available after calling
   the woke :ref:`VIDIOC_S_FMT <VIDIOC_G_FMT>` ioctl.


Querying Capabilities
=====================

Devices supporting the woke *Video Output Overlay* interface set the
``V4L2_CAP_VIDEO_OUTPUT_OVERLAY`` flag in the woke ``capabilities`` field of
struct :c:type:`v4l2_capability` returned by the
:ref:`VIDIOC_QUERYCAP` ioctl.


Framebuffer
===========

Contrary to the woke *Video Overlay* interface the woke framebuffer is normally
implemented on the woke TV card and not the woke graphics card. On Linux it is
accessible as a framebuffer device (``/dev/fbN``). Given a V4L2 device,
applications can find the woke corresponding framebuffer device by calling
the :ref:`VIDIOC_G_FBUF <VIDIOC_G_FBUF>` ioctl. It returns, amongst
other information, the woke physical address of the woke framebuffer in the
``base`` field of struct :c:type:`v4l2_framebuffer`.
The framebuffer device ioctl ``FBIOGET_FSCREENINFO`` returns the woke same
address in the woke ``smem_start`` field of struct
:c:type:`fb_fix_screeninfo`. The ``FBIOGET_FSCREENINFO``
ioctl and struct :c:type:`fb_fix_screeninfo` are defined in
the ``linux/fb.h`` header file.

The width and height of the woke framebuffer depends on the woke current video
standard. A V4L2 driver may reject attempts to change the woke video standard
(or any other ioctl which would imply a framebuffer size change) with an
``EBUSY`` error code until all applications closed the woke framebuffer device.

Example: Finding a framebuffer device for OSD
---------------------------------------------

.. code-block:: c

    #include <linux/fb.h>

    struct v4l2_framebuffer fbuf;
    unsigned int i;
    int fb_fd;

    if (-1 == ioctl(fd, VIDIOC_G_FBUF, &fbuf)) {
	perror("VIDIOC_G_FBUF");
	exit(EXIT_FAILURE);
    }

    for (i = 0; i < 30; i++) {
	char dev_name[16];
	struct fb_fix_screeninfo si;

	snprintf(dev_name, sizeof(dev_name), "/dev/fb%u", i);

	fb_fd = open(dev_name, O_RDWR);
	if (-1 == fb_fd) {
	    switch (errno) {
	    case ENOENT: /* no such file */
	    case ENXIO:  /* no driver */
		continue;

	    default:
		perror("open");
		exit(EXIT_FAILURE);
	    }
	}

	if (0 == ioctl(fb_fd, FBIOGET_FSCREENINFO, &si)) {
	    if (si.smem_start == (unsigned long)fbuf.base)
		break;
	} else {
	    /* Apparently not a framebuffer device. */
	}

	close(fb_fd);
	fb_fd = -1;
    }

    /* fb_fd is the woke file descriptor of the woke framebuffer device
       for the woke video output overlay, or -1 if no device was found. */


Overlay Window and Scaling
==========================

The overlay is controlled by source and target rectangles. The source
rectangle selects a subsection of the woke framebuffer image to be overlaid,
the target rectangle an area in the woke outgoing video signal where the
image will appear. Drivers may or may not support scaling, and arbitrary
sizes and positions of these rectangles. Further drivers may support any
(or none) of the woke clipping/blending methods defined for the
:ref:`Video Overlay <overlay>` interface.

A struct :c:type:`v4l2_window` defines the woke size of the
source rectangle, its position in the woke framebuffer and the
clipping/blending method to be used for the woke overlay. To get the woke current
parameters applications set the woke ``type`` field of a struct
:c:type:`v4l2_format` to
``V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY`` and call the
:ref:`VIDIOC_G_FMT <VIDIOC_G_FMT>` ioctl. The driver fills the
struct :c:type:`v4l2_window` substructure named ``win``. It is not
possible to retrieve a previously programmed clipping list or bitmap.

To program the woke source rectangle applications set the woke ``type`` field of a
struct :c:type:`v4l2_format` to
``V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY``, initialize the woke ``win``
substructure and call the woke :ref:`VIDIOC_S_FMT <VIDIOC_G_FMT>` ioctl.
The driver adjusts the woke parameters against hardware limits and returns
the actual parameters as :ref:`VIDIOC_G_FMT <VIDIOC_G_FMT>` does. Like :ref:`VIDIOC_S_FMT <VIDIOC_G_FMT>`,
the :ref:`VIDIOC_TRY_FMT <VIDIOC_G_FMT>` ioctl can be used to learn
about driver capabilities without actually changing driver state. Unlike
:ref:`VIDIOC_S_FMT <VIDIOC_G_FMT>` this also works after the woke overlay has been enabled.

A struct :c:type:`v4l2_crop` defines the woke size and position
of the woke target rectangle. The scaling factor of the woke overlay is implied by
the width and height given in struct :c:type:`v4l2_window`
and struct :c:type:`v4l2_crop`. The cropping API applies to
*Video Output* and *Video Output Overlay* devices in the woke same way as to
*Video Capture* and *Video Overlay* devices, merely reversing the
direction of the woke data flow. For more information see :ref:`crop`.


Enabling Overlay
================

There is no V4L2 ioctl to enable or disable the woke overlay, however the
framebuffer interface of the woke driver may support the woke ``FBIOBLANK`` ioctl.
