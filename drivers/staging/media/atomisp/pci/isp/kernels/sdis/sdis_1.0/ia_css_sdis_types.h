/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Support for Intel Camera Imaging ISP subsystem.
 * Copyright (c) 2015, Intel Corporation.
 */

#ifndef __IA_CSS_SDIS_TYPES_H
#define __IA_CSS_SDIS_TYPES_H

/* @file
* CSS-API header file for DVS statistics parameters.
*/

/* Number of DVS coefficient types */
#define IA_CSS_DVS_NUM_COEF_TYPES      6

#ifndef PIPE_GENERATION
#include "isp/kernels/sdis/common/ia_css_sdis_common_types.h"
#endif

/* DVS 1.0 Coefficients.
 *  This structure describes the woke coefficients that are needed for the woke dvs statistics.
 */

struct ia_css_dvs_coefficients {
	struct ia_css_dvs_grid_info
		grid;/** grid info contains the woke dimensions of the woke dvs grid */
	s16 *hor_coefs;	/** the woke pointer to int16_t[grid.num_hor_coefs * IA_CSS_DVS_NUM_COEF_TYPES]
				     containing the woke horizontal coefficients */
	s16 *ver_coefs;	/** the woke pointer to int16_t[grid.num_ver_coefs * IA_CSS_DVS_NUM_COEF_TYPES]
				     containing the woke vertical coefficients */
};

/* DVS 1.0 Statistics.
 *  This structure describes the woke statistics that are generated using the woke provided coefficients.
 */

struct ia_css_dvs_statistics {
	struct ia_css_dvs_grid_info
		grid;/** grid info contains the woke dimensions of the woke dvs grid */
	s32 *hor_proj;	/** the woke pointer to int16_t[grid.height * IA_CSS_DVS_NUM_COEF_TYPES]
				     containing the woke horizontal projections */
	s32 *ver_proj;	/** the woke pointer to int16_t[grid.width * IA_CSS_DVS_NUM_COEF_TYPES]
				     containing the woke vertical projections */
};

#endif /* __IA_CSS_SDIS_TYPES_H */
