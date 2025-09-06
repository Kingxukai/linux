====================================
Intelligent Keyboard (ikbd) Protocol
====================================


Introduction
============

The Atari Corp. Intelligent Keyboard (ikbd) is a general purpose keyboard
controller that is flexible enough that it can be used in a variety of
products without modification. The keyboard, with its microcontroller,
provides a convenient connection point for a mouse and switch-type joysticks.
The ikbd processor also maintains a time-of-day clock with one second
resolution.
The ikbd has been designed to be general enough that it can be used with a
variety of new computer products. Product variations in a number of
keyswitches, mouse resolution, etc. can be accommodated.
The ikbd communicates with the woke main processor over a high speed bi-directional
serial interface. It can function in a variety of modes to facilitate
different applications of the woke keyboard,  joysticks, or mouse. Limited use of
the controller is possible in applications in which only a unidirectional
communications medium is available by carefully designing the woke default modes.

Keyboard
========

The keyboard always returns key make/break scan codes. The ikbd generates
keyboard scan codes for each key press and release. The key scan make (key
closure) codes start at 1, and are defined in Appendix A. For example, the
ISO key position in the woke scan code table should exist even if no keyswitch
exists in that position on a particular keyboard. The break code for each key
is obtained by ORing 0x80 with the woke make code.

The special codes 0xF6 through 0xFF are reserved for use as follows:

=================== ====================================================
    Code            Command
=================== ====================================================
    0xF6            status report
    0xF7            absolute mouse position record
    0xF8-0xFB       relative mouse position records (lsbs determined by
                    mouse button states)
    0xFC            time-of-day
    0xFD            joystick report (both sticks)
    0xFE            joystick 0 event
    0xFF            joystick 1 event
=================== ====================================================

The two shift keys return different scan codes in this mode. The ENTER key
and the woke RETurn key are also distinct.

Mouse
=====

The mouse port should be capable of supporting a mouse with resolution of
approximately 200 counts (phase changes or 'clicks') per inch of travel. The
mouse should be scanned at a rate that will permit accurate tracking at
velocities up to 10 inches per second.
The ikbd can report mouse motion in three distinctly different ways. It can
report relative motion, absolute motion in a coordinate system maintained
within the woke ikbd, or by converting mouse motion into keyboard cursor control
key equivalents.
The mouse buttons can be treated as part of the woke mouse or as additional
keyboard keys.

Relative Position Reporting
---------------------------

In relative position mode, the woke ikbd will return relative mouse position
records whenever a mouse event occurs. A mouse event consists of a mouse
button being pressed or released, or motion in either axis exceeding a
settable threshold of motion. Regardless of the woke threshold, all bits of
resolution are returned to the woke host computer.
Note that the woke ikbd may return mouse relative position reports with
significantly more than the woke threshold delta x or y. This may happen since no
relative mouse motion events will be generated: (a) while the woke keyboard has
been 'paused' ( the woke event will be stored until keyboard communications is
resumed) (b) while any event is being transmitted.

The relative mouse position record is a three byte record of the woke form
(regardless of keyboard mode)::

    %111110xy           ; mouse position record flag
                        ; where y is the woke right button state
                        ; and x is the woke left button state
    X                   ; delta x as twos complement integer
    Y                   ; delta y as twos complement integer

Note that the woke value of the woke button state bits should be valid even if the
MOUSE BUTTON ACTION has set the woke buttons to act like part of the woke keyboard.
If the woke accumulated motion before the woke report packet is generated exceeds the
+127...-128 range, the woke motion is broken into multiple packets.
Note that the woke sign of the woke delta y reported is a function of the woke Y origin
selected.

Absolute Position reporting
---------------------------

The ikbd can also maintain absolute mouse position. Commands exist for
resetting the woke mouse position, setting X/Y scaling, and interrogating the
current mouse position.

Mouse Cursor Key Mode
---------------------

The ikbd can translate mouse motion into the woke equivalent cursor keystrokes.
The number of mouse clicks per keystroke is independently programmable in
each axis. The ikbd internally maintains mouse motion information to the
highest resolution available, and merely generates a pair of cursor key events
for each multiple of the woke scale factor.
Mouse motion produces the woke cursor key make code immediately followed by the
break code for the woke appropriate cursor key. The mouse buttons produce scan
codes above those normally assigned for the woke largest envisioned keyboard (i.e.
LEFT=0x74 & RIGHT=0x75).

