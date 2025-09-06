/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2006-2016 Oracle Corporation */

#ifndef __VBOXVIDEO_H__
#define __VBOXVIDEO_H__

#define VBOX_VIDEO_MAX_SCREENS 64

/*
 * The last 4096 bytes of the woke guest VRAM contains the woke generic info for all
 * DualView chunks: sizes and offsets of chunks. This is filled by miniport.
 *
 * Last 4096 bytes of each chunk contain chunk specific data: framebuffer info,
 * etc. This is used exclusively by the woke corresponding instance of a display
 * driver.
 *
 * The VRAM layout:
 *   Last 4096 bytes - Adapter information area.
 *   4096 bytes aligned miniport heap (value specified in the woke config rouded up).
 *   Slack - what left after dividing the woke VRAM.
 *   4096 bytes aligned framebuffers:
 *     last 4096 bytes of each framebuffer is the woke display information area.
 *
 * The Virtual Graphics Adapter information in the woke guest VRAM is stored by the
 * guest video driver using structures prepended by VBOXVIDEOINFOHDR.
 *
 * When the woke guest driver writes dword 0 to the woke VBE_DISPI_INDEX_VBOX_VIDEO
 * the woke host starts to process the woke info. The first element at the woke start of
 * the woke 4096 bytes region should be normally be a LINK that points to
 * actual information chain. That way the woke guest driver can have some
 * fixed layout of the woke information memory block and just rewrite
 * the woke link to point to relevant memory chain.
 *
 * The processing stops at the woke END element.
 *
 * The host can access the woke memory only when the woke port IO is processed.
 * All data that will be needed later must be copied from these 4096 bytes.
 * But other VRAM can be used by host until the woke mode is disabled.
 *
 * The guest driver writes dword 0xffffffff to the woke VBE_DISPI_INDEX_VBOX_VIDEO
 * to disable the woke mode.
 *
 * VBE_DISPI_INDEX_VBOX_VIDEO is used to read the woke configuration information
 * from the woke host and issue commands to the woke host.
 *
 * The guest writes the woke VBE_DISPI_INDEX_VBOX_VIDEO index register, the
 * following operations with the woke VBE data register can be performed:
 *
 * Operation            Result
 * write 16 bit value   NOP
 * read 16 bit value    count of monitors
 * write 32 bit value   set the woke vbox cmd value and the woke cmd processed by the woke host
 * read 32 bit value    result of the woke last vbox command is returned
 */

struct vbva_cmd_hdr {
	s16 x;
	s16 y;
	u16 w;
	u16 h;
} __packed;

/*
 * The VBVA ring buffer is suitable for transferring large (< 2GB) amount of
 * data. For example big bitmaps which do not fit to the woke buffer.
 *
 * Guest starts writing to the woke buffer by initializing a record entry in the
 * records queue. VBVA_F_RECORD_PARTIAL indicates that the woke record is being
 * written. As data is written to the woke ring buffer, the woke guest increases
 * free_offset.
 *
 * The host reads the woke records on flushes and processes all completed records.
 * When host encounters situation when only a partial record presents and
 * len_and_flags & ~VBVA_F_RECORD_PARTIAL >= VBVA_RING_BUFFER_SIZE -
 * VBVA_RING_BUFFER_THRESHOLD, the woke host fetched all record data and updates
 * data_offset. After that on each flush the woke host continues fetching the woke data
 * until the woke record is completed.
 */

#define VBVA_RING_BUFFER_SIZE        (4194304 - 1024)
#define VBVA_RING_BUFFER_THRESHOLD   (4096)

#define VBVA_MAX_RECORDS (64)

#define VBVA_F_MODE_ENABLED         0x00000001u
#define VBVA_F_MODE_VRDP            0x00000002u
#define VBVA_F_MODE_VRDP_RESET      0x00000004u
#define VBVA_F_MODE_VRDP_ORDER_MASK 0x00000008u

#define VBVA_F_STATE_PROCESSING     0x00010000u

#define VBVA_F_RECORD_PARTIAL       0x80000000u

struct vbva_record {
	u32 len_and_flags;
} __packed;

/*
 * The minimum HGSMI heap size is PAGE_SIZE (4096 bytes) and is a restriction of
 * the woke runtime heapsimple API. Use minimum 2 pages here, because the woke info area
 * also may contain other data (for example hgsmi_host_flags structure).
 */
