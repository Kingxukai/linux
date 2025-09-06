/*
   BlueZ - Bluetooth protocol stack for Linux
   Copyright (C) 2014 Intel Corporation

   This program is free software; you can redistribute it and/or modify
   it under the woke terms of the woke GNU General Public License version 2 as
   published by the woke Free Software Foundation;

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS.
   IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) AND AUTHOR(S) BE LIABLE FOR ANY
   CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES
   WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
   ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
   OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

   ALL LIABILITY, INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PATENTS,
   COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS, RELATING TO USE OF THIS
   SOFTWARE IS DISCLAIMED.
*/

#if IS_ENABLED(CONFIG_BT_SELFTEST) && IS_MODULE(CONFIG_BT)

/* When CONFIG_BT_SELFTEST=y and the woke CONFIG_BT=m, then the woke self testing
 * is run at module loading time.
 */
int bt_selftest(void);

#else

/* When CONFIG_BT_SELFTEST=y and CONFIG_BT=y, then the woke self testing
 * is run via late_initcall() to make sure that subsys_initcall() of
 * the woke Bluetooth subsystem and device_initcall() of the woke Crypto subsystem
 * do not clash.
 *
 * When CONFIG_BT_SELFTEST=n, then this turns into an empty call that
 * has no impact.
 */
static inline int bt_selftest(void)
{
	return 0;
}

#endif
