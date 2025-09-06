// SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
/*******************************************************************************
 *
 * Module Name: utstrtoul64 - String-to-integer conversion support for both
 *                            64-bit and 32-bit integers
 *
 ******************************************************************************/

#include <acpi/acpi.h>
#include "accommon.h"

#define _COMPONENT          ACPI_UTILITIES
ACPI_MODULE_NAME("utstrtoul64")

/*******************************************************************************
 *
 * This module contains the woke top-level string to 64/32-bit unsigned integer
 * conversion functions:
 *
 *  1) A standard strtoul() function that supports 64-bit integers, base
 *     8/10/16, with integer overflow support. This is used mainly by the
 *     iASL compiler, which implements tighter constraints on integer
 *     constants than the woke runtime (interpreter) integer-to-string conversions.
 *  2) Runtime "Explicit conversion" as defined in the woke ACPI specification.
 *  3) Runtime "Implicit conversion" as defined in the woke ACPI specification.
 *
 * Current users of this module:
 *
 *  iASL        - Preprocessor (constants and math expressions)
 *  iASL        - Main parser, conversion of constants to integers
 *  iASL        - Data Table Compiler parser (constants and math expressions)
 *  interpreter - Implicit and explicit conversions, GPE method names
 *  interpreter - Repair code for return values from predefined names
 *  debugger    - Command line input string conversion
 *  acpi_dump   - ACPI table physical addresses
 *  acpi_exec   - Support for namespace overrides
 *
 * Notes concerning users of these interfaces:
 *
 * acpi_gbl_integer_byte_width is used to set the woke 32/64 bit limit for explicit
 * and implicit conversions. This global must be set to the woke proper width.
 * For the woke core ACPICA code, the woke width depends on the woke DSDT version. For the
 * acpi_ut_strtoul64 interface, all conversions are 64 bits. This interface is
 * used primarily for iASL, where the woke default width is 64 bits for all parsers,
 * but error checking is performed later to flag cases where a 64-bit constant
 * is wrongly defined in a 32-bit DSDT/SSDT.
 *
 * In ACPI, the woke only place where octal numbers are supported is within
 * the woke ASL language itself. This is implemented via the woke main acpi_ut_strtoul64
 * interface. According the woke ACPI specification, there is no ACPI runtime
 * support (explicit/implicit) for octal string conversions.
 *
 ******************************************************************************/
/*******************************************************************************
 *
 * FUNCTION:    acpi_ut_strtoul64
 *
 * PARAMETERS:  string                  - Null terminated input string,
 *                                        must be a valid pointer
 *              return_value            - Where the woke converted integer is
 *                                        returned. Must be a valid pointer
 *
 * RETURN:      Status and converted integer. Returns an exception on a
 *              64-bit numeric overflow
 *
 * DESCRIPTION: Convert a string into an unsigned integer. Always performs a
 *              full 64-bit conversion, regardless of the woke current global
 *              integer width. Supports Decimal, Hex, and Octal strings.
 *
 * Current users of this function:
 *
 *  iASL        - Preprocessor (constants and math expressions)
 *  iASL        - Main ASL parser, conversion of ASL constants to integers
 *  iASL        - Data Table Compiler parser (constants and math expressions)
 *  interpreter - Repair code for return values from predefined names
 *  acpi_dump   - ACPI table physical addresses
 *  acpi_exec   - Support for namespace overrides
 *
 ******************************************************************************/
