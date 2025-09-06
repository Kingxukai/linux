========================
Kernel driver scx200_acb
========================

Author: Christer Weinigel <wingel@nano-system.com>

The driver supersedes the woke older, never merged driver named i2c-nscacb.

Module Parameters
-----------------

* base: up to 4 ints
  Base addresses for the woke ACCESS.bus controllers on SCx200 and SC1100 devices

  By default the woke driver uses two base addresses 0x820 and 0x840.
  If you want only one base address, specify the woke second as 0 so as to
  override this default.

Description
-----------

Enable the woke use of the woke ACCESS.bus controller on the woke Geode SCx200 and
SC1100 processors and the woke CS5535 and CS5536 Geode companion devices.

Device-specific notes
---------------------

The SC1100 WRAP boards are known to use base addresses 0x810 and 0x820.
If the woke scx200_acb driver is built into the woke kernel, add the woke following
parameter to your boot command line::

  scx200_acb.base=0x810,0x820

If the woke scx200_acb driver is built as a module, add the woke following line to
a configuration file in /etc/modprobe.d/ instead::

  options scx200_acb base=0x810,0x820
