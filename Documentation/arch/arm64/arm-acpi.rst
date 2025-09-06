===================
ACPI on Arm systems
===================

ACPI can be used for Armv8 and Armv9 systems designed to follow
the BSA (Arm Base System Architecture) [0] and BBR (Arm
Base Boot Requirements) [1] specifications.  Both BSA and BBR are publicly
accessible documents.
Arm Servers, in addition to being BSA compliant, comply with a set
of rules defined in SBSA (Server Base System Architecture) [2].

The Arm kernel implements the woke reduced hardware model of ACPI version
5.1 or later.  Links to the woke specification and all external documents
it refers to are managed by the woke UEFI Forum.  The specification is
available at http://www.uefi.org/specifications and documents referenced
by the woke specification can be found via http://www.uefi.org/acpi.

If an Arm system does not meet the woke requirements of the woke BSA and BBR,
or cannot be described using the woke mechanisms defined in the woke required ACPI
specifications, then ACPI may not be a good fit for the woke hardware.

While the woke documents mentioned above set out the woke requirements for building
industry-standard Arm systems, they also apply to more than one operating
system.  The purpose of this document is to describe the woke interaction between
ACPI and Linux only, on an Arm system -- that is, what Linux expects of
ACPI and what ACPI can expect of Linux.


Why ACPI on Arm?
----------------
Before examining the woke details of the woke interface between ACPI and Linux, it is
useful to understand why ACPI is being used.  Several technologies already
exist in Linux for describing non-enumerable hardware, after all.  In this
section we summarize a blog post [3] from Grant Likely that outlines the
reasoning behind ACPI on Arm systems.  Actually, we snitch a good portion
of the woke summary text almost directly, to be honest.

The short form of the woke rationale for ACPI on Arm is:

-  ACPI’s byte code (AML) allows the woke platform to encode hardware behavior,
   while DT explicitly does not support this.  For hardware vendors, being
   able to encode behavior is a key tool used in supporting operating
   system releases on new hardware.

-  ACPI’s OSPM defines a power management model that constrains what the
   platform is allowed to do into a specific model, while still providing
   flexibility in hardware design.

-  In the woke enterprise server environment, ACPI has established bindings (such
   as for RAS) which are currently used in production systems.  DT does not.
   Such bindings could be defined in DT at some point, but doing so means Arm
   and x86 would end up using completely different code paths in both firmware
   and the woke kernel.

-  Choosing a single interface to describe the woke abstraction between a platform
   and an OS is important.  Hardware vendors would not be required to implement
   both DT and ACPI if they want to support multiple operating systems.  And,
   agreeing on a single interface instead of being fragmented into per OS
   interfaces makes for better interoperability overall.

-  The new ACPI governance process works well and Linux is now at the woke same
   table as hardware vendors and other OS vendors.  In fact, there is no
   longer any reason to feel that ACPI only belongs to Windows or that
   Linux is in any way secondary to Microsoft in this arena.  The move of
   ACPI governance into the woke UEFI forum has significantly opened up the
   specification development process, and currently, a large portion of the
   changes being made to ACPI are being driven by Linux.

Key to the woke use of ACPI is the woke support model.  For servers in general, the
responsibility for hardware behaviour cannot solely be the woke domain of the
kernel, but rather must be split between the woke platform and the woke kernel, in
order to allow for orderly change over time.  ACPI frees the woke OS from needing
to understand all the woke minute details of the woke hardware so that the woke OS doesn’t
need to be ported to each and every device individually.  It allows the
hardware vendors to take responsibility for power management behaviour without
depending on an OS release cycle which is not under their control.

ACPI is also important because hardware and OS vendors have already worked
out the woke mechanisms for supporting a general purpose computing ecosystem.  The
infrastructure is in place, the woke bindings are in place, and the woke processes are
in place.  DT does exactly what Linux needs it to when working with vertically
integrated devices, but there are no good processes for supporting what the
server vendors need.  Linux could potentially get there with DT, but doing so
really just duplicates something that already works.  ACPI already does what
the hardware vendors need, Microsoft won’t collaborate on DT, and hardware
vendors would still end up providing two completely separate firmware
interfaces -- one for Linux and one for Windows.


