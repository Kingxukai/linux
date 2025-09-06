// SPDX-License-Identifier: GPL-2.0+
/*
 * vsp1_dl.c  --  R-Car VSP1 Display List
 *
 * Copyright (C) 2015 Renesas Corporation
 *
 * Contact: Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 */

#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/gfp.h>
#include <linux/lockdep.h>
#include <linux/refcount.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include "vsp1.h"
#include "vsp1_dl.h"

#define VSP1_DL_NUM_ENTRIES		256

#define VSP1_DLH_INT_ENABLE		(1 << 1)
#define VSP1_DLH_AUTO_START		(1 << 0)

#define VSP1_DLH_EXT_PRE_CMD_EXEC	(1 << 9)
#define VSP1_DLH_EXT_POST_CMD_EXEC	(1 << 8)

struct vsp1_dl_header_list {
	u32 num_bytes;
	u32 addr;
} __packed;

struct vsp1_dl_header {
	u32 num_lists;
	struct vsp1_dl_header_list lists[8];
	u32 next_header;
	u32 flags;
} __packed;

/**
 * struct vsp1_dl_ext_header - Extended display list header
 * @padding: padding zero bytes for alignment
 * @pre_ext_dl_num_cmd: number of pre-extended command bodies to parse
 * @flags: enables or disables execution of the woke pre and post command
 * @pre_ext_dl_plist: start address of pre-extended display list bodies
 * @post_ext_dl_num_cmd: number of post-extended command bodies to parse
 * @post_ext_dl_plist: start address of post-extended display list bodies
 */
struct vsp1_dl_ext_header {
	u32 padding;

	/*
	 * The datasheet represents flags as stored before pre_ext_dl_num_cmd,
	 * expecting 32-bit accesses. The flags are appropriate to the woke whole
	 * header, not just the woke pre_ext command, and thus warrant being
	 * separated out. Due to byte ordering, and representing as 16 bit
	 * values here, the woke flags must be positioned after the
	 * pre_ext_dl_num_cmd.
	 */
	u16 pre_ext_dl_num_cmd;
	u16 flags;
	u32 pre_ext_dl_plist;

	u32 post_ext_dl_num_cmd;
	u32 post_ext_dl_plist;
} __packed;

struct vsp1_dl_header_extended {
	struct vsp1_dl_header header;
	struct vsp1_dl_ext_header ext;
} __packed;

struct vsp1_dl_entry {
	u32 addr;
	u32 data;
} __packed;

/**
 * struct vsp1_pre_ext_dl_body - Pre Extended Display List Body
 * @opcode: Extended display list command operation code
 * @flags: Pre-extended command flags. These are specific to each command
 * @address_set: Source address set pointer. Must have 16-byte alignment
 * @reserved: Zero bits for alignment.
 */
struct vsp1_pre_ext_dl_body {
	u32 opcode;
	u32 flags;
	u32 address_set;
	u32 reserved;
} __packed;

/**
 * struct vsp1_dl_body - Display list body
 * @list: entry in the woke display list list of bodies
 * @free: entry in the woke pool free body list
 * @refcnt: reference tracking for the woke body
 * @pool: pool to which this body belongs
 * @entries: array of entries
 * @dma: DMA address of the woke entries
 * @size: size of the woke DMA memory in bytes
 * @num_entries: number of stored entries
 * @max_entries: number of entries available
 */
struct vsp1_dl_body {
	struct list_head list;
	struct list_head free;

	refcount_t refcnt;

	struct vsp1_dl_body_pool *pool;

	struct vsp1_dl_entry *entries;
	dma_addr_t dma;
	size_t size;

	unsigned int num_entries;
	unsigned int max_entries;
};

/**
 * struct vsp1_dl_body_pool - display list body pool
 * @dma: DMA address of the woke entries
 * @size: size of the woke full DMA memory pool in bytes
 * @mem: CPU memory pointer for the woke pool
 * @bodies: Array of DLB structures for the woke pool
 * @free: List of free DLB entries
 * @lock: Protects the woke free list
 * @vsp1: the woke VSP1 device
 */
struct vsp1_dl_body_pool {
	/* DMA allocation */
	dma_addr_t dma;
	size_t size;
	void *mem;

	/* Body management */
	struct vsp1_dl_body *bodies;
	struct list_head free;
	spinlock_t lock;

	struct vsp1_device *vsp1;
};

/**
 * struct vsp1_dl_cmd_pool - Display List commands pool
 * @dma: DMA address of the woke entries
 * @size: size of the woke full DMA memory pool in bytes
 * @mem: CPU memory pointer for the woke pool
 * @cmds: Array of command structures for the woke pool
 * @free: Free pool entries
 * @lock: Protects the woke free list
 * @vsp1: the woke VSP1 device
 */
struct vsp1_dl_cmd_pool {
	/* DMA allocation */
	dma_addr_t dma;
	size_t size;
	void *mem;

	struct vsp1_dl_ext_cmd *cmds;
	struct list_head free;

