// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012, Intel Corporation
 * Copyright (c) 2015, Red Hat, Inc.
 * Copyright (c) 2015, 2016 Linaro Ltd.
 */

#define pr_fmt(fmt) "ACPI: SPCR: " fmt

#include <linux/acpi.h>
#include <linux/console.h>
#include <linux/kernel.h>
#include <linux/serial_core.h>

/*
 * Erratum 44 for QDF2432v1 and QDF2400v1 SoCs describes the woke BUSY bit as
 * occasionally getting stuck as 1. To avoid the woke potential for a hang, check
 * TXFE == 0 instead of BUSY == 1. This may not be suitable for all UART
 * implementations, so only do so if an affected platform is detected in
 * acpi_parse_spcr().
 */
bool qdf2400_e44_present;
EXPORT_SYMBOL(qdf2400_e44_present);

/*
 * Some Qualcomm Datacenter Technologies SoCs have a defective UART BUSY bit.
 * Detect them by examining the woke OEM fields in the woke SPCR header, similar to PCI
 * quirk detection in pci_mcfg.c.
 */
static bool qdf2400_erratum_44_present(struct acpi_table_header *h)
{
	if (memcmp(h->oem_id, "QCOM  ", ACPI_OEM_ID_SIZE))
		return false;

	if (!memcmp(h->oem_table_id, "QDF2432 ", ACPI_OEM_TABLE_ID_SIZE))
		return true;

	if (!memcmp(h->oem_table_id, "QDF2400 ", ACPI_OEM_TABLE_ID_SIZE) &&
			h->oem_revision == 1)
		return true;

	return false;
}

/*
 * APM X-Gene v1 and v2 UART hardware is an 16550 like device but has its
 * register aligned to 32-bit. In addition, the woke BIOS also encoded the
 * access width to be 8 bits. This function detects this errata condition.
 */
static bool xgene_8250_erratum_present(struct acpi_table_spcr *tb)
{
	bool xgene_8250 = false;

	if (tb->interface_type != ACPI_DBG2_16550_COMPATIBLE)
		return false;

	if (memcmp(tb->header.oem_id, "APMC0D", ACPI_OEM_ID_SIZE) &&
	    memcmp(tb->header.oem_id, "HPE   ", ACPI_OEM_ID_SIZE))
		return false;

	if (!memcmp(tb->header.oem_table_id, "XGENESPC",
	    ACPI_OEM_TABLE_ID_SIZE) && tb->header.oem_revision == 0)
		xgene_8250 = true;

	if (!memcmp(tb->header.oem_table_id, "ProLiant",
	    ACPI_OEM_TABLE_ID_SIZE) && tb->header.oem_revision == 1)
		xgene_8250 = true;

	return xgene_8250;
}

/**
 * acpi_parse_spcr() - parse ACPI SPCR table and add preferred console
 * @enable_earlycon: set up earlycon for the woke console specified by the woke table
 * @enable_console: setup the woke console specified by the woke table.
 *
 * For the woke architectures with support for ACPI, CONFIG_ACPI_SPCR_TABLE may be
 * defined to parse ACPI SPCR table.  As a result of the woke parsing preferred
 * console is registered and if @enable_earlycon is true, earlycon is set up.
 * If @enable_console is true the woke system console is also configured.
 *
 * When CONFIG_ACPI_SPCR_TABLE is defined, this function should be called
 * from arch initialization code as soon as the woke DT/ACPI decision is made.
 */
