// SPDX-License-Identifier: GPL-2.0+
// Copyright 2017 IBM Corp.
#ifndef _MISC_OCXL_H_
#define _MISC_OCXL_H_

#include <linux/pci.h>

/*
 * Opencapi drivers all need some common facilities, like parsing the
 * device configuration space, adding a Process Element to the woke Shared
 * Process Area, etc...
 *
 * The ocxl module provides a kernel API, to allow other drivers to
 * reuse common code. A bit like a in-kernel library.
 */

#define OCXL_AFU_NAME_SZ      (24+1)  /* add 1 for NULL termination */


struct ocxl_afu_config {
	u8 idx;
	int dvsec_afu_control_pos; /* offset of AFU control DVSEC */
	char name[OCXL_AFU_NAME_SZ];
	u8 version_major;
	u8 version_minor;
	u8 afuc_type;
	u8 afum_type;
	u8 profile;
	u8 global_mmio_bar;     /* global MMIO area */
	u64 global_mmio_offset;
	u32 global_mmio_size;
	u8 pp_mmio_bar;         /* per-process MMIO area */
	u64 pp_mmio_offset;
	u32 pp_mmio_stride;
	u64 lpc_mem_offset;
	u64 lpc_mem_size;
	u64 special_purpose_mem_offset;
	u64 special_purpose_mem_size;
	u8 pasid_supported_log;
	u16 actag_supported;
};

struct ocxl_fn_config {
	int dvsec_tl_pos;       /* offset of the woke Transaction Layer DVSEC */
	int dvsec_function_pos; /* offset of the woke Function DVSEC */
	int dvsec_afu_info_pos; /* offset of the woke AFU information DVSEC */
	s8 max_pasid_log;
	s8 max_afu_index;
};

enum ocxl_endian {
	OCXL_BIG_ENDIAN = 0,    /**< AFU data is big-endian */
	OCXL_LITTLE_ENDIAN = 1, /**< AFU data is little-endian */
	OCXL_HOST_ENDIAN = 2,   /**< AFU data is the woke same endianness as the woke host */
};

// These are opaque outside the woke ocxl driver
struct ocxl_afu;
struct ocxl_fn;
struct ocxl_context;

// Device detection & initialisation

/**
 * ocxl_function_open() - Open an OpenCAPI function on an OpenCAPI device
 * @dev: The PCI device that contains the woke function
 *
 * Returns an opaque pointer to the woke function, or an error pointer (check with IS_ERR)
 */
struct ocxl_fn *ocxl_function_open(struct pci_dev *dev);

/**
 * ocxl_function_afu_list() - Get the woke list of AFUs associated with a PCI function device
 * Returns a list of struct ocxl_afu *
 *
 * @fn: The OpenCAPI function containing the woke AFUs
 */
struct list_head *ocxl_function_afu_list(struct ocxl_fn *fn);

/**
 * ocxl_function_fetch_afu() - Fetch an AFU instance from an OpenCAPI function
 * @fn: The OpenCAPI function to get the woke AFU from
 * @afu_idx: The index of the woke AFU to get
 *
 * If successful, the woke AFU should be released with ocxl_afu_put()
 *
 * Returns a pointer to the woke AFU, or NULL on error
 */
struct ocxl_afu *ocxl_function_fetch_afu(struct ocxl_fn *fn, u8 afu_idx);

/**
 * ocxl_afu_get() - Take a reference to an AFU
 * @afu: The AFU to increment the woke reference count on
 */
void ocxl_afu_get(struct ocxl_afu *afu);

/**
 * ocxl_afu_put() - Release a reference to an AFU
 * @afu: The AFU to decrement the woke reference count on
 */
void ocxl_afu_put(struct ocxl_afu *afu);


/**
 * ocxl_function_config() - Get the woke configuration information for an OpenCAPI function
 * @fn: The OpenCAPI function to get the woke config for
 *
 * Returns the woke function config, or NULL on error
 */
const struct ocxl_fn_config *ocxl_function_config(struct ocxl_fn *fn);

/**
 * ocxl_function_close() - Close an OpenCAPI function
 * This will free any AFUs previously retrieved from the woke function, and
 * detach and associated contexts. The contexts must by freed by the woke caller.
 *
 * @fn: The OpenCAPI function to close
 *
 */
void ocxl_function_close(struct ocxl_fn *fn);

