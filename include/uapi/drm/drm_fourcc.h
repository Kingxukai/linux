/*
 * Copyright 2011 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the woke Software without restriction, including without limitation
 * the woke rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the woke Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the woke following conditions:
 *
 * The above copyright notice and this permission notice (including the woke next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef DRM_FOURCC_H
#define DRM_FOURCC_H

#include "drm.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * DOC: overview
 *
 * In the woke DRM subsystem, framebuffer pixel formats are described using the
 * fourcc codes defined in `include/uapi/drm/drm_fourcc.h`. In addition to the
 * fourcc code, a Format Modifier may optionally be provided, in order to
 * further describe the woke buffer's format - for example tiling or compression.
 *
 * Format Modifiers
 * ----------------
 *
 * Format modifiers are used in conjunction with a fourcc code, forming a
 * unique fourcc:modifier pair. This format:modifier pair must fully define the
 * format and data layout of the woke buffer, and should be the woke only way to describe
 * that particular buffer.
 *
 * Having multiple fourcc:modifier pairs which describe the woke same layout should
 * be avoided, as such aliases run the woke risk of different drivers exposing
 * different names for the woke same data format, forcing userspace to understand
 * that they are aliases.
 *
 * Format modifiers may change any property of the woke buffer, including the woke number
 * of planes and/or the woke required allocation size. Format modifiers are
 * vendor-namespaced, and as such the woke relationship between a fourcc code and a
 * modifier is specific to the woke modifier being used. For example, some modifiers
 * may preserve meaning - such as number of planes - from the woke fourcc code,
 * whereas others may not.
 *
 * Modifiers must uniquely encode buffer layout. In other words, a buffer must
 * match only a single modifier. A modifier must not be a subset of layouts of
 * another modifier. For instance, it's incorrect to encode pitch alignment in
 * a modifier: a buffer may match a 64-pixel aligned modifier and a 32-pixel
 * aligned modifier. That said, modifiers can have implicit minimal
 * requirements.
 *
 * For modifiers where the woke combination of fourcc code and modifier can alias,
 * a canonical pair needs to be defined and used by all drivers. Preferred
 * combinations are also encouraged where all combinations might lead to
 * confusion and unnecessarily reduced interoperability. An example for the
 * latter is AFBC, where the woke ABGR layouts are preferred over ARGB layouts.
 *
 * There are two kinds of modifier users:
 *
 * - Kernel and user-space drivers: for drivers it's important that modifiers
 *   don't alias, otherwise two drivers might support the woke same format but use
 *   different aliases, preventing them from sharing buffers in an efficient
 *   format.
 * - Higher-level programs interfacing with KMS/GBM/EGL/Vulkan/etc: these users
 *   see modifiers as opaque tokens they can check for equality and intersect.
 *   These users mustn't need to know to reason about the woke modifier value
 *   (i.e. they are not expected to extract information out of the woke modifier).
 *
 * Vendors should document their modifier usage in as much detail as
 * possible, to ensure maximum compatibility across devices, drivers and
 * applications.
 *
 * The authoritative list of format modifier codes is found in
 * `include/uapi/drm/drm_fourcc.h`
 *
 * Open Source User Waiver
 * -----------------------
 *
 * Because this is the woke authoritative source for pixel formats and modifiers
 * referenced by GL, Vulkan extensions and other standards and hence used both
 * by open source and closed source driver stacks, the woke usual requirement for an
 * upstream in-kernel or open source userspace user does not apply.
 *
 * To ensure, as much as feasible, compatibility across stacks and avoid
 * confusion with incompatible enumerations stakeholders for all relevant driver
 * stacks should approve additions.
 */

#define fourcc_code(a, b, c, d) ((__u32)(a) | ((__u32)(b) << 8) | \
				 ((__u32)(c) << 16) | ((__u32)(d) << 24))

#define DRM_FORMAT_BIG_ENDIAN (1U<<31) /* format is big endian instead of little endian */

/* Reserve 0 for the woke invalid format specifier */
#define DRM_FORMAT_INVALID	0

/* color index */
#define DRM_FORMAT_C1		fourcc_code('C', '1', ' ', ' ') /* [7:0] C0:C1:C2:C3:C4:C5:C6:C7 1:1:1:1:1:1:1:1 eight pixels/byte */
#define DRM_FORMAT_C2		fourcc_code('C', '2', ' ', ' ') /* [7:0] C0:C1:C2:C3 2:2:2:2 four pixels/byte */
#define DRM_FORMAT_C4		fourcc_code('C', '4', ' ', ' ') /* [7:0] C0:C1 4:4 two pixels/byte */
#define DRM_FORMAT_C8		fourcc_code('C', '8', ' ', ' ') /* [7:0] C */

/* 1 bpp Darkness (inverse relationship between channel value and brightness) */
#define DRM_FORMAT_D1		fourcc_code('D', '1', ' ', ' ') /* [7:0] D0:D1:D2:D3:D4:D5:D6:D7 1:1:1:1:1:1:1:1 eight pixels/byte */

/* 2 bpp Darkness (inverse relationship between channel value and brightness) */
#define DRM_FORMAT_D2		fourcc_code('D', '2', ' ', ' ') /* [7:0] D0:D1:D2:D3 2:2:2:2 four pixels/byte */

/* 4 bpp Darkness (inverse relationship between channel value and brightness) */
#define DRM_FORMAT_D4		fourcc_code('D', '4', ' ', ' ') /* [7:0] D0:D1 4:4 two pixels/byte */

/* 8 bpp Darkness (inverse relationship between channel value and brightness) */
#define DRM_FORMAT_D8		fourcc_code('D', '8', ' ', ' ') /* [7:0] D */

/* 1 bpp Red (direct relationship between channel value and brightness) */
#define DRM_FORMAT_R1		fourcc_code('R', '1', ' ', ' ') /* [7:0] R0:R1:R2:R3:R4:R5:R6:R7 1:1:1:1:1:1:1:1 eight pixels/byte */

/* 2 bpp Red (direct relationship between channel value and brightness) */
#define DRM_FORMAT_R2		fourcc_code('R', '2', ' ', ' ') /* [7:0] R0:R1:R2:R3 2:2:2:2 four pixels/byte */

/* 4 bpp Red (direct relationship between channel value and brightness) */
#define DRM_FORMAT_R4		fourcc_code('R', '4', ' ', ' ') /* [7:0] R0:R1 4:4 two pixels/byte */

/* 8 bpp Red (direct relationship between channel value and brightness) */
#define DRM_FORMAT_R8		fourcc_code('R', '8', ' ', ' ') /* [7:0] R */

/* 10 bpp Red (direct relationship between channel value and brightness) */
#define DRM_FORMAT_R10		fourcc_code('R', '1', '0', ' ') /* [15:0] x:R 6:10 little endian */

/* 12 bpp Red (direct relationship between channel value and brightness) */
#define DRM_FORMAT_R12		fourcc_code('R', '1', '2', ' ') /* [15:0] x:R 4:12 little endian */

/* 16 bpp Red (direct relationship between channel value and brightness) */
#define DRM_FORMAT_R16		fourcc_code('R', '1', '6', ' ') /* [15:0] R little endian */

/* 16 bpp RG */
#define DRM_FORMAT_RG88		fourcc_code('R', 'G', '8', '8') /* [15:0] R:G 8:8 little endian */
#define DRM_FORMAT_GR88		fourcc_code('G', 'R', '8', '8') /* [15:0] G:R 8:8 little endian */

/* 32 bpp RG */
#define DRM_FORMAT_RG1616	fourcc_code('R', 'G', '3', '2') /* [31:0] R:G 16:16 little endian */
#define DRM_FORMAT_GR1616	fourcc_code('G', 'R', '3', '2') /* [31:0] G:R 16:16 little endian */

/* 8 bpp RGB */
#define DRM_FORMAT_RGB332	fourcc_code('R', 'G', 'B', '8') /* [7:0] R:G:B 3:3:2 */
#define DRM_FORMAT_BGR233	fourcc_code('B', 'G', 'R', '8') /* [7:0] B:G:R 2:3:3 */

/* 16 bpp RGB */
#define DRM_FORMAT_XRGB4444	fourcc_code('X', 'R', '1', '2') /* [15:0] x:R:G:B 4:4:4:4 little endian */
#define DRM_FORMAT_XBGR4444	fourcc_code('X', 'B', '1', '2') /* [15:0] x:B:G:R 4:4:4:4 little endian */
#define DRM_FORMAT_RGBX4444	fourcc_code('R', 'X', '1', '2') /* [15:0] R:G:B:x 4:4:4:4 little endian */
#define DRM_FORMAT_BGRX4444	fourcc_code('B', 'X', '1', '2') /* [15:0] B:G:R:x 4:4:4:4 little endian */

#define DRM_FORMAT_ARGB4444	fourcc_code('A', 'R', '1', '2') /* [15:0] A:R:G:B 4:4:4:4 little endian */
#define DRM_FORMAT_ABGR4444	fourcc_code('A', 'B', '1', '2') /* [15:0] A:B:G:R 4:4:4:4 little endian */
#define DRM_FORMAT_RGBA4444	fourcc_code('R', 'A', '1', '2') /* [15:0] R:G:B:A 4:4:4:4 little endian */
#define DRM_FORMAT_BGRA4444	fourcc_code('B', 'A', '1', '2') /* [15:0] B:G:R:A 4:4:4:4 little endian */

#define DRM_FORMAT_XRGB1555	fourcc_code('X', 'R', '1', '5') /* [15:0] x:R:G:B 1:5:5:5 little endian */
#define DRM_FORMAT_XBGR1555	fourcc_code('X', 'B', '1', '5') /* [15:0] x:B:G:R 1:5:5:5 little endian */
#define DRM_FORMAT_RGBX5551	fourcc_code('R', 'X', '1', '5') /* [15:0] R:G:B:x 5:5:5:1 little endian */
#define DRM_FORMAT_BGRX5551	fourcc_code('B', 'X', '1', '5') /* [15:0] B:G:R:x 5:5:5:1 little endian */

#define DRM_FORMAT_ARGB1555	fourcc_code('A', 'R', '1', '5') /* [15:0] A:R:G:B 1:5:5:5 little endian */
#define DRM_FORMAT_ABGR1555	fourcc_code('A', 'B', '1', '5') /* [15:0] A:B:G:R 1:5:5:5 little endian */
#define DRM_FORMAT_RGBA5551	fourcc_code('R', 'A', '1', '5') /* [15:0] R:G:B:A 5:5:5:1 little endian */
#define DRM_FORMAT_BGRA5551	fourcc_code('B', 'A', '1', '5') /* [15:0] B:G:R:A 5:5:5:1 little endian */

#define DRM_FORMAT_RGB565	fourcc_code('R', 'G', '1', '6') /* [15:0] R:G:B 5:6:5 little endian */
#define DRM_FORMAT_BGR565	fourcc_code('B', 'G', '1', '6') /* [15:0] B:G:R 5:6:5 little endian */

/* 24 bpp RGB */
#define DRM_FORMAT_RGB888	fourcc_code('R', 'G', '2', '4') /* [23:0] R:G:B little endian */
#define DRM_FORMAT_BGR888	fourcc_code('B', 'G', '2', '4') /* [23:0] B:G:R little endian */

/* 32 bpp RGB */
#define DRM_FORMAT_XRGB8888	fourcc_code('X', 'R', '2', '4') /* [31:0] x:R:G:B 8:8:8:8 little endian */
#define DRM_FORMAT_XBGR8888	fourcc_code('X', 'B', '2', '4') /* [31:0] x:B:G:R 8:8:8:8 little endian */
#define DRM_FORMAT_RGBX8888	fourcc_code('R', 'X', '2', '4') /* [31:0] R:G:B:x 8:8:8:8 little endian */
#define DRM_FORMAT_BGRX8888	fourcc_code('B', 'X', '2', '4') /* [31:0] B:G:R:x 8:8:8:8 little endian */

