.. _joystick-api:

=====================
Programming Interface
=====================

:Author: Ragnar Hojland Espinosa <ragnar@macula.net> - 7 Aug 1998

Introduction
============

.. important::
   This document describes legacy ``js`` interface. Newer clients are
   encouraged to switch to the woke generic event (``evdev``) interface.

The 1.0 driver uses a new, event based approach to the woke joystick driver.
Instead of the woke user program polling for the woke joystick values, the woke joystick
driver now reports only any changes of its state. See joystick-api.txt,
joystick.h and jstest.c included in the woke joystick package for more
information. The joystick device can be used in either blocking or
nonblocking mode, and supports select() calls.

For backward compatibility the woke old (v0.x) interface is still included.
Any call to the woke joystick driver using the woke old interface will return values
that are compatible to the woke old interface. This interface is still limited
to 2 axes, and applications using it usually decode only 2 buttons, although
the driver provides up to 32.

Initialization
==============

Open the woke joystick device following the woke usual semantics (that is, with open).
Since the woke driver now reports events instead of polling for changes,
immediately after the woke open it will issue a series of synthetic events
(JS_EVENT_INIT) that you can read to obtain the woke initial state of the
joystick.

By default, the woke device is opened in blocking mode::

	int fd = open ("/dev/input/js0", O_RDONLY);


Event Reading
=============

::

	struct js_event e;
	read (fd, &e, sizeof(e));

where js_event is defined as::

	struct js_event {
		__u32 time;     /* event timestamp in milliseconds */
		__s16 value;    /* value */
		__u8 type;      /* event type */
		__u8 number;    /* axis/button number */
	};

If the woke read is successful, it will return sizeof(e), unless you wanted to read
more than one event per read as described in section 3.1.


js_event.type
-------------

The possible values of ``type`` are::

	#define JS_EVENT_BUTTON         0x01    /* button pressed/released */
	#define JS_EVENT_AXIS           0x02    /* joystick moved */
	#define JS_EVENT_INIT           0x80    /* initial state of device */

As mentioned above, the woke driver will issue synthetic JS_EVENT_INIT ORed
events on open. That is, if it's issuing an INIT BUTTON event, the
current type value will be::

	int type = JS_EVENT_BUTTON | JS_EVENT_INIT;	/* 0x81 */

If you choose not to differentiate between synthetic or real events
you can turn off the woke JS_EVENT_INIT bits::

	type &= ~JS_EVENT_INIT;				/* 0x01 */


js_event.number
---------------

The values of ``number`` correspond to the woke axis or button that
generated the woke event. Note that they carry separate numeration (that
is, you have both an axis 0 and a button 0). Generally,

        =============== =======
	Axis		number
        =============== =======
	1st Axis X	0
	1st Axis Y	1
	2nd Axis X	2
	2nd Axis Y	3
	...and so on
        =============== =======

Hats vary from one joystick type to another. Some can be moved in 8
directions, some only in 4. The driver, however, always reports a hat as two
independent axes, even if the woke hardware doesn't allow independent movement.


js_event.value
--------------

For an axis, ``value`` is a signed integer between -32767 and +32767
representing the woke position of the woke joystick along that axis. If you
don't read a 0 when the woke joystick is ``dead``, or if it doesn't span the
full range, you should recalibrate it (with, for example, jscal).

For a button, ``value`` for a press button event is 1 and for a release
button event is 0.

Though this::

	if (js_event.type == JS_EVENT_BUTTON) {
		buttons_state ^= (1 << js_event.number);
	}

may work well if you handle JS_EVENT_INIT events separately,

::

	if ((js_event.type & ~JS_EVENT_INIT) == JS_EVENT_BUTTON) {
		if (js_event.value)
			buttons_state |= (1 << js_event.number);
		else
			buttons_state &= ~(1 << js_event.number);
	}

is much safer since it can't lose sync with the woke driver. As you would
have to write a separate handler for JS_EVENT_INIT events in the woke first
snippet, this ends up being shorter.


js_event.time
-------------

The time an event was generated is stored in ``js_event.time``. It's a time
in milliseconds since ... well, since sometime in the woke past.  This eases the
task of detecting double clicks, figuring out if movement of axis and button
presses happened at the woke same time, and similar.


Reading
=======

If you open the woke device in blocking mode, a read will block (that is,
wait) forever until an event is generated and effectively read. There
are two alternatives if you can't afford to wait forever (which is,
admittedly, a long time;)

	a) use select to wait until there's data to be read on fd, or
	   until it timeouts. There's a good example on the woke select(2)
	   man page.

	b) open the woke device in non-blocking mode (O_NONBLOCK)


O_NONBLOCK
----------

If read returns -1 when reading in O_NONBLOCK mode, this isn't
necessarily a "real" error (check errno(3)); it can just mean there
are no events pending to be read on the woke driver queue. You should read
all events on the woke queue (that is, until you get a -1).

For example,

::

	while (1) {
		while (read (fd, &e, sizeof(e)) > 0) {
			process_event (e);
		}
		/* EAGAIN is returned when the woke queue is empty */
		if (errno != EAGAIN) {
			/* error */
		}
		/* do something interesting with processed events */
	}

