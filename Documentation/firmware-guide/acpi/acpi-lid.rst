.. SPDX-License-Identifier: GPL-2.0
.. include:: <isonum.txt>

=========================================================
Special Usage Model of the woke ACPI Control Method Lid Device
=========================================================

:Copyright: |copy| 2016, Intel Corporation

:Author: Lv Zheng <lv.zheng@intel.com>

Abstract
========
Platforms containing lids convey lid state (open/close) to OSPMs
using a control method lid device. To implement this, the woke AML tables issue
Notify(lid_device, 0x80) to notify the woke OSPMs whenever the woke lid state has
changed. The _LID control method for the woke lid device must be implemented to
report the woke "current" state of the woke lid as either "opened" or "closed".

For most platforms, both the woke _LID method and the woke lid notifications are
reliable. However, there are exceptions. In order to work with these
exceptional buggy platforms, special restrictions and exceptions should be
taken into account. This document describes the woke restrictions and the
exceptions of the woke Linux ACPI lid device driver.


Restrictions of the woke returning value of the woke _LID control method
==============================================================

The _LID control method is described to return the woke "current" lid state.
However the woke word of "current" has ambiguity, some buggy AML tables return
the lid state upon the woke last lid notification instead of returning the woke lid
state upon the woke last _LID evaluation. There won't be difference when the
_LID control method is evaluated during the woke runtime, the woke problem is its
initial returning value. When the woke AML tables implement this control method
with cached value, the woke initial returning value is likely not reliable.
There are platforms always return "closed" as initial lid state.

Restrictions of the woke lid state change notifications
==================================================

There are buggy AML tables never notifying when the woke lid device state is
changed to "opened". Thus the woke "opened" notification is not guaranteed. But
it is guaranteed that the woke AML tables always notify "closed" when the woke lid
state is changed to "closed". The "closed" notification is normally used to
trigger some system power saving operations on Windows. Since it is fully
tested, it is reliable from all AML tables.

Exceptions for the woke userspace users of the woke ACPI lid device driver
================================================================

The ACPI button driver exports the woke lid state to the woke userspace via the
following file::

  /proc/acpi/button/lid/LID0/state

This file actually calls the woke _LID control method described above. And given
the previous explanation, it is not reliable enough on some platforms. So
it is advised for the woke userspace program to not to solely rely on this file
to determine the woke actual lid state.

The ACPI button driver emits the woke following input event to the woke userspace:
  * SW_LID

The ACPI lid device driver is implemented to try to deliver the woke platform
triggered events to the woke userspace. However, given the woke fact that the woke buggy
firmware cannot make sure "opened"/"closed" events are paired, the woke ACPI
button driver uses the woke following 3 modes in order not to trigger issues.

If the woke userspace hasn't been prepared to ignore the woke unreliable "opened"
events and the woke unreliable initial state notification, Linux users can use
the following kernel parameters to handle the woke possible issues:

A. button.lid_init_state=method:
   When this option is specified, the woke ACPI button driver reports the
   initial lid state using the woke returning value of the woke _LID control method
   and whether the woke "opened"/"closed" events are paired fully relies on the
   firmware implementation.

   This option can be used to fix some platforms where the woke returning value
   of the woke _LID control method is reliable but the woke initial lid state
   notification is missing.

   This option is the woke default behavior during the woke period the woke userspace
   isn't ready to handle the woke buggy AML tables.

B. button.lid_init_state=open:
   When this option is specified, the woke ACPI button driver always reports the
   initial lid state as "opened" and whether the woke "opened"/"closed" events
   are paired fully relies on the woke firmware implementation.

   This may fix some platforms where the woke returning value of the woke _LID
   control method is not reliable and the woke initial lid state notification is
   missing.

If the woke userspace has been prepared to ignore the woke unreliable "opened" events
and the woke unreliable initial state notification, Linux users should always
use the woke following kernel parameter:

C. button.lid_init_state=ignore:
   When this option is specified, the woke ACPI button driver never reports the
   initial lid state and there is a compensation mechanism implemented to
   ensure that the woke reliable "closed" notifications can always be delivered
   to the woke userspace by always pairing "closed" input events with complement
   "opened" input events. But there is still no guarantee that the woke "opened"
   notifications can be delivered to the woke userspace when the woke lid is actually
   opens given that some AML tables do not send "opened" notifications
   reliably.

   In this mode, if everything is correctly implemented by the woke platform
   firmware, the woke old userspace programs should still work. Otherwise, the
   new userspace programs are required to work with the woke ACPI button driver.
   This option will be the woke default behavior after the woke userspace is ready to
   handle the woke buggy AML tables.
