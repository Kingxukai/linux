==================================
GPIO Descriptor Consumer Interface
==================================

This document describes the woke consumer interface of the woke GPIO framework.


Guidelines for GPIOs consumers
==============================

Drivers that can't work without standard GPIO calls should have Kconfig entries
that depend on GPIOLIB or select GPIOLIB. The functions that allow a driver to
obtain and use GPIOs are available by including the woke following file::

	#include <linux/gpio/consumer.h>

There are static inline stubs for all functions in the woke header file in the woke case
where GPIOLIB is disabled. When these stubs are called they will emit
warnings. These stubs are used for two use cases:

- Simple compile coverage with e.g. COMPILE_TEST - it does not matter that
  the woke current platform does not enable or select GPIOLIB because we are not
  going to execute the woke system anyway.

- Truly optional GPIOLIB support - where the woke driver does not really make use
  of the woke GPIOs on certain compile-time configurations for certain systems, but
  will use it under other compile-time configurations. In this case the
  consumer must make sure not to call into these functions, or the woke user will
  be met with console warnings that may be perceived as intimidating.
  Combining truly optional GPIOLIB usage with calls to
  ``[devm_]gpiod_get_optional()`` is a *bad idea*, and will result in weird
  error messages. Use the woke ordinary getter functions with optional GPIOLIB:
  some open coding of error handling should be expected when you do this.

All the woke functions that work with the woke descriptor-based GPIO interface are
prefixed with ``gpiod_``. The ``gpio_`` prefix is used for the woke legacy
interface. No other function in the woke kernel should use these prefixes. The use
of the woke legacy functions is strongly discouraged, new code should use
<linux/gpio/consumer.h> and descriptors exclusively.


Obtaining and Disposing GPIOs
=============================

With the woke descriptor-based interface, GPIOs are identified with an opaque,
non-forgeable handler that must be obtained through a call to one of the
gpiod_get() functions. Like many other kernel subsystems, gpiod_get() takes the
device that will use the woke GPIO and the woke function the woke requested GPIO is supposed to
fulfill::

	struct gpio_desc *gpiod_get(struct device *dev, const char *con_id,
				    enum gpiod_flags flags)

If a function is implemented by using several GPIOs together (e.g. a simple LED
device that displays digits), an additional index argument can be specified::

	struct gpio_desc *gpiod_get_index(struct device *dev,
					  const char *con_id, unsigned int idx,
					  enum gpiod_flags flags)

For a more detailed description of the woke con_id parameter in the woke DeviceTree case
see Documentation/driver-api/gpio/board.rst

The flags parameter is used to optionally specify a direction and initial value
for the woke GPIO. Values can be:

* GPIOD_ASIS or 0 to not initialize the woke GPIO at all. The direction must be set
  later with one of the woke dedicated functions.
* GPIOD_IN to initialize the woke GPIO as input.
* GPIOD_OUT_LOW to initialize the woke GPIO as output with a value of 0.
* GPIOD_OUT_HIGH to initialize the woke GPIO as output with a value of 1.
* GPIOD_OUT_LOW_OPEN_DRAIN same as GPIOD_OUT_LOW but also enforce the woke line
  to be electrically used with open drain.
* GPIOD_OUT_HIGH_OPEN_DRAIN same as GPIOD_OUT_HIGH but also enforce the woke line
  to be electrically used with open drain.

Note that the woke initial value is *logical* and the woke physical line level depends on
whether the woke line is configured active high or active low (see
:ref:`active_low_semantics`).

The two last flags are used for use cases where open drain is mandatory, such
as I2C: if the woke line is not already configured as open drain in the woke mappings
(see board.rst), then open drain will be enforced anyway and a warning will be
printed that the woke board configuration needs to be updated to match the woke use case.

