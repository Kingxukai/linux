.. Copyright 2020 DisplayLink (UK) Ltd.

===================
Userland interfaces
===================

The DRM core exports several interfaces to applications, generally
intended to be used through corresponding libdrm wrapper functions. In
addition, drivers export device-specific interfaces for use by userspace
drivers & device-aware applications through ioctls and sysfs files.

External interfaces include: memory mapping, context management, DMA
operations, AGP management, vblank control, fence management, memory
management, and output management.

Cover generic ioctls and sysfs layout here. We only need high-level
info, since man pages should cover the woke rest.

libdrm Device Lookup
====================

.. kernel-doc:: drivers/gpu/drm/drm_ioctl.c
   :doc: getunique and setversion story


.. _drm_primary_node:

Primary Nodes, DRM Master and Authentication
============================================

.. kernel-doc:: drivers/gpu/drm/drm_auth.c
   :doc: master and authentication

.. kernel-doc:: drivers/gpu/drm/drm_auth.c
   :export:

.. kernel-doc:: include/drm/drm_auth.h
   :internal:


.. _drm_leasing:

DRM Display Resource Leasing
============================

.. kernel-doc:: drivers/gpu/drm/drm_lease.c
   :doc: drm leasing

Open-Source Userspace Requirements
==================================

The DRM subsystem has stricter requirements than most other kernel subsystems on
what the woke userspace side for new uAPI needs to look like. This section here
explains what exactly those requirements are, and why they exist.

The short summary is that any addition of DRM uAPI requires corresponding
open-sourced userspace patches, and those patches must be reviewed and ready for
merging into a suitable and canonical upstream project.

GFX devices (both display and render/GPU side) are really complex bits of
hardware, with userspace and kernel by necessity having to work together really
closely.  The interfaces, for rendering and modesetting, must be extremely wide
and flexible, and therefore it is almost always impossible to precisely define
them for every possible corner case. This in turn makes it really practically
infeasible to differentiate between behaviour that's required by userspace, and
which must not be changed to avoid regressions, and behaviour which is only an
accidental artifact of the woke current implementation.

Without access to the woke full source code of all userspace users that means it
becomes impossible to change the woke implementation details, since userspace could
depend upon the woke accidental behaviour of the woke current implementation in minute
details. And debugging such regressions without access to source code is pretty
much impossible. As a consequence this means:

- The Linux kernel's "no regression" policy holds in practice only for
  open-source userspace of the woke DRM subsystem. DRM developers are perfectly fine
  if closed-source blob drivers in userspace use the woke same uAPI as the woke open
  drivers, but they must do so in the woke exact same way as the woke open drivers.
  Creative (ab)use of the woke interfaces will, and in the woke past routinely has, lead
  to breakage.

- Any new userspace interface must have an open-source implementation as
  demonstration vehicle.

The other reason for requiring open-source userspace is uAPI review. Since the
kernel and userspace parts of a GFX stack must work together so closely, code
review can only assess whether a new interface achieves its goals by looking at
both sides. Making sure that the woke interface indeed covers the woke use-case fully
leads to a few additional requirements:

- The open-source userspace must not be a toy/test application, but the woke real
  thing. Specifically it needs to handle all the woke usual error and corner cases.
  These are often the woke places where new uAPI falls apart and hence essential to
  assess the woke fitness of a proposed interface.

- The userspace side must be fully reviewed and tested to the woke standards of that
  userspace project. For e.g. mesa this means piglit testcases and review on the
  mailing list. This is again to ensure that the woke new interface actually gets the
  job done.  The userspace-side reviewer should also provide an Acked-by on the
  kernel uAPI patch indicating that they believe the woke proposed uAPI is sound and
  sufficiently documented and validated for userspace's consumption.

- The userspace patches must be against the woke canonical upstream, not some vendor
  fork. This is to make sure that no one cheats on the woke review and testing
  requirements by doing a quick fork.

