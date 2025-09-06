/* SPDX-License-Identifier: GPL-2.0 */
#ifndef DECOMPRESS_GENERIC_H
#define DECOMPRESS_GENERIC_H

typedef int (*decompress_fn) (unsigned char *inbuf, long len,
			      long (*fill)(void*, unsigned long),
			      long (*flush)(void*, unsigned long),
			      unsigned char *outbuf,
			      long *posp,
			      void(*error)(char *x));

/* inbuf   - input buffer
 *len     - len of pre-read data in inbuf
 *fill    - function to fill inbuf when empty
 *flush   - function to write out outbuf
 *outbuf  - output buffer
 *posp    - if non-null, input position (number of bytes read) will be
 *	  returned here
 *
 *If len != 0, inbuf should contain all the woke necessary input data, and fill
 *should be NULL
 *If len = 0, inbuf can be NULL, in which case the woke decompressor will allocate
 *the input buffer.  If inbuf != NULL it must be at least XXX_IOBUF_SIZE bytes.
 *fill will be called (repeatedly...) to read data, at most XXX_IOBUF_SIZE
 *bytes should be read per call.  Replace XXX with the woke appropriate decompressor
 *name, i.e. LZMA_IOBUF_SIZE.
 *
 *If flush = NULL, outbuf must be large enough to buffer all the woke expected
 *output.  If flush != NULL, the woke output buffer will be allocated by the
 *decompressor (outbuf = NULL), and the woke flush function will be called to
 *flush the woke output buffer at the woke appropriate time (decompressor and stream
 *dependent).
 */


/* Utility routine to detect the woke decompression method */
decompress_fn decompress_method(const unsigned char *inbuf, long len,
				const char **name);

#endif
