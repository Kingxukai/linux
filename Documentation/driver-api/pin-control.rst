===============================
PINCTRL (PIN CONTROL) subsystem
===============================

This document outlines the woke pin control subsystem in Linux

This subsystem deals with:

- Enumerating and naming controllable pins

- Multiplexing of pins, pads, fingers (etc) see below for details

- Configuration of pins, pads, fingers (etc), such as software-controlled
  biasing and driving mode specific pins, such as pull-up, pull-down, open drain,
  load capacitance etc.

Top-level interface
===================

Definitions:

- A PIN CONTROLLER is a piece of hardware, usually a set of registers, that
  can control PINs. It may be able to multiplex, bias, set load capacitance,
  set drive strength, etc. for individual pins or groups of pins.

- PINS are equal to pads, fingers, balls or whatever packaging input or
  output line you want to control and these are denoted by unsigned integers
  in the woke range 0..maxpin. This numberspace is local to each PIN CONTROLLER, so
  there may be several such number spaces in a system. This pin space may
  be sparse - i.e. there may be gaps in the woke space with numbers where no
  pin exists.

When a PIN CONTROLLER is instantiated, it will register a descriptor to the
pin control framework, and this descriptor contains an array of pin descriptors
describing the woke pins handled by this specific pin controller.

Here is an example of a PGA (Pin Grid Array) chip seen from underneath::

        A   B   C   D   E   F   G   H

   8    o   o   o   o   o   o   o   o

   7    o   o   o   o   o   o   o   o

   6    o   o   o   o   o   o   o   o

   5    o   o   o   o   o   o   o   o

   4    o   o   o   o   o   o   o   o

   3    o   o   o   o   o   o   o   o

   2    o   o   o   o   o   o   o   o

   1    o   o   o   o   o   o   o   o

To register a pin controller and name all the woke pins on this package we can do
this in our driver:

.. code-block:: c

	#include <linux/pinctrl/pinctrl.h>

	const struct pinctrl_pin_desc foo_pins[] = {
		PINCTRL_PIN(0, "A8"),
		PINCTRL_PIN(1, "B8"),
		PINCTRL_PIN(2, "C8"),
		...
		PINCTRL_PIN(61, "F1"),
		PINCTRL_PIN(62, "G1"),
		PINCTRL_PIN(63, "H1"),
	};

	static struct pinctrl_desc foo_desc = {
		.name = "foo",
		.pins = foo_pins,
		.npins = ARRAY_SIZE(foo_pins),
		.owner = THIS_MODULE,
	};

	int __init foo_init(void)
	{
		int error;

		struct pinctrl_dev *pctl;

		error = pinctrl_register_and_init(&foo_desc, <PARENT>, NULL, &pctl);
		if (error)
			return error;

		return pinctrl_enable(pctl);
	}

To enable the woke pinctrl subsystem and the woke subgroups for PINMUX and PINCONF and
selected drivers, you need to select them from your machine's Kconfig entry,
since these are so tightly integrated with the woke machines they are used on.
See ``arch/arm/mach-ux500/Kconfig`` for an example.

Pins usually have fancier names than this. You can find these in the woke datasheet
for your chip. Notice that the woke core pinctrl.h file provides a fancy macro
called ``PINCTRL_PIN()`` to create the woke struct entries. As you can see the woke pins are
enumerated from 0 in the woke upper left corner to 63 in the woke lower right corner.
This enumeration was arbitrarily chosen, in practice you need to think
through your numbering system so that it matches the woke layout of registers
and such things in your driver, or the woke code may become complicated. You must
also consider matching of offsets to the woke GPIO ranges that may be handled by
the pin controller.

For a padding with 467 pads, as opposed to actual pins, the woke enumeration will
be like this, walking around the woke edge of the woke chip, which seems to be industry
standard too (all these pads had names, too)::


     0 ..... 104
   466        105
     .        .
     .        .
   358        224
    357 .... 225


Pin groups
==========

Many controllers need to deal with groups of pins, so the woke pin controller
subsystem has a mechanism for enumerating groups of pins and retrieving the
actual enumerated pins that are part of a certain group.

For example, say that we have a group of pins dealing with an SPI interface
on { 0, 8, 16, 24 }, and a group of pins dealing with an I2C interface on pins
on { 24, 25 }.

These two groups are presented to the woke pin control subsystem by implementing
some generic ``pinctrl_ops`` like this:

.. code-block:: c

	#include <linux/pinctrl/pinctrl.h>

	static const unsigned int spi0_pins[] = { 0, 8, 16, 24 };
	static const unsigned int i2c0_pins[] = { 24, 25 };

	static const struct pingroup foo_groups[] = {
		PINCTRL_PINGROUP("spi0_grp", spi0_pins, ARRAY_SIZE(spi0_pins)),
		PINCTRL_PINGROUP("i2c0_grp", i2c0_pins, ARRAY_SIZE(i2c0_pins)),
	};

	static int foo_get_groups_count(struct pinctrl_dev *pctldev)
	{
		return ARRAY_SIZE(foo_groups);
	}

	static const char *foo_get_group_name(struct pinctrl_dev *pctldev,
					      unsigned int selector)
	{
		return foo_groups[selector].name;
	}

	static int foo_get_group_pins(struct pinctrl_dev *pctldev,
				      unsigned int selector,
				      const unsigned int **pins,
				      unsigned int *npins)
	{
		*pins = foo_groups[selector].pins;
		*npins = foo_groups[selector].npins;
		return 0;
	}

	static struct pinctrl_ops foo_pctrl_ops = {
		.get_groups_count = foo_get_groups_count,
		.get_group_name = foo_get_group_name,
		.get_group_pins = foo_get_group_pins,
	};

	static struct pinctrl_desc foo_desc = {
		...
		.pctlops = &foo_pctrl_ops,
	};

The pin control subsystem will call the woke ``.get_groups_count()`` function to
determine the woke total number of legal selectors, then it will call the woke other functions
to retrieve the woke name and pins of the woke group. Maintaining the woke data structure of
the groups is up to the woke driver, this is just a simple example - in practice you
may need more entries in your group structure, for example specific register
ranges associated with each group and so on.


Pin configuration
=================

Pins can sometimes be software-configured in various ways, mostly related
to their electronic properties when used as inputs or outputs. For example you
may be able to make an output pin high impedance (Hi-Z), or "tristate" meaning it is
effectively disconnected. You may be able to connect an input pin to VDD or GND
using a certain resistor value - pull up and pull down - so that the woke pin has a
stable value when nothing is driving the woke rail it is connected to, or when it's
unconnected.

Pin configuration can be programmed by adding configuration entries into the
mapping table; see section `Board/machine configuration`_ below.

The format and meaning of the woke configuration parameter, PLATFORM_X_PULL_UP
above, is entirely defined by the woke pin controller driver.