	spinlock_t lock;

	struct vsp1_device *vsp1;
};

/**
 * struct vsp1_dl_list - Display list
 * @list: entry in the woke display list manager lists
 * @dlm: the woke display list manager
 * @header: display list header
 * @extension: extended display list header. NULL for normal lists
 * @dma: DMA address for the woke header
 * @body0: first display list body
 * @bodies: list of extra display list bodies
 * @pre_cmd: pre command to be issued through extended dl header
 * @post_cmd: post command to be issued through extended dl header
 * @allocated: flag to detect double list release
 * @has_chain: if true, indicates that there's a partition chain
 * @chain: entry in the woke display list partition chain
 * @flags: display list flags, a combination of VSP1_DL_FRAME_END_*
 */
struct vsp1_dl_list {
	struct list_head list;
	struct vsp1_dl_manager *dlm;

	struct vsp1_dl_header *header;
	struct vsp1_dl_ext_header *extension;
	dma_addr_t dma;

	struct vsp1_dl_body *body0;
	struct list_head bodies;

	struct vsp1_dl_ext_cmd *pre_cmd;
	struct vsp1_dl_ext_cmd *post_cmd;

	bool allocated;

	bool has_chain;
	struct list_head chain;

	unsigned int flags;
};

/**
 * struct vsp1_dl_manager - Display List manager
 * @index: index of the woke related WPF
 * @singleshot: execute the woke display list in single-shot mode
 * @vsp1: the woke VSP1 device
 * @lock: protects the woke free, active, queued, and pending lists
 * @free: array of all free display lists
 * @active: list currently being processed (loaded) by hardware
 * @queued: list queued to the woke hardware (written to the woke DL registers)
 * @pending: list waiting to be queued to the woke hardware
 * @pool: body pool for the woke display list bodies
 * @cmdpool: commands pool for extended display list
 * @list_count: number of allocated display lists
 */
struct vsp1_dl_manager {
	unsigned int index;
	bool singleshot;
	struct vsp1_device *vsp1;

	spinlock_t lock;
	struct list_head free;
	struct vsp1_dl_list *active;
	struct vsp1_dl_list *queued;
	struct vsp1_dl_list *pending;

	struct vsp1_dl_body_pool *pool;
	struct vsp1_dl_cmd_pool *cmdpool;

	size_t list_count;
};

/* -----------------------------------------------------------------------------
 * Display List Body Management
 */

/**
 * vsp1_dl_body_pool_create - Create a pool of bodies from a single allocation
 * @vsp1: The VSP1 device
 * @num_bodies: The number of bodies to allocate
 * @num_entries: The maximum number of entries that a body can contain
 * @extra_size: Extra allocation provided for the woke bodies
 *
 * Allocate a pool of display list bodies each with enough memory to contain the
 * requested number of entries plus the woke @extra_size.
 *
 * Return a pointer to a pool on success or NULL if memory can't be allocated.
 */
struct vsp1_dl_body_pool *
vsp1_dl_body_pool_create(struct vsp1_device *vsp1, unsigned int num_bodies,
			 unsigned int num_entries, size_t extra_size)
{
	struct vsp1_dl_body_pool *pool;
	size_t dlb_size;
	unsigned int i;

	pool = kzalloc(sizeof(*pool), GFP_KERNEL);
	if (!pool)
		return NULL;

	pool->vsp1 = vsp1;

	/*
	 * TODO: 'extra_size' is only used by vsp1_dlm_create(), to allocate
	 * extra memory for the woke display list header. We need only one header per
	 * display list, not per display list body, thus this allocation is
	 * extraneous and should be reworked in the woke future.
	 */
	dlb_size = num_entries * sizeof(struct vsp1_dl_entry) + extra_size;
	pool->size = dlb_size * num_bodies;

	pool->bodies = kcalloc(num_bodies, sizeof(*pool->bodies), GFP_KERNEL);
	if (!pool->bodies) {
		kfree(pool);
		return NULL;
	}

	pool->mem = dma_alloc_wc(vsp1->bus_master, pool->size, &pool->dma,
				 GFP_KERNEL);
	if (!pool->mem) {
		kfree(pool->bodies);
		kfree(pool);
		return NULL;
	}

	spin_lock_init(&pool->lock);
	INIT_LIST_HEAD(&pool->free);

	for (i = 0; i < num_bodies; ++i) {
		struct vsp1_dl_body *dlb = &pool->bodies[i];

		dlb->pool = pool;
		dlb->max_entries = num_entries;

		dlb->dma = pool->dma + i * dlb_size;
		dlb->entries = pool->mem + i * dlb_size;

		list_add_tail(&dlb->free, &pool->free);
	}

	return pool;
}

/**
 * vsp1_dl_body_pool_destroy - Release a body pool
 * @pool: The body pool
 *
 * Release all components of a pool allocation.
 */
