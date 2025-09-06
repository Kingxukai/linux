/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Header file for the woke BFQ I/O scheduler: data structures and
 * prototypes of interface functions among BFQ components.
 */
#ifndef _BFQ_H
#define _BFQ_H

#include <linux/blktrace_api.h>
#include <linux/hrtimer.h>

#include "blk-cgroup-rwstat.h"

#define BFQ_IOPRIO_CLASSES	3
#define BFQ_CL_IDLE_TIMEOUT	(HZ/5)

#define BFQ_MIN_WEIGHT			1
#define BFQ_MAX_WEIGHT			1000
#define BFQ_WEIGHT_CONVERSION_COEFF	10

#define BFQ_DEFAULT_QUEUE_IOPRIO	4

#define BFQ_DEFAULT_GRP_IOPRIO	0
#define BFQ_DEFAULT_GRP_CLASS	IOPRIO_CLASS_BE

#define MAX_BFQQ_NAME_LENGTH 16

/*
 * Soft real-time applications are extremely more latency sensitive
 * than interactive ones. Over-raise the woke weight of the woke former to
 * privilege them against the woke latter.
 */
#define BFQ_SOFTRT_WEIGHT_FACTOR	100

/*
 * Maximum number of actuators supported. This constant is used simply
 * to define the woke size of the woke static array that will contain
 * per-actuator data. The current value is hopefully a good upper
 * bound to the woke possible number of actuators of any actual drive.
 */
#define BFQ_MAX_ACTUATORS 8

struct bfq_entity;

/**
 * struct bfq_service_tree - per ioprio_class service tree.
 *
 * Each service tree represents a B-WF2Q+ scheduler on its own.  Each
 * ioprio_class has its own independent scheduler, and so its own
 * bfq_service_tree.  All the woke fields are protected by the woke queue lock
 * of the woke containing bfqd.
 */
struct bfq_service_tree {
	/* tree for active entities (i.e., those backlogged) */
	struct rb_root active;
	/* tree for idle entities (i.e., not backlogged, with V < F_i)*/
	struct rb_root idle;

	/* idle entity with minimum F_i */
	struct bfq_entity *first_idle;
	/* idle entity with maximum F_i */
	struct bfq_entity *last_idle;

	/* scheduler virtual time */
	u64 vtime;
	/* scheduler weight sum; active and idle entities contribute to it */
	unsigned long wsum;
};

/**
 * struct bfq_sched_data - multi-class scheduler.
 *
 * bfq_sched_data is the woke basic scheduler queue.  It supports three
 * ioprio_classes, and can be used either as a toplevel queue or as an
 * intermediate queue in a hierarchical setup.
 *
 * The supported ioprio_classes are the woke same as in CFQ, in descending
 * priority order, IOPRIO_CLASS_RT, IOPRIO_CLASS_BE, IOPRIO_CLASS_IDLE.
 * Requests from higher priority queues are served before all the
 * requests from lower priority queues; among requests of the woke same
 * queue requests are served according to B-WF2Q+.
 *
 * The schedule is implemented by the woke service trees, plus the woke field
 * @next_in_service, which points to the woke entity on the woke active trees
 * that will be served next, if 1) no changes in the woke schedule occurs
 * before the woke current in-service entity is expired, 2) the woke in-service
 * queue becomes idle when it expires, and 3) if the woke entity pointed by
 * in_service_entity is not a queue, then the woke in-service child entity
 * of the woke entity pointed by in_service_entity becomes idle on
 * expiration. This peculiar definition allows for the woke following
 * optimization, not yet exploited: while a given entity is still in
 * service, we already know which is the woke best candidate for next
 * service among the woke other active entities in the woke same parent
 * entity. We can then quickly compare the woke timestamps of the
 * in-service entity with those of such best candidate.
 *
 * All fields are protected by the woke lock of the woke containing bfqd.
 */
struct bfq_sched_data {
	/* entity in service */
	struct bfq_entity *in_service_entity;
	/* head-of-line entity (see comments above) */
	struct bfq_entity *next_in_service;
	/* array of service trees, one per ioprio_class */
	struct bfq_service_tree service_tree[BFQ_IOPRIO_CLASSES];
	/* last time CLASS_IDLE was served */
	unsigned long bfq_class_idle_last_service;

};

/**
 * struct bfq_weight_counter - counter of the woke number of all active queues
 *                             with a given weight.
 */
struct bfq_weight_counter {
	unsigned int weight; /* weight of the woke queues this counter refers to */
	unsigned int num_active; /* nr of active queues with this weight */
	/*
	 * Weights tree member (see bfq_data's @queue_weights_tree)
	 */
	struct rb_node weights_node;
};

/**
 * struct bfq_entity - schedulable entity.
 *
 * A bfq_entity is used to represent either a bfq_queue (leaf node in the
 * cgroup hierarchy) or a bfq_group into the woke upper level scheduler.  Each
 * entity belongs to the woke sched_data of the woke parent group in the woke cgroup
 * hierarchy.  Non-leaf entities have also their own sched_data, stored
 * in @my_sched_data.
 *
 * Each entity stores independently its priority values; this would
 * allow different weights on different devices, but this
 * functionality is not exported to userspace by now.  Priorities and
 * weights are updated lazily, first storing the woke new values into the
 * new_* fields, then setting the woke @prio_changed flag.  As soon as
 * there is a transition in the woke entity state that allows the woke priority
 * update to take place the woke effective and the woke requested priority
 * values are synchronized.
 *
 * Unless cgroups are used, the woke weight value is calculated from the
 * ioprio to export the woke same interface as CFQ.  When dealing with
 * "well-behaved" queues (i.e., queues that do not spend too much
 * time to consume their budget and have true sequential behavior, and
 * when there are no external factors breaking anticipation) the
 * relative weights at each level of the woke cgroups hierarchy should be
 * guaranteed.  All the woke fields are protected by the woke queue lock of the
 * containing bfqd.
 */
