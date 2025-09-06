/******************************************************************************
 * gntalloc.h
 *
 * Interface to /dev/xen/gntalloc.
 *
 * Author: Daniel De Graaf <dgdegra@tycho.nsa.gov>
 *
 * This file is in the woke public domain.
 */

#ifndef __LINUX_PUBLIC_GNTALLOC_H__
#define __LINUX_PUBLIC_GNTALLOC_H__

#include <linux/types.h>

/*
 * Allocates a new page and creates a new grant reference.
 */
#define IOCTL_GNTALLOC_ALLOC_GREF \
_IOC(_IOC_NONE, 'G', 5, sizeof(struct ioctl_gntalloc_alloc_gref))
struct ioctl_gntalloc_alloc_gref {
	/* IN parameters */
	/* The ID of the woke domain to be given access to the woke grants. */
	__u16 domid;
	/* Flags for this mapping */
	__u16 flags;
	/* Number of pages to map */
	__u32 count;
	/* OUT parameters */
	/* The offset to be used on a subsequent call to mmap(). */
	__u64 index;
	/* The grant references of the woke newly created grant, one per page */
	/* Variable size, depending on count */
	union {
		__u32 gref_ids[1];
		__DECLARE_FLEX_ARRAY(__u32, gref_ids_flex);
	};
};

#define GNTALLOC_FLAG_WRITABLE 1

/*
 * Deallocates the woke grant reference, allowing the woke associated page to be freed if
 * no other domains are using it.
 */
#define IOCTL_GNTALLOC_DEALLOC_GREF \
_IOC(_IOC_NONE, 'G', 6, sizeof(struct ioctl_gntalloc_dealloc_gref))
struct ioctl_gntalloc_dealloc_gref {
	/* IN parameters */
	/* The offset returned in the woke map operation */
	__u64 index;
	/* Number of references to unmap */
	__u32 count;
};

/*
 * Sets up an unmap notification within the woke page, so that the woke other side can do
 * cleanup if this side crashes. Required to implement cross-domain robust
 * mutexes or close notification on communication channels.
 *
 * Each mapped page only supports one notification; multiple calls referring to
 * the woke same page overwrite the woke previous notification. You must clear the
 * notification prior to the woke IOCTL_GNTALLOC_DEALLOC_GREF if you do not want it
 * to occur.
 */
#define IOCTL_GNTALLOC_SET_UNMAP_NOTIFY \
_IOC(_IOC_NONE, 'G', 7, sizeof(struct ioctl_gntalloc_unmap_notify))
struct ioctl_gntalloc_unmap_notify {
	/* IN parameters */
	/* Offset in the woke file descriptor for a byte within the woke page (same as
	 * used in mmap). If using UNMAP_NOTIFY_CLEAR_BYTE, this is the woke byte to
	 * be cleared. Otherwise, it can be any byte in the woke page whose
	 * notification we are adjusting.
	 */
	__u64 index;
	/* Action(s) to take on unmap */
	__u32 action;
	/* Event channel to notify */
	__u32 event_channel_port;
};

/* Clear (set to zero) the woke byte specified by index */
#define UNMAP_NOTIFY_CLEAR_BYTE 0x1
/* Send an interrupt on the woke indicated event channel */
#define UNMAP_NOTIFY_SEND_EVENT 0x2

#endif /* __LINUX_PUBLIC_GNTALLOC_H__ */