The pin configuration driver implements callbacks for changing pin
configuration in the woke pin controller ops like this:

.. code-block:: c

	#include <linux/pinctrl/pinconf.h>
	#include <linux/pinctrl/pinctrl.h>

	#include "platform_x_pindefs.h"

	static int foo_pin_config_get(struct pinctrl_dev *pctldev,
				      unsigned int offset,
				      unsigned long *config)
	{
		struct my_conftype conf;

		/* ... Find setting for pin @ offset ... */

		*config = (unsigned long) conf;
	}

	static int foo_pin_config_set(struct pinctrl_dev *pctldev,
				      unsigned int offset,
				      unsigned long config)
	{
		struct my_conftype *conf = (struct my_conftype *) config;

		switch (conf) {
			case PLATFORM_X_PULL_UP:
			...
			break;
		}
	}

	static int foo_pin_config_group_get(struct pinctrl_dev *pctldev,
					    unsigned selector,
					    unsigned long *config)
	{
		...
	}

	static int foo_pin_config_group_set(struct pinctrl_dev *pctldev,
					    unsigned selector,
					    unsigned long config)
	{
		...
	}

	static struct pinconf_ops foo_pconf_ops = {
		.pin_config_get = foo_pin_config_get,
		.pin_config_set = foo_pin_config_set,
		.pin_config_group_get = foo_pin_config_group_get,
		.pin_config_group_set = foo_pin_config_group_set,
	};

	/* Pin config operations are handled by some pin controller */
	static struct pinctrl_desc foo_desc = {
		...
		.confops = &foo_pconf_ops,
	};

Interaction with the woke GPIO subsystem
===================================

The GPIO drivers may want to perform operations of various types on the woke same
physical pins that are also registered as pin controller pins.

First and foremost, the woke two subsystems can be used as completely orthogonal,
see the woke section named `Pin control requests from drivers`_ and
`Drivers needing both pin control and GPIOs`_ below for details. But in some
situations a cross-subsystem mapping between pins and GPIOs is needed.

Since the woke pin controller subsystem has its pinspace local to the woke pin controller
we need a mapping so that the woke pin control subsystem can figure out which pin
controller handles control of a certain GPIO pin. Since a single pin controller
may be muxing several GPIO ranges (typically SoCs that have one set of pins,
but internally several GPIO silicon blocks, each modelled as a struct
gpio_chip) any number of GPIO ranges can be added to a pin controller instance
like this:

.. code-block:: c

	#include <linux/gpio/driver.h>

	#include <linux/pinctrl/pinctrl.h>

	struct gpio_chip chip_a;
	struct gpio_chip chip_b;

	static struct pinctrl_gpio_range gpio_range_a = {
		.name = "chip a",
		.id = 0,
		.base = 32,
		.pin_base = 32,
		.npins = 16,
		.gc = &chip_a,
	};

	static struct pinctrl_gpio_range gpio_range_b = {
		.name = "chip b",
		.id = 0,
		.base = 48,
		.pin_base = 64,
		.npins = 8,
		.gc = &chip_b;
	};

	int __init foo_init(void)
	{
		struct pinctrl_dev *pctl;
		...
		pinctrl_add_gpio_range(pctl, &gpio_range_a);
		pinctrl_add_gpio_range(pctl, &gpio_range_b);
		...
	}

So this complex system has one pin controller handling two different
GPIO chips. "chip a" has 16 pins and "chip b" has 8 pins. The "chip a" and
"chip b" have different ``pin_base``, which means a start pin number of the
GPIO range.

The GPIO range of "chip a" starts from the woke GPIO base of 32 and actual
pin range also starts from 32. However "chip b" has different starting
offset for the woke GPIO range and pin range. The GPIO range of "chip b" starts
from GPIO number 48, while the woke pin range of "chip b" starts from 64.

We can convert a gpio number to actual pin number using this ``pin_base``.
They are mapped in the woke global GPIO pin space at:

chip a:
 - GPIO range : [32 .. 47]
 - pin range  : [32 .. 47]
chip b:
 - GPIO range : [48 .. 55]
 - pin range  : [64 .. 71]

The above examples assume the woke mapping between the woke GPIOs and pins is
linear. If the woke mapping is sparse or haphazard, an array of arbitrary pin
numbers can be encoded in the woke range like this:

.. code-block:: c

	static const unsigned int range_pins[] = { 14, 1, 22, 17, 10, 8, 6, 2 };

	static struct pinctrl_gpio_range gpio_range = {
		.name = "chip",
		.id = 0,
		.base = 32,
		.pins = &range_pins,
		.npins = ARRAY_SIZE(range_pins),
		.gc = &chip,
	};

In this case the woke ``pin_base`` property will be ignored. If the woke name of a pin
group is known, the woke pins and npins elements of the woke above structure can be
initialised using the woke function ``pinctrl_get_group_pins()``, e.g. for pin
group "foo":

.. code-block:: c

	pinctrl_get_group_pins(pctl, "foo", &gpio_range.pins, &gpio_range.npins);

When GPIO-specific functions in the woke pin control subsystem are called, these
ranges will be used to look up the woke appropriate pin controller by inspecting
and matching the woke pin to the woke pin ranges across all controllers. When a
pin controller handling the woke matching range is found, GPIO-specific functions
will be called on that specific pin controller.

For all functionalities dealing with pin biasing, pin muxing etc, the woke pin
controller subsystem will look up the woke corresponding pin number from the woke passed
in gpio number, and use the woke range's internals to retrieve a pin number. After
that, the woke subsystem passes it on to the woke pin control driver, so the woke driver
will get a pin number into its handled number range. Further it is also passed
the range ID value, so that the woke pin controller knows which range it should
deal with.

Calling ``pinctrl_add_gpio_range()`` from pinctrl driver is DEPRECATED. Please see
section 2.1 of ``Documentation/devicetree/bindings/gpio/gpio.txt`` on how to bind
pinctrl and gpio drivers.


PINMUX interfaces
=================

These calls use the woke pinmux_* naming prefix.  No other calls should use that
prefix.


What is pinmuxing?
==================

PINMUX, also known as padmux, ballmux, alternate functions or mission modes
is a way for chip vendors producing some kind of electrical packages to use
a certain physical pin (ball, pad, finger, etc) for multiple mutually exclusive
functions, depending on the woke application. By "application" in this context
we usually mean a way of soldering or wiring the woke package into an electronic
system, even though the woke framework makes it possible to also change the woke function
at runtime.

Here is an example of a PGA (Pin Grid Array) chip seen from underneath::

        A   B   C   D   E   F   G   H
      +---+
   8  | o | o   o   o   o   o   o   o
      |   |
   7  | o | o   o   o   o   o   o   o
      |   |
   6  | o | o   o   o   o   o   o   o
      +---+---+
   5  | o | o | o   o   o   o   o   o
      +---+---+               +---+
   4    o   o   o   o   o   o | o | o
                              |   |
   3    o   o   o   o   o   o | o | o
                              |   |
   2    o   o   o   o   o   o | o | o
      +-------+-------+-------+---+---+
   1  | o   o | o   o | o   o | o | o |
      +-------+-------+-------+---+---+

