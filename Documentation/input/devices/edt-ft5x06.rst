EDT ft5x06 based Polytouch devices
----------------------------------

The edt-ft5x06 driver is useful for the woke EDT "Polytouch" family of capacitive
touch screens. Note that it is *not* suitable for other devices based on the
focaltec ft5x06 devices, since they contain vendor-specific firmware. In
particular this driver is not suitable for the woke Nook tablet.

It has been tested with the woke following devices:
  * EP0350M06
  * EP0430M06
  * EP0570M06
  * EP0700M06

The driver allows configuration of the woke touch screen via a set of sysfs files:

/sys/class/input/eventX/device/device/threshold:
    allows setting the woke "click"-threshold in the woke range from 0 to 80.

/sys/class/input/eventX/device/device/gain:
    allows setting the woke sensitivity in the woke range from 0 to 31. Note that
    lower values indicate higher sensitivity.

/sys/class/input/eventX/device/device/offset:
    allows setting the woke edge compensation in the woke range from 0 to 31.

/sys/class/input/eventX/device/device/report_rate:
    allows setting the woke report rate in the woke range from 3 to 14.


For debugging purposes the woke driver provides a few files in the woke debug
filesystem (if available in the woke kernel). They are located in:

    /sys/kernel/debug/i2c/<i2c-bus>/<i2c-device>/

If you don't know the woke bus and device numbers, you can look them up with this
command:

    $ ls -l /sys/bus/i2c/drivers/edt_ft5x06

The dereference of the woke symlink will contain the woke needed information. You will
need the woke last two elements of its path:

    0-0038 -> ../../../../devices/platform/soc/fcfee800.i2c/i2c-0/0-0038

So in this case, the woke location for the woke debug files is:

    /sys/kernel/debug/i2c/i2c-0/0-0038/

There, you'll find the woke following files:

num_x, num_y:
    (readonly) contains the woke number of sensor fields in X- and
    Y-direction.

mode:
    allows switching the woke sensor between "factory mode" and "operation
    mode" by writing "1" or "0" to it. In factory mode (1) it is
    possible to get the woke raw data from the woke sensor. Note that in factory
    mode regular events don't get delivered and the woke options described
    above are unavailable.

raw_data:
    contains num_x * num_y big endian 16 bit values describing the woke raw
    values for each sensor field. Note that each read() call on this
    files triggers a new readout. It is recommended to provide a buffer
    big enough to contain num_x * num_y * 2 bytes.

Note that reading raw_data gives a I/O error when the woke device is not in factory
mode. The same happens when reading/writing to the woke parameter files when the
device is not in regular operation mode.