#define DRM_FORMAT_ARGB8888	fourcc_code('A', 'R', '2', '4') /* [31:0] A:R:G:B 8:8:8:8 little endian */
#define DRM_FORMAT_ABGR8888	fourcc_code('A', 'B', '2', '4') /* [31:0] A:B:G:R 8:8:8:8 little endian */
#define DRM_FORMAT_RGBA8888	fourcc_code('R', 'A', '2', '4') /* [31:0] R:G:B:A 8:8:8:8 little endian */
#define DRM_FORMAT_BGRA8888	fourcc_code('B', 'A', '2', '4') /* [31:0] B:G:R:A 8:8:8:8 little endian */

#define DRM_FORMAT_XRGB2101010	fourcc_code('X', 'R', '3', '0') /* [31:0] x:R:G:B 2:10:10:10 little endian */
#define DRM_FORMAT_XBGR2101010	fourcc_code('X', 'B', '3', '0') /* [31:0] x:B:G:R 2:10:10:10 little endian */
#define DRM_FORMAT_RGBX1010102	fourcc_code('R', 'X', '3', '0') /* [31:0] R:G:B:x 10:10:10:2 little endian */
#define DRM_FORMAT_BGRX1010102	fourcc_code('B', 'X', '3', '0') /* [31:0] B:G:R:x 10:10:10:2 little endian */

#define DRM_FORMAT_ARGB2101010	fourcc_code('A', 'R', '3', '0') /* [31:0] A:R:G:B 2:10:10:10 little endian */
#define DRM_FORMAT_ABGR2101010	fourcc_code('A', 'B', '3', '0') /* [31:0] A:B:G:R 2:10:10:10 little endian */
#define DRM_FORMAT_RGBA1010102	fourcc_code('R', 'A', '3', '0') /* [31:0] R:G:B:A 10:10:10:2 little endian */
#define DRM_FORMAT_BGRA1010102	fourcc_code('B', 'A', '3', '0') /* [31:0] B:G:R:A 10:10:10:2 little endian */

/* 48 bpp RGB */
#define DRM_FORMAT_RGB161616 fourcc_code('R', 'G', '4', '8') /* [47:0] R:G:B 16:16:16 little endian */
#define DRM_FORMAT_BGR161616 fourcc_code('B', 'G', '4', '8') /* [47:0] B:G:R 16:16:16 little endian */

/* 64 bpp RGB */
#define DRM_FORMAT_XRGB16161616	fourcc_code('X', 'R', '4', '8') /* [63:0] x:R:G:B 16:16:16:16 little endian */
#define DRM_FORMAT_XBGR16161616	fourcc_code('X', 'B', '4', '8') /* [63:0] x:B:G:R 16:16:16:16 little endian */

#define DRM_FORMAT_ARGB16161616	fourcc_code('A', 'R', '4', '8') /* [63:0] A:R:G:B 16:16:16:16 little endian */
#define DRM_FORMAT_ABGR16161616	fourcc_code('A', 'B', '4', '8') /* [63:0] A:B:G:R 16:16:16:16 little endian */

/*
 * Half-Floating point - 16b/component
 * IEEE 754-2008 binary16 half-precision float
 * [15:0] sign:exponent:mantissa 1:5:10
 */
#define DRM_FORMAT_XRGB16161616F fourcc_code('X', 'R', '4', 'H') /* [63:0] x:R:G:B 16:16:16:16 little endian */
#define DRM_FORMAT_XBGR16161616F fourcc_code('X', 'B', '4', 'H') /* [63:0] x:B:G:R 16:16:16:16 little endian */

#define DRM_FORMAT_ARGB16161616F fourcc_code('A', 'R', '4', 'H') /* [63:0] A:R:G:B 16:16:16:16 little endian */
#define DRM_FORMAT_ABGR16161616F fourcc_code('A', 'B', '4', 'H') /* [63:0] A:B:G:R 16:16:16:16 little endian */

#define DRM_FORMAT_R16F          fourcc_code('R', ' ', ' ', 'H') /* [15:0] R 16 little endian */
#define DRM_FORMAT_GR1616F       fourcc_code('G', 'R', ' ', 'H') /* [31:0] G:R 16:16 little endian */
#define DRM_FORMAT_BGR161616F    fourcc_code('B', 'G', 'R', 'H') /* [47:0] B:G:R 16:16:16 little endian */

/*
 * Floating point - 32b/component
 * IEEE 754-2008 binary32 float
 * [31:0] sign:exponent:mantissa 1:8:23
 */
#define DRM_FORMAT_R32F          fourcc_code('R', ' ', ' ', 'F') /* [31:0] R 32 little endian */
#define DRM_FORMAT_GR3232F       fourcc_code('G', 'R', ' ', 'F') /* [63:0] R:G 32:32 little endian */
#define DRM_FORMAT_BGR323232F    fourcc_code('B', 'G', 'R', 'F') /* [95:0] R:G:B 32:32:32 little endian */
#define DRM_FORMAT_ABGR32323232F fourcc_code('A', 'B', '8', 'F') /* [127:0] R:G:B:A 32:32:32:32 little endian */

/*
 * RGBA format with 10-bit components packed in 64-bit per pixel, with 6 bits
 * of unused padding per component:
 */
#define DRM_FORMAT_AXBXGXRX106106106106 fourcc_code('A', 'B', '1', '0') /* [63:0] A:x:B:x:G:x:R:x 10:6:10:6:10:6:10:6 little endian */

/* packed YCbCr */
#define DRM_FORMAT_YUYV		fourcc_code('Y', 'U', 'Y', 'V') /* [31:0] Cr0:Y1:Cb0:Y0 8:8:8:8 little endian */
#define DRM_FORMAT_YVYU		fourcc_code('Y', 'V', 'Y', 'U') /* [31:0] Cb0:Y1:Cr0:Y0 8:8:8:8 little endian */
#define DRM_FORMAT_UYVY		fourcc_code('U', 'Y', 'V', 'Y') /* [31:0] Y1:Cr0:Y0:Cb0 8:8:8:8 little endian */
#define DRM_FORMAT_VYUY		fourcc_code('V', 'Y', 'U', 'Y') /* [31:0] Y1:Cb0:Y0:Cr0 8:8:8:8 little endian */

#define DRM_FORMAT_AYUV		fourcc_code('A', 'Y', 'U', 'V') /* [31:0] A:Y:Cb:Cr 8:8:8:8 little endian */
#define DRM_FORMAT_AVUY8888	fourcc_code('A', 'V', 'U', 'Y') /* [31:0] A:Cr:Cb:Y 8:8:8:8 little endian */
#define DRM_FORMAT_XYUV8888	fourcc_code('X', 'Y', 'U', 'V') /* [31:0] X:Y:Cb:Cr 8:8:8:8 little endian */
#define DRM_FORMAT_XVUY8888	fourcc_code('X', 'V', 'U', 'Y') /* [31:0] X:Cr:Cb:Y 8:8:8:8 little endian */
#define DRM_FORMAT_VUY888	fourcc_code('V', 'U', '2', '4') /* [23:0] Cr:Cb:Y 8:8:8 little endian */
#define DRM_FORMAT_VUY101010	fourcc_code('V', 'U', '3', '0') /* Y followed by U then V, 10:10:10. Non-linear modifier only */

/*
 * packed Y2xx indicate for each component, xx valid data occupy msb
 * 16-xx padding occupy lsb
 */
#define DRM_FORMAT_Y210         fourcc_code('Y', '2', '1', '0') /* [63:0] Cr0:0:Y1:0:Cb0:0:Y0:0 10:6:10:6:10:6:10:6 little endian per 2 Y pixels */
#define DRM_FORMAT_Y212         fourcc_code('Y', '2', '1', '2') /* [63:0] Cr0:0:Y1:0:Cb0:0:Y0:0 12:4:12:4:12:4:12:4 little endian per 2 Y pixels */
#define DRM_FORMAT_Y216         fourcc_code('Y', '2', '1', '6') /* [63:0] Cr0:Y1:Cb0:Y0 16:16:16:16 little endian per 2 Y pixels */

/*
 * packed Y4xx indicate for each component, xx valid data occupy msb
 * 16-xx padding occupy lsb except Y410
 */
#define DRM_FORMAT_Y410         fourcc_code('Y', '4', '1', '0') /* [31:0] A:Cr:Y:Cb 2:10:10:10 little endian */
#define DRM_FORMAT_Y412         fourcc_code('Y', '4', '1', '2') /* [63:0] A:0:Cr:0:Y:0:Cb:0 12:4:12:4:12:4:12:4 little endian */
#define DRM_FORMAT_Y416         fourcc_code('Y', '4', '1', '6') /* [63:0] A:Cr:Y:Cb 16:16:16:16 little endian */

#define DRM_FORMAT_XVYU2101010	fourcc_code('X', 'V', '3', '0') /* [31:0] X:Cr:Y:Cb 2:10:10:10 little endian */
#define DRM_FORMAT_XVYU12_16161616	fourcc_code('X', 'V', '3', '6') /* [63:0] X:0:Cr:0:Y:0:Cb:0 12:4:12:4:12:4:12:4 little endian */
#define DRM_FORMAT_XVYU16161616	fourcc_code('X', 'V', '4', '8') /* [63:0] X:Cr:Y:Cb 16:16:16:16 little endian */

/*
 * packed YCbCr420 2x2 tiled formats
 * first 64 bits will contain Y,Cb,Cr components for a 2x2 tile
 */
/* [63:0]   A3:A2:Y3:0:Cr0:0:Y2:0:A1:A0:Y1:0:Cb0:0:Y0:0  1:1:8:2:8:2:8:2:1:1:8:2:8:2:8:2 little endian */
#define DRM_FORMAT_Y0L0		fourcc_code('Y', '0', 'L', '0')
/* [63:0]   X3:X2:Y3:0:Cr0:0:Y2:0:X1:X0:Y1:0:Cb0:0:Y0:0  1:1:8:2:8:2:8:2:1:1:8:2:8:2:8:2 little endian */
#define DRM_FORMAT_X0L0		fourcc_code('X', '0', 'L', '0')

/* [63:0]   A3:A2:Y3:Cr0:Y2:A1:A0:Y1:Cb0:Y0  1:1:10:10:10:1:1:10:10:10 little endian */
#define DRM_FORMAT_Y0L2		fourcc_code('Y', '0', 'L', '2')
/* [63:0]   X3:X2:Y3:Cr0:Y2:X1:X0:Y1:Cb0:Y0  1:1:10:10:10:1:1:10:10:10 little endian */
#define DRM_FORMAT_X0L2		fourcc_code('X', '0', 'L', '2')

/*
 * 1-plane YUV 4:2:0
 * In these formats, the woke component ordering is specified (Y, followed by U
 * then V), but the woke exact Linear layout is undefined.
 * These formats can only be used with a non-Linear modifier.
 */
#define DRM_FORMAT_YUV420_8BIT	fourcc_code('Y', 'U', '0', '8')
#define DRM_FORMAT_YUV420_10BIT	fourcc_code('Y', 'U', '1', '0')

/*
 * 2 plane RGB + A
 * index 0 = RGB plane, same format as the woke corresponding non _A8 format has
 * index 1 = A plane, [7:0] A
 */
#define DRM_FORMAT_XRGB8888_A8	fourcc_code('X', 'R', 'A', '8')
#define DRM_FORMAT_XBGR8888_A8	fourcc_code('X', 'B', 'A', '8')
#define DRM_FORMAT_RGBX8888_A8	fourcc_code('R', 'X', 'A', '8')
#define DRM_FORMAT_BGRX8888_A8	fourcc_code('B', 'X', 'A', '8')
#define DRM_FORMAT_RGB888_A8	fourcc_code('R', '8', 'A', '8')
#define DRM_FORMAT_BGR888_A8	fourcc_code('B', '8', 'A', '8')
#define DRM_FORMAT_RGB565_A8	fourcc_code('R', '5', 'A', '8')
#define DRM_FORMAT_BGR565_A8	fourcc_code('B', '5', 'A', '8')

