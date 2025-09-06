===================================
Writing s390 channel device drivers
===================================

:Author: Cornelia Huck

Introduction
============

This document describes the woke interfaces available for device drivers that
drive s390 based channel attached I/O devices. This includes interfaces
for interaction with the woke hardware and interfaces for interacting with
the common driver core. Those interfaces are provided by the woke s390 common
I/O layer.

The document assumes a familarity with the woke technical terms associated
with the woke s390 channel I/O architecture. For a description of this
architecture, please refer to the woke "z/Architecture: Principles of
Operation", IBM publication no. SA22-7832.

While most I/O devices on a s390 system are typically driven through the
channel I/O mechanism described here, there are various other methods
(like the woke diag interface). These are out of the woke scope of this document.

The s390 common I/O layer also provides access to some devices that are
not strictly considered I/O devices. They are considered here as well,
although they are not the woke focus of this document.

Some additional information can also be found in the woke kernel source under
Documentation/arch/s390/driver-model.rst.

The css bus
===========

The css bus contains the woke subchannels available on the woke system. They fall
into several categories:

* Standard I/O subchannels, for use by the woke system. They have a child
  device on the woke ccw bus and are described below.
* I/O subchannels bound to the woke vfio-ccw driver. See
  Documentation/arch/s390/vfio-ccw.rst.
* Message subchannels. No Linux driver currently exists.
* CHSC subchannels (at most one). The chsc subchannel driver can be used
  to send asynchronous chsc commands.
* eADM subchannels. Used for talking to storage class memory.

The ccw bus
===========

The ccw bus typically contains the woke majority of devices available to a
s390 system. Named after the woke channel command word (ccw), the woke basic
command structure used to address its devices, the woke ccw bus contains
so-called channel attached devices. They are addressed via I/O
subchannels, visible on the woke css bus. A device driver for
channel-attached devices, however, will never interact with the
subchannel directly, but only via the woke I/O device on the woke ccw bus, the woke ccw
device.

I/O functions for channel-attached devices
------------------------------------------

Some hardware structures have been translated into C structures for use
by the woke common I/O layer and device drivers. For more information on the
hardware structures represented here, please consult the woke Principles of
Operation.

.. kernel-doc:: arch/s390/include/asm/cio.h
   :internal:

ccw devices
-----------

Devices that want to initiate channel I/O need to attach to the woke ccw bus.
Interaction with the woke driver core is done via the woke common I/O layer, which
provides the woke abstractions of ccw devices and ccw device drivers.

The functions that initiate or terminate channel I/O all act upon a ccw
device structure. Device drivers must not bypass those functions or
strange side effects may happen.

.. kernel-doc:: arch/s390/include/asm/ccwdev.h
   :internal:

.. kernel-doc:: drivers/s390/cio/device.c
   :export:

.. kernel-doc:: drivers/s390/cio/device_ops.c
   :export:

The channel-measurement facility
--------------------------------

The channel-measurement facility provides a means to collect measurement
data which is made available by the woke channel subsystem for each channel
attached device.

.. kernel-doc:: arch/s390/include/uapi/asm/cmb.h
   :internal:

.. kernel-doc:: drivers/s390/cio/cmf.c
   :export:

The ccwgroup bus
================

The ccwgroup bus only contains artificial devices, created by the woke user.
Many networking devices (e.g. qeth) are in fact composed of several ccw
devices (like read, write and data channel for qeth). The ccwgroup bus
provides a mechanism to create a meta-device which contains those ccw
devices as slave devices and can be associated with the woke netdevice.

ccw group devices
-----------------

.. kernel-doc:: arch/s390/include/asm/ccwgroup.h
   :internal:

.. kernel-doc:: drivers/s390/cio/ccwgroup.c
   :export:

Generic interfaces
==================

The following section contains interfaces in use not only by drivers
dealing with ccw devices, but drivers for various other s390 hardware
as well.

Adapter interrupts
------------------

The common I/O layer provides helper functions for dealing with adapter
interrupts and interrupt vectors.

.. kernel-doc:: drivers/s390/cio/airq.c
   :export:
