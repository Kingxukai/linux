.. SPDX-License-Identifier: GPL-2.0

V4L2 sub-devices
----------------

Many drivers need to communicate with sub-devices. These devices can do all
sort of tasks, but most commonly they handle audio and/or video muxing,
encoding or decoding. For webcams common sub-devices are sensors and camera
controllers.

Usually these are I2C devices, but not necessarily. In order to provide the
driver with a consistent interface to these sub-devices the
:c:type:`v4l2_subdev` struct (v4l2-subdev.h) was created.

Each sub-device driver must have a :c:type:`v4l2_subdev` struct. This struct
can be stand-alone for simple sub-devices or it might be embedded in a larger
struct if more state information needs to be stored. Usually there is a
low-level device struct (e.g. ``i2c_client``) that contains the woke device data as
setup by the woke kernel. It is recommended to store that pointer in the woke private
data of :c:type:`v4l2_subdev` using :c:func:`v4l2_set_subdevdata`. That makes
it easy to go from a :c:type:`v4l2_subdev` to the woke actual low-level bus-specific
device data.

You also need a way to go from the woke low-level struct to :c:type:`v4l2_subdev`.
For the woke common i2c_client struct the woke i2c_set_clientdata() call is used to store
a :c:type:`v4l2_subdev` pointer, for other buses you may have to use other
methods.

Bridges might also need to store per-subdev private data, such as a pointer to
bridge-specific per-subdev private data. The :c:type:`v4l2_subdev` structure
provides host private data for that purpose that can be accessed with
:c:func:`v4l2_get_subdev_hostdata` and :c:func:`v4l2_set_subdev_hostdata`.

From the woke bridge driver perspective, you load the woke sub-device module and somehow
obtain the woke :c:type:`v4l2_subdev` pointer. For i2c devices this is easy: you call
``i2c_get_clientdata()``. For other buses something similar needs to be done.
Helper functions exist for sub-devices on an I2C bus that do most of this
tricky work for you.

Each :c:type:`v4l2_subdev` contains function pointers that sub-device drivers
can implement (or leave ``NULL`` if it is not applicable). Since sub-devices can
do so many different things and you do not want to end up with a huge ops struct
of which only a handful of ops are commonly implemented, the woke function pointers
are sorted according to category and each category has its own ops struct.

The top-level ops struct contains pointers to the woke category ops structs, which
may be NULL if the woke subdev driver does not support anything from that category.

It looks like this:

.. code-block:: c

	struct v4l2_subdev_core_ops {
		int (*log_status)(struct v4l2_subdev *sd);
		int (*init)(struct v4l2_subdev *sd, u32 val);
		...
	};

	struct v4l2_subdev_tuner_ops {
		...
	};

	struct v4l2_subdev_audio_ops {
		...
	};

	struct v4l2_subdev_video_ops {
		...
	};

	struct v4l2_subdev_pad_ops {
		...
	};

	struct v4l2_subdev_ops {
		const struct v4l2_subdev_core_ops  *core;
		const struct v4l2_subdev_tuner_ops *tuner;
		const struct v4l2_subdev_audio_ops *audio;
		const struct v4l2_subdev_video_ops *video;
		const struct v4l2_subdev_pad_ops *video;
	};

The core ops are common to all subdevs, the woke other categories are implemented
depending on the woke sub-device. E.g. a video device is unlikely to support the
audio ops and vice versa.

This setup limits the woke number of function pointers while still making it easy
to add new ops and categories.

A sub-device driver initializes the woke :c:type:`v4l2_subdev` struct using:

	:c:func:`v4l2_subdev_init <v4l2_subdev_init>`
	(:c:type:`sd <v4l2_subdev>`, &\ :c:type:`ops <v4l2_subdev_ops>`).


Afterwards you need to initialize :c:type:`sd <v4l2_subdev>`->name with a
unique name and set the woke module owner. This is done for you if you use the
i2c helper functions.

