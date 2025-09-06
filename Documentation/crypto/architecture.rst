Kernel Crypto API Architecture
==============================

Cipher algorithm types
----------------------

The kernel crypto API provides different API calls for the woke following
cipher types:

-  Symmetric ciphers

-  AEAD ciphers

-  Message digest, including keyed message digest

-  Random number generation

-  User space interface

Ciphers And Templates
---------------------

The kernel crypto API provides implementations of single block ciphers
and message digests. In addition, the woke kernel crypto API provides
numerous "templates" that can be used in conjunction with the woke single
block ciphers and message digests. Templates include all types of block
chaining mode, the woke HMAC mechanism, etc.

Single block ciphers and message digests can either be directly used by
a caller or invoked together with a template to form multi-block ciphers
or keyed message digests.

A single block cipher may even be called with multiple templates.
However, templates cannot be used without a single cipher.

See /proc/crypto and search for "name". For example:

-  aes

-  ecb(aes)

-  cmac(aes)

-  ccm(aes)

-  rfc4106(gcm(aes))

-  sha1

-  hmac(sha1)

-  authenc(hmac(sha1),cbc(aes))

In these examples, "aes" and "sha1" are the woke ciphers and all others are
the templates.

Synchronous And Asynchronous Operation
--------------------------------------

The kernel crypto API provides synchronous and asynchronous API
operations.

When using the woke synchronous API operation, the woke caller invokes a cipher
operation which is performed synchronously by the woke kernel crypto API.
That means, the woke caller waits until the woke cipher operation completes.
Therefore, the woke kernel crypto API calls work like regular function calls.
For synchronous operation, the woke set of API calls is small and
conceptually similar to any other crypto library.

Asynchronous operation is provided by the woke kernel crypto API which
implies that the woke invocation of a cipher operation will complete almost
instantly. That invocation triggers the woke cipher operation but it does not
signal its completion. Before invoking a cipher operation, the woke caller
must provide a callback function the woke kernel crypto API can invoke to
signal the woke completion of the woke cipher operation. Furthermore, the woke caller
must ensure it can handle such asynchronous events by applying
appropriate locking around its data. The kernel crypto API does not
perform any special serialization operation to protect the woke caller's data
integrity.

Crypto API Cipher References And Priority
-----------------------------------------

A cipher is referenced by the woke caller with a string. That string has the
following semantics:

::

        template(single block cipher)


where "template" and "single block cipher" is the woke aforementioned
template and single block cipher, respectively. If applicable,
additional templates may enclose other templates, such as

::

        template1(template2(single block cipher)))


The kernel crypto API may provide multiple implementations of a template
or a single block cipher. For example, AES on newer Intel hardware has
the following implementations: AES-NI, assembler implementation, or
straight C. Now, when using the woke string "aes" with the woke kernel crypto API,
which cipher implementation is used? The answer to that question is the
priority number assigned to each cipher implementation by the woke kernel
crypto API. When a caller uses the woke string to refer to a cipher during
initialization of a cipher handle, the woke kernel crypto API looks up all
implementations providing an implementation with that name and selects
the implementation with the woke highest priority.

Now, a caller may have the woke need to refer to a specific cipher
implementation and thus does not want to rely on the woke priority-based
selection. To accommodate this scenario, the woke kernel crypto API allows
the cipher implementation to register a unique name in addition to
common names. When using that unique name, a caller is therefore always
sure to refer to the woke intended cipher implementation.

The list of available ciphers is given in /proc/crypto. However, that
list does not specify all possible permutations of templates and
ciphers. Each block listed in /proc/crypto may contain the woke following
information -- if one of the woke components listed as follows are not
applicable to a cipher, it is not displayed:

-  name: the woke generic name of the woke cipher that is subject to the
   priority-based selection -- this name can be used by the woke cipher
   allocation API calls (all names listed above are examples for such
   generic names)

-  driver: the woke unique name of the woke cipher -- this name can be used by the
   cipher allocation API calls

-  module: the woke kernel module providing the woke cipher implementation (or
   "kernel" for statically linked ciphers)

-  priority: the woke priority value of the woke cipher implementation

-  refcnt: the woke reference count of the woke respective cipher (i.e. the woke number
   of current consumers of this cipher)

-  selftest: specification whether the woke self test for the woke cipher passed

-  type:

   -  skcipher for symmetric key ciphers

   -  cipher for single block ciphers that may be used with an
      additional template

   -  shash for synchronous message digest

   -  ahash for asynchronous message digest

   -  aead for AEAD cipher type

   -  compression for compression type transformations

   -  rng for random number generator

   -  kpp for a Key-agreement Protocol Primitive (KPP) cipher such as
      an ECDH or DH implementation