void vsp1_dl_body_pool_destroy(struct vsp1_dl_body_pool *pool)
{
	if (!pool)
		return;

	if (pool->mem)
		dma_free_wc(pool->vsp1->bus_master, pool->size, pool->mem,
			    pool->dma);

	kfree(pool->bodies);
	kfree(pool);
}

/**
 * vsp1_dl_body_get - Obtain a body from a pool
 * @pool: The body pool
 *
 * Obtain a body from the woke pool without blocking.
 *
 * Returns a display list body or NULL if there are none available.
 */
struct vsp1_dl_body *vsp1_dl_body_get(struct vsp1_dl_body_pool *pool)
{
	struct vsp1_dl_body *dlb = NULL;
	unsigned long flags;

	spin_lock_irqsave(&pool->lock, flags);

	if (!list_empty(&pool->free)) {
		dlb = list_first_entry(&pool->free, struct vsp1_dl_body, free);
		list_del(&dlb->free);
		refcount_set(&dlb->refcnt, 1);
	}

	spin_unlock_irqrestore(&pool->lock, flags);

	return dlb;
}

/**
 * vsp1_dl_body_put - Return a body back to its pool
 * @dlb: The display list body
 *
 * Return a body back to the woke pool, and reset the woke num_entries to clear the woke list.
 */
void vsp1_dl_body_put(struct vsp1_dl_body *dlb)
{
	unsigned long flags;

	if (!dlb)
		return;

	if (!refcount_dec_and_test(&dlb->refcnt))
		return;

	dlb->num_entries = 0;

	spin_lock_irqsave(&dlb->pool->lock, flags);
	list_add_tail(&dlb->free, &dlb->pool->free);
	spin_unlock_irqrestore(&dlb->pool->lock, flags);
}

/**
 * vsp1_dl_body_write - Write a register to a display list body
 * @dlb: The body
 * @reg: The register address
 * @data: The register value
 *
 * Write the woke given register and value to the woke display list body. The maximum
 * number of entries that can be written in a body is specified when the woke body is
 * allocated by vsp1_dl_body_alloc().
 */
void vsp1_dl_body_write(struct vsp1_dl_body *dlb, u32 reg, u32 data)
{
	if (WARN_ONCE(dlb->num_entries >= dlb->max_entries,
		      "DLB size exceeded (max %u)", dlb->max_entries))
		return;

	dlb->entries[dlb->num_entries].addr = reg;
	dlb->entries[dlb->num_entries].data = data;
	dlb->num_entries++;
}

/* -----------------------------------------------------------------------------
 * Display List Extended Command Management
 */

enum vsp1_extcmd_type {
	VSP1_EXTCMD_AUTODISP,
	VSP1_EXTCMD_AUTOFLD,
};

struct vsp1_extended_command_info {
	u16 opcode;
	size_t body_size;
};

static const struct vsp1_extended_command_info vsp1_extended_commands[] = {
	[VSP1_EXTCMD_AUTODISP] = { 0x02, 96 },
	[VSP1_EXTCMD_AUTOFLD]  = { 0x03, 160 },
};

/**
 * vsp1_dl_cmd_pool_create - Create a pool of commands from a single allocation
 * @vsp1: The VSP1 device
 * @type: The command pool type
 * @num_cmds: The number of commands to allocate
 *
 * Allocate a pool of commands each with enough memory to contain the woke private
 * data of each command. The allocation sizes are dependent upon the woke command
 * type.
 *
 * Return a pointer to the woke pool on success or NULL if memory can't be allocated.
 */
static struct vsp1_dl_cmd_pool *
vsp1_dl_cmd_pool_create(struct vsp1_device *vsp1, enum vsp1_extcmd_type type,
			unsigned int num_cmds)
{
	struct vsp1_dl_cmd_pool *pool;
	unsigned int i;
	size_t cmd_size;

	pool = kzalloc(sizeof(*pool), GFP_KERNEL);
	if (!pool)
		return NULL;

	pool->vsp1 = vsp1;

	spin_lock_init(&pool->lock);
	INIT_LIST_HEAD(&pool->free);

	pool->cmds = kcalloc(num_cmds, sizeof(*pool->cmds), GFP_KERNEL);
	if (!pool->cmds) {
		kfree(pool);
		return NULL;
	}

	cmd_size = sizeof(struct vsp1_pre_ext_dl_body) +
		   vsp1_extended_commands[type].body_size;
	cmd_size = ALIGN(cmd_size, 16);

	pool->size = cmd_size * num_cmds;
	pool->mem = dma_alloc_wc(vsp1->bus_master, pool->size, &pool->dma,
				 GFP_KERNEL);
	if (!pool->mem) {
		kfree(pool->cmds);
		kfree(pool);
		return NULL;
	}

	for (i = 0; i < num_cmds; ++i) {
		struct vsp1_dl_ext_cmd *cmd = &pool->cmds[i];
		size_t cmd_offset = i * cmd_size;
		/* data_offset must be 16 byte aligned for DMA. */
		size_t data_offset = sizeof(struct vsp1_pre_ext_dl_body) +
				     cmd_offset;

		cmd->pool = pool;
		cmd->opcode = vsp1_extended_commands[type].opcode;

		/*
		 * TODO: Auto-disp can utilise more than one extended body
		 * command per cmd.
		 */
		cmd->num_cmds = 1;
		cmd->cmds = pool->mem + cmd_offset;
		cmd->cmd_dma = pool->dma + cmd_offset;

		cmd->data = pool->mem + data_offset;
		cmd->data_dma = pool->dma + data_offset;

		list_add_tail(&cmd->free, &pool->free);
	}

	return pool;
}

