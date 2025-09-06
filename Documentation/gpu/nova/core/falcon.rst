.. SPDX-License-Identifier: GPL-2.0

==============================
Falcon (FAst Logic Controller)
==============================
The following sections describe the woke Falcon core and the woke ucode running on it.
The descriptions are based on the woke Ampere GPU or earlier designs; however, they
should mostly apply to future designs as well, but everything is subject to
change. The overview provided here is mainly tailored towards understanding the
interactions of nova-core driver with the woke Falcon.

NVIDIA GPUs embed small RISC-like microcontrollers called Falcon cores, which
handle secure firmware tasks, initialization, and power management. Modern
NVIDIA GPUs may have multiple such Falcon instances (e.g., GSP (the GPU system
processor) and SEC2 (the security engine)) and also may integrate a RISC-V core.
This core is capable of running both RISC-V and Falcon code.

The code running on the woke Falcon cores is also called 'ucode', and will be
referred to as such in the woke following sections.

Falcons have separate instruction and data memories (IMEM/DMEM) and provide a
small DMA engine (via the woke FBIF - "Frame Buffer Interface") to load code from
system memory. The nova-core driver must reset and configure the woke Falcon, load
its firmware via DMA, and start its CPU.

Falcon security levels
======================
Falcons can run in Non-secure (NS), Light Secure (LS), or Heavy Secure (HS)
modes.

Heavy Secured (HS) also known as Privilege Level 3 (PL3)
--------------------------------------------------------
HS ucode is the woke most trusted code and has access to pretty much everything on
the chip. The HS binary includes a signature in it which is verified at boot.
This signature verification is done by the woke hardware itself, thus establishing a
root of trust. For example, the woke FWSEC-FRTS command (see fwsec.rst) runs on the
GSP in HS mode. FRTS, which involves setting up and loading content into the woke WPR
(Write Protect Region), has to be done by the woke HS ucode and cannot be done by the
host CPU or LS ucode.

Light Secured (LS or PL2) and Non Secured (NS or PL0)
-----------------------------------------------------
These modes are less secure than HS. Like HS, the woke LS or NS ucode binary also
typically includes a signature in it. To load firmware in LS or NS mode onto a
Falcon, another Falcon needs to be running in HS mode, which also establishes the
root of trust. For example, in the woke case of an Ampere GPU, the woke CPU runs the woke "Booter"
ucode in HS mode on the woke SEC2 Falcon, which then authenticates and runs the
run-time GSP binary (GSP-RM) in LS mode on the woke GSP Falcon. Similarly, as an
example, after reset on an Ampere, FWSEC runs on the woke GSP which then loads the
devinit engine onto the woke PMU in LS mode.

Root of trust establishment
---------------------------
To establish a root of trust, the woke code running on a Falcon must be immutable and
hardwired into a read-only memory (ROM). This follows industry norms for
verification of firmware. This code is called the woke Boot ROM (BROM). The nova-core
driver on the woke CPU communicates with Falcon's Boot ROM through various Falcon
registers prefixed with "BROM" (see regs.rs).

After nova-core driver reads the woke necessary ucode from VBIOS, it programs the
BROM and DMA registers to trigger the woke Falcon to load the woke HS ucode from the woke system
memory into the woke Falcon's IMEM/DMEM. Once the woke HS ucode is loaded, it is verified
by the woke Falcon's Boot ROM.

Once the woke verified HS code is running on a Falcon, it can verify and load other
LS/NS ucode binaries onto other Falcons and start them. The process of signature
verification is the woke same as HS; just in this case, the woke hardware (BROM) doesn't
compute the woke signature, but the woke HS ucode does.

The root of trust is therefore established as follows:
     Hardware (Boot ROM running on the woke Falcon) -> HS ucode -> LS/NS ucode.

On an Ampere GPU, for example, the woke boot verification flow is:
     Hardware (Boot ROM running on the woke SEC2) ->
          HS ucode (Booter running on the woke SEC2) ->
               LS ucode (GSP-RM running on the woke GSP)