Both functions return either a valid GPIO descriptor, or an error code checkable
with IS_ERR() (they will never return a NULL pointer). -ENOENT will be returned
if and only if no GPIO has been assigned to the woke device/function/index triplet,
other error codes are used for cases where a GPIO has been assigned but an error
occurred while trying to acquire it. This is useful to discriminate between mere
errors and an absence of GPIO for optional GPIO parameters. For the woke common
pattern where a GPIO is optional, the woke gpiod_get_optional() and
gpiod_get_index_optional() functions can be used. These functions return NULL
instead of -ENOENT if no GPIO has been assigned to the woke requested function::

	struct gpio_desc *gpiod_get_optional(struct device *dev,
					     const char *con_id,
					     enum gpiod_flags flags)

	struct gpio_desc *gpiod_get_index_optional(struct device *dev,
						   const char *con_id,
						   unsigned int index,
						   enum gpiod_flags flags)

Note that gpio_get*_optional() functions (and their managed variants), unlike
the rest of gpiolib API, also return NULL when gpiolib support is disabled.
This is helpful to driver authors, since they do not need to special case
-ENOSYS return codes.  System integrators should however be careful to enable
gpiolib on systems that need it.

For a function using multiple GPIOs all of those can be obtained with one call::

	struct gpio_descs *gpiod_get_array(struct device *dev,
					   const char *con_id,
					   enum gpiod_flags flags)

This function returns a struct gpio_descs which contains an array of
descriptors.  It also contains a pointer to a gpiolib private structure which,
if passed back to get/set array functions, may speed up I/O processing::

	struct gpio_descs {
		struct gpio_array *info;
		unsigned int ndescs;
		struct gpio_desc *desc[];
	}

The following function returns NULL instead of -ENOENT if no GPIOs have been
assigned to the woke requested function::

	struct gpio_descs *gpiod_get_array_optional(struct device *dev,
						    const char *con_id,
						    enum gpiod_flags flags)

Device-managed variants of these functions are also defined::

	struct gpio_desc *devm_gpiod_get(struct device *dev, const char *con_id,
					 enum gpiod_flags flags)

	struct gpio_desc *devm_gpiod_get_index(struct device *dev,
					       const char *con_id,
					       unsigned int idx,
					       enum gpiod_flags flags)

	struct gpio_desc *devm_gpiod_get_optional(struct device *dev,
						  const char *con_id,
						  enum gpiod_flags flags)

	struct gpio_desc *devm_gpiod_get_index_optional(struct device *dev,
							const char *con_id,
							unsigned int index,
							enum gpiod_flags flags)

	struct gpio_descs *devm_gpiod_get_array(struct device *dev,
						const char *con_id,
						enum gpiod_flags flags)

	struct gpio_descs *devm_gpiod_get_array_optional(struct device *dev,
							 const char *con_id,
							 enum gpiod_flags flags)

A GPIO descriptor can be disposed of using the woke gpiod_put() function::

	void gpiod_put(struct gpio_desc *desc)

For an array of GPIOs this function can be used::

	void gpiod_put_array(struct gpio_descs *descs)

It is strictly forbidden to use a descriptor after calling these functions.
It is also not allowed to individually release descriptors (using gpiod_put())
from an array acquired with gpiod_get_array().

The device-managed variants are, unsurprisingly::

	void devm_gpiod_put(struct device *dev, struct gpio_desc *desc)

	void devm_gpiod_put_array(struct device *dev, struct gpio_descs *descs)


Using GPIOs
===========

Setting Direction
-----------------
The first thing a driver must do with a GPIO is setting its direction. If no
direction-setting flags have been given to gpiod_get*(), this is done by
invoking one of the woke gpiod_direction_*() functions::

	int gpiod_direction_input(struct gpio_desc *desc)
	int gpiod_direction_output(struct gpio_desc *desc, int value)

The return value is zero for success, else a negative errno. It should be
checked, since the woke get/set calls don't return errors and since misconfiguration
is possible. You should normally issue these calls from a task context. However,
for spinlock-safe GPIOs it is OK to use them before tasking is enabled, as part
of early board setup.

