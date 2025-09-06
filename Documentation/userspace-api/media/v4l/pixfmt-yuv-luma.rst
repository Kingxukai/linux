.. SPDX-License-Identifier: GFDL-1.1-no-invariants-or-later

.. _yuv-luma-only:

*****************
Luma-Only Formats
*****************

This family of formats only store the woke luma component of a Y'CbCr image. They
are often referred to as greyscale formats.

.. note::

   - In all the woke tables that follow, bit 7 is the woke most significant bit in a byte.
   - Formats are described with the woke minimum number of pixels needed to create a
     byte-aligned repeating pattern. `...` indicates repetition of the woke pattern.
   - Y'\ :sub:`x`\ [9:2] denotes bits 9 to 2 of the woke Y' value for pixel at column
     `x`.
   - `0` denotes padding bits set to 0.


.. raw:: latex

    \tiny

.. tabularcolumns:: |p{3.6cm}|p{2.4cm}|p{1.3cm}|p{1.3cm}|p{1.3cm}|p{1.3cm}|p{1.3cm}|p{1.3cm}|p{1.3cm}|

.. flat-table:: Luma-Only Image Formats
    :header-rows: 1
    :stub-columns: 0

    * - Identifier
      - Code
      - Byte 0
      - Byte 1
      - Byte 2
      - Byte 3
      - Byte 4
      - Byte 5
      - Byte 6

    * .. _V4L2-PIX-FMT-GREY:

      - ``V4L2_PIX_FMT_GREY``
      - 'GREY'

      - Y'\ :sub:`0`\ [7:0]
      - ...
      - ...
      - ...
      - ...
      - ...
      - ...

    * .. _V4L2-PIX-FMT-IPU3-Y10:

      - ``V4L2_PIX_FMT_IPU3_Y10``
      - 'ip3y'

      - Y'\ :sub:`0`\ [7:0]
      - Y'\ :sub:`1`\ [5:0] Y'\ :sub:`0`\ [9:8]
      - Y'\ :sub:`2`\ [3:0] Y'\ :sub:`1`\ [9:6]
      - Y'\ :sub:`3`\ [1:0] Y'\ :sub:`2`\ [9:4]
      - Y'\ :sub:`3`\ [9:2]
      - ...
      - ...

    * .. _V4L2-PIX-FMT-Y10:

      - ``V4L2_PIX_FMT_Y10``
      - 'Y10 '

      - Y'\ :sub:`0`\ [7:0]
      - `000000` Y'\ :sub:`0`\ [9:8]
      - ...
      - ...
      - ...
      - ...
      - ...

    * .. _V4L2-PIX-FMT-Y10BPACK:

      - ``V4L2_PIX_FMT_Y10BPACK``
      - 'Y10B'

      - Y'\ :sub:`0`\ [9:2]
      - Y'\ :sub:`0`\ [1:0] Y'\ :sub:`1`\ [9:4]
      - Y'\ :sub:`1`\ [3:0] Y'\ :sub:`2`\ [9:6]
      - Y'\ :sub:`2`\ [5:0] Y'\ :sub:`3`\ [9:8]
      - Y'\ :sub:`3`\ [7:0]
      - ...
      - ...

    * .. _V4L2-PIX-FMT-Y10P:

      - ``V4L2_PIX_FMT_Y10P``
      - 'Y10P'

      - Y'\ :sub:`0`\ [9:2]
      - Y'\ :sub:`1`\ [9:2]
      - Y'\ :sub:`2`\ [9:2]
      - Y'\ :sub:`3`\ [9:2]
      - Y'\ :sub:`3`\ [1:0] Y'\ :sub:`2`\ [1:0] Y'\ :sub:`1`\ [1:0] Y'\ :sub:`0`\ [1:0]
      - ...
      - ...

    * .. _V4L2-PIX-FMT-Y12:

      - ``V4L2_PIX_FMT_Y12``
      - 'Y12 '

      - Y'\ :sub:`0`\ [7:0]
      - `0000` Y'\ :sub:`0`\ [11:8]
      - ...
      - ...
      - ...
      - ...
      - ...

    * .. _V4L2-PIX-FMT-Y012:

      - ``V4L2_PIX_FMT_Y012``
      - 'Y012'

      - Y'\ :sub:`0`\ [3:0] `0000`
      - Y'\ :sub:`0`\ [11:4]
      - ...
      - ...
      - ...
      - ...
      - ...

    * .. _V4L2-PIX-FMT-Y12P:

      - ``V4L2_PIX_FMT_Y12P``
      - 'Y12P'

      - Y'\ :sub:`0`\ [11:4]
      - Y'\ :sub:`1`\ [11:4]
      - Y'\ :sub:`1`\ [3:0] Y'\ :sub:`0`\ [3:0]
      - ...
      - ...
      - ...
      - ...

    * .. _V4L2-PIX-FMT-Y14:

      - ``V4L2_PIX_FMT_Y14``
      - 'Y14 '

      - Y'\ :sub:`0`\ [7:0]
      - `00` Y'\ :sub:`0`\ [13:8]
      - ...
      - ...
      - ...
      - ...
      - ...

    * .. _V4L2-PIX-FMT-Y14P:

      - ``V4L2_PIX_FMT_Y14P``
      - 'Y14P'

      - Y'\ :sub:`0`\ [13:6]
      - Y'\ :sub:`1`\ [13:6]
      - Y'\ :sub:`2`\ [13:6]
      - Y'\ :sub:`3`\ [13:6]
      - Y'\ :sub:`1`\ [1:0] Y'\ :sub:`0`\ [5:0]
      - Y'\ :sub:`2`\ [3:0] Y'\ :sub:`1`\ [5:2]
      - Y'\ :sub:`3`\ [5:0] Y'\ :sub:`2`\ [5:4]

    * .. _V4L2-PIX-FMT-Y16:

      - ``V4L2_PIX_FMT_Y16``
      - 'Y16 '

      - Y'\ :sub:`0`\ [7:0]
      - Y'\ :sub:`0`\ [15:8]
      - ...
      - ...
      - ...
      - ...
      - ...

    * .. _V4L2-PIX-FMT-Y16-BE:

      - ``V4L2_PIX_FMT_Y16_BE``
      - 'Y16 ' | (1U << 31)

      - Y'\ :sub:`0`\ [15:8]
      - Y'\ :sub:`0`\ [7:0]
      - ...
      - ...
      - ...
      - ...
      - ...

.. raw:: latex

    \normalsize

.. note::

    For the woke Y16 and Y16_BE formats, the woke actual sampling precision may be lower
    than 16 bits. For example, 10 bits per pixel uses values in the woke range 0 to
    1023. For the woke IPU3_Y10 format 25 pixels are packed into 32 bytes, which
    leaves the woke 6 most significant bits of the woke last byte padded with 0.

    For Y012 and Y12 formats, Y012 places its data in the woke 12 high bits, with
    padding zeros in the woke 4 low bits, in contrast to the woke Y12 format, which has
    its padding located in the woke most significant bits of the woke 16 bit word.

    The 'P' variations of the woke Y10, Y12 and Y14 formats are packed according to
    the woke RAW10, RAW12 and RAW14 packing scheme as defined by the woke MIPI CSI-2
    specification.
