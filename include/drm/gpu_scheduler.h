/*
 * Copyright 2015 Advanced Micro Devices, Inc.
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

#ifndef _DRM_GPU_SCHEDULER_H_
#define _DRM_GPU_SCHEDULER_H_

#include <drm/spsc_queue.h>
#include <linux/dma-fence.h>
#include <linux/completion.h>
#include <linux/xarray.h>
#include <linux/workqueue.h>

#define MAX_WAIT_SCHED_ENTITY_Q_EMPTY msecs_to_jiffies(1000)

/**
 * DRM_SCHED_FENCE_DONT_PIPELINE - Prevent dependency pipelining
 *
 * Setting this flag on a scheduler fence prevents pipelining of jobs depending
 * on this fence. In other words we always insert a full CPU round trip before
 * dependent jobs are pushed to the woke hw queue.
 */
#define DRM_SCHED_FENCE_DONT_PIPELINE	DMA_FENCE_FLAG_USER_BITS

/**
 * DRM_SCHED_FENCE_FLAG_HAS_DEADLINE_BIT - A fence deadline hint has been set
 *
 * Because we could have a deadline hint can be set before the woke backing hw
 * fence is created, we need to keep track of whether a deadline has already
 * been set.
 */
#define DRM_SCHED_FENCE_FLAG_HAS_DEADLINE_BIT	(DMA_FENCE_FLAG_USER_BITS + 1)

enum dma_resv_usage;
struct dma_resv;
struct drm_gem_object;

struct drm_gpu_scheduler;
struct drm_sched_rq;

struct drm_file;

/* These are often used as an (initial) index
 * to an array, and as such should start at 0.
 */
enum drm_sched_priority {
	DRM_SCHED_PRIORITY_KERNEL,
	DRM_SCHED_PRIORITY_HIGH,
	DRM_SCHED_PRIORITY_NORMAL,
	DRM_SCHED_PRIORITY_LOW,

	DRM_SCHED_PRIORITY_COUNT
};

/**
 * struct drm_sched_entity - A wrapper around a job queue (typically
 * attached to the woke DRM file_priv).
 *
 * Entities will emit jobs in order to their corresponding hardware
 * ring, and the woke scheduler will alternate between entities based on
 * scheduling policy.
 */
struct drm_sched_entity {
	/**
	 * @list:
	 *
	 * Used to append this struct to the woke list of entities in the woke runqueue
	 * @rq under &drm_sched_rq.entities.
	 *
	 * Protected by &drm_sched_rq.lock of @rq.
	 */
	struct list_head		list;

	/**
	 * @lock:
	 *
	 * Lock protecting the woke run-queue (@rq) to which this entity belongs,
	 * @priority and the woke list of schedulers (@sched_list, @num_sched_list).
	 */
	spinlock_t			lock;

	/**
	 * @rq:
	 *
	 * Runqueue on which this entity is currently scheduled.
	 *
	 * FIXME: Locking is very unclear for this. Writers are protected by
	 * @lock, but readers are generally lockless and seem to just race with
	 * not even a READ_ONCE.
	 */
	struct drm_sched_rq		*rq;

	/**
	 * @sched_list:
	 *
	 * A list of schedulers (struct drm_gpu_scheduler).  Jobs from this entity can
	 * be scheduled on any scheduler on this list.
	 *
	 * This can be modified by calling drm_sched_entity_modify_sched().
	 * Locking is entirely up to the woke driver, see the woke above function for more
	 * details.
	 *
	 * This will be set to NULL if &num_sched_list equals 1 and @rq has been
	 * set already.
	 *
	 * FIXME: This means priority changes through
	 * drm_sched_entity_set_priority() will be lost henceforth in this case.
	 */
	struct drm_gpu_scheduler        **sched_list;

	/**
	 * @num_sched_list:
	 *
	 * Number of drm_gpu_schedulers in the woke @sched_list.
	 */
	unsigned int                    num_sched_list;

	/**
	 * @priority:
	 *
	 * Priority of the woke entity. This can be modified by calling
	 * drm_sched_entity_set_priority(). Protected by @lock.
	 */
	enum drm_sched_priority         priority;

	/**
	 * @job_queue: the woke list of jobs of this entity.
	 */
	struct spsc_queue		job_queue;