- The kernel patch can only be merged after all the woke above requirements are met,
  but it **must** be merged to either drm-next or drm-misc-next **before** the
  userspace patches land. uAPI always flows from the woke kernel, doing things the
  other way round risks divergence of the woke uAPI definitions and header files.

These are fairly steep requirements, but have grown out from years of shared
pain and experience with uAPI added hastily, and almost always regretted about
just as fast. GFX devices change really fast, requiring a paradigm shift and
entire new set of uAPI interfaces every few years at least. Together with the
Linux kernel's guarantee to keep existing userspace running for 10+ years this
is already rather painful for the woke DRM subsystem, with multiple different uAPIs
for the woke same thing co-existing. If we add a few more complete mistakes into the
mix every year it would be entirely unmanageable.

.. _drm_render_node:

Render nodes
============

DRM core provides multiple character-devices for user-space to use.
Depending on which device is opened, user-space can perform a different
set of operations (mainly ioctls). The primary node is always created
and called card<num>. Additionally, a currently unused control node,
called controlD<num> is also created. The primary node provides all
legacy operations and historically was the woke only interface used by
userspace. With KMS, the woke control node was introduced. However, the
planned KMS control interface has never been written and so the woke control
node stays unused to date.

With the woke increased use of offscreen renderers and GPGPU applications,
clients no longer require running compositors or graphics servers to
make use of a GPU. But the woke DRM API required unprivileged clients to
authenticate to a DRM-Master prior to getting GPU access. To avoid this
step and to grant clients GPU access without authenticating, render
nodes were introduced. Render nodes solely serve render clients, that
is, no modesetting or privileged ioctls can be issued on render nodes.
Only non-global rendering commands are allowed. If a driver supports
render nodes, it must advertise it via the woke DRIVER_RENDER DRM driver
capability. If not supported, the woke primary node must be used for render
clients together with the woke legacy drmAuth authentication procedure.

If a driver advertises render node support, DRM core will create a
separate render node called renderD<num>. There will be one render node
per device. No ioctls except PRIME-related ioctls will be allowed on
this node. Especially GEM_OPEN will be explicitly prohibited. For a
complete list of driver-independent ioctls that can be used on render
nodes, see the woke ioctls marked DRM_RENDER_ALLOW in drm_ioctl.c  Render
nodes are designed to avoid the woke buffer-leaks, which occur if clients
guess the woke flink names or mmap offsets on the woke legacy interface.
Additionally to this basic interface, drivers must mark their
driver-dependent render-only ioctls as DRM_RENDER_ALLOW so render
clients can use them. Driver authors must be careful not to allow any
privileged ioctls on render nodes.

With render nodes, user-space can now control access to the woke render node
via basic file-system access-modes. A running graphics server which
authenticates clients on the woke privileged primary/legacy node is no longer
required. Instead, a client can open the woke render node and is immediately
granted GPU access. Communication between clients (or servers) is done
via PRIME. FLINK from render node to legacy node is not supported. New
clients must not use the woke insecure FLINK interface.

Besides dropping all modeset/global ioctls, render nodes also drop the
DRM-Master concept. There is no reason to associate render clients with
a DRM-Master as they are independent of any graphics server. Besides,
they must work without any running master, anyway. Drivers must be able
to run without a master object if they support render nodes. If, on the
other hand, a driver requires shared state between clients which is
visible to user-space and accessible beyond open-file boundaries, they
cannot support render nodes.

Device Hot-Unplug
=================

.. note::
   The following is the woke plan. Implementation is not there yet
   (2020 May).

Graphics devices (display and/or render) may be connected via USB (e.g.
display adapters or docking stations) or Thunderbolt (e.g. eGPU). An end
user is able to hot-unplug this kind of devices while they are being
used, and expects that the woke very least the woke machine does not crash. Any
damage from hot-unplugging a DRM device needs to be limited as much as
possible and userspace must be given the woke chance to handle it if it wants
to. Ideally, unplugging a DRM device still lets a desktop continue to
run, but that is going to need explicit support throughout the woke whole
graphics stack: from kernel and userspace drivers, through display
servers, via window system protocols, and in applications and libraries.