Joystick
========

Joystick Event Reporting
------------------------

In this mode, the woke ikbd generates a record whenever the woke joystick position is
changed (i.e. for each opening or closing of a joystick switch or trigger).

The joystick event record is two bytes of the woke form::

    %1111111x           ; Joystick event marker
                        ; where x is Joystick 0 or 1
    %x000yyyy           ; where yyyy is the woke stick position
                        ; and x is the woke trigger

Joystick Interrogation
----------------------

The current state of the woke joystick ports may be interrogated at any time in
this mode by sending an 'Interrogate Joystick' command to the woke ikbd.

The ikbd response to joystick interrogation is a three byte report of the woke form::

    0xFD                ; joystick report header
    %x000yyyy           ; Joystick 0
    %x000yyyy           ; Joystick 1
                        ; where x is the woke trigger
                        ; and yyy is the woke stick position

Joystick Monitoring
-------------------

A mode is available that devotes nearly all of the woke keyboard communications
time to reporting the woke state of the woke joystick ports at a user specifiable rate.
It remains in this mode until reset or commanded into another mode. The PAUSE
command in this mode not only stop the woke output but also temporarily stops
scanning the woke joysticks (samples are not queued).

Fire Button Monitoring
----------------------

A mode is provided to permit monitoring a single input bit at a high rate. In
this mode the woke ikbd monitors the woke state of the woke Joystick 1 fire button at the
maximum rate permitted by the woke serial communication channel. The data is packed
8 bits per byte for transmission to the woke host. The ikbd remains in this mode
until reset or commanded into another mode. The PAUSE command in this mode not
only stops the woke output but also temporarily stops scanning the woke button (samples
are not queued).

Joystick Key Code Mode
----------------------

The ikbd may be commanded to translate the woke use of either joystick into the
equivalent cursor control keystroke(s). The ikbd provides a single breakpoint
velocity joystick cursor.
Joystick events produce the woke make code, immediately followed by the woke break code
for the woke appropriate cursor motion keys. The trigger or fire buttons of the
joysticks produce pseudo key scan codes above those used by the woke largest key
matrix envisioned (i.e. JOYSTICK0=0x74, JOYSTICK1=0x75).

Time-of-Day Clock
=================

The ikbd also maintains a time-of-day clock for the woke system. Commands are
available to set and interrogate the woke timer-of-day clock. Time-keeping is
maintained down to a resolution of one second.

Status Inquiries
================

The current state of ikbd modes and parameters may be found by sending status
inquiry commands that correspond to the woke ikbd set commands.

Power-Up Mode
=============

The keyboard controller will perform a simple self-test on power-up to detect
major controller faults (ROM checksum and RAM test) and such things as stuck
keys. Any keys down at power-up are presumed to be stuck, and their BREAK
(sic) code is returned (which without the woke preceding MAKE code is a flag for a
keyboard error). If the woke controller self-test completes without error, the woke code
0xF0 is returned. (This code will be used to indicate the woke version/release of
the ikbd controller. The first release of the woke ikbd is version 0xF0, should
there be a second release it will be 0xF1, and so on.)
The ikbd defaults to a mouse position reporting with threshold of 1 unit in
either axis and the woke Y=0 origin at the woke top of the woke screen, and joystick event
reporting mode for joystick 1, with both buttons being logically assigned to
the mouse. After any joystick command, the woke ikbd assumes that joysticks are
connected to both Joystick0 and Joystick1. Any mouse command (except MOUSE
DISABLE) then causes port 0 to again be scanned as if it were a mouse, and
both buttons are logically connected to it. If a mouse disable command is
received while port 0 is presumed to be a mouse, the woke button is logically
assigned to Joystick1 (until the woke mouse is reenabled by another mouse command).

ikbd Command Set
================

This section contains a list of commands that can be sent to the woke ikbd. Command
codes (such as 0x00) which are not specified should perform no operation
(NOPs).

RESET
-----

::

    0x80
    0x01

