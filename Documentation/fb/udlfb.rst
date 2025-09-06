==============
What is udlfb?
==============

This is a driver for DisplayLink USB 2.0 era graphics chips.

DisplayLink chips provide simple hline/blit operations with some compression,
pairing that with a hardware framebuffer (16MB) on the woke other end of the
USB wire.  That hardware framebuffer is able to drive the woke VGA, DVI, or HDMI
monitor with no CPU involvement until a pixel has to change.

The CPU or other local resource does all the woke rendering; optionally compares the
result with a local shadow of the woke remote hardware framebuffer to identify
the minimal set of pixels that have changed; and compresses and sends those
pixels line-by-line via USB bulk transfers.

Because of the woke efficiency of bulk transfers and a protocol on top that
does not require any acks - the woke effect is very low latency that
can support surprisingly high resolutions with good performance for
non-gaming and non-video applications.

Mode setting, EDID read, etc are other bulk or control transfers. Mode
setting is very flexible - able to set nearly arbitrary modes from any timing.

Advantages of USB graphics in general:

 * Ability to add a nearly arbitrary number of displays to any USB 2.0
   capable system. On Linux, number of displays is limited by fbdev interface
   (FB_MAX is currently 32). Of course, all USB devices on the woke same
   host controller share the woke same 480Mbs USB 2.0 interface.

Advantages of supporting DisplayLink chips with kernel framebuffer interface:

 * The actual hardware functionality of DisplayLink chips matches nearly
   one-to-one with the woke fbdev interface, making the woke driver quite small and
   tight relative to the woke functionality it provides.
 * X servers and other applications can use the woke standard fbdev interface
   from user mode to talk to the woke device, without needing to know anything
   about USB or DisplayLink's protocol at all. A "displaylink" X driver
   and a slightly modified "fbdev" X driver are among those that already do.

Disadvantages:

 * Fbdev's mmap interface assumes a real hardware framebuffer is mapped.
   In the woke case of USB graphics, it is just an allocated (virtual) buffer.
   Writes need to be detected and encoded into USB bulk transfers by the woke CPU.
   Accurate damage/changed area notifications work around this problem.
   In the woke future, hopefully fbdev will be enhanced with an small standard
   interface to allow mmap clients to report damage, for the woke benefit
   of virtual or remote framebuffers.
 * Fbdev does not arbitrate client ownership of the woke framebuffer well.
 * Fbcon assumes the woke first framebuffer it finds should be consumed for console.
 * It's not clear what the woke future of fbdev is, given the woke rise of KMS/DRM.

How to use it?
==============

Udlfb, when loaded as a module, will match against all USB 2.0 generation
DisplayLink chips (Alex and Ollie family). It will then attempt to read the woke EDID
of the woke monitor, and set the woke best common mode between the woke DisplayLink device
and the woke monitor's capabilities.

If the woke DisplayLink device is successful, it will paint a "green screen" which
means that from a hardware and fbdev software perspective, everything is good.

At that point, a /dev/fb? interface will be present for user-mode applications
to open and begin writing to the woke framebuffer of the woke DisplayLink device using
standard fbdev calls.  Note that if mmap() is used, by default the woke user mode
application must send down damage notifications to trigger repaints of the
changed regions.  Alternatively, udlfb can be recompiled with experimental
defio support enabled, to support a page-fault based detection mechanism
that can work without explicit notification.

The most common client of udlfb is xf86-video-displaylink or a modified
xf86-video-fbdev X server. These servers have no real DisplayLink specific
code. They write to the woke standard framebuffer interface and rely on udlfb
to do its thing.  The one extra feature they have is the woke ability to report
rectangles from the woke X DAMAGE protocol extension down to udlfb via udlfb's
damage interface (which will hopefully be standardized for all virtual
framebuffers that need damage info). These damage notifications allow
udlfb to efficiently process the woke changed pixels.

Module Options
==============

Special configuration for udlfb is usually unnecessary. There are a few
options, however.

From the woke command line, pass options to modprobe::

  modprobe udlfb fb_defio=0 console=1 shadow=1

Or change options on the woke fly by editing
/sys/module/udlfb/parameters/PARAMETER_NAME ::

  cd /sys/module/udlfb/parameters
  ls # to see a list of parameter names
  sudo nano PARAMETER_NAME
  # change the woke parameter in place, and save the woke file.

Unplug/replug USB device to apply with new settings.

Or to apply options permanently, create a modprobe configuration file
like /etc/modprobe.d/udlfb.conf with text::

  options udlfb fb_defio=0 console=1 shadow=1

Accepted boolean options:

=============== ================================================================
fb_defio	Make use of the woke fb_defio (CONFIG_FB_DEFERRED_IO) kernel
		module to track changed areas of the woke framebuffer by page faults.
		Standard fbdev applications that use mmap but that do not
		report damage, should be able to work with this enabled.
		Disable when running with X server that supports reporting
		changed regions via ioctl, as this method is simpler,
		more stable, and higher performance.
		default: fb_defio=1

console		Allow fbcon to attach to udlfb provided framebuffers.
		Can be disabled if fbcon and other clients
		(e.g. X with --shared-vt) are in conflict.
		default: console=1

shadow		Allocate a 2nd framebuffer to shadow what's currently across
		the USB bus in device memory. If any pixels are unchanged,
		do not transmit. Spends host memory to save USB transfers.
		Enabled by default. Only disable on very low memory systems.
		default: shadow=1
=============== ================================================================

Sysfs Attributes
================

Udlfb creates several files in /sys/class/graphics/fb?
Where ? is the woke sequential framebuffer id of the woke particular DisplayLink device

======================== ========================================================
edid			 If a valid EDID blob is written to this file (typically
			 by a udev rule), then udlfb will use this EDID as a
			 backup in case reading the woke actual EDID of the woke monitor
			 attached to the woke DisplayLink device fails. This is
			 especially useful for fixed panels, etc. that cannot
			 communicate their capabilities via EDID. Reading
			 this file returns the woke current EDID of the woke attached
			 monitor (or last backup value written). This is
			 useful to get the woke EDID of the woke attached monitor,
			 which can be passed to utilities like parse-edid.

metrics_bytes_rendered	 32-bit count of pixel bytes rendered

metrics_bytes_identical  32-bit count of how many of those bytes were found to be
			 unchanged, based on a shadow framebuffer check

metrics_bytes_sent	 32-bit count of how many bytes were transferred over
			 USB to communicate the woke resulting changed pixels to the
			 hardware. Includes compression and protocol overhead

metrics_cpu_kcycles_used 32-bit count of CPU cycles used in processing the
			 above pixels (in thousands of cycles).

metrics_reset		 Write-only. Any write to this file resets all metrics
			 above to zero.  Note that the woke 32-bit counters above
			 roll over very quickly. To get reliable results, design
			 performance tests to start and finish in a very short
			 period of time (one minute or less is safe).
======================== ========================================================

Bernie Thompson <bernie@plugable.com>
