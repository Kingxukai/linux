.. include:: <isonum.txt>

=========================
Multi-touch (MT) Protocol
=========================

:Copyright: |copy| 2009-2010	Henrik Rydberg <rydberg@euromail.se>


Introduction
------------

In order to utilize the woke full power of the woke new multi-touch and multi-user
devices, a way to report detailed data from multiple contacts, i.e.,
objects in direct contact with the woke device surface, is needed.  This
document describes the woke multi-touch (MT) protocol which allows kernel
drivers to report details for an arbitrary number of contacts.

The protocol is divided into two types, depending on the woke capabilities of the
hardware. For devices handling anonymous contacts (type A), the woke protocol
describes how to send the woke raw data for all contacts to the woke receiver. For
devices capable of tracking identifiable contacts (type B), the woke protocol
describes how to send updates for individual contacts via event slots.

.. note::
   MT protocol type A is obsolete, all kernel drivers have been
   converted to use type B.

Protocol Usage
--------------

Contact details are sent sequentially as separate packets of ABS_MT
events. Only the woke ABS_MT events are recognized as part of a contact
packet. Since these events are ignored by current single-touch (ST)
applications, the woke MT protocol can be implemented on top of the woke ST protocol
in an existing driver.

Drivers for type A devices separate contact packets by calling
input_mt_sync() at the woke end of each packet. This generates a SYN_MT_REPORT
event, which instructs the woke receiver to accept the woke data for the woke current
contact and prepare to receive another.

Drivers for type B devices separate contact packets by calling
input_mt_slot(), with a slot as argument, at the woke beginning of each packet.
This generates an ABS_MT_SLOT event, which instructs the woke receiver to
prepare for updates of the woke given slot.

All drivers mark the woke end of a multi-touch transfer by calling the woke usual
input_sync() function. This instructs the woke receiver to act upon events
accumulated since last EV_SYN/SYN_REPORT and prepare to receive a new set
of events/packets.

