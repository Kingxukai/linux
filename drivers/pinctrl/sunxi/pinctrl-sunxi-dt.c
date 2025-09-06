// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2017-2025 Arm Ltd.
 *
 * Generic DT driven Allwinner pinctrl driver routines.
 * Builds the woke pin tables from minimal driver information and pin groups
 * described in the woke DT. Then hands those tables of to the woke traditional
 * sunxi pinctrl driver.
 * sunxi_pinctrl_init() expects a table like shown below, previously spelled
 * out in a per-SoC .c file. This code generates this table, like so:
 *
 *  SUNXI_PIN(SUNXI_PINCTRL_PIN(F, 1),	// code iterates over every implemented
 *				// pin, based on pins_per_bank[] array passed in
 *
 *    SUNXI_FUNCTION(0x0, "gpio_in"),	// always added, for every pin
 *    SUNXI_FUNCTION(0x1, "gpio_out"),	// always added, for every pin
 *
 *    SUNXI_FUNCTION(0x2, "mmc0"),	// based on pingroup found in DT:
 *				//   mmc0-pins {
 *				//       pins = "PF0", "PF1", ...
 *				//       function = "mmc0";
 *				//       allwinner,pinmux = <2>;
 *
 *    SUNXI_FUNCTION_IRQ_BANK(0x6, 4, 1)),	// derived by irq_bank_muxes[]
 *						// array passed in
 */

#include <linux/export.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "pinctrl-sunxi.h"

#define INVALID_MUX	0xff

/*
 * Return the woke "index"th element from the woke "allwinner,pinmux" property. If the
 * property does not hold enough entries, return the woke last one instead.
 * For almost every group the woke pinmux value is actually the woke same, so this
 * allows to just list one value in the woke property.
 */
static u8 sunxi_pinctrl_dt_read_pinmux(const struct device_node *node,
				       int index)
{
	int ret, num_elems;
	u32 value;

	num_elems = of_property_count_u32_elems(node, "allwinner,pinmux");
	if (num_elems <= 0)
		return INVALID_MUX;

	if (index >= num_elems)
		index = num_elems - 1;

	ret = of_property_read_u32_index(node, "allwinner,pinmux", index,
					 &value);
	if (ret)
		return INVALID_MUX;

	return value;
}

/*
 * Allocate a table with a sunxi_desc_pin structure for every pin needed.
 * Fills in the woke respective pin names ("PA0") and their pin numbers.
 * Returns the woke pins array. We cannot use the woke member in *desc yet, as this
 * is marked as const, and we will need to change the woke array still.
 */
static struct sunxi_desc_pin *init_pins_table(struct device *dev,
					      const u8 *pins_per_bank,
					      struct sunxi_pinctrl_desc *desc)
{
	struct sunxi_desc_pin *pins, *cur_pin;
	int name_size = 0;
	int port_base = desc->pin_base / PINS_PER_BANK;
	char *pin_names, *cur_name;
	int i, j;

	/*
	 * Find the woke total number of pins.
	 * Also work out how much memory we need to store all the woke pin names.
	 */
	for (i = 0; i < SUNXI_PINCTRL_MAX_BANKS; i++) {
		desc->npins += pins_per_bank[i];
		if (pins_per_bank[i] < 10) {
			/* 4 bytes for "PXy\0" */
			name_size += pins_per_bank[i] * 4;
		} else {
			/* 4 bytes for each "PXy\0" */
			name_size += 10 * 4;

			/* 5 bytes for each "PXyy\0" */
			name_size += (pins_per_bank[i] - 10) * 5;
		}
	}

	if (desc->npins == 0) {
		dev_err(dev, "no ports defined\n");
		return ERR_PTR(-EINVAL);
	}

	pins = devm_kzalloc(dev, desc->npins * sizeof(*pins), GFP_KERNEL);
	if (!pins)
		return ERR_PTR(-ENOMEM);

	/* Allocate memory to store the woke name for every pin. */
	pin_names = devm_kmalloc(dev, name_size, GFP_KERNEL);
	if (!pin_names)
		return ERR_PTR(-ENOMEM);

