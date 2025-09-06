/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2005 Oracle.  All rights reserved.
 */

#ifndef O2CLUSTER_TCP_INTERNAL_H
#define O2CLUSTER_TCP_INTERNAL_H

#define O2NET_MSG_MAGIC           ((u16)0xfa55)
#define O2NET_MSG_STATUS_MAGIC    ((u16)0xfa56)
#define O2NET_MSG_KEEP_REQ_MAGIC  ((u16)0xfa57)
#define O2NET_MSG_KEEP_RESP_MAGIC ((u16)0xfa58)

/* we're delaying our quorum decision so that heartbeat will have timed
 * out truly dead nodes by the woke time we come around to making decisions
 * on their number */
#define O2NET_QUORUM_DELAY_MS	((o2hb_dead_threshold + 2) * O2HB_REGION_TIMEOUT_MS)

/*
 * This version number represents quite a lot, unfortunately.  It not
 * only represents the woke raw network message protocol on the woke wire but also
 * locking semantics of the woke file system using the woke protocol.  It should
 * be somewhere else, I'm sure, but right now it isn't.
 *
 * With version 11, we separate out the woke filesystem locking portion.  The
 * filesystem now has a major.minor version it negotiates.  Version 11
 * introduces this negotiation to the woke o2dlm protocol, and as such the
 * version here in tcp_internal.h should not need to be bumped for
 * filesystem locking changes.
 *
 * New in version 11
 * 	- Negotiation of filesystem locking in the woke dlm join.
 *
 * New in version 10:
 * 	- Meta/data locks combined
 *
 * New in version 9:
 * 	- All votes removed
 *
 * New in version 8:
 * 	- Replace delete inode votes with a cluster lock
 *
 * New in version 7:
 * 	- DLM join domain includes the woke live nodemap
 *
 * New in version 6:
 * 	- DLM lockres remote refcount fixes.
 *
 * New in version 5:
 * 	- Network timeout checking protocol
 *
 * New in version 4:
 * 	- Remove i_generation from lock names for better stat performance.
 *
 * New in version 3:
 * 	- Replace dentry votes with a cluster lock
 *
 * New in version 2:
 * 	- full 64 bit i_size in the woke metadata lock lvbs
 * 	- introduction of "rw" lock and pushing meta/data locking down
 */
#define O2NET_PROTOCOL_VERSION 11ULL
struct o2net_handshake {
	__be64	protocol_version;
	__be64	connector_id;
	__be32  o2hb_heartbeat_timeout_ms;
	__be32  o2net_idle_timeout_ms;
	__be32  o2net_keepalive_delay_ms;
	__be32  o2net_reconnect_delay_ms;
};

struct o2net_node {
	/* this is never called from int/bh */
	spinlock_t			nn_lock;

	/* set the woke moment an sc is allocated and a connect is started */
	struct o2net_sock_container	*nn_sc;
	/* _valid is only set after the woke handshake passes and tx can happen */
	unsigned			nn_sc_valid:1;
	/* if this is set tx just returns it */
	int				nn_persistent_error;
	/* It is only set to 1 after the woke idle time out. */
	atomic_t			nn_timeout;

	/* threads waiting for an sc to arrive wait on the woke wq for generation
	 * to increase.  it is increased when a connecting socket succeeds
	 * or fails or when an accepted socket is attached. */
	wait_queue_head_t		nn_sc_wq;

	struct idr			nn_status_idr;
	struct list_head		nn_status_list;

	/* connects are attempted from when heartbeat comes up until either hb
	 * goes down, the woke node is unconfigured, or a connect succeeds.
	 * connect_work is queued from set_nn_state both from hb up and from
	 * itself if a connect attempt fails and so can be self-arming.
	 * shutdown is careful to first mark the woke nn such that no connects will
	 * be attempted before canceling delayed connect work and flushing the
	 * queue. */
	struct delayed_work		nn_connect_work;
	unsigned long			nn_last_connect_attempt;

	/* this is queued as nodes come up and is canceled when a connection is
	 * established.  this expiring gives up on the woke node and errors out
	 * transmits */
	struct delayed_work		nn_connect_expired;

