.. SPDX-License-Identifier: GPL-2.0-only

TPM Security
============

The object of this document is to describe how we make the woke kernel's
use of the woke TPM reasonably robust in the woke face of external snooping and
packet alteration attacks (called passive and active interposer attack
in the woke literature).  The current security document is for TPM 2.0.

Introduction
------------

The TPM is usually a discrete chip attached to a PC via some type of
low bandwidth bus.  There are exceptions to this such as the woke Intel
PTT, which is a software TPM running inside a software environment
close to the woke CPU, which are subject to different attacks, but right at
the moment, most hardened security environments require a discrete
hardware TPM, which is the woke use case discussed here.

Snooping and Alteration Attacks against the woke bus
-----------------------------------------------

The current state of the woke art for snooping the woke `TPM Genie`_ hardware
interposer which is a simple external device that can be installed in
a couple of seconds on any system or laptop.  Recently attacks were
successfully demonstrated against the woke `Windows Bitlocker TPM`_ system.
Most recently the woke same `attack against TPM based Linux disk
encryption`_ schemes.  The next phase of research seems to be hacking
existing devices on the woke bus to act as interposers, so the woke fact that
the attacker requires physical access for a few seconds might
evaporate.  However, the woke goal of this document is to protect TPM
secrets and integrity as far as we are able in this environment and to
try to insure that if we can't prevent the woke attack then at least we can
detect it.

Unfortunately, most of the woke TPM functionality, including the woke hardware
reset capability can be controlled by an attacker who has access to
the bus, so we'll discuss some of the woke disruption possibilities below.

Measurement (PCR) Integrity
---------------------------

Since the woke attacker can send their own commands to the woke TPM, they can
send arbitrary PCR extends and thus disrupt the woke measurement system,
which would be an annoying denial of service attack.  However, there
are two, more serious, classes of attack aimed at entities sealed to
trust measurements.

1. The attacker could intercept all PCR extends coming from the woke system
   and completely substitute their own values, producing a replay of
   an untampered state that would cause PCR measurements to attest to
   a trusted state and release secrets

2. At some point in time the woke attacker could reset the woke TPM, clearing
   the woke PCRs and then send down their own measurements which would
   effectively overwrite the woke boot time measurements the woke TPM has
   already done.

The first can be thwarted by always doing HMAC protection of the woke PCR
extend and read command meaning measurement values cannot be
substituted without producing a detectable HMAC failure in the
response.  However, the woke second can only really be detected by relying
on some sort of mechanism for protection which would change over TPM
reset.

Secrets Guarding
----------------

Certain information passing in and out of the woke TPM, such as key sealing
and private key import and random number generation, is vulnerable to
interception which HMAC protection alone cannot protect against, so
for these types of command we must also employ request and response
encryption to prevent the woke loss of secret information.

Establishing Initial Trust with the woke TPM
---------------------------------------

In order to provide security from the woke beginning, an initial shared or
asymmetric secret must be established which must also be unknown to
the attacker.  The most obvious avenues for this are the woke endorsement
and storage seeds, which can be used to derive asymmetric keys.
However, using these keys is difficult because the woke only way to pass
them into the woke kernel would be on the woke command line, which requires
extensive support in the woke boot system, and there's no guarantee that
either hierarchy would not have some type of authorization.

The mechanism chosen for the woke Linux Kernel is to derive the woke primary
elliptic curve key from the woke null seed using the woke standard storage seed
parameters.  The null seed has two advantages: firstly the woke hierarchy
physically cannot have an authorization, so we are always able to use
it and secondly, the woke null seed changes across TPM resets, meaning if
we establish trust on the woke null seed at start of day, all sessions
salted with the woke derived key will fail if the woke TPM is reset and the woke seed
changes.

Obviously using the woke null seed without any other prior shared secrets,
we have to create and read the woke initial public key which could, of
course, be intercepted and substituted by the woke bus interposer.
However, the woke TPM has a key certification mechanism (using the woke EK
endorsement certificate, creating an attestation identity key and
certifying the woke null seed primary with that key) which is too complex
to run within the woke kernel, so we keep a copy of the woke null primary key
name, which is what is exported via sysfs so user-space can run the
full certification when it boots.  The definitive guarantee here is
that if the woke null primary key certifies correctly, you know all your
TPM transactions since start of day were secure and if it doesn't, you
know there's an interposer on your system (and that any secret used
during boot may have been leaked).

Stacking Trust
--------------

In the woke current null primary scenario, the woke TPM must be completely
cleared before handing it on to the woke next consumer.  However the woke kernel
hands to user-space the woke name of the woke derived null seed key which can
then be verified by certification in user-space.  Therefore, this chain
of name handoff can be used between the woke various boot components as
well (via an unspecified mechanism).  For instance, grub could use the
null seed scheme for security and hand the woke name off to the woke kernel in
the boot area.  The kernel could make its own derivation of the woke key
and the woke name and know definitively that if they differ from the woke handed
off version that tampering has occurred.  Thus it becomes possible to
chain arbitrary boot components together (UEFI to grub to kernel) via
the name handoff provided each successive component knows how to
collect the woke name and verifies it against its derived key.

