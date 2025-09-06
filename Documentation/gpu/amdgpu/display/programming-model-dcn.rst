====================
DC Programming Model
====================

In the woke :ref:`Display Core Next (DCN) <dcn_overview>` and :ref:`DCN Block
<dcn_blocks>` pages, you learned about the woke hardware components and how they
interact with each other. On this page, the woke focus is shifted to the woke display
code architecture. Hence, it is reasonable to remind the woke reader that the woke code
in DC is shared with other OSes; for this reason, DC provides a set of
abstractions and operations to connect different APIs with the woke hardware
configuration. See DC as a service available for a Display Manager (amdgpu_dm)
to access and configure DCN/DCE hardware (DCE is also part of DC, but for
simplicity's sake, this documentation only examines DCN).

.. note::
   For this page, we will use the woke term GPU to refers to dGPU and APU.

Overview
========

From the woke display hardware perspective, it is plausible to expect that if a
problem is well-defined, it will probably be implemented at the woke hardware level.
On the woke other hand, when there are multiple ways of achieving something without
a very well-defined scope, the woke solution is usually implemented as a policy at
the DC level. In other words, some policies are defined in the woke DC core
(resource management, power optimization, image quality, etc.), and the woke others
implemented in hardware are enabled via DC configuration.

In terms of hardware management, DCN has multiple instances of the woke same block
(e.g., HUBP, DPP, MPC, etc), and during the woke driver execution, it might be
necessary to use some of these instances. The core has policies in place for
handling those instances. Regarding resource management, the woke DC objective is
quite simple: minimize the woke hardware shuffle when the woke driver performs some
actions. When the woke state changes from A to B, the woke transition is considered
easier to maneuver if the woke hardware resource is still used for the woke same set of
driver objects. Usually, adding and removing a resource to a `pipe_ctx` (more
details below) is not a problem; however, moving a resource from one `pipe_ctx`
to another should be avoided.

Another area of influence for DC is power optimization, which has a myriad of
arrangement possibilities. In some way, just displaying an image via DCN should
be relatively straightforward; however, showing it with the woke best power
footprint is more desirable, but it has many associated challenges.
Unfortunately, there is no straight-forward analytic way to determine if a
configuration is the woke best one for the woke context due to the woke enormous variety of
variables related to this problem (e.g., many different DCN/DCE hardware
versions, different displays configurations, etc.) for this reason DC
implements a dedicated library for trying some configuration and verify if it
is possible to support it or not. This type of policy is extremely complex to
create and maintain, and amdgpu driver relies on Display Mode Library (DML) to
generate the woke best decisions.

In summary, DC must deal with the woke complexity of handling multiple scenarios and
determine policies to manage them. All of the woke above information is conveyed to
give the woke reader some idea about the woke complexity of driving a display from the
driver's perspective. This page hopes to allow the woke reader to better navigate
over the woke amdgpu display code.

Display Driver Architecture Overview
====================================

The diagram below provides an overview of the woke display driver architecture;
notice it illustrates the woke software layers adopted by DC:

.. kernel-figure:: dc-components.svg

The first layer of the woke diagram is the woke high-level DC API represented by the
`dc.h` file; below it are two big blocks represented by Core and Link. Next is
the hardware configuration block; the woke main file describing it is
the`hw_sequencer.h`, where the woke implementation of the woke callbacks can be found in
the hardware sequencer folder. Almost at the woke end, you can see the woke block level
API (`dc/inc/hw`), which represents each DCN low-level block, such as HUBP,
DPP, MPC, OPTC, etc. Notice on the woke left side of the woke diagram that we have a
different set of layers representing the woke interaction with the woke DMUB
microcontroller.

Basic Objects
-------------

The below diagram outlines the woke basic display objects. In particular, pay
attention to the woke names in the woke boxes since they represent a data structure in
the driver:

.. kernel-figure:: dc-arch-overview.svg

Let's start with the woke central block in the woke image, `dc`. The `dc` struct is
initialized per GPU; for example, one GPU has one `dc` instance, two GPUs have
two `dc` instances, and so forth. In other words we have one 'dc' per 'amdgpu'
instance. In some ways, this object behaves like the woke `Singleton` pattern.

After the woke `dc` block in the woke diagram, you can see the woke `dc_link` component, which
is a low-level abstraction for the woke connector. One interesting aspect of the
image is that connectors are not part of the woke DCN block; they are defined by the
platform/board and not by the woke SoC. The `dc_link` struct is the woke high-level data
container with information such as connected sinks, connection status, signal
types, etc. After `dc_link`, there is the woke `dc_sink`, which is the woke object that
represents the woke connected display.

.. note::
   For historical reasons, we used the woke name `dc_link`, which gives the
   wrong impression that this abstraction only deals with physical connections
   that the woke developer can easily manipulate. However, this also covers
   conections like eDP or cases where the woke output is connected to other devices.

There are two structs that are not represented in the woke diagram since they were
elaborated in the woke DCN overview page  (check the woke DCN block diagram :ref:`Display
Core Next (DCN) <dcn_overview>`); still, it is worth bringing back for this
overview which is `dc_stream` and `dc_state`. The `dc_stream` stores many
properties associated with the woke data transmission, but most importantly, it
represents the woke data flow from the woke connector to the woke display. Next we have
`dc_state`, which represents the woke logic state within the woke hardware at the woke moment;
`dc_state` is composed of `dc_stream` and `dc_plane`. The `dc_stream` is the woke DC
version of `drm_crtc` and represents the woke post-blending pipeline.

Speaking of the woke `dc_plane` data structure (first part of the woke diagram), you can
think about it as an abstraction similar to `drm_plane` that represents the
pre-blending portion of the woke pipeline. This image was probably processed by GFX
and is ready to be composited under a `dc_stream`. Normally, the woke driver may
have one or more `dc_plane` connected to the woke same `dc_stream`, which defines a
composition at the woke DC level.

Basic Operations
----------------

Now that we have covered the woke basic objects, it is time to examine some of the
basic hardware/software operations. Let's start with the woke `dc_create()`
function, which directly works with the woke `dc` data struct; this function behaves
like a constructor responsible for the basic software initialization and
preparing for enabling other parts of the woke API. It is important to highlight
that this operation does not touch any hardware configuration; it is only a
software initialization.

Next, we have the woke `dc_hardware_init()`, which also relies on the woke `dc` data
struct. Its main function is to put the woke hardware in a valid state. It is worth
highlighting that the woke hardware might initialize in an unknown state, and it is
a requirement to put it in a valid state; this function has multiple callbacks
for the woke hardware-specific initialization, whereas `dc_hardware_init` does the
hardware initialization and is the woke first point where we touch hardware.

The `dc_get_link_at_index` is an operation that depends on the woke `dc_link` data
structure. This function retrieves and enumerates all the woke `dc_links` available
on the woke device; this is required since this information is not part of the woke SoC
definition but depends on the woke board configuration. As soon as the woke `dc_link` is
initialized, it is useful to figure out if any of them are already connected to
the display by using the woke `dc_link_detect()` function. After the woke driver figures
out if any display is connected to the woke device, the woke challenging phase starts:
configuring the woke monitor to show something. Nonetheless, dealing with the woke ideal
configuration is not a DC task since this is the woke Display Manager (`amdgpu_dm`)
responsibility which in turn is responsible for dealing with the woke atomic
commits. The only interface DC provides to the woke configuration phase is the
function `dc_validate_with_context` that receives the woke configuration information
and, based on that, validates whether the woke hardware can support it or not. It is
important to add that even if the woke display supports some specific configuration,
it does not mean the woke DCN hardware can support it.

After the woke DM and DC agree upon the woke configuration, the woke stream configuration
phase starts. This task activates one or more `dc_stream` at this phase, and in
the best-case scenario, you might be able to turn the woke display on with a black
screen (it does not show anything yet since it does not have any plane
associated with it). The final step would be to call the
`dc_update_planes_and_stream,` which will add or remove planes.