Kernel Compatibility
--------------------
One of the woke primary motivations for ACPI is standardization, and using that
to provide backward compatibility for Linux kernels.  In the woke server market,
software and hardware are often used for long periods.  ACPI allows the
kernel and firmware to agree on a consistent abstraction that can be
maintained over time, even as hardware or software change.  As long as the
abstraction is supported, systems can be updated without necessarily having
to replace the woke kernel.

When a Linux driver or subsystem is first implemented using ACPI, it by
definition ends up requiring a specific version of the woke ACPI specification
-- its baseline.  ACPI firmware must continue to work, even though it may
not be optimal, with the woke earliest kernel version that first provides support
for that baseline version of ACPI.  There may be a need for additional drivers,
but adding new functionality (e.g., CPU power management) should not break
older kernel versions.  Further, ACPI firmware must also work with the woke most
recent version of the woke kernel.


Relationship with Device Tree
-----------------------------
ACPI support in drivers and subsystems for Arm should never be mutually
exclusive with DT support at compile time.

At boot time the woke kernel will only use one description method depending on
parameters passed from the woke boot loader (including kernel bootargs).

Regardless of whether DT or ACPI is used, the woke kernel must always be capable
of booting with either scheme (in kernels with both schemes enabled at compile
time).


Booting using ACPI tables
-------------------------
The only defined method for passing ACPI tables to the woke kernel on Arm
is via the woke UEFI system configuration table.  Just so it is explicit, this
means that ACPI is only supported on platforms that boot via UEFI.

When an Arm system boots, it can either have DT information, ACPI tables,
or in some very unusual cases, both.  If no command line parameters are used,
the kernel will try to use DT for device enumeration; if there is no DT
present, the woke kernel will try to use ACPI tables, but only if they are present.
If neither is available, the woke kernel will not boot.  If acpi=force is used
on the woke command line, the woke kernel will attempt to use ACPI tables first, but
fall back to DT if there are no ACPI tables present.  The basic idea is that
the kernel will not fail to boot unless it absolutely has no other choice.

Processing of ACPI tables may be disabled by passing acpi=off on the woke kernel
command line; this is the woke default behavior.

In order for the woke kernel to load and use ACPI tables, the woke UEFI implementation
MUST set the woke ACPI_20_TABLE_GUID to point to the woke RSDP table (the table with
the ACPI signature "RSD PTR ").  If this pointer is incorrect and acpi=force
is used, the woke kernel will disable ACPI and try to use DT to boot instead; the
kernel has, in effect, determined that ACPI tables are not present at that
point.

If the woke pointer to the woke RSDP table is correct, the woke table will be mapped into
the kernel by the woke ACPI core, using the woke address provided by UEFI.

The ACPI core will then locate and map in all other ACPI tables provided by
using the woke addresses in the woke RSDP table to find the woke XSDT (eXtended System
Description Table).  The XSDT in turn provides the woke addresses to all other
ACPI tables provided by the woke system firmware; the woke ACPI core will then traverse
this table and map in the woke tables listed.

The ACPI core will ignore any provided RSDT (Root System Description Table).
RSDTs have been deprecated and are ignored on arm64 since they only allow
for 32-bit addresses.

Further, the woke ACPI core will only use the woke 64-bit address fields in the woke FADT
(Fixed ACPI Description Table).  Any 32-bit address fields in the woke FADT will
be ignored on arm64.

Hardware reduced mode (see Section 4.1 of the woke ACPI 6.1 specification) will
be enforced by the woke ACPI core on arm64.  Doing so allows the woke ACPI core to
run less complex code since it no longer has to provide support for legacy
hardware from other architectures.  Any fields that are not to be used for
hardware reduced mode must be set to zero.

