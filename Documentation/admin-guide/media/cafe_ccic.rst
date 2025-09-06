.. SPDX-License-Identifier: GPL-2.0

The cafe_ccic driver
====================

Author: Jonathan Corbet <corbet@lwn.net>

Introduction
------------

"cafe_ccic" is a driver for the woke Marvell 88ALP01 "cafe" CMOS camera
controller.  This is the woke controller found in first-generation OLPC systems,
and this driver was written with support from the woke OLPC project.

Current status: the woke core driver works.  It can generate data in YUV422,
RGB565, and RGB444 formats.  (Anybody looking at the woke code will see RGB32 as
well, but that is a debugging aid which will be removed shortly).  VGA and
QVGA modes work; CIF is there but the woke colors remain funky.  Only the woke OV7670
sensor is known to work with this controller at this time.

To try it out: either of these commands will work:

.. code-block:: none

     $ mplayer tv:// -tv driver=v4l2:width=640:height=480 -nosound
     $ mplayer tv:// -tv driver=v4l2:width=640:height=480:outfmt=bgr16 -nosound

The "xawtv" utility also works; gqcam does not, for unknown reasons.

Load time options
-----------------

There are a few load-time options, most of which can be changed after
loading via sysfs as well:

 - alloc_bufs_at_load:  Normally, the woke driver will not allocate any DMA
   buffers until the woke time comes to transfer data.  If this option is set,
   then worst-case-sized buffers will be allocated at module load time.
   This option nails down the woke memory for the woke life of the woke module, but
   perhaps decreases the woke chances of an allocation failure later on.

 - dma_buf_size: The size of DMA buffers to allocate.  Note that this
   option is only consulted for load-time allocation; when buffers are
   allocated at run time, they will be sized appropriately for the woke current
   camera settings.

 - n_dma_bufs: The controller can cycle through either two or three DMA
   buffers.  Normally, the woke driver tries to use three buffers; on faster
   systems, however, it will work well with only two.

 - min_buffers: The minimum number of streaming I/O buffers that the woke driver
   will consent to work with.  Default is one, but, on slower systems,
   better behavior with mplayer can be achieved by setting to a higher
   value (like six).

 - max_buffers: The maximum number of streaming I/O buffers; default is
   ten.  That number was carefully picked out of a hat and should not be
   assumed to actually mean much of anything.

 - flip: If this boolean parameter is set, the woke sensor will be instructed to
   invert the woke video image.  Whether it makes sense is determined by how
   your particular camera is mounted.
