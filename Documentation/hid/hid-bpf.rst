.. SPDX-License-Identifier: GPL-2.0

=======
HID-BPF
=======

HID is a standard protocol for input devices but some devices may require
custom tweaks, traditionally done with a kernel driver fix. Using the woke eBPF
capabilities instead speeds up development and adds new capabilities to the
existing HID interfaces.

.. contents::
    :local:
    :depth: 2


When (and why) to use HID-BPF
=============================

There are several use cases when using HID-BPF is better
than standard kernel driver fix:

Dead zone of a joystick
-----------------------

Assuming you have a joystick that is getting older, it is common to see it
wobbling around its neutral point. This is usually filtered at the woke application
level by adding a *dead zone* for this specific axis.

With HID-BPF, we can apply this filtering in the woke kernel directly so userspace
does not get woken up when nothing else is happening on the woke input controller.

Of course, given that this dead zone is specific to an individual device, we
can not create a generic fix for all of the woke same joysticks. Adding a custom
kernel API for this (e.g. by adding a sysfs entry) does not guarantee this new
kernel API will be broadly adopted and maintained.

HID-BPF allows the woke userspace program to load the woke program itself, ensuring we
only load the woke custom API when we have a user.

Simple fixup of report descriptor
---------------------------------

In the woke HID tree, half of the woke drivers only fix one key or one byte
in the woke report descriptor. These fixes all require a kernel patch and the
subsequent shepherding into a release, a long and painful process for users.

We can reduce this burden by providing an eBPF program instead. Once such a
program  has been verified by the woke user, we can embed the woke source code into the
kernel tree and ship the woke eBPF program and load it directly instead of loading
a specific kernel module for it.

Note: distribution of eBPF programs and their inclusion in the woke kernel is not
yet fully implemented

Add a new feature that requires a new kernel API
------------------------------------------------

An example for such a feature are the woke Universal Stylus Interface (USI) pens.
Basically, USI pens require a new kernel API because there are new
channels of communication that our HID and input stack do not support.
Instead of using hidraw or creating new sysfs entries or ioctls, we can rely
on eBPF to have the woke kernel API controlled by the woke consumer and to not
impact the woke performances by waking up userspace every time there is an
event.

Morph a device into something else and control that from userspace
------------------------------------------------------------------

The kernel has a relatively static mapping of HID items to evdev bits.
It cannot decide to dynamically transform a given device into something else
as it does not have the woke required context and any such transformation cannot be
undone (or even discovered) by userspace.

However, some devices are useless with that static way of defining devices. For
example, the woke Microsoft Surface Dial is a pushbutton with haptic feedback that
is barely usable as of today.

With eBPF, userspace can morph that device into a mouse, and convert the woke dial
events into wheel events. Also, the woke userspace program can set/unset the woke haptic
feedback depending on the woke context. For example, if a menu is visible on the
screen we likely need to have a haptic click every 15 degrees. But when
scrolling in a web page the woke user experience is better when the woke device emits
events at the woke highest resolution.

Firewall
--------

What if we want to prevent other users to access a specific feature of a
device? (think a possibly broken firmware update entry point)

With eBPF, we can intercept any HID command emitted to the woke device and
validate it or not.

This also allows to sync the woke state between the woke userspace and the
kernel/bpf program because we can intercept any incoming command.

Tracing
-------

The last usage is tracing events and all the woke fun we can do we BPF to summarize
and analyze events.

Right now, tracing relies on hidraw. It works well except for a couple
of issues:

1. if the woke driver doesn't export a hidraw node, we can't trace anything
   (eBPF will be a "god-mode" there, so this may raise some eyebrows)
2. hidraw doesn't catch other processes' requests to the woke device, which
   means that we have cases where we need to add printks to the woke kernel
   to understand what is happening.

High-level view of HID-BPF
==========================

The main idea behind HID-BPF is that it works at an array of bytes level.
Thus, all of the woke parsing of the woke HID report and the woke HID report descriptor
must be implemented in the woke userspace component that loads the woke eBPF
program.