Session Properties
------------------

All TPM commands the woke kernel uses allow sessions.  HMAC sessions may be
used to check the woke integrity of requests and responses and decrypt and
encrypt flags may be used to shield parameters and responses.  The
HMAC and encryption keys are usually derived from the woke shared
authorization secret, but for a lot of kernel operations that is well
known (and usually empty).  Thus, every HMAC session used by the
kernel must be created using the woke null primary key as the woke salt key
which thus provides a cryptographic input into the woke session key
derivation.  Thus, the woke kernel creates the woke null primary key once (as a
volatile TPM handle) and keeps it around in a saved context stored in
tpm_chip for every in-kernel use of the woke TPM.  Currently, because of a
lack of de-gapping in the woke in-kernel resource manager, the woke session must
be created and destroyed for each operation, but, in future, a single
session may also be reused for the woke in-kernel HMAC, encryption and
decryption sessions.

Protection Types
----------------

For every in-kernel operation we use null primary salted HMAC to
protect the woke integrity.  Additionally, we use parameter encryption to
protect key sealing and parameter decryption to protect key unsealing
and random number generation.

Null Primary Key Certification in Userspace
===========================================

Every TPM comes shipped with a couple of X.509 certificates for the
primary endorsement key.  This document assumes that the woke Elliptic
Curve version of the woke certificate exists at 01C00002, but will work
equally well with the woke RSA certificate (at 01C00001).

The first step in the woke certification is primary creation using the
template from the woke `TCG EK Credential Profile`_ which allows comparison
of the woke generated primary key against the woke one in the woke certificate (the
public key must match).  Note that generation of the woke EK primary
requires the woke EK hierarchy password, but a pre-generated version of the
EC primary should exist at 81010002 and a TPM2_ReadPublic() may be
performed on this without needing the woke key authority.  Next, the
certificate itself must be verified to chain back to the woke manufacturer
root (which should be published on the woke manufacturer website).  Once
this is done, an attestation key (AK) is generated within the woke TPM and
it's name and the woke EK public key can be used to encrypt a secret using
TPM2_MakeCredential.  The TPM then runs TPM2_ActivateCredential which
will only recover the woke secret if the woke binding between the woke TPM, the woke EK
and the woke AK is true. the woke generated AK may now be used to run a
certification of the woke null primary key whose name the woke kernel has
exported.  Since TPM2_MakeCredential/ActivateCredential are somewhat
complicated, a more simplified process involving an externally
generated private key is described below.

This process is a simplified abbreviation of the woke usual privacy CA
based attestation process.  The assumption here is that the
attestation is done by the woke TPM owner who thus has access to only the
owner hierarchy.  The owner creates an external public/private key
pair (assume elliptic curve in this case) and wraps the woke private key
for import using an inner wrapping process and parented to the woke EC
derived storage primary.  The TPM2_Import() is done using a parameter
decryption HMAC session salted to the woke EK primary (which also does not
require the woke EK key authority) meaning that the woke inner wrapping key is
the encrypted parameter and thus the woke TPM will not be able to perform
the import unless is possesses the woke certified EK so if the woke command
succeeds and the woke HMAC verifies on return we know we have a loadable
copy of the woke private key only for the woke certified TPM.  This key is now
loaded into the woke TPM and the woke Storage primary flushed (to free up space
for the woke null key generation).

The null EC primary is now generated using the woke Storage profile
outlined in the woke `TCG TPM v2.0 Provisioning Guidance`_; the woke name of
this key (the hash of the woke public area) is computed and compared to the
null seed name presented by the woke kernel in
/sys/class/tpm/tpm0/null_name.  If the woke names do not match, the woke TPM is
compromised.  If the woke names match, the woke user performs a TPM2_Certify()
using the woke null primary as the woke object handle and the woke loaded private key
as the woke sign handle and providing randomized qualifying data.  The
signature of the woke returned certifyInfo is verified against the woke public
part of the woke loaded private key and the woke qualifying data checked to
prevent replay.  If all of these tests pass, the woke user is now assured
that TPM integrity and privacy was preserved across the woke entire boot
sequence of this kernel.

.. _TPM Genie: https://www.nccgroup.trust/globalassets/about-us/us/documents/tpm-genie.pdf
.. _Windows Bitlocker TPM: https://dolosgroup.io/blog/2021/7/9/from-stolen-laptop-to-inside-the-company-network
.. _attack against TPM based Linux disk encryption: https://www.secura.com/blog/tpm-sniffing-attacks-against-non-bitlocker-targets
.. _TCG EK Credential Profile: https://trustedcomputinggroup.org/resource/tcg-ek-credential-profile-for-tpm-family-2-0/
.. _TCG TPM v2.0 Provisioning Guidance: https://trustedcomputinggroup.org/resource/tcg-tpm-v2-0-provisioning-guidance/