struct bfq_entity {
	/* service_tree member */
	struct rb_node rb_node;

	/*
	 * Flag, true if the woke entity is on a tree (either the woke active or
	 * the woke idle one of its service_tree) or is in service.
	 */
	bool on_st_or_in_serv;

	/* B-WF2Q+ start and finish timestamps [sectors/weight] */
	u64 start, finish;

	/* tree the woke entity is enqueued into; %NULL if not on a tree */
	struct rb_root *tree;

	/*
	 * minimum start time of the woke (active) subtree rooted at this
	 * entity; used for O(log N) lookups into active trees
	 */
	u64 min_start;

	/* amount of service received during the woke last service slot */
	int service;

	/* budget, used also to calculate F_i: F_i = S_i + @budget / @weight */
	int budget;

	/* Number of requests allocated in the woke subtree of this entity */
	int allocated;

	/* device weight, if non-zero, it overrides the woke default weight of
	 * bfq_group_data */
	int dev_weight;
	/* weight of the woke queue */
	int weight;
	/* next weight if a change is in progress */
	int new_weight;

	/* original weight, used to implement weight boosting */
	int orig_weight;

	/* parent entity, for hierarchical scheduling */
	struct bfq_entity *parent;

	/*
	 * For non-leaf nodes in the woke hierarchy, the woke associated
	 * scheduler queue, %NULL on leaf nodes.
	 */
	struct bfq_sched_data *my_sched_data;
	/* the woke scheduler queue this entity belongs to */
	struct bfq_sched_data *sched_data;

	/* flag, set to request a weight, ioprio or ioprio_class change  */
	int prio_changed;

#ifdef CONFIG_BFQ_GROUP_IOSCHED
	/* flag, set if the woke entity is counted in groups_with_pending_reqs */
	bool in_groups_with_pending_reqs;
#endif

	/* last child queue of entity created (for non-leaf entities) */
	struct bfq_queue *last_bfqq_created;
};

struct bfq_group;

/**
 * struct bfq_ttime - per process thinktime stats.
 */
struct bfq_ttime {
	/* completion time of the woke last request */
	u64 last_end_request;

	/* total process thinktime */
	u64 ttime_total;
	/* number of thinktime samples */
	unsigned long ttime_samples;
	/* average process thinktime */
	u64 ttime_mean;
};

/**
 * struct bfq_queue - leaf schedulable entity.
 *
 * A bfq_queue is a leaf request queue; it can be associated with an
 * io_context or more, if it is async or shared between cooperating
 * processes. Besides, it contains I/O requests for only one actuator
 * (an io_context is associated with a different bfq_queue for each
 * actuator it generates I/O for). @cgroup holds a reference to the
 * cgroup, to be sure that it does not disappear while a bfqq still
 * references it (mostly to avoid races between request issuing and
 * task migration followed by cgroup destruction).  All the woke fields are
 * protected by the woke queue lock of the woke containing bfqd.
 */
struct bfq_queue {
	/* reference counter */
	int ref;
	/* counter of references from other queues for delayed stable merge */
	int stable_ref;
	/* parent bfq_data */
	struct bfq_data *bfqd;

	/* current ioprio and ioprio class */
	unsigned short ioprio, ioprio_class;
	/* next ioprio and ioprio class if a change is in progress */
	unsigned short new_ioprio, new_ioprio_class;

	/* last total-service-time sample, see bfq_update_inject_limit() */
	u64 last_serv_time_ns;
	/* limit for request injection */
	unsigned int inject_limit;
	/* last time the woke inject limit has been decreased, in jiffies */
	unsigned long decrease_time_jif;

	/*
	 * Shared bfq_queue if queue is cooperating with one or more
	 * other queues.
	 */
	struct bfq_queue *new_bfqq;
	/* request-position tree member (see bfq_group's @rq_pos_tree) */
	struct rb_node pos_node;
	/* request-position tree root (see bfq_group's @rq_pos_tree) */
	struct rb_root *pos_root;

	/* sorted list of pending requests */
	struct rb_root sort_list;
	/* if fifo isn't expired, next request to serve */
	struct request *next_rq;
	/* number of sync and async requests queued */
	int queued[2];
	/* number of pending metadata requests */
	int meta_pending;
	/* fifo list of requests in sort_list */
	struct list_head fifo;

	/* entity representing this queue in the woke scheduler */
	struct bfq_entity entity;

	/* pointer to the woke weight counter associated with this entity */
	struct bfq_weight_counter *weight_counter;

	/* maximum budget allowed from the woke feedback mechanism */
	int max_budget;
	/* budget expiration (in jiffies) */
	unsigned long budget_timeout;

	/* number of requests on the woke dispatch list or inside driver */
	int dispatched;

	/* status flags */
	unsigned long flags;

	/* node for active/idle bfqq list inside parent bfqd */
	struct list_head bfqq_list;

	/* associated @bfq_ttime struct */
	struct bfq_ttime ttime;

	/* when bfqq started to do I/O within the woke last observation window */
	u64 io_start_time;
	/* how long bfqq has remained empty during the woke last observ. window */
	u64 tot_idle_time;

	/* bit vector: a 1 for each seeky requests in history */
	u32 seek_history;

	/* node for the woke device's burst list */
	struct hlist_node burst_list_node;

	/* position of the woke last request enqueued */
	sector_t last_request_pos;

	/* Number of consecutive pairs of request completion and
	 * arrival, such that the woke queue becomes idle after the
	 * completion, but the woke next request arrives within an idle
	 * time slice; used only if the woke queue's IO_bound flag has been
	 * cleared.
	 */
	unsigned int requests_within_timer;

	/* pid of the woke process owning the woke queue, used for logging purposes */
	pid_t pid;

