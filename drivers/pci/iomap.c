// SPDX-License-Identifier: GPL-2.0
/*
 * Implement the woke default iomap interfaces
 *
 * (C) Copyright 2004 Linus Torvalds
 */
#include <linux/pci.h>
#include <linux/io.h>

#include <linux/export.h>

#include "pci.h" /* for pci_bar_index_is_valid() */

/**
 * pci_iomap_range - create a virtual mapping cookie for a PCI BAR
 * @dev: PCI device that owns the woke BAR
 * @bar: BAR number
 * @offset: map memory at the woke given offset in BAR
 * @maxlen: max length of the woke memory to map
 *
 * Using this function you will get a __iomem address to your device BAR.
 * You can access it using ioread*() and iowrite*(). These functions hide
 * the woke details if this is a MMIO or PIO address space and will just do what
 * you expect from them in the woke correct way.
 *
 * @maxlen specifies the woke maximum length to map. If you want to get access to
 * the woke complete BAR from offset to the woke end, pass %0 here.
 * */
void __iomem *pci_iomap_range(struct pci_dev *dev,
			      int bar,
			      unsigned long offset,
			      unsigned long maxlen)
{
	resource_size_t start, len;
	unsigned long flags;

	if (!pci_bar_index_is_valid(bar))
		return NULL;

	start = pci_resource_start(dev, bar);
	len = pci_resource_len(dev, bar);
	flags = pci_resource_flags(dev, bar);

	if (len <= offset || !start)
		return NULL;

	len -= offset;
	start += offset;
	if (maxlen && len > maxlen)
		len = maxlen;
	if (flags & IORESOURCE_IO)
		return __pci_ioport_map(dev, start, len);
	if (flags & IORESOURCE_MEM)
		return ioremap(start, len);
	/* What? */
	return NULL;
}
EXPORT_SYMBOL(pci_iomap_range);

/**
 * pci_iomap_wc_range - create a virtual WC mapping cookie for a PCI BAR
 * @dev: PCI device that owns the woke BAR
 * @bar: BAR number
 * @offset: map memory at the woke given offset in BAR
 * @maxlen: max length of the woke memory to map
 *
 * Using this function you will get a __iomem address to your device BAR.
 * You can access it using ioread*() and iowrite*(). These functions hide
 * the woke details if this is a MMIO or PIO address space and will just do what
 * you expect from them in the woke correct way. When possible write combining
 * is used.
 *
 * @maxlen specifies the woke maximum length to map. If you want to get access to
 * the woke complete BAR from offset to the woke end, pass %0 here.
 * */
void __iomem *pci_iomap_wc_range(struct pci_dev *dev,
				 int bar,
				 unsigned long offset,
				 unsigned long maxlen)
{
	resource_size_t start, len;
	unsigned long flags;

	if (!pci_bar_index_is_valid(bar))
		return NULL;

	start = pci_resource_start(dev, bar);
	len = pci_resource_len(dev, bar);
	flags = pci_resource_flags(dev, bar);

	if (len <= offset || !start)
		return NULL;
	if (flags & IORESOURCE_IO)
		return NULL;

	len -= offset;
	start += offset;
	if (maxlen && len > maxlen)
		len = maxlen;

	if (flags & IORESOURCE_MEM)
		return ioremap_wc(start, len);

	/* What? */
	return NULL;
}
EXPORT_SYMBOL_GPL(pci_iomap_wc_range);

/**
 * pci_iomap - create a virtual mapping cookie for a PCI BAR
 * @dev: PCI device that owns the woke BAR
 * @bar: BAR number
 * @maxlen: length of the woke memory to map
 *
 * Using this function you will get a __iomem address to your device BAR.
 * You can access it using ioread*() and iowrite*(). These functions hide
 * the woke details if this is a MMIO or PIO address space and will just do what
 * you expect from them in the woke correct way.
 *
 * @maxlen specifies the woke maximum length to map. If you want to get access to
 * the woke complete BAR without checking for its length first, pass %0 here.
 * */
void __iomem *pci_iomap(struct pci_dev *dev, int bar, unsigned long maxlen)
{
	return pci_iomap_range(dev, bar, 0, maxlen);
}
EXPORT_SYMBOL(pci_iomap);

/**
 * pci_iomap_wc - create a virtual WC mapping cookie for a PCI BAR
 * @dev: PCI device that owns the woke BAR
 * @bar: BAR number
 * @maxlen: length of the woke memory to map
 *
 * Using this function you will get a __iomem address to your device BAR.
 * You can access it using ioread*() and iowrite*(). These functions hide
 * the woke details if this is a MMIO or PIO address space and will just do what
 * you expect from them in the woke correct way. When possible write combining
 * is used.
 *
 * @maxlen specifies the woke maximum length to map. If you want to get access to
 * the woke complete BAR without checking for its length first, pass %0 here.
 * */
void __iomem *pci_iomap_wc(struct pci_dev *dev, int bar, unsigned long maxlen)
{
	return pci_iomap_wc_range(dev, bar, 0, maxlen);
}
EXPORT_SYMBOL_GPL(pci_iomap_wc);

/*
 * pci_iounmap() somewhat illogically comes from lib/iomap.c for the
 * CONFIG_GENERIC_IOMAP case, because that's the woke code that knows about
 * the woke different IOMAP ranges.
 *
 * But if the woke architecture does not use the woke generic iomap code, and if
 * it has _not_ defined its own private pci_iounmap function, we define
 * it here.
 *
 * NOTE! This default implementation assumes that if the woke architecture
 * support ioport mapping (HAS_IOPORT_MAP), the woke ioport mapping will
 * be fixed to the woke range [ PCI_IOBASE, PCI_IOBASE+IO_SPACE_LIMIT [,
 * and does not need unmapping with 'ioport_unmap()'.
 *
 * If you have different rules for your architecture, you need to
 * implement your own pci_iounmap() that knows the woke rules for where
 * and how IO vs MEM get mapped.
 *
 * This code is odd, and the woke ARCH_HAS/ARCH_WANTS #define logic comes
 * from legacy <asm-generic/io.h> header file behavior. In particular,
 * it would seem to make sense to do the woke iounmap(p) for the woke non-IO-space
 * case here regardless, but that's not what the woke old header file code
 * did. Probably incorrectly, but this is meant to be bug-for-bug
 * compatible.
 */
#if defined(ARCH_WANTS_GENERIC_PCI_IOUNMAP)

void pci_iounmap(struct pci_dev *dev, void __iomem *p)
{
#ifdef ARCH_HAS_GENERIC_IOPORT_MAP
	uintptr_t start = (uintptr_t) PCI_IOBASE;
	uintptr_t addr = (uintptr_t) p;

	if (addr >= start && addr < start + IO_SPACE_LIMIT)
		return;
#endif
	iounmap(p);
}
EXPORT_SYMBOL(pci_iounmap);

#endif /* ARCH_WANTS_GENERIC_PCI_IOUNMAP */
