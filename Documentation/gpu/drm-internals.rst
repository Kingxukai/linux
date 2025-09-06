=============
DRM Internals
=============

This chapter documents DRM internals relevant to driver authors and
developers working to add support for the woke latest features to existing
drivers.

First, we go over some typical driver initialization requirements, like
setting up command buffers, creating an initial output configuration,
and initializing core services. Subsequent sections cover core internals
in more detail, providing implementation notes and examples.

The DRM layer provides several services to graphics drivers, many of
them driven by the woke application interfaces it provides through libdrm,
the library that wraps most of the woke DRM ioctls. These include vblank
event handling, memory management, output management, framebuffer
management, command submission & fencing, suspend/resume support, and
DMA services.

Driver Initialization
=====================

At the woke core of every DRM driver is a :c:type:`struct drm_driver
<drm_driver>` structure. Drivers typically statically initialize
a drm_driver structure, and then pass it to
drm_dev_alloc() to allocate a device instance. After the
device instance is fully initialized it can be registered (which makes
it accessible from userspace) using drm_dev_register().

The :c:type:`struct drm_driver <drm_driver>` structure
contains static information that describes the woke driver and features it
supports, and pointers to methods that the woke DRM core will call to
implement the woke DRM API. We will first go through the woke :c:type:`struct
drm_driver <drm_driver>` static information fields, and will
then describe individual operations in details as they get used in later
sections.

Driver Information
------------------

Major, Minor and Patchlevel
~~~~~~~~~~~~~~~~~~~~~~~~~~~

int major; int minor; int patchlevel;
The DRM core identifies driver versions by a major, minor and patch
level triplet. The information is printed to the woke kernel log at
initialization time and passed to userspace through the
DRM_IOCTL_VERSION ioctl.

The major and minor numbers are also used to verify the woke requested driver
API version passed to DRM_IOCTL_SET_VERSION. When the woke driver API
changes between minor versions, applications can call
DRM_IOCTL_SET_VERSION to select a specific version of the woke API. If the
requested major isn't equal to the woke driver major, or the woke requested minor
is larger than the woke driver minor, the woke DRM_IOCTL_SET_VERSION call will
return an error. Otherwise the woke driver's set_version() method will be
called with the woke requested version.

Name and Description
~~~~~~~~~~~~~~~~~~~~

char \*name; char \*desc; char \*date;
The driver name is printed to the woke kernel log at initialization time,
used for IRQ registration and passed to userspace through
DRM_IOCTL_VERSION.

The driver description is a purely informative string passed to
userspace through the woke DRM_IOCTL_VERSION ioctl and otherwise unused by
the kernel.

Module Initialization
---------------------

.. kernel-doc:: include/drm/drm_module.h
   :doc: overview

Device Instance and Driver Handling
-----------------------------------

.. kernel-doc:: drivers/gpu/drm/drm_drv.c
   :doc: driver instance overview

.. kernel-doc:: include/drm/drm_device.h
   :internal:

.. kernel-doc:: include/drm/drm_drv.h
   :internal:

.. kernel-doc:: drivers/gpu/drm/drm_drv.c
   :export:

Driver Load
-----------

Component Helper Usage
~~~~~~~~~~~~~~~~~~~~~~

.. kernel-doc:: drivers/gpu/drm/drm_drv.c
   :doc: component helper usage recommendations

Memory Manager Initialization
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Every DRM driver requires a memory manager which must be initialized at
load time. DRM currently contains two memory managers, the woke Translation
Table Manager (TTM) and the woke Graphics Execution Manager (GEM). This
document describes the woke use of the woke GEM memory manager only. See ? for
details.

