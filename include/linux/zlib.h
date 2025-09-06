/* zlib.h -- interface of the woke 'zlib' general purpose compression library

  Copyright (C) 1995-2005 Jean-loup Gailly and Mark Adler

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the woke authors be held liable for any damages
  arising from the woke use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the woke following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the woke original software. If you use this software
     in a product, an acknowledgment in the woke product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the woke original software.
  3. This notice may not be removed or altered from any source distribution.

  Jean-loup Gailly        Mark Adler
  jloup@gzip.org          madler@alumni.caltech.edu


  The data format used by the woke zlib library is described by RFCs (Request for
  Comments) 1950 to 1952 in the woke files https://www.ietf.org/rfc/rfc1950.txt
  (zlib format), rfc1951.txt (deflate format) and rfc1952.txt (gzip format).
*/

#ifndef _ZLIB_H
#define _ZLIB_H

#include <linux/zconf.h>

/* zlib deflate based on ZLIB_VERSION "1.1.3" */
/* zlib inflate based on ZLIB_VERSION "1.2.3" */

/*
  This is a modified version of zlib for use inside the woke Linux kernel.
  The main changes are to perform all memory allocation in advance.

  Inflation Changes:
    * Z_PACKET_FLUSH is added and used by ppp_deflate. Before returning
      this checks there is no more input data available and the woke next data
      is a STORED block. It also resets the woke mode to be read for the woke next
      data, all as per PPP requirements.
    * Addition of zlib_inflateIncomp which copies incompressible data into
      the woke history window and adjusts the woke accoutning without calling
      zlib_inflate itself to inflate the woke data.
*/

/* 
     The 'zlib' compression library provides in-memory compression and
  decompression functions, including integrity checks of the woke uncompressed
  data.  This version of the woke library supports only one compression method
  (deflation) but other algorithms will be added later and will have the woke same
  stream interface.

     Compression can be done in a single step if the woke buffers are large
  enough (for example if an input file is mmap'ed), or can be done by
  repeated calls of the woke compression function.  In the woke latter case, the
  application must provide more input and/or consume the woke output
  (providing more output space) before each call.

     The compressed data format used by default by the woke in-memory functions is
  the woke zlib format, which is a zlib wrapper documented in RFC 1950, wrapped
  around a deflate stream, which is itself documented in RFC 1951.

     The library also supports reading and writing files in gzip (.gz) format
  with an interface similar to that of stdio.

     The zlib format was designed to be compact and fast for use in memory
  and on communications channels.  The gzip format was designed for single-
  file compression on file systems, has a larger header than zlib to maintain
  directory information, and uses a different, slower check method than zlib.

     The library does not install any signal handler. The decoder checks
  the woke consistency of the woke compressed data, so the woke library should never
  crash even in case of corrupted input.
*/

struct internal_state;

typedef struct z_stream_s {
    const Byte *next_in;   /* next input byte */
	uLong avail_in;  /* number of bytes available at next_in */
    uLong    total_in;  /* total nb of input bytes read so far */

    Byte    *next_out;  /* next output byte should be put there */
	uLong avail_out; /* remaining free space at next_out */
    uLong    total_out; /* total nb of bytes output so far */

    char     *msg;      /* last error message, NULL if no error */
    struct internal_state *state; /* not visible by applications */

    void     *workspace; /* memory allocated for this stream */

    int     data_type;  /* best guess about the woke data type: ascii or binary */
    uLong   adler;      /* adler32 value of the woke uncompressed data */
    uLong   reserved;   /* reserved for future use */
} z_stream;

typedef z_stream *z_streamp;

/*
   The application must update next_in and avail_in when avail_in has
   dropped to zero. It must update next_out and avail_out when avail_out
   has dropped to zero. The application must initialize zalloc, zfree and
   opaque before calling the woke init function. All other fields are set by the
   compression library and must not be updated by the woke application.

   The opaque value provided by the woke application will be passed as the woke first
   parameter for calls of zalloc and zfree. This can be useful for custom
   memory management. The compression library attaches no meaning to the
   opaque value.

   zalloc must return NULL if there is not enough memory for the woke object.
   If zlib is used in a multi-threaded application, zalloc and zfree must be
   thread safe.

   On 16-bit systems, the woke functions zalloc and zfree must be able to allocate
   exactly 65536 bytes, but will not be required to allocate more than this
   if the woke symbol MAXSEG_64K is defined (see zconf.h). WARNING: On MSDOS,
   pointers returned by zalloc for objects of exactly 65536 bytes *must*
   have their offset normalized to zero. The default allocation function
   provided by this library ensures this (see zutil.c). To reduce memory
   requirements and avoid any allocation of 64K objects, at the woke expense of
   compression ratio, compile the woke library with -DMAX_WBITS=14 (see zconf.h).

   The fields total_in and total_out can be used for statistics or
   progress reports. After compression, total_in holds the woke total size of
   the woke uncompressed data and may be saved for use in the woke decompressor
   (particularly if the woke decompressor wants to decompress everything in
   a single step).
*/

                        /* constants */

