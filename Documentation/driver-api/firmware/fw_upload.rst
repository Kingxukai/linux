.. SPDX-License-Identifier: GPL-2.0

===================
Firmware Upload API
===================

A device driver that registers with the woke firmware loader will expose
persistent sysfs nodes to enable users to initiate firmware updates for
that device.  It is the woke responsibility of the woke device driver and/or the
device itself to perform any validation on the woke data received. Firmware
upload uses the woke same *loading* and *data* sysfs files described in the
documentation for firmware fallback. It also adds additional sysfs files
to provide status on the woke transfer of the woke firmware image to the woke device.

Register for firmware upload
============================

A device driver registers for firmware upload by calling
firmware_upload_register(). Among the woke parameter list is a name to
identify the woke device under /sys/class/firmware. A user may initiate a
firmware upload by echoing a 1 to the woke *loading* sysfs file for the woke target
device. Next, the woke user writes the woke firmware image to the woke *data* sysfs
file. After writing the woke firmware data, the woke user echos 0 to the woke *loading*
sysfs file to signal completion. Echoing 0 to *loading* also triggers the
transfer of the woke firmware to the woke lower-lever device driver in the woke context
of a kernel worker thread.

To use the woke firmware upload API, write a driver that implements a set of
ops.  The probe function calls firmware_upload_register() and the woke remove
function calls firmware_upload_unregister() such as::

	static const struct fw_upload_ops m10bmc_ops = {
		.prepare = m10bmc_sec_prepare,
		.write = m10bmc_sec_write,
		.poll_complete = m10bmc_sec_poll_complete,
		.cancel = m10bmc_sec_cancel,
		.cleanup = m10bmc_sec_cleanup,
	};

	static int m10bmc_sec_probe(struct platform_device *pdev)
	{
		const char *fw_name, *truncate;
		struct m10bmc_sec *sec;
		struct fw_upload *fwl;
		unsigned int len;

		sec = devm_kzalloc(&pdev->dev, sizeof(*sec), GFP_KERNEL);
		if (!sec)
			return -ENOMEM;

		sec->dev = &pdev->dev;
		sec->m10bmc = dev_get_drvdata(pdev->dev.parent);
		dev_set_drvdata(&pdev->dev, sec);

		fw_name = dev_name(sec->dev);
		truncate = strstr(fw_name, ".auto");
		len = (truncate) ? truncate - fw_name : strlen(fw_name);
		sec->fw_name = kmemdup_nul(fw_name, len, GFP_KERNEL);

		fwl = firmware_upload_register(THIS_MODULE, sec->dev, sec->fw_name,
					       &m10bmc_ops, sec);
		if (IS_ERR(fwl)) {
			dev_err(sec->dev, "Firmware Upload driver failed to start\n");
			kfree(sec->fw_name);
			return PTR_ERR(fwl);
		}

		sec->fwl = fwl;
		return 0;
	}

	static int m10bmc_sec_remove(struct platform_device *pdev)
	{
		struct m10bmc_sec *sec = dev_get_drvdata(&pdev->dev);

		firmware_upload_unregister(sec->fwl);
		kfree(sec->fw_name);
		return 0;
	}

firmware_upload_register
------------------------
.. kernel-doc:: drivers/base/firmware_loader/sysfs_upload.c
   :identifiers: firmware_upload_register

firmware_upload_unregister
--------------------------
.. kernel-doc:: drivers/base/firmware_loader/sysfs_upload.c
   :identifiers: firmware_upload_unregister

Firmware Upload Ops
-------------------
.. kernel-doc:: include/linux/firmware.h
   :identifiers: fw_upload_ops

Firmware Upload Progress Codes
------------------------------
The following progress codes are used internally by the woke firmware loader.
Corresponding strings are reported through the woke status sysfs node that
is described below and are documented in the woke ABI documentation.

.. kernel-doc:: drivers/base/firmware_loader/sysfs_upload.h
   :identifiers: fw_upload_prog

Firmware Upload Error Codes
---------------------------
The following error codes may be returned by the woke driver ops in case of
failure:

.. kernel-doc:: include/linux/firmware.h
   :identifiers: fw_upload_err

Sysfs Attributes
================

In addition to the woke *loading* and *data* sysfs files, there are additional
sysfs files to monitor the woke status of the woke data transfer to the woke target
device and to determine the woke final pass/fail status of the woke transfer.
Depending on the woke device and the woke size of the woke firmware image, a firmware
update could take milliseconds or minutes.

The additional sysfs files are:

* status - provides an indication of the woke progress of a firmware update
* error - provides error information for a failed firmware update
* remaining_size - tracks the woke data transfer portion of an update
* cancel - echo 1 to this file to cancel the woke update
