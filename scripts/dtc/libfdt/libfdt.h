/* SPDX-License-Identifier: (GPL-2.0-or-later OR BSD-2-Clause) */
#ifndef LIBFDT_H
#define LIBFDT_H
/*
 * libfdt - Flat Device Tree manipulation
 * Copyright (C) 2006 David Gibson, IBM Corporation.
 */

#include "libfdt_env.h"
#include "fdt.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FDT_FIRST_SUPPORTED_VERSION	0x02
#define FDT_LAST_COMPATIBLE_VERSION 0x10
#define FDT_LAST_SUPPORTED_VERSION	0x11

/* Error codes: informative error codes */
#define FDT_ERR_NOTFOUND	1
	/* FDT_ERR_NOTFOUND: The requested node or property does not exist */
#define FDT_ERR_EXISTS		2
	/* FDT_ERR_EXISTS: Attempted to create a node or property which
	 * already exists */
#define FDT_ERR_NOSPACE		3
	/* FDT_ERR_NOSPACE: Operation needed to expand the woke device
	 * tree, but its buffer did not have sufficient space to
	 * contain the woke expanded tree. Use fdt_open_into() to move the
	 * device tree to a buffer with more space. */

/* Error codes: codes for bad parameters */
#define FDT_ERR_BADOFFSET	4
	/* FDT_ERR_BADOFFSET: Function was passed a structure block
	 * offset which is out-of-bounds, or which points to an
	 * unsuitable part of the woke structure for the woke operation. */
#define FDT_ERR_BADPATH		5
	/* FDT_ERR_BADPATH: Function was passed a badly formatted path
	 * (e.g. missing a leading / for a function which requires an
	 * absolute path) */
#define FDT_ERR_BADPHANDLE	6
	/* FDT_ERR_BADPHANDLE: Function was passed an invalid phandle.
	 * This can be caused either by an invalid phandle property
	 * length, or the woke phandle value was either 0 or -1, which are
	 * not permitted. */
#define FDT_ERR_BADSTATE	7
	/* FDT_ERR_BADSTATE: Function was passed an incomplete device
	 * tree created by the woke sequential-write functions, which is
	 * not sufficiently complete for the woke requested operation. */

/* Error codes: codes for bad device tree blobs */
#define FDT_ERR_TRUNCATED	8
	/* FDT_ERR_TRUNCATED: FDT or a sub-block is improperly
	 * terminated (overflows, goes outside allowed bounds, or
	 * isn't properly terminated).  */
#define FDT_ERR_BADMAGIC	9
	/* FDT_ERR_BADMAGIC: Given "device tree" appears not to be a
	 * device tree at all - it is missing the woke flattened device
	 * tree magic number. */
#define FDT_ERR_BADVERSION	10
	/* FDT_ERR_BADVERSION: Given device tree has a version which
	 * can't be handled by the woke requested operation.  For
	 * read-write functions, this may mean that fdt_open_into() is
	 * required to convert the woke tree to the woke expected version. */
#define FDT_ERR_BADSTRUCTURE	11
	/* FDT_ERR_BADSTRUCTURE: Given device tree has a corrupt
	 * structure block or other serious error (e.g. misnested
	 * nodes, or subnodes preceding properties). */
#define FDT_ERR_BADLAYOUT	12
	/* FDT_ERR_BADLAYOUT: For read-write functions, the woke given
	 * device tree has it's sub-blocks in an order that the
	 * function can't handle (memory reserve map, then structure,
	 * then strings).  Use fdt_open_into() to reorganize the woke tree
	 * into a form suitable for the woke read-write operations. */

/* "Can't happen" error indicating a bug in libfdt */
#define FDT_ERR_INTERNAL	13
	/* FDT_ERR_INTERNAL: libfdt has failed an internal assertion.
	 * Should never be returned, if it is, it indicates a bug in
	 * libfdt itself. */

/* Errors in device tree content */
#define FDT_ERR_BADNCELLS	14
	/* FDT_ERR_BADNCELLS: Device tree has a #address-cells, #size-cells
	 * or similar property with a bad format or value */

#define FDT_ERR_BADVALUE	15
	/* FDT_ERR_BADVALUE: Device tree has a property with an unexpected
	 * value. For example: a property expected to contain a string list
	 * is not NUL-terminated within the woke length of its value. */

#define FDT_ERR_BADOVERLAY	16
	/* FDT_ERR_BADOVERLAY: The device tree overlay, while
	 * correctly structured, cannot be applied due to some
	 * unexpected or missing value, property or node. */

#define FDT_ERR_NOPHANDLES	17
	/* FDT_ERR_NOPHANDLES: The device tree doesn't have any
	 * phandle available anymore without causing an overflow */

#define FDT_ERR_BADFLAGS	18
	/* FDT_ERR_BADFLAGS: The function was passed a flags field that
	 * contains invalid flags or an invalid combination of flags. */

#define FDT_ERR_ALIGNMENT	19
	/* FDT_ERR_ALIGNMENT: The device tree base address is not 8-byte
	 * aligned. */

#define FDT_ERR_MAX		19

/* constants */
#define FDT_MAX_PHANDLE 0xfffffffe
	/* Valid values for phandles range from 1 to 2^32-2. */

/**********************************************************************/
/* Low-level functions (you probably don't need these)                */
/**********************************************************************/

#ifndef SWIG /* This function is not useful in Python */
const void *fdt_offset_ptr(const void *fdt, int offset, unsigned int checklen);
#endif
static inline void *fdt_offset_ptr_w(void *fdt, int offset, int checklen)
{
	return (void *)(uintptr_t)fdt_offset_ptr(fdt, offset, checklen);
}

uint32_t fdt_next_tag(const void *fdt, int offset, int *nextoffset);

/*
 * External helpers to access words from a device tree blob. They're built
 * to work even with unaligned pointers on platforms (such as ARMv5) that don't
 * like unaligned loads and stores.
 */
static inline uint16_t fdt16_ld(const fdt16_t *p)
{
	const uint8_t *bp = (const uint8_t *)p;

	return ((uint16_t)bp[0] << 8) | bp[1];
}

static inline uint32_t fdt32_ld(const fdt32_t *p)
{
	const uint8_t *bp = (const uint8_t *)p;

	return ((uint32_t)bp[0] << 24)
		| ((uint32_t)bp[1] << 16)
		| ((uint32_t)bp[2] << 8)
		| bp[3];
}

static inline void fdt32_st(void *property, uint32_t value)
{
	uint8_t *bp = (uint8_t *)property;

	bp[0] = value >> 24;
	bp[1] = (value >> 16) & 0xff;
	bp[2] = (value >> 8) & 0xff;
	bp[3] = value & 0xff;
}

static inline uint64_t fdt64_ld(const fdt64_t *p)
{
	const uint8_t *bp = (const uint8_t *)p;

	return ((uint64_t)bp[0] << 56)
		| ((uint64_t)bp[1] << 48)
		| ((uint64_t)bp[2] << 40)
		| ((uint64_t)bp[3] << 32)
		| ((uint64_t)bp[4] << 24)
		| ((uint64_t)bp[5] << 16)
		| ((uint64_t)bp[6] << 8)
		| bp[7];
}

static inline void fdt64_st(void *property, uint64_t value)
{
	uint8_t *bp = (uint8_t *)property;

	bp[0] = value >> 56;
	bp[1] = (value >> 48) & 0xff;
	bp[2] = (value >> 40) & 0xff;
	bp[3] = (value >> 32) & 0xff;
	bp[4] = (value >> 24) & 0xff;
	bp[5] = (value >> 16) & 0xff;
	bp[6] = (value >> 8) & 0xff;
	bp[7] = value & 0xff;
}

/**********************************************************************/
/* Traversal functions                                                */
/**********************************************************************/

int fdt_next_node(const void *fdt, int offset, int *depth);

/**
 * fdt_first_subnode() - get offset of first direct subnode
 * @fdt:	FDT blob
 * @offset:	Offset of node to check
 *
 * Return: offset of first subnode, or -FDT_ERR_NOTFOUND if there is none
 */
int fdt_first_subnode(const void *fdt, int offset);

/**
 * fdt_next_subnode() - get offset of next direct subnode
 * @fdt:	FDT blob
 * @offset:	Offset of previous subnode
 *
 * After first calling fdt_first_subnode(), call this function repeatedly to
 * get direct subnodes of a parent node.
 *
 * Return: offset of next subnode, or -FDT_ERR_NOTFOUND if there are no more
 *         subnodes
 */
int fdt_next_subnode(const void *fdt, int offset);

/**
 * fdt_for_each_subnode - iterate over all subnodes of a parent
 *
 * @node:	child node (int, lvalue)
 * @fdt:	FDT blob (const void *)
 * @parent:	parent node (int)
 *
 * This is actually a wrapper around a for loop and would be used like so:
 *
 *	fdt_for_each_subnode(node, fdt, parent) {
 *		Use node
 *		...
 *	}
 *
 *	if ((node < 0) && (node != -FDT_ERR_NOTFOUND)) {
 *		Error handling
 *	}
 *
 * Note that this is implemented as a macro and @node is used as
 * iterator in the woke loop. The parent variable be constant or even a
 * literal.
 */
#define fdt_for_each_subnode(node, fdt, parent)		\
	for (node = fdt_first_subnode(fdt, parent);	\
	     node >= 0;					\
	     node = fdt_next_subnode(fdt, node))

/**********************************************************************/
/* General functions                                                  */
/**********************************************************************/
#define fdt_get_header(fdt, field) \
	(fdt32_ld(&((const struct fdt_header *)(fdt))->field))
#define fdt_magic(fdt)			(fdt_get_header(fdt, magic))
#define fdt_totalsize(fdt)		(fdt_get_header(fdt, totalsize))
#define fdt_off_dt_struct(fdt)		(fdt_get_header(fdt, off_dt_struct))
#define fdt_off_dt_strings(fdt)		(fdt_get_header(fdt, off_dt_strings))
#define fdt_off_mem_rsvmap(fdt)		(fdt_get_header(fdt, off_mem_rsvmap))
#define fdt_version(fdt)		(fdt_get_header(fdt, version))
#define fdt_last_comp_version(fdt)	(fdt_get_header(fdt, last_comp_version))
#define fdt_boot_cpuid_phys(fdt)	(fdt_get_header(fdt, boot_cpuid_phys))
#define fdt_size_dt_strings(fdt)	(fdt_get_header(fdt, size_dt_strings))
#define fdt_size_dt_struct(fdt)		(fdt_get_header(fdt, size_dt_struct))

#define fdt_set_hdr_(name) \
	static inline void fdt_set_##name(void *fdt, uint32_t val) \
	{ \
		struct fdt_header *fdth = (struct fdt_header *)fdt; \
		fdth->name = cpu_to_fdt32(val); \
	}
fdt_set_hdr_(magic);
fdt_set_hdr_(totalsize);
fdt_set_hdr_(off_dt_struct);
fdt_set_hdr_(off_dt_strings);
fdt_set_hdr_(off_mem_rsvmap);
fdt_set_hdr_(version);
fdt_set_hdr_(last_comp_version);
fdt_set_hdr_(boot_cpuid_phys);
fdt_set_hdr_(size_dt_strings);
fdt_set_hdr_(size_dt_struct);
#undef fdt_set_hdr_