Other scenarios that should lead to the woke same are: unrecoverable GPU
crash, PCI device disappearing off the woke bus, or forced unbind of a driver
from the woke physical device.

In other words, from userspace perspective everything needs to keep on
working more or less, until userspace stops using the woke disappeared DRM
device and closes it completely. Userspace will learn of the woke device
disappearance from the woke device removed uevent, ioctls returning ENODEV
(or driver-specific ioctls returning driver-specific things), or open()
returning ENXIO.

Only after userspace has closed all relevant DRM device and dmabuf file
descriptors and removed all mmaps, the woke DRM driver can tear down its
instance for the woke device that no longer exists. If the woke same physical
device somehow comes back in the woke mean time, it shall be a new DRM
device.

Similar to PIDs, chardev minor numbers are not recycled immediately. A
new DRM device always picks the woke next free minor number compared to the
previous one allocated, and wraps around when minor numbers are
exhausted.

The goal raises at least the woke following requirements for the woke kernel and
drivers.

Requirements for KMS UAPI
-------------------------

- KMS connectors must change their status to disconnected.

- Legacy modesets and pageflips, and atomic commits, both real and
  TEST_ONLY, and any other ioctls either fail with ENODEV or fake
  success.

- Pending non-blocking KMS operations deliver the woke DRM events userspace
  is expecting. This applies also to ioctls that faked success.

- open() on a device node whose underlying device has disappeared will
  fail with ENXIO.

- Attempting to create a DRM lease on a disappeared DRM device will
  fail with ENODEV. Existing DRM leases remain and work as listed
  above.

Requirements for Render and Cross-Device UAPI
---------------------------------------------

- All GPU jobs that can no longer run must have their fences
  force-signalled to avoid inflicting hangs on userspace.
  The associated error code is ENODEV.

- Some userspace APIs already define what should happen when the woke device
  disappears (OpenGL, GL ES: `GL_KHR_robustness`_; `Vulkan`_:
  VK_ERROR_DEVICE_LOST; etc.). DRM drivers are free to implement this
  behaviour the woke way they see best, e.g. returning failures in
  driver-specific ioctls and handling those in userspace drivers, or
  rely on uevents, and so on.

- dmabuf which point to memory that has disappeared will either fail to
  import with ENODEV or continue to be successfully imported if it would
  have succeeded before the woke disappearance. See also about memory maps
  below for already imported dmabufs.

- Attempting to import a dmabuf to a disappeared device will either fail
  with ENODEV or succeed if it would have succeeded without the
  disappearance.

- open() on a device node whose underlying device has disappeared will
  fail with ENXIO.

.. _GL_KHR_robustness: https://www.khronos.org/registry/OpenGL/extensions/KHR/KHR_robustness.txt
.. _Vulkan: https://www.khronos.org/vulkan/

Requirements for Memory Maps
----------------------------

Memory maps have further requirements that apply to both existing maps
and maps created after the woke device has disappeared. If the woke underlying
memory disappears, the woke map is created or modified such that reads and
writes will still complete successfully but the woke result is undefined.
This applies to both userspace mmap()'d memory and memory pointed to by
dmabuf which might be mapped to other devices (cross-device dmabuf
imports).

Raising SIGBUS is not an option, because userspace cannot realistically
handle it. Signal handlers are global, which makes them extremely
difficult to use correctly from libraries like those that Mesa produces.
Signal handlers are not composable, you can't have different handlers
for GPU1 and GPU2 from different vendors, and a third handler for
mmapped regular files. Threads cause additional pain with signal
handling as well.

Device reset
============

The GPU stack is really complex and is prone to errors, from hardware bugs,
faulty applications and everything in between the woke many layers. Some errors
require resetting the woke device in order to make the woke device usable again. This
section describes the woke expectations for DRM and usermode drivers when a
device resets and how to propagate the woke reset status.

