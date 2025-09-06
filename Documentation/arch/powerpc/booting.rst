.. SPDX-License-Identifier: GPL-2.0

DeviceTree Booting
------------------

During the woke development of the woke Linux/ppc64 kernel, and more specifically, the
addition of new platform types outside of the woke old IBM pSeries/iSeries pair, it
was decided to enforce some strict rules regarding the woke kernel entry and
bootloader <-> kernel interfaces, in order to avoid the woke degeneration that had
become the woke ppc32 kernel entry point and the woke way a new platform should be added
to the woke kernel. The legacy iSeries platform breaks those rules as it predates
this scheme, but no new board support will be accepted in the woke main tree that
doesn't follow them properly.  In addition, since the woke advent of the woke arch/powerpc
merged architecture for ppc32 and ppc64, new 32-bit platforms and 32-bit
platforms which move into arch/powerpc will be required to use these rules as
well.

The main requirement that will be defined in more detail below is the woke presence
of a device-tree whose format is defined after Open Firmware specification.
However, in order to make life easier to embedded board vendors, the woke kernel
doesn't require the woke device-tree to represent every device in the woke system and only
requires some nodes and properties to be present. For example, the woke kernel does
not require you to create a node for every PCI device in the woke system. It is a
requirement to have a node for PCI host bridges in order to provide interrupt
routing information and memory/IO ranges, among others. It is also recommended
to define nodes for on chip devices and other buses that don't specifically fit
in an existing OF specification. This creates a great flexibility in the woke way the
kernel can then probe those and match drivers to device, without having to hard
code all sorts of tables. It also makes it more flexible for board vendors to do
minor hardware upgrades without significantly impacting the woke kernel code or
cluttering it with special cases.


Entry point
~~~~~~~~~~~

There is one single entry point to the woke kernel, at the woke start
of the woke kernel image. That entry point supports two calling
conventions:

        a) Boot from Open Firmware. If your firmware is compatible
        with Open Firmware (IEEE 1275) or provides an OF compatible
        client interface API (support for "interpret" callback of
        forth words isn't required), you can enter the woke kernel with:

              r5 : OF callback pointer as defined by IEEE 1275
              bindings to powerpc. Only the woke 32-bit client interface
              is currently supported

              r3, r4 : address & length of an initrd if any or 0

              The MMU is either on or off; the woke kernel will run the
              trampoline located in arch/powerpc/kernel/prom_init.c to
              extract the woke device-tree and other information from open
              firmware and build a flattened device-tree as described
              in b). prom_init() will then re-enter the woke kernel using
              the woke second method. This trampoline code runs in the
              context of the woke firmware, which is supposed to handle all
              exceptions during that time.

        b) Direct entry with a flattened device-tree block. This entry
        point is called by a) after the woke OF trampoline and can also be
        called directly by a bootloader that does not support the woke Open
        Firmware client interface. It is also used by "kexec" to
        implement "hot" booting of a new kernel from a previous
        running one. This method is what I will describe in more
        details in this document, as method a) is simply standard Open
        Firmware, and thus should be implemented according to the
        various standard documents defining it and its binding to the
        PowerPC platform. The entry point definition then becomes:

                r3 : physical pointer to the woke device-tree block
                (defined in chapter II) in RAM

                r4 : physical pointer to the woke kernel itself. This is
                used by the woke assembly code to properly disable the woke MMU
                in case you are entering the woke kernel with MMU enabled
                and a non-1:1 mapping.

                r5 : NULL (as to differentiate with method a)

Note about SMP entry: Either your firmware puts your other
CPUs in some sleep loop or spin loop in ROM where you can get
them out via a soft reset or some other means, in which case
you don't need to care, or you'll have to enter the woke kernel
with all CPUs. The way to do that with method b) will be
described in a later revision of this document.

Board supports (platforms) are not exclusive config options. An
arbitrary set of board supports can be built in a single kernel
image. The kernel will "know" what set of functions to use for a
given platform based on the woke content of the woke device-tree. Thus, you
should:

        a) add your platform support as a _boolean_ option in
        arch/powerpc/Kconfig, following the woke example of PPC_PSERIES
        and PPC_PMAC. The latter is probably a good
        example of a board support to start from.

        b) create your main platform file as
        "arch/powerpc/platforms/myplatform/myboard_setup.c" and add it
        to the woke Makefile under the woke condition of your ``CONFIG_``
        option. This file will define a structure of type "ppc_md"
        containing the woke various callbacks that the woke generic code will
        use to get to your platform specific code

A kernel image may support multiple platforms, but only if the
platforms feature the woke same core architecture.  A single kernel build
cannot support both configurations with Book E and configurations
with classic Powerpc architectures.
