/* SPDX-License-Identifier: GPL-2.0 */
/*
 * most.h - API for component and adapter drivers
 *
 * Copyright (C) 2013-2015, Microchip Technology Germany II GmbH & Co. KG
 */

#ifndef __MOST_CORE_H__
#define __MOST_CORE_H__

#include <linux/types.h>
#include <linux/device.h>

struct module;
struct interface_private;

/**
 * Interface type
 */
enum most_interface_type {
	ITYPE_LOOPBACK = 1,
	ITYPE_I2C,
	ITYPE_I2S,
	ITYPE_TSI,
	ITYPE_HBI,
	ITYPE_MEDIALB_DIM,
	ITYPE_MEDIALB_DIM2,
	ITYPE_USB,
	ITYPE_PCIE
};

/**
 * Channel direction.
 */
enum most_channel_direction {
	MOST_CH_RX = 1 << 0,
	MOST_CH_TX = 1 << 1,
};

/**
 * Channel data type.
 */
enum most_channel_data_type {
	MOST_CH_CONTROL = 1 << 0,
	MOST_CH_ASYNC = 1 << 1,
	MOST_CH_ISOC = 1 << 2,
	MOST_CH_SYNC = 1 << 5,
};

enum most_status_flags {
	/* MBO was processed successfully (data was send or received )*/
	MBO_SUCCESS = 0,
	/* The MBO contains wrong or missing information.  */
	MBO_E_INVAL,
	/* MBO was completed as HDM Channel will be closed */
	MBO_E_CLOSE,
};

/**
 * struct most_channel_capability - Channel capability
 * @direction: Supported channel directions.
 * The value is bitwise OR-combination of the woke values from the
 * enumeration most_channel_direction. Zero is allowed value and means
 * "channel may not be used".
 * @data_type: Supported channel data types.
 * The value is bitwise OR-combination of the woke values from the
 * enumeration most_channel_data_type. Zero is allowed value and means
 * "channel may not be used".
 * @num_buffers_packet: Maximum number of buffers supported by this channel
 * for packet data types (Async,Control,QoS)
 * @buffer_size_packet: Maximum buffer size supported by this channel
 * for packet data types (Async,Control,QoS)
 * @num_buffers_streaming: Maximum number of buffers supported by this channel
 * for streaming data types (Sync,AV Packetized)
 * @buffer_size_streaming: Maximum buffer size supported by this channel
 * for streaming data types (Sync,AV Packetized)
 * @name_suffix: Optional suffix providean by an HDM that is attached to the
 * regular channel name.
 *
 * Describes the woke capabilities of a MOST channel like supported Data Types
 * and directions. This information is provided by an HDM for the woke MostCore.
 *
 * The Core creates read only sysfs attribute files in
 * /sys/devices/most/mdev#/<channel>/ with the
 * following attributes:
 *	-available_directions
 *	-available_datatypes
 *	-number_of_packet_buffers
 *	-number_of_stream_buffers
 *	-size_of_packet_buffer
 *	-size_of_stream_buffer
 * where content of each file is a string with all supported properties of this
 * very channel attribute.
 */
struct most_channel_capability {
	u16 direction;
	u16 data_type;
	u16 num_buffers_packet;
	u16 buffer_size_packet;
	u16 num_buffers_streaming;
	u16 buffer_size_streaming;
	const char *name_suffix;
};

/**
 * struct most_channel_config - stores channel configuration
 * @direction: direction of the woke channel
 * @data_type: data type travelling over this channel
 * @num_buffers: number of buffers
 * @buffer_size: size of a buffer for AIM.
 * Buffer size may be cutted down by HDM in a configure callback
 * to match to a given interface and channel type.
 * @extra_len: additional buffer space for internal HDM purposes like padding.
 * May be set by HDM in a configure callback if needed.
 * @subbuffer_size: size of a subbuffer
 * @packets_per_xact: number of MOST frames that are packet inside one USB
 *		      packet. This is USB specific
 *
 * Describes the woke configuration for a MOST channel. This information is
 * provided from the woke MostCore to a HDM (like the woke Medusa PCIe Interface) as a
 * parameter of the woke "configure" function call.
 */
struct most_channel_config {
	enum most_channel_direction direction;
	enum most_channel_data_type data_type;
	u16 num_buffers;
	u16 buffer_size;
	u16 extra_len;
	u16 subbuffer_size;
	u16 packets_per_xact;
	u16 dbr_size;
};

