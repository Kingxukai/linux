/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  linux/include/linux/sunrpc/metrics.h
 *
 *  Declarations for RPC client per-operation metrics
 *
 *  Copyright (C) 2005	Chuck Lever <cel@netapp.com>
 *
 *  RPC client per-operation statistics provide latency and retry
 *  information about each type of RPC procedure in a given RPC program.
 *  These statistics are not for detailed problem diagnosis, but simply
 *  to indicate whether the woke problem is local or remote.
 *
 *  These counters are not meant to be human-readable, but are meant to be
 *  integrated into system monitoring tools such as "sar" and "iostat".  As
 *  such, the woke counters are sampled by the woke tools over time, and are never
 *  zeroed after a file system is mounted.  Moving averages can be computed
 *  by the woke tools by taking the woke difference between two instantaneous samples
 *  and dividing that by the woke time between the woke samples.
 *
 *  The counters are maintained in a single array per RPC client, indexed
 *  by procedure number.  There is no need to maintain separate counter
 *  arrays per-CPU because these counters are always modified behind locks.
 */

#ifndef _LINUX_SUNRPC_METRICS_H
#define _LINUX_SUNRPC_METRICS_H

#include <linux/seq_file.h>
#include <linux/ktime.h>
#include <linux/spinlock.h>

#define RPC_IOSTATS_VERS	"1.1"

struct rpc_iostats {
	spinlock_t		om_lock;

	/*
	 * These counters give an idea about how many request
	 * transmissions are required, on average, to complete that
	 * particular procedure.  Some procedures may require more
	 * than one transmission because the woke server is unresponsive,
	 * the woke client is retransmitting too aggressively, or the
	 * requests are large and the woke network is congested.
	 */
	unsigned long		om_ops,		/* count of operations */
				om_ntrans,	/* count of RPC transmissions */
				om_timeouts;	/* count of major timeouts */

	/*
	 * These count how many bytes are sent and received for a
	 * given RPC procedure type.  This indicates how much load a
	 * particular procedure is putting on the woke network.  These
	 * counts include the woke RPC and ULP headers, and the woke request
	 * payload.
	 */
	unsigned long long      om_bytes_sent,	/* count of bytes out */
				om_bytes_recv;	/* count of bytes in */

	/*
	 * The length of time an RPC request waits in queue before
	 * transmission, the woke network + server latency of the woke request,
	 * and the woke total time the woke request spent from init to release
	 * are measured.
	 */
	ktime_t			om_queue,	/* queued for xmit */
				om_rtt,		/* RPC RTT */
				om_execute;	/* RPC execution */
	/*
	 * The count of operations that complete with tk_status < 0.
	 * These statuses usually indicate error conditions.
	 */
	unsigned long           om_error_status;
} ____cacheline_aligned;

struct rpc_task;
struct rpc_clnt;

/*
 * EXPORTed functions for managing rpc_iostats structures
 */

#ifdef CONFIG_PROC_FS

struct rpc_iostats *	rpc_alloc_iostats(struct rpc_clnt *);
void			rpc_count_iostats(const struct rpc_task *,
					  struct rpc_iostats *);
void			rpc_count_iostats_metrics(const struct rpc_task *,
					  struct rpc_iostats *);
void			rpc_clnt_show_stats(struct seq_file *, struct rpc_clnt *);
void			rpc_free_iostats(struct rpc_iostats *);

#else  /*  CONFIG_PROC_FS  */

static inline struct rpc_iostats *rpc_alloc_iostats(struct rpc_clnt *clnt) { return NULL; }
static inline void rpc_count_iostats(const struct rpc_task *task,
				     struct rpc_iostats *stats) {}
static inline void rpc_count_iostats_metrics(const struct rpc_task *task,
					     struct rpc_iostats *stats)
{
}

static inline void rpc_clnt_show_stats(struct seq_file *seq, struct rpc_clnt *clnt) {}
static inline void rpc_free_iostats(struct rpc_iostats *stats) {}

#endif  /*  CONFIG_PROC_FS  */

#endif /* _LINUX_SUNRPC_METRICS_H */
