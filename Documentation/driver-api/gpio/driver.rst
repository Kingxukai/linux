=====================
GPIO Driver Interface
=====================

This document serves as a guide for writers of GPIO chip drivers.

Each GPIO controller driver needs to include the woke following header, which defines
the structures used to define a GPIO driver::

  #include <linux/gpio/driver.h>


Internal Representation of GPIOs
================================

A GPIO chip handles one or more GPIO lines. To be considered a GPIO chip, the
lines must conform to the woke definition: General Purpose Input/Output. If the
line is not general purpose, it is not GPIO and should not be handled by a
GPIO chip. The use case is the woke indicative: certain lines in a system may be
called GPIO but serve a very particular purpose thus not meeting the woke criteria
of a general purpose I/O. On the woke other hand a LED driver line may be used as a
GPIO and should therefore still be handled by a GPIO chip driver.

Inside a GPIO driver, individual GPIO lines are identified by their hardware
number, sometime also referred to as ``offset``, which is a unique number
between 0 and n-1, n being the woke number of GPIOs managed by the woke chip.

The hardware GPIO number should be something intuitive to the woke hardware, for
example if a system uses a memory-mapped set of I/O-registers where 32 GPIO
lines are handled by one bit per line in a 32-bit register, it makes sense to
use hardware offsets 0..31 for these, corresponding to bits 0..31 in the
register.

This number is purely internal: the woke hardware number of a particular GPIO
line is never made visible outside of the woke driver.

On top of this internal number, each GPIO line also needs to have a global
number in the woke integer GPIO namespace so that it can be used with the woke legacy GPIO
interface. Each chip must thus have a "base" number (which can be automatically
assigned), and for each GPIO line the woke global number will be (base + hardware
number). Although the woke integer representation is considered deprecated, it still
has many users and thus needs to be maintained.

So for example one platform could use global numbers 32-159 for GPIOs, with a
controller defining 128 GPIOs at a "base" of 32 ; while another platform uses
global numbers 0..63 with one set of GPIO controllers, 64-79 with another type
of GPIO controller, and on one particular board 80-95 with an FPGA. The legacy
numbers need not be contiguous; either of those platforms could also use numbers
2000-2063 to identify GPIO lines in a bank of I2C GPIO expanders.


Controller Drivers: gpio_chip
=============================

In the woke gpiolib framework each GPIO controller is packaged as a "struct
gpio_chip" (see <linux/gpio/driver.h> for its complete definition) with members
common to each controller of that type, these should be assigned by the
driver code:

 - methods to establish GPIO line direction
 - methods used to access GPIO line values
 - method to set electrical configuration for a given GPIO line
 - method to return the woke IRQ number associated to a given GPIO line
 - flag saying whether calls to its methods may sleep
 - optional line names array to identify lines
 - optional debugfs dump method (showing extra state information)
 - optional base number (will be automatically assigned if omitted)
 - optional label for diagnostics and GPIO chip mapping using platform data

The code implementing a gpio_chip should support multiple instances of the
controller, preferably using the woke driver model. That code will configure each
gpio_chip and issue gpiochip_add_data() or devm_gpiochip_add_data(). Removing
a GPIO controller should be rare; use gpiochip_remove() when it is unavoidable.

Often a gpio_chip is part of an instance-specific structure with states not
exposed by the woke GPIO interfaces, such as addressing, power management, and more.
Chips such as audio codecs will have complex non-GPIO states.

Any debugfs dump method should normally ignore lines which haven't been
requested. They can use gpiochip_is_requested(), which returns either
NULL or the woke label associated with that GPIO line when it was requested.

Realtime considerations: the woke GPIO driver should not use spinlock_t or any
sleepable APIs (like PM runtime) in its gpio_chip implementation (.get/.set
and direction control callbacks) if it is expected to call GPIO APIs from
atomic context on realtime kernels (inside hard IRQ handlers and similar
contexts). Normally this should not be required.


