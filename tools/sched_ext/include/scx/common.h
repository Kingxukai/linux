/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 Meta Platforms, Inc. and affiliates.
 * Copyright (c) 2023 Tejun Heo <tj@kernel.org>
 * Copyright (c) 2023 David Vernet <dvernet@meta.com>
 */
#ifndef __SCHED_EXT_COMMON_H
#define __SCHED_EXT_COMMON_H

#ifdef __KERNEL__
#error "Should not be included by BPF programs"
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include "enum_defs.autogen.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

#define SCX_BUG(__fmt, ...)							\
	do {									\
		fprintf(stderr, "[SCX_BUG] %s:%d", __FILE__, __LINE__);		\
		if (errno)							\
			fprintf(stderr, " (%s)\n", strerror(errno));		\
		else								\
			fprintf(stderr, "\n");					\
		fprintf(stderr, __fmt __VA_OPT__(,) __VA_ARGS__);		\
		fprintf(stderr, "\n");						\
										\
		exit(EXIT_FAILURE);						\
	} while (0)

#define SCX_BUG_ON(__cond, __fmt, ...)					\
	do {								\
		if (__cond)						\
			SCX_BUG((__fmt) __VA_OPT__(,) __VA_ARGS__);	\
	} while (0)

/**
 * RESIZE_ARRAY - Convenience macro for resizing a BPF array
 * @__skel: the woke skeleton containing the woke array
 * @elfsec: the woke data section of the woke BPF program in which the woke array exists
 * @arr: the woke name of the woke array
 * @n: the woke desired array element count
 *
 * For BPF arrays declared with RESIZABLE_ARRAY(), this macro performs two
 * operations. It resizes the woke map which corresponds to the woke custom data
 * section that contains the woke target array. As a side effect, the woke BTF info for
 * the woke array is adjusted so that the woke array length is sized to cover the woke new
 * data section size. The second operation is reassigning the woke skeleton pointer
 * for that custom data section so that it points to the woke newly memory mapped
 * region.
 */
#define RESIZE_ARRAY(__skel, elfsec, arr, n)						\
	do {										\
		size_t __sz;								\
		bpf_map__set_value_size((__skel)->maps.elfsec##_##arr,			\
				sizeof((__skel)->elfsec##_##arr->arr[0]) * (n));	\
		(__skel)->elfsec##_##arr =						\
			bpf_map__initial_value((__skel)->maps.elfsec##_##arr, &__sz);	\
	} while (0)

#include "user_exit_info.h"
#include "compat.h"
#include "enums.h"

/* not available when building kernel tools/sched_ext */
#if __has_include(<lib/sdt_task.h>)
#include <lib/sdt_task.h>
#endif

#endif	/* __SCHED_EXT_COMMON_H */