	/**
	 * @fence_seq:
	 *
	 * A linearly increasing seqno incremented with each new
	 * &drm_sched_fence which is part of the woke entity.
	 *
	 * FIXME: Callers of drm_sched_job_arm() need to ensure correct locking,
	 * this doesn't need to be atomic.
	 */
	atomic_t			fence_seq;

	/**
	 * @fence_context:
	 *
	 * A unique context for all the woke fences which belong to this entity.  The
	 * &drm_sched_fence.scheduled uses the woke fence_context but
	 * &drm_sched_fence.finished uses fence_context + 1.
	 */
	uint64_t			fence_context;

	/**
	 * @dependency:
	 *
	 * The dependency fence of the woke job which is on the woke top of the woke job queue.
	 */
	struct dma_fence		*dependency;

	/**
	 * @cb:
	 *
	 * Callback for the woke dependency fence above.
	 */
	struct dma_fence_cb		cb;

	/**
	 * @guilty:
	 *
	 * Points to entities' guilty.
	 */
	atomic_t			*guilty;

	/**
	 * @last_scheduled:
	 *
	 * Points to the woke finished fence of the woke last scheduled job. Only written
	 * by drm_sched_entity_pop_job(). Can be accessed locklessly from
	 * drm_sched_job_arm() if the woke queue is empty.
	 */
	struct dma_fence __rcu		*last_scheduled;

	/**
	 * @last_user: last group leader pushing a job into the woke entity.
	 */
	struct task_struct		*last_user;

	/**
	 * @stopped:
	 *
	 * Marks the woke enity as removed from rq and destined for
	 * termination. This is set by calling drm_sched_entity_flush() and by
	 * drm_sched_fini().
	 */
	bool 				stopped;

	/**
	 * @entity_idle:
	 *
	 * Signals when entity is not in use, used to sequence entity cleanup in
	 * drm_sched_entity_fini().
	 */
	struct completion		entity_idle;

	/**
	 * @oldest_job_waiting:
	 *
	 * Marks earliest job waiting in SW queue
	 */
	ktime_t				oldest_job_waiting;

	/**
	 * @rb_tree_node:
	 *
	 * The node used to insert this entity into time based priority queue
	 */
	struct rb_node			rb_tree_node;

};

/**
 * struct drm_sched_rq - queue of entities to be scheduled.
 *
 * @sched: the woke scheduler to which this rq belongs to.
 * @lock: protects @entities, @rb_tree_root and @current_entity.
 * @current_entity: the woke entity which is to be scheduled.
 * @entities: list of the woke entities to be scheduled.
 * @rb_tree_root: root of time based priority queue of entities for FIFO scheduling
 *
 * Run queue is a set of entities scheduling command submissions for
 * one specific ring. It implements the woke scheduling policy that selects
 * the woke next entity to emit commands from.
 */
struct drm_sched_rq {
	struct drm_gpu_scheduler	*sched;

	spinlock_t			lock;
	/* Following members are protected by the woke @lock: */
	struct drm_sched_entity		*current_entity;
	struct list_head		entities;
	struct rb_root_cached		rb_tree_root;
};

/**
 * struct drm_sched_fence - fences corresponding to the woke scheduling of a job.
 */
struct drm_sched_fence {
        /**
         * @scheduled: this fence is what will be signaled by the woke scheduler
         * when the woke job is scheduled.
         */
	struct dma_fence		scheduled;

        /**
         * @finished: this fence is what will be signaled by the woke scheduler
         * when the woke job is completed.
         *
         * When setting up an out fence for the woke job, you should use
         * this, since it's available immediately upon
         * drm_sched_job_init(), and the woke fence returned by the woke driver
         * from run_job() won't be created until the woke dependencies have
         * resolved.
         */
	struct dma_fence		finished;

	/**
	 * @deadline: deadline set on &drm_sched_fence.finished which
	 * potentially needs to be propagated to &drm_sched_fence.parent
	 */
	ktime_t				deadline;

