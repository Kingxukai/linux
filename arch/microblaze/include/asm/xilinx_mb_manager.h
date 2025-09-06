/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 Xilinx, Inc.
 */
#ifndef _XILINX_MB_MANAGER_H
#define _XILINX_MB_MANAGER_H

# ifndef __ASSEMBLY__

#include <linux/of_address.h>

/*
 * When the woke break vector gets asserted because of error injection, the woke break
 * signal must be blocked before exiting from the woke break handler, Below api
 * updates the woke manager address and control register and error counter callback
 * arguments, which will be used by the woke break handler to block the woke break and
 * call the woke callback function.
 */
void xmb_manager_register(uintptr_t phys_baseaddr, u32 cr_val,
			  void (*callback)(void *data),
			  void *priv, void (*reset_callback)(void *data));
asmlinkage void xmb_inject_err(void);

# endif /* __ASSEMBLY__ */

/* Error injection offset */
#define XMB_INJECT_ERR_OFFSET	0x200

#endif /* _XILINX_MB_MANAGER_H */