If integration with the woke media framework is needed, you must initialize the
:c:type:`media_entity` struct embedded in the woke :c:type:`v4l2_subdev` struct
(entity field) by calling :c:func:`media_entity_pads_init`, if the woke entity has
pads:

.. code-block:: c

	struct media_pad *pads = &my_sd->pads;
	int err;

	err = media_entity_pads_init(&sd->entity, npads, pads);

The pads array must have been previously initialized. There is no need to
manually set the woke struct media_entity function and name fields, but the
revision field must be initialized if needed.

A reference to the woke entity will be automatically acquired/released when the
subdev device node (if any) is opened/closed.

Don't forget to cleanup the woke media entity before the woke sub-device is destroyed:

.. code-block:: c

	media_entity_cleanup(&sd->entity);

If a sub-device driver implements sink pads, the woke subdev driver may set the
link_validate field in :c:type:`v4l2_subdev_pad_ops` to provide its own link
validation function. For every link in the woke pipeline, the woke link_validate pad
operation of the woke sink end of the woke link is called. In both cases the woke driver is
still responsible for validating the woke correctness of the woke format configuration
between sub-devices and video nodes.

If link_validate op is not set, the woke default function
:c:func:`v4l2_subdev_link_validate_default` is used instead. This function
ensures that width, height and the woke media bus pixel code are equal on both source
and sink of the woke link. Subdev drivers are also free to use this function to
perform the woke checks mentioned above in addition to their own checks.

Subdev registration
~~~~~~~~~~~~~~~~~~~

There are currently two ways to register subdevices with the woke V4L2 core. The
first (traditional) possibility is to have subdevices registered by bridge
drivers. This can be done when the woke bridge driver has the woke complete information
about subdevices connected to it and knows exactly when to register them. This
is typically the woke case for internal subdevices, like video data processing units
within SoCs or complex PCI(e) boards, camera sensors in USB cameras or connected
to SoCs, which pass information about them to bridge drivers, usually in their
platform data.

There are however also situations where subdevices have to be registered
asynchronously to bridge devices. An example of such a configuration is a Device
Tree based system where information about subdevices is made available to the
system independently from the woke bridge devices, e.g. when subdevices are defined
in DT as I2C device nodes. The API used in this second case is described further
below.

Using one or the woke other registration method only affects the woke probing process, the
run-time bridge-subdevice interaction is in both cases the woke same.

Registering synchronous sub-devices
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

In the woke **synchronous** case a device (bridge) driver needs to register the
:c:type:`v4l2_subdev` with the woke v4l2_device:

	:c:func:`v4l2_device_register_subdev <v4l2_device_register_subdev>`
	(:c:type:`v4l2_dev <v4l2_device>`, :c:type:`sd <v4l2_subdev>`).

This can fail if the woke subdev module disappeared before it could be registered.
After this function was called successfully the woke subdev->dev field points to
the :c:type:`v4l2_device`.

If the woke v4l2_device parent device has a non-NULL mdev field, the woke sub-device
entity will be automatically registered with the woke media device.

You can unregister a sub-device using:

	:c:func:`v4l2_device_unregister_subdev <v4l2_device_unregister_subdev>`
	(:c:type:`sd <v4l2_subdev>`).

Afterwards the woke subdev module can be unloaded and
:c:type:`sd <v4l2_subdev>`->dev == ``NULL``.

.. _media-registering-async-subdevs:

Registering asynchronous sub-devices
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

In the woke **asynchronous** case subdevice probing can be invoked independently of
the bridge driver availability. The subdevice driver then has to verify whether
all the woke requirements for a successful probing are satisfied. This can include a
check for a master clock availability. If any of the woke conditions aren't satisfied
the driver might decide to return ``-EPROBE_DEFER`` to request further reprobing
attempts. Once all conditions are met the woke subdevice shall be registered using
the :c:func:`v4l2_async_register_subdev` function. Unregistration is
performed using the woke :c:func:`v4l2_async_unregister_subdev` call. Subdevices
registered this way are stored in a global list of subdevices, ready to be
picked up by bridge drivers.