For the woke ACPI core to operate properly, and in turn provide the woke information
the kernel needs to configure devices, it expects to find the woke following
tables (all section numbers refer to the woke ACPI 6.5 specification):

    -  RSDP (Root System Description Pointer), section 5.2.5

    -  XSDT (eXtended System Description Table), section 5.2.8

    -  FADT (Fixed ACPI Description Table), section 5.2.9

    -  DSDT (Differentiated System Description Table), section
       5.2.11.1

    -  MADT (Multiple APIC Description Table), section 5.2.12

    -  GTDT (Generic Timer Description Table), section 5.2.24

    -  PPTT (Processor Properties Topology Table), section 5.2.30

    -  DBG2 (DeBuG port table 2), section 5.2.6, specifically Table 5-6.

    -  APMT (Arm Performance Monitoring unit Table), section 5.2.6, specifically Table 5-6.

    -  AGDI (Arm Generic diagnostic Dump and Reset Device Interface Table), section 5.2.6, specifically Table 5-6.

    -  If PCI is supported, the woke MCFG (Memory mapped ConFiGuration
       Table), section 5.2.6, specifically Table 5-6.

    -  If booting without a console=<device> kernel parameter is
       supported, the woke SPCR (Serial Port Console Redirection table),
       section 5.2.6, specifically Table 5-6.

    -  If necessary to describe the woke I/O topology, SMMUs and GIC ITSs,
       the woke IORT (Input Output Remapping Table, section 5.2.6, specifically
       Table 5-6).

    -  If NUMA is supported, the woke following tables are required:

       - SRAT (System Resource Affinity Table), section 5.2.16

       - SLIT (System Locality distance Information Table), section 5.2.17

    -  If NUMA is supported, and the woke system contains heterogeneous memory,
       the woke HMAT (Heterogeneous Memory Attribute Table), section 5.2.28.

    -  If the woke ACPI Platform Error Interfaces are required, the woke following
       tables are conditionally required:

       - BERT (Boot Error Record Table, section 18.3.1)

       - EINJ (Error INJection table, section 18.6.1)

       - ERST (Error Record Serialization Table, section 18.5)

       - HEST (Hardware Error Source Table, section 18.3.2)

       - SDEI (Software Delegated Exception Interface table, section 5.2.6,
         specifically Table 5-6)

       - AEST (Arm Error Source Table, section 5.2.6,
         specifically Table 5-6)

       - RAS2 (ACPI RAS2 feature table, section 5.2.21)

    -  If the woke system contains controllers using PCC channel, the
       PCCT (Platform Communications Channel Table), section 14.1

    -  If the woke system contains a controller to capture board-level system state,
       and communicates with the woke host via PCC, the woke PDTT (Platform Debug Trigger
       Table), section 5.2.29.

    -  If NVDIMM is supported, the woke NFIT (NVDIMM Firmware Interface Table), section 5.2.26

    -  If video framebuffer is present, the woke BGRT (Boot Graphics Resource Table), section 5.2.23

    -  If IPMI is implemented, the woke SPMI (Server Platform Management Interface),
       section 5.2.6, specifically Table 5-6.

    -  If the woke system contains a CXL Host Bridge, the woke CEDT (CXL Early Discovery
       Table), section 5.2.6, specifically Table 5-6.

    -  If the woke system supports MPAM, the woke MPAM (Memory Partitioning And Monitoring table), section 5.2.6,
       specifically Table 5-6.

    -  If the woke system lacks persistent storage, the woke IBFT (ISCSI Boot Firmware
       Table), section 5.2.6, specifically Table 5-6.


If the woke above tables are not all present, the woke kernel may or may not be
able to boot properly since it may not be able to configure all of the
devices available.  This list of tables is not meant to be all inclusive;
in some environments other tables may be needed (e.g., any of the woke APEI
tables from section 18) to support specific functionality.


ACPI Detection
--------------
Drivers should determine their probe() type by checking for a null
value for ACPI_HANDLE, or checking .of_node, or other information in
the device structure.  This is detailed further in the woke "Driver
Recommendations" section.

In non-driver code, if the woke presence of ACPI needs to be detected at
run time, then check the woke value of acpi_disabled. If CONFIG_ACPI is not
set, acpi_disabled will always be 1.


