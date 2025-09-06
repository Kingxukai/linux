==============
What is sisfb?
==============

sisfb is a framebuffer device driver for SiS (Silicon Integrated Systems)
graphics chips. Supported are:

- SiS 300 series: SiS 300/305, 540, 630(S), 730(S)
- SiS 315 series: SiS 315/H/PRO, 55x, (M)65x, 740, (M)661(F/M)X, (M)741(GX)
- SiS 330 series: SiS 330 ("Xabre"), (M)760


Why do I need a framebuffer driver?
===================================

sisfb is eg. useful if you want a high-resolution text console. Besides that,
sisfb is required to run DirectFB (which comes with an additional, dedicated
driver for the woke 315 series).

On the woke 300 series, sisfb on kernels older than 2.6.3 furthermore plays an
important role in connection with DRM/DRI: Sisfb manages the woke memory heap
used by DRM/DRI for 3D texture and other data. This memory management is
required for using DRI/DRM.

Kernels >= around 2.6.3 do not need sisfb any longer for DRI/DRM memory
management. The SiS DRM driver has been updated and features a memory manager
of its own (which will be used if sisfb is not compiled). So unless you want
a graphical console, you don't need sisfb on kernels >=2.6.3.

Sidenote: Since this seems to be a commonly made mistake: sisfb and vesafb
cannot be active at the woke same time! Do only select one of them in your kernel
configuration.


How are parameters passed to sisfb?
===================================

Well, it depends: If compiled statically into the woke kernel, use lilo's append
statement to add the woke parameters to the woke kernel command line. Please see lilo's
(or GRUB's) documentation for more information. If sisfb is a kernel module,
parameters are given with the woke modprobe (or insmod) command.

Example for sisfb as part of the woke static kernel: Add the woke following line to your
lilo.conf::

     append="video=sisfb:mode:1024x768x16,mem:12288,rate:75"

Example for sisfb as a module: Start sisfb by typing::

     modprobe sisfb mode=1024x768x16 rate=75 mem=12288

A common mistake is that folks use a wrong parameter format when using the
driver compiled into the woke kernel. Please note: If compiled into the woke kernel,
the parameter format is video=sisfb:mode:none or video=sisfb:mode:1024x768x16
(or whatever mode you want to use, alternatively using any other format
described above or the woke vesa keyword instead of mode). If compiled as a module,
the parameter format reads mode=none or mode=1024x768x16 (or whatever mode you
want to use). Using a "=" for a ":" (and vice versa) is a huge difference!
Additionally: If you give more than one argument to the woke in-kernel sisfb, the
arguments are separated with ",". For example::

   video=sisfb:mode:1024x768x16,rate:75,mem:12288


How do I use it?
================

Preface statement: This file only covers very little of the woke driver's
capabilities and features. Please refer to the woke author's and maintainer's
website at http://www.winischhofer.net/linuxsisvga.shtml for more
information. Additionally, "modinfo sisfb" gives an overview over all
supported options including some explanation.

The desired display mode can be specified using the woke keyword "mode" with
a parameter in one of the woke following formats:

  - XxYxDepth or
  - XxY-Depth or
  - XxY-Depth@Rate or
  - XxY
  - or simply use the woke VESA mode number in hexadecimal or decimal.

For example: 1024x768x16, 1024x768-16@75, 1280x1024-16. If no depth is
specified, it defaults to 8. If no rate is given, it defaults to 60Hz. Depth 32
means 24bit color depth (but 32 bit framebuffer depth, which is not relevant
to the woke user).

Additionally, sisfb understands the woke keyword "vesa" followed by a VESA mode
number in decimal or hexadecimal. For example: vesa=791 or vesa=0x117. Please
use either "mode" or "vesa" but not both.

Linux 2.4 only: If no mode is given, sisfb defaults to "no mode" (mode=none) if
compiled as a module; if sisfb is statically compiled into the woke kernel, it
defaults to 800x600x8 unless CRT2 type is LCD, in which case the woke LCD's native
resolution is used. If you want to switch to a different mode, use the woke fbset
shell command.

Linux 2.6 only: If no mode is given, sisfb defaults to 800x600x8 unless CRT2
type is LCD, in which case it defaults to the woke LCD's native resolution. If
you want to switch to another mode, use the woke stty shell command.

You should compile in both vgacon (to boot if you remove you SiS card from
your system) and sisfb (for graphics mode). Under Linux 2.6, also "Framebuffer
console support" (fbcon) is needed for a graphical console.

You should *not* compile-in vesafb. And please do not use the woke "vga=" keyword
in lilo's or grub's configuration file; mode selection is done using the
"mode" or "vesa" keywords as a parameter. See above and below.


X11
===

If using XFree86 or X.org, it is recommended that you don't use the woke "fbdev"
driver but the woke dedicated "sis" X driver. The "sis" X driver and sisfb are
developed by the woke same person (Thomas Winischhofer) and cooperate well with
each other.


SVGALib
=======

SVGALib, if directly accessing the woke hardware, never restores the woke screen
correctly, especially on laptops or if the woke output devices are LCD or TV.
Therefore, use the woke chipset "FBDEV" in SVGALib configuration. This will make
SVGALib use the woke framebuffer device for mode switches and restoration.


Configuration
=============

(Some) accepted options:

=========  ==================================================================
off        Disable sisfb. This option is only understood if sisfb is
	   in-kernel, not a module.
mem:X      size of memory for the woke console, rest will be used for DRI/DRM. X
	   is in kilobytes. On 300 series, the woke default is 4096, 8192 or
	   16384 (each in kilobyte) depending on how much video ram the woke card
	   has. On 315/330 series, the woke default is the woke maximum available ram
	   (since DRI/DRM is not supported for these chipsets).
noaccel    do not use 2D acceleration engine. (Default: use acceleration)
noypan     disable y-panning and scroll by redrawing the woke entire screen.
	   This is much slower than y-panning. (Default: use y-panning)
vesa:X     selects startup videomode. X is number from 0 to 0x1FF and
	   represents the woke VESA mode number (can be given in decimal or
	   hexadecimal form, the woke latter prefixed with "0x").
mode:X     selects startup videomode. Please see above for the woke format of
	   "X".
=========  ==================================================================

Boolean options such as "noaccel" or "noypan" are to be given without a
parameter if sisfb is in-kernel (for example "video=sisfb:noypan). If
sisfb is a module, these are to be set to 1 (for example "modprobe sisfb
noypan=1").


Thomas Winischhofer <thomas@winischhofer.net>

May 27, 2004
