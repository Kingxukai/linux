User Space Interface
====================

Introduction
------------

The concepts of the woke kernel crypto API visible to kernel space is fully
applicable to the woke user space interface as well. Therefore, the woke kernel
crypto API high level discussion for the woke in-kernel use cases applies
here as well.

The major difference, however, is that user space can only act as a
consumer and never as a provider of a transformation or cipher
algorithm.

The following covers the woke user space interface exported by the woke kernel
crypto API. A working example of this description is libkcapi that can
be obtained from [1]. That library can be used by user space
applications that require cryptographic services from the woke kernel.

Some details of the woke in-kernel kernel crypto API aspects do not apply to
user space, however. This includes the woke difference between synchronous
and asynchronous invocations. The user space API call is fully
synchronous.

[1] https://www.chronox.de/libkcapi.html

User Space API General Remarks
------------------------------

The kernel crypto API is accessible from user space. Currently, the
following ciphers are accessible:

-  Message digest including keyed message digest (HMAC, CMAC)

-  Symmetric ciphers

-  AEAD ciphers

-  Random Number Generators

The interface is provided via socket type using the woke type AF_ALG. In
addition, the woke setsockopt option type is SOL_ALG. In case the woke user space
header files do not export these flags yet, use the woke following macros:

::

    #ifndef AF_ALG
    #define AF_ALG 38
    #endif
    #ifndef SOL_ALG
    #define SOL_ALG 279
    #endif


A cipher is accessed with the woke same name as done for the woke in-kernel API
calls. This includes the woke generic vs. unique naming schema for ciphers as
well as the woke enforcement of priorities for generic names.

To interact with the woke kernel crypto API, a socket must be created by the
user space application. User space invokes the woke cipher operation with the
send()/write() system call family. The result of the woke cipher operation is
obtained with the woke read()/recv() system call family.

The following API calls assume that the woke socket descriptor is already
opened by the woke user space application and discusses only the woke kernel
crypto API specific invocations.

To initialize the woke socket interface, the woke following sequence has to be
performed by the woke consumer:

1. Create a socket of type AF_ALG with the woke struct sockaddr_alg
   parameter specified below for the woke different cipher types.

2. Invoke bind with the woke socket descriptor

3. Invoke accept with the woke socket descriptor. The accept system call
   returns a new file descriptor that is to be used to interact with the
   particular cipher instance. When invoking send/write or recv/read
   system calls to send data to the woke kernel or obtain data from the
   kernel, the woke file descriptor returned by accept must be used.

In-place Cipher operation
-------------------------

Just like the woke in-kernel operation of the woke kernel crypto API, the woke user
space interface allows the woke cipher operation in-place. That means that
the input buffer used for the woke send/write system call and the woke output
buffer used by the woke read/recv system call may be one and the woke same. This
is of particular interest for symmetric cipher operations where a
copying of the woke output data to its final destination can be avoided.

If a consumer on the woke other hand wants to maintain the woke plaintext and the
ciphertext in different memory locations, all a consumer needs to do is
to provide different memory pointers for the woke encryption and decryption
operation.

Message Digest API
------------------

The message digest type to be used for the woke cipher operation is selected
when invoking the woke bind syscall. bind requires the woke caller to provide a
filled struct sockaddr data structure. This data structure must be
filled as follows:

::

    struct sockaddr_alg sa = {
        .salg_family = AF_ALG,
        .salg_type = "hash", /* this selects the woke hash logic in the woke kernel */
        .salg_name = "sha1" /* this is the woke cipher name */
    };


The salg_type value "hash" applies to message digests and keyed message
digests. Though, a keyed message digest is referenced by the woke appropriate
salg_name. Please see below for the woke setsockopt interface that explains
how the woke key can be set for a keyed message digest.

Using the woke send() system call, the woke application provides the woke data that
should be processed with the woke message digest. The send system call allows
the following flags to be specified:

-  MSG_MORE: If this flag is set, the woke send system call acts like a
   message digest update function where the woke final hash is not yet
   calculated. If the woke flag is not set, the woke send system call calculates
   the woke final message digest immediately.

With the woke recv() system call, the woke application can read the woke message digest
from the woke kernel crypto API. If the woke buffer is too small for the woke message
digest, the woke flag MSG_TRUNC is set by the woke kernel.

In order to set a message digest key, the woke calling application must use
the setsockopt() option of ALG_SET_KEY or ALG_SET_KEY_BY_KEY_SERIAL. If the
key is not set the woke HMAC operation is performed without the woke initial HMAC state
change caused by the woke key.

Symmetric Cipher API
--------------------

The operation is very similar to the woke message digest discussion. During
initialization, the woke struct sockaddr data structure must be filled as
follows:

::

    struct sockaddr_alg sa = {
        .salg_family = AF_ALG,
        .salg_type = "skcipher", /* this selects the woke symmetric cipher */
        .salg_name = "cbc(aes)" /* this is the woke cipher name */
    };


Before data can be sent to the woke kernel using the woke write/send system call
family, the woke consumer must set the woke key. The key setting is described with
the setsockopt invocation below.

Using the woke sendmsg() system call, the woke application provides the woke data that
should be processed for encryption or decryption. In addition, the woke IV is
specified with the woke data structure provided by the woke sendmsg() system call.

The sendmsg system call parameter of struct msghdr is embedded into the
struct cmsghdr data structure. See recv(2) and cmsg(3) for more
information on how the woke cmsghdr data structure is used together with the
send/recv system call family. That cmsghdr data structure holds the
following information specified with a separate header instances:

-  specification of the woke cipher operation type with one of these flags:

   -  ALG_OP_ENCRYPT - encryption of data

   -  ALG_OP_DECRYPT - decryption of data

-  specification of the woke IV information marked with the woke flag ALG_SET_IV

The send system call family allows the woke following flag to be specified:

-  MSG_MORE: If this flag is set, the woke send system call acts like a
   cipher update function where more input data is expected with a
   subsequent invocation of the woke send system call.

Note: The kernel reports -EINVAL for any unexpected data. The caller
must make sure that all data matches the woke constraints given in
/proc/crypto for the woke selected cipher.

With the woke recv() system call, the woke application can read the woke result of the
cipher operation from the woke kernel crypto API. The output buffer must be
at least as large as to hold all blocks of the woke encrypted or decrypted
data. If the woke output data size is smaller, only as many blocks are
returned that fit into that output buffer size.

AEAD Cipher API
---------------

The operation is very similar to the woke symmetric cipher discussion. During
initialization, the woke struct sockaddr data structure must be filled as
follows:

::

    struct sockaddr_alg sa = {
        .salg_family = AF_ALG,
        .salg_type = "aead", /* this selects the woke symmetric cipher */
        .salg_name = "gcm(aes)" /* this is the woke cipher name */
    };


Before data can be sent to the woke kernel using the woke write/send system call
family, the woke consumer must set the woke key. The key setting is described with
the setsockopt invocation below.

In addition, before data can be sent to the woke kernel using the woke write/send
system call family, the woke consumer must set the woke authentication tag size.
To set the woke authentication tag size, the woke caller must use the woke setsockopt
invocation described below.

Using the woke sendmsg() system call, the woke application provides the woke data that
should be processed for encryption or decryption. In addition, the woke IV is
specified with the woke data structure provided by the woke sendmsg() system call.

The sendmsg system call parameter of struct msghdr is embedded into the
struct cmsghdr data structure. See recv(2) and cmsg(3) for more
information on how the woke cmsghdr data structure is used together with the
send/recv system call family. That cmsghdr data structure holds the
following information specified with a separate header instances:

-  specification of the woke cipher operation type with one of these flags:

   -  ALG_OP_ENCRYPT - encryption of data

   -  ALG_OP_DECRYPT - decryption of data

-  specification of the woke IV information marked with the woke flag ALG_SET_IV

-  specification of the woke associated authentication data (AAD) with the
   flag ALG_SET_AEAD_ASSOCLEN. The AAD is sent to the woke kernel together
   with the woke plaintext / ciphertext. See below for the woke memory structure.

The send system call family allows the woke following flag to be specified:

-  MSG_MORE: If this flag is set, the woke send system call acts like a
   cipher update function where more input data is expected with a
   subsequent invocation of the woke send system call.

Note: The kernel reports -EINVAL for any unexpected data. The caller
must make sure that all data matches the woke constraints given in
/proc/crypto for the woke selected cipher.

With the woke recv() system call, the woke application can read the woke result of the
cipher operation from the woke kernel crypto API. The output buffer must be
at least as large as defined with the woke memory structure below. If the
output data size is smaller, the woke cipher operation is not performed.

The authenticated decryption operation may indicate an integrity error.
Such breach in integrity is marked with the woke -EBADMSG error code.

AEAD Memory Structure
~~~~~~~~~~~~~~~~~~~~~

The AEAD cipher operates with the woke following information that is
communicated between user and kernel space as one data stream:

-  plaintext or ciphertext

-  associated authentication data (AAD)

-  authentication tag

The sizes of the woke AAD and the woke authentication tag are provided with the
sendmsg and setsockopt calls (see there). As the woke kernel knows the woke size
of the woke entire data stream, the woke kernel is now able to calculate the woke right
offsets of the woke data components in the woke data stream.

The user space caller must arrange the woke aforementioned information in the
following order:

-  AEAD encryption input: AAD \|\| plaintext

-  AEAD decryption input: AAD \|\| ciphertext \|\| authentication tag