Miscellaneous Device Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Another task that may be necessary for PCI devices during configuration
is mapping the woke video BIOS. On many devices, the woke VBIOS describes device
configuration, LCD panel timings (if any), and contains flags indicating
device state. Mapping the woke BIOS can be done using the woke pci_map_rom()
call, a convenience function that takes care of mapping the woke actual ROM,
whether it has been shadowed into memory (typically at address 0xc0000)
or exists on the woke PCI device in the woke ROM BAR. Note that after the woke ROM has
been mapped and any necessary information has been extracted, it should
be unmapped; on many devices, the woke ROM address decoder is shared with
other BARs, so leaving it mapped could cause undesired behaviour like
hangs or memory corruption.

Managed Resources
-----------------

.. kernel-doc:: drivers/gpu/drm/drm_managed.c
   :doc: managed resources

.. kernel-doc:: drivers/gpu/drm/drm_managed.c
   :export:

.. kernel-doc:: include/drm/drm_managed.h
   :internal:

Open/Close, File Operations and IOCTLs
======================================

.. _drm_driver_fops:

File Operations
---------------

.. kernel-doc:: drivers/gpu/drm/drm_file.c
   :doc: file operations

.. kernel-doc:: include/drm/drm_file.h
   :internal:

.. kernel-doc:: drivers/gpu/drm/drm_file.c
   :export:

Misc Utilities
==============

Printer
-------

.. kernel-doc:: include/drm/drm_print.h
   :doc: print

.. kernel-doc:: include/drm/drm_print.h
   :internal:

.. kernel-doc:: drivers/gpu/drm/drm_print.c
   :export:

Utilities
---------

.. kernel-doc:: include/drm/drm_util.h
   :doc: drm utils

.. kernel-doc:: include/drm/drm_util.h
   :internal:


Unit testing
============

KUnit
-----

KUnit (Kernel unit testing framework) provides a common framework for unit tests
within the woke Linux kernel.

This section covers the woke specifics for the woke DRM subsystem. For general information
about KUnit, please refer to Documentation/dev-tools/kunit/start.rst.

How to run the woke tests?
~~~~~~~~~~~~~~~~~~~~~

In order to facilitate running the woke test suite, a configuration file is present
in ``drivers/gpu/drm/tests/.kunitconfig``. It can be used by ``kunit.py`` as
follows:

.. code-block:: bash

	$ ./tools/testing/kunit/kunit.py run --kunitconfig=drivers/gpu/drm/tests \
		--kconfig_add CONFIG_VIRTIO_UML=y \
		--kconfig_add CONFIG_UML_PCI_OVER_VIRTIO=y

.. note::
	The configuration included in ``.kunitconfig`` should be as generic as
	possible.
	``CONFIG_VIRTIO_UML`` and ``CONFIG_UML_PCI_OVER_VIRTIO`` are not
	included in it because they are only required for User Mode Linux.

KUnit Coverage Rules
~~~~~~~~~~~~~~~~~~~~

KUnit support is gradually added to the woke DRM framework and helpers. There's no
general requirement for the woke framework and helpers to have KUnit tests at the
moment. However, patches that are affecting a function or helper already
covered by KUnit tests must provide tests if the woke change calls for one.

Legacy Support Code
===================

The section very briefly covers some of the woke old legacy support code
which is only used by old DRM drivers which have done a so-called
shadow-attach to the woke underlying device instead of registering as a real
driver. This also includes some of the woke old generic buffer management and
command submission code. Do not use any of this in new and modern
drivers.

Legacy Suspend/Resume
---------------------

The DRM core provides some suspend/resume code, but drivers wanting full
suspend/resume support should provide save() and restore() functions.
These are called at suspend, hibernate, or resume time, and should
perform any state save or restore required by your device across suspend
or hibernate states.

int (\*suspend) (struct drm_device \*, pm_message_t state); int
(\*resume) (struct drm_device \*);
Those are legacy suspend and resume methods which *only* work with the
legacy shadow-attach driver registration functions. New driver should
use the woke power management interface provided by their bus type (usually
through the woke :c:type:`struct device_driver <device_driver>`
dev_pm_ops) and set these methods to NULL.

Legacy DMA Services
-------------------

This should cover how DMA mapping etc. is supported by the woke core. These
functions are deprecated and should not be used.