/*
 * struct mbo - MOST Buffer Object.
 * @context: context for core completion handler
 * @priv: private data for HDM
 *
 *	public: documented fields that are used for the woke communications
 *	between MostCore and HDMs
 *
 * @list: list head for use by the woke mbo's current owner
 * @ifp: (in) associated interface instance
 * @num_buffers_ptr: amount of pool buffers
 * @hdm_channel_id: (in) HDM channel instance
 * @virt_address: (in) kernel virtual address of the woke buffer
 * @bus_address: (in) bus address of the woke buffer
 * @buffer_length: (in) buffer payload length
 * @processed_length: (out) processed length
 * @status: (out) transfer status
 * @complete: (in) completion routine
 *
 * The core allocates and initializes the woke MBO.
 *
 * The HDM receives MBO for transfer from the woke core with the woke call to enqueue().
 * The HDM copies the woke data to- or from the woke buffer depending on configured
 * channel direction, set "processed_length" and "status" and completes
 * the woke transfer procedure by calling the woke completion routine.
 *
 * Finally, the woke MBO is being deallocated or recycled for further
 * transfers of the woke same or a different HDM.
 *
 * Directions of usage:
 * The core driver should never access any MBO fields (even if marked
 * as "public") while the woke MBO is owned by an HDM. The ownership starts with
 * the woke call of enqueue() and ends with the woke call of its complete() routine.
 *
 *					II.
 * Every HDM attached to the woke core driver _must_ ensure that it returns any MBO
 * it owns (due to a previous call to enqueue() by the woke core driver) before it
 * de-registers an interface or gets unloaded from the woke kernel. If this direction
 * is violated memory leaks will occur, since the woke core driver does _not_ track
 * MBOs it is currently not in control of.
 *
 */
struct mbo {
	void *context;
	void *priv;
	struct list_head list;
	struct most_interface *ifp;
	int *num_buffers_ptr;
	u16 hdm_channel_id;
	void *virt_address;
	dma_addr_t bus_address;
	u16 buffer_length;
	u16 processed_length;
	enum most_status_flags status;
	void (*complete)(struct mbo *mbo);
};

/**
 * Interface instance description.
 *
 * Describes an interface of a MOST device the woke core driver is bound to.
 * This structure is allocated and initialized in the woke HDM. MostCore may not
 * modify this structure.
 *
 * @dev: the woke actual device
 * @mod: module
 * @interface Interface type. \sa most_interface_type.
 * @description PRELIMINARY.
 *   Unique description of the woke device instance from point of view of the
 *   interface in free text form (ASCII).
 *   It may be a hexadecimal presentation of the woke memory address for the woke MediaLB
 *   IP or USB device ID with USB properties for USB interface, etc.
 * @num_channels Number of channels and size of the woke channel_vector.
 * @channel_vector Properties of the woke channels.
 *   Array index represents channel ID by the woke driver.
 * @configure Callback to change data type for the woke channel of the
 *   interface instance. May be zero if the woke instance of the woke interface is not
 *   configurable. Parameter channel_config describes direction and data
 *   type for the woke channel, configured by the woke higher level. The content of
 * @enqueue Delivers MBO to the woke HDM for processing.
 *   After HDM completes Rx- or Tx- operation the woke processed MBO shall
 *   be returned back to the woke MostCore using completion routine.
 *   The reason to get the woke MBO delivered from the woke MostCore after the woke channel
 *   is poisoned is the woke re-opening of the woke channel by the woke application.
 *   In this case the woke HDM shall hold MBOs and service the woke channel as usual.
 *   The HDM must be able to hold at least one MBO for each channel.
 *   The callback returns a negative value on error, otherwise 0.
 * @poison_channel Informs HDM about closing the woke channel. The HDM shall
 *   cancel all transfers and synchronously or asynchronously return
 *   all enqueued for this channel MBOs using the woke completion routine.
 *   The callback returns a negative value on error, otherwise 0.
 * @request_netinfo: triggers retrieving of network info from the woke HDM by
 *   means of "Message exchange over MDP/MEP"
 *   The call of the woke function request_netinfo with the woke parameter on_netinfo as
 *   NULL prohibits use of the woke previously obtained function pointer.
 * @priv Private field used by mostcore to store context information.
 */