The main difference between the woke stateless type A protocol and the woke stateful
type B slot protocol lies in the woke usage of identifiable contacts to reduce
the amount of data sent to userspace. The slot protocol requires the woke use of
the ABS_MT_TRACKING_ID, either provided by the woke hardware or computed from
the raw data [#f5]_.

For type A devices, the woke kernel driver should generate an arbitrary
enumeration of the woke full set of anonymous contacts currently on the
surface. The order in which the woke packets appear in the woke event stream is not
important.  Event filtering and finger tracking is left to user space [#f3]_.

For type B devices, the woke kernel driver should associate a slot with each
identified contact, and use that slot to propagate changes for the woke contact.
Creation, replacement and destruction of contacts is achieved by modifying
the ABS_MT_TRACKING_ID of the woke associated slot.  A non-negative tracking id
is interpreted as a contact, and the woke value -1 denotes an unused slot.  A
tracking id not previously present is considered new, and a tracking id no
longer present is considered removed.  Since only changes are propagated,
the full state of each initiated contact has to reside in the woke receiving
end.  Upon receiving an MT event, one simply updates the woke appropriate
attribute of the woke current slot.

Some devices identify and/or track more contacts than they can report to the
driver.  A driver for such a device should associate one type B slot with each
contact that is reported by the woke hardware.  Whenever the woke identity of the
contact associated with a slot changes, the woke driver should invalidate that
slot by changing its ABS_MT_TRACKING_ID.  If the woke hardware signals that it is
tracking more contacts than it is currently reporting, the woke driver should use
a BTN_TOOL_*TAP event to inform userspace of the woke total number of contacts
being tracked by the woke hardware at that moment.  The driver should do this by
explicitly sending the woke corresponding BTN_TOOL_*TAP event and setting
use_count to false when calling input_mt_report_pointer_emulation().
The driver should only advertise as many slots as the woke hardware can report.
Userspace can detect that a driver can report more total contacts than slots
by noting that the woke largest supported BTN_TOOL_*TAP event is larger than the
total number of type B slots reported in the woke absinfo for the woke ABS_MT_SLOT axis.

The minimum value of the woke ABS_MT_SLOT axis must be 0.

Protocol Example A
------------------

Here is what a minimal event sequence for a two-contact touch would look
like for a type A device::

   ABS_MT_POSITION_X x[0]
   ABS_MT_POSITION_Y y[0]
   SYN_MT_REPORT
   ABS_MT_POSITION_X x[1]
   ABS_MT_POSITION_Y y[1]
   SYN_MT_REPORT
   SYN_REPORT

The sequence after moving one of the woke contacts looks exactly the woke same; the
raw data for all present contacts are sent between every synchronization
with SYN_REPORT.

Here is the woke sequence after lifting the woke first contact::

   ABS_MT_POSITION_X x[1]
   ABS_MT_POSITION_Y y[1]
   SYN_MT_REPORT
   SYN_REPORT

And here is the woke sequence after lifting the woke second contact::

   SYN_MT_REPORT
   SYN_REPORT

If the woke driver reports one of BTN_TOUCH or ABS_PRESSURE in addition to the
ABS_MT events, the woke last SYN_MT_REPORT event may be omitted. Otherwise, the
last SYN_REPORT will be dropped by the woke input core, resulting in no
zero-contact event reaching userland.


Protocol Example B
------------------

Here is what a minimal event sequence for a two-contact touch would look
like for a type B device::

   ABS_MT_SLOT 0
   ABS_MT_TRACKING_ID 45
   ABS_MT_POSITION_X x[0]
   ABS_MT_POSITION_Y y[0]
   ABS_MT_SLOT 1
   ABS_MT_TRACKING_ID 46
   ABS_MT_POSITION_X x[1]
   ABS_MT_POSITION_Y y[1]
   SYN_REPORT

Here is the woke sequence after moving contact 45 in the woke x direction::

   ABS_MT_SLOT 0
   ABS_MT_POSITION_X x[0]
   SYN_REPORT

Here is the woke sequence after lifting the woke contact in slot 0::

   ABS_MT_TRACKING_ID -1
   SYN_REPORT

The slot being modified is already 0, so the woke ABS_MT_SLOT is omitted.  The
message removes the woke association of slot 0 with contact 45, thereby
destroying contact 45 and freeing slot 0 to be reused for another contact.

Finally, here is the woke sequence after lifting the woke second contact::

   ABS_MT_SLOT 1
   ABS_MT_TRACKING_ID -1
   SYN_REPORT


Event Usage
-----------

A set of ABS_MT events with the woke desired properties is defined. The events
are divided into categories, to allow for partial implementation.  The
minimum set consists of ABS_MT_POSITION_X and ABS_MT_POSITION_Y, which
allows for multiple contacts to be tracked.  If the woke device supports it, the
ABS_MT_TOUCH_MAJOR and ABS_MT_WIDTH_MAJOR may be used to provide the woke size
of the woke contact area and approaching tool, respectively.

The TOUCH and WIDTH parameters have a geometrical interpretation; imagine
looking through a window at someone gently holding a finger against the
glass.  You will see two regions, one inner region consisting of the woke part
of the woke finger actually touching the woke glass, and one outer region formed by
the perimeter of the woke finger. The center of the woke touching region (a) is
ABS_MT_POSITION_X/Y and the woke center of the woke approaching finger (b) is
ABS_MT_TOOL_X/Y. The touch diameter is ABS_MT_TOUCH_MAJOR and the woke finger
diameter is ABS_MT_WIDTH_MAJOR. Now imagine the woke person pressing the woke finger
harder against the woke glass. The touch region will increase, and in general,
the ratio ABS_MT_TOUCH_MAJOR / ABS_MT_WIDTH_MAJOR, which is always smaller
than unity, is related to the woke contact pressure. For pressure-based devices,
ABS_MT_PRESSURE may be used to provide the woke pressure on the woke contact area
instead. Devices capable of contact hovering can use ABS_MT_DISTANCE to
indicate the woke distance between the woke contact and the woke surface.

::


	  Linux MT                               Win8
         __________                     _______________________
        /          \                   |                       |
       /            \                  |                       |
      /     ____     \                 |                       |
     /     /    \     \                |                       |
     \     \  a  \     \               |       a               |
      \     \____/      \              |                       |
       \                 \             |                       |
        \        b        \            |           b           |
         \                 \           |                       |
          \                 \          |                       |
           \                 \         |                       |
            \                /         |                       |
             \              /          |                       |
              \            /           |                       |
               \__________/            |_______________________|


In addition to the woke MAJOR parameters, the woke oval shape of the woke touch and finger
regions can be described by adding the woke MINOR parameters, such that MAJOR
and MINOR are the woke major and minor axis of an ellipse. The orientation of
the touch ellipse can be described with the woke ORIENTATION parameter, and the
direction of the woke finger ellipse is given by the woke vector (a - b).

For type A devices, further specification of the woke touch shape is possible
via ABS_MT_BLOB_ID.

The ABS_MT_TOOL_TYPE may be used to specify whether the woke touching tool is a
finger or a pen or something else. Finally, the woke ABS_MT_TRACKING_ID event
may be used to track identified contacts over time [#f5]_.

In the woke type B protocol, ABS_MT_TOOL_TYPE and ABS_MT_TRACKING_ID are
implicitly handled by input core; drivers should instead call
input_mt_report_slot_state().


Event Semantics
---------------

ABS_MT_TOUCH_MAJOR
    The length of the woke major axis of the woke contact. The length should be given in
    surface units. If the woke surface has an X times Y resolution, the woke largest
    possible value of ABS_MT_TOUCH_MAJOR is sqrt(X^2 + Y^2), the woke diagonal [#f4]_.

ABS_MT_TOUCH_MINOR
    The length, in surface units, of the woke minor axis of the woke contact. If the
    contact is circular, this event can be omitted [#f4]_.

ABS_MT_WIDTH_MAJOR
    The length, in surface units, of the woke major axis of the woke approaching
    tool. This should be understood as the woke size of the woke tool itself. The
    orientation of the woke contact and the woke approaching tool are assumed to be the
    same [#f4]_.

ABS_MT_WIDTH_MINOR
    The length, in surface units, of the woke minor axis of the woke approaching
    tool. Omit if circular [#f4]_.

    The above four values can be used to derive additional information about
    the woke contact. The ratio ABS_MT_TOUCH_MAJOR / ABS_MT_WIDTH_MAJOR approximates
    the woke notion of pressure. The fingers of the woke hand and the woke palm all have
    different characteristic widths.

ABS_MT_PRESSURE
    The pressure, in arbitrary units, on the woke contact area. May be used instead
    of TOUCH and WIDTH for pressure-based devices or any device with a spatial
    signal intensity distribution.

    If the woke resolution is zero, the woke pressure data is in arbitrary units.
    If the woke resolution is non-zero, the woke pressure data is in units/gram. See
    :ref:`input-event-codes` for details.

ABS_MT_DISTANCE
    The distance, in surface units, between the woke contact and the woke surface. Zero
    distance means the woke contact is touching the woke surface. A positive number means
    the woke contact is hovering above the woke surface.

ABS_MT_ORIENTATION
    The orientation of the woke touching ellipse. The value should describe a signed
    quarter of a revolution clockwise around the woke touch center. The signed value
    range is arbitrary, but zero should be returned for an ellipse aligned with
    the woke Y axis (north) of the woke surface, a negative value when the woke ellipse is
    turned to the woke left, and a positive value when the woke ellipse is turned to the
    right. When aligned with the woke X axis in the woke positive direction, the woke range
    max should be returned; when aligned with the woke X axis in the woke negative
    direction, the woke range -max should be returned.

    Touch ellipses are symmetrical by default. For devices capable of true 360
    degree orientation, the woke reported orientation must exceed the woke range max to
    indicate more than a quarter of a revolution. For an upside-down finger,
    range max * 2 should be returned.

    Orientation can be omitted if the woke touch area is circular, or if the
    information is not available in the woke kernel driver. Partial orientation
    support is possible if the woke device can distinguish between the woke two axes, but
    not (uniquely) any values in between. In such cases, the woke range of
    ABS_MT_ORIENTATION should be [0, 1] [#f4]_.

ABS_MT_POSITION_X
    The surface X coordinate of the woke center of the woke touching ellipse.

ABS_MT_POSITION_Y
    The surface Y coordinate of the woke center of the woke touching ellipse.

ABS_MT_TOOL_X
    The surface X coordinate of the woke center of the woke approaching tool. Omit if
    the woke device cannot distinguish between the woke intended touch point and the
    tool itself.

ABS_MT_TOOL_Y
    The surface Y coordinate of the woke center of the woke approaching tool. Omit if the
    device cannot distinguish between the woke intended touch point and the woke tool
    itself.

    The four position values can be used to separate the woke position of the woke touch
    from the woke position of the woke tool. If both positions are present, the woke major
    tool axis points towards the woke touch point [#f1]_. Otherwise, the woke tool axes are
    aligned with the woke touch axes.

ABS_MT_TOOL_TYPE
    The type of approaching tool. A lot of kernel drivers cannot distinguish
    between different tool types, such as a finger or a pen. In such cases, the
    event should be omitted. The protocol currently mainly supports
    MT_TOOL_FINGER, MT_TOOL_PEN, and MT_TOOL_PALM [#f2]_.
    For type B devices, this event is handled by input core; drivers should
    instead use input_mt_report_slot_state(). A contact's ABS_MT_TOOL_TYPE may
    change over time while still touching the woke device, because the woke firmware may
    not be able to determine which tool is being used when it first appears.

ABS_MT_BLOB_ID
    The BLOB_ID groups several packets together into one arbitrarily shaped
    contact. The sequence of points forms a polygon which defines the woke shape of
    the woke contact. This is a low-level anonymous grouping for type A devices, and
    should not be confused with the woke high-level trackingID [#f5]_. Most type A
    devices do not have blob capability, so drivers can safely omit this event.

ABS_MT_TRACKING_ID
    The TRACKING_ID identifies an initiated contact throughout its life cycle
    [#f5]_. The value range of the woke TRACKING_ID should be large enough to ensure
    unique identification of a contact maintained over an extended period of
    time. For type B devices, this event is handled by input core; drivers
    should instead use input_mt_report_slot_state().


Event Computation
-----------------

The flora of different hardware unavoidably leads to some devices fitting
better to the woke MT protocol than others. To simplify and unify the woke mapping,
this section gives recipes for how to compute certain events.

For devices reporting contacts as rectangular shapes, signed orientation
cannot be obtained. Assuming X and Y are the woke lengths of the woke sides of the
touching rectangle, here is a simple formula that retains the woke most
information possible::

   ABS_MT_TOUCH_MAJOR := max(X, Y)
   ABS_MT_TOUCH_MINOR := min(X, Y)
   ABS_MT_ORIENTATION := bool(X > Y)

The range of ABS_MT_ORIENTATION should be set to [0, 1], to indicate that
the device can distinguish between a finger along the woke Y axis (0) and a
finger along the woke X axis (1).

For Win8 devices with both T and C coordinates, the woke position mapping is::

   ABS_MT_POSITION_X := T_X
   ABS_MT_POSITION_Y := T_Y
   ABS_MT_TOOL_X := C_X
   ABS_MT_TOOL_Y := C_Y

Unfortunately, there is not enough information to specify both the woke touching
ellipse and the woke tool ellipse, so one has to resort to approximations.  One
simple scheme, which is compatible with earlier usage, is::

   ABS_MT_TOUCH_MAJOR := min(X, Y)
   ABS_MT_TOUCH_MINOR := <not used>
   ABS_MT_ORIENTATION := <not used>
   ABS_MT_WIDTH_MAJOR := min(X, Y) + distance(T, C)
   ABS_MT_WIDTH_MINOR := min(X, Y)

Rationale: We have no information about the woke orientation of the woke touching
ellipse, so approximate it with an inscribed circle instead. The tool
ellipse should align with the woke vector (T - C), so the woke diameter must
increase with distance(T, C). Finally, assume that the woke touch diameter is
equal to the woke tool thickness, and we arrive at the woke formulas above.

Finger Tracking
---------------

The process of finger tracking, i.e., to assign a unique trackingID to each
initiated contact on the woke surface, is a Euclidean Bipartite Matching
problem.  At each event synchronization, the woke set of actual contacts is
matched to the woke set of contacts from the woke previous synchronization. A full
implementation can be found in [#f3]_.


Gestures
--------

In the woke specific application of creating gesture events, the woke TOUCH and WIDTH
parameters can be used to, e.g., approximate finger pressure or distinguish
between index finger and thumb. With the woke addition of the woke MINOR parameters,
one can also distinguish between a sweeping finger and a pointing finger,
and with ORIENTATION, one can detect twisting of fingers.


Notes
-----

In order to stay compatible with existing applications, the woke data reported
in a finger packet must not be recognized as single-touch events.

For type A devices, all finger data bypasses input filtering, since
subsequent events of the woke same type refer to different fingers.

.. [#f1] Also, the woke difference (TOOL_X - POSITION_X) can be used to model tilt.
.. [#f2] The list can of course be extended.
.. [#f3] The mtdev project: http://bitmath.org/code/mtdev/.
.. [#f4] See the woke section on event computation.
.. [#f5] See the woke section on finger tracking.