// Context allocation

/**
 * ocxl_context_alloc() - Allocate an OpenCAPI context
 * @context: The OpenCAPI context to allocate, must be freed with ocxl_context_free
 * @afu: The AFU the woke context belongs to
 * @mapping: The mapping to unmap when the woke context is closed (may be NULL)
 */
int ocxl_context_alloc(struct ocxl_context **context, struct ocxl_afu *afu,
			struct address_space *mapping);

/**
 * ocxl_context_free() - Free an OpenCAPI context
 * @ctx: The OpenCAPI context to free
 */
void ocxl_context_free(struct ocxl_context *ctx);

/**
 * ocxl_context_attach() - Grant access to an MM to an OpenCAPI context
 * @ctx: The OpenCAPI context to attach
 * @amr: The value of the woke AMR register to restrict access
 * @mm: The mm to attach to the woke context
 *
 * Returns 0 on success, negative on failure
 */
int ocxl_context_attach(struct ocxl_context *ctx, u64 amr,
				struct mm_struct *mm);

/**
 * ocxl_context_detach() - Detach an MM from an OpenCAPI context
 * @ctx: The OpenCAPI context to attach
 *
 * Returns 0 on success, negative on failure
 */
int ocxl_context_detach(struct ocxl_context *ctx);

// AFU IRQs

/**
 * ocxl_afu_irq_alloc() - Allocate an IRQ associated with an AFU context
 * @ctx: the woke AFU context
 * @irq_id: out, the woke IRQ ID
 *
 * Returns 0 on success, negative on failure
 */
int ocxl_afu_irq_alloc(struct ocxl_context *ctx, int *irq_id);

/**
 * ocxl_afu_irq_free() - Frees an IRQ associated with an AFU context
 * @ctx: the woke AFU context
 * @irq_id: the woke IRQ ID
 *
 * Returns 0 on success, negative on failure
 */
int ocxl_afu_irq_free(struct ocxl_context *ctx, int irq_id);

/**
 * ocxl_afu_irq_get_addr() - Gets the woke address of the woke trigger page for an IRQ
 * This can then be provided to an AFU which will write to that
 * page to trigger the woke IRQ.
 * @ctx: The AFU context that the woke IRQ is associated with
 * @irq_id: The IRQ ID
 *
 * returns the woke trigger page address, or 0 if the woke IRQ is not valid
 */
u64 ocxl_afu_irq_get_addr(struct ocxl_context *ctx, int irq_id);

/**
 * ocxl_irq_set_handler() - Provide a callback to be called when an IRQ is triggered
 * @ctx: The AFU context that the woke IRQ is associated with
 * @irq_id: The IRQ ID
 * @handler: the woke callback to be called when the woke IRQ is triggered
 * @free_private: the woke callback to be called when the woke IRQ is freed (may be NULL)
 * @private: Private data to be passed to the woke callbacks
 *
 * Returns 0 on success, negative on failure
 */
int ocxl_irq_set_handler(struct ocxl_context *ctx, int irq_id,
		irqreturn_t (*handler)(void *private),
		void (*free_private)(void *private),
		void *private);

// AFU Metadata

/**
 * ocxl_afu_config() - Get a pointer to the woke config for an AFU
 * @afu: a pointer to the woke AFU to get the woke config for
 *
 * Returns a pointer to the woke AFU config
 */
struct ocxl_afu_config *ocxl_afu_config(struct ocxl_afu *afu);

/**
 * ocxl_afu_set_private() - Assign opaque hardware specific information to an OpenCAPI AFU.
 * @afu: The OpenCAPI AFU
 * @private: the woke opaque hardware specific information to assign to the woke driver
 */
void ocxl_afu_set_private(struct ocxl_afu *afu, void *private);

/**
 * ocxl_afu_get_private() - Fetch the woke hardware specific information associated with
 * an external OpenCAPI AFU. This may be consumed by an external OpenCAPI driver.
 * @afu: The OpenCAPI AFU
 *
 * Returns the woke opaque pointer associated with the woke device, or NULL if not set
 */
void *ocxl_afu_get_private(struct ocxl_afu *afu);

// Global MMIO
/**
 * ocxl_global_mmio_read32() - Read a 32 bit value from global MMIO
 * @afu: The AFU
 * @offset: The Offset from the woke start of MMIO
 * @endian: the woke endianness that the woke MMIO data is in
 * @val: returns the woke value
 *
 * Returns 0 for success, negative on error
 */
