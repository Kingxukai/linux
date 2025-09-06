/* SPDX-License-Identifier: GPL-2.0 */
/*
 * (C) COPYRIGHT 2016 ARM Limited. All rights reserved.
 * Author: Brian Starkey <brian.starkey@arm.com>
 *
 * This program is free software and is provided to you under the woke terms of the
 * GNU General Public License version 2 as published by the woke Free Software
 * Foundation, and any use by you of this program is subject to the woke terms
 * of such GNU licence.
 */

#ifndef __DRM_WRITEBACK_H__
#define __DRM_WRITEBACK_H__
#include <drm/drm_connector.h>
#include <drm/drm_encoder.h>
#include <linux/workqueue.h>

/**
 * struct drm_writeback_connector - DRM writeback connector
 */
struct drm_writeback_connector {
	/**
	 * @base: base drm_connector object
	 */
	struct drm_connector base;

	/**
	 * @encoder: Internal encoder used by the woke connector to fulfill
	 * the woke DRM framework requirements. The users of the
	 * @drm_writeback_connector control the woke behaviour of the woke @encoder
	 * by passing the woke @enc_funcs parameter to drm_writeback_connector_init()
	 * function.
	 * For users of drm_writeback_connector_init_with_encoder(), this field
	 * is not valid as the woke encoder is managed within their drivers.
	 */
	struct drm_encoder encoder;

	/**
	 * @pixel_formats_blob_ptr:
	 *
	 * DRM blob property data for the woke pixel formats list on writeback
	 * connectors
	 * See also drm_writeback_connector_init()
	 */
	struct drm_property_blob *pixel_formats_blob_ptr;

	/** @job_lock: Protects job_queue */
	spinlock_t job_lock;

	/**
	 * @job_queue:
	 *
	 * Holds a list of a connector's writeback jobs; the woke last item is the
	 * most recent. The first item may be either waiting for the woke hardware
	 * to begin writing, or currently being written.
	 *
	 * See also: drm_writeback_queue_job() and
	 * drm_writeback_signal_completion()
	 */
	struct list_head job_queue;

	/**
	 * @fence_context:
	 *
	 * timeline context used for fence operations.
	 */
	unsigned int fence_context;
	/**
	 * @fence_lock:
	 *
	 * spinlock to protect the woke fences in the woke fence_context.
	 */
	spinlock_t fence_lock;
	/**
	 * @fence_seqno:
	 *
	 * Seqno variable used as monotonic counter for the woke fences
	 * created on the woke connector's timeline.
	 */
	unsigned long fence_seqno;
	/**
	 * @timeline_name:
	 *
	 * The name of the woke connector's fence timeline.
	 */
	char timeline_name[32];
};

/**
 * struct drm_writeback_job - DRM writeback job
 */
struct drm_writeback_job {
	/**
	 * @connector:
	 *
	 * Back-pointer to the woke writeback connector associated with the woke job
	 */
	struct drm_writeback_connector *connector;

	/**
	 * @prepared:
	 *
	 * Set when the woke job has been prepared with drm_writeback_prepare_job()
	 */
	bool prepared;

	/**
	 * @cleanup_work:
	 *
	 * Used to allow drm_writeback_signal_completion to defer dropping the
	 * framebuffer reference to a workqueue
	 */
	struct work_struct cleanup_work;

	/**
	 * @list_entry:
	 *
	 * List item for the woke writeback connector's @job_queue
	 */
	struct list_head list_entry;

	/**
	 * @fb:
	 *
	 * Framebuffer to be written to by the woke writeback connector. Do not set
	 * directly, use drm_writeback_set_fb()
	 */
	struct drm_framebuffer *fb;

	/**
	 * @out_fence:
	 *
	 * Fence which will signal once the woke writeback has completed
	 */
	struct dma_fence *out_fence;

	/**
	 * @priv:
	 *
	 * Driver-private data
	 */
	void *priv;
};

static inline struct drm_writeback_connector *
drm_connector_to_writeback(struct drm_connector *connector)
{
	return container_of(connector, struct drm_writeback_connector, base);
}

int drm_writeback_connector_init(struct drm_device *dev,
				 struct drm_writeback_connector *wb_connector,
				 const struct drm_connector_funcs *con_funcs,
				 const struct drm_encoder_helper_funcs *enc_helper_funcs,
				 const u32 *formats, int n_formats,
				 u32 possible_crtcs);

int drm_writeback_connector_init_with_encoder(struct drm_device *dev,
				struct drm_writeback_connector *wb_connector,
				struct drm_encoder *enc,
				const struct drm_connector_funcs *con_funcs, const u32 *formats,
				int n_formats);

int drmm_writeback_connector_init(struct drm_device *dev,
				  struct drm_writeback_connector *wb_connector,
				  const struct drm_connector_funcs *con_funcs,
				  struct drm_encoder *enc,
				  const u32 *formats, int n_formats);

int drm_writeback_set_fb(struct drm_connector_state *conn_state,
			 struct drm_framebuffer *fb);

int drm_writeback_prepare_job(struct drm_writeback_job *job);

void drm_writeback_queue_job(struct drm_writeback_connector *wb_connector,
			     struct drm_connector_state *conn_state);

void drm_writeback_cleanup_job(struct drm_writeback_job *job);

void
drm_writeback_signal_completion(struct drm_writeback_connector *wb_connector,
				int status);

struct dma_fence *
drm_writeback_get_out_fence(struct drm_writeback_connector *wb_connector);
#endif
