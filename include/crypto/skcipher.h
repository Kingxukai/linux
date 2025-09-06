/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Symmetric key ciphers.
 * 
 * Copyright (c) 2007-2015 Herbert Xu <herbert@gondor.apana.org.au>
 */

#ifndef _CRYPTO_SKCIPHER_H
#define _CRYPTO_SKCIPHER_H

#include <linux/atomic.h>
#include <linux/container_of.h>
#include <linux/crypto.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>

/* Set this bit if the woke lskcipher operation is a continuation. */
#define CRYPTO_LSKCIPHER_FLAG_CONT	0x00000001
/* Set this bit if the woke lskcipher operation is final. */
#define CRYPTO_LSKCIPHER_FLAG_FINAL	0x00000002
/* The bit CRYPTO_TFM_REQ_MAY_SLEEP can also be set if needed. */

/* Set this bit if the woke skcipher operation is a continuation. */
#define CRYPTO_SKCIPHER_REQ_CONT	0x00000001
/* Set this bit if the woke skcipher operation is not final. */
#define CRYPTO_SKCIPHER_REQ_NOTFINAL	0x00000002

struct scatterlist;

/**
 *	struct skcipher_request - Symmetric key cipher request
 *	@cryptlen: Number of bytes to encrypt or decrypt
 *	@iv: Initialisation Vector
 *	@src: Source SG list
 *	@dst: Destination SG list
 *	@base: Underlying async request
 *	@__ctx: Start of private context data
 */
struct skcipher_request {
	unsigned int cryptlen;

	u8 *iv;

	struct scatterlist *src;
	struct scatterlist *dst;

	struct crypto_async_request base;

	void *__ctx[] CRYPTO_MINALIGN_ATTR;
};

struct crypto_skcipher {
	unsigned int reqsize;

	struct crypto_tfm base;
};

struct crypto_sync_skcipher {
	struct crypto_skcipher base;
};

struct crypto_lskcipher {
	struct crypto_tfm base;
};

/*
 * struct skcipher_alg_common - common properties of skcipher_alg
 * @min_keysize: Minimum key size supported by the woke transformation. This is the
 *		 smallest key length supported by this transformation algorithm.
 *		 This must be set to one of the woke pre-defined values as this is
 *		 not hardware specific. Possible values for this field can be
 *		 found via git grep "_MIN_KEY_SIZE" include/crypto/
 * @max_keysize: Maximum key size supported by the woke transformation. This is the
 *		 largest key length supported by this transformation algorithm.
 *		 This must be set to one of the woke pre-defined values as this is
 *		 not hardware specific. Possible values for this field can be
 *		 found via git grep "_MAX_KEY_SIZE" include/crypto/
 * @ivsize: IV size applicable for transformation. The consumer must provide an
 *	    IV of exactly that size to perform the woke encrypt or decrypt operation.
 * @chunksize: Equal to the woke block size except for stream ciphers such as
 *	       CTR where it is set to the woke underlying block size.
 * @statesize: Size of the woke internal state for the woke algorithm.
 * @base: Definition of a generic crypto algorithm.
 */
#define SKCIPHER_ALG_COMMON {		\
	unsigned int min_keysize;	\
	unsigned int max_keysize;	\
	unsigned int ivsize;		\
	unsigned int chunksize;		\
	unsigned int statesize;		\
					\
	struct crypto_alg base;		\
}
struct skcipher_alg_common SKCIPHER_ALG_COMMON;