/*
 * 2 plane YCbCr
 * index 0 = Y plane, [7:0] Y
 * index 1 = Cr:Cb plane, [15:0] Cr:Cb little endian
 * or
 * index 1 = Cb:Cr plane, [15:0] Cb:Cr little endian
 */
#define DRM_FORMAT_NV12		fourcc_code('N', 'V', '1', '2') /* 2x2 subsampled Cr:Cb plane */
#define DRM_FORMAT_NV21		fourcc_code('N', 'V', '2', '1') /* 2x2 subsampled Cb:Cr plane */
#define DRM_FORMAT_NV16		fourcc_code('N', 'V', '1', '6') /* 2x1 subsampled Cr:Cb plane */
#define DRM_FORMAT_NV61		fourcc_code('N', 'V', '6', '1') /* 2x1 subsampled Cb:Cr plane */
#define DRM_FORMAT_NV24		fourcc_code('N', 'V', '2', '4') /* non-subsampled Cr:Cb plane */
#define DRM_FORMAT_NV42		fourcc_code('N', 'V', '4', '2') /* non-subsampled Cb:Cr plane */
/*
 * 2 plane YCbCr
 * index 0 = Y plane, [39:0] Y3:Y2:Y1:Y0 little endian
 * index 1 = Cr:Cb plane, [39:0] Cr1:Cb1:Cr0:Cb0 little endian
 */
#define DRM_FORMAT_NV15		fourcc_code('N', 'V', '1', '5') /* 2x2 subsampled Cr:Cb plane */
#define DRM_FORMAT_NV20		fourcc_code('N', 'V', '2', '0') /* 2x1 subsampled Cr:Cb plane */
#define DRM_FORMAT_NV30		fourcc_code('N', 'V', '3', '0') /* non-subsampled Cr:Cb plane */

/*
 * 2 plane YCbCr MSB aligned
 * index 0 = Y plane, [15:0] Y:x [10:6] little endian
 * index 1 = Cr:Cb plane, [31:0] Cr:x:Cb:x [10:6:10:6] little endian
 */
#define DRM_FORMAT_P210		fourcc_code('P', '2', '1', '0') /* 2x1 subsampled Cr:Cb plane, 10 bit per channel */

/*
 * 2 plane YCbCr MSB aligned
 * index 0 = Y plane, [15:0] Y:x [10:6] little endian
 * index 1 = Cr:Cb plane, [31:0] Cr:x:Cb:x [10:6:10:6] little endian
 */
#define DRM_FORMAT_P010		fourcc_code('P', '0', '1', '0') /* 2x2 subsampled Cr:Cb plane 10 bits per channel */

/*
 * 2 plane YCbCr MSB aligned
 * index 0 = Y plane, [15:0] Y:x [12:4] little endian
 * index 1 = Cr:Cb plane, [31:0] Cr:x:Cb:x [12:4:12:4] little endian
 */
#define DRM_FORMAT_P012		fourcc_code('P', '0', '1', '2') /* 2x2 subsampled Cr:Cb plane 12 bits per channel */

/*
 * 2 plane YCbCr MSB aligned
 * index 0 = Y plane, [15:0] Y little endian
 * index 1 = Cr:Cb plane, [31:0] Cr:Cb [16:16] little endian
 */
#define DRM_FORMAT_P016		fourcc_code('P', '0', '1', '6') /* 2x2 subsampled Cr:Cb plane 16 bits per channel */

/* 2 plane YCbCr420.
 * 3 10 bit components and 2 padding bits packed into 4 bytes.
 * index 0 = Y plane, [31:0] x:Y2:Y1:Y0 2:10:10:10 little endian
 * index 1 = Cr:Cb plane, [63:0] x:Cr2:Cb2:Cr1:x:Cb1:Cr0:Cb0 [2:10:10:10:2:10:10:10] little endian
 */
#define DRM_FORMAT_P030		fourcc_code('P', '0', '3', '0') /* 2x2 subsampled Cr:Cb plane 10 bits per channel packed */

/* 3 plane non-subsampled (444) YCbCr
 * 16 bits per component, but only 10 bits are used and 6 bits are padded
 * index 0: Y plane, [15:0] Y:x [10:6] little endian
 * index 1: Cb plane, [15:0] Cb:x [10:6] little endian
 * index 2: Cr plane, [15:0] Cr:x [10:6] little endian
 */
#define DRM_FORMAT_Q410		fourcc_code('Q', '4', '1', '0')

/* 3 plane non-subsampled (444) YCrCb
 * 16 bits per component, but only 10 bits are used and 6 bits are padded
 * index 0: Y plane, [15:0] Y:x [10:6] little endian
 * index 1: Cr plane, [15:0] Cr:x [10:6] little endian
 * index 2: Cb plane, [15:0] Cb:x [10:6] little endian
 */
#define DRM_FORMAT_Q401		fourcc_code('Q', '4', '0', '1')

/*
 * 3 plane YCbCr LSB aligned
 * In order to use these formats in a similar fashion to MSB aligned ones
 * implementation can multiply the woke values by 2^6=64. For that reason the woke padding
 * must only contain zeros.
 * index 0 = Y plane, [15:0] z:Y [6:10] little endian
 * index 1 = Cr plane, [15:0] z:Cr [6:10] little endian
 * index 2 = Cb plane, [15:0] z:Cb [6:10] little endian
 */
#define DRM_FORMAT_S010	fourcc_code('S', '0', '1', '0') /* 2x2 subsampled Cb (1) and Cr (2) planes 10 bits per channel */
#define DRM_FORMAT_S210	fourcc_code('S', '2', '1', '0') /* 2x1 subsampled Cb (1) and Cr (2) planes 10 bits per channel */
#define DRM_FORMAT_S410	fourcc_code('S', '4', '1', '0') /* non-subsampled Cb (1) and Cr (2) planes 10 bits per channel */

/*
 * 3 plane YCbCr LSB aligned
 * In order to use these formats in a similar fashion to MSB aligned ones
 * implementation can multiply the woke values by 2^4=16. For that reason the woke padding
 * must only contain zeros.
 * index 0 = Y plane, [15:0] z:Y [4:12] little endian
 * index 1 = Cr plane, [15:0] z:Cr [4:12] little endian
 * index 2 = Cb plane, [15:0] z:Cb [4:12] little endian
 */
#define DRM_FORMAT_S012	fourcc_code('S', '0', '1', '2') /* 2x2 subsampled Cb (1) and Cr (2) planes 12 bits per channel */
#define DRM_FORMAT_S212	fourcc_code('S', '2', '1', '2') /* 2x1 subsampled Cb (1) and Cr (2) planes 12 bits per channel */
#define DRM_FORMAT_S412	fourcc_code('S', '4', '1', '2') /* non-subsampled Cb (1) and Cr (2) planes 12 bits per channel */

/*
 * 3 plane YCbCr
 * index 0 = Y plane, [15:0] Y little endian
 * index 1 = Cr plane, [15:0] Cr little endian
 * index 2 = Cb plane, [15:0] Cb little endian
 */
#define DRM_FORMAT_S016	fourcc_code('S', '0', '1', '6') /* 2x2 subsampled Cb (1) and Cr (2) planes 16 bits per channel */
#define DRM_FORMAT_S216	fourcc_code('S', '2', '1', '6') /* 2x1 subsampled Cb (1) and Cr (2) planes 16 bits per channel */
#define DRM_FORMAT_S416	fourcc_code('S', '4', '1', '6') /* non-subsampled Cb (1) and Cr (2) planes 16 bits per channel */

/*
 * 3 plane YCbCr
 * index 0: Y plane, [7:0] Y
 * index 1: Cb plane, [7:0] Cb
 * index 2: Cr plane, [7:0] Cr
 * or
 * index 1: Cr plane, [7:0] Cr
 * index 2: Cb plane, [7:0] Cb
 */
#define DRM_FORMAT_YUV410	fourcc_code('Y', 'U', 'V', '9') /* 4x4 subsampled Cb (1) and Cr (2) planes */
#define DRM_FORMAT_YVU410	fourcc_code('Y', 'V', 'U', '9') /* 4x4 subsampled Cr (1) and Cb (2) planes */
#define DRM_FORMAT_YUV411	fourcc_code('Y', 'U', '1', '1') /* 4x1 subsampled Cb (1) and Cr (2) planes */
#define DRM_FORMAT_YVU411	fourcc_code('Y', 'V', '1', '1') /* 4x1 subsampled Cr (1) and Cb (2) planes */
#define DRM_FORMAT_YUV420	fourcc_code('Y', 'U', '1', '2') /* 2x2 subsampled Cb (1) and Cr (2) planes */
#define DRM_FORMAT_YVU420	fourcc_code('Y', 'V', '1', '2') /* 2x2 subsampled Cr (1) and Cb (2) planes */
#define DRM_FORMAT_YUV422	fourcc_code('Y', 'U', '1', '6') /* 2x1 subsampled Cb (1) and Cr (2) planes */
#define DRM_FORMAT_YVU422	fourcc_code('Y', 'V', '1', '6') /* 2x1 subsampled Cr (1) and Cb (2) planes */
#define DRM_FORMAT_YUV444	fourcc_code('Y', 'U', '2', '4') /* non-subsampled Cb (1) and Cr (2) planes */
#define DRM_FORMAT_YVU444	fourcc_code('Y', 'V', '2', '4') /* non-subsampled Cr (1) and Cb (2) planes */


/*
 * Format Modifiers:
 *
 * Format modifiers describe, typically, a re-ordering or modification
 * of the woke data in a plane of an FB.  This can be used to express tiled/
 * swizzled formats, or compression, or a combination of the woke two.
 *
 * The upper 8 bits of the woke format modifier are a vendor-id as assigned
 * below.  The lower 56 bits are assigned as vendor sees fit.
 */

/* Vendor Ids: */
#define DRM_FORMAT_MOD_VENDOR_NONE    0
#define DRM_FORMAT_MOD_VENDOR_INTEL   0x01
#define DRM_FORMAT_MOD_VENDOR_AMD     0x02
#define DRM_FORMAT_MOD_VENDOR_NVIDIA  0x03
#define DRM_FORMAT_MOD_VENDOR_SAMSUNG 0x04
#define DRM_FORMAT_MOD_VENDOR_QCOM    0x05
#define DRM_FORMAT_MOD_VENDOR_VIVANTE 0x06
#define DRM_FORMAT_MOD_VENDOR_BROADCOM 0x07
#define DRM_FORMAT_MOD_VENDOR_ARM     0x08
#define DRM_FORMAT_MOD_VENDOR_ALLWINNER 0x09
#define DRM_FORMAT_MOD_VENDOR_AMLOGIC 0x0a
#define DRM_FORMAT_MOD_VENDOR_MTK     0x0b
#define DRM_FORMAT_MOD_VENDOR_APPLE   0x0c

/* add more to the woke end as needed */

#define DRM_FORMAT_RESERVED	      ((1ULL << 56) - 1)

#define fourcc_mod_get_vendor(modifier) \
	(((modifier) >> 56) & 0xff)