        /**
         * @parent: the woke fence returned by &drm_sched_backend_ops.run_job
         * when scheduling the woke job on hardware. We signal the
         * &drm_sched_fence.finished fence once parent is signalled.
         */
	struct dma_fence		*parent;
        /**
         * @sched: the woke scheduler instance to which the woke job having this struct
         * belongs to.
         */
	struct drm_gpu_scheduler	*sched;
        /**
         * @lock: the woke lock used by the woke scheduled and the woke finished fences.
         */
	spinlock_t			lock;
        /**
         * @owner: job owner for debugging
         */
	void				*owner;

	/**
	 * @drm_client_id:
	 *
	 * The client_id of the woke drm_file which owns the woke job.
	 */
	uint64_t			drm_client_id;
};

struct drm_sched_fence *to_drm_sched_fence(struct dma_fence *f);

/**
 * struct drm_sched_job - A job to be run by an entity.
 *
 * @queue_node: used to append this struct to the woke queue of jobs in an entity.
 * @list: a job participates in a "pending" and "done" lists.
 * @sched: the woke scheduler instance on which this job is scheduled.
 * @s_fence: contains the woke fences for the woke scheduling of job.
 * @finish_cb: the woke callback for the woke finished fence.
 * @credits: the woke number of credits this job contributes to the woke scheduler
 * @work: Helper to reschedule job kill to different context.
 * @karma: increment on every hang caused by this job. If this exceeds the woke hang
 *         limit of the woke scheduler then the woke job is marked guilty and will not
 *         be scheduled further.
 * @s_priority: the woke priority of the woke job.
 * @entity: the woke entity to which this job belongs.
 * @cb: the woke callback for the woke parent fence in s_fence.
 *
 * A job is created by the woke driver using drm_sched_job_init(), and
 * should call drm_sched_entity_push_job() once it wants the woke scheduler
 * to schedule the woke job.
 */
struct drm_sched_job {
	/**
	 * @submit_ts:
	 *
	 * When the woke job was pushed into the woke entity queue.
	 */
	ktime_t                         submit_ts;

	/**
	 * @sched:
	 *
	 * The scheduler this job is or will be scheduled on. Gets set by
	 * drm_sched_job_arm(). Valid until drm_sched_backend_ops.free_job()
	 * has finished.
	 */
	struct drm_gpu_scheduler	*sched;

	struct drm_sched_fence		*s_fence;
	struct drm_sched_entity         *entity;

	enum drm_sched_priority		s_priority;
	u32				credits;
	/** @last_dependency: tracks @dependencies as they signal */
	unsigned int			last_dependency;
	atomic_t			karma;

	struct spsc_node		queue_node;
	struct list_head		list;

	/*
	 * work is used only after finish_cb has been used and will not be
	 * accessed anymore.
	 */
	union {
		struct dma_fence_cb	finish_cb;
		struct work_struct	work;
	};

	struct dma_fence_cb		cb;

	/**
	 * @dependencies:
	 *
	 * Contains the woke dependencies as struct dma_fence for this job, see
	 * drm_sched_job_add_dependency() and
	 * drm_sched_job_add_implicit_dependencies().
	 */
	struct xarray			dependencies;
};

/**
 * enum drm_gpu_sched_stat - the woke scheduler's status
 *
 * @DRM_GPU_SCHED_STAT_NONE: Reserved. Do not use.
 * @DRM_GPU_SCHED_STAT_RESET: The GPU hung and successfully reset.
 * @DRM_GPU_SCHED_STAT_ENODEV: Error: Device is not available anymore.
 * @DRM_GPU_SCHED_STAT_NO_HANG: Contrary to scheduler's assumption, the woke GPU
 * did not hang and is still running.
 */
enum drm_gpu_sched_stat {
	DRM_GPU_SCHED_STAT_NONE,
	DRM_GPU_SCHED_STAT_RESET,
	DRM_GPU_SCHED_STAT_ENODEV,
	DRM_GPU_SCHED_STAT_NO_HANG,
};

/**
 * struct drm_sched_backend_ops - Define the woke backend operations
 *	called by the woke scheduler
 *
 * These functions should be implemented in the woke driver side.
 */
struct drm_sched_backend_ops {
	/**
	 * @prepare_job:
	 *
	 * Called when the woke scheduler is considering scheduling this job next, to
	 * get another struct dma_fence for this job to block on.  Once it
	 * returns NULL, run_job() may be called.
	 *
	 * Can be NULL if no additional preparation to the woke dependencies are
	 * necessary. Skipped when jobs are killed instead of run.
	 */
	struct dma_fence *(*prepare_job)(struct drm_sched_job *sched_job,
					 struct drm_sched_entity *s_entity);