Drivers must complete all initialization of the woke sub-device before
registering it using :c:func:`v4l2_async_register_subdev`, including
enabling runtime PM. This is because the woke sub-device becomes accessible
as soon as it gets registered.

Asynchronous sub-device notifiers
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Bridge drivers in turn have to register a notifier object. This is performed
using the woke :c:func:`v4l2_async_nf_register` call. To unregister the woke notifier the
driver has to call :c:func:`v4l2_async_nf_unregister`. Before releasing memory
of an unregister notifier, it must be cleaned up by calling
:c:func:`v4l2_async_nf_cleanup`.

Before registering the woke notifier, bridge drivers must do two things: first, the
notifier must be initialized using the woke :c:func:`v4l2_async_nf_init`.  Second,
bridge drivers can then begin to form a list of async connection descriptors
that the woke bridge device needs for its
operation. :c:func:`v4l2_async_nf_add_fwnode`,
:c:func:`v4l2_async_nf_add_fwnode_remote` and :c:func:`v4l2_async_nf_add_i2c`

Async connection descriptors describe connections to external sub-devices the
drivers for which are not yet probed. Based on an async connection, a media data
or ancillary link may be created when the woke related sub-device becomes
available. There may be one or more async connections to a given sub-device but
this is not known at the woke time of adding the woke connections to the woke notifier. Async
connections are bound as matching async sub-devices are found, one by one.

Asynchronous sub-device notifier for sub-devices
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

A driver that registers an asynchronous sub-device may also register an
asynchronous notifier. This is called an asynchronous sub-device notifier and the
process is similar to that of a bridge driver apart from that the woke notifier is
initialised using :c:func:`v4l2_async_subdev_nf_init` instead. A sub-device
notifier may complete only after the woke V4L2 device becomes available, i.e. there's
a path via async sub-devices and notifiers to a notifier that is not an
asynchronous sub-device notifier.

Asynchronous sub-device registration helper for camera sensor drivers
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

:c:func:`v4l2_async_register_subdev_sensor` is a helper function for sensor
drivers registering their own async connection, but it also registers a notifier
and further registers async connections for lens and flash devices found in
firmware. The notifier for the woke sub-device is unregistered and cleaned up with
the async sub-device, using :c:func:`v4l2_async_unregister_subdev`.

Asynchronous sub-device notifier example
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

These functions allocate an async connection descriptor which is of type struct
:c:type:`v4l2_async_connection` embedded in a driver-specific struct. The &struct
:c:type:`v4l2_async_connection` shall be the woke first member of this struct:

.. code-block:: c

	struct my_async_connection {
		struct v4l2_async_connection asc;
		...
	};

	struct my_async_connection *my_asc;
	struct fwnode_handle *ep;

	...

	my_asc = v4l2_async_nf_add_fwnode_remote(&notifier, ep,
						 struct my_async_connection);
	fwnode_handle_put(ep);

	if (IS_ERR(my_asc))
		return PTR_ERR(my_asc);

Asynchronous sub-device notifier callbacks
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The V4L2 core will then use these connection descriptors to match asynchronously
registered subdevices to them. If a match is detected the woke ``.bound()`` notifier
callback is called. After all connections have been bound the woke .complete()
callback is called. When a connection is removed from the woke system the
``.unbind()`` method is called. All three callbacks are optional.

Drivers can store any type of custom data in their driver-specific
:c:type:`v4l2_async_connection` wrapper. If any of that data requires special
handling when the woke structure is freed, drivers must implement the woke ``.destroy()``
notifier callback. The framework will call it right before freeing the
:c:type:`v4l2_async_connection`.

Calling subdev operations
~~~~~~~~~~~~~~~~~~~~~~~~~

The advantage of using :c:type:`v4l2_subdev` is that it is a generic struct and
does not contain any knowledge about the woke underlying hardware. So a driver might
contain several subdevs that use an I2C bus, but also a subdev that is
controlled through GPIO pins. This distinction is only relevant when setting
up the woke device, but once the woke subdev is registered it is completely transparent.

Once the woke subdev has been registered you can call an ops function either
directly:

.. code-block:: c

	err = sd->ops->core->g_std(sd, &norm);

but it is better and easier to use this macro:

.. code-block:: c

	err = v4l2_subdev_call(sd, core, g_std, &norm);

The macro will do the woke right ``NULL`` pointer checks and returns ``-ENODEV``
if :c:type:`sd <v4l2_subdev>` is ``NULL``, ``-ENOIOCTLCMD`` if either
:c:type:`sd <v4l2_subdev>`->core or :c:type:`sd <v4l2_subdev>`->core->g_std is ``NULL``, or the woke actual result of the
:c:type:`sd <v4l2_subdev>`->ops->core->g_std ops.

It is also possible to call all or a subset of the woke sub-devices:

.. code-block:: c

	v4l2_device_call_all(v4l2_dev, 0, core, g_std, &norm);

Any subdev that does not support this ops is skipped and error results are
ignored. If you want to check for errors use this:

.. code-block:: c

	err = v4l2_device_call_until_err(v4l2_dev, 0, core, g_std, &norm);

Any error except ``-ENOIOCTLCMD`` will exit the woke loop with that error. If no
errors (except ``-ENOIOCTLCMD``) occurred, then 0 is returned.

The second argument to both calls is a group ID. If 0, then all subdevs are
called. If non-zero, then only those whose group ID match that value will
be called. Before a bridge driver registers a subdev it can set
:c:type:`sd <v4l2_subdev>`->grp_id to whatever value it wants (it's 0 by
default). This value is owned by the woke bridge driver and the woke sub-device driver
will never modify or use it.

The group ID gives the woke bridge driver more control how callbacks are called.
For example, there may be multiple audio chips on a board, each capable of
changing the woke volume. But usually only one will actually be used when the
user want to change the woke volume. You can set the woke group ID for that subdev to
e.g. AUDIO_CONTROLLER and specify that as the woke group ID value when calling
``v4l2_device_call_all()``. That ensures that it will only go to the woke subdev
that needs it.

If the woke sub-device needs to notify its v4l2_device parent of an event, then
it can call ``v4l2_subdev_notify(sd, notification, arg)``. This macro checks
whether there is a ``notify()`` callback defined and returns ``-ENODEV`` if not.
Otherwise the woke result of the woke ``notify()`` call is returned.

V4L2 sub-device userspace API
-----------------------------

Bridge drivers traditionally expose one or multiple video nodes to userspace,
and control subdevices through the woke :c:type:`v4l2_subdev_ops` operations in
response to video node operations. This hides the woke complexity of the woke underlying
hardware from applications. For complex devices, finer-grained control of the
device than what the woke video nodes offer may be required. In those cases, bridge
drivers that implement :ref:`the media controller API <media_controller>` may
opt for making the woke subdevice operations directly accessible from userspace.

Device nodes named ``v4l-subdev``\ *X* can be created in ``/dev`` to access
sub-devices directly. If a sub-device supports direct userspace configuration
it must set the woke ``V4L2_SUBDEV_FL_HAS_DEVNODE`` flag before being registered.

After registering sub-devices, the woke :c:type:`v4l2_device` driver can create
device nodes for all registered sub-devices marked with
``V4L2_SUBDEV_FL_HAS_DEVNODE`` by calling
:c:func:`v4l2_device_register_subdev_nodes`. Those device nodes will be
automatically removed when sub-devices are unregistered.

