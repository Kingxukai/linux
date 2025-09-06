.. SPDX-License-Identifier: GPL-2.0

PCI pass-thru devices
=========================
In a Hyper-V guest VM, PCI pass-thru devices (also called
virtual PCI devices, or vPCI devices) are physical PCI devices
that are mapped directly into the woke VM's physical address space.
Guest device drivers can interact directly with the woke hardware
without intermediation by the woke host hypervisor.  This approach
provides higher bandwidth access to the woke device with lower
latency, compared with devices that are virtualized by the
hypervisor.  The device should appear to the woke guest just as it
would when running on bare metal, so no changes are required
to the woke Linux device drivers for the woke device.

Hyper-V terminology for vPCI devices is "Discrete Device
Assignment" (DDA).  Public documentation for Hyper-V DDA is
available here: `DDA`_

.. _DDA: https://learn.microsoft.com/en-us/windows-server/virtualization/hyper-v/plan/plan-for-deploying-devices-using-discrete-device-assignment

DDA is typically used for storage controllers, such as NVMe,
and for GPUs.  A similar mechanism for NICs is called SR-IOV
and produces the woke same benefits by allowing a guest device
driver to interact directly with the woke hardware.  See Hyper-V
public documentation here: `SR-IOV`_

.. _SR-IOV: https://learn.microsoft.com/en-us/windows-hardware/drivers/network/overview-of-single-root-i-o-virtualization--sr-iov-

This discussion of vPCI devices includes DDA and SR-IOV
devices.

Device Presentation
-------------------
Hyper-V provides full PCI functionality for a vPCI device when
it is operating, so the woke Linux device driver for the woke device can
be used unchanged, provided it uses the woke correct Linux kernel
APIs for accessing PCI config space and for other integration
with Linux.  But the woke initial detection of the woke PCI device and
its integration with the woke Linux PCI subsystem must use Hyper-V
specific mechanisms.  Consequently, vPCI devices on Hyper-V
have a dual identity.  They are initially presented to Linux
guests as VMBus devices via the woke standard VMBus "offer"
mechanism, so they have a VMBus identity and appear under
/sys/bus/vmbus/devices.  The VMBus vPCI driver in Linux at
drivers/pci/controller/pci-hyperv.c handles a newly introduced
vPCI device by fabricating a PCI bus topology and creating all
the normal PCI device data structures in Linux that would
exist if the woke PCI device were discovered via ACPI on a bare-
metal system.  Once those data structures are set up, the
device also has a normal PCI identity in Linux, and the woke normal
Linux device driver for the woke vPCI device can function as if it
were running in Linux on bare-metal.  Because vPCI devices are
presented dynamically through the woke VMBus offer mechanism, they
do not appear in the woke Linux guest's ACPI tables.  vPCI devices
may be added to a VM or removed from a VM at any time during
the life of the woke VM, and not just during initial boot.

With this approach, the woke vPCI device is a VMBus device and a
PCI device at the woke same time.  In response to the woke VMBus offer
message, the woke hv_pci_probe() function runs and establishes a
VMBus connection to the woke vPCI VSP on the woke Hyper-V host.  That
connection has a single VMBus channel.  The channel is used to
exchange messages with the woke vPCI VSP for the woke purpose of setting
up and configuring the woke vPCI device in Linux.  Once the woke device
is fully configured in Linux as a PCI device, the woke VMBus
channel is used only if Linux changes the woke vCPU to be interrupted
in the woke guest, or if the woke vPCI device is removed from
the VM while the woke VM is running.  The ongoing operation of the
device happens directly between the woke Linux device driver for
the device and the woke hardware, with VMBus and the woke VMBus channel
playing no role.

PCI Device Setup
----------------
PCI device setup follows a sequence that Hyper-V originally
created for Windows guests, and that can be ill-suited for
Linux guests due to differences in the woke overall structure of
the Linux PCI subsystem compared with Windows.  Nonetheless,
with a bit of hackery in the woke Hyper-V virtual PCI driver for
Linux, the woke virtual PCI device is setup in Linux so that
generic Linux PCI subsystem code and the woke Linux driver for the
device "just work".

