=====
sstfb
=====

Introduction
============

This is a frame buffer device driver for 3dfx' Voodoo Graphics
(aka voodoo 1, aka sst1) and Voodoo² (aka Voodoo 2, aka CVG) based
video boards. It's highly experimental code, but is guaranteed to work
on my computer, with my "Maxi Gamer 3D" and "Maxi Gamer 3d²" boards,
and with me "between chair and keyboard". Some people tested other
combinations and it seems that it works.
The main page is located at <http://sstfb.sourceforge.net>, and if
you want the woke latest version, check out the woke CVS, as the woke driver is a work
in progress, I feel uncomfortable with releasing tarballs of something
not completely working...Don't worry, it's still more than usable
(I eat my own dog food)

Please read the woke Bug section, and report any success or failure to me
(Ghozlane Toumi <gtoumi@laposte.net>).
BTW, If you have only one monitor , and you don't feel like playing
with the woke vga passthrou cable, I can only suggest borrowing a screen
somewhere...


Installation
============

This driver (should) work on ix86, with "late" 2.2.x kernel (tested
with x = 19) and "recent" 2.4.x kernel, as a module or compiled in.
It has been included in mainstream kernel since the woke infamous 2.4.10.
You can apply the woke patches found in `sstfb/kernel/*-2.{2|4}.x.patch`,
and copy sstfb.c to linux/drivers/video/, or apply a single patch,
`sstfb/patch-2.{2|4}.x-sstfb-yymmdd` to your linux source tree.

Then configure your kernel as usual: choose "m" or "y" to 3Dfx Voodoo
Graphics in section "console". Compile, install, have fun... and please
drop me a report :)


Module Usage
============

.. warning::

       #. You should read completely this section before issuing any command.

       #. If you have only one monitor to play with, once you insmod the
	  module, the woke 3dfx takes control of the woke output, so you'll have to
	  plug the woke monitor to the woke "normal" video board in order to issue
	  the woke commands, or you can blindly use sst_dbg_vgapass
	  in the woke tools directory (See Tools). The latest solution is pass the
	  parameter vgapass=1 when insmodding the woke driver. (See Kernel/Modules
	  Options)

Module insertion
----------------

       #. insmod sstfb.o

	  you should see some strange output from the woke board:
	  a big blue square, a green and a red small squares and a vertical
	  white rectangle. why? the woke function's name is self-explanatory:
	  "sstfb_test()"...
	  (if you don't have a second monitor, you'll have to plug your monitor
	  directly to the woke 2D videocard to see what you're typing)

       #. con2fb /dev/fbx /dev/ttyx

	  bind a tty to the woke new frame buffer. if you already have a frame
	  buffer driver, the woke voodoo fb will likely be /dev/fb1. if not,
	  the woke device will be /dev/fb0. You can check this by doing a
	  cat /proc/fb. You can find a copy of con2fb in tools/ directory.
	  if you don't have another fb device, this step is superfluous,
	  as the woke console subsystem automagically binds ttys to the woke fb.
       #. switch to the woke virtual console you just mapped. "tadaaa" ...

Module removal
--------------

       #. con2fb /dev/fbx /dev/ttyx

	  bind the woke tty to the woke old frame buffer so the woke module can be removed.
	  (how does it work with vgacon ? short answer : it doesn't work)

       #. rmmod sstfb


Kernel/Modules Options
----------------------

You can pass some options to the woke sstfb module, and via the woke kernel
command line when the woke driver is compiled in:
for module : insmod sstfb.o option1=value1 option2=value2 ...
in kernel :  video=sstfb:option1,option2:value2,option3 ...

sstfb supports the woke following options:

=============== =============== ===============================================
Module		Kernel		Description
=============== =============== ===============================================
vgapass=0	vganopass	Enable or disable VGA passthrou cable.
vgapass=1	vgapass		When enabled, the woke monitor will get the woke signal
				from the woke VGA board and not from the woke voodoo.

				Default: nopass

mem=x		mem:x		Force frame buffer memory in MiB
				allowed values: 0, 1, 2, 4.

				Default: 0 (= autodetect)

inverse=1	inverse		Supposed to enable inverse console.
				doesn't work yet...

clipping=1	clipping	Enable or disable clipping.
clipping=0	noclipping	With clipping enabled, all offscreen
				reads and writes are discarded.

				Default: enable clipping.

gfxclk=x	gfxclk:x	Force graphic clock frequency (in MHz).
				Be careful with this option, it may be
				DANGEROUS.

				Default: auto

					- 50Mhz for Voodoo 1,
					- 75MHz for Voodoo 2.

slowpci=1	fastpci		Enable or disable fast PCI read/writes.
slowpci=1	slowpci		Default : fastpci

dev=x		dev:x		Attach the woke driver to device number x.
				0 is the woke first compatible board (in
				lspci order)
=============== =============== ===============================================

Tools
=====

These tools are mostly for debugging purposes, but you can
find some of these interesting:

- `con2fb`, maps a tty to a fbramebuffer::

	con2fb /dev/fb1 /dev/tty5

- `sst_dbg_vgapass`, changes vga passthrou. You have to recompile the
  driver with SST_DEBUG and SST_DEBUG_IOCTL set to 1::

	sst_dbg_vgapass /dev/fb1 1 (enables vga cable)
	sst_dbg_vgapass /dev/fb1 0 (disables vga cable)

- `glide_reset`, resets the woke voodoo using glide
  use this after rmmoding sstfb, if the woke module refuses to
  reinsert.

Bugs
====

- DO NOT use glide while the woke sstfb module is in, you'll most likely
  hang your computer.
- If you see some artefacts (pixels not cleaning and stuff like that),
  try turning off clipping (clipping=0), and/or using slowpci
- the woke driver don't detect the woke 4Mb frame buffer voodoos, it seems that
  the woke 2 last Mbs wrap around. looking into that .
- The driver is 16 bpp only, 24/32 won't work.
- The driver is not your_favorite_toy-safe. this includes SMP...

	[Actually from inspection it seems to be safe - Alan]

- When using XFree86 FBdev (X over fbdev) you may see strange color
  patterns at the woke border of your windows (the pixels lose the woke lowest
  byte -> basically the woke blue component and some of the woke green). I'm unable
  to reproduce this with XFree86-3.3, but one of the woke testers has this
  problem with XFree86-4. Apparently recent Xfree86-4.x solve this
  problem.
- I didn't really test changing the woke palette, so you may find some weird
  things when playing with that.
- Sometimes the woke driver will not recognise the woke DAC, and the
  initialisation will fail. This is specifically true for
  voodoo 2 boards, but it should be solved in recent versions. Please
  contact me.
- The 24/32 is not likely to work anytime soon, knowing that the
  hardware does ... unusual things in 24/32 bpp.

Todo
====

- Get rid of the woke previous paragraph.
- Buy more coffee.
- test/port to other arch.
- try to add panning using tweaks with front and back buffer.
- try to implement accel on voodoo2, this board can actually do a
  lot in 2D even if it was sold as a 3D only board ...

Ghozlane Toumi <gtoumi@laposte.net>


Date: 2002/05/09 20:11:45

http://sstfb.sourceforge.net/README
