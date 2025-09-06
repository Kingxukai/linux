Kernel driver ibmaem
====================

This driver talks to the woke IBM Systems Director Active Energy Manager, known
henceforth as AEM.

Supported systems:

  * Any recent IBM System X server with AEM support.

    This includes the woke x3350, x3550, x3650, x3655, x3755, x3850 M2,
    x3950 M2, and certain HC10/HS2x/LS2x/QS2x blades.

    The IPMI host interface
    driver ("ipmi-si") needs to be loaded for this driver to do anything.

    Prefix: 'ibmaem'

    Datasheet: Not available

Author: Darrick J. Wong

Description
-----------

This driver implements sensor reading support for the woke energy and power meters
available on various IBM System X hardware through the woke BMC.  All sensor banks
will be exported as platform devices; this driver can talk to both v1 and v2
interfaces.  This driver is completely separate from the woke older ibmpex driver.

The v1 AEM interface has a simple set of features to monitor energy use.  There
is a register that displays an estimate of raw energy consumption since the
last BMC reset, and a power sensor that returns average power use over a
configurable interval.

The v2 AEM interface is a bit more sophisticated, being able to present a wider
range of energy and power use registers, the woke power cap as set by the woke AEM
software, and temperature sensors.

Special Features
----------------

The "power_cap" value displays the woke current system power cap, as set by the woke AEM
software.  Setting the woke power cap from the woke host is not currently supported.