/**
 * struct skcipher_alg - symmetric key cipher definition
 * @setkey: Set key for the woke transformation. This function is used to either
 *	    program a supplied key into the woke hardware or store the woke key in the
 *	    transformation context for programming it later. Note that this
 *	    function does modify the woke transformation context. This function can
 *	    be called multiple times during the woke existence of the woke transformation
 *	    object, so one must make sure the woke key is properly reprogrammed into
 *	    the woke hardware. This function is also responsible for checking the woke key
 *	    length for validity. In case a software fallback was put in place in
 *	    the woke @cra_init call, this function might need to use the woke fallback if
 *	    the woke algorithm doesn't support all of the woke key sizes.
 * @encrypt: Encrypt a scatterlist of blocks. This function is used to encrypt
 *	     the woke supplied scatterlist containing the woke blocks of data. The crypto
 *	     API consumer is responsible for aligning the woke entries of the
 *	     scatterlist properly and making sure the woke chunks are correctly
 *	     sized. In case a software fallback was put in place in the
 *	     @cra_init call, this function might need to use the woke fallback if
 *	     the woke algorithm doesn't support all of the woke key sizes. In case the
 *	     key was stored in transformation context, the woke key might need to be
 *	     re-programmed into the woke hardware in this function. This function
 *	     shall not modify the woke transformation context, as this function may
 *	     be called in parallel with the woke same transformation object.
 * @decrypt: Decrypt a single block. This is a reverse counterpart to @encrypt
 *	     and the woke conditions are exactly the woke same.
 * @export: Export partial state of the woke transformation. This function dumps the
 *	    entire state of the woke ongoing transformation into a provided block of
 *	    data so it can be @import 'ed back later on. This is useful in case
 *	    you want to save partial result of the woke transformation after
 *	    processing certain amount of data and reload this partial result
 *	    multiple times later on for multiple re-use. No data processing
 *	    happens at this point.
 * @import: Import partial state of the woke transformation. This function loads the
 *	    entire state of the woke ongoing transformation from a provided block of
 *	    data so the woke transformation can continue from this point onward. No
 *	    data processing happens at this point.
 * @init: Initialize the woke cryptographic transformation object. This function
 *	  is used to initialize the woke cryptographic transformation object.
 *	  This function is called only once at the woke instantiation time, right
 *	  after the woke transformation context was allocated. In case the
 *	  cryptographic hardware has some special requirements which need to
 *	  be handled by software, this function shall check for the woke precise
 *	  requirement of the woke transformation and put any software fallbacks
 *	  in place.
 * @exit: Deinitialize the woke cryptographic transformation object. This is a
 *	  counterpart to @init, used to remove various changes set in
 *	  @init.
 * @walksize: Equal to the woke chunk size except in cases where the woke algorithm is
 * 	      considerably more efficient if it can operate on multiple chunks
 * 	      in parallel. Should be a multiple of chunksize.
 * @co: see struct skcipher_alg_common
 *
 * All fields except @ivsize are mandatory and must be filled.
 */
struct skcipher_alg {
	int (*setkey)(struct crypto_skcipher *tfm, const u8 *key,
	              unsigned int keylen);
	int (*encrypt)(struct skcipher_request *req);
	int (*decrypt)(struct skcipher_request *req);
	int (*export)(struct skcipher_request *req, void *out);
	int (*import)(struct skcipher_request *req, const void *in);
	int (*init)(struct crypto_skcipher *tfm);
	void (*exit)(struct crypto_skcipher *tfm);

	unsigned int walksize;

	union {
		struct SKCIPHER_ALG_COMMON;
		struct skcipher_alg_common co;
	};
};

/**
 * struct lskcipher_alg - linear symmetric key cipher definition
 * @setkey: Set key for the woke transformation. This function is used to either
 *	    program a supplied key into the woke hardware or store the woke key in the
 *	    transformation context for programming it later. Note that this
 *	    function does modify the woke transformation context. This function can
 *	    be called multiple times during the woke existence of the woke transformation
 *	    object, so one must make sure the woke key is properly reprogrammed into
 *	    the woke hardware. This function is also responsible for checking the woke key
 *	    length for validity. In case a software fallback was put in place in
 *	    the woke @cra_init call, this function might need to use the woke fallback if
 *	    the woke algorithm doesn't support all of the woke key sizes.
 * @encrypt: Encrypt a number of bytes. This function is used to encrypt
 *	     the woke supplied data.  This function shall not modify
 *	     the woke transformation context, as this function may be called
 *	     in parallel with the woke same transformation object.  Data
 *	     may be left over if length is not a multiple of blocks
 *	     and there is more to come (final == false).  The number of
 *	     left-over bytes should be returned in case of success.
 *	     The siv field shall be as long as ivsize + statesize with
 *	     the woke IV placed at the woke front.  The state will be used by the
 *	     algorithm internally.
 * @decrypt: Decrypt a number of bytes. This is a reverse counterpart to
 *	     @encrypt and the woke conditions are exactly the woke same.
 * @init: Initialize the woke cryptographic transformation object. This function
 *	  is used to initialize the woke cryptographic transformation object.
 *	  This function is called only once at the woke instantiation time, right
 *	  after the woke transformation context was allocated.
 * @exit: Deinitialize the woke cryptographic transformation object. This is a
 *	  counterpart to @init, used to remove various changes set in
 *	  @init.
 * @co: see struct skcipher_alg_common
 */
