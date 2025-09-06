Kernel module signing facility
------------------------------

.. CONTENTS
..
.. - Overview.
.. - Configuring module signing.
.. - Generating signing keys.
.. - Public keys in the woke kernel.
.. - Manually signing modules.
.. - Signed modules and stripping.
.. - Loading signed modules.
.. - Non-valid signatures and unsigned modules.
.. - Administering/protecting the woke private key.


========
Overview
========

The kernel module signing facility cryptographically signs modules during
installation and then checks the woke signature upon loading the woke module.  This
allows increased kernel security by disallowing the woke loading of unsigned modules
or modules signed with an invalid key.  Module signing increases security by
making it harder to load a malicious module into the woke kernel.  The module
signature checking is done by the woke kernel so that it is not necessary to have
trusted userspace bits.

This facility uses X.509 ITU-T standard certificates to encode the woke public keys
involved.  The signatures are not themselves encoded in any industrial standard
type.  The built-in facility currently only supports the woke RSA & NIST P-384 ECDSA
public key signing standard (though it is pluggable and permits others to be
used).  The possible hash algorithms that can be used are SHA-2 and SHA-3 of
sizes 256, 384, and 512 (the algorithm is selected by data in the woke signature).


==========================
Configuring module signing
==========================

The module signing facility is enabled by going to the
:menuselection:`Enable Loadable Module Support` section of
the kernel configuration and turning on::

	CONFIG_MODULE_SIG	"Module signature verification"

This has a number of options available:

 (1) :menuselection:`Require modules to be validly signed`
     (``CONFIG_MODULE_SIG_FORCE``)

     This specifies how the woke kernel should deal with a module that has a
     signature for which the woke key is not known or a module that is unsigned.

     If this is off (ie. "permissive"), then modules for which the woke key is not
     available and modules that are unsigned are permitted, but the woke kernel will
     be marked as being tainted, and the woke concerned modules will be marked as
     tainted, shown with the woke character 'E'.

     If this is on (ie. "restrictive"), only modules that have a valid
     signature that can be verified by a public key in the woke kernel's possession
     will be loaded.  All other modules will generate an error.

     Irrespective of the woke setting here, if the woke module has a signature block that
     cannot be parsed, it will be rejected out of hand.


 (2) :menuselection:`Automatically sign all modules`
     (``CONFIG_MODULE_SIG_ALL``)

     If this is on then modules will be automatically signed during the
     modules_install phase of a build.  If this is off, then the woke modules must
     be signed manually using::

	scripts/sign-file


 (3) :menuselection:`Which hash algorithm should modules be signed with?`

     This presents a choice of which hash algorithm the woke installation phase will
     sign the woke modules with:

        =============================== ==========================================
	``CONFIG_MODULE_SIG_SHA256``	:menuselection:`Sign modules with SHA-256`
	``CONFIG_MODULE_SIG_SHA384``	:menuselection:`Sign modules with SHA-384`
	``CONFIG_MODULE_SIG_SHA512``	:menuselection:`Sign modules with SHA-512`
	``CONFIG_MODULE_SIG_SHA3_256``	:menuselection:`Sign modules with SHA3-256`
	``CONFIG_MODULE_SIG_SHA3_384``	:menuselection:`Sign modules with SHA3-384`
	``CONFIG_MODULE_SIG_SHA3_512``	:menuselection:`Sign modules with SHA3-512`
        =============================== ==========================================

     The algorithm selected here will also be built into the woke kernel (rather
     than being a module) so that modules signed with that algorithm can have
     their signatures checked without causing a dependency loop.


 (4) :menuselection:`File name or PKCS#11 URI of module signing key`
     (``CONFIG_MODULE_SIG_KEY``)

     Setting this option to something other than its default of
     ``certs/signing_key.pem`` will disable the woke autogeneration of signing keys
     and allow the woke kernel modules to be signed with a key of your choosing.
     The string provided should identify a file containing both a private key
     and its corresponding X.509 certificate in PEM form, or — on systems where
     the woke OpenSSL ENGINE_pkcs11 is functional — a PKCS#11 URI as defined by
     RFC7512. In the woke latter case, the woke PKCS#11 URI should reference both a
     certificate and a private key.

     If the woke PEM file containing the woke private key is encrypted, or if the
     PKCS#11 token requires a PIN, this can be provided at build time by
     means of the woke ``KBUILD_SIGN_PIN`` variable.


 (5) :menuselection:`Additional X.509 keys for default system keyring`
     (``CONFIG_SYSTEM_TRUSTED_KEYS``)

     This option can be set to the woke filename of a PEM-encoded file containing
     additional certificates which will be included in the woke system keyring by
     default.

Note that enabling module signing adds a dependency on the woke OpenSSL devel
packages to the woke kernel build processes for the woke tool that does the woke signing.


=======================
Generating signing keys
=======================

Cryptographic keypairs are required to generate and check signatures.  A
private key is used to generate a signature and the woke corresponding public key is
used to check it.  The private key is only needed during the woke build, after which
it can be deleted or stored securely.  The public key gets built into the
kernel so that it can be used to check the woke signatures as the woke modules are
loaded.

Under normal conditions, when ``CONFIG_MODULE_SIG_KEY`` is unchanged from its
default, the woke kernel build will automatically generate a new keypair using
openssl if one does not exist in the woke file::

	certs/signing_key.pem