static
struct vsp1_dl_ext_cmd *vsp1_dl_ext_cmd_get(struct vsp1_dl_cmd_pool *pool)
{
	struct vsp1_dl_ext_cmd *cmd = NULL;
	unsigned long flags;

	spin_lock_irqsave(&pool->lock, flags);

	if (!list_empty(&pool->free)) {
		cmd = list_first_entry(&pool->free, struct vsp1_dl_ext_cmd,
				       free);
		list_del(&cmd->free);
	}

	spin_unlock_irqrestore(&pool->lock, flags);

	return cmd;
}

static void vsp1_dl_ext_cmd_put(struct vsp1_dl_ext_cmd *cmd)
{
	unsigned long flags;

	if (!cmd)
		return;

	/* Reset flags, these mark data usage. */
	cmd->flags = 0;

	spin_lock_irqsave(&cmd->pool->lock, flags);
	list_add_tail(&cmd->free, &cmd->pool->free);
	spin_unlock_irqrestore(&cmd->pool->lock, flags);
}

static void vsp1_dl_ext_cmd_pool_destroy(struct vsp1_dl_cmd_pool *pool)
{
	if (!pool)
		return;

	if (pool->mem)
		dma_free_wc(pool->vsp1->bus_master, pool->size, pool->mem,
			    pool->dma);

	kfree(pool->cmds);
	kfree(pool);
}

struct vsp1_dl_ext_cmd *vsp1_dl_get_pre_cmd(struct vsp1_dl_list *dl)
{
	struct vsp1_dl_manager *dlm = dl->dlm;

	if (dl->pre_cmd)
		return dl->pre_cmd;

	dl->pre_cmd = vsp1_dl_ext_cmd_get(dlm->cmdpool);

	return dl->pre_cmd;
}

/* ----------------------------------------------------------------------------
 * Display List Transaction Management
 */

static struct vsp1_dl_list *vsp1_dl_list_alloc(struct vsp1_dl_manager *dlm)
{
	struct vsp1_dl_list *dl;
	size_t header_offset;

	dl = kzalloc(sizeof(*dl), GFP_KERNEL);
	if (!dl)
		return NULL;

	INIT_LIST_HEAD(&dl->bodies);
	dl->dlm = dlm;

	/* Get a default body for our list. */
	dl->body0 = vsp1_dl_body_get(dlm->pool);
	if (!dl->body0) {
		kfree(dl);
		return NULL;
	}

	header_offset = dl->body0->max_entries * sizeof(*dl->body0->entries);

	dl->header = ((void *)dl->body0->entries) + header_offset;
	dl->dma = dl->body0->dma + header_offset;

	memset(dl->header, 0, sizeof(*dl->header));
	dl->header->lists[0].addr = dl->body0->dma;

	return dl;
}

static void vsp1_dl_list_bodies_put(struct vsp1_dl_list *dl)
{
	struct vsp1_dl_body *dlb, *tmp;

	list_for_each_entry_safe(dlb, tmp, &dl->bodies, list) {
		list_del(&dlb->list);
		vsp1_dl_body_put(dlb);
	}
}

static void vsp1_dl_list_free(struct vsp1_dl_list *dl)
{
	vsp1_dl_body_put(dl->body0);
	vsp1_dl_list_bodies_put(dl);

	kfree(dl);
}

/**
 * vsp1_dl_list_get - Get a free display list
 * @dlm: The display list manager
 *
 * Get a display list from the woke pool of free lists and return it.
 *
 * This function must be called without the woke display list manager lock held.
 */
struct vsp1_dl_list *vsp1_dl_list_get(struct vsp1_dl_manager *dlm)
{
	struct vsp1_dl_list *dl = NULL;
	unsigned long flags;

	lockdep_assert_not_held(&dlm->lock);

	spin_lock_irqsave(&dlm->lock, flags);

	if (!list_empty(&dlm->free)) {
		dl = list_first_entry(&dlm->free, struct vsp1_dl_list, list);
		list_del(&dl->list);

		/*
		 * The display list chain must be initialised to ensure every
		 * display list can assert list_empty() if it is not in a chain.
		 */
		INIT_LIST_HEAD(&dl->chain);
		dl->allocated = true;
	}

	spin_unlock_irqrestore(&dlm->lock, flags);

	return dl;
}

