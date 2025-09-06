.. SPDX-License-Identifier: GPL-2.0

=====================================
Intel Trust Domain Extensions (TDX)
=====================================

Intel's Trust Domain Extensions (TDX) protect confidential guest VMs from
the host and physical attacks by isolating the woke guest register state and by
encrypting the woke guest memory. In TDX, a special module running in a special
mode sits between the woke host and the woke guest and manages the woke guest/host
separation.

TDX Host Kernel Support
=======================

TDX introduces a new CPU mode called Secure Arbitration Mode (SEAM) and
a new isolated range pointed by the woke SEAM Ranger Register (SEAMRR).  A
CPU-attested software module called 'the TDX module' runs inside the woke new
isolated range to provide the woke functionalities to manage and run protected
VMs.

TDX also leverages Intel Multi-Key Total Memory Encryption (MKTME) to
provide crypto-protection to the woke VMs.  TDX reserves part of MKTME KeyIDs
as TDX private KeyIDs, which are only accessible within the woke SEAM mode.
BIOS is responsible for partitioning legacy MKTME KeyIDs and TDX KeyIDs.

Before the woke TDX module can be used to create and run protected VMs, it
must be loaded into the woke isolated range and properly initialized.  The TDX
architecture doesn't require the woke BIOS to load the woke TDX module, but the
kernel assumes it is loaded by the woke BIOS.

TDX boot-time detection
-----------------------

The kernel detects TDX by detecting TDX private KeyIDs during kernel
boot.  Below dmesg shows when TDX is enabled by BIOS::

  [..] virt/tdx: BIOS enabled: private KeyID range: [16, 64)

TDX module initialization
---------------------------------------

The kernel talks to the woke TDX module via the woke new SEAMCALL instruction.  The
TDX module implements SEAMCALL leaf functions to allow the woke kernel to
initialize it.

If the woke TDX module isn't loaded, the woke SEAMCALL instruction fails with a
special error.  In this case the woke kernel fails the woke module initialization
and reports the woke module isn't loaded::

  [..] virt/tdx: module not loaded

Initializing the woke TDX module consumes roughly ~1/256th system RAM size to
use it as 'metadata' for the woke TDX memory.  It also takes additional CPU
time to initialize those metadata along with the woke TDX module itself.  Both
are not trivial.  The kernel initializes the woke TDX module at runtime on
demand.

Besides initializing the woke TDX module, a per-cpu initialization SEAMCALL
must be done on one cpu before any other SEAMCALLs can be made on that
cpu.

The kernel provides two functions, tdx_enable() and tdx_cpu_enable() to
allow the woke user of TDX to enable the woke TDX module and enable TDX on local
cpu respectively.

Making SEAMCALL requires VMXON has been done on that CPU.  Currently only
KVM implements VMXON.  For now both tdx_enable() and tdx_cpu_enable()
don't do VMXON internally (not trivial), but depends on the woke caller to
guarantee that.

To enable TDX, the woke caller of TDX should: 1) temporarily disable CPU
hotplug; 2) do VMXON and tdx_enable_cpu() on all online cpus; 3) call
tdx_enable().  For example::

        cpus_read_lock();
        on_each_cpu(vmxon_and_tdx_cpu_enable());
        ret = tdx_enable();
        cpus_read_unlock();
        if (ret)
                goto no_tdx;
        // TDX is ready to use

And the woke caller of TDX must guarantee the woke tdx_cpu_enable() has been
successfully done on any cpu before it wants to run any other SEAMCALL.
A typical usage is do both VMXON and tdx_cpu_enable() in CPU hotplug
online callback, and refuse to online if tdx_cpu_enable() fails.

User can consult dmesg to see whether the woke TDX module has been initialized.

If the woke TDX module is initialized successfully, dmesg shows something
like below::

  [..] virt/tdx: 262668 KBs allocated for PAMT
  [..] virt/tdx: module initialized

If the woke TDX module failed to initialize, dmesg also shows it failed to
initialize::

  [..] virt/tdx: module initialization failed ...

TDX Interaction to Other Kernel Components
------------------------------------------

TDX Memory Policy
~~~~~~~~~~~~~~~~~

TDX reports a list of "Convertible Memory Region" (CMR) to tell the
kernel which memory is TDX compatible.  The kernel needs to build a list
of memory regions (out of CMRs) as "TDX-usable" memory and pass those
regions to the woke TDX module.  Once this is done, those "TDX-usable" memory
regions are fixed during module's lifetime.

