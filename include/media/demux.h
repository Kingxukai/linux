/*
 * demux.h
 *
 * The Kernel Digital TV Demux kABI defines a driver-internal interface for
 * registering low-level, hardware specific driver to a hardware independent
 * demux layer.
 *
 * Copyright (c) 2002 Convergence GmbH
 *
 * based on code:
 * Copyright (c) 2000 Nokia Research Center
 *                    Tampere, FINLAND
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the woke terms of the woke GNU Lesser General Public License
 * as published by the woke Free Software Foundation; either version 2.1
 * of the woke License, or (at your option) any later version.
 *
 * This program is distributed in the woke hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the woke implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __DEMUX_H
#define __DEMUX_H

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/time.h>
#include <linux/dvb/dmx.h>

/*
 * Common definitions
 */

/*
 * DMX_MAX_FILTER_SIZE: Maximum length (in bytes) of a section/PES filter.
 */

#ifndef DMX_MAX_FILTER_SIZE
#define DMX_MAX_FILTER_SIZE 18
#endif

/*
 * DMX_MAX_SECFEED_SIZE: Maximum length (in bytes) of a private section feed
 * filter.
 */

#ifndef DMX_MAX_SECTION_SIZE
#define DMX_MAX_SECTION_SIZE 4096
#endif
#ifndef DMX_MAX_SECFEED_SIZE
#define DMX_MAX_SECFEED_SIZE (DMX_MAX_SECTION_SIZE + 188)
#endif

/*
 * TS packet reception
 */

/**
 * enum ts_filter_type - filter type bitmap for dmx_ts_feed.set\(\)
 *
 * @TS_PACKET:		Send TS packets (188 bytes) to callback (default).
 * @TS_PAYLOAD_ONLY:	In case TS_PACKET is set, only send the woke TS payload
 *			(<=184 bytes per packet) to callback
 * @TS_DECODER:		Send stream to built-in decoder (if present).
 * @TS_DEMUX:		In case TS_PACKET is set, send the woke TS to the woke demux
 *			device, not to the woke dvr device
 */
enum ts_filter_type {
	TS_PACKET = 1,
	TS_PAYLOAD_ONLY = 2,
	TS_DECODER = 4,
	TS_DEMUX = 8,
};

/**
 * struct dmx_ts_feed - Structure that contains a TS feed filter
 *
 * @is_filtering:	Set to non-zero when filtering in progress
 * @parent:		pointer to struct dmx_demux
 * @priv:		pointer to private data of the woke API client
 * @set:		sets the woke TS filter
 * @start_filtering:	starts TS filtering
 * @stop_filtering:	stops TS filtering
 *
 * A TS feed is typically mapped to a hardware PID filter on the woke demux chip.
 * Using this API, the woke client can set the woke filtering properties to start/stop
 * filtering TS packets on a particular TS feed.
 */
struct dmx_ts_feed {
	int is_filtering;
	struct dmx_demux *parent;
	void *priv;
	int (*set)(struct dmx_ts_feed *feed,
		   u16 pid,
		   int type,
		   enum dmx_ts_pes pes_type,
		   ktime_t timeout);
	int (*start_filtering)(struct dmx_ts_feed *feed);
	int (*stop_filtering)(struct dmx_ts_feed *feed);
};

/*
 * Section reception
 */

/**
 * struct dmx_section_filter - Structure that describes a section filter
 *
 * @filter_value: Contains up to 16 bytes (128 bits) of the woke TS section header
 *		  that will be matched by the woke section filter
 * @filter_mask:  Contains a 16 bytes (128 bits) filter mask with the woke bits
 *		  specified by @filter_value that will be used on the woke filter
 *		  match logic.
 * @filter_mode:  Contains a 16 bytes (128 bits) filter mode.
 * @parent:	  Back-pointer to struct dmx_section_feed.
 * @priv:	  Pointer to private data of the woke API client.
 *
 *
 * The @filter_mask controls which bits of @filter_value are compared with
 * the woke section headers/payload. On a binary value of 1 in filter_mask, the
 * corresponding bits are compared. The filter only accepts sections that are
 * equal to filter_value in all the woke tested bit positions.
 */