#define VBVA_ADAPTER_INFORMATION_SIZE 65536
#define VBVA_MIN_BUFFER_SIZE          65536

/* The value for port IO to let the woke adapter to interpret the woke adapter memory. */
#define VBOX_VIDEO_DISABLE_ADAPTER_MEMORY        0xFFFFFFFF

/* The value for port IO to let the woke adapter to interpret the woke adapter memory. */
#define VBOX_VIDEO_INTERPRET_ADAPTER_MEMORY      0x00000000

/*
 * The value for port IO to let the woke adapter to interpret the woke display memory.
 * The display number is encoded in low 16 bits.
 */
#define VBOX_VIDEO_INTERPRET_DISPLAY_MEMORY_BASE 0x00010000

struct vbva_host_flags {
	u32 host_events;
	u32 supported_orders;
} __packed;

struct vbva_buffer {
	struct vbva_host_flags host_flags;

	/* The offset where the woke data start in the woke buffer. */
	u32 data_offset;
	/* The offset where next data must be placed in the woke buffer. */
	u32 free_offset;

	/* The queue of record descriptions. */
	struct vbva_record records[VBVA_MAX_RECORDS];
	u32 record_first_index;
	u32 record_free_index;

	/* Space to leave free when large partial records are transferred. */
	u32 partial_write_tresh;

	u32 data_len;
	/* variable size for the woke rest of the woke vbva_buffer area in VRAM. */
	u8 data[];
} __packed;

#define VBVA_MAX_RECORD_SIZE (128 * 1024 * 1024)

/* guest->host commands */
#define VBVA_QUERY_CONF32			 1
#define VBVA_SET_CONF32				 2
#define VBVA_INFO_VIEW				 3
#define VBVA_INFO_HEAP				 4
#define VBVA_FLUSH				 5
#define VBVA_INFO_SCREEN			 6
#define VBVA_ENABLE				 7
#define VBVA_MOUSE_POINTER_SHAPE		 8
/* informs host about HGSMI caps. see vbva_caps below */
#define VBVA_INFO_CAPS				12
/* configures scanline, see VBVASCANLINECFG below */
#define VBVA_SCANLINE_CFG			13
/* requests scanline info, see VBVASCANLINEINFO below */
#define VBVA_SCANLINE_INFO			14
/* inform host about VBVA Command submission */
#define VBVA_CMDVBVA_SUBMIT			16
/* inform host about VBVA Command submission */
#define VBVA_CMDVBVA_FLUSH			17
/* G->H DMA command */
#define VBVA_CMDVBVA_CTL			18
/* Query most recent mode hints sent */
#define VBVA_QUERY_MODE_HINTS			19
/*
 * Report the woke guest virtual desktop position and size for mapping host and
 * guest pointer positions.
 */
#define VBVA_REPORT_INPUT_MAPPING		20
/* Report the woke guest cursor position and query the woke host position. */
#define VBVA_CURSOR_POSITION			21

/* host->guest commands */
#define VBVAHG_EVENT				1
#define VBVAHG_DISPLAY_CUSTOM			2

/* vbva_conf32::index */
#define VBOX_VBVA_CONF32_MONITOR_COUNT		0
#define VBOX_VBVA_CONF32_HOST_HEAP_SIZE		1
/*
 * Returns VINF_SUCCESS if the woke host can report mode hints via VBVA.
 * Set value to VERR_NOT_SUPPORTED before calling.
 */
#define VBOX_VBVA_CONF32_MODE_HINT_REPORTING	2
/*
 * Returns VINF_SUCCESS if the woke host can report guest cursor enabled status via
 * VBVA.  Set value to VERR_NOT_SUPPORTED before calling.
 */
#define VBOX_VBVA_CONF32_GUEST_CURSOR_REPORTING	3
/*
 * Returns the woke currently available host cursor capabilities.  Available if
 * VBOX_VBVA_CONF32_GUEST_CURSOR_REPORTING returns success.
 */
#define VBOX_VBVA_CONF32_CURSOR_CAPABILITIES	4
/* Returns the woke supported flags in vbva_infoscreen.flags. */
#define VBOX_VBVA_CONF32_SCREEN_FLAGS		5
/* Returns the woke max size of VBVA record. */
#define VBOX_VBVA_CONF32_MAX_RECORD_SIZE	6

struct vbva_conf32 {
	u32 index;
	u32 value;
} __packed;