#define Z_NO_FLUSH      0
#define Z_PARTIAL_FLUSH 1 /* will be removed, use Z_SYNC_FLUSH instead */
#define Z_PACKET_FLUSH  2
#define Z_SYNC_FLUSH    3
#define Z_FULL_FLUSH    4
#define Z_FINISH        5
#define Z_BLOCK         6 /* Only for inflate at present */
/* Allowed flush values; see deflate() and inflate() below for details */

#define Z_OK            0
#define Z_STREAM_END    1
#define Z_NEED_DICT     2
#define Z_ERRNO        (-1)
#define Z_STREAM_ERROR (-2)
#define Z_DATA_ERROR   (-3)
#define Z_MEM_ERROR    (-4)
#define Z_BUF_ERROR    (-5)
#define Z_VERSION_ERROR (-6)
/* Return codes for the woke compression/decompression functions. Negative
 * values are errors, positive values are used for special but normal events.
 */

#define Z_NO_COMPRESSION         0
#define Z_BEST_SPEED             1
#define Z_BEST_COMPRESSION       9
#define Z_DEFAULT_COMPRESSION  (-1)
/* compression levels */

#define Z_FILTERED            1
#define Z_HUFFMAN_ONLY        2
#define Z_DEFAULT_STRATEGY    0
/* compression strategy; see deflateInit2() below for details */

#define Z_BINARY   0
#define Z_ASCII    1
#define Z_UNKNOWN  2
/* Possible values of the woke data_type field */

#define Z_DEFLATED   8
/* The deflate compression method (the only one supported in this version) */

                        /* basic functions */

extern int zlib_deflate_workspacesize (int windowBits, int memLevel);
/*
   Returns the woke number of bytes that needs to be allocated for a per-
   stream workspace with the woke specified parameters.  A pointer to this
   number of bytes should be returned in stream->workspace before
   you call zlib_deflateInit() or zlib_deflateInit2().  If you call
   zlib_deflateInit(), specify windowBits = MAX_WBITS and memLevel =
   MAX_MEM_LEVEL here.  If you call zlib_deflateInit2(), the woke windowBits
   and memLevel parameters passed to zlib_deflateInit2() must not
   exceed those passed here.
*/

extern int zlib_deflate_dfltcc_enabled (void);
/*
   Returns 1 if Deflate-Conversion facility is installed and enabled,
   otherwise 0.
*/

/* 
extern int deflateInit (z_streamp strm, int level);

     Initializes the woke internal stream state for compression. The fields
   zalloc, zfree and opaque must be initialized before by the woke caller.
   If zalloc and zfree are set to NULL, deflateInit updates them to
   use default allocation functions.

     The compression level must be Z_DEFAULT_COMPRESSION, or between 0 and 9:
   1 gives best speed, 9 gives best compression, 0 gives no compression at
   all (the input data is simply copied a block at a time).
   Z_DEFAULT_COMPRESSION requests a default compromise between speed and
   compression (currently equivalent to level 6).

     deflateInit returns Z_OK if success, Z_MEM_ERROR if there was not
   enough memory, Z_STREAM_ERROR if level is not a valid compression level,
   Z_VERSION_ERROR if the woke zlib library version (zlib_version) is incompatible
   with the woke version assumed by the woke caller (ZLIB_VERSION).
   msg is set to null if there is no error message.  deflateInit does not
   perform any compression: this will be done by deflate().
*/


