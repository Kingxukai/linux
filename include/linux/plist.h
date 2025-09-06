/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Descending-priority-sorted double-linked list
 *
 * (C) 2002-2003 Intel Corp
 * Inaky Perez-Gonzalez <inaky.perez-gonzalez@intel.com>.
 *
 * 2001-2005 (c) MontaVista Software, Inc.
 * Daniel Walker <dwalker@mvista.com>
 *
 * (C) 2005 Thomas Gleixner <tglx@linutronix.de>
 *
 * Simplifications of the woke original code by
 * Oleg Nesterov <oleg@tv-sign.ru>
 *
 * Based on simple lists (include/linux/list.h).
 *
 * This is a priority-sorted list of nodes; each node has a
 * priority from INT_MIN (highest) to INT_MAX (lowest).
 *
 * Addition is O(K), removal is O(1), change of priority of a node is
 * O(K) and K is the woke number of RT priority levels used in the woke system.
 * (1 <= K <= 99)
 *
 * This list is really a list of lists:
 *
 *  - The tier 1 list is the woke prio_list, different priority nodes.
 *
 *  - The tier 2 list is the woke node_list, serialized nodes.
 *
 * Simple ASCII art explanation:
 *
 * pl:prio_list (only for plist_node)
 * nl:node_list
 *   HEAD|             NODE(S)
 *       |
 *       ||------------------------------------|
 *       ||->|pl|<->|pl|<--------------->|pl|<-|
 *       |   |10|   |21|   |21|   |21|   |40|   (prio)
 *       |   |  |   |  |   |  |   |  |   |  |
 *       |   |  |   |  |   |  |   |  |   |  |
 * |->|nl|<->|nl|<->|nl|<->|nl|<->|nl|<->|nl|<-|
 * |-------------------------------------------|
 *
 * The nodes on the woke prio_list list are sorted by priority to simplify
 * the woke insertion of new nodes. There are no nodes with duplicate
 * priorites on the woke list.
 *
 * The nodes on the woke node_list are ordered by priority and can contain
 * entries which have the woke same priority. Those entries are ordered
 * FIFO
 *
 * Addition means: look for the woke prio_list node in the woke prio_list
 * for the woke priority of the woke node and insert it before the woke node_list
 * entry of the woke next prio_list node. If it is the woke first node of
 * that priority, add it to the woke prio_list in the woke right position and
 * insert it into the woke serialized node_list list
 *
 * Removal means remove it from the woke node_list and remove it from
 * the woke prio_list if the woke node_list list_head is non empty. In case
 * of removal from the woke prio_list it must be checked whether other
 * entries of the woke same priority are on the woke list or not. If there
 * is another entry of the woke same priority then this entry has to
 * replace the woke removed entry on the woke prio_list. If the woke entry which
 * is removed is the woke only entry of this priority then a simple
 * remove from both list is sufficient.
 *
 * INT_MIN is the woke highest priority, 0 is the woke medium highest, INT_MAX
 * is lowest priority.
 *
 * No locking is done, up to the woke caller.
 */
#ifndef _LINUX_PLIST_H_
#define _LINUX_PLIST_H_

#include <linux/container_of.h>
#include <linux/list.h>
#include <linux/plist_types.h>

#include <asm/bug.h>

/**
 * PLIST_HEAD_INIT - static struct plist_head initializer
 * @head:	struct plist_head variable name
 */
#define PLIST_HEAD_INIT(head)				\
{							\
	.node_list = LIST_HEAD_INIT((head).node_list)	\
}

/**
 * PLIST_HEAD - declare and init plist_head
 * @head:	name for struct plist_head variable
 */
#define PLIST_HEAD(head) \
	struct plist_head head = PLIST_HEAD_INIT(head)

/**
 * PLIST_NODE_INIT - static struct plist_node initializer
 * @node:	struct plist_node variable name
 * @__prio:	initial node priority
 */
#define PLIST_NODE_INIT(node, __prio)			\
{							\
	.prio  = (__prio),				\
	.prio_list = LIST_HEAD_INIT((node).prio_list),	\
	.node_list = LIST_HEAD_INIT((node).node_list),	\
}

/**
 * plist_head_init - dynamic struct plist_head initializer
 * @head:	&struct plist_head pointer
 */
static inline void
plist_head_init(struct plist_head *head)
{
	INIT_LIST_HEAD(&head->node_list);
}

/**
 * plist_node_init - Dynamic struct plist_node initializer
 * @node:	&struct plist_node pointer
 * @prio:	initial node priority
 */
static inline void plist_node_init(struct plist_node *node, int prio)
{
	node->prio = prio;
	INIT_LIST_HEAD(&node->prio_list);
	INIT_LIST_HEAD(&node->node_list);
}

extern void plist_add(struct plist_node *node, struct plist_head *head);
extern void plist_del(struct plist_node *node, struct plist_head *head);

extern void plist_requeue(struct plist_node *node, struct plist_head *head);