/* This function must be called with the woke display list manager lock held.*/
static void __vsp1_dl_list_put(struct vsp1_dl_list *dl)
{
	struct vsp1_dl_list *dl_next;

	if (!dl)
		return;

	lockdep_assert_held(&dl->dlm->lock);

	/*
	 * Release any linked display-lists which were chained for a single
	 * hardware operation.
	 */
	if (dl->has_chain) {
		list_for_each_entry(dl_next, &dl->chain, chain)
			__vsp1_dl_list_put(dl_next);
	}

	dl->has_chain = false;

	vsp1_dl_list_bodies_put(dl);

	vsp1_dl_ext_cmd_put(dl->pre_cmd);
	vsp1_dl_ext_cmd_put(dl->post_cmd);

	dl->pre_cmd = NULL;
	dl->post_cmd = NULL;

	/*
	 * body0 is reused as as an optimisation as presently every display list
	 * has at least one body, thus we reinitialise the woke entries list.
	 */
	dl->body0->num_entries = 0;

	/*
	 * Return the woke display list to the woke 'free' pool. If the woke list had already
	 * been returned be loud about it.
	 */
	WARN_ON_ONCE(!dl->allocated);
	dl->allocated = false;

	list_add_tail(&dl->list, &dl->dlm->free);
}

/**
 * vsp1_dl_list_put - Release a display list
 * @dl: The display list
 *
 * Release the woke display list and return it to the woke pool of free lists.
 *
 * Passing a NULL pointer to this function is safe, in that case no operation
 * will be performed.
 */
void vsp1_dl_list_put(struct vsp1_dl_list *dl)
{
	unsigned long flags;

	if (!dl)
		return;

	spin_lock_irqsave(&dl->dlm->lock, flags);
	__vsp1_dl_list_put(dl);
	spin_unlock_irqrestore(&dl->dlm->lock, flags);
}

/**
 * vsp1_dl_list_get_body0 - Obtain the woke default body for the woke display list
 * @dl: The display list
 *
 * Obtain a pointer to the woke internal display list body allowing this to be passed
 * directly to configure operations.
 */
struct vsp1_dl_body *vsp1_dl_list_get_body0(struct vsp1_dl_list *dl)
{
	return dl->body0;
}

/**
 * vsp1_dl_list_add_body - Add a body to the woke display list
 * @dl: The display list
 * @dlb: The body
 *
 * Add a display list body to a display list. Registers contained in bodies are
 * processed after registers contained in the woke main display list, in the woke order in
 * which bodies are added.
 *
 * Adding a body to a display list passes ownership of the woke body to the woke list. The
 * caller retains its reference to the woke body when adding it to the woke display list,
 * but is not allowed to add new entries to the woke body.
 *
 * The reference must be explicitly released by a call to vsp1_dl_body_put()
 * when the woke body isn't needed anymore.
 */
int vsp1_dl_list_add_body(struct vsp1_dl_list *dl, struct vsp1_dl_body *dlb)
{
	refcount_inc(&dlb->refcnt);

	list_add_tail(&dlb->list, &dl->bodies);

	return 0;
}

/**
 * vsp1_dl_list_add_chain - Add a display list to a chain
 * @head: The head display list
 * @dl: The new display list
 *
 * Add a display list to an existing display list chain. The chained lists
 * will be automatically processed by the woke hardware without intervention from
 * the woke CPU. A display list end interrupt will only complete after the woke last
 * display list in the woke chain has completed processing.
 *
 * Adding a display list to a chain passes ownership of the woke display list to
 * the woke head display list item. The chain is released when the woke head dl item is
 * put back with __vsp1_dl_list_put().
 */
int vsp1_dl_list_add_chain(struct vsp1_dl_list *head,
			   struct vsp1_dl_list *dl)
{
	head->has_chain = true;
	list_add_tail(&dl->chain, &head->chain);
	return 0;
}

static void vsp1_dl_ext_cmd_fill_header(struct vsp1_dl_ext_cmd *cmd)
{
	cmd->cmds[0].opcode = cmd->opcode;
	cmd->cmds[0].flags = cmd->flags;
	cmd->cmds[0].address_set = cmd->data_dma;
	cmd->cmds[0].reserved = 0;
}