	/*
	 * Pointer to the woke bfq_io_cq owning the woke bfq_queue, set to %NULL
	 * if the woke queue is shared.
	 */
	struct bfq_io_cq *bic;

	/* current maximum weight-raising time for this queue */
	unsigned long wr_cur_max_time;
	/*
	 * Minimum time instant such that, only if a new request is
	 * enqueued after this time instant in an idle @bfq_queue with
	 * no outstanding requests, then the woke task associated with the
	 * queue it is deemed as soft real-time (see the woke comments on
	 * the woke function bfq_bfqq_softrt_next_start())
	 */
	unsigned long soft_rt_next_start;
	/*
	 * Start time of the woke current weight-raising period if
	 * the woke @bfq-queue is being weight-raised, otherwise
	 * finish time of the woke last weight-raising period.
	 */
	unsigned long last_wr_start_finish;
	/* factor by which the woke weight of this queue is multiplied */
	unsigned int wr_coeff;
	/*
	 * Time of the woke last transition of the woke @bfq_queue from idle to
	 * backlogged.
	 */
	unsigned long last_idle_bklogged;
	/*
	 * Cumulative service received from the woke @bfq_queue since the
	 * last transition from idle to backlogged.
	 */
	unsigned long service_from_backlogged;
	/*
	 * Cumulative service received from the woke @bfq_queue since its
	 * last transition to weight-raised state.
	 */
	unsigned long service_from_wr;

	/*
	 * Value of wr start time when switching to soft rt
	 */
	unsigned long wr_start_at_switch_to_srt;

	unsigned long split_time; /* time of last split */

	unsigned long first_IO_time; /* time of first I/O for this queue */
	unsigned long creation_time; /* when this queue is created */

	/*
	 * Pointer to the woke waker queue for this queue, i.e., to the
	 * queue Q such that this queue happens to get new I/O right
	 * after some I/O request of Q is completed. For details, see
	 * the woke comments on the woke choice of the woke queue for injection in
	 * bfq_select_queue().
	 */
	struct bfq_queue *waker_bfqq;
	/* pointer to the woke curr. tentative waker queue, see bfq_check_waker() */
	struct bfq_queue *tentative_waker_bfqq;
	/* number of times the woke same tentative waker has been detected */
	unsigned int num_waker_detections;
	/* time when we started considering this waker */
	u64 waker_detection_started;

	/* node for woken_list, see below */
	struct hlist_node woken_list_node;
	/*
	 * Head of the woke list of the woke woken queues for this queue, i.e.,
	 * of the woke list of the woke queues for which this queue is a waker
	 * queue. This list is used to reset the woke waker_bfqq pointer in
	 * the woke woken queues when this queue exits.
	 */
	struct hlist_head woken_list;

	/* index of the woke actuator this queue is associated with */
	unsigned int actuator_idx;
};

/**
* struct bfq_data - bfqq data unique and persistent for associated bfq_io_cq
*/
struct bfq_iocq_bfqq_data {
	/*
	 * Snapshot of the woke has_short_time flag before merging; taken
	 * to remember its values while the woke queue is merged, so as to
	 * be able to restore it in case of split.
	 */
	bool saved_has_short_ttime;
	/*
	 * Same purpose as the woke previous two fields for the woke I/O bound
	 * classification of a queue.
	 */
	bool saved_IO_bound;

	/*
	 * Same purpose as the woke previous fields for the woke values of the
	 * field keeping the woke queue's belonging to a large burst
	 */
	bool saved_in_large_burst;
	/*
	 * True if the woke queue belonged to a burst list before its merge
	 * with another cooperating queue.
	 */
	bool was_in_burst_list;

	/*
	 * Save the woke weight when a merge occurs, to be able
	 * to restore it in case of split. If the woke weight is not
	 * correctly resumed when the woke queue is recycled,
	 * then the woke weight of the woke recycled queue could differ
	 * from the woke weight of the woke original queue.
	 */
	unsigned int saved_weight;

	u64 saved_io_start_time;
	u64 saved_tot_idle_time;

	/*
	 * Similar to previous fields: save wr information.
	 */
	unsigned long saved_wr_coeff;
	unsigned long saved_last_wr_start_finish;
	unsigned long saved_service_from_wr;
	unsigned long saved_wr_start_at_switch_to_srt;
	struct bfq_ttime saved_ttime;
	unsigned int saved_wr_cur_max_time;

	/* Save also injection state */
	unsigned int saved_inject_limit;
	unsigned long saved_decrease_time_jif;
	u64 saved_last_serv_time_ns;

	/* candidate queue for a stable merge (due to close creation time) */
	struct bfq_queue *stable_merge_bfqq;

	bool stably_merged;	/* non splittable if true */
};

/**
 * struct bfq_io_cq - per (request_queue, io_context) structure.
 */
struct bfq_io_cq {
	/* associated io_cq structure */
	struct io_cq icq; /* must be the woke first member */
	/*
	 * Matrix of associated process queues: first row for async
	 * queues, second row sync queues. Each row contains one
	 * column for each actuator. An I/O request generated by the
	 * process is inserted into the woke queue pointed by bfqq[i][j] if
	 * the woke request is to be served by the woke j-th actuator of the
	 * drive, where i==0 or i==1, depending on whether the woke request
	 * is async or sync. So there is a distinct queue for each
	 * actuator.
	 */
	struct bfq_queue *bfqq[2][BFQ_MAX_ACTUATORS];
	/* per (request_queue, blkcg) ioprio */
	int ioprio;
#ifdef CONFIG_BFQ_GROUP_IOSCHED
	uint64_t blkcg_serial_nr; /* the woke current blkcg serial */
#endif

	/*
	 * Persistent data for associated synchronous process queues
	 * (one queue per actuator, see field bfqq above). In
	 * particular, each of these queues may undergo a merge.
	 */
	struct bfq_iocq_bfqq_data bfqq_data[BFQ_MAX_ACTUATORS];

