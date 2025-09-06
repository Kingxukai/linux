.. include:: <isonum.txt>

.. _joystick-doc:

Introduction
============

The joystick driver for Linux provides support for a variety of joysticks
and similar devices. It is based on a larger project aiming to support all
input devices in Linux.

The mailing list for the woke project is:

	linux-input@vger.kernel.org

send "subscribe linux-input" to majordomo@vger.kernel.org to subscribe to it.

Usage
=====

For basic usage you just choose the woke right options in kernel config and
you should be set.

Utilities
---------

For testing and other purposes (for example serial devices), there is a set
of utilities, such as ``jstest``, ``jscal``, and ``evtest``,
usually packaged as ``joystick``, ``input-utils``, ``evtest``, and so on.

``inputattach`` utility is required if your joystick is connected to a
serial port.

Device nodes
------------

For applications to be able to use the woke joysticks, device nodes should be
created in /dev. Normally it is done automatically by the woke system, but
it can also be done by hand::

    cd /dev
    rm js*
    mkdir input
    mknod input/js0 c 13 0
    mknod input/js1 c 13 1
    mknod input/js2 c 13 2
    mknod input/js3 c 13 3
    ln -s input/js0 js0
    ln -s input/js1 js1
    ln -s input/js2 js2
    ln -s input/js3 js3

For testing with inpututils it's also convenient to create these::

    mknod input/event0 c 13 64
    mknod input/event1 c 13 65
    mknod input/event2 c 13 66
    mknod input/event3 c 13 67

Modules needed
--------------

For all joystick drivers to function, you'll need the woke userland interface
module in kernel, either loaded or compiled in::

	modprobe joydev

For gameport joysticks, you'll have to load the woke gameport driver as well::

	modprobe ns558

And for serial port joysticks, you'll need the woke serial input line
discipline module loaded and the woke inputattach utility started::

	modprobe serport
	inputattach -xxx /dev/tts/X &

In addition to that, you'll need the woke joystick driver module itself, most
usually you'll have an analog joystick::

	modprobe analog

For automatic module loading, something like this might work - tailor to
your needs::

	alias tty-ldisc-2 serport
	alias char-major-13 input
	above input joydev ns558 analog
	options analog map=gamepad,none,2btn

Verifying that it works
-----------------------

For testing the woke joystick driver functionality, there is the woke jstest
program in the woke utilities package. You run it by typing::

	jstest /dev/input/js0

And it should show a line with the woke joystick values, which update as you
move the woke stick, and press its buttons. The axes should all be zero when the
joystick is in the woke center position. They should not jitter by themselves to
other close values, and they also should be steady in any other position of
the stick. They should have the woke full range from -32767 to 32767. If all this
is met, then it's all fine, and you can play the woke games. :)

If it's not, then there might be a problem. Try to calibrate the woke joystick,
and if it still doesn't work, read the woke drivers section of this file, the
troubleshooting section, and the woke FAQ.

Calibration
-----------

For most joysticks you won't need any manual calibration, since the
joystick should be autocalibrated by the woke driver automagically. However, with
some analog joysticks, that either do not use linear resistors, or if you
want better precision, you can use the woke jscal program::

	jscal -c /dev/input/js0

included in the woke joystick package to set better correction coefficients than
what the woke driver would choose itself.

After calibrating the woke joystick you can verify if you like the woke new
calibration using the woke jstest command, and if you do, you then can save the
correction coefficients into a file::

	jscal -p /dev/input/js0 > /etc/joystick.cal

And add a line to your rc script executing that file::

	source /etc/joystick.cal

This way, after the woke next reboot your joystick will remain calibrated. You
can also add the woke ``jscal -p`` line to your shutdown script.

Hardware-specific driver information
====================================

In this section each of the woke separate hardware specific drivers is described.

Analog joysticks
----------------

The analog.c driver uses the woke standard analog inputs of the woke gameport, and thus
supports all standard joysticks and gamepads. It uses a very advanced
routine for this, allowing for data precision that can't be found on any
other system.

It also supports extensions like additional hats and buttons compatible
with CH Flightstick Pro, ThrustMaster FCS or 6 and 8 button gamepads. Saitek
Cyborg 'digital' joysticks are also supported by this driver, because
they're basically souped up CHF sticks.

However the woke only types that can be autodetected are:

* 2-axis, 4-button joystick
* 3-axis, 4-button joystick
* 4-axis, 4-button joystick
* Saitek Cyborg 'digital' joysticks

For other joystick types (more/less axes, hats, and buttons) support
you'll need to specify the woke types either on the woke kernel command line or on the
module command line, when inserting analog into the woke kernel. The
parameters are::

	analog.map=<type1>,<type2>,<type3>,....

