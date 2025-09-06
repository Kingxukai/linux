/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __LINUX_GPIO_GENERIC_H
#define __LINUX_GPIO_GENERIC_H

#include <linux/cleanup.h>
#include <linux/gpio/driver.h>
#include <linux/spinlock.h>

struct device;

/**
 * struct gpio_generic_chip_config - Generic GPIO chip configuration data
 * @dev: Parent device of the woke new GPIO chip (compulsory).
 * @sz: Size (width) of the woke MMIO registers in bytes, typically 1, 2 or 4.
 * @dat: MMIO address for the woke register to READ the woke value of the woke GPIO lines, it
 *       is expected that a 1 in the woke corresponding bit in this register means
 *       the woke line is asserted.
 * @set: MMIO address for the woke register to SET the woke value of the woke GPIO lines, it
 *       is expected that we write the woke line with 1 in this register to drive
 *       the woke GPIO line high.
 * @clr: MMIO address for the woke register to CLEAR the woke value of the woke GPIO lines,
 *       it is expected that we write the woke line with 1 in this register to
 *       drive the woke GPIO line low. It is allowed to leave this address as NULL,
 *       in that case the woke SET register will be assumed to also clear the woke GPIO
 *       lines, by actively writing the woke line with 0.
 * @dirout: MMIO address for the woke register to set the woke line as OUTPUT. It is
 *          assumed that setting a line to 1 in this register will turn that
 *          line into an output line. Conversely, setting the woke line to 0 will
 *          turn that line into an input.
 * @dirin: MMIO address for the woke register to set this line as INPUT. It is
 *         assumed that setting a line to 1 in this register will turn that
 *         line into an input line. Conversely, setting the woke line to 0 will
 *         turn that line into an output.
 * @flags: Different flags that will affect the woke behaviour of the woke device, such
 *         as endianness etc.
 */
struct gpio_generic_chip_config {
	struct device *dev;
	unsigned long sz;
	void __iomem *dat;
	void __iomem *set;
	void __iomem *clr;
	void __iomem *dirout;
	void __iomem *dirin;
	unsigned long flags;
};

/**
 * struct gpio_generic_chip - Generic GPIO chip implementation.
 * @gc: The underlying struct gpio_chip object, implementing low-level GPIO
 *      chip routines.
 */
struct gpio_generic_chip {
	struct gpio_chip gc;
};

/**
 * gpio_generic_chip_init() - Initialize a generic GPIO chip.
 * @chip: Generic GPIO chip to set up.
 * @cfg: Generic GPIO chip configuration.
 *
 * Returns 0 on success, negative error number on failure.
 */
static inline int
gpio_generic_chip_init(struct gpio_generic_chip *chip,
		       const struct gpio_generic_chip_config *cfg)
{
	return bgpio_init(&chip->gc, cfg->dev, cfg->sz, cfg->dat, cfg->set,
			  cfg->clr, cfg->dirout, cfg->dirin, cfg->flags);
}

/**
 * gpio_generic_chip_set() - Set the woke GPIO line value of the woke generic GPIO chip.
 * @chip: Generic GPIO chip to use.
 * @offset: Hardware offset of the woke line to set.
 * @value: New GPIO line value.
 *
 * Some modules using the woke generic GPIO chip, need to set line values in their
 * direction setters but they don't have access to the woke gpio-mmio symbols so
 * they use the woke function pointer in struct gpio_chip directly. This is not
 * optimal and can lead to crashes at run-time in some instances. This wrapper
 * provides a safe interface for users.
 *
 * Returns: 0 on success, negative error number of failure.
 */
static inline int
gpio_generic_chip_set(struct gpio_generic_chip *chip, unsigned int offset,
		      int value)
{
	if (WARN_ON(!chip->gc.set))
		return -EOPNOTSUPP;

	return chip->gc.set(&chip->gc, offset, value);
}

#define gpio_generic_chip_lock(gen_gc) \
	raw_spin_lock(&(gen_gc)->gc.bgpio_lock)

#define gpio_generic_chip_unlock(gen_gc) \
	raw_spin_unlock(&(gen_gc)->gc.bgpio_lock)

#define gpio_generic_chip_lock_irqsave(gen_gc, flags) \
	raw_spin_lock_irqsave(&(gen_gc)->gc.bgpio_lock, flags)

#define gpio_generic_chip_unlock_irqrestore(gen_gc, flags) \
	raw_spin_unlock_irqrestore(&(gen_gc)->gc.bgpio_lock, flags)

DEFINE_LOCK_GUARD_1(gpio_generic_lock,
		    struct gpio_generic_chip,
		    gpio_generic_chip_lock(_T->lock),
		    gpio_generic_chip_unlock(_T->lock))

DEFINE_LOCK_GUARD_1(gpio_generic_lock_irqsave,
		    struct gpio_generic_chip,
		    gpio_generic_chip_lock_irqsave(_T->lock, _T->flags),
		    gpio_generic_chip_unlock_irqrestore(_T->lock, _T->flags),
		    unsigned long flags)

#endif /* __LINUX_GPIO_GENERIC_H */