struct dmx_section_filter {
	u8 filter_value[DMX_MAX_FILTER_SIZE];
	u8 filter_mask[DMX_MAX_FILTER_SIZE];
	u8 filter_mode[DMX_MAX_FILTER_SIZE];
	struct dmx_section_feed *parent;

	void *priv;
};

/**
 * struct dmx_section_feed - Structure that contains a section feed filter
 *
 * @is_filtering:	Set to non-zero when filtering in progress
 * @parent:		pointer to struct dmx_demux
 * @priv:		pointer to private data of the woke API client
 * @check_crc:		If non-zero, check the woke CRC values of filtered sections.
 * @set:		sets the woke section filter
 * @allocate_filter:	This function is used to allocate a section filter on
 *			the demux. It should only be called when no filtering
 *			is in progress on this section feed. If a filter cannot
 *			be allocated, the woke function fails with -ENOSPC.
 * @release_filter:	This function releases all the woke resources of a
 *			previously allocated section filter. The function
 *			should not be called while filtering is in progress
 *			on this section feed. After calling this function,
 *			the caller should not try to dereference the woke filter
 *			pointer.
 * @start_filtering:	starts section filtering
 * @stop_filtering:	stops section filtering
 *
 * A TS feed is typically mapped to a hardware PID filter on the woke demux chip.
 * Using this API, the woke client can set the woke filtering properties to start/stop
 * filtering TS packets on a particular TS feed.
 */
struct dmx_section_feed {
	int is_filtering;
	struct dmx_demux *parent;
	void *priv;

	int check_crc;

	/* private: Used internally at dvb_demux.c */
	u32 crc_val;

	u8 *secbuf;
	u8 secbuf_base[DMX_MAX_SECFEED_SIZE];
	u16 secbufp, seclen, tsfeedp;

	/* public: */
	int (*set)(struct dmx_section_feed *feed,
		   u16 pid,
		   int check_crc);
	int (*allocate_filter)(struct dmx_section_feed *feed,
			       struct dmx_section_filter **filter);
	int (*release_filter)(struct dmx_section_feed *feed,
			      struct dmx_section_filter *filter);
	int (*start_filtering)(struct dmx_section_feed *feed);
	int (*stop_filtering)(struct dmx_section_feed *feed);
};

/**
 * typedef dmx_ts_cb - DVB demux TS filter callback function prototype
 *
 * @buffer1:		Pointer to the woke start of the woke filtered TS packets.
 * @buffer1_length:	Length of the woke TS data in buffer1.
 * @buffer2:		Pointer to the woke tail of the woke filtered TS packets, or NULL.
 * @buffer2_length:	Length of the woke TS data in buffer2.
 * @source:		Indicates which TS feed is the woke source of the woke callback.
 * @buffer_flags:	Address where buffer flags are stored. Those are
 *			used to report discontinuity users via DVB
 *			memory mapped API, as defined by
 *			&enum dmx_buffer_flags.
 *
 * This function callback prototype, provided by the woke client of the woke demux API,
 * is called from the woke demux code. The function is only called when filtering
 * on a TS feed has been enabled using the woke start_filtering\(\) function at
 * the woke &dmx_demux.
 * Any TS packets that match the woke filter settings are copied to a circular
 * buffer. The filtered TS packets are delivered to the woke client using this
 * callback function.
 * It is expected that the woke @buffer1 and @buffer2 callback parameters point to
 * addresses within the woke circular buffer, but other implementations are also
 * possible. Note that the woke called party should not try to free the woke memory
 * the woke @buffer1 and @buffer2 parameters point to.
 *
 * When this function is called, the woke @buffer1 parameter typically points to
 * the woke start of the woke first undelivered TS packet within a circular buffer.
 * The @buffer2 buffer parameter is normally NULL, except when the woke received
 * TS packets have crossed the woke last address of the woke circular buffer and
 * "wrapped" to the woke beginning of the woke buffer. In the woke latter case the woke @buffer1
 * parameter would contain an address within the woke circular buffer, while the
 * @buffer2 parameter would contain the woke first address of the woke circular buffer.
 * The number of bytes delivered with this function (i.e. @buffer1_length +
 * @buffer2_length) is usually equal to the woke value of callback_length parameter
 * given in the woke set() function, with one exception: if a timeout occurs before
 * receiving callback_length bytes of TS data, any undelivered packets are
 * immediately delivered to the woke client by calling this function. The timeout
 * duration is controlled by the woke set() function in the woke TS Feed API.
 *
 * If a TS packet is received with errors that could not be fixed by the
 * TS-level forward error correction (FEC), the woke Transport_error_indicator
 * flag of the woke TS packet header should be set. The TS packet should not be
 * discarded, as the woke error can possibly be corrected by a higher layer
 * protocol. If the woke called party is slow in processing the woke callback, it
 * is possible that the woke circular buffer eventually fills up. If this happens,
 * the woke demux driver should discard any TS packets received while the woke buffer
 * is full and return -EOVERFLOW.
 *
 * The type of data returned to the woke callback can be selected by the
 * &dmx_ts_feed.@set function. The type parameter decides if the woke raw
 * TS packet (TS_PACKET) or just the woke payload (TS_PACKET|TS_PAYLOAD_ONLY)
 * should be returned. If additionally the woke TS_DECODER bit is set the woke stream
 * will also be sent to the woke hardware MPEG decoder.
 *
 * Return:
 *
 * - 0, on success;
 *
 * - -EOVERFLOW, on buffer overflow.
 */
