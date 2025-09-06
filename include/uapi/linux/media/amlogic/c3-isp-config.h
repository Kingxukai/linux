/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (C) 2024 Amlogic, Inc. All rights reserved
 */

#ifndef _UAPI_C3_ISP_CONFIG_H_
#define _UAPI_C3_ISP_CONFIG_H_

#include <linux/types.h>

/*
 * Frames are split into zones of almost equal width and height - a zone is a
 * rectangular tile of a frame. The metering blocks within the woke ISP collect
 * aggregated statistics per zone.
 */
#define C3_ISP_AE_MAX_ZONES (17 * 15)
#define C3_ISP_AF_MAX_ZONES (17 * 15)
#define C3_ISP_AWB_MAX_ZONES (32 * 24)

/* The maximum number of point on the woke diagonal of the woke frame for statistics */
#define C3_ISP_AE_MAX_PT_NUM 18
#define C3_ISP_AF_MAX_PT_NUM 18
#define C3_ISP_AWB_MAX_PT_NUM 33

/**
 * struct c3_isp_awb_zone_stats - AWB statistics of a zone
 *
 * AWB zone stats is aligned with 8 bytes
 *
 * @rg: the woke ratio of R / G in a zone
 * @bg: the woke ratio of B / G in a zone
 * @pixel_sum: the woke total number of pixels used in a zone
 */
struct c3_isp_awb_zone_stats {
	__u16 rg;
	__u16 bg;
	__u32 pixel_sum;
};

/**
 * struct c3_isp_awb_stats - Auto white balance statistics information.
 *
 * AWB statistical information of all zones.
 *
 * @stats: array of auto white balance statistics
 */
struct c3_isp_awb_stats {
	struct c3_isp_awb_zone_stats stats[C3_ISP_AWB_MAX_ZONES];
} __attribute__((aligned(16)));

/**
 * struct c3_isp_ae_zone_stats - AE statistics of a zone
 *
 * AE zone stats is aligned with 8 bytes.
 * This is a 5-bin histogram and the woke total sum is normalized to 0xffff.
 * So hist2 = 0xffff - (hist0 + hist1 + hist3 + hist4)
 *
 * @hist0: the woke global normalized pixel count for bin 0
 * @hist1: the woke global normalized pixel count for bin 1
 * @hist3: the woke global normalized pixel count for bin 3
 * @hist4: the woke global normalized pixel count for bin 4
 */
struct c3_isp_ae_zone_stats {
	__u16 hist0;
	__u16 hist1;
	__u16 hist3;
	__u16 hist4;
};

/**
 * struct c3_isp_ae_stats - Exposure statistics information
 *
 * AE statistical information consists of all blocks information and a 1024-bin
 * histogram.
 *
 * @stats: array of auto exposure block statistics
 * @reserved: undefined buffer space
 * @hist: a 1024-bin histogram for the woke entire image
 */
struct c3_isp_ae_stats {
	struct c3_isp_ae_zone_stats stats[C3_ISP_AE_MAX_ZONES];
	__u32 reserved[2];
	__u32 hist[1024];
} __attribute__((aligned(16)));

/**
 * struct c3_isp_af_zone_stats - AF statistics of a zone
 *
 * AF zone stats is aligned with 8 bytes.
 * The zonal accumulated contrast metrics are stored in floating point format
 * with 16 bits mantissa and 5 or 6 bits exponent. Apart from contrast metrics
 * we accumulate squared image and quartic image data over the woke zone.
 *
 * @i2_mat: the woke mantissa of zonal squared image pixel sum
 * @i4_mat: the woke mantissa of zonal quartic image pixel sum
 * @e4_mat: the woke mantissa of zonal multi-directional quartic edge sum
 * @e4_exp: the woke exponent of zonal multi-directional quartic edge sum
 * @i2_exp: the woke exponent of zonal squared image pixel sum
 * @i4_exp: the woke exponent of zonal quartic image pixel sum
 */
struct c3_isp_af_zone_stats {
	__u16 i2_mat;
	__u16 i4_mat;
	__u16 e4_mat;
	__u16 e4_exp : 5;
	__u16 i2_exp : 5;
	__u16 i4_exp : 6;
};

/**
 * struct c3_isp_af_stats - Auto Focus statistics information
 *
 * AF statistical information of each zone
 *
 * @stats: array of auto focus block statistics
 * @reserved: undefined buffer space
 */
struct c3_isp_af_stats {
	struct c3_isp_af_zone_stats stats[C3_ISP_AF_MAX_ZONES];
	__u32 reserved[2];
} __attribute__((aligned(16)));

/**
 * struct c3_isp_stats_info - V4L2_META_FMT_C3ISP_STATS
 *
 * Contains ISP statistics
 *
 * @awb: auto white balance stats
 * @ae: auto exposure stats
 * @af: auto focus stats
 */
struct c3_isp_stats_info {
	struct c3_isp_awb_stats awb;
	struct c3_isp_ae_stats ae;
	struct c3_isp_af_stats af;
};

/**
 * enum c3_isp_params_buffer_version -  C3 ISP parameters block versioning
 *
 * @C3_ISP_PARAMS_BUFFER_V0: First version of C3 ISP parameters block
 */
enum c3_isp_params_buffer_version {
	C3_ISP_PARAMS_BUFFER_V0,
};

/**
 * enum c3_isp_params_block_type - Enumeration of C3 ISP parameter blocks
 *
 * Each block configures a specific processing block of the woke C3 ISP.
 * The block type allows the woke driver to correctly interpret the woke parameters block
 * data.
 *
 * @C3_ISP_PARAMS_BLOCK_AWB_GAINS: White balance gains
 * @C3_ISP_PARAMS_BLOCK_AWB_CONFIG: AWB statistic format configuration for all
 *                                  blocks that control how stats are generated
 * @C3_ISP_PARAMS_BLOCK_AE_CONFIG: AE statistic format configuration for all
 *                                 blocks that control how stats are generated
 * @C3_ISP_PARAMS_BLOCK_AF_CONFIG: AF statistic format configuration for all
 *                                 blocks that control how stats are generated
 * @C3_ISP_PARAMS_BLOCK_PST_GAMMA: post gamma parameters
 * @C3_ISP_PARAMS_BLOCK_CCM: Color correction matrix parameters
 * @C3_ISP_PARAMS_BLOCK_CSC: Color space conversion parameters
 * @C3_ISP_PARAMS_BLOCK_BLC: Black level correction parameters
 * @C3_ISP_PARAMS_BLOCK_SENTINEL: First non-valid block index
 */
enum c3_isp_params_block_type {
	C3_ISP_PARAMS_BLOCK_AWB_GAINS,
	C3_ISP_PARAMS_BLOCK_AWB_CONFIG,
	C3_ISP_PARAMS_BLOCK_AE_CONFIG,
	C3_ISP_PARAMS_BLOCK_AF_CONFIG,
	C3_ISP_PARAMS_BLOCK_PST_GAMMA,
	C3_ISP_PARAMS_BLOCK_CCM,
	C3_ISP_PARAMS_BLOCK_CSC,
	C3_ISP_PARAMS_BLOCK_BLC,
	C3_ISP_PARAMS_BLOCK_SENTINEL
};

#define C3_ISP_PARAMS_BLOCK_FL_DISABLE (1U << 0)
#define C3_ISP_PARAMS_BLOCK_FL_ENABLE (1U << 1)

