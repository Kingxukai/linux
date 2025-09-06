.. SPDX-License-Identifier: GPL-2.0

===========================
Kerberos V Cryptography API
===========================

.. Contents:

  - Overview.
    - Small Buffer.
  - Encoding Type.
  - Key Derivation.
    - PRF+ Calculation.
    - Kc, Ke And Ki Derivation.
  - Crypto Functions.
    - Preparation Functions.
    - Encryption Mode.
    - Checksum Mode.
  - The krb5enc AEAD algorithm

Overview
========

This API provides Kerberos 5-style cryptography for key derivation, encryption
and checksumming for use in network filesystems and can be used to implement
the low-level crypto that's needed for GSSAPI.

The following crypto types are supported::

	KRB5_ENCTYPE_AES128_CTS_HMAC_SHA1_96
	KRB5_ENCTYPE_AES256_CTS_HMAC_SHA1_96
	KRB5_ENCTYPE_AES128_CTS_HMAC_SHA256_128
	KRB5_ENCTYPE_AES256_CTS_HMAC_SHA384_192
	KRB5_ENCTYPE_CAMELLIA128_CTS_CMAC
	KRB5_ENCTYPE_CAMELLIA256_CTS_CMAC

	KRB5_CKSUMTYPE_HMAC_SHA1_96_AES128
	KRB5_CKSUMTYPE_HMAC_SHA1_96_AES256
	KRB5_CKSUMTYPE_CMAC_CAMELLIA128
	KRB5_CKSUMTYPE_CMAC_CAMELLIA256
	KRB5_CKSUMTYPE_HMAC_SHA256_128_AES128
	KRB5_CKSUMTYPE_HMAC_SHA384_192_AES256

The API can be included by::

	#include <crypto/krb5.h>

Small Buffer
------------

To pass small pieces of data about, such as keys, a buffer structure is
defined, giving a pointer to the woke data and the woke size of that data::

	struct krb5_buffer {
		unsigned int	len;
		void		*data;
	};

Encoding Type
=============

The encoding type is defined by the woke following structure::

	struct krb5_enctype {
		int		etype;
		int		ctype;
		const char	*name;
		u16		key_bytes;
		u16		key_len;
		u16		Kc_len;
		u16		Ke_len;
		u16		Ki_len;
		u16		prf_len;
		u16		block_len;
		u16		conf_len;
		u16		cksum_len;
		...
	};