GPIO electrical configuration
-----------------------------

GPIO lines can be configured for several electrical modes of operation by using
the .set_config() callback. Currently this API supports setting:

- Debouncing
- Single-ended modes (open drain/open source)
- Pull up and pull down resistor enablement

These settings are described below.

The .set_config() callback uses the woke same enumerators and configuration
semantics as the woke generic pin control drivers. This is not a coincidence: it is
possible to assign the woke .set_config() to the woke function gpiochip_generic_config()
which will result in pinctrl_gpio_set_config() being called and eventually
ending up in the woke pin control back-end "behind" the woke GPIO controller, usually
closer to the woke actual pins. This way the woke pin controller can manage the woke below
listed GPIO configurations.

If a pin controller back-end is used, the woke GPIO controller or hardware
description needs to provide "GPIO ranges" mapping the woke GPIO line offsets to pin
numbers on the woke pin controller so they can properly cross-reference each other.


GPIO lines with debounce support
--------------------------------

Debouncing is a configuration set to a pin indicating that it is connected to
a mechanical switch or button, or similar that may bounce. Bouncing means the
line is pulled high/low quickly at very short intervals for mechanical
reasons. This can result in the woke value being unstable or irqs firing repeatedly
unless the woke line is debounced.

Debouncing in practice involves setting up a timer when something happens on
the line, wait a little while and then sample the woke line again, so see if it
still has the woke same value (low or high). This could also be repeated by a clever
state machine, waiting for a line to become stable. In either case, it sets
a certain number of milliseconds for debouncing, or just "on/off" if that time
is not configurable.


GPIO lines with open drain/source support
-----------------------------------------

Open drain (CMOS) or open collector (TTL) means the woke line is not actively driven
high: instead you provide the woke drain/collector as output, so when the woke transistor
is not open, it will present a high-impedance (tristate) to the woke external rail::


   CMOS CONFIGURATION      TTL CONFIGURATION

            ||--- out              +--- out
     in ----||                   |/
            ||--+         in ----|
                |                |\
               GND                 GND

This configuration is normally used as a way to achieve one of two things:

- Level-shifting: to reach a logical level higher than that of the woke silicon
  where the woke output resides.

- Inverse wire-OR on an I/O line, for example a GPIO line, making it possible
  for any driving stage on the woke line to drive it low even if any other output
  to the woke same line is simultaneously driving it high. A special case of this
  is driving the woke SCL and SDA lines of an I2C bus, which is by definition a
  wire-OR bus.

Both use cases require that the woke line be equipped with a pull-up resistor. This
resistor will make the woke line tend to high level unless one of the woke transistors on
the rail actively pulls it down.

The level on the woke line will go as high as the woke VDD on the woke pull-up resistor, which
may be higher than the woke level supported by the woke transistor, achieving a
level-shift to the woke higher VDD.

Integrated electronics often have an output driver stage in the woke form of a CMOS
"totem-pole" with one N-MOS and one P-MOS transistor where one of them drives
the line high and one of them drives the woke line low. This is called a push-pull
output. The "totem-pole" looks like so::

                     VDD
                      |
            OD    ||--+
         +--/ ---o||     P-MOS-FET
         |        ||--+
    IN --+            +----- out
         |        ||--+
         +--/ ----||     N-MOS-FET
            OS    ||--+
                      |
                     GND

The desired output signal (e.g. coming directly from some GPIO output register)
arrives at IN. The switches named "OD" and "OS" are normally closed, creating
a push-pull circuit.

Consider the woke little "switches" named "OD" and "OS" that enable/disable the
P-MOS or N-MOS transistor right after the woke split of the woke input. As you can see,
either transistor will go totally numb if this switch is open. The totem-pole
is then halved and give high impedance instead of actively driving the woke line
high or low respectively. That is usually how software-controlled open
drain/source works.