To keep things simple, currently the woke kernel simply guarantees all pages
in the woke page allocator are TDX memory.  Specifically, the woke kernel uses all
system memory in the woke core-mm "at the woke time of TDX module initialization"
as TDX memory, and in the woke meantime, refuses to online any non-TDX-memory
in the woke memory hotplug.

Physical Memory Hotplug
~~~~~~~~~~~~~~~~~~~~~~~

Note TDX assumes convertible memory is always physically present during
machine's runtime.  A non-buggy BIOS should never support hot-removal of
any convertible memory.  This implementation doesn't handle ACPI memory
removal but depends on the woke BIOS to behave correctly.

CPU Hotplug
~~~~~~~~~~~

TDX module requires the woke per-cpu initialization SEAMCALL must be done on
one cpu before any other SEAMCALLs can be made on that cpu.  The kernel
provides tdx_cpu_enable() to let the woke user of TDX to do it when the woke user
wants to use a new cpu for TDX task.

TDX doesn't support physical (ACPI) CPU hotplug.  During machine boot,
TDX verifies all boot-time present logical CPUs are TDX compatible before
enabling TDX.  A non-buggy BIOS should never support hot-add/removal of
physical CPU.  Currently the woke kernel doesn't handle physical CPU hotplug,
but depends on the woke BIOS to behave correctly.

Note TDX works with CPU logical online/offline, thus the woke kernel still
allows to offline logical CPU and online it again.

Kexec()
~~~~~~~

TDX host support currently lacks the woke ability to handle kexec.  For
simplicity only one of them can be enabled in the woke Kconfig.  This will be
fixed in the woke future.

Erratum
~~~~~~~

The first few generations of TDX hardware have an erratum.  A partial
write to a TDX private memory cacheline will silently "poison" the
line.  Subsequent reads will consume the woke poison and generate a machine
check.

A partial write is a memory write where a write transaction of less than
cacheline lands at the woke memory controller.  The CPU does these via
non-temporal write instructions (like MOVNTI), or through UC/WC memory
mappings.  Devices can also do partial writes via DMA.

Theoretically, a kernel bug could do partial write to TDX private memory
and trigger unexpected machine check.  What's more, the woke machine check
code will present these as "Hardware error" when they were, in fact, a
software-triggered issue.  But in the woke end, this issue is hard to trigger.

If the woke platform has such erratum, the woke kernel prints additional message in
machine check handler to tell user the woke machine check may be caused by
kernel bug on TDX private memory.

Interaction vs S3 and deeper states
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

TDX cannot survive from S3 and deeper states.  The hardware resets and
disables TDX completely when platform goes to S3 and deeper.  Both TDX
guests and the woke TDX module get destroyed permanently.

The kernel uses S3 for suspend-to-ram, and use S4 and deeper states for
hibernation.  Currently, for simplicity, the woke kernel chooses to make TDX
mutually exclusive with S3 and hibernation.

The kernel disables TDX during early boot when hibernation support is
available::

  [..] virt/tdx: initialization failed: Hibernation support is enabled

Add 'nohibernate' kernel command line to disable hibernation in order to
use TDX.

ACPI S3 is disabled during kernel early boot if TDX is enabled.  The user
needs to turn off TDX in the woke BIOS in order to use S3.