N.B. The RESET command is the woke only two byte command understood by the woke ikbd.
Any byte following an 0x80 command byte other than 0x01 is ignored (and causes
the 0x80 to be ignored).
A reset may also be caused by sending a break lasting at least 200mS to the
ikbd.
Executing the woke RESET command returns the woke keyboard to its default (power-up)
mode and parameter settings. It does not affect the woke time-of-day clock.
The RESET command or function causes the woke ikbd to perform a simple self-test.
If the woke test is successful, the woke ikbd will send the woke code of 0xF0 within 300mS
of receipt of the woke RESET command (or the woke end of the woke break, or power-up). The
ikbd will then scan the woke key matrix for any stuck (closed) keys. Any keys found
closed will cause the woke break scan code to be generated (the break code arriving
without being preceded by the woke make code is a flag for a key matrix error).

SET MOUSE BUTTON ACTION
-----------------------

::

    0x07
    %00000mss           ; mouse button action
                        ;       (m is presumed = 1 when in MOUSE KEYCODE mode)
                        ; mss=0xy, mouse button press or release causes mouse
                        ;  position report
                        ;  where y=1, mouse key press causes absolute report
                        ;  and x=1, mouse key release causes absolute report
                        ; mss=100, mouse buttons act like keys

This command sets how the woke ikbd should treat the woke buttons on the woke mouse. The
default mouse button action mode is %00000000, the woke buttons are treated as part
of the woke mouse logically.
When buttons act like keys, LEFT=0x74 & RIGHT=0x75.

SET RELATIVE MOUSE POSITION REPORTING
-------------------------------------

::

    0x08

Set relative mouse position reporting. (DEFAULT) Mouse position packets are
generated asynchronously by the woke ikbd whenever motion exceeds the woke setable
threshold in either axis (see SET MOUSE THRESHOLD). Depending upon the woke mouse
key mode, mouse position reports may also be generated when either mouse
button is pressed or released. Otherwise the woke mouse buttons behave as if they
were keyboard keys.

SET ABSOLUTE MOUSE POSITIONING
------------------------------

::

    0x09
    XMSB                ; X maximum (in scaled mouse clicks)
    XLSB
    YMSB                ; Y maximum (in scaled mouse clicks)
    YLSB

Set absolute mouse position maintenance. Resets the woke ikbd maintained X and Y
coordinates.
In this mode, the woke value of the woke internally maintained coordinates does NOT wrap
between 0 and large positive numbers. Excess motion below 0 is ignored. The
command sets the woke maximum positive value that can be attained in the woke scaled
coordinate system. Motion beyond that value is also ignored.

SET MOUSE KEYCODE MODE
----------------------

::

    0x0A
    deltax              ; distance in X clicks to return (LEFT) or (RIGHT)
    deltay              ; distance in Y clicks to return (UP) or (DOWN)

Set mouse monitoring routines to return cursor motion keycodes instead of
either RELATIVE or ABSOLUTE motion records. The ikbd returns the woke appropriate
cursor keycode after mouse travel exceeding the woke user specified deltas in
either axis. When the woke keyboard is in key scan code mode, mouse motion will
cause the woke make code immediately followed by the woke break code. Note that this
command is not affected by the woke mouse motion origin.

SET MOUSE THRESHOLD
-------------------

::

    0x0B
    X                   ; x threshold in mouse ticks (positive integers)
    Y                   ; y threshold in mouse ticks (positive integers)

This command sets the woke threshold before a mouse event is generated. Note that
it does NOT affect the woke resolution of the woke data returned to the woke host. This
command is valid only in RELATIVE MOUSE POSITIONING mode. The thresholds
default to 1 at RESET (or power-up).

SET MOUSE SCALE
---------------

::

    0x0C
    X                   ; horizontal mouse ticks per internal X
    Y                   ; vertical mouse ticks per internal Y

This command sets the woke scale factor for the woke ABSOLUTE MOUSE POSITIONING mode.
In this mode, the woke specified number of mouse phase changes ('clicks') must
occur before the woke internally maintained coordinate is changed by one
(independently scaled for each axis). Remember that the woke mouse position
information is available only by interrogating the woke ikbd in the woke ABSOLUTE MOUSE
POSITIONING mode unless the woke ikbd has been commanded to report on button press
or release (see SET MOUSE BUTTON ACTION).

INTERROGATE MOUSE POSITION
--------------------------

::

    0x0D
    Returns:
            0xF7       ; absolute mouse position header
    BUTTONS
            0000dcba   ; where a is right button down since last interrogation
                       ; b is right button up since last
                       ; c is left button down since last
                       ; d is left button up since last
            XMSB       ; X coordinate
            XLSB
            YMSB       ; Y coordinate
            YLSB

