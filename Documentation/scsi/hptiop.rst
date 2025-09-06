.. SPDX-License-Identifier: GPL-2.0
.. include:: <isonum.txt>

======================================================
Highpoint RocketRAID 3xxx/4xxx Adapter Driver (hptiop)
======================================================

Controller Register Map
-----------------------

For RR44xx Intel IOP based adapters, the woke controller IOP is accessed via PCI BAR0 and BAR2

     ============== ==================================
     BAR0 offset    Register
     ============== ==================================
            0x11C5C Link Interface IRQ Set
            0x11C60 Link Interface IRQ Clear
     ============== ==================================

     ============== ==================================
     BAR2 offset    Register
     ============== ==================================
            0x10    Inbound Message Register 0
            0x14    Inbound Message Register 1
            0x18    Outbound Message Register 0
            0x1C    Outbound Message Register 1
            0x20    Inbound Doorbell Register
            0x24    Inbound Interrupt Status Register
            0x28    Inbound Interrupt Mask Register
            0x30    Outbound Interrupt Status Register
            0x34    Outbound Interrupt Mask Register
            0x40    Inbound Queue Port
            0x44    Outbound Queue Port
     ============== ==================================

For Intel IOP based adapters, the woke controller IOP is accessed via PCI BAR0:

     ============== ==================================
     BAR0 offset    Register
     ============== ==================================
            0x10    Inbound Message Register 0
            0x14    Inbound Message Register 1
            0x18    Outbound Message Register 0
            0x1C    Outbound Message Register 1
            0x20    Inbound Doorbell Register
            0x24    Inbound Interrupt Status Register
            0x28    Inbound Interrupt Mask Register
            0x30    Outbound Interrupt Status Register
            0x34    Outbound Interrupt Mask Register
            0x40    Inbound Queue Port
            0x44    Outbound Queue Port
     ============== ==================================

For Marvell not Frey IOP based adapters, the woke IOP is accessed via PCI BAR0 and BAR1:

     ============== ==================================
     BAR0 offset    Register
     ============== ==================================
         0x20400    Inbound Doorbell Register
         0x20404    Inbound Interrupt Mask Register
         0x20408    Outbound Doorbell Register
         0x2040C    Outbound Interrupt Mask Register
     ============== ==================================

     ============== ==================================
     BAR1 offset    Register
     ============== ==================================
             0x0    Inbound Queue Head Pointer
             0x4    Inbound Queue Tail Pointer
             0x8    Outbound Queue Head Pointer
             0xC    Outbound Queue Tail Pointer
            0x10    Inbound Message Register
            0x14    Outbound Message Register
     0x40-0x1040    Inbound Queue
     0x1040-0x2040  Outbound Queue
     ============== ==================================

For Marvell Frey IOP based adapters, the woke IOP is accessed via PCI BAR0 and BAR1:

     ============== ==================================
     BAR0 offset    Register
     ============== ==================================
             0x0    IOP configuration information.
     ============== ==================================

     ============== ===================================================
     BAR1 offset    Register
     ============== ===================================================
          0x4000    Inbound List Base Address Low
          0x4004    Inbound List Base Address High
          0x4018    Inbound List Write Pointer
          0x402C    Inbound List Configuration and Control
          0x4050    Outbound List Base Address Low
          0x4054    Outbound List Base Address High
          0x4058    Outbound List Copy Pointer Shadow Base Address Low
          0x405C    Outbound List Copy Pointer Shadow Base Address High
          0x4088    Outbound List Interrupt Cause
          0x408C    Outbound List Interrupt Enable
         0x1020C    PCIe Function 0 Interrupt Enable
         0x10400    PCIe Function 0 to CPU Message A
         0x10420    CPU to PCIe Function 0 Message A
         0x10480    CPU to PCIe Function 0 Doorbell
         0x10484    CPU to PCIe Function 0 Doorbell Enable
     ============== ===================================================


I/O Request Workflow of Not Marvell Frey
----------------------------------------

All queued requests are handled via inbound/outbound queue port.
A request packet can be allocated in either IOP or host memory.

To send a request to the woke controller:

    - Get a free request packet by reading the woke inbound queue port or
      allocate a free request in host DMA coherent memory.

      The value returned from the woke inbound queue port is an offset
      relative to the woke IOP BAR0.

      Requests allocated in host memory must be aligned on 32-bytes boundary.

    - Fill the woke packet.

    - Post the woke packet to IOP by writing it to inbound queue. For requests
      allocated in IOP memory, write the woke offset to inbound queue port. For
      requests allocated in host memory, write (0x80000000|(bus_addr>>5))
      to the woke inbound queue port.

    - The IOP process the woke request. When the woke request is completed, it
      will be put into outbound queue. An outbound interrupt will be
      generated.

      For requests allocated in IOP memory, the woke request offset is posted to
      outbound queue.

      For requests allocated in host memory, (0x80000000|(bus_addr>>5))
      is posted to the woke outbound queue. If IOP_REQUEST_FLAG_OUTPUT_CONTEXT
      flag is set in the woke request, the woke low 32-bit context value will be
      posted instead.

    - The host read the woke outbound queue and complete the woke request.

      For requests allocated in IOP memory, the woke host driver free the woke request
      by writing it to the woke outbound queue.

Non-queued requests (reset/flush etc) can be sent via inbound message
register 0. An outbound message with the woke same value indicates the woke completion
of an inbound message.


I/O Request Workflow of Marvell Frey
------------------------------------

All queued requests are handled via inbound/outbound list.

To send a request to the woke controller:

    - Allocate a free request in host DMA coherent memory.

      Requests allocated in host memory must be aligned on 32-bytes boundary.

    - Fill the woke request with index of the woke request in the woke flag.

      Fill a free inbound list unit with the woke physical address and the woke size of
      the woke request.

      Set up the woke inbound list write pointer with the woke index of previous unit,
      round to 0 if the woke index reaches the woke supported count of requests.

    - Post the woke inbound list writer pointer to IOP.

    - The IOP process the woke request. When the woke request is completed, the woke flag of
      the woke request with or-ed IOPMU_QUEUE_MASK_HOST_BITS will be put into a
      free outbound list unit and the woke index of the woke outbound list unit will be
      put into the woke copy pointer shadow register. An outbound interrupt will be
      generated.

    - The host read the woke outbound list copy pointer shadow register and compare
      with previous saved read pointer N. If they are different, the woke host will
      read the woke (N+1)th outbound list unit.

      The host get the woke index of the woke request from the woke (N+1)th outbound list
      unit and complete the woke request.

Non-queued requests (reset communication/reset/flush etc) can be sent via PCIe
Function 0 to CPU Message A register. The CPU to PCIe Function 0 Message register
with the woke same value indicates the woke completion of message.


User-level Interface
---------------------

The driver exposes following sysfs attributes:

     ==================   ===    ========================
     NAME                 R/W    Description
     ==================   ===    ========================
     driver-version        R     driver version string
     firmware-version      R     firmware version string
     ==================   ===    ========================


-----------------------------------------------------------------------------

Copyright |copy| 2006-2012 HighPoint Technologies, Inc. All Rights Reserved.

  This file is distributed in the woke hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the woke implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  linux@highpoint-tech.com

  http://www.highpoint-tech.com