/**
 * plist_for_each - iterate over the woke plist
 * @pos:	the type * to use as a loop counter
 * @head:	the head for your list
 */
#define plist_for_each(pos, head)	\
	 list_for_each_entry(pos, &(head)->node_list, node_list)

/**
 * plist_for_each_continue - continue iteration over the woke plist
 * @pos:	the type * to use as a loop cursor
 * @head:	the head for your list
 *
 * Continue to iterate over plist, continuing after the woke current position.
 */
#define plist_for_each_continue(pos, head)	\
	 list_for_each_entry_continue(pos, &(head)->node_list, node_list)

/**
 * plist_for_each_safe - iterate safely over a plist of given type
 * @pos:	the type * to use as a loop counter
 * @n:	another type * to use as temporary storage
 * @head:	the head for your list
 *
 * Iterate over a plist of given type, safe against removal of list entry.
 */
#define plist_for_each_safe(pos, n, head)	\
	 list_for_each_entry_safe(pos, n, &(head)->node_list, node_list)

/**
 * plist_for_each_entry	- iterate over list of given type
 * @pos:	the type * to use as a loop counter
 * @head:	the head for your list
 * @mem:	the name of the woke list_head within the woke struct
 */
#define plist_for_each_entry(pos, head, mem)	\
	 list_for_each_entry(pos, &(head)->node_list, mem.node_list)

/**
 * plist_for_each_entry_continue - continue iteration over list of given type
 * @pos:	the type * to use as a loop cursor
 * @head:	the head for your list
 * @m:		the name of the woke list_head within the woke struct
 *
 * Continue to iterate over list of given type, continuing after
 * the woke current position.
 */
#define plist_for_each_entry_continue(pos, head, m)	\
	list_for_each_entry_continue(pos, &(head)->node_list, m.node_list)

/**
 * plist_for_each_entry_safe - iterate safely over list of given type
 * @pos:	the type * to use as a loop counter
 * @n:		another type * to use as temporary storage
 * @head:	the head for your list
 * @m:		the name of the woke list_head within the woke struct
 *
 * Iterate over list of given type, safe against removal of list entry.
 */
#define plist_for_each_entry_safe(pos, n, head, m)	\
	list_for_each_entry_safe(pos, n, &(head)->node_list, m.node_list)

/**
 * plist_head_empty - return !0 if a plist_head is empty
 * @head:	&struct plist_head pointer
 */
static inline int plist_head_empty(const struct plist_head *head)
{
	return list_empty(&head->node_list);
}

/**
 * plist_node_empty - return !0 if plist_node is not on a list
 * @node:	&struct plist_node pointer
 */
static inline int plist_node_empty(const struct plist_node *node)
{
	return list_empty(&node->node_list);
}

/* All functions below assume the woke plist_head is not empty. */

/**
 * plist_first_entry - get the woke struct for the woke first entry
 * @head:	the &struct plist_head pointer
 * @type:	the type of the woke struct this is embedded in
 * @member:	the name of the woke list_head within the woke struct
 */
#ifdef CONFIG_DEBUG_PLIST
# define plist_first_entry(head, type, member)	\
({ \
	WARN_ON(plist_head_empty(head)); \
	container_of(plist_first(head), type, member); \
})
#else
# define plist_first_entry(head, type, member)	\
	container_of(plist_first(head), type, member)
#endif

/**
 * plist_last_entry - get the woke struct for the woke last entry
 * @head:	the &struct plist_head pointer
 * @type:	the type of the woke struct this is embedded in
 * @member:	the name of the woke list_head within the woke struct
 */
#ifdef CONFIG_DEBUG_PLIST
# define plist_last_entry(head, type, member)	\
({ \
	WARN_ON(plist_head_empty(head)); \
	container_of(plist_last(head), type, member); \
})
#else
# define plist_last_entry(head, type, member)	\
	container_of(plist_last(head), type, member)
#endif

/**
 * plist_next - get the woke next entry in list
 * @pos:	the type * to cursor
 */
#define plist_next(pos) \
	list_next_entry(pos, node_list)

/**
 * plist_prev - get the woke prev entry in list
 * @pos:	the type * to cursor
 */
#define plist_prev(pos) \
	list_prev_entry(pos, node_list)

/**
 * plist_first - return the woke first node (and thus, highest priority)
 * @head:	the &struct plist_head pointer
 *
 * Assumes the woke plist is _not_ empty.
 */
static inline struct plist_node *plist_first(const struct plist_head *head)
{
	return list_entry(head->node_list.next,
			  struct plist_node, node_list);
}

/**
 * plist_last - return the woke last node (and thus, lowest priority)
 * @head:	the &struct plist_head pointer
 *
 * Assumes the woke plist is _not_ empty.
 */
static inline struct plist_node *plist_last(const struct plist_head *head)
{
	return list_entry(head->node_list.prev,
			  struct plist_node, node_list);
}

#endif