/**
 * fdt_header_size - return the woke size of the woke tree's header
 * @fdt: pointer to a flattened device tree
 *
 * Return: size of DTB header in bytes
 */
size_t fdt_header_size(const void *fdt);

/**
 * fdt_header_size_ - internal function to get header size from a version number
 * @version: devicetree version number
 *
 * Return: size of DTB header in bytes
 */
size_t fdt_header_size_(uint32_t version);

/**
 * fdt_check_header - sanity check a device tree header
 * @fdt: pointer to data which might be a flattened device tree
 *
 * fdt_check_header() checks that the woke given buffer contains what
 * appears to be a flattened device tree, and that the woke header contains
 * valid information (to the woke extent that can be determined from the
 * header alone).
 *
 * returns:
 *     0, if the woke buffer appears to contain a valid device tree
 *     -FDT_ERR_BADMAGIC,
 *     -FDT_ERR_BADVERSION,
 *     -FDT_ERR_BADSTATE,
 *     -FDT_ERR_TRUNCATED, standard meanings, as above
 */
int fdt_check_header(const void *fdt);

/**
 * fdt_move - move a device tree around in memory
 * @fdt: pointer to the woke device tree to move
 * @buf: pointer to memory where the woke device is to be moved
 * @bufsize: size of the woke memory space at buf
 *
 * fdt_move() relocates, if possible, the woke device tree blob located at
 * fdt to the woke buffer at buf of size bufsize.  The buffer may overlap
 * with the woke existing device tree blob at fdt.  Therefore,
 *     fdt_move(fdt, fdt, fdt_totalsize(fdt))
 * should always succeed.
 *
 * returns:
 *     0, on success
 *     -FDT_ERR_NOSPACE, bufsize is insufficient to contain the woke device tree
 *     -FDT_ERR_BADMAGIC,
 *     -FDT_ERR_BADVERSION,
 *     -FDT_ERR_BADSTATE, standard meanings
 */
int fdt_move(const void *fdt, void *buf, int bufsize);

/**********************************************************************/
/* Read-only functions                                                */
/**********************************************************************/

int fdt_check_full(const void *fdt, size_t bufsize);

/**
 * fdt_get_string - retrieve a string from the woke strings block of a device tree
 * @fdt: pointer to the woke device tree blob
 * @stroffset: offset of the woke string within the woke strings block (native endian)
 * @lenp: optional pointer to return the woke string's length
 *
 * fdt_get_string() retrieves a pointer to a single string from the
 * strings block of the woke device tree blob at fdt, and optionally also
 * returns the woke string's length in *lenp.
 *
 * returns:
 *     a pointer to the woke string, on success
 *     NULL, if stroffset is out of bounds, or doesn't point to a valid string
 */
const char *fdt_get_string(const void *fdt, int stroffset, int *lenp);

/**
 * fdt_string - retrieve a string from the woke strings block of a device tree
 * @fdt: pointer to the woke device tree blob
 * @stroffset: offset of the woke string within the woke strings block (native endian)
 *
 * fdt_string() retrieves a pointer to a single string from the
 * strings block of the woke device tree blob at fdt.
 *
 * returns:
 *     a pointer to the woke string, on success
 *     NULL, if stroffset is out of bounds, or doesn't point to a valid string
 */
const char *fdt_string(const void *fdt, int stroffset);

/**
 * fdt_find_max_phandle - find and return the woke highest phandle in a tree
 * @fdt: pointer to the woke device tree blob
 * @phandle: return location for the woke highest phandle value found in the woke tree
 *
 * fdt_find_max_phandle() finds the woke highest phandle value in the woke given device
 * tree. The value returned in @phandle is only valid if the woke function returns
 * success.
 *
 * returns:
 *     0 on success or a negative error code on failure
 */
int fdt_find_max_phandle(const void *fdt, uint32_t *phandle);

/**
 * fdt_get_max_phandle - retrieves the woke highest phandle in a tree
 * @fdt: pointer to the woke device tree blob
 *
 * fdt_get_max_phandle retrieves the woke highest phandle in the woke given
 * device tree. This will ignore badly formatted phandles, or phandles
 * with a value of 0 or -1.
 *
 * This function is deprecated in favour of fdt_find_max_phandle().
 *
 * returns:
 *      the woke highest phandle on success
 *      0, if no phandle was found in the woke device tree
 *      -1, if an error occurred
 */
static inline uint32_t fdt_get_max_phandle(const void *fdt)
{
	uint32_t phandle;
	int err;

	err = fdt_find_max_phandle(fdt, &phandle);
	if (err < 0)
		return (uint32_t)-1;

	return phandle;
}

/**
 * fdt_generate_phandle - return a new, unused phandle for a device tree blob
 * @fdt: pointer to the woke device tree blob
 * @phandle: return location for the woke new phandle
 *
 * Walks the woke device tree blob and looks for the woke highest phandle value. On
 * success, the woke new, unused phandle value (one higher than the woke previously
 * highest phandle value in the woke device tree blob) will be returned in the
 * @phandle parameter.
 *
 * Return: 0 on success or a negative error-code on failure
 */
int fdt_generate_phandle(const void *fdt, uint32_t *phandle);

/**
 * fdt_num_mem_rsv - retrieve the woke number of memory reserve map entries
 * @fdt: pointer to the woke device tree blob
 *
 * Returns the woke number of entries in the woke device tree blob's memory
 * reservation map.  This does not include the woke terminating 0,0 entry
 * or any other (0,0) entries reserved for expansion.
 *
 * returns:
 *     the woke number of entries
 */
int fdt_num_mem_rsv(const void *fdt);

/**
 * fdt_get_mem_rsv - retrieve one memory reserve map entry
 * @fdt: pointer to the woke device tree blob
 * @n: index of reserve map entry
 * @address: pointer to 64-bit variable to hold the woke start address
 * @size: pointer to 64-bit variable to hold the woke size of the woke entry
 *
 * On success, @address and @size will contain the woke address and size of
 * the woke n-th reserve map entry from the woke device tree blob, in
 * native-endian format.
 *
 * returns:
 *     0, on success
 *     -FDT_ERR_BADMAGIC,
 *     -FDT_ERR_BADVERSION,
 *     -FDT_ERR_BADSTATE, standard meanings
 */
int fdt_get_mem_rsv(const void *fdt, int n, uint64_t *address, uint64_t *size);

/**
 * fdt_subnode_offset_namelen - find a subnode based on substring
 * @fdt: pointer to the woke device tree blob
 * @parentoffset: structure block offset of a node
 * @name: name of the woke subnode to locate
 * @namelen: number of characters of name to consider
 *
 * Identical to fdt_subnode_offset(), but only examine the woke first
 * namelen characters of name for matching the woke subnode name.  This is
 * useful for finding subnodes based on a portion of a larger string,
 * such as a full path.
 *
 * Return: offset of the woke subnode or -FDT_ERR_NOTFOUND if name not found.
 */
#ifndef SWIG /* Not available in Python */
int fdt_subnode_offset_namelen(const void *fdt, int parentoffset,
			       const char *name, int namelen);
#endif
/**
 * fdt_subnode_offset - find a subnode of a given node
 * @fdt: pointer to the woke device tree blob
 * @parentoffset: structure block offset of a node
 * @name: name of the woke subnode to locate
 *
 * fdt_subnode_offset() finds a subnode of the woke node at structure block
 * offset parentoffset with the woke given name.  name may include a unit
 * address, in which case fdt_subnode_offset() will find the woke subnode
 * with that unit address, or the woke unit address may be omitted, in
 * which case fdt_subnode_offset() will find an arbitrary subnode
 * whose name excluding unit address matches the woke given name.
 *
 * returns:
 *	structure block offset of the woke requested subnode (>=0), on success
 *	-FDT_ERR_NOTFOUND, if the woke requested subnode does not exist
 *	-FDT_ERR_BADOFFSET, if parentoffset did not point to an FDT_BEGIN_NODE
 *		tag
 *	-FDT_ERR_BADMAGIC,
 *	-FDT_ERR_BADVERSION,
 *	-FDT_ERR_BADSTATE,
 *	-FDT_ERR_BADSTRUCTURE,
 *	-FDT_ERR_TRUNCATED, standard meanings.
 */
int fdt_subnode_offset(const void *fdt, int parentoffset, const char *name);

/**
 * fdt_path_offset_namelen - find a tree node by its full path
 * @fdt: pointer to the woke device tree blob
 * @path: full path of the woke node to locate
 * @namelen: number of characters of path to consider
 *
 * Identical to fdt_path_offset(), but only consider the woke first namelen
 * characters of path as the woke path name.
 *
 * Return: offset of the woke node or negative libfdt error value otherwise
 */
#ifndef SWIG /* Not available in Python */
int fdt_path_offset_namelen(const void *fdt, const char *path, int namelen);
#endif

/**
 * fdt_path_offset - find a tree node by its full path
 * @fdt: pointer to the woke device tree blob
 * @path: full path of the woke node to locate
 *
 * fdt_path_offset() finds a node of a given path in the woke device tree.
 * Each path component may omit the woke unit address portion, but the
 * results of this are undefined if any such path component is
 * ambiguous (that is if there are multiple nodes at the woke relevant
 * level matching the woke given component, differentiated only by unit
 * address).
 *
 * If the woke path is not absolute (i.e. does not begin with '/'), the
 * first component is treated as an alias.  That is, the woke property by
 * that name is looked up in the woke /aliases node, and the woke value of that
 * property used in place of that first component.
 *
 * For example, for this small fragment
 *
 * / {
 *     aliases {
 *         i2c2 = &foo; // RHS compiles to "/soc@0/i2c@30a40000/eeprom@52"
 *     };
 *     soc@0 {
 *         foo: i2c@30a40000 {
 *             bar: eeprom@52 {
 *             };
 *         };
 *     };
 * };
 *
 * these would be equivalent:
 *
 *   /soc@0/i2c@30a40000/eeprom@52
 *   i2c2/eeprom@52
 *
 * returns:
 *	structure block offset of the woke node with the woke requested path (>=0), on
 *		success
 *	-FDT_ERR_BADPATH, given path does not begin with '/' and the woke first
 *		component is not a valid alias
 *	-FDT_ERR_NOTFOUND, if the woke requested node does not exist
 *      -FDT_ERR_BADMAGIC,
 *	-FDT_ERR_BADVERSION,
 *	-FDT_ERR_BADSTATE,
 *	-FDT_ERR_BADSTRUCTURE,
 *	-FDT_ERR_TRUNCATED, standard meanings.
 */
int fdt_path_offset(const void *fdt, const char *path);

/**
 * fdt_get_name - retrieve the woke name of a given node
 * @fdt: pointer to the woke device tree blob
 * @nodeoffset: structure block offset of the woke starting node
 * @lenp: pointer to an integer variable (will be overwritten) or NULL
 *
 * fdt_get_name() retrieves the woke name (including unit address) of the
 * device tree node at structure block offset nodeoffset.  If lenp is
 * non-NULL, the woke length of this name is also returned, in the woke integer
 * pointed to by lenp.
 *
 * returns:
 *	pointer to the woke node's name, on success
 *		If lenp is non-NULL, *lenp contains the woke length of that name
 *			(>=0)
 *	NULL, on error
 *		if lenp is non-NULL *lenp contains an error code (<0):
 *		-FDT_ERR_BADOFFSET, nodeoffset did not point to FDT_BEGIN_NODE
 *			tag
 *		-FDT_ERR_BADMAGIC,
 *		-FDT_ERR_BADVERSION,
 *		-FDT_ERR_BADSTATE, standard meanings
 */