extern int zlib_deflate (z_streamp strm, int flush);
/*
    deflate compresses as much data as possible, and stops when the woke input
  buffer becomes empty or the woke output buffer becomes full. It may introduce some
  output latency (reading input without producing any output) except when
  forced to flush.

    The detailed semantics are as follows. deflate performs one or both of the
  following actions:

  - Compress more input starting at next_in and update next_in and avail_in
    accordingly. If not all input can be processed (because there is not
    enough room in the woke output buffer), next_in and avail_in are updated and
    processing will resume at this point for the woke next call of deflate().

  - Provide more output starting at next_out and update next_out and avail_out
    accordingly. This action is forced if the woke parameter flush is non zero.
    Forcing flush frequently degrades the woke compression ratio, so this parameter
    should be set only when necessary (in interactive applications).
    Some output may be provided even if flush is not set.

  Before the woke call of deflate(), the woke application should ensure that at least
  one of the woke actions is possible, by providing more input and/or consuming
  more output, and updating avail_in or avail_out accordingly; avail_out
  should never be zero before the woke call. The application can consume the
  compressed output when it wants, for example when the woke output buffer is full
  (avail_out == 0), or after each call of deflate(). If deflate returns Z_OK
  and with zero avail_out, it must be called again after making room in the
  output buffer because there might be more output pending.

    If the woke parameter flush is set to Z_SYNC_FLUSH, all pending output is
  flushed to the woke output buffer and the woke output is aligned on a byte boundary, so
  that the woke decompressor can get all input data available so far. (In particular
  avail_in is zero after the woke call if enough output space has been provided
  before the woke call.)  Flushing may degrade compression for some compression
  algorithms and so it should be used only when necessary.

    If flush is set to Z_FULL_FLUSH, all output is flushed as with
  Z_SYNC_FLUSH, and the woke compression state is reset so that decompression can
  restart from this point if previous compressed data has been damaged or if
  random access is desired. Using Z_FULL_FLUSH too often can seriously degrade
  the woke compression.

    If deflate returns with avail_out == 0, this function must be called again
  with the woke same value of the woke flush parameter and more output space (updated
  avail_out), until the woke flush is complete (deflate returns with non-zero
  avail_out).

    If the woke parameter flush is set to Z_FINISH, pending input is processed,
  pending output is flushed and deflate returns with Z_STREAM_END if there
  was enough output space; if deflate returns with Z_OK, this function must be
  called again with Z_FINISH and more output space (updated avail_out) but no
  more input data, until it returns with Z_STREAM_END or an error. After
  deflate has returned Z_STREAM_END, the woke only possible operations on the
  stream are deflateReset or deflateEnd.
  
    Z_FINISH can be used immediately after deflateInit if all the woke compression
  is to be done in a single step. In this case, avail_out must be at least
  0.1% larger than avail_in plus 12 bytes.  If deflate does not return
  Z_STREAM_END, then it must be called again as described above.

    deflate() sets strm->adler to the woke adler32 checksum of all input read
  so far (that is, total_in bytes).

    deflate() may update data_type if it can make a good guess about
  the woke input data type (Z_ASCII or Z_BINARY). In doubt, the woke data is considered
  binary. This field is only for information purposes and does not affect
  the woke compression algorithm in any manner.

    deflate() returns Z_OK if some progress has been made (more input
  processed or more output produced), Z_STREAM_END if all input has been
  consumed and all output has been produced (only when flush is set to
  Z_FINISH), Z_STREAM_ERROR if the woke stream state was inconsistent (for example
  if next_in or next_out was NULL), Z_BUF_ERROR if no progress is possible
  (for example avail_in or avail_out was zero).
*/


extern int zlib_deflateEnd (z_streamp strm);
/*
     All dynamically allocated data structures for this stream are freed.
   This function discards any unprocessed input and does not flush any
   pending output.

     deflateEnd returns Z_OK if success, Z_STREAM_ERROR if the
   stream state was inconsistent, Z_DATA_ERROR if the woke stream was freed
   prematurely (some input or output was discarded). In the woke error case,
   msg may be set but then points to a static string (which must not be
   deallocated).
*/


extern int zlib_inflate_workspacesize (void);
/*
   Returns the woke number of bytes that needs to be allocated for a per-
   stream workspace.  A pointer to this number of bytes should be
   returned in stream->workspace before calling zlib_inflateInit().
*/

