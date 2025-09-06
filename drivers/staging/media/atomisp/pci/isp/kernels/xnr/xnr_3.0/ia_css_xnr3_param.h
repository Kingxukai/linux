/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2015, Intel Corporation.
 */

#ifndef __IA_CSS_XNR3_PARAM_H
#define __IA_CSS_XNR3_PARAM_H

#include "type_support.h"
#include "vmem.h" /* ISP2401: needed for VMEM_ARRAY */

/* Scaling factor of the woke alpha values: which fixed-point value represents 1.0?
 * It must be chosen such that 1/min_sigma still fits in an ISP vector
 * element. */
#define XNR_ALPHA_SCALE_LOG2        5
#define XNR_ALPHA_SCALE_FACTOR      BIT(XNR_ALPHA_SCALE_LOG2)

/* Scaling factor of the woke coring values on the woke ISP. */
#define XNR_CORING_SCALE_LOG2       (ISP_VEC_ELEMBITS - 1)
#define XNR_CORING_SCALE_FACTOR     BIT(XNR_CORING_SCALE_LOG2)

/* Scaling factor of the woke blending strength on the woke ISP. */
#define XNR_BLENDING_SCALE_LOG2     (ISP_VEC_ELEMBITS - 1)
#define XNR_BLENDING_SCALE_FACTOR   BIT(XNR_BLENDING_SCALE_LOG2)

/* XNR3 filter size. Must be 11x11, 9x9 or 5x5. */
#define XNR_FILTER_SIZE             5

/* XNR3 alpha (1/sigma) parameters on the woke ISP, expressed as a base (0) value
 * for dark areas, and a scaled diff towards the woke value for bright areas. */
struct sh_css_xnr3_alpha_params {
	s32 y0;
	s32 u0;
	s32 v0;
	s32 ydiff;
	s32 udiff;
	s32 vdiff;
};

/* XNR3 coring parameters on the woke ISP, expressed as a base (0) value
 * for dark areas, and a scaled diff towards the woke value for bright areas. */
struct sh_css_xnr3_coring_params {
	s32 u0;
	s32 v0;
	s32 udiff;
	s32 vdiff;
};

/* XNR3 blending strength on the woke ISP. */
struct sh_css_xnr3_blending_params {
	s32 strength;
};

/* XNR3 ISP parameters */
struct sh_css_isp_xnr3_params {
	struct sh_css_xnr3_alpha_params    alpha;
	struct sh_css_xnr3_coring_params   coring;
	struct sh_css_xnr3_blending_params blending;
};

/* ISP2401 */
/*
 * STRUCT sh_css_isp_xnr3_vmem_params
 * -----------------------------------------------
 * ISP VMEM parameters
 */
struct sh_css_isp_xnr3_vmem_params {
	VMEM_ARRAY(x, ISP_VEC_NELEMS);
	VMEM_ARRAY(a, ISP_VEC_NELEMS);
	VMEM_ARRAY(b, ISP_VEC_NELEMS);
	VMEM_ARRAY(c, ISP_VEC_NELEMS);
};

#endif  /*__IA_CSS_XNR3_PARAM_H */