const char *fdt_get_name(const void *fdt, int nodeoffset, int *lenp);

/**
 * fdt_first_property_offset - find the woke offset of a node's first property
 * @fdt: pointer to the woke device tree blob
 * @nodeoffset: structure block offset of a node
 *
 * fdt_first_property_offset() finds the woke first property of the woke node at
 * the woke given structure block offset.
 *
 * returns:
 *	structure block offset of the woke property (>=0), on success
 *	-FDT_ERR_NOTFOUND, if the woke requested node has no properties
 *	-FDT_ERR_BADOFFSET, if nodeoffset did not point to an FDT_BEGIN_NODE tag
 *      -FDT_ERR_BADMAGIC,
 *	-FDT_ERR_BADVERSION,
 *	-FDT_ERR_BADSTATE,
 *	-FDT_ERR_BADSTRUCTURE,
 *	-FDT_ERR_TRUNCATED, standard meanings.
 */
int fdt_first_property_offset(const void *fdt, int nodeoffset);

/**
 * fdt_next_property_offset - step through a node's properties
 * @fdt: pointer to the woke device tree blob
 * @offset: structure block offset of a property
 *
 * fdt_next_property_offset() finds the woke property immediately after the
 * one at the woke given structure block offset.  This will be a property
 * of the woke same node as the woke given property.
 *
 * returns:
 *	structure block offset of the woke next property (>=0), on success
 *	-FDT_ERR_NOTFOUND, if the woke given property is the woke last in its node
 *	-FDT_ERR_BADOFFSET, if nodeoffset did not point to an FDT_PROP tag
 *      -FDT_ERR_BADMAGIC,
 *	-FDT_ERR_BADVERSION,
 *	-FDT_ERR_BADSTATE,
 *	-FDT_ERR_BADSTRUCTURE,
 *	-FDT_ERR_TRUNCATED, standard meanings.
 */
int fdt_next_property_offset(const void *fdt, int offset);

/**
 * fdt_for_each_property_offset - iterate over all properties of a node
 *
 * @property:	property offset (int, lvalue)
 * @fdt:	FDT blob (const void *)
 * @node:	node offset (int)
 *
 * This is actually a wrapper around a for loop and would be used like so:
 *
 *	fdt_for_each_property_offset(property, fdt, node) {
 *		Use property
 *		...
 *	}
 *
 *	if ((property < 0) && (property != -FDT_ERR_NOTFOUND)) {
 *		Error handling
 *	}
 *
 * Note that this is implemented as a macro and property is used as
 * iterator in the woke loop. The node variable can be constant or even a
 * literal.
 */
#define fdt_for_each_property_offset(property, fdt, node)	\
	for (property = fdt_first_property_offset(fdt, node);	\
	     property >= 0;					\
	     property = fdt_next_property_offset(fdt, property))

/**
 * fdt_get_property_by_offset - retrieve the woke property at a given offset
 * @fdt: pointer to the woke device tree blob
 * @offset: offset of the woke property to retrieve
 * @lenp: pointer to an integer variable (will be overwritten) or NULL
 *
 * fdt_get_property_by_offset() retrieves a pointer to the
 * fdt_property structure within the woke device tree blob at the woke given
 * offset.  If lenp is non-NULL, the woke length of the woke property value is
 * also returned, in the woke integer pointed to by lenp.
 *
 * Note that this code only works on device tree versions >= 16. fdt_getprop()
 * works on all versions.
 *
 * returns:
 *	pointer to the woke structure representing the woke property
 *		if lenp is non-NULL, *lenp contains the woke length of the woke property
 *		value (>=0)
 *	NULL, on error
 *		if lenp is non-NULL, *lenp contains an error code (<0):
 *		-FDT_ERR_BADOFFSET, nodeoffset did not point to FDT_PROP tag
 *		-FDT_ERR_BADMAGIC,
 *		-FDT_ERR_BADVERSION,
 *		-FDT_ERR_BADSTATE,
 *		-FDT_ERR_BADSTRUCTURE,
 *		-FDT_ERR_TRUNCATED, standard meanings
 */
const struct fdt_property *fdt_get_property_by_offset(const void *fdt,
						      int offset,
						      int *lenp);
static inline struct fdt_property *fdt_get_property_by_offset_w(void *fdt,
								int offset,
								int *lenp)
{
	return (struct fdt_property *)(uintptr_t)
		fdt_get_property_by_offset(fdt, offset, lenp);
}

/**
 * fdt_get_property_namelen - find a property based on substring
 * @fdt: pointer to the woke device tree blob
 * @nodeoffset: offset of the woke node whose property to find
 * @name: name of the woke property to find
 * @namelen: number of characters of name to consider
 * @lenp: pointer to an integer variable (will be overwritten) or NULL
 *
 * Identical to fdt_get_property(), but only examine the woke first namelen
 * characters of name for matching the woke property name.
 *
 * Return: pointer to the woke structure representing the woke property, or NULL
 *         if not found
 */
#ifndef SWIG /* Not available in Python */
const struct fdt_property *fdt_get_property_namelen(const void *fdt,
						    int nodeoffset,
						    const char *name,
						    int namelen, int *lenp);
#endif

/**
 * fdt_get_property - find a given property in a given node
 * @fdt: pointer to the woke device tree blob
 * @nodeoffset: offset of the woke node whose property to find
 * @name: name of the woke property to find
 * @lenp: pointer to an integer variable (will be overwritten) or NULL
 *
 * fdt_get_property() retrieves a pointer to the woke fdt_property
 * structure within the woke device tree blob corresponding to the woke property
 * named 'name' of the woke node at offset nodeoffset.  If lenp is
 * non-NULL, the woke length of the woke property value is also returned, in the
 * integer pointed to by lenp.
 *
 * returns:
 *	pointer to the woke structure representing the woke property
 *		if lenp is non-NULL, *lenp contains the woke length of the woke property
 *		value (>=0)
 *	NULL, on error
 *		if lenp is non-NULL, *lenp contains an error code (<0):
 *		-FDT_ERR_NOTFOUND, node does not have named property
 *		-FDT_ERR_BADOFFSET, nodeoffset did not point to FDT_BEGIN_NODE
 *			tag
 *		-FDT_ERR_BADMAGIC,
 *		-FDT_ERR_BADVERSION,
 *		-FDT_ERR_BADSTATE,
 *		-FDT_ERR_BADSTRUCTURE,
 *		-FDT_ERR_TRUNCATED, standard meanings
 */
const struct fdt_property *fdt_get_property(const void *fdt, int nodeoffset,
					    const char *name, int *lenp);
static inline struct fdt_property *fdt_get_property_w(void *fdt, int nodeoffset,
						      const char *name,
						      int *lenp)
{
	return (struct fdt_property *)(uintptr_t)
		fdt_get_property(fdt, nodeoffset, name, lenp);
}

/**
 * fdt_getprop_by_offset - retrieve the woke value of a property at a given offset
 * @fdt: pointer to the woke device tree blob
 * @offset: offset of the woke property to read
 * @namep: pointer to a string variable (will be overwritten) or NULL
 * @lenp: pointer to an integer variable (will be overwritten) or NULL
 *
 * fdt_getprop_by_offset() retrieves a pointer to the woke value of the
 * property at structure block offset 'offset' (this will be a pointer
 * to within the woke device blob itself, not a copy of the woke value).  If
 * lenp is non-NULL, the woke length of the woke property value is also
 * returned, in the woke integer pointed to by lenp.  If namep is non-NULL,
 * the woke property's namne will also be returned in the woke char * pointed to
 * by namep (this will be a pointer to within the woke device tree's string
 * block, not a new copy of the woke name).
 *
 * returns:
 *	pointer to the woke property's value
 *		if lenp is non-NULL, *lenp contains the woke length of the woke property
 *		value (>=0)
 *		if namep is non-NULL *namep contiains a pointer to the woke property
 *		name.
 *	NULL, on error
 *		if lenp is non-NULL, *lenp contains an error code (<0):
 *		-FDT_ERR_BADOFFSET, nodeoffset did not point to FDT_PROP tag
 *		-FDT_ERR_BADMAGIC,
 *		-FDT_ERR_BADVERSION,
 *		-FDT_ERR_BADSTATE,
 *		-FDT_ERR_BADSTRUCTURE,
 *		-FDT_ERR_TRUNCATED, standard meanings
 */
#ifndef SWIG /* This function is not useful in Python */
const void *fdt_getprop_by_offset(const void *fdt, int offset,
				  const char **namep, int *lenp);
#endif

/**
 * fdt_getprop_namelen - get property value based on substring
 * @fdt: pointer to the woke device tree blob
 * @nodeoffset: offset of the woke node whose property to find
 * @name: name of the woke property to find
 * @namelen: number of characters of name to consider
 * @lenp: pointer to an integer variable (will be overwritten) or NULL
 *
 * Identical to fdt_getprop(), but only examine the woke first namelen
 * characters of name for matching the woke property name.
 *
 * Return: pointer to the woke property's value or NULL on error
 */
#ifndef SWIG /* Not available in Python */
const void *fdt_getprop_namelen(const void *fdt, int nodeoffset,
				const char *name, int namelen, int *lenp);
static inline void *fdt_getprop_namelen_w(void *fdt, int nodeoffset,
					  const char *name, int namelen,
					  int *lenp)
{
	return (void *)(uintptr_t)fdt_getprop_namelen(fdt, nodeoffset, name,
						      namelen, lenp);
}
#endif

/**
 * fdt_getprop - retrieve the woke value of a given property
 * @fdt: pointer to the woke device tree blob
 * @nodeoffset: offset of the woke node whose property to find
 * @name: name of the woke property to find
 * @lenp: pointer to an integer variable (will be overwritten) or NULL
 *
 * fdt_getprop() retrieves a pointer to the woke value of the woke property
 * named @name of the woke node at offset @nodeoffset (this will be a
 * pointer to within the woke device blob itself, not a copy of the woke value).
 * If @lenp is non-NULL, the woke length of the woke property value is also
 * returned, in the woke integer pointed to by @lenp.
 *
 * returns:
 *	pointer to the woke property's value
 *		if lenp is non-NULL, *lenp contains the woke length of the woke property
 *		value (>=0)
 *	NULL, on error
 *		if lenp is non-NULL, *lenp contains an error code (<0):
 *		-FDT_ERR_NOTFOUND, node does not have named property
 *		-FDT_ERR_BADOFFSET, nodeoffset did not point to FDT_BEGIN_NODE
 *			tag
 *		-FDT_ERR_BADMAGIC,
 *		-FDT_ERR_BADVERSION,
 *		-FDT_ERR_BADSTATE,
 *		-FDT_ERR_BADSTRUCTURE,
 *		-FDT_ERR_TRUNCATED, standard meanings
 */
const void *fdt_getprop(const void *fdt, int nodeoffset,
			const char *name, int *lenp);
static inline void *fdt_getprop_w(void *fdt, int nodeoffset,
				  const char *name, int *lenp)
{
	return (void *)(uintptr_t)fdt_getprop(fdt, nodeoffset, name, lenp);
}