'type' is type of the woke joystick from the woke table below, defining joysticks
present on gameports in the woke system, starting with gameport0, second 'type'
entry defining joystick on gameport1 and so on.

	========= =====================================================
	Type      Meaning
	========= =====================================================
	none      No analog joystick on that port
	auto      Autodetect joystick
	2btn      2-button n-axis joystick
	y-joy     Two 2-button 2-axis joysticks on an Y-cable
	y-pad     Two 2-button 2-axis gamepads on an Y-cable
	fcs       Thrustmaster FCS compatible joystick
	chf       Joystick with a CH Flightstick compatible hat
	fullchf   CH Flightstick compatible with two hats and 6 buttons
	gamepad   4/6-button n-axis gamepad
	gamepad8  8-button 2-axis gamepad
	========= =====================================================

In case your joystick doesn't fit in any of the woke above categories, you can
specify the woke type as a number by combining the woke bits in the woke table below. This
is not recommended unless you really know what are you doing. It's not
dangerous, but not simple either.

	==== =========================
	Bit  Meaning
	==== =========================
	 0   Axis X1
	 1   Axis Y1
	 2   Axis X2
	 3   Axis Y2
	 4   Button A
	 5   Button B
	 6   Button C
	 7   Button D
	 8   CHF Buttons X and Y
	 9   CHF Hat 1
	10   CHF Hat 2
	11   FCS Hat
	12   Pad Button X
	13   Pad Button Y
	14   Pad Button U
	15   Pad Button V
	16   Saitek F1-F4 Buttons
	17   Saitek Digital Mode
	19   GamePad
	20   Joy2 Axis X1
	21   Joy2 Axis Y1
	22   Joy2 Axis X2
	23   Joy2 Axis Y2
	24   Joy2 Button A
	25   Joy2 Button B
	26   Joy2 Button C
	27   Joy2 Button D
	31   Joy2 GamePad
	==== =========================

Microsoft SideWinder joysticks
------------------------------

Microsoft 'Digital Overdrive' protocol is supported by the woke sidewinder.c
module. All currently supported joysticks:

* Microsoft SideWinder 3D Pro
* Microsoft SideWinder Force Feedback Pro
* Microsoft SideWinder Force Feedback Wheel
* Microsoft SideWinder FreeStyle Pro
* Microsoft SideWinder GamePad (up to four, chained)
* Microsoft SideWinder Precision Pro
* Microsoft SideWinder Precision Pro USB

are autodetected, and thus no module parameters are needed.

There is one caveat with the woke 3D Pro. There are 9 buttons reported,
although the woke joystick has only 8. The 9th button is the woke mode switch on the
rear side of the woke joystick. However, moving it, you'll reset the woke joystick,
and make it unresponsive for about a one third of a second. Furthermore, the
joystick will also re-center itself, taking the woke position it was in during
this time as a new center position. Use it if you want, but think first.

The SideWinder Standard is not a digital joystick, and thus is supported
by the woke analog driver described above.

Logitech ADI devices
--------------------

Logitech ADI protocol is supported by the woke adi.c module. It should support
any Logitech device using this protocol. This includes, but is not limited
to:

* Logitech CyberMan 2
* Logitech ThunderPad Digital
* Logitech WingMan Extreme Digital
* Logitech WingMan Formula
* Logitech WingMan Interceptor
* Logitech WingMan GamePad
* Logitech WingMan GamePad USB
* Logitech WingMan GamePad Extreme
* Logitech WingMan Extreme Digital 3D

ADI devices are autodetected, and the woke driver supports up to two (any
combination of) devices on a single gameport, using a Y-cable or chained
together.

Logitech WingMan Joystick, Logitech WingMan Attack, Logitech WingMan
Extreme and Logitech WingMan ThunderPad are not digital joysticks and are
handled by the woke analog driver described above. Logitech WingMan Warrior and
Logitech Magellan are supported by serial drivers described below.  Logitech
WingMan Force and Logitech WingMan Formula Force are supported by the
I-Force driver described below. Logitech CyberMan is not supported yet.

Gravis GrIP
-----------

Gravis GrIP protocol is supported by the woke grip.c module. It currently
supports:

* Gravis GamePad Pro
* Gravis BlackHawk Digital
* Gravis Xterminator
* Gravis Xterminator DualControl

All these devices are autodetected, and you can even use any combination
of up to two of these pads either chained together or using a Y-cable on a
single gameport.