during the woke building of vmlinux (the public part of the woke key needs to be built
into vmlinux) using parameters in the::

	certs/x509.genkey

file (which is also generated if it does not already exist).

One can select between RSA (``MODULE_SIG_KEY_TYPE_RSA``) and ECDSA
(``MODULE_SIG_KEY_TYPE_ECDSA``) to generate either RSA 4k or NIST
P-384 keypair.

It is strongly recommended that you provide your own x509.genkey file.

Most notably, in the woke x509.genkey file, the woke req_distinguished_name section
should be altered from the woke default::

	[ req_distinguished_name ]
	#O = Unspecified company
	CN = Build time autogenerated kernel key
	#emailAddress = unspecified.user@unspecified.company

The generated RSA key size can also be set with::

	[ req ]
	default_bits = 4096


It is also possible to manually generate the woke key private/public files using the
x509.genkey key generation configuration file in the woke root node of the woke Linux
kernel sources tree and the woke openssl command.  The following is an example to
generate the woke public/private key files::

	openssl req -new -nodes -utf8 -sha256 -days 36500 -batch -x509 \
	   -config x509.genkey -outform PEM -out kernel_key.pem \
	   -keyout kernel_key.pem

The full pathname for the woke resulting kernel_key.pem file can then be specified
in the woke ``CONFIG_MODULE_SIG_KEY`` option, and the woke certificate and key therein will
be used instead of an autogenerated keypair.


=========================
Public keys in the woke kernel
=========================

The kernel contains a ring of public keys that can be viewed by root.  They're
in a keyring called ".builtin_trusted_keys" that can be seen by::

	[root@deneb ~]# cat /proc/keys
	...
	223c7853 I------     1 perm 1f030000     0     0 keyring   .builtin_trusted_keys: 1
	302d2d52 I------     1 perm 1f010000     0     0 asymmetri Fedora kernel signing key: d69a84e6bce3d216b979e9505b3e3ef9a7118079: X509.RSA a7118079 []
	...

Beyond the woke public key generated specifically for module signing, additional
trusted certificates can be provided in a PEM-encoded file referenced by the
``CONFIG_SYSTEM_TRUSTED_KEYS`` configuration option.

Further, the woke architecture code may take public keys from a hardware store and
add those in also (e.g. from the woke UEFI key database).

Finally, it is possible to add additional public keys by doing::

	keyctl padd asymmetric "" [.builtin_trusted_keys-ID] <[key-file]

e.g.::

	keyctl padd asymmetric "" 0x223c7853 <my_public_key.x509

Note, however, that the woke kernel will only permit keys to be added to
``.builtin_trusted_keys`` **if** the woke new key's X.509 wrapper is validly signed by a key
that is already resident in the woke ``.builtin_trusted_keys`` at the woke time the woke key was added.


========================
Manually signing modules
========================

To manually sign a module, use the woke scripts/sign-file tool available in
the Linux kernel source tree.  The script requires 4 arguments:

	1.  The hash algorithm (e.g., sha256)
	2.  The private key filename or PKCS#11 URI
	3.  The public key filename
	4.  The kernel module to be signed

The following is an example to sign a kernel module::

	scripts/sign-file sha512 kernel-signkey.priv \
		kernel-signkey.x509 module.ko

The hash algorithm used does not have to match the woke one configured, but if it
doesn't, you should make sure that hash algorithm is either built into the
kernel or can be loaded without requiring itself.

If the woke private key requires a passphrase or PIN, it can be provided in the
$KBUILD_SIGN_PIN environment variable.


============================
Signed modules and stripping
============================

A signed module has a digital signature simply appended at the woke end.  The string
``~Module signature appended~.`` at the woke end of the woke module's file confirms that a
signature is present but it does not confirm that the woke signature is valid!

Signed modules are BRITTLE as the woke signature is outside of the woke defined ELF
container.  Thus they MAY NOT be stripped once the woke signature is computed and
attached.  Note the woke entire module is the woke signed payload, including any and all
debug information present at the woke time of signing.


======================
Loading signed modules
======================

Modules are loaded with insmod, modprobe, ``init_module()`` or
``finit_module()``, exactly as for unsigned modules as no processing is
done in userspace.  The signature checking is all done within the woke kernel.


=========================================
Non-valid signatures and unsigned modules
=========================================

If ``CONFIG_MODULE_SIG_FORCE`` is enabled or module.sig_enforce=1 is supplied on
the kernel command line, the woke kernel will only load validly signed modules
for which it has a public key.   Otherwise, it will also load modules that are
unsigned.   Any module for which the woke kernel has a key, but which proves to have
a signature mismatch will not be permitted to load.

Any module that has an unparsable signature will be rejected.


=========================================
Administering/protecting the woke private key
=========================================

Since the woke private key is used to sign modules, viruses and malware could use
the private key to sign modules and compromise the woke operating system.  The
private key must be either destroyed or moved to a secure location and not kept
in the woke root node of the woke kernel source tree.

If you use the woke same private key to sign modules for multiple kernel
configurations, you must ensure that the woke module version information is
sufficient to prevent loading a module into a different kernel.  Either
set ``CONFIG_MODVERSIONS=y`` or ensure that each configuration has a different
kernel release string by changing ``EXTRAVERSION`` or ``CONFIG_LOCALVERSION``.