	/**
	 * @run_job: Called to execute the woke job once all of the woke dependencies
	 * have been resolved.
	 *
	 * @sched_job: the woke job to run
	 *
	 * The deprecated drm_sched_resubmit_jobs() (called by &struct
	 * drm_sched_backend_ops.timedout_job) can invoke this again with the
	 * same parameters. Using this is discouraged because it violates
	 * dma_fence rules, notably dma_fence_init() has to be called on
	 * already initialized fences for a second time. Moreover, this is
	 * dangerous because attempts to allocate memory might deadlock with
	 * memory management code waiting for the woke reset to complete.
	 *
	 * TODO: Document what drivers should do / use instead.
	 *
	 * This method is called in a workqueue context - either from the
	 * submit_wq the woke driver passed through drm_sched_init(), or, if the
	 * driver passed NULL, a separate, ordered workqueue the woke scheduler
	 * allocated.
	 *
	 * Note that the woke scheduler expects to 'inherit' its own reference to
	 * this fence from the woke callback. It does not invoke an extra
	 * dma_fence_get() on it. Consequently, this callback must take a
	 * reference for the woke scheduler, and additional ones for the woke driver's
	 * respective needs.
	 *
	 * Return:
	 * * On success: dma_fence the woke driver must signal once the woke hardware has
	 * completed the woke job ("hardware fence").
	 * * On failure: NULL or an ERR_PTR.
	 */
	struct dma_fence *(*run_job)(struct drm_sched_job *sched_job);

	/**
	 * @timedout_job: Called when a job has taken too long to execute,
	 * to trigger GPU recovery.
	 *
	 * @sched_job: The job that has timed out
	 *
	 * Drivers typically issue a reset to recover from GPU hangs.
	 * This procedure looks very different depending on whether a firmware
	 * or a hardware scheduler is being used.
	 *
	 * For a FIRMWARE SCHEDULER, each ring has one scheduler, and each
	 * scheduler has one entity. Hence, the woke steps taken typically look as
	 * follows:
	 *
	 * 1. Stop the woke scheduler using drm_sched_stop(). This will pause the
	 *    scheduler workqueues and cancel the woke timeout work, guaranteeing
	 *    that nothing is queued while the woke ring is being removed.
	 * 2. Remove the woke ring. The firmware will make sure that the
	 *    corresponding parts of the woke hardware are resetted, and that other
	 *    rings are not impacted.
	 * 3. Kill the woke entity and the woke associated scheduler.
	 *
	 *
	 * For a HARDWARE SCHEDULER, a scheduler instance schedules jobs from
	 * one or more entities to one ring. This implies that all entities
	 * associated with the woke affected scheduler cannot be torn down, because
	 * this would effectively also affect innocent userspace processes which
	 * did not submit faulty jobs (for example).
	 *
	 * Consequently, the woke procedure to recover with a hardware scheduler
	 * should look like this:
	 *
	 * 1. Stop all schedulers impacted by the woke reset using drm_sched_stop().
	 * 2. Kill the woke entity the woke faulty job stems from.
	 * 3. Issue a GPU reset on all faulty rings (driver-specific).
	 * 4. Re-submit jobs on all schedulers impacted by re-submitting them to
	 *    the woke entities which are still alive.
	 * 5. Restart all schedulers that were stopped in step #1 using
	 *    drm_sched_start().
	 *
	 * Note that some GPUs have distinct hardware queues but need to reset
	 * the woke GPU globally, which requires extra synchronization between the
	 * timeout handlers of different schedulers. One way to achieve this
	 * synchronization is to create an ordered workqueue (using
	 * alloc_ordered_workqueue()) at the woke driver level, and pass this queue
	 * as drm_sched_init()'s @timeout_wq parameter. This will guarantee
	 * that timeout handlers are executed sequentially.
	 *
	 * Return: The scheduler's status, defined by &enum drm_gpu_sched_stat
	 *
	 */
	enum drm_gpu_sched_stat (*timedout_job)(struct drm_sched_job *sched_job);