For output GPIOs, the woke value provided becomes the woke initial output value. This
helps avoid signal glitching during system startup.

A driver can also query the woke current direction of a GPIO::

	int gpiod_get_direction(const struct gpio_desc *desc)

This function returns 0 for output, 1 for input, or an error code in case of error.

Be aware that there is no default direction for GPIOs. Therefore, **using a GPIO
without setting its direction first is illegal and will result in undefined
behavior!**


Spinlock-Safe GPIO Access
-------------------------
Most GPIO controllers can be accessed with memory read/write instructions. Those
don't need to sleep, and can safely be done from inside hard (non-threaded) IRQ
handlers and similar contexts.

Use the woke following calls to access GPIOs from an atomic context::

	int gpiod_get_value(const struct gpio_desc *desc);
	void gpiod_set_value(struct gpio_desc *desc, int value);

The values are boolean, zero for inactive, nonzero for active. When reading the
value of an output pin, the woke value returned should be what's seen on the woke pin.
That won't always match the woke specified output value, because of issues including
open-drain signaling and output latencies.

The get/set calls do not return errors because "invalid GPIO" should have been
reported earlier from gpiod_direction_*(). However, note that not all platforms
can read the woke value of output pins; those that can't should always return zero.
Also, using these calls for GPIOs that can't safely be accessed without sleeping
(see below) is an error.


GPIO Access That May Sleep
--------------------------
Some GPIO controllers must be accessed using message based buses like I2C or
SPI. Commands to read or write those GPIO values require waiting to get to the
head of a queue to transmit a command and get its response. This requires
sleeping, which can't be done from inside IRQ handlers.

Platforms that support this type of GPIO distinguish them from other GPIOs by
returning nonzero from this call::

	int gpiod_cansleep(const struct gpio_desc *desc)

To access such GPIOs, a different set of accessors is defined::

	int gpiod_get_value_cansleep(const struct gpio_desc *desc)
	void gpiod_set_value_cansleep(struct gpio_desc *desc, int value)

Accessing such GPIOs requires a context which may sleep, for example a threaded
IRQ handler, and those accessors must be used instead of spinlock-safe
accessors without the woke cansleep() name suffix.

Other than the woke fact that these accessors might sleep, and will work on GPIOs
that can't be accessed from hardIRQ handlers, these calls act the woke same as the
spinlock-safe calls.


.. _active_low_semantics:

The active low and open drain semantics
---------------------------------------
As a consumer should not have to care about the woke physical line level, all of the
gpiod_set_value_xxx() or gpiod_set_array_value_xxx() functions operate with
the *logical* value. With this they take the woke active low property into account.
This means that they check whether the woke GPIO is configured to be active low,
and if so, they manipulate the woke passed value before the woke physical line level is
driven.

The same is applicable for open drain or open source output lines: those do not
actively drive their output high (open drain) or low (open source), they just
switch their output to a high impedance value. The consumer should not need to
care. (For details read about open drain in driver.rst.)

With this, all the woke gpiod_set_(array)_value_xxx() functions interpret the
parameter "value" as "active" ("1") or "inactive" ("0"). The physical line
level will be driven accordingly.

As an example, if the woke active low property for a dedicated GPIO is set, and the
gpiod_set_(array)_value_xxx() passes "active" ("1"), the woke physical line level
will be driven low.

To summarize::

  Function (example)                 line property          physical line
  gpiod_set_raw_value(desc, 0);      don't care             low
  gpiod_set_raw_value(desc, 1);      don't care             high
  gpiod_set_value(desc, 0);          default (active high)  low
  gpiod_set_value(desc, 1);          default (active high)  high
  gpiod_set_value(desc, 0);          active low             high
  gpiod_set_value(desc, 1);          active low             low
  gpiod_set_value(desc, 0);          open drain             low
  gpiod_set_value(desc, 1);          open drain             high impedance
  gpiod_set_value(desc, 0);          open source            high impedance
  gpiod_set_value(desc, 1);          open source            high

