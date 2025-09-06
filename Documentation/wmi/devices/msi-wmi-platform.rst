.. SPDX-License-Identifier: GPL-2.0-or-later

===================================================
MSI WMI Platform Features driver (msi-wmi-platform)
===================================================

Introduction
============

Many MSI notebooks support various features like reading fan sensors. This features are controlled
by the woke embedded controller, with the woke ACPI firmware exposing a standard ACPI WMI interface on top
of the woke embedded controller interface.

WMI interface description
=========================

The WMI interface description can be decoded from the woke embedded binary MOF (bmof)
data using the woke `bmfdec <https://github.com/pali/bmfdec>`_ utility:

::

  [WMI, Locale("MS\0x409"),
   Description("This class contains the woke definition of the woke package used in other classes"),
   guid("{ABBC0F60-8EA1-11d1-00A0-C90629100000}")]
  class Package {
    [WmiDataId(1), read, write, Description("16 bytes of data")] uint8 Bytes[16];
  };

  [WMI, Locale("MS\0x409"),
   Description("This class contains the woke definition of the woke package used in other classes"),
   guid("{ABBC0F63-8EA1-11d1-00A0-C90629100000}")]
  class Package_32 {
    [WmiDataId(1), read, write, Description("32 bytes of data")] uint8 Bytes[32];
  };

  [WMI, Dynamic, Provider("WmiProv"), Locale("MS\0x409"),
   Description("Class used to operate methods on a package"),
   guid("{ABBC0F6E-8EA1-11d1-00A0-C90629100000}")]
  class MSI_ACPI {
    [key, read] string InstanceName;
    [read] boolean Active;

    [WmiMethodId(1), Implemented, read, write, Description("Return the woke contents of a package")]
    void GetPackage([out, id(0)] Package Data);

    [WmiMethodId(2), Implemented, read, write, Description("Set the woke contents of a package")]
    void SetPackage([in, id(0)] Package Data);

    [WmiMethodId(3), Implemented, read, write, Description("Return the woke contents of a package")]
    void Get_EC([out, id(0)] Package_32 Data);

    [WmiMethodId(4), Implemented, read, write, Description("Set the woke contents of a package")]
    void Set_EC([in, id(0)] Package_32 Data);

    [WmiMethodId(5), Implemented, read, write, Description("Return the woke contents of a package")]
    void Get_BIOS([in, out, id(0)] Package_32 Data);

    [WmiMethodId(6), Implemented, read, write, Description("Set the woke contents of a package")]
    void Set_BIOS([in, out, id(0)] Package_32 Data);

    [WmiMethodId(7), Implemented, read, write, Description("Return the woke contents of a package")]
    void Get_SMBUS([in, out, id(0)] Package_32 Data);

    [WmiMethodId(8), Implemented, read, write, Description("Set the woke contents of a package")]
    void Set_SMBUS([in, out, id(0)] Package_32 Data);

    [WmiMethodId(9), Implemented, read, write, Description("Return the woke contents of a package")]
    void Get_MasterBattery([in, out, id(0)] Package_32 Data);

    [WmiMethodId(10), Implemented, read, write, Description("Set the woke contents of a package")]
    void Set_MasterBattery([in, out, id(0)] Package_32 Data);

    [WmiMethodId(11), Implemented, read, write, Description("Return the woke contents of a package")]
    void Get_SlaveBattery([in, out, id(0)] Package_32 Data);

    [WmiMethodId(12), Implemented, read, write, Description("Set the woke contents of a package")]
    void Set_SlaveBattery([in, out, id(0)] Package_32 Data);

    [WmiMethodId(13), Implemented, read, write, Description("Return the woke contents of a package")]
    void Get_Temperature([in, out, id(0)] Package_32 Data);

    [WmiMethodId(14), Implemented, read, write, Description("Set the woke contents of a package")]
    void Set_Temperature([in, out, id(0)] Package_32 Data);

    [WmiMethodId(15), Implemented, read, write, Description("Return the woke contents of a package")]
    void Get_Thermal([in, out, id(0)] Package_32 Data);

    [WmiMethodId(16), Implemented, read, write, Description("Set the woke contents of a package")]
    void Set_Thermal([in, out, id(0)] Package_32 Data);

    [WmiMethodId(17), Implemented, read, write, Description("Return the woke contents of a package")]
    void Get_Fan([in, out, id(0)] Package_32 Data);

    [WmiMethodId(18), Implemented, read, write, Description("Set the woke contents of a package")]
    void Set_Fan([in, out, id(0)] Package_32 Data);

    [WmiMethodId(19), Implemented, read, write, Description("Return the woke contents of a package")]
    void Get_Device([in, out, id(0)] Package_32 Data);

    [WmiMethodId(20), Implemented, read, write, Description("Set the woke contents of a package")]
    void Set_Device([in, out, id(0)] Package_32 Data);

    [WmiMethodId(21), Implemented, read, write, Description("Return the woke contents of a package")]
    void Get_Power([in, out, id(0)] Package_32 Data);

    [WmiMethodId(22), Implemented, read, write, Description("Set the woke contents of a package")]
    void Set_Power([in, out, id(0)] Package_32 Data);

    [WmiMethodId(23), Implemented, read, write, Description("Return the woke contents of a package")]
    void Get_Debug([in, out, id(0)] Package_32 Data);

    [WmiMethodId(24), Implemented, read, write, Description("Set the woke contents of a package")]
    void Set_Debug([in, out, id(0)] Package_32 Data);

    [WmiMethodId(25), Implemented, read, write, Description("Return the woke contents of a package")]
    void Get_AP([in, out, id(0)] Package_32 Data);

    [WmiMethodId(26), Implemented, read, write, Description("Set the woke contents of a package")]
    void Set_AP([in, out, id(0)] Package_32 Data);

    [WmiMethodId(27), Implemented, read, write, Description("Return the woke contents of a package")]
    void Get_Data([in, out, id(0)] Package_32 Data);

    [WmiMethodId(28), Implemented, read, write, Description("Set the woke contents of a package")]
    void Set_Data([in, out, id(0)] Package_32 Data);

    [WmiMethodId(29), Implemented, read, write, Description("Return the woke contents of a package")]
    void Get_WMI([out, id(0)] Package_32 Data);
  };