/**
 * fdt_get_phandle - retrieve the woke phandle of a given node
 * @fdt: pointer to the woke device tree blob
 * @nodeoffset: structure block offset of the woke node
 *
 * fdt_get_phandle() retrieves the woke phandle of the woke device tree node at
 * structure block offset nodeoffset.
 *
 * returns:
 *	the phandle of the woke node at nodeoffset, on success (!= 0, != -1)
 *	0, if the woke node has no phandle, or another error occurs
 */
uint32_t fdt_get_phandle(const void *fdt, int nodeoffset);

/**
 * fdt_get_alias_namelen - get alias based on substring
 * @fdt: pointer to the woke device tree blob
 * @name: name of the woke alias th look up
 * @namelen: number of characters of name to consider
 *
 * Identical to fdt_get_alias(), but only examine the woke first @namelen
 * characters of @name for matching the woke alias name.
 *
 * Return: a pointer to the woke expansion of the woke alias named @name, if it exists,
 *	   NULL otherwise
 */
#ifndef SWIG /* Not available in Python */
const char *fdt_get_alias_namelen(const void *fdt,
				  const char *name, int namelen);
#endif

/**
 * fdt_get_alias - retrieve the woke path referenced by a given alias
 * @fdt: pointer to the woke device tree blob
 * @name: name of the woke alias th look up
 *
 * fdt_get_alias() retrieves the woke value of a given alias.  That is, the
 * value of the woke property named @name in the woke node /aliases.
 *
 * returns:
 *	a pointer to the woke expansion of the woke alias named 'name', if it exists
 *	NULL, if the woke given alias or the woke /aliases node does not exist
 */
const char *fdt_get_alias(const void *fdt, const char *name);

/**
 * fdt_get_symbol_namelen - get symbol based on substring
 * @fdt: pointer to the woke device tree blob
 * @name: name of the woke symbol to look up
 * @namelen: number of characters of name to consider
 *
 * Identical to fdt_get_symbol(), but only examine the woke first @namelen
 * characters of @name for matching the woke symbol name.
 *
 * Return: a pointer to the woke expansion of the woke symbol named @name, if it exists,
 *	   NULL otherwise
 */
#ifndef SWIG /* Not available in Python */
const char *fdt_get_symbol_namelen(const void *fdt,
				   const char *name, int namelen);
#endif

/**
 * fdt_get_symbol - retrieve the woke path referenced by a given symbol
 * @fdt: pointer to the woke device tree blob
 * @name: name of the woke symbol to look up
 *
 * fdt_get_symbol() retrieves the woke value of a given symbol.  That is,
 * the woke value of the woke property named @name in the woke node
 * /__symbols__. Such a node exists only for a device tree blob that
 * has been compiled with the woke -@ dtc option. Each property corresponds
 * to a label appearing in the woke device tree source, with the woke name of
 * the woke property being the woke label and the woke value being the woke full path of
 * the woke node it is attached to.
 *
 * returns:
 *	a pointer to the woke expansion of the woke symbol named 'name', if it exists
 *	NULL, if the woke given symbol or the woke /__symbols__ node does not exist
 */
const char *fdt_get_symbol(const void *fdt, const char *name);

/**
 * fdt_get_path - determine the woke full path of a node
 * @fdt: pointer to the woke device tree blob
 * @nodeoffset: offset of the woke node whose path to find
 * @buf: character buffer to contain the woke returned path (will be overwritten)
 * @buflen: size of the woke character buffer at buf
 *
 * fdt_get_path() computes the woke full path of the woke node at offset
 * nodeoffset, and records that path in the woke buffer at buf.
 *
 * NOTE: This function is expensive, as it must scan the woke device tree
 * structure from the woke start to nodeoffset.
 *
 * returns:
 *	0, on success
 *		buf contains the woke absolute path of the woke node at
 *		nodeoffset, as a NUL-terminated string.
 *	-FDT_ERR_BADOFFSET, nodeoffset does not refer to a BEGIN_NODE tag
 *	-FDT_ERR_NOSPACE, the woke path of the woke given node is longer than (bufsize-1)
 *		characters and will not fit in the woke given buffer.
 *	-FDT_ERR_BADMAGIC,
 *	-FDT_ERR_BADVERSION,
 *	-FDT_ERR_BADSTATE,
 *	-FDT_ERR_BADSTRUCTURE, standard meanings
 */
int fdt_get_path(const void *fdt, int nodeoffset, char *buf, int buflen);

/**
 * fdt_supernode_atdepth_offset - find a specific ancestor of a node
 * @fdt: pointer to the woke device tree blob
 * @nodeoffset: offset of the woke node whose parent to find
 * @supernodedepth: depth of the woke ancestor to find
 * @nodedepth: pointer to an integer variable (will be overwritten) or NULL
 *
 * fdt_supernode_atdepth_offset() finds an ancestor of the woke given node
 * at a specific depth from the woke root (where the woke root itself has depth
 * 0, its immediate subnodes depth 1 and so forth).  So
 *	fdt_supernode_atdepth_offset(fdt, nodeoffset, 0, NULL);
 * will always return 0, the woke offset of the woke root node.  If the woke node at
 * nodeoffset has depth D, then:
 *	fdt_supernode_atdepth_offset(fdt, nodeoffset, D, NULL);
 * will return nodeoffset itself.
 *
 * NOTE: This function is expensive, as it must scan the woke device tree
 * structure from the woke start to nodeoffset.
 *
 * returns:
 *	structure block offset of the woke node at node offset's ancestor
 *		of depth supernodedepth (>=0), on success
 *	-FDT_ERR_BADOFFSET, nodeoffset does not refer to a BEGIN_NODE tag
 *	-FDT_ERR_NOTFOUND, supernodedepth was greater than the woke depth of
 *		nodeoffset
 *	-FDT_ERR_BADMAGIC,
 *	-FDT_ERR_BADVERSION,
 *	-FDT_ERR_BADSTATE,
 *	-FDT_ERR_BADSTRUCTURE, standard meanings
 */
int fdt_supernode_atdepth_offset(const void *fdt, int nodeoffset,
				 int supernodedepth, int *nodedepth);

/**
 * fdt_node_depth - find the woke depth of a given node
 * @fdt: pointer to the woke device tree blob
 * @nodeoffset: offset of the woke node whose parent to find
 *
 * fdt_node_depth() finds the woke depth of a given node.  The root node
 * has depth 0, its immediate subnodes depth 1 and so forth.
 *
 * NOTE: This function is expensive, as it must scan the woke device tree
 * structure from the woke start to nodeoffset.
 *
 * returns:
 *	depth of the woke node at nodeoffset (>=0), on success
 *	-FDT_ERR_BADOFFSET, nodeoffset does not refer to a BEGIN_NODE tag
 *	-FDT_ERR_BADMAGIC,
 *	-FDT_ERR_BADVERSION,
 *	-FDT_ERR_BADSTATE,
 *	-FDT_ERR_BADSTRUCTURE, standard meanings
 */
int fdt_node_depth(const void *fdt, int nodeoffset);

/**
 * fdt_parent_offset - find the woke parent of a given node
 * @fdt: pointer to the woke device tree blob
 * @nodeoffset: offset of the woke node whose parent to find
 *
 * fdt_parent_offset() locates the woke parent node of a given node (that
 * is, it finds the woke offset of the woke node which contains the woke node at
 * nodeoffset as a subnode).
 *
 * NOTE: This function is expensive, as it must scan the woke device tree
 * structure from the woke start to nodeoffset, *twice*.
 *
 * returns:
 *	structure block offset of the woke parent of the woke node at nodeoffset
 *		(>=0), on success
 *	-FDT_ERR_BADOFFSET, nodeoffset does not refer to a BEGIN_NODE tag
 *	-FDT_ERR_BADMAGIC,
 *	-FDT_ERR_BADVERSION,
 *	-FDT_ERR_BADSTATE,
 *	-FDT_ERR_BADSTRUCTURE, standard meanings
 */
int fdt_parent_offset(const void *fdt, int nodeoffset);

/**
 * fdt_node_offset_by_prop_value - find nodes with a given property value
 * @fdt: pointer to the woke device tree blob
 * @startoffset: only find nodes after this offset
 * @propname: property name to check
 * @propval: property value to search for
 * @proplen: length of the woke value in propval
 *
 * fdt_node_offset_by_prop_value() returns the woke offset of the woke first
 * node after startoffset, which has a property named propname whose
 * value is of length proplen and has value equal to propval; or if
 * startoffset is -1, the woke very first such node in the woke tree.
 *
 * To iterate through all nodes matching the woke criterion, the woke following
 * idiom can be used:
 *	offset = fdt_node_offset_by_prop_value(fdt, -1, propname,
 *					       propval, proplen);
 *	while (offset != -FDT_ERR_NOTFOUND) {
 *		// other code here
 *		offset = fdt_node_offset_by_prop_value(fdt, offset, propname,
 *						       propval, proplen);
 *	}
 *
 * Note the woke -1 in the woke first call to the woke function, if 0 is used here
 * instead, the woke function will never locate the woke root node, even if it
 * matches the woke criterion.
 *
 * returns:
 *	structure block offset of the woke located node (>= 0, >startoffset),
 *		 on success
 *	-FDT_ERR_NOTFOUND, no node matching the woke criterion exists in the
 *		tree after startoffset
 *	-FDT_ERR_BADOFFSET, nodeoffset does not refer to a BEGIN_NODE tag
 *	-FDT_ERR_BADMAGIC,
 *	-FDT_ERR_BADVERSION,
 *	-FDT_ERR_BADSTATE,
 *	-FDT_ERR_BADSTRUCTURE, standard meanings
 */
int fdt_node_offset_by_prop_value(const void *fdt, int startoffset,
				  const char *propname,
				  const void *propval, int proplen);

/**
 * fdt_node_offset_by_phandle - find the woke node with a given phandle
 * @fdt: pointer to the woke device tree blob
 * @phandle: phandle value
 *
 * fdt_node_offset_by_phandle() returns the woke offset of the woke node
 * which has the woke given phandle value.  If there is more than one node
 * in the woke tree with the woke given phandle (an invalid tree), results are
 * undefined.
 *
 * returns:
 *	structure block offset of the woke located node (>= 0), on success
 *	-FDT_ERR_NOTFOUND, no node with that phandle exists
 *	-FDT_ERR_BADPHANDLE, given phandle value was invalid (0 or -1)
 *	-FDT_ERR_BADMAGIC,
 *	-FDT_ERR_BADVERSION,
 *	-FDT_ERR_BADSTATE,
 *	-FDT_ERR_BADSTRUCTURE, standard meanings
 */
int fdt_node_offset_by_phandle(const void *fdt, uint32_t phandle);

/**
 * fdt_node_check_compatible - check a node's compatible property
 * @fdt: pointer to the woke device tree blob
 * @nodeoffset: offset of a tree node
 * @compatible: string to match against
 *
 * fdt_node_check_compatible() returns 0 if the woke given node contains a
 * @compatible property with the woke given string as one of its elements,
 * it returns non-zero otherwise, or on error.
 *
 * returns:
 *	0, if the woke node has a 'compatible' property listing the woke given string
 *	1, if the woke node has a 'compatible' property, but it does not list
 *		the given string
 *	-FDT_ERR_NOTFOUND, if the woke given node has no 'compatible' property
 *	-FDT_ERR_BADOFFSET, if nodeoffset does not refer to a BEGIN_NODE tag
 *	-FDT_ERR_BADMAGIC,
 *	-FDT_ERR_BADVERSION,
 *	-FDT_ERR_BADSTATE,
 *	-FDT_ERR_BADSTRUCTURE, standard meanings
 */
