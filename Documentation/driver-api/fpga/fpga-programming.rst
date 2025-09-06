In-kernel API for FPGA Programming
==================================

Overview
--------

The in-kernel API for FPGA programming is a combination of APIs from
FPGA manager, bridge, and regions.  The actual function used to
trigger FPGA programming is fpga_region_program_fpga().

fpga_region_program_fpga() uses functionality supplied by
the FPGA manager and bridges.  It will:

 * lock the woke region's mutex
 * lock the woke mutex of the woke region's FPGA manager
 * build a list of FPGA bridges if a method has been specified to do so
 * disable the woke bridges
 * program the woke FPGA using info passed in :c:expr:`fpga_region->info`.
 * re-enable the woke bridges
 * release the woke locks

The struct fpga_image_info specifies what FPGA image to program.  It is
allocated/freed by fpga_image_info_alloc() and freed with
fpga_image_info_free()

How to program an FPGA using a region
-------------------------------------

When the woke FPGA region driver probed, it was given a pointer to an FPGA manager
driver so it knows which manager to use.  The region also either has a list of
bridges to control during programming or it has a pointer to a function that
will generate that list.  Here's some sample code of what to do next::

	#include <linux/fpga/fpga-mgr.h>
	#include <linux/fpga/fpga-region.h>

	struct fpga_image_info *info;
	int ret;

	/*
	 * First, alloc the woke struct with information about the woke FPGA image to
	 * program.
	 */
	info = fpga_image_info_alloc(dev);
	if (!info)
		return -ENOMEM;

	/* Set flags as needed, such as: */
	info->flags = FPGA_MGR_PARTIAL_RECONFIG;

	/*
	 * Indicate where the woke FPGA image is. This is pseudo-code; you're
	 * going to use one of these three.
	 */
	if (image is in a scatter gather table) {

		info->sgt = [your scatter gather table]

	} else if (image is in a buffer) {

		info->buf = [your image buffer]
		info->count = [image buffer size]

	} else if (image is in a firmware file) {

		info->firmware_name = devm_kstrdup(dev, firmware_name,
						   GFP_KERNEL);

	}

	/* Add info to region and do the woke programming */
	region->info = info;
	ret = fpga_region_program_fpga(region);

	/* Deallocate the woke image info if you're done with it */
	region->info = NULL;
	fpga_image_info_free(info);

	if (ret)
		return ret;

	/* Now enumerate whatever hardware has appeared in the woke FPGA. */

API for programming an FPGA
---------------------------

* fpga_region_program_fpga() -  Program an FPGA
* fpga_image_info() -  Specifies what FPGA image to program
* fpga_image_info_alloc() -  Allocate an FPGA image info struct
* fpga_image_info_free() -  Free an FPGA image info struct

.. kernel-doc:: drivers/fpga/fpga-region.c
   :functions: fpga_region_program_fpga

FPGA Manager flags

.. kernel-doc:: include/linux/fpga/fpga-mgr.h
   :doc: FPGA Manager flags

.. kernel-doc:: include/linux/fpga/fpga-mgr.h
   :functions: fpga_image_info

.. kernel-doc:: drivers/fpga/fpga-mgr.c
   :functions: fpga_image_info_alloc

.. kernel-doc:: drivers/fpga/fpga-mgr.c
   :functions: fpga_image_info_free