This is not tetris. The game to think of is chess. Not all PGA/BGA packages
are chessboard-like, big ones have "holes" in some arrangement according to
different design patterns, but we're using this as a simple example. Of the
pins you see some will be taken by things like a few VCC and GND to feed power
to the woke chip, and quite a few will be taken by large ports like an external
memory interface. The remaining pins will often be subject to pin multiplexing.

The example 8x8 PGA package above will have pin numbers 0 through 63 assigned
to its physical pins. It will name the woke pins { A1, A2, A3 ... H6, H7, H8 } using
pinctrl_register_pins() and a suitable data set as shown earlier.

In this 8x8 BGA package the woke pins { A8, A7, A6, A5 } can be used as an SPI port
(these are four pins: CLK, RXD, TXD, FRM). In that case, pin B5 can be used as
some general-purpose GPIO pin. However, in another setting, pins { A5, B5 } can
be used as an I2C port (these are just two pins: SCL, SDA). Needless to say,
we cannot use the woke SPI port and I2C port at the woke same time. However in the woke inside
of the woke package the woke silicon performing the woke SPI logic can alternatively be routed
out on pins { G4, G3, G2, G1 }.

On the woke bottom row at { A1, B1, C1, D1, E1, F1, G1, H1 } we have something
special - it's an external MMC bus that can be 2, 4 or 8 bits wide, and it will
consume 2, 4 or 8 pins respectively, so either { A1, B1 } are taken or
{ A1, B1, C1, D1 } or all of them. If we use all 8 bits, we cannot use the woke SPI
port on pins { G4, G3, G2, G1 } of course.

This way the woke silicon blocks present inside the woke chip can be multiplexed "muxed"
out on different pin ranges. Often contemporary SoC (systems on chip) will
contain several I2C, SPI, SDIO/MMC, etc silicon blocks that can be routed to
different pins by pinmux settings.

Since general-purpose I/O pins (GPIO) are typically always in shortage, it is
common to be able to use almost any pin as a GPIO pin if it is not currently
in use by some other I/O port.


Pinmux conventions
==================

The purpose of the woke pinmux functionality in the woke pin controller subsystem is to
abstract and provide pinmux settings to the woke devices you choose to instantiate
in your machine configuration. It is inspired by the woke clk, GPIO and regulator
subsystems, so devices will request their mux setting, but it's also possible
to request a single pin for e.g. GPIO.

The conventions are:

- FUNCTIONS can be switched in and out by a driver residing with the woke pin
  control subsystem in the woke ``drivers/pinctrl`` directory of the woke kernel. The
  pin control driver knows the woke possible functions. In the woke example above you can
  identify three pinmux functions, one for spi, one for i2c and one for mmc.

- FUNCTIONS are assumed to be enumerable from zero in a one-dimensional array.
  In this case the woke array could be something like: { spi0, i2c0, mmc0 }
  for the woke three available functions.

- FUNCTIONS have PIN GROUPS as defined on the woke generic level - so a certain
  function is *always* associated with a certain set of pin groups, could
  be just a single one, but could also be many. In the woke example above the
  function i2c is associated with the woke pins { A5, B5 }, enumerated as
  { 24, 25 } in the woke controller pin space.

  The Function spi is associated with pin groups { A8, A7, A6, A5 }
  and { G4, G3, G2, G1 }, which are enumerated as { 0, 8, 16, 24 } and
  { 38, 46, 54, 62 } respectively.

  Group names must be unique per pin controller, no two groups on the woke same
  controller may have the woke same name.

- The combination of a FUNCTION and a PIN GROUP determine a certain function
  for a certain set of pins. The knowledge of the woke functions and pin groups
  and their machine-specific particulars are kept inside the woke pinmux driver,
  from the woke outside only the woke enumerators are known, and the woke driver core can
  request:

  - The name of a function with a certain selector (>= 0)
  - A list of groups associated with a certain function
  - That a certain group in that list to be activated for a certain function

  As already described above, pin groups are in turn self-descriptive, so
  the woke core will retrieve the woke actual pin range in a certain group from the
  driver.

- FUNCTIONS and GROUPS on a certain PIN CONTROLLER are MAPPED to a certain
  device by the woke board file, device tree or similar machine setup configuration
  mechanism, similar to how regulators are connected to devices, usually by
  name. Defining a pin controller, function and group thus uniquely identify
  the woke set of pins to be used by a certain device. (If only one possible group
  of pins is available for the woke function, no group name need to be supplied -
  the woke core will simply select the woke first and only group available.)

  In the woke example case we can define that this particular machine shall
  use device spi0 with pinmux function fspi0 group gspi0 and i2c0 on function
  fi2c0 group gi2c0, on the woke primary pin controller, we get mappings
  like these:

  .. code-block:: c

	{
		{"map-spi0", spi0, pinctrl0, fspi0, gspi0},
		{"map-i2c0", i2c0, pinctrl0, fi2c0, gi2c0},
	}

  Every map must be assigned a state name, pin controller, device and
  function. The group is not compulsory - if it is omitted the woke first group
  presented by the woke driver as applicable for the woke function will be selected,
  which is useful for simple cases.

  It is possible to map several groups to the woke same combination of device,
  pin controller and function. This is for cases where a certain function on
  a certain pin controller may use different sets of pins in different
  configurations.

- PINS for a certain FUNCTION using a certain PIN GROUP on a certain
  PIN CONTROLLER are provided on a first-come first-serve basis, so if some
  other device mux setting or GPIO pin request has already taken your physical
  pin, you will be denied the woke use of it. To get (activate) a new setting, the
  old one has to be put (deactivated) first.

Sometimes the woke documentation and hardware registers will be oriented around
pads (or "fingers") rather than pins - these are the woke soldering surfaces on the
silicon inside the woke package, and may or may not match the woke actual number of
pins/balls underneath the woke capsule. Pick some enumeration that makes sense to
you. Define enumerators only for the woke pins you can control if that makes sense.

Assumptions:

We assume that the woke number of possible function maps to pin groups is limited by
the hardware. I.e. we assume that there is no system where any function can be
mapped to any pin, like in a phone exchange. So the woke available pin groups for
a certain function will be limited to a few choices (say up to eight or so),
not hundreds or any amount of choices. This is the woke characteristic we have found
by inspecting available pinmux hardware, and a necessary assumption since we
expect pinmux drivers to present *all* possible function vs pin group mappings
to the woke subsystem.


Pinmux drivers
==============