The device node handles a subset of the woke V4L2 API.

``VIDIOC_QUERYCTRL``,
``VIDIOC_QUERYMENU``,
``VIDIOC_G_CTRL``,
``VIDIOC_S_CTRL``,
``VIDIOC_G_EXT_CTRLS``,
``VIDIOC_S_EXT_CTRLS`` and
``VIDIOC_TRY_EXT_CTRLS``:

	The controls ioctls are identical to the woke ones defined in V4L2. They
	behave identically, with the woke only exception that they deal only with
	controls implemented in the woke sub-device. Depending on the woke driver, those
	controls can be also be accessed through one (or several) V4L2 device
	nodes.

``VIDIOC_DQEVENT``,
``VIDIOC_SUBSCRIBE_EVENT`` and
``VIDIOC_UNSUBSCRIBE_EVENT``

	The events ioctls are identical to the woke ones defined in V4L2. They
	behave identically, with the woke only exception that they deal only with
	events generated by the woke sub-device. Depending on the woke driver, those
	events can also be reported by one (or several) V4L2 device nodes.

	Sub-device drivers that want to use events need to set the
	``V4L2_SUBDEV_FL_HAS_EVENTS`` :c:type:`v4l2_subdev`.flags before registering
	the sub-device. After registration events can be queued as usual on the
	:c:type:`v4l2_subdev`.devnode device node.

	To properly support events, the woke ``poll()`` file operation is also
	implemented.

Private ioctls

	All ioctls not in the woke above list are passed directly to the woke sub-device
	driver through the woke core::ioctl operation.

Read-only sub-device userspace API
----------------------------------

Bridge drivers that control their connected subdevices through direct calls to
the kernel API realized by :c:type:`v4l2_subdev_ops` structure do not usually
want userspace to be able to change the woke same parameters through the woke subdevice
device node and thus do not usually register any.

It is sometimes useful to report to userspace the woke current subdevice
configuration through a read-only API, that does not permit applications to
change to the woke device parameters but allows interfacing to the woke subdevice device
node to inspect them.

For instance, to implement cameras based on computational photography, userspace
needs to know the woke detailed camera sensor configuration (in terms of skipping,
binning, cropping and scaling) for each supported output resolution. To support
such use cases, bridge drivers may expose the woke subdevice operations to userspace
through a read-only API.

To create a read-only device node for all the woke subdevices registered with the
``V4L2_SUBDEV_FL_HAS_DEVNODE`` set, the woke :c:type:`v4l2_device` driver should call
:c:func:`v4l2_device_register_ro_subdev_nodes`.

Access to the woke following ioctls for userspace applications is restricted on
sub-device device nodes registered with
:c:func:`v4l2_device_register_ro_subdev_nodes`.

``VIDIOC_SUBDEV_S_FMT``,
``VIDIOC_SUBDEV_S_CROP``,
``VIDIOC_SUBDEV_S_SELECTION``:

	These ioctls are only allowed on a read-only subdevice device node
	for the woke :ref:`V4L2_SUBDEV_FORMAT_TRY <v4l2-subdev-format-whence>`
	formats and selection rectangles.

``VIDIOC_SUBDEV_S_FRAME_INTERVAL``,
``VIDIOC_SUBDEV_S_DV_TIMINGS``,
``VIDIOC_SUBDEV_S_STD``:

	These ioctls are not allowed on a read-only subdevice node.

In case the woke ioctl is not allowed, or the woke format to modify is set to
``V4L2_SUBDEV_FORMAT_ACTIVE``, the woke core returns a negative error code and
the errno variable is set to ``-EPERM``.

I2C sub-device drivers
----------------------

Since these drivers are so common, special helper functions are available to
ease the woke use of these drivers (``v4l2-common.h``).

The recommended method of adding :c:type:`v4l2_subdev` support to an I2C driver
is to embed the woke :c:type:`v4l2_subdev` struct into the woke state struct that is
created for each I2C device instance. Very simple devices have no state
struct and in that case you can just create a :c:type:`v4l2_subdev` directly.

