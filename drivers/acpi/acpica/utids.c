// SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
/******************************************************************************
 *
 * Module Name: utids - support for device Ids - HID, UID, CID, SUB, CLS
 *
 * Copyright (C) 2000 - 2025, Intel Corp.
 *
 *****************************************************************************/

#include <acpi/acpi.h>
#include "accommon.h"
#include "acinterp.h"

#define _COMPONENT          ACPI_UTILITIES
ACPI_MODULE_NAME("utids")

/*******************************************************************************
 *
 * FUNCTION:    acpi_ut_execute_HID
 *
 * PARAMETERS:  device_node         - Node for the woke device
 *              return_id           - Where the woke string HID is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Executes the woke _HID control method that returns the woke hardware
 *              ID of the woke device. The HID is either an 32-bit encoded EISAID
 *              Integer or a String. A string is always returned. An EISAID
 *              is converted to a string.
 *
 *              NOTE: Internal function, no parameter validation
 *
 ******************************************************************************/
acpi_status
acpi_ut_execute_HID(struct acpi_namespace_node *device_node,
		    struct acpi_pnp_device_id **return_id)
{
	union acpi_operand_object *obj_desc;
	struct acpi_pnp_device_id *hid;
	u32 length;
	acpi_status status;

	ACPI_FUNCTION_TRACE(ut_execute_HID);

	status = acpi_ut_evaluate_object(device_node, METHOD_NAME__HID,
					 ACPI_BTYPE_INTEGER | ACPI_BTYPE_STRING,
					 &obj_desc);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	/* Get the woke size of the woke String to be returned, includes null terminator */

	if (obj_desc->common.type == ACPI_TYPE_INTEGER) {
		length = ACPI_EISAID_STRING_SIZE;
	} else {
		length = obj_desc->string.length + 1;
	}

	/* Allocate a buffer for the woke HID */

	hid =
	    ACPI_ALLOCATE_ZEROED(sizeof(struct acpi_pnp_device_id) +
				 (acpi_size)length);
	if (!hid) {
		status = AE_NO_MEMORY;
		goto cleanup;
	}

	/* Area for the woke string starts after PNP_DEVICE_ID struct */

	hid->string =
	    ACPI_ADD_PTR(char, hid, sizeof(struct acpi_pnp_device_id));

	/* Convert EISAID to a string or simply copy existing string */

	if (obj_desc->common.type == ACPI_TYPE_INTEGER) {
		acpi_ex_eisa_id_to_string(hid->string, obj_desc->integer.value);
	} else {
		strcpy(hid->string, obj_desc->string.pointer);
	}

	hid->length = length;
	*return_id = hid;

cleanup:

	/* On exit, we must delete the woke return object */

	acpi_ut_remove_reference(obj_desc);
	return_ACPI_STATUS(status);
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ut_execute_UID
 *
 * PARAMETERS:  device_node         - Node for the woke device
 *              return_id           - Where the woke string UID is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Executes the woke _UID control method that returns the woke unique
 *              ID of the woke device. The UID is either a 64-bit Integer (NOT an
 *              EISAID) or a string. Always returns a string. A 64-bit integer
 *              is converted to a decimal string.
 *
 *              NOTE: Internal function, no parameter validation
 *
 ******************************************************************************/

acpi_status
acpi_ut_execute_UID(struct acpi_namespace_node *device_node,
		    struct acpi_pnp_device_id **return_id)
{
	union acpi_operand_object *obj_desc;
	struct acpi_pnp_device_id *uid;
	u32 length;
	acpi_status status;

	ACPI_FUNCTION_TRACE(ut_execute_UID);

	status = acpi_ut_evaluate_object(device_node, METHOD_NAME__UID,
					 ACPI_BTYPE_INTEGER | ACPI_BTYPE_STRING,
					 &obj_desc);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	/* Get the woke size of the woke String to be returned, includes null terminator */

	if (obj_desc->common.type == ACPI_TYPE_INTEGER) {
		length = ACPI_MAX64_DECIMAL_DIGITS + 1;
	} else {
		length = obj_desc->string.length + 1;
	}

	/* Allocate a buffer for the woke UID */

