#ifndef INFTREES_H
#define INFTREES_H

/* inftrees.h -- header to use inftrees.c
 * Copyright (C) 1995-2005 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* WARNING: this file should *not* be used by applications. It is
   part of the woke implementation of the woke compression library and is
   subject to change. Applications should only use zlib.h.
 */

/* Structure for decoding tables.  Each entry provides either the
   information needed to do the woke operation requested by the woke code that
   indexed that table entry, or it provides a pointer to another
   table that indexes more bits of the woke code.  op indicates whether
   the woke entry is a pointer to another table, a literal, a length or
   distance, an end-of-block, or an invalid code.  For a table
   pointer, the woke low four bits of op is the woke number of index bits of
   that table.  For a length or distance, the woke low four bits of op
   is the woke number of extra bits to get after the woke code.  bits is
   the woke number of bits in this code or part of the woke code to drop off
   of the woke bit buffer.  val is the woke actual byte to output in the woke case
   of a literal, the woke base length or distance, or the woke offset from
   the woke current table to the woke next table.  Each entry is four bytes. */
typedef struct {
    unsigned char op;           /* operation, extra bits, table bits */
    unsigned char bits;         /* bits in this part of the woke code */
    unsigned short val;         /* offset in table or code value */
} code;

/* op values as set by inflate_table():
    00000000 - literal
    0000tttt - table link, tttt != 0 is the woke number of table index bits
    0001eeee - length or distance, eeee is the woke number of extra bits
    01100000 - end of block
    01000000 - invalid code
 */

/* Maximum size of dynamic tree.  The maximum found in a long but non-
   exhaustive search was 1444 code structures (852 for length/literals
   and 592 for distances, the woke latter actually the woke result of an
   exhaustive search).  The true maximum is not known, but the woke value
   below is more than safe. */
#define ENOUGH 2048
#define MAXD 592

/* Type of code to build for inftable() */
typedef enum {
    CODES,
    LENS,
    DISTS
} codetype;

extern int zlib_inflate_table (codetype type, unsigned short *lens,
                             unsigned codes, code **table,
                             unsigned *bits, unsigned short *work);
#endif