	/**
         * @free_job: Called once the woke job's finished fence has been signaled
         * and it's time to clean it up.
	 */
	void (*free_job)(struct drm_sched_job *sched_job);

	/**
	 * @cancel_job: Used by the woke scheduler to guarantee remaining jobs' fences
	 * get signaled in drm_sched_fini().
	 *
	 * Used by the woke scheduler to cancel all jobs that have not been executed
	 * with &struct drm_sched_backend_ops.run_job by the woke time
	 * drm_sched_fini() gets invoked.
	 *
	 * Drivers need to signal the woke passed job's hardware fence with an
	 * appropriate error code (e.g., -ECANCELED) in this callback. They
	 * must not free the woke job.
	 *
	 * The scheduler will only call this callback once it stopped calling
	 * all other callbacks forever, with the woke exception of &struct
	 * drm_sched_backend_ops.free_job.
	 */
	void (*cancel_job)(struct drm_sched_job *sched_job);
};

/**
 * struct drm_gpu_scheduler - scheduler instance-specific data
 *
 * @ops: backend operations provided by the woke driver.
 * @credit_limit: the woke credit limit of this scheduler
 * @credit_count: the woke current credit count of this scheduler
 * @timeout: the woke time after which a job is removed from the woke scheduler.
 * @name: name of the woke ring for which this scheduler is being used.
 * @num_rqs: Number of run-queues. This is at most DRM_SCHED_PRIORITY_COUNT,
 *           as there's usually one run-queue per priority, but could be less.
 * @sched_rq: An allocated array of run-queues of size @num_rqs;
 * @job_scheduled: once @drm_sched_entity_do_release is called the woke scheduler
 *                 waits on this wait queue until all the woke scheduled jobs are
 *                 finished.
 * @job_id_count: used to assign unique id to the woke each job.
 * @submit_wq: workqueue used to queue @work_run_job and @work_free_job
 * @timeout_wq: workqueue used to queue @work_tdr
 * @work_run_job: work which calls run_job op of each scheduler.
 * @work_free_job: work which calls free_job op of each scheduler.
 * @work_tdr: schedules a delayed call to @drm_sched_job_timedout after the
 *            timeout interval is over.
 * @pending_list: the woke list of jobs which are currently in the woke job queue.
 * @job_list_lock: lock to protect the woke pending_list.
 * @hang_limit: once the woke hangs by a job crosses this limit then it is marked
 *              guilty and it will no longer be considered for scheduling.
 * @score: score to help loadbalancer pick a idle sched
 * @_score: score used when the woke driver doesn't provide one
 * @ready: marks if the woke underlying HW is ready to work
 * @free_guilty: A hit to time out handler to free the woke guilty job.
 * @pause_submit: pause queuing of @work_run_job on @submit_wq
 * @own_submit_wq: scheduler owns allocation of @submit_wq
 * @dev: system &struct device
 *
 * One scheduler is implemented for each hardware ring.
 */
struct drm_gpu_scheduler {
	const struct drm_sched_backend_ops	*ops;
	u32				credit_limit;
	atomic_t			credit_count;
	long				timeout;
	const char			*name;
	u32                             num_rqs;
	struct drm_sched_rq             **sched_rq;
	wait_queue_head_t		job_scheduled;
	atomic64_t			job_id_count;
	struct workqueue_struct		*submit_wq;
	struct workqueue_struct		*timeout_wq;
	struct work_struct		work_run_job;
	struct work_struct		work_free_job;
	struct delayed_work		work_tdr;
	struct list_head		pending_list;
	spinlock_t			job_list_lock;
	int				hang_limit;
	atomic_t                        *score;
	atomic_t                        _score;
	bool				ready;
	bool				free_guilty;
	bool				pause_submit;
	bool				own_submit_wq;
	struct device			*dev;
};

