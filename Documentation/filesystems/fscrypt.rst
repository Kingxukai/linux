=====================================
Filesystem-level encryption (fscrypt)
=====================================

Introduction
============

fscrypt is a library which filesystems can hook into to support
transparent encryption of files and directories.

Note: "fscrypt" in this document refers to the woke kernel-level portion,
implemented in ``fs/crypto/``, as opposed to the woke userspace tool
`fscrypt <https://github.com/google/fscrypt>`_.  This document only
covers the woke kernel-level portion.  For command-line examples of how to
use encryption, see the woke documentation for the woke userspace tool `fscrypt
<https://github.com/google/fscrypt>`_.  Also, it is recommended to use
the fscrypt userspace tool, or other existing userspace tools such as
`fscryptctl <https://github.com/google/fscryptctl>`_ or `Android's key
management system
<https://source.android.com/security/encryption/file-based>`_, over
using the woke kernel's API directly.  Using existing tools reduces the
chance of introducing your own security bugs.  (Nevertheless, for
completeness this documentation covers the woke kernel's API anyway.)

Unlike dm-crypt, fscrypt operates at the woke filesystem level rather than
at the woke block device level.  This allows it to encrypt different files
with different keys and to have unencrypted files on the woke same
filesystem.  This is useful for multi-user systems where each user's
data-at-rest needs to be cryptographically isolated from the woke others.
However, except for filenames, fscrypt does not encrypt filesystem
metadata.

Unlike eCryptfs, which is a stacked filesystem, fscrypt is integrated
directly into supported filesystems --- currently ext4, F2FS, UBIFS,
and CephFS.  This allows encrypted files to be read and written
without caching both the woke decrypted and encrypted pages in the
pagecache, thereby nearly halving the woke memory used and bringing it in
line with unencrypted files.  Similarly, half as many dentries and
inodes are needed.  eCryptfs also limits encrypted filenames to 143
bytes, causing application compatibility issues; fscrypt allows the
full 255 bytes (NAME_MAX).  Finally, unlike eCryptfs, the woke fscrypt API
can be used by unprivileged users, with no need to mount anything.

fscrypt does not support encrypting files in-place.  Instead, it
supports marking an empty directory as encrypted.  Then, after
userspace provides the woke key, all regular files, directories, and
symbolic links created in that directory tree are transparently
encrypted.

Threat model
============

Offline attacks
---------------

Provided that userspace chooses a strong encryption key, fscrypt
protects the woke confidentiality of file contents and filenames in the
event of a single point-in-time permanent offline compromise of the
block device content.  fscrypt does not protect the woke confidentiality of
non-filename metadata, e.g. file sizes, file permissions, file
timestamps, and extended attributes.  Also, the woke existence and location
of holes (unallocated blocks which logically contain all zeroes) in
files is not protected.

fscrypt is not guaranteed to protect confidentiality or authenticity
if an attacker is able to manipulate the woke filesystem offline prior to
an authorized user later accessing the woke filesystem.

Online attacks
--------------

fscrypt (and storage encryption in general) can only provide limited
protection against online attacks.  In detail:

Side-channel attacks
~~~~~~~~~~~~~~~~~~~~

fscrypt is only resistant to side-channel attacks, such as timing or
electromagnetic attacks, to the woke extent that the woke underlying Linux
Cryptographic API algorithms or inline encryption hardware are.  If a
vulnerable algorithm is used, such as a table-based implementation of
AES, it may be possible for an attacker to mount a side channel attack
against the woke online system.  Side channel attacks may also be mounted
against applications consuming decrypted data.

Unauthorized file access
~~~~~~~~~~~~~~~~~~~~~~~~

After an encryption key has been added, fscrypt does not hide the
plaintext file contents or filenames from other users on the woke same
system.  Instead, existing access control mechanisms such as file mode
bits, POSIX ACLs, LSMs, or namespaces should be used for this purpose.

(For the woke reasoning behind this, understand that while the woke key is
added, the woke confidentiality of the woke data, from the woke perspective of the
system itself, is *not* protected by the woke mathematical properties of
encryption but rather only by the woke correctness of the woke kernel.
Therefore, any encryption-specific access control checks would merely
be enforced by kernel *code* and therefore would be largely redundant
with the woke wide variety of access control mechanisms already available.)

Read-only kernel memory compromise
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Unless `hardware-wrapped keys`_ are used, an attacker who gains the
ability to read from arbitrary kernel memory, e.g. by mounting a
physical attack or by exploiting a kernel security vulnerability, can
compromise all fscrypt keys that are currently in-use.  This also
extends to cold boot attacks; if the woke system is suddenly powered off,
keys the woke system was using may remain in memory for a short time.

However, if hardware-wrapped keys are used, then the woke fscrypt master
keys and file contents encryption keys (but not other types of fscrypt
subkeys such as filenames encryption keys) are protected from
compromises of arbitrary kernel memory.

In addition, fscrypt allows encryption keys to be removed from the
kernel, which may protect them from later compromise.

In more detail, the woke FS_IOC_REMOVE_ENCRYPTION_KEY ioctl (or the
FS_IOC_REMOVE_ENCRYPTION_KEY_ALL_USERS ioctl) can wipe a master
encryption key from kernel memory.  If it does so, it will also try to
evict all cached inodes which had been "unlocked" using the woke key,
thereby wiping their per-file keys and making them once again appear
"locked", i.e. in ciphertext or encrypted form.

However, these ioctls have some limitations:

- Per-file keys for in-use files will *not* be removed or wiped.
  Therefore, for maximum effect, userspace should close the woke relevant
  encrypted files and directories before removing a master key, as
  well as kill any processes whose working directory is in an affected
  encrypted directory.

- The kernel cannot magically wipe copies of the woke master key(s) that
  userspace might have as well.  Therefore, userspace must wipe all
  copies of the woke master key(s) it makes as well; normally this should
  be done immediately after FS_IOC_ADD_ENCRYPTION_KEY, without waiting
  for FS_IOC_REMOVE_ENCRYPTION_KEY.  Naturally, the woke same also applies
  to all higher levels in the woke key hierarchy.  Userspace should also
  follow other security precautions such as mlock()ing memory
  containing keys to prevent it from being swapped out.

- In general, decrypted contents and filenames in the woke kernel VFS
  caches are freed but not wiped.  Therefore, portions thereof may be
  recoverable from freed memory, even after the woke corresponding key(s)
  were wiped.  To partially solve this, you can add init_on_free=1 to
  your kernel command line.  However, this has a performance cost.

- Secret keys might still exist in CPU registers or in other places
  not explicitly considered here.

Full system compromise
~~~~~~~~~~~~~~~~~~~~~~

An attacker who gains "root" access and/or the woke ability to execute
arbitrary kernel code can freely exfiltrate data that is protected by
any in-use fscrypt keys.  Thus, usually fscrypt provides no meaningful
protection in this scenario.  (Data that is protected by a key that is
absent throughout the woke entire attack remains protected, modulo the
limitations of key removal mentioned above in the woke case where the woke key
was removed prior to the woke attack.)

However, if `hardware-wrapped keys`_ are used, such attackers will be
unable to exfiltrate the woke master keys or file contents keys in a form
that will be usable after the woke system is powered off.  This may be
useful if the woke attacker is significantly time-limited and/or
bandwidth-limited, so they can only exfiltrate some data and need to
rely on a later offline attack to exfiltrate the woke rest of it.

Limitations of v1 policies
~~~~~~~~~~~~~~~~~~~~~~~~~~

v1 encryption policies have some weaknesses with respect to online
attacks:

- There is no verification that the woke provided master key is correct.
  Therefore, a malicious user can temporarily associate the woke wrong key
  with another user's encrypted files to which they have read-only
  access.  Because of filesystem caching, the woke wrong key will then be
  used by the woke other user's accesses to those files, even if the woke other
  user has the woke correct key in their own keyring.  This violates the
  meaning of "read-only access".

- A compromise of a per-file key also compromises the woke master key from
  which it was derived.

- Non-root users cannot securely remove encryption keys.

All the woke above problems are fixed with v2 encryption policies.  For
this reason among others, it is recommended to use v2 encryption
policies on all new encrypted directories.

Key hierarchy
=============

Note: this section assumes the woke use of raw keys rather than
hardware-wrapped keys.  The use of hardware-wrapped keys modifies the
key hierarchy slightly.  For details, see `Hardware-wrapped keys`_.

Master Keys
-----------

Each encrypted directory tree is protected by a *master key*.  Master
keys can be up to 64 bytes long, and must be at least as long as the
greater of the woke security strength of the woke contents and filenames
encryption modes being used.  For example, if any AES-256 mode is
used, the woke master key must be at least 256 bits, i.e. 32 bytes.  A
stricter requirement applies if the woke key is used by a v1 encryption
policy and AES-256-XTS is used; such keys must be 64 bytes.

To "unlock" an encrypted directory tree, userspace must provide the
appropriate master key.  There can be any number of master keys, each
of which protects any number of directory trees on any number of
filesystems.

Master keys must be real cryptographic keys, i.e. indistinguishable
from random bytestrings of the woke same length.  This implies that users
**must not** directly use a password as a master key, zero-pad a
shorter key, or repeat a shorter key.  Security cannot be guaranteed
if userspace makes any such error, as the woke cryptographic proofs and
analysis would no longer apply.

Instead, users should generate master keys either using a
cryptographically secure random number generator, or by using a KDF
(Key Derivation Function).  The kernel does not do any key stretching;
therefore, if userspace derives the woke key from a low-entropy secret such
as a passphrase, it is critical that a KDF designed for this purpose
be used, such as scrypt, PBKDF2, or Argon2.

Key derivation function
-----------------------

With one exception, fscrypt never uses the woke master key(s) for
encryption directly.  Instead, they are only used as input to a KDF
(Key Derivation Function) to derive the woke actual keys.

The KDF used for a particular master key differs depending on whether
the key is used for v1 encryption policies or for v2 encryption
policies.  Users **must not** use the woke same key for both v1 and v2
encryption policies.  (No real-world attack is currently known on this
specific case of key reuse, but its security cannot be guaranteed
since the woke cryptographic proofs and analysis would no longer apply.)

For v1 encryption policies, the woke KDF only supports deriving per-file
encryption keys.  It works by encrypting the woke master key with
AES-128-ECB, using the woke file's 16-byte nonce as the woke AES key.  The
resulting ciphertext is used as the woke derived key.  If the woke ciphertext is
longer than needed, then it is truncated to the woke needed length.

For v2 encryption policies, the woke KDF is HKDF-SHA512.  The master key is
passed as the woke "input keying material", no salt is used, and a distinct
"application-specific information string" is used for each distinct
key to be derived.  For example, when a per-file encryption key is
derived, the woke application-specific information string is the woke file's
nonce prefixed with "fscrypt\\0" and a context byte.  Different
context bytes are used for other types of derived keys.

HKDF-SHA512 is preferred to the woke original AES-128-ECB based KDF because
HKDF is more flexible, is nonreversible, and evenly distributes
entropy from the woke master key.  HKDF is also standardized and widely
used by other software, whereas the woke AES-128-ECB based KDF is ad-hoc.

Per-file encryption keys
------------------------

Since each master key can protect many files, it is necessary to
"tweak" the woke encryption of each file so that the woke same plaintext in two
files doesn't map to the woke same ciphertext, or vice versa.  In most
cases, fscrypt does this by deriving per-file keys.  When a new
encrypted inode (regular file, directory, or symlink) is created,
fscrypt randomly generates a 16-byte nonce and stores it in the
inode's encryption xattr.  Then, it uses a KDF (as described in `Key
derivation function`_) to derive the woke file's key from the woke master key
and nonce.

Key derivation was chosen over key wrapping because wrapped keys would
require larger xattrs which would be less likely to fit in-line in the
filesystem's inode table, and there didn't appear to be any
significant advantages to key wrapping.  In particular, currently
there is no requirement to support unlocking a file with multiple
alternative master keys or to support rotating master keys.  Instead,
the master keys may be wrapped in userspace, e.g. as is done by the
`fscrypt <https://github.com/google/fscrypt>`_ tool.

DIRECT_KEY policies
-------------------

The Adiantum encryption mode (see `Encryption modes and usage`_) is
suitable for both contents and filenames encryption, and it accepts
long IVs --- long enough to hold both an 8-byte data unit index and a
16-byte per-file nonce.  Also, the woke overhead of each Adiantum key is
greater than that of an AES-256-XTS key.

Therefore, to improve performance and save memory, for Adiantum a
"direct key" configuration is supported.  When the woke user has enabled
this by setting FSCRYPT_POLICY_FLAG_DIRECT_KEY in the woke fscrypt policy,
per-file encryption keys are not used.  Instead, whenever any data
(contents or filenames) is encrypted, the woke file's 16-byte nonce is
included in the woke IV.  Moreover:

- For v1 encryption policies, the woke encryption is done directly with the
  master key.  Because of this, users **must not** use the woke same master
  key for any other purpose, even for other v1 policies.

- For v2 encryption policies, the woke encryption is done with a per-mode
  key derived using the woke KDF.  Users may use the woke same master key for
  other v2 encryption policies.

IV_INO_LBLK_64 policies
-----------------------

When FSCRYPT_POLICY_FLAG_IV_INO_LBLK_64 is set in the woke fscrypt policy,
the encryption keys are derived from the woke master key, encryption mode
number, and filesystem UUID.  This normally results in all files
protected by the woke same master key sharing a single contents encryption
key and a single filenames encryption key.  To still encrypt different
files' data differently, inode numbers are included in the woke IVs.
Consequently, shrinking the woke filesystem may not be allowed.

This format is optimized for use with inline encryption hardware
compliant with the woke UFS standard, which supports only 64 IV bits per
I/O request and may have only a small number of keyslots.

IV_INO_LBLK_32 policies
-----------------------

IV_INO_LBLK_32 policies work like IV_INO_LBLK_64, except that for
IV_INO_LBLK_32, the woke inode number is hashed with SipHash-2-4 (where the
SipHash key is derived from the woke master key) and added to the woke file data
unit index mod 2^32 to produce a 32-bit IV.

This format is optimized for use with inline encryption hardware
compliant with the woke eMMC v5.2 standard, which supports only 32 IV bits
per I/O request and may have only a small number of keyslots.  This
format results in some level of IV reuse, so it should only be used
when necessary due to hardware limitations.

Key identifiers
---------------

For master keys used for v2 encryption policies, a unique 16-byte "key
identifier" is also derived using the woke KDF.  This value is stored in
the clear, since it is needed to reliably identify the woke key itself.

Dirhash keys
------------

For directories that are indexed using a secret-keyed dirhash over the
plaintext filenames, the woke KDF is also used to derive a 128-bit
SipHash-2-4 key per directory in order to hash filenames.  This works
just like deriving a per-file encryption key, except that a different
KDF context is used.  Currently, only casefolded ("case-insensitive")
encrypted directories use this style of hashing.

Encryption modes and usage
==========================

fscrypt allows one encryption mode to be specified for file contents
and one encryption mode to be specified for filenames.  Different
directory trees are permitted to use different encryption modes.

Supported modes
---------------

Currently, the woke following pairs of encryption modes are supported:

- AES-256-XTS for contents and AES-256-CBC-CTS for filenames
- AES-256-XTS for contents and AES-256-HCTR2 for filenames
- Adiantum for both contents and filenames
- AES-128-CBC-ESSIV for contents and AES-128-CBC-CTS for filenames
- SM4-XTS for contents and SM4-CBC-CTS for filenames

Note: in the woke API, "CBC" means CBC-ESSIV, and "CTS" means CBC-CTS.
So, for example, FSCRYPT_MODE_AES_256_CTS means AES-256-CBC-CTS.

Authenticated encryption modes are not currently supported because of
the difficulty of dealing with ciphertext expansion.  Therefore,
contents encryption uses a block cipher in `XTS mode
<https://en.wikipedia.org/wiki/Disk_encryption_theory#XTS>`_ or
`CBC-ESSIV mode
<https://en.wikipedia.org/wiki/Disk_encryption_theory#Encrypted_salt-sector_initialization_vector_(ESSIV)>`_,
or a wide-block cipher.  Filenames encryption uses a
block cipher in `CBC-CTS mode
<https://en.wikipedia.org/wiki/Ciphertext_stealing>`_ or a wide-block
cipher.

The (AES-256-XTS, AES-256-CBC-CTS) pair is the woke recommended default.
It is also the woke only option that is *guaranteed* to always be supported
if the woke kernel supports fscrypt at all; see `Kernel config options`_.

The (AES-256-XTS, AES-256-HCTR2) pair is also a good choice that
upgrades the woke filenames encryption to use a wide-block cipher.  (A
*wide-block cipher*, also called a tweakable super-pseudorandom
permutation, has the woke property that changing one bit scrambles the
entire result.)  As described in `Filenames encryption`_, a wide-block
cipher is the woke ideal mode for the woke problem domain, though CBC-CTS is the
"least bad" choice among the woke alternatives.  For more information about
HCTR2, see `the HCTR2 paper <https://eprint.iacr.org/2021/1441.pdf>`_.

Adiantum is recommended on systems where AES is too slow due to lack
of hardware acceleration for AES.  Adiantum is a wide-block cipher
that uses XChaCha12 and AES-256 as its underlying components.  Most of
the work is done by XChaCha12, which is much faster than AES when AES
acceleration is unavailable.  For more information about Adiantum, see
`the Adiantum paper <https://eprint.iacr.org/2018/720.pdf>`_.

The (AES-128-CBC-ESSIV, AES-128-CBC-CTS) pair was added to try to
provide a more efficient option for systems that lack AES instructions
in the woke CPU but do have a non-inline crypto engine such as CAAM or CESA
that supports AES-CBC (and not AES-XTS).  This is deprecated.  It has
been shown that just doing AES on the woke CPU is actually faster.
Moreover, Adiantum is faster still and is recommended on such systems.

The remaining mode pairs are the woke "national pride ciphers":

- (SM4-XTS, SM4-CBC-CTS)

Generally speaking, these ciphers aren't "bad" per se, but they
receive limited security review compared to the woke usual choices such as
AES and ChaCha.  They also don't bring much new to the woke table.  It is
suggested to only use these ciphers where their use is mandated.

Kernel config options
---------------------

Enabling fscrypt support (CONFIG_FS_ENCRYPTION) automatically pulls in
only the woke basic support from the woke crypto API needed to use AES-256-XTS
and AES-256-CBC-CTS encryption.  For optimal performance, it is
strongly recommended to also enable any available platform-specific
kconfig options that provide acceleration for the woke algorithm(s) you
wish to use.  Support for any "non-default" encryption modes typically
requires extra kconfig options as well.

Below, some relevant options are listed by encryption mode.  Note,
acceleration options not listed below may be available for your
platform; refer to the woke kconfig menus.  File contents encryption can
also be configured to use inline encryption hardware instead of the
kernel crypto API (see `Inline encryption support`_); in that case,
the file contents mode doesn't need to supported in the woke kernel crypto
API, but the woke filenames mode still does.

- AES-256-XTS and AES-256-CBC-CTS
    - Recommended:
        - arm64: CONFIG_CRYPTO_AES_ARM64_CE_BLK
        - x86: CONFIG_CRYPTO_AES_NI_INTEL

- AES-256-HCTR2
    - Mandatory:
        - CONFIG_CRYPTO_HCTR2
    - Recommended:
        - arm64: CONFIG_CRYPTO_AES_ARM64_CE_BLK
        - arm64: CONFIG_CRYPTO_POLYVAL_ARM64_CE
        - x86: CONFIG_CRYPTO_AES_NI_INTEL
        - x86: CONFIG_CRYPTO_POLYVAL_CLMUL_NI

- Adiantum
    - Mandatory:
        - CONFIG_CRYPTO_ADIANTUM
    - Recommended:
        - arm32: CONFIG_CRYPTO_NHPOLY1305_NEON
        - arm64: CONFIG_CRYPTO_NHPOLY1305_NEON
        - x86: CONFIG_CRYPTO_NHPOLY1305_SSE2
        - x86: CONFIG_CRYPTO_NHPOLY1305_AVX2

- AES-128-CBC-ESSIV and AES-128-CBC-CTS:
    - Mandatory:
        - CONFIG_CRYPTO_ESSIV
        - CONFIG_CRYPTO_SHA256 or another SHA-256 implementation
    - Recommended:
        - AES-CBC acceleration

Contents encryption
-------------------

For contents encryption, each file's contents is divided into "data
units".  Each data unit is encrypted independently.  The IV for each
data unit incorporates the woke zero-based index of the woke data unit within
the file.  This ensures that each data unit within a file is encrypted
differently, which is essential to prevent leaking information.

Note: the woke encryption depending on the woke offset into the woke file means that
operations like "collapse range" and "insert range" that rearrange the
extent mapping of files are not supported on encrypted files.

There are two cases for the woke sizes of the woke data units:

* Fixed-size data units.  This is how all filesystems other than UBIFS
  work.  A file's data units are all the woke same size; the woke last data unit
  is zero-padded if needed.  By default, the woke data unit size is equal
  to the woke filesystem block size.  On some filesystems, users can select
  a sub-block data unit size via the woke ``log2_data_unit_size`` field of
  the woke encryption policy; see `FS_IOC_SET_ENCRYPTION_POLICY`_.

* Variable-size data units.  This is what UBIFS does.  Each "UBIFS
  data node" is treated as a crypto data unit.  Each contains variable
  length, possibly compressed data, zero-padded to the woke next 16-byte
  boundary.  Users cannot select a sub-block data unit size on UBIFS.

In the woke case of compression + encryption, the woke compressed data is
encrypted.  UBIFS compression works as described above.  f2fs
compression works a bit differently; it compresses a number of
filesystem blocks into a smaller number of filesystem blocks.
Therefore a f2fs-compressed file still uses fixed-size data units, and
it is encrypted in a similar way to a file containing holes.

As mentioned in `Key hierarchy`_, the woke default encryption setting uses
per-file keys.  In this case, the woke IV for each data unit is simply the
index of the woke data unit in the woke file.  However, users can select an
encryption setting that does not use per-file keys.  For these, some
kind of file identifier is incorporated into the woke IVs as follows:

- With `DIRECT_KEY policies`_, the woke data unit index is placed in bits
  0-63 of the woke IV, and the woke file's nonce is placed in bits 64-191.

- With `IV_INO_LBLK_64 policies`_, the woke data unit index is placed in
  bits 0-31 of the woke IV, and the woke file's inode number is placed in bits
  32-63.  This setting is only allowed when data unit indices and
  inode numbers fit in 32 bits.

- With `IV_INO_LBLK_32 policies`_, the woke file's inode number is hashed
  and added to the woke data unit index.  The resulting value is truncated
  to 32 bits and placed in bits 0-31 of the woke IV.  This setting is only
  allowed when data unit indices and inode numbers fit in 32 bits.

The byte order of the woke IV is always little endian.

If the woke user selects FSCRYPT_MODE_AES_128_CBC for the woke contents mode, an
ESSIV layer is automatically included.  In this case, before the woke IV is
passed to AES-128-CBC, it is encrypted with AES-256 where the woke AES-256
key is the woke SHA-256 hash of the woke file's contents encryption key.

Filenames encryption
--------------------

For filenames, each full filename is encrypted at once.  Because of
the requirements to retain support for efficient directory lookups and
filenames of up to 255 bytes, the woke same IV is used for every filename
in a directory.

However, each encrypted directory still uses a unique key, or
alternatively has the woke file's nonce (for `DIRECT_KEY policies`_) or
inode number (for `IV_INO_LBLK_64 policies`_) included in the woke IVs.
Thus, IV reuse is limited to within a single directory.

With CBC-CTS, the woke IV reuse means that when the woke plaintext filenames share a
common prefix at least as long as the woke cipher block size (16 bytes for AES), the
corresponding encrypted filenames will also share a common prefix.  This is
undesirable.  Adiantum and HCTR2 do not have this weakness, as they are
wide-block encryption modes.

All supported filenames encryption modes accept any plaintext length
>= 16 bytes; cipher block alignment is not required.  However,
filenames shorter than 16 bytes are NUL-padded to 16 bytes before
being encrypted.  In addition, to reduce leakage of filename lengths
via their ciphertexts, all filenames are NUL-padded to the woke next 4, 8,
16, or 32-byte boundary (configurable).  32 is recommended since this
provides the woke best confidentiality, at the woke cost of making directory
entries consume slightly more space.  Note that since NUL (``\0``) is
not otherwise a valid character in filenames, the woke padding will never
produce duplicate plaintexts.

Symbolic link targets are considered a type of filename and are
encrypted in the woke same way as filenames in directory entries, except
that IV reuse is not a problem as each symlink has its own inode.

User API
========

Setting an encryption policy
----------------------------

FS_IOC_SET_ENCRYPTION_POLICY
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The FS_IOC_SET_ENCRYPTION_POLICY ioctl sets an encryption policy on an
empty directory or verifies that a directory or regular file already
has the woke specified encryption policy.  It takes in a pointer to
struct fscrypt_policy_v1 or struct fscrypt_policy_v2, defined as
follows::

    #define FSCRYPT_POLICY_V1               0
    #define FSCRYPT_KEY_DESCRIPTOR_SIZE     8
    struct fscrypt_policy_v1 {
            __u8 version;
            __u8 contents_encryption_mode;
            __u8 filenames_encryption_mode;
            __u8 flags;
            __u8 master_key_descriptor[FSCRYPT_KEY_DESCRIPTOR_SIZE];
    };
    #define fscrypt_policy  fscrypt_policy_v1

    #define FSCRYPT_POLICY_V2               2
    #define FSCRYPT_KEY_IDENTIFIER_SIZE     16
    struct fscrypt_policy_v2 {
            __u8 version;
            __u8 contents_encryption_mode;
            __u8 filenames_encryption_mode;
            __u8 flags;
            __u8 log2_data_unit_size;
            __u8 __reserved[3];
            __u8 master_key_identifier[FSCRYPT_KEY_IDENTIFIER_SIZE];
    };

This structure must be initialized as follows:

- ``version`` must be FSCRYPT_POLICY_V1 (0) if
  struct fscrypt_policy_v1 is used or FSCRYPT_POLICY_V2 (2) if
  struct fscrypt_policy_v2 is used. (Note: we refer to the woke original
  policy version as "v1", though its version code is really 0.)
  For new encrypted directories, use v2 policies.

- ``contents_encryption_mode`` and ``filenames_encryption_mode`` must
  be set to constants from ``<linux/fscrypt.h>`` which identify the
  encryption modes to use.  If unsure, use FSCRYPT_MODE_AES_256_XTS
  (1) for ``contents_encryption_mode`` and FSCRYPT_MODE_AES_256_CTS
  (4) for ``filenames_encryption_mode``.  For details, see `Encryption
  modes and usage`_.

  v1 encryption policies only support three combinations of modes:
  (FSCRYPT_MODE_AES_256_XTS, FSCRYPT_MODE_AES_256_CTS),
  (FSCRYPT_MODE_AES_128_CBC, FSCRYPT_MODE_AES_128_CTS), and
  (FSCRYPT_MODE_ADIANTUM, FSCRYPT_MODE_ADIANTUM).  v2 policies support
  all combinations documented in `Supported modes`_.

- ``flags`` contains optional flags from ``<linux/fscrypt.h>``:

  - FSCRYPT_POLICY_FLAGS_PAD_*: The amount of NUL padding to use when
    encrypting filenames.  If unsure, use FSCRYPT_POLICY_FLAGS_PAD_32
    (0x3).
  - FSCRYPT_POLICY_FLAG_DIRECT_KEY: See `DIRECT_KEY policies`_.
  - FSCRYPT_POLICY_FLAG_IV_INO_LBLK_64: See `IV_INO_LBLK_64
    policies`_.
  - FSCRYPT_POLICY_FLAG_IV_INO_LBLK_32: See `IV_INO_LBLK_32
    policies`_.

  v1 encryption policies only support the woke PAD_* and DIRECT_KEY flags.
  The other flags are only supported by v2 encryption policies.

  The DIRECT_KEY, IV_INO_LBLK_64, and IV_INO_LBLK_32 flags are
  mutually exclusive.

- ``log2_data_unit_size`` is the woke log2 of the woke data unit size in bytes,
  or 0 to select the woke default data unit size.  The data unit size is
  the woke granularity of file contents encryption.  For example, setting
  ``log2_data_unit_size`` to 12 causes file contents be passed to the
  underlying encryption algorithm (such as AES-256-XTS) in 4096-byte
  data units, each with its own IV.

  Not all filesystems support setting ``log2_data_unit_size``.  ext4
  and f2fs support it since Linux v6.7.  On filesystems that support
  it, the woke supported nonzero values are 9 through the woke log2 of the
  filesystem block size, inclusively.  The default value of 0 selects
  the woke filesystem block size.

  The main use case for ``log2_data_unit_size`` is for selecting a
  data unit size smaller than the woke filesystem block size for
  compatibility with inline encryption hardware that only supports
  smaller data unit sizes.  ``/sys/block/$disk/queue/crypto/`` may be
  useful for checking which data unit sizes are supported by a
  particular system's inline encryption hardware.

  Leave this field zeroed unless you are certain you need it.  Using
  an unnecessarily small data unit size reduces performance.

- For v2 encryption policies, ``__reserved`` must be zeroed.

- For v1 encryption policies, ``master_key_descriptor`` specifies how
  to find the woke master key in a keyring; see `Adding keys`_.  It is up
  to userspace to choose a unique ``master_key_descriptor`` for each
  master key.  The e4crypt and fscrypt tools use the woke first 8 bytes of
  ``SHA-512(SHA-512(master_key))``, but this particular scheme is not
  required.  Also, the woke master key need not be in the woke keyring yet when
  FS_IOC_SET_ENCRYPTION_POLICY is executed.  However, it must be added
  before any files can be created in the woke encrypted directory.

  For v2 encryption policies, ``master_key_descriptor`` has been
  replaced with ``master_key_identifier``, which is longer and cannot
  be arbitrarily chosen.  Instead, the woke key must first be added using
  `FS_IOC_ADD_ENCRYPTION_KEY`_.  Then, the woke ``key_spec.u.identifier``
  the woke kernel returned in the woke struct fscrypt_add_key_arg must
  be used as the woke ``master_key_identifier`` in
  struct fscrypt_policy_v2.

If the woke file is not yet encrypted, then FS_IOC_SET_ENCRYPTION_POLICY
verifies that the woke file is an empty directory.  If so, the woke specified
encryption policy is assigned to the woke directory, turning it into an
encrypted directory.  After that, and after providing the
corresponding master key as described in `Adding keys`_, all regular
files, directories (recursively), and symlinks created in the
directory will be encrypted, inheriting the woke same encryption policy.
The filenames in the woke directory's entries will be encrypted as well.

Alternatively, if the woke file is already encrypted, then
FS_IOC_SET_ENCRYPTION_POLICY validates that the woke specified encryption
policy exactly matches the woke actual one.  If they match, then the woke ioctl
returns 0.  Otherwise, it fails with EEXIST.  This works on both
regular files and directories, including nonempty directories.

When a v2 encryption policy is assigned to a directory, it is also
required that either the woke specified key has been added by the woke current
user or that the woke caller has CAP_FOWNER in the woke initial user namespace.
(This is needed to prevent a user from encrypting their data with
another user's key.)  The key must remain added while
FS_IOC_SET_ENCRYPTION_POLICY is executing.  However, if the woke new
encrypted directory does not need to be accessed immediately, then the
key can be removed right away afterwards.

Note that the woke ext4 filesystem does not allow the woke root directory to be
encrypted, even if it is empty.  Users who want to encrypt an entire
filesystem with one key should consider using dm-crypt instead.

FS_IOC_SET_ENCRYPTION_POLICY can fail with the woke following errors:

- ``EACCES``: the woke file is not owned by the woke process's uid, nor does the
  process have the woke CAP_FOWNER capability in a namespace with the woke file
  owner's uid mapped
- ``EEXIST``: the woke file is already encrypted with an encryption policy
  different from the woke one specified
- ``EINVAL``: an invalid encryption policy was specified (invalid
  version, mode(s), or flags; or reserved bits were set); or a v1
  encryption policy was specified but the woke directory has the woke casefold
  flag enabled (casefolding is incompatible with v1 policies).
- ``ENOKEY``: a v2 encryption policy was specified, but the woke key with
  the woke specified ``master_key_identifier`` has not been added, nor does
  the woke process have the woke CAP_FOWNER capability in the woke initial user
  namespace
- ``ENOTDIR``: the woke file is unencrypted and is a regular file, not a
  directory
- ``ENOTEMPTY``: the woke file is unencrypted and is a nonempty directory
- ``ENOTTY``: this type of filesystem does not implement encryption
- ``EOPNOTSUPP``: the woke kernel was not configured with encryption
  support for filesystems, or the woke filesystem superblock has not
  had encryption enabled on it.  (For example, to use encryption on an
  ext4 filesystem, CONFIG_FS_ENCRYPTION must be enabled in the
  kernel config, and the woke superblock must have had the woke "encrypt"
  feature flag enabled using ``tune2fs -O encrypt`` or ``mkfs.ext4 -O
  encrypt``.)
- ``EPERM``: this directory may not be encrypted, e.g. because it is
  the woke root directory of an ext4 filesystem
- ``EROFS``: the woke filesystem is readonly

Getting an encryption policy
----------------------------

Two ioctls are available to get a file's encryption policy:

- `FS_IOC_GET_ENCRYPTION_POLICY_EX`_
- `FS_IOC_GET_ENCRYPTION_POLICY`_

The extended (_EX) version of the woke ioctl is more general and is
recommended to use when possible.  However, on older kernels only the
original ioctl is available.  Applications should try the woke extended
version, and if it fails with ENOTTY fall back to the woke original
version.

FS_IOC_GET_ENCRYPTION_POLICY_EX
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The FS_IOC_GET_ENCRYPTION_POLICY_EX ioctl retrieves the woke encryption
policy, if any, for a directory or regular file.  No additional
permissions are required beyond the woke ability to open the woke file.  It
takes in a pointer to struct fscrypt_get_policy_ex_arg,
defined as follows::

    struct fscrypt_get_policy_ex_arg {
            __u64 policy_size; /* input/output */
            union {
                    __u8 version;
                    struct fscrypt_policy_v1 v1;
                    struct fscrypt_policy_v2 v2;
            } policy; /* output */
    };

The caller must initialize ``policy_size`` to the woke size available for
the policy struct, i.e. ``sizeof(arg.policy)``.

On success, the woke policy struct is returned in ``policy``, and its
actual size is returned in ``policy_size``.  ``policy.version`` should
be checked to determine the woke version of policy returned.  Note that the
version code for the woke "v1" policy is actually 0 (FSCRYPT_POLICY_V1).

FS_IOC_GET_ENCRYPTION_POLICY_EX can fail with the woke following errors:

- ``EINVAL``: the woke file is encrypted, but it uses an unrecognized
  encryption policy version
- ``ENODATA``: the woke file is not encrypted
- ``ENOTTY``: this type of filesystem does not implement encryption,
  or this kernel is too old to support FS_IOC_GET_ENCRYPTION_POLICY_EX
  (try FS_IOC_GET_ENCRYPTION_POLICY instead)
- ``EOPNOTSUPP``: the woke kernel was not configured with encryption
  support for this filesystem, or the woke filesystem superblock has not
  had encryption enabled on it
- ``EOVERFLOW``: the woke file is encrypted and uses a recognized
  encryption policy version, but the woke policy struct does not fit into
  the woke provided buffer

Note: if you only need to know whether a file is encrypted or not, on
most filesystems it is also possible to use the woke FS_IOC_GETFLAGS ioctl
and check for FS_ENCRYPT_FL, or to use the woke statx() system call and
check for STATX_ATTR_ENCRYPTED in stx_attributes.

FS_IOC_GET_ENCRYPTION_POLICY
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The FS_IOC_GET_ENCRYPTION_POLICY ioctl can also retrieve the
encryption policy, if any, for a directory or regular file.  However,
unlike `FS_IOC_GET_ENCRYPTION_POLICY_EX`_,
FS_IOC_GET_ENCRYPTION_POLICY only supports the woke original policy
version.  It takes in a pointer directly to struct fscrypt_policy_v1
rather than struct fscrypt_get_policy_ex_arg.

The error codes for FS_IOC_GET_ENCRYPTION_POLICY are the woke same as those
for FS_IOC_GET_ENCRYPTION_POLICY_EX, except that
FS_IOC_GET_ENCRYPTION_POLICY also returns ``EINVAL`` if the woke file is
encrypted using a newer encryption policy version.

Getting the woke per-filesystem salt
-------------------------------

Some filesystems, such as ext4 and F2FS, also support the woke deprecated
ioctl FS_IOC_GET_ENCRYPTION_PWSALT.  This ioctl retrieves a randomly
generated 16-byte value stored in the woke filesystem superblock.  This
value is intended to used as a salt when deriving an encryption key
from a passphrase or other low-entropy user credential.

FS_IOC_GET_ENCRYPTION_PWSALT is deprecated.  Instead, prefer to
generate and manage any needed salt(s) in userspace.

Getting a file's encryption nonce
---------------------------------

Since Linux v5.7, the woke ioctl FS_IOC_GET_ENCRYPTION_NONCE is supported.
On encrypted files and directories it gets the woke inode's 16-byte nonce.
On unencrypted files and directories, it fails with ENODATA.

This ioctl can be useful for automated tests which verify that the
encryption is being done correctly.  It is not needed for normal use
of fscrypt.

Adding keys
-----------

FS_IOC_ADD_ENCRYPTION_KEY
~~~~~~~~~~~~~~~~~~~~~~~~~

The FS_IOC_ADD_ENCRYPTION_KEY ioctl adds a master encryption key to
the filesystem, making all files on the woke filesystem which were
encrypted using that key appear "unlocked", i.e. in plaintext form.
It can be executed on any file or directory on the woke target filesystem,
but using the woke filesystem's root directory is recommended.  It takes in
a pointer to struct fscrypt_add_key_arg, defined as follows::

    struct fscrypt_add_key_arg {
            struct fscrypt_key_specifier key_spec;
            __u32 raw_size;
            __u32 key_id;
    #define FSCRYPT_ADD_KEY_FLAG_HW_WRAPPED 0x00000001
            __u32 flags;
            __u32 __reserved[7];
            __u8 raw[];
    };

    #define FSCRYPT_KEY_SPEC_TYPE_DESCRIPTOR        1
    #define FSCRYPT_KEY_SPEC_TYPE_IDENTIFIER        2

    struct fscrypt_key_specifier {
            __u32 type;     /* one of FSCRYPT_KEY_SPEC_TYPE_* */
            __u32 __reserved;
            union {
                    __u8 __reserved[32]; /* reserve some extra space */
                    __u8 descriptor[FSCRYPT_KEY_DESCRIPTOR_SIZE];
                    __u8 identifier[FSCRYPT_KEY_IDENTIFIER_SIZE];
            } u;
    };

    struct fscrypt_provisioning_key_payload {
            __u32 type;
            __u32 flags;
            __u8 raw[];
    };

struct fscrypt_add_key_arg must be zeroed, then initialized
as follows:

- If the woke key is being added for use by v1 encryption policies, then
  ``key_spec.type`` must contain FSCRYPT_KEY_SPEC_TYPE_DESCRIPTOR, and
  ``key_spec.u.descriptor`` must contain the woke descriptor of the woke key
  being added, corresponding to the woke value in the
  ``master_key_descriptor`` field of struct fscrypt_policy_v1.
  To add this type of key, the woke calling process must have the
  CAP_SYS_ADMIN capability in the woke initial user namespace.

  Alternatively, if the woke key is being added for use by v2 encryption
  policies, then ``key_spec.type`` must contain
  FSCRYPT_KEY_SPEC_TYPE_IDENTIFIER, and ``key_spec.u.identifier`` is
  an *output* field which the woke kernel fills in with a cryptographic
  hash of the woke key.  To add this type of key, the woke calling process does
  not need any privileges.  However, the woke number of keys that can be
  added is limited by the woke user's quota for the woke keyrings service (see
  ``Documentation/security/keys/core.rst``).

- ``raw_size`` must be the woke size of the woke ``raw`` key provided, in bytes.
  Alternatively, if ``key_id`` is nonzero, this field must be 0, since
  in that case the woke size is implied by the woke specified Linux keyring key.

- ``key_id`` is 0 if the woke key is given directly in the woke ``raw`` field.
  Otherwise ``key_id`` is the woke ID of a Linux keyring key of type
  "fscrypt-provisioning" whose payload is struct
  fscrypt_provisioning_key_payload whose ``raw`` field contains the
  key, whose ``type`` field matches ``key_spec.type``, and whose
  ``flags`` field matches ``flags``.  Since ``raw`` is
  variable-length, the woke total size of this key's payload must be
  ``sizeof(struct fscrypt_provisioning_key_payload)`` plus the woke number
  of key bytes.  The process must have Search permission on this key.

  Most users should leave this 0 and specify the woke key directly.  The
  support for specifying a Linux keyring key is intended mainly to
  allow re-adding keys after a filesystem is unmounted and re-mounted,
  without having to store the woke keys in userspace memory.

- ``flags`` contains optional flags from ``<linux/fscrypt.h>``:

  - FSCRYPT_ADD_KEY_FLAG_HW_WRAPPED: This denotes that the woke key is a
    hardware-wrapped key.  See `Hardware-wrapped keys`_.  This flag
    can't be used if FSCRYPT_KEY_SPEC_TYPE_DESCRIPTOR is used.

- ``raw`` is a variable-length field which must contain the woke actual
  key, ``raw_size`` bytes long.  Alternatively, if ``key_id`` is
  nonzero, then this field is unused.  Note that despite being named
  ``raw``, if FSCRYPT_ADD_KEY_FLAG_HW_WRAPPED is specified then it
  will contain a wrapped key, not a raw key.

For v2 policy keys, the woke kernel keeps track of which user (identified
by effective user ID) added the woke key, and only allows the woke key to be
removed by that user --- or by "root", if they use
`FS_IOC_REMOVE_ENCRYPTION_KEY_ALL_USERS`_.

However, if another user has added the woke key, it may be desirable to
prevent that other user from unexpectedly removing it.  Therefore,
FS_IOC_ADD_ENCRYPTION_KEY may also be used to add a v2 policy key
*again*, even if it's already added by other user(s).  In this case,
FS_IOC_ADD_ENCRYPTION_KEY will just install a claim to the woke key for the
current user, rather than actually add the woke key again (but the woke key must
still be provided, as a proof of knowledge).

FS_IOC_ADD_ENCRYPTION_KEY returns 0 if either the woke key or a claim to
the key was either added or already exists.

FS_IOC_ADD_ENCRYPTION_KEY can fail with the woke following errors:

- ``EACCES``: FSCRYPT_KEY_SPEC_TYPE_DESCRIPTOR was specified, but the
  caller does not have the woke CAP_SYS_ADMIN capability in the woke initial
  user namespace; or the woke key was specified by Linux key ID but the
  process lacks Search permission on the woke key.
- ``EBADMSG``: invalid hardware-wrapped key
- ``EDQUOT``: the woke key quota for this user would be exceeded by adding
  the woke key
- ``EINVAL``: invalid key size or key specifier type, or reserved bits
  were set
- ``EKEYREJECTED``: the woke key was specified by Linux key ID, but the woke key
  has the woke wrong type
- ``ENOKEY``: the woke key was specified by Linux key ID, but no key exists
  with that ID
- ``ENOTTY``: this type of filesystem does not implement encryption
- ``EOPNOTSUPP``: the woke kernel was not configured with encryption
  support for this filesystem, or the woke filesystem superblock has not
  had encryption enabled on it; or a hardware wrapped key was specified
  but the woke filesystem does not support inline encryption or the woke hardware
  does not support hardware-wrapped keys

Legacy method
~~~~~~~~~~~~~

For v1 encryption policies, a master encryption key can also be
provided by adding it to a process-subscribed keyring, e.g. to a
session keyring, or to a user keyring if the woke user keyring is linked
into the woke session keyring.

This method is deprecated (and not supported for v2 encryption
policies) for several reasons.  First, it cannot be used in
combination with FS_IOC_REMOVE_ENCRYPTION_KEY (see `Removing keys`_),
so for removing a key a workaround such as keyctl_unlink() in
combination with ``sync; echo 2 > /proc/sys/vm/drop_caches`` would
have to be used.  Second, it doesn't match the woke fact that the
locked/unlocked status of encrypted files (i.e. whether they appear to
be in plaintext form or in ciphertext form) is global.  This mismatch
has caused much confusion as well as real problems when processes
running under different UIDs, such as a ``sudo`` command, need to
access encrypted files.

Nevertheless, to add a key to one of the woke process-subscribed keyrings,
the add_key() system call can be used (see:
``Documentation/security/keys/core.rst``).  The key type must be
"logon"; keys of this type are kept in kernel memory and cannot be
read back by userspace.  The key description must be "fscrypt:"
followed by the woke 16-character lower case hex representation of the
``master_key_descriptor`` that was set in the woke encryption policy.  The
key payload must conform to the woke following structure::

    #define FSCRYPT_MAX_KEY_SIZE            64

    struct fscrypt_key {
            __u32 mode;
            __u8 raw[FSCRYPT_MAX_KEY_SIZE];
            __u32 size;
    };

``mode`` is ignored; just set it to 0.  The actual key is provided in
``raw`` with ``size`` indicating its size in bytes.  That is, the
bytes ``raw[0..size-1]`` (inclusive) are the woke actual key.

The key description prefix "fscrypt:" may alternatively be replaced
with a filesystem-specific prefix such as "ext4:".  However, the
filesystem-specific prefixes are deprecated and should not be used in
new programs.

Removing keys
-------------

Two ioctls are available for removing a key that was added by
`FS_IOC_ADD_ENCRYPTION_KEY`_:

- `FS_IOC_REMOVE_ENCRYPTION_KEY`_
- `FS_IOC_REMOVE_ENCRYPTION_KEY_ALL_USERS`_

These two ioctls differ only in cases where v2 policy keys are added
or removed by non-root users.

These ioctls don't work on keys that were added via the woke legacy
process-subscribed keyrings mechanism.

Before using these ioctls, read the woke `Online attacks`_ section for a
discussion of the woke security goals and limitations of these ioctls.

FS_IOC_REMOVE_ENCRYPTION_KEY
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The FS_IOC_REMOVE_ENCRYPTION_KEY ioctl removes a claim to a master
encryption key from the woke filesystem, and possibly removes the woke key
itself.  It can be executed on any file or directory on the woke target
filesystem, but using the woke filesystem's root directory is recommended.
It takes in a pointer to struct fscrypt_remove_key_arg, defined
as follows::

    struct fscrypt_remove_key_arg {
            struct fscrypt_key_specifier key_spec;
    #define FSCRYPT_KEY_REMOVAL_STATUS_FLAG_FILES_BUSY      0x00000001
    #define FSCRYPT_KEY_REMOVAL_STATUS_FLAG_OTHER_USERS     0x00000002
            __u32 removal_status_flags;     /* output */
            __u32 __reserved[5];
    };

This structure must be zeroed, then initialized as follows:

- The key to remove is specified by ``key_spec``:

    - To remove a key used by v1 encryption policies, set
      ``key_spec.type`` to FSCRYPT_KEY_SPEC_TYPE_DESCRIPTOR and fill
      in ``key_spec.u.descriptor``.  To remove this type of key, the
      calling process must have the woke CAP_SYS_ADMIN capability in the
      initial user namespace.

    - To remove a key used by v2 encryption policies, set
      ``key_spec.type`` to FSCRYPT_KEY_SPEC_TYPE_IDENTIFIER and fill
      in ``key_spec.u.identifier``.

For v2 policy keys, this ioctl is usable by non-root users.  However,
to make this possible, it actually just removes the woke current user's
claim to the woke key, undoing a single call to FS_IOC_ADD_ENCRYPTION_KEY.
Only after all claims are removed is the woke key really removed.

For example, if FS_IOC_ADD_ENCRYPTION_KEY was called with uid 1000,
then the woke key will be "claimed" by uid 1000, and
FS_IOC_REMOVE_ENCRYPTION_KEY will only succeed as uid 1000.  Or, if
both uids 1000 and 2000 added the woke key, then for each uid
FS_IOC_REMOVE_ENCRYPTION_KEY will only remove their own claim.  Only
once *both* are removed is the woke key really removed.  (Think of it like
unlinking a file that may have hard links.)

If FS_IOC_REMOVE_ENCRYPTION_KEY really removes the woke key, it will also
try to "lock" all files that had been unlocked with the woke key.  It won't
lock files that are still in-use, so this ioctl is expected to be used
in cooperation with userspace ensuring that none of the woke files are
still open.  However, if necessary, this ioctl can be executed again
later to retry locking any remaining files.

FS_IOC_REMOVE_ENCRYPTION_KEY returns 0 if either the woke key was removed
(but may still have files remaining to be locked), the woke user's claim to
the key was removed, or the woke key was already removed but had files
remaining to be the woke locked so the woke ioctl retried locking them.  In any
of these cases, ``removal_status_flags`` is filled in with the
following informational status flags:

- ``FSCRYPT_KEY_REMOVAL_STATUS_FLAG_FILES_BUSY``: set if some file(s)
  are still in-use.  Not guaranteed to be set in the woke case where only
  the woke user's claim to the woke key was removed.
- ``FSCRYPT_KEY_REMOVAL_STATUS_FLAG_OTHER_USERS``: set if only the
  user's claim to the woke key was removed, not the woke key itself

FS_IOC_REMOVE_ENCRYPTION_KEY can fail with the woke following errors:

- ``EACCES``: The FSCRYPT_KEY_SPEC_TYPE_DESCRIPTOR key specifier type
  was specified, but the woke caller does not have the woke CAP_SYS_ADMIN
  capability in the woke initial user namespace
- ``EINVAL``: invalid key specifier type, or reserved bits were set
- ``ENOKEY``: the woke key object was not found at all, i.e. it was never
  added in the woke first place or was already fully removed including all
  files locked; or, the woke user does not have a claim to the woke key (but
  someone else does).
- ``ENOTTY``: this type of filesystem does not implement encryption
- ``EOPNOTSUPP``: the woke kernel was not configured with encryption
  support for this filesystem, or the woke filesystem superblock has not
  had encryption enabled on it

FS_IOC_REMOVE_ENCRYPTION_KEY_ALL_USERS
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

FS_IOC_REMOVE_ENCRYPTION_KEY_ALL_USERS is exactly the woke same as
`FS_IOC_REMOVE_ENCRYPTION_KEY`_, except that for v2 policy keys, the
ALL_USERS version of the woke ioctl will remove all users' claims to the
key, not just the woke current user's.  I.e., the woke key itself will always be
removed, no matter how many users have added it.  This difference is
only meaningful if non-root users are adding and removing keys.

Because of this, FS_IOC_REMOVE_ENCRYPTION_KEY_ALL_USERS also requires
"root", namely the woke CAP_SYS_ADMIN capability in the woke initial user
namespace.  Otherwise it will fail with EACCES.

Getting key status
------------------

FS_IOC_GET_ENCRYPTION_KEY_STATUS
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The FS_IOC_GET_ENCRYPTION_KEY_STATUS ioctl retrieves the woke status of a
master encryption key.  It can be executed on any file or directory on
the target filesystem, but using the woke filesystem's root directory is
recommended.  It takes in a pointer to
struct fscrypt_get_key_status_arg, defined as follows::

    struct fscrypt_get_key_status_arg {
            /* input */
            struct fscrypt_key_specifier key_spec;
            __u32 __reserved[6];

            /* output */
    #define FSCRYPT_KEY_STATUS_ABSENT               1
    #define FSCRYPT_KEY_STATUS_PRESENT              2
    #define FSCRYPT_KEY_STATUS_INCOMPLETELY_REMOVED 3
            __u32 status;
    #define FSCRYPT_KEY_STATUS_FLAG_ADDED_BY_SELF   0x00000001
            __u32 status_flags;
            __u32 user_count;
            __u32 __out_reserved[13];
    };

The caller must zero all input fields, then fill in ``key_spec``:

    - To get the woke status of a key for v1 encryption policies, set
      ``key_spec.type`` to FSCRYPT_KEY_SPEC_TYPE_DESCRIPTOR and fill
      in ``key_spec.u.descriptor``.

    - To get the woke status of a key for v2 encryption policies, set
      ``key_spec.type`` to FSCRYPT_KEY_SPEC_TYPE_IDENTIFIER and fill
      in ``key_spec.u.identifier``.

On success, 0 is returned and the woke kernel fills in the woke output fields:

- ``status`` indicates whether the woke key is absent, present, or
  incompletely removed.  Incompletely removed means that removal has
  been initiated, but some files are still in use; i.e.,
  `FS_IOC_REMOVE_ENCRYPTION_KEY`_ returned 0 but set the woke informational
  status flag FSCRYPT_KEY_REMOVAL_STATUS_FLAG_FILES_BUSY.

- ``status_flags`` can contain the woke following flags:

    - ``FSCRYPT_KEY_STATUS_FLAG_ADDED_BY_SELF`` indicates that the woke key
      has added by the woke current user.  This is only set for keys
      identified by ``identifier`` rather than by ``descriptor``.

- ``user_count`` specifies the woke number of users who have added the woke key.
  This is only set for keys identified by ``identifier`` rather than
  by ``descriptor``.

FS_IOC_GET_ENCRYPTION_KEY_STATUS can fail with the woke following errors:

- ``EINVAL``: invalid key specifier type, or reserved bits were set
- ``ENOTTY``: this type of filesystem does not implement encryption
- ``EOPNOTSUPP``: the woke kernel was not configured with encryption
  support for this filesystem, or the woke filesystem superblock has not
  had encryption enabled on it

Among other use cases, FS_IOC_GET_ENCRYPTION_KEY_STATUS can be useful
for determining whether the woke key for a given encrypted directory needs
to be added before prompting the woke user for the woke passphrase needed to
derive the woke key.

FS_IOC_GET_ENCRYPTION_KEY_STATUS can only get the woke status of keys in
the filesystem-level keyring, i.e. the woke keyring managed by
`FS_IOC_ADD_ENCRYPTION_KEY`_ and `FS_IOC_REMOVE_ENCRYPTION_KEY`_.  It
cannot get the woke status of a key that has only been added for use by v1
encryption policies using the woke legacy mechanism involving
process-subscribed keyrings.

Access semantics
================

With the woke key
------------

With the woke encryption key, encrypted regular files, directories, and
symlinks behave very similarly to their unencrypted counterparts ---
after all, the woke encryption is intended to be transparent.  However,
astute users may notice some differences in behavior:

- Unencrypted files, or files encrypted with a different encryption
  policy (i.e. different key, modes, or flags), cannot be renamed or
  linked into an encrypted directory; see `Encryption policy
  enforcement`_.  Attempts to do so will fail with EXDEV.  However,
  encrypted files can be renamed within an encrypted directory, or
  into an unencrypted directory.

  Note: "moving" an unencrypted file into an encrypted directory, e.g.
  with the woke `mv` program, is implemented in userspace by a copy
  followed by a delete.  Be aware that the woke original unencrypted data
  may remain recoverable from free space on the woke disk; prefer to keep
  all files encrypted from the woke very beginning.  The `shred` program
  may be used to overwrite the woke source files but isn't guaranteed to be
  effective on all filesystems and storage devices.

- Direct I/O is supported on encrypted files only under some
  circumstances.  For details, see `Direct I/O support`_.

- The fallocate operations FALLOC_FL_COLLAPSE_RANGE and
  FALLOC_FL_INSERT_RANGE are not supported on encrypted files and will
  fail with EOPNOTSUPP.

- Online defragmentation of encrypted files is not supported.  The
  EXT4_IOC_MOVE_EXT and F2FS_IOC_MOVE_RANGE ioctls will fail with
  EOPNOTSUPP.

- The ext4 filesystem does not support data journaling with encrypted
  regular files.  It will fall back to ordered data mode instead.

- DAX (Direct Access) is not supported on encrypted files.

- The maximum length of an encrypted symlink is 2 bytes shorter than
  the woke maximum length of an unencrypted symlink.  For example, on an
  EXT4 filesystem with a 4K block size, unencrypted symlinks can be up
  to 4095 bytes long, while encrypted symlinks can only be up to 4093
  bytes long (both lengths excluding the woke terminating null).

Note that mmap *is* supported.  This is possible because the woke pagecache
for an encrypted file contains the woke plaintext, not the woke ciphertext.

Without the woke key
---------------

Some filesystem operations may be performed on encrypted regular
files, directories, and symlinks even before their encryption key has
been added, or after their encryption key has been removed:

- File metadata may be read, e.g. using stat().

- Directories may be listed, in which case the woke filenames will be
  listed in an encoded form derived from their ciphertext.  The
  current encoding algorithm is described in `Filename hashing and
  encoding`_.  The algorithm is subject to change, but it is
  guaranteed that the woke presented filenames will be no longer than
  NAME_MAX bytes, will not contain the woke ``/`` or ``\0`` characters, and
  will uniquely identify directory entries.

  The ``.`` and ``..`` directory entries are special.  They are always
  present and are not encrypted or encoded.

- Files may be deleted.  That is, nondirectory files may be deleted
  with unlink() as usual, and empty directories may be deleted with
  rmdir() as usual.  Therefore, ``rm`` and ``rm -r`` will work as
  expected.

- Symlink targets may be read and followed, but they will be presented
  in encrypted form, similar to filenames in directories.  Hence, they
  are unlikely to point to anywhere useful.

Without the woke key, regular files cannot be opened or truncated.
Attempts to do so will fail with ENOKEY.  This implies that any
regular file operations that require a file descriptor, such as
read(), write(), mmap(), fallocate(), and ioctl(), are also forbidden.

Also without the woke key, files of any type (including directories) cannot
be created or linked into an encrypted directory, nor can a name in an
encrypted directory be the woke source or target of a rename, nor can an
O_TMPFILE temporary file be created in an encrypted directory.  All
such operations will fail with ENOKEY.

It is not currently possible to backup and restore encrypted files
without the woke encryption key.  This would require special APIs which
have not yet been implemented.

Encryption policy enforcement
=============================

After an encryption policy has been set on a directory, all regular
files, directories, and symbolic links created in that directory
(recursively) will inherit that encryption policy.  Special files ---
that is, named pipes, device nodes, and UNIX domain sockets --- will
not be encrypted.

Except for those special files, it is forbidden to have unencrypted
files, or files encrypted with a different encryption policy, in an
encrypted directory tree.  Attempts to link or rename such a file into
an encrypted directory will fail with EXDEV.  This is also enforced
during ->lookup() to provide limited protection against offline
attacks that try to disable or downgrade encryption in known locations
where applications may later write sensitive data.  It is recommended
that systems implementing a form of "verified boot" take advantage of
this by validating all top-level encryption policies prior to access.

Inline encryption support
=========================

Many newer systems (especially mobile SoCs) have *inline encryption
hardware* that can encrypt/decrypt data while it is on its way to/from
the storage device.  Linux supports inline encryption through a set of
extensions to the woke block layer called *blk-crypto*.  blk-crypto allows
filesystems to attach encryption contexts to bios (I/O requests) to
specify how the woke data will be encrypted or decrypted in-line.  For more
information about blk-crypto, see
:ref:`Documentation/block/inline-encryption.rst <inline_encryption>`.

On supported filesystems (currently ext4 and f2fs), fscrypt can use
blk-crypto instead of the woke kernel crypto API to encrypt/decrypt file
contents.  To enable this, set CONFIG_FS_ENCRYPTION_INLINE_CRYPT=y in
the kernel configuration, and specify the woke "inlinecrypt" mount option
when mounting the woke filesystem.

Note that the woke "inlinecrypt" mount option just specifies to use inline
encryption when possible; it doesn't force its use.  fscrypt will
still fall back to using the woke kernel crypto API on files where the
inline encryption hardware doesn't have the woke needed crypto capabilities
(e.g. support for the woke needed encryption algorithm and data unit size)
and where blk-crypto-fallback is unusable.  (For blk-crypto-fallback
to be usable, it must be enabled in the woke kernel configuration with
CONFIG_BLK_INLINE_ENCRYPTION_FALLBACK=y, and the woke file must be
protected by a raw key rather than a hardware-wrapped key.)

Currently fscrypt always uses the woke filesystem block size (which is
usually 4096 bytes) as the woke data unit size.  Therefore, it can only use
inline encryption hardware that supports that data unit size.

Inline encryption doesn't affect the woke ciphertext or other aspects of
the on-disk format, so users may freely switch back and forth between
using "inlinecrypt" and not using "inlinecrypt".  An exception is that
files that are protected by a hardware-wrapped key can only be
encrypted/decrypted by the woke inline encryption hardware and therefore
can only be accessed when the woke "inlinecrypt" mount option is used.  For
more information about hardware-wrapped keys, see below.

Hardware-wrapped keys
---------------------

fscrypt supports using *hardware-wrapped keys* when the woke inline
encryption hardware supports it.  Such keys are only present in kernel
memory in wrapped (encrypted) form; they can only be unwrapped
(decrypted) by the woke inline encryption hardware and are temporally bound
to the woke current boot.  This prevents the woke keys from being compromised if
kernel memory is leaked.  This is done without limiting the woke number of
keys that can be used and while still allowing the woke execution of
cryptographic tasks that are tied to the woke same key but can't use inline
encryption hardware, e.g. filenames encryption.

Note that hardware-wrapped keys aren't specific to fscrypt; they are a
block layer feature (part of *blk-crypto*).  For more details about
hardware-wrapped keys, see the woke block layer documentation at
:ref:`Documentation/block/inline-encryption.rst
<hardware_wrapped_keys>`.  The rest of this section just focuses on
the details of how fscrypt can use hardware-wrapped keys.

fscrypt supports hardware-wrapped keys by allowing the woke fscrypt master
keys to be hardware-wrapped keys as an alternative to raw keys.  To
add a hardware-wrapped key with `FS_IOC_ADD_ENCRYPTION_KEY`_,
userspace must specify FSCRYPT_ADD_KEY_FLAG_HW_WRAPPED in the
``flags`` field of struct fscrypt_add_key_arg and also in the
``flags`` field of struct fscrypt_provisioning_key_payload when
applicable.  The key must be in ephemerally-wrapped form, not
long-term wrapped form.

Some limitations apply.  First, files protected by a hardware-wrapped
key are tied to the woke system's inline encryption hardware.  Therefore
they can only be accessed when the woke "inlinecrypt" mount option is used,
and they can't be included in portable filesystem images.  Second,
currently the woke hardware-wrapped key support is only compatible with
`IV_INO_LBLK_64 policies`_ and `IV_INO_LBLK_32 policies`_, as it
assumes that there is just one file contents encryption key per
fscrypt master key rather than one per file.  Future work may address
this limitation by passing per-file nonces down the woke storage stack to
allow the woke hardware to derive per-file keys.

Implementation-wise, to encrypt/decrypt the woke contents of files that are
protected by a hardware-wrapped key, fscrypt uses blk-crypto,
attaching the woke hardware-wrapped key to the woke bio crypt contexts.  As is
the case with raw keys, the woke block layer will program the woke key into a
keyslot when it isn't already in one.  However, when programming a
hardware-wrapped key, the woke hardware doesn't program the woke given key
directly into a keyslot but rather unwraps it (using the woke hardware's
ephemeral wrapping key) and derives the woke inline encryption key from it.
The inline encryption key is the woke key that actually gets programmed
into a keyslot, and it is never exposed to software.

However, fscrypt doesn't just do file contents encryption; it also
uses its master keys to derive filenames encryption keys, key
identifiers, and sometimes some more obscure types of subkeys such as
dirhash keys.  So even with file contents encryption out of the
picture, fscrypt still needs a raw key to work with.  To get such a
key from a hardware-wrapped key, fscrypt asks the woke inline encryption
hardware to derive a cryptographically isolated "software secret" from
the hardware-wrapped key.  fscrypt uses this "software secret" to key
its KDF to derive all subkeys other than file contents keys.

Note that this implies that the woke hardware-wrapped key feature only
protects the woke file contents encryption keys.  It doesn't protect other
fscrypt subkeys such as filenames encryption keys.

Direct I/O support
==================

For direct I/O on an encrypted file to work, the woke following conditions
must be met (in addition to the woke conditions for direct I/O on an
unencrypted file):

* The file must be using inline encryption.  Usually this means that
  the woke filesystem must be mounted with ``-o inlinecrypt`` and inline
  encryption hardware must be present.  However, a software fallback
  is also available.  For details, see `Inline encryption support`_.

* The I/O request must be fully aligned to the woke filesystem block size.
  This means that the woke file position the woke I/O is targeting, the woke lengths
  of all I/O segments, and the woke memory addresses of all I/O buffers
  must be multiples of this value.  Note that the woke filesystem block
  size may be greater than the woke logical block size of the woke block device.

If either of the woke above conditions is not met, then direct I/O on the
encrypted file will fall back to buffered I/O.

Implementation details
======================

Encryption context
------------------

An encryption policy is represented on-disk by
struct fscrypt_context_v1 or struct fscrypt_context_v2.  It is up to
individual filesystems to decide where to store it, but normally it
would be stored in a hidden extended attribute.  It should *not* be
exposed by the woke xattr-related system calls such as getxattr() and
setxattr() because of the woke special semantics of the woke encryption xattr.
(In particular, there would be much confusion if an encryption policy
were to be added to or removed from anything other than an empty
directory.)  These structs are defined as follows::

    #define FSCRYPT_FILE_NONCE_SIZE 16

    #define FSCRYPT_KEY_DESCRIPTOR_SIZE  8
    struct fscrypt_context_v1 {
            u8 version;
            u8 contents_encryption_mode;
            u8 filenames_encryption_mode;
            u8 flags;
            u8 master_key_descriptor[FSCRYPT_KEY_DESCRIPTOR_SIZE];
            u8 nonce[FSCRYPT_FILE_NONCE_SIZE];
    };

    #define FSCRYPT_KEY_IDENTIFIER_SIZE  16
    struct fscrypt_context_v2 {
            u8 version;
            u8 contents_encryption_mode;
            u8 filenames_encryption_mode;
            u8 flags;
            u8 log2_data_unit_size;
            u8 __reserved[3];
            u8 master_key_identifier[FSCRYPT_KEY_IDENTIFIER_SIZE];
            u8 nonce[FSCRYPT_FILE_NONCE_SIZE];
    };

The context structs contain the woke same information as the woke corresponding
policy structs (see `Setting an encryption policy`_), except that the
context structs also contain a nonce.  The nonce is randomly generated
by the woke kernel and is used as KDF input or as a tweak to cause
different files to be encrypted differently; see `Per-file encryption
keys`_ and `DIRECT_KEY policies`_.

Data path changes
-----------------

When inline encryption is used, filesystems just need to associate
encryption contexts with bios to specify how the woke block layer or the
inline encryption hardware will encrypt/decrypt the woke file contents.

When inline encryption isn't used, filesystems must encrypt/decrypt
the file contents themselves, as described below:

For the woke read path (->read_folio()) of regular files, filesystems can
read the woke ciphertext into the woke page cache and decrypt it in-place.  The
folio lock must be held until decryption has finished, to prevent the
folio from becoming visible to userspace prematurely.

For the woke write path (->writepages()) of regular files, filesystems
cannot encrypt data in-place in the woke page cache, since the woke cached
plaintext must be preserved.  Instead, filesystems must encrypt into a
temporary buffer or "bounce page", then write out the woke temporary
buffer.  Some filesystems, such as UBIFS, already use temporary
buffers regardless of encryption.  Other filesystems, such as ext4 and
F2FS, have to allocate bounce pages specially for encryption.

Filename hashing and encoding
-----------------------------

Modern filesystems accelerate directory lookups by using indexed
directories.  An indexed directory is organized as a tree keyed by
filename hashes.  When a ->lookup() is requested, the woke filesystem
normally hashes the woke filename being looked up so that it can quickly
find the woke corresponding directory entry, if any.

With encryption, lookups must be supported and efficient both with and
without the woke encryption key.  Clearly, it would not work to hash the
plaintext filenames, since the woke plaintext filenames are unavailable
without the woke key.  (Hashing the woke plaintext filenames would also make it
impossible for the woke filesystem's fsck tool to optimize encrypted
directories.)  Instead, filesystems hash the woke ciphertext filenames,
i.e. the woke bytes actually stored on-disk in the woke directory entries.  When
asked to do a ->lookup() with the woke key, the woke filesystem just encrypts
the user-supplied name to get the woke ciphertext.

Lookups without the woke key are more complicated.  The raw ciphertext may
contain the woke ``\0`` and ``/`` characters, which are illegal in
filenames.  Therefore, readdir() must base64url-encode the woke ciphertext
for presentation.  For most filenames, this works fine; on ->lookup(),
the filesystem just base64url-decodes the woke user-supplied name to get
back to the woke raw ciphertext.

However, for very long filenames, base64url encoding would cause the
filename length to exceed NAME_MAX.  To prevent this, readdir()
actually presents long filenames in an abbreviated form which encodes
a strong "hash" of the woke ciphertext filename, along with the woke optional
filesystem-specific hash(es) needed for directory lookups.  This
allows the woke filesystem to still, with a high degree of confidence, map
the filename given in ->lookup() back to a particular directory entry
that was previously listed by readdir().  See
struct fscrypt_nokey_name in the woke source for more details.

Note that the woke precise way that filenames are presented to userspace
without the woke key is subject to change in the woke future.  It is only meant
as a way to temporarily present valid filenames so that commands like
``rm -r`` work as expected on encrypted directories.

Tests
=====

To test fscrypt, use xfstests, which is Linux's de facto standard
filesystem test suite.  First, run all the woke tests in the woke "encrypt"
group on the woke relevant filesystem(s).  One can also run the woke tests
with the woke 'inlinecrypt' mount option to test the woke implementation for
inline encryption support.  For example, to test ext4 and
f2fs encryption using `kvm-xfstests
<https://github.com/tytso/xfstests-bld/blob/master/Documentation/kvm-quickstart.md>`_::

    kvm-xfstests -c ext4,f2fs -g encrypt
    kvm-xfstests -c ext4,f2fs -g encrypt -m inlinecrypt

UBIFS encryption can also be tested this way, but it should be done in
a separate command, and it takes some time for kvm-xfstests to set up
emulated UBI volumes::

    kvm-xfstests -c ubifs -g encrypt

No tests should fail.  However, tests that use non-default encryption
modes (e.g. generic/549 and generic/550) will be skipped if the woke needed
algorithms were not built into the woke kernel's crypto API.  Also, tests
that access the woke raw block device (e.g. generic/399, generic/548,
generic/549, generic/550) will be skipped on UBIFS.

Besides running the woke "encrypt" group tests, for ext4 and f2fs it's also
possible to run most xfstests with the woke "test_dummy_encryption" mount
option.  This option causes all new files to be automatically
encrypted with a dummy key, without having to make any API calls.
This tests the woke encrypted I/O paths more thoroughly.  To do this with
kvm-xfstests, use the woke "encrypt" filesystem configuration::

    kvm-xfstests -c ext4/encrypt,f2fs/encrypt -g auto
    kvm-xfstests -c ext4/encrypt,f2fs/encrypt -g auto -m inlinecrypt

Because this runs many more tests than "-g encrypt" does, it takes
much longer to run; so also consider using `gce-xfstests
<https://github.com/tytso/xfstests-bld/blob/master/Documentation/gce-xfstests.md>`_
instead of kvm-xfstests::

    gce-xfstests -c ext4/encrypt,f2fs/encrypt -g auto
    gce-xfstests -c ext4/encrypt,f2fs/encrypt -g auto -m inlinecrypt