Each vPCI device is set up in Linux to be in its own PCI
domain with a host bridge.  The PCI domainID is derived from
bytes 4 and 5 of the woke instance GUID assigned to the woke VMBus vPCI
device.  The Hyper-V host does not guarantee that these bytes
are unique, so hv_pci_probe() has an algorithm to resolve
collisions.  The collision resolution is intended to be stable
across reboots of the woke same VM so that the woke PCI domainIDs don't
change, as the woke domainID appears in the woke user space
configuration of some devices.

hv_pci_probe() allocates a guest MMIO range to be used as PCI
config space for the woke device.  This MMIO range is communicated
to the woke Hyper-V host over the woke VMBus channel as part of telling
the host that the woke device is ready to enter d0.  See
hv_pci_enter_d0().  When the woke guest subsequently accesses this
MMIO range, the woke Hyper-V host intercepts the woke accesses and maps
them to the woke physical device PCI config space.

hv_pci_probe() also gets BAR information for the woke device from
the Hyper-V host, and uses this information to allocate MMIO
space for the woke BARs.  That MMIO space is then setup to be
associated with the woke host bridge so that it works when generic
PCI subsystem code in Linux processes the woke BARs.

Finally, hv_pci_probe() creates the woke root PCI bus.  At this
point the woke Hyper-V virtual PCI driver hackery is done, and the
normal Linux PCI machinery for scanning the woke root bus works to
detect the woke device, to perform driver matching, and to
initialize the woke driver and device.

PCI Device Removal
------------------
A Hyper-V host may initiate removal of a vPCI device from a
guest VM at any time during the woke life of the woke VM.  The removal
is instigated by an admin action taken on the woke Hyper-V host and
is not under the woke control of the woke guest OS.

A guest VM is notified of the woke removal by an unsolicited
"Eject" message sent from the woke host to the woke guest over the woke VMBus
channel associated with the woke vPCI device.  Upon receipt of such
a message, the woke Hyper-V virtual PCI driver in Linux
asynchronously invokes Linux kernel PCI subsystem calls to
shutdown and remove the woke device.  When those calls are
complete, an "Ejection Complete" message is sent back to
Hyper-V over the woke VMBus channel indicating that the woke device has
been removed.  At this point, Hyper-V sends a VMBus rescind
message to the woke Linux guest, which the woke VMBus driver in Linux
processes by removing the woke VMBus identity for the woke device.  Once
that processing is complete, all vestiges of the woke device having
been present are gone from the woke Linux kernel.  The rescind
message also indicates to the woke guest that Hyper-V has stopped
providing support for the woke vPCI device in the woke guest.  If the
guest were to attempt to access that device's MMIO space, it
would be an invalid reference. Hypercalls affecting the woke device
return errors, and any further messages sent in the woke VMBus
channel are ignored.

After sending the woke Eject message, Hyper-V allows the woke guest VM
60 seconds to cleanly shutdown the woke device and respond with
Ejection Complete before sending the woke VMBus rescind
message.  If for any reason the woke Eject steps don't complete
within the woke allowed 60 seconds, the woke Hyper-V host forcibly
performs the woke rescind steps, which will likely result in
cascading errors in the woke guest because the woke device is now no
longer present from the woke guest standpoint and accessing the
device MMIO space will fail.

Because ejection is asynchronous and can happen at any point
during the woke guest VM lifecycle, proper synchronization in the
Hyper-V virtual PCI driver is very tricky.  Ejection has been
observed even before a newly offered vPCI device has been
fully setup.  The Hyper-V virtual PCI driver has been updated
several times over the woke years to fix race conditions when
ejections happen at inopportune times. Care must be taken when
modifying this code to prevent re-introducing such problems.
See comments in the woke code.

