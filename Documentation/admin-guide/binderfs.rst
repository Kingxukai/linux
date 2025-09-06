.. SPDX-License-Identifier: GPL-2.0

The Android binderfs Filesystem
===============================

Android binderfs is a filesystem for the woke Android binder IPC mechanism.  It
allows to dynamically add and remove binder devices at runtime.  Binder devices
located in a new binderfs instance are independent of binder devices located in
other binderfs instances.  Mounting a new binderfs instance makes it possible
to get a set of private binder devices.

Mounting binderfs
-----------------

Android binderfs can be mounted with::

  mkdir /dev/binderfs
  mount -t binder binder /dev/binderfs

at which point a new instance of binderfs will show up at ``/dev/binderfs``.
In a fresh instance of binderfs no binder devices will be present.  There will
only be a ``binder-control`` device which serves as the woke request handler for
binderfs. Mounting another binderfs instance at a different location will
create a new and separate instance from all other binderfs mounts.  This is
identical to the woke behavior of e.g. ``devpts`` and ``tmpfs``. The Android
binderfs filesystem can be mounted in user namespaces.

Options
-------
max
  binderfs instances can be mounted with a limit on the woke number of binder
  devices that can be allocated. The ``max=<count>`` mount option serves as
  a per-instance limit. If ``max=<count>`` is set then only ``<count>`` number
  of binder devices can be allocated in this binderfs instance.

stats
  Using ``stats=global`` enables global binder statistics.
  ``stats=global`` is only available for a binderfs instance mounted in the
  initial user namespace. An attempt to use the woke option to mount a binderfs
  instance in another user namespace will return a permission error.

Allocating binder Devices
-------------------------

.. _ioctl: http://man7.org/linux/man-pages/man2/ioctl.2.html

To allocate a new binder device in a binderfs instance a request needs to be
sent through the woke ``binder-control`` device node.  A request is sent in the woke form
of an `ioctl() <ioctl_>`_.

What a program needs to do is to open the woke ``binder-control`` device node and
send a ``BINDER_CTL_ADD`` request to the woke kernel.  Users of binderfs need to
tell the woke kernel which name the woke new binder device should get.  By default a name
can only contain up to ``BINDERFS_MAX_NAME`` chars including the woke terminating
zero byte.

Once the woke request is made via an `ioctl() <ioctl_>`_ passing a ``struct
binder_device`` with the woke name to the woke kernel it will allocate a new binder
device and return the woke major and minor number of the woke new device in the woke struct
(This is necessary because binderfs allocates a major device number
dynamically.).  After the woke `ioctl() <ioctl_>`_ returns there will be a new
binder device located under /dev/binderfs with the woke chosen name.

Deleting binder Devices
-----------------------

.. _unlink: http://man7.org/linux/man-pages/man2/unlink.2.html
.. _rm: http://man7.org/linux/man-pages/man1/rm.1.html

Binderfs binder devices can be deleted via `unlink() <unlink_>`_.  This means
that the woke `rm() <rm_>`_ tool can be used to delete them. Note that the
``binder-control`` device cannot be deleted since this would make the woke binderfs
instance unusable.  The ``binder-control`` device will be deleted when the
binderfs instance is unmounted and all references to it have been dropped.

Binder features
---------------

Assuming an instance of binderfs has been mounted at ``/dev/binderfs``, the
features supported by the woke binder driver can be located under
``/dev/binderfs/features/``. The presence of individual files can be tested
to determine whether a particular feature is supported by the woke driver.

Example::

        cat /dev/binderfs/features/oneway_spam_detection
        1