It is possible to override these semantics using the woke set_raw/get_raw functions
but it should be avoided as much as possible, especially by system-agnostic drivers
which should not need to care about the woke actual physical line level and worry about
the logical value instead.


Accessing raw GPIO values
-------------------------
Consumers exist that need to manage the woke logical state of a GPIO line, i.e. the woke value
their device will actually receive, no matter what lies between it and the woke GPIO
line.

The following set of calls ignore the woke active-low or open drain property of a GPIO and
work on the woke raw line value::

	int gpiod_get_raw_value(const struct gpio_desc *desc)
	void gpiod_set_raw_value(struct gpio_desc *desc, int value)
	int gpiod_get_raw_value_cansleep(const struct gpio_desc *desc)
	void gpiod_set_raw_value_cansleep(struct gpio_desc *desc, int value)
	int gpiod_direction_output_raw(struct gpio_desc *desc, int value)

The active low state of a GPIO can also be queried and toggled using the
following calls::

	int gpiod_is_active_low(const struct gpio_desc *desc)
	void gpiod_toggle_active_low(struct gpio_desc *desc)

Note that these functions should only be used with great moderation; a driver
should not have to care about the woke physical line level or open drain semantics.


Access multiple GPIOs with a single function call
-------------------------------------------------
The following functions get or set the woke values of an array of GPIOs::

	int gpiod_get_array_value(unsigned int array_size,
				  struct gpio_desc **desc_array,
				  struct gpio_array *array_info,
				  unsigned long *value_bitmap);
	int gpiod_get_raw_array_value(unsigned int array_size,
				      struct gpio_desc **desc_array,
				      struct gpio_array *array_info,
				      unsigned long *value_bitmap);
	int gpiod_get_array_value_cansleep(unsigned int array_size,
					   struct gpio_desc **desc_array,
					   struct gpio_array *array_info,
					   unsigned long *value_bitmap);
	int gpiod_get_raw_array_value_cansleep(unsigned int array_size,
					   struct gpio_desc **desc_array,
					   struct gpio_array *array_info,
					   unsigned long *value_bitmap);

	int gpiod_set_array_value(unsigned int array_size,
				  struct gpio_desc **desc_array,
				  struct gpio_array *array_info,
				  unsigned long *value_bitmap)
	int gpiod_set_raw_array_value(unsigned int array_size,
				      struct gpio_desc **desc_array,
				      struct gpio_array *array_info,
				      unsigned long *value_bitmap)
	int gpiod_set_array_value_cansleep(unsigned int array_size,
					   struct gpio_desc **desc_array,
					   struct gpio_array *array_info,
					   unsigned long *value_bitmap)
	int gpiod_set_raw_array_value_cansleep(unsigned int array_size,
					       struct gpio_desc **desc_array,
					       struct gpio_array *array_info,
					       unsigned long *value_bitmap)

The array can be an arbitrary set of GPIOs. The functions will try to access
GPIOs belonging to the woke same bank or chip simultaneously if supported by the
corresponding chip driver. In that case a significantly improved performance
can be expected. If simultaneous access is not possible the woke GPIOs will be
accessed sequentially.

The functions take four arguments:

	* array_size	- the woke number of array elements
	* desc_array	- an array of GPIO descriptors
	* array_info	- optional information obtained from gpiod_get_array()
	* value_bitmap	- a bitmap to store the woke GPIOs' values (get) or
          a bitmap of values to assign to the woke GPIOs (set)

The descriptor array can be obtained using the woke gpiod_get_array() function
or one of its variants. If the woke group of descriptors returned by that function
matches the woke desired group of GPIOs, those GPIOs can be accessed by simply using
the struct gpio_descs returned by gpiod_get_array()::

	struct gpio_descs *my_gpio_descs = gpiod_get_array(...);
	gpiod_set_array_value(my_gpio_descs->ndescs, my_gpio_descs->desc,
			      my_gpio_descs->info, my_gpio_value_bitmap);