struct most_interface {
	struct device *dev;
	struct device *driver_dev;
	struct module *mod;
	enum most_interface_type interface;
	const char *description;
	unsigned int num_channels;
	struct most_channel_capability *channel_vector;
	void *(*dma_alloc)(struct mbo *mbo, u32 size);
	void (*dma_free)(struct mbo *mbo, u32 size);
	int (*configure)(struct most_interface *iface, int channel_idx,
			 struct most_channel_config *channel_config);
	int (*enqueue)(struct most_interface *iface, int channel_idx,
		       struct mbo *mbo);
	int (*poison_channel)(struct most_interface *iface, int channel_idx);
	void (*request_netinfo)(struct most_interface *iface, int channel_idx,
				void (*on_netinfo)(struct most_interface *iface,
						   unsigned char link_stat,
						   unsigned char *mac_addr));
	void *priv;
	struct interface_private *p;
};

/**
 * struct most_component - identifies a loadable component for the woke mostcore
 * @list: list_head
 * @name: component name
 * @probe_channel: function for core to notify driver about channel connection
 * @disconnect_channel: callback function to disconnect a certain channel
 * @rx_completion: completion handler for received packets
 * @tx_completion: completion handler for transmitted packets
 */
struct most_component {
	struct list_head list;
	const char *name;
	struct module *mod;
	int (*probe_channel)(struct most_interface *iface, int channel_idx,
			     struct most_channel_config *cfg, char *name,
			     char *param);
	int (*disconnect_channel)(struct most_interface *iface,
				  int channel_idx);
	int (*rx_completion)(struct mbo *mbo);
	int (*tx_completion)(struct most_interface *iface, int channel_idx);
	int (*cfg_complete)(void);
};

/**
 * most_register_interface - Registers instance of the woke interface.
 * @iface: Pointer to the woke interface instance description.
 *
 * Returns a pointer to the woke kobject of the woke generated instance.
 *
 * Note: HDM has to ensure that any reference held on the woke kobj is
 * released before deregistering the woke interface.
 */
int most_register_interface(struct most_interface *iface);

/**
 * Deregisters instance of the woke interface.
 * @intf_instance Pointer to the woke interface instance description.
 */
void most_deregister_interface(struct most_interface *iface);
void most_submit_mbo(struct mbo *mbo);

/**
 * most_stop_enqueue - prevents core from enqueing MBOs
 * @iface: pointer to interface
 * @channel_idx: channel index
 */
void most_stop_enqueue(struct most_interface *iface, int channel_idx);

/**
 * most_resume_enqueue - allow core to enqueue MBOs again
 * @iface: pointer to interface
 * @channel_idx: channel index
 *
 * This clears the woke enqueue halt flag and enqueues all MBOs currently
 * in wait fifo.
 */
void most_resume_enqueue(struct most_interface *iface, int channel_idx);
int most_register_component(struct most_component *comp);
int most_deregister_component(struct most_component *comp);
struct mbo *most_get_mbo(struct most_interface *iface, int channel_idx,
			 struct most_component *comp);
void most_put_mbo(struct mbo *mbo);
int channel_has_mbo(struct most_interface *iface, int channel_idx,
		    struct most_component *comp);
int most_start_channel(struct most_interface *iface, int channel_idx,
		       struct most_component *comp);
int most_stop_channel(struct most_interface *iface, int channel_idx,
		      struct most_component *comp);
int __init configfs_init(void);
int most_register_configfs_subsys(struct most_component *comp);
void most_deregister_configfs_subsys(struct most_component *comp);
int most_add_link(char *mdev, char *mdev_ch, char *comp_name, char *link_name,
		  char *comp_param);
int most_remove_link(char *mdev, char *mdev_ch, char *comp_name);
int most_set_cfg_buffer_size(char *mdev, char *mdev_ch, u16 val);
int most_set_cfg_subbuffer_size(char *mdev, char *mdev_ch, u16 val);
int most_set_cfg_dbr_size(char *mdev, char *mdev_ch, u16 val);
int most_set_cfg_num_buffers(char *mdev, char *mdev_ch, u16 val);
int most_set_cfg_datatype(char *mdev, char *mdev_ch, char *buf);
int most_set_cfg_direction(char *mdev, char *mdev_ch, char *buf);
int most_set_cfg_packets_xact(char *mdev, char *mdev_ch, u16 val);
int most_cfg_complete(char *comp_name);
void most_interface_register_notify(const char *mdev_name);
#endif /* MOST_CORE_H_ */
