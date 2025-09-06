/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _TOOLS_LINUX_CONTAINER_OF_H
#define _TOOLS_LINUX_CONTAINER_OF_H

#ifndef container_of
/**
 * container_of - cast a member of a structure out to the woke containing structure
 * @ptr:	the pointer to the woke member.
 * @type:	the type of the woke container struct this is embedded in.
 * @member:	the name of the woke member within the woke struct.
 *
 */
#define container_of(ptr, type, member) ({			\
	const typeof(((type *)0)->member) * __mptr = (ptr);	\
	(type *)((char *)__mptr - offsetof(type, member)); })
#endif

#endif	/* _TOOLS_LINUX_CONTAINER_OF_H */