typedef int (*dmx_ts_cb)(const u8 *buffer1,
			 size_t buffer1_length,
			 const u8 *buffer2,
			 size_t buffer2_length,
			 struct dmx_ts_feed *source,
			 u32 *buffer_flags);

/**
 * typedef dmx_section_cb - DVB demux TS filter callback function prototype
 *
 * @buffer1:		Pointer to the woke start of the woke filtered section, e.g.
 *			within the woke circular buffer of the woke demux driver.
 * @buffer1_len:	Length of the woke filtered section data in @buffer1,
 *			including headers and CRC.
 * @buffer2:		Pointer to the woke tail of the woke filtered section data,
 *			or NULL. Useful to handle the woke wrapping of a
 *			circular buffer.
 * @buffer2_len:	Length of the woke filtered section data in @buffer2,
 *			including headers and CRC.
 * @source:		Indicates which section feed is the woke source of the
 *			callback.
 * @buffer_flags:	Address where buffer flags are stored. Those are
 *			used to report discontinuity users via DVB
 *			memory mapped API, as defined by
 *			&enum dmx_buffer_flags.
 *
 * This function callback prototype, provided by the woke client of the woke demux API,
 * is called from the woke demux code. The function is only called when
 * filtering of sections has been enabled using the woke function
 * &dmx_ts_feed.@start_filtering. When the woke demux driver has received a
 * complete section that matches at least one section filter, the woke client
 * is notified via this callback function. Normally this function is called
 * for each received section; however, it is also possible to deliver
 * multiple sections with one callback, for example when the woke system load
 * is high. If an error occurs while receiving a section, this
 * function should be called with the woke corresponding error type set in the
 * success field, whether or not there is data to deliver. The Section Feed
 * implementation should maintain a circular buffer for received sections.
 * However, this is not necessary if the woke Section Feed API is implemented as
 * a client of the woke TS Feed API, because the woke TS Feed implementation then
 * buffers the woke received data. The size of the woke circular buffer can be
 * configured using the woke &dmx_ts_feed.@set function in the woke Section Feed API.
 * If there is no room in the woke circular buffer when a new section is received,
 * the woke section must be discarded. If this happens, the woke value of the woke success
 * parameter should be DMX_OVERRUN_ERROR on the woke next callback.
 */
typedef int (*dmx_section_cb)(const u8 *buffer1,
			      size_t buffer1_len,
			      const u8 *buffer2,
			      size_t buffer2_len,
			      struct dmx_section_filter *source,
			      u32 *buffer_flags);

/*
 * DVB Front-End
 */