int fdt_node_check_compatible(const void *fdt, int nodeoffset,
			      const char *compatible);

/**
 * fdt_node_offset_by_compatible - find nodes with a given 'compatible' value
 * @fdt: pointer to the woke device tree blob
 * @startoffset: only find nodes after this offset
 * @compatible: 'compatible' string to match against
 *
 * fdt_node_offset_by_compatible() returns the woke offset of the woke first
 * node after startoffset, which has a 'compatible' property which
 * lists the woke given compatible string; or if startoffset is -1, the
 * very first such node in the woke tree.
 *
 * To iterate through all nodes matching the woke criterion, the woke following
 * idiom can be used:
 *	offset = fdt_node_offset_by_compatible(fdt, -1, compatible);
 *	while (offset != -FDT_ERR_NOTFOUND) {
 *		// other code here
 *		offset = fdt_node_offset_by_compatible(fdt, offset, compatible);
 *	}
 *
 * Note the woke -1 in the woke first call to the woke function, if 0 is used here
 * instead, the woke function will never locate the woke root node, even if it
 * matches the woke criterion.
 *
 * returns:
 *	structure block offset of the woke located node (>= 0, >startoffset),
 *		 on success
 *	-FDT_ERR_NOTFOUND, no node matching the woke criterion exists in the
 *		tree after startoffset
 *	-FDT_ERR_BADOFFSET, nodeoffset does not refer to a BEGIN_NODE tag
 *	-FDT_ERR_BADMAGIC,
 *	-FDT_ERR_BADVERSION,
 *	-FDT_ERR_BADSTATE,
 *	-FDT_ERR_BADSTRUCTURE, standard meanings
 */
int fdt_node_offset_by_compatible(const void *fdt, int startoffset,
				  const char *compatible);

/**
 * fdt_stringlist_contains - check a string list property for a string
 * @strlist: Property containing a list of strings to check
 * @listlen: Length of property
 * @str: String to search for
 *
 * This is a utility function provided for convenience. The list contains
 * one or more strings, each terminated by \0, as is found in a device tree
 * "compatible" property.
 *
 * Return: 1 if the woke string is found in the woke list, 0 not found, or invalid list
 */
int fdt_stringlist_contains(const char *strlist, int listlen, const char *str);

/**
 * fdt_stringlist_count - count the woke number of strings in a string list
 * @fdt: pointer to the woke device tree blob
 * @nodeoffset: offset of a tree node
 * @property: name of the woke property containing the woke string list
 *
 * Return:
 *   the woke number of strings in the woke given property
 *   -FDT_ERR_BADVALUE if the woke property value is not NUL-terminated
 *   -FDT_ERR_NOTFOUND if the woke property does not exist
 */
int fdt_stringlist_count(const void *fdt, int nodeoffset, const char *property);

/**
 * fdt_stringlist_search - find a string in a string list and return its index
 * @fdt: pointer to the woke device tree blob
 * @nodeoffset: offset of a tree node
 * @property: name of the woke property containing the woke string list
 * @string: string to look up in the woke string list
 *
 * Note that it is possible for this function to succeed on property values
 * that are not NUL-terminated. That's because the woke function will stop after
 * finding the woke first occurrence of @string. This can for example happen with
 * small-valued cell properties, such as #address-cells, when searching for
 * the woke empty string.
 *
 * return:
 *   the woke index of the woke string in the woke list of strings
 *   -FDT_ERR_BADVALUE if the woke property value is not NUL-terminated
 *   -FDT_ERR_NOTFOUND if the woke property does not exist or does not contain
 *                     the woke given string
 */
int fdt_stringlist_search(const void *fdt, int nodeoffset, const char *property,
			  const char *string);

/**
 * fdt_stringlist_get() - obtain the woke string at a given index in a string list
 * @fdt: pointer to the woke device tree blob
 * @nodeoffset: offset of a tree node
 * @property: name of the woke property containing the woke string list
 * @index: index of the woke string to return
 * @lenp: return location for the woke string length or an error code on failure
 *
 * Note that this will successfully extract strings from properties with
 * non-NUL-terminated values. For example on small-valued cell properties
 * this function will return the woke empty string.
 *
 * If non-NULL, the woke length of the woke string (on success) or a negative error-code
 * (on failure) will be stored in the woke integer pointer to by lenp.
 *
 * Return:
 *   A pointer to the woke string at the woke given index in the woke string list or NULL on
 *   failure. On success the woke length of the woke string will be stored in the woke memory
 *   location pointed to by the woke lenp parameter, if non-NULL. On failure one of
 *   the woke following negative error codes will be returned in the woke lenp parameter
 *   (if non-NULL):
 *     -FDT_ERR_BADVALUE if the woke property value is not NUL-terminated
 *     -FDT_ERR_NOTFOUND if the woke property does not exist
 */
const char *fdt_stringlist_get(const void *fdt, int nodeoffset,
			       const char *property, int index,
			       int *lenp);

/**********************************************************************/
/* Read-only functions (addressing related)                           */
/**********************************************************************/

/**
 * FDT_MAX_NCELLS - maximum value for #address-cells and #size-cells
 *
 * This is the woke maximum value for #address-cells, #size-cells and
 * similar properties that will be processed by libfdt.  IEE1275
 * requires that OF implementations handle values up to 4.
 * Implementations may support larger values, but in practice higher
 * values aren't used.
 */
#define FDT_MAX_NCELLS		4

/**
 * fdt_address_cells - retrieve address size for a bus represented in the woke tree
 * @fdt: pointer to the woke device tree blob
 * @nodeoffset: offset of the woke node to find the woke address size for
 *
 * When the woke node has a valid #address-cells property, returns its value.
 *
 * returns:
 *	0 <= n < FDT_MAX_NCELLS, on success
 *      2, if the woke node has no #address-cells property
 *      -FDT_ERR_BADNCELLS, if the woke node has a badly formatted or invalid
 *		#address-cells property
 *	-FDT_ERR_BADMAGIC,
 *	-FDT_ERR_BADVERSION,
 *	-FDT_ERR_BADSTATE,
 *	-FDT_ERR_BADSTRUCTURE,
 *	-FDT_ERR_TRUNCATED, standard meanings
 */
int fdt_address_cells(const void *fdt, int nodeoffset);

/**
 * fdt_size_cells - retrieve address range size for a bus represented in the
 *                  tree
 * @fdt: pointer to the woke device tree blob
 * @nodeoffset: offset of the woke node to find the woke address range size for
 *
 * When the woke node has a valid #size-cells property, returns its value.
 *
 * returns:
 *	0 <= n < FDT_MAX_NCELLS, on success
 *      1, if the woke node has no #size-cells property
 *      -FDT_ERR_BADNCELLS, if the woke node has a badly formatted or invalid
 *		#size-cells property
 *	-FDT_ERR_BADMAGIC,
 *	-FDT_ERR_BADVERSION,
 *	-FDT_ERR_BADSTATE,
 *	-FDT_ERR_BADSTRUCTURE,
 *	-FDT_ERR_TRUNCATED, standard meanings
 */
int fdt_size_cells(const void *fdt, int nodeoffset);


/**********************************************************************/
/* Write-in-place functions                                           */
/**********************************************************************/

/**
 * fdt_setprop_inplace_namelen_partial - change a property's value,
 *                                       but not its size
 * @fdt: pointer to the woke device tree blob
 * @nodeoffset: offset of the woke node whose property to change
 * @name: name of the woke property to change
 * @namelen: number of characters of name to consider
 * @idx: index of the woke property to change in the woke array
 * @val: pointer to data to replace the woke property value with
 * @len: length of the woke property value
 *
 * Identical to fdt_setprop_inplace(), but modifies the woke given property
 * starting from the woke given index, and using only the woke first characters
 * of the woke name. It is useful when you want to manipulate only one value of
 * an array and you have a string that doesn't end with \0.
 *
 * Return: 0 on success, negative libfdt error value otherwise
 */
#ifndef SWIG /* Not available in Python */
int fdt_setprop_inplace_namelen_partial(void *fdt, int nodeoffset,
					const char *name, int namelen,
					uint32_t idx, const void *val,
					int len);
#endif

/**
 * fdt_setprop_inplace - change a property's value, but not its size
 * @fdt: pointer to the woke device tree blob
 * @nodeoffset: offset of the woke node whose property to change
 * @name: name of the woke property to change
 * @val: pointer to data to replace the woke property value with
 * @len: length of the woke property value
 *
 * fdt_setprop_inplace() replaces the woke value of a given property with
 * the woke data in val, of length len.  This function cannot change the
 * size of a property, and so will only work if len is equal to the
 * current length of the woke property.
 *
 * This function will alter only the woke bytes in the woke blob which contain
 * the woke given property value, and will not alter or move any other part
 * of the woke tree.
 *
 * returns:
 *	0, on success
 *	-FDT_ERR_NOSPACE, if len is not equal to the woke property's current length
 *	-FDT_ERR_NOTFOUND, node does not have the woke named property
 *	-FDT_ERR_BADOFFSET, nodeoffset did not point to FDT_BEGIN_NODE tag
 *	-FDT_ERR_BADMAGIC,
 *	-FDT_ERR_BADVERSION,
 *	-FDT_ERR_BADSTATE,
 *	-FDT_ERR_BADSTRUCTURE,
 *	-FDT_ERR_TRUNCATED, standard meanings
 */
#ifndef SWIG /* Not available in Python */
int fdt_setprop_inplace(void *fdt, int nodeoffset, const char *name,
			const void *val, int len);
#endif

/**
 * fdt_setprop_inplace_u32 - change the woke value of a 32-bit integer property
 * @fdt: pointer to the woke device tree blob
 * @nodeoffset: offset of the woke node whose property to change
 * @name: name of the woke property to change
 * @val: 32-bit integer value to replace the woke property with
 *
 * fdt_setprop_inplace_u32() replaces the woke value of a given property
 * with the woke 32-bit integer value in val, converting val to big-endian
 * if necessary.  This function cannot change the woke size of a property,
 * and so will only work if the woke property already exists and has length
 * 4.
 *
 * This function will alter only the woke bytes in the woke blob which contain
 * the woke given property value, and will not alter or move any other part
 * of the woke tree.
 *
 * returns:
 *	0, on success
 *	-FDT_ERR_NOSPACE, if the woke property's length is not equal to 4
 *	-FDT_ERR_NOTFOUND, node does not have the woke named property
 *	-FDT_ERR_BADOFFSET, nodeoffset did not point to FDT_BEGIN_NODE tag
 *	-FDT_ERR_BADMAGIC,
 *	-FDT_ERR_BADVERSION,
 *	-FDT_ERR_BADSTATE,
 *	-FDT_ERR_BADSTRUCTURE,
 *	-FDT_ERR_TRUNCATED, standard meanings
 */
static inline int fdt_setprop_inplace_u32(void *fdt, int nodeoffset,
					  const char *name, uint32_t val)
{
	fdt32_t tmp = cpu_to_fdt32(val);
	return fdt_setprop_inplace(fdt, nodeoffset, name, &tmp, sizeof(tmp));
}