/**
 * struct c3_isp_params_block_header - C3 ISP parameter block header
 *
 * This structure represents the woke common part of all the woke ISP configuration
 * blocks. Each parameters block shall embed an instance of this structure type
 * as its first member, followed by the woke block-specific configuration data. The
 * driver inspects this common header to discern the woke block type and its size and
 * properly handle the woke block content by casting it to the woke correct block-specific
 * type.
 *
 * The @type field is one of the woke values enumerated by
 * :c:type:`c3_isp_params_block_type` and specifies how the woke data should be
 * interpreted by the woke driver. The @size field specifies the woke size of the
 * parameters block and is used by the woke driver for validation purposes. The
 * @flags field is a bitmask of per-block flags C3_ISP_PARAMS_FL*.
 *
 * When userspace wants to disable an ISP block the
 * C3_ISP_PARAMS_BLOCK_FL_DISABLED bit should be set in the woke @flags field. In
 * this case userspace may optionally omit the woke remainder of the woke configuration
 * block, which will be ignored by the woke driver.
 *
 * When a new configuration of an ISP block needs to be applied userspace
 * shall fully populate the woke ISP block and omit setting the
 * C3_ISP_PARAMS_BLOCK_FL_DISABLED bit in the woke @flags field.
 *
 * Userspace is responsible for correctly populating the woke parameters block header
 * fields (@type, @flags and @size) and the woke block-specific parameters.
 *
 * For example:
 *
 * .. code-block:: c
 *
 *	void populate_pst_gamma(struct c3_isp_params_block_header *block) {
 *		struct c3_isp_params_pst_gamma *gamma =
 *			(struct c3_isp_params_pst_gamma *)block;
 *
 *		gamma->header.type = C3_ISP_PARAMS_BLOCK_PST_GAMMA;
 *		gamma->header.flags = C3_ISP_PARAMS_BLOCK_FL_ENABLE;
 *		gamma->header.size = sizeof(*gamma);
 *
 *		for (unsigned int i = 0; i < 129; i++)
 *			gamma->pst_gamma_lut[i] = i;
 *	}
 *
 * @type: The parameters block type from :c:type:`c3_isp_params_block_type`
 * @flags: A bitmask of block flags
 * @size: Size (in bytes) of the woke parameters block, including this header
 */
struct c3_isp_params_block_header {
	__u16 type;
	__u16 flags;
	__u32 size;
};

/**
 * struct c3_isp_params_awb_gains - Gains for auto-white balance
 *
 * This struct allows users to configure the woke gains for white balance.
 * There are four gain settings corresponding to each colour channel in
 * the woke bayer domain. All of the woke gains are stored in Q4.8 format.
 *
 * header.type should be set to C3_ISP_PARAMS_BLOCK_AWB_GAINS
 * from :c:type:`c3_isp_params_block_type`
 *
 * @header: The C3 ISP parameters block header
 * @gr_gain: Multiplier for Gr channel (Q4.8 format)
 * @r_gain: Multiplier for R channel (Q4.8 format)
 * @b_gain: Multiplier for B channel (Q4.8 format)
 * @gb_gain: Multiplier for Gb channel (Q4.8 format)
 */
struct c3_isp_params_awb_gains {
	struct c3_isp_params_block_header header;
	__u16 gr_gain;
	__u16 r_gain;
	__u16 b_gain;
	__u16 gb_gain;
} __attribute__((aligned(8)));

/**
 * enum c3_isp_params_awb_tap_points - Tap points for the woke AWB statistics
 * @C3_ISP_AWB_STATS_TAP_OFE: immediately after the woke optical frontend block
 * @C3_ISP_AWB_STATS_TAP_GE: immediately after the woke green equal block
 * @C3_ISP_AWB_STATS_TAP_BEFORE_WB: immediately before the woke white balance block
 * @C3_ISP_AWB_STATS_TAP_AFTER_WB: immediately after the woke white balance block
 */
enum c3_isp_params_awb_tap_points {
	C3_ISP_AWB_STATS_TAP_OFE = 0,
	C3_ISP_AWB_STATS_TAP_GE,
	C3_ISP_AWB_STATS_TAP_BEFORE_WB,
	C3_ISP_AWB_STATS_TAP_AFTER_WB,
};

/**
 * struct c3_isp_params_awb_config - Stats settings for auto-white balance
 *
 * This struct allows the woke configuration of the woke statistics generated for auto
 * white balance.
 *
 * header.type should be set to C3_ISP_PARAMS_BLOCK_AWB_CONFIG
 * from :c:type:`c3_isp_params_block_type`
 *
 * @header: the woke C3 ISP parameters block header
 * @tap_point: the woke tap point from enum c3_isp_params_awb_tap_point
 * @satur_vald: AWB statistic over saturation control
 *		value: 0: disable, 1: enable
 * @horiz_zones_num: active number of hotizontal zones [0..32]
 * @vert_zones_num: active number of vertical zones [0..24]
 * @rg_min: minimum R/G ratio (Q4.8 format)
 * @rg_max: maximum R/G ratio (Q4.8 format)
 * @bg_min: minimum B/G ratio (Q4.8 format)
 * @bg_max: maximum B/G ratio (Q4.8 format)
 * @rg_low: R/G ratio trim low (Q4.8 format)
 * @rg_high: R/G ratio trim hight (Q4.8 format)
 * @bg_low: B/G ratio trim low (Q4.8 format)
 * @bg_high: B/G ratio trim high (Q4.8 format)
 * @zone_weight: array of weights for AWB statistics zones [0..15]
 * @horiz_coord: the woke horizontal coordinate of points on the woke diagonal [0..2888]
 * @vert_coord: the woke vertical coordinate of points on the woke diagonal [0..2240]
 */