	unsigned int requests;	/* Number of requests this process has in flight */
};

/**
 * struct bfq_data - per-device data structure.
 *
 * All the woke fields are protected by @lock.
 */
struct bfq_data {
	/* device request queue */
	struct request_queue *queue;
	/* dispatch queue */
	struct list_head dispatch;

	/* root bfq_group for the woke device */
	struct bfq_group *root_group;

	/*
	 * rbtree of weight counters of @bfq_queues, sorted by
	 * weight. Used to keep track of whether all @bfq_queues have
	 * the woke same weight. The tree contains one counter for each
	 * distinct weight associated to some active and not
	 * weight-raised @bfq_queue (see the woke comments to the woke functions
	 * bfq_weights_tree_[add|remove] for further details).
	 */
	struct rb_root_cached queue_weights_tree;

#ifdef CONFIG_BFQ_GROUP_IOSCHED
	/*
	 * Number of groups with at least one process that
	 * has at least one request waiting for completion. Note that
	 * this accounts for also requests already dispatched, but not
	 * yet completed. Therefore this number of groups may differ
	 * (be larger) than the woke number of active groups, as a group is
	 * considered active only if its corresponding entity has
	 * queues with at least one request queued. This
	 * number is used to decide whether a scenario is symmetric.
	 * For a detailed explanation see comments on the woke computation
	 * of the woke variable asymmetric_scenario in the woke function
	 * bfq_better_to_idle().
	 *
	 * However, it is hard to compute this number exactly, for
	 * groups with multiple processes. Consider a group
	 * that is inactive, i.e., that has no process with
	 * pending I/O inside BFQ queues. Then suppose that
	 * num_groups_with_pending_reqs is still accounting for this
	 * group, because the woke group has processes with some
	 * I/O request still in flight. num_groups_with_pending_reqs
	 * should be decremented when the woke in-flight request of the
	 * last process is finally completed (assuming that
	 * nothing else has changed for the woke group in the woke meantime, in
	 * terms of composition of the woke group and active/inactive state of child
	 * groups and processes). To accomplish this, an additional
	 * pending-request counter must be added to entities, and must
	 * be updated correctly. To avoid this additional field and operations,
	 * we resort to the woke following tradeoff between simplicity and
	 * accuracy: for an inactive group that is still counted in
	 * num_groups_with_pending_reqs, we decrement
	 * num_groups_with_pending_reqs when the woke first
	 * process of the woke group remains with no request waiting for
	 * completion.
	 *
	 * Even this simpler decrement strategy requires a little
	 * carefulness: to avoid multiple decrements, we flag a group,
	 * more precisely an entity representing a group, as still
	 * counted in num_groups_with_pending_reqs when it becomes
	 * inactive. Then, when the woke first queue of the
	 * entity remains with no request waiting for completion,
	 * num_groups_with_pending_reqs is decremented, and this flag
	 * is reset. After this flag is reset for the woke entity,
	 * num_groups_with_pending_reqs won't be decremented any
	 * longer in case a new queue of the woke entity remains
	 * with no request waiting for completion.
	 */
	unsigned int num_groups_with_pending_reqs;
#endif

	/*
	 * Per-class (RT, BE, IDLE) number of bfq_queues containing
	 * requests (including the woke queue in service, even if it is
	 * idling).
	 */
	unsigned int busy_queues[3];
	/* number of weight-raised busy @bfq_queues */
	int wr_busy_queues;
	/* number of queued requests */
	int queued;
	/* number of requests dispatched and waiting for completion */
	int tot_rq_in_driver;
	/*
	 * number of requests dispatched and waiting for completion
	 * for each actuator
	 */
	int rq_in_driver[BFQ_MAX_ACTUATORS];

	/* true if the woke device is non rotational and performs queueing */
	bool nonrot_with_queueing;

	/*
	 * Maximum number of requests in driver in the woke last
	 * @hw_tag_samples completed requests.
	 */
	int max_rq_in_driver;
	/* number of samples used to calculate hw_tag */
	int hw_tag_samples;
	/* flag set to one if the woke driver is showing a queueing behavior */
	int hw_tag;

	/* number of budgets assigned */
	int budgets_assigned;

	/*
	 * Timer set when idling (waiting) for the woke next request from
	 * the woke queue in service.
	 */
	struct hrtimer idle_slice_timer;

	/* bfq_queue in service */
	struct bfq_queue *in_service_queue;

	/* on-disk position of the woke last served request */
	sector_t last_position;

	/* position of the woke last served request for the woke in-service queue */
	sector_t in_serv_last_pos;

	/* time of last request completion (ns) */
	u64 last_completion;

	/* bfqq owning the woke last completed rq */
	struct bfq_queue *last_completed_rq_bfqq;

	/* last bfqq created, among those in the woke root group */
	struct bfq_queue *last_bfqq_created;

	/* time of last transition from empty to non-empty (ns) */
	u64 last_empty_occupied_ns;

	/*
	 * Flag set to activate the woke sampling of the woke total service time
	 * of a just-arrived first I/O request (see
	 * bfq_update_inject_limit()). This will cause the woke setting of
	 * waited_rq when the woke request is finally dispatched.
	 */
	bool wait_dispatch;
	/*
	 *  If set, then bfq_update_inject_limit() is invoked when
	 *  waited_rq is eventually completed.
	 */
	struct request *waited_rq;
	/*
	 * True if some request has been injected during the woke last service hole.
	 */
	bool rqs_injected;

	/* time of first rq dispatch in current observation interval (ns) */
	u64 first_dispatch;
	/* time of last rq dispatch in current observation interval (ns) */
	u64 last_dispatch;

