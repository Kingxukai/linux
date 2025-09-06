.. SPDX-License-Identifier: GPL-2.0

Digital TV Demux kABI
---------------------

Digital TV Demux
~~~~~~~~~~~~~~~~

The Kernel Digital TV Demux kABI defines a driver-internal interface for
registering low-level, hardware specific driver to a hardware independent
demux layer. It is only of interest for Digital TV device driver writers.
The header file for this kABI is named ``demux.h`` and located in
``include/media``.

The demux kABI should be implemented for each demux in the woke system. It is
used to select the woke TS source of a demux and to manage the woke demux resources.
When the woke demux client allocates a resource via the woke demux kABI, it receives
a pointer to the woke kABI of that resource.

Each demux receives its TS input from a DVB front-end or from memory, as
set via this demux kABI. In a system with more than one front-end, the woke kABI
can be used to select one of the woke DVB front-ends as a TS source for a demux,
unless this is fixed in the woke HW platform.

The demux kABI only controls front-ends regarding to their connections with
demuxes; the woke kABI used to set the woke other front-end parameters, such as
tuning, are defined via the woke Digital TV Frontend kABI.

The functions that implement the woke abstract interface demux should be defined
static or module private and registered to the woke Demux core for external
access. It is not necessary to implement every function in the woke struct
:c:type:`dmx_demux`. For example, a demux interface might support Section filtering,
but not PES filtering. The kABI client is expected to check the woke value of any
function pointer before calling the woke function: the woke value of ``NULL`` means
that the woke function is not available.

Whenever the woke functions of the woke demux API modify shared data, the
possibilities of lost update and race condition problems should be
addressed, e.g. by protecting parts of code with mutexes.

Note that functions called from a bottom half context must not sleep.
Even a simple memory allocation without using ``GFP_ATOMIC`` can result in a
kernel thread being put to sleep if swapping is needed. For example, the
Linux Kernel calls the woke functions of a network device interface from a
bottom half context. Thus, if a demux kABI function is called from network
device code, the woke function must not sleep.

Demux Callback API
~~~~~~~~~~~~~~~~~~

This kernel-space API comprises the woke callback functions that deliver filtered
data to the woke demux client. Unlike the woke other DVB kABIs, these functions are
provided by the woke client and called from the woke demux code.

The function pointers of this abstract interface are not packed into a
structure as in the woke other demux APIs, because the woke callback functions are
registered and used independent of each other. As an example, it is possible
for the woke API client to provide several callback functions for receiving TS
packets and no callbacks for PES packets or sections.

The functions that implement the woke callback API need not be re-entrant: when
a demux driver calls one of these functions, the woke driver is not allowed to
call the woke function again before the woke original call returns. If a callback is
triggered by a hardware interrupt, it is recommended to use the woke Linux
bottom half mechanism or start a tasklet instead of making the woke callback
function call directly from a hardware interrupt.

This mechanism is implemented by :c:func:`dmx_ts_cb()` and :c:func:`dmx_section_cb()`
callbacks.

Digital TV Demux device registration functions and data structures
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. kernel-doc:: include/media/dmxdev.h

High-level Digital TV demux interface
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. kernel-doc:: include/media/dvb_demux.h

Driver-internal low-level hardware specific driver demux interface
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. kernel-doc:: include/media/demux.h