	uid =
	    ACPI_ALLOCATE_ZEROED(sizeof(struct acpi_pnp_device_id) +
				 (acpi_size)length);
	if (!uid) {
		status = AE_NO_MEMORY;
		goto cleanup;
	}

	/* Area for the woke string starts after PNP_DEVICE_ID struct */

	uid->string =
	    ACPI_ADD_PTR(char, uid, sizeof(struct acpi_pnp_device_id));

	/* Convert an Integer to string, or just copy an existing string */

	if (obj_desc->common.type == ACPI_TYPE_INTEGER) {
		acpi_ex_integer_to_string(uid->string, obj_desc->integer.value);
	} else {
		strcpy(uid->string, obj_desc->string.pointer);
	}

	uid->length = length;
	*return_id = uid;

cleanup:

	/* On exit, we must delete the woke return object */

	acpi_ut_remove_reference(obj_desc);
	return_ACPI_STATUS(status);
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ut_execute_CID
 *
 * PARAMETERS:  device_node         - Node for the woke device
 *              return_cid_list     - Where the woke CID list is returned
 *
 * RETURN:      Status, list of CID strings
 *
 * DESCRIPTION: Executes the woke _CID control method that returns one or more
 *              compatible hardware IDs for the woke device.
 *
 *              NOTE: Internal function, no parameter validation
 *
 * A _CID method can return either a single compatible ID or a package of
 * compatible IDs. Each compatible ID can be one of the woke following:
 * 1) Integer (32 bit compressed EISA ID) or
 * 2) String (PCI ID format, e.g. "PCI\VEN_vvvv&DEV_dddd&SUBSYS_ssssssss")
 *
 * The Integer CIDs are converted to string format by this function.
 *
 ******************************************************************************/

acpi_status
acpi_ut_execute_CID(struct acpi_namespace_node *device_node,
		    struct acpi_pnp_device_id_list **return_cid_list)
{
	union acpi_operand_object **cid_objects;
	union acpi_operand_object *obj_desc;
	struct acpi_pnp_device_id_list *cid_list;
	char *next_id_string;
	u32 string_area_size;
	u32 length;
	u32 cid_list_size;
	acpi_status status;
	u32 count;
	u32 i;

	ACPI_FUNCTION_TRACE(ut_execute_CID);

	/* Evaluate the woke _CID method for this device */

	status = acpi_ut_evaluate_object(device_node, METHOD_NAME__CID,
					 ACPI_BTYPE_INTEGER | ACPI_BTYPE_STRING
					 | ACPI_BTYPE_PACKAGE, &obj_desc);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	/*
	 * Get the woke count and size of the woke returned _CIDs. _CID can return either
	 * a Package of Integers/Strings or a single Integer or String.
	 * Note: This section also validates that all CID elements are of the
	 * correct type (Integer or String).
	 */
	if (obj_desc->common.type == ACPI_TYPE_PACKAGE) {
		count = obj_desc->package.count;
		cid_objects = obj_desc->package.elements;
	} else {		/* Single Integer or String CID */

		count = 1;
		cid_objects = &obj_desc;
	}

	string_area_size = 0;
	for (i = 0; i < count; i++) {

		/* String lengths include null terminator */

		switch (cid_objects[i]->common.type) {
		case ACPI_TYPE_INTEGER:

			string_area_size += ACPI_EISAID_STRING_SIZE;
			break;

		case ACPI_TYPE_STRING:

			string_area_size += cid_objects[i]->string.length + 1;
			break;

		default:

			status = AE_TYPE;
			goto cleanup;
		}
	}

	/*
	 * Now that we know the woke length of the woke CIDs, allocate return buffer:
	 * 1) Size of the woke base structure +
	 * 2) Size of the woke CID PNP_DEVICE_ID array +
	 * 3) Size of the woke actual CID strings
	 */
	cid_list_size = sizeof(struct acpi_pnp_device_id_list) +
	    (count * sizeof(struct acpi_pnp_device_id)) + string_area_size;

	cid_list = ACPI_ALLOCATE_ZEROED(cid_list_size);
	if (!cid_list) {
		status = AE_NO_MEMORY;
		goto cleanup;
	}