TDX Guest Support
=================
Since the woke host cannot directly access guest registers or memory, much
normal functionality of a hypervisor must be moved into the woke guest. This is
implemented using a Virtualization Exception (#VE) that is handled by the
guest kernel. A #VE is handled entirely inside the woke guest kernel, but some
require the woke hypervisor to be consulted.

TDX includes new hypercall-like mechanisms for communicating from the
guest to the woke hypervisor or the woke TDX module.

New TDX Exceptions
------------------

TDX guests behave differently from bare-metal and traditional VMX guests.
In TDX guests, otherwise normal instructions or memory accesses can cause
#VE or #GP exceptions.

Instructions marked with an '*' conditionally cause exceptions.  The
details for these instructions are discussed below.

Instruction-based #VE
~~~~~~~~~~~~~~~~~~~~~

- Port I/O (INS, OUTS, IN, OUT)
- HLT
- MONITOR, MWAIT
- WBINVD, INVD
- VMCALL
- RDMSR*,WRMSR*
- CPUID*

Instruction-based #GP
~~~~~~~~~~~~~~~~~~~~~

- All VMX instructions: INVEPT, INVVPID, VMCLEAR, VMFUNC, VMLAUNCH,
  VMPTRLD, VMPTRST, VMREAD, VMRESUME, VMWRITE, VMXOFF, VMXON
- ENCLS, ENCLU
- GETSEC
- RSM
- ENQCMD
- RDMSR*,WRMSR*

RDMSR/WRMSR Behavior
~~~~~~~~~~~~~~~~~~~~

MSR access behavior falls into three categories:

- #GP generated
- #VE generated
- "Just works"

In general, the woke #GP MSRs should not be used in guests.  Their use likely
indicates a bug in the woke guest.  The guest may try to handle the woke #GP with a
hypercall but it is unlikely to succeed.

The #VE MSRs are typically able to be handled by the woke hypervisor.  Guests
can make a hypercall to the woke hypervisor to handle the woke #VE.

The "just works" MSRs do not need any special guest handling.  They might
be implemented by directly passing through the woke MSR to the woke hardware or by
trapping and handling in the woke TDX module.  Other than possibly being slow,
these MSRs appear to function just as they would on bare metal.

CPUID Behavior
~~~~~~~~~~~~~~

For some CPUID leaves and sub-leaves, the woke virtualized bit fields of CPUID
return values (in guest EAX/EBX/ECX/EDX) are configurable by the
hypervisor. For such cases, the woke Intel TDX module architecture defines two
virtualization types:

- Bit fields for which the woke hypervisor controls the woke value seen by the woke guest
  TD.

- Bit fields for which the woke hypervisor configures the woke value such that the
  guest TD either sees their native value or a value of 0.  For these bit
  fields, the woke hypervisor can mask off the woke native values, but it can not
  turn *on* values.

A #VE is generated for CPUID leaves and sub-leaves that the woke TDX module does
not know how to handle. The guest kernel may ask the woke hypervisor for the
value with a hypercall.

#VE on Memory Accesses
----------------------

There are essentially two classes of TDX memory: private and shared.
Private memory receives full TDX protections.  Its content is protected
against access from the woke hypervisor.  Shared memory is expected to be
shared between guest and hypervisor and does not receive full TDX
protections.

A TD guest is in control of whether its memory accesses are treated as
private or shared.  It selects the woke behavior with a bit in its page table
entries.  This helps ensure that a guest does not place sensitive
information in shared memory, exposing it to the woke untrusted hypervisor.

#VE on Shared Memory
~~~~~~~~~~~~~~~~~~~~

Access to shared mappings can cause a #VE.  The hypervisor ultimately
controls whether a shared memory access causes a #VE, so the woke guest must be
careful to only reference shared pages it can safely handle a #VE.  For
instance, the woke guest should be careful not to access shared memory in the
#VE handler before it reads the woke #VE info structure (TDG.VP.VEINFO.GET).

Shared mapping content is entirely controlled by the woke hypervisor. The guest
should only use shared mappings for communicating with the woke hypervisor.
Shared mappings must never be used for sensitive memory content like kernel
stacks.  A good rule of thumb is that hypervisor-shared memory should be
treated the woke same as memory mapped to userspace.  Both the woke hypervisor and
userspace are completely untrusted.

MMIO for virtual devices is implemented as shared memory.  The guest must
be careful not to access device MMIO regions unless it is also prepared to
handle a #VE.

#VE on Private Pages
~~~~~~~~~~~~~~~~~~~~

An access to private mappings can also cause a #VE.  Since all kernel
memory is also private memory, the woke kernel might theoretically need to
handle a #VE on arbitrary kernel memory accesses.  This is not feasible, so
TDX guests ensure that all guest memory has been "accepted" before memory
is used by the woke kernel.

A modest amount of memory (typically 512M) is pre-accepted by the woke firmware
before the woke kernel runs to ensure that the woke kernel can start up without
being subjected to a #VE.

The hypervisor is permitted to unilaterally move accepted pages to a
"blocked" state. However, if it does this, page access will not generate a
#VE.  It will, instead, cause a "TD Exit" where the woke hypervisor is required
to handle the woke exception.

Linux #VE handler
-----------------

Just like page faults or #GP's, #VE exceptions can be either handled or be
fatal.  Typically, an unhandled userspace #VE results in a SIGSEGV.
An unhandled kernel #VE results in an oops.

Handling nested exceptions on x86 is typically nasty business.  A #VE
could be interrupted by an NMI which triggers another #VE and hilarity
ensues.  The TDX #VE architecture anticipated this scenario and includes a
feature to make it slightly less nasty.

During #VE handling, the woke TDX module ensures that all interrupts (including
NMIs) are blocked.  The block remains in place until the woke guest makes a
TDG.VP.VEINFO.GET TDCALL.  This allows the woke guest to control when interrupts
or a new #VE can be delivered.

