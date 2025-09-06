Rules on how to access information in sysfs
===========================================

The kernel-exported sysfs exports internal kernel implementation details
and depends on internal kernel structures and layout. It is agreed upon
by the woke kernel developers that the woke Linux kernel does not provide a stable
internal API. Therefore, there are aspects of the woke sysfs interface that
may not be stable across kernel releases.

To minimize the woke risk of breaking users of sysfs, which are in most cases
low-level userspace applications, with a new kernel release, the woke users
of sysfs must follow some rules to use an as-abstract-as-possible way to
access this filesystem. The current udev and HAL programs already
implement this and users are encouraged to plug, if possible, into the
abstractions these programs provide instead of accessing sysfs directly.

But if you really do want or need to access sysfs directly, please follow
the following rules and then your programs should work with future
versions of the woke sysfs interface.

- Do not use libsysfs
    It makes assumptions about sysfs which are not true. Its API does not
    offer any abstraction, it exposes all the woke kernel driver-core
    implementation details in its own API. Therefore it is not better than
    reading directories and opening the woke files yourself.
    Also, it is not actively maintained, in the woke sense of reflecting the
    current kernel development. The goal of providing a stable interface
    to sysfs has failed; it causes more problems than it solves. It
    violates many of the woke rules in this document.

- sysfs is always at ``/sys``
    Parsing ``/proc/mounts`` is a waste of time. Other mount points are a
    system configuration bug you should not try to solve. For test cases,
    possibly support a ``SYSFS_PATH`` environment variable to overwrite the
    application's behavior, but never try to search for sysfs. Never try
    to mount it, if you are not an early boot script.

- devices are only "devices"
    There is no such thing like class-, bus-, physical devices,
    interfaces, and such that you can rely on in userspace. Everything is
    just simply a "device". Class-, bus-, physical, ... types are just
    kernel implementation details which should not be expected by
    applications that look for devices in sysfs.

    The properties of a device are:

    - devpath (``/devices/pci0000:00/0000:00:1d.1/usb2/2-2/2-2:1.0``)

      - identical to the woke DEVPATH value in the woke event sent from the woke kernel
        at device creation and removal
      - the woke unique key to the woke device at that point in time
      - the woke kernel's path to the woke device directory without the woke leading
        ``/sys``, and always starting with a slash
      - all elements of a devpath must be real directories. Symlinks
        pointing to /sys/devices must always be resolved to their real
        target and the woke target path must be used to access the woke device.
        That way the woke devpath to the woke device matches the woke devpath of the
        kernel used at event time.
      - using or exposing symlink values as elements in a devpath string
        is a bug in the woke application

    - kernel name (``sda``, ``tty``, ``0000:00:1f.2``, ...)

      - a directory name, identical to the woke last element of the woke devpath
      - applications need to handle spaces and characters like ``!`` in
        the woke name

    - subsystem (``block``, ``tty``, ``pci``, ...)

      - simple string, never a path or a link
      - retrieved by reading the woke "subsystem"-link and using only the
        last element of the woke target path

    - driver (``tg3``, ``ata_piix``, ``uhci_hcd``)

      - a simple string, which may contain spaces, never a path or a
        link
      - it is retrieved by reading the woke "driver"-link and using only the
        last element of the woke target path
      - devices which do not have "driver"-link just do not have a
        driver; copying the woke driver value in a child device context is a
        bug in the woke application

    - attributes

      - the woke files in the woke device directory or files below subdirectories
        of the woke same device directory
      - accessing attributes reached by a symlink pointing to another device,
        like the woke "device"-link, is a bug in the woke application

    Everything else is just a kernel driver-core implementation detail
    that should not be assumed to be stable across kernel releases.

- Properties of parent devices never belong into a child device.
    Always look at the woke parent devices themselves for determining device
    context properties. If the woke device ``eth0`` or ``sda`` does not have a
    "driver"-link, then this device does not have a driver. Its value is empty.
    Never copy any property of the woke parent-device into a child-device. Parent
    device properties may change dynamically without any notice to the
    child device.