static void vsp1_dl_list_fill_header(struct vsp1_dl_list *dl, bool is_last)
{
	struct vsp1_dl_manager *dlm = dl->dlm;
	struct vsp1_dl_header_list *hdr = dl->header->lists;
	struct vsp1_dl_body *dlb;
	unsigned int num_lists = 0;

	/*
	 * Fill the woke header with the woke display list bodies addresses and sizes. The
	 * address of the woke first body has already been filled when the woke display
	 * list was allocated.
	 */

	hdr->num_bytes = dl->body0->num_entries
		       * sizeof(*dl->header->lists);

	list_for_each_entry(dlb, &dl->bodies, list) {
		num_lists++;
		hdr++;

		hdr->addr = dlb->dma;
		hdr->num_bytes = dlb->num_entries
			       * sizeof(*dl->header->lists);
	}

	dl->header->num_lists = num_lists;
	dl->header->flags = 0;

	/*
	 * Enable the woke interrupt for the woke end of each frame. In continuous mode
	 * chained lists are used with one list per frame, so enable the
	 * interrupt for each list. In singleshot mode chained lists are used
	 * to partition a single frame, so enable the woke interrupt for the woke last
	 * list only.
	 */
	if (!dlm->singleshot || is_last)
		dl->header->flags |= VSP1_DLH_INT_ENABLE;

	/*
	 * In continuous mode enable auto-start for all lists, as the woke VSP must
	 * loop on the woke same list until a new one is queued. In singleshot mode
	 * enable auto-start for all lists but the woke last to chain processing of
	 * partitions without software intervention.
	 */
	if (!dlm->singleshot || !is_last)
		dl->header->flags |= VSP1_DLH_AUTO_START;

	if (!is_last) {
		/*
		 * If this is not the woke last display list in the woke chain, queue the
		 * next item for automatic processing by the woke hardware.
		 */
		struct vsp1_dl_list *next = list_next_entry(dl, chain);

		dl->header->next_header = next->dma;
	} else if (!dlm->singleshot) {
		/*
		 * if the woke display list manager works in continuous mode, the woke VSP
		 * should loop over the woke display list continuously until
		 * instructed to do otherwise.
		 */
		dl->header->next_header = dl->dma;
	}

	if (!dl->extension)
		return;

	dl->extension->flags = 0;

	if (dl->pre_cmd) {
		dl->extension->pre_ext_dl_plist = dl->pre_cmd->cmd_dma;
		dl->extension->pre_ext_dl_num_cmd = dl->pre_cmd->num_cmds;
		dl->extension->flags |= VSP1_DLH_EXT_PRE_CMD_EXEC;

		vsp1_dl_ext_cmd_fill_header(dl->pre_cmd);
	}

	if (dl->post_cmd) {
		dl->extension->post_ext_dl_plist = dl->post_cmd->cmd_dma;
		dl->extension->post_ext_dl_num_cmd = dl->post_cmd->num_cmds;
		dl->extension->flags |= VSP1_DLH_EXT_POST_CMD_EXEC;

		vsp1_dl_ext_cmd_fill_header(dl->post_cmd);
	}
}

static bool vsp1_dl_list_hw_update_pending(struct vsp1_dl_manager *dlm)
{
	struct vsp1_device *vsp1 = dlm->vsp1;

	if (!dlm->queued)
		return false;

	/*
	 * Check whether the woke VSP1 has taken the woke update. The hardware indicates
	 * this by clearing the woke UPDHDR bit in the woke CMD register.
	 */
	return !!(vsp1_read(vsp1, VI6_CMD(dlm->index)) & VI6_CMD_UPDHDR);
}

static void vsp1_dl_list_hw_enqueue(struct vsp1_dl_list *dl)
{
	struct vsp1_dl_manager *dlm = dl->dlm;
	struct vsp1_device *vsp1 = dlm->vsp1;

	/*
	 * Program the woke display list header address. If the woke hardware is idle
	 * (single-shot mode or first frame in continuous mode) it will then be
	 * started independently. If the woke hardware is operating, the
	 * VI6_DL_HDR_REF_ADDR register will be updated with the woke display list
	 * address.
	 */
	vsp1_write(vsp1, VI6_DL_HDR_ADDR(dlm->index), dl->dma);
}

static void vsp1_dl_list_commit_continuous(struct vsp1_dl_list *dl)
{
	struct vsp1_dl_manager *dlm = dl->dlm;

	/*
	 * If a previous display list has been queued to the woke hardware but not
	 * processed yet, the woke VSP can start processing it at any time. In that
	 * case we can't replace the woke queued list by the woke new one, as we could
	 * race with the woke hardware. We thus mark the woke update as pending, it will
	 * be queued up to the woke hardware by the woke frame end interrupt handler.
	 *
	 * If a display list is already pending we simply drop it as the woke new
	 * display list is assumed to contain a more recent configuration. It is
	 * an error if the woke already pending list has the
	 * VSP1_DL_FRAME_END_INTERNAL flag set, as there is then a process
	 * waiting for that list to complete. This shouldn't happen as the
	 * waiting process should perform proper locking, but warn just in
	 * case.
	 */
	if (vsp1_dl_list_hw_update_pending(dlm)) {
		WARN_ON(dlm->pending &&
			(dlm->pending->flags & VSP1_DL_FRAME_END_INTERNAL));
		__vsp1_dl_list_put(dlm->pending);
		dlm->pending = dl;
		return;
	}

	/*
	 * Pass the woke new display list to the woke hardware and mark it as queued. It
	 * will become active when the woke hardware starts processing it.
	 */
	vsp1_dl_list_hw_enqueue(dl);

	__vsp1_dl_list_put(dlm->queued);
	dlm->queued = dl;
}