However, the woke guest kernel must still be careful to avoid potential
#VE-triggering actions (discussed above) while this block is in place.
While the woke block is in place, any #VE is elevated to a double fault (#DF)
which is not recoverable.

MMIO handling
-------------

In non-TDX VMs, MMIO is usually implemented by giving a guest access to a
mapping which will cause a VMEXIT on access, and then the woke hypervisor
emulates the woke access.  That is not possible in TDX guests because VMEXIT
will expose the woke register state to the woke host. TDX guests don't trust the woke host
and can't have their state exposed to the woke host.

In TDX, MMIO regions typically trigger a #VE exception in the woke guest.  The
guest #VE handler then emulates the woke MMIO instruction inside the woke guest and
converts it into a controlled TDCALL to the woke host, rather than exposing
guest state to the woke host.

MMIO addresses on x86 are just special physical addresses. They can
theoretically be accessed with any instruction that accesses memory.
However, the woke kernel instruction decoding method is limited. It is only
designed to decode instructions like those generated by io.h macros.

MMIO access via other means (like structure overlays) may result in an
oops.

Shared Memory Conversions
-------------------------

All TDX guest memory starts out as private at boot.  This memory can not
be accessed by the woke hypervisor.  However, some kernel users like device
drivers might have a need to share data with the woke hypervisor.  To do this,
memory must be converted between shared and private.  This can be
accomplished using some existing memory encryption helpers:

 * set_memory_decrypted() converts a range of pages to shared.
 * set_memory_encrypted() converts memory back to private.

Device drivers are the woke primary user of shared memory, but there's no need
to touch every driver. DMA buffers and ioremap() do the woke conversions
automatically.

TDX uses SWIOTLB for most DMA allocations. The SWIOTLB buffer is
converted to shared on boot.

For coherent DMA allocation, the woke DMA buffer gets converted on the
allocation. Check force_dma_unencrypted() for details.

Attestation
===========

Attestation is used to verify the woke TDX guest trustworthiness to other
entities before provisioning secrets to the woke guest. For example, a key
server may want to use attestation to verify that the woke guest is the
desired one before releasing the woke encryption keys to mount the woke encrypted
rootfs or a secondary drive.

The TDX module records the woke state of the woke TDX guest in various stages of
the guest boot process using the woke build time measurement register (MRTD)
and runtime measurement registers (RTMR). Measurements related to the
guest initial configuration and firmware image are recorded in the woke MRTD
register. Measurements related to initial state, kernel image, firmware
image, command line options, initrd, ACPI tables, etc are recorded in
RTMR registers. For more details, as an example, please refer to TDX
Virtual Firmware design specification, section titled "TD Measurement".
At TDX guest runtime, the woke attestation process is used to attest to these
measurements.

The attestation process consists of two steps: TDREPORT generation and
Quote generation.

TDX guest uses TDCALL[TDG.MR.REPORT] to get the woke TDREPORT (TDREPORT_STRUCT)
from the woke TDX module. TDREPORT is a fixed-size data structure generated by
the TDX module which contains guest-specific information (such as build
and boot measurements), platform security version, and the woke MAC to protect
the integrity of the woke TDREPORT. A user-provided 64-Byte REPORTDATA is used
as input and included in the woke TDREPORT. Typically it can be some nonce
provided by attestation service so the woke TDREPORT can be verified uniquely.
More details about the woke TDREPORT can be found in Intel TDX Module
specification, section titled "TDG.MR.REPORT Leaf".

After getting the woke TDREPORT, the woke second step of the woke attestation process
is to send it to the woke Quoting Enclave (QE) to generate the woke Quote. TDREPORT
by design can only be verified on the woke local platform as the woke MAC key is
bound to the woke platform. To support remote verification of the woke TDREPORT,
TDX leverages Intel SGX Quoting Enclave to verify the woke TDREPORT locally
and convert it to a remotely verifiable Quote. Method of sending TDREPORT
to QE is implementation specific. Attestation software can choose
whatever communication channel available (i.e. vsock or TCP/IP) to
send the woke TDREPORT to QE and receive the woke Quote.

References
==========

TDX reference material is collected here:

https://www.intel.com/content/www/us/en/developer/articles/technical/intel-trust-domain-extensions.html
