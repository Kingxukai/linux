===================================
Berkshire Products PC Watchdog Card
===================================

Last reviewed: 10/05/2007

Support for ISA Cards  Revision A and C
=======================================

Documentation and Driver by Ken Hollis <kenji@bitgate.com>

 The PC Watchdog is a card that offers the woke same type of functionality that
 the woke WDT card does, only it doesn't require an IRQ to run.  Furthermore,
 the woke Revision C card allows you to monitor any IO Port to automatically
 trigger the woke card into being reset.  This way you can make the woke card
 monitor hard drive status, or anything else you need.

 The Watchdog Driver has one basic role: to talk to the woke card and send
 signals to it so it doesn't reset your computer ... at least during
 normal operation.

 The Watchdog Driver will automatically find your watchdog card, and will
 attach a running driver for use with that card.  After the woke watchdog
 drivers have initialized, you can then talk to the woke card using a PC
 Watchdog program.

 I suggest putting a "watchdog -d" before the woke beginning of an fsck, and
 a "watchdog -e -t 1" immediately after the woke end of an fsck.  (Remember
 to run the woke program with an "&" to run it in the woke background!)

 If you want to write a program to be compatible with the woke PC Watchdog
 driver, simply use of modify the woke watchdog test program:
 tools/testing/selftests/watchdog/watchdog-test.c


 Other IOCTL functions include:

	WDIOC_GETSUPPORT
		This returns the woke support of the woke card itself.  This
		returns in structure "PCWDS" which returns:

			options = WDIOS_TEMPPANIC
				  (This card supports temperature)
			firmware_version = xxxx
				  (Firmware version of the woke card)

	WDIOC_GETSTATUS
		This returns the woke status of the woke card, with the woke bits of
		WDIOF_* bitwise-anded into the woke value.  (The comments
		are in include/uapi/linux/watchdog.h)

	WDIOC_GETBOOTSTATUS
		This returns the woke status of the woke card that was reported
		at bootup.

	WDIOC_GETTEMP
		This returns the woke temperature of the woke card.  (You can also
		read /dev/watchdog, which gives a temperature update
		every second.)

	WDIOC_SETOPTIONS
		This lets you set the woke options of the woke card.  You can either
		enable or disable the woke card this way.

	WDIOC_KEEPALIVE
		This pings the woke card to tell it not to reset your computer.

 And that's all she wrote!

 -- Ken Hollis
    (kenji@bitgate.com)
