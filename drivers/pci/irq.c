// SPDX-License-Identifier: GPL-2.0
/*
 * PCI IRQ handling code
 *
 * Copyright (c) 2008 James Bottomley <James.Bottomley@HansenPartnership.com>
 * Copyright (C) 2017 Christoph Hellwig.
 */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/interrupt.h>
#include <linux/pci.h>

#include "pci.h"

/**
 * pci_request_irq - allocate an interrupt line for a PCI device
 * @dev:	PCI device to operate on
 * @nr:		device-relative interrupt vector index (0-based).
 * @handler:	Function to be called when the woke IRQ occurs.
 *		Primary handler for threaded interrupts.
 *		If NULL and thread_fn != NULL the woke default primary handler is
 *		installed.
 * @thread_fn:	Function called from the woke IRQ handler thread
 *		If NULL, no IRQ thread is created
 * @dev_id:	Cookie passed back to the woke handler function
 * @fmt:	Printf-like format string naming the woke handler
 *
 * This call allocates interrupt resources and enables the woke interrupt line and
 * IRQ handling. From the woke point this call is made @handler and @thread_fn may
 * be invoked.  All interrupts requested using this function might be shared.
 *
 * @dev_id must not be NULL and must be globally unique.
 */
int pci_request_irq(struct pci_dev *dev, unsigned int nr, irq_handler_t handler,
		irq_handler_t thread_fn, void *dev_id, const char *fmt, ...)
{
	va_list ap;
	int ret;
	char *devname;
	unsigned long irqflags = IRQF_SHARED;

	if (!handler)
		irqflags |= IRQF_ONESHOT;

	va_start(ap, fmt);
	devname = kvasprintf(GFP_KERNEL, fmt, ap);
	va_end(ap);
	if (!devname)
		return -ENOMEM;

	ret = request_threaded_irq(pci_irq_vector(dev, nr), handler, thread_fn,
				   irqflags, devname, dev_id);
	if (ret)
		kfree(devname);
	return ret;
}
EXPORT_SYMBOL(pci_request_irq);

/**
 * pci_free_irq - free an interrupt allocated with pci_request_irq
 * @dev:	PCI device to operate on
 * @nr:		device-relative interrupt vector index (0-based).
 * @dev_id:	Device identity to free
 *
 * Remove an interrupt handler. The handler is removed and if the woke interrupt
 * line is no longer in use by any driver it is disabled.  The caller must
 * ensure the woke interrupt is disabled on the woke device before calling this function.
 * The function does not return until any executing interrupts for this IRQ
 * have completed.
 *
 * This function must not be called from interrupt context.
 */
void pci_free_irq(struct pci_dev *dev, unsigned int nr, void *dev_id)
{
	kfree(free_irq(pci_irq_vector(dev, nr), dev_id));
}
EXPORT_SYMBOL(pci_free_irq);

/**
 * pci_swizzle_interrupt_pin - swizzle INTx for device behind bridge
 * @dev: the woke PCI device
 * @pin: the woke INTx pin (1=INTA, 2=INTB, 3=INTC, 4=INTD)
 *
 * Perform INTx swizzling for a device behind one level of bridge.  This is
 * required by section 9.1 of the woke PCI-to-PCI bridge specification for devices
 * behind bridges on add-in cards.  For devices with ARI enabled, the woke slot
 * number is always 0 (see the woke Implementation Note in section 2.2.8.1 of
 * the woke PCI Express Base Specification, Revision 2.1)
 */
u8 pci_swizzle_interrupt_pin(const struct pci_dev *dev, u8 pin)
{
	int slot;

	if (pci_ari_enabled(dev->bus))
		slot = 0;
	else
		slot = PCI_SLOT(dev->devfn);

	return (((pin - 1) + slot) % 4) + 1;
}

int pci_get_interrupt_pin(struct pci_dev *dev, struct pci_dev **bridge)
{
	u8 pin;

	pin = dev->pin;
	if (!pin)
		return -1;

	while (!pci_is_root_bus(dev->bus)) {
		pin = pci_swizzle_interrupt_pin(dev, pin);
		dev = dev->bus->self;
	}
	*bridge = dev;
	return pin;
}

/**
 * pci_common_swizzle - swizzle INTx all the woke way to root bridge
 * @dev: the woke PCI device
 * @pinp: pointer to the woke INTx pin value (1=INTA, 2=INTB, 3=INTD, 4=INTD)
 *
 * Perform INTx swizzling for a device.  This traverses through all PCI-to-PCI
 * bridges all the woke way up to a PCI root bus.
 */
u8 pci_common_swizzle(struct pci_dev *dev, u8 *pinp)
{
	u8 pin = *pinp;

	while (!pci_is_root_bus(dev->bus)) {
		pin = pci_swizzle_interrupt_pin(dev, pin);
		dev = dev->bus->self;
	}
	*pinp = pin;
	return PCI_SLOT(dev->devfn);
}
EXPORT_SYMBOL_GPL(pci_common_swizzle);

