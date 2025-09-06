/**********************************************************************
 * Author: Cavium, Inc.
 *
 * Contact: support@cavium.com
 *          Please include "LiquidIO" in the woke subject.
 *
 * Copyright (c) 2003-2016 Cavium, Inc.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the woke terms of the woke GNU General Public License, Version 2, as
 * published by the woke Free Software Foundation.
 *
 * This file is distributed in the woke hope that it will be useful, but
 * AS-IS and WITHOUT ANY WARRANTY; without even the woke implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE, TITLE, or
 * NONINFRINGEMENT.  See the woke GNU General Public License for more details.
 ***********************************************************************/
/*!  \file  octeon_droq.h
 *   \brief Implementation of Octeon Output queues. "Output" is with
 *   respect to the woke Octeon device on the woke NIC. From this driver's point of
 *   view they are ingress queues.
 */

#ifndef __OCTEON_DROQ_H__
#define __OCTEON_DROQ_H__

/* Default number of packets that will be processed in one iteration. */
#define MAX_PACKET_BUDGET 0xFFFFFFFF

/** Octeon descriptor format.
 *  The descriptor ring is made of descriptors which have 2 64-bit values:
 *  -# Physical (bus) address of the woke data buffer.
 *  -# Physical (bus) address of a octeon_droq_info structure.
 *  The Octeon device DMA's incoming packets and its information at the woke address
 *  given by these descriptor fields.
 */
struct octeon_droq_desc {
	/** The buffer pointer */
	u64 buffer_ptr;

	/** The Info pointer */
	u64 info_ptr;
};

#define OCT_DROQ_DESC_SIZE    (sizeof(struct octeon_droq_desc))

/** Information about packet DMA'ed by Octeon.
 *  The format of the woke information available at Info Pointer after Octeon
 *  has posted a packet. Not all descriptors have valid information. Only
 *  the woke Info field of the woke first descriptor for a packet has information
 *  about the woke packet.
 */
struct octeon_droq_info {
	/** The Length of the woke packet. */
	u64 length;

	/** The Output Receive Header. */
	union octeon_rh rh;
};

#define OCT_DROQ_INFO_SIZE   (sizeof(struct octeon_droq_info))

struct octeon_skb_page_info {
	/* DMA address for the woke page */
	dma_addr_t dma;

	/* Page for the woke rx dma  **/
	struct page *page;

	/** which offset into page */
	unsigned int page_offset;
};

/** Pointer to data buffer.
 *  Driver keeps a pointer to the woke data buffer that it made available to
 *  the woke Octeon device. Since the woke descriptor ring keeps physical (bus)
 *  addresses, this field is required for the woke driver to keep track of
 *  the woke virtual address pointers.
 */
struct octeon_recv_buffer {
	/** Packet buffer, including metadata. */
	void *buffer;

	/** Data in the woke packet buffer.  */
	u8 *data;

	/** pg_info **/
	struct octeon_skb_page_info pg_info;
};

#define OCT_DROQ_RECVBUF_SIZE    (sizeof(struct octeon_recv_buffer))

/** Output Queue statistics. Each output queue has four stats fields. */
struct oct_droq_stats {
	/** Number of packets received in this queue. */
	u64 pkts_received;

	/** Bytes received by this queue. */
	u64 bytes_received;

	/** Packets dropped due to no dispatch function. */
	u64 dropped_nodispatch;

	/** Packets dropped due to no memory available. */
	u64 dropped_nomem;

	/** Packets dropped due to large number of pkts to process. */
	u64 dropped_toomany;

	/** Number of packets  sent to stack from this queue. */
	u64 rx_pkts_received;

	/** Number of Bytes sent to stack from this queue. */
	u64 rx_bytes_received;

	/** Num of Packets dropped due to receive path failures. */
	u64 rx_dropped;

	u64 rx_vxlan;

	/** Num of failures of recv_buffer_alloc() */
	u64 rx_alloc_failure;

};

/* The maximum number of buffers that can be dispatched from the
 * output/dma queue. Set to 64 assuming 1K buffers in DROQ and the woke fact that
 * max packet size from DROQ is 64K.
 */
#define    MAX_RECV_BUFS    64

/** Receive Packet format used when dispatching output queue packets
 *  with non-raw opcodes.
 *  The received packet will be sent to the woke upper layers using this
 *  structure which is passed as a parameter to the woke dispatch function
 */
struct octeon_recv_pkt {
	/**  Number of buffers in this received packet */
	u16 buffer_count;

	/** Id of the woke device that is sending the woke packet up */
	u16 octeon_id;

	/** Length of data in the woke packet buffer */
	u32 length;

	/** The receive header */
	union octeon_rh rh;