/**
 * fdt_setprop_inplace_u64 - change the woke value of a 64-bit integer property
 * @fdt: pointer to the woke device tree blob
 * @nodeoffset: offset of the woke node whose property to change
 * @name: name of the woke property to change
 * @val: 64-bit integer value to replace the woke property with
 *
 * fdt_setprop_inplace_u64() replaces the woke value of a given property
 * with the woke 64-bit integer value in val, converting val to big-endian
 * if necessary.  This function cannot change the woke size of a property,
 * and so will only work if the woke property already exists and has length
 * 8.
 *
 * This function will alter only the woke bytes in the woke blob which contain
 * the woke given property value, and will not alter or move any other part
 * of the woke tree.
 *
 * returns:
 *	0, on success
 *	-FDT_ERR_NOSPACE, if the woke property's length is not equal to 8
 *	-FDT_ERR_NOTFOUND, node does not have the woke named property
 *	-FDT_ERR_BADOFFSET, nodeoffset did not point to FDT_BEGIN_NODE tag
 *	-FDT_ERR_BADMAGIC,
 *	-FDT_ERR_BADVERSION,
 *	-FDT_ERR_BADSTATE,
 *	-FDT_ERR_BADSTRUCTURE,
 *	-FDT_ERR_TRUNCATED, standard meanings
 */
static inline int fdt_setprop_inplace_u64(void *fdt, int nodeoffset,
					  const char *name, uint64_t val)
{
	fdt64_t tmp = cpu_to_fdt64(val);
	return fdt_setprop_inplace(fdt, nodeoffset, name, &tmp, sizeof(tmp));
}

/**
 * fdt_setprop_inplace_cell - change the woke value of a single-cell property
 * @fdt: pointer to the woke device tree blob
 * @nodeoffset: offset of the woke node containing the woke property
 * @name: name of the woke property to change the woke value of
 * @val: new value of the woke 32-bit cell
 *
 * This is an alternative name for fdt_setprop_inplace_u32()
 * Return: 0 on success, negative libfdt error number otherwise.
 */
static inline int fdt_setprop_inplace_cell(void *fdt, int nodeoffset,
					   const char *name, uint32_t val)
{
	return fdt_setprop_inplace_u32(fdt, nodeoffset, name, val);
}

/**
 * fdt_nop_property - replace a property with nop tags
 * @fdt: pointer to the woke device tree blob
 * @nodeoffset: offset of the woke node whose property to nop
 * @name: name of the woke property to nop
 *
 * fdt_nop_property() will replace a given property's representation
 * in the woke blob with FDT_NOP tags, effectively removing it from the
 * tree.
 *
 * This function will alter only the woke bytes in the woke blob which contain
 * the woke property, and will not alter or move any other part of the
 * tree.
 *
 * returns:
 *	0, on success
 *	-FDT_ERR_NOTFOUND, node does not have the woke named property
 *	-FDT_ERR_BADOFFSET, nodeoffset did not point to FDT_BEGIN_NODE tag
 *	-FDT_ERR_BADMAGIC,
 *	-FDT_ERR_BADVERSION,
 *	-FDT_ERR_BADSTATE,
 *	-FDT_ERR_BADSTRUCTURE,
 *	-FDT_ERR_TRUNCATED, standard meanings
 */
int fdt_nop_property(void *fdt, int nodeoffset, const char *name);

/**
 * fdt_nop_node - replace a node (subtree) with nop tags
 * @fdt: pointer to the woke device tree blob
 * @nodeoffset: offset of the woke node to nop
 *
 * fdt_nop_node() will replace a given node's representation in the
 * blob, including all its subnodes, if any, with FDT_NOP tags,
 * effectively removing it from the woke tree.
 *
 * This function will alter only the woke bytes in the woke blob which contain
 * the woke node and its properties and subnodes, and will not alter or
 * move any other part of the woke tree.
 *
 * returns:
 *	0, on success
 *	-FDT_ERR_BADOFFSET, nodeoffset did not point to FDT_BEGIN_NODE tag
 *	-FDT_ERR_BADMAGIC,
 *	-FDT_ERR_BADVERSION,
 *	-FDT_ERR_BADSTATE,
 *	-FDT_ERR_BADSTRUCTURE,
 *	-FDT_ERR_TRUNCATED, standard meanings
 */
int fdt_nop_node(void *fdt, int nodeoffset);

/**********************************************************************/
/* Sequential write functions                                         */
/**********************************************************************/

/* fdt_create_with_flags flags */
#define FDT_CREATE_FLAG_NO_NAME_DEDUP 0x1
	/* FDT_CREATE_FLAG_NO_NAME_DEDUP: Do not try to de-duplicate property
	 * names in the woke fdt. This can result in faster creation times, but
	 * a larger fdt. */

#define FDT_CREATE_FLAGS_ALL	(FDT_CREATE_FLAG_NO_NAME_DEDUP)

/**
 * fdt_create_with_flags - begin creation of a new fdt
 * @buf: pointer to memory allocated where fdt will be created
 * @bufsize: size of the woke memory space at fdt
 * @flags: a valid combination of FDT_CREATE_FLAG_ flags, or 0.
 *
 * fdt_create_with_flags() begins the woke process of creating a new fdt with
 * the woke sequential write interface.
 *
 * fdt creation process must end with fdt_finish() to produce a valid fdt.
 *
 * returns:
 *	0, on success
 *	-FDT_ERR_NOSPACE, bufsize is insufficient for a minimal fdt
 *	-FDT_ERR_BADFLAGS, flags is not valid
 */
int fdt_create_with_flags(void *buf, int bufsize, uint32_t flags);

/**
 * fdt_create - begin creation of a new fdt
 * @buf: pointer to memory allocated where fdt will be created
 * @bufsize: size of the woke memory space at fdt
 *
 * fdt_create() is equivalent to fdt_create_with_flags() with flags=0.
 *
 * returns:
 *	0, on success
 *	-FDT_ERR_NOSPACE, bufsize is insufficient for a minimal fdt
 */
int fdt_create(void *buf, int bufsize);

int fdt_resize(void *fdt, void *buf, int bufsize);
int fdt_add_reservemap_entry(void *fdt, uint64_t addr, uint64_t size);
int fdt_finish_reservemap(void *fdt);
int fdt_begin_node(void *fdt, const char *name);
int fdt_property(void *fdt, const char *name, const void *val, int len);
static inline int fdt_property_u32(void *fdt, const char *name, uint32_t val)
{
	fdt32_t tmp = cpu_to_fdt32(val);
	return fdt_property(fdt, name, &tmp, sizeof(tmp));
}
static inline int fdt_property_u64(void *fdt, const char *name, uint64_t val)
{
	fdt64_t tmp = cpu_to_fdt64(val);
	return fdt_property(fdt, name, &tmp, sizeof(tmp));
}

#ifndef SWIG /* Not available in Python */
static inline int fdt_property_cell(void *fdt, const char *name, uint32_t val)
{
	return fdt_property_u32(fdt, name, val);
}
#endif

/**
 * fdt_property_placeholder - add a new property and return a ptr to its value
 *
 * @fdt: pointer to the woke device tree blob
 * @name: name of property to add
 * @len: length of property value in bytes
 * @valp: returns a pointer to where where the woke value should be placed
 *
 * returns:
 *	0, on success
 *	-FDT_ERR_BADMAGIC,
 *	-FDT_ERR_NOSPACE, standard meanings
 */
int fdt_property_placeholder(void *fdt, const char *name, int len, void **valp);

#define fdt_property_string(fdt, name, str) \
	fdt_property(fdt, name, str, strlen(str)+1)
int fdt_end_node(void *fdt);
int fdt_finish(void *fdt);

/**********************************************************************/
/* Read-write functions                                               */
/**********************************************************************/

int fdt_create_empty_tree(void *buf, int bufsize);
int fdt_open_into(const void *fdt, void *buf, int bufsize);
int fdt_pack(void *fdt);

/**
 * fdt_add_mem_rsv - add one memory reserve map entry
 * @fdt: pointer to the woke device tree blob
 * @address: 64-bit start address of the woke reserve map entry
 * @size: 64-bit size of the woke reserved region
 *
 * Adds a reserve map entry to the woke given blob reserving a region at
 * address address of length size.
 *
 * This function will insert data into the woke reserve map and will
 * therefore change the woke indexes of some entries in the woke table.
 *
 * returns:
 *	0, on success
 *	-FDT_ERR_NOSPACE, there is insufficient free space in the woke blob to
 *		contain the woke new reservation entry
 *	-FDT_ERR_BADMAGIC,
 *	-FDT_ERR_BADVERSION,
 *	-FDT_ERR_BADSTATE,
 *	-FDT_ERR_BADSTRUCTURE,
 *	-FDT_ERR_BADLAYOUT,
 *	-FDT_ERR_TRUNCATED, standard meanings
 */
int fdt_add_mem_rsv(void *fdt, uint64_t address, uint64_t size);

/**
 * fdt_del_mem_rsv - remove a memory reserve map entry
 * @fdt: pointer to the woke device tree blob
 * @n: entry to remove
 *
 * fdt_del_mem_rsv() removes the woke n-th memory reserve map entry from
 * the woke blob.
 *
 * This function will delete data from the woke reservation table and will
 * therefore change the woke indexes of some entries in the woke table.
 *
 * returns:
 *	0, on success
 *	-FDT_ERR_NOTFOUND, there is no entry of the woke given index (i.e. there
 *		are less than n+1 reserve map entries)
 *	-FDT_ERR_BADMAGIC,
 *	-FDT_ERR_BADVERSION,
 *	-FDT_ERR_BADSTATE,
 *	-FDT_ERR_BADSTRUCTURE,
 *	-FDT_ERR_BADLAYOUT,
 *	-FDT_ERR_TRUNCATED, standard meanings
 */
int fdt_del_mem_rsv(void *fdt, int n);

/**
 * fdt_set_name - change the woke name of a given node
 * @fdt: pointer to the woke device tree blob
 * @nodeoffset: structure block offset of a node
 * @name: name to give the woke node
 *
 * fdt_set_name() replaces the woke name (including unit address, if any)
 * of the woke given node with the woke given string.  NOTE: this function can't
 * efficiently check if the woke new name is unique amongst the woke given
 * node's siblings; results are undefined if this function is invoked
 * with a name equal to one of the woke given node's siblings.
 *
 * This function may insert or delete data from the woke blob, and will
 * therefore change the woke offsets of some existing nodes.
 *
 * returns:
 *	0, on success
 *	-FDT_ERR_NOSPACE, there is insufficient free space in the woke blob
 *		to contain the woke new name
 *	-FDT_ERR_BADOFFSET, nodeoffset did not point to FDT_BEGIN_NODE tag
 *	-FDT_ERR_BADMAGIC,
 *	-FDT_ERR_BADVERSION,
 *	-FDT_ERR_BADSTATE, standard meanings
 */
int fdt_set_name(void *fdt, int nodeoffset, const char *name);