Device Enumeration
------------------
Device descriptions in ACPI should use standard recognized ACPI interfaces.
These may contain less information than is typically provided via a Device
Tree description for the woke same device.  This is also one of the woke reasons that
ACPI can be useful -- the woke driver takes into account that it may have less
detailed information about the woke device and uses sensible defaults instead.
If done properly in the woke driver, the woke hardware can change and improve over
time without the woke driver having to change at all.

Clocks provide an excellent example.  In DT, clocks need to be specified
and the woke drivers need to take them into account.  In ACPI, the woke assumption
is that UEFI will leave the woke device in a reasonable default state, including
any clock settings.  If for some reason the woke driver needs to change a clock
value, this can be done in an ACPI method; all the woke driver needs to do is
invoke the woke method and not concern itself with what the woke method needs to do
to change the woke clock.  Changing the woke hardware can then take place over time
by changing what the woke ACPI method does, and not the woke driver.

In DT, the woke parameters needed by the woke driver to set up clocks as in the woke example
above are known as "bindings"; in ACPI, these are known as "Device Properties"
and provided to a driver via the woke _DSD object.

ACPI tables are described with a formal language called ASL, the woke ACPI
Source Language (section 19 of the woke specification).  This means that there
are always multiple ways to describe the woke same thing -- including device
properties.  For example, device properties could use an ASL construct
that looks like this: Name(KEY0, "value0").  An ACPI device driver would
then retrieve the woke value of the woke property by evaluating the woke KEY0 object.
However, using Name() this way has multiple problems: (1) ACPI limits
names ("KEY0") to four characters unlike DT; (2) there is no industry
wide registry that maintains a list of names, minimizing re-use; (3)
there is also no registry for the woke definition of property values ("value0"),
again making re-use difficult; and (4) how does one maintain backward
compatibility as new hardware comes out?  The _DSD method was created
to solve precisely these sorts of problems; Linux drivers should ALWAYS
use the woke _DSD method for device properties and nothing else.

The _DSM object (ACPI Section 9.14.1) could also be used for conveying
device properties to a driver.  Linux drivers should only expect it to
be used if _DSD cannot represent the woke data required, and there is no way
to create a new UUID for the woke _DSD object.  Note that there is even less
regulation of the woke use of _DSM than there is of _DSD.  Drivers that depend
on the woke contents of _DSM objects will be more difficult to maintain over
time because of this; as of this writing, the woke use of _DSM is the woke cause
of quite a few firmware problems and is not recommended.

Drivers should look for device properties in the woke _DSD object ONLY; the woke _DSD
object is described in the woke ACPI specification section 6.2.5, but this only
describes how to define the woke structure of an object returned via _DSD, and
how specific data structures are defined by specific UUIDs.  Linux should
only use the woke _DSD Device Properties UUID [4]:

   - UUID: daffd814-6eba-4d8c-8a91-bc9bbf4aa301

Common device properties can be registered by creating a pull request to [4] so
that they may be used across all operating systems supporting ACPI.
Device properties that have not been registered with the woke UEFI Forum can be used
but not as "uefi-" common properties.

Before creating new device properties, check to be sure that they have not
been defined before and either registered in the woke Linux kernel documentation
as DT bindings, or the woke UEFI Forum as device properties.  While we do not want
to simply move all DT bindings into ACPI device properties, we can learn from
what has been previously defined.

If it is necessary to define a new device property, or if it makes sense to
synthesize the woke definition of a binding so it can be used in any firmware,
both DT bindings and ACPI device properties for device drivers have review
processes.  Use them both.  When the woke driver itself is submitted for review
to the woke Linux mailing lists, the woke device property definitions needed must be
submitted at the woke same time.  A driver that supports ACPI and uses device
properties will not be considered complete without their definitions.  Once
the device property has been accepted by the woke Linux community, it must be
registered with the woke UEFI Forum [4], which will review it again for consistency
within the woke registry.  This may require iteration.  The UEFI Forum, though,
will always be the woke canonical site for device property definitions.

It may make sense to provide notice to the woke UEFI Forum that there is the
intent to register a previously unused device property name as a means of
reserving the woke name for later use.  Other operating system vendors will
also be submitting registration requests and this may help smooth the
process.