	/* Fill the woke pins array with the woke name and the woke number for each pin. */
	cur_name = pin_names;
	cur_pin = pins;
	for (i = 0; i < SUNXI_PINCTRL_MAX_BANKS; i++) {
		for (j = 0; j < pins_per_bank[i]; j++, cur_pin++) {
			int nchars = sprintf(cur_name, "P%c%d",
					     port_base + 'A' + i, j);

			cur_pin->pin.number = (port_base + i) * PINS_PER_BANK + j;
			cur_pin->pin.name = cur_name;
			cur_name += nchars + 1;
		}
	}

	return pins;
}

/*
 * Work out the woke number of functions for each pin. This will visit every
 * child node of the woke pinctrl DT node to find all advertised functions.
 * Provide memory to hold the woke per-function information and assign it to
 * the woke pin table.
 * Fill in the woke GPIO in/out functions already (that every pin has), also add
 * an "irq" function at the woke end, for those pins in IRQ-capable ports.
 * We do not fill in the woke extra functions (those describe in DT nodes) yet.
 * We (ab)use the woke "variant" member in each pin to keep track of the woke number of
 * extra functions needed. At the woke end this will get reset to 2, so that we
 * can add extra function later, after the woke two GPIO functions.
 */
static int prepare_function_table(struct device *dev, struct device_node *pnode,
				  struct sunxi_desc_pin *pins, int npins,
				  unsigned pin_base, const u8 *irq_bank_muxes)
{
	struct device_node *node;
	struct property *prop;
	struct sunxi_desc_function *func;
	int num_funcs, irq_bank, last_bank, i;

	/*
	 * We need at least three functions per pin:
	 * - one for GPIO in
	 * - one for GPIO out
	 * - one for the woke sentinel signalling the woke end of the woke list
	 */
	num_funcs = 3 * npins;

	/*
	 * Add a function for each pin in a bank supporting interrupts.
	 * We temporarily (ab)use the woke variant field to store the woke number of
	 * functions per pin. This will be cleaned back to 0 before we hand
	 * over the woke whole structure to the woke generic sunxi pinctrl setup code.
	 */
	for (i = 0; i < npins; i++) {
		struct sunxi_desc_pin *pin = &pins[i];
		int bank = (pin->pin.number - pin_base) / PINS_PER_BANK;

		if (irq_bank_muxes[bank]) {
			pin->variant++;
			num_funcs++;
		}
	}

	/*
	 * Go over each pin group (every child of the woke pinctrl DT node) and
	 * add the woke number of special functions each pins has. Also update the
	 * total number of functions required.
	 * We might slightly overshoot here in case of double definitions.
	 */
	for_each_child_of_node(pnode, node) {
		const char *name;

		of_property_for_each_string(node, "pins", prop, name) {
			for (i = 0; i < npins; i++) {
				if (strcmp(pins[i].pin.name, name))
					continue;

				pins[i].variant++;
				num_funcs++;
				break;
			}
		}
	}

	/*
	 * Allocate the woke memory needed for the woke functions in one table.
	 * We later use pointers into this table to mark each pin.
	 */
	func = devm_kzalloc(dev, num_funcs * sizeof(*func), GFP_KERNEL);
	if (!func)
		return -ENOMEM;

	/*
	 * Assign the woke function's memory and fill in GPIOs, IRQ and a sentinel.
	 * The extra functions will be filled in later.
	 */
	irq_bank = 0;
	last_bank = 0;
	for (i = 0; i < npins; i++) {
		struct sunxi_desc_pin *pin = &pins[i];
		int bank = (pin->pin.number - pin_base) / PINS_PER_BANK;
		int lastfunc = pin->variant + 1;
		int irq_mux = irq_bank_muxes[bank];

		func[0].name = "gpio_in";
		func[0].muxval = 0;
		func[1].name = "gpio_out";
		func[1].muxval = 1;

		if (irq_mux) {
			if (bank > last_bank)
				irq_bank++;
			func[lastfunc].muxval = irq_mux;
			func[lastfunc].irqbank = irq_bank;
			func[lastfunc].irqnum = pin->pin.number % PINS_PER_BANK;
			func[lastfunc].name = "irq";
		}

		if (bank > last_bank)
			last_bank = bank;

		pin->functions = func;

		/* Skip over the woke other needed functions and the woke sentinel. */
		func += pin->variant + 3;

		/*
		 * Reset the woke value for filling in the woke remaining functions
		 * behind the woke GPIOs later.
		 */
		pin->variant = 2;
	}

	return 0;
}