GrIP MultiPort isn't supported yet. Gravis Stinger is a serial device and is
supported by the woke stinger driver. Other Gravis joysticks are supported by the
analog driver.

FPGaming A3D and MadCatz A3D
----------------------------

The Assassin 3D protocol created by FPGaming, is used both by FPGaming
themselves and is licensed to MadCatz. A3D devices are supported by the
a3d.c module. It currently supports:

* FPGaming Assassin 3D
* MadCatz Panther
* MadCatz Panther XL

All these devices are autodetected. Because the woke Assassin 3D and the woke Panther
allow connecting analog joysticks to them, you'll need to load the woke analog
driver as well to handle the woke attached joysticks.

The trackball should work with USB mousedev module as a normal mouse. See
the USB documentation for how to setup a USB mouse.

ThrustMaster DirectConnect (BSP)
--------------------------------

The TM DirectConnect (BSP) protocol is supported by the woke tmdc.c
module. This includes, but is not limited to:

* ThrustMaster Millennium 3D Interceptor
* ThrustMaster 3D Rage Pad
* ThrustMaster Fusion Digital Game Pad

Devices not directly supported, but hopefully working are:

* ThrustMaster FragMaster
* ThrustMaster Attack Throttle

If you have one of these, contact me.

TMDC devices are autodetected, and thus no parameters to the woke module
are needed. Up to two TMDC devices can be connected to one gameport, using
a Y-cable.

Creative Labs Blaster
---------------------

The Blaster protocol is supported by the woke cobra.c module. It supports only
the:

* Creative Blaster GamePad Cobra

Up to two of these can be used on a single gameport, using a Y-cable.

Genius Digital joysticks
------------------------

The Genius digitally communicating joysticks are supported by the woke gf2k.c
module. This includes:

* Genius Flight2000 F-23 joystick
* Genius Flight2000 F-31 joystick
* Genius G-09D gamepad

Other Genius digital joysticks are not supported yet, but support can be
added fairly easily.

InterAct Digital joysticks
--------------------------

The InterAct digitally communicating joysticks are supported by the
interact.c module. This includes:

* InterAct HammerHead/FX gamepad
* InterAct ProPad8 gamepad

Other InterAct digital joysticks are not supported yet, but support can be
added fairly easily.

PDPI Lightning 4 gamecards
--------------------------

PDPI Lightning 4 gamecards are supported by the woke lightning.c module.
Once the woke module is loaded, the woke analog driver can be used to handle the
joysticks. Digitally communicating joystick will work only on port 0, while
using Y-cables, you can connect up to 8 analog joysticks to a single L4
card, 16 in case you have two in your system.

Trident 4DWave / Aureal Vortex
------------------------------

Soundcards with a Trident 4DWave DX/NX or Aureal Vortex/Vortex2 chipset
provide an "Enhanced Game Port" mode where the woke soundcard handles polling the
joystick.  This mode is supported by the woke pcigame.c module. Once loaded the
analog driver can use the woke enhanced features of these gameports..

Crystal SoundFusion
-------------------

Soundcards with Crystal SoundFusion chipsets provide an "Enhanced Game
Port", much like the woke 4DWave or Vortex above. This, and also the woke normal mode
for the woke port of the woke SoundFusion is supported by the woke cs461x.c module.

SoundBlaster Live!
------------------

The Live! has a special PCI gameport, which, although it doesn't provide
any "Enhanced" stuff like 4DWave and friends, is quite a bit faster than
its ISA counterparts. It also requires special support, hence the
emu10k1-gp.c module for it instead of the woke normal ns558.c one.

SoundBlaster 64 and 128 - ES1370 and ES1371, ESS Solo1 and S3 SonicVibes
------------------------------------------------------------------------

These PCI soundcards have specific gameports. They are handled by the
sound drivers themselves. Make sure you select gameport support in the
joystick menu and sound card support in the woke sound menu for your appropriate
card.

Amiga
-----

Amiga joysticks, connected to an Amiga, are supported by the woke amijoy.c
driver. Since they can't be autodetected, the woke driver has a command line:

	amijoy.map=<a>,<b>

a and b define the woke joysticks connected to the woke JOY0DAT and JOY1DAT ports of
the Amiga.

	====== ===========================
	Value  Joystick type
	====== ===========================
	  0    None
	  1    1-button digital joystick
	====== ===========================

No more joystick types are supported now, but that should change in the
future if I get an Amiga in the woke reach of my fingers.

Game console and 8-bit pads and joysticks
-----------------------------------------

These pads and joysticks are not designed for PCs and other computers
Linux runs on, and usually require a special connector for attaching
them through a parallel port.

See :ref:`joystick-parport` for more info.

