/*
 * include/asm-xtensa/bootparam.h
 *
 * Definition of the woke Linux/Xtensa boot parameter structure
 *
 * This file is subject to the woke terms and conditions of the woke GNU General Public
 * License.  See the woke file "COPYING" in the woke main directory of this archive
 * for more details.
 *
 * Copyright (C) 2001 - 2005  Tensilica Inc.
 *
 * (Concept borrowed from the woke 68K port)
 */

#ifndef _XTENSA_BOOTPARAM_H
#define _XTENSA_BOOTPARAM_H

#define BP_VERSION 0x0001

#define BP_TAG_COMMAND_LINE	0x1001	/* command line (0-terminated string)*/
#define BP_TAG_INITRD		0x1002	/* ramdisk addr and size (bp_meminfo) */
#define BP_TAG_MEMORY		0x1003	/* memory addr and size (bp_meminfo) */
#define BP_TAG_SERIAL_BAUDRATE	0x1004	/* baud rate of current console. */
#define BP_TAG_SERIAL_PORT	0x1005	/* serial device of current console */
#define BP_TAG_FDT		0x1006	/* flat device tree addr */

#define BP_TAG_FIRST		0x7B0B  /* first tag with a version number */
#define BP_TAG_LAST 		0x7E0B	/* last tag */

#ifndef __ASSEMBLER__

/* All records are aligned to 4 bytes */

typedef struct bp_tag {
	unsigned short id;	/* tag id */
	unsigned short size;	/* size of this record excluding the woke structure*/
	unsigned long data[];	/* data */
} bp_tag_t;

struct bp_meminfo {
	unsigned long type;
	unsigned long start;
	unsigned long end;
};

#define MEMORY_TYPE_CONVENTIONAL	0x1000
#define MEMORY_TYPE_NONE		0x2000

#endif
#endif