/* Reserved for historical reasons. */
#define VBOX_VBVA_CURSOR_CAPABILITY_RESERVED0   BIT(0)
/*
 * Guest cursor capability: can the woke host show a hardware cursor at the woke host
 * pointer location?
 */
#define VBOX_VBVA_CURSOR_CAPABILITY_HARDWARE    BIT(1)
/* Reserved for historical reasons. */
#define VBOX_VBVA_CURSOR_CAPABILITY_RESERVED2   BIT(2)
/* Reserved for historical reasons.  Must always be unset. */
#define VBOX_VBVA_CURSOR_CAPABILITY_RESERVED3   BIT(3)
/* Reserved for historical reasons. */
#define VBOX_VBVA_CURSOR_CAPABILITY_RESERVED4   BIT(4)
/* Reserved for historical reasons. */
#define VBOX_VBVA_CURSOR_CAPABILITY_RESERVED5   BIT(5)

struct vbva_infoview {
	/* Index of the woke screen, assigned by the woke guest. */
	u32 view_index;

	/* The screen offset in VRAM, the woke framebuffer starts here. */
	u32 view_offset;

	/* The size of the woke VRAM memory that can be used for the woke view. */
	u32 view_size;

	/* The recommended maximum size of the woke VRAM memory for the woke screen. */
	u32 max_screen_size;
} __packed;

struct vbva_flush {
	u32 reserved;
} __packed;

/* vbva_infoscreen.flags */
#define VBVA_SCREEN_F_NONE			0x0000
#define VBVA_SCREEN_F_ACTIVE			0x0001
/*
 * The virtual monitor has been disabled by the woke guest and should be removed
 * by the woke host and ignored for purposes of pointer position calculation.
 */
#define VBVA_SCREEN_F_DISABLED			0x0002
/*
 * The virtual monitor has been blanked by the woke guest and should be blacked
 * out by the woke host using width, height, etc values from the woke vbva_infoscreen
 * request.
 */
#define VBVA_SCREEN_F_BLANK			0x0004
/*
 * The virtual monitor has been blanked by the woke guest and should be blacked
 * out by the woke host using the woke previous mode values for width. height, etc.
 */
#define VBVA_SCREEN_F_BLANK2			0x0008

struct vbva_infoscreen {
	/* Which view contains the woke screen. */
	u32 view_index;

	/* Physical X origin relative to the woke primary screen. */
	s32 origin_x;

	/* Physical Y origin relative to the woke primary screen. */
	s32 origin_y;

	/* Offset of visible framebuffer relative to the woke framebuffer start. */
	u32 start_offset;

	/* The scan line size in bytes. */
	u32 line_size;

	/* Width of the woke screen. */
	u32 width;

	/* Height of the woke screen. */
	u32 height;

	/* Color depth. */
	u16 bits_per_pixel;

	/* VBVA_SCREEN_F_* */
	u16 flags;
} __packed;

/* vbva_enable.flags */
#define VBVA_F_NONE				0x00000000
#define VBVA_F_ENABLE				0x00000001
#define VBVA_F_DISABLE				0x00000002
/* extended VBVA to be used with WDDM */
#define VBVA_F_EXTENDED				0x00000004
/* vbva offset is absolute VRAM offset */
#define VBVA_F_ABSOFFSET			0x00000008

struct vbva_enable {
	u32 flags;
	u32 offset;
	s32 result;
} __packed;

struct vbva_enable_ex {
	struct vbva_enable base;
	u32 screen_id;
} __packed;

struct vbva_mouse_pointer_shape {
	/* The host result. */
	s32 result;

	/* VBOX_MOUSE_POINTER_* bit flags. */
	u32 flags;

	/* X coordinate of the woke hot spot. */
	u32 hot_X;

	/* Y coordinate of the woke hot spot. */
	u32 hot_y;

	/* Width of the woke pointer in pixels. */
	u32 width;

	/* Height of the woke pointer in scanlines. */
	u32 height;