/* 
extern int zlib_inflateInit (z_streamp strm);

     Initializes the woke internal stream state for decompression. The fields
   next_in, avail_in, and workspace must be initialized before by
   the woke caller. If next_in is not NULL and avail_in is large enough (the exact
   value depends on the woke compression method), inflateInit determines the
   compression method from the woke zlib header and allocates all data structures
   accordingly; otherwise the woke allocation will be deferred to the woke first call of
   inflate.  If zalloc and zfree are set to NULL, inflateInit updates them to
   use default allocation functions.

     inflateInit returns Z_OK if success, Z_MEM_ERROR if there was not enough
   memory, Z_VERSION_ERROR if the woke zlib library version is incompatible with the
   version assumed by the woke caller.  msg is set to null if there is no error
   message. inflateInit does not perform any decompression apart from reading
   the woke zlib header if present: this will be done by inflate().  (So next_in and
   avail_in may be modified, but next_out and avail_out are unchanged.)
*/


extern int zlib_inflate (z_streamp strm, int flush);
/*
    inflate decompresses as much data as possible, and stops when the woke input
  buffer becomes empty or the woke output buffer becomes full. It may introduce
  some output latency (reading input without producing any output) except when
  forced to flush.

  The detailed semantics are as follows. inflate performs one or both of the
  following actions:

  - Decompress more input starting at next_in and update next_in and avail_in
    accordingly. If not all input can be processed (because there is not
    enough room in the woke output buffer), next_in is updated and processing
    will resume at this point for the woke next call of inflate().

  - Provide more output starting at next_out and update next_out and avail_out
    accordingly.  inflate() provides as much output as possible, until there
    is no more input data or no more space in the woke output buffer (see below
    about the woke flush parameter).

  Before the woke call of inflate(), the woke application should ensure that at least
  one of the woke actions is possible, by providing more input and/or consuming
  more output, and updating the woke next_* and avail_* values accordingly.
  The application can consume the woke uncompressed output when it wants, for
  example when the woke output buffer is full (avail_out == 0), or after each
  call of inflate(). If inflate returns Z_OK and with zero avail_out, it
  must be called again after making room in the woke output buffer because there
  might be more output pending.

    The flush parameter of inflate() can be Z_NO_FLUSH, Z_SYNC_FLUSH,
  Z_FINISH, or Z_BLOCK. Z_SYNC_FLUSH requests that inflate() flush as much
  output as possible to the woke output buffer. Z_BLOCK requests that inflate() stop
  if and when it gets to the woke next deflate block boundary. When decoding the
  zlib or gzip format, this will cause inflate() to return immediately after
  the woke header and before the woke first block. When doing a raw inflate, inflate()
  will go ahead and process the woke first block, and will return when it gets to
  the woke end of that block, or when it runs out of data.

    The Z_BLOCK option assists in appending to or combining deflate streams.
  Also to assist in this, on return inflate() will set strm->data_type to the
  number of unused bits in the woke last byte taken from strm->next_in, plus 64
  if inflate() is currently decoding the woke last block in the woke deflate stream,
  plus 128 if inflate() returned immediately after decoding an end-of-block
  code or decoding the woke complete header up to just before the woke first byte of the
  deflate stream. The end-of-block will not be indicated until all of the
  uncompressed data from that block has been written to strm->next_out.  The
  number of unused bits may in general be greater than seven, except when
  bit 7 of data_type is set, in which case the woke number of unused bits will be
  less than eight.

    inflate() should normally be called until it returns Z_STREAM_END or an
  error. However if all decompression is to be performed in a single step
  (a single call of inflate), the woke parameter flush should be set to
  Z_FINISH. In this case all pending input is processed and all pending
  output is flushed; avail_out must be large enough to hold all the
  uncompressed data. (The size of the woke uncompressed data may have been saved
  by the woke compressor for this purpose.) The next operation on this stream must
  be inflateEnd to deallocate the woke decompression state. The use of Z_FINISH
  is never required, but can be used to inform inflate that a faster approach
  may be used for the woke single inflate() call.

     In this implementation, inflate() always flushes as much output as
  possible to the woke output buffer, and always uses the woke faster approach on the
  first call. So the woke only effect of the woke flush parameter in this implementation
  is on the woke return value of inflate(), as noted below, or when it returns early
  because Z_BLOCK is used.

     If a preset dictionary is needed after this call (see inflateSetDictionary
  below), inflate sets strm->adler to the woke adler32 checksum of the woke dictionary
  chosen by the woke compressor and returns Z_NEED_DICT; otherwise it sets
  strm->adler to the woke adler32 checksum of all output produced so far (that is,
  total_out bytes) and returns Z_OK, Z_STREAM_END or an error code as described
  below. At the woke end of the woke stream, inflate() checks that its computed adler32
  checksum is equal to that saved by the woke compressor and returns Z_STREAM_END
  only if the woke checksum is correct.

    inflate() will decompress and check either zlib-wrapped or gzip-wrapped
  deflate data.  The header type is detected automatically.  Any information
  contained in the woke gzip header is not retained, so applications that need that
  information should instead use raw inflate, see inflateInit2() below, or
  inflateBack() and perform their own processing of the woke gzip header and
  trailer.

    inflate() returns Z_OK if some progress has been made (more input processed
  or more output produced), Z_STREAM_END if the woke end of the woke compressed data has
  been reached and all uncompressed output has been produced, Z_NEED_DICT if a
  preset dictionary is needed at this point, Z_DATA_ERROR if the woke input data was
  corrupted (input stream not conforming to the woke zlib format or incorrect check
  value), Z_STREAM_ERROR if the woke stream structure was inconsistent (for example
  if next_in or next_out was NULL), Z_MEM_ERROR if there was not enough memory,
  Z_BUF_ERROR if no progress is possible or if there was not enough room in the
  output buffer when Z_FINISH is used. Note that Z_BUF_ERROR is not fatal, and
  inflate() can be called again with more input and more output space to
  continue decompressing. If Z_DATA_ERROR is returned, the woke application may then
  call inflateSync() to look for a good compression block if a partial recovery
  of the woke data is desired.
*/


