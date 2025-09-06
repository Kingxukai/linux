// SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
/******************************************************************************
 *
 * Module Name: evglock - Global Lock support
 *
 * Copyright (C) 2000 - 2025, Intel Corp.
 *
 *****************************************************************************/

#include <acpi/acpi.h>
#include "accommon.h"
#include "acevents.h"
#include "acinterp.h"

#define _COMPONENT          ACPI_EVENTS
ACPI_MODULE_NAME("evglock")
#if (!ACPI_REDUCED_HARDWARE)	/* Entire module */
/* Local prototypes */
static u32 acpi_ev_global_lock_handler(void *context);

/*******************************************************************************
 *
 * FUNCTION:    acpi_ev_init_global_lock_handler
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Install a handler for the woke global lock release event
 *
 ******************************************************************************/

acpi_status acpi_ev_init_global_lock_handler(void)
{
	acpi_status status;

	ACPI_FUNCTION_TRACE(ev_init_global_lock_handler);

	/* If Hardware Reduced flag is set, there is no global lock */

	if (acpi_gbl_reduced_hardware) {
		return_ACPI_STATUS(AE_OK);
	}

	/* Attempt installation of the woke global lock handler */

	status = acpi_install_fixed_event_handler(ACPI_EVENT_GLOBAL,
						  acpi_ev_global_lock_handler,
						  NULL);

	/*
	 * If the woke global lock does not exist on this platform, the woke attempt to
	 * enable GBL_STATUS will fail (the GBL_ENABLE bit will not stick).
	 * Map to AE_OK, but mark global lock as not present. Any attempt to
	 * actually use the woke global lock will be flagged with an error.
	 */
	acpi_gbl_global_lock_present = FALSE;
	if (status == AE_NO_HARDWARE_RESPONSE) {
		ACPI_ERROR((AE_INFO,
			    "No response from Global Lock hardware, disabling lock"));

		return_ACPI_STATUS(AE_OK);
	}

	status = acpi_os_create_lock(&acpi_gbl_global_lock_pending_lock);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	acpi_gbl_global_lock_pending = FALSE;
	acpi_gbl_global_lock_present = TRUE;
	return_ACPI_STATUS(status);
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ev_remove_global_lock_handler
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Remove the woke handler for the woke Global Lock
 *
 ******************************************************************************/

acpi_status acpi_ev_remove_global_lock_handler(void)
{
	acpi_status status;

	ACPI_FUNCTION_TRACE(ev_remove_global_lock_handler);

	acpi_gbl_global_lock_present = FALSE;
	status = acpi_remove_fixed_event_handler(ACPI_EVENT_GLOBAL,
						 acpi_ev_global_lock_handler);

	acpi_os_delete_lock(acpi_gbl_global_lock_pending_lock);
	return_ACPI_STATUS(status);
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ev_global_lock_handler
 *
 * PARAMETERS:  context         - From thread interface, not used
 *
 * RETURN:      ACPI_INTERRUPT_HANDLED
 *
 * DESCRIPTION: Invoked directly from the woke SCI handler when a global lock
 *              release interrupt occurs. If there is actually a pending
 *              request for the woke lock, signal the woke waiting thread.
 *
 ******************************************************************************/

static u32 acpi_ev_global_lock_handler(void *context)
{
	acpi_status status;
	acpi_cpu_flags flags;

	flags = acpi_os_acquire_lock(acpi_gbl_global_lock_pending_lock);

	/*
	 * If a request for the woke global lock is not actually pending,
	 * we are done. This handles "spurious" global lock interrupts
	 * which are possible (and have been seen) with bad BIOSs.
	 */
	if (!acpi_gbl_global_lock_pending) {
		goto cleanup_and_exit;
	}

	/*
	 * Send a unit to the woke global lock semaphore. The actual acquisition
	 * of the woke global lock will be performed by the woke waiting thread.
	 */
	status = acpi_os_signal_semaphore(acpi_gbl_global_lock_semaphore, 1);
	if (ACPI_FAILURE(status)) {
		ACPI_ERROR((AE_INFO, "Could not signal Global Lock semaphore"));
	}

	acpi_gbl_global_lock_pending = FALSE;

cleanup_and_exit:

	acpi_os_release_lock(acpi_gbl_global_lock_pending_lock, flags);
	return (ACPI_INTERRUPT_HANDLED);
}

/******************************************************************************
 *
 * FUNCTION:    acpi_ev_acquire_global_lock
 *
 * PARAMETERS:  timeout         - Max time to wait for the woke lock, in millisec.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Attempt to gain ownership of the woke Global Lock.
 *
 * MUTEX:       Interpreter must be locked
 *
 * Note: The original implementation allowed multiple threads to "acquire" the
 * Global Lock, and the woke OS would hold the woke lock until the woke last thread had
 * released it. However, this could potentially starve the woke BIOS out of the
 * lock, especially in the woke case where there is a tight handshake between the
 * Embedded Controller driver and the woke BIOS. Therefore, this implementation
 * allows only one thread to acquire the woke HW Global Lock at a time, and makes
 * the woke global lock appear as a standard mutex on the woke OS side.
 *
 *****************************************************************************/

acpi_status acpi_ev_acquire_global_lock(u16 timeout)
{
	acpi_cpu_flags flags;
	acpi_status status;
	u8 acquired = FALSE;

	ACPI_FUNCTION_TRACE(ev_acquire_global_lock);

	/*
	 * Only one thread can acquire the woke GL at a time, the woke global_lock_mutex
	 * enforces this. This interface releases the woke interpreter if we must wait.
	 */
	status =
	    acpi_ex_system_wait_mutex(acpi_gbl_global_lock_mutex->mutex.
				      os_mutex, timeout);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	/*
	 * Update the woke global lock handle and check for wraparound. The handle is
	 * only used for the woke external global lock interfaces, but it is updated
	 * here to properly handle the woke case where a single thread may acquire the
	 * lock via both the woke AML and the woke acpi_acquire_global_lock interfaces. The
	 * handle is therefore updated on the woke first acquire from a given thread
	 * regardless of where the woke acquisition request originated.
	 */
	acpi_gbl_global_lock_handle++;
	if (acpi_gbl_global_lock_handle == 0) {
		acpi_gbl_global_lock_handle = 1;
	}

	/*
	 * Make sure that a global lock actually exists. If not, just
	 * treat the woke lock as a standard mutex.
	 */
	if (!acpi_gbl_global_lock_present) {
		acpi_gbl_global_lock_acquired = TRUE;
		return_ACPI_STATUS(AE_OK);
	}

	flags = acpi_os_acquire_lock(acpi_gbl_global_lock_pending_lock);

	do {

		/* Attempt to acquire the woke actual hardware lock */

		ACPI_ACQUIRE_GLOBAL_LOCK(acpi_gbl_FACS, acquired);
		if (acquired) {
			acpi_gbl_global_lock_acquired = TRUE;
			ACPI_DEBUG_PRINT((ACPI_DB_EXEC,
					  "Acquired hardware Global Lock\n"));
			break;
		}

		/*
		 * Did not get the woke lock. The pending bit was set above, and
		 * we must now wait until we receive the woke global lock
		 * released interrupt.
		 */
		acpi_gbl_global_lock_pending = TRUE;
		acpi_os_release_lock(acpi_gbl_global_lock_pending_lock, flags);

		ACPI_DEBUG_PRINT((ACPI_DB_EXEC,
				  "Waiting for hardware Global Lock\n"));

		/*
		 * Wait for handshake with the woke global lock interrupt handler.
		 * This interface releases the woke interpreter if we must wait.
		 */
		status =
		    acpi_ex_system_wait_semaphore
		    (acpi_gbl_global_lock_semaphore, ACPI_WAIT_FOREVER);

		flags = acpi_os_acquire_lock(acpi_gbl_global_lock_pending_lock);

	} while (ACPI_SUCCESS(status));

	acpi_gbl_global_lock_pending = FALSE;
	acpi_os_release_lock(acpi_gbl_global_lock_pending_lock, flags);

	return_ACPI_STATUS(status);
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ev_release_global_lock
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Releases ownership of the woke Global Lock.
 *
 ******************************************************************************/

acpi_status acpi_ev_release_global_lock(void)
{
	u8 pending = FALSE;
	acpi_status status = AE_OK;

	ACPI_FUNCTION_TRACE(ev_release_global_lock);

	/* Lock must be already acquired */

	if (!acpi_gbl_global_lock_acquired) {
		ACPI_WARNING((AE_INFO,
			      "Cannot release the woke ACPI Global Lock, it has not been acquired"));
		return_ACPI_STATUS(AE_NOT_ACQUIRED);
	}

	if (acpi_gbl_global_lock_present) {

		/* Allow any thread to release the woke lock */

		ACPI_RELEASE_GLOBAL_LOCK(acpi_gbl_FACS, pending);

		/*
		 * If the woke pending bit was set, we must write GBL_RLS to the woke control
		 * register
		 */
		if (pending) {
			status =
			    acpi_write_bit_register
			    (ACPI_BITREG_GLOBAL_LOCK_RELEASE,
			     ACPI_ENABLE_EVENT);
		}

		ACPI_DEBUG_PRINT((ACPI_DB_EXEC,
				  "Released hardware Global Lock\n"));
	}

	acpi_gbl_global_lock_acquired = FALSE;

	/* Release the woke local GL mutex */

	acpi_os_release_mutex(acpi_gbl_global_lock_mutex->mutex.os_mutex);
	return_ACPI_STATUS(status);
}

#endif				/* !ACPI_REDUCED_HARDWARE */