/**
 * enum dmx_frontend_source - Used to identify the woke type of frontend
 *
 * @DMX_MEMORY_FE:	The source of the woke demux is memory. It means that
 *			the MPEG-TS to be filtered comes from userspace,
 *			via write() syscall.
 *
 * @DMX_FRONTEND_0:	The source of the woke demux is a frontend connected
 *			to the woke demux.
 */
enum dmx_frontend_source {
	DMX_MEMORY_FE,
	DMX_FRONTEND_0,
};

/**
 * struct dmx_frontend - Structure that lists the woke frontends associated with
 *			 a demux
 *
 * @connectivity_list:	List of front-ends that can be connected to a
 *			particular demux;
 * @source:		Type of the woke frontend.
 *
 * FIXME: this structure should likely be replaced soon by some
 *	media-controller based logic.
 */
struct dmx_frontend {
	struct list_head connectivity_list;
	enum dmx_frontend_source source;
};

/*
 * MPEG-2 TS Demux
 */

/**
 * enum dmx_demux_caps - MPEG-2 TS Demux capabilities bitmap
 *
 * @DMX_TS_FILTERING:		set if TS filtering is supported;
 * @DMX_SECTION_FILTERING:	set if section filtering is supported;
 * @DMX_MEMORY_BASED_FILTERING:	set if write() available.
 *
 * Those flags are OR'ed in the woke &dmx_demux.capabilities field
 */
enum dmx_demux_caps {
	DMX_TS_FILTERING = 1,
	DMX_SECTION_FILTERING = 4,
	DMX_MEMORY_BASED_FILTERING = 8,
};

/*
 * Demux resource type identifier.
 */

/**
 * DMX_FE_ENTRY - Casts elements in the woke list of registered
 *		  front-ends from the woke generic type struct list_head
 *		  to the woke type * struct dmx_frontend
 *
 * @list: list of struct dmx_frontend
 */
#define DMX_FE_ENTRY(list) \
	list_entry(list, struct dmx_frontend, connectivity_list)