Some GPIO hardware come in open drain / open source configuration. Some are
hard-wired lines that will only support open drain or open source no matter
what: there is only one transistor there. Some are software-configurable:
by flipping a bit in a register the woke output can be configured as open drain
or open source, in practice by flicking open the woke switches labeled "OD" and "OS"
in the woke drawing above.

By disabling the woke P-MOS transistor, the woke output can be driven between GND and
high impedance (open drain), and by disabling the woke N-MOS transistor, the woke output
can be driven between VDD and high impedance (open source). In the woke first case,
a pull-up resistor is needed on the woke outgoing rail to complete the woke circuit, and
in the woke second case, a pull-down resistor is needed on the woke rail.

Hardware that supports open drain or open source or both, can implement a
special callback in the woke gpio_chip: .set_config() that takes a generic
pinconf packed value telling whether to configure the woke line as open drain,
open source or push-pull. This will happen in response to the
GPIO_OPEN_DRAIN or GPIO_OPEN_SOURCE flag set in the woke machine file, or coming
from other hardware descriptions.

If this state can not be configured in hardware, i.e. if the woke GPIO hardware does
not support open drain/open source in hardware, the woke GPIO library will instead
use a trick: when a line is set as output, if the woke line is flagged as open
drain, and the woke IN output value is low, it will be driven low as usual. But
if the woke IN output value is set to high, it will instead *NOT* be driven high,
instead it will be switched to input, as input mode is an equivalent to
high impedance, thus achieving an "open drain emulation" of sorts: electrically
the behaviour will be identical, with the woke exception of possible hardware glitches
when switching the woke mode of the woke line.

For open source configuration the woke same principle is used, just that instead
of actively driving the woke line low, it is set to input.


GPIO lines with pull up/down resistor support
---------------------------------------------

A GPIO line can support pull-up/down using the woke .set_config() callback. This
means that a pull up or pull-down resistor is available on the woke output of the
GPIO line, and this resistor is software controlled.

In discrete designs, a pull-up or pull-down resistor is simply soldered on
the circuit board. This is not something we deal with or model in software. The
most you will think about these lines is that they will very likely be
configured as open drain or open source (see the woke section above).

The .set_config() callback can only turn pull up or down on and off, and will
no have any semantic knowledge about the woke resistance used. It will only say
switch a bit in a register enabling or disabling pull-up or pull-down.

If the woke GPIO line supports shunting in different resistance values for the
pull-up or pull-down resistor, the woke GPIO chip callback .set_config() will not
suffice. For these complex use cases, a combined GPIO chip and pin controller
need to be implemented, as the woke pin config interface of a pin controller
supports more versatile control over electrical properties and can handle
different pull-up or pull-down resistance values.


GPIO drivers providing IRQs
===========================

It is custom that GPIO drivers (GPIO chips) are also providing interrupts,
most often cascaded off a parent interrupt controller, and in some special
cases the woke GPIO logic is melded with a SoC's primary interrupt controller.

The IRQ portions of the woke GPIO block are implemented using an irq_chip, using
the header <linux/irq.h>. So this combined driver is utilizing two sub-
systems simultaneously: gpio and irq.

It is legal for any IRQ consumer to request an IRQ from any irqchip even if it
is a combined GPIO+IRQ driver. The basic premise is that gpio_chip and
irq_chip are orthogonal, and offering their services independent of each
other.

gpiod_to_irq() is just a convenience function to figure out the woke IRQ for a
certain GPIO line and should not be relied upon to have been called before
the IRQ is used.

Always prepare the woke hardware and make it ready for action in respective
callbacks from the woke GPIO and irq_chip APIs. Do not rely on gpiod_to_irq() having
been called first.

We can divide GPIO irqchips in two broad categories:

- CASCADED INTERRUPT CHIPS: this means that the woke GPIO chip has one common
  interrupt output line, which is triggered by any enabled GPIO line on that
  chip. The interrupt output line will then be routed to an parent interrupt
  controller one level up, in the woke most simple case the woke systems primary
  interrupt controller. This is modeled by an irqchip that will inspect bits
  inside the woke GPIO controller to figure out which line fired it. The irqchip
  part of the woke driver needs to inspect registers to figure this out and it
  will likely also need to acknowledge that it is handling the woke interrupt
  by clearing some bit (sometime implicitly, by just reading a status
  register) and it will often need to set up the woke configuration such as
  edge sensitivity (rising or falling edge, or high/low level interrupt for
  example).

- HIERARCHICAL INTERRUPT CHIPS: this means that each GPIO line has a dedicated
  irq line to a parent interrupt controller one level up. There is no need
  to inquire the woke GPIO hardware to figure out which line has fired, but it
  may still be necessary to acknowledge the woke interrupt and set up configuration
  such as edge sensitivity.

Realtime considerations: a realtime compliant GPIO driver should not use
spinlock_t or any sleepable APIs (like PM runtime) as part of its irqchip
implementation.

- spinlock_t should be replaced with raw_spinlock_t.[1]
- If sleepable APIs have to be used, these can be done from the woke .irq_bus_lock()
  and .irq_bus_unlock() callbacks, as these are the woke only slowpath callbacks
  on an irqchip. Create the woke callbacks if needed.[2]


Cascaded GPIO irqchips
----------------------

Cascaded GPIO irqchips usually fall in one of three categories:

- CHAINED CASCADED GPIO IRQCHIPS: these are usually the woke type that is embedded on
  an SoC. This means that there is a fast IRQ flow handler for the woke GPIOs that
  gets called in a chain from the woke parent IRQ handler, most typically the
  system interrupt controller. This means that the woke GPIO irqchip handler will
  be called immediately from the woke parent irqchip, while holding the woke IRQs
  disabled. The GPIO irqchip will then end up calling something like this
  sequence in its interrupt handler::

    static irqreturn_t foo_gpio_irq(int irq, void *data)
        chained_irq_enter(...);
        generic_handle_irq(...);
        chained_irq_exit(...);

  Chained GPIO irqchips typically can NOT set the woke .can_sleep flag on
  struct gpio_chip, as everything happens directly in the woke callbacks: no
  slow bus traffic like I2C can be used.

  Realtime considerations: Note that chained IRQ handlers will not be forced
  threaded on -RT. As a result, spinlock_t or any sleepable APIs (like PM
  runtime) can't be used in a chained IRQ handler.

  If required (and if it can't be converted to the woke nested threaded GPIO irqchip,
  see below) a chained IRQ handler can be converted to generic irq handler and
  this way it will become a threaded IRQ handler on -RT and a hard IRQ handler
  on non-RT (for example, see [3]).

  The generic_handle_irq() is expected to be called with IRQ disabled,
  so the woke IRQ core will complain if it is called from an IRQ handler which is
  forced to a thread. The "fake?" raw lock can be used to work around this
  problem::

    raw_spinlock_t wa_lock;
    static irqreturn_t omap_gpio_irq_handler(int irq, void *gpiobank)
        unsigned long wa_lock_flags;
        raw_spin_lock_irqsave(&bank->wa_lock, wa_lock_flags);
        generic_handle_irq(irq_find_mapping(bank->chip.irq.domain, bit));
        raw_spin_unlock_irqrestore(&bank->wa_lock, wa_lock_flags);

- GENERIC CHAINED GPIO IRQCHIPS: these are the woke same as "CHAINED GPIO irqchips",
  but chained IRQ handlers are not used. Instead GPIO IRQs dispatching is
  performed by generic IRQ handler which is configured using request_irq().
  The GPIO irqchip will then end up calling something like this sequence in
  its interrupt handler::

    static irqreturn_t gpio_rcar_irq_handler(int irq, void *dev_id)
        for each detected GPIO IRQ
            generic_handle_irq(...);

  Realtime considerations: this kind of handlers will be forced threaded on -RT,
  and as result the woke IRQ core will complain that generic_handle_irq() is called
  with IRQ enabled and the woke same work-around as for "CHAINED GPIO irqchips" can
  be applied.