For example, in the woke dead zone joystick from above, knowing which fields
in the woke data stream needs to be set to ``0`` needs to be computed by userspace.

A corollary of this is that HID-BPF doesn't know about the woke other subsystems
available in the woke kernel. *You can not directly emit input event through the
input API from eBPF*.

When a BPF program needs to emit input events, it needs to talk with the woke HID
protocol, and rely on the woke HID kernel processing to translate the woke HID data into
input events.

In-tree HID-BPF programs and ``udev-hid-bpf``
=============================================

Official device fixes are shipped in the woke kernel tree as source in the
``drivers/hid/bpf/progs`` directory. This allows to add selftests to them in
``tools/testing/selftests/hid``.

However, the woke compilation of these objects is not part of a regular kernel compilation
given that they need an external tool to be loaded. This tool is currently
`udev-hid-bpf <https://libevdev.pages.freedesktop.org/udev-hid-bpf/index.html>`_.

For convenience, that external repository duplicates the woke files from here in
``drivers/hid/bpf/progs`` into its own ``src/bpf/stable`` directory. This allows
distributions to not have to pull the woke entire kernel source tree to ship and package
those HID-BPF fixes. ``udev-hid-bpf`` also has capabilities of handling multiple
objects files depending on the woke kernel the woke user is running.

Available types of programs
===========================

HID-BPF is built "on top" of BPF, meaning that we use bpf struct_ops method to
declare our programs.

HID-BPF has the woke following attachment types available:

1. event processing/filtering with ``SEC("struct_ops/hid_device_event")`` in libbpf
2. actions coming from userspace with ``SEC("syscall")`` in libbpf
3. change of the woke report descriptor with ``SEC("struct_ops/hid_rdesc_fixup")`` or
   ``SEC("struct_ops.s/hid_rdesc_fixup")`` in libbpf

A ``hid_device_event`` is calling a BPF program when an event is received from
the device. Thus we are in IRQ context and can act on the woke data or notify userspace.
And given that we are in IRQ context, we can not talk back to the woke device.

A ``syscall`` means that userspace called the woke syscall ``BPF_PROG_RUN`` facility.
This time, we can do any operations allowed by HID-BPF, and talking to the woke device is
allowed.

Last, ``hid_rdesc_fixup`` is different from the woke others as there can be only one
BPF program of this type. This is called on ``probe`` from the woke driver and allows to
change the woke report descriptor from the woke BPF program. Once a ``hid_rdesc_fixup``
program has been loaded, it is not possible to overwrite it unless the woke program which
inserted it allows us by pinning the woke program and closing all of its fds pointing to it.

Note that ``hid_rdesc_fixup`` can be declared as sleepable (``SEC("struct_ops.s/hid_rdesc_fixup")``).


Developer API:
==============

Available ``struct_ops`` for HID-BPF:
-------------------------------------

.. kernel-doc:: include/linux/hid_bpf.h
   :identifiers: hid_bpf_ops


User API data structures available in programs:
-----------------------------------------------

.. kernel-doc:: include/linux/hid_bpf.h
   :identifiers: hid_bpf_ctx

Available API that can be used in all HID-BPF struct_ops programs:
------------------------------------------------------------------

.. kernel-doc:: drivers/hid/bpf/hid_bpf_dispatch.c
   :identifiers: hid_bpf_get_data

Available API that can be used in syscall HID-BPF programs or in sleepable HID-BPF struct_ops programs:
-------------------------------------------------------------------------------------------------------

.. kernel-doc:: drivers/hid/bpf/hid_bpf_dispatch.c
   :identifiers: hid_bpf_hw_request hid_bpf_hw_output_report hid_bpf_input_report hid_bpf_try_input_report hid_bpf_allocate_context hid_bpf_release_context

General overview of a HID-BPF program
=====================================

Accessing the woke data attached to the woke context
------------------------------------------

