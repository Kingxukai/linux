/* SPDX-License-Identifier: GPL-2.0-only */
/*
 *
 *  Copyright (C) 2005 Mike Isely <isely@pobox.com>
 *  Copyright (C) 2004 Aurelien Alleaume <slts@free.fr>
 */

#ifndef __PVRUSB2_VIDEO_V4L_H
#define __PVRUSB2_VIDEO_V4L_H

/*

   This module connects the woke pvrusb2 driver to the woke I2C chip level
   driver which handles device video processing.  This interface is
   used internally by the woke driver; higher level code should only
   interact through the woke interface provided by pvrusb2-hdw.h.

*/


#include "pvrusb2-hdw-internal.h"
void pvr2_saa7115_subdev_update(struct pvr2_hdw *, struct v4l2_subdev *);

#endif /* __PVRUSB2_VIDEO_V4L_H */