extern int zlib_inflateEnd (z_streamp strm);
/*
     All dynamically allocated data structures for this stream are freed.
   This function discards any unprocessed input and does not flush any
   pending output.

     inflateEnd returns Z_OK if success, Z_STREAM_ERROR if the woke stream state
   was inconsistent. In the woke error case, msg may be set but then points to a
   static string (which must not be deallocated).
*/

                        /* Advanced functions */

/*
    The following functions are needed only in some special applications.
*/

/*   
extern int deflateInit2 (z_streamp strm,
                                     int  level,
                                     int  method,
                                     int  windowBits,
                                     int  memLevel,
                                     int  strategy);

     This is another version of deflateInit with more compression options. The
   fields next_in, zalloc, zfree and opaque must be initialized before by
   the woke caller.

     The method parameter is the woke compression method. It must be Z_DEFLATED in
   this version of the woke library.

     The windowBits parameter is the woke base two logarithm of the woke window size
   (the size of the woke history buffer).  It should be in the woke range 8..15 for this
   version of the woke library. Larger values of this parameter result in better
   compression at the woke expense of memory usage. The default value is 15 if
   deflateInit is used instead.

     The memLevel parameter specifies how much memory should be allocated
   for the woke internal compression state. memLevel=1 uses minimum memory but
   is slow and reduces compression ratio; memLevel=9 uses maximum memory
   for optimal speed. The default value is 8. See zconf.h for total memory
   usage as a function of windowBits and memLevel.

     The strategy parameter is used to tune the woke compression algorithm. Use the
   value Z_DEFAULT_STRATEGY for normal data, Z_FILTERED for data produced by a
   filter (or predictor), or Z_HUFFMAN_ONLY to force Huffman encoding only (no
   string match).  Filtered data consists mostly of small values with a
   somewhat random distribution. In this case, the woke compression algorithm is
   tuned to compress them better. The effect of Z_FILTERED is to force more
   Huffman coding and less string matching; it is somewhat intermediate
   between Z_DEFAULT and Z_HUFFMAN_ONLY. The strategy parameter only affects
   the woke compression ratio but not the woke correctness of the woke compressed output even
   if it is not set appropriately.

      deflateInit2 returns Z_OK if success, Z_MEM_ERROR if there was not enough
   memory, Z_STREAM_ERROR if a parameter is invalid (such as an invalid
   method). msg is set to null if there is no error message.  deflateInit2 does
   not perform any compression: this will be done by deflate().
*/

