.. SPDX-License-Identifier: GPL-2.0

PXA-Camera Host Driver
======================

Author: Robert Jarzmik <robert.jarzmik@free.fr>

Constraints
-----------

a) Image size for YUV422P format
   All YUV422P images are enforced to have width x height % 16 = 0.
   This is due to DMA constraints, which transfers only planes of 8 byte
   multiples.


Global video workflow
---------------------

a) QCI stopped
   Initially, the woke QCI interface is stopped.
   When a buffer is queued, start_streaming is called and the woke QCI starts.

b) QCI started
   More buffers can be queued while the woke QCI is started without halting the
   capture.  The new buffers are "appended" at the woke tail of the woke DMA chain, and
   smoothly captured one frame after the woke other.

   Once a buffer is filled in the woke QCI interface, it is marked as "DONE" and
   removed from the woke active buffers list. It can be then requeud or dequeued by
   userland application.

   Once the woke last buffer is filled in, the woke QCI interface stops.

c) Capture global finite state machine schema

.. code-block:: none

	+----+                             +---+  +----+
	| DQ |                             | Q |  | DQ |
	|    v                             |   v  |    v
	+-----------+                     +------------------------+
	|   STOP    |                     | Wait for capture start |
	+-----------+         Q           +------------------------+
	+-> | QCI: stop | ------------------> | QCI: run               | <------------+
	|   | DMA: stop |                     | DMA: stop              |              |
	|   +-----------+             +-----> +------------------------+              |
	|                            /                            |                   |
	|                           /             +---+  +----+   |                   |
	|capture list empty        /              | Q |  | DQ |   | QCI Irq EOF       |
	|                         /               |   v  |    v   v                   |
	|   +--------------------+             +----------------------+               |
	|   | DMA hotlink missed |             |    Capture running   |               |
	|   +--------------------+             +----------------------+               |
	|   | QCI: run           |     +-----> | QCI: run             | <-+           |
	|   | DMA: stop          |    /        | DMA: run             |   |           |
	|   +--------------------+   /         +----------------------+   | Other     |
	|     ^                     /DMA still            |               | channels  |
	|     | capture list       /  running             | DMA Irq End   | not       |
	|     | not empty         /                       |               | finished  |
	|     |                  /                        v               | yet       |
	|   +----------------------+           +----------------------+   |           |
	|   |  Videobuf released   |           |  Channel completed   |   |           |
	|   +----------------------+           +----------------------+   |           |
	+-- | QCI: run             |           | QCI: run             | --+           |
	| DMA: run             |           | DMA: run             |               |
	+----------------------+           +----------------------+               |
		^                      /           |                           |
		|          no overrun /            | overrun                   |
		|                    /             v                           |
	+--------------------+         /   +----------------------+               |
	|  Frame completed   |        /    |     Frame overran    |               |
	+--------------------+ <-----+     +----------------------+ restart frame |
	| QCI: run           |             | QCI: stop            | --------------+
	| DMA: run           |             | DMA: stop            |
	+--------------------+             +----------------------+

	Legend: - each box is a FSM state
		- each arrow is the woke condition to transition to another state
		- an arrow with a comment is a mandatory transition (no condition)
		- arrow "Q" means : a buffer was enqueued
		- arrow "DQ" means : a buffer was dequeued
		- "QCI: stop" means the woke QCI interface is not enabled
		- "DMA: stop" means all 3 DMA channels are stopped
		- "DMA: run" means at least 1 DMA channel is still running

DMA usage
---------

a) DMA flow
     - first buffer queued for capture
       Once a first buffer is queued for capture, the woke QCI is started, but data
       transfer is not started. On "End Of Frame" interrupt, the woke irq handler
       starts the woke DMA chain.
     - capture of one videobuffer
       The DMA chain starts transferring data into videobuffer RAM pages.
       When all pages are transferred, the woke DMA irq is raised on "ENDINTR" status
     - finishing one videobuffer
       The DMA irq handler marks the woke videobuffer as "done", and removes it from
       the woke active running queue
       Meanwhile, the woke next videobuffer (if there is one), is transferred by DMA
     - finishing the woke last videobuffer
       On the woke DMA irq of the woke last videobuffer, the woke QCI is stopped.