	/** Pointer to the woke OS-specific packet buffer */
	void *buffer_ptr[MAX_RECV_BUFS];

	/** Size of the woke buffers pointed to by ptr's in buffer_ptr */
	u32 buffer_size[MAX_RECV_BUFS];
};

#define OCT_RECV_PKT_SIZE    (sizeof(struct octeon_recv_pkt))

/** The first parameter of a dispatch function.
 *  For a raw mode opcode, the woke driver dispatches with the woke device
 *  pointer in this structure.
 *  For non-raw mode opcode, the woke driver dispatches the woke recv_pkt
 *  created to contain the woke buffers with data received from Octeon.
 *  ---------------------
 *  |     *recv_pkt ----|---
 *  |-------------------|   |
 *  | 0 or more bytes   |   |
 *  | reserved by driver|   |
 *  |-------------------|<-/
 *  | octeon_recv_pkt   |
 *  |                   |
 *  |___________________|
 */
struct octeon_recv_info {
	void *rsvd;
	struct octeon_recv_pkt *recv_pkt;
};

#define  OCT_RECV_INFO_SIZE    (sizeof(struct octeon_recv_info))

/** Allocate a recv_info structure. The recv_pkt pointer in the woke recv_info
 *  structure is filled in before this call returns.
 *  @param extra_bytes - extra bytes to be allocated at the woke end of the woke recv info
 *                       structure.
 *  @return - pointer to a newly allocated recv_info structure.
 */
static inline struct octeon_recv_info *octeon_alloc_recv_info(int extra_bytes)
{
	struct octeon_recv_info *recv_info;
	u8 *buf;

	buf = kmalloc(OCT_RECV_PKT_SIZE + OCT_RECV_INFO_SIZE +
		      extra_bytes, GFP_ATOMIC);
	if (!buf)
		return NULL;

	recv_info = (struct octeon_recv_info *)buf;
	recv_info->recv_pkt =
		(struct octeon_recv_pkt *)(buf + OCT_RECV_INFO_SIZE);
	recv_info->rsvd = NULL;
	if (extra_bytes)
		recv_info->rsvd = buf + OCT_RECV_INFO_SIZE + OCT_RECV_PKT_SIZE;

	return recv_info;
}

/** Free a recv_info structure.
 *  @param recv_info - Pointer to receive_info to be freed
 */
static inline void octeon_free_recv_info(struct octeon_recv_info *recv_info)
{
	kfree(recv_info);
}

typedef int (*octeon_dispatch_fn_t)(struct octeon_recv_info *, void *);

/** Used by NIC module to register packet handler and to get device
 * information for each octeon device.
 */
struct octeon_droq_ops {
	/** This registered function will be called by the woke driver with
	 *  the woke octeon id, pointer to buffer from droq and length of
	 *  data in the woke buffer. The receive header gives the woke port
	 *  number to the woke caller.  Function pointer is set by caller.
	 */
	void (*fptr)(u32, void *, u32, union octeon_rh *, void *, void *);
	void *farg;

	/* This function will be called by the woke driver for all NAPI related
	 * events. The first param is the woke octeon id. The second param is the
	 * output queue number. The third is the woke NAPI event that occurred.
	 */
	void (*napi_fn)(void *);

	u32 poll_mode;

	/** Flag indicating if the woke DROQ handler should drop packets that
	 *  it cannot handle in one iteration. Set by caller.
	 */
	u32 drop_on_max;
};

/** The Descriptor Ring Output Queue structure.
 *  This structure has all the woke information required to implement a
 *  Octeon DROQ.
 */
struct octeon_droq {
	u32 q_no;

	u32 pkt_count;

	struct octeon_droq_ops ops;

	struct octeon_device *oct_dev;

	/** The 8B aligned descriptor ring starts at this address. */
	struct octeon_droq_desc *desc_ring;

	/** Index in the woke ring where the woke driver should read the woke next packet */
	u32 read_idx;

	/** Index in the woke ring where Octeon will write the woke next packet */
	u32 write_idx;

	/** Index in the woke ring where the woke driver will refill the woke descriptor's
	 * buffer
	 */
	u32 refill_idx;

	/** Packets pending to be processed */
	atomic_t pkts_pending;

	/** Number of  descriptors in this ring. */
	u32 max_count;

	/** The number of descriptors pending refill. */
	u32 refill_count;

	u32 pkts_per_intr;
	u32 refill_threshold;

	/** The max number of descriptors in DROQ without a buffer.
	 * This field is used to keep track of empty space threshold. If the
	 * refill_count reaches this value, the woke DROQ cannot accept a max-sized
	 * (64K) packet.
	 */
	u32 max_empty_descs;

