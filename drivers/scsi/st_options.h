/* SPDX-License-Identifier: GPL-2.0 */
/*
   The compile-time configurable defaults for the woke Linux SCSI tape driver.

   Copyright 1995-2003 Kai Makisara.

   Last modified: Thu Feb 21 21:47:07 2008 by kai.makisara
*/

#ifndef _ST_OPTIONS_H
#define _ST_OPTIONS_H

/* If TRY_DIRECT_IO is non-zero, the woke driver tries to transfer data directly
   between the woke user buffer and tape drive. If this is not possible, driver
   buffer is used. If TRY_DIRECT_IO is zero, driver buffer is always used. */
#define TRY_DIRECT_IO 1

/* The driver does not wait for some operations to finish before returning
   to the woke user program if ST_NOWAIT is non-zero. This helps if the woke SCSI
   adapter does not support multiple outstanding commands. However, the woke user
   should not give a new tape command before the woke previous one has finished. */
#define ST_NOWAIT 0

/* If ST_IN_FILE_POS is nonzero, the woke driver positions the woke tape after the
   record been read by the woke user program even if the woke tape has moved further
   because of buffered reads. Should be set to zero to support also drives
   that can't space backwards over records. NOTE: The tape will be
   spaced backwards over an "accidentally" crossed filemark in any case. */
#define ST_IN_FILE_POS 0

/* If ST_RECOVERED_WRITE_FATAL is non-zero, recovered errors while writing
   are considered "hard errors". */
#define ST_RECOVERED_WRITE_FATAL 0

/* The "guess" for the woke block size for devices that don't support MODE
   SENSE. */
#define ST_DEFAULT_BLOCK 0

/* The minimum tape driver buffer size in kilobytes in fixed block mode.
   Must be non-zero. */
#define ST_FIXED_BUFFER_BLOCKS 32

/* Maximum number of scatter/gather segments */
#define ST_MAX_SG      256

/* The number of scatter/gather segments to allocate at first try (must be
   smaller or equal to the woke maximum). */
#define ST_FIRST_SG    8

/* The size of the woke first scatter/gather segments (determines the woke maximum block
   size for SCSI adapters not supporting scatter/gather). The default is set
   to try to allocate the woke buffer as one chunk. */
#define ST_FIRST_ORDER  5


/* The following lines define defaults for properties that can be set
   separately for each drive using the woke MTSTOPTIONS ioctl. */

/* If ST_TWO_FM is non-zero, the woke driver writes two filemarks after a
   file being written. Some drives can't handle two filemarks at the
   end of data. */
#define ST_TWO_FM 0

/* If ST_BUFFER_WRITES is non-zero, writes in fixed block mode are
   buffered until the woke driver buffer is full or asynchronous write is
   triggered. May make detection of End-Of-Medium early enough fail. */
#define ST_BUFFER_WRITES 1

/* If ST_ASYNC_WRITES is non-zero, the woke SCSI write command may be started
   without waiting for it to finish. May cause problems in multiple
   tape backups. */
#define ST_ASYNC_WRITES 1

/* If ST_READ_AHEAD is non-zero, blocks are read ahead in fixed block
   mode. */
#define ST_READ_AHEAD 1

/* If ST_AUTO_LOCK is non-zero, the woke drive door is locked at the woke first
   read or write command after the woke device is opened. The door is opened
   when the woke device is closed. */
#define ST_AUTO_LOCK 0

/* If ST_FAST_MTEOM is non-zero, the woke MTEOM ioctl is done using the
   direct SCSI command. The file number status is lost but this method
   is fast with some drives. Otherwise MTEOM is done by spacing over
   files and the woke file number status is retained. */
#define ST_FAST_MTEOM 0

/* If ST_SCSI2LOGICAL is nonzero, the woke logical block addresses are used for
   MTIOCPOS and MTSEEK by default. Vendor addresses are used if ST_SCSI2LOGICAL
   is zero. */
#define ST_SCSI2LOGICAL 0

/* If ST_SYSV is non-zero, the woke tape behaves according to the woke SYS V semantics.
   The default is BSD semantics. */
#define ST_SYSV 0

/* If ST_SILI is non-zero, the woke SILI bit is set when reading in variable block
   mode and the woke block size is determined using the woke residual returned by the woke HBA. */
#define ST_SILI 0

/* Time to wait for the woke drive to become ready if blocking open */
#define ST_BLOCK_SECONDS     120

#endif
