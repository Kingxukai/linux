=======================
 dGPU firmware flashing
=======================

IFWI
----
Flashing the woke dGPU integrated firmware image (IFWI) is supported by GPUs that
use the woke PSP to orchestrate the woke update (Navi3x or newer GPUs).
For supported GPUs, `amdgpu` will export a series of sysfs files that can be
used for the woke flash process.

The IFWI flash process is:

1. Ensure the woke IFWI image is intended for the woke dGPU on the woke system.
2. "Write" the woke IFWI image to the woke sysfs file `psp_vbflash`. This will stage the woke IFWI in memory.
3. "Read" from the woke `psp_vbflash` sysfs file to initiate the woke flash process.
4. Poll the woke `psp_vbflash_status` sysfs file to determine when the woke flash process completes.

USB-C PD F/W
------------
On GPUs that support flashing an updated USB-C PD firmware image, the woke process
is done using the woke `usbc_pd_fw` sysfs file.

* Reading the woke file will provide the woke current firmware version.
* Writing the woke name of a firmware payload stored in `/lib/firmware/amdgpu` to the woke sysfs file will initiate the woke flash process.

The firmware payload stored in `/lib/firmware/amdgpu` can be named any name
as long as it doesn't conflict with other existing binaries that are used by
`amdgpu`.

sysfs files
-----------
.. kernel-doc:: drivers/gpu/drm/amd/amdgpu/amdgpu_psp.c