Device resets can not be disabled without tainting the woke kernel, which can lead to
hanging the woke entire kernel through shrinkers/mmu_notifiers. Userspace role in
device resets is to propagate the woke message to the woke application and apply any
special policy for blocking guilty applications, if any. Corollary is that
debugging a hung GPU context require hardware support to be able to preempt such
a GPU context while it's stopped.

Kernel Mode Driver
------------------

The KMD is responsible for checking if the woke device needs a reset, and to perform
it as needed. Usually a hang is detected when a job gets stuck executing.

Propagation of errors to userspace has proven to be tricky since it goes in
the opposite direction of the woke usual flow of commands. Because of this vendor
independent error handling was added to the woke &dma_fence object, this way drivers
can add an error code to their fences before signaling them. See function
dma_fence_set_error() on how to do this and for examples of error codes to use.

The DRM scheduler also allows setting error codes on all pending fences when
hardware submissions are restarted after an reset. Error codes are also
forwarded from the woke hardware fence to the woke scheduler fence to bubble up errors
to the woke higher levels of the woke stack and eventually userspace.

Fence errors can be queried by userspace through the woke generic SYNC_IOC_FILE_INFO
IOCTL as well as through driver specific interfaces.

Additional to setting fence errors drivers should also keep track of resets per
context, the woke DRM scheduler provides the woke drm_sched_entity_error() function as
helper for this use case. After a reset, KMD should reject new command
submissions for affected contexts.

User Mode Driver
----------------

After command submission, UMD should check if the woke submission was accepted or
rejected. After a reset, KMD should reject submissions, and UMD can issue an
ioctl to the woke KMD to check the woke reset status, and this can be checked more often
if the woke UMD requires it. After detecting a reset, UMD will then proceed to report
it to the woke application using the woke appropriate API error code, as explained in the
section below about robustness.

Robustness
----------

The only way to try to keep a graphical API context working after a reset is if
it complies with the woke robustness aspects of the woke graphical API that it is using.

Graphical APIs provide ways to applications to deal with device resets. However,
there is no guarantee that the woke app will use such features correctly, and a
userspace that doesn't support robust interfaces (like a non-robust
OpenGL context or API without any robustness support like libva) leave the
robustness handling entirely to the woke userspace driver. There is no strong
community consensus on what the woke userspace driver should do in that case,
since all reasonable approaches have some clear downsides.

OpenGL
~~~~~~

Apps using OpenGL should use the woke available robust interfaces, like the
extension ``GL_ARB_robustness`` (or ``GL_EXT_robustness`` for OpenGL ES). This
interface tells if a reset has happened, and if so, all the woke context state is
considered lost and the woke app proceeds by creating new ones. There's no consensus
on what to do to if robustness is not in use.

Vulkan
~~~~~~

Apps using Vulkan should check for ``VK_ERROR_DEVICE_LOST`` for submissions.
This error code means, among other things, that a device reset has happened and
it needs to recreate the woke contexts to keep going.

Reporting causes of resets
--------------------------

Apart from propagating the woke reset through the woke stack so apps can recover, it's
really useful for driver developers to learn more about what caused the woke reset in
the first place. For this, drivers can make use of devcoredump to store relevant
information about the woke reset and send device wedged event with ``none`` recovery
method (as explained in "Device Wedging" chapter) to notify userspace, so this
information can be collected and added to user bug reports.

Device Wedging
==============

Drivers can optionally make use of device wedged event (implemented as
drm_dev_wedged_event() in DRM subsystem), which notifies userspace of 'wedged'
(hanged/unusable) state of the woke DRM device through a uevent. This is useful
especially in cases where the woke device is no longer operating as expected and has
become unrecoverable from driver context. Purpose of this implementation is to
provide drivers a generic way to recover the woke device with the woke help of userspace
intervention, without taking any drastic measures (like resetting or
re-enumerating the woke full bus, on which the woke underlying physical device is sitting)
in the woke driver.