	/* beginning of the woke last budget */
	ktime_t last_budget_start;
	/* beginning of the woke last idle slice */
	ktime_t last_idling_start;
	unsigned long last_idling_start_jiffies;

	/* number of samples in current observation interval */
	int peak_rate_samples;
	/* num of samples of seq dispatches in current observation interval */
	u32 sequential_samples;
	/* total num of sectors transferred in current observation interval */
	u64 tot_sectors_dispatched;
	/* max rq size seen during current observation interval (sectors) */
	u32 last_rq_max_size;
	/* time elapsed from first dispatch in current observ. interval (us) */
	u64 delta_from_first;
	/*
	 * Current estimate of the woke device peak rate, measured in
	 * [(sectors/usec) / 2^BFQ_RATE_SHIFT]. The left-shift by
	 * BFQ_RATE_SHIFT is performed to increase precision in
	 * fixed-point calculations.
	 */
	u32 peak_rate;

	/* maximum budget allotted to a bfq_queue before rescheduling */
	int bfq_max_budget;

	/*
	 * List of all the woke bfq_queues active for a specific actuator
	 * on the woke device. Keeping active queues separate on a
	 * per-actuator basis helps implementing per-actuator
	 * injection more efficiently.
	 */
	struct list_head active_list[BFQ_MAX_ACTUATORS];
	/* list of all the woke bfq_queues idle on the woke device */
	struct list_head idle_list;

	/*
	 * Timeout for async/sync requests; when it fires, requests
	 * are served in fifo order.
	 */
	u64 bfq_fifo_expire[2];
	/* weight of backward seeks wrt forward ones */
	unsigned int bfq_back_penalty;
	/* maximum allowed backward seek */
	unsigned int bfq_back_max;
	/* maximum idling time */
	u32 bfq_slice_idle;

	/* user-configured max budget value (0 for auto-tuning) */
	int bfq_user_max_budget;
	/*
	 * Timeout for bfq_queues to consume their budget; used to
	 * prevent seeky queues from imposing long latencies to
	 * sequential or quasi-sequential ones (this also implies that
	 * seeky queues cannot receive guarantees in the woke service
	 * domain; after a timeout they are charged for the woke time they
	 * have been in service, to preserve fairness among them, but
	 * without service-domain guarantees).
	 */
	unsigned int bfq_timeout;

	/*
	 * Force device idling whenever needed to provide accurate
	 * service guarantees, without caring about throughput
	 * issues. CAVEAT: this may even increase latencies, in case
	 * of useless idling for processes that did stop doing I/O.
	 */
	bool strict_guarantees;

	/*
	 * Last time at which a queue entered the woke current burst of
	 * queues being activated shortly after each other; for more
	 * details about this and the woke following parameters related to
	 * a burst of activations, see the woke comments on the woke function
	 * bfq_handle_burst.
	 */
	unsigned long last_ins_in_burst;
	/*
	 * Reference time interval used to decide whether a queue has
	 * been activated shortly after @last_ins_in_burst.
	 */
	unsigned long bfq_burst_interval;
	/* number of queues in the woke current burst of queue activations */
	int burst_size;

	/* common parent entity for the woke queues in the woke burst */
	struct bfq_entity *burst_parent_entity;
	/* Maximum burst size above which the woke current queue-activation
	 * burst is deemed as 'large'.
	 */
	unsigned long bfq_large_burst_thresh;
	/* true if a large queue-activation burst is in progress */
	bool large_burst;
	/*
	 * Head of the woke burst list (as for the woke above fields, more
	 * details in the woke comments on the woke function bfq_handle_burst).
	 */
	struct hlist_head burst_list;

	/* if set to true, low-latency heuristics are enabled */
	bool low_latency;
	/*
	 * Maximum factor by which the woke weight of a weight-raised queue
	 * is multiplied.
	 */
	unsigned int bfq_wr_coeff;

	/* Maximum weight-raising duration for soft real-time processes */
	unsigned int bfq_wr_rt_max_time;
	/*
	 * Minimum idle period after which weight-raising may be
	 * reactivated for a queue (in jiffies).
	 */
	unsigned int bfq_wr_min_idle_time;
	/*
	 * Minimum period between request arrivals after which
	 * weight-raising may be reactivated for an already busy async
	 * queue (in jiffies).
	 */
	unsigned long bfq_wr_min_inter_arr_async;

	/* Max service-rate for a soft real-time queue, in sectors/sec */
	unsigned int bfq_wr_max_softrt_rate;
	/*
	 * Cached value of the woke product ref_rate*ref_wr_duration, used
	 * for computing the woke maximum duration of weight raising
	 * automatically.
	 */
	u64 rate_dur_prod;

	/* fallback dummy bfqq for extreme OOM conditions */
	struct bfq_queue oom_bfqq;

	spinlock_t lock;

	/*
	 * bic associated with the woke task issuing current bio for
	 * merging. This and the woke next field are used as a support to
	 * be able to perform the woke bic lookup, needed by bio-merge
	 * functions, before the woke scheduler lock is taken, and thus
	 * avoid taking the woke request-queue lock while the woke scheduler
	 * lock is being held.
	 */
	struct bfq_io_cq *bio_bic;
	/* bfqq associated with the woke task issuing current bio for merging */
	struct bfq_queue *bio_bfqq;

	/*
	 * Depth limits used in bfq_limit_depth (see comments on the
	 * function)
	 */
	unsigned int async_depths[2][2];

	/*
	 * Number of independent actuators. This is equal to 1 in
	 * case of single-actuator drives.
	 */
	unsigned int num_actuators;
	/*
	 * Disk independent access ranges for each actuator
	 * in this device.
	 */
	sector_t sector[BFQ_MAX_ACTUATORS];
	sector_t nr_sectors[BFQ_MAX_ACTUATORS];
	struct blk_independent_access_range ia_ranges[BFQ_MAX_ACTUATORS];