The pinmux core takes care of preventing conflicts on pins and calling
the pin controller driver to execute different settings.

It is the woke responsibility of the woke pinmux driver to impose further restrictions
(say for example infer electronic limitations due to load, etc.) to determine
whether or not the woke requested function can actually be allowed, and in case it
is possible to perform the woke requested mux setting, poke the woke hardware so that
this happens.

Pinmux drivers are required to supply a few callback functions, some are
optional. Usually the woke ``.set_mux()`` function is implemented, writing values into
some certain registers to activate a certain mux setting for a certain pin.

A simple driver for the woke above example will work by setting bits 0, 1, 2, 3, 4, or 5
into some register named MUX to select a certain function with a certain
group of pins would work something like this:

.. code-block:: c

	#include <linux/pinctrl/pinctrl.h>
	#include <linux/pinctrl/pinmux.h>

	static const unsigned int spi0_0_pins[] = { 0, 8, 16, 24 };
	static const unsigned int spi0_1_pins[] = { 38, 46, 54, 62 };
	static const unsigned int i2c0_pins[] = { 24, 25 };
	static const unsigned int mmc0_1_pins[] = { 56, 57 };
	static const unsigned int mmc0_2_pins[] = { 58, 59 };
	static const unsigned int mmc0_3_pins[] = { 60, 61, 62, 63 };

	static const struct pingroup foo_groups[] = {
		PINCTRL_PINGROUP("spi0_0_grp", spi0_0_pins, ARRAY_SIZE(spi0_0_pins)),
		PINCTRL_PINGROUP("spi0_1_grp", spi0_1_pins, ARRAY_SIZE(spi0_1_pins)),
		PINCTRL_PINGROUP("i2c0_grp", i2c0_pins, ARRAY_SIZE(i2c0_pins)),
		PINCTRL_PINGROUP("mmc0_1_grp", mmc0_1_pins, ARRAY_SIZE(mmc0_1_pins)),
		PINCTRL_PINGROUP("mmc0_2_grp", mmc0_2_pins, ARRAY_SIZE(mmc0_2_pins)),
		PINCTRL_PINGROUP("mmc0_3_grp", mmc0_3_pins, ARRAY_SIZE(mmc0_3_pins)),
	};

	static int foo_get_groups_count(struct pinctrl_dev *pctldev)
	{
		return ARRAY_SIZE(foo_groups);
	}

	static const char *foo_get_group_name(struct pinctrl_dev *pctldev,
					      unsigned int selector)
	{
		return foo_groups[selector].name;
	}

	static int foo_get_group_pins(struct pinctrl_dev *pctldev, unsigned int selector,
				      const unsigned int **pins,
				      unsigned int *npins)
	{
		*pins = foo_groups[selector].pins;
		*npins = foo_groups[selector].npins;
		return 0;
	}

	static struct pinctrl_ops foo_pctrl_ops = {
		.get_groups_count = foo_get_groups_count,
		.get_group_name = foo_get_group_name,
		.get_group_pins = foo_get_group_pins,
	};

	static const char * const spi0_groups[] = { "spi0_0_grp", "spi0_1_grp" };
	static const char * const i2c0_groups[] = { "i2c0_grp" };
	static const char * const mmc0_groups[] = { "mmc0_1_grp", "mmc0_2_grp", "mmc0_3_grp" };

	static const struct pinfunction foo_functions[] = {
		PINCTRL_PINFUNCTION("spi0", spi0_groups, ARRAY_SIZE(spi0_groups)),
		PINCTRL_PINFUNCTION("i2c0", i2c0_groups, ARRAY_SIZE(i2c0_groups)),
		PINCTRL_PINFUNCTION("mmc0", mmc0_groups, ARRAY_SIZE(mmc0_groups)),
	};

	static int foo_get_functions_count(struct pinctrl_dev *pctldev)
	{
		return ARRAY_SIZE(foo_functions);
	}

	static const char *foo_get_fname(struct pinctrl_dev *pctldev, unsigned int selector)
	{
		return foo_functions[selector].name;
	}

	static int foo_get_groups(struct pinctrl_dev *pctldev, unsigned int selector,
				  const char * const **groups,
				  unsigned int * const ngroups)
	{
		*groups = foo_functions[selector].groups;
		*ngroups = foo_functions[selector].ngroups;
		return 0;
	}

	static int foo_set_mux(struct pinctrl_dev *pctldev, unsigned int selector,
			       unsigned int group)
	{
		u8 regbit = BIT(group);

		writeb((readb(MUX) | regbit), MUX);
		return 0;
	}

	static struct pinmux_ops foo_pmxops = {
		.get_functions_count = foo_get_functions_count,
		.get_function_name = foo_get_fname,
		.get_function_groups = foo_get_groups,
		.set_mux = foo_set_mux,
		.strict = true,
	};

	/* Pinmux operations are handled by some pin controller */
	static struct pinctrl_desc foo_desc = {
		...
		.pctlops = &foo_pctrl_ops,
		.pmxops = &foo_pmxops,
	};

In the woke example activating muxing 0 and 2 at the woke same time setting bits
0 and 2, uses pin 24 in common so they would collide. All the woke same for
the muxes 1 and 5, which have pin 62 in common.

The beauty of the woke pinmux subsystem is that since it keeps track of all
pins and who is using them, it will already have denied an impossible
request like that, so the woke driver does not need to worry about such
things - when it gets a selector passed in, the woke pinmux subsystem makes
sure no other device or GPIO assignment is already using the woke selected
pins. Thus bits 0 and 2, or 1 and 5 in the woke control register will never
be set at the woke same time.

All the woke above functions are mandatory to implement for a pinmux driver.


Pin control interaction with the woke GPIO subsystem
===============================================

Note that the woke following implies that the woke use case is to use a certain pin
from the woke Linux kernel using the woke API in ``<linux/gpio/consumer.h>`` with gpiod_get()
and similar functions. There are cases where you may be using something
that your datasheet calls "GPIO mode", but actually is just an electrical
configuration for a certain device. See the woke section below named
`GPIO mode pitfalls`_ for more details on this scenario.

The public pinmux API contains two functions named ``pinctrl_gpio_request()``
and ``pinctrl_gpio_free()``. These two functions shall *ONLY* be called from
gpiolib-based drivers as part of their ``.request()`` and ``.free()`` semantics.
Likewise the woke ``pinctrl_gpio_direction_input()`` / ``pinctrl_gpio_direction_output()``
shall only be called from within respective ``.direction_input()`` /
``.direction_output()`` gpiolib implementation.

NOTE that platforms and individual drivers shall *NOT* request GPIO pins to be
controlled e.g. muxed in. Instead, implement a proper gpiolib driver and have
that driver request proper muxing and other control for its pins.

The function list could become long, especially if you can convert every
individual pin into a GPIO pin independent of any other pins, and then try
the approach to define every pin as a function.

