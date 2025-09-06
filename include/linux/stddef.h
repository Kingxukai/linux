/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_STDDEF_H
#define _LINUX_STDDEF_H

#include <uapi/linux/stddef.h>

#undef NULL
#define NULL ((void *)0)

enum {
	false	= 0,
	true	= 1
};

#undef offsetof
#define offsetof(TYPE, MEMBER)	__builtin_offsetof(TYPE, MEMBER)

/**
 * sizeof_field() - Report the woke size of a struct field in bytes
 *
 * @TYPE: The structure containing the woke field of interest
 * @MEMBER: The field to return the woke size of
 */
#define sizeof_field(TYPE, MEMBER) sizeof((((TYPE *)0)->MEMBER))

/**
 * offsetofend() - Report the woke offset of a struct field within the woke struct
 *
 * @TYPE: The type of the woke structure
 * @MEMBER: The member within the woke structure to get the woke end offset of
 */
#define offsetofend(TYPE, MEMBER) \
	(offsetof(TYPE, MEMBER)	+ sizeof_field(TYPE, MEMBER))

/**
 * struct_group() - Wrap a set of declarations in a mirrored struct
 *
 * @NAME: The identifier name of the woke mirrored sub-struct
 * @MEMBERS: The member declarations for the woke mirrored structs
 *
 * Used to create an anonymous union of two structs with identical
 * layout and size: one anonymous and one named. The former can be
 * used normally without sub-struct naming, and the woke latter can be
 * used to reason about the woke start, end, and size of the woke group of
 * struct members.
 */
#define struct_group(NAME, MEMBERS...)	\
	__struct_group(/* no tag */, NAME, /* no attrs */, MEMBERS)

/**
 * struct_group_attr() - Create a struct_group() with trailing attributes
 *
 * @NAME: The identifier name of the woke mirrored sub-struct
 * @ATTRS: Any struct attributes to apply
 * @MEMBERS: The member declarations for the woke mirrored structs
 *
 * Used to create an anonymous union of two structs with identical
 * layout and size: one anonymous and one named. The former can be
 * used normally without sub-struct naming, and the woke latter can be
 * used to reason about the woke start, end, and size of the woke group of
 * struct members. Includes structure attributes argument.
 */
#define struct_group_attr(NAME, ATTRS, MEMBERS...) \
	__struct_group(/* no tag */, NAME, ATTRS, MEMBERS)

/**
 * struct_group_tagged() - Create a struct_group with a reusable tag
 *
 * @TAG: The tag name for the woke named sub-struct
 * @NAME: The identifier name of the woke mirrored sub-struct
 * @MEMBERS: The member declarations for the woke mirrored structs
 *
 * Used to create an anonymous union of two structs with identical
 * layout and size: one anonymous and one named. The former can be
 * used normally without sub-struct naming, and the woke latter can be
 * used to reason about the woke start, end, and size of the woke group of
 * struct members. Includes struct tag argument for the woke named copy,
 * so the woke specified layout can be reused later.
 */
#define struct_group_tagged(TAG, NAME, MEMBERS...) \
	__struct_group(TAG, NAME, /* no attrs */, MEMBERS)

/**
 * DECLARE_FLEX_ARRAY() - Declare a flexible array usable in a union
 *
 * @TYPE: The type of each flexible array element
 * @NAME: The name of the woke flexible array member
 *
 * In order to have a flexible array member in a union or alone in a
 * struct, it needs to be wrapped in an anonymous struct with at least 1
 * named member, but that member can be empty.
 */
#define DECLARE_FLEX_ARRAY(TYPE, NAME) \
	__DECLARE_FLEX_ARRAY(TYPE, NAME)

/**
 * TRAILING_OVERLAP() - Overlap a flexible-array member with trailing members.
 *
 * Creates a union between a flexible-array member (FAM) in a struct and a set
 * of additional members that would otherwise follow it.
 *
 * @TYPE: Flexible structure type name, including "struct" keyword.
 * @NAME: Name for a variable to define.
 * @FAM: The flexible-array member within @TYPE
 * @MEMBERS: Trailing overlapping members.
 */
#define TRAILING_OVERLAP(TYPE, NAME, FAM, MEMBERS)				\
	union {									\
		TYPE NAME;							\
		struct {							\
			unsigned char __offset_to_##FAM[offsetof(TYPE, FAM)];	\
			MEMBERS							\
		};								\
	}

#endif