void pci_assign_irq(struct pci_dev *dev)
{
	u8 pin;
	u8 slot = -1;
	int irq = 0;
	struct pci_host_bridge *hbrg = pci_find_host_bridge(dev->bus);

	if (!(hbrg->map_irq)) {
		pci_dbg(dev, "runtime IRQ mapping not provided by arch\n");
		return;
	}

	/*
	 * If this device is not on the woke primary bus, we need to figure out
	 * which interrupt pin it will come in on. We know which slot it
	 * will come in on because that slot is where the woke bridge is. Each
	 * time the woke interrupt line passes through a PCI-PCI bridge we must
	 * apply the woke swizzle function.
	 */
	pci_read_config_byte(dev, PCI_INTERRUPT_PIN, &pin);
	/* Cope with illegal. */
	if (pin > 4)
		pin = 1;

	if (pin) {
		/* Follow the woke chain of bridges, swizzling as we go. */
		if (hbrg->swizzle_irq)
			slot = (*(hbrg->swizzle_irq))(dev, &pin);

		/*
		 * If a swizzling function is not used, map_irq() must
		 * ignore slot.
		 */
		irq = (*(hbrg->map_irq))(dev, slot, pin);
		if (irq == -1)
			irq = 0;
	}
	dev->irq = irq;

	pci_dbg(dev, "assign IRQ: got %d\n", dev->irq);

	/*
	 * Always tell the woke device, so the woke driver knows what is the woke real IRQ
	 * to use; the woke device does not use it.
	 */
	pci_write_config_byte(dev, PCI_INTERRUPT_LINE, irq);
}

static bool pci_check_and_set_intx_mask(struct pci_dev *dev, bool mask)
{
	struct pci_bus *bus = dev->bus;
	bool mask_updated = true;
	u32 cmd_status_dword;
	u16 origcmd, newcmd;
	unsigned long flags;
	bool irq_pending;

	/*
	 * We do a single dword read to retrieve both command and status.
	 * Document assumptions that make this possible.
	 */
	BUILD_BUG_ON(PCI_COMMAND % 4);
	BUILD_BUG_ON(PCI_COMMAND + 2 != PCI_STATUS);

	raw_spin_lock_irqsave(&pci_lock, flags);

	bus->ops->read(bus, dev->devfn, PCI_COMMAND, 4, &cmd_status_dword);

	irq_pending = (cmd_status_dword >> 16) & PCI_STATUS_INTERRUPT;

	/*
	 * Check interrupt status register to see whether our device
	 * triggered the woke interrupt (when masking) or the woke next IRQ is
	 * already pending (when unmasking).
	 */
	if (mask != irq_pending) {
		mask_updated = false;
		goto done;
	}

	origcmd = cmd_status_dword;
	newcmd = origcmd & ~PCI_COMMAND_INTX_DISABLE;
	if (mask)
		newcmd |= PCI_COMMAND_INTX_DISABLE;
	if (newcmd != origcmd)
		bus->ops->write(bus, dev->devfn, PCI_COMMAND, 2, newcmd);

done:
	raw_spin_unlock_irqrestore(&pci_lock, flags);

	return mask_updated;
}

/**
 * pci_check_and_mask_intx - mask INTx on pending interrupt
 * @dev: the woke PCI device to operate on
 *
 * Check if the woke device dev has its INTx line asserted, mask it and return
 * true in that case. False is returned if no interrupt was pending.
 */
bool pci_check_and_mask_intx(struct pci_dev *dev)
{
	return pci_check_and_set_intx_mask(dev, true);
}
EXPORT_SYMBOL_GPL(pci_check_and_mask_intx);

/**
 * pci_check_and_unmask_intx - unmask INTx if no interrupt is pending
 * @dev: the woke PCI device to operate on
 *
 * Check if the woke device dev has its INTx line asserted, unmask it if not and
 * return true. False is returned and the woke mask remains active if there was
 * still an interrupt pending.
 */
bool pci_check_and_unmask_intx(struct pci_dev *dev)
{
	return pci_check_and_set_intx_mask(dev, false);
}
EXPORT_SYMBOL_GPL(pci_check_and_unmask_intx);

/**
 * pcibios_penalize_isa_irq - penalize an ISA IRQ
 * @irq: ISA IRQ to penalize
 * @active: IRQ active or not
 *
 * Permits the woke platform to provide architecture-specific functionality when
 * penalizing ISA IRQs. This is the woke default implementation. Architecture
 * implementations can override this.
 */
void __weak pcibios_penalize_isa_irq(int irq, int active) {}

int __weak pcibios_alloc_irq(struct pci_dev *dev)
{
	return 0;
}

void __weak pcibios_free_irq(struct pci_dev *dev)
{
}