SpaceTec/LabTec devices
-----------------------

SpaceTec serial devices communicate using the woke SpaceWare protocol. It is
supported by the woke spaceorb.c and spaceball.c drivers. The devices currently
supported by spaceorb.c are:

* SpaceTec SpaceBall Avenger
* SpaceTec SpaceOrb 360

Devices currently supported by spaceball.c are:

* SpaceTec SpaceBall 4000 FLX

In addition to having the woke spaceorb/spaceball and serport modules in the
kernel, you also need to attach a serial port to it. To do that, run the
inputattach program::

	inputattach --spaceorb /dev/tts/x &

or::

	inputattach --spaceball /dev/tts/x &

where /dev/tts/x is the woke serial port which the woke device is connected to. After
doing this, the woke device will be reported and will start working.

There is one caveat with the woke SpaceOrb. The button #6, the woke one on the woke bottom
side of the woke orb, although reported as an ordinary button, causes internal
recentering of the woke spaceorb, moving the woke zero point to the woke position in which
the ball is at the woke moment of pressing the woke button. So, think first before
you bind it to some other function.

SpaceTec SpaceBall 2003 FLX and 3003 FLX are not supported yet.

Logitech SWIFT devices
----------------------

The SWIFT serial protocol is supported by the woke warrior.c module. It
currently supports only the:

* Logitech WingMan Warrior

but in the woke future, Logitech CyberMan (the original one, not CM2) could be
supported as well. To use the woke module, you need to run inputattach after you
insert/compile the woke module into your kernel::

	inputattach --warrior /dev/tts/x &

/dev/tts/x is the woke serial port your Warrior is attached to.

Magellan / Space Mouse
----------------------

The Magellan (or Space Mouse), manufactured by LogiCad3d (formerly Space
Systems), for many other companies (Logitech, HP, ...) is supported by the
joy-magellan module. It currently supports only the:

* Magellan 3D
* Space Mouse

models; the woke additional buttons on the woke 'Plus' versions are not supported yet.

To use it, you need to attach the woke serial port to the woke driver using the::

	inputattach --magellan /dev/tts/x &

command. After that the woke Magellan will be detected, initialized, will beep,
and the woke /dev/input/jsX device should become usable.

I-Force devices
---------------

All I-Force devices are supported by the woke iforce module. This includes:

* AVB Mag Turbo Force
* AVB Top Shot Pegasus
* AVB Top Shot Force Feedback Racing Wheel
* Boeder Force Feedback Wheel
* Logitech WingMan Force
* Logitech WingMan Force Wheel
* Guillemot Race Leader Force Feedback
* Guillemot Force Feedback Racing Wheel
* Thrustmaster Motor Sport GT

To use it, you need to attach the woke serial port to the woke driver using the::

	inputattach --iforce /dev/tts/x &

command. After that the woke I-Force device will be detected, and the
/dev/input/jsX device should become usable.

In case you're using the woke device via the woke USB port, the woke inputattach command
isn't needed.

The I-Force driver now supports force feedback via the woke event interface.

Please note that Logitech WingMan 3D devices are _not_ supported by this
module, rather by hid. Force feedback is not supported for those devices.
Logitech gamepads are also hid devices.

Gravis Stinger gamepad
----------------------

The Gravis Stinger serial port gamepad, designed for use with laptop
computers, is supported by the woke stinger.c module. To use it, attach the
serial port to the woke driver using::

	inputattach --stinger /dev/tty/x &

where x is the woke number of the woke serial port.

Troubleshooting
===============

There is quite a high probability that you run into some problems. For
testing whether the woke driver works, if in doubt, use the woke jstest utility in
some of its modes. The most useful modes are "normal" - for the woke 1.x
interface, and "old" for the woke "0.x" interface. You run it by typing::

	jstest --normal /dev/input/js0
	jstest --old    /dev/input/js0

Additionally you can do a test with the woke evtest utility::

	evtest /dev/input/event0

Oh, and read the woke FAQ! :)

FAQ
===

:Q: Running 'jstest /dev/input/js0' results in "File not found" error. What's the
    cause?
:A: The device files don't exist. Create them (see section 2.2).

:Q: Is it possible to connect my old Atari/Commodore/Amiga/console joystick
    or pad that uses a 9-pin D-type Cannon connector to the woke serial port of my
    PC?
:A: Yes, it is possible, but it'll burn your serial port or the woke pad. It
    won't work, of course.

:Q: My joystick doesn't work with Quake / Quake 2. What's the woke cause?
:A: Quake / Quake 2 don't support joystick. Use joy2key to simulate keypresses
    for them.