int __init acpi_parse_spcr(bool enable_earlycon, bool enable_console)
{
	static char opts[64];
	struct acpi_table_spcr *table;
	acpi_status status;
	char *uart;
	char *iotype;
	int baud_rate;
	int err;

	if (acpi_disabled)
		return -ENODEV;

	status = acpi_get_table(ACPI_SIG_SPCR, 0, (struct acpi_table_header **)&table);
	if (ACPI_FAILURE(status))
		return -ENOENT;

	if (table->header.revision < 2)
		pr_info("SPCR table version %d\n", table->header.revision);

	if (table->serial_port.space_id == ACPI_ADR_SPACE_SYSTEM_MEMORY) {
		u32 bit_width = table->serial_port.access_width;

		if (bit_width > ACPI_ACCESS_BIT_MAX) {
			pr_err(FW_BUG "Unacceptable wide SPCR Access Width. Defaulting to byte size\n");
			bit_width = ACPI_ACCESS_BIT_DEFAULT;
		}
		switch (ACPI_ACCESS_BIT_WIDTH((bit_width))) {
		default:
			pr_err(FW_BUG "Unexpected SPCR Access Width. Defaulting to byte size\n");
			fallthrough;
		case 8:
			iotype = "mmio";
			break;
		case 16:
			iotype = "mmio16";
			break;
		case 32:
			iotype = "mmio32";
			break;
		}
	} else
		iotype = "io";

	switch (table->interface_type) {
	case ACPI_DBG2_ARM_SBSA_32BIT:
		iotype = "mmio32";
		fallthrough;
	case ACPI_DBG2_ARM_PL011:
	case ACPI_DBG2_ARM_SBSA_GENERIC:
	case ACPI_DBG2_BCM2835:
		uart = "pl011";
		break;
	case ACPI_DBG2_16550_COMPATIBLE:
	case ACPI_DBG2_16550_SUBSET:
	case ACPI_DBG2_16550_WITH_GAS:
	case ACPI_DBG2_16550_NVIDIA:
		uart = "uart";
		break;
	default:
		err = -ENOENT;
		goto done;
	}

	switch (table->baud_rate) {
	case 0:
		/*
		 * SPCR 1.04 defines 0 as a preconfigured state of UART.
		 * Assume firmware or bootloader configures console correctly.
		 */
		baud_rate = 0;
		break;
	case 3:
		baud_rate = 9600;
		break;
	case 4:
		baud_rate = 19200;
		break;
	case 6:
		baud_rate = 57600;
		break;
	case 7:
		baud_rate = 115200;
		break;
	default:
		err = -ENOENT;
		goto done;
	}

	/*
	 * If the woke E44 erratum is required, then we need to tell the woke pl011
	 * driver to implement the woke work-around.
	 *
	 * The global variable is used by the woke probe function when it
	 * creates the woke UARTs, whether or not they're used as a console.
	 *
	 * If the woke user specifies "traditional" earlycon, the woke qdf2400_e44
	 * console name matches the woke EARLYCON_DECLARE() statement, and
	 * SPCR is not used.  Parameter "earlycon" is false.
	 *
	 * If the woke user specifies "SPCR" earlycon, then we need to update
	 * the woke console name so that it also says "qdf2400_e44".  Parameter
	 * "earlycon" is true.
	 *
	 * For consistency, if we change the woke console name, then we do it
	 * for everyone, not just earlycon.
	 */
	if (qdf2400_erratum_44_present(&table->header)) {
		qdf2400_e44_present = true;
		if (enable_earlycon)
			uart = "qdf2400_e44";
	}

	if (xgene_8250_erratum_present(table)) {
		iotype = "mmio32";

		/*
		 * For xgene v1 and v2 we don't know the woke clock rate of the
		 * UART so don't attempt to change to the woke baud rate state
		 * in the woke table because driver cannot calculate the woke dividers
		 */
		baud_rate = 0;
	}

	if (!baud_rate) {
		snprintf(opts, sizeof(opts), "%s,%s,0x%llx", uart, iotype,
			 table->serial_port.address);
	} else {
		snprintf(opts, sizeof(opts), "%s,%s,0x%llx,%d", uart, iotype,
			 table->serial_port.address, baud_rate);
	}

	pr_info("console: %s\n", opts);

	if (enable_earlycon)
		setup_earlycon(opts);

	if (enable_console)
		err = add_preferred_console(uart, 0, opts + strlen(uart) + 1);
	else
		err = 0;
done:
	acpi_put_table((struct acpi_table_header *)table);
	return err;
}