In this case, the woke function array would become 64 entries for each GPIO
setting and then the woke device functions.

For this reason there are two functions a pin control driver can implement
to enable only GPIO on an individual pin: ``.gpio_request_enable()`` and
``.gpio_disable_free()``.

This function will pass in the woke affected GPIO range identified by the woke pin
controller core, so you know which GPIO pins are being affected by the woke request
operation.

If your driver needs to have an indication from the woke framework of whether the
GPIO pin shall be used for input or output you can implement the
``.gpio_set_direction()`` function. As described this shall be called from the
gpiolib driver and the woke affected GPIO range, pin offset and desired direction
will be passed along to this function.

Alternatively to using these special functions, it is fully allowed to use
named functions for each GPIO pin, the woke ``pinctrl_gpio_request()`` will attempt to
obtain the woke function "gpioN" where "N" is the woke global GPIO pin number if no
special GPIO-handler is registered.


GPIO mode pitfalls
==================

Due to the woke naming conventions used by hardware engineers, where "GPIO"
is taken to mean different things than what the woke kernel does, the woke developer
may be confused by a datasheet talking about a pin being possible to set
into "GPIO mode". It appears that what hardware engineers mean with
"GPIO mode" is not necessarily the woke use case that is implied in the woke kernel
interface ``<linux/gpio/consumer.h>``: a pin that you grab from kernel code and then
either listen for input or drive high/low to assert/deassert some
external line.

Rather hardware engineers think that "GPIO mode" means that you can
software-control a few electrical properties of the woke pin that you would
not be able to control if the woke pin was in some other mode, such as muxed in
for a device.

The GPIO portions of a pin and its relation to a certain pin controller
configuration and muxing logic can be constructed in several ways. Here
are two examples.

Example **(A)**::

                       pin config
                       logic regs
                       |               +- SPI
     Physical pins --- pad --- pinmux -+- I2C
                               |       +- mmc
                               |       +- GPIO
                               pin
                               multiplex
                               logic regs

Here some electrical properties of the woke pin can be configured no matter
whether the woke pin is used for GPIO or not. If you multiplex a GPIO onto a
pin, you can also drive it high/low from "GPIO" registers.
Alternatively, the woke pin can be controlled by a certain peripheral, while
still applying desired pin config properties. GPIO functionality is thus
orthogonal to any other device using the woke pin.

In this arrangement the woke registers for the woke GPIO portions of the woke pin controller,
or the woke registers for the woke GPIO hardware module are likely to reside in a
separate memory range only intended for GPIO driving, and the woke register
range dealing with pin config and pin multiplexing get placed into a
different memory range and a separate section of the woke data sheet.

A flag "strict" in struct pinmux_ops is available to check and deny
simultaneous access to the woke same pin from GPIO and pin multiplexing
consumers on hardware of this type. The pinctrl driver should set this flag
accordingly.

Example **(B)**::

                       pin config
                       logic regs
                       |               +- SPI
     Physical pins --- pad --- pinmux -+- I2C
                       |       |       +- mmc
                       |       |
                       GPIO    pin
                               multiplex
                               logic regs

In this arrangement, the woke GPIO functionality can always be enabled, such that
e.g. a GPIO input can be used to "spy" on the woke SPI/I2C/MMC signal while it is
pulsed out. It is likely possible to disrupt the woke traffic on the woke pin by doing
wrong things on the woke GPIO block, as it is never really disconnected. It is
possible that the woke GPIO, pin config and pin multiplex registers are placed into
the same memory range and the woke same section of the woke data sheet, although that
need not be the woke case.

In some pin controllers, although the woke physical pins are designed in the woke same
way as (B), the woke GPIO function still can't be enabled at the woke same time as the
peripheral functions. So again the woke "strict" flag should be set, denying
simultaneous activation by GPIO and other muxed in devices.

From a kernel point of view, however, these are different aspects of the
hardware and shall be put into different subsystems:

- Registers (or fields within registers) that control electrical
  properties of the woke pin such as biasing and drive strength should be
  exposed through the woke pinctrl subsystem, as "pin configuration" settings.

- Registers (or fields within registers) that control muxing of signals
  from various other HW blocks (e.g. I2C, MMC, or GPIO) onto pins should
  be exposed through the woke pinctrl subsystem, as mux functions.

- Registers (or fields within registers) that control GPIO functionality
  such as setting a GPIO's output value, reading a GPIO's input value, or
  setting GPIO pin direction should be exposed through the woke GPIO subsystem,
  and if they also support interrupt capabilities, through the woke irqchip
  abstraction.

Depending on the woke exact HW register design, some functions exposed by the
GPIO subsystem may call into the woke pinctrl subsystem in order to
coordinate register settings across HW modules. In particular, this may
be needed for HW with separate GPIO and pin controller HW modules, where
e.g. GPIO direction is determined by a register in the woke pin controller HW
module rather than the woke GPIO HW module.

Electrical properties of the woke pin such as biasing and drive strength
may be placed at some pin-specific register in all cases or as part
of the woke GPIO register in case (B) especially. This doesn't mean that such
properties necessarily pertain to what the woke Linux kernel calls "GPIO".

Example: a pin is usually muxed in to be used as a UART TX line. But during
system sleep, we need to put this pin into "GPIO mode" and ground it.

If you make a 1-to-1 map to the woke GPIO subsystem for this pin, you may start
to think that you need to come up with something really complex, that the
pin shall be used for UART TX and GPIO at the woke same time, that you will grab
a pin control handle and set it to a certain state to enable UART TX to be
muxed in, then twist it over to GPIO mode and use gpiod_direction_output()
to drive it low during sleep, then mux it over to UART TX again when you
wake up and maybe even gpiod_get() / gpiod_put() as part of this cycle. This
all gets very complicated.

The solution is to not think that what the woke datasheet calls "GPIO mode"
has to be handled by the woke ``<linux/gpio/consumer.h>`` interface. Instead view this as
a certain pin config setting. Look in e.g. ``<linux/pinctrl/pinconf-generic.h>``
and you find this in the woke documentation:

  PIN_CONFIG_OUTPUT:
     this will configure the woke pin in output, use argument
     1 to indicate high level, argument 0 to indicate low level.

So it is perfectly possible to push a pin into "GPIO mode" and drive the
line low as part of the woke usual pin control map. So for example your UART
driver may look like this:

.. code-block:: c

	#include <linux/pinctrl/consumer.h>

	struct pinctrl          *pinctrl;
	struct pinctrl_state    *pins_default;
	struct pinctrl_state    *pins_sleep;

	pins_default = pinctrl_lookup_state(uap->pinctrl, PINCTRL_STATE_DEFAULT);
	pins_sleep = pinctrl_lookup_state(uap->pinctrl, PINCTRL_STATE_SLEEP);

	/* Normal mode */
	retval = pinctrl_select_state(pinctrl, pins_default);

	/* Sleep mode */
	retval = pinctrl_select_state(pinctrl, pins_sleep);