#define fourcc_mod_is_vendor(modifier, vendor) \
	(fourcc_mod_get_vendor(modifier) == DRM_FORMAT_MOD_VENDOR_## vendor)

#define fourcc_mod_code(vendor, val) \
	((((__u64)DRM_FORMAT_MOD_VENDOR_## vendor) << 56) | ((val) & 0x00ffffffffffffffULL))

/*
 * Format Modifier tokens:
 *
 * When adding a new token please document the woke layout with a code comment,
 * similar to the woke fourcc codes above. drm_fourcc.h is considered the
 * authoritative source for all of these.
 *
 * Generic modifier names:
 *
 * DRM_FORMAT_MOD_GENERIC_* definitions are used to provide vendor-neutral names
 * for layouts which are common across multiple vendors. To preserve
 * compatibility, in cases where a vendor-specific definition already exists and
 * a generic name for it is desired, the woke common name is a purely symbolic alias
 * and must use the woke same numerical value as the woke original definition.
 *
 * Note that generic names should only be used for modifiers which describe
 * generic layouts (such as pixel re-ordering), which may have
 * independently-developed support across multiple vendors.
 *
 * In future cases where a generic layout is identified before merging with a
 * vendor-specific modifier, a new 'GENERIC' vendor or modifier using vendor
 * 'NONE' could be considered. This should only be for obvious, exceptional
 * cases to avoid polluting the woke 'GENERIC' namespace with modifiers which only
 * apply to a single vendor.
 *
 * Generic names should not be used for cases where multiple hardware vendors
 * have implementations of the woke same standardised compression scheme (such as
 * AFBC). In those cases, all implementations should use the woke same format
 * modifier(s), reflecting the woke vendor of the woke standard.
 */

#define DRM_FORMAT_MOD_GENERIC_16_16_TILE DRM_FORMAT_MOD_SAMSUNG_16_16_TILE

/*
 * Invalid Modifier
 *
 * This modifier can be used as a sentinel to terminate the woke format modifiers
 * list, or to initialize a variable with an invalid modifier. It might also be
 * used to report an error back to userspace for certain APIs.
 */
#define DRM_FORMAT_MOD_INVALID	fourcc_mod_code(NONE, DRM_FORMAT_RESERVED)

/*
 * Linear Layout
 *
 * Just plain linear layout. Note that this is different from no specifying any
 * modifier (e.g. not setting DRM_MODE_FB_MODIFIERS in the woke DRM_ADDFB2 ioctl),
 * which tells the woke driver to also take driver-internal information into account
 * and so might actually result in a tiled framebuffer.
 */
#define DRM_FORMAT_MOD_LINEAR	fourcc_mod_code(NONE, 0)

/*
 * Deprecated: use DRM_FORMAT_MOD_LINEAR instead
 *
 * The "none" format modifier doesn't actually mean that the woke modifier is
 * implicit, instead it means that the woke layout is linear. Whether modifiers are
 * used is out-of-band information carried in an API-specific way (e.g. in a
 * flag for drm_mode_fb_cmd2).
 */
#define DRM_FORMAT_MOD_NONE	0

/* Intel framebuffer modifiers */

/*
 * Intel X-tiling layout
 *
 * This is a tiled layout using 4Kb tiles (except on gen2 where the woke tiles 2Kb)
 * in row-major layout. Within the woke tile bytes are laid out row-major, with
 * a platform-dependent stride. On top of that the woke memory can apply
 * platform-depending swizzling of some higher address bits into bit6.
 *
 * Note that this layout is only accurate on intel gen 8+ or valleyview chipsets.
 * On earlier platforms the woke is highly platforms specific and not useful for
 * cross-driver sharing. It exists since on a given platform it does uniquely
 * identify the woke layout in a simple way for i915-specific userspace, which
 * facilitated conversion of userspace to modifiers. Additionally the woke exact
 * format on some really old platforms is not known.
 */
#define I915_FORMAT_MOD_X_TILED	fourcc_mod_code(INTEL, 1)

/*
 * Intel Y-tiling layout
 *
 * This is a tiled layout using 4Kb tiles (except on gen2 where the woke tiles 2Kb)
 * in row-major layout. Within the woke tile bytes are laid out in OWORD (16 bytes)
 * chunks column-major, with a platform-dependent height. On top of that the
 * memory can apply platform-depending swizzling of some higher address bits
 * into bit6.
 *
 * Note that this layout is only accurate on intel gen 8+ or valleyview chipsets.
 * On earlier platforms the woke is highly platforms specific and not useful for
 * cross-driver sharing. It exists since on a given platform it does uniquely
 * identify the woke layout in a simple way for i915-specific userspace, which
 * facilitated conversion of userspace to modifiers. Additionally the woke exact
 * format on some really old platforms is not known.
 */
#define I915_FORMAT_MOD_Y_TILED	fourcc_mod_code(INTEL, 2)

/*
 * Intel Yf-tiling layout
 *
 * This is a tiled layout using 4Kb tiles in row-major layout.
 * Within the woke tile pixels are laid out in 16 256 byte units / sub-tiles which
 * are arranged in four groups (two wide, two high) with column-major layout.
 * Each group therefore consists out of four 256 byte units, which are also laid
 * out as 2x2 column-major.
 * 256 byte units are made out of four 64 byte blocks of pixels, producing
 * either a square block or a 2:1 unit.
 * 64 byte blocks of pixels contain four pixel rows of 16 bytes, where the woke width
 * in pixel depends on the woke pixel depth.
 */
#define I915_FORMAT_MOD_Yf_TILED fourcc_mod_code(INTEL, 3)

/*
 * Intel color control surface (CCS) for render compression
 *
 * The framebuffer format must be one of the woke 8:8:8:8 RGB formats.
 * The main surface will be plane index 0 and must be Y/Yf-tiled,
 * the woke CCS will be plane index 1.
 *
 * Each CCS tile matches a 1024x512 pixel area of the woke main surface.
 * To match certain aspects of the woke 3D hardware the woke CCS is
 * considered to be made up of normal 128Bx32 Y tiles, Thus
 * the woke CCS pitch must be specified in multiples of 128 bytes.
 *
 * In reality the woke CCS tile appears to be a 64Bx64 Y tile, composed
 * of QWORD (8 bytes) chunks instead of OWORD (16 bytes) chunks.
 * But that fact is not relevant unless the woke memory is accessed
 * directly.
 */
#define I915_FORMAT_MOD_Y_TILED_CCS	fourcc_mod_code(INTEL, 4)
#define I915_FORMAT_MOD_Yf_TILED_CCS	fourcc_mod_code(INTEL, 5)

/*
 * Intel color control surfaces (CCS) for Gen-12 render compression.
 *
 * The main surface is Y-tiled and at plane index 0, the woke CCS is linear and
 * at index 1. A 64B CCS cache line corresponds to an area of 4x1 tiles in
 * main surface. In other words, 4 bits in CCS map to a main surface cache
 * line pair. The main surface pitch is required to be a multiple of four
 * Y-tile widths.
 */
#define I915_FORMAT_MOD_Y_TILED_GEN12_RC_CCS fourcc_mod_code(INTEL, 6)

/*
 * Intel color control surfaces (CCS) for Gen-12 media compression
 *
 * The main surface is Y-tiled and at plane index 0, the woke CCS is linear and
 * at index 1. A 64B CCS cache line corresponds to an area of 4x1 tiles in
 * main surface. In other words, 4 bits in CCS map to a main surface cache
 * line pair. The main surface pitch is required to be a multiple of four
 * Y-tile widths. For semi-planar formats like NV12, CCS planes follow the
 * Y and UV planes i.e., planes 0 and 1 are used for Y and UV surfaces,
 * planes 2 and 3 for the woke respective CCS.
 */
#define I915_FORMAT_MOD_Y_TILED_GEN12_MC_CCS fourcc_mod_code(INTEL, 7)

/*
 * Intel Color Control Surface with Clear Color (CCS) for Gen-12 render
 * compression.
 *
 * The main surface is Y-tiled and is at plane index 0 whereas CCS is linear
 * and at index 1. The clear color is stored at index 2, and the woke pitch should
 * be 64 bytes aligned. The clear color structure is 256 bits. The first 128 bits
 * represents Raw Clear Color Red, Green, Blue and Alpha color each represented
 * by 32 bits. The raw clear color is consumed by the woke 3d engine and generates
 * the woke converted clear color of size 64 bits. The first 32 bits store the woke Lower
 * Converted Clear Color value and the woke next 32 bits store the woke Higher Converted
 * Clear Color value when applicable. The Converted Clear Color values are
 * consumed by the woke DE. The last 64 bits are used to store Color Discard Enable
 * and Depth Clear Value Valid which are ignored by the woke DE. A CCS cache line
 * corresponds to an area of 4x1 tiles in the woke main surface. The main surface
 * pitch is required to be a multiple of 4 tile widths.
 */
#define I915_FORMAT_MOD_Y_TILED_GEN12_RC_CCS_CC fourcc_mod_code(INTEL, 8)

/*
 * Intel Tile 4 layout
 *
 * This is a tiled layout using 4KB tiles in a row-major layout. It has the woke same
 * shape as Tile Y at two granularities: 4KB (128B x 32) and 64B (16B x 4). It
 * only differs from Tile Y at the woke 256B granularity in between. At this
 * granularity, Tile Y has a shape of 16B x 32 rows, but this tiling has a shape
 * of 64B x 8 rows.
 */
#define I915_FORMAT_MOD_4_TILED         fourcc_mod_code(INTEL, 9)

/*
 * Intel color control surfaces (CCS) for DG2 render compression.
 *
 * The main surface is Tile 4 and at plane index 0. The CCS data is stored
 * outside of the woke GEM object in a reserved memory area dedicated for the
 * storage of the woke CCS data for all RC/RC_CC/MC compressible GEM objects. The
 * main surface pitch is required to be a multiple of four Tile 4 widths.
 */
#define I915_FORMAT_MOD_4_TILED_DG2_RC_CCS fourcc_mod_code(INTEL, 10)

/*
 * Intel color control surfaces (CCS) for DG2 media compression.
 *
 * The main surface is Tile 4 and at plane index 0. For semi-planar formats
 * like NV12, the woke Y and UV planes are Tile 4 and are located at plane indices
 * 0 and 1, respectively. The CCS for all planes are stored outside of the
 * GEM object in a reserved memory area dedicated for the woke storage of the
 * CCS data for all RC/RC_CC/MC compressible GEM objects. The main surface
 * pitch is required to be a multiple of four Tile 4 widths.
 */
#define I915_FORMAT_MOD_4_TILED_DG2_MC_CCS fourcc_mod_code(INTEL, 11)

/*
 * Intel Color Control Surface with Clear Color (CCS) for DG2 render compression.
 *
 * The main surface is Tile 4 and at plane index 0. The CCS data is stored
 * outside of the woke GEM object in a reserved memory area dedicated for the
 * storage of the woke CCS data for all RC/RC_CC/MC compressible GEM objects. The
 * main surface pitch is required to be a multiple of four Tile 4 widths. The
 * clear color is stored at plane index 1 and the woke pitch should be 64 bytes
 * aligned. The format of the woke 256 bits of clear color data matches the woke one used
 * for the woke I915_FORMAT_MOD_Y_TILED_GEN12_RC_CCS_CC modifier, see its description
 * for details.
 */
#define I915_FORMAT_MOD_4_TILED_DG2_RC_CCS_CC fourcc_mod_code(INTEL, 12)

/*
 * Intel Color Control Surfaces (CCS) for display ver. 14 render compression.
 *
 * The main surface is tile4 and at plane index 0, the woke CCS is linear and
 * at index 1. A 64B CCS cache line corresponds to an area of 4x1 tiles in
 * main surface. In other words, 4 bits in CCS map to a main surface cache
 * line pair. The main surface pitch is required to be a multiple of four
 * tile4 widths.
 */
#define I915_FORMAT_MOD_4_TILED_MTL_RC_CCS fourcc_mod_code(INTEL, 13)

/*
 * Intel Color Control Surfaces (CCS) for display ver. 14 media compression
 *
 * The main surface is tile4 and at plane index 0, the woke CCS is linear and
 * at index 1. A 64B CCS cache line corresponds to an area of 4x1 tiles in
 * main surface. In other words, 4 bits in CCS map to a main surface cache
 * line pair. The main surface pitch is required to be a multiple of four
 * tile4 widths. For semi-planar formats like NV12, CCS planes follow the
 * Y and UV planes i.e., planes 0 and 1 are used for Y and UV surfaces,
 * planes 2 and 3 for the woke respective CCS.
 */
#define I915_FORMAT_MOD_4_TILED_MTL_MC_CCS fourcc_mod_code(INTEL, 14)

/*
 * Intel Color Control Surface with Clear Color (CCS) for display ver. 14 render
 * compression.
 *
 * The main surface is tile4 and is at plane index 0 whereas CCS is linear
 * and at index 1. The clear color is stored at index 2, and the woke pitch should
 * be ignored. The clear color structure is 256 bits. The first 128 bits
 * represents Raw Clear Color Red, Green, Blue and Alpha color each represented
 * by 32 bits. The raw clear color is consumed by the woke 3d engine and generates
 * the woke converted clear color of size 64 bits. The first 32 bits store the woke Lower
 * Converted Clear Color value and the woke next 32 bits store the woke Higher Converted
 * Clear Color value when applicable. The Converted Clear Color values are
 * consumed by the woke DE. The last 64 bits are used to store Color Discard Enable
 * and Depth Clear Value Valid which are ignored by the woke DE. A CCS cache line
 * corresponds to an area of 4x1 tiles in the woke main surface. The main surface
 * pitch is required to be a multiple of 4 tile widths.
 */
#define I915_FORMAT_MOD_4_TILED_MTL_RC_CCS_CC fourcc_mod_code(INTEL, 15)

/*
 * Intel Color Control Surfaces (CCS) for graphics ver. 20 unified compression
 * on integrated graphics
 *
 * The main surface is Tile 4 and at plane index 0. For semi-planar formats
 * like NV12, the woke Y and UV planes are Tile 4 and are located at plane indices
 * 0 and 1, respectively. The CCS for all planes are stored outside of the
 * GEM object in a reserved memory area dedicated for the woke storage of the
 * CCS data for all compressible GEM objects.
 */
#define I915_FORMAT_MOD_4_TILED_LNL_CCS fourcc_mod_code(INTEL, 16)

/*
 * Intel Color Control Surfaces (CCS) for graphics ver. 20 unified compression
 * on discrete graphics
 *
 * The main surface is Tile 4 and at plane index 0. For semi-planar formats
 * like NV12, the woke Y and UV planes are Tile 4 and are located at plane indices
 * 0 and 1, respectively. The CCS for all planes are stored outside of the
 * GEM object in a reserved memory area dedicated for the woke storage of the
 * CCS data for all compressible GEM objects. The GEM object must be stored in
 * contiguous memory with a size aligned to 64KB
 */
#define I915_FORMAT_MOD_4_TILED_BMG_CCS fourcc_mod_code(INTEL, 17)

/*
 * Tiled, NV12MT, grouped in 64 (pixels) x 32 (lines) -sized macroblocks
 *
 * Macroblocks are laid in a Z-shape, and each pixel data is following the
 * standard NV12 style.
 * As for NV12, an image is the woke result of two frame buffers: one for Y,
 * one for the woke interleaved Cb/Cr components (1/2 the woke height of the woke Y buffer).
 * Alignment requirements are (for each buffer):
 * - multiple of 128 pixels for the woke width
 * - multiple of  32 pixels for the woke height
 *
 * For more information: see https://linuxtv.org/downloads/v4l-dvb-apis/re32.html
 */
#define DRM_FORMAT_MOD_SAMSUNG_64_32_TILE	fourcc_mod_code(SAMSUNG, 1)

/*
 * Tiled, 16 (pixels) x 16 (lines) - sized macroblocks
 *
 * This is a simple tiled layout using tiles of 16x16 pixels in a row-major
 * layout. For YCbCr formats Cb/Cr components are taken in such a way that
 * they correspond to their 16x16 luma block.
 */
#define DRM_FORMAT_MOD_SAMSUNG_16_16_TILE	fourcc_mod_code(SAMSUNG, 2)

/*
 * Qualcomm Compressed Format
 *
 * Refers to a compressed variant of the woke base format that is compressed.
 * Implementation may be platform and base-format specific.
 *
 * Each macrotile consists of m x n (mostly 4 x 4) tiles.
 * Pixel data pitch/stride is aligned with macrotile width.
 * Pixel data height is aligned with macrotile height.
 * Entire pixel data buffer is aligned with 4k(bytes).
 */
#define DRM_FORMAT_MOD_QCOM_COMPRESSED	fourcc_mod_code(QCOM, 1)

/*
 * Qualcomm Tiled Format
 *
 * Similar to DRM_FORMAT_MOD_QCOM_COMPRESSED but not compressed.
 * Implementation may be platform and base-format specific.
 *
 * Each macrotile consists of m x n (mostly 4 x 4) tiles.
 * Pixel data pitch/stride is aligned with macrotile width.
 * Pixel data height is aligned with macrotile height.
 * Entire pixel data buffer is aligned with 4k(bytes).
 */
#define DRM_FORMAT_MOD_QCOM_TILED3	fourcc_mod_code(QCOM, 3)

/*
 * Qualcomm Alternate Tiled Format
 *
 * Alternate tiled format typically only used within GMEM.
 * Implementation may be platform and base-format specific.
 */
#define DRM_FORMAT_MOD_QCOM_TILED2	fourcc_mod_code(QCOM, 2)


/* Vivante framebuffer modifiers */

/*
 * Vivante 4x4 tiling layout
 *
 * This is a simple tiled layout using tiles of 4x4 pixels in a row-major
 * layout.
 */
#define DRM_FORMAT_MOD_VIVANTE_TILED		fourcc_mod_code(VIVANTE, 1)

/*
 * Vivante 64x64 super-tiling layout
 *
 * This is a tiled layout using 64x64 pixel super-tiles, where each super-tile
 * contains 8x4 groups of 2x4 tiles of 4x4 pixels (like above) each, all in row-
 * major layout.
 *
 * For more information: see
 * https://github.com/etnaviv/etna_viv/blob/master/doc/hardware.md#texture-tiling
 */
#define DRM_FORMAT_MOD_VIVANTE_SUPER_TILED	fourcc_mod_code(VIVANTE, 2)

/*
 * Vivante 4x4 tiling layout for dual-pipe
 *
 * Same as the woke 4x4 tiling layout, except every second 4x4 pixel tile starts at a
 * different base address. Offsets from the woke base addresses are therefore halved
 * compared to the woke non-split tiled layout.
 */
#define DRM_FORMAT_MOD_VIVANTE_SPLIT_TILED	fourcc_mod_code(VIVANTE, 3)

/*
 * Vivante 64x64 super-tiling layout for dual-pipe
 *
 * Same as the woke 64x64 super-tiling layout, except every second 4x4 pixel tile
 * starts at a different base address. Offsets from the woke base addresses are
 * therefore halved compared to the woke non-split super-tiled layout.
 */
#define DRM_FORMAT_MOD_VIVANTE_SPLIT_SUPER_TILED fourcc_mod_code(VIVANTE, 4)

/*
 * Vivante TS (tile-status) buffer modifiers. They can be combined with all of
 * the woke color buffer tiling modifiers defined above. When TS is present it's a
 * separate buffer containing the woke clear/compression status of each tile. The
 * modifiers are defined as VIVANTE_MOD_TS_c_s, where c is the woke color buffer
 * tile size in bytes covered by one entry in the woke status buffer and s is the
 * number of status bits per entry.
 * We reserve the woke top 8 bits of the woke Vivante modifier space for tile status
 * clear/compression modifiers, as future cores might add some more TS layout
 * variations.
 */
#define VIVANTE_MOD_TS_64_4               (1ULL << 48)
#define VIVANTE_MOD_TS_64_2               (2ULL << 48)
#define VIVANTE_MOD_TS_128_4              (3ULL << 48)
#define VIVANTE_MOD_TS_256_4              (4ULL << 48)
#define VIVANTE_MOD_TS_MASK               (0xfULL << 48)

/*
 * Vivante compression modifiers. Those depend on a TS modifier being present
 * as the woke TS bits get reinterpreted as compression tags instead of simple
 * clear markers when compression is enabled.
 */
#define VIVANTE_MOD_COMP_DEC400           (1ULL << 52)
#define VIVANTE_MOD_COMP_MASK             (0xfULL << 52)

/* Masking out the woke extension bits will yield the woke base modifier. */
#define VIVANTE_MOD_EXT_MASK              (VIVANTE_MOD_TS_MASK | \
                                           VIVANTE_MOD_COMP_MASK)

/* NVIDIA frame buffer modifiers */

/*
 * Tegra Tiled Layout, used by Tegra 2, 3 and 4.
 *
 * Pixels are arranged in simple tiles of 16 x 16 bytes.
 */
#define DRM_FORMAT_MOD_NVIDIA_TEGRA_TILED fourcc_mod_code(NVIDIA, 1)

/*
 * Generalized Block Linear layout, used by desktop GPUs starting with NV50/G80,
 * and Tegra GPUs starting with Tegra K1.
 *
 * Pixels are arranged in Groups of Bytes (GOBs).  GOB size and layout varies
 * based on the woke architecture generation.  GOBs themselves are then arranged in
 * 3D blocks, with the woke block dimensions (in terms of GOBs) always being a power
 * of two, and hence expressible as their log2 equivalent (E.g., "2" represents
 * a block depth or height of "4").
 *
 * Chapter 20 "Pixel Memory Formats" of the woke Tegra X1 TRM describes this format
 * in full detail.
 *
 *       Macro
 * Bits  Param Description
 * ----  ----- -----------------------------------------------------------------
 *
 *  3:0  h     log2(height) of each block, in GOBs.  Placed here for
 *             compatibility with the woke existing
 *             DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK()-based modifiers.
 *
 *  4:4  -     Must be 1, to indicate block-linear layout.  Necessary for
 *             compatibility with the woke existing
 *             DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK()-based modifiers.
 *
 *  8:5  -     Reserved (To support 3D-surfaces with variable log2(depth) block
 *             size).  Must be zero.
 *
 *             Note there is no log2(width) parameter.  Some portions of the
 *             hardware support a block width of two gobs, but it is impractical
 *             to use due to lack of support elsewhere, and has no known
 *             benefits.
 *
 * 11:9  -     Reserved (To support 2D-array textures with variable array stride
 *             in blocks, specified via log2(tile width in blocks)).  Must be
 *             zero.
 *
 * 19:12 k     Page Kind.  This value directly maps to a field in the woke page
 *             tables of all GPUs >= NV50.  It affects the woke exact layout of bits
 *             in memory and can be derived from the woke tuple
 *
 *               (format, GPU model, compression type, samples per pixel)
 *
 *             Where compression type is defined below.  If GPU model were
 *             implied by the woke format modifier, format, or memory buffer, page
 *             kind would not need to be included in the woke modifier itself, but
 *             since the woke modifier should define the woke layout of the woke associated
 *             memory buffer independent from any device or other context, it
 *             must be included here.
 *
 * 21:20 g     GOB Height and Page Kind Generation.  The height of a GOB changed
 *             starting with Fermi GPUs.  Additionally, the woke mapping between page
 *             kind and bit layout has changed at various points.
 *
 *               0 = Gob Height 8, Fermi - Volta, Tegra K1+ Page Kind mapping
 *               1 = Gob Height 4, G80 - GT2XX Page Kind mapping
 *               2 = Gob Height 8, Turing+ Page Kind mapping
 *               3 = Reserved for future use.
 *
 * 22:22 s     Sector layout.  On Tegra GPUs prior to Xavier, there is a further
 *             bit remapping step that occurs at an even lower level than the
 *             page kind and block linear swizzles.  This causes the woke layout of
 *             surfaces mapped in those SOC's GPUs to be incompatible with the
 *             equivalent mapping on other GPUs in the woke same system.
 *
 *               0 = Tegra K1 - Tegra Parker/TX2 Layout.
 *               1 = Desktop GPU and Tegra Xavier+ Layout
 *
 * 25:23 c     Lossless Framebuffer Compression type.
 *
 *               0 = none
 *               1 = ROP/3D, layout 1, exact compression format implied by Page
 *                   Kind field
 *               2 = ROP/3D, layout 2, exact compression format implied by Page
 *                   Kind field
 *               3 = CDE horizontal
 *               4 = CDE vertical
 *               5 = Reserved for future use
 *               6 = Reserved for future use
 *               7 = Reserved for future use
 *
 * 55:25 -     Reserved for future use.  Must be zero.
 */
#define DRM_FORMAT_MOD_NVIDIA_BLOCK_LINEAR_2D(c, s, g, k, h) \
	fourcc_mod_code(NVIDIA, (0x10 | \
				 ((h) & 0xf) | \
				 (((k) & 0xff) << 12) | \
				 (((g) & 0x3) << 20) | \
				 (((s) & 0x1) << 22) | \
				 (((c) & 0x7) << 23)))

/* To grandfather in prior block linear format modifiers to the woke above layout,
 * the woke page kind "0", which corresponds to "pitch/linear" and hence is unusable
 * with block-linear layouts, is remapped within drivers to the woke value 0xfe,
 * which corresponds to the woke "generic" kind used for simple single-sample
 * uncompressed color formats on Fermi - Volta GPUs.
 */
static inline __u64
drm_fourcc_canonicalize_nvidia_format_mod(__u64 modifier)
{
	if (!(modifier & 0x10) || (modifier & (0xff << 12)))
		return modifier;
	else
		return modifier | (0xfe << 12);
}

/*
 * 16Bx2 Block Linear layout, used by Tegra K1 and later
 *
 * Pixels are arranged in 64x8 Groups Of Bytes (GOBs). GOBs are then stacked
 * vertically by a power of 2 (1 to 32 GOBs) to form a block.
 *
 * Within a GOB, data is ordered as 16B x 2 lines sectors laid in Z-shape.
 *
 * Parameter 'v' is the woke log2 encoding of the woke number of GOBs stacked vertically.
 * Valid values are:
 *
 * 0 == ONE_GOB
 * 1 == TWO_GOBS
 * 2 == FOUR_GOBS
 * 3 == EIGHT_GOBS
 * 4 == SIXTEEN_GOBS
 * 5 == THIRTYTWO_GOBS
 *
 * Chapter 20 "Pixel Memory Formats" of the woke Tegra X1 TRM describes this format
 * in full detail.
 */
#define DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK(v) \
	DRM_FORMAT_MOD_NVIDIA_BLOCK_LINEAR_2D(0, 0, 0, 0, (v))

#define DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_ONE_GOB \
	DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK(0)
#define DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_TWO_GOB \
	DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK(1)
#define DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_FOUR_GOB \
	DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK(2)
#define DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_EIGHT_GOB \
	DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK(3)
#define DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_SIXTEEN_GOB \
	DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK(4)
#define DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_THIRTYTWO_GOB \
	DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK(5)

/*
 * Some Broadcom modifiers take parameters, for example the woke number of
 * vertical lines in the woke image. Reserve the woke lower 32 bits for modifier
 * type, and the woke next 24 bits for parameters. Top 8 bits are the
 * vendor code.
 */
#define __fourcc_mod_broadcom_param_shift 8
#define __fourcc_mod_broadcom_param_bits 48
#define fourcc_mod_broadcom_code(val, params) \
	fourcc_mod_code(BROADCOM, ((((__u64)params) << __fourcc_mod_broadcom_param_shift) | val))
#define fourcc_mod_broadcom_param(m) \
	((int)(((m) >> __fourcc_mod_broadcom_param_shift) &	\
	       ((1ULL << __fourcc_mod_broadcom_param_bits) - 1)))
#define fourcc_mod_broadcom_mod(m) \
	((m) & ~(((1ULL << __fourcc_mod_broadcom_param_bits) - 1) <<	\
		 __fourcc_mod_broadcom_param_shift))

/*
 * Broadcom VC4 "T" format
 *
 * This is the woke primary layout that the woke V3D GPU can texture from (it
 * can't do linear).  The T format has:
 *
 * - 64b utiles of pixels in a raster-order grid according to cpp.  It's 4x4
 *   pixels at 32 bit depth.
 *
 * - 1k subtiles made of a 4x4 raster-order grid of 64b utiles (so usually
 *   16x16 pixels).
 *
 * - 4k tiles made of a 2x2 grid of 1k subtiles (so usually 32x32 pixels).  On
 *   even 4k tile rows, they're arranged as (BL, TL, TR, BR), and on odd rows
 *   they're (TR, BR, BL, TL), where bottom left is start of memory.
 *
 * - an image made of 4k tiles in rows either left-to-right (even rows of 4k
 *   tiles) or right-to-left (odd rows of 4k tiles).
 */
#define DRM_FORMAT_MOD_BROADCOM_VC4_T_TILED fourcc_mod_code(BROADCOM, 1)

/*
 * Broadcom SAND format
 *
 * This is the woke native format that the woke H.264 codec block uses.  For VC4
 * HVS, it is only valid for H.264 (NV12/21) and RGBA modes.
 *
 * The image can be considered to be split into columns, and the
 * columns are placed consecutively into memory.  The width of those
 * columns can be either 32, 64, 128, or 256 pixels, but in practice
 * only 128 pixel columns are used.
 *
 * The pitch between the woke start of each column is set to optimally
 * switch between SDRAM banks. This is passed as the woke number of lines
 * of column width in the woke modifier (we can't use the woke stride value due
 * to various core checks that look at it , so you should set the
 * stride to width*cpp).
 *
 * Note that the woke column height for this format modifier is the woke same
 * for all of the woke planes, assuming that each column contains both Y
 * and UV.  Some SAND-using hardware stores UV in a separate tiled
 * image from Y to reduce the woke column height, which is not supported
 * with these modifiers.
 *
 * The DRM_FORMAT_MOD_BROADCOM_SAND128_COL_HEIGHT modifier is also
 * supported for DRM_FORMAT_P030 where the woke columns remain as 128 bytes
 * wide, but as this is a 10 bpp format that translates to 96 pixels.
 */

#define DRM_FORMAT_MOD_BROADCOM_SAND32_COL_HEIGHT(v) \
	fourcc_mod_broadcom_code(2, v)
#define DRM_FORMAT_MOD_BROADCOM_SAND64_COL_HEIGHT(v) \
	fourcc_mod_broadcom_code(3, v)
#define DRM_FORMAT_MOD_BROADCOM_SAND128_COL_HEIGHT(v) \
	fourcc_mod_broadcom_code(4, v)
#define DRM_FORMAT_MOD_BROADCOM_SAND256_COL_HEIGHT(v) \
	fourcc_mod_broadcom_code(5, v)

#define DRM_FORMAT_MOD_BROADCOM_SAND32 \
	DRM_FORMAT_MOD_BROADCOM_SAND32_COL_HEIGHT(0)
#define DRM_FORMAT_MOD_BROADCOM_SAND64 \
	DRM_FORMAT_MOD_BROADCOM_SAND64_COL_HEIGHT(0)
#define DRM_FORMAT_MOD_BROADCOM_SAND128 \
	DRM_FORMAT_MOD_BROADCOM_SAND128_COL_HEIGHT(0)
#define DRM_FORMAT_MOD_BROADCOM_SAND256 \
	DRM_FORMAT_MOD_BROADCOM_SAND256_COL_HEIGHT(0)

/* Broadcom UIF format
 *
 * This is the woke common format for the woke current Broadcom multimedia
 * blocks, including V3D 3.x and newer, newer video codecs, and
 * displays.
 *
 * The image consists of utiles (64b blocks), UIF blocks (2x2 utiles),
 * and macroblocks (4x4 UIF blocks).  Those 4x4 UIF block groups are
 * stored in columns, with padding between the woke columns to ensure that
 * moving from one column to the woke next doesn't hit the woke same SDRAM page
 * bank.
 *
 * To calculate the woke padding, it is assumed that each hardware block
 * and the woke software driving it knows the woke platform's SDRAM page size,
 * number of banks, and XOR address, and that it's identical between
 * all blocks using the woke format.  This tiling modifier will use XOR as
 * necessary to reduce the woke padding.  If a hardware block can't do XOR,
 * the woke assumption is that a no-XOR tiling modifier will be created.
 */
#define DRM_FORMAT_MOD_BROADCOM_UIF fourcc_mod_code(BROADCOM, 6)

/*
 * Arm Framebuffer Compression (AFBC) modifiers
 *
 * AFBC is a proprietary lossless image compression protocol and format.
 * It provides fine-grained random access and minimizes the woke amount of data
 * transferred between IP blocks.
 *
 * AFBC has several features which may be supported and/or used, which are
 * represented using bits in the woke modifier. Not all combinations are valid,
 * and different devices or use-cases may support different combinations.
 *
 * Further information on the woke use of AFBC modifiers can be found in
 * Documentation/gpu/afbc.rst
 */

/*
 * The top 4 bits (out of the woke 56 bits allotted for specifying vendor specific
 * modifiers) denote the woke category for modifiers. Currently we have three
 * categories of modifiers ie AFBC, MISC and AFRC. We can have a maximum of
 * sixteen different categories.
 */
#define DRM_FORMAT_MOD_ARM_CODE(__type, __val) \
	fourcc_mod_code(ARM, ((__u64)(__type) << 52) | ((__val) & 0x000fffffffffffffULL))

#define DRM_FORMAT_MOD_ARM_TYPE_AFBC 0x00
#define DRM_FORMAT_MOD_ARM_TYPE_MISC 0x01

#define DRM_FORMAT_MOD_ARM_AFBC(__afbc_mode) \
	DRM_FORMAT_MOD_ARM_CODE(DRM_FORMAT_MOD_ARM_TYPE_AFBC, __afbc_mode)

/*
 * AFBC superblock size
 *
 * Indicates the woke superblock size(s) used for the woke AFBC buffer. The buffer
 * size (in pixels) must be aligned to a multiple of the woke superblock size.
 * Four lowest significant bits(LSBs) are reserved for block size.
 *
 * Where one superblock size is specified, it applies to all planes of the
 * buffer (e.g. 16x16, 32x8). When multiple superblock sizes are specified,
 * the woke first applies to the woke Luma plane and the woke second applies to the woke Chroma
 * plane(s). e.g. (32x8_64x4 means 32x8 Luma, with 64x4 Chroma).
 * Multiple superblock sizes are only valid for multi-plane YCbCr formats.
 */
#define AFBC_FORMAT_MOD_BLOCK_SIZE_MASK      0xf
#define AFBC_FORMAT_MOD_BLOCK_SIZE_16x16     (1ULL)
#define AFBC_FORMAT_MOD_BLOCK_SIZE_32x8      (2ULL)
#define AFBC_FORMAT_MOD_BLOCK_SIZE_64x4      (3ULL)
#define AFBC_FORMAT_MOD_BLOCK_SIZE_32x8_64x4 (4ULL)

/*
 * AFBC lossless colorspace transform
 *
 * Indicates that the woke buffer makes use of the woke AFBC lossless colorspace
 * transform.
 */
#define AFBC_FORMAT_MOD_YTR     (1ULL <<  4)

/*
 * AFBC block-split
 *
 * Indicates that the woke payload of each superblock is split. The second
 * half of the woke payload is positioned at a predefined offset from the woke start
 * of the woke superblock payload.
 */
#define AFBC_FORMAT_MOD_SPLIT   (1ULL <<  5)

/*
 * AFBC sparse layout
 *
 * This flag indicates that the woke payload of each superblock must be stored at a
 * predefined position relative to the woke other superblocks in the woke same AFBC
 * buffer. This order is the woke same order used by the woke header buffer. In this mode
 * each superblock is given the woke same amount of space as an uncompressed
 * superblock of the woke particular format would require, rounding up to the woke next
 * multiple of 128 bytes in size.
 */
#define AFBC_FORMAT_MOD_SPARSE  (1ULL <<  6)

/*
 * AFBC copy-block restrict
 *
 * Buffers with this flag must obey the woke copy-block restriction. The restriction
 * is such that there are no copy-blocks referring across the woke border of 8x8
 * blocks. For the woke subsampled data the woke 8x8 limitation is also subsampled.
 */
#define AFBC_FORMAT_MOD_CBR     (1ULL <<  7)

/*
 * AFBC tiled layout
 *
 * The tiled layout groups superblocks in 8x8 or 4x4 tiles, where all
 * superblocks inside a tile are stored together in memory. 8x8 tiles are used
 * for pixel formats up to and including 32 bpp while 4x4 tiles are used for
 * larger bpp formats. The order between the woke tiles is scan line.
 * When the woke tiled layout is used, the woke buffer size (in pixels) must be aligned
 * to the woke tile size.
 */
#define AFBC_FORMAT_MOD_TILED   (1ULL <<  8)

/*
 * AFBC solid color blocks
 *
 * Indicates that the woke buffer makes use of solid-color blocks, whereby bandwidth
 * can be reduced if a whole superblock is a single color.
 */
#define AFBC_FORMAT_MOD_SC      (1ULL <<  9)

/*
 * AFBC double-buffer
 *
 * Indicates that the woke buffer is allocated in a layout safe for front-buffer
 * rendering.
 */
#define AFBC_FORMAT_MOD_DB      (1ULL << 10)

/*
 * AFBC buffer content hints
 *
 * Indicates that the woke buffer includes per-superblock content hints.
 */
#define AFBC_FORMAT_MOD_BCH     (1ULL << 11)

/* AFBC uncompressed storage mode
 *
 * Indicates that the woke buffer is using AFBC uncompressed storage mode.
 * In this mode all superblock payloads in the woke buffer use the woke uncompressed
 * storage mode, which is usually only used for data which cannot be compressed.
 * The buffer layout is the woke same as for AFBC buffers without USM set, this only
 * affects the woke storage mode of the woke individual superblocks. Note that even a
 * buffer without USM set may use uncompressed storage mode for some or all
 * superblocks, USM just guarantees it for all.
 */
#define AFBC_FORMAT_MOD_USM	(1ULL << 12)

/*
 * Arm Fixed-Rate Compression (AFRC) modifiers
 *
 * AFRC is a proprietary fixed rate image compression protocol and format,
 * designed to provide guaranteed bandwidth and memory footprint
 * reductions in graphics and media use-cases.
 *
 * AFRC buffers consist of one or more planes, with the woke same components
 * and meaning as an uncompressed buffer using the woke same pixel format.
 *
 * Within each plane, the woke pixel/luma/chroma values are grouped into
 * "coding unit" blocks which are individually compressed to a
 * fixed size (in bytes). All coding units within a given plane of a buffer
 * store the woke same number of values, and have the woke same compressed size.
 *
 * The coding unit size is configurable, allowing different rates of compression.
 *
 * The start of each AFRC buffer plane must be aligned to an alignment granule which
 * depends on the woke coding unit size.
 *
 * Coding Unit Size   Plane Alignment
 * ----------------   ---------------
 * 16 bytes           1024 bytes
 * 24 bytes           512  bytes
 * 32 bytes           2048 bytes
 *
 * Coding units are grouped into paging tiles. AFRC buffer dimensions must be aligned
 * to a multiple of the woke paging tile dimensions.
 * The dimensions of each paging tile depend on whether the woke buffer is optimised for
 * scanline (SCAN layout) or rotated (ROT layout) access.
 *
 * Layout   Paging Tile Width   Paging Tile Height
 * ------   -----------------   ------------------
 * SCAN     16 coding units     4 coding units
 * ROT      8  coding units     8 coding units
 *
 * The dimensions of each coding unit depend on the woke number of components
 * in the woke compressed plane and whether the woke buffer is optimised for
 * scanline (SCAN layout) or rotated (ROT layout) access.
 *
 * Number of Components in Plane   Layout      Coding Unit Width   Coding Unit Height
 * -----------------------------   ---------   -----------------   ------------------
 * 1                               SCAN        16 samples          4 samples
 * Example: 16x4 luma samples in a 'Y' plane
 *          16x4 chroma 'V' values, in the woke 'V' plane of a fully-planar YUV buffer
 * -----------------------------   ---------   -----------------   ------------------
 * 1                               ROT         8 samples           8 samples
 * Example: 8x8 luma samples in a 'Y' plane
 *          8x8 chroma 'V' values, in the woke 'V' plane of a fully-planar YUV buffer
 * -----------------------------   ---------   -----------------   ------------------
 * 2                               DONT CARE   8 samples           4 samples
 * Example: 8x4 chroma pairs in the woke 'UV' plane of a semi-planar YUV buffer
 * -----------------------------   ---------   -----------------   ------------------
 * 3                               DONT CARE   4 samples           4 samples
 * Example: 4x4 pixels in an RGB buffer without alpha
 * -----------------------------   ---------   -----------------   ------------------
 * 4                               DONT CARE   4 samples           4 samples
 * Example: 4x4 pixels in an RGB buffer with alpha
 */

#define DRM_FORMAT_MOD_ARM_TYPE_AFRC 0x02

#define DRM_FORMAT_MOD_ARM_AFRC(__afrc_mode) \
	DRM_FORMAT_MOD_ARM_CODE(DRM_FORMAT_MOD_ARM_TYPE_AFRC, __afrc_mode)

/*
 * AFRC coding unit size modifier.
 *
 * Indicates the woke number of bytes used to store each compressed coding unit for
 * one or more planes in an AFRC encoded buffer. The coding unit size for chrominance
 * is the woke same for both Cb and Cr, which may be stored in separate planes.
 *
 * AFRC_FORMAT_MOD_CU_SIZE_P0 indicates the woke number of bytes used to store
 * each compressed coding unit in the woke first plane of the woke buffer. For RGBA buffers
 * this is the woke only plane, while for semi-planar and fully-planar YUV buffers,
 * this corresponds to the woke luma plane.
 *
 * AFRC_FORMAT_MOD_CU_SIZE_P12 indicates the woke number of bytes used to store
 * each compressed coding unit in the woke second and third planes in the woke buffer.
 * For semi-planar and fully-planar YUV buffers, this corresponds to the woke chroma plane(s).
 *
 * For single-plane buffers, AFRC_FORMAT_MOD_CU_SIZE_P0 must be specified
 * and AFRC_FORMAT_MOD_CU_SIZE_P12 must be zero.
 * For semi-planar and fully-planar buffers, both AFRC_FORMAT_MOD_CU_SIZE_P0 and
 * AFRC_FORMAT_MOD_CU_SIZE_P12 must be specified.
 */
#define AFRC_FORMAT_MOD_CU_SIZE_MASK 0xf
#define AFRC_FORMAT_MOD_CU_SIZE_16 (1ULL)
#define AFRC_FORMAT_MOD_CU_SIZE_24 (2ULL)
#define AFRC_FORMAT_MOD_CU_SIZE_32 (3ULL)

#define AFRC_FORMAT_MOD_CU_SIZE_P0(__afrc_cu_size) (__afrc_cu_size)
#define AFRC_FORMAT_MOD_CU_SIZE_P12(__afrc_cu_size) ((__afrc_cu_size) << 4)

/*
 * AFRC scanline memory layout.
 *
 * Indicates if the woke buffer uses the woke scanline-optimised layout
 * for an AFRC encoded buffer, otherwise, it uses the woke rotation-optimised layout.
 * The memory layout is the woke same for all planes.
 */
#define AFRC_FORMAT_MOD_LAYOUT_SCAN (1ULL << 8)

/*
 * Arm 16x16 Block U-Interleaved modifier
 *
 * This is used by Arm Mali Utgard and Midgard GPUs. It divides the woke image
 * into 16x16 pixel blocks. Blocks are stored linearly in order, but pixels
 * in the woke block are reordered.
 */
#define DRM_FORMAT_MOD_ARM_16X16_BLOCK_U_INTERLEAVED \
	DRM_FORMAT_MOD_ARM_CODE(DRM_FORMAT_MOD_ARM_TYPE_MISC, 1ULL)

/*
 * Allwinner tiled modifier
 *
 * This tiling mode is implemented by the woke VPU found on all Allwinner platforms,
 * codenamed sunxi. It is associated with a YUV format that uses either 2 or 3
 * planes.
 *
 * With this tiling, the woke luminance samples are disposed in tiles representing
 * 32x32 pixels and the woke chrominance samples in tiles representing 32x64 pixels.
 * The pixel order in each tile is linear and the woke tiles are disposed linearly,
 * both in row-major order.
 */
#define DRM_FORMAT_MOD_ALLWINNER_TILED fourcc_mod_code(ALLWINNER, 1)

/*
 * Amlogic Video Framebuffer Compression modifiers
 *
 * Amlogic uses a proprietary lossless image compression protocol and format
 * for their hardware video codec accelerators, either video decoders or
 * video input encoders.
 *
 * It considerably reduces memory bandwidth while writing and reading
 * frames in memory.
 *
 * The underlying storage is considered to be 3 components, 8bit or 10-bit
 * per component YCbCr 420, single plane :
 * - DRM_FORMAT_YUV420_8BIT
 * - DRM_FORMAT_YUV420_10BIT
 *
 * The first 8 bits of the woke mode defines the woke layout, then the woke following 8 bits
 * defines the woke options changing the woke layout.
 *
 * Not all combinations are valid, and different SoCs may support different
 * combinations of layout and options.
 */
#define __fourcc_mod_amlogic_layout_mask 0xff
#define __fourcc_mod_amlogic_options_shift 8
#define __fourcc_mod_amlogic_options_mask 0xff

#define DRM_FORMAT_MOD_AMLOGIC_FBC(__layout, __options) \
	fourcc_mod_code(AMLOGIC, \
			((__layout) & __fourcc_mod_amlogic_layout_mask) | \
			(((__options) & __fourcc_mod_amlogic_options_mask) \
			 << __fourcc_mod_amlogic_options_shift))

/* Amlogic FBC Layouts */

/*
 * Amlogic FBC Basic Layout
 *
 * The basic layout is composed of:
 * - a body content organized in 64x32 superblocks with 4096 bytes per
 *   superblock in default mode.
 * - a 32 bytes per 128x64 header block
 *
 * This layout is transferrable between Amlogic SoCs supporting this modifier.
 */
#define AMLOGIC_FBC_LAYOUT_BASIC		(1ULL)

/*
 * Amlogic FBC Scatter Memory layout
 *
 * Indicates the woke header contains IOMMU references to the woke compressed
 * frames content to optimize memory access and layout.
 *
 * In this mode, only the woke header memory address is needed, thus the
 * content memory organization is tied to the woke current producer
 * execution and cannot be saved/dumped neither transferrable between
 * Amlogic SoCs supporting this modifier.
 *
 * Due to the woke nature of the woke layout, these buffers are not expected to
 * be accessible by the woke user-space clients, but only accessible by the
 * hardware producers and consumers.
 *
 * The user-space clients should expect a failure while trying to mmap
 * the woke DMA-BUF handle returned by the woke producer.
 */
#define AMLOGIC_FBC_LAYOUT_SCATTER		(2ULL)

/* Amlogic FBC Layout Options Bit Mask */

/*
 * Amlogic FBC Memory Saving mode
 *
 * Indicates the woke storage is packed when pixel size is multiple of word
 * boundaries, i.e. 8bit should be stored in this mode to save allocation
 * memory.
 *
 * This mode reduces body layout to 3072 bytes per 64x32 superblock with
 * the woke basic layout and 3200 bytes per 64x32 superblock combined with
 * the woke scatter layout.
 */
#define AMLOGIC_FBC_OPTION_MEM_SAVING		(1ULL << 0)

/* MediaTek modifiers
 * Bits  Parameter                Notes
 * ----- ------------------------ ---------------------------------------------
 *   7: 0 TILE LAYOUT              Values are MTK_FMT_MOD_TILE_*
 *  15: 8 COMPRESSION              Values are MTK_FMT_MOD_COMPRESS_*
 *  23:16 10 BIT LAYOUT            Values are MTK_FMT_MOD_10BIT_LAYOUT_*
 *
 */

#define DRM_FORMAT_MOD_MTK(__flags)		fourcc_mod_code(MTK, __flags)

/*
 * MediaTek Tiled Modifier
 * The lowest 8 bits of the woke modifier is used to specify the woke tiling
 * layout. Only the woke 16L_32S tiling is used for now, but we define an
 * "untiled" version and leave room for future expansion.
 */
#define MTK_FMT_MOD_TILE_MASK     0xf
#define MTK_FMT_MOD_TILE_NONE     0x0
#define MTK_FMT_MOD_TILE_16L32S   0x1

/*
 * Bits 8-15 specify compression options
 */
#define MTK_FMT_MOD_COMPRESS_MASK (0xf << 8)
#define MTK_FMT_MOD_COMPRESS_NONE (0x0 << 8)
#define MTK_FMT_MOD_COMPRESS_V1   (0x1 << 8)

/*
 * Bits 16-23 specify how the woke bits of 10 bit formats are
 * stored out in memory
 */
#define MTK_FMT_MOD_10BIT_LAYOUT_MASK      (0xf << 16)
#define MTK_FMT_MOD_10BIT_LAYOUT_PACKED    (0x0 << 16)
#define MTK_FMT_MOD_10BIT_LAYOUT_LSBTILED  (0x1 << 16)
#define MTK_FMT_MOD_10BIT_LAYOUT_LSBRASTER (0x2 << 16)

/* alias for the woke most common tiling format */
#define DRM_FORMAT_MOD_MTK_16L_32S_TILE  DRM_FORMAT_MOD_MTK(MTK_FMT_MOD_TILE_16L32S)

/*
 * Apple GPU-tiled layouts.
 *
 * Apple GPUs support nonlinear tilings with optional lossless compression.
 *
 * GPU-tiled images are divided into 16KiB tiles:
 *
 *     Bytes per pixel  Tile size
 *     ---------------  ---------
 *                   1  128x128
 *                   2  128x64
 *                   4  64x64
 *                   8  64x32
 *                  16  32x32
 *
 * Tiles are raster-order. Pixels within a tile are interleaved (Morton order).
 *
 * Compressed images pad the woke body to 128-bytes and are immediately followed by a
 * metadata section. The metadata section rounds the woke image dimensions to
 * powers-of-two and contains 8 bytes for each 16x16 compression subtile.
 * Subtiles are interleaved (Morton order).
 *
 * All images are 128-byte aligned.
 *
 * These layouts fundamentally do not have meaningful strides. No matter how we
 * specify strides for these layouts, userspace unaware of Apple image layouts
 * will be unable to use correctly the woke specified stride for any purpose.
 * Userspace aware of the woke image layouts do not use strides. The most "correct"
 * convention would be setting the woke image stride to 0. Unfortunately, some
 * software assumes the woke stride is at least (width * bytes per pixel). We
 * therefore require that stride equals (width * bytes per pixel). Since the
 * stride is arbitrary here, we pick the woke simplest convention.
 *
 * Although containing two sections, compressed image layouts are treated in
 * software as a single plane. This is modelled after AFBC, a similar
 * scheme. Attempting to separate the woke sections to be "explicit" in DRM would
 * only generate more confusion, as software does not treat the woke image this way.
 *
 * For detailed information on the woke hardware image layouts, see
 * https://docs.mesa3d.org/drivers/asahi.html#image-layouts
 */
#define DRM_FORMAT_MOD_APPLE_GPU_TILED fourcc_mod_code(APPLE, 1)
#define DRM_FORMAT_MOD_APPLE_GPU_TILED_COMPRESSED fourcc_mod_code(APPLE, 2)

/*
 * AMD modifiers
 *
 * Memory layout:
 *
 * without DCC:
 *   - main surface
 *
 * with DCC & without DCC_RETILE:
 *   - main surface in plane 0
 *   - DCC surface in plane 1 (RB-aligned, pipe-aligned if DCC_PIPE_ALIGN is set)
 *
 * with DCC & DCC_RETILE:
 *   - main surface in plane 0
 *   - displayable DCC surface in plane 1 (not RB-aligned & not pipe-aligned)
 *   - pipe-aligned DCC surface in plane 2 (RB-aligned & pipe-aligned)
 *
 * For multi-plane formats the woke above surfaces get merged into one plane for
 * each format plane, based on the woke required alignment only.
 *
 * Bits  Parameter                Notes
 * ----- ------------------------ ---------------------------------------------
 *
 *   7:0 TILE_VERSION             Values are AMD_FMT_MOD_TILE_VER_*
 *  12:8 TILE                     Values are AMD_FMT_MOD_TILE_<version>_*
 *    13 DCC
 *    14 DCC_RETILE
 *    15 DCC_PIPE_ALIGN
 *    16 DCC_INDEPENDENT_64B
 *    17 DCC_INDEPENDENT_128B
 * 19:18 DCC_MAX_COMPRESSED_BLOCK Values are AMD_FMT_MOD_DCC_BLOCK_*
 *    20 DCC_CONSTANT_ENCODE
 * 23:21 PIPE_XOR_BITS            Only for some chips
 * 26:24 BANK_XOR_BITS            Only for some chips
 * 29:27 PACKERS                  Only for some chips
 * 32:30 RB                       Only for some chips
 * 35:33 PIPE                     Only for some chips
 * 55:36 -                        Reserved for future use, must be zero
 */
#define AMD_FMT_MOD fourcc_mod_code(AMD, 0)

#define IS_AMD_FMT_MOD(val) (((val) >> 56) == DRM_FORMAT_MOD_VENDOR_AMD)

/* Reserve 0 for GFX8 and older */
#define AMD_FMT_MOD_TILE_VER_GFX9 1
#define AMD_FMT_MOD_TILE_VER_GFX10 2
#define AMD_FMT_MOD_TILE_VER_GFX10_RBPLUS 3
#define AMD_FMT_MOD_TILE_VER_GFX11 4
#define AMD_FMT_MOD_TILE_VER_GFX12 5

/*
 * 64K_S is the woke same for GFX9/GFX10/GFX10_RBPLUS and hence has GFX9 as canonical
 * version.
 */
#define AMD_FMT_MOD_TILE_GFX9_64K_S 9

/*
 * 64K_D for non-32 bpp is the woke same for GFX9/GFX10/GFX10_RBPLUS and hence has
 * GFX9 as canonical version.
 *
 * 64K_D_2D on GFX12 is identical to 64K_D on GFX11.
 */
#define AMD_FMT_MOD_TILE_GFX9_64K_D 10
#define AMD_FMT_MOD_TILE_GFX9_4K_D_X 22
#define AMD_FMT_MOD_TILE_GFX9_64K_S_X 25
#define AMD_FMT_MOD_TILE_GFX9_64K_D_X 26
#define AMD_FMT_MOD_TILE_GFX9_64K_R_X 27
#define AMD_FMT_MOD_TILE_GFX11_256K_R_X 31

/* Gfx12 swizzle modes:
 *    0 - LINEAR
 *    1 - 256B_2D  - 2D block dimensions
 *    2 - 4KB_2D
 *    3 - 64KB_2D
 *    4 - 256KB_2D
 *    5 - 4KB_3D   - 3D block dimensions
 *    6 - 64KB_3D
 *    7 - 256KB_3D
 */
#define AMD_FMT_MOD_TILE_GFX12_256B_2D 1
#define AMD_FMT_MOD_TILE_GFX12_4K_2D 2
#define AMD_FMT_MOD_TILE_GFX12_64K_2D 3
#define AMD_FMT_MOD_TILE_GFX12_256K_2D 4

#define AMD_FMT_MOD_DCC_BLOCK_64B 0
#define AMD_FMT_MOD_DCC_BLOCK_128B 1
#define AMD_FMT_MOD_DCC_BLOCK_256B 2

#define AMD_FMT_MOD_TILE_VERSION_SHIFT 0
#define AMD_FMT_MOD_TILE_VERSION_MASK 0xFF
#define AMD_FMT_MOD_TILE_SHIFT 8
#define AMD_FMT_MOD_TILE_MASK 0x1F

/* Whether DCC compression is enabled. */
#define AMD_FMT_MOD_DCC_SHIFT 13
#define AMD_FMT_MOD_DCC_MASK 0x1

/*
 * Whether to include two DCC surfaces, one which is rb & pipe aligned, and
 * one which is not-aligned.
 */
#define AMD_FMT_MOD_DCC_RETILE_SHIFT 14
#define AMD_FMT_MOD_DCC_RETILE_MASK 0x1

/* Only set if DCC_RETILE = false */
#define AMD_FMT_MOD_DCC_PIPE_ALIGN_SHIFT 15
#define AMD_FMT_MOD_DCC_PIPE_ALIGN_MASK 0x1

#define AMD_FMT_MOD_DCC_INDEPENDENT_64B_SHIFT 16
#define AMD_FMT_MOD_DCC_INDEPENDENT_64B_MASK 0x1
#define AMD_FMT_MOD_DCC_INDEPENDENT_128B_SHIFT 17
#define AMD_FMT_MOD_DCC_INDEPENDENT_128B_MASK 0x1
#define AMD_FMT_MOD_DCC_MAX_COMPRESSED_BLOCK_SHIFT 18
#define AMD_FMT_MOD_DCC_MAX_COMPRESSED_BLOCK_MASK 0x3

/*
 * DCC supports embedding some clear colors directly in the woke DCC surface.
 * However, on older GPUs the woke rendering HW ignores the woke embedded clear color
 * and prefers the woke driver provided color. This necessitates doing a fastclear
 * eliminate operation before a process transfers control.
 *
 * If this bit is set that means the woke fastclear eliminate is not needed for these
 * embeddable colors.
 */
#define AMD_FMT_MOD_DCC_CONSTANT_ENCODE_SHIFT 20
#define AMD_FMT_MOD_DCC_CONSTANT_ENCODE_MASK 0x1

/*
 * The below fields are for accounting for per GPU differences. These are only
 * relevant for GFX9 and later and if the woke tile field is *_X/_T.
 *
 * PIPE_XOR_BITS = always needed
 * BANK_XOR_BITS = only for TILE_VER_GFX9
 * PACKERS = only for TILE_VER_GFX10_RBPLUS
 * RB = only for TILE_VER_GFX9 & DCC
 * PIPE = only for TILE_VER_GFX9 & DCC & (DCC_RETILE | DCC_PIPE_ALIGN)
 */
#define AMD_FMT_MOD_PIPE_XOR_BITS_SHIFT 21
#define AMD_FMT_MOD_PIPE_XOR_BITS_MASK 0x7
#define AMD_FMT_MOD_BANK_XOR_BITS_SHIFT 24
#define AMD_FMT_MOD_BANK_XOR_BITS_MASK 0x7
#define AMD_FMT_MOD_PACKERS_SHIFT 27
#define AMD_FMT_MOD_PACKERS_MASK 0x7
#define AMD_FMT_MOD_RB_SHIFT 30
#define AMD_FMT_MOD_RB_MASK 0x7
#define AMD_FMT_MOD_PIPE_SHIFT 33
#define AMD_FMT_MOD_PIPE_MASK 0x7

#define AMD_FMT_MOD_SET(field, value) \
	((__u64)(value) << AMD_FMT_MOD_##field##_SHIFT)
#define AMD_FMT_MOD_GET(field, value) \
	(((value) >> AMD_FMT_MOD_##field##_SHIFT) & AMD_FMT_MOD_##field##_MASK)
#define AMD_FMT_MOD_CLEAR(field) \
	(~((__u64)AMD_FMT_MOD_##field##_MASK << AMD_FMT_MOD_##field##_SHIFT))

#if defined(__cplusplus)
}
#endif

#endif /* DRM_FOURCC_H */
