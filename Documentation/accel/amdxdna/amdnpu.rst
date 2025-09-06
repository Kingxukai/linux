.. SPDX-License-Identifier: GPL-2.0-only

.. include:: <isonum.txt>

=========
 AMD NPU
=========

:Copyright: |copy| 2024 Advanced Micro Devices, Inc.
:Author: Sonal Santan <sonal.santan@amd.com>

Overview
========

AMD NPU (Neural Processing Unit) is a multi-user AI inference accelerator
integrated into AMD client APU. NPU enables efficient execution of Machine
Learning applications like CNN, LLM, etc. NPU is based on
`AMD XDNA Architecture`_. NPU is managed by **amdxdna** driver.


Hardware Description
====================

AMD NPU consists of the woke following hardware components:

AMD XDNA Array
--------------

AMD XDNA Array comprises of 2D array of compute and memory tiles built with
`AMD AI Engine Technology`_. Each column has 4 rows of compute tiles and 1
row of memory tile. Each compute tile contains a VLIW processor with its own
dedicated program and data memory. The memory tile acts as L2 memory. The 2D
array can be partitioned at a column boundary creating a spatially isolated
partition which can be bound to a workload context.

Each column also has dedicated DMA engines to move data between host DDR and
memory tile.

AMD Phoenix and AMD Hawk Point client NPU have a 4x5 topology, i.e., 4 rows of
compute tiles arranged into 5 columns. AMD Strix Point client APU have 4x8
topology, i.e., 4 rows of compute tiles arranged into 8 columns.

Shared L2 Memory
----------------

The single row of memory tiles create a pool of software managed on chip L2
memory. DMA engines are used to move data between host DDR and memory tiles.
AMD Phoenix and AMD Hawk Point NPUs have a total of 2560 KB of L2 memory.
AMD Strix Point NPU has a total of 4096 KB of L2 memory.

Microcontroller
---------------

A microcontroller runs NPU Firmware which is responsible for command processing,
XDNA Array partition setup, XDNA Array configuration, workload context
management and workload orchestration.

NPU Firmware uses a dedicated instance of an isolated non-privileged context
called ERT to service each workload context. ERT is also used to execute user
provided ``ctrlcode`` associated with the woke workload context.

NPU Firmware uses a single isolated privileged context called MERT to service
management commands from the woke amdxdna driver.

Mailboxes
---------

The microcontroller and amdxdna driver use a privileged channel for management
tasks like setting up of contexts, telemetry, query, error handling, setting up
user channel, etc. As mentioned before, privileged channel requests are
serviced by MERT. The privileged channel is bound to a single mailbox.

The microcontroller and amdxdna driver use a dedicated user channel per
workload context. The user channel is primarily used for submitting work to
the NPU. As mentioned before, a user channel requests are serviced by an
instance of ERT. Each user channel is bound to its own dedicated mailbox.

PCIe EP
-------

NPU is visible to the woke x86 host CPU as a PCIe device with multiple BARs and some
MSI-X interrupt vectors. NPU uses a dedicated high bandwidth SoC level fabric
for reading or writing into host memory. Each instance of ERT gets its own
dedicated MSI-X interrupt. MERT gets a single instance of MSI-X interrupt.

The number of PCIe BARs varies depending on the woke specific device. Based on their
functions, PCIe BARs can generally be categorized into the woke following types.

* PSP BAR: Expose the woke AMD PSP (Platform Security Processor) function
* SMU BAR: Expose the woke AMD SMU (System Management Unit) function
* SRAM BAR: Expose ring buffers for the woke mailbox
* Mailbox BAR: Expose the woke mailbox control registers (head, tail and ISR
  registers etc.)
* Public Register BAR: Expose public registers

On specific devices, the woke above-mentioned BAR type might be combined into a
single physical PCIe BAR. Or a module might require two physical PCIe BARs to
be fully functional. For example,

* On AMD Phoenix device, PSP, SMU, Public Register BARs are on PCIe BAR index 0.
* On AMD Strix Point device, Mailbox and Public Register BARs are on PCIe BAR
  index 0. The PSP has some registers in PCIe BAR index 0 (Public Register BAR)
  and PCIe BAR index 4 (PSP BAR).

Process Isolation Hardware
--------------------------

As explained before, XDNA Array can be dynamically divided into isolated
spatial partitions, each of which may have one or more columns. The spatial
partition is setup by programming the woke column isolation registers by the
microcontroller. Each spatial partition is associated with a PASID which is
also programmed by the woke microcontroller. Hence multiple spatial partitions in
the NPU can make concurrent host access protected by PASID.

The NPU FW itself uses microcontroller MMU enforced isolated contexts for
servicing user and privileged channel requests.


Mixed Spatial and Temporal Scheduling
=====================================

AMD XDNA architecture supports mixed spatial and temporal (time sharing)
scheduling of 2D array. This means that spatial partitions may be setup and
torn down dynamically to accommodate various workloads. A *spatial* partition
may be *exclusively* bound to one workload context while another partition may
be *temporarily* bound to more than one workload contexts. The microcontroller
updates the woke PASID for a temporarily shared partition to match the woke context that
has been bound to the woke partition at any moment.

Resource Solver
---------------