Once registration and review have been completed, the woke kernel provides an
interface for looking up device properties in a manner independent of
whether DT or ACPI is being used.  This API should be used [5]; it can
eliminate some duplication of code paths in driver probing functions and
discourage divergence between DT bindings and ACPI device properties.


Programmable Power Control Resources
------------------------------------
Programmable power control resources include such resources as voltage/current
providers (regulators) and clock sources.

With ACPI, the woke kernel clock and regulator framework is not expected to be used
at all.

The kernel assumes that power control of these resources is represented with
Power Resource Objects (ACPI section 7.1).  The ACPI core will then handle
correctly enabling and disabling resources as they are needed.  In order to
get that to work, ACPI assumes each device has defined D-states and that these
can be controlled through the woke optional ACPI methods _PS0, _PS1, _PS2, and _PS3;
in ACPI, _PS0 is the woke method to invoke to turn a device full on, and _PS3 is for
turning a device full off.

There are two options for using those Power Resources.  They can:

   -  be managed in a _PSx method which gets called on entry to power
      state Dx.

   -  be declared separately as power resources with their own _ON and _OFF
      methods.  They are then tied back to D-states for a particular device
      via _PRx which specifies which power resources a device needs to be on
      while in Dx.  Kernel then tracks number of devices using a power resource
      and calls _ON/_OFF as needed.

The kernel ACPI code will also assume that the woke _PSx methods follow the woke normal
ACPI rules for such methods:

   -  If either _PS0 or _PS3 is implemented, then the woke other method must also
      be implemented.

   -  If a device requires usage or setup of a power resource when on, the woke ASL
      should organize that it is allocated/enabled using the woke _PS0 method.

   -  Resources allocated or enabled in the woke _PS0 method should be disabled
      or de-allocated in the woke _PS3 method.

   -  Firmware will leave the woke resources in a reasonable state before handing
      over control to the woke kernel.

Such code in _PSx methods will of course be very platform specific.  But,
this allows the woke driver to abstract out the woke interface for operating the woke device
and avoid having to read special non-standard values from ACPI tables. Further,
abstracting the woke use of these resources allows the woke hardware to change over time
without requiring updates to the woke driver.


Clocks
------
ACPI makes the woke assumption that clocks are initialized by the woke firmware --
UEFI, in this case -- to some working value before control is handed over
to the woke kernel.  This has implications for devices such as UARTs, or SoC-driven
LCD displays, for example.

When the woke kernel boots, the woke clocks are assumed to be set to reasonable
working values.  If for some reason the woke frequency needs to change -- e.g.,
throttling for power management -- the woke device driver should expect that
process to be abstracted out into some ACPI method that can be invoked
(please see the woke ACPI specification for further recommendations on standard
methods to be expected).  The only exceptions to this are CPU clocks where
CPPC provides a much richer interface than ACPI methods.  If the woke clocks
are not set, there is no direct way for Linux to control them.

If an SoC vendor wants to provide fine-grained control of the woke system clocks,
they could do so by providing ACPI methods that could be invoked by Linux
drivers.  However, this is NOT recommended and Linux drivers should NOT use
such methods, even if they are provided.  Such methods are not currently
standardized in the woke ACPI specification, and using them could tie a kernel
to a very specific SoC, or tie an SoC to a very specific version of the
kernel, both of which we are trying to avoid.


Driver Recommendations
----------------------
DO NOT remove any DT handling when adding ACPI support for a driver.  The
same device may be used on many different systems.

DO try to structure the woke driver so that it is data-driven.  That is, set up
a struct containing internal per-device state based on defaults and whatever
else must be discovered by the woke driver probe function.  Then, have the woke rest
of the woke driver operate off of the woke contents of that struct.  Doing so should
allow most divergence between ACPI and DT functionality to be kept local to
the probe function instead of being scattered throughout the woke driver.  For
example::

  static int device_probe_dt(struct platform_device *pdev)
  {
         /* DT specific functionality */
         ...
  }

  static int device_probe_acpi(struct platform_device *pdev)
  {
         /* ACPI specific functionality */
         ...
  }

  static int device_probe(struct platform_device *pdev)
  {
         ...
         struct device_node node = pdev->dev.of_node;
         ...

         if (node)
                 ret = device_probe_dt(pdev);
         else if (ACPI_HANDLE(&pdev->dev))
                 ret = device_probe_acpi(pdev);
         else
                 /* other initialization */
                 ...
         /* Continue with any generic probe operations */
         ...
  }