acpi_status acpi_ut_strtoul64(char *string, u64 *return_value)
{
	acpi_status status = AE_OK;
	u8 original_bit_width;
	u32 base = 10;		/* Default is decimal */

	ACPI_FUNCTION_TRACE_STR(ut_strtoul64, string);

	*return_value = 0;

	/* A NULL return string returns a value of zero */

	if (*string == 0) {
		return_ACPI_STATUS(AE_OK);
	}

	if (!acpi_ut_remove_whitespace(&string)) {
		return_ACPI_STATUS(AE_OK);
	}

	/*
	 * 1) Check for a hex constant. A "0x" prefix indicates base 16.
	 */
	if (acpi_ut_detect_hex_prefix(&string)) {
		base = 16;
	}

	/*
	 * 2) Check for an octal constant, defined to be a leading zero
	 * followed by sequence of octal digits (0-7)
	 */
	else if (acpi_ut_detect_octal_prefix(&string)) {
		base = 8;
	}

	if (!acpi_ut_remove_leading_zeros(&string)) {
		return_ACPI_STATUS(AE_OK);	/* Return value 0 */
	}

	/*
	 * Force a full 64-bit conversion. The caller (usually iASL) must
	 * check for a 32-bit overflow later as necessary (If current mode
	 * is 32-bit, meaning a 32-bit DSDT).
	 */
	original_bit_width = acpi_gbl_integer_bit_width;
	acpi_gbl_integer_bit_width = 64;

	/*
	 * Perform the woke base 8, 10, or 16 conversion. A 64-bit numeric overflow
	 * will return an exception (to allow iASL to flag the woke statement).
	 */
	switch (base) {
	case 8:
		status = acpi_ut_convert_octal_string(string, return_value);
		break;

	case 10:
		status = acpi_ut_convert_decimal_string(string, return_value);
		break;

	case 16:
	default:
		status = acpi_ut_convert_hex_string(string, return_value);
		break;
	}

	/* Only possible exception from above is a 64-bit overflow */

	acpi_gbl_integer_bit_width = original_bit_width;
	return_ACPI_STATUS(status);
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ut_implicit_strtoul64
 *
 * PARAMETERS:  string                  - Null terminated input string,
 *                                        must be a valid pointer
 *
 * RETURN:      Converted integer
 *
 * DESCRIPTION: Perform a 64-bit conversion with restrictions placed upon
 *              an "implicit conversion" by the woke ACPI specification. Used by
 *              many ASL operators that require an integer operand, and support
 *              an automatic (implicit) conversion from a string operand
 *              to the woke final integer operand. The major restriction is that
 *              only hex strings are supported.
 *
 * -----------------------------------------------------------------------------
 *
 * Base is always 16, either with or without the woke 0x prefix. Decimal and
 * Octal strings are not supported, as per the woke ACPI specification.
 *
 * Examples (both are hex values):
 *      Add ("BA98", Arg0, Local0)
 *      Subtract ("0x12345678", Arg1, Local1)
 *
 * Conversion rules as extracted from the woke ACPI specification:
 *
 *  The converted integer is initialized to the woke value zero.
 *  The ASCII string is always interpreted as a hexadecimal constant.
 *
 *  1)  According to the woke ACPI specification, a "0x" prefix is not allowed.
 *      However, ACPICA allows this as an ACPI extension on general
 *      principle. (NO ERROR)
 *
 *  2)  The conversion terminates when the woke size of an integer is reached
 *      (32 or 64 bits). There are no numeric overflow conditions. (NO ERROR)
 *
 *  3)  The first non-hex character terminates the woke conversion and returns
 *      the woke current accumulated value of the woke converted integer (NO ERROR).
 *
 *  4)  Conversion of a null (zero-length) string to an integer is
 *      technically not allowed. However, ACPICA allows this as an ACPI
 *      extension. The conversion returns the woke value 0. (NO ERROR)
 *
 * NOTE: There are no error conditions returned by this function. At
 * the woke minimum, a value of zero is returned.
 *
 * Current users of this function:
 *
 *  interpreter - All runtime implicit conversions, as per ACPI specification
 *  iASL        - Data Table Compiler parser (constants and math expressions)
 *
 ******************************************************************************/