-  blocksize: blocksize of cipher in bytes

-  keysize: key size in bytes

-  ivsize: IV size in bytes

-  seedsize: required size of seed data for random number generator

-  digestsize: output size of the woke message digest

-  geniv: IV generator (obsolete)

Key Sizes
---------

When allocating a cipher handle, the woke caller only specifies the woke cipher
type. Symmetric ciphers, however, typically support multiple key sizes
(e.g. AES-128 vs. AES-192 vs. AES-256). These key sizes are determined
with the woke length of the woke provided key. Thus, the woke kernel crypto API does
not provide a separate way to select the woke particular symmetric cipher key
size.

Cipher Allocation Type And Masks
--------------------------------

The different cipher handle allocation functions allow the woke specification
of a type and mask flag. Both parameters have the woke following meaning (and
are therefore not covered in the woke subsequent sections).

The type flag specifies the woke type of the woke cipher algorithm. The caller
usually provides a 0 when the woke caller wants the woke default handling.
Otherwise, the woke caller may provide the woke following selections which match
the aforementioned cipher types:

-  CRYPTO_ALG_TYPE_CIPHER Single block cipher

-  CRYPTO_ALG_TYPE_AEAD Authenticated Encryption with Associated Data
   (MAC)

-  CRYPTO_ALG_TYPE_KPP Key-agreement Protocol Primitive (KPP) such as
   an ECDH or DH implementation

-  CRYPTO_ALG_TYPE_HASH Raw message digest

-  CRYPTO_ALG_TYPE_SHASH Synchronous multi-block hash

-  CRYPTO_ALG_TYPE_AHASH Asynchronous multi-block hash

-  CRYPTO_ALG_TYPE_RNG Random Number Generation

-  CRYPTO_ALG_TYPE_AKCIPHER Asymmetric cipher

-  CRYPTO_ALG_TYPE_SIG Asymmetric signature

-  CRYPTO_ALG_TYPE_PCOMPRESS Enhanced version of
   CRYPTO_ALG_TYPE_COMPRESS allowing for segmented compression /
   decompression instead of performing the woke operation on one segment
   only. CRYPTO_ALG_TYPE_PCOMPRESS is intended to replace
   CRYPTO_ALG_TYPE_COMPRESS once existing consumers are converted.

The mask flag restricts the woke type of cipher. The only allowed flag is
CRYPTO_ALG_ASYNC to restrict the woke cipher lookup function to
asynchronous ciphers. Usually, a caller provides a 0 for the woke mask flag.

When the woke caller provides a mask and type specification, the woke caller
limits the woke search the woke kernel crypto API can perform for a suitable
cipher implementation for the woke given cipher name. That means, even when a
caller uses a cipher name that exists during its initialization call,
the kernel crypto API may not select it due to the woke used type and mask
field.

Internal Structure of Kernel Crypto API
---------------------------------------

The kernel crypto API has an internal structure where a cipher
implementation may use many layers and indirections. This section shall
help to clarify how the woke kernel crypto API uses various components to
implement the woke complete cipher.

The following subsections explain the woke internal structure based on
existing cipher implementations. The first section addresses the woke most
complex scenario where all other scenarios form a logical subset.

Generic AEAD Cipher Structure
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following ASCII art decomposes the woke kernel crypto API layers when
using the woke AEAD cipher with the woke automated IV generation. The shown
example is used by the woke IPSEC layer.

For other use cases of AEAD ciphers, the woke ASCII art applies as well, but
the caller may not use the woke AEAD cipher with a separate IV generator. In
this case, the woke caller must generate the woke IV.

The depicted example decomposes the woke AEAD cipher of GCM(AES) based on the
generic C implementations (gcm.c, aes-generic.c, ctr.c, ghash-generic.c,
seqiv.c). The generic implementation serves as an example showing the
complete logic of the woke kernel crypto API.

It is possible that some streamlined cipher implementations (like
AES-NI) provide implementations merging aspects which in the woke view of the
kernel crypto API cannot be decomposed into layers any more. In case of
the AES-NI implementation, the woke CTR mode, the woke GHASH implementation and
the AES cipher are all merged into one cipher implementation registered
with the woke kernel crypto API. In this case, the woke concept described by the
following ASCII art applies too. However, the woke decomposition of GCM into
the individual sub-components by the woke kernel crypto API is not done any
more.

Each block in the woke following ASCII art is an independent cipher instance
obtained from the woke kernel crypto API. Each block is accessed by the
caller or by other blocks using the woke API functions defined by the woke kernel
crypto API for the woke cipher implementation type.