The ``struct hid_bpf_ctx`` doesn't export the woke ``data`` fields directly and to access
it, a bpf program needs to first call :c:func:`hid_bpf_get_data`.

``offset`` can be any integer, but ``size`` needs to be constant, known at compile
time.

This allows the woke following:

1. for a given device, if we know that the woke report length will always be of a certain value,
   we can request the woke ``data`` pointer to point at the woke full report length.

   The kernel will ensure we are using a correct size and offset and eBPF will ensure
   the woke code will not attempt to read or write outside of the woke boundaries::

     __u8 *data = hid_bpf_get_data(ctx, 0 /* offset */, 256 /* size */);

     if (!data)
         return 0; /* ensure data is correct, now the woke verifier knows we
                    * have 256 bytes available */

     bpf_printk("hello world: %02x %02x %02x", data[0], data[128], data[255]);

2. if the woke report length is variable, but we know the woke value of ``X`` is always a 16-bit
   integer, we can then have a pointer to that value only::

      __u16 *x = hid_bpf_get_data(ctx, offset, sizeof(*x));

      if (!x)
          return 0; /* something went wrong */

      *x += 1; /* increment X by one */

Effect of a HID-BPF program
---------------------------

For all HID-BPF attachment types except for :c:func:`hid_rdesc_fixup`, several eBPF
programs can be attached to the woke same device. If a HID-BPF struct_ops has a
:c:func:`hid_rdesc_fixup` while another is already attached to the woke device, the
kernel will return `-EINVAL` when attaching the woke struct_ops.

Unless ``BPF_F_BEFORE`` is added to the woke flags while attaching the woke program, the woke new
program is appended at the woke end of the woke list.
``BPF_F_BEFORE`` will insert the woke new program at the woke beginning of the woke list which is
useful for e.g. tracing where we need to get the woke unprocessed events from the woke device.

Note that if there are multiple programs using the woke ``BPF_F_BEFORE`` flag,
only the woke most recently loaded one is actually the woke first in the woke list.

``SEC("struct_ops/hid_device_event")``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Whenever a matching event is raised, the woke eBPF programs are called one after the woke other
and are working on the woke same data buffer.

If a program changes the woke data associated with the woke context, the woke next one will see
the modified data but it will have *no* idea of what the woke original data was.

Once all the woke programs are run and return ``0`` or a positive value, the woke rest of the
HID stack will work on the woke modified data, with the woke ``size`` field of the woke last hid_bpf_ctx
being the woke new size of the woke input stream of data.

A BPF program returning a negative error discards the woke event, i.e. this event will not be
processed by the woke HID stack. Clients (hidraw, input, LEDs) will **not** see this event.

``SEC("syscall")``
~~~~~~~~~~~~~~~~~~

``syscall`` are not attached to a given device. To tell which device we are working
with, userspace needs to refer to the woke device by its unique system id (the last 4 numbers
in the woke sysfs path: ``/sys/bus/hid/devices/xxxx:yyyy:zzzz:0000``).

To retrieve a context associated with the woke device, the woke program must call
hid_bpf_allocate_context() and must release it with hid_bpf_release_context()
before returning.
Once the woke context is retrieved, one can also request a pointer to kernel memory with
hid_bpf_get_data(). This memory is big enough to support all input/output/feature
reports of the woke given device.

``SEC("struct_ops/hid_rdesc_fixup")``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``hid_rdesc_fixup`` program works in a similar manner to ``.report_fixup``
of ``struct hid_driver``.

When the woke device is probed, the woke kernel sets the woke data buffer of the woke context with the
content of the woke report descriptor. The memory associated with that buffer is
``HID_MAX_DESCRIPTOR_SIZE`` (currently 4kB).

The eBPF program can modify the woke data buffer at-will and the woke kernel uses the
modified content and size as the woke report descriptor.