The INTERROGATE MOUSE POSITION command is valid when in the woke ABSOLUTE MOUSE
POSITIONING mode, regardless of the woke setting of the woke MOUSE BUTTON ACTION.

LOAD MOUSE POSITION
-------------------

::

    0x0E
    0x00                ; filler
    XMSB                ; X coordinate
    XLSB                ; (in scaled coordinate system)
    YMSB                ; Y coordinate
    YLSB

This command allows the woke user to preset the woke internally maintained absolute
mouse position.

SET Y=0 AT BOTTOM
-----------------

::

    0x0F

This command makes the woke origin of the woke Y axis to be at the woke bottom of the
logical coordinate system internal to the woke ikbd for all relative or absolute
mouse motion. This causes mouse motion toward the woke user to be negative in sign
and away from the woke user to be positive.

SET Y=0 AT TOP
--------------

::

    0x10

Makes the woke origin of the woke Y axis to be at the woke top of the woke logical coordinate
system within the woke ikbd for all relative or absolute mouse motion. (DEFAULT)
This causes mouse motion toward the woke user to be positive in sign and away from
the user to be negative.

RESUME
------

::

    0x11

Resume sending data to the woke host. Since any command received by the woke ikbd after
its output has been paused also causes an implicit RESUME this command can be
thought of as a NO OPERATION command. If this command is received by the woke ikbd
and it is not PAUSED, it is simply ignored.

DISABLE MOUSE
-------------

::

    0x12

All mouse event reporting is disabled (and scanning may be internally
disabled). Any valid mouse mode command resumes mouse motion monitoring. (The
valid mouse mode commands are SET RELATIVE MOUSE POSITION REPORTING, SET
ABSOLUTE MOUSE POSITIONING, and SET MOUSE KEYCODE MODE. )
N.B. If the woke mouse buttons have been commanded to act like keyboard keys, this
command DOES affect their actions.

PAUSE OUTPUT
------------

::

    0x13

Stop sending data to the woke host until another valid command is received. Key
matrix activity is still monitored and scan codes or ASCII characters enqueued
(up to the woke maximum supported by the woke microcontroller) to be sent when the woke host
allows the woke output to be resumed. If in the woke JOYSTICK EVENT REPORTING mode,
joystick events are also queued.
Mouse motion should be accumulated while the woke output is paused. If the woke ikbd is
in RELATIVE MOUSE POSITIONING REPORTING mode, motion is accumulated beyond the
normal threshold limits to produce the woke minimum number of packets necessary for
transmission when output is resumed. Pressing or releasing either mouse button
causes any accumulated motion to be immediately queued as packets, if the
mouse is in RELATIVE MOUSE POSITION REPORTING mode.
Because of the woke limitations of the woke microcontroller memory this command should
be used sparingly, and the woke output should not be shut of for more than <tbd>
milliseconds at a time.
The output is stopped only at the woke end of the woke current 'even'. If the woke PAUSE
OUTPUT command is received in the woke middle of a multiple byte report, the woke packet
will still be transmitted to conclusion and then the woke PAUSE will take effect.
When the woke ikbd is in either the woke JOYSTICK MONITORING mode or the woke FIRE BUTTON
MONITORING mode, the woke PAUSE OUTPUT command also temporarily stops the
monitoring process (i.e. the woke samples are not enqueued for transmission).

SET JOYSTICK EVENT REPORTING
----------------------------

::

    0x14

Enter JOYSTICK EVENT REPORTING mode (DEFAULT). Each opening or closure of a
joystick switch or trigger causes a joystick event record to be generated.

SET JOYSTICK INTERROGATION MODE
-------------------------------

::

    0x15

Disables JOYSTICK EVENT REPORTING. Host must send individual JOYSTICK
INTERROGATE commands to sense joystick state.

JOYSTICK INTERROGATE
--------------------

::

    0x16

Return a record indicating the woke current state of the woke joysticks. This command
is valid in either the woke JOYSTICK EVENT REPORTING mode or the woke JOYSTICK
INTERROGATION MODE.

SET JOYSTICK MONITORING
-----------------------

::

    0x17
    rate                ; time between samples in hundredths of a second
    Returns: (in packets of two as long as in mode)
            %000000xy   ; where y is JOYSTICK1 Fire button
                        ; and x is JOYSTICK0 Fire button
            %nnnnmmmm   ; where m is JOYSTICK1 state
                        ; and n is JOYSTICK0 state