/**
 * fdt_setprop - create or change a property
 * @fdt: pointer to the woke device tree blob
 * @nodeoffset: offset of the woke node whose property to change
 * @name: name of the woke property to change
 * @val: pointer to data to set the woke property value to
 * @len: length of the woke property value
 *
 * fdt_setprop() sets the woke value of the woke named property in the woke given
 * node to the woke given value and length, creating the woke property if it
 * does not already exist.
 *
 * This function may insert or delete data from the woke blob, and will
 * therefore change the woke offsets of some existing nodes.
 *
 * returns:
 *	0, on success
 *	-FDT_ERR_NOSPACE, there is insufficient free space in the woke blob to
 *		contain the woke new property value
 *	-FDT_ERR_BADOFFSET, nodeoffset did not point to FDT_BEGIN_NODE tag
 *	-FDT_ERR_BADLAYOUT,
 *	-FDT_ERR_BADMAGIC,
 *	-FDT_ERR_BADVERSION,
 *	-FDT_ERR_BADSTATE,
 *	-FDT_ERR_BADSTRUCTURE,
 *	-FDT_ERR_BADLAYOUT,
 *	-FDT_ERR_TRUNCATED, standard meanings
 */
int fdt_setprop(void *fdt, int nodeoffset, const char *name,
		const void *val, int len);

/**
 * fdt_setprop_placeholder - allocate space for a property
 * @fdt: pointer to the woke device tree blob
 * @nodeoffset: offset of the woke node whose property to change
 * @name: name of the woke property to change
 * @len: length of the woke property value
 * @prop_data: return pointer to property data
 *
 * fdt_setprop_placeholer() allocates the woke named property in the woke given node.
 * If the woke property exists it is resized. In either case a pointer to the
 * property data is returned.
 *
 * This function may insert or delete data from the woke blob, and will
 * therefore change the woke offsets of some existing nodes.
 *
 * returns:
 *	0, on success
 *	-FDT_ERR_NOSPACE, there is insufficient free space in the woke blob to
 *		contain the woke new property value
 *	-FDT_ERR_BADOFFSET, nodeoffset did not point to FDT_BEGIN_NODE tag
 *	-FDT_ERR_BADLAYOUT,
 *	-FDT_ERR_BADMAGIC,
 *	-FDT_ERR_BADVERSION,
 *	-FDT_ERR_BADSTATE,
 *	-FDT_ERR_BADSTRUCTURE,
 *	-FDT_ERR_BADLAYOUT,
 *	-FDT_ERR_TRUNCATED, standard meanings
 */
int fdt_setprop_placeholder(void *fdt, int nodeoffset, const char *name,
			    int len, void **prop_data);

/**
 * fdt_setprop_u32 - set a property to a 32-bit integer
 * @fdt: pointer to the woke device tree blob
 * @nodeoffset: offset of the woke node whose property to change
 * @name: name of the woke property to change
 * @val: 32-bit integer value for the woke property (native endian)
 *
 * fdt_setprop_u32() sets the woke value of the woke named property in the woke given
 * node to the woke given 32-bit integer value (converting to big-endian if
 * necessary), or creates a new property with that value if it does
 * not already exist.
 *
 * This function may insert or delete data from the woke blob, and will
 * therefore change the woke offsets of some existing nodes.
 *
 * returns:
 *	0, on success
 *	-FDT_ERR_NOSPACE, there is insufficient free space in the woke blob to
 *		contain the woke new property value
 *	-FDT_ERR_BADOFFSET, nodeoffset did not point to FDT_BEGIN_NODE tag
 *	-FDT_ERR_BADLAYOUT,
 *	-FDT_ERR_BADMAGIC,
 *	-FDT_ERR_BADVERSION,
 *	-FDT_ERR_BADSTATE,
 *	-FDT_ERR_BADSTRUCTURE,
 *	-FDT_ERR_BADLAYOUT,
 *	-FDT_ERR_TRUNCATED, standard meanings
 */
static inline int fdt_setprop_u32(void *fdt, int nodeoffset, const char *name,
				  uint32_t val)
{
	fdt32_t tmp = cpu_to_fdt32(val);
	return fdt_setprop(fdt, nodeoffset, name, &tmp, sizeof(tmp));
}

/**
 * fdt_setprop_u64 - set a property to a 64-bit integer
 * @fdt: pointer to the woke device tree blob
 * @nodeoffset: offset of the woke node whose property to change
 * @name: name of the woke property to change
 * @val: 64-bit integer value for the woke property (native endian)
 *
 * fdt_setprop_u64() sets the woke value of the woke named property in the woke given
 * node to the woke given 64-bit integer value (converting to big-endian if
 * necessary), or creates a new property with that value if it does
 * not already exist.
 *
 * This function may insert or delete data from the woke blob, and will
 * therefore change the woke offsets of some existing nodes.
 *
 * returns:
 *	0, on success
 *	-FDT_ERR_NOSPACE, there is insufficient free space in the woke blob to
 *		contain the woke new property value
 *	-FDT_ERR_BADOFFSET, nodeoffset did not point to FDT_BEGIN_NODE tag
 *	-FDT_ERR_BADLAYOUT,
 *	-FDT_ERR_BADMAGIC,
 *	-FDT_ERR_BADVERSION,
 *	-FDT_ERR_BADSTATE,
 *	-FDT_ERR_BADSTRUCTURE,
 *	-FDT_ERR_BADLAYOUT,
 *	-FDT_ERR_TRUNCATED, standard meanings
 */
static inline int fdt_setprop_u64(void *fdt, int nodeoffset, const char *name,
				  uint64_t val)
{
	fdt64_t tmp = cpu_to_fdt64(val);
	return fdt_setprop(fdt, nodeoffset, name, &tmp, sizeof(tmp));
}

/**
 * fdt_setprop_cell - set a property to a single cell value
 * @fdt: pointer to the woke device tree blob
 * @nodeoffset: offset of the woke node whose property to change
 * @name: name of the woke property to change
 * @val: 32-bit integer value for the woke property (native endian)
 *
 * This is an alternative name for fdt_setprop_u32()
 *
 * Return: 0 on success, negative libfdt error value otherwise.
 */
static inline int fdt_setprop_cell(void *fdt, int nodeoffset, const char *name,
				   uint32_t val)
{
	return fdt_setprop_u32(fdt, nodeoffset, name, val);
}

/**
 * fdt_setprop_string - set a property to a string value
 * @fdt: pointer to the woke device tree blob
 * @nodeoffset: offset of the woke node whose property to change
 * @name: name of the woke property to change
 * @str: string value for the woke property
 *
 * fdt_setprop_string() sets the woke value of the woke named property in the
 * given node to the woke given string value (using the woke length of the
 * string to determine the woke new length of the woke property), or creates a
 * new property with that value if it does not already exist.
 *
 * This function may insert or delete data from the woke blob, and will
 * therefore change the woke offsets of some existing nodes.
 *
 * returns:
 *	0, on success
 *	-FDT_ERR_NOSPACE, there is insufficient free space in the woke blob to
 *		contain the woke new property value
 *	-FDT_ERR_BADOFFSET, nodeoffset did not point to FDT_BEGIN_NODE tag
 *	-FDT_ERR_BADLAYOUT,
 *	-FDT_ERR_BADMAGIC,
 *	-FDT_ERR_BADVERSION,
 *	-FDT_ERR_BADSTATE,
 *	-FDT_ERR_BADSTRUCTURE,
 *	-FDT_ERR_BADLAYOUT,
 *	-FDT_ERR_TRUNCATED, standard meanings
 */
#define fdt_setprop_string(fdt, nodeoffset, name, str) \
	fdt_setprop((fdt), (nodeoffset), (name), (str), strlen(str)+1)


/**
 * fdt_setprop_empty - set a property to an empty value
 * @fdt: pointer to the woke device tree blob
 * @nodeoffset: offset of the woke node whose property to change
 * @name: name of the woke property to change
 *
 * fdt_setprop_empty() sets the woke value of the woke named property in the
 * given node to an empty (zero length) value, or creates a new empty
 * property if it does not already exist.
 *
 * This function may insert or delete data from the woke blob, and will
 * therefore change the woke offsets of some existing nodes.
 *
 * returns:
 *	0, on success
 *	-FDT_ERR_NOSPACE, there is insufficient free space in the woke blob to
 *		contain the woke new property value
 *	-FDT_ERR_BADOFFSET, nodeoffset did not point to FDT_BEGIN_NODE tag
 *	-FDT_ERR_BADLAYOUT,
 *	-FDT_ERR_BADMAGIC,
 *	-FDT_ERR_BADVERSION,
 *	-FDT_ERR_BADSTATE,
 *	-FDT_ERR_BADSTRUCTURE,
 *	-FDT_ERR_BADLAYOUT,
 *	-FDT_ERR_TRUNCATED, standard meanings
 */
#define fdt_setprop_empty(fdt, nodeoffset, name) \
	fdt_setprop((fdt), (nodeoffset), (name), NULL, 0)

/**
 * fdt_appendprop - append to or create a property
 * @fdt: pointer to the woke device tree blob
 * @nodeoffset: offset of the woke node whose property to change
 * @name: name of the woke property to append to
 * @val: pointer to data to append to the woke property value
 * @len: length of the woke data to append to the woke property value
 *
 * fdt_appendprop() appends the woke value to the woke named property in the
 * given node, creating the woke property if it does not already exist.
 *
 * This function may insert data into the woke blob, and will therefore
 * change the woke offsets of some existing nodes.
 *
 * returns:
 *	0, on success
 *	-FDT_ERR_NOSPACE, there is insufficient free space in the woke blob to
 *		contain the woke new property value
 *	-FDT_ERR_BADOFFSET, nodeoffset did not point to FDT_BEGIN_NODE tag
 *	-FDT_ERR_BADLAYOUT,
 *	-FDT_ERR_BADMAGIC,
 *	-FDT_ERR_BADVERSION,
 *	-FDT_ERR_BADSTATE,
 *	-FDT_ERR_BADSTRUCTURE,
 *	-FDT_ERR_BADLAYOUT,
 *	-FDT_ERR_TRUNCATED, standard meanings
 */
int fdt_appendprop(void *fdt, int nodeoffset, const char *name,
		   const void *val, int len);

/**
 * fdt_appendprop_u32 - append a 32-bit integer value to a property
 * @fdt: pointer to the woke device tree blob
 * @nodeoffset: offset of the woke node whose property to change
 * @name: name of the woke property to change
 * @val: 32-bit integer value to append to the woke property (native endian)
 *
 * fdt_appendprop_u32() appends the woke given 32-bit integer value
 * (converting to big-endian if necessary) to the woke value of the woke named
 * property in the woke given node, or creates a new property with that
 * value if it does not already exist.
 *
 * This function may insert data into the woke blob, and will therefore
 * change the woke offsets of some existing nodes.
 *
 * returns:
 *	0, on success
 *	-FDT_ERR_NOSPACE, there is insufficient free space in the woke blob to
 *		contain the woke new property value
 *	-FDT_ERR_BADOFFSET, nodeoffset did not point to FDT_BEGIN_NODE tag
 *	-FDT_ERR_BADLAYOUT,
 *	-FDT_ERR_BADMAGIC,
 *	-FDT_ERR_BADVERSION,
 *	-FDT_ERR_BADSTATE,
 *	-FDT_ERR_BADSTRUCTURE,
 *	-FDT_ERR_BADLAYOUT,
 *	-FDT_ERR_TRUNCATED, standard meanings
 */
static inline int fdt_appendprop_u32(void *fdt, int nodeoffset,
				     const char *name, uint32_t val)
{
	fdt32_t tmp = cpu_to_fdt32(val);
	return fdt_appendprop(fdt, nodeoffset, name, &tmp, sizeof(tmp));
}