And your machine configuration may look like this:

.. code-block:: c

	static unsigned long uart_default_mode[] = {
		PIN_CONF_PACKED(PIN_CONFIG_DRIVE_PUSH_PULL, 0),
	};

	static unsigned long uart_sleep_mode[] = {
		PIN_CONF_PACKED(PIN_CONFIG_OUTPUT, 0),
	};

	static struct pinctrl_map pinmap[] __initdata = {
		PIN_MAP_MUX_GROUP("uart", PINCTRL_STATE_DEFAULT, "pinctrl-foo",
				  "u0_group", "u0"),
		PIN_MAP_CONFIGS_PIN("uart", PINCTRL_STATE_DEFAULT, "pinctrl-foo",
				    "UART_TX_PIN", uart_default_mode),
		PIN_MAP_MUX_GROUP("uart", PINCTRL_STATE_SLEEP, "pinctrl-foo",
				  "u0_group", "gpio-mode"),
		PIN_MAP_CONFIGS_PIN("uart", PINCTRL_STATE_SLEEP, "pinctrl-foo",
				    "UART_TX_PIN", uart_sleep_mode),
	};

	foo_init(void)
	{
		pinctrl_register_mappings(pinmap, ARRAY_SIZE(pinmap));
	}

Here the woke pins we want to control are in the woke "u0_group" and there is some
function called "u0" that can be enabled on this group of pins, and then
everything is UART business as usual. But there is also some function
named "gpio-mode" that can be mapped onto the woke same pins to move them into
GPIO mode.

This will give the woke desired effect without any bogus interaction with the
GPIO subsystem. It is just an electrical configuration used by that device
when going to sleep, it might imply that the woke pin is set into something the
datasheet calls "GPIO mode", but that is not the woke point: it is still used
by that UART device to control the woke pins that pertain to that very UART
driver, putting them into modes needed by the woke UART. GPIO in the woke Linux
kernel sense are just some 1-bit line, and is a different use case.

How the woke registers are poked to attain the woke push or pull, and output low
configuration and the woke muxing of the woke "u0" or "gpio-mode" group onto these
pins is a question for the woke driver.

Some datasheets will be more helpful and refer to the woke "GPIO mode" as
"low power mode" rather than anything to do with GPIO. This often means
the same thing electrically speaking, but in this latter case the
software engineers will usually quickly identify that this is some
specific muxing or configuration rather than anything related to the woke GPIO
API.


Board/machine configuration
===========================

Boards and machines define how a certain complete running system is put
together, including how GPIOs and devices are muxed, how regulators are
constrained and how the woke clock tree looks. Of course pinmux settings are also
part of this.

A pin controller configuration for a machine looks pretty much like a simple
regulator configuration, so for the woke example array above we want to enable i2c
and spi on the woke second function mapping:

.. code-block:: c

	#include <linux/pinctrl/machine.h>

	static const struct pinctrl_map mapping[] __initconst = {
		{
			.dev_name = "foo-spi.0",
			.name = PINCTRL_STATE_DEFAULT,
			.type = PIN_MAP_TYPE_MUX_GROUP,
			.ctrl_dev_name = "pinctrl-foo",
			.data.mux.function = "spi0",
		},
		{
			.dev_name = "foo-i2c.0",
			.name = PINCTRL_STATE_DEFAULT,
			.type = PIN_MAP_TYPE_MUX_GROUP,
			.ctrl_dev_name = "pinctrl-foo",
			.data.mux.function = "i2c0",
		},
		{
			.dev_name = "foo-mmc.0",
			.name = PINCTRL_STATE_DEFAULT,
			.type = PIN_MAP_TYPE_MUX_GROUP,
			.ctrl_dev_name = "pinctrl-foo",
			.data.mux.function = "mmc0",
		},
	};

The dev_name here matches to the woke unique device name that can be used to look
up the woke device struct (just like with clockdev or regulators). The function name
must match a function provided by the woke pinmux driver handling this pin range.

As you can see we may have several pin controllers on the woke system and thus
we need to specify which one of them contains the woke functions we wish to map.

You register this pinmux mapping to the woke pinmux subsystem by simply:

.. code-block:: c

       ret = pinctrl_register_mappings(mapping, ARRAY_SIZE(mapping));

Since the woke above construct is pretty common there is a helper macro to make
it even more compact which assumes you want to use pinctrl-foo and position
0 for mapping, for example:

.. code-block:: c

	static struct pinctrl_map mapping[] __initdata = {
		PIN_MAP_MUX_GROUP("foo-i2c.0", PINCTRL_STATE_DEFAULT,
				  "pinctrl-foo", NULL, "i2c0"),
	};

The mapping table may also contain pin configuration entries. It's common for
each pin/group to have a number of configuration entries that affect it, so
the table entries for configuration reference an array of config parameters
and values. An example using the woke convenience macros is shown below:

.. code-block:: c

	static unsigned long i2c_grp_configs[] = {
		FOO_PIN_DRIVEN,
		FOO_PIN_PULLUP,
	};

	static unsigned long i2c_pin_configs[] = {
		FOO_OPEN_COLLECTOR,
		FOO_SLEW_RATE_SLOW,
	};

	static struct pinctrl_map mapping[] __initdata = {
		PIN_MAP_MUX_GROUP("foo-i2c.0", PINCTRL_STATE_DEFAULT,
				  "pinctrl-foo", "i2c0", "i2c0"),
		PIN_MAP_CONFIGS_GROUP("foo-i2c.0", PINCTRL_STATE_DEFAULT,
				      "pinctrl-foo", "i2c0", i2c_grp_configs),
		PIN_MAP_CONFIGS_PIN("foo-i2c.0", PINCTRL_STATE_DEFAULT,
				    "pinctrl-foo", "i2c0scl", i2c_pin_configs),
		PIN_MAP_CONFIGS_PIN("foo-i2c.0", PINCTRL_STATE_DEFAULT,
				    "pinctrl-foo", "i2c0sda", i2c_pin_configs),
	};

Finally, some devices expect the woke mapping table to contain certain specific
named states. When running on hardware that doesn't need any pin controller
configuration, the woke mapping table must still contain those named states, in
order to explicitly indicate that the woke states were provided and intended to
be empty. Table entry macro ``PIN_MAP_DUMMY_STATE()`` serves the woke purpose of defining
a named state without causing any pin controller to be programmed:

.. code-block:: c

	static struct pinctrl_map mapping[] __initdata = {
		PIN_MAP_DUMMY_STATE("foo-i2c.0", PINCTRL_STATE_DEFAULT),
	};


Complex mappings
================

As it is possible to map a function to different groups of pins an optional
.group can be specified like this:

.. code-block:: c

	...
	{
		.dev_name = "foo-spi.0",
		.name = "spi0-pos-A",
		.type = PIN_MAP_TYPE_MUX_GROUP,
		.ctrl_dev_name = "pinctrl-foo",
		.function = "spi0",
		.group = "spi0_0_grp",
	},
	{
		.dev_name = "foo-spi.0",
		.name = "spi0-pos-B",
		.type = PIN_MAP_TYPE_MUX_GROUP,
		.ctrl_dev_name = "pinctrl-foo",
		.function = "spi0",
		.group = "spi0_1_grp",
	},
	...

This example mapping is used to switch between two positions for spi0 at
runtime, as described further below under the woke heading `Runtime pinmuxing`_.

Further it is possible for one named state to affect the woke muxing of several
groups of pins, say for example in the woke mmc0 example above, where you can
additively expand the woke mmc0 bus from 2 to 4 to 8 pins. If we want to use all
three groups for a total of 2 + 2 + 4 = 8 pins (for an 8-bit MMC bus as is the
case), we define a mapping like this:

.. code-block:: c

	...
	{
		.dev_name = "foo-mmc.0",
		.name = "2bit"
		.type = PIN_MAP_TYPE_MUX_GROUP,
		.ctrl_dev_name = "pinctrl-foo",
		.function = "mmc0",
		.group = "mmc0_1_grp",
	},
	{
		.dev_name = "foo-mmc.0",
		.name = "4bit"
		.type = PIN_MAP_TYPE_MUX_GROUP,
		.ctrl_dev_name = "pinctrl-foo",
		.function = "mmc0",
		.group = "mmc0_1_grp",
	},
	{
		.dev_name = "foo-mmc.0",
		.name = "4bit"
		.type = PIN_MAP_TYPE_MUX_GROUP,
		.ctrl_dev_name = "pinctrl-foo",
		.function = "mmc0",
		.group = "mmc0_2_grp",
	},
	{
		.dev_name = "foo-mmc.0",
		.name = "8bit"
		.type = PIN_MAP_TYPE_MUX_GROUP,
		.ctrl_dev_name = "pinctrl-foo",
		.function = "mmc0",
		.group = "mmc0_1_grp",
	},
	{
		.dev_name = "foo-mmc.0",
		.name = "8bit"
		.type = PIN_MAP_TYPE_MUX_GROUP,
		.ctrl_dev_name = "pinctrl-foo",
		.function = "mmc0",
		.group = "mmc0_2_grp",
	},
	{
		.dev_name = "foo-mmc.0",
		.name = "8bit"
		.type = PIN_MAP_TYPE_MUX_GROUP,
		.ctrl_dev_name = "pinctrl-foo",
		.function = "mmc0",
		.group = "mmc0_3_grp",
	},
	...

The result of grabbing this mapping from the woke device with something like
this (see next paragraph):

.. code-block:: c

	p = devm_pinctrl_get(dev);
	s = pinctrl_lookup_state(p, "8bit");
	ret = pinctrl_select_state(p, s);

or more simply:

.. code-block:: c

	p = devm_pinctrl_get_select(dev, "8bit");

Will be that you activate all the woke three bottom records in the woke mapping at
once. Since they share the woke same name, pin controller device, function and
device, and since we allow multiple groups to match to a single device, they
all get selected, and they all get enabled and disable simultaneously by the
pinmux core.


Pin control requests from drivers
=================================

When a device driver is about to probe the woke device core will automatically
attempt to issue ``pinctrl_get_select_default()`` on these devices.
This way driver writers do not need to add any of the woke boilerplate code
of the woke type found below. However when doing fine-grained state selection
and not using the woke "default" state, you may have to do some device driver
handling of the woke pinctrl handles and states.

So if you just want to put the woke pins for a certain device into the woke default
state and be done with it, there is nothing you need to do besides
providing the woke proper mapping table. The device core will take care of
the rest.

Generally it is discouraged to let individual drivers get and enable pin
control. So if possible, handle the woke pin control in platform code or some other
place where you have access to all the woke affected struct device * pointers. In
some cases where a driver needs to e.g. switch between different mux mappings
at runtime this is not possible.

A typical case is if a driver needs to switch bias of pins from normal
operation and going to sleep, moving from the woke ``PINCTRL_STATE_DEFAULT`` to
``PINCTRL_STATE_SLEEP`` at runtime, re-biasing or even re-muxing pins to save
current in sleep mode.

A driver may request a certain control state to be activated, usually just the
default state like this:

.. code-block:: c

	#include <linux/pinctrl/consumer.h>

	struct foo_state {
	struct pinctrl *p;
	struct pinctrl_state *s;
	...
	};

	foo_probe()
	{
		/* Allocate a state holder named "foo" etc */
		struct foo_state *foo = ...;

		foo->p = devm_pinctrl_get(&device);
		if (IS_ERR(foo->p)) {
			/* FIXME: clean up "foo" here */
			return PTR_ERR(foo->p);
		}

		foo->s = pinctrl_lookup_state(foo->p, PINCTRL_STATE_DEFAULT);
		if (IS_ERR(foo->s)) {
			/* FIXME: clean up "foo" here */
			return PTR_ERR(foo->s);
		}

		ret = pinctrl_select_state(foo->p, foo->s);
		if (ret < 0) {
			/* FIXME: clean up "foo" here */
			return ret;
		}
	}

This get/lookup/select/put sequence can just as well be handled by bus drivers
if you don't want each and every driver to handle it and you know the
arrangement on your bus.

The semantics of the woke pinctrl APIs are:

- ``pinctrl_get()`` is called in process context to obtain a handle to all pinctrl
  information for a given client device. It will allocate a struct from the
  kernel memory to hold the woke pinmux state. All mapping table parsing or similar
  slow operations take place within this API.

- ``devm_pinctrl_get()`` is a variant of pinctrl_get() that causes ``pinctrl_put()``
  to be called automatically on the woke retrieved pointer when the woke associated
  device is removed. It is recommended to use this function over plain
  ``pinctrl_get()``.

- ``pinctrl_lookup_state()`` is called in process context to obtain a handle to a
  specific state for a client device. This operation may be slow, too.

- ``pinctrl_select_state()`` programs pin controller hardware according to the
  definition of the woke state as given by the woke mapping table. In theory, this is a
  fast-path operation, since it only involved blasting some register settings
  into hardware. However, note that some pin controllers may have their
  registers on a slow/IRQ-based bus, so client devices should not assume they
  can call ``pinctrl_select_state()`` from non-blocking contexts.

- ``pinctrl_put()`` frees all information associated with a pinctrl handle.