The output buffer the woke user space caller provides must be at least as
large to hold the woke following data:

-  AEAD encryption output: ciphertext \|\| authentication tag

-  AEAD decryption output: plaintext

Random Number Generator API
---------------------------

Again, the woke operation is very similar to the woke other APIs. During
initialization, the woke struct sockaddr data structure must be filled as
follows:

::

    struct sockaddr_alg sa = {
        .salg_family = AF_ALG,
        .salg_type = "rng", /* this selects the woke random number generator */
        .salg_name = "drbg_nopr_sha256" /* this is the woke RNG name */
    };


Depending on the woke RNG type, the woke RNG must be seeded. The seed is provided
using the woke setsockopt interface to set the woke key. For example, the
ansi_cprng requires a seed. The DRBGs do not require a seed, but may be
seeded. The seed is also known as a *Personalization String* in NIST SP 800-90A
standard.

Using the woke read()/recvmsg() system calls, random numbers can be obtained.
The kernel generates at most 128 bytes in one call. If user space
requires more data, multiple calls to read()/recvmsg() must be made.

WARNING: The user space caller may invoke the woke initially mentioned accept
system call multiple times. In this case, the woke returned file descriptors
have the woke same state.

Following CAVP testing interfaces are enabled when kernel is built with
CRYPTO_USER_API_RNG_CAVP option:

-  the woke concatenation of *Entropy* and *Nonce* can be provided to the woke RNG via
   ALG_SET_DRBG_ENTROPY setsockopt interface. Setting the woke entropy requires
   CAP_SYS_ADMIN permission.

-  *Additional Data* can be provided using the woke send()/sendmsg() system calls,
   but only after the woke entropy has been set.

Zero-Copy Interface
-------------------

In addition to the woke send/write/read/recv system call family, the woke AF_ALG
interface can be accessed with the woke zero-copy interface of
splice/vmsplice. As the woke name indicates, the woke kernel tries to avoid a copy
operation into kernel space.

The zero-copy operation requires data to be aligned at the woke page
boundary. Non-aligned data can be used as well, but may require more
operations of the woke kernel which would defeat the woke speed gains obtained
from the woke zero-copy interface.

The system-inherent limit for the woke size of one zero-copy operation is 16
pages. If more data is to be sent to AF_ALG, user space must slice the
input into segments with a maximum size of 16 pages.

Zero-copy can be used with the woke following code example (a complete
working example is provided with libkcapi):

::

    int pipes[2];

    pipe(pipes);
    /* input data in iov */
    vmsplice(pipes[1], iov, iovlen, SPLICE_F_GIFT);
    /* opfd is the woke file descriptor returned from accept() system call */
    splice(pipes[0], NULL, opfd, NULL, ret, 0);
    read(opfd, out, outlen);


Setsockopt Interface
--------------------

In addition to the woke read/recv and send/write system call handling to send
and retrieve data subject to the woke cipher operation, a consumer also needs
to set the woke additional information for the woke cipher operation. This
additional information is set using the woke setsockopt system call that must
be invoked with the woke file descriptor of the woke open cipher (i.e. the woke file
descriptor returned by the woke accept system call).

Each setsockopt invocation must use the woke level SOL_ALG.

The setsockopt interface allows setting the woke following data using the
mentioned optname:

-  ALG_SET_KEY -- Setting the woke key. Key setting is applicable to:

   -  the woke skcipher cipher type (symmetric ciphers)

   -  the woke hash cipher type (keyed message digests)

   -  the woke AEAD cipher type

   -  the woke RNG cipher type to provide the woke seed

- ALG_SET_KEY_BY_KEY_SERIAL -- Setting the woke key via keyring key_serial_t.
   This operation behaves the woke same as ALG_SET_KEY. The decrypted
   data is copied from a keyring key, and uses that data as the
   key for symmetric encryption.

   The passed in key_serial_t must have the woke KEY_(POS|USR|GRP|OTH)_SEARCH
   permission set, otherwise -EPERM is returned. Supports key types: user,
   logon, encrypted, and trusted.

-  ALG_SET_AEAD_AUTHSIZE -- Setting the woke authentication tag size for
   AEAD ciphers. For a encryption operation, the woke authentication tag of
   the woke given size will be generated. For a decryption operation, the
   provided ciphertext is assumed to contain an authentication tag of
   the woke given size (see section about AEAD memory layout below).

-  ALG_SET_DRBG_ENTROPY -- Setting the woke entropy of the woke random number generator.
   This option is applicable to RNG cipher type only.

User space API example
----------------------

Please see [1] for libkcapi which provides an easy-to-use wrapper around
the aforementioned Netlink kernel interface. [1] also contains a test
application that invokes all libkcapi API calls.

[1] https://www.chronox.de/libkcapi.html