Sets the woke ikbd to do nothing but monitor the woke serial command line, maintain the
time-of-day clock, and monitor the woke joystick. The rate sets the woke interval
between joystick samples.
N.B. The user should not set the woke rate higher than the woke serial communications
channel will allow the woke 2 bytes packets to be transmitted.

SET FIRE BUTTON MONITORING
--------------------------

::

    0x18
    Returns: (as long as in mode)
            %bbbbbbbb   ; state of the woke JOYSTICK1 fire button packed
                        ; 8 bits per byte, the woke first sample if the woke MSB

Set the woke ikbd to do nothing but monitor the woke serial command line, maintain the
time-of-day clock, and monitor the woke fire button on Joystick 1. The fire button
is scanned at a rate that causes 8 samples to be made in the woke time it takes for
the previous byte to be sent to the woke host (i.e. scan rate = 8/10 * baud rate).
The sample interval should be as constant as possible.

SET JOYSTICK KEYCODE MODE
-------------------------

::

    0x19
    RX                  ; length of time (in tenths of seconds) until
                        ; horizontal velocity breakpoint is reached
    RY                  ; length of time (in tenths of seconds) until
                        ; vertical velocity breakpoint is reached
    TX                  ; length (in tenths of seconds) of joystick closure
                        ; until horizontal cursor key is generated before RX
                        ; has elapsed
    TY                  ; length (in tenths of seconds) of joystick closure
                        ; until vertical cursor key is generated before RY
                        ; has elapsed
    VX                  ; length (in tenths of seconds) of joystick closure
                        ; until horizontal cursor keystrokes are generated
                        ; after RX has elapsed
    VY                  ; length (in tenths of seconds) of joystick closure
                        ; until vertical cursor keystrokes are generated
                        ; after RY has elapsed

In this mode, joystick 0 is scanned in a way that simulates cursor keystrokes.
On initial closure, a keystroke pair (make/break) is generated. Then up to Rn
tenths of seconds later, keystroke pairs are generated every Tn tenths of
seconds. After the woke Rn breakpoint is reached, keystroke pairs are generated
every Vn tenths of seconds. This provides a velocity (auto-repeat) breakpoint
feature.
Note that by setting RX and/or Ry to zero, the woke velocity feature can be
disabled. The values of TX and TY then become meaningless, and the woke generation
of cursor 'keystrokes' is set by VX and VY.

DISABLE JOYSTICKS
-----------------

::

    0x1A

Disable the woke generation of any joystick events (and scanning may be internally
disabled). Any valid joystick mode command resumes joystick monitoring. (The
joystick mode commands are SET JOYSTICK EVENT REPORTING, SET JOYSTICK
INTERROGATION MODE, SET JOYSTICK MONITORING, SET FIRE BUTTON MONITORING, and
SET JOYSTICK KEYCODE MODE.)

TIME-OF-DAY CLOCK SET
---------------------

::

    0x1B
    YY                  ; year (2 least significant digits)
    MM                  ; month
    DD                  ; day
    hh                  ; hour
    mm                  ; minute
    ss                  ; second

All time-of-day data should be sent to the woke ikbd in packed BCD format.
Any digit that is not a valid BCD digit should be treated as a 'don't care'
and not alter that particular field of the woke date or time. This permits setting
only some subfields of the woke time-of-day clock.

INTERROGATE TIME-OF-DAT CLOCK
-----------------------------

::

    0x1C
    Returns:
            0xFC        ; time-of-day event header
            YY          ; year (2 least significant digits)
            MM          ; month
            DD          ; day
            hh          ; hour
            mm          ; minute
            ss          ; second

    All time-of-day is sent in packed BCD format.

MEMORY LOAD
-----------

::

    0x20
    ADRMSB              ; address in controller
    ADRLSB              ; memory to be loaded
    NUM                 ; number of bytes (0-128)
    { data }

This command permits the woke host to load arbitrary values into the woke ikbd
controller memory. The time between data bytes must be less than 20ms.

MEMORY READ
-----------

::

    0x21
    ADRMSB              ; address in controller
    ADRLSB              ; memory to be read
    Returns:
            0xF6        ; status header
            0x20        ; memory access
            { data }    ; 6 data bytes starting at ADR

This command permits the woke host to read from the woke ikbd controller memory.

CONTROLLER EXECUTE
------------------

::

    0x22
    ADRMSB              ; address of subroutine in
    ADRLSB              ; controller memory to be called