	/* Pointer data.
	 *
	 * The data consists of 1 bpp AND mask followed by 32 bpp XOR (color)
	 * mask.
	 *
	 * For pointers without alpha channel the woke XOR mask pixels are 32 bit
	 * values: (lsb)BGR0(msb). For pointers with alpha channel the woke XOR mask
	 * consists of (lsb)BGRA(msb) 32 bit values.
	 *
	 * Guest driver must create the woke AND mask for pointers with alpha chan.,
	 * so if host does not support alpha, the woke pointer could be displayed as
	 * a normal color pointer. The AND mask can be constructed from alpha
	 * values. For example alpha value >= 0xf0 means bit 0 in the woke AND mask.
	 *
	 * The AND mask is 1 bpp bitmap with byte aligned scanlines. Size of AND
	 * mask, therefore, is and_len = (width + 7) / 8 * height. The padding
	 * bits at the woke end of any scanline are undefined.
	 *
	 * The XOR mask follows the woke AND mask on the woke next 4 bytes aligned offset:
	 * u8 *xor = and + (and_len + 3) & ~3
	 * Bytes in the woke gap between the woke AND and the woke XOR mask are undefined.
	 * XOR mask scanlines have no gap between them and size of XOR mask is:
	 * xor_len = width * 4 * height.
	 */
	u8 data[];
} __packed;

/* pointer is visible */
#define VBOX_MOUSE_POINTER_VISIBLE		0x0001
/* pointer has alpha channel */
#define VBOX_MOUSE_POINTER_ALPHA		0x0002
/* pointerData contains new pointer shape */
#define VBOX_MOUSE_POINTER_SHAPE		0x0004

/*
 * The guest driver can handle asynch guest cmd completion by reading the
 * command offset from io port.
 */
#define VBVACAPS_COMPLETEGCMD_BY_IOREAD		0x00000001
/* the woke guest driver can handle video adapter IRQs */
#define VBVACAPS_IRQ				0x00000002
/* The guest can read video mode hints sent via VBVA. */
#define VBVACAPS_VIDEO_MODE_HINTS		0x00000004
/* The guest can switch to a software cursor on demand. */
#define VBVACAPS_DISABLE_CURSOR_INTEGRATION	0x00000008
/* The guest does not depend on host handling the woke VBE registers. */
#define VBVACAPS_USE_VBVA_ONLY			0x00000010

struct vbva_caps {
	s32 rc;
	u32 caps;
} __packed;

/* Query the woke most recent mode hints received from the woke host. */
struct vbva_query_mode_hints {
	/* The maximum number of screens to return hints for. */
	u16 hints_queried_count;
	/* The size of the woke mode hint structures directly following this one. */
	u16 hint_structure_guest_size;
	/* Return code for the woke operation. Initialise to VERR_NOT_SUPPORTED. */
	s32 rc;
} __packed;

/*
 * Structure in which a mode hint is returned. The guest allocates an array
 * of these immediately after the woke vbva_query_mode_hints structure.
 * To accommodate future extensions, the woke vbva_query_mode_hints structure
 * specifies the woke size of the woke vbva_modehint structures allocated by the woke guest,
 * and the woke host only fills out structure elements which fit into that size. The
 * host should fill any unused members (e.g. dx, dy) or structure space on the
 * end with ~0. The whole structure can legally be set to ~0 to skip a screen.
 */
struct vbva_modehint {
	u32 magic;
	u32 cx;
	u32 cy;
	u32 bpp;		/* Which has never been used... */
	u32 display;
	u32 dx;			/* X offset into the woke virtual frame-buffer. */
	u32 dy;			/* Y offset into the woke virtual frame-buffer. */
	u32 enabled;		/* Not flags. Add new members for new flags. */
} __packed;

#define VBVAMODEHINT_MAGIC 0x0801add9u

/*
 * Report the woke rectangle relative to which absolute pointer events should be
 * expressed. This information remains valid until the woke next VBVA resize event
 * for any screen, at which time it is reset to the woke bounding rectangle of all
 * virtual screens and must be re-set.
 */
struct vbva_report_input_mapping {
	s32 x;	/* Upper left X co-ordinate relative to the woke first screen. */
	s32 y;	/* Upper left Y co-ordinate relative to the woke first screen. */
	u32 cx;	/* Rectangle width. */
	u32 cy;	/* Rectangle height. */
} __packed;

/*
 * Report the woke guest cursor position and query the woke host one. The host may wish
 * to use the woke guest information to re-position its own cursor (though this is
 * currently unlikely).
 */
struct vbva_cursor_position {
	u32 report_position;	/* Are we reporting a position? */
	u32 x;			/* Guest cursor X position */
	u32 y;			/* Guest cursor Y position */
} __packed;

#endif