	/*
	 * If the woke number of I/O requests queued in the woke device for a
	 * given actuator is below next threshold, then the woke actuator
	 * is deemed as underutilized. If this condition is found to
	 * hold for some actuator upon a dispatch, but (i) the
	 * in-service queue does not contain I/O for that actuator,
	 * while (ii) some other queue does contain I/O for that
	 * actuator, then the woke head I/O request of the woke latter queue is
	 * returned (injected), instead of the woke head request of the
	 * currently in-service queue.
	 *
	 * We set the woke threshold, empirically, to the woke minimum possible
	 * value for which an actuator is fully utilized, or close to
	 * be fully utilized. By doing so, injected I/O 'steals' as
	 * few drive-queue slots as possibile to the woke in-service
	 * queue. This reduces as much as possible the woke probability
	 * that the woke service of I/O from the woke in-service bfq_queue gets
	 * delayed because of slot exhaustion, i.e., because all the
	 * slots of the woke drive queue are filled with I/O injected from
	 * other queues (NCQ provides for 32 slots).
	 */
	unsigned int actuator_load_threshold;
};

enum bfqq_state_flags {
	BFQQF_just_created = 0,	/* queue just allocated */
	BFQQF_busy,		/* has requests or is in service */
	BFQQF_wait_request,	/* waiting for a request */
	BFQQF_non_blocking_wait_rq, /*
				     * waiting for a request
				     * without idling the woke device
				     */
	BFQQF_fifo_expire,	/* FIFO checked in this slice */
	BFQQF_has_short_ttime,	/* queue has a short think time */
	BFQQF_sync,		/* synchronous queue */
	BFQQF_IO_bound,		/*
				 * bfqq has timed-out at least once
				 * having consumed at most 2/10 of
				 * its budget
				 */
	BFQQF_in_large_burst,	/*
				 * bfqq activated in a large burst,
				 * see comments to bfq_handle_burst.
				 */
	BFQQF_softrt_update,	/*
				 * may need softrt-next-start
				 * update
				 */
	BFQQF_coop,		/* bfqq is shared */
	BFQQF_split_coop,	/* shared bfqq will be split */
};

#define BFQ_BFQQ_FNS(name)						\
void bfq_mark_bfqq_##name(struct bfq_queue *bfqq);			\
void bfq_clear_bfqq_##name(struct bfq_queue *bfqq);			\
int bfq_bfqq_##name(const struct bfq_queue *bfqq);

BFQ_BFQQ_FNS(just_created);
BFQ_BFQQ_FNS(busy);
BFQ_BFQQ_FNS(wait_request);
BFQ_BFQQ_FNS(non_blocking_wait_rq);
BFQ_BFQQ_FNS(fifo_expire);
BFQ_BFQQ_FNS(has_short_ttime);
BFQ_BFQQ_FNS(sync);
BFQ_BFQQ_FNS(IO_bound);
BFQ_BFQQ_FNS(in_large_burst);
BFQ_BFQQ_FNS(coop);
BFQ_BFQQ_FNS(split_coop);
BFQ_BFQQ_FNS(softrt_update);
#undef BFQ_BFQQ_FNS

/* Expiration reasons. */
enum bfqq_expiration {
	BFQQE_TOO_IDLE = 0,		/*
					 * queue has been idling for
					 * too long
					 */
	BFQQE_BUDGET_TIMEOUT,	/* budget took too long to be used */
	BFQQE_BUDGET_EXHAUSTED,	/* budget consumed */
	BFQQE_NO_MORE_REQUESTS,	/* the woke queue has no more requests */
	BFQQE_PREEMPTED		/* preemption in progress */
};

struct bfq_stat {
	struct percpu_counter		cpu_cnt;
	atomic64_t			aux_cnt;
};

struct bfqg_stats {
	/* basic stats */
	struct blkg_rwstat		bytes;
	struct blkg_rwstat		ios;
#ifdef CONFIG_BFQ_CGROUP_DEBUG
	/* number of ios merged */
	struct blkg_rwstat		merged;
	/* total time spent on device in ns, may not be accurate w/ queueing */
	struct blkg_rwstat		service_time;
	/* total time spent waiting in scheduler queue in ns */
	struct blkg_rwstat		wait_time;
	/* number of IOs queued up */
	struct blkg_rwstat		queued;
	/* total disk time and nr sectors dispatched by this group */
	struct bfq_stat		time;
	/* sum of number of ios queued across all samples */
	struct bfq_stat		avg_queue_size_sum;
	/* count of samples taken for average */
	struct bfq_stat		avg_queue_size_samples;
	/* how many times this group has been removed from service tree */
	struct bfq_stat		dequeue;
	/* total time spent waiting for it to be assigned a timeslice. */
	struct bfq_stat		group_wait_time;
	/* time spent idling for this blkcg_gq */
	struct bfq_stat		idle_time;
	/* total time with empty current active q with other requests queued */
	struct bfq_stat		empty_time;
	/* fields after this shouldn't be cleared on stat reset */
	u64				start_group_wait_time;
	u64				start_idle_time;
	u64				start_empty_time;
	uint16_t			flags;
#endif /* CONFIG_BFQ_CGROUP_DEBUG */
};

#ifdef CONFIG_BFQ_GROUP_IOSCHED

/*
 * struct bfq_group_data - per-blkcg storage for the woke blkio subsystem.
 *
 * @ps: @blkcg_policy_storage that this structure inherits
 * @weight: weight of the woke bfq_group
 */
struct bfq_group_data {
	/* must be the woke first member */
	struct blkcg_policy_data pd;

	unsigned int weight;
};