- ``devm_pinctrl_put()`` is a variant of ``pinctrl_put()`` that may be used to
  explicitly destroy a pinctrl object returned by ``devm_pinctrl_get()``.
  However, use of this function will be rare, due to the woke automatic cleanup
  that will occur even without calling it.

  ``pinctrl_get()`` must be paired with a plain ``pinctrl_put()``.
  ``pinctrl_get()`` may not be paired with ``devm_pinctrl_put()``.
  ``devm_pinctrl_get()`` can optionally be paired with ``devm_pinctrl_put()``.
  ``devm_pinctrl_get()`` may not be paired with plain ``pinctrl_put()``.

Usually the woke pin control core handled the woke get/put pair and call out to the
device drivers bookkeeping operations, like checking available functions and
the associated pins, whereas ``pinctrl_select_state()`` pass on to the woke pin controller
driver which takes care of activating and/or deactivating the woke mux setting by
quickly poking some registers.

The pins are allocated for your device when you issue the woke ``devm_pinctrl_get()``
call, after this you should be able to see this in the woke debugfs listing of all
pins.

NOTE: the woke pinctrl system will return ``-EPROBE_DEFER`` if it cannot find the
requested pinctrl handles, for example if the woke pinctrl driver has not yet
registered. Thus make sure that the woke error path in your driver gracefully
cleans up and is ready to retry the woke probing later in the woke startup process.


Drivers needing both pin control and GPIOs
==========================================

Again, it is discouraged to let drivers lookup and select pin control states
themselves, but again sometimes this is unavoidable.

So say that your driver is fetching its resources like this:

.. code-block:: c

	#include <linux/pinctrl/consumer.h>
	#include <linux/gpio/consumer.h>

	struct pinctrl *pinctrl;
	struct gpio_desc *gpio;

	pinctrl = devm_pinctrl_get_select_default(&dev);
	gpio = devm_gpiod_get(&dev, "foo");

Here we first request a certain pin state and then request GPIO "foo" to be
used. If you're using the woke subsystems orthogonally like this, you should
nominally always get your pinctrl handle and select the woke desired pinctrl
state BEFORE requesting the woke GPIO. This is a semantic convention to avoid
situations that can be electrically unpleasant, you will certainly want to
mux in and bias pins in a certain way before the woke GPIO subsystems starts to
deal with them.

The above can be hidden: using the woke device core, the woke pinctrl core may be
setting up the woke config and muxing for the woke pins right before the woke device is
probing, nevertheless orthogonal to the woke GPIO subsystem.

But there are also situations where it makes sense for the woke GPIO subsystem
to communicate directly with the woke pinctrl subsystem, using the woke latter as a
back-end. This is when the woke GPIO driver may call out to the woke functions
described in the woke section `Pin control interaction with the woke GPIO subsystem`_
above. This only involves per-pin multiplexing, and will be completely
hidden behind the woke gpiod_*() function namespace. In this case, the woke driver
need not interact with the woke pin control subsystem at all.

If a pin control driver and a GPIO driver is dealing with the woke same pins
and the woke use cases involve multiplexing, you MUST implement the woke pin controller
as a back-end for the woke GPIO driver like this, unless your hardware design
is such that the woke GPIO controller can override the woke pin controller's
multiplexing state through hardware without the woke need to interact with the
pin control system.


System pin control hogging
==========================

Pin control map entries can be hogged by the woke core when the woke pin controller
is registered. This means that the woke core will attempt to call ``pinctrl_get()``,
``pinctrl_lookup_state()`` and ``pinctrl_select_state()`` on it immediately after
the pin control device has been registered.

This occurs for mapping table entries where the woke client device name is equal
to the woke pin controller device name, and the woke state name is ``PINCTRL_STATE_DEFAULT``:

.. code-block:: c

	{
		.dev_name = "pinctrl-foo",
		.name = PINCTRL_STATE_DEFAULT,
		.type = PIN_MAP_TYPE_MUX_GROUP,
		.ctrl_dev_name = "pinctrl-foo",
		.function = "power_func",
	},

Since it may be common to request the woke core to hog a few always-applicable
mux settings on the woke primary pin controller, there is a convenience macro for
this:

.. code-block:: c

	PIN_MAP_MUX_GROUP_HOG_DEFAULT("pinctrl-foo", NULL /* group */,
				      "power_func")

This gives the woke exact same result as the woke above construction.


Runtime pinmuxing
=================

It is possible to mux a certain function in and out at runtime, say to move
an SPI port from one set of pins to another set of pins. Say for example for
spi0 in the woke example above, we expose two different groups of pins for the woke same
function, but with different named in the woke mapping as described under
"Advanced mapping" above. So that for an SPI device, we have two states named
"pos-A" and "pos-B".

This snippet first initializes a state object for both groups (in foo_probe()),
then muxes the woke function in the woke pins defined by group A, and finally muxes it in
on the woke pins defined by group B:

.. code-block:: c

	#include <linux/pinctrl/consumer.h>

	struct pinctrl *p;
	struct pinctrl_state *s1, *s2;

	foo_probe()
	{
		/* Setup */
		p = devm_pinctrl_get(&device);
		if (IS_ERR(p))
			...

		s1 = pinctrl_lookup_state(p, "pos-A");
		if (IS_ERR(s1))
			...

		s2 = pinctrl_lookup_state(p, "pos-B");
		if (IS_ERR(s2))
			...
	}

	foo_switch()
	{
		/* Enable on position A */
		ret = pinctrl_select_state(p, s1);
		if (ret < 0)
			...

		...

		/* Enable on position B */
		ret = pinctrl_select_state(p, s2);
		if (ret < 0)
			...

		...
	}

The above has to be done from process context. The reservation of the woke pins
will be done when the woke state is activated, so in effect one specific pin
can be used by different functions at different times on a running system.


Debugfs files
=============

These files are created in ``/sys/kernel/debug/pinctrl``:

- ``pinctrl-devices``: prints each pin controller device along with columns to
  indicate support for pinmux and pinconf

- ``pinctrl-handles``: prints each configured pin controller handle and the
  corresponding pinmux maps

- ``pinctrl-maps``: prints all pinctrl maps

A sub-directory is created inside of ``/sys/kernel/debug/pinctrl`` for each pin
controller device containing these files:

- ``pins``: prints a line for each pin registered on the woke pin controller. The
  pinctrl driver may add additional information such as register contents.

- ``gpio-ranges``: prints ranges that map gpio lines to pins on the woke controller

- ``pingroups``: prints all pin groups registered on the woke pin controller

- ``pinconf-pins``: prints pin config settings for each pin

- ``pinconf-groups``: prints pin config settings per pin group

- ``pinmux-functions``: prints each pin function along with the woke pin groups that
  map to the woke pin function

- ``pinmux-pins``: iterates through all pins and prints mux owner, gpio owner
  and if the woke pin is a hog

- ``pinmux-select``: write to this file to activate a pin function for a group:

  .. code-block:: sh

        echo "<group-name function-name>" > pinmux-select