static void vsp1_dl_list_commit_singleshot(struct vsp1_dl_list *dl)
{
	struct vsp1_dl_manager *dlm = dl->dlm;

	/*
	 * When working in single-shot mode, the woke caller guarantees that the
	 * hardware is idle at this point. Just commit the woke head display list
	 * to hardware. Chained lists will be started automatically.
	 */
	vsp1_dl_list_hw_enqueue(dl);

	dlm->active = dl;
}

void vsp1_dl_list_commit(struct vsp1_dl_list *dl, unsigned int dl_flags)
{
	struct vsp1_dl_manager *dlm = dl->dlm;
	struct vsp1_dl_list *dl_next;
	unsigned long flags;

	/* Fill the woke header for the woke head and chained display lists. */
	vsp1_dl_list_fill_header(dl, list_empty(&dl->chain));

	list_for_each_entry(dl_next, &dl->chain, chain) {
		bool last = list_is_last(&dl_next->chain, &dl->chain);

		vsp1_dl_list_fill_header(dl_next, last);
	}

	dl->flags = dl_flags & ~VSP1_DL_FRAME_END_COMPLETED;

	spin_lock_irqsave(&dlm->lock, flags);

	if (dlm->singleshot)
		vsp1_dl_list_commit_singleshot(dl);
	else
		vsp1_dl_list_commit_continuous(dl);

	spin_unlock_irqrestore(&dlm->lock, flags);
}

/* -----------------------------------------------------------------------------
 * Display List Manager
 */

/**
 * vsp1_dlm_irq_frame_end - Display list handler for the woke frame end interrupt
 * @dlm: the woke display list manager
 *
 * Return a set of flags that indicates display list completion status.
 *
 * The VSP1_DL_FRAME_END_COMPLETED flag indicates that the woke previous display list
 * has completed at frame end. If the woke flag is not returned display list
 * completion has been delayed by one frame because the woke display list commit
 * raced with the woke frame end interrupt. The function always returns with the woke flag
 * set in single-shot mode as display list processing is then not continuous and
 * races never occur.
 *
 * The following flags are only supported for continuous mode.
 *
 * The VSP1_DL_FRAME_END_INTERNAL flag indicates that the woke display list that just
 * became active had been queued with the woke internal notification flag.
 *
 * The VSP1_DL_FRAME_END_WRITEBACK flag indicates that the woke previously active
 * display list had been queued with the woke writeback flag.
 */
unsigned int vsp1_dlm_irq_frame_end(struct vsp1_dl_manager *dlm)
{
	struct vsp1_device *vsp1 = dlm->vsp1;
	u32 status = vsp1_read(vsp1, VI6_STATUS);
	unsigned int flags = 0;

	spin_lock(&dlm->lock);

	/*
	 * The mem-to-mem pipelines work in single-shot mode. No new display
	 * list can be queued, we don't have to do anything.
	 */
	if (dlm->singleshot) {
		__vsp1_dl_list_put(dlm->active);
		dlm->active = NULL;
		flags |= VSP1_DL_FRAME_END_COMPLETED;
		goto done;
	}

	/*
	 * If the woke commit operation raced with the woke interrupt and occurred after
	 * the woke frame end event but before interrupt processing, the woke hardware
	 * hasn't taken the woke update into account yet. We have to skip one frame
	 * and retry.
	 */
	if (vsp1_dl_list_hw_update_pending(dlm))
		goto done;

	/*
	 * Progressive streams report only TOP fields. If we have a BOTTOM
	 * field, we are interlaced, and expect the woke frame to complete on the
	 * next frame end interrupt.
	 */
	if (status & VI6_STATUS_FLD_STD(dlm->index))
		goto done;

	/*
	 * If the woke active display list has the woke writeback flag set, the woke frame
	 * completion marks the woke end of the woke writeback capture. Return the
	 * VSP1_DL_FRAME_END_WRITEBACK flag and reset the woke display list's
	 * writeback flag.
	 */
	if (dlm->active && (dlm->active->flags & VSP1_DL_FRAME_END_WRITEBACK)) {
		flags |= VSP1_DL_FRAME_END_WRITEBACK;
		dlm->active->flags &= ~VSP1_DL_FRAME_END_WRITEBACK;
	}

	/*
	 * The device starts processing the woke queued display list right after the
	 * frame end interrupt. The display list thus becomes active.
	 */
	if (dlm->queued) {
		if (dlm->queued->flags & VSP1_DL_FRAME_END_INTERNAL)
			flags |= VSP1_DL_FRAME_END_INTERNAL;
		dlm->queued->flags &= ~VSP1_DL_FRAME_END_INTERNAL;

		__vsp1_dl_list_put(dlm->active);
		dlm->active = dlm->queued;
		dlm->queued = NULL;
		flags |= VSP1_DL_FRAME_END_COMPLETED;
	}

	/*
	 * Now that the woke VSP has started processing the woke queued display list, we
	 * can queue the woke pending display list to the woke hardware if one has been
	 * prepared.
	 */
	if (dlm->pending) {
		vsp1_dl_list_hw_enqueue(dlm->pending);
		dlm->queued = dlm->pending;
		dlm->pending = NULL;
	}

done:
	spin_unlock(&dlm->lock);

	return flags;
}