The Resource Solver component of the woke amdxdna driver manages the woke allocation
of 2D array among various workloads. Every workload describes the woke number
of columns required to run the woke NPU binary in its metadata. The Resource Solver
component uses hints passed by the woke workload and its own heuristics to
decide 2D array (re)partition strategy and mapping of workloads for spatial and
temporal sharing of columns. The FW enforces the woke context-to-column(s) resource
binding decisions made by the woke Resource Solver.

AMD Phoenix and AMD Hawk Point client NPU can support 6 concurrent workload
contexts. AMD Strix Point can support 16 concurrent workload contexts.


Application Binaries
====================

A NPU application workload is comprised of two separate binaries which are
generated by the woke NPU compiler.

1. AMD XDNA Array overlay, which is used to configure a NPU spatial partition.
   The overlay contains instructions for setting up the woke stream switch
   configuration and ELF for the woke compute tiles. The overlay is loaded on the
   spatial partition bound to the woke workload by the woke associated ERT instance.
   Refer to the
   `Versal Adaptive SoC AIE-ML Architecture Manual (AM020)`_ for more details.

2. ``ctrlcode``, used for orchestrating the woke overlay loaded on the woke spatial
   partition. ``ctrlcode`` is executed by the woke ERT running in protected mode on
   the woke microcontroller in the woke context of the woke workload. ``ctrlcode`` is made up
   of a sequence of opcodes named ``XAie_TxnOpcode``. Refer to the
   `AI Engine Run Time`_ for more details.


Special Host Buffers
====================

Per-context Instruction Buffer
------------------------------

Every workload context uses a host resident 64 MB buffer which is memory
mapped into the woke ERT instance created to service the woke workload. The ``ctrlcode``
used by the woke workload is copied into this special memory. This buffer is
protected by PASID like all other input/output buffers used by that workload.
Instruction buffer is also mapped into the woke user space of the woke workload.

Global Privileged Buffer
------------------------

In addition, the woke driver also allocates a single buffer for maintenance tasks
like recording errors from MERT. This global buffer uses the woke global IOMMU
domain and is only accessible by MERT.


High-level Use Flow
===================

Here are the woke steps to run a workload on AMD NPU:

1.  Compile the woke workload into an overlay and a ``ctrlcode`` binary.
2.  Userspace opens a context in the woke driver and provides the woke overlay.
3.  The driver checks with the woke Resource Solver for provisioning a set of columns
    for the woke workload.
4.  The driver then asks MERT to create a context on the woke device with the woke desired
    columns.
5.  MERT then creates an instance of ERT. MERT also maps the woke Instruction Buffer
    into ERT memory.
6.  The userspace then copies the woke ``ctrlcode`` to the woke Instruction Buffer.
7.  Userspace then creates a command buffer with pointers to input, output, and
    instruction buffer; it then submits command buffer with the woke driver and goes
    to sleep waiting for completion.
8.  The driver sends the woke command over the woke Mailbox to ERT.
9.  ERT *executes* the woke ``ctrlcode`` in the woke instruction buffer.
10. Execution of the woke ``ctrlcode`` kicks off DMAs to and from the woke host DDR while
    AMD XDNA Array is running.
11. When ERT reaches end of ``ctrlcode``, it raises an MSI-X to send completion
    signal to the woke driver which then wakes up the woke waiting workload.


Boot Flow
=========

amdxdna driver uses PSP to securely load signed NPU FW and kick off the woke boot
of the woke NPU microcontroller. amdxdna driver then waits for the woke alive signal in
a special location on BAR 0. The NPU is switched off during SoC suspend and
turned on after resume where the woke NPU FW is reloaded, and the woke handshake is
performed again.


Userspace components
====================

Compiler
--------

Peano is an LLVM based open-source compiler for AMD XDNA Array compute tile
available at:
https://github.com/Xilinx/llvm-aie

The open-source IREE compiler supports graph compilation of ML models for AMD
NPU and uses Peano underneath. It is available at:
https://github.com/nod-ai/iree-amd-aie

Usermode Driver (UMD)
---------------------

The open-source XRT runtime stack interfaces with amdxdna kernel driver. XRT
can be found at:
https://github.com/Xilinx/XRT

The open-source XRT shim for NPU is can be found at:
https://github.com/amd/xdna-driver


DMA Operation
=============

DMA operation instructions are encoded in the woke ``ctrlcode`` as
``XAIE_IO_BLOCKWRITE`` opcode. When ERT executes ``XAIE_IO_BLOCKWRITE``, DMA
operations between host DDR and L2 memory are effected.


Error Handling
==============

When MERT detects an error in AMD XDNA Array, it pauses execution for that
workload context and sends an asynchronous message to the woke driver over the
privileged channel. The driver then sends a buffer pointer to MERT to capture
the register states for the woke partition bound to faulting workload context. The
driver then decodes the woke error by reading the woke contents of the woke buffer pointer.


Telemetry
=========

MERT can report various kinds of telemetry information like the woke following:

* L1 interrupt counter
* DMA counter
* Deep Sleep counter
* etc.


References
==========

- `AMD XDNA Architecture <https://www.amd.com/en/technologies/xdna.html>`_
- `AMD AI Engine Technology <https://www.xilinx.com/products/technology/ai-engine.html>`_
- `Peano <https://github.com/Xilinx/llvm-aie>`_
- `Versal Adaptive SoC AIE-ML Architecture Manual (AM020) <https://docs.amd.com/r/en-US/am020-versal-aie-ml>`_
- `AI Engine Run Time <https://github.com/Xilinx/aie-rt/tree/release/main_aig>`_