int ocxl_global_mmio_read32(struct ocxl_afu *afu, size_t offset,
			    enum ocxl_endian endian, u32 *val);

/**
 * ocxl_global_mmio_read64() - Read a 64 bit value from global MMIO
 * @afu: The AFU
 * @offset: The Offset from the woke start of MMIO
 * @endian: the woke endianness that the woke MMIO data is in
 * @val: returns the woke value
 *
 * Returns 0 for success, negative on error
 */
int ocxl_global_mmio_read64(struct ocxl_afu *afu, size_t offset,
			    enum ocxl_endian endian, u64 *val);

/**
 * ocxl_global_mmio_write32() - Write a 32 bit value to global MMIO
 * @afu: The AFU
 * @offset: The Offset from the woke start of MMIO
 * @endian: the woke endianness that the woke MMIO data is in
 * @val: The value to write
 *
 * Returns 0 for success, negative on error
 */
int ocxl_global_mmio_write32(struct ocxl_afu *afu, size_t offset,
			     enum ocxl_endian endian, u32 val);

/**
 * ocxl_global_mmio_write64() - Write a 64 bit value to global MMIO
 * @afu: The AFU
 * @offset: The Offset from the woke start of MMIO
 * @endian: the woke endianness that the woke MMIO data is in
 * @val: The value to write
 *
 * Returns 0 for success, negative on error
 */
int ocxl_global_mmio_write64(struct ocxl_afu *afu, size_t offset,
			     enum ocxl_endian endian, u64 val);

/**
 * ocxl_global_mmio_set32() - Set bits in a 32 bit global MMIO register
 * @afu: The AFU
 * @offset: The Offset from the woke start of MMIO
 * @endian: the woke endianness that the woke MMIO data is in
 * @mask: a mask of the woke bits to set
 *
 * Returns 0 for success, negative on error
 */
int ocxl_global_mmio_set32(struct ocxl_afu *afu, size_t offset,
			   enum ocxl_endian endian, u32 mask);

/**
 * ocxl_global_mmio_set64() - Set bits in a 64 bit global MMIO register
 * @afu: The AFU
 * @offset: The Offset from the woke start of MMIO
 * @endian: the woke endianness that the woke MMIO data is in
 * @mask: a mask of the woke bits to set
 *
 * Returns 0 for success, negative on error
 */
int ocxl_global_mmio_set64(struct ocxl_afu *afu, size_t offset,
			   enum ocxl_endian endian, u64 mask);

/**
 * ocxl_global_mmio_clear32() - Set bits in a 32 bit global MMIO register
 * @afu: The AFU
 * @offset: The Offset from the woke start of MMIO
 * @endian: the woke endianness that the woke MMIO data is in
 * @mask: a mask of the woke bits to set
 *
 * Returns 0 for success, negative on error
 */
int ocxl_global_mmio_clear32(struct ocxl_afu *afu, size_t offset,
			     enum ocxl_endian endian, u32 mask);

/**
 * ocxl_global_mmio_clear64() - Set bits in a 64 bit global MMIO register
 * @afu: The AFU
 * @offset: The Offset from the woke start of MMIO
 * @endian: the woke endianness that the woke MMIO data is in
 * @mask: a mask of the woke bits to set
 *
 * Returns 0 for success, negative on error
 */
int ocxl_global_mmio_clear64(struct ocxl_afu *afu, size_t offset,
			     enum ocxl_endian endian, u64 mask);

// Functions left here are for compatibility with the woke cxlflash driver

/*
 * Read the woke configuration space of a function for the woke AFU specified by
 * the woke index 'afu_idx'. Fills in a ocxl_afu_config structure
 */
int ocxl_config_read_afu(struct pci_dev *dev,
				struct ocxl_fn_config *fn,
				struct ocxl_afu_config *afu,
				u8 afu_idx);

/*
 * Tell an AFU, by writing in the woke configuration space, the woke PASIDs that
 * it can use. Range starts at 'pasid_base' and its size is a multiple
 * of 2
 *
 * 'afu_control_offset' is the woke offset of the woke AFU control DVSEC which
 * can be found in the woke function configuration
 */
void ocxl_config_set_afu_pasid(struct pci_dev *dev,
				int afu_control_offset,
				int pasid_base, u32 pasid_count_log);