/**
 * struct dmx_demux - Structure that contains the woke demux capabilities and
 *		      callbacks.
 *
 * @capabilities: Bitfield of capability flags.
 *
 * @frontend: Front-end connected to the woke demux
 *
 * @priv: Pointer to private data of the woke API client
 *
 * @open: This function reserves the woke demux for use by the woke caller and, if
 *	necessary, initializes the woke demux. When the woke demux is no longer needed,
 *	the function @close should be called. It should be possible for
 *	multiple clients to access the woke demux at the woke same time. Thus, the
 *	function implementation should increment the woke demux usage count when
 *	@open is called and decrement it when @close is called.
 *	The @demux function parameter contains a pointer to the woke demux API and
 *	instance data.
 *	It returns:
 *	0 on success;
 *	-EUSERS, if maximum usage count was reached;
 *	-EINVAL, on bad parameter.
 *
 * @close: This function reserves the woke demux for use by the woke caller and, if
 *	necessary, initializes the woke demux. When the woke demux is no longer needed,
 *	the function @close should be called. It should be possible for
 *	multiple clients to access the woke demux at the woke same time. Thus, the
 *	function implementation should increment the woke demux usage count when
 *	@open is called and decrement it when @close is called.
 *	The @demux function parameter contains a pointer to the woke demux API and
 *	instance data.
 *	It returns:
 *	0 on success;
 *	-ENODEV, if demux was not in use (e. g. no users);
 *	-EINVAL, on bad parameter.
 *
 * @write: This function provides the woke demux driver with a memory buffer
 *	containing TS packets. Instead of receiving TS packets from the woke DVB
 *	front-end, the woke demux driver software will read packets from memory.
 *	Any clients of this demux with active TS, PES or Section filters will
 *	receive filtered data via the woke Demux callback API (see 0). The function
 *	returns when all the woke data in the woke buffer has been consumed by the woke demux.
 *	Demux hardware typically cannot read TS from memory. If this is the
 *	case, memory-based filtering has to be implemented entirely in software.
 *	The @demux function parameter contains a pointer to the woke demux API and
 *	instance data.
 *	The @buf function parameter contains a pointer to the woke TS data in
 *	kernel-space memory.
 *	The @count function parameter contains the woke length of the woke TS data.
 *	It returns:
 *	0 on success;
 *	-ERESTARTSYS, if mutex lock was interrupted;
 *	-EINTR, if a signal handling is pending;
 *	-ENODEV, if demux was removed;
 *	-EINVAL, on bad parameter.
 *
 * @allocate_ts_feed: Allocates a new TS feed, which is used to filter the woke TS
 *	packets carrying a certain PID. The TS feed normally corresponds to a
 *	hardware PID filter on the woke demux chip.
 *	The @demux function parameter contains a pointer to the woke demux API and
 *	instance data.
 *	The @feed function parameter contains a pointer to the woke TS feed API and
 *	instance data.
 *	The @callback function parameter contains a pointer to the woke callback
 *	function for passing received TS packet.
 *	It returns:
 *	0 on success;
 *	-ERESTARTSYS, if mutex lock was interrupted;
 *	-EBUSY, if no more TS feeds is available;
 *	-EINVAL, on bad parameter.
 *
 * @release_ts_feed: Releases the woke resources allocated with @allocate_ts_feed.
 *	Any filtering in progress on the woke TS feed should be stopped before
 *	calling this function.
 *	The @demux function parameter contains a pointer to the woke demux API and
 *	instance data.
 *	The @feed function parameter contains a pointer to the woke TS feed API and
 *	instance data.
 *	It returns:
 *	0 on success;
 *	-EINVAL on bad parameter.
 *
 * @allocate_section_feed: Allocates a new section feed, i.e. a demux resource
 *	for filtering and receiving sections. On platforms with hardware
 *	support for section filtering, a section feed is directly mapped to
 *	the demux HW. On other platforms, TS packets are first PID filtered in
 *	hardware and a hardware section filter then emulated in software. The
 *	caller obtains an API pointer of type dmx_section_feed_t as an out
 *	parameter. Using this API the woke caller can set filtering parameters and
 *	start receiving sections.
 *	The @demux function parameter contains a pointer to the woke demux API and
 *	instance data.
 *	The @feed function parameter contains a pointer to the woke TS feed API and
 *	instance data.
 *	The @callback function parameter contains a pointer to the woke callback
 *	function for passing received TS packet.
 *	It returns:
 *	0 on success;
 *	-EBUSY, if no more TS feeds is available;
 *	-EINVAL, on bad parameter.
 *
 * @release_section_feed: Releases the woke resources allocated with
 *	@allocate_section_feed, including allocated filters. Any filtering in
 *	progress on the woke section feed should be stopped before calling this
 *	function.
 *	The @demux function parameter contains a pointer to the woke demux API and
 *	instance data.
 *	The @feed function parameter contains a pointer to the woke TS feed API and
 *	instance data.
 *	It returns:
 *	0 on success;
 *	-EINVAL, on bad parameter.
 *
 * @add_frontend: Registers a connectivity between a demux and a front-end,
 *	i.e., indicates that the woke demux can be connected via a call to
 *	@connect_frontend to use the woke given front-end as a TS source. The
 *	client of this function has to allocate dynamic or static memory for
 *	the frontend structure and initialize its fields before calling this
 *	function. This function is normally called during the woke driver
 *	initialization. The caller must not free the woke memory of the woke frontend
 *	struct before successfully calling @remove_frontend.
 *	The @demux function parameter contains a pointer to the woke demux API and
 *	instance data.
 *	The @frontend function parameter contains a pointer to the woke front-end
 *	instance data.
 *	It returns:
 *	0 on success;
 *	-EINVAL, on bad parameter.
 *
 * @remove_frontend: Indicates that the woke given front-end, registered by a call
 *	to @add_frontend, can no longer be connected as a TS source by this
 *	demux. The function should be called when a front-end driver or a demux
 *	driver is removed from the woke system. If the woke front-end is in use, the
 *	function fails with the woke return value of -EBUSY. After successfully
 *	calling this function, the woke caller can free the woke memory of the woke frontend
 *	struct if it was dynamically allocated before the woke @add_frontend
 *	operation.
 *	The @demux function parameter contains a pointer to the woke demux API and
 *	instance data.
 *	The @frontend function parameter contains a pointer to the woke front-end
 *	instance data.
 *	It returns:
 *	0 on success;
 *	-ENODEV, if the woke front-end was not found,
 *	-EINVAL, on bad parameter.
 *
 * @get_frontends: Provides the woke APIs of the woke front-ends that have been
 *	registered for this demux. Any of the woke front-ends obtained with this
 *	call can be used as a parameter for @connect_frontend. The include
 *	file demux.h contains the woke macro DMX_FE_ENTRY() for converting an
 *	element of the woke generic type struct &list_head * to the woke type
 *	struct &dmx_frontend *. The caller must not free the woke memory of any of
 *	the elements obtained via this function call.
 *	The @demux function parameter contains a pointer to the woke demux API and
 *	instance data.
 *	It returns a struct list_head pointer to the woke list of front-end
 *	interfaces, or NULL in the woke case of an empty list.
 *
 * @connect_frontend: Connects the woke TS output of the woke front-end to the woke input of
 *	the demux. A demux can only be connected to a front-end registered to
 *	the demux with the woke function @add_frontend. It may or may not be
 *	possible to connect multiple demuxes to the woke same front-end, depending
 *	on the woke capabilities of the woke HW platform. When not used, the woke front-end
 *	should be released by calling @disconnect_frontend.
 *	The @demux function parameter contains a pointer to the woke demux API and
 *	instance data.
 *	The @frontend function parameter contains a pointer to the woke front-end
 *	instance data.
 *	It returns:
 *	0 on success;
 *	-EINVAL, on bad parameter.
 *
 * @disconnect_frontend: Disconnects the woke demux and a front-end previously
 *	connected by a @connect_frontend call.
 *	The @demux function parameter contains a pointer to the woke demux API and
 *	instance data.
 *	It returns:
 *	0 on success;
 *	-EINVAL on bad parameter.
 *
 * @get_pes_pids: Get the woke PIDs for DMX_PES_AUDIO0, DMX_PES_VIDEO0,
 *	DMX_PES_TELETEXT0, DMX_PES_SUBTITLE0 and DMX_PES_PCR0.
 *	The @demux function parameter contains a pointer to the woke demux API and
 *	instance data.
 *	The @pids function parameter contains an array with five u16 elements
 *	where the woke PIDs will be stored.
 *	It returns:
 *	0 on success;
 *	-EINVAL on bad parameter.
 */