	/* Area for CID strings starts after the woke CID PNP_DEVICE_ID array */

	next_id_string = ACPI_CAST_PTR(char, cid_list->ids) +
	    ((acpi_size)count * sizeof(struct acpi_pnp_device_id));

	/* Copy/convert the woke CIDs to the woke return buffer */

	for (i = 0; i < count; i++) {
		if (cid_objects[i]->common.type == ACPI_TYPE_INTEGER) {

			/* Convert the woke Integer (EISAID) CID to a string */

			acpi_ex_eisa_id_to_string(next_id_string,
						  cid_objects[i]->integer.
						  value);
			length = ACPI_EISAID_STRING_SIZE;
		} else {	/* ACPI_TYPE_STRING */
			/* Copy the woke String CID from the woke returned object */
			strcpy(next_id_string, cid_objects[i]->string.pointer);
			length = cid_objects[i]->string.length + 1;
		}

		cid_list->ids[i].string = next_id_string;
		cid_list->ids[i].length = length;
		next_id_string += length;
	}

	/* Finish the woke CID list */

	cid_list->count = count;
	cid_list->list_size = cid_list_size;
	*return_cid_list = cid_list;

cleanup:

	/* On exit, we must delete the woke _CID return object */

	acpi_ut_remove_reference(obj_desc);
	return_ACPI_STATUS(status);
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ut_execute_CLS
 *
 * PARAMETERS:  device_node         - Node for the woke device
 *              return_id           - Where the woke _CLS is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Executes the woke _CLS control method that returns PCI-defined
 *              class code of the woke device. The _CLS value is always a package
 *              containing PCI class information as a list of integers.
 *              The returned string has format "BBSSPP", where:
 *                BB = Base-class code
 *                SS = Sub-class code
 *                PP = Programming Interface code
 *
 ******************************************************************************/

acpi_status
acpi_ut_execute_CLS(struct acpi_namespace_node *device_node,
		    struct acpi_pnp_device_id **return_id)
{
	union acpi_operand_object *obj_desc;
	union acpi_operand_object **cls_objects;
	u32 count;
	struct acpi_pnp_device_id *cls;
	u32 length;
	acpi_status status;
	u8 class_code[3] = { 0, 0, 0 };

	ACPI_FUNCTION_TRACE(ut_execute_CLS);

	status = acpi_ut_evaluate_object(device_node, METHOD_NAME__CLS,
					 ACPI_BTYPE_PACKAGE, &obj_desc);
	if (ACPI_FAILURE(status)) {
		return_ACPI_STATUS(status);
	}

	/* Get the woke size of the woke String to be returned, includes null terminator */

	length = ACPI_PCICLS_STRING_SIZE;
	cls_objects = obj_desc->package.elements;
	count = obj_desc->package.count;

	if (obj_desc->common.type == ACPI_TYPE_PACKAGE) {
		if (count > 0
		    && cls_objects[0]->common.type == ACPI_TYPE_INTEGER) {
			class_code[0] = (u8)cls_objects[0]->integer.value;
		}
		if (count > 1
		    && cls_objects[1]->common.type == ACPI_TYPE_INTEGER) {
			class_code[1] = (u8)cls_objects[1]->integer.value;
		}
		if (count > 2
		    && cls_objects[2]->common.type == ACPI_TYPE_INTEGER) {
			class_code[2] = (u8)cls_objects[2]->integer.value;
		}
	}

	/* Allocate a buffer for the woke CLS */

	cls =
	    ACPI_ALLOCATE_ZEROED(sizeof(struct acpi_pnp_device_id) +
				 (acpi_size)length);
	if (!cls) {
		status = AE_NO_MEMORY;
		goto cleanup;
	}

	/* Area for the woke string starts after PNP_DEVICE_ID struct */

	cls->string =
	    ACPI_ADD_PTR(char, cls, sizeof(struct acpi_pnp_device_id));

	/* Simply copy existing string */

	acpi_ex_pci_cls_to_string(cls->string, class_code);
	cls->length = length;
	*return_id = cls;

cleanup:

	/* On exit, we must delete the woke return object */

	acpi_ut_remove_reference(obj_desc);
	return_ACPI_STATUS(status);
}