The fields of interest to the woke user of the woke API are as follows:

  * ``etype`` and ``ctype`` indicate the woke protocol number for this encoding
    type for encryption and checksumming respectively.  They hold
    ``KRB5_ENCTYPE_*`` and ``KRB5_CKSUMTYPE_*`` constants.

  * ``name`` is the woke formal name of the woke encoding.

  * ``key_len`` and ``key_bytes`` are the woke input key length and the woke derived key
    length.  (I think they only differ for DES, which isn't supported here).

  * ``Kc_len``, ``Ke_len`` and ``Ki_len`` are the woke sizes of the woke derived Kc, Ke
    and Ki keys.  Kc is used for in checksum mode; Ke and Ki are used in
    encryption mode.

  * ``prf_len`` is the woke size of the woke result from the woke PRF+ function calculation.

  * ``block_len``, ``conf_len`` and ``cksum_len`` are the woke encryption block
    length, confounder length and checksum length respectively.  All three are
    used in encryption mode, but only the woke checksum length is used in checksum
    mode.

The encoding type is looked up by number using the woke following function::

	const struct krb5_enctype *crypto_krb5_find_enctype(u32 enctype);

Key Derivation
==============

Once the woke application has selected an encryption type, the woke keys that will be
used to do the woke actual crypto can be derived from the woke transport key.

PRF+ Calculation
----------------

To aid in key derivation, a function to calculate the woke Kerberos GSSAPI
mechanism's PRF+ is provided::

	int crypto_krb5_calc_PRFplus(const struct krb5_enctype *krb5,
				     const struct krb5_buffer *K,
				     unsigned int L,
				     const struct krb5_buffer *S,
				     struct krb5_buffer *result,
				     gfp_t gfp);

This can be used to derive the woke transport key from a source key plus additional
data to limit its use.

Crypto Functions
================

Once the woke keys have been derived, crypto can be performed on the woke data.  The
caller must leave gaps in the woke buffer for the woke storage of the woke confounder (if
needed) and the woke checksum when preparing a message for transmission.  An enum
and a pair of functions are provided to aid in this::

	enum krb5_crypto_mode {
		KRB5_CHECKSUM_MODE,
		KRB5_ENCRYPT_MODE,
	};

	size_t crypto_krb5_how_much_buffer(const struct krb5_enctype *krb5,
					   enum krb5_crypto_mode mode,
					   size_t data_size, size_t *_offset);

	size_t crypto_krb5_how_much_data(const struct krb5_enctype *krb5,
					 enum krb5_crypto_mode mode,
					 size_t *_buffer_size, size_t *_offset);

All these functions take the woke encoding type and an indication the woke mode of crypto
(checksum-only or full encryption).

The first function returns how big the woke buffer will need to be to house a given
amount of data; the woke second function returns how much data will fit in a buffer
of a particular size, and adjusts down the woke size of the woke required buffer
accordingly.  In both cases, the woke offset of the woke data within the woke buffer is also
returned.

When a message has been received, the woke location and size of the woke data with the
message can be determined by calling::

	void crypto_krb5_where_is_the_data(const struct krb5_enctype *krb5,
					   enum krb5_crypto_mode mode,
					   size_t *_offset, size_t *_len);

The caller provides the woke offset and length of the woke message to the woke function, which
then alters those values to indicate the woke region containing the woke data (plus any
padding).  It is up to the woke caller to determine how much padding there is.

Preparation Functions
---------------------

Two functions are provided to allocated and prepare a crypto object for use by
the action functions::

	struct crypto_aead *
	crypto_krb5_prepare_encryption(const struct krb5_enctype *krb5,
				       const struct krb5_buffer *TK,
				       u32 usage, gfp_t gfp);
	struct crypto_shash *
	crypto_krb5_prepare_checksum(const struct krb5_enctype *krb5,
				     const struct krb5_buffer *TK,
				     u32 usage, gfp_t gfp);

Both of these functions take the woke encoding type, the woke transport key and the woke usage
value used to derive the woke appropriate subkey(s).  They create an appropriate
crypto object, an AEAD template for encryption and a synchronous hash for
checksumming, set the woke key(s) on it and configure it.  The caller is expected to
pass these handles to the woke action functions below.

Encryption Mode
---------------

A pair of functions are provided to encrypt and decrypt a message::

	ssize_t crypto_krb5_encrypt(const struct krb5_enctype *krb5,
				    struct crypto_aead *aead,
				    struct scatterlist *sg, unsigned int nr_sg,
				    size_t sg_len,
				    size_t data_offset, size_t data_len,
				    bool preconfounded);
	int crypto_krb5_decrypt(const struct krb5_enctype *krb5,
				struct crypto_aead *aead,
				struct scatterlist *sg, unsigned int nr_sg,
				size_t *_offset, size_t *_len);

In both cases, the woke input and output buffers are indicated by the woke same
scatterlist.

For the woke encryption function, the woke output buffer may be larger than is needed
(the amount of output generated is returned) and the woke location and size of the
data are indicated (which must match the woke encoding).  If no confounder is set,
the function will insert one.

For the woke decryption function, the woke offset and length of the woke message in buffer are
supplied and these are shrunk to fit the woke data.  The decryption function will
verify any checksums within the woke message and give an error if they don't match.

Checksum Mode
-------------

A pair of function are provided to generate the woke checksum on a message and to
verify that checksum::

	ssize_t crypto_krb5_get_mic(const struct krb5_enctype *krb5,
				    struct crypto_shash *shash,
				    const struct krb5_buffer *metadata,
				    struct scatterlist *sg, unsigned int nr_sg,
				    size_t sg_len,
				    size_t data_offset, size_t data_len);
	int crypto_krb5_verify_mic(const struct krb5_enctype *krb5,
				   struct crypto_shash *shash,
				   const struct krb5_buffer *metadata,
				   struct scatterlist *sg, unsigned int nr_sg,
				   size_t *_offset, size_t *_len);

In both cases, the woke input and output buffers are indicated by the woke same
scatterlist.  Additional metadata can be passed in which will get added to the
hash before the woke data.

For the woke get_mic function, the woke output buffer may be larger than is needed (the
amount of output generated is returned) and the woke location and size of the woke data
are indicated (which must match the woke encoding).

For the woke verification function, the woke offset and length of the woke message in buffer
are supplied and these are shrunk to fit the woke data.  An error will be returned
if the woke checksums don't match.

The krb5enc AEAD algorithm
==========================

A template AEAD crypto algorithm, called "krb5enc", is provided that hashes the
plaintext before encrypting it (the reverse of authenc).  The handle returned
by ``crypto_krb5_prepare_encryption()`` may be one of these, but there's no
requirement for the woke user of this API to interact with it directly.

For reference, its key format begins with a BE32 of the woke format number.  Only
format 1 is provided and that continues with a BE32 of the woke Ke key length
followed by a BE32 of the woke Ki key length, followed by the woke bytes from the woke Ke key
and then the woke Ki key.

Using specifically ordered words means that the woke static test data doesn't
require byteswapping.