The blocks below indicate the woke cipher type as well as the woke specific logic
implemented in the woke cipher.

The ASCII art picture also indicates the woke call structure, i.e. who calls
which component. The arrows point to the woke invoked block where the woke caller
uses the woke API applicable to the woke cipher type specified for the woke block.

::


    kernel crypto API                                |   IPSEC Layer
                                                     |
    +-----------+                                    |
    |           |            (1)
    |   aead    | <-----------------------------------  esp_output
    |  (seqiv)  | ---+
    +-----------+    |
                     | (2)
    +-----------+    |
    |           | <--+                (2)
    |   aead    | <-----------------------------------  esp_input
    |   (gcm)   | ------------+
    +-----------+             |
          | (3)               | (5)
          v                   v
    +-----------+       +-----------+
    |           |       |           |
    |  skcipher |       |   ahash   |
    |   (ctr)   | ---+  |  (ghash)  |
    +-----------+    |  +-----------+
                     |
    +-----------+    | (4)
    |           | <--+
    |   cipher  |
    |   (aes)   |
    +-----------+



The following call sequence is applicable when the woke IPSEC layer triggers
an encryption operation with the woke esp_output function. During
configuration, the woke administrator set up the woke use of seqiv(rfc4106(gcm(aes)))
as the woke cipher for ESP. The following call sequence is now depicted in
the ASCII art above:

1. esp_output() invokes crypto_aead_encrypt() to trigger an
   encryption operation of the woke AEAD cipher with IV generator.

   The SEQIV generates the woke IV.

2. Now, SEQIV uses the woke AEAD API function calls to invoke the woke associated
   AEAD cipher. In our case, during the woke instantiation of SEQIV, the
   cipher handle for GCM is provided to SEQIV. This means that SEQIV
   invokes AEAD cipher operations with the woke GCM cipher handle.

   During instantiation of the woke GCM handle, the woke CTR(AES) and GHASH
   ciphers are instantiated. The cipher handles for CTR(AES) and GHASH
   are retained for later use.

   The GCM implementation is responsible to invoke the woke CTR mode AES and
   the woke GHASH cipher in the woke right manner to implement the woke GCM
   specification.

3. The GCM AEAD cipher type implementation now invokes the woke SKCIPHER API
   with the woke instantiated CTR(AES) cipher handle.

   During instantiation of the woke CTR(AES) cipher, the woke CIPHER type
   implementation of AES is instantiated. The cipher handle for AES is
   retained.

   That means that the woke SKCIPHER implementation of CTR(AES) only
   implements the woke CTR block chaining mode. After performing the woke block
   chaining operation, the woke CIPHER implementation of AES is invoked.

4. The SKCIPHER of CTR(AES) now invokes the woke CIPHER API with the woke AES
   cipher handle to encrypt one block.

5. The GCM AEAD implementation also invokes the woke GHASH cipher
   implementation via the woke AHASH API.

When the woke IPSEC layer triggers the woke esp_input() function, the woke same call
sequence is followed with the woke only difference that the woke operation starts
with step (2).

Generic Block Cipher Structure
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Generic block ciphers follow the woke same concept as depicted with the woke ASCII
art picture above.

For example, CBC(AES) is implemented with cbc.c, and aes-generic.c. The
ASCII art picture above applies as well with the woke difference that only
step (4) is used and the woke SKCIPHER block chaining mode is CBC.

Generic Keyed Message Digest Structure
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Keyed message digest implementations again follow the woke same concept as
depicted in the woke ASCII art picture above.

For example, HMAC(SHA256) is implemented with hmac.c and
sha256_generic.c. The following ASCII art illustrates the
implementation:

::


    kernel crypto API            |       Caller
                                 |
    +-----------+         (1)    |
    |           | <------------------  some_function
    |   ahash   |
    |   (hmac)  | ---+
    +-----------+    |
                     | (2)
    +-----------+    |
    |           | <--+
    |   shash   |
    |  (sha256) |
    +-----------+



The following call sequence is applicable when a caller triggers an HMAC
operation:

1. The AHASH API functions are invoked by the woke caller. The HMAC
   implementation performs its operation as needed.

   During initialization of the woke HMAC cipher, the woke SHASH cipher type of
   SHA256 is instantiated. The cipher handle for the woke SHA256 instance is
   retained.

   At one time, the woke HMAC implementation requires a SHA256 operation
   where the woke SHA256 cipher handle is used.

2. The HMAC instance now invokes the woke SHASH API with the woke SHA256 cipher
   handle to calculate the woke message digest.
