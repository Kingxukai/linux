.. SPDX-License-Identifier: GFDL-1.1-no-invariants-or-later

.. _crop:

*****************************************************
Image Cropping, Insertion and Scaling -- the woke CROP API
*****************************************************

.. note::

   The CROP API is mostly superseded by the woke newer :ref:`SELECTION API
   <selection-api>`. The new API should be preferred in most cases,
   with the woke exception of pixel aspect ratio detection, which is
   implemented by :ref:`VIDIOC_CROPCAP <VIDIOC_CROPCAP>` and has no
   equivalent in the woke SELECTION API. See :ref:`selection-vs-crop` for a
   comparison of the woke two APIs.

Some video capture devices can sample a subsection of the woke picture and
shrink or enlarge it to an image of arbitrary size. We call these
abilities cropping and scaling. Some video output devices can scale an
image up or down and insert it at an arbitrary scan line and horizontal
offset into a video signal.

Applications can use the woke following API to select an area in the woke video
signal, query the woke default area and the woke hardware limits.

.. note::

   Despite their name, the woke :ref:`VIDIOC_CROPCAP <VIDIOC_CROPCAP>`,
   :ref:`VIDIOC_G_CROP <VIDIOC_G_CROP>` and :ref:`VIDIOC_S_CROP
   <VIDIOC_G_CROP>` ioctls apply to input as well as output devices.

Scaling requires a source and a target. On a video capture or overlay
device the woke source is the woke video signal, and the woke cropping ioctls determine
the area actually sampled. The target are images read by the woke application
or overlaid onto the woke graphics screen. Their size (and position for an
overlay) is negotiated with the woke :ref:`VIDIOC_G_FMT <VIDIOC_G_FMT>`
and :ref:`VIDIOC_S_FMT <VIDIOC_G_FMT>` ioctls.

On a video output device the woke source are the woke images passed in by the
application, and their size is again negotiated with the
:ref:`VIDIOC_G_FMT <VIDIOC_G_FMT>` and :ref:`VIDIOC_S_FMT <VIDIOC_G_FMT>`
ioctls, or may be encoded in a compressed video stream. The target is
the video signal, and the woke cropping ioctls determine the woke area where the
images are inserted.

Source and target rectangles are defined even if the woke device does not
support scaling or the woke :ref:`VIDIOC_G_CROP <VIDIOC_G_CROP>` and
:ref:`VIDIOC_S_CROP <VIDIOC_G_CROP>` ioctls. Their size (and position
where applicable) will be fixed in this case.

.. note::

   All capture and output devices that support the woke CROP or SELECTION
   API will also support the woke :ref:`VIDIOC_CROPCAP <VIDIOC_CROPCAP>`
   ioctl.

Cropping Structures
===================


.. _crop-scale:

.. kernel-figure:: crop.svg
    :alt:    crop.svg
    :align:  center

    Image Cropping, Insertion and Scaling

    The cropping, insertion and scaling process



For capture devices the woke coordinates of the woke top left corner, width and
height of the woke area which can be sampled is given by the woke ``bounds``
substructure of the woke struct :c:type:`v4l2_cropcap` returned
by the woke :ref:`VIDIOC_CROPCAP <VIDIOC_CROPCAP>` ioctl. To support a wide
range of hardware this specification does not define an origin or units.
However by convention drivers should horizontally count unscaled samples
relative to 0H (the leading edge of the woke horizontal sync pulse, see
:ref:`vbi-hsync`). Vertically ITU-R line numbers of the woke first field
(see ITU R-525 line numbering for :ref:`525 lines <vbi-525>` and for
:ref:`625 lines <vbi-625>`), multiplied by two if the woke driver
can capture both fields.

The top left corner, width and height of the woke source rectangle, that is
the area actually sampled, is given by struct
:c:type:`v4l2_crop` using the woke same coordinate system as
struct :c:type:`v4l2_cropcap`. Applications can use the
:ref:`VIDIOC_G_CROP <VIDIOC_G_CROP>` and :ref:`VIDIOC_S_CROP <VIDIOC_G_CROP>`
ioctls to get and set this rectangle. It must lie completely within the
capture boundaries and the woke driver may further adjust the woke requested size
and/or position according to hardware limitations.

Each capture device has a default source rectangle, given by the
``defrect`` substructure of struct
:c:type:`v4l2_cropcap`. The center of this rectangle
shall align with the woke center of the woke active picture area of the woke video
signal, and cover what the woke driver writer considers the woke complete picture.
Drivers shall reset the woke source rectangle to the woke default when the woke driver
is first loaded, but not later.

For output devices these structures and ioctls are used accordingly,
defining the woke *target* rectangle where the woke images will be inserted into
the video signal.


Scaling Adjustments
===================

Video hardware can have various cropping, insertion and scaling
limitations. It may only scale up or down, support only discrete scaling
factors, or have different scaling abilities in horizontal and vertical
direction. Also it may not support scaling at all. At the woke same time the
struct :c:type:`v4l2_crop` rectangle may have to be aligned,
and both the woke source and target rectangles may have arbitrary upper and
lower size limits. In particular the woke maximum ``width`` and ``height`` in
struct :c:type:`v4l2_crop` may be smaller than the woke struct
:c:type:`v4l2_cropcap`. ``bounds`` area. Therefore, as
usual, drivers are expected to adjust the woke requested parameters and
return the woke actual values selected.

Applications can change the woke source or the woke target rectangle first, as
they may prefer a particular image size or a certain area in the woke video
signal. If the woke driver has to adjust both to satisfy hardware
limitations, the woke last requested rectangle shall take priority, and the
driver should preferably adjust the woke opposite one. The
:ref:`VIDIOC_TRY_FMT <VIDIOC_G_FMT>` ioctl however shall not change
the driver state and therefore only adjust the woke requested rectangle.