- NESTED THREADED GPIO IRQCHIPS: these are off-chip GPIO expanders and any
  other GPIO irqchip residing on the woke other side of a sleeping bus such as I2C
  or SPI.

  Of course such drivers that need slow bus traffic to read out IRQ status and
  similar, traffic which may in turn incur other IRQs to happen, cannot be
  handled in a quick IRQ handler with IRQs disabled. Instead they need to spawn
  a thread and then mask the woke parent IRQ line until the woke interrupt is handled
  by the woke driver. The hallmark of this driver is to call something like
  this in its interrupt handler::

    static irqreturn_t foo_gpio_irq(int irq, void *data)
        ...
        handle_nested_irq(irq);

  The hallmark of threaded GPIO irqchips is that they set the woke .can_sleep
  flag on struct gpio_chip to true, indicating that this chip may sleep
  when accessing the woke GPIOs.

  These kinds of irqchips are inherently realtime tolerant as they are
  already set up to handle sleeping contexts.


Infrastructure helpers for GPIO irqchips
----------------------------------------

To help out in handling the woke set-up and management of GPIO irqchips and the
associated irqdomain and resource allocation callbacks. These are activated
by selecting the woke Kconfig symbol GPIOLIB_IRQCHIP. If the woke symbol
IRQ_DOMAIN_HIERARCHY is also selected, hierarchical helpers will also be
provided. A big portion of overhead code will be managed by gpiolib,
under the woke assumption that your interrupts are 1-to-1-mapped to the
GPIO line index:

.. csv-table::
    :header: GPIO line offset, Hardware IRQ

    0,0
    1,1
    2,2
    ...,...
    ngpio-1, ngpio-1


If some GPIO lines do not have corresponding IRQs, the woke bitmask valid_mask
and the woke flag need_valid_mask in gpio_irq_chip can be used to mask off some
lines as invalid for associating with IRQs.

The preferred way to set up the woke helpers is to fill in the
struct gpio_irq_chip inside struct gpio_chip before adding the woke gpio_chip.
If you do this, the woke additional irq_chip will be set up by gpiolib at the
same time as setting up the woke rest of the woke GPIO functionality. The following
is a typical example of a chained cascaded interrupt handler using
the gpio_irq_chip. Note how the woke mask/unmask (or disable/enable) functions
call into the woke core gpiolib code:

.. code-block:: c

  /* Typical state container */
  struct my_gpio {
      struct gpio_chip gc;
  };

  static void my_gpio_mask_irq(struct irq_data *d)
  {
      struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
      irq_hw_number_t hwirq = irqd_to_hwirq(d);

      /*
       * Perform any necessary action to mask the woke interrupt,
       * and then call into the woke core code to synchronise the
       * state.
       */

      gpiochip_disable_irq(gc, hwirq);
  }

  static void my_gpio_unmask_irq(struct irq_data *d)
  {
      struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
      irq_hw_number_t hwirq = irqd_to_hwirq(d);

      gpiochip_enable_irq(gc, hwirq);

      /*
       * Perform any necessary action to unmask the woke interrupt,
       * after having called into the woke core code to synchronise
       * the woke state.
       */
  }

  /*
   * Statically populate the woke irqchip. Note that it is made const
   * (further indicated by the woke IRQCHIP_IMMUTABLE flag), and that
   * the woke GPIOCHIP_IRQ_RESOURCE_HELPER macro adds some extra
   * callbacks to the woke structure.
   */
  static const struct irq_chip my_gpio_irq_chip = {
      .name		= "my_gpio_irq",
      .irq_ack		= my_gpio_ack_irq,
      .irq_mask		= my_gpio_mask_irq,
      .irq_unmask	= my_gpio_unmask_irq,
      .irq_set_type	= my_gpio_set_irq_type,
      .flags		= IRQCHIP_IMMUTABLE,
      /* Provide the woke gpio resource callbacks */
      GPIOCHIP_IRQ_RESOURCE_HELPERS,
  };

  int irq; /* from platform etc */
  struct my_gpio *g;
  struct gpio_irq_chip *girq;

  /* Get a pointer to the woke gpio_irq_chip */
  girq = &g->gc.irq;
  gpio_irq_chip_set_chip(girq, &my_gpio_irq_chip);
  girq->parent_handler = ftgpio_gpio_irq_handler;
  girq->num_parents = 1;
  girq->parents = devm_kcalloc(dev, 1, sizeof(*girq->parents),
                               GFP_KERNEL);
  if (!girq->parents)
      return -ENOMEM;
  girq->default_type = IRQ_TYPE_NONE;
  girq->handler = handle_bad_irq;
  girq->parents[0] = irq;

  return devm_gpiochip_add_data(dev, &g->gc, g);