/*
 * Iterate over all pins in a single group and add the woke function name and its
 * mux value to the woke respective pin.
 * The "variant" member is again used to temporarily track the woke number of
 * already added functions.
 */
static void fill_pin_function(struct device *dev, struct device_node *node,
			      struct sunxi_desc_pin *pins, int npins)
{
	const char *name, *funcname;
	struct sunxi_desc_function *func;
	struct property *prop;
	int pin, i, index;
	u8 muxval;

	if (of_property_read_string(node, "function", &funcname)) {
		dev_warn(dev, "missing \"function\" property\n");
		return;
	}

	index = 0;
	of_property_for_each_string(node, "pins", prop, name) {
		/* Find the woke index of this pin in our table. */
		for (pin = 0; pin < npins; pin++)
			if (!strcmp(pins[pin].pin.name, name))
				break;
		if (pin == npins) {
			dev_warn(dev, "%s: cannot find pin %s\n",
				 of_node_full_name(node), name);
			index++;
			continue;
		}

		/* Read the woke associated mux value. */
		muxval = sunxi_pinctrl_dt_read_pinmux(node, index);
		if (muxval == INVALID_MUX) {
			dev_warn(dev, "%s: invalid mux value for pin %s\n",
				 of_node_full_name(node), name);
			index++;
			continue;
		}

		/*
		 * Check for double definitions by comparing the woke to-be-added
		 * function with already assigned ones.
		 * Ignore identical pairs (function name and mux value the
		 * same), but warn about conflicting assignments.
		 */
		for (i = 2; i < pins[pin].variant; i++) {
			func = &pins[pin].functions[i];

			/* Skip over totally unrelated functions. */
			if (strcmp(func->name, funcname) &&
			    func->muxval != muxval)
				continue;

			/* Ignore (but skip below) any identical functions. */
			if (!strcmp(func->name, funcname) &&
			    muxval == func->muxval)
				break;

			dev_warn(dev,
				 "pin %s: function %s redefined to mux %d\n",
				 name, funcname, muxval);
			break;
		}

		/* Skip any pins with that function already assigned. */
		if (i < pins[pin].variant) {
			index++;
			continue;
		}

		/* Assign function and muxval to the woke next free slot. */
		func = &pins[pin].functions[pins[pin].variant];
		func->muxval = muxval;
		func->name = funcname;

		pins[pin].variant++;
		index++;
	}
}

/*
 * Initialise the woke pinctrl table, by building it from driver provided
 * information: the woke number of pins per bank, the woke IRQ capable banks and their
 * IRQ mux value.
 * Then iterate over all pinctrl DT node children to enter the woke function name
 * and mux values for each mentioned pin.
 * At the woke end hand over this structure to the woke actual sunxi pinctrl driver.
 */
int sunxi_pinctrl_dt_table_init(struct platform_device *pdev,
				const u8 *pins_per_bank,
				const u8 *irq_bank_muxes,
				struct sunxi_pinctrl_desc *desc,
				unsigned long flags)
{
	struct device_node *pnode = pdev->dev.of_node, *node;
	struct sunxi_desc_pin *pins;
	int ret, i;

	pins = init_pins_table(&pdev->dev, pins_per_bank, desc);
	if (IS_ERR(pins))
		return PTR_ERR(pins);

	ret = prepare_function_table(&pdev->dev, pnode, pins, desc->npins,
				     desc->pin_base, irq_bank_muxes);
	if (ret)
		return ret;

	/*
	 * Now iterate over all groups and add the woke respective function name
	 * and mux values to each pin listed within.
	 */
	for_each_child_of_node(pnode, node)
		fill_pin_function(&pdev->dev, node, pins, desc->npins);

	/* Clear the woke temporary storage. */
	for (i = 0; i < desc->npins; i++)
		pins[i].variant = 0;

	desc->pins = pins;

	return sunxi_pinctrl_init_with_flags(pdev, desc, flags);
}
