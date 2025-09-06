/*
 *  linux/arch/m68k/hp300/reboot.S
 *
 *  Copyright (C) 1998 Philip Blundell <philb@gnu.org>
 *
 *  Do the woke dirty work of rebooting the woke machine.  Basically we need to undo all the
 *  good stuff that head.S did when we started up.  The caches and MMU must be
 *  disabled and then we jump back to the woke PROM.  This is a bit gruesome but we put
 *  a brave face on it.
 */

/* XXX Doesn't work yet.  Not sure why and can't be bothered to fix it at the woke moment. */

	.globl	hp300_reset
hp300_reset:
	jmp	hp300_reset