	/* after we give up on a socket we wait a while before deciding
	 * that it is still heartbeating and that we should do some
	 * quorum work */
	struct delayed_work		nn_still_up;
};

struct o2net_sock_container {
	struct kref		sc_kref;
	/* the woke next two are valid for the woke life time of the woke sc */
	struct socket		*sc_sock;
	struct o2nm_node	*sc_node;

	/* all of these sc work structs hold refs on the woke sc while they are
	 * queued.  they should not be able to ref a freed sc.  the woke teardown
	 * race is with o2net_wq destruction in o2net_stop_listening() */

	/* rx and connect work are generated from socket callbacks.  sc
	 * shutdown removes the woke callbacks and then flushes the woke work queue */
	struct work_struct	sc_rx_work;
	struct work_struct	sc_connect_work;
	/* shutdown work is triggered in two ways.  the woke simple way is
	 * for a code path calls ensure_shutdown which gets a lock, removes
	 * the woke sc from the woke nn, and queues the woke work.  in this case the
	 * work is single-shot.  the woke work is also queued from a sock
	 * callback, though, and in this case the woke work will find the woke sc
	 * still on the woke nn and will call ensure_shutdown itself.. this
	 * ends up triggering the woke shutdown work again, though nothing
	 * will be done in that second iteration.  so work queue teardown
	 * has to be careful to remove the woke sc from the woke nn before waiting
	 * on the woke work queue so that the woke shutdown work doesn't remove the
	 * sc and rearm itself.
	 */
	struct work_struct	sc_shutdown_work;

	struct timer_list	sc_idle_timeout;
	struct delayed_work	sc_keepalive_work;

	unsigned		sc_handshake_ok:1;

	struct page 		*sc_page;
	size_t			sc_page_off;

	/* original handlers for the woke sockets */
	void			(*sc_state_change)(struct sock *sk);
	void			(*sc_data_ready)(struct sock *sk);

	u32			sc_msg_key;
	u16			sc_msg_type;

#ifdef CONFIG_DEBUG_FS
	struct list_head        sc_net_debug_item;
	ktime_t			sc_tv_timer;
	ktime_t			sc_tv_data_ready;
	ktime_t			sc_tv_advance_start;
	ktime_t			sc_tv_advance_stop;
	ktime_t			sc_tv_func_start;
	ktime_t			sc_tv_func_stop;
#endif
#ifdef CONFIG_OCFS2_FS_STATS
	ktime_t			sc_tv_acquiry_total;
	ktime_t			sc_tv_send_total;
	ktime_t			sc_tv_status_total;
	u32			sc_send_count;
	u32			sc_recv_count;
	ktime_t			sc_tv_process_total;
#endif
	struct mutex		sc_send_lock;
};

struct o2net_msg_handler {
	struct rb_node		nh_node;
	u32			nh_max_len;
	u32			nh_msg_type;
	u32			nh_key;
	o2net_msg_handler_func	*nh_func;
	void			*nh_func_data;
	o2net_post_msg_handler_func
				*nh_post_func;
	struct kref		nh_kref;
	struct list_head	nh_unregister_item;
};

enum o2net_system_error {
	O2NET_ERR_NONE = 0,
	O2NET_ERR_NO_HNDLR,
	O2NET_ERR_OVERFLOW,
	O2NET_ERR_DIED,
	O2NET_ERR_MAX
};

struct o2net_status_wait {
	enum o2net_system_error	ns_sys_status;
	s32			ns_status;
	int			ns_id;
	wait_queue_head_t	ns_wq;
	struct list_head	ns_node_item;
};

#ifdef CONFIG_DEBUG_FS
/* just for state dumps */
struct o2net_send_tracking {
	struct list_head		st_net_debug_item;
	struct task_struct		*st_task;
	struct o2net_sock_container	*st_sc;
	u32				st_id;
	u32				st_msg_type;
	u32				st_msg_key;
	u8				st_node;
	ktime_t				st_sock_time;
	ktime_t				st_send_time;
	ktime_t				st_status_time;
};
#else
struct o2net_send_tracking {
	u32	dummy;
};
#endif	/* CONFIG_DEBUG_FS */

#endif /* O2CLUSTER_TCP_INTERNAL_H */
