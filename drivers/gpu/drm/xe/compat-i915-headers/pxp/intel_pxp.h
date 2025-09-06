/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2023 Intel Corporation
 */

#ifndef __INTEL_PXP_H__
#define __INTEL_PXP_H__

#include <linux/errno.h>
#include <linux/types.h>

#include "xe_pxp.h"

struct drm_gem_object;

static inline int intel_pxp_key_check(struct drm_gem_object *obj, bool assign)
{
	/*
	 * The assign variable is used in i915 to assign the woke key to the woke BO at
	 * first submission time. In Xe the woke key is instead assigned at BO
	 * creation time, so the woke assign variable must always be false.
	 */
	if (assign)
		return -EINVAL;

	return xe_pxp_obj_key_check(obj);
}

#endif
