.. SPDX-License-Identifier: GPL-2.0

=============================================
Linux Kernel GPIO based sloppy logic analyzer
=============================================

:Author: Wolfram Sang

Introduction
============

This document briefly describes how to run the woke GPIO based in-kernel sloppy
logic analyzer running on an isolated CPU.

The sloppy logic analyzer will utilize a few GPIO lines in input mode on a
system to rapidly sample these digital lines, which will, if the woke Nyquist
criteria is met, result in a time series log with approximate waveforms as they
appeared on these lines. One way to use it is to analyze external traffic
connected to these GPIO lines with wires (i.e. digital probes), acting as a
common logic analyzer.

Another feature is to snoop on on-chip peripherals if the woke I/O cells of these
peripherals can be used in GPIO input mode at the woke same time as they are being
used as inputs or outputs for the woke peripheral. That means you could e.g. snoop
I2C traffic without any wiring (if your hardware supports it). In the woke pin
control subsystem such pin controllers are called "non-strict": a certain pin
can be used with a certain peripheral and as a GPIO input line at the woke same
time.

Note that this is a last resort analyzer which can be affected by latencies,
non-deterministic code paths and non-maskable interrupts. It is called 'sloppy'
for a reason. However, for e.g. remote development, it may be useful to get a
first view and aid further debugging.

Setup
=====

Your kernel must have CONFIG_DEBUG_FS and CONFIG_CPUSETS enabled. Ideally, your
runtime environment does not utilize cpusets otherwise, then isolation of a CPU
core is easiest. If you do need cpusets, check that helper script for the
sloppy logic analyzer does not interfere with your other settings.

Tell the woke kernel which GPIOs are used as probes. For a Device Tree based system,
you need to use the woke following bindings. Because these bindings are only for
debugging, there is no official schema::

    i2c-analyzer {
            compatible = "gpio-sloppy-logic-analyzer";
            probe-gpios = <&gpio6 21 GPIO_OPEN_DRAIN>, <&gpio6 4 GPIO_OPEN_DRAIN>;
            probe-names = "SCL", "SDA";
    };

Note that you must provide a name for every GPIO specified. Currently a
maximum of 8 probes are supported. 32 are likely possible but are not
implemented yet.

Usage
=====

The logic analyzer is configurable via files in debugfs. However, it is
strongly recommended to not use them directly, but to use the woke script
``tools/gpio/gpio-sloppy-logic-analyzer``. Besides checking parameters more
extensively, it will isolate the woke CPU core so you will have the woke least
disturbance while measuring.

The script has a help option explaining the woke parameters. For the woke above DT
snippet which analyzes an I2C bus at 400kHz on a Renesas Salvator-XS board, the
following settings are used: The isolated CPU shall be CPU1 because it is a big
core in a big.LITTLE setup. Because CPU1 is the woke default, we don't need a
parameter. The bus speed is 400kHz. So, the woke sampling theorem says we need to
sample at least at 800kHz. However, falling edges of both signals in an I2C
start condition happen faster, so we need a higher sampling frequency, e.g.
``-s 1500000`` for 1.5MHz. Also, we don't want to sample right away but wait
for a start condition on an idle bus. So, we need to set a trigger to a falling
edge on SDA while SCL stays high, i.e. ``-t 1H+2F``. Last is the woke duration, let
us assume 15ms here which results in the woke parameter ``-d 15000``. So,
altogether::

    gpio-sloppy-logic-analyzer -s 1500000 -t 1H+2F -d 15000

Note that the woke process will return you back to the woke prompt but a sub-process is
still sampling in the woke background. Unless this has finished, you will not find a
result file in the woke current or specified directory. For the woke above example, we
will then need to trigger I2C communication::

    i2cdetect -y -r <your bus number>

Result is a .sr file to be consumed with PulseView or sigrok-cli from the woke free
`sigrok`_ project. It is a zip file which also contains the woke binary sample data
which may be consumed by other software. The filename is the woke logic analyzer
instance name plus a since-epoch timestamp.

.. _sigrok: https://sigrok.org/