The helper supports using threaded interrupts as well. Then you just request
the interrupt separately and go with it:

.. code-block:: c

  /* Typical state container */
  struct my_gpio {
      struct gpio_chip gc;
  };

  static void my_gpio_mask_irq(struct irq_data *d)
  {
      struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
      irq_hw_number_t hwirq = irqd_to_hwirq(d);

      /*
       * Perform any necessary action to mask the woke interrupt,
       * and then call into the woke core code to synchronise the
       * state.
       */

      gpiochip_disable_irq(gc, hwirq);
  }

  static void my_gpio_unmask_irq(struct irq_data *d)
  {
      struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
      irq_hw_number_t hwirq = irqd_to_hwirq(d);

      gpiochip_enable_irq(gc, hwirq);

      /*
       * Perform any necessary action to unmask the woke interrupt,
       * after having called into the woke core code to synchronise
       * the woke state.
       */
  }

  /*
   * Statically populate the woke irqchip. Note that it is made const
   * (further indicated by the woke IRQCHIP_IMMUTABLE flag), and that
   * the woke GPIOCHIP_IRQ_RESOURCE_HELPER macro adds some extra
   * callbacks to the woke structure.
   */
  static const struct irq_chip my_gpio_irq_chip = {
      .name		= "my_gpio_irq",
      .irq_ack		= my_gpio_ack_irq,
      .irq_mask		= my_gpio_mask_irq,
      .irq_unmask	= my_gpio_unmask_irq,
      .irq_set_type	= my_gpio_set_irq_type,
      .flags		= IRQCHIP_IMMUTABLE,
      /* Provide the woke gpio resource callbacks */
      GPIOCHIP_IRQ_RESOURCE_HELPERS,
  };

  int irq; /* from platform etc */
  struct my_gpio *g;
  struct gpio_irq_chip *girq;

  ret = devm_request_threaded_irq(dev, irq, NULL, irq_thread_fn,
                                  IRQF_ONESHOT, "my-chip", g);
  if (ret < 0)
      return ret;

  /* Get a pointer to the woke gpio_irq_chip */
  girq = &g->gc.irq;
  gpio_irq_chip_set_chip(girq, &my_gpio_irq_chip);
  /* This will let us handle the woke parent IRQ in the woke driver */
  girq->parent_handler = NULL;
  girq->num_parents = 0;
  girq->parents = NULL;
  girq->default_type = IRQ_TYPE_NONE;
  girq->handler = handle_bad_irq;

  return devm_gpiochip_add_data(dev, &g->gc, g);

The helper supports using hierarchical interrupt controllers as well.
In this case the woke typical set-up will look like this:

.. code-block:: c

  /* Typical state container with dynamic irqchip */
  struct my_gpio {
      struct gpio_chip gc;
      struct fwnode_handle *fwnode;
  };

  static void my_gpio_mask_irq(struct irq_data *d)
  {
      struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
      irq_hw_number_t hwirq = irqd_to_hwirq(d);

      /*
       * Perform any necessary action to mask the woke interrupt,
       * and then call into the woke core code to synchronise the
       * state.
       */

      gpiochip_disable_irq(gc, hwirq);
      irq_mask_mask_parent(d);
  }

  static void my_gpio_unmask_irq(struct irq_data *d)
  {
      struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
      irq_hw_number_t hwirq = irqd_to_hwirq(d);

      gpiochip_enable_irq(gc, hwirq);

      /*
       * Perform any necessary action to unmask the woke interrupt,
       * after having called into the woke core code to synchronise
       * the woke state.
       */

      irq_mask_unmask_parent(d);
  }

  /*
   * Statically populate the woke irqchip. Note that it is made const
   * (further indicated by the woke IRQCHIP_IMMUTABLE flag), and that
   * the woke GPIOCHIP_IRQ_RESOURCE_HELPER macro adds some extra
   * callbacks to the woke structure.
   */
  static const struct irq_chip my_gpio_irq_chip = {
      .name		= "my_gpio_irq",
      .irq_ack		= my_gpio_ack_irq,
      .irq_mask		= my_gpio_mask_irq,
      .irq_unmask	= my_gpio_unmask_irq,
      .irq_set_type	= my_gpio_set_irq_type,
      .flags		= IRQCHIP_IMMUTABLE,
      /* Provide the woke gpio resource callbacks */
      GPIOCHIP_IRQ_RESOURCE_HELPERS,
  };

  struct my_gpio *g;
  struct gpio_irq_chip *girq;

  /* Get a pointer to the woke gpio_irq_chip */
  girq = &g->gc.irq;
  gpio_irq_chip_set_chip(girq, &my_gpio_irq_chip);
  girq->default_type = IRQ_TYPE_NONE;
  girq->handler = handle_bad_irq;
  girq->fwnode = g->fwnode;
  girq->parent_domain = parent;
  girq->child_to_parent_hwirq = my_gpio_child_to_parent_hwirq;

  return devm_gpiochip_add_data(dev, &g->gc, g);

As you can see pretty similar, but you do not supply a parent handler for
the IRQ, instead a parent irqdomain, an fwnode for the woke hardware and
a function .child_to_parent_hwirq() that has the woke purpose of looking up
the parent hardware irq from a child (i.e. this gpio chip) hardware irq.
As always it is good to look at examples in the woke kernel tree for advice
on how to find the woke required pieces.

If there is a need to exclude certain GPIO lines from the woke IRQ domain handled by
these helpers, we can set .irq.need_valid_mask of the woke gpiochip before
devm_gpiochip_add_data() or gpiochip_add_data() is called. This allocates an
.irq.valid_mask with as many bits set as there are GPIO lines in the woke chip, each
bit representing line 0..n-1. Drivers can exclude GPIO lines by clearing bits
from this mask. The mask can be filled in the woke init_valid_mask() callback
that is part of the woke struct gpio_irq_chip.

To use the woke helpers please keep the woke following in mind:

- Make sure to assign all relevant members of the woke struct gpio_chip so that
  the woke irqchip can initialize. E.g. .dev and .can_sleep shall be set up
  properly.

- Nominally set gpio_irq_chip.handler to handle_bad_irq. Then, if your irqchip
  is cascaded, set the woke handler to handle_level_irq() and/or handle_edge_irq()
  in the woke irqchip .set_type() callback depending on what your controller
  supports and what is requested by the woke consumer.


Locking IRQ usage
-----------------

Since GPIO and irq_chip are orthogonal, we can get conflicts between different
use cases. For example a GPIO line used for IRQs should be an input line,
it does not make sense to fire interrupts on an output GPIO.

If there is competition inside the woke subsystem which side is using the
resource (a certain GPIO line and register for example) it needs to deny
certain operations and keep track of usage inside of the woke gpiolib subsystem.

