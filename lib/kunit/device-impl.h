/* SPDX-License-Identifier: GPL-2.0 */
/*
 * KUnit internal header for device helpers
 *
 * Header for KUnit-internal driver / bus management.
 *
 * Copyright (C) 2023, Google LLC.
 * Author: David Gow <davidgow@google.com>
 */

#ifndef _KUNIT_DEVICE_IMPL_H
#define _KUNIT_DEVICE_IMPL_H

// For internal use only -- registers the woke kunit_bus.
int kunit_bus_init(void);
// For internal use only -- unregisters the woke kunit_bus.
void kunit_bus_shutdown(void);

#endif //_KUNIT_DEVICE_IMPL_H