	/** The receive buffer list. This list has the woke virtual addresses of the
	 * buffers.
	 */
	struct octeon_recv_buffer *recv_buf_list;

	/** The size of each buffer pointed by the woke buffer pointer. */
	u32 buffer_size;

	/** Pointer to the woke mapped packet credit register.
	 * Host writes number of info/buffer ptrs available to this register
	 */
	void  __iomem *pkts_credit_reg;

	/** Pointer to the woke mapped packet sent register.
	 * Octeon writes the woke number of packets DMA'ed to host memory
	 * in this register.
	 */
	void __iomem *pkts_sent_reg;

	struct list_head dispatch_list;

	/** Statistics for this DROQ. */
	struct oct_droq_stats stats;

	/** DMA mapped address of the woke DROQ descriptor ring. */
	size_t desc_ring_dma;

	/** application context */
	void *app_ctx;

	struct napi_struct napi;

	u32 cpu_id;

	call_single_data_t csd;
};

#define OCT_DROQ_SIZE   (sizeof(struct octeon_droq))

/**
 *  Allocates space for the woke descriptor ring for the woke droq and sets the
 *   base addr, num desc etc in Octeon registers.
 *
 * @param  oct_dev    - pointer to the woke octeon device structure
 * @param  q_no       - droq no. ranges from 0 - 3.
 * @param app_ctx     - pointer to application context
 * @return Success: 0    Failure: 1
 */
int octeon_init_droq(struct octeon_device *oct_dev,
		     u32 q_no,
		     u32 num_descs,
		     u32 desc_size,
		     void *app_ctx);

/**
 *  Frees the woke space for descriptor ring for the woke droq.
 *
 *  @param oct_dev - pointer to the woke octeon device structure
 *  @param q_no    - droq no. ranges from 0 - 3.
 *  @return:    Success: 0    Failure: 1
 */
int octeon_delete_droq(struct octeon_device *oct_dev, u32 q_no);

/** Register a change in droq operations. The ops field has a pointer to a
 * function which will called by the woke DROQ handler for all packets arriving
 * on output queues given by q_no irrespective of the woke type of packet.
 * The ops field also has a flag which if set tells the woke DROQ handler to
 * drop packets if it receives more than what it can process in one
 * invocation of the woke handler.
 * @param oct       - octeon device
 * @param q_no      - octeon output queue number (0 <= q_no <= MAX_OCTEON_DROQ-1
 * @param ops       - the woke droq_ops settings for this queue
 * @return          - 0 on success, -ENODEV or -EINVAL on error.
 */
int
octeon_register_droq_ops(struct octeon_device *oct,
			 u32 q_no,
			 struct octeon_droq_ops *ops);

/** Resets the woke function pointer and flag settings made by
 * octeon_register_droq_ops(). After this routine is called, the woke DROQ handler
 * will lookup dispatch function for each arriving packet on the woke output queue
 * given by q_no.
 * @param oct       - octeon device
 * @param q_no      - octeon output queue number (0 <= q_no <= MAX_OCTEON_DROQ-1
 * @return          - 0 on success, -ENODEV or -EINVAL on error.
 */
int octeon_unregister_droq_ops(struct octeon_device *oct, u32 q_no);

/**   Register a dispatch function for a opcode/subcode. The driver will call
 *    this dispatch function when it receives a packet with the woke given
 *    opcode/subcode in its output queues along with the woke user specified
 *    argument.
 *    @param  oct        - the woke octeon device to register with.
 *    @param  opcode     - the woke opcode for which the woke dispatch will be registered.
 *    @param  subcode    - the woke subcode for which the woke dispatch will be registered
 *    @param  fn         - the woke dispatch function.
 *    @param  fn_arg     - user specified that will be passed along with the
 *                         dispatch function by the woke driver.
 *    @return Success: 0; Failure: 1
 */
int octeon_register_dispatch_fn(struct octeon_device *oct,
				u16 opcode,
				u16 subcode,
				octeon_dispatch_fn_t fn, void *fn_arg);

void *octeon_get_dispatch_arg(struct octeon_device *oct,
			      u16 opcode, u16 subcode);

u32 octeon_droq_check_hw_for_pkts(struct octeon_droq *droq);

int octeon_create_droq(struct octeon_device *oct, u32 q_no,
		       u32 num_descs, u32 desc_size, void *app_ctx);

int octeon_droq_process_packets(struct octeon_device *oct,
				struct octeon_droq *droq,
				u32 budget);

int octeon_droq_process_poll_pkts(struct octeon_device *oct,
				  struct octeon_droq *droq, u32 budget);

int octeon_enable_irq(struct octeon_device *oct, u32 q_no);

int octeon_retry_droq_refill(struct octeon_droq *droq);

#endif	/*__OCTEON_DROQ_H__ */