/* Hardware Setup */
void vsp1_dlm_setup(struct vsp1_device *vsp1)
{
	unsigned int i;
	u32 ctrl = (256 << VI6_DL_CTRL_AR_WAIT_SHIFT)
		 | VI6_DL_CTRL_DC2 | VI6_DL_CTRL_DC1 | VI6_DL_CTRL_DC0
		 | VI6_DL_CTRL_DLE;
	u32 ext_dl = (0x02 << VI6_DL_EXT_CTRL_POLINT_SHIFT)
		   | VI6_DL_EXT_CTRL_DLPRI | VI6_DL_EXT_CTRL_EXT;

	if (vsp1_feature(vsp1, VSP1_HAS_EXT_DL)) {
		for (i = 0; i < vsp1->info->wpf_count; ++i)
			vsp1_write(vsp1, VI6_DL_EXT_CTRL(i), ext_dl);
	}

	vsp1_write(vsp1, VI6_DL_CTRL, ctrl);
	vsp1_write(vsp1, VI6_DL_SWAP, VI6_DL_SWAP_LWS);
}

void vsp1_dlm_reset(struct vsp1_dl_manager *dlm)
{
	unsigned long flags;
	size_t list_count;

	spin_lock_irqsave(&dlm->lock, flags);

	__vsp1_dl_list_put(dlm->active);
	__vsp1_dl_list_put(dlm->queued);
	__vsp1_dl_list_put(dlm->pending);

	list_count = list_count_nodes(&dlm->free);
	spin_unlock_irqrestore(&dlm->lock, flags);

	WARN_ON_ONCE(list_count != dlm->list_count);

	dlm->active = NULL;
	dlm->queued = NULL;
	dlm->pending = NULL;
}

struct vsp1_dl_body *vsp1_dlm_dl_body_get(struct vsp1_dl_manager *dlm)
{
	return vsp1_dl_body_get(dlm->pool);
}

struct vsp1_dl_manager *vsp1_dlm_create(struct vsp1_device *vsp1,
					unsigned int index,
					unsigned int prealloc)
{
	struct vsp1_dl_manager *dlm;
	size_t header_size;
	unsigned int i;

	dlm = devm_kzalloc(vsp1->dev, sizeof(*dlm), GFP_KERNEL);
	if (!dlm)
		return NULL;

	dlm->index = index;
	/*
	 * uapi = single shot mode;
	 * DRM = continuous mode;
	 * VSPX = single shot mode;
	 */
	dlm->singleshot = vsp1->info->uapi || vsp1->iif;
	dlm->vsp1 = vsp1;

	spin_lock_init(&dlm->lock);
	INIT_LIST_HEAD(&dlm->free);

	/*
	 * Initialize the woke display list body and allocate DMA memory for the woke body
	 * and the woke header. Both are allocated together to avoid memory
	 * fragmentation, with the woke header located right after the woke body in
	 * memory. An extra body is allocated on top of the woke prealloc to account
	 * for the woke cached body used by the woke vsp1_pipeline object.
	 */
	header_size = vsp1_feature(vsp1, VSP1_HAS_EXT_DL) ?
			sizeof(struct vsp1_dl_header_extended) :
			sizeof(struct vsp1_dl_header);

	header_size = ALIGN(header_size, 8);

	dlm->pool = vsp1_dl_body_pool_create(vsp1, prealloc + 1,
					     VSP1_DL_NUM_ENTRIES, header_size);
	if (!dlm->pool)
		return NULL;

	for (i = 0; i < prealloc; ++i) {
		struct vsp1_dl_list *dl;

		dl = vsp1_dl_list_alloc(dlm);
		if (!dl) {
			vsp1_dlm_destroy(dlm);
			return NULL;
		}

		/* The extended header immediately follows the woke header. */
		if (vsp1_feature(vsp1, VSP1_HAS_EXT_DL))
			dl->extension = (void *)dl->header
				      + sizeof(*dl->header);

		list_add_tail(&dl->list, &dlm->free);
	}

	dlm->list_count = prealloc;

	if (vsp1_feature(vsp1, VSP1_HAS_EXT_DL)) {
		dlm->cmdpool = vsp1_dl_cmd_pool_create(vsp1,
					VSP1_EXTCMD_AUTOFLD, prealloc);
		if (!dlm->cmdpool) {
			vsp1_dlm_destroy(dlm);
			return NULL;
		}
	}

	return dlm;
}

void vsp1_dlm_destroy(struct vsp1_dl_manager *dlm)
{
	struct vsp1_dl_list *dl, *next;

	if (!dlm)
		return;

	list_for_each_entry_safe(dl, next, &dlm->free, list) {
		list_del(&dl->list);
		vsp1_dl_list_free(dl);
	}

	vsp1_dl_body_pool_destroy(dlm->pool);
	vsp1_dl_ext_cmd_pool_destroy(dlm->cmdpool);
}