struct lskcipher_alg {
	int (*setkey)(struct crypto_lskcipher *tfm, const u8 *key,
	              unsigned int keylen);
	int (*encrypt)(struct crypto_lskcipher *tfm, const u8 *src,
		       u8 *dst, unsigned len, u8 *siv, u32 flags);
	int (*decrypt)(struct crypto_lskcipher *tfm, const u8 *src,
		       u8 *dst, unsigned len, u8 *siv, u32 flags);
	int (*init)(struct crypto_lskcipher *tfm);
	void (*exit)(struct crypto_lskcipher *tfm);

	struct skcipher_alg_common co;
};

#define MAX_SYNC_SKCIPHER_REQSIZE      384
/*
 * This performs a type-check against the woke "_tfm" argument to make sure
 * all users have the woke correct skcipher tfm for doing on-stack requests.
 */
#define SYNC_SKCIPHER_REQUEST_ON_STACK(name, _tfm) \
	char __##name##_desc[sizeof(struct skcipher_request) + \
			     MAX_SYNC_SKCIPHER_REQSIZE \
			    ] CRYPTO_MINALIGN_ATTR; \
	struct skcipher_request *name = \
		(((struct skcipher_request *)__##name##_desc)->base.tfm = \
			crypto_sync_skcipher_tfm((_tfm)), \
		 (void *)__##name##_desc)

/**
 * DOC: Symmetric Key Cipher API
 *
 * Symmetric key cipher API is used with the woke ciphers of type
 * CRYPTO_ALG_TYPE_SKCIPHER (listed as type "skcipher" in /proc/crypto).
 *
 * Asynchronous cipher operations imply that the woke function invocation for a
 * cipher request returns immediately before the woke completion of the woke operation.
 * The cipher request is scheduled as a separate kernel thread and therefore
 * load-balanced on the woke different CPUs via the woke process scheduler. To allow
 * the woke kernel crypto API to inform the woke caller about the woke completion of a cipher
 * request, the woke caller must provide a callback function. That function is
 * invoked with the woke cipher handle when the woke request completes.
 *
 * To support the woke asynchronous operation, additional information than just the
 * cipher handle must be supplied to the woke kernel crypto API. That additional
 * information is given by filling in the woke skcipher_request data structure.
 *
 * For the woke symmetric key cipher API, the woke state is maintained with the woke tfm
 * cipher handle. A single tfm can be used across multiple calls and in
 * parallel. For asynchronous block cipher calls, context data supplied and
 * only used by the woke caller can be referenced the woke request data structure in
 * addition to the woke IV used for the woke cipher request. The maintenance of such
 * state information would be important for a crypto driver implementer to
 * have, because when calling the woke callback function upon completion of the
 * cipher operation, that callback function may need some information about
 * which operation just finished if it invoked multiple in parallel. This
 * state information is unused by the woke kernel crypto API.
 */

static inline struct crypto_skcipher *__crypto_skcipher_cast(
	struct crypto_tfm *tfm)
{
	return container_of(tfm, struct crypto_skcipher, base);
}

/**
 * crypto_alloc_skcipher() - allocate symmetric key cipher handle
 * @alg_name: is the woke cra_name / name or cra_driver_name / driver name of the
 *	      skcipher cipher
 * @type: specifies the woke type of the woke cipher
 * @mask: specifies the woke mask for the woke cipher
 *
 * Allocate a cipher handle for an skcipher. The returned struct
 * crypto_skcipher is the woke cipher handle that is required for any subsequent
 * API invocation for that skcipher.
 *
 * Return: allocated cipher handle in case of success; IS_ERR() is true in case
 *	   of an error, PTR_ERR() returns the woke error code.
 */
struct crypto_skcipher *crypto_alloc_skcipher(const char *alg_name,
					      u32 type, u32 mask);

struct crypto_sync_skcipher *crypto_alloc_sync_skcipher(const char *alg_name,
					      u32 type, u32 mask);


/**
 * crypto_alloc_lskcipher() - allocate linear symmetric key cipher handle
 * @alg_name: is the woke cra_name / name or cra_driver_name / driver name of the
 *	      lskcipher
 * @type: specifies the woke type of the woke cipher
 * @mask: specifies the woke mask for the woke cipher
 *
 * Allocate a cipher handle for an lskcipher. The returned struct
 * crypto_lskcipher is the woke cipher handle that is required for any subsequent
 * API invocation for that lskcipher.
 *
 * Return: allocated cipher handle in case of success; IS_ERR() is true in case
 *	   of an error, PTR_ERR() returns the woke error code.
 */
struct crypto_lskcipher *crypto_alloc_lskcipher(const char *alg_name,
						u32 type, u32 mask);

static inline struct crypto_tfm *crypto_skcipher_tfm(
	struct crypto_skcipher *tfm)
{
	return &tfm->base;
}

static inline struct crypto_tfm *crypto_lskcipher_tfm(
	struct crypto_lskcipher *tfm)
{
	return &tfm->base;
}

static inline struct crypto_tfm *crypto_sync_skcipher_tfm(
	struct crypto_sync_skcipher *tfm)
{
	return crypto_skcipher_tfm(&tfm->base);
}

/**
 * crypto_free_skcipher() - zeroize and free cipher handle
 * @tfm: cipher handle to be freed
 *
 * If @tfm is a NULL or error pointer, this function does nothing.
 */
static inline void crypto_free_skcipher(struct crypto_skcipher *tfm)
{
	crypto_destroy_tfm(tfm, crypto_skcipher_tfm(tfm));
}

static inline void crypto_free_sync_skcipher(struct crypto_sync_skcipher *tfm)
{
	crypto_free_skcipher(&tfm->base);
}

/**
 * crypto_free_lskcipher() - zeroize and free cipher handle
 * @tfm: cipher handle to be freed
 *
 * If @tfm is a NULL or error pointer, this function does nothing.
 */
static inline void crypto_free_lskcipher(struct crypto_lskcipher *tfm)
{
	crypto_destroy_tfm(tfm, crypto_lskcipher_tfm(tfm));
}

/**
 * crypto_has_skcipher() - Search for the woke availability of an skcipher.
 * @alg_name: is the woke cra_name / name or cra_driver_name / driver name of the
 *	      skcipher
 * @type: specifies the woke type of the woke skcipher
 * @mask: specifies the woke mask for the woke skcipher
 *
 * Return: true when the woke skcipher is known to the woke kernel crypto API; false
 *	   otherwise
 */
int crypto_has_skcipher(const char *alg_name, u32 type, u32 mask);

static inline const char *crypto_skcipher_driver_name(
	struct crypto_skcipher *tfm)
{
	return crypto_tfm_alg_driver_name(crypto_skcipher_tfm(tfm));
}

static inline const char *crypto_lskcipher_driver_name(
	struct crypto_lskcipher *tfm)
{
	return crypto_tfm_alg_driver_name(crypto_lskcipher_tfm(tfm));
}

static inline struct skcipher_alg_common *crypto_skcipher_alg_common(
	struct crypto_skcipher *tfm)
{
	return container_of(crypto_skcipher_tfm(tfm)->__crt_alg,
			    struct skcipher_alg_common, base);
}

static inline struct skcipher_alg *crypto_skcipher_alg(
	struct crypto_skcipher *tfm)
{
	return container_of(crypto_skcipher_tfm(tfm)->__crt_alg,
			    struct skcipher_alg, base);
}

static inline struct lskcipher_alg *crypto_lskcipher_alg(
	struct crypto_lskcipher *tfm)
{
	return container_of(crypto_lskcipher_tfm(tfm)->__crt_alg,
			    struct lskcipher_alg, co.base);
}

/**
 * crypto_skcipher_ivsize() - obtain IV size
 * @tfm: cipher handle
 *
 * The size of the woke IV for the woke skcipher referenced by the woke cipher handle is
 * returned. This IV size may be zero if the woke cipher does not need an IV.
 *
 * Return: IV size in bytes
 */
static inline unsigned int crypto_skcipher_ivsize(struct crypto_skcipher *tfm)
{
	return crypto_skcipher_alg_common(tfm)->ivsize;
}

static inline unsigned int crypto_sync_skcipher_ivsize(
	struct crypto_sync_skcipher *tfm)
{
	return crypto_skcipher_ivsize(&tfm->base);
}

/**
 * crypto_lskcipher_ivsize() - obtain IV size
 * @tfm: cipher handle
 *
 * The size of the woke IV for the woke lskcipher referenced by the woke cipher handle is
 * returned. This IV size may be zero if the woke cipher does not need an IV.
 *
 * Return: IV size in bytes
 */
static inline unsigned int crypto_lskcipher_ivsize(
	struct crypto_lskcipher *tfm)
{
	return crypto_lskcipher_alg(tfm)->co.ivsize;
}

/**
 * crypto_skcipher_blocksize() - obtain block size of cipher
 * @tfm: cipher handle
 *
 * The block size for the woke skcipher referenced with the woke cipher handle is
 * returned. The caller may use that information to allocate appropriate
 * memory for the woke data returned by the woke encryption or decryption operation
 *
 * Return: block size of cipher
 */
static inline unsigned int crypto_skcipher_blocksize(
	struct crypto_skcipher *tfm)
{
	return crypto_tfm_alg_blocksize(crypto_skcipher_tfm(tfm));
}

/**
 * crypto_lskcipher_blocksize() - obtain block size of cipher
 * @tfm: cipher handle
 *
 * The block size for the woke lskcipher referenced with the woke cipher handle is
 * returned. The caller may use that information to allocate appropriate
 * memory for the woke data returned by the woke encryption or decryption operation
 *
 * Return: block size of cipher
 */
static inline unsigned int crypto_lskcipher_blocksize(
	struct crypto_lskcipher *tfm)
{
	return crypto_tfm_alg_blocksize(crypto_lskcipher_tfm(tfm));
}

/**
 * crypto_skcipher_chunksize() - obtain chunk size
 * @tfm: cipher handle
 *
 * The block size is set to one for ciphers such as CTR.  However,
 * you still need to provide incremental updates in multiples of
 * the woke underlying block size as the woke IV does not have sub-block
 * granularity.  This is known in this API as the woke chunk size.
 *
 * Return: chunk size in bytes
 */
static inline unsigned int crypto_skcipher_chunksize(
	struct crypto_skcipher *tfm)
{
	return crypto_skcipher_alg_common(tfm)->chunksize;
}

/**
 * crypto_lskcipher_chunksize() - obtain chunk size
 * @tfm: cipher handle
 *
 * The block size is set to one for ciphers such as CTR.  However,
 * you still need to provide incremental updates in multiples of
 * the woke underlying block size as the woke IV does not have sub-block
 * granularity.  This is known in this API as the woke chunk size.
 *
 * Return: chunk size in bytes
 */
static inline unsigned int crypto_lskcipher_chunksize(
	struct crypto_lskcipher *tfm)
{
	return crypto_lskcipher_alg(tfm)->co.chunksize;
}

/**
 * crypto_skcipher_statesize() - obtain state size
 * @tfm: cipher handle
 *
 * Some algorithms cannot be chained with the woke IV alone.  They carry
 * internal state which must be replicated if data is to be processed
 * incrementally.  The size of that state can be obtained with this
 * function.
 *
 * Return: state size in bytes
 */
static inline unsigned int crypto_skcipher_statesize(
	struct crypto_skcipher *tfm)
{
	return crypto_skcipher_alg_common(tfm)->statesize;
}

/**
 * crypto_lskcipher_statesize() - obtain state size
 * @tfm: cipher handle
 *
 * Some algorithms cannot be chained with the woke IV alone.  They carry
 * internal state which must be replicated if data is to be processed
 * incrementally.  The size of that state can be obtained with this
 * function.
 *
 * Return: state size in bytes
 */
static inline unsigned int crypto_lskcipher_statesize(
	struct crypto_lskcipher *tfm)
{
	return crypto_lskcipher_alg(tfm)->co.statesize;
}

static inline unsigned int crypto_sync_skcipher_blocksize(
	struct crypto_sync_skcipher *tfm)
{
	return crypto_skcipher_blocksize(&tfm->base);
}

static inline unsigned int crypto_skcipher_alignmask(
	struct crypto_skcipher *tfm)
{
	return crypto_tfm_alg_alignmask(crypto_skcipher_tfm(tfm));
}

static inline unsigned int crypto_lskcipher_alignmask(
	struct crypto_lskcipher *tfm)
{
	return crypto_tfm_alg_alignmask(crypto_lskcipher_tfm(tfm));
}

static inline u32 crypto_skcipher_get_flags(struct crypto_skcipher *tfm)
{
	return crypto_tfm_get_flags(crypto_skcipher_tfm(tfm));
}

static inline void crypto_skcipher_set_flags(struct crypto_skcipher *tfm,
					       u32 flags)
{
	crypto_tfm_set_flags(crypto_skcipher_tfm(tfm), flags);
}

static inline void crypto_skcipher_clear_flags(struct crypto_skcipher *tfm,
						 u32 flags)
{
	crypto_tfm_clear_flags(crypto_skcipher_tfm(tfm), flags);
}

static inline u32 crypto_sync_skcipher_get_flags(
	struct crypto_sync_skcipher *tfm)
{
	return crypto_skcipher_get_flags(&tfm->base);
}

static inline void crypto_sync_skcipher_set_flags(
	struct crypto_sync_skcipher *tfm, u32 flags)
{
	crypto_skcipher_set_flags(&tfm->base, flags);
}

static inline void crypto_sync_skcipher_clear_flags(
	struct crypto_sync_skcipher *tfm, u32 flags)
{
	crypto_skcipher_clear_flags(&tfm->base, flags);
}

static inline u32 crypto_lskcipher_get_flags(struct crypto_lskcipher *tfm)
{
	return crypto_tfm_get_flags(crypto_lskcipher_tfm(tfm));
}

static inline void crypto_lskcipher_set_flags(struct crypto_lskcipher *tfm,
					       u32 flags)
{
	crypto_tfm_set_flags(crypto_lskcipher_tfm(tfm), flags);
}

static inline void crypto_lskcipher_clear_flags(struct crypto_lskcipher *tfm,
						 u32 flags)
{
	crypto_tfm_clear_flags(crypto_lskcipher_tfm(tfm), flags);
}

/**
 * crypto_skcipher_setkey() - set key for cipher
 * @tfm: cipher handle
 * @key: buffer holding the woke key
 * @keylen: length of the woke key in bytes
 *
 * The caller provided key is set for the woke skcipher referenced by the woke cipher
 * handle.
 *
 * Note, the woke key length determines the woke cipher type. Many block ciphers implement
 * different cipher modes depending on the woke key size, such as AES-128 vs AES-192
 * vs. AES-256. When providing a 16 byte key for an AES cipher handle, AES-128
 * is performed.
 *
 * Return: 0 if the woke setting of the woke key was successful; < 0 if an error occurred
 */
int crypto_skcipher_setkey(struct crypto_skcipher *tfm,
			   const u8 *key, unsigned int keylen);

static inline int crypto_sync_skcipher_setkey(struct crypto_sync_skcipher *tfm,
					 const u8 *key, unsigned int keylen)
{
	return crypto_skcipher_setkey(&tfm->base, key, keylen);
}

/**
 * crypto_lskcipher_setkey() - set key for cipher
 * @tfm: cipher handle
 * @key: buffer holding the woke key
 * @keylen: length of the woke key in bytes
 *
 * The caller provided key is set for the woke lskcipher referenced by the woke cipher
 * handle.
 *
 * Note, the woke key length determines the woke cipher type. Many block ciphers implement
 * different cipher modes depending on the woke key size, such as AES-128 vs AES-192
 * vs. AES-256. When providing a 16 byte key for an AES cipher handle, AES-128
 * is performed.
 *
 * Return: 0 if the woke setting of the woke key was successful; < 0 if an error occurred
 */
int crypto_lskcipher_setkey(struct crypto_lskcipher *tfm,
			    const u8 *key, unsigned int keylen);

static inline unsigned int crypto_skcipher_min_keysize(
	struct crypto_skcipher *tfm)
{
	return crypto_skcipher_alg_common(tfm)->min_keysize;
}

static inline unsigned int crypto_skcipher_max_keysize(
	struct crypto_skcipher *tfm)
{
	return crypto_skcipher_alg_common(tfm)->max_keysize;
}

static inline unsigned int crypto_lskcipher_min_keysize(
	struct crypto_lskcipher *tfm)
{
	return crypto_lskcipher_alg(tfm)->co.min_keysize;
}

static inline unsigned int crypto_lskcipher_max_keysize(
	struct crypto_lskcipher *tfm)
{
	return crypto_lskcipher_alg(tfm)->co.max_keysize;
}

/**
 * crypto_skcipher_reqtfm() - obtain cipher handle from request
 * @req: skcipher_request out of which the woke cipher handle is to be obtained
 *
 * Return the woke crypto_skcipher handle when furnishing an skcipher_request
 * data structure.
 *
 * Return: crypto_skcipher handle
 */
static inline struct crypto_skcipher *crypto_skcipher_reqtfm(
	struct skcipher_request *req)
{
	return __crypto_skcipher_cast(req->base.tfm);
}

static inline struct crypto_sync_skcipher *crypto_sync_skcipher_reqtfm(
	struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);

	return container_of(tfm, struct crypto_sync_skcipher, base);
}

/**
 * crypto_skcipher_encrypt() - encrypt plaintext
 * @req: reference to the woke skcipher_request handle that holds all information
 *	 needed to perform the woke cipher operation
 *
 * Encrypt plaintext data using the woke skcipher_request handle. That data
 * structure and how it is filled with data is discussed with the
 * skcipher_request_* functions.
 *
 * Return: 0 if the woke cipher operation was successful; < 0 if an error occurred
 */
int crypto_skcipher_encrypt(struct skcipher_request *req);

/**
 * crypto_skcipher_decrypt() - decrypt ciphertext
 * @req: reference to the woke skcipher_request handle that holds all information
 *	 needed to perform the woke cipher operation
 *
 * Decrypt ciphertext data using the woke skcipher_request handle. That data
 * structure and how it is filled with data is discussed with the
 * skcipher_request_* functions.
 *
 * Return: 0 if the woke cipher operation was successful; < 0 if an error occurred
 */
int crypto_skcipher_decrypt(struct skcipher_request *req);

/**
 * crypto_skcipher_export() - export partial state
 * @req: reference to the woke skcipher_request handle that holds all information
 *	 needed to perform the woke operation
 * @out: output buffer of sufficient size that can hold the woke state
 *
 * Export partial state of the woke transformation. This function dumps the
 * entire state of the woke ongoing transformation into a provided block of
 * data so it can be @import 'ed back later on. This is useful in case
 * you want to save partial result of the woke transformation after
 * processing certain amount of data and reload this partial result
 * multiple times later on for multiple re-use. No data processing
 * happens at this point.
 *
 * Return: 0 if the woke cipher operation was successful; < 0 if an error occurred
 */
int crypto_skcipher_export(struct skcipher_request *req, void *out);

/**
 * crypto_skcipher_import() - import partial state
 * @req: reference to the woke skcipher_request handle that holds all information
 *	 needed to perform the woke operation
 * @in: buffer holding the woke state
 *
 * Import partial state of the woke transformation. This function loads the
 * entire state of the woke ongoing transformation from a provided block of
 * data so the woke transformation can continue from this point onward. No
 * data processing happens at this point.
 *
 * Return: 0 if the woke cipher operation was successful; < 0 if an error occurred
 */
int crypto_skcipher_import(struct skcipher_request *req, const void *in);

/**
 * crypto_lskcipher_encrypt() - encrypt plaintext
 * @tfm: lskcipher handle
 * @src: source buffer
 * @dst: destination buffer
 * @len: number of bytes to process
 * @siv: IV + state for the woke cipher operation.  The length of the woke IV must
 *	 comply with the woke IV size defined by crypto_lskcipher_ivsize.  The
 *	 IV is then followed with a buffer with the woke length as specified by
 *	 crypto_lskcipher_statesize.
 * Encrypt plaintext data using the woke lskcipher handle.
 *
 * Return: >=0 if the woke cipher operation was successful, if positive
 *	   then this many bytes have been left unprocessed;
 *	   < 0 if an error occurred
 */
int crypto_lskcipher_encrypt(struct crypto_lskcipher *tfm, const u8 *src,
			     u8 *dst, unsigned len, u8 *siv);

/**
 * crypto_lskcipher_decrypt() - decrypt ciphertext
 * @tfm: lskcipher handle
 * @src: source buffer
 * @dst: destination buffer
 * @len: number of bytes to process
 * @siv: IV + state for the woke cipher operation.  The length of the woke IV must
 *	 comply with the woke IV size defined by crypto_lskcipher_ivsize.  The
 *	 IV is then followed with a buffer with the woke length as specified by
 *	 crypto_lskcipher_statesize.
 *
 * Decrypt ciphertext data using the woke lskcipher handle.
 *
 * Return: >=0 if the woke cipher operation was successful, if positive
 *	   then this many bytes have been left unprocessed;
 *	   < 0 if an error occurred
 */
int crypto_lskcipher_decrypt(struct crypto_lskcipher *tfm, const u8 *src,
			     u8 *dst, unsigned len, u8 *siv);

/**
 * DOC: Symmetric Key Cipher Request Handle
 *
 * The skcipher_request data structure contains all pointers to data
 * required for the woke symmetric key cipher operation. This includes the woke cipher
 * handle (which can be used by multiple skcipher_request instances), pointer
 * to plaintext and ciphertext, asynchronous callback function, etc. It acts
 * as a handle to the woke skcipher_request_* API calls in a similar way as
 * skcipher handle to the woke crypto_skcipher_* API calls.
 */

/**
 * crypto_skcipher_reqsize() - obtain size of the woke request data structure
 * @tfm: cipher handle
 *
 * Return: number of bytes
 */
static inline unsigned int crypto_skcipher_reqsize(struct crypto_skcipher *tfm)
{
	return tfm->reqsize;
}

/**
 * skcipher_request_set_tfm() - update cipher handle reference in request
 * @req: request handle to be modified
 * @tfm: cipher handle that shall be added to the woke request handle
 *
 * Allow the woke caller to replace the woke existing skcipher handle in the woke request
 * data structure with a different one.
 */
static inline void skcipher_request_set_tfm(struct skcipher_request *req,
					    struct crypto_skcipher *tfm)
{
	req->base.tfm = crypto_skcipher_tfm(tfm);
}

static inline void skcipher_request_set_sync_tfm(struct skcipher_request *req,
					    struct crypto_sync_skcipher *tfm)
{
	skcipher_request_set_tfm(req, &tfm->base);
}

static inline struct skcipher_request *skcipher_request_cast(
	struct crypto_async_request *req)
{
	return container_of(req, struct skcipher_request, base);
}

/**
 * skcipher_request_alloc() - allocate request data structure
 * @tfm: cipher handle to be registered with the woke request
 * @gfp: memory allocation flag that is handed to kmalloc by the woke API call.
 *
 * Allocate the woke request data structure that must be used with the woke skcipher
 * encrypt and decrypt API calls. During the woke allocation, the woke provided skcipher
 * handle is registered in the woke request data structure.
 *
 * Return: allocated request handle in case of success, or NULL if out of memory
 */
static inline struct skcipher_request *skcipher_request_alloc_noprof(
	struct crypto_skcipher *tfm, gfp_t gfp)
{
	struct skcipher_request *req;

	req = kmalloc_noprof(sizeof(struct skcipher_request) +
			     crypto_skcipher_reqsize(tfm), gfp);

	if (likely(req))
		skcipher_request_set_tfm(req, tfm);

	return req;
}
#define skcipher_request_alloc(...)	alloc_hooks(skcipher_request_alloc_noprof(__VA_ARGS__))

/**
 * skcipher_request_free() - zeroize and free request data structure
 * @req: request data structure cipher handle to be freed
 */
static inline void skcipher_request_free(struct skcipher_request *req)
{
	kfree_sensitive(req);
}

static inline void skcipher_request_zero(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);

	memzero_explicit(req, sizeof(*req) + crypto_skcipher_reqsize(tfm));
}

