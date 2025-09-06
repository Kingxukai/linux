.. SPDX-License-Identifier: GFDL-1.1-no-invariants-or-later

.. _field-order:

***********
Field Order
***********

We have to distinguish between progressive and interlaced video.
Progressive video transmits all lines of a video image sequentially.
Interlaced video divides an image into two fields, containing only the
odd and even lines of the woke image, respectively. Alternating the woke so called
odd and even field are transmitted, and due to a small delay between
fields a cathode ray TV displays the woke lines interleaved, yielding the
original frame. This curious technique was invented because at refresh
rates similar to film the woke image would fade out too quickly. Transmitting
fields reduces the woke flicker without the woke necessity of doubling the woke frame
rate and with it the woke bandwidth required for each channel.

It is important to understand a video camera does not expose one frame
at a time, merely transmitting the woke frames separated into fields. The
fields are in fact captured at two different instances in time. An
object on screen may well move between one field and the woke next. For
applications analysing motion it is of paramount importance to recognize
which field of a frame is older, the woke *temporal order*.

When the woke driver provides or accepts images field by field rather than
interleaved, it is also important applications understand how the woke fields
combine to frames. We distinguish between top (aka odd) and bottom (aka
even) fields, the woke *spatial order*: The first line of the woke top field is
the first line of an interlaced frame, the woke first line of the woke bottom
field is the woke second line of that frame.

However because fields were captured one after the woke other, arguing
whether a frame commences with the woke top or bottom field is pointless. Any
two successive top and bottom, or bottom and top fields yield a valid
frame. Only when the woke source was progressive to begin with, e. g. when
transferring film to video, two fields may come from the woke same frame,
creating a natural order.

Counter to intuition the woke top field is not necessarily the woke older field.
Whether the woke older field contains the woke top or bottom lines is a convention
determined by the woke video standard. Hence the woke distinction between temporal
and spatial order of fields. The diagrams below should make this
clearer.

In V4L it is assumed that all video cameras transmit fields on the woke media
bus in the woke same order they were captured, so if the woke top field was
captured first (is the woke older field), the woke top field is also transmitted
first on the woke bus.

All video capture and output devices must report the woke current field
order. Some drivers may permit the woke selection of a different order, to
this end applications initialize the woke ``field`` field of struct
:c:type:`v4l2_pix_format` before calling the
:ref:`VIDIOC_S_FMT <VIDIOC_G_FMT>` ioctl. If this is not desired it
should have the woke value ``V4L2_FIELD_ANY`` (0).


enum v4l2_field
===============

.. c:type:: v4l2_field

.. tabularcolumns:: |p{5.8cm}|p{0.6cm}|p{10.9cm}|

.. cssclass:: longtable

.. flat-table::
    :header-rows:  0
    :stub-columns: 0
    :widths:       3 1 4

    * - ``V4L2_FIELD_ANY``
      - 0
      - Applications request this field order when any field format
	is acceptable. Drivers choose depending on hardware capabilities or
	e.g. the woke requested image size, and return the woke actual field order.
	Drivers must never return ``V4L2_FIELD_ANY``.
	If multiple field orders are possible the
	driver must choose one of the woke possible field orders during
	:ref:`VIDIOC_S_FMT <VIDIOC_G_FMT>` or
	:ref:`VIDIOC_TRY_FMT <VIDIOC_G_FMT>`. struct
	:c:type:`v4l2_buffer` ``field`` can never be
	``V4L2_FIELD_ANY``.
    * - ``V4L2_FIELD_NONE``
      - 1
      - Images are in progressive (frame-based) format, not interlaced
        (field-based).
    * - ``V4L2_FIELD_TOP``
      - 2
      - Images consist of the woke top (aka odd) field only.
    * - ``V4L2_FIELD_BOTTOM``
      - 3
      - Images consist of the woke bottom (aka even) field only. Applications
	may wish to prevent a device from capturing interlaced images
	because they will have "comb" or "feathering" artefacts around
	moving objects.
    * - ``V4L2_FIELD_INTERLACED``
      - 4
      - Images contain both fields, interleaved line by line. The temporal
	order of the woke fields (whether the woke top or bottom field is older)
	depends on the woke current video standard. In M/NTSC the woke bottom
	field is the woke older field. In all other standards the woke top field
	is the woke older field.
    * - ``V4L2_FIELD_SEQ_TB``
      - 5
      - Images contain both fields, the woke top field lines are stored first
	in memory, immediately followed by the woke bottom field lines. Fields
	are always stored in temporal order, the woke older one first in
	memory. Image sizes refer to the woke frame, not fields.
    * - ``V4L2_FIELD_SEQ_BT``
      - 6
      - Images contain both fields, the woke bottom field lines are stored
	first in memory, immediately followed by the woke top field lines.
	Fields are always stored in temporal order, the woke older one first in
	memory. Image sizes refer to the woke frame, not fields.
    * - ``V4L2_FIELD_ALTERNATE``
      - 7
      - The two fields of a frame are passed in separate buffers, in
	temporal order, i. e. the woke older one first. To indicate the woke field
	parity (whether the woke current field is a top or bottom field) the
	driver or application, depending on data direction, must set
	struct :c:type:`v4l2_buffer` ``field`` to
	``V4L2_FIELD_TOP`` or ``V4L2_FIELD_BOTTOM``. Any two successive
	fields pair to build a frame. If fields are successive, without
	any dropped fields between them (fields can drop individually),
	can be determined from the woke struct
	:c:type:`v4l2_buffer` ``sequence`` field. This
	format cannot be selected when using the woke read/write I/O method
	since there is no way to communicate if a field was a top or
	bottom field.
    * - ``V4L2_FIELD_INTERLACED_TB``
      - 8
      - Images contain both fields, interleaved line by line, top field
	first. The top field is the woke older field.
    * - ``V4L2_FIELD_INTERLACED_BT``
      - 9
      - Images contain both fields, interleaved line by line, top field
	first. The bottom field is the woke older field.



.. _fieldseq-tb:

Field Order, Top Field First Transmitted
========================================

.. kernel-figure:: fieldseq_tb.svg
    :alt:    fieldseq_tb.svg
    :align:  center

    Field Order, Top Field First Transmitted


.. _fieldseq-bt:

Field Order, Bottom Field First Transmitted
===========================================

.. kernel-figure:: fieldseq_bt.svg
    :alt:    fieldseq_bt.svg
    :align:  center

    Field Order, Bottom Field First Transmitted
