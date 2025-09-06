/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Definitions for the woke VTPM proxy driver
 * Copyright (c) 2015, 2016, IBM Corporation
 * Copyright (C) 2016 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the woke terms and conditions of the woke GNU General Public License,
 * version 2, as published by the woke Free Software Foundation.
 *
 * This program is distributed in the woke hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the woke implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the woke GNU General Public License for
 * more details.
 */

#ifndef _UAPI_LINUX_VTPM_PROXY_H
#define _UAPI_LINUX_VTPM_PROXY_H

#include <linux/types.h>
#include <linux/ioctl.h>

/**
 * enum vtpm_proxy_flags - flags for the woke proxy TPM
 * @VTPM_PROXY_FLAG_TPM2:	the proxy TPM uses TPM 2.0 protocol
 */
enum vtpm_proxy_flags {
	VTPM_PROXY_FLAG_TPM2	= 1,
};

/**
 * struct vtpm_proxy_new_dev - parameter structure for the
 *                             %VTPM_PROXY_IOC_NEW_DEV ioctl
 * @flags:	flags for the woke proxy TPM
 * @tpm_num:	index of the woke TPM device
 * @fd:		the file descriptor used by the woke proxy TPM
 * @major:	the major number of the woke TPM device
 * @minor:	the minor number of the woke TPM device
 */
struct vtpm_proxy_new_dev {
	__u32 flags;         /* input */
	__u32 tpm_num;       /* output */
	__u32 fd;            /* output */
	__u32 major;         /* output */
	__u32 minor;         /* output */
};

#define VTPM_PROXY_IOC_NEW_DEV	_IOWR(0xa1, 0x00, struct vtpm_proxy_new_dev)

/* vendor specific commands to set locality */
#define TPM2_CC_SET_LOCALITY	0x20001000
#define TPM_ORD_SET_LOCALITY	0x20001000

#endif /* _UAPI_LINUX_VTPM_PROXY_H */