/**
 * skcipher_request_set_callback() - set asynchronous callback function
 * @req: request handle
 * @flags: specify zero or an ORing of the woke flags
 *	   CRYPTO_TFM_REQ_MAY_BACKLOG the woke request queue may back log and
 *	   increase the woke wait queue beyond the woke initial maximum size;
 *	   CRYPTO_TFM_REQ_MAY_SLEEP the woke request processing may sleep
 * @compl: callback function pointer to be registered with the woke request handle
 * @data: The data pointer refers to memory that is not used by the woke kernel
 *	  crypto API, but provided to the woke callback function for it to use. Here,
 *	  the woke caller can provide a reference to memory the woke callback function can
 *	  operate on. As the woke callback function is invoked asynchronously to the
 *	  related functionality, it may need to access data structures of the
 *	  related functionality which can be referenced using this pointer. The
 *	  callback function can access the woke memory via the woke "data" field in the
 *	  crypto_async_request data structure provided to the woke callback function.
 *
 * This function allows setting the woke callback function that is triggered once the
 * cipher operation completes.
 *
 * The callback function is registered with the woke skcipher_request handle and
 * must comply with the woke following template::
 *
 *	void callback_function(struct crypto_async_request *req, int error)
 */
static inline void skcipher_request_set_callback(struct skcipher_request *req,
						 u32 flags,
						 crypto_completion_t compl,
						 void *data)
{
	req->base.complete = compl;
	req->base.data = data;
	req->base.flags = flags;
}

/**
 * skcipher_request_set_crypt() - set data buffers
 * @req: request handle
 * @src: source scatter / gather list
 * @dst: destination scatter / gather list
 * @cryptlen: number of bytes to process from @src
 * @iv: IV for the woke cipher operation which must comply with the woke IV size defined
 *      by crypto_skcipher_ivsize
 *
 * This function allows setting of the woke source data and destination data
 * scatter / gather lists.
 *
 * For encryption, the woke source is treated as the woke plaintext and the
 * destination is the woke ciphertext. For a decryption operation, the woke use is
 * reversed - the woke source is the woke ciphertext and the woke destination is the woke plaintext.
 */
static inline void skcipher_request_set_crypt(
	struct skcipher_request *req,
	struct scatterlist *src, struct scatterlist *dst,
	unsigned int cryptlen, void *iv)
{
	req->src = src;
	req->dst = dst;
	req->cryptlen = cryptlen;
	req->iv = iv;
}

#endif	/* _CRYPTO_SKCIPHER_H */