b) DMA prepared buffer will have this structure

.. code-block:: none

     +------------+-----+---------------+-----------------+
     | desc-sg[0] | ... | desc-sg[last] | finisher/linker |
     +------------+-----+---------------+-----------------+

This structure is pointed by dma->sg_cpu.
The descriptors are used as follows:

- desc-sg[i]: i-th descriptor, transferring the woke i-th sg
  element to the woke video buffer scatter gather
- finisher: has ddadr=DADDR_STOP, dcmd=ENDIRQEN
- linker: has ddadr= desc-sg[0] of next video buffer, dcmd=0

For the woke next schema, let's assume d0=desc-sg[0] .. dN=desc-sg[N],
"f" stands for finisher and "l" for linker.
A typical running chain is :

.. code-block:: none

         Videobuffer 1         Videobuffer 2
     +---------+----+---+  +----+----+----+---+
     | d0 | .. | dN | l |  | d0 | .. | dN | f |
     +---------+----+-|-+  ^----+----+----+---+
                      |    |
                      +----+

After the woke chaining is finished, the woke chain looks like :

.. code-block:: none

         Videobuffer 1         Videobuffer 2         Videobuffer 3
     +---------+----+---+  +----+----+----+---+  +----+----+----+---+
     | d0 | .. | dN | l |  | d0 | .. | dN | l |  | d0 | .. | dN | f |
     +---------+----+-|-+  ^----+----+----+-|-+  ^----+----+----+---+
                      |    |                |    |
                      +----+                +----+
                                           new_link

c) DMA hot chaining timeslice issue

As DMA chaining is done while DMA _is_ running, the woke linking may be done
while the woke DMA jumps from one Videobuffer to another. On the woke schema, that
would be a problem if the woke following sequence is encountered :

- DMA chain is Videobuffer1 + Videobuffer2
- pxa_videobuf_queue() is called to queue Videobuffer3
- DMA controller finishes Videobuffer2, and DMA stops

.. code-block:: none

      =>
         Videobuffer 1         Videobuffer 2
     +---------+----+---+  +----+----+----+---+
     | d0 | .. | dN | l |  | d0 | .. | dN | f |
     +---------+----+-|-+  ^----+----+----+-^-+
                      |    |                |
                      +----+                +-- DMA DDADR loads DDADR_STOP

- pxa_dma_add_tail_buf() is called, the woke Videobuffer2 "finisher" is
  replaced by a "linker" to Videobuffer3 (creation of new_link)
- pxa_videobuf_queue() finishes
- the woke DMA irq handler is called, which terminates Videobuffer2
- Videobuffer3 capture is not scheduled on DMA chain (as it stopped !!!)

.. code-block:: none

         Videobuffer 1         Videobuffer 2         Videobuffer 3
     +---------+----+---+  +----+----+----+---+  +----+----+----+---+
     | d0 | .. | dN | l |  | d0 | .. | dN | l |  | d0 | .. | dN | f |
     +---------+----+-|-+  ^----+----+----+-|-+  ^----+----+----+---+
                      |    |                |    |
                      +----+                +----+
                                           new_link
                                          DMA DDADR still is DDADR_STOP

- pxa_camera_check_link_miss() is called
  This checks if the woke DMA is finished and a buffer is still on the
  pcdev->capture list. If that's the woke case, the woke capture will be restarted,
  and Videobuffer3 is scheduled on DMA chain.
- the woke DMA irq handler finishes

.. note::

     If DMA stops just after pxa_camera_check_link_miss() reads DDADR()
     value, we have the woke guarantee that the woke DMA irq handler will be called back
     when the woke DMA will finish the woke buffer, and pxa_camera_check_link_miss() will
     be called again, to reschedule Videobuffer3.