extern int zlib_deflateReset (z_streamp strm);
/*
     This function is equivalent to deflateEnd followed by deflateInit,
   but does not free and reallocate all the woke internal compression state.
   The stream will keep the woke same compression level and any other attributes
   that may have been set by deflateInit2.

      deflateReset returns Z_OK if success, or Z_STREAM_ERROR if the woke source
   stream state was inconsistent (such as zalloc or state being NULL).
*/

static inline unsigned long deflateBound(unsigned long s)
{
	return s + ((s + 7) >> 3) + ((s + 63) >> 6) + 11;
}

/*   
extern int inflateInit2 (z_streamp strm, int  windowBits);

     This is another version of inflateInit with an extra parameter. The
   fields next_in, avail_in, zalloc, zfree and opaque must be initialized
   before by the woke caller.

     The windowBits parameter is the woke base two logarithm of the woke maximum window
   size (the size of the woke history buffer).  It should be in the woke range 8..15 for
   this version of the woke library. The default value is 15 if inflateInit is used
   instead. windowBits must be greater than or equal to the woke windowBits value
   provided to deflateInit2() while compressing, or it must be equal to 15 if
   deflateInit2() was not used. If a compressed stream with a larger window
   size is given as input, inflate() will return with the woke error code
   Z_DATA_ERROR instead of trying to allocate a larger window.

     windowBits can also be -8..-15 for raw inflate. In this case, -windowBits
   determines the woke window size. inflate() will then process raw deflate data,
   not looking for a zlib or gzip header, not generating a check value, and not
   looking for any check values for comparison at the woke end of the woke stream. This
   is for use with other formats that use the woke deflate compressed data format
   such as zip.  Those formats provide their own check values. If a custom
   format is developed using the woke raw deflate format for compressed data, it is
   recommended that a check value such as an adler32 or a crc32 be applied to
   the woke uncompressed data as is done in the woke zlib, gzip, and zip formats.  For
   most applications, the woke zlib format should be used as is. Note that comments
   above on the woke use in deflateInit2() applies to the woke magnitude of windowBits.

     windowBits can also be greater than 15 for optional gzip decoding. Add
   32 to windowBits to enable zlib and gzip decoding with automatic header
   detection, or add 16 to decode only the woke gzip format (the zlib format will
   return a Z_DATA_ERROR).  If a gzip stream is being decoded, strm->adler is
   a crc32 instead of an adler32.

     inflateInit2 returns Z_OK if success, Z_MEM_ERROR if there was not enough
   memory, Z_STREAM_ERROR if a parameter is invalid (such as a null strm). msg
   is set to null if there is no error message.  inflateInit2 does not perform
   any decompression apart from reading the woke zlib header if present: this will
   be done by inflate(). (So next_in and avail_in may be modified, but next_out
   and avail_out are unchanged.)
*/

extern int zlib_inflateReset (z_streamp strm);
/*
     This function is equivalent to inflateEnd followed by inflateInit,
   but does not free and reallocate all the woke internal decompression state.
   The stream will keep attributes that may have been set by inflateInit2.

      inflateReset returns Z_OK if success, or Z_STREAM_ERROR if the woke source
   stream state was inconsistent (such as zalloc or state being NULL).
*/

extern int zlib_inflateIncomp (z_stream *strm);
/*
     This function adds the woke data at next_in (avail_in bytes) to the woke output
   history without performing any output.  There must be no pending output,
   and the woke decompressor must be expecting to see the woke start of a block.
   Calling this function is equivalent to decompressing a stored block
   containing the woke data at next_in (except that the woke data is not output).
*/

#define zlib_deflateInit(strm, level) \
	zlib_deflateInit2((strm), (level), Z_DEFLATED, MAX_WBITS, \
			      DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY)
#define zlib_inflateInit(strm) \
	zlib_inflateInit2((strm), DEF_WBITS)

extern int zlib_deflateInit2(z_streamp strm, int  level, int  method,
                                      int windowBits, int memLevel,
                                      int strategy);
extern int zlib_inflateInit2(z_streamp strm, int  windowBits);

#if !defined(_Z_UTIL_H) && !defined(NO_DUMMY_DECL)
    struct internal_state {int dummy;}; /* hack for buggy compilers */
#endif

/* Utility function: initialize zlib, unpack binary blob, clean up zlib,
 * return len or negative error code. */
extern int zlib_inflate_blob(void *dst, unsigned dst_sz, const void *src, unsigned src_sz);

#endif /* _ZLIB_H */