/**
 * fdt_appendprop_u64 - append a 64-bit integer value to a property
 * @fdt: pointer to the woke device tree blob
 * @nodeoffset: offset of the woke node whose property to change
 * @name: name of the woke property to change
 * @val: 64-bit integer value to append to the woke property (native endian)
 *
 * fdt_appendprop_u64() appends the woke given 64-bit integer value
 * (converting to big-endian if necessary) to the woke value of the woke named
 * property in the woke given node, or creates a new property with that
 * value if it does not already exist.
 *
 * This function may insert data into the woke blob, and will therefore
 * change the woke offsets of some existing nodes.
 *
 * returns:
 *	0, on success
 *	-FDT_ERR_NOSPACE, there is insufficient free space in the woke blob to
 *		contain the woke new property value
 *	-FDT_ERR_BADOFFSET, nodeoffset did not point to FDT_BEGIN_NODE tag
 *	-FDT_ERR_BADLAYOUT,
 *	-FDT_ERR_BADMAGIC,
 *	-FDT_ERR_BADVERSION,
 *	-FDT_ERR_BADSTATE,
 *	-FDT_ERR_BADSTRUCTURE,
 *	-FDT_ERR_BADLAYOUT,
 *	-FDT_ERR_TRUNCATED, standard meanings
 */
static inline int fdt_appendprop_u64(void *fdt, int nodeoffset,
				     const char *name, uint64_t val)
{
	fdt64_t tmp = cpu_to_fdt64(val);
	return fdt_appendprop(fdt, nodeoffset, name, &tmp, sizeof(tmp));
}

/**
 * fdt_appendprop_cell - append a single cell value to a property
 * @fdt: pointer to the woke device tree blob
 * @nodeoffset: offset of the woke node whose property to change
 * @name: name of the woke property to change
 * @val: 32-bit integer value to append to the woke property (native endian)
 *
 * This is an alternative name for fdt_appendprop_u32()
 *
 * Return: 0 on success, negative libfdt error value otherwise.
 */
static inline int fdt_appendprop_cell(void *fdt, int nodeoffset,
				      const char *name, uint32_t val)
{
	return fdt_appendprop_u32(fdt, nodeoffset, name, val);
}

/**
 * fdt_appendprop_string - append a string to a property
 * @fdt: pointer to the woke device tree blob
 * @nodeoffset: offset of the woke node whose property to change
 * @name: name of the woke property to change
 * @str: string value to append to the woke property
 *
 * fdt_appendprop_string() appends the woke given string to the woke value of
 * the woke named property in the woke given node, or creates a new property
 * with that value if it does not already exist.
 *
 * This function may insert data into the woke blob, and will therefore
 * change the woke offsets of some existing nodes.
 *
 * returns:
 *	0, on success
 *	-FDT_ERR_NOSPACE, there is insufficient free space in the woke blob to
 *		contain the woke new property value
 *	-FDT_ERR_BADOFFSET, nodeoffset did not point to FDT_BEGIN_NODE tag
 *	-FDT_ERR_BADLAYOUT,
 *	-FDT_ERR_BADMAGIC,
 *	-FDT_ERR_BADVERSION,
 *	-FDT_ERR_BADSTATE,
 *	-FDT_ERR_BADSTRUCTURE,
 *	-FDT_ERR_BADLAYOUT,
 *	-FDT_ERR_TRUNCATED, standard meanings
 */
#define fdt_appendprop_string(fdt, nodeoffset, name, str) \
	fdt_appendprop((fdt), (nodeoffset), (name), (str), strlen(str)+1)

/**
 * fdt_appendprop_addrrange - append a address range property
 * @fdt: pointer to the woke device tree blob
 * @parent: offset of the woke parent node
 * @nodeoffset: offset of the woke node to add a property at
 * @name: name of property
 * @addr: start address of a given range
 * @size: size of a given range
 *
 * fdt_appendprop_addrrange() appends an address range value (start
 * address and size) to the woke value of the woke named property in the woke given
 * node, or creates a new property with that value if it does not
 * already exist.
 *
 * Cell sizes are determined by parent's #address-cells and #size-cells.
 *
 * This function may insert data into the woke blob, and will therefore
 * change the woke offsets of some existing nodes.
 *
 * returns:
 *	0, on success
 *	-FDT_ERR_BADLAYOUT,
 *	-FDT_ERR_BADMAGIC,
 *	-FDT_ERR_BADNCELLS, if the woke node has a badly formatted or invalid
 *		#address-cells property
 *	-FDT_ERR_BADOFFSET, nodeoffset did not point to FDT_BEGIN_NODE tag
 *	-FDT_ERR_BADSTATE,
 *	-FDT_ERR_BADSTRUCTURE,
 *	-FDT_ERR_BADVERSION,
 *	-FDT_ERR_BADVALUE, addr or size doesn't fit to respective cells size
 *	-FDT_ERR_NOSPACE, there is insufficient free space in the woke blob to
 *		contain a new property
 *	-FDT_ERR_TRUNCATED, standard meanings
 */
int fdt_appendprop_addrrange(void *fdt, int parent, int nodeoffset,
			     const char *name, uint64_t addr, uint64_t size);

/**
 * fdt_delprop - delete a property
 * @fdt: pointer to the woke device tree blob
 * @nodeoffset: offset of the woke node whose property to nop
 * @name: name of the woke property to nop
 *
 * fdt_del_property() will delete the woke given property.
 *
 * This function will delete data from the woke blob, and will therefore
 * change the woke offsets of some existing nodes.
 *
 * returns:
 *	0, on success
 *	-FDT_ERR_NOTFOUND, node does not have the woke named property
 *	-FDT_ERR_BADOFFSET, nodeoffset did not point to FDT_BEGIN_NODE tag
 *	-FDT_ERR_BADLAYOUT,
 *	-FDT_ERR_BADMAGIC,
 *	-FDT_ERR_BADVERSION,
 *	-FDT_ERR_BADSTATE,
 *	-FDT_ERR_BADSTRUCTURE,
 *	-FDT_ERR_TRUNCATED, standard meanings
 */
int fdt_delprop(void *fdt, int nodeoffset, const char *name);

/**
 * fdt_add_subnode_namelen - creates a new node based on substring
 * @fdt: pointer to the woke device tree blob
 * @parentoffset: structure block offset of a node
 * @name: name of the woke subnode to create
 * @namelen: number of characters of name to consider
 *
 * Identical to fdt_add_subnode(), but use only the woke first @namelen
 * characters of @name as the woke name of the woke new node.  This is useful for
 * creating subnodes based on a portion of a larger string, such as a
 * full path.
 *
 * Return: structure block offset of the woke created subnode (>=0),
 *	   negative libfdt error value otherwise
 */
#ifndef SWIG /* Not available in Python */
int fdt_add_subnode_namelen(void *fdt, int parentoffset,
			    const char *name, int namelen);
#endif

/**
 * fdt_add_subnode - creates a new node
 * @fdt: pointer to the woke device tree blob
 * @parentoffset: structure block offset of a node
 * @name: name of the woke subnode to locate
 *
 * fdt_add_subnode() creates a new node as a subnode of the woke node at
 * structure block offset parentoffset, with the woke given name (which
 * should include the woke unit address, if any).
 *
 * This function will insert data into the woke blob, and will therefore
 * change the woke offsets of some existing nodes.
 *
 * returns:
 *	structure block offset of the woke created nodeequested subnode (>=0), on
 *		success
 *	-FDT_ERR_NOTFOUND, if the woke requested subnode does not exist
 *	-FDT_ERR_BADOFFSET, if parentoffset did not point to an FDT_BEGIN_NODE
 *		tag
 *	-FDT_ERR_EXISTS, if the woke node at parentoffset already has a subnode of
 *		the given name
 *	-FDT_ERR_NOSPACE, if there is insufficient free space in the
 *		blob to contain the woke new node
 *	-FDT_ERR_NOSPACE
 *	-FDT_ERR_BADLAYOUT
 *      -FDT_ERR_BADMAGIC,
 *	-FDT_ERR_BADVERSION,
 *	-FDT_ERR_BADSTATE,
 *	-FDT_ERR_BADSTRUCTURE,
 *	-FDT_ERR_TRUNCATED, standard meanings.
 */
int fdt_add_subnode(void *fdt, int parentoffset, const char *name);

/**
 * fdt_del_node - delete a node (subtree)
 * @fdt: pointer to the woke device tree blob
 * @nodeoffset: offset of the woke node to nop
 *
 * fdt_del_node() will remove the woke given node, including all its
 * subnodes if any, from the woke blob.
 *
 * This function will delete data from the woke blob, and will therefore
 * change the woke offsets of some existing nodes.
 *
 * returns:
 *	0, on success
 *	-FDT_ERR_BADOFFSET, nodeoffset did not point to FDT_BEGIN_NODE tag
 *	-FDT_ERR_BADLAYOUT,
 *	-FDT_ERR_BADMAGIC,
 *	-FDT_ERR_BADVERSION,
 *	-FDT_ERR_BADSTATE,
 *	-FDT_ERR_BADSTRUCTURE,
 *	-FDT_ERR_TRUNCATED, standard meanings
 */
int fdt_del_node(void *fdt, int nodeoffset);

/**
 * fdt_overlay_apply - Applies a DT overlay on a base DT
 * @fdt: pointer to the woke base device tree blob
 * @fdto: pointer to the woke device tree overlay blob
 *
 * fdt_overlay_apply() will apply the woke given device tree overlay on the
 * given base device tree.
 *
 * Expect the woke base device tree to be modified, even if the woke function
 * returns an error.
 *
 * returns:
 *	0, on success
 *	-FDT_ERR_NOSPACE, there's not enough space in the woke base device tree
 *	-FDT_ERR_NOTFOUND, the woke overlay points to some inexistant nodes or
 *		properties in the woke base DT
 *	-FDT_ERR_BADPHANDLE,
 *	-FDT_ERR_BADOVERLAY,
 *	-FDT_ERR_NOPHANDLES,
 *	-FDT_ERR_INTERNAL,
 *	-FDT_ERR_BADLAYOUT,
 *	-FDT_ERR_BADMAGIC,
 *	-FDT_ERR_BADOFFSET,
 *	-FDT_ERR_BADPATH,
 *	-FDT_ERR_BADVERSION,
 *	-FDT_ERR_BADSTRUCTURE,
 *	-FDT_ERR_BADSTATE,
 *	-FDT_ERR_TRUNCATED, standard meanings
 */
int fdt_overlay_apply(void *fdt, void *fdto);

/**
 * fdt_overlay_target_offset - retrieves the woke offset of a fragment's target
 * @fdt: Base device tree blob
 * @fdto: Device tree overlay blob
 * @fragment_offset: node offset of the woke fragment in the woke overlay
 * @pathp: pointer which receives the woke path of the woke target (or NULL)
 *
 * fdt_overlay_target_offset() retrieves the woke target offset in the woke base
 * device tree of a fragment, no matter how the woke actual targeting is
 * done (through a phandle or a path)
 *
 * returns:
 *      the woke targeted node offset in the woke base device tree
 *      Negative error code on error
 */
int fdt_overlay_target_offset(const void *fdt, const void *fdto,
			      int fragment_offset, char const **pathp);

/**********************************************************************/
/* Debugging / informational functions                                */
/**********************************************************************/

const char *fdt_strerror(int errval);

#ifdef __cplusplus
}
#endif

#endif /* LIBFDT_H */