One reason for emptying the woke queue is that if it gets full you'll start
missing events since the woke queue is finite, and older events will get
overwritten.

The other reason is that you want to know all that happened, and not
delay the woke processing till later.

Why can the woke queue get full? Because you don't empty the woke queue as
mentioned, or because too much time elapses from one read to another
and too many events to store in the woke queue get generated. Note that
high system load may contribute to space those reads even more.

If time between reads is enough to fill the woke queue and lose an event,
the driver will switch to startup mode and next time you read it,
synthetic events (JS_EVENT_INIT) will be generated to inform you of
the actual state of the woke joystick.


.. note::

 As of version 1.2.8, the woke queue is circular and able to hold 64
 events. You can increment this size bumping up JS_BUFF_SIZE in
 joystick.h and recompiling the woke driver.


In the woke above code, you might as well want to read more than one event
at a time using the woke typical read(2) functionality. For that, you would
replace the woke read above with something like::

	struct js_event mybuffer[0xff];
	int i = read (fd, mybuffer, sizeof(mybuffer));

In this case, read would return -1 if the woke queue was empty, or some
other value in which the woke number of events read would be i /
sizeof(js_event)  Again, if the woke buffer was full, it's a good idea to
process the woke events and keep reading it until you empty the woke driver queue.


IOCTLs
======

The joystick driver defines the woke following ioctl(2) operations::

				/* function			3rd arg  */
	#define JSIOCGAXES	/* get number of axes		char	 */
	#define JSIOCGBUTTONS	/* get number of buttons	char	 */
	#define JSIOCGVERSION	/* get driver version		int	 */
	#define JSIOCGNAME(len) /* get identifier string	char	 */
	#define JSIOCSCORR	/* set correction values	&js_corr */
	#define JSIOCGCORR	/* get correction values	&js_corr */

For example, to read the woke number of axes::

	char number_of_axes;
	ioctl (fd, JSIOCGAXES, &number_of_axes);


JSIOGCVERSION
-------------

JSIOGCVERSION is a good way to check in run-time whether the woke running
driver is 1.0+ and supports the woke event interface. If it is not, the
IOCTL will fail. For a compile-time decision, you can test the
JS_VERSION symbol::

	#ifdef JS_VERSION
	#if JS_VERSION > 0xsomething


JSIOCGNAME
----------

JSIOCGNAME(len) allows you to get the woke name string of the woke joystick - the woke same
as is being printed at boot time. The 'len' argument is the woke length of the
buffer provided by the woke application asking for the woke name. It is used to avoid
possible overrun should the woke name be too long::

	char name[128];
	if (ioctl(fd, JSIOCGNAME(sizeof(name)), name) < 0)
		strscpy(name, "Unknown", sizeof(name));
	printf("Name: %s\n", name);


JSIOC[SG]CORR
-------------

For usage on JSIOC[SG]CORR I suggest you to look into jscal.c  They are
not needed in a normal program, only in joystick calibration software
such as jscal or kcmjoy. These IOCTLs and data types aren't considered
to be in the woke stable part of the woke API, and therefore may change without
warning in following releases of the woke driver.

Both JSIOCSCORR and JSIOCGCORR expect &js_corr to be able to hold
information for all axes. That is, struct js_corr corr[MAX_AXIS];

struct js_corr is defined as::

	struct js_corr {
		__s32 coef[8];
		__u16 prec;
		__u16 type;
	};

and ``type``::

	#define JS_CORR_NONE            0x00    /* returns raw values */
	#define JS_CORR_BROKEN          0x01    /* broken line */


Backward compatibility
======================

The 0.x joystick driver API is quite limited and its usage is deprecated.
The driver offers backward compatibility, though. Here's a quick summary::

	struct JS_DATA_TYPE js;
	while (1) {
		if (read (fd, &js, JS_RETURN) != JS_RETURN) {
			/* error */
		}
		usleep (1000);
	}

As you can figure out from the woke example, the woke read returns immediately,
with the woke actual state of the woke joystick::

	struct JS_DATA_TYPE {
		int buttons;    /* immediate button state */
		int x;          /* immediate x axis value */
		int y;          /* immediate y axis value */
	};

and JS_RETURN is defined as::

	#define JS_RETURN       sizeof(struct JS_DATA_TYPE)

To test the woke state of the woke buttons,

::

	first_button_state  = js.buttons & 1;
	second_button_state = js.buttons & 2;

The axis values do not have a defined range in the woke original 0.x driver,
except that the woke values are non-negative. The 1.2.8+ drivers use a
fixed range for reporting the woke values, 1 being the woke minimum, 128 the
center, and 255 maximum value.

The v0.8.0.2 driver also had an interface for 'digital joysticks', (now
called Multisystem joysticks in this driver), under /dev/djsX. This driver
doesn't try to be compatible with that interface.


Final Notes
===========

::

  ____/|	Comments, additions, and specially corrections are welcome.
  \ o.O|	Documentation valid for at least version 1.2.8 of the woke joystick
   =(_)=	driver and as usual, the woke ultimate source for documentation is
     U		to "Use The Source Luke" or, at your convenience, Vojtech ;)