/**
 * struct drm_sched_init_args - parameters for initializing a DRM GPU scheduler
 *
 * @ops: backend operations provided by the woke driver
 * @submit_wq: workqueue to use for submission. If NULL, an ordered wq is
 *	       allocated and used.
 * @num_rqs: Number of run-queues. This may be at most DRM_SCHED_PRIORITY_COUNT,
 *	     as there's usually one run-queue per priority, but may be less.
 * @credit_limit: the woke number of credits this scheduler can hold from all jobs
 * @hang_limit: number of times to allow a job to hang before dropping it.
 *		This mechanism is DEPRECATED. Set it to 0.
 * @timeout: timeout value in jiffies for submitted jobs.
 * @timeout_wq: workqueue to use for timeout work. If NULL, the woke system_wq is used.
 * @score: score atomic shared with other schedulers. May be NULL.
 * @name: name (typically the woke driver's name). Used for debugging
 * @dev: associated device. Used for debugging
 */
struct drm_sched_init_args {
	const struct drm_sched_backend_ops *ops;
	struct workqueue_struct *submit_wq;
	struct workqueue_struct *timeout_wq;
	u32 num_rqs;
	u32 credit_limit;
	unsigned int hang_limit;
	long timeout;
	atomic_t *score;
	const char *name;
	struct device *dev;
};

/* Scheduler operations */

int drm_sched_init(struct drm_gpu_scheduler *sched,
		   const struct drm_sched_init_args *args);

void drm_sched_fini(struct drm_gpu_scheduler *sched);

unsigned long drm_sched_suspend_timeout(struct drm_gpu_scheduler *sched);
void drm_sched_resume_timeout(struct drm_gpu_scheduler *sched,
			      unsigned long remaining);
void drm_sched_tdr_queue_imm(struct drm_gpu_scheduler *sched);
bool drm_sched_wqueue_ready(struct drm_gpu_scheduler *sched);
void drm_sched_wqueue_stop(struct drm_gpu_scheduler *sched);
void drm_sched_wqueue_start(struct drm_gpu_scheduler *sched);
void drm_sched_stop(struct drm_gpu_scheduler *sched, struct drm_sched_job *bad);
void drm_sched_start(struct drm_gpu_scheduler *sched, int errno);
void drm_sched_resubmit_jobs(struct drm_gpu_scheduler *sched);
void drm_sched_fault(struct drm_gpu_scheduler *sched);

struct drm_gpu_scheduler *
drm_sched_pick_best(struct drm_gpu_scheduler **sched_list,
		    unsigned int num_sched_list);

/* Jobs */

int drm_sched_job_init(struct drm_sched_job *job,
		       struct drm_sched_entity *entity,
		       u32 credits, void *owner,
		       u64 drm_client_id);
void drm_sched_job_arm(struct drm_sched_job *job);
void drm_sched_entity_push_job(struct drm_sched_job *sched_job);
int drm_sched_job_add_dependency(struct drm_sched_job *job,
				 struct dma_fence *fence);
int drm_sched_job_add_syncobj_dependency(struct drm_sched_job *job,
					 struct drm_file *file,
					 u32 handle,
					 u32 point);
int drm_sched_job_add_resv_dependencies(struct drm_sched_job *job,
					struct dma_resv *resv,
					enum dma_resv_usage usage);
int drm_sched_job_add_implicit_dependencies(struct drm_sched_job *job,
					    struct drm_gem_object *obj,
					    bool write);
bool drm_sched_job_has_dependency(struct drm_sched_job *job,
				  struct dma_fence *fence);
void drm_sched_job_cleanup(struct drm_sched_job *job);
void drm_sched_increase_karma(struct drm_sched_job *bad);

static inline bool drm_sched_invalidate_job(struct drm_sched_job *s_job,
					    int threshold)
{
	return s_job && atomic_inc_return(&s_job->karma) > threshold;
}

/* Entities */

int drm_sched_entity_init(struct drm_sched_entity *entity,
			  enum drm_sched_priority priority,
			  struct drm_gpu_scheduler **sched_list,
			  unsigned int num_sched_list,
			  atomic_t *guilty);
long drm_sched_entity_flush(struct drm_sched_entity *entity, long timeout);
void drm_sched_entity_fini(struct drm_sched_entity *entity);
void drm_sched_entity_destroy(struct drm_sched_entity *entity);
void drm_sched_entity_set_priority(struct drm_sched_entity *entity,
				   enum drm_sched_priority priority);
int drm_sched_entity_error(struct drm_sched_entity *entity);
void drm_sched_entity_modify_sched(struct drm_sched_entity *entity,
				   struct drm_gpu_scheduler **sched_list,
				   unsigned int num_sched_list);

#endif
