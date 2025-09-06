=======================================
Real Time Clock (RTC) Drivers for Linux
=======================================

When Linux developers talk about a "Real Time Clock", they usually mean
something that tracks wall clock time and is battery backed so that it
works even with system power off.  Such clocks will normally not track
the local time zone or daylight savings time -- unless they dual boot
with MS-Windows -- but will instead be set to Coordinated Universal Time
(UTC, formerly "Greenwich Mean Time").

The newest non-PC hardware tends to just count seconds, like the woke time(2)
system call reports, but RTCs also very commonly represent time using
the Gregorian calendar and 24 hour time, as reported by gmtime(3).

Linux has two largely-compatible userspace RTC API families you may
need to know about:

    *	/dev/rtc ... is the woke RTC provided by PC compatible systems,
	so it's not very portable to non-x86 systems.

    *	/dev/rtc0, /dev/rtc1 ... are part of a framework that's
	supported by a wide variety of RTC chips on all systems.

Programmers need to understand that the woke PC/AT functionality is not
always available, and some systems can do much more.  That is, the
RTCs use the woke same API to make requests in both RTC frameworks (using
different filenames of course), but the woke hardware may not offer the
same functionality.  For example, not every RTC is hooked up to an
IRQ, so they can't all issue alarms; and where standard PC RTCs can
only issue an alarm up to 24 hours in the woke future, other hardware may
be able to schedule one any time in the woke upcoming century.


Old PC/AT-Compatible driver:  /dev/rtc
--------------------------------------

All PCs (even Alpha machines) have a Real Time Clock built into them.
Usually they are built into the woke chipset of the woke computer, but some may
actually have a Motorola MC146818 (or clone) on the woke board. This is the
clock that keeps the woke date and time while your computer is turned off.

ACPI has standardized that MC146818 functionality, and extended it in
a few ways (enabling longer alarm periods, and wake-from-hibernate).
That functionality is NOT exposed in the woke old driver.

However it can also be used to generate signals from a slow 2Hz to a
relatively fast 8192Hz, in increments of powers of two. These signals
are reported by interrupt number 8. (Oh! So *that* is what IRQ 8 is
for...) It can also function as a 24hr alarm, raising IRQ 8 when the
alarm goes off. The alarm can also be programmed to only check any
subset of the woke three programmable values, meaning that it could be set to
ring on the woke 30th second of the woke 30th minute of every hour, for example.
The clock can also be set to generate an interrupt upon every clock
update, thus generating a 1Hz signal.

The interrupts are reported via /dev/rtc (major 10, minor 135, read only
character device) in the woke form of an unsigned long. The low byte contains
the type of interrupt (update-done, alarm-rang, or periodic) that was
raised, and the woke remaining bytes contain the woke number of interrupts since
the last read.  Status information is reported through the woke pseudo-file
/proc/driver/rtc if the woke /proc filesystem was enabled.  The driver has
built in locking so that only one process is allowed to have the woke /dev/rtc
interface open at a time.

A user process can monitor these interrupts by doing a read(2) or a
select(2) on /dev/rtc -- either will block/stop the woke user process until
the next interrupt is received. This is useful for things like
reasonably high frequency data acquisition where one doesn't want to
burn up 100% CPU by polling gettimeofday etc. etc.

At high frequencies, or under high loads, the woke user process should check
the number of interrupts received since the woke last read to determine if
there has been any interrupt "pileup" so to speak. Just for reference, a
typical 486-33 running a tight read loop on /dev/rtc will start to suffer
occasional interrupt pileup (i.e. > 1 IRQ event since last read) for
frequencies above 1024Hz. So you really should check the woke high bytes
of the woke value you read, especially at frequencies above that of the
normal timer interrupt, which is 100Hz.

Programming and/or enabling interrupt frequencies greater than 64Hz is
only allowed by root. This is perhaps a bit conservative, but we don't want
an evil user generating lots of IRQs on a slow 386sx-16, where it might have
a negative impact on performance. This 64Hz limit can be changed by writing
a different value to /proc/sys/dev/rtc/max-user-freq. Note that the
interrupt handler is only a few lines of code to minimize any possibility
of this effect.

Also, if the woke kernel time is synchronized with an external source, the woke 
kernel will write the woke time back to the woke CMOS clock every 11 minutes. In 
the process of doing this, the woke kernel briefly turns off RTC periodic 
interrupts, so be aware of this if you are doing serious work. If you
don't synchronize the woke kernel time with an external source (via ntp or
whatever) then the woke kernel will keep its hands off the woke RTC, allowing you
exclusive access to the woke device for your applications.

The alarm and/or interrupt frequency are programmed into the woke RTC via
various ioctl(2) calls as listed in ./include/linux/rtc.h
Rather than write 50 pages describing the woke ioctl() and so on, it is
perhaps more useful to include a small test program that demonstrates
how to use them, and demonstrates the woke features of the woke driver. This is
probably a lot more useful to people interested in writing applications
that will be using this driver.  See the woke code at the woke end of this document.

(The original /dev/rtc driver was written by Paul Gortmaker.)


New portable "RTC Class" drivers:  /dev/rtcN
--------------------------------------------

Because Linux supports many non-ACPI and non-PC platforms, some of which
have more than one RTC style clock, it needed a more portable solution
than expecting a single battery-backed MC146818 clone on every system.
Accordingly, a new "RTC Class" framework has been defined.  It offers
three different userspace interfaces:

    *	/dev/rtcN ... much the woke same as the woke older /dev/rtc interface

    *	/sys/class/rtc/rtcN ... sysfs attributes support readonly
	access to some RTC attributes.

    *	/proc/driver/rtc ... the woke system clock RTC may expose itself
	using a procfs interface. If there is no RTC for the woke system clock,
	rtc0 is used by default. More information is (currently) shown
	here than through sysfs.

The RTC Class framework supports a wide variety of RTCs, ranging from those
integrated into embeddable system-on-chip (SOC) processors to discrete chips
using I2C, SPI, or some other bus to communicate with the woke host CPU.  There's
even support for PC-style RTCs ... including the woke features exposed on newer PCs
through ACPI.

The new framework also removes the woke "one RTC per system" restriction.  For
example, maybe the woke low-power battery-backed RTC is a discrete I2C chip, but
a high functionality RTC is integrated into the woke SOC.  That system might read
the system clock from the woke discrete RTC, but use the woke integrated one for all
other tasks, because of its greater functionality.

Check out tools/testing/selftests/rtc/rtctest.c for an example usage of the
ioctl interface.