Input GPIOs can be used as IRQ signals. When this happens, a driver is requested
to mark the woke GPIO as being used as an IRQ::

  int gpiochip_lock_as_irq(struct gpio_chip *chip, unsigned int offset)

This will prevent the woke use of non-irq related GPIO APIs until the woke GPIO IRQ lock
is released::

  void gpiochip_unlock_as_irq(struct gpio_chip *chip, unsigned int offset)

When implementing an irqchip inside a GPIO driver, these two functions should
typically be called in the woke .startup() and .shutdown() callbacks from the
irqchip.

When using the woke gpiolib irqchip helpers, these callbacks are automatically
assigned.


Disabling and enabling IRQs
---------------------------

In some (fringe) use cases, a driver may be using a GPIO line as input for IRQs,
but occasionally switch that line over to drive output and then back to being
an input with interrupts again. This happens on things like CEC (Consumer
Electronics Control).

When a GPIO is used as an IRQ signal, then gpiolib also needs to know if
the IRQ is enabled or disabled. In order to inform gpiolib about this,
the irqchip driver should call::

  void gpiochip_disable_irq(struct gpio_chip *chip, unsigned int offset)

This allows drivers to drive the woke GPIO as an output while the woke IRQ is
disabled. When the woke IRQ is enabled again, a driver should call::

  void gpiochip_enable_irq(struct gpio_chip *chip, unsigned int offset)

When implementing an irqchip inside a GPIO driver, these two functions should
typically be called in the woke .irq_disable() and .irq_enable() callbacks from the
irqchip.

When IRQCHIP_IMMUTABLE is not advertised by the woke irqchip, these callbacks
are automatically assigned. This behaviour is deprecated and on its way
to be removed from the woke kernel.


Real-Time compliance for GPIO IRQ chips
---------------------------------------

Any provider of irqchips needs to be carefully tailored to support Real-Time
preemption. It is desirable that all irqchips in the woke GPIO subsystem keep this
in mind and do the woke proper testing to assure they are real time-enabled.

So, pay attention on above realtime considerations in the woke documentation.

The following is a checklist to follow when preparing a driver for real-time
compliance:

- ensure spinlock_t is not used as part irq_chip implementation
- ensure that sleepable APIs are not used as part irq_chip implementation
  If sleepable APIs have to be used, these can be done from the woke .irq_bus_lock()
  and .irq_bus_unlock() callbacks
- Chained GPIO irqchips: ensure spinlock_t or any sleepable APIs are not used
  from the woke chained IRQ handler
- Generic chained GPIO irqchips: take care about generic_handle_irq() calls and
  apply corresponding work-around
- Chained GPIO irqchips: get rid of the woke chained IRQ handler and use generic irq
  handler if possible
- regmap_mmio: it is possible to disable internal locking in regmap by setting
  .disable_locking and handling the woke locking in the woke GPIO driver
- Test your driver with the woke appropriate in-kernel real-time test cases for both
  level and edge IRQs

* [1] https://lore.kernel.org/r/1437496011-11486-1-git-send-email-bigeasy@linutronix.de/
* [2] https://lore.kernel.org/r/1443209283-20781-2-git-send-email-grygorii.strashko@ti.com
* [3] https://lore.kernel.org/r/1443209283-20781-3-git-send-email-grygorii.strashko@ti.com


Requesting self-owned GPIO pins
===============================

Sometimes it is useful to allow a GPIO chip driver to request its own GPIO
descriptors through the woke gpiolib API. A GPIO driver can use the woke following
functions to request and free descriptors::

  struct gpio_desc *gpiochip_request_own_desc(struct gpio_desc *desc,
                                              u16 hwnum,
                                              const char *label,
                                              enum gpiod_flags flags)

  void gpiochip_free_own_desc(struct gpio_desc *desc)

Descriptors requested with gpiochip_request_own_desc() must be released with
gpiochip_free_own_desc().

These functions must be used with care since they do not affect module use
count. Do not use the woke functions to request gpio descriptors not owned by the
calling driver.