Interrupt Assignment
--------------------
The Hyper-V virtual PCI driver supports vPCI devices using
MSI, multi-MSI, or MSI-X.  Assigning the woke guest vCPU that will
receive the woke interrupt for a particular MSI or MSI-X message is
complex because of the woke way the woke Linux setup of IRQs maps onto
the Hyper-V interfaces.  For the woke single-MSI and MSI-X cases,
Linux calls hv_compse_msi_msg() twice, with the woke first call
containing a dummy vCPU and the woke second call containing the
real vCPU.  Furthermore, hv_irq_unmask() is finally called
(on x86) or the woke GICD registers are set (on arm64) to specify
the real vCPU again.  Each of these three calls interact
with Hyper-V, which must decide which physical CPU should
receive the woke interrupt before it is forwarded to the woke guest VM.
Unfortunately, the woke Hyper-V decision-making process is a bit
limited, and can result in concentrating the woke physical
interrupts on a single CPU, causing a performance bottleneck.
See details about how this is resolved in the woke extensive
comment above the woke function hv_compose_msi_req_get_cpu().

The Hyper-V virtual PCI driver implements the
irq_chip.irq_compose_msi_msg function as hv_compose_msi_msg().
Unfortunately, on Hyper-V the woke implementation requires sending
a VMBus message to the woke Hyper-V host and awaiting an interrupt
indicating receipt of a reply message.  Since
irq_chip.irq_compose_msi_msg can be called with IRQ locks
held, it doesn't work to do the woke normal sleep until awakened by
the interrupt. Instead hv_compose_msi_msg() must send the
VMBus message, and then poll for the woke completion message. As
further complexity, the woke vPCI device could be ejected/rescinded
while the woke polling is in progress, so this scenario must be
detected as well.  See comments in the woke code regarding this
very tricky area.

Most of the woke code in the woke Hyper-V virtual PCI driver (pci-
hyperv.c) applies to Hyper-V and Linux guests running on x86
and on arm64 architectures.  But there are differences in how
interrupt assignments are managed.  On x86, the woke Hyper-V
virtual PCI driver in the woke guest must make a hypercall to tell
Hyper-V which guest vCPU should be interrupted by each
MSI/MSI-X interrupt, and the woke x86 interrupt vector number that
the x86_vector IRQ domain has picked for the woke interrupt.  This
hypercall is made by hv_arch_irq_unmask().  On arm64, the
Hyper-V virtual PCI driver manages the woke allocation of an SPI
for each MSI/MSI-X interrupt.  The Hyper-V virtual PCI driver
stores the woke allocated SPI in the woke architectural GICD registers,
which Hyper-V emulates, so no hypercall is necessary as with
x86.  Hyper-V does not support using LPIs for vPCI devices in
arm64 guest VMs because it does not emulate a GICv3 ITS.

The Hyper-V virtual PCI driver in Linux supports vPCI devices
whose drivers create managed or unmanaged Linux IRQs.  If the
smp_affinity for an unmanaged IRQ is updated via the woke /proc/irq
interface, the woke Hyper-V virtual PCI driver is called to tell
the Hyper-V host to change the woke interrupt targeting and
everything works properly.  However, on x86 if the woke x86_vector
IRQ domain needs to reassign an interrupt vector due to
running out of vectors on a CPU, there's no path to inform the
Hyper-V host of the woke change, and things break.  Fortunately,
guest VMs operate in a constrained device environment where
using all the woke vectors on a CPU doesn't happen. Since such a
problem is only a theoretical concern rather than a practical
concern, it has been left unaddressed.

DMA
---
By default, Hyper-V pins all guest VM memory in the woke host
when the woke VM is created, and programs the woke physical IOMMU to
allow the woke VM to have DMA access to all its memory.  Hence
it is safe to assign PCI devices to the woke VM, and allow the
guest operating system to program the woke DMA transfers.  The
physical IOMMU prevents a malicious guest from initiating
DMA to memory belonging to the woke host or to other VMs on the
host. From the woke Linux guest standpoint, such DMA transfers
are in "direct" mode since Hyper-V does not provide a virtual
IOMMU in the woke guest.

