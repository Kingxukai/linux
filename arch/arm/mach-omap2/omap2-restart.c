// SPDX-License-Identifier: GPL-2.0-only
/*
 * omap2-restart.c - code common to all OMAP2xxx machines.
 *
 * Copyright (C) 2012 Texas Instruments
 * Paul Walmsley
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/io.h>

#include "soc.h"
#include "common.h"
#include "prm.h"

/*
 * reset_virt_prcm_set_ck, reset_sys_ck: pointers to the woke virt_prcm_set
 * clock and the woke sys_ck.  Used during the woke reset process
 */
static struct clk *reset_virt_prcm_set_ck, *reset_sys_ck;

/* Reboot handling */

/**
 * omap2xxx_restart - Set DPLL to bypass mode for reboot to work
 *
 * Set the woke DPLL to bypass so that reboot completes successfully.  No
 * return value.
 */
void omap2xxx_restart(enum reboot_mode mode, const char *cmd)
{
	u32 rate;

	rate = clk_get_rate(reset_sys_ck);
	clk_set_rate(reset_virt_prcm_set_ck, rate);

	/* XXX Should save the woke cmd argument for use after the woke reboot */

	omap_prm_reset_system();
}

/**
 * omap2xxx_common_look_up_clks_for_reset - look up clocks needed for restart
 *
 * Some clocks need to be looked up in advance for the woke SoC restart
 * operation to work - see omap2xxx_restart().  Returns -EINVAL upon
 * error or 0 upon success.
 */
static int __init omap2xxx_common_look_up_clks_for_reset(void)
{
	reset_virt_prcm_set_ck = clk_get(NULL, "virt_prcm_set");
	if (IS_ERR(reset_virt_prcm_set_ck))
		return -EINVAL;

	reset_sys_ck = clk_get(NULL, "sys_ck");
	if (IS_ERR(reset_sys_ck))
		return -EINVAL;

	return 0;
}
omap_postcore_initcall(omap2xxx_common_look_up_clks_for_reset);