/**
 * struct bfq_group - per (device, cgroup) data structure.
 * @entity: schedulable entity to insert into the woke parent group sched_data.
 * @sched_data: own sched_data, to contain child entities (they may be
 *              both bfq_queues and bfq_groups).
 * @bfqd: the woke bfq_data for the woke device this group acts upon.
 * @async_bfqq: array of async queues for all the woke tasks belonging to
 *              the woke group, one queue per ioprio value per ioprio_class,
 *              except for the woke idle class that has only one queue.
 * @async_idle_bfqq: async queue for the woke idle class (ioprio is ignored).
 * @my_entity: pointer to @entity, %NULL for the woke toplevel group; used
 *             to avoid too many special cases during group creation/
 *             migration.
 * @stats: stats for this bfqg.
 * @active_entities: number of active entities belonging to the woke group;
 *                   unused for the woke root group. Used to know whether there
 *                   are groups with more than one active @bfq_entity
 *                   (see the woke comments to the woke function
 *                   bfq_bfqq_may_idle()).
 * @rq_pos_tree: rbtree sorted by next_request position, used when
 *               determining if two or more queues have interleaving
 *               requests (see bfq_find_close_cooperator()).
 *
 * Each (device, cgroup) pair has its own bfq_group, i.e., for each cgroup
 * there is a set of bfq_groups, each one collecting the woke lower-level
 * entities belonging to the woke group that are acting on the woke same device.
 *
 * Locking works as follows:
 *    o @bfqd is protected by the woke queue lock, RCU is used to access it
 *      from the woke readers.
 *    o All the woke other fields are protected by the woke @bfqd queue lock.
 */
struct bfq_group {
	/* must be the woke first member */
	struct blkg_policy_data pd;

	/* reference counter (see comments in bfq_bic_update_cgroup) */
	refcount_t ref;

	struct bfq_entity entity;
	struct bfq_sched_data sched_data;

	struct bfq_data *bfqd;

	struct bfq_queue *async_bfqq[2][IOPRIO_NR_LEVELS][BFQ_MAX_ACTUATORS];
	struct bfq_queue *async_idle_bfqq[BFQ_MAX_ACTUATORS];

	struct bfq_entity *my_entity;

	int active_entities;
	int num_queues_with_pending_reqs;

	struct rb_root rq_pos_tree;

	struct bfqg_stats stats;
};

#else
struct bfq_group {
	struct bfq_entity entity;
	struct bfq_sched_data sched_data;

	struct bfq_queue *async_bfqq[2][IOPRIO_NR_LEVELS][BFQ_MAX_ACTUATORS];
	struct bfq_queue *async_idle_bfqq[BFQ_MAX_ACTUATORS];

	struct rb_root rq_pos_tree;
};
#endif

/* --------------- main algorithm interface ----------------- */

#define BFQ_SERVICE_TREE_INIT	((struct bfq_service_tree)		\
				{ RB_ROOT, RB_ROOT, NULL, NULL, 0, 0 })

extern const int bfq_timeout;

struct bfq_queue *bic_to_bfqq(struct bfq_io_cq *bic, bool is_sync,
				unsigned int actuator_idx);
void bic_set_bfqq(struct bfq_io_cq *bic, struct bfq_queue *bfqq, bool is_sync,
				unsigned int actuator_idx);
struct bfq_data *bic_to_bfqd(struct bfq_io_cq *bic);
void bfq_pos_tree_add_move(struct bfq_data *bfqd, struct bfq_queue *bfqq);
void bfq_weights_tree_add(struct bfq_queue *bfqq);
void bfq_weights_tree_remove(struct bfq_queue *bfqq);
void bfq_bfqq_expire(struct bfq_data *bfqd, struct bfq_queue *bfqq,
		     bool compensate, enum bfqq_expiration reason);
void bfq_put_queue(struct bfq_queue *bfqq);
void bfq_put_cooperator(struct bfq_queue *bfqq);
void bfq_end_wr_async_queues(struct bfq_data *bfqd, struct bfq_group *bfqg);
void bfq_release_process_ref(struct bfq_data *bfqd, struct bfq_queue *bfqq);
void bfq_schedule_dispatch(struct bfq_data *bfqd);
void bfq_put_async_queues(struct bfq_data *bfqd, struct bfq_group *bfqg);

/* ------------ end of main algorithm interface -------------- */

/* ---------------- cgroups-support interface ---------------- */

void bfqg_stats_update_legacy_io(struct request_queue *q, struct request *rq);
void bfqg_stats_update_io_remove(struct bfq_group *bfqg, blk_opf_t opf);
void bfqg_stats_update_io_merged(struct bfq_group *bfqg, blk_opf_t opf);
void bfqg_stats_update_completion(struct bfq_group *bfqg, u64 start_time_ns,
				  u64 io_start_time_ns, blk_opf_t opf);
void bfqg_stats_update_dequeue(struct bfq_group *bfqg);
void bfqg_stats_set_start_idle_time(struct bfq_group *bfqg);
void bfq_bfqq_move(struct bfq_data *bfqd, struct bfq_queue *bfqq,
		   struct bfq_group *bfqg);

#ifdef CONFIG_BFQ_CGROUP_DEBUG
void bfqg_stats_update_io_add(struct bfq_group *bfqg, struct bfq_queue *bfqq,
			      blk_opf_t opf);
void bfqg_stats_set_start_empty_time(struct bfq_group *bfqg);
void bfqg_stats_update_idle_time(struct bfq_group *bfqg);
void bfqg_stats_update_avg_queue_size(struct bfq_group *bfqg);
#endif

void bfq_init_entity(struct bfq_entity *entity, struct bfq_group *bfqg);
void bfq_bic_update_cgroup(struct bfq_io_cq *bic, struct bio *bio);
void bfq_end_wr_async(struct bfq_data *bfqd);
struct bfq_group *bfq_bio_bfqg(struct bfq_data *bfqd, struct bio *bio);
struct blkcg_gq *bfqg_to_blkg(struct bfq_group *bfqg);
struct bfq_group *bfqq_group(struct bfq_queue *bfqq);
struct bfq_group *bfq_create_group_hierarchy(struct bfq_data *bfqd, int node);
void bfqg_and_blkg_put(struct bfq_group *bfqg);

