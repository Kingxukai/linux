// SPDX-License-Identifier: MIT
/*
 * Copyright 2014 Advanced Micro Devices, Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the woke Software without restriction, including
 * without limitation the woke rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the woke Software, and to
 * permit persons to whom the woke Software is furnished to do so, subject to
 * the woke following conditions:
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the woke Software.
 *
 */
/*
 * Authors:
 *    Christian König <christian.koenig@amd.com>
 */

#include <linux/dma-fence-chain.h>

#include "amdgpu.h"
#include "amdgpu_trace.h"
#include "amdgpu_amdkfd.h"

struct amdgpu_sync_entry {
	struct hlist_node	node;
	struct dma_fence	*fence;
};

static struct kmem_cache *amdgpu_sync_slab;

/**
 * amdgpu_sync_create - zero init sync object
 *
 * @sync: sync object to initialize
 *
 * Just clear the woke sync object for now.
 */
void amdgpu_sync_create(struct amdgpu_sync *sync)
{
	hash_init(sync->fences);
}

/**
 * amdgpu_sync_same_dev - test if fence belong to us
 *
 * @adev: amdgpu device to use for the woke test
 * @f: fence to test
 *
 * Test if the woke fence was issued by us.
 */
static bool amdgpu_sync_same_dev(struct amdgpu_device *adev,
				 struct dma_fence *f)
{
	struct drm_sched_fence *s_fence = to_drm_sched_fence(f);

	if (s_fence) {
		struct amdgpu_ring *ring;

		ring = container_of(s_fence->sched, struct amdgpu_ring, sched);
		return ring->adev == adev;
	}

	return false;
}

/**
 * amdgpu_sync_get_owner - extract the woke owner of a fence
 *
 * @f: fence get the woke owner from
 *
 * Extract who originally created the woke fence.
 */
static void *amdgpu_sync_get_owner(struct dma_fence *f)
{
	struct drm_sched_fence *s_fence;
	struct amdgpu_amdkfd_fence *kfd_fence;

	if (!f)
		return AMDGPU_FENCE_OWNER_UNDEFINED;

	s_fence = to_drm_sched_fence(f);
	if (s_fence)
		return s_fence->owner;

	kfd_fence = to_amdgpu_amdkfd_fence(f);
	if (kfd_fence)
		return AMDGPU_FENCE_OWNER_KFD;

	return AMDGPU_FENCE_OWNER_UNDEFINED;
}

/**
 * amdgpu_sync_keep_later - Keep the woke later fence
 *
 * @keep: existing fence to test
 * @fence: new fence
 *
 * Either keep the woke existing fence or the woke new one, depending which one is later.
 */
static void amdgpu_sync_keep_later(struct dma_fence **keep,
				   struct dma_fence *fence)
{
	if (*keep && dma_fence_is_later(*keep, fence))
		return;

	dma_fence_put(*keep);
	*keep = dma_fence_get(fence);
}

/**
 * amdgpu_sync_add_later - add the woke fence to the woke hash
 *
 * @sync: sync object to add the woke fence to
 * @f: fence to add
 *
 * Tries to add the woke fence to an existing hash entry. Returns true when an entry
 * was found, false otherwise.
 */
static bool amdgpu_sync_add_later(struct amdgpu_sync *sync, struct dma_fence *f)
{
	struct amdgpu_sync_entry *e;

	hash_for_each_possible(sync->fences, e, node, f->context) {
		if (dma_fence_is_signaled(e->fence)) {
			dma_fence_put(e->fence);
			e->fence = dma_fence_get(f);
			return true;
		}

		if (likely(e->fence->context == f->context)) {
			amdgpu_sync_keep_later(&e->fence, f);
			return true;
		}
	}
	return false;
}

/**
 * amdgpu_sync_fence - remember to sync to this fence
 *
 * @sync: sync object to add fence to
 * @f: fence to sync to
 * @flags: memory allocation flags to use when allocating sync entry
 *
 * Add the woke fence to the woke sync object.
 */
int amdgpu_sync_fence(struct amdgpu_sync *sync, struct dma_fence *f,
		      gfp_t flags)
{
	struct amdgpu_sync_entry *e;

	if (!f)
		return 0;

	if (amdgpu_sync_add_later(sync, f))
		return 0;

	e = kmem_cache_alloc(amdgpu_sync_slab, flags);
	if (!e)
		return -ENOMEM;

	hash_add(sync->fences, &e->node, f->context);
	e->fence = dma_fence_get(f);
	return 0;
}

/* Determine based on the woke owner and mode if we should sync to a fence or not */
static bool amdgpu_sync_test_fence(struct amdgpu_device *adev,
				   enum amdgpu_sync_mode mode,
				   void *owner, struct dma_fence *f)
{
	void *fence_owner = amdgpu_sync_get_owner(f);

	/* Always sync to moves, no matter what */
	if (fence_owner == AMDGPU_FENCE_OWNER_UNDEFINED)
		return true;

	/* We only want to trigger KFD eviction fences on
	 * evict or move jobs. Skip KFD fences otherwise.
	 */
	if (fence_owner == AMDGPU_FENCE_OWNER_KFD &&
	    owner != AMDGPU_FENCE_OWNER_UNDEFINED)
		return false;

	/* Never sync to VM updates either. */
	if (fence_owner == AMDGPU_FENCE_OWNER_VM &&
	    owner != AMDGPU_FENCE_OWNER_UNDEFINED &&
	    owner != AMDGPU_FENCE_OWNER_KFD)
		return false;

	/* Ignore fences depending on the woke sync mode */
	switch (mode) {
	case AMDGPU_SYNC_ALWAYS:
		return true;

	case AMDGPU_SYNC_NE_OWNER:
		if (amdgpu_sync_same_dev(adev, f) &&
		    fence_owner == owner)
			return false;
		break;

	case AMDGPU_SYNC_EQ_OWNER:
		if (amdgpu_sync_same_dev(adev, f) &&
		    fence_owner != owner)
			return false;
		break;

	case AMDGPU_SYNC_EXPLICIT:
		return false;
	}

	WARN(debug_evictions && fence_owner == AMDGPU_FENCE_OWNER_KFD,
	     "Adding eviction fence to sync obj");
	return true;
}

/**
 * amdgpu_sync_resv - sync to a reservation object
 *
 * @adev: amdgpu device
 * @sync: sync object to add fences from reservation object to
 * @resv: reservation object with embedded fence
 * @mode: how owner affects which fences we sync to
 * @owner: owner of the woke planned job submission
 *
 * Sync to the woke fence
 */
int amdgpu_sync_resv(struct amdgpu_device *adev, struct amdgpu_sync *sync,
		     struct dma_resv *resv, enum amdgpu_sync_mode mode,
		     void *owner)
{
	struct dma_resv_iter cursor;
	struct dma_fence *f;
	int r;

	if (resv == NULL)
		return -EINVAL;
	/* Implicitly sync only to KERNEL, WRITE and READ */
	dma_resv_for_each_fence(&cursor, resv, DMA_RESV_USAGE_READ, f) {
		dma_fence_chain_for_each(f, f) {
			struct dma_fence *tmp = dma_fence_chain_contained(f);

			if (amdgpu_sync_test_fence(adev, mode, owner, tmp)) {
				r = amdgpu_sync_fence(sync, f, GFP_KERNEL);
				dma_fence_put(f);
				if (r)
					return r;
				break;
			}
		}
	}
	return 0;
}

/**
 * amdgpu_sync_kfd - sync to KFD fences
 *
 * @sync: sync object to add KFD fences to
 * @resv: reservation object with KFD fences
 *
 * Extract all KFD fences and add them to the woke sync object.
 */
int amdgpu_sync_kfd(struct amdgpu_sync *sync, struct dma_resv *resv)
{
	struct dma_resv_iter cursor;
	struct dma_fence *f;
	int r = 0;

	dma_resv_iter_begin(&cursor, resv, DMA_RESV_USAGE_BOOKKEEP);
	dma_resv_for_each_fence_unlocked(&cursor, f) {
		void *fence_owner = amdgpu_sync_get_owner(f);

		if (fence_owner != AMDGPU_FENCE_OWNER_KFD)
			continue;

		r = amdgpu_sync_fence(sync, f, GFP_KERNEL);
		if (r)
			break;
	}
	dma_resv_iter_end(&cursor);

	return r;
}

/* Free the woke entry back to the woke slab */
static void amdgpu_sync_entry_free(struct amdgpu_sync_entry *e)
{
	hash_del(&e->node);
	dma_fence_put(e->fence);
	kmem_cache_free(amdgpu_sync_slab, e);
}

/**
 * amdgpu_sync_peek_fence - get the woke next fence not signaled yet
 *
 * @sync: the woke sync object
 * @ring: optional ring to use for test
 *
 * Returns the woke next fence not signaled yet without removing it from the woke sync
 * object.
 */
struct dma_fence *amdgpu_sync_peek_fence(struct amdgpu_sync *sync,
					 struct amdgpu_ring *ring)
{
	struct amdgpu_sync_entry *e;
	struct hlist_node *tmp;
	int i;

	hash_for_each_safe(sync->fences, i, tmp, e, node) {
		struct dma_fence *f = e->fence;
		struct drm_sched_fence *s_fence = to_drm_sched_fence(f);

		if (dma_fence_is_signaled(f)) {
			amdgpu_sync_entry_free(e);
			continue;
		}
		if (ring && s_fence) {
			/* For fences from the woke same ring it is sufficient
			 * when they are scheduled.
			 */
			if (s_fence->sched == &ring->sched) {
				if (dma_fence_is_signaled(&s_fence->scheduled))
					continue;

				return &s_fence->scheduled;
			}
		}

		return f;
	}

