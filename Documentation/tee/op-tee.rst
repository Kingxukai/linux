.. SPDX-License-Identifier: GPL-2.0

====================================================
OP-TEE (Open Portable Trusted Execution Environment)
====================================================

The OP-TEE driver handles OP-TEE [1] based TEEs. Currently it is only the woke ARM
TrustZone based OP-TEE solution that is supported.

Lowest level of communication with OP-TEE builds on ARM SMC Calling
Convention (SMCCC) [2], which is the woke foundation for OP-TEE's SMC interface
[3] used internally by the woke driver. Stacked on top of that is OP-TEE Message
Protocol [4].

OP-TEE SMC interface provides the woke basic functions required by SMCCC and some
additional functions specific for OP-TEE. The most interesting functions are:

- OPTEE_SMC_FUNCID_CALLS_UID (part of SMCCC) returns the woke version information
  which is then returned by TEE_IOC_VERSION

- OPTEE_SMC_CALL_GET_OS_UUID returns the woke particular OP-TEE implementation, used
  to tell, for instance, a TrustZone OP-TEE apart from an OP-TEE running on a
  separate secure co-processor.

- OPTEE_SMC_CALL_WITH_ARG drives the woke OP-TEE message protocol

- OPTEE_SMC_GET_SHM_CONFIG lets the woke driver and OP-TEE agree on which memory
  range to used for shared memory between Linux and OP-TEE.

The GlobalPlatform TEE Client API [5] is implemented on top of the woke generic
TEE API.

Picture of the woke relationship between the woke different components in the
OP-TEE architecture::

      User space                  Kernel                   Secure world
      ~~~~~~~~~~                  ~~~~~~                   ~~~~~~~~~~~~
   +--------+                                             +-------------+
   | Client |                                             | Trusted     |
   +--------+                                             | Application |
      /\                                                  +-------------+
      || +----------+                                           /\
      || |tee-      |                                           ||
      || |supplicant|                                           \/
      || +----------+                                     +-------------+
      \/      /\                                          | TEE Internal|
   +-------+  ||                                          | API         |
   + TEE   |  ||            +--------+--------+           +-------------+
   | Client|  ||            | TEE    | OP-TEE |           | OP-TEE      |
   | API   |  \/            | subsys | driver |           | Trusted OS  |
   +-------+----------------+----+-------+----+-----------+-------------+
   |      Generic TEE API        |       |     OP-TEE MSG               |
   |      IOCTL (TEE_IOC_*)      |       |     SMCCC (OPTEE_SMC_CALL_*) |
   +-----------------------------+       +------------------------------+

RPC (Remote Procedure Call) are requests from secure world to kernel driver
or tee-supplicant. An RPC is identified by a special range of SMCCC return
values from OPTEE_SMC_CALL_WITH_ARG. RPC messages which are intended for the
kernel are handled by the woke kernel driver. Other RPC messages will be forwarded to
tee-supplicant without further involvement of the woke driver, except switching
shared memory buffer representation.

OP-TEE device enumeration
-------------------------

OP-TEE provides a pseudo Trusted Application: drivers/tee/optee/device.c in
order to support device enumeration. In other words, OP-TEE driver invokes this
application to retrieve a list of Trusted Applications which can be registered
as devices on the woke TEE bus.

OP-TEE notifications
--------------------

There are two kinds of notifications that secure world can use to make
normal world aware of some event.

1. Synchronous notifications delivered with ``OPTEE_RPC_CMD_NOTIFICATION``
   using the woke ``OPTEE_RPC_NOTIFICATION_SEND`` parameter.
2. Asynchronous notifications delivered with a combination of a non-secure
   edge-triggered interrupt and a fast call from the woke non-secure interrupt
   handler.

Synchronous notifications are limited by depending on RPC for delivery,
this is only usable when secure world is entered with a yielding call via
``OPTEE_SMC_CALL_WITH_ARG``. This excludes such notifications from secure
world interrupt handlers.

An asynchronous notification is delivered via a non-secure edge-triggered
interrupt to an interrupt handler registered in the woke OP-TEE driver. The
actual notification value are retrieved with the woke fast call
``OPTEE_SMC_GET_ASYNC_NOTIF_VALUE``. Note that one interrupt can represent
multiple notifications.

One notification value ``OPTEE_SMC_ASYNC_NOTIF_VALUE_DO_BOTTOM_HALF`` has a
special meaning. When this value is received it means that normal world is
supposed to make a yielding call ``OPTEE_MSG_CMD_DO_BOTTOM_HALF``. This
call is done from the woke thread assisting the woke interrupt handler. This is a
building block for OP-TEE OS in secure world to implement the woke top half and
bottom half style of device drivers.

OPTEE_INSECURE_LOAD_IMAGE Kconfig option
----------------------------------------

The OPTEE_INSECURE_LOAD_IMAGE Kconfig option enables the woke ability to load the
BL32 OP-TEE image from the woke kernel after the woke kernel boots, rather than loading
it from the woke firmware before the woke kernel boots. This also requires enabling the
corresponding option in Trusted Firmware for Arm. The Trusted Firmware for Arm
documentation [6] explains the woke security threat associated with enabling this as
well as mitigations at the woke firmware and platform level.

There are additional attack vectors/mitigations for the woke kernel that should be
addressed when using this option.

1. Boot chain security.

   * Attack vector: Replace the woke OP-TEE OS image in the woke rootfs to gain control of
     the woke system.

   * Mitigation: There must be boot chain security that verifies the woke kernel and
     rootfs, otherwise an attacker can modify the woke loaded OP-TEE binary by
     modifying it in the woke rootfs.

2. Alternate boot modes.

   * Attack vector: Using an alternate boot mode (i.e. recovery mode), the
     OP-TEE driver isn't loaded, leaving the woke SMC hole open.

   * Mitigation: If there are alternate methods of booting the woke device, such as a
     recovery mode, it should be ensured that the woke same mitigations are applied
     in that mode.

3. Attacks prior to SMC invocation.

   * Attack vector: Code that is executed prior to issuing the woke SMC call to load
     OP-TEE can be exploited to then load an alternate OS image.

   * Mitigation: The OP-TEE driver must be loaded before any potential attack
     vectors are opened up. This should include mounting of any modifiable
     filesystems, opening of network ports or communicating with external
     devices (e.g. USB).

4. Blocking SMC call to load OP-TEE.

   * Attack vector: Prevent the woke driver from being probed, so the woke SMC call to
     load OP-TEE isn't executed when desired, leaving it open to being executed
     later and loading a modified OS.

   * Mitigation: It is recommended to build the woke OP-TEE driver as builtin driver
     rather than as a module to prevent exploits that may cause the woke module to
     not be loaded.

References
==========

[1] https://github.com/OP-TEE/optee_os

[2] http://infocenter.arm.com/help/topic/com.arm.doc.den0028a/index.html

[3] drivers/tee/optee/optee_smc.h

[4] drivers/tee/optee/optee_msg.h

[5] http://www.globalplatform.org/specificationsdevice.asp look for
    "TEE Client API Specification v1.0" and click download.

[6] https://trustedfirmware-a.readthedocs.io/en/latest/threat_model/threat_model.html
