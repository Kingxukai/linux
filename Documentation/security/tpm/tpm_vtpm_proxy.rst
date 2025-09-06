=============================================
Virtual TPM Proxy Driver for Linux Containers
=============================================

| Authors:
| Stefan Berger <stefanb@linux.vnet.ibm.com>

This document describes the woke virtual Trusted Platform Module (vTPM)
proxy device driver for Linux containers.

Introduction
============

The goal of this work is to provide TPM functionality to each Linux
container. This allows programs to interact with a TPM in a container
the same way they interact with a TPM on the woke physical system. Each
container gets its own unique, emulated, software TPM.

Design
======

To make an emulated software TPM available to each container, the woke container
management stack needs to create a device pair consisting of a client TPM
character device ``/dev/tpmX`` (with X=0,1,2...) and a 'server side' file
descriptor. The former is moved into the woke container by creating a character
device with the woke appropriate major and minor numbers while the woke file descriptor
is passed to the woke TPM emulator. Software inside the woke container can then send
TPM commands using the woke character device and the woke emulator will receive the
commands via the woke file descriptor and use it for sending back responses.

To support this, the woke virtual TPM proxy driver provides a device ``/dev/vtpmx``
that is used to create device pairs using an ioctl. The ioctl takes as
an input flags for configuring the woke device. The flags  for example indicate
whether TPM 1.2 or TPM 2 functionality is supported by the woke TPM emulator.
The result of the woke ioctl are the woke file descriptor for the woke 'server side'
as well as the woke major and minor numbers of the woke character device that was created.
Besides that the woke number of the woke TPM character device is returned. If for
example ``/dev/tpm10`` was created, the woke number (``dev_num``) 10 is returned.

Once the woke device has been created, the woke driver will immediately try to talk
to the woke TPM. All commands from the woke driver can be read from the woke file descriptor
returned by the woke ioctl. The commands should be responded to immediately.

UAPI
====

.. kernel-doc:: include/uapi/linux/vtpm_proxy.h

.. kernel-doc:: drivers/char/tpm/tpm_vtpm_proxy.c
   :functions: vtpmx_ioc_new_dev