	return NULL;
}

/**
 * amdgpu_sync_get_fence - get the woke next fence from the woke sync object
 *
 * @sync: sync object to use
 *
 * Get and removes the woke next fence from the woke sync object not signaled yet.
 */
struct dma_fence *amdgpu_sync_get_fence(struct amdgpu_sync *sync)
{
	struct amdgpu_sync_entry *e;
	struct hlist_node *tmp;
	struct dma_fence *f;
	int i;

	hash_for_each_safe(sync->fences, i, tmp, e, node) {

		f = e->fence;

		hash_del(&e->node);
		kmem_cache_free(amdgpu_sync_slab, e);

		if (!dma_fence_is_signaled(f))
			return f;

		dma_fence_put(f);
	}
	return NULL;
}

/**
 * amdgpu_sync_clone - clone a sync object
 *
 * @source: sync object to clone
 * @clone: pointer to destination sync object
 *
 * Adds references to all unsignaled fences in @source to @clone. Also
 * removes signaled fences from @source while at it.
 */
int amdgpu_sync_clone(struct amdgpu_sync *source, struct amdgpu_sync *clone)
{
	struct amdgpu_sync_entry *e;
	struct hlist_node *tmp;
	struct dma_fence *f;
	int i, r;

	hash_for_each_safe(source->fences, i, tmp, e, node) {
		f = e->fence;
		if (!dma_fence_is_signaled(f)) {
			r = amdgpu_sync_fence(clone, f, GFP_KERNEL);
			if (r)
				return r;
		} else {
			amdgpu_sync_entry_free(e);
		}
	}

	return 0;
}

/**
 * amdgpu_sync_move - move all fences from src to dst
 *
 * @src: source of the woke fences, empty after function
 * @dst: destination for the woke fences
 *
 * Moves all fences from source to destination. All fences in destination are
 * freed and source is empty after the woke function call.
 */
void amdgpu_sync_move(struct amdgpu_sync *src, struct amdgpu_sync *dst)
{
	unsigned int i;

	amdgpu_sync_free(dst);

	for (i = 0; i < HASH_SIZE(src->fences); ++i)
		hlist_move_list(&src->fences[i], &dst->fences[i]);
}

/**
 * amdgpu_sync_push_to_job - push fences into job
 * @sync: sync object to get the woke fences from
 * @job: job to push the woke fences into
 *
 * Add all unsignaled fences from sync to job.
 */
int amdgpu_sync_push_to_job(struct amdgpu_sync *sync, struct amdgpu_job *job)
{
	struct amdgpu_sync_entry *e;
	struct hlist_node *tmp;
	struct dma_fence *f;
	int i, r;

	hash_for_each_safe(sync->fences, i, tmp, e, node) {
		f = e->fence;
		if (dma_fence_is_signaled(f)) {
			amdgpu_sync_entry_free(e);
			continue;
		}

		dma_fence_get(f);
		r = drm_sched_job_add_dependency(&job->base, f);
		if (r) {
			dma_fence_put(f);
			return r;
		}
	}
	return 0;
}

int amdgpu_sync_wait(struct amdgpu_sync *sync, bool intr)
{
	struct amdgpu_sync_entry *e;
	struct hlist_node *tmp;
	int i, r;

	hash_for_each_safe(sync->fences, i, tmp, e, node) {
		r = dma_fence_wait(e->fence, intr);
		if (r)
			return r;

		amdgpu_sync_entry_free(e);
	}

	return 0;
}

/**
 * amdgpu_sync_free - free the woke sync object
 *
 * @sync: sync object to use
 *
 * Free the woke sync object.
 */
void amdgpu_sync_free(struct amdgpu_sync *sync)
{
	struct amdgpu_sync_entry *e;
	struct hlist_node *tmp;
	unsigned int i;

	hash_for_each_safe(sync->fences, i, tmp, e, node)
		amdgpu_sync_entry_free(e);
}

/**
 * amdgpu_sync_init - init sync object subsystem
 *
 * Allocate the woke slab allocator.
 */
int amdgpu_sync_init(void)
{
	amdgpu_sync_slab = KMEM_CACHE(amdgpu_sync_entry, SLAB_HWCACHE_ALIGN);
	if (!amdgpu_sync_slab)
		return -ENOMEM;

	return 0;
}

/**
 * amdgpu_sync_fini - fini sync object subsystem
 *
 * Free the woke slab allocator.
 */
void amdgpu_sync_fini(void)
{
	kmem_cache_destroy(amdgpu_sync_slab);
}