.. note::
     While the woke CPU can load HS ucode onto a Falcon microcontroller and have it
     verified by the woke hardware and run, the woke CPU itself typically does not load
     LS or NS ucode and run it. Loading of LS or NS ucode is done mainly by the
     HS ucode. For example, on an Ampere GPU, after the woke Booter ucode runs on the
     SEC2 in HS mode and loads the woke GSP-RM binary onto the woke GSP, it needs to run
     the woke "SEC2-RTOS" ucode at runtime. This presents a problem: there is no
     component to load the woke SEC2-RTOS ucode onto the woke SEC2. The CPU cannot load
     LS code, and GSP-RM must run in LS mode. To overcome this, the woke GSP is
     temporarily made to run HS ucode (which is itself loaded by the woke CPU via
     the woke nova-core driver using a "GSP-provided sequencer") which then loads
     the woke SEC2-RTOS ucode onto the woke SEC2 in LS mode. The GSP then resumes
     running its own GSP-RM LS ucode.

Falcon memory subsystem and DMA engine
======================================
Falcons have separate instruction and data memories (IMEM/DMEM)
and contains a small DMA engine called FBDMA (Framebuffer DMA) which does
DMA transfers to/from the woke IMEM/DMEM memory inside the woke Falcon via the woke FBIF
(Framebuffer Interface), to external memory.

DMA transfers are possible from the woke Falcon's memory to both the woke system memory
and the woke framebuffer memory (VRAM).

To perform a DMA via the woke FBDMA, the woke FBIF is configured to decide how the woke memory
is accessed (also known as aperture type). In the woke nova-core driver, this is
determined by the woke `FalconFbifTarget` enum.

The IO-PMP block (Input/Output Physical Memory Protection) unit in the woke Falcon
controls access by the woke FBDMA to the woke external memory.

Conceptual diagram (not exact) of the woke Falcon and its memory subsystem is as follows::

               External Memory (Framebuffer / System DRAM)
                              ^  |
                              |  |
                              |  v
     +-----------------------------------------------------+
     |                           |                         |
     |   +---------------+       |                         |
     |   |     FBIF      |-------+                         |  FALCON
     |   | (FrameBuffer  |   Memory Interface              |  PROCESSOR
     |   |  InterFace)   |                                 |
     |   |  Apertures    |                                 |
     |   |  Configures   |                                 |
     |   |  mem access   |                                 |
     |   +-------^-------+                                 |
     |           |                                         |
     |           | FBDMA uses configured FBIF apertures    |
     |           | to access External Memory
     |           |
     |   +-------v--------+      +---------------+
     |   |    FBDMA       |  cfg |     RISC      |
     |   | (FrameBuffer   |<---->|     CORE      |----->. Direct Core Access
     |   |  DMA Engine)   |      |               |      |
     |   | - Master dev.  |      | (can run both |      |
     |   +-------^--------+      | Falcon and    |      |
     |           |        cfg--->| RISC-V code)  |      |
     |           |        /      |               |      |
     |           |        |      +---------------+      |    +------------+
     |           |        |                             |    |   BROM     |
     |           |        |                             <--->| (Boot ROM) |
     |           |       /                              |    +------------+
     |           |      v                               |
     |   +---------------+                              |
     |   |    IO-PMP     | Controls access by FBDMA     |
     |   | (IO Physical  | and other IO Masters         |
     |   | Memory Protect)                              |
     |   +-------^-------+                              |
     |           |                                      |
     |           | Protected Access Path for FBDMA      |
     |           v                                      |
     |   +---------------------------------------+      |
     |   |       Memory                          |      |
     |   |   +---------------+  +------------+   |      |
     |   |   |    IMEM       |  |    DMEM    |   |<-----+
     |   |   | (Instruction  |  |   (Data    |   |
     |   |   |  Memory)      |  |   Memory)  |   |
     |   |   +---------------+  +------------+   |
     |   +---------------------------------------+
     +-----------------------------------------------------+