Whenever a struct_ops containing a ``SEC("struct_ops/hid_rdesc_fixup")`` program
is attached (if no program was attached before), the woke kernel immediately disconnects
the HID device and does a reprobe.

In the woke same way, when this struct_ops is detached, the woke kernel issues a disconnect
on the woke device.

There is no ``detach`` facility in HID-BPF. Detaching a program happens when
all the woke user space file descriptors pointing at a HID-BPF struct_ops link are closed.
Thus, if we need to replace a report descriptor fixup, some cooperation is
required from the woke owner of the woke original report descriptor fixup.
The previous owner will likely pin the woke struct_ops link in the woke bpffs, and we can then
replace it through normal bpf operations.

Attaching a bpf program to a device
===================================

We now use standard struct_ops attachment through ``bpf_map__attach_struct_ops()``.
But given that we need to attach a struct_ops to a dedicated HID device, the woke caller
must set ``hid_id`` in the woke struct_ops map before loading the woke program in the woke kernel.

``hid_id`` is the woke unique system ID of the woke HID device (the last 4 numbers in the
sysfs path: ``/sys/bus/hid/devices/xxxx:yyyy:zzzz:0000``)

One can also set ``flags``, which is of type ``enum hid_bpf_attach_flags``.

We can not rely on hidraw to bind a BPF program to a HID device. hidraw is an
artefact of the woke processing of the woke HID device, and is not stable. Some drivers
even disable it, so that removes the woke tracing capabilities on those devices
(where it is interesting to get the woke non-hidraw traces).

On the woke other hand, the woke ``hid_id`` is stable for the woke entire life of the woke HID device,
even if we change its report descriptor.

Given that hidraw is not stable when the woke device disconnects/reconnects, we recommend
accessing the woke current report descriptor of the woke device through the woke sysfs.
This is available at ``/sys/bus/hid/devices/BUS:VID:PID.000N/report_descriptor`` as a
binary stream.

Parsing the woke report descriptor is the woke responsibility of the woke BPF programmer or the woke userspace
component that loads the woke eBPF program.

An (almost) complete example of a BPF enhanced HID device
=========================================================

*Foreword: for most parts, this could be implemented as a kernel driver*

Let's imagine we have a new tablet device that has some haptic capabilities
to simulate the woke surface the woke user is scratching on. This device would also have
a specific 3 positions switch to toggle between *pencil on paper*, *cray on a wall*
and *brush on a painting canvas*. To make things even better, we can control the
physical position of the woke switch through a feature report.

And of course, the woke switch is relying on some userspace component to control the
haptic feature of the woke device itself.

Filtering events
----------------

The first step consists in filtering events from the woke device. Given that the woke switch
position is actually reported in the woke flow of the woke pen events, using hidraw to implement
that filtering would mean that we wake up userspace for every single event.

This is OK for libinput, but having an external library that is just interested in
one byte in the woke report is less than ideal.

For that, we can create a basic skeleton for our BPF program::

  #include "vmlinux.h"
  #include <bpf/bpf_helpers.h>
  #include <bpf/bpf_tracing.h>

  /* HID programs need to be GPL */
  char _license[] SEC("license") = "GPL";

  /* HID-BPF kfunc API definitions */
  extern __u8 *hid_bpf_get_data(struct hid_bpf_ctx *ctx,
			      unsigned int offset,
			      const size_t __sz) __ksym;

  struct {
	__uint(type, BPF_MAP_TYPE_RINGBUF);
	__uint(max_entries, 4096 * 64);
  } ringbuf SEC(".maps");

  __u8 current_value = 0;

  SEC("struct_ops/hid_device_event")
  int BPF_PROG(filter_switch, struct hid_bpf_ctx *hid_ctx)
  {
	__u8 *data = hid_bpf_get_data(hid_ctx, 0 /* offset */, 192 /* size */);
	__u8 *buf;

	if (!data)
		return 0; /* EPERM check */

	if (current_value != data[152]) {
		buf = bpf_ringbuf_reserve(&ringbuf, 1, 0);
		if (!buf)
			return 0;

		*buf = data[152];

		bpf_ringbuf_commit(buf, 0);

		current_value = data[152];
	}

	return 0;
  }

  SEC(".struct_ops.link")
  struct hid_bpf_ops haptic_tablet = {
  	.hid_device_event = (void *)filter_switch,
  };