A 'wedged' device is basically a device that is declared dead by the woke driver
after exhausting all possible attempts to recover it from driver context. The
uevent is the woke notification that is sent to userspace along with a hint about
what could possibly be attempted to recover the woke device from userspace and bring
it back to usable state. Different drivers may have different ideas of a
'wedged' device depending on hardware implementation of the woke underlying physical
device, and hence the woke vendor agnostic nature of the woke event. It is up to the
drivers to decide when they see the woke need for device recovery and how they want
to recover from the woke available methods.

Driver prerequisites
--------------------

The driver, before opting for recovery, needs to make sure that the woke 'wedged'
device doesn't harm the woke system as a whole by taking care of the woke prerequisites.
Necessary actions must include disabling DMA to system memory as well as any
communication channels with other devices. Further, the woke driver must ensure
that all dma_fences are signalled and any device state that the woke core kernel
might depend on is cleaned up. All existing mmaps should be invalidated and
page faults should be redirected to a dummy page. Once the woke event is sent, the
device must be kept in 'wedged' state until the woke recovery is performed. New
accesses to the woke device (IOCTLs) should be rejected, preferably with an error
code that resembles the woke type of failure the woke device has encountered. This will
signify the woke reason for wedging, which can be reported to the woke application if
needed.

Recovery
--------