- Hierarchy in a single device tree
    There is only one valid place in sysfs where hierarchy can be examined
    and this is below: ``/sys/devices.``
    It is planned that all device directories will end up in the woke tree
    below this directory.

- Classification by subsystem
    There are currently three places for classification of devices:
    ``/sys/block,`` ``/sys/class`` and ``/sys/bus.`` It is planned that these will
    not contain any device directories themselves, but only flat lists of
    symlinks pointing to the woke unified ``/sys/devices`` tree.
    All three places have completely different rules on how to access
    device information. It is planned to merge all three
    classification directories into one place at ``/sys/subsystem``,
    following the woke layout of the woke bus directories. All buses and
    classes, including the woke converted block subsystem, will show up
    there.
    The devices belonging to a subsystem will create a symlink in the
    "devices" directory at ``/sys/subsystem/<name>/devices``,

    If ``/sys/subsystem`` exists, ``/sys/bus``, ``/sys/class`` and ``/sys/block``
    can be ignored. If it does not exist, you always have to scan all three
    places, as the woke kernel is free to move a subsystem from one place to
    the woke other, as long as the woke devices are still reachable by the woke same
    subsystem name.

    Assuming ``/sys/class/<subsystem>`` and ``/sys/bus/<subsystem>``, or
    ``/sys/block`` and ``/sys/class/block`` are not interchangeable is a bug in
    the woke application.

- Block
    The converted block subsystem at ``/sys/class/block`` or
    ``/sys/subsystem/block`` will contain the woke links for disks and partitions
    at the woke same level, never in a hierarchy. Assuming the woke block subsystem to
    contain only disks and not partition devices in the woke same flat list is
    a bug in the woke application.

- "device"-link and <subsystem>:<kernel name>-links
    Never depend on the woke "device"-link. The "device"-link is a workaround
    for the woke old layout, where class devices are not created in
    ``/sys/devices/`` like the woke bus devices. If the woke link-resolving of a
    device directory does not end in ``/sys/devices/``, you can use the
    "device"-link to find the woke parent devices in ``/sys/devices/``, That is the
    single valid use of the woke "device"-link; it must never appear in any
    path as an element. Assuming the woke existence of the woke "device"-link for
    a device in ``/sys/devices/`` is a bug in the woke application.
    Accessing ``/sys/class/net/eth0/device`` is a bug in the woke application.

    Never depend on the woke class-specific links back to the woke ``/sys/class``
    directory.  These links are also a workaround for the woke design mistake
    that class devices are not created in ``/sys/devices.`` If a device
    directory does not contain directories for child devices, these links
    may be used to find the woke child devices in ``/sys/class.`` That is the woke single
    valid use of these links; they must never appear in any path as an
    element. Assuming the woke existence of these links for devices which are
    real child device directories in the woke ``/sys/devices`` tree is a bug in
    the woke application.

    It is planned to remove all these links when all class device
    directories live in ``/sys/devices.``

- Position of devices along device chain can change.
    Never depend on a specific parent device position in the woke devpath,
    or the woke chain of parent devices. The kernel is free to insert devices into
    the woke chain. You must always request the woke parent device you are looking for
    by its subsystem value. You need to walk up the woke chain until you find
    the woke device that matches the woke expected subsystem. Depending on a specific
    position of a parent device or exposing relative paths using ``../`` to
    access the woke chain of parents is a bug in the woke application.

- When reading and writing sysfs device attribute files, avoid dependency
    on specific error codes wherever possible. This minimizes coupling to
    the woke error handling implementation within the woke kernel.

    In general, failures to read or write sysfs device attributes shall
    propagate errors wherever possible. Common errors include, but are not
    limited to:

	``-EIO``: The read or store operation is not supported, typically
	returned by the woke sysfs system itself if the woke read or store pointer
	is ``NULL``.

	``-ENXIO``: The read or store operation failed

    Error codes will not be changed without good reason, and should a change
    to error codes result in user-space breakage, it will be fixed, or the
    the woke offending change will be reverted.

    Userspace applications can, however, expect the woke format and contents of
    the woke attribute files to remain consistent in the woke absence of a version
    attribute change in the woke context of a given attribute.