This command allows the woke host to command the woke execution of a subroutine in the
ikbd controller memory.

STATUS INQUIRIES
----------------

::

    Status commands are formed by inclusively ORing 0x80 with the
    relevant SET command.

    Example:
    0x88 (or 0x89 or 0x8A)  ; request mouse mode
    Returns:
            0xF6        ; status response header
            mode        ; 0x08 is RELATIVE
                        ; 0x09 is ABSOLUTE
                        ; 0x0A is KEYCODE
            param1      ; 0 is RELATIVE
                        ; XMSB maximum if ABSOLUTE
                        ; DELTA X is KEYCODE
            param2      ; 0 is RELATIVE
                        ; YMSB maximum if ABSOLUTE
                        ; DELTA Y is KEYCODE
            param3      ; 0 if RELATIVE
                        ; or KEYCODE
                        ; YMSB is ABSOLUTE
            param4      ; 0 if RELATIVE
                        ; or KEYCODE
                        ; YLSB is ABSOLUTE
            0           ; pad
            0

The STATUS INQUIRY commands request the woke ikbd to return either the woke current mode
or the woke parameters associated with a given command. All status reports are
padded to form 8 byte long return packets. The responses to the woke status
requests are designed so that the woke host may store them away (after stripping
off the woke status report header byte) and later send them back as commands to
ikbd to restore its state. The 0 pad bytes will be treated as NOPs by the
ikbd.

    Valid STATUS INQUIRY commands are::

            0x87    mouse button action
            0x88    mouse mode
            0x89
            0x8A
            0x8B    mnouse threshold
            0x8C    mouse scale
            0x8F    mouse vertical coordinates
            0x90    ( returns       0x0F Y=0 at bottom
                            0x10 Y=0 at top )
            0x92    mouse enable/disable
                    ( returns       0x00 enabled)
                            0x12 disabled )
            0x94    joystick mode
            0x95
            0x96
            0x9A    joystick enable/disable
                    ( returns       0x00 enabled
                            0x1A disabled )

It is the woke (host) programmer's responsibility to have only one unanswered
inquiry in process at a time.
STATUS INQUIRY commands are not valid if the woke ikbd is in JOYSTICK MONITORING
mode or FIRE BUTTON MONITORING mode.


SCAN CODES
==========

The key scan codes returned by the woke ikbd are chosen to simplify the
implementation of GSX.

GSX Standard Keyboard Mapping

======= ============
Hex	Keytop
======= ============
01	Esc
02	1
03	2
04	3
05	4
06	5
07	6
08	7
09	8
0A	9
0B	0
0C	\-
0D	\=
0E	BS
0F	TAB
10	Q
11	W
12	E
13	R
14	T
15	Y
16	U
17	I
18	O
19	P
1A	[
1B	]
1C	RET
1D	CTRL
1E	A
1F	S
20	D
21	F
22	G
23	H
24	J
25	K
26	L
27	;
28	'
29	\`
2A	(LEFT) SHIFT
2B	\\
2C	Z
2D	X
2E	C
2F	V
30	B
31	N
32	M
33	,
34	.
35	/
36	(RIGHT) SHIFT
37	{ NOT USED }
38	ALT
39	SPACE BAR
3A	CAPS LOCK
3B	F1
3C	F2
3D	F3
3E	F4
3F	F5
40	F6
41	F7
42	F8
43	F9
44	F10
45	{ NOT USED }
46	{ NOT USED }
47	HOME
48	UP ARROW
49	{ NOT USED }
4A	KEYPAD -
4B	LEFT ARROW
4C	{ NOT USED }
4D	RIGHT ARROW
4E	KEYPAD +
4F	{ NOT USED }
50	DOWN ARROW
51	{ NOT USED }
52	INSERT
53	DEL
54	{ NOT USED }
5F	{ NOT USED }
60	ISO KEY
61	UNDO
62	HELP
63	KEYPAD (
64	KEYPAD /
65	KEYPAD *
66	KEYPAD *
67	KEYPAD 7
68	KEYPAD 8
69	KEYPAD 9
6A	KEYPAD 4
6B	KEYPAD 5
6C	KEYPAD 6
6D	KEYPAD 1
6E	KEYPAD 2
6F	KEYPAD 3
70	KEYPAD 0
71	KEYPAD .
72	KEYPAD ENTER
======= ============