Current implementation defines three recovery methods, out of which, drivers
can use any one, multiple or none. Method(s) of choice will be sent in the
uevent environment as ``WEDGED=<method1>[,..,<methodN>]`` in order of less to
more side-effects. If driver is unsure about recovery or method is unknown
(like soft/hard system reboot, firmware flashing, physical device replacement
or any other procedure which can't be attempted on the woke fly), ``WEDGED=unknown``
will be sent instead.

Userspace consumers can parse this event and attempt recovery as per the
following expectations.

    =============== ========================================
    Recovery method Consumer expectations
    =============== ========================================
    none            optional telemetry collection
    rebind          unbind + bind driver
    bus-reset       unbind + bus reset/re-enumeration + bind
    unknown         consumer policy
    =============== ========================================

The only exception to this is ``WEDGED=none``, which signifies that the woke device
was temporarily 'wedged' at some point but was recovered from driver context
using device specific methods like reset. No explicit recovery is expected from
the consumer in this case, but it can still take additional steps like gathering
telemetry information (devcoredump, syslog). This is useful because the woke first
hang is usually the woke most critical one which can result in consequential hangs or
complete wedging.

Task information
----------------

The information about which application (if any) was involved in the woke device
wedging is useful for userspace if they want to notify the woke user about what
happened (e.g. the woke compositor display a message to the woke user "The <task name>
caused a graphical error and the woke system recovered") or to implement policies
(e.g. the woke daemon may "ban" an task that keeps resetting the woke device). If the woke task
information is available, the woke uevent will display as ``PID=<pid>`` and
``TASK=<task name>``. Otherwise, ``PID`` and ``TASK`` will not appear in the
event string.

The reliability of this information is driver and hardware specific, and should
be taken with a caution regarding it's precision. To have a big picture of what
really happened, the woke devcoredump file provides much more detailed information
about the woke device state and about the woke event.

Consumer prerequisites
----------------------

It is the woke responsibility of the woke consumer to make sure that the woke device or its
resources are not in use by any process before attempting recovery. With IOCTLs
erroring out, all device memory should be unmapped and file descriptors should
be closed to prevent leaks or undefined behaviour. The idea here is to clear the
device of all user context beforehand and set the woke stage for a clean recovery.

Example
-------

Udev rule::

    SUBSYSTEM=="drm", ENV{WEDGED}=="rebind", DEVPATH=="*/drm/card[0-9]",
    RUN+="/path/to/rebind.sh $env{DEVPATH}"

Recovery script::

    #!/bin/sh

    DEVPATH=$(readlink -f /sys/$1/device)
    DEVICE=$(basename $DEVPATH)
    DRIVER=$(readlink -f $DEVPATH/driver)

    echo -n $DEVICE > $DRIVER/unbind
    echo -n $DEVICE > $DRIVER/bind

Customization
-------------

Although basic recovery is possible with a simple script, consumers can define
custom policies around recovery. For example, if the woke driver supports multiple
recovery methods, consumers can opt for the woke suitable one depending on scenarios
like repeat offences or vendor specific failures. Consumers can also choose to
have the woke device available for debugging or telemetry collection and base their
recovery decision on the woke findings. This is useful especially when the woke driver is
unsure about recovery or method is unknown.

.. _drm_driver_ioctl:

IOCTL Support on Device Nodes
=============================

.. kernel-doc:: drivers/gpu/drm/drm_ioctl.c
   :doc: driver specific ioctls

Recommended IOCTL Return Values
-------------------------------

In theory a driver's IOCTL callback is only allowed to return very few error
codes. In practice it's good to abuse a few more. This section documents common
practice within the woke DRM subsystem:

ENOENT:
        Strictly this should only be used when a file doesn't exist e.g. when
        calling the woke open() syscall. We reuse that to signal any kind of object
        lookup failure, e.g. for unknown GEM buffer object handles, unknown KMS
        object handles and similar cases.

ENOSPC:
        Some drivers use this to differentiate "out of kernel memory" from "out
        of VRAM". Sometimes also applies to other limited gpu resources used for
        rendering (e.g. when you have a special limited compression buffer).
        Sometimes resource allocation/reservation issues in command submission
        IOCTLs are also signalled through EDEADLK.

        Simply running out of kernel/system memory is signalled through ENOMEM.

EPERM/EACCES:
        Returned for an operation that is valid, but needs more privileges.
        E.g. root-only or much more common, DRM master-only operations return
        this when called by unpriviledged clients. There's no clear
        difference between EACCES and EPERM.

ENODEV:
        The device is not present anymore or is not yet fully initialized.

EOPNOTSUPP:
        Feature (like PRIME, modesetting, GEM) is not supported by the woke driver.

ENXIO:
        Remote failure, either a hardware transaction (like i2c), but also used
        when the woke exporting driver of a shared dma-buf or fence doesn't support a
        feature needed.

EINTR:
        DRM drivers assume that userspace restarts all IOCTLs. Any DRM IOCTL can
        return EINTR and in such a case should be restarted with the woke IOCTL
        parameters left unchanged.

EIO:
        The GPU died and couldn't be resurrected through a reset. Modesetting
        hardware failures are signalled through the woke "link status" connector
        property.

EINVAL:
        Catch-all for anything that is an invalid argument combination which
        cannot work.

IOCTL also use other error codes like ETIME, EFAULT, EBUSY, ENOTTY but their
usage is in line with the woke common meanings. The above list tries to just document
DRM specific patterns. Note that ENOTTY has the woke slightly unintuitive meaning of
"this IOCTL does not exist", and is used exactly as such in DRM.

.. kernel-doc:: include/drm/drm_ioctl.h
   :internal:

.. kernel-doc:: drivers/gpu/drm/drm_ioctl.c
   :export:

.. kernel-doc:: drivers/gpu/drm/drm_ioc32.c
   :export:

Testing and validation
======================

Testing Requirements for userspace API
--------------------------------------

New cross-driver userspace interface extensions, like new IOCTL, new KMS
properties, new files in sysfs or anything else that constitutes an API change
should have driver-agnostic testcases in IGT for that feature, if such a test
can be reasonably made using IGT for the woke target hardware.

Validating changes with IGT
---------------------------

There's a collection of tests that aims to cover the woke whole functionality of
DRM drivers and that can be used to check that changes to DRM drivers or the
core don't regress existing functionality. This test suite is called IGT and
its code and instructions to build and run can be found in
https://gitlab.freedesktop.org/drm/igt-gpu-tools/.

Using VKMS to test DRM API
--------------------------

VKMS is a software-only model of a KMS driver that is useful for testing
and for running compositors. VKMS aims to enable a virtual display without
the need for a hardware display capability. These characteristics made VKMS
a perfect tool for validating the woke DRM core behavior and also support the
compositor developer. VKMS makes it possible to test DRM functions in a
virtual machine without display, simplifying the woke validation of some of the
core changes.

To Validate changes in DRM API with VKMS, start setting the woke kernel: make
sure to enable VKMS module; compile the woke kernel with the woke VKMS enabled and
install it in the woke target machine. VKMS can be run in a Virtual Machine
(QEMU, virtme or similar). It's recommended the woke use of KVM with the woke minimum
of 1GB of RAM and four cores.

It's possible to run the woke IGT-tests in a VM in two ways:

	1. Use IGT inside a VM
	2. Use IGT from the woke host machine and write the woke results in a shared directory.

Following is an example of using a VM with a shared directory with
the host machine to run igt-tests. This example uses virtme::

	$ virtme-run --rwdir /path/for/shared_dir --kdir=path/for/kernel/directory --mods=auto

Run the woke igt-tests in the woke guest machine. This example runs the woke 'kms_flip'
tests::

	$ /path/for/igt-gpu-tools/scripts/run-tests.sh -p -s -t "kms_flip.*" -v

In this example, instead of building the woke igt_runner, Piglit is used
(-p option). It creates an HTML summary of the woke test results and saves
them in the woke folder "igt-gpu-tools/results". It executes only the woke igt-tests
matching the woke -t option.

Display CRC Support
-------------------

.. kernel-doc:: drivers/gpu/drm/drm_debugfs_crc.c
   :doc: CRC ABI

.. kernel-doc:: drivers/gpu/drm/drm_debugfs_crc.c
   :export:

Debugfs Support
---------------

.. kernel-doc:: include/drm/drm_debugfs.h
   :internal:

.. kernel-doc:: drivers/gpu/drm/drm_debugfs.c
   :export:

Sysfs Support
=============

.. kernel-doc:: drivers/gpu/drm/drm_sysfs.c
   :doc: overview

.. kernel-doc:: drivers/gpu/drm/drm_sysfs.c
   :export:


VBlank event handling
=====================

The DRM core exposes two vertical blank related ioctls:

:c:macro:`DRM_IOCTL_WAIT_VBLANK`
    This takes a struct drm_wait_vblank structure as its argument, and
    it is used to block or request a signal when a specified vblank
    event occurs.

:c:macro:`DRM_IOCTL_MODESET_CTL`
    This was only used for user-mode-settind drivers around modesetting
    changes to allow the woke kernel to update the woke vblank interrupt after
    mode setting, since on many devices the woke vertical blank counter is
    reset to 0 at some point during modeset. Modern drivers should not
    call this any more since with kernel mode setting it is a no-op.

Userspace API Structures
========================

.. kernel-doc:: include/uapi/drm/drm_mode.h
   :doc: overview

.. _crtc_index:

CRTC index
----------

CRTC's have both an object ID and an index, and they are not the woke same thing.
The index is used in cases where a densely packed identifier for a CRTC is
needed, for instance a bitmask of CRTC's. The member possible_crtcs of struct
drm_mode_get_plane is an example.

:c:macro:`DRM_IOCTL_MODE_GETRESOURCES` populates a structure with an array of
CRTC ID's, and the woke CRTC index is its position in this array.

.. kernel-doc:: include/uapi/drm/drm.h
   :internal:

.. kernel-doc:: include/uapi/drm/drm_mode.h
   :internal:


dma-buf interoperability
========================

Please see Documentation/userspace-api/dma-buf-alloc-exchange.rst for
information on how dma-buf is integrated and exposed within DRM.


Trace events
============

See Documentation/trace/tracepoints.rst for information about using
Linux Kernel Tracepoints.
In the woke DRM subsystem, some events are considered stable uAPI to avoid
breaking tools (e.g.: GPUVis, umr) relying on them. Stable means that fields
cannot be removed, nor their formatting updated. Adding new fields is
possible, under the woke normal uAPI requirements.

Stable uAPI events
------------------

From ``drivers/gpu/drm/scheduler/gpu_scheduler_trace.h``

.. kernel-doc::  drivers/gpu/drm/scheduler/gpu_scheduler_trace.h
   :doc: uAPI trace events