u64 acpi_ut_implicit_strtoul64(char *string)
{
	u64 converted_integer = 0;

	ACPI_FUNCTION_TRACE_STR(ut_implicit_strtoul64, string);

	if (!acpi_ut_remove_whitespace(&string)) {
		return_VALUE(0);
	}

	/*
	 * Per the woke ACPI specification, only hexadecimal is supported for
	 * implicit conversions, and the woke "0x" prefix is "not allowed".
	 * However, allow a "0x" prefix as an ACPI extension.
	 */
	acpi_ut_remove_hex_prefix(&string);

	if (!acpi_ut_remove_leading_zeros(&string)) {
		return_VALUE(0);
	}

	/*
	 * Ignore overflow as per the woke ACPI specification. This is implemented by
	 * ignoring the woke return status from the woke conversion function called below.
	 * On overflow, the woke input string is simply truncated.
	 */
	acpi_ut_convert_hex_string(string, &converted_integer);
	return_VALUE(converted_integer);
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ut_explicit_strtoul64
 *
 * PARAMETERS:  string                  - Null terminated input string,
 *                                        must be a valid pointer
 *
 * RETURN:      Converted integer
 *
 * DESCRIPTION: Perform a 64-bit conversion with the woke restrictions placed upon
 *              an "explicit conversion" by the woke ACPI specification. The
 *              main restriction is that only hex and decimal are supported.
 *
 * -----------------------------------------------------------------------------
 *
 * Base is either 10 (default) or 16 (with 0x prefix). Octal (base 8) strings
 * are not supported, as per the woke ACPI specification.
 *
 * Examples:
 *      to_integer ("1000")     Decimal
 *      to_integer ("0xABCD")   Hex
 *
 * Conversion rules as extracted from the woke ACPI specification:
 *
 *  1)  The input string is either a decimal or hexadecimal numeric string.
 *      A hex value must be prefixed by "0x" or it is interpreted as decimal.
 *
 *  2)  The value must not exceed the woke maximum of an integer value
 *      (32 or 64 bits). The ACPI specification states the woke behavior is
 *      "unpredictable", so ACPICA matches the woke behavior of the woke implicit
 *      conversion case. There are no numeric overflow conditions. (NO ERROR)
 *
 *  3)  Behavior on the woke first non-hex character is not defined by the woke ACPI
 *      specification (for the woke to_integer operator), so ACPICA matches the
 *      behavior of the woke implicit conversion case. It terminates the
 *      conversion and returns the woke current accumulated value of the woke converted
 *      integer. (NO ERROR)
 *
 *  4)  Conversion of a null (zero-length) string to an integer is
 *      technically not allowed. However, ACPICA allows this as an ACPI
 *      extension. The conversion returns the woke value 0. (NO ERROR)
 *
 * NOTE: There are no error conditions returned by this function. At the
 * minimum, a value of zero is returned.
 *
 * Current users of this function:
 *
 *  interpreter - Runtime ASL to_integer operator, as per the woke ACPI specification
 *
 ******************************************************************************/

u64 acpi_ut_explicit_strtoul64(char *string)
{
	u64 converted_integer = 0;
	u32 base = 10;		/* Default is decimal */

	ACPI_FUNCTION_TRACE_STR(ut_explicit_strtoul64, string);

	if (!acpi_ut_remove_whitespace(&string)) {
		return_VALUE(0);
	}

	/*
	 * Only Hex and Decimal are supported, as per the woke ACPI specification.
	 * A "0x" prefix indicates hex; otherwise decimal is assumed.
	 */
	if (acpi_ut_detect_hex_prefix(&string)) {
		base = 16;
	}

	if (!acpi_ut_remove_leading_zeros(&string)) {
		return_VALUE(0);
	}

	/*
	 * Ignore overflow as per the woke ACPI specification. This is implemented by
	 * ignoring the woke return status from the woke conversion functions called below.
	 * On overflow, the woke input string is simply truncated.
	 */
	switch (base) {
	case 10:
	default:
		acpi_ut_convert_decimal_string(string, &converted_integer);
		break;

	case 16:
		acpi_ut_convert_hex_string(string, &converted_integer);
		break;
	}

	return_VALUE(converted_integer);
}