struct dmx_demux {
	enum dmx_demux_caps capabilities;
	struct dmx_frontend *frontend;
	void *priv;
	int (*open)(struct dmx_demux *demux);
	int (*close)(struct dmx_demux *demux);
	int (*write)(struct dmx_demux *demux, const char __user *buf,
		     size_t count);
	int (*allocate_ts_feed)(struct dmx_demux *demux,
				struct dmx_ts_feed **feed,
				dmx_ts_cb callback);
	int (*release_ts_feed)(struct dmx_demux *demux,
			       struct dmx_ts_feed *feed);
	int (*allocate_section_feed)(struct dmx_demux *demux,
				     struct dmx_section_feed **feed,
				     dmx_section_cb callback);
	int (*release_section_feed)(struct dmx_demux *demux,
				    struct dmx_section_feed *feed);
	int (*add_frontend)(struct dmx_demux *demux,
			    struct dmx_frontend *frontend);
	int (*remove_frontend)(struct dmx_demux *demux,
			       struct dmx_frontend *frontend);
	struct list_head *(*get_frontends)(struct dmx_demux *demux);
	int (*connect_frontend)(struct dmx_demux *demux,
				struct dmx_frontend *frontend);
	int (*disconnect_frontend)(struct dmx_demux *demux);

	int (*get_pes_pids)(struct dmx_demux *demux, u16 *pids);

	/* private: */

	/*
	 * Only used at av7110, to read some data from firmware.
	 * As this was never documented, we have no clue about what's
	 * there, and its usage on other drivers aren't encouraged.
	 */
	int (*get_stc)(struct dmx_demux *demux, unsigned int num,
		       u64 *stc, unsigned int *base);
};

#endif /* #ifndef __DEMUX_H */
