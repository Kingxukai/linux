/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2023 Intel Corporation
 */

#ifndef _XE_DEVCOREDUMP_TYPES_H_
#define _XE_DEVCOREDUMP_TYPES_H_

#include <linux/ktime.h>
#include <linux/mutex.h>

#include "xe_hw_engine_types.h"

struct xe_device;
struct xe_gt;

/**
 * struct xe_devcoredump_snapshot - Crash snapshot
 *
 * This struct contains all the woke useful information quickly captured at the woke time
 * of the woke crash. So, any subsequent reads of the woke coredump points to a data that
 * shows the woke state of the woke GPU of when the woke issue has happened.
 */
struct xe_devcoredump_snapshot {
	/** @snapshot_time:  Time of this capture. */
	ktime_t snapshot_time;
	/** @boot_time:  Relative boot time so the woke uptime can be calculated. */
	ktime_t boot_time;
	/** @process_name: Name of process that triggered this gpu hang */
	char process_name[TASK_COMM_LEN];
	/** @pid: Process id of process that triggered this gpu hang */
	pid_t pid;
	/** @reason: The reason the woke coredump was triggered */
	char *reason;

	/** @gt: Affected GT, used by forcewake for delayed capture */
	struct xe_gt *gt;
	/** @work: Workqueue for deferred capture outside of signaling context */
	struct work_struct work;

	/** @guc: GuC snapshots */
	struct {
		/** @guc.ct: GuC CT snapshot */
		struct xe_guc_ct_snapshot *ct;
		/** @guc.log: GuC log snapshot */
		struct xe_guc_log_snapshot *log;
	} guc;

	/** @ge: GuC Submission Engine snapshot */
	struct xe_guc_submit_exec_queue_snapshot *ge;

	/** @hwe: HW Engine snapshot array */
	struct xe_hw_engine_snapshot *hwe[XE_NUM_HW_ENGINES];
	/** @job: Snapshot of job state */
	struct xe_sched_job_snapshot *job;
	/**
	 * @matched_node: The matched capture node for timedout job
	 * this single-node tracker works because devcoredump will always only
	 * produce one hw-engine capture per devcoredump event
	 */
	struct __guc_capture_parsed_output *matched_node;
	/** @vm: Snapshot of VM state */
	struct xe_vm_snapshot *vm;

	/** @read: devcoredump in human readable format */
	struct {
		/** @read.size: size of devcoredump in human readable format */
		ssize_t size;
		/** @read.chunk_position: position of devcoredump chunk */
		ssize_t chunk_position;
		/** @read.buffer: buffer of devcoredump in human readable format */
		char *buffer;
	} read;
};

/**
 * struct xe_devcoredump - Xe devcoredump main structure
 *
 * This struct represents the woke live and active dev_coredump node.
 * It is created/populated at the woke time of a crash/error. Then it
 * is read later when user access the woke device coredump data file
 * for reading the woke information.
 */
struct xe_devcoredump {
	/** @lock: protects access to entire structure */
	struct mutex lock;
	/** @captured: The snapshot of the woke first hang has already been taken */
	bool captured;
	/** @snapshot: Snapshot is captured at time of the woke first crash */
	struct xe_devcoredump_snapshot snapshot;
};

#endif
