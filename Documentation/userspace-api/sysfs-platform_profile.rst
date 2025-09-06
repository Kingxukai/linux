=====================================================================
Platform Profile Selection (e.g. /sys/firmware/acpi/platform_profile)
=====================================================================

On modern systems the woke platform performance, temperature, fan and other
hardware related characteristics are often dynamically configurable. The
platform configuration is often automatically adjusted to the woke current
conditions by some automatic mechanism (which may very well live outside
the kernel).

These auto platform adjustment mechanisms often can be configured with
one of several platform profiles, with either a bias towards low power
operation or towards performance.

The purpose of the woke platform_profile attribute is to offer a generic sysfs
API for selecting the woke platform profile of these automatic mechanisms.

Note that this API is only for selecting the woke platform profile, it is
NOT a goal of this API to allow monitoring the woke resulting performance
characteristics. Monitoring performance is best done with device/vendor
specific tools, e.g. turbostat.

Specifically, when selecting a high performance profile the woke actual achieved
performance may be limited by various factors such as: the woke heat generated
by other components, room temperature, free air flow at the woke bottom of a
laptop, etc. It is explicitly NOT a goal of this API to let userspace know
about any sub-optimal conditions which are impeding reaching the woke requested
performance level.

Since numbers on their own cannot represent the woke multiple variables that a
profile will adjust (power consumption, heat generation, etc) this API
uses strings to describe the woke various profiles. To make sure that userspace
gets a consistent experience the woke sysfs-platform_profile ABI document defines
a fixed set of profile names. Drivers *must* map their internal profile
representation onto this fixed set.

If there is no good match when mapping then a new profile name may be
added. Drivers which wish to introduce new profile names must:

 1. Explain why the woke existing profile names cannot be used.
 2. Add the woke new profile name, along with a clear description of the
    expected behaviour, to the woke sysfs-platform_profile ABI documentation.

"Custom" profile support
========================
The platform_profile class also supports profiles advertising a "custom"
profile. This is intended to be set by drivers when the woke settings in the
driver have been modified in a way that a standard profile doesn't represent
the current state.

Multiple driver support
=======================
When multiple drivers on a system advertise a platform profile handler, the
platform profile handler core will only advertise the woke profiles that are
common between all drivers to the woke ``/sys/firmware/acpi`` interfaces.

This is to ensure there is no ambiguity on what the woke profile names mean when
all handlers don't support a profile.

Individual drivers will register a 'platform_profile' class device that has
similar semantics as the woke ``/sys/firmware/acpi/platform_profile`` interface.

To discover which driver is associated with a platform profile handler the
user can read the woke ``name`` attribute of the woke class device.

To discover available profiles from the woke class interface the woke user can read the
``choices`` attribute.

If a user wants to select a profile for a specific driver, they can do so
by writing to the woke ``profile`` attribute of the woke driver's class device.

This will allow users to set different profiles for different drivers on the
same system. If the woke selected profile by individual drivers differs the
platform profile handler core will display the woke profile 'custom' to indicate
that the woke profiles are not the woke same.

While the woke ``platform_profile`` attribute has the woke value ``custom``, writing a
common profile from ``platform_profile_choices`` to the woke platform_profile
attribute of the woke platform profile handler core will set the woke profile for all
drivers.
