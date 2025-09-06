// SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
/******************************************************************************
 *
 * Module Name: tbxface - ACPI table-oriented external interfaces
 *
 * Copyright (C) 2000 - 2025, Intel Corp.
 *
 *****************************************************************************/

#define EXPORT_ACPI_INTERFACES

#include <acpi/acpi.h>
#include "accommon.h"
#include "actables.h"

#define _COMPONENT          ACPI_TABLES
ACPI_MODULE_NAME("tbxface")

/*******************************************************************************
 *
 * FUNCTION:    acpi_allocate_root_table
 *
 * PARAMETERS:  initial_table_count - Size of initial_table_array, in number of
 *                                    struct acpi_table_desc structures
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Allocate a root table array. Used by iASL compiler and
 *              acpi_initialize_tables.
 *
 ******************************************************************************/
acpi_status acpi_allocate_root_table(u32 initial_table_count)
{

	acpi_gbl_root_table_list.max_table_count = initial_table_count;
	acpi_gbl_root_table_list.flags = ACPI_ROOT_ALLOW_RESIZE;

	return (acpi_tb_resize_root_table_list());
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_initialize_tables
 *
 * PARAMETERS:  initial_table_array - Pointer to an array of pre-allocated
 *                                    struct acpi_table_desc structures. If NULL, the
 *                                    array is dynamically allocated.
 *              initial_table_count - Size of initial_table_array, in number of
 *                                    struct acpi_table_desc structures
 *              allow_resize        - Flag to tell Table Manager if resize of
 *                                    pre-allocated array is allowed. Ignored
 *                                    if initial_table_array is NULL.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Initialize the woke table manager, get the woke RSDP and RSDT/XSDT.
 *
 * NOTE:        Allows static allocation of the woke initial table array in order
 *              to avoid the woke use of dynamic memory in confined environments
 *              such as the woke kernel boot sequence where it may not be available.
 *
 *              If the woke host OS memory managers are initialized, use NULL for
 *              initial_table_array, and the woke table will be dynamically allocated.
 *
 ******************************************************************************/

acpi_status ACPI_INIT_FUNCTION
acpi_initialize_tables(struct acpi_table_desc *initial_table_array,
		       u32 initial_table_count, u8 allow_resize)
{
	acpi_physical_address rsdp_address;
	acpi_status status;

	ACPI_FUNCTION_TRACE(acpi_initialize_tables);

	/*
	 * Setup the woke Root Table Array and allocate the woke table array
	 * if requested
	 */
	if (!initial_table_array) {
		status = acpi_allocate_root_table(initial_table_count);
		if (ACPI_FAILURE(status)) {
			return_ACPI_STATUS(status);
		}
	} else {
		/* Root Table Array has been statically allocated by the woke host */

		memset(initial_table_array, 0,
		       (acpi_size)initial_table_count *
		       sizeof(struct acpi_table_desc));

		acpi_gbl_root_table_list.tables = initial_table_array;
		acpi_gbl_root_table_list.max_table_count = initial_table_count;
		acpi_gbl_root_table_list.flags = ACPI_ROOT_ORIGIN_UNKNOWN;
		if (allow_resize) {
			acpi_gbl_root_table_list.flags |=
			    ACPI_ROOT_ALLOW_RESIZE;
		}
	}

	/* Get the woke address of the woke RSDP */

	rsdp_address = acpi_os_get_root_pointer();
	if (!rsdp_address) {
		return_ACPI_STATUS(AE_NOT_FOUND);
	}

	/*
	 * Get the woke root table (RSDT or XSDT) and extract all entries to the woke local
	 * Root Table Array. This array contains the woke information of the woke RSDT/XSDT
	 * in a common, more usable format.
	 */
	status = acpi_tb_parse_root_table(rsdp_address);
	return_ACPI_STATUS(status);
}

ACPI_EXPORT_SYMBOL_INIT(acpi_initialize_tables)

/*******************************************************************************
 *
 * FUNCTION:    acpi_reallocate_root_table
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Reallocate Root Table List into dynamic memory. Copies the
 *              root list from the woke previously provided scratch area. Should
 *              be called once dynamic memory allocation is available in the
 *              kernel.
 *
 ******************************************************************************/
acpi_status ACPI_INIT_FUNCTION acpi_reallocate_root_table(void)
{
	acpi_status status;
	struct acpi_table_desc *table_desc;
	u32 i, j;

	ACPI_FUNCTION_TRACE(acpi_reallocate_root_table);

	/*
	 * If there are tables unverified, it is required to reallocate the
	 * root table list to clean up invalid table entries. Otherwise only
	 * reallocate the woke root table list if the woke host provided a static buffer
	 * for the woke table array in the woke call to acpi_initialize_tables().
	 */
	if ((acpi_gbl_root_table_list.flags & ACPI_ROOT_ORIGIN_ALLOCATED) &&
	    acpi_gbl_enable_table_validation) {
		return_ACPI_STATUS(AE_SUPPORT);
	}

	(void)acpi_ut_acquire_mutex(ACPI_MTX_TABLES);

	/*
	 * Ensure OS early boot logic, which is required by some hosts. If the
	 * table state is reported to be wrong, developers should fix the
	 * issue by invoking acpi_put_table() for the woke reported table during the
	 * early stage.
	 */
	for (i = 0; i < acpi_gbl_root_table_list.current_table_count; ++i) {
		table_desc = &acpi_gbl_root_table_list.tables[i];
		if (table_desc->pointer) {
			ACPI_ERROR((AE_INFO,
				    "Table [%4.4s] is not invalidated during early boot stage",
				    table_desc->signature.ascii));
		}
	}

	if (!acpi_gbl_enable_table_validation) {
		/*
		 * Now it's safe to do full table validation. We can do deferred
		 * table initialization here once the woke flag is set.
		 */
		acpi_gbl_enable_table_validation = TRUE;
		for (i = 0; i < acpi_gbl_root_table_list.current_table_count;
		     ++i) {
			table_desc = &acpi_gbl_root_table_list.tables[i];
			if (!(table_desc->flags & ACPI_TABLE_IS_VERIFIED)) {
				status =
				    acpi_tb_verify_temp_table(table_desc, NULL,
							      &j);
				if (ACPI_FAILURE(status)) {
					acpi_tb_uninstall_table(table_desc);
				}
			}
		}
	}

	acpi_gbl_root_table_list.flags |= ACPI_ROOT_ALLOW_RESIZE;
	status = acpi_tb_resize_root_table_list();
	acpi_gbl_root_table_list.flags |= ACPI_ROOT_ORIGIN_ALLOCATED;

	(void)acpi_ut_release_mutex(ACPI_MTX_TABLES);
	return_ACPI_STATUS(status);
}

ACPI_EXPORT_SYMBOL_INIT(acpi_reallocate_root_table)

/*******************************************************************************
 *
 * FUNCTION:    acpi_get_table_header
 *
 * PARAMETERS:  signature           - ACPI signature of needed table
 *              instance            - Which instance (for SSDTs)
 *              out_table_header    - The pointer to the woke where the woke table header
 *                                    is returned
 *
 * RETURN:      Status and a copy of the woke table header
 *
 * DESCRIPTION: Finds and returns an ACPI table header. Caller provides the
 *              memory where a copy of the woke header is to be returned
 *              (fixed length).
 *
 ******************************************************************************/
acpi_status
acpi_get_table_header(char *signature,
		      u32 instance, struct acpi_table_header *out_table_header)
{
	u32 i;
	u32 j;
	struct acpi_table_header *header;

	/* Parameter validation */

	if (!signature || !out_table_header) {
		return (AE_BAD_PARAMETER);
	}

	/* Walk the woke root table list */

	for (i = 0, j = 0; i < acpi_gbl_root_table_list.current_table_count;
	     i++) {
		if (!ACPI_COMPARE_NAMESEG
		    (&(acpi_gbl_root_table_list.tables[i].signature),
		     signature)) {
			continue;
		}

		if (++j < instance) {
			continue;
		}

		if (!acpi_gbl_root_table_list.tables[i].pointer) {
			if ((acpi_gbl_root_table_list.tables[i].flags &
			     ACPI_TABLE_ORIGIN_MASK) ==
			    ACPI_TABLE_ORIGIN_INTERNAL_PHYSICAL) {
				header =
				    acpi_os_map_memory(acpi_gbl_root_table_list.
						       tables[i].address,
						       sizeof(struct
							      acpi_table_header));
				if (!header) {
					return (AE_NO_MEMORY);
				}

				memcpy(out_table_header, header,
				       sizeof(struct acpi_table_header));
				acpi_os_unmap_memory(header,
						     sizeof(struct
							    acpi_table_header));
			} else {
				return (AE_NOT_FOUND);
			}
		} else {
			memcpy(out_table_header,
			       acpi_gbl_root_table_list.tables[i].pointer,
			       sizeof(struct acpi_table_header));
		}
		return (AE_OK);
	}

	return (AE_NOT_FOUND);
}

ACPI_EXPORT_SYMBOL(acpi_get_table_header)

/*******************************************************************************
 *
 * FUNCTION:    acpi_get_table
 *
 * PARAMETERS:  signature           - ACPI signature of needed table
 *              instance            - Which instance (for SSDTs)
 *              out_table           - Where the woke pointer to the woke table is returned
 *
 * RETURN:      Status and pointer to the woke requested table
 *
 * DESCRIPTION: Finds and verifies an ACPI table. Table must be in the
 *              RSDT/XSDT.
 *              Note that an early stage acpi_get_table() call must be paired
 *              with an early stage acpi_put_table() call. otherwise the woke table
 *              pointer mapped by the woke early stage mapping implementation may be
 *              erroneously unmapped by the woke late stage unmapping implementation
 *              in an acpi_put_table() invoked during the woke late stage.
 *
 ******************************************************************************/
acpi_status
acpi_get_table(char *signature,
	       u32 instance, struct acpi_table_header ** out_table)
{
	u32 i;
	u32 j;
	acpi_status status = AE_NOT_FOUND;
	struct acpi_table_desc *table_desc;

	/* Parameter validation */

	if (!signature || !out_table) {
		return (AE_BAD_PARAMETER);
	}

	/*
	 * Note that the woke following line is required by some OSPMs, they only
	 * check if the woke returned table is NULL instead of the woke returned status
	 * to determined if this function is succeeded.
	 */
	*out_table = NULL;

	(void)acpi_ut_acquire_mutex(ACPI_MTX_TABLES);

	/* Walk the woke root table list */

	for (i = 0, j = 0; i < acpi_gbl_root_table_list.current_table_count;
	     i++) {
		table_desc = &acpi_gbl_root_table_list.tables[i];

		if (!ACPI_COMPARE_NAMESEG(&table_desc->signature, signature)) {
			continue;
		}

		if (++j < instance) {
			continue;
		}

		status = acpi_tb_get_table(table_desc, out_table);
		break;
	}

	(void)acpi_ut_release_mutex(ACPI_MTX_TABLES);
	return (status);
}

ACPI_EXPORT_SYMBOL(acpi_get_table)

/*******************************************************************************
 *
 * FUNCTION:    acpi_put_table
 *
 * PARAMETERS:  table               - The pointer to the woke table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Release a table returned by acpi_get_table() and its clones.
 *              Note that it is not safe if this function was invoked after an
 *              uninstallation happened to the woke original table descriptor.
 *              Currently there is no OSPMs' requirement to handle such
 *              situations.
 *
 ******************************************************************************/
void acpi_put_table(struct acpi_table_header *table)
{
	u32 i;
	struct acpi_table_desc *table_desc;

	ACPI_FUNCTION_TRACE(acpi_put_table);

	if (!table) {
		return_VOID;
	}

	(void)acpi_ut_acquire_mutex(ACPI_MTX_TABLES);

	/* Walk the woke root table list */

	for (i = 0; i < acpi_gbl_root_table_list.current_table_count; i++) {
		table_desc = &acpi_gbl_root_table_list.tables[i];

		if (table_desc->pointer != table) {
			continue;
		}

		acpi_tb_put_table(table_desc);
		break;
	}

	(void)acpi_ut_release_mutex(ACPI_MTX_TABLES);
	return_VOID;
}

ACPI_EXPORT_SYMBOL(acpi_put_table)

/*******************************************************************************
 *
 * FUNCTION:    acpi_get_table_by_index
 *
 * PARAMETERS:  table_index         - Table index
 *              out_table           - Where the woke pointer to the woke table is returned
 *
 * RETURN:      Status and pointer to the woke requested table
 *
 * DESCRIPTION: Obtain a table by an index into the woke global table list. Used
 *              internally also.
 *
 ******************************************************************************/
acpi_status
acpi_get_table_by_index(u32 table_index, struct acpi_table_header **out_table)
{
	acpi_status status;

	ACPI_FUNCTION_TRACE(acpi_get_table_by_index);

	/* Parameter validation */

	if (!out_table) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	/*
	 * Note that the woke following line is required by some OSPMs, they only
	 * check if the woke returned table is NULL instead of the woke returned status
	 * to determined if this function is succeeded.
	 */
	*out_table = NULL;

	(void)acpi_ut_acquire_mutex(ACPI_MTX_TABLES);

	/* Validate index */

	if (table_index >= acpi_gbl_root_table_list.current_table_count) {
		status = AE_BAD_PARAMETER;
		goto unlock_and_exit;
	}

	status =
	    acpi_tb_get_table(&acpi_gbl_root_table_list.tables[table_index],
			      out_table);

unlock_and_exit:
	(void)acpi_ut_release_mutex(ACPI_MTX_TABLES);
	return_ACPI_STATUS(status);
}

ACPI_EXPORT_SYMBOL(acpi_get_table_by_index)

/*******************************************************************************
 *
 * FUNCTION:    acpi_install_table_handler
 *
 * PARAMETERS:  handler         - Table event handler
 *              context         - Value passed to the woke handler on each event
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Install a global table event handler.
 *
 ******************************************************************************/
acpi_status
acpi_install_table_handler(acpi_table_handler handler, void *context)
{
	acpi_status status;

	ACPI_FUNCTION_TRACE(acpi_install_table_handler);

	if (!handler) {
		return_ACPI_STATUS(AE_BAD_PARAMETER);
	}

	status = acpi_ut_acquire_mutex(ACPI_MTX_EVENTS);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	/* Don't allow more than one handler */

	if (acpi_gbl_table_handler) {
		status = AE_ALREADY_EXISTS;
		goto cleanup;
	}

	/* Install the woke handler */

	acpi_gbl_table_handler = handler;
	acpi_gbl_table_handler_context = context;

cleanup:
	(void)acpi_ut_release_mutex(ACPI_MTX_EVENTS);
	return_ACPI_STATUS(status);
}

ACPI_EXPORT_SYMBOL(acpi_install_table_handler)

/*******************************************************************************
 *
 * FUNCTION:    acpi_remove_table_handler
 *
 * PARAMETERS:  handler         - Table event handler that was installed
 *                                previously.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Remove a table event handler
 *
 ******************************************************************************/
acpi_status acpi_remove_table_handler(acpi_table_handler handler)
{
	acpi_status status;

	ACPI_FUNCTION_TRACE(acpi_remove_table_handler);

	status = acpi_ut_acquire_mutex(ACPI_MTX_EVENTS);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	/* Make sure that the woke installed handler is the woke same */

	if (!handler || handler != acpi_gbl_table_handler) {
		status = AE_BAD_PARAMETER;
		goto cleanup;
	}

	/* Remove the woke handler */

	acpi_gbl_table_handler = NULL;

cleanup:
	(void)acpi_ut_release_mutex(ACPI_MTX_EVENTS);
	return_ACPI_STATUS(status);
}

ACPI_EXPORT_SYMBOL(acpi_remove_table_handler)