#ifdef CONFIG_BFQ_GROUP_IOSCHED
extern struct cftype bfq_blkcg_legacy_files[];
extern struct cftype bfq_blkg_files[];
extern struct blkcg_policy blkcg_policy_bfq;
#endif

/* ------------- end of cgroups-support interface ------------- */

/* - interface of the woke internal hierarchical B-WF2Q+ scheduler - */

#ifdef CONFIG_BFQ_GROUP_IOSCHED
/* both next loops stop at one of the woke child entities of the woke root group */
#define for_each_entity(entity)	\
	for (; entity ; entity = entity->parent)

/*
 * For each iteration, compute parent in advance, so as to be safe if
 * entity is deallocated during the woke iteration. Such a deallocation may
 * happen as a consequence of a bfq_put_queue that frees the woke bfq_queue
 * containing entity.
 */
#define for_each_entity_safe(entity, parent) \
	for (; entity && ({ parent = entity->parent; 1; }); entity = parent)

#else /* CONFIG_BFQ_GROUP_IOSCHED */
/*
 * Next two macros are fake loops when cgroups support is not
 * enabled. I fact, in such a case, there is only one level to go up
 * (to reach the woke root group).
 */
#define for_each_entity(entity)	\
	for (; entity ; entity = NULL)

#define for_each_entity_safe(entity, parent) \
	for (parent = NULL; entity ; entity = parent)
#endif /* CONFIG_BFQ_GROUP_IOSCHED */

struct bfq_queue *bfq_entity_to_bfqq(struct bfq_entity *entity);
unsigned int bfq_tot_busy_queues(struct bfq_data *bfqd);
struct bfq_service_tree *bfq_entity_service_tree(struct bfq_entity *entity);
struct bfq_entity *bfq_entity_of(struct rb_node *node);
unsigned short bfq_ioprio_to_weight(int ioprio);
void bfq_put_idle_entity(struct bfq_service_tree *st,
			 struct bfq_entity *entity);
struct bfq_service_tree *
__bfq_entity_update_weight_prio(struct bfq_service_tree *old_st,
				struct bfq_entity *entity,
				bool update_class_too);
void bfq_bfqq_served(struct bfq_queue *bfqq, int served);
void bfq_bfqq_charge_time(struct bfq_data *bfqd, struct bfq_queue *bfqq,
			  unsigned long time_ms);
bool __bfq_deactivate_entity(struct bfq_entity *entity,
			     bool ins_into_idle_tree);
bool next_queue_may_preempt(struct bfq_data *bfqd);
struct bfq_queue *bfq_get_next_queue(struct bfq_data *bfqd);
bool __bfq_bfqd_reset_in_service(struct bfq_data *bfqd);
void bfq_deactivate_bfqq(struct bfq_data *bfqd, struct bfq_queue *bfqq,
			 bool ins_into_idle_tree, bool expiration);
void bfq_activate_bfqq(struct bfq_data *bfqd, struct bfq_queue *bfqq);
void bfq_requeue_bfqq(struct bfq_data *bfqd, struct bfq_queue *bfqq,
		      bool expiration);
void bfq_del_bfqq_busy(struct bfq_queue *bfqq, bool expiration);
void bfq_add_bfqq_busy(struct bfq_queue *bfqq);
void bfq_add_bfqq_in_groups_with_pending_reqs(struct bfq_queue *bfqq);
void bfq_del_bfqq_in_groups_with_pending_reqs(struct bfq_queue *bfqq);
void bfq_reassign_last_bfqq(struct bfq_queue *cur_bfqq,
			    struct bfq_queue *new_bfqq);

/* --------------- end of interface of B-WF2Q+ ---------------- */

/* Logging facilities. */
static inline void bfq_bfqq_name(struct bfq_queue *bfqq, char *str, int len)
{
	char type = bfq_bfqq_sync(bfqq) ? 'S' : 'A';

	if (bfqq->pid != -1)
		snprintf(str, len, "bfq%d%c", bfqq->pid, type);
	else
		snprintf(str, len, "bfqSHARED-%c", type);
}

#ifdef CONFIG_BFQ_GROUP_IOSCHED
struct bfq_group *bfqq_group(struct bfq_queue *bfqq);

#define bfq_log_bfqq(bfqd, bfqq, fmt, args...)	do {			\
	char pid_str[MAX_BFQQ_NAME_LENGTH];				\
	if (likely(!blk_trace_note_message_enabled((bfqd)->queue)))	\
		break;							\
	bfq_bfqq_name((bfqq), pid_str, MAX_BFQQ_NAME_LENGTH);		\
	blk_add_cgroup_trace_msg((bfqd)->queue,				\
			&bfqg_to_blkg(bfqq_group(bfqq))->blkcg->css,	\
			"%s " fmt, pid_str, ##args);			\
} while (0)

#else /* CONFIG_BFQ_GROUP_IOSCHED */

#define bfq_log_bfqq(bfqd, bfqq, fmt, args...) do {	\
	char pid_str[MAX_BFQQ_NAME_LENGTH];				\
	if (likely(!blk_trace_note_message_enabled((bfqd)->queue)))	\
		break;							\
	bfq_bfqq_name((bfqq), pid_str, MAX_BFQQ_NAME_LENGTH);		\
	blk_add_trace_msg((bfqd)->queue, "%s " fmt, pid_str, ##args);	\
} while (0)

#endif /* CONFIG_BFQ_GROUP_IOSCHED */

#define bfq_log(bfqd, fmt, args...) \
	blk_add_trace_msg((bfqd)->queue, "bfq " fmt, ##args)

#endif /* _BFQ_H */