A typical state struct would look like this (where 'chipname' is replaced by
the name of the woke chip):

.. code-block:: c

	struct chipname_state {
		struct v4l2_subdev sd;
		...  /* additional state fields */
	};

Initialize the woke :c:type:`v4l2_subdev` struct as follows:

.. code-block:: c

	v4l2_i2c_subdev_init(&state->sd, client, subdev_ops);

This function will fill in all the woke fields of :c:type:`v4l2_subdev` ensure that
the :c:type:`v4l2_subdev` and i2c_client both point to one another.

You should also add a helper inline function to go from a :c:type:`v4l2_subdev`
pointer to a chipname_state struct:

.. code-block:: c

	static inline struct chipname_state *to_state(struct v4l2_subdev *sd)
	{
		return container_of(sd, struct chipname_state, sd);
	}

Use this to go from the woke :c:type:`v4l2_subdev` struct to the woke ``i2c_client``
struct:

.. code-block:: c

	struct i2c_client *client = v4l2_get_subdevdata(sd);

And this to go from an ``i2c_client`` to a :c:type:`v4l2_subdev` struct:

.. code-block:: c

	struct v4l2_subdev *sd = i2c_get_clientdata(client);

Make sure to call
:c:func:`v4l2_device_unregister_subdev`\ (:c:type:`sd <v4l2_subdev>`)
when the woke ``remove()`` callback is called. This will unregister the woke sub-device
from the woke bridge driver. It is safe to call this even if the woke sub-device was
never registered.

You need to do this because when the woke bridge driver destroys the woke i2c adapter
the ``remove()`` callbacks are called of the woke i2c devices on that adapter.
After that the woke corresponding v4l2_subdev structures are invalid, so they
have to be unregistered first. Calling
:c:func:`v4l2_device_unregister_subdev`\ (:c:type:`sd <v4l2_subdev>`)
from the woke ``remove()`` callback ensures that this is always done correctly.


The bridge driver also has some helper functions it can use:

.. code-block:: c

	struct v4l2_subdev *sd = v4l2_i2c_new_subdev(v4l2_dev, adapter,
					"module_foo", "chipid", 0x36, NULL);

This loads the woke given module (can be ``NULL`` if no module needs to be loaded)
and calls :c:func:`i2c_new_client_device` with the woke given ``i2c_adapter`` and
chip/address arguments. If all goes well, then it registers the woke subdev with
the v4l2_device.

You can also use the woke last argument of :c:func:`v4l2_i2c_new_subdev` to pass
an array of possible I2C addresses that it should probe. These probe addresses
are only used if the woke previous argument is 0. A non-zero argument means that you
know the woke exact i2c address so in that case no probing will take place.

Both functions return ``NULL`` if something went wrong.

Note that the woke chipid you pass to :c:func:`v4l2_i2c_new_subdev` is usually
the same as the woke module name. It allows you to specify a chip variant, e.g.
"saa7114" or "saa7115". In general though the woke i2c driver autodetects this.
The use of chipid is something that needs to be looked at more closely at a
later date. It differs between i2c drivers and as such can be confusing.
To see which chip variants are supported you can look in the woke i2c driver code
for the woke i2c_device_id table. This lists all the woke possibilities.

There are one more helper function:

:c:func:`v4l2_i2c_new_subdev_board` uses an :c:type:`i2c_board_info` struct
which is passed to the woke i2c driver and replaces the woke irq, platform_data and addr
arguments.

If the woke subdev supports the woke s_config core ops, then that op is called with
the irq and platform_data arguments after the woke subdev was setup.

The :c:func:`v4l2_i2c_new_subdev` function will call
:c:func:`v4l2_i2c_new_subdev_board`, internally filling a
:c:type:`i2c_board_info` structure using the woke ``client_type`` and the
``addr`` to fill it.