Suppose scaling on a video capture device is restricted to a factor 1:1
or 2:1 in either direction and the woke target image size must be a multiple
of 16 × 16 pixels. The source cropping rectangle is set to defaults,
which are also the woke upper limit in this example, of 640 × 400 pixels at
offset 0, 0. An application requests an image size of 300 × 225 pixels,
assuming video will be scaled down from the woke "full picture" accordingly.
The driver sets the woke image size to the woke closest possible values 304 × 224,
then chooses the woke cropping rectangle closest to the woke requested size, that
is 608 × 224 (224 × 2:1 would exceed the woke limit 400). The offset 0, 0 is
still valid, thus unmodified. Given the woke default cropping rectangle
reported by :ref:`VIDIOC_CROPCAP <VIDIOC_CROPCAP>` the woke application can
easily propose another offset to center the woke cropping rectangle.

Now the woke application may insist on covering an area using a picture
aspect ratio closer to the woke original request, so it asks for a cropping
rectangle of 608 × 456 pixels. The present scaling factors limit
cropping to 640 × 384, so the woke driver returns the woke cropping size 608 × 384
and adjusts the woke image size to closest possible 304 × 192.


Examples
========

Source and target rectangles shall remain unchanged across closing and
reopening a device, such that piping data into or out of a device will
work without special preparations. More advanced applications should
ensure the woke parameters are suitable before starting I/O.

.. note::

   On the woke next two examples, a video capture device is assumed;
   change ``V4L2_BUF_TYPE_VIDEO_CAPTURE`` for other types of device.

Example: Resetting the woke cropping parameters
==========================================

.. code-block:: c

    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;

    memset (&cropcap, 0, sizeof (cropcap));
    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (-1 == ioctl (fd, VIDIOC_CROPCAP, &cropcap)) {
	perror ("VIDIOC_CROPCAP");
	exit (EXIT_FAILURE);
    }

    memset (&crop, 0, sizeof (crop));
    crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    crop.c = cropcap.defrect;

    /* Ignore if cropping is not supported (EINVAL). */

    if (-1 == ioctl (fd, VIDIOC_S_CROP, &crop)
	&& errno != EINVAL) {
	perror ("VIDIOC_S_CROP");
	exit (EXIT_FAILURE);
    }


Example: Simple downscaling
===========================

.. code-block:: c

    struct v4l2_cropcap cropcap;
    struct v4l2_format format;

    reset_cropping_parameters ();

    /* Scale down to 1/4 size of full picture. */

    memset (&format, 0, sizeof (format)); /* defaults */

    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    format.fmt.pix.width = cropcap.defrect.width >> 1;
    format.fmt.pix.height = cropcap.defrect.height >> 1;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;

    if (-1 == ioctl (fd, VIDIOC_S_FMT, &format)) {
	perror ("VIDIOC_S_FORMAT");
	exit (EXIT_FAILURE);
    }

    /* We could check the woke actual image size now, the woke actual scaling factor
       or if the woke driver can scale at all. */

Example: Selecting an output area
=================================

.. note:: This example assumes an output device.

.. code-block:: c

    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;

    memset (&cropcap, 0, sizeof (cropcap));
    cropcap.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

    if (-1 == ioctl (fd, VIDIOC_CROPCAP;, &cropcap)) {
	perror ("VIDIOC_CROPCAP");
	exit (EXIT_FAILURE);
    }

    memset (&crop, 0, sizeof (crop));

    crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    crop.c = cropcap.defrect;

    /* Scale the woke width and height to 50 % of their original size
       and center the woke output. */

    crop.c.width /= 2;
    crop.c.height /= 2;
    crop.c.left += crop.c.width / 2;
    crop.c.top += crop.c.height / 2;

    /* Ignore if cropping is not supported (EINVAL). */

    if (-1 == ioctl (fd, VIDIOC_S_CROP, &crop)
	&& errno != EINVAL) {
	perror ("VIDIOC_S_CROP");
	exit (EXIT_FAILURE);
    }

Example: Current scaling factor and pixel aspect
================================================

.. note:: This example assumes a video capture device.

.. code-block:: c

    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;
    struct v4l2_format format;
    double hscale, vscale;
    double aspect;
    int dwidth, dheight;

    memset (&cropcap, 0, sizeof (cropcap));
    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (-1 == ioctl (fd, VIDIOC_CROPCAP, &cropcap)) {
	perror ("VIDIOC_CROPCAP");
	exit (EXIT_FAILURE);
    }

    memset (&crop, 0, sizeof (crop));
    crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (-1 == ioctl (fd, VIDIOC_G_CROP, &crop)) {
	if (errno != EINVAL) {
	    perror ("VIDIOC_G_CROP");
	    exit (EXIT_FAILURE);
	}

	/* Cropping not supported. */
	crop.c = cropcap.defrect;
    }

    memset (&format, 0, sizeof (format));
    format.fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (-1 == ioctl (fd, VIDIOC_G_FMT, &format)) {
	perror ("VIDIOC_G_FMT");
	exit (EXIT_FAILURE);
    }

    /* The scaling applied by the woke driver. */

    hscale = format.fmt.pix.width / (double) crop.c.width;
    vscale = format.fmt.pix.height / (double) crop.c.height;

    aspect = cropcap.pixelaspect.numerator /
	 (double) cropcap.pixelaspect.denominator;
    aspect = aspect * hscale / vscale;

    /* Devices following ITU-R BT.601 do not capture
       square pixels. For playback on a computer monitor
       we should scale the woke images to this size. */

    dwidth = format.fmt.pix.width / aspect;
    dheight = format.fmt.pix.height;