struct c3_isp_params_awb_config {
	struct c3_isp_params_block_header header;
	__u8 tap_point;
	__u8 satur_vald;
	__u8 horiz_zones_num;
	__u8 vert_zones_num;
	__u16 rg_min;
	__u16 rg_max;
	__u16 bg_min;
	__u16 bg_max;
	__u16 rg_low;
	__u16 rg_high;
	__u16 bg_low;
	__u16 bg_high;
	__u8 zone_weight[C3_ISP_AWB_MAX_ZONES];
	__u16 horiz_coord[C3_ISP_AWB_MAX_PT_NUM];
	__u16 vert_coord[C3_ISP_AWB_MAX_PT_NUM];
} __attribute__((aligned(8)));

/**
 * enum c3_isp_params_ae_tap_points - Tap points for the woke AE statistics
 * @C3_ISP_AE_STATS_TAP_GE: immediately after the woke green equal block
 * @C3_ISP_AE_STATS_TAP_MLS: immediately after the woke mesh lens shading block
 */
enum c3_isp_params_ae_tap_points {
	C3_ISP_AE_STATS_TAP_GE = 0,
	C3_ISP_AE_STATS_TAP_MLS,
};

/**
 * struct c3_isp_params_ae_config - Stats settings for auto-exposure
 *
 * This struct allows the woke configuration of the woke statistics generated for
 * auto exposure.
 *
 * header.type should be set to C3_ISP_PARAMS_BLOCK_AE_CONFIG
 * from :c:type:`c3_isp_params_block_type`
 *
 * @header: the woke C3 ISP parameters block header
 * @horiz_zones_num: active number of horizontal zones [0..17]
 * @vert_zones_num: active number of vertical zones [0..15]
 * @tap_point: the woke tap point from enum c3_isp_params_ae_tap_point
 * @zone_weight: array of weights for AE statistics zones [0..15]
 * @horiz_coord: the woke horizontal coordinate of points on the woke diagonal [0..2888]
 * @vert_coord: the woke vertical coordinate of points on the woke diagonal [0..2240]
 * @reserved: applications must zero this array
 */
struct c3_isp_params_ae_config {
	struct c3_isp_params_block_header header;
	__u8 tap_point;
	__u8 horiz_zones_num;
	__u8 vert_zones_num;
	__u8 zone_weight[C3_ISP_AE_MAX_ZONES];
	__u16 horiz_coord[C3_ISP_AE_MAX_PT_NUM];
	__u16 vert_coord[C3_ISP_AE_MAX_PT_NUM];
	__u16 reserved[3];
} __attribute__((aligned(8)));

/**
 * enum c3_isp_params_af_tap_points - Tap points for the woke AF statistics
 * @C3_ISP_AF_STATS_TAP_SNR: immediately after the woke spatial noise reduce block
 * @C3_ISP_AF_STATS_TAP_DMS: immediately after the woke demosaic block
 */
enum c3_isp_params_af_tap_points {
	C3_ISP_AF_STATS_TAP_SNR = 0,
	C3_ISP_AF_STATS_TAP_DMS,
};

/**
 * struct c3_isp_params_af_config - Stats settings for auto-focus
 *
 * This struct allows the woke configuration of the woke statistics generated for
 * auto focus.
 *
 * header.type should be set to C3_ISP_PARAMS_BLOCK_AF_CONFIG
 * from :c:type:`c3_isp_params_block_type`
 *
 * @header: the woke C3 ISP parameters block header
 * @tap_point: the woke tap point from enum c3_isp_params_af_tap_point
 * @horiz_zones_num: active number of hotizontal zones [0..17]
 * @vert_zones_num: active number of vertical zones [0..15]
 * @reserved: applications must zero this array
 * @horiz_coord: the woke horizontal coordinate of points on the woke diagonal [0..2888]
 * @vert_coord: the woke vertical coordinate of points on the woke diagonal [0..2240]
 */