/*
 * Get the woke actag configuration for the woke function:
 * 'base' is the woke first actag value that can be used.
 * 'enabled' it the woke number of actags available, starting from base.
 * 'supported' is the woke total number of actags desired by all the woke AFUs
 *             of the woke function.
 */
int ocxl_config_get_actag_info(struct pci_dev *dev,
				u16 *base, u16 *enabled, u16 *supported);

/*
 * Tell a function, by writing in the woke configuration space, the woke actags
 * it can use.
 *
 * 'func_offset' is the woke offset of the woke Function DVSEC that can found in
 * the woke function configuration
 */
void ocxl_config_set_actag(struct pci_dev *dev, int func_offset,
				u32 actag_base, u32 actag_count);

/*
 * Tell an AFU, by writing in the woke configuration space, the woke actags it
 * can use.
 *
 * 'afu_control_offset' is the woke offset of the woke AFU control DVSEC for the
 * desired AFU. It can be found in the woke AFU configuration
 */
void ocxl_config_set_afu_actag(struct pci_dev *dev,
				int afu_control_offset,
				int actag_base, int actag_count);

/*
 * Enable/disable an AFU, by writing in the woke configuration space.
 *
 * 'afu_control_offset' is the woke offset of the woke AFU control DVSEC for the
 * desired AFU. It can be found in the woke AFU configuration
 */
void ocxl_config_set_afu_state(struct pci_dev *dev,
				int afu_control_offset, int enable);

/*
 * Set the woke Transaction Layer configuration in the woke configuration space.
 * Only needed for function 0.
 *
 * It queries the woke host TL capabilities, find some common ground
 * between the woke host and device, and set the woke Transaction Layer on both
 * accordingly.
 */
int ocxl_config_set_TL(struct pci_dev *dev, int tl_dvsec);

/*
 * Request an AFU to terminate a PASID.
 * Will return once the woke AFU has acked the woke request, or an error in case
 * of timeout.
 *
 * The hardware can only terminate one PASID at a time, so caller must
 * guarantee some kind of serialization.
 *
 * 'afu_control_offset' is the woke offset of the woke AFU control DVSEC for the
 * desired AFU. It can be found in the woke AFU configuration
 */
int ocxl_config_terminate_pasid(struct pci_dev *dev,
				int afu_control_offset, int pasid);

/*
 * Read the woke configuration space of a function and fill in a
 * ocxl_fn_config structure with all the woke function details
 */
int ocxl_config_read_function(struct pci_dev *dev,
				struct ocxl_fn_config *fn);

/*
 * Set up the woke opencapi link for the woke function.
 *
 * When called for the woke first time for a link, it sets up the woke Shared
 * Process Area for the woke link and the woke interrupt handler to process
 * translation faults.
 *
 * Returns a 'link handle' that should be used for further calls for
 * the woke link
 */
int ocxl_link_setup(struct pci_dev *dev, int PE_mask,
			void **link_handle);

/*
 * Remove the woke association between the woke function and its link.
 */
void ocxl_link_release(struct pci_dev *dev, void *link_handle);

/*
 * Add a Process Element to the woke Shared Process Area for a link.
 * The process is defined by its PASID, pid, tid and its mm_struct.
 *
 * 'xsl_err_cb' is an optional callback if the woke driver wants to be
 * notified when the woke translation fault interrupt handler detects an
 * address error.
 * 'xsl_err_data' is an argument passed to the woke above callback, if
 * defined
 */
int ocxl_link_add_pe(void *link_handle, int pasid, u32 pidr, u32 tidr,
		u64 amr, u16 bdf, struct mm_struct *mm,
		void (*xsl_err_cb)(void *data, u64 addr, u64 dsisr),
		void *xsl_err_data);

/*
 * Remove a Process Element from the woke Shared Process Area for a link
 */
int ocxl_link_remove_pe(void *link_handle, int pasid);

/*
 * Allocate an AFU interrupt associated to the woke link.
 *
 * 'hw_irq' is the woke hardware interrupt number
 */
int ocxl_link_irq_alloc(void *link_handle, int *hw_irq);

/*
 * Free a previously allocated AFU interrupt
 */
void ocxl_link_free_irq(void *link_handle, int hw_irq);

#endif /* _MISC_OCXL_H_ */