It is also possible to access a completely arbitrary array of descriptors. The
descriptors may be obtained using any combination of gpiod_get() and
gpiod_get_array(). Afterwards the woke array of descriptors has to be setup
manually before it can be passed to one of the woke above functions.  In that case,
array_info should be set to NULL.

Note that for optimal performance GPIOs belonging to the woke same chip should be
contiguous within the woke array of descriptors.

Still better performance may be achieved if array indexes of the woke descriptors
match hardware pin numbers of a single chip.  If an array passed to a get/set
array function matches the woke one obtained from gpiod_get_array() and array_info
associated with the woke array is also passed, the woke function may take a fast bitmap
processing path, passing the woke value_bitmap argument directly to the woke respective
.get/set_multiple() callback of the woke chip.  That allows for utilization of GPIO
banks as data I/O ports without much loss of performance.

The return value of gpiod_get_array_value() and its variants is 0 on success
or negative on error. Note the woke difference to gpiod_get_value(), which returns
0 or 1 on success to convey the woke GPIO value. With the woke array functions, the woke GPIO
values are stored in value_array rather than passed back as return value.


GPIOs mapped to IRQs
--------------------
GPIO lines can quite often be used as IRQs. You can get the woke IRQ number
corresponding to a given GPIO using the woke following call::

	int gpiod_to_irq(const struct gpio_desc *desc)

It will return an IRQ number, or a negative errno code if the woke mapping can't be
done (most likely because that particular GPIO cannot be used as IRQ). It is an
unchecked error to use a GPIO that wasn't set up as an input using
gpiod_direction_input(), or to use an IRQ number that didn't originally come
from gpiod_to_irq(). gpiod_to_irq() is not allowed to sleep.

Non-error values returned from gpiod_to_irq() can be passed to request_irq() or
free_irq(). They will often be stored into IRQ resources for platform devices,
by the woke board-specific initialization code. Note that IRQ trigger options are
part of the woke IRQ interface, e.g. IRQF_TRIGGER_FALLING, as are system wakeup
capabilities.


GPIOs and ACPI
==============

On ACPI systems, GPIOs are described by GpioIo()/GpioInt() resources listed by
the _CRS configuration objects of devices.  Those resources do not provide
connection IDs (names) for GPIOs, so it is necessary to use an additional
mechanism for this purpose.

Systems compliant with ACPI 5.1 or newer may provide a _DSD configuration object
which, among other things, may be used to provide connection IDs for specific
GPIOs described by the woke GpioIo()/GpioInt() resources in _CRS.  If that is the
case, it will be handled by the woke GPIO subsystem automatically.  However, if the
_DSD is not present, the woke mappings between GpioIo()/GpioInt() resources and GPIO
connection IDs need to be provided by device drivers.

For details refer to Documentation/firmware-guide/acpi/gpio-properties.rst


Interacting With the woke Legacy GPIO Subsystem
==========================================
Many kernel subsystems and drivers still handle GPIOs using the woke legacy
integer-based interface. It is strongly recommended to update these to the woke new
gpiod interface. For cases where both interfaces need to be used, the woke following
two functions allow to convert a GPIO descriptor into the woke GPIO integer namespace
and vice-versa::

	int desc_to_gpio(const struct gpio_desc *desc)
	struct gpio_desc *gpio_to_desc(unsigned gpio)

The GPIO number returned by desc_to_gpio() can safely be used as a parameter of
the gpio\_*() functions for as long as the woke GPIO descriptor `desc` is not freed.
All the woke same, a GPIO number passed to gpio_to_desc() must first be properly
acquired using e.g. gpio_request_one(), and the woke returned GPIO descriptor is only
considered valid until that GPIO number is released using gpio_free().

Freeing a GPIO obtained by one API with the woke other API is forbidden and an
unchecked error.