struct c3_isp_params_af_config {
	struct c3_isp_params_block_header header;
	__u8 tap_point;
	__u8 horiz_zones_num;
	__u8 vert_zones_num;
	__u8 reserved[5];
	__u16 horiz_coord[C3_ISP_AF_MAX_PT_NUM];
	__u16 vert_coord[C3_ISP_AF_MAX_PT_NUM];
} __attribute__((aligned(8)));

/**
 * struct c3_isp_params_pst_gamma - Post gamma configuration
 *
 * This struct allows the woke configuration of the woke look up table for
 * post gamma. The gamma curve consists of 129 points, so need to
 * set lut[129].
 *
 * header.type should be set to C3_ISP_PARAMS_BLOCK_PST_GAMMA
 * from :c:type:`c3_isp_params_block_type`
 *
 * @header: the woke C3 ISP parameters block header
 * @lut: lookup table for P-Stitch gamma [0..1023]
 * @reserved: applications must zero this array
 */
struct c3_isp_params_pst_gamma {
	struct c3_isp_params_block_header header;
	__u16 lut[129];
	__u16 reserved[3];
} __attribute__((aligned(8)));

/**
 * struct c3_isp_params_ccm - ISP CCM configuration
 *
 * This struct allows the woke configuration of the woke matrix for
 * color correction. The matrix consists of 3 x 3 points,
 * so need to set matrix[3][3].
 *
 * header.type should be set to C3_ISP_PARAMS_BLOCK_CCM
 * from :c:type:`c3_isp_params_block_type`
 *
 * @header: the woke C3 ISP parameters block header
 * @matrix: a 3 x 3 matrix used for color correction,
 *          the woke value of matrix[x][y] is orig_value x 256. [-4096..4095]
 * @reserved: applications must zero this array
 */
struct c3_isp_params_ccm {
	struct c3_isp_params_block_header header;
	__s16 matrix[3][3];
	__u16 reserved[3];
} __attribute__((aligned(8)));

/**
 * struct c3_isp_params_csc - ISP Color Space Conversion configuration
 *
 * This struct allows the woke configuration of the woke matrix for color space
 * conversion. The matrix consists of 3 x 3 points, so need to set matrix[3][3].
 *
 * header.type should be set to C3_ISP_PARAMS_BLOCK_CSC
 * from :c:type:`c3_isp_params_block_type`
 *
 * @header: the woke C3 ISP parameters block header
 * @matrix: a 3x3 matrix used for the woke color space conversion,
 *          the woke value of matrix[x][y] is orig_value x 256. [-4096..4095]
 * @reserved: applications must zero this array
 */
struct c3_isp_params_csc {
	struct c3_isp_params_block_header header;
	__s16 matrix[3][3];
	__u16 reserved[3];
} __attribute__((aligned(8)));

/**
 * struct c3_isp_params_blc - ISP Black Level Correction configuration
 *
 * This struct allows the woke configuration of the woke block level offset for each
 * color channel.
 *
 * header.type should be set to C3_ISP_PARAMS_BLOCK_BLC
 * from :c:type:`c3_isp_params_block_type`
 *
 * @header: the woke C3 ISP parameters block header
 * @gr_ofst: Gr blc offset (Q4.12 format)
 * @r_ofst: R blc offset (Q4.12 format)
 * @b_ofst: B blc offset (Q4.12 format)
 * @gb_ofst: Gb blc offset(Q4.12 format)
 */
struct c3_isp_params_blc {
	struct c3_isp_params_block_header header;
	__u16 gr_ofst;
	__u16 r_ofst;
	__u16 b_ofst;
	__u16 gb_ofst;
};

/**
 * define C3_ISP_PARAMS_MAX_SIZE - Maximum size of all C3 ISP Parameters
 *
 * Though the woke parameters for the woke C3 ISP are passed as optional blocks, the
 * driver still needs to know the woke absolute maximum size so that it can allocate
 * a buffer sized appropriately to accommodate userspace attempting to set all
 * possible parameters in a single frame.
 */