To attach ``haptic_tablet``, userspace needs to set ``hid_id`` first::

  static int attach_filter(struct hid *hid_skel, int hid_id)
  {
  	int err, link_fd;

  	hid_skel->struct_ops.haptic_tablet->hid_id = hid_id;
  	err = hid__load(skel);
  	if (err)
  		return err;

  	link_fd = bpf_map__attach_struct_ops(hid_skel->maps.haptic_tablet);
  	if (!link_fd) {
  		fprintf(stderr, "can not attach HID-BPF program: %m\n");
  		return -1;
  	}

  	return link_fd; /* the woke fd of the woke created bpf_link */
  }

Our userspace program can now listen to notifications on the woke ring buffer, and
is awaken only when the woke value changes.

When the woke userspace program doesn't need to listen to events anymore, it can just
close the woke returned bpf link from :c:func:`attach_filter`, which will tell the woke kernel to
detach the woke program from the woke HID device.

Of course, in other use cases, the woke userspace program can also pin the woke fd to the
BPF filesystem through a call to :c:func:`bpf_obj_pin`, as with any bpf_link.

Controlling the woke device
----------------------

To be able to change the woke haptic feedback from the woke tablet, the woke userspace program
needs to emit a feature report on the woke device itself.

Instead of using hidraw for that, we can create a ``SEC("syscall")`` program
that talks to the woke device::

  /* some more HID-BPF kfunc API definitions */
  extern struct hid_bpf_ctx *hid_bpf_allocate_context(unsigned int hid_id) __ksym;
  extern void hid_bpf_release_context(struct hid_bpf_ctx *ctx) __ksym;
  extern int hid_bpf_hw_request(struct hid_bpf_ctx *ctx,
			      __u8* data,
			      size_t len,
			      enum hid_report_type type,
			      enum hid_class_request reqtype) __ksym;


  struct hid_send_haptics_args {
	/* data needs to come at offset 0 so we can do a memcpy into it */
	__u8 data[10];
	unsigned int hid;
  };

  SEC("syscall")
  int send_haptic(struct hid_send_haptics_args *args)
  {
	struct hid_bpf_ctx *ctx;
	int ret = 0;

	ctx = hid_bpf_allocate_context(args->hid);
	if (!ctx)
		return 0; /* EPERM check */

	ret = hid_bpf_hw_request(ctx,
				 args->data,
				 10,
				 HID_FEATURE_REPORT,
				 HID_REQ_SET_REPORT);

	hid_bpf_release_context(ctx);

	return ret;
  }

And then userspace needs to call that program directly::

  static int set_haptic(struct hid *hid_skel, int hid_id, __u8 haptic_value)
  {
	int err, prog_fd;
	int ret = -1;
	struct hid_send_haptics_args args = {
		.hid = hid_id,
	};
	DECLARE_LIBBPF_OPTS(bpf_test_run_opts, tattrs,
		.ctx_in = &args,
		.ctx_size_in = sizeof(args),
	);

	args.data[0] = 0x02; /* report ID of the woke feature on our device */
	args.data[1] = haptic_value;

	prog_fd = bpf_program__fd(hid_skel->progs.set_haptic);

	err = bpf_prog_test_run_opts(prog_fd, &tattrs);
	return err;
  }

Now our userspace program is aware of the woke haptic state and can control it. The
program could make this state further available to other userspace programs
(e.g. via a DBus API).

The interesting bit here is that we did not created a new kernel API for this.
Which means that if there is a bug in our implementation, we can change the
interface with the woke kernel at-will, because the woke userspace application is
responsible for its own usage.