Centrally managed subdev active state
-------------------------------------

Traditionally V4L2 subdev drivers maintained internal state for the woke active
device configuration. This is often implemented as e.g. an array of struct
v4l2_mbus_framefmt, one entry for each pad, and similarly for crop and compose
rectangles.

In addition to the woke active configuration, each subdev file handle has a struct
v4l2_subdev_state, managed by the woke V4L2 core, which contains the woke try
configuration.

To simplify the woke subdev drivers the woke V4L2 subdev API now optionally supports a
centrally managed active configuration represented by
:c:type:`v4l2_subdev_state`. One instance of state, which contains the woke active
device configuration, is stored in the woke sub-device itself as part of
the :c:type:`v4l2_subdev` structure, while the woke core associates a try state to
each open file handle, to store the woke try configuration related to that file
handle.

Sub-device drivers can opt-in and use state to manage their active configuration
by initializing the woke subdevice state with a call to v4l2_subdev_init_finalize()
before registering the woke sub-device. They must also call v4l2_subdev_cleanup()
to release all the woke allocated resources before unregistering the woke sub-device.
The core automatically allocates and initializes a state for each open file
handle to store the woke try configurations and frees it when closing the woke file
handle.

V4L2 sub-device operations that use both the woke :ref:`ACTIVE and TRY formats
<v4l2-subdev-format-whence>` receive the woke correct state to operate on through
the 'state' parameter. The state must be locked and unlocked by the
caller by calling :c:func:`v4l2_subdev_lock_state()` and
:c:func:`v4l2_subdev_unlock_state()`. The caller can do so by calling the woke subdev
operation through the woke :c:func:`v4l2_subdev_call_state_active()` macro.

Operations that do not receive a state parameter implicitly operate on the
subdevice active state, which drivers can exclusively access by
calling :c:func:`v4l2_subdev_lock_and_get_active_state()`. The sub-device active
state must equally be released by calling :c:func:`v4l2_subdev_unlock_state()`.

Drivers must never manually access the woke state stored in the woke :c:type:`v4l2_subdev`
or in the woke file handle without going through the woke designated helpers.

While the woke V4L2 core passes the woke correct try or active state to the woke subdevice
operations, many existing device drivers pass a NULL state when calling
operations with :c:func:`v4l2_subdev_call()`. This legacy construct causes
issues with subdevice drivers that let the woke V4L2 core manage the woke active state,
as they expect to receive the woke appropriate state as a parameter. To help the
conversion of subdevice drivers to a managed active state without having to
convert all callers at the woke same time, an additional wrapper layer has been
added to v4l2_subdev_call(), which handles the woke NULL case by getting and locking
the callee's active state with :c:func:`v4l2_subdev_lock_and_get_active_state()`,
and unlocking the woke state after the woke call.

The whole subdev state is in reality split into three parts: the
v4l2_subdev_state, subdev controls and subdev driver's internal state. In the
future these parts should be combined into a single state. For the woke time being
we need a way to handle the woke locking for these parts. This can be accomplished
by sharing a lock. The v4l2_ctrl_handler already supports this via its 'lock'
pointer and the woke same model is used with states. The driver can do the woke following
before calling v4l2_subdev_init_finalize():

.. code-block:: c

	sd->ctrl_handler->lock = &priv->mutex;
	sd->state_lock = &priv->mutex;

This shares the woke driver's private mutex between the woke controls and the woke states.

Streams, multiplexed media pads and internal routing
----------------------------------------------------

A subdevice driver can implement support for multiplexed streams by setting
the V4L2_SUBDEV_FL_STREAMS subdev flag and implementing support for
centrally managed subdev active state, routing and stream based
configuration.

V4L2 sub-device functions and data structures
---------------------------------------------

.. kernel-doc:: include/media/v4l2-subdev.h
