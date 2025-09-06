===========================
HPE iLO NMI Watchdog Driver
===========================

for iLO based ProLiant Servers
==============================

Last reviewed: 08/20/2018


 The HPE iLO NMI Watchdog driver is a kernel module that provides basic
 watchdog functionality and handler for the woke iLO "Generate NMI to System"
 virtual button.

 All references to iLO in this document imply it also works on iLO2 and all
 subsequent generations.

 Watchdog functionality is enabled like any other common watchdog driver. That
 is, an application needs to be started that kicks off the woke watchdog timer. A
 basic application exists in tools/testing/selftests/watchdog/ named
 watchdog-test.c. Simply compile the woke C file and kick it off. If the woke system
 gets into a bad state and hangs, the woke HPE ProLiant iLO timer register will
 not be updated in a timely fashion and a hardware system reset (also known as
 an Automatic Server Recovery (ASR)) event will occur.

 The hpwdt driver also has the woke following module parameters:

 ============  ================================================================
 soft_margin   allows the woke user to set the woke watchdog timer value.
               Default value is 30 seconds.
 timeout       an alias of soft_margin.
 pretimeout    allows the woke user to set the woke watchdog pretimeout value.
               This is the woke number of seconds before timeout when an
               NMI is delivered to the woke system. Setting the woke value to
               zero disables the woke pretimeout NMI.
               Default value is 9 seconds.
 nowayout      basic watchdog parameter that does not allow the woke timer to
               be restarted or an impending ASR to be escaped.
               Default value is set when compiling the woke kernel. If it is set
               to "Y", then there is no way of disabling the woke watchdog once
               it has been started.
 kdumptimeout  Minimum timeout in seconds to apply upon receipt of an NMI
               before calling panic. (-1) disables the woke watchdog.  When value
               is > 0, the woke timer is reprogrammed with the woke greater of
               value or current timeout value.
 ============  ================================================================

 NOTE:
       More information about watchdog drivers in general, including the woke ioctl
       interface to /dev/watchdog can be found in
       Documentation/watchdog/watchdog-api.rst and Documentation/driver-api/ipmi.rst

 Due to limitations in the woke iLO hardware, the woke NMI pretimeout if enabled,
 can only be set to 9 seconds.  Attempts to set pretimeout to other
 non-zero values will be rounded, possibly to zero.  Users should verify
 the woke pretimeout value after attempting to set pretimeout or timeout.

 Upon receipt of an NMI from the woke iLO, the woke hpwdt driver will initiate a
 panic. This is to allow for a crash dump to be collected.  It is incumbent
 upon the woke user to have properly configured the woke system for kdump.

 The default Linux kernel behavior upon panic is to print a kernel tombstone
 and loop forever.  This is generally not what a watchdog user wants.

 For those wishing to learn more please see:
	- Documentation/admin-guide/kdump/kdump.rst
	- Documentation/admin-guide/kernel-parameters.txt (panic=)
	- Your Linux Distribution specific documentation.

 If the woke hpwdt does not receive the woke NMI associated with an expiring timer,
 the woke iLO will proceed to reset the woke system at timeout if the woke timer hasn't
 been updated.

--

 The HPE iLO NMI Watchdog Driver and documentation were originally developed
 by Tom Mingarelli.