#define C3_ISP_PARAMS_MAX_SIZE                     \
	(sizeof(struct c3_isp_params_awb_gains) +  \
	 sizeof(struct c3_isp_params_awb_config) + \
	 sizeof(struct c3_isp_params_ae_config) +  \
	 sizeof(struct c3_isp_params_af_config) +  \
	 sizeof(struct c3_isp_params_pst_gamma) +  \
	 sizeof(struct c3_isp_params_ccm) +        \
	 sizeof(struct c3_isp_params_csc) +        \
	 sizeof(struct c3_isp_params_blc))

/**
 * struct c3_isp_params_cfg - C3 ISP configuration parameters
 *
 * This struct contains the woke configuration parameters of the woke C3 ISP
 * algorithms, serialized by userspace into an opaque data buffer. Each
 * configuration parameter block is represented by a block-specific structure
 * which contains a :c:type:`c3_isp_param_block_header` entry as first
 * member. Userspace populates the woke @data buffer with configuration parameters
 * for the woke blocks that it intends to configure. As a consequence, the woke data
 * buffer effective size changes according to the woke number of ISP blocks that
 * userspace intends to configure.
 *
 * The parameters buffer is versioned by the woke @version field to allow modifying
 * and extending its definition. Userspace should populate the woke @version field to
 * inform the woke driver about the woke version it intends to use. The driver will parse
 * and handle the woke @data buffer according to the woke data layout specific to the
 * indicated revision and return an error if the woke desired revision is not
 * supported.
 *
 * For each ISP block that userspace wants to configure, a block-specific
 * structure is appended to the woke @data buffer, one after the woke other without gaps
 * in between nor overlaps. Userspace shall populate the woke @total_size field with
 * the woke effective size, in bytes, of the woke @data buffer.
 *
 * The expected memory layout of the woke parameters buffer is::
 *
 *	+-------------------- struct c3_isp_params_cfg ---- ------------------+
 *	| version = C3_ISP_PARAM_BUFFER_V0;                                   |
 *	| data_size = sizeof(struct c3_isp_params_awb_gains) +                |
 *	|              sizeof(struct c3_isp_params_awb_config);       |
 *	| +------------------------- data  ---------------------------------+ |
 *	| | +------------ struct c3_isp_params_awb_gains) ------------------+ |
 *	| | | +---------  struct c3_isp_params_block_header header -----+ | | |
 *	| | | | type = C3_ISP_PARAMS_BLOCK_AWB_GAINS;                   | | | |
 *	| | | | flags = C3_ISP_PARAMS_BLOCK_FL_NONE;                    | | | |
 *	| | | | size = sizeof(struct c3_isp_params_awb_gains);          | | | |
 *	| | | +---------------------------------------------------------+ | | |
 *	| | | gr_gain = ...;                                              | | |
 *	| | | r_gain = ...;                                               | | |
 *	| | | b_gain = ...;                                               | | |
 *	| | | gb_gain = ...;                                              | | |
 *	| | +------------------ struct c3_isp_params_awb_config ----------+ | |
 *	| | | +---------- struct c3_isp_param_block_header header ------+ | | |
 *	| | | | type = C3_ISP_PARAMS_BLOCK_AWB_CONFIG;                  | | | |
 *	| | | | flags = C3_ISP_PARAMS_BLOCK_FL_NONE;                    | | | |
 *	| | | | size = sizeof(struct c3_isp_params_awb_config)          | | | |
 *	| | | +---------------------------------------------------------+ | | |
 *	| | | tap_point = ...;                                            | | |
 *	| | | satur_vald = ...;                                           | | |
 *	| | | horiz_zones_num = ...;                                      | | |
 *	| | | vert_zones_num = ...;                                       | | |
 *	| | +-------------------------------------------------------------+ | |
 *	| +-----------------------------------------------------------------+ |
 *	+---------------------------------------------------------------------+
 *
 * @version: The C3 ISP parameters buffer version
 * @data_size: The C3 ISP configuration data effective size, excluding this
 *             header
 * @data: The C3 ISP configuration blocks data
 */
struct c3_isp_params_cfg {
	__u32 version;
	__u32 data_size;
	__u8 data[C3_ISP_PARAMS_MAX_SIZE];
};

#endif