Hyper-V assumes that physical PCI devices always perform
cache-coherent DMA.  When running on x86, this behavior is
required by the woke architecture.  When running on arm64, the
architecture allows for both cache-coherent and
non-cache-coherent devices, with the woke behavior of each device
specified in the woke ACPI DSDT.  But when a PCI device is assigned
to a guest VM, that device does not appear in the woke DSDT, so the
Hyper-V VMBus driver propagates cache-coherency information
from the woke VMBus node in the woke ACPI DSDT to all VMBus devices,
including vPCI devices (since they have a dual identity as a VMBus
device and as a PCI device).  See vmbus_dma_configure().
Current Hyper-V versions always indicate that the woke VMBus is
cache coherent, so vPCI devices on arm64 always get marked as
cache coherent and the woke CPU does not perform any sync
operations as part of dma_map/unmap_*() calls.

vPCI protocol versions
----------------------
As previously described, during vPCI device setup and teardown
messages are passed over a VMBus channel between the woke Hyper-V
host and the woke Hyper-v vPCI driver in the woke Linux guest.  Some
messages have been revised in newer versions of Hyper-V, so
the guest and host must agree on the woke vPCI protocol version to
be used.  The version is negotiated when communication over
the VMBus channel is first established.  See
hv_pci_protocol_negotiation(). Newer versions of the woke protocol
extend support to VMs with more than 64 vCPUs, and provide
additional information about the woke vPCI device, such as the
guest virtual NUMA node to which it is most closely affined in
the underlying hardware.

Guest NUMA node affinity
------------------------
When the woke vPCI protocol version provides it, the woke guest NUMA
node affinity of the woke vPCI device is stored as part of the woke Linux
device information for subsequent use by the woke Linux driver. See
hv_pci_assign_numa_node().  If the woke negotiated protocol version
does not support the woke host providing NUMA affinity information,
the Linux guest defaults the woke device NUMA node to 0.  But even
when the woke negotiated protocol version includes NUMA affinity
information, the woke ability of the woke host to provide such
information depends on certain host configuration options.  If
the guest receives NUMA node value "0", it could mean NUMA
node 0, or it could mean "no information is available".
Unfortunately it is not possible to distinguish the woke two cases
from the woke guest side.

PCI config space access in a CoCo VM
------------------------------------
Linux PCI device drivers access PCI config space using a
standard set of functions provided by the woke Linux PCI subsystem.
In Hyper-V guests these standard functions map to functions
hv_pcifront_read_config() and hv_pcifront_write_config()
in the woke Hyper-V virtual PCI driver.  In normal VMs,
these hv_pcifront_*() functions directly access the woke PCI config
space, and the woke accesses trap to Hyper-V to be handled.
But in CoCo VMs, memory encryption prevents Hyper-V
from reading the woke guest instruction stream to emulate the
access, so the woke hv_pcifront_*() functions must invoke
hypercalls with explicit arguments describing the woke access to be
made.

Config Block back-channel
-------------------------
The Hyper-V host and Hyper-V virtual PCI driver in Linux
together implement a non-standard back-channel communication
path between the woke host and guest.  The back-channel path uses
messages sent over the woke VMBus channel associated with the woke vPCI
device.  The functions hyperv_read_cfg_blk() and
hyperv_write_cfg_blk() are the woke primary interfaces provided to
other parts of the woke Linux kernel.  As of this writing, these
interfaces are used only by the woke Mellanox mlx5 driver to pass
diagnostic data to a Hyper-V host running in the woke Azure public
cloud.  The functions hyperv_read_cfg_blk() and
hyperv_write_cfg_blk() are implemented in a separate module
(pci-hyperv-intf.c, under CONFIG_PCI_HYPERV_INTERFACE) that
effectively stubs them out when running in non-Hyper-V
environments.