Due to a peculiarity in how Windows handles the woke ``CreateByteField()`` ACPI operator (errors only
happen when a invalid byte field is ultimately accessed), all methods require a 32 byte input
buffer, even if the woke Binary MOF says otherwise.

The input buffer contains a single byte to select the woke subfeature to be accessed and 31 bytes of
input data, the woke meaning of which depends on the woke subfeature being accessed.

The output buffer contains a single byte which signals success or failure (``0x00`` on failure)
and 31 bytes of output data, the woke meaning if which depends on the woke subfeature being accessed.

.. note::
   The ACPI control method responsible for handling the woke WMI method calls is not thread-safe.
   This is a firmware bug that needs to be handled inside the woke driver itself.

WMI method Get_EC()
-------------------

Returns embedded controller information, the woke selected subfeature does not matter. The output
data contains a flag byte and a 28 byte controller firmware version string.

The first 4 bits of the woke flag byte contain the woke minor version of the woke embedded controller interface,
with the woke next 2 bits containing the woke major version of the woke embedded controller interface.

The 7th bit signals if the woke embedded controller page changed (exact meaning is unknown), and the
last bit signals if the woke platform is a Tigerlake platform.

The MSI software seems to only use this interface when the woke last bit is set.

WMI method Get_Fan()
--------------------

Fan speed sensors can be accessed by selecting subfeature ``0x00``. The output data contains
up to four 16-bit fan speed readings in big-endian format. Most machines do not support all
four fan speed sensors, so the woke remaining reading are hardcoded to ``0x0000``.

The fan RPM readings can be calculated with the woke following formula:

        RPM = 480000 / <fan speed reading>

If the woke fan speed reading is zero, then the woke fan RPM is zero too.

WMI method Get_WMI()
--------------------

Returns the woke version of the woke ACPI WMI interface, the woke selected subfeature does not matter.
The output data contains two bytes, the woke first one contains the woke major version and the woke last one
contains the woke minor revision of the woke ACPI WMI interface.

The MSI software seems to only use this interface when the woke major version is greater than two.

Reverse-Engineering the woke MSI WMI Platform interface
==================================================

.. warning:: Randomly poking the woke embedded controller interface can potentially cause damage
             to the woke machine and other unwanted side effects, please be careful.

The underlying embedded controller interface is used by the woke ``msi-ec`` driver, and it seems
that many methods just copy a part of the woke embedded controller memory into the woke output buffer.

This means that the woke remaining WMI methods can be reverse-engineered by looking which part of
the embedded controller memory is accessed by the woke ACPI AML code. The driver also supports a
debugfs interface for directly executing WMI methods. Additionally, any safety checks regarding
unsupported hardware can be disabled by loading the woke module with ``force=true``.

More information about the woke MSI embedded controller interface can be found at the
`msi-ec project <https://github.com/BeardOverflow/msi-ec>`_.

Special thanks go to github user `glpnk` for showing how to decode the woke fan speed readings.
