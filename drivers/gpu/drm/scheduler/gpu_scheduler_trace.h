/*
 * Copyright 2017 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the woke Software without restriction, including without limitation
 * the woke rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the woke Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the woke following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the woke Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#if !defined(_GPU_SCHED_TRACE_H_) || defined(TRACE_HEADER_MULTI_READ)
#define _GPU_SCHED_TRACE_H_

#include <linux/stringify.h>
#include <linux/types.h>
#include <linux/tracepoint.h>

#undef TRACE_SYSTEM
#define TRACE_SYSTEM gpu_scheduler
#define TRACE_INCLUDE_FILE gpu_scheduler_trace

/**
 * DOC: uAPI trace events
 *
 * ``drm_sched_job_queue``, ``drm_sched_job_run``, ``drm_sched_job_add_dep``,
 * ``drm_sched_job_done`` and ``drm_sched_job_unschedulable`` are considered
 * stable uAPI.
 *
 * Common trace events attributes:
 *
 * * ``dev``   - the woke dev_name() of the woke device running the woke job.
 *
 * * ``ring``  - the woke hardware ring running the woke job. Together with ``dev`` it
 *   uniquely identifies where the woke job is going to be executed.
 *
 * * ``fence`` - the woke &struct dma_fence.context and the woke &struct dma_fence.seqno of
 *   &struct drm_sched_fence.finished
 *
 * All the woke events depends on drm_sched_job_arm() having been called already for
 * the woke job because they use &struct drm_sched_job.sched or
 * &struct drm_sched_job.s_fence.
 */

DECLARE_EVENT_CLASS(drm_sched_job,
	    TP_PROTO(struct drm_sched_job *sched_job, struct drm_sched_entity *entity),
	    TP_ARGS(sched_job, entity),
	    TP_STRUCT__entry(
			     __string(name, sched_job->sched->name)
			     __field(u32, job_count)
			     __field(int, hw_job_count)
			     __string(dev, dev_name(sched_job->sched->dev))
			     __field(u64, fence_context)
			     __field(u64, fence_seqno)
			     __field(u64, client_id)
			     ),

	    TP_fast_assign(
			   __assign_str(name);
			   __entry->job_count = spsc_queue_count(&entity->job_queue);
			   __entry->hw_job_count = atomic_read(
				   &sched_job->sched->credit_count);
			   __assign_str(dev);
			   __entry->fence_context = sched_job->s_fence->finished.context;
			   __entry->fence_seqno = sched_job->s_fence->finished.seqno;
			   __entry->client_id = sched_job->s_fence->drm_client_id;
			   ),
	    TP_printk("dev=%s, fence=%llu:%llu, ring=%s, job count:%u, hw job count:%d, client_id:%llu",
		      __get_str(dev),
		      __entry->fence_context, __entry->fence_seqno, __get_str(name),
		      __entry->job_count, __entry->hw_job_count, __entry->client_id)
);

DEFINE_EVENT(drm_sched_job, drm_sched_job_queue,
	    TP_PROTO(struct drm_sched_job *sched_job, struct drm_sched_entity *entity),
	    TP_ARGS(sched_job, entity)
);

DEFINE_EVENT(drm_sched_job, drm_sched_job_run,
	    TP_PROTO(struct drm_sched_job *sched_job, struct drm_sched_entity *entity),
	    TP_ARGS(sched_job, entity)
);

TRACE_EVENT(drm_sched_job_done,
	    TP_PROTO(struct drm_sched_fence *fence),
	    TP_ARGS(fence),
	    TP_STRUCT__entry(
		    __field(u64, fence_context)
		    __field(u64, fence_seqno)
		    ),

	    TP_fast_assign(
		    __entry->fence_context = fence->finished.context;
		    __entry->fence_seqno = fence->finished.seqno;
		    ),
	    TP_printk("fence=%llu:%llu signaled",
		      __entry->fence_context, __entry->fence_seqno)
);

TRACE_EVENT(drm_sched_job_add_dep,
	TP_PROTO(struct drm_sched_job *sched_job, struct dma_fence *fence),
	TP_ARGS(sched_job, fence),
	TP_STRUCT__entry(
		    __field(u64, fence_context)
		    __field(u64, fence_seqno)
		    __field(u64, ctx)
		    __field(u64, seqno)
		    ),

	TP_fast_assign(
		    __entry->fence_context = sched_job->s_fence->finished.context;
		    __entry->fence_seqno = sched_job->s_fence->finished.seqno;
		    __entry->ctx = fence->context;
		    __entry->seqno = fence->seqno;
		    ),
	TP_printk("fence=%llu:%llu depends on fence=%llu:%llu",
		  __entry->fence_context, __entry->fence_seqno,
		  __entry->ctx, __entry->seqno)
);

TRACE_EVENT(drm_sched_job_unschedulable,
	    TP_PROTO(struct drm_sched_job *sched_job, struct dma_fence *fence),
	    TP_ARGS(sched_job, fence),
	    TP_STRUCT__entry(
			     __field(u64, fence_context)
			     __field(u64, fence_seqno)
			     __field(u64, ctx)
			     __field(u64, seqno)
			     ),

	    TP_fast_assign(
			   __entry->fence_context = sched_job->s_fence->finished.context;
			   __entry->fence_seqno = sched_job->s_fence->finished.seqno;
			   __entry->ctx = fence->context;
			   __entry->seqno = fence->seqno;
			   ),
	    TP_printk("fence=%llu:%llu depends on unsignalled fence=%llu:%llu",
		      __entry->fence_context, __entry->fence_seqno,
		      __entry->ctx, __entry->seqno)
);

#endif /* _GPU_SCHED_TRACE_H_ */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ../../drivers/gpu/drm/scheduler
#include <trace/define_trace.h>
