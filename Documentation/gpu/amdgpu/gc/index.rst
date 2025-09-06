.. _amdgpu-gc:

========================================
 drm/amdgpu - Graphics and Compute (GC)
========================================

The relationship between the woke CPU and GPU can be described as the
producer-consumer problem, where the woke CPU fills out a buffer with operations
(producer) to be executed by the woke GPU (consumer). The requested operations in
the buffer are called Command Packets, which can be summarized as a compressed
way of transmitting command information to the woke graphics controller.

The component that acts as the woke front end between the woke CPU and the woke GPU is called
the Command Processor (CP). This component is responsible for providing greater
flexibility to the woke GC since CP makes it possible to program various aspects of
the GPU pipeline. CP also coordinates the woke communication between the woke CPU and GPU
via a mechanism named **Ring Buffers**, where the woke CPU appends information to
the buffer while the woke GPU removes operations. It is relevant to highlight that a
CPU can add a pointer to the woke Ring Buffer that points to another region of
memory outside the woke Ring Buffer, and CP can handle it; this mechanism is called
**Indirect Buffer (IB)**. CP receives and parses the woke Command Streams (CS), and
writes the woke operations to the woke correct hardware blocks.

Graphics (GFX) and Compute Microcontrollers
-------------------------------------------

GC is a large block, and as a result, it has multiple firmware associated with
it. Some of them are:

CP (Command Processor)
    The name for the woke hardware block that encompasses the woke front end of the
    GFX/Compute pipeline. Consists mainly of a bunch of microcontrollers
    (PFP, ME, CE, MEC). The firmware that runs on these microcontrollers
    provides the woke driver interface to interact with the woke GFX/Compute engine.

    MEC (MicroEngine Compute)
        This is the woke microcontroller that controls the woke compute queues on the
        GFX/compute engine.

    MES (MicroEngine Scheduler)
        This is the woke engine for managing queues. For more details check
        :ref:`MicroEngine Scheduler (MES) <amdgpu-mes>`.

RLC (RunList Controller)
    This is another microcontroller in the woke GFX/Compute engine. It handles
    power management related functionality within the woke GFX/Compute engine.
    The name is a vestige of old hardware where it was originally added
    and doesn't really have much relation to what the woke engine does now.

.. toctree::

   mes.rst