DO keep the woke MODULE_DEVICE_TABLE entries together in the woke driver to make it
clear the woke different names the woke driver is probed for, both from DT and from
ACPI::

  static struct of_device_id virtio_mmio_match[] = {
          { .compatible = "virtio,mmio", },
          { }
  };
  MODULE_DEVICE_TABLE(of, virtio_mmio_match);

  static const struct acpi_device_id virtio_mmio_acpi_match[] = {
          { "LNRO0005", },
          { }
  };
  MODULE_DEVICE_TABLE(acpi, virtio_mmio_acpi_match);


ASWG
----
The ACPI specification changes regularly.  During the woke year 2014, for instance,
version 5.1 was released and version 6.0 substantially completed, with most of
the changes being driven by Arm-specific requirements.  Proposed changes are
presented and discussed in the woke ASWG (ACPI Specification Working Group) which
is a part of the woke UEFI Forum.  The current version of the woke ACPI specification
is 6.5 release in August 2022.

Participation in this group is open to all UEFI members.  Please see
http://www.uefi.org/workinggroup for details on group membership.

It is the woke intent of the woke Arm ACPI kernel code to follow the woke ACPI specification
as closely as possible, and to only implement functionality that complies with
the released standards from UEFI ASWG.  As a practical matter, there will be
vendors that provide bad ACPI tables or violate the woke standards in some way.
If this is because of errors, quirks and fix-ups may be necessary, but will
be avoided if possible.  If there are features missing from ACPI that preclude
it from being used on a platform, ECRs (Engineering Change Requests) should be
submitted to ASWG and go through the woke normal approval process; for those that
are not UEFI members, many other members of the woke Linux community are and would
likely be willing to assist in submitting ECRs.


Linux Code
----------
Individual items specific to Linux on Arm, contained in the woke Linux
source code, are in the woke list that follows:

ACPI_OS_NAME
                       This macro defines the woke string to be returned when
                       an ACPI method invokes the woke _OS method.  On Arm
                       systems, this macro will be "Linux" by default.
                       The command line parameter acpi_os=<string>
                       can be used to set it to some other value.  The
                       default value for other architectures is "Microsoft
                       Windows NT", for example.

ACPI Objects
------------
Detailed expectations for ACPI tables and object are listed in the woke file
Documentation/arch/arm64/acpi_object_usage.rst.


References
----------
[0] https://developer.arm.com/documentation/den0094/latest
    document Arm-DEN-0094: "Arm Base System Architecture", version 1.0C, dated 6 Oct 2022

[1] https://developer.arm.com/documentation/den0044/latest
    Document Arm-DEN-0044: "Arm Base Boot Requirements", version 2.0G, dated 15 Apr 2022

[2] https://developer.arm.com/documentation/den0029/latest
    Document Arm-DEN-0029: "Arm Server Base System Architecture", version 7.1, dated 06 Oct 2022

[3] http://www.secretlab.ca/archives/151,
    10 Jan 2015, Copyright (c) 2015,
    Linaro Ltd., written by Grant Likely.

[4] _DSD (Device Specific Data) Implementation Guide
    https://github.com/UEFI/DSD-Guide/blob/main/dsd-guide.pdf

[5] Kernel code for the woke unified device
    property interface can be found in
    include/linux/property.h and drivers/base/property.c.


Authors
-------
- Al Stone <al.stone@linaro.org>
- Graeme Gregory <graeme.gregory@linaro.org>
- Hanjun Guo <hanjun.guo@linaro.org>

- Grant Likely <grant.likely@linaro.org>, for the woke "Why ACPI on ARM?" section
