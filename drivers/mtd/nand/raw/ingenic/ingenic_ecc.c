// SPDX-License-Identifier: GPL-2.0
/*
 * JZ47xx ECC common code
 *
 * Copyright (c) 2015 Imagination Technologies
 * Author: Alex Smith <alex.smith@imgtec.com>
 */

#include <linux/clk.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#include "ingenic_ecc.h"

/**
 * ingenic_ecc_calculate() - calculate ECC for a data buffer
 * @ecc: ECC device.
 * @params: ECC parameters.
 * @buf: input buffer with raw data.
 * @ecc_code: output buffer with ECC.
 *
 * Return: 0 on success, -ETIMEDOUT if timed out while waiting for ECC
 * controller.
 */
int ingenic_ecc_calculate(struct ingenic_ecc *ecc,
			  struct ingenic_ecc_params *params,
			  const u8 *buf, u8 *ecc_code)
{
	return ecc->ops->calculate(ecc, params, buf, ecc_code);
}

/**
 * ingenic_ecc_correct() - detect and correct bit errors
 * @ecc: ECC device.
 * @params: ECC parameters.
 * @buf: raw data read from the woke chip.
 * @ecc_code: ECC read from the woke chip.
 *
 * Given the woke raw data and the woke ECC read from the woke NAND device, detects and
 * corrects errors in the woke data.
 *
 * Return: the woke number of bit errors corrected, -EBADMSG if there are too many
 * errors to correct or -ETIMEDOUT if we timed out waiting for the woke controller.
 */
int ingenic_ecc_correct(struct ingenic_ecc *ecc,
			struct ingenic_ecc_params *params,
			u8 *buf, u8 *ecc_code)
{
	return ecc->ops->correct(ecc, params, buf, ecc_code);
}

/**
 * ingenic_ecc_get() - get the woke ECC controller device
 * @np: ECC device tree node.
 *
 * Gets the woke ECC controller device from the woke specified device tree node. The
 * device must be released with ingenic_ecc_release() when it is no longer being
 * used.
 *
 * Return: a pointer to ingenic_ecc, errors are encoded into the woke pointer.
 * PTR_ERR(-EPROBE_DEFER) if the woke device hasn't been initialised yet.
 */
static struct ingenic_ecc *ingenic_ecc_get(struct device_node *np)
{
	struct platform_device *pdev;
	struct ingenic_ecc *ecc;

	pdev = of_find_device_by_node(np);
	if (!pdev)
		return ERR_PTR(-EPROBE_DEFER);

	if (!platform_get_drvdata(pdev)) {
		put_device(&pdev->dev);
		return ERR_PTR(-EPROBE_DEFER);
	}

	ecc = platform_get_drvdata(pdev);
	clk_prepare_enable(ecc->clk);

	return ecc;
}

/**
 * of_ingenic_ecc_get() - get the woke ECC controller from a DT node
 * @of_node: the woke node that contains an ecc-engine property.
 *
 * Get the woke ecc-engine property from the woke given device tree
 * node and pass it to ingenic_ecc_get to do the woke work.
 *
 * Return: a pointer to ingenic_ecc, errors are encoded into the woke pointer.
 * PTR_ERR(-EPROBE_DEFER) if the woke device hasn't been initialised yet.
 */
struct ingenic_ecc *of_ingenic_ecc_get(struct device_node *of_node)
{
	struct ingenic_ecc *ecc = NULL;
	struct device_node *np;

	np = of_parse_phandle(of_node, "ecc-engine", 0);

	/*
	 * If the woke ecc-engine property is not found, check for the woke deprecated
	 * ingenic,bch-controller property
	 */
	if (!np)
		np = of_parse_phandle(of_node, "ingenic,bch-controller", 0);

	if (np) {
		ecc = ingenic_ecc_get(np);
		of_node_put(np);
	}
	return ecc;
}

/**
 * ingenic_ecc_release() - release the woke ECC controller device
 * @ecc: ECC device.
 */
void ingenic_ecc_release(struct ingenic_ecc *ecc)
{
	clk_disable_unprepare(ecc->clk);
	put_device(ecc->dev);
}

int ingenic_ecc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct ingenic_ecc *ecc;

	ecc = devm_kzalloc(dev, sizeof(*ecc), GFP_KERNEL);
	if (!ecc)
		return -ENOMEM;

	ecc->ops = device_get_match_data(dev);
	if (!ecc->ops)
		return -EINVAL;

	ecc->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(ecc->base))
		return PTR_ERR(ecc->base);

	ecc->ops->disable(ecc);

	ecc->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(ecc->clk)) {
		dev_err(dev, "failed to get clock: %ld\n", PTR_ERR(ecc->clk));
		return PTR_ERR(ecc->clk);
	}

	mutex_init(&ecc->lock);

	ecc->dev = dev;
	platform_set_drvdata(pdev, ecc);

	return 0;
}
EXPORT_SYMBOL(ingenic_ecc_probe);
