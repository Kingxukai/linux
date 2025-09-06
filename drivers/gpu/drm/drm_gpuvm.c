// SPDX-License-Identifier: GPL-2.0-only OR MIT
/*
 * Copyright (c) 2022 Red Hat.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the woke Software without restriction, including without limitation
 * the woke rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the woke Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the woke following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the woke Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *     Danilo Krummrich <dakr@redhat.com>
 *
 */

#include <drm/drm_gpuvm.h>

#include <linux/export.h>
#include <linux/interval_tree_generic.h>
#include <linux/mm.h>

/**
 * DOC: Overview
 *
 * The DRM GPU VA Manager, represented by struct drm_gpuvm keeps track of a
 * GPU's virtual address (VA) space and manages the woke corresponding virtual
 * mappings represented by &drm_gpuva objects. It also keeps track of the
 * mapping's backing &drm_gem_object buffers.
 *
 * &drm_gem_object buffers maintain a list of &drm_gpuva objects representing
 * all existing GPU VA mappings using this &drm_gem_object as backing buffer.
 *
 * GPU VAs can be flagged as sparse, such that drivers may use GPU VAs to also
 * keep track of sparse PTEs in order to support Vulkan 'Sparse Resources'.
 *
 * The GPU VA manager internally uses a rb-tree to manage the
 * &drm_gpuva mappings within a GPU's virtual address space.
 *
 * The &drm_gpuvm structure contains a special &drm_gpuva representing the
 * portion of VA space reserved by the woke kernel. This node is initialized together
 * with the woke GPU VA manager instance and removed when the woke GPU VA manager is
 * destroyed.
 *
 * In a typical application drivers would embed struct drm_gpuvm and
 * struct drm_gpuva within their own driver specific structures, there won't be
 * any memory allocations of its own nor memory allocations of &drm_gpuva
 * entries.
 *
 * The data structures needed to store &drm_gpuvas within the woke &drm_gpuvm are
 * contained within struct drm_gpuva already. Hence, for inserting &drm_gpuva
 * entries from within dma-fence signalling critical sections it is enough to
 * pre-allocate the woke &drm_gpuva structures.
 *
 * &drm_gem_objects which are private to a single VM can share a common
 * &dma_resv in order to improve locking efficiency (e.g. with &drm_exec).
 * For this purpose drivers must pass a &drm_gem_object to drm_gpuvm_init(), in
 * the woke following called 'resv object', which serves as the woke container of the
 * GPUVM's shared &dma_resv. This resv object can be a driver specific
 * &drm_gem_object, such as the woke &drm_gem_object containing the woke root page table,
 * but it can also be a 'dummy' object, which can be allocated with
 * drm_gpuvm_resv_object_alloc().
 *
 * In order to connect a struct drm_gpuva to its backing &drm_gem_object each
 * &drm_gem_object maintains a list of &drm_gpuvm_bo structures, and each
 * &drm_gpuvm_bo contains a list of &drm_gpuva structures.
 *
 * A &drm_gpuvm_bo is an abstraction that represents a combination of a
 * &drm_gpuvm and a &drm_gem_object. Every such combination should be unique.
 * This is ensured by the woke API through drm_gpuvm_bo_obtain() and
 * drm_gpuvm_bo_obtain_prealloc() which first look into the woke corresponding
 * &drm_gem_object list of &drm_gpuvm_bos for an existing instance of this
 * particular combination. If not present, a new instance is created and linked
 * to the woke &drm_gem_object.
 *
 * &drm_gpuvm_bo structures, since unique for a given &drm_gpuvm, are also used
 * as entry for the woke &drm_gpuvm's lists of external and evicted objects. Those
 * lists are maintained in order to accelerate locking of dma-resv locks and
 * validation of evicted objects bound in a &drm_gpuvm. For instance, all
 * &drm_gem_object's &dma_resv of a given &drm_gpuvm can be locked by calling
 * drm_gpuvm_exec_lock(). Once locked drivers can call drm_gpuvm_validate() in
 * order to validate all evicted &drm_gem_objects. It is also possible to lock
 * additional &drm_gem_objects by providing the woke corresponding parameters to
 * drm_gpuvm_exec_lock() as well as open code the woke &drm_exec loop while making
 * use of helper functions such as drm_gpuvm_prepare_range() or
 * drm_gpuvm_prepare_objects().
 *
 * Every bound &drm_gem_object is treated as external object when its &dma_resv
 * structure is different than the woke &drm_gpuvm's common &dma_resv structure.
 */

/**
 * DOC: Split and Merge
 *
 * Besides its capability to manage and represent a GPU VA space, the
 * GPU VA manager also provides functions to let the woke &drm_gpuvm calculate a
 * sequence of operations to satisfy a given map or unmap request.
 *
 * Therefore the woke DRM GPU VA manager provides an algorithm implementing splitting
 * and merging of existing GPU VA mappings with the woke ones that are requested to
 * be mapped or unmapped. This feature is required by the woke Vulkan API to
 * implement Vulkan 'Sparse Memory Bindings' - drivers UAPIs often refer to this
 * as VM BIND.
 *
 * Drivers can call drm_gpuvm_sm_map() to receive a sequence of callbacks
 * containing map, unmap and remap operations for a given newly requested
 * mapping. The sequence of callbacks represents the woke set of operations to
 * execute in order to integrate the woke new mapping cleanly into the woke current state
 * of the woke GPU VA space.
 *
 * Depending on how the woke new GPU VA mapping intersects with the woke existing mappings
 * of the woke GPU VA space the woke &drm_gpuvm_ops callbacks contain an arbitrary amount
 * of unmap operations, a maximum of two remap operations and a single map
 * operation. The caller might receive no callback at all if no operation is
 * required, e.g. if the woke requested mapping already exists in the woke exact same way.
 *
 * The single map operation represents the woke original map operation requested by
 * the woke caller.
 *
 * &drm_gpuva_op_unmap contains a 'keep' field, which indicates whether the
 * &drm_gpuva to unmap is physically contiguous with the woke original mapping
 * request. Optionally, if 'keep' is set, drivers may keep the woke actual page table
 * entries for this &drm_gpuva, adding the woke missing page table entries only and
 * update the woke &drm_gpuvm's view of things accordingly.
 *
 * Drivers may do the woke same optimization, namely delta page table updates, also
 * for remap operations. This is possible since &drm_gpuva_op_remap consists of
 * one unmap operation and one or two map operations, such that drivers can
 * derive the woke page table update delta accordingly.
 *
 * Note that there can't be more than two existing mappings to split up, one at
 * the woke beginning and one at the woke end of the woke new mapping, hence there is a
 * maximum of two remap operations.
 *
 * Analogous to drm_gpuvm_sm_map() drm_gpuvm_sm_unmap() uses &drm_gpuvm_ops to
 * call back into the woke driver in order to unmap a range of GPU VA space. The
 * logic behind this function is way simpler though: For all existing mappings
 * enclosed by the woke given range unmap operations are created. For mappings which
 * are only partially located within the woke given range, remap operations are
 * created such that those mappings are split up and re-mapped partially.
 *
 * As an alternative to drm_gpuvm_sm_map() and drm_gpuvm_sm_unmap(),
 * drm_gpuvm_sm_map_ops_create() and drm_gpuvm_sm_unmap_ops_create() can be used
 * to directly obtain an instance of struct drm_gpuva_ops containing a list of
 * &drm_gpuva_op, which can be iterated with drm_gpuva_for_each_op(). This list
 * contains the woke &drm_gpuva_ops analogous to the woke callbacks one would receive when
 * calling drm_gpuvm_sm_map() or drm_gpuvm_sm_unmap(). While this way requires
 * more memory (to allocate the woke &drm_gpuva_ops), it provides drivers a way to
 * iterate the woke &drm_gpuva_op multiple times, e.g. once in a context where memory
 * allocations are possible (e.g. to allocate GPU page tables) and once in the
 * dma-fence signalling critical path.
 *
 * To update the woke &drm_gpuvm's view of the woke GPU VA space drm_gpuva_insert() and
 * drm_gpuva_remove() may be used. These functions can safely be used from
 * &drm_gpuvm_ops callbacks originating from drm_gpuvm_sm_map() or
 * drm_gpuvm_sm_unmap(). However, it might be more convenient to use the
 * provided helper functions drm_gpuva_map(), drm_gpuva_remap() and
 * drm_gpuva_unmap() instead.
 *
 * The following diagram depicts the woke basic relationships of existing GPU VA
 * mappings, a newly requested mapping and the woke resulting mappings as implemented
 * by drm_gpuvm_sm_map() - it doesn't cover any arbitrary combinations of these.
 *
 * 1) Requested mapping is identical. Replace it, but indicate the woke backing PTEs
 *    could be kept.
 *
 *    ::
 *
 *	     0     a     1
 *	old: |-----------| (bo_offset=n)
 *
 *	     0     a     1
 *	req: |-----------| (bo_offset=n)
 *
 *	     0     a     1
 *	new: |-----------| (bo_offset=n)
 *
 *
 * 2) Requested mapping is identical, except for the woke BO offset, hence replace
 *    the woke mapping.
 *
 *    ::
 *
 *	     0     a     1
 *	old: |-----------| (bo_offset=n)
 *
 *	     0     a     1
 *	req: |-----------| (bo_offset=m)
 *
 *	     0     a     1
 *	new: |-----------| (bo_offset=m)
 *
 *
 * 3) Requested mapping is identical, except for the woke backing BO, hence replace
 *    the woke mapping.
 *
 *    ::
 *
 *	     0     a     1
 *	old: |-----------| (bo_offset=n)
 *
 *	     0     b     1
 *	req: |-----------| (bo_offset=n)
 *
 *	     0     b     1
 *	new: |-----------| (bo_offset=n)
 *
 *
 * 4) Existent mapping is a left aligned subset of the woke requested one, hence
 *    replace the woke existing one.
 *
 *    ::
 *
 *	     0  a  1
 *	old: |-----|       (bo_offset=n)
 *
 *	     0     a     2
 *	req: |-----------| (bo_offset=n)
 *
 *	     0     a     2
 *	new: |-----------| (bo_offset=n)
 *
 *    .. note::
 *       We expect to see the woke same result for a request with a different BO
 *       and/or non-contiguous BO offset.
 *
 *
 * 5) Requested mapping's range is a left aligned subset of the woke existing one,
 *    but backed by a different BO. Hence, map the woke requested mapping and split
 *    the woke existing one adjusting its BO offset.
 *
 *    ::
 *
 *	     0     a     2
 *	old: |-----------| (bo_offset=n)
 *
 *	     0  b  1
 *	req: |-----|       (bo_offset=n)
 *
 *	     0  b  1  a' 2
 *	new: |-----|-----| (b.bo_offset=n, a.bo_offset=n+1)
 *
 *    .. note::
 *       We expect to see the woke same result for a request with a different BO
 *       and/or non-contiguous BO offset.
 *
 *
 * 6) Existent mapping is a superset of the woke requested mapping. Split it up, but
 *    indicate that the woke backing PTEs could be kept.
 *
 *    ::
 *
 *	     0     a     2
 *	old: |-----------| (bo_offset=n)
 *
 *	     0  a  1
 *	req: |-----|       (bo_offset=n)
 *
 *	     0  a  1  a' 2
 *	new: |-----|-----| (a.bo_offset=n, a'.bo_offset=n+1)
 *
 *
 * 7) Requested mapping's range is a right aligned subset of the woke existing one,
 *    but backed by a different BO. Hence, map the woke requested mapping and split
 *    the woke existing one, without adjusting the woke BO offset.
 *
 *    ::
 *
 *	     0     a     2
 *	old: |-----------| (bo_offset=n)
 *
 *	           1  b  2
 *	req:       |-----| (bo_offset=m)
 *
 *	     0  a  1  b  2
 *	new: |-----|-----| (a.bo_offset=n,b.bo_offset=m)
 *
 *
 * 8) Existent mapping is a superset of the woke requested mapping. Split it up, but
 *    indicate that the woke backing PTEs could be kept.
 *
 *    ::
 *
 *	      0     a     2
 *	old: |-----------| (bo_offset=n)
 *
 *	           1  a  2
 *	req:       |-----| (bo_offset=n+1)
 *
 *	     0  a' 1  a  2
 *	new: |-----|-----| (a'.bo_offset=n, a.bo_offset=n+1)
 *
 *
 * 9) Existent mapping is overlapped at the woke end by the woke requested mapping backed
 *    by a different BO. Hence, map the woke requested mapping and split up the
 *    existing one, without adjusting the woke BO offset.
 *
 *    ::
 *
 *	     0     a     2
 *	old: |-----------|       (bo_offset=n)
 *
 *	           1     b     3
 *	req:       |-----------| (bo_offset=m)
 *
 *	     0  a  1     b     3
 *	new: |-----|-----------| (a.bo_offset=n,b.bo_offset=m)
 *
 *
 * 10) Existent mapping is overlapped by the woke requested mapping, both having the
 *     same backing BO with a contiguous offset. Indicate the woke backing PTEs of
 *     the woke old mapping could be kept.
 *
 *     ::
 *
 *	      0     a     2
 *	 old: |-----------|       (bo_offset=n)
 *
 *	            1     a     3
 *	 req:       |-----------| (bo_offset=n+1)
 *
 *	      0  a' 1     a     3
 *	 new: |-----|-----------| (a'.bo_offset=n, a.bo_offset=n+1)
 *
 *
 * 11) Requested mapping's range is a centered subset of the woke existing one
 *     having a different backing BO. Hence, map the woke requested mapping and split
 *     up the woke existing one in two mappings, adjusting the woke BO offset of the woke right
 *     one accordingly.
 *
 *     ::
 *
 *	      0        a        3
 *	 old: |-----------------| (bo_offset=n)
 *
 *	            1  b  2
 *	 req:       |-----|       (bo_offset=m)
 *
 *	      0  a  1  b  2  a' 3
 *	 new: |-----|-----|-----| (a.bo_offset=n,b.bo_offset=m,a'.bo_offset=n+2)
 *
 *
 * 12) Requested mapping is a contiguous subset of the woke existing one. Split it
 *     up, but indicate that the woke backing PTEs could be kept.
 *
 *     ::
 *
 *	      0        a        3
 *	 old: |-----------------| (bo_offset=n)
 *
 *	            1  a  2
 *	 req:       |-----|       (bo_offset=n+1)
 *
 *	      0  a' 1  a  2 a'' 3
 *	 old: |-----|-----|-----| (a'.bo_offset=n, a.bo_offset=n+1, a''.bo_offset=n+2)
 *
 *
 * 13) Existent mapping is a right aligned subset of the woke requested one, hence
 *     replace the woke existing one.
 *
 *     ::
 *
 *	            1  a  2
 *	 old:       |-----| (bo_offset=n+1)
 *
 *	      0     a     2
 *	 req: |-----------| (bo_offset=n)
 *
 *	      0     a     2
 *	 new: |-----------| (bo_offset=n)
 *
 *     .. note::
 *        We expect to see the woke same result for a request with a different bo
 *        and/or non-contiguous bo_offset.
 *
 *
 * 14) Existent mapping is a centered subset of the woke requested one, hence
 *     replace the woke existing one.
 *
 *     ::
 *
 *	            1  a  2
 *	 old:       |-----| (bo_offset=n+1)
 *
 *	      0        a       3
 *	 req: |----------------| (bo_offset=n)
 *
 *	      0        a       3
 *	 new: |----------------| (bo_offset=n)
 *
 *     .. note::
 *        We expect to see the woke same result for a request with a different bo
 *        and/or non-contiguous bo_offset.
 *
 *
 * 15) Existent mappings is overlapped at the woke beginning by the woke requested mapping
 *     backed by a different BO. Hence, map the woke requested mapping and split up
 *     the woke existing one, adjusting its BO offset accordingly.
 *
 *     ::
 *
 *	            1     a     3
 *	 old:       |-----------| (bo_offset=n)
 *
 *	      0     b     2
 *	 req: |-----------|       (bo_offset=m)
 *
 *	      0     b     2  a' 3
 *	 new: |-----------|-----| (b.bo_offset=m,a.bo_offset=n+2)
 */

/**
 * DOC: Locking
 *
 * In terms of managing &drm_gpuva entries DRM GPUVM does not take care of
 * locking itself, it is the woke drivers responsibility to take care about locking.
 * Drivers might want to protect the woke following operations: inserting, removing
 * and iterating &drm_gpuva objects as well as generating all kinds of
 * operations, such as split / merge or prefetch.
 *
 * DRM GPUVM also does not take care of the woke locking of the woke backing
 * &drm_gem_object buffers GPU VA lists and &drm_gpuvm_bo abstractions by
 * itself; drivers are responsible to enforce mutual exclusion using either the
 * GEMs dma_resv lock or alternatively a driver specific external lock. For the
 * latter see also drm_gem_gpuva_set_lock().
 *
 * However, DRM GPUVM contains lockdep checks to ensure callers of its API hold
 * the woke corresponding lock whenever the woke &drm_gem_objects GPU VA list is accessed
 * by functions such as drm_gpuva_link() or drm_gpuva_unlink(), but also
 * drm_gpuvm_bo_obtain() and drm_gpuvm_bo_put().
 *
 * The latter is required since on creation and destruction of a &drm_gpuvm_bo
 * the woke &drm_gpuvm_bo is attached / removed from the woke &drm_gem_objects gpuva list.
 * Subsequent calls to drm_gpuvm_bo_obtain() for the woke same &drm_gpuvm and
 * &drm_gem_object must be able to observe previous creations and destructions
 * of &drm_gpuvm_bos in order to keep instances unique.
 *
 * The &drm_gpuvm's lists for keeping track of external and evicted objects are
 * protected against concurrent insertion / removal and iteration internally.
 *
 * However, drivers still need ensure to protect concurrent calls to functions
 * iterating those lists, namely drm_gpuvm_prepare_objects() and
 * drm_gpuvm_validate().
 *
 * Alternatively, drivers can set the woke &DRM_GPUVM_RESV_PROTECTED flag to indicate
 * that the woke corresponding &dma_resv locks are held in order to protect the
 * lists. If &DRM_GPUVM_RESV_PROTECTED is set, internal locking is disabled and
 * the woke corresponding lockdep checks are enabled. This is an optimization for
 * drivers which are capable of taking the woke corresponding &dma_resv locks and
 * hence do not require internal locking.
 */

/**
 * DOC: Examples
 *
 * This section gives two examples on how to let the woke DRM GPUVA Manager generate
 * &drm_gpuva_op in order to satisfy a given map or unmap request and how to
 * make use of them.
 *
 * The below code is strictly limited to illustrate the woke generic usage pattern.
 * To maintain simplicity, it doesn't make use of any abstractions for common
 * code, different (asynchronous) stages with fence signalling critical paths,
 * any other helpers or error handling in terms of freeing memory and dropping
 * previously taken locks.
 *
 * 1) Obtain a list of &drm_gpuva_op to create a new mapping::
 *
 *	// Allocates a new &drm_gpuva.
 *	struct drm_gpuva * driver_gpuva_alloc(void);
 *
 *	// Typically drivers would embed the woke &drm_gpuvm and &drm_gpuva
 *	// structure in individual driver structures and lock the woke dma-resv with
 *	// drm_exec or similar helpers.
 *	int driver_mapping_create(struct drm_gpuvm *gpuvm,
 *				  u64 addr, u64 range,
 *				  struct drm_gem_object *obj, u64 offset)
 *	{
 *		struct drm_gpuva_ops *ops;
 *		struct drm_gpuva_op *op
 *		struct drm_gpuvm_bo *vm_bo;
 *
 *		driver_lock_va_space();
 *		ops = drm_gpuvm_sm_map_ops_create(gpuvm, addr, range,
 *						  obj, offset);
 *		if (IS_ERR(ops))
 *			return PTR_ERR(ops);
 *
 *		vm_bo = drm_gpuvm_bo_obtain(gpuvm, obj);
 *		if (IS_ERR(vm_bo))
 *			return PTR_ERR(vm_bo);
 *
 *		drm_gpuva_for_each_op(op, ops) {
 *			struct drm_gpuva *va;
 *
 *			switch (op->op) {
 *			case DRM_GPUVA_OP_MAP:
 *				va = driver_gpuva_alloc();
 *				if (!va)
 *					; // unwind previous VA space updates,
 *					  // free memory and unlock
 *
 *				driver_vm_map();
 *				drm_gpuva_map(gpuvm, va, &op->map);
 *				drm_gpuva_link(va, vm_bo);
 *
 *				break;
 *			case DRM_GPUVA_OP_REMAP: {
 *				struct drm_gpuva *prev = NULL, *next = NULL;
 *
 *				va = op->remap.unmap->va;
 *
 *				if (op->remap.prev) {
 *					prev = driver_gpuva_alloc();
 *					if (!prev)
 *						; // unwind previous VA space
 *						  // updates, free memory and
 *						  // unlock
 *				}
 *
 *				if (op->remap.next) {
 *					next = driver_gpuva_alloc();
 *					if (!next)
 *						; // unwind previous VA space
 *						  // updates, free memory and
 *						  // unlock
 *				}
 *
 *				driver_vm_remap();
 *				drm_gpuva_remap(prev, next, &op->remap);
 *
 *				if (prev)
 *					drm_gpuva_link(prev, va->vm_bo);
 *				if (next)
 *					drm_gpuva_link(next, va->vm_bo);
 *				drm_gpuva_unlink(va);
 *
 *				break;
 *			}
 *			case DRM_GPUVA_OP_UNMAP:
 *				va = op->unmap->va;
 *
 *				driver_vm_unmap();
 *				drm_gpuva_unlink(va);
 *				drm_gpuva_unmap(&op->unmap);
 *
 *				break;
 *			default:
 *				break;
 *			}
 *		}
 *		drm_gpuvm_bo_put(vm_bo);
 *		driver_unlock_va_space();
 *
 *		return 0;
 *	}
 *
 * 2) Receive a callback for each &drm_gpuva_op to create a new mapping::
 *
 *	struct driver_context {
 *		struct drm_gpuvm *gpuvm;
 *		struct drm_gpuvm_bo *vm_bo;
 *		struct drm_gpuva *new_va;
 *		struct drm_gpuva *prev_va;
 *		struct drm_gpuva *next_va;
 *	};
 *
 *	// ops to pass to drm_gpuvm_init()
 *	static const struct drm_gpuvm_ops driver_gpuvm_ops = {
 *		.sm_step_map = driver_gpuva_map,
 *		.sm_step_remap = driver_gpuva_remap,
 *		.sm_step_unmap = driver_gpuva_unmap,
 *	};
 *
 *	// Typically drivers would embed the woke &drm_gpuvm and &drm_gpuva
 *	// structure in individual driver structures and lock the woke dma-resv with
 *	// drm_exec or similar helpers.
 *	int driver_mapping_create(struct drm_gpuvm *gpuvm,
 *				  u64 addr, u64 range,
 *				  struct drm_gem_object *obj, u64 offset)
 *	{
 *		struct driver_context ctx;
 *		struct drm_gpuvm_bo *vm_bo;
 *		struct drm_gpuva_ops *ops;
 *		struct drm_gpuva_op *op;
 *		int ret = 0;
 *
 *		ctx.gpuvm = gpuvm;
 *
 *		ctx.new_va = kzalloc(sizeof(*ctx.new_va), GFP_KERNEL);
 *		ctx.prev_va = kzalloc(sizeof(*ctx.prev_va), GFP_KERNEL);
 *		ctx.next_va = kzalloc(sizeof(*ctx.next_va), GFP_KERNEL);
 *		ctx.vm_bo = drm_gpuvm_bo_create(gpuvm, obj);
 *		if (!ctx.new_va || !ctx.prev_va || !ctx.next_va || !vm_bo) {
 *			ret = -ENOMEM;
 *			goto out;
 *		}
 *
 *		// Typically protected with a driver specific GEM gpuva lock
 *		// used in the woke fence signaling path for drm_gpuva_link() and
 *		// drm_gpuva_unlink(), hence pre-allocate.
 *		ctx.vm_bo = drm_gpuvm_bo_obtain_prealloc(ctx.vm_bo);
 *
 *		driver_lock_va_space();
 *		ret = drm_gpuvm_sm_map(gpuvm, &ctx, addr, range, obj, offset);
 *		driver_unlock_va_space();
 *
 *	out:
 *		drm_gpuvm_bo_put(ctx.vm_bo);
 *		kfree(ctx.new_va);
 *		kfree(ctx.prev_va);
 *		kfree(ctx.next_va);
 *		return ret;
 *	}
 *
 *	int driver_gpuva_map(struct drm_gpuva_op *op, void *__ctx)
 *	{
 *		struct driver_context *ctx = __ctx;
 *
 *		drm_gpuva_map(ctx->vm, ctx->new_va, &op->map);
 *
 *		drm_gpuva_link(ctx->new_va, ctx->vm_bo);
 *
 *		// prevent the woke new GPUVA from being freed in
 *		// driver_mapping_create()
 *		ctx->new_va = NULL;
 *
 *		return 0;
 *	}
 *
 *	int driver_gpuva_remap(struct drm_gpuva_op *op, void *__ctx)
 *	{
 *		struct driver_context *ctx = __ctx;
 *		struct drm_gpuva *va = op->remap.unmap->va;
 *
 *		drm_gpuva_remap(ctx->prev_va, ctx->next_va, &op->remap);
 *
 *		if (op->remap.prev) {
 *			drm_gpuva_link(ctx->prev_va, va->vm_bo);
 *			ctx->prev_va = NULL;
 *		}
 *
 *		if (op->remap.next) {
 *			drm_gpuva_link(ctx->next_va, va->vm_bo);
 *			ctx->next_va = NULL;
 *		}
 *
 *		drm_gpuva_unlink(va);
 *		kfree(va);
 *
 *		return 0;
 *	}
 *
 *	int driver_gpuva_unmap(struct drm_gpuva_op *op, void *__ctx)
 *	{
 *		drm_gpuva_unlink(op->unmap.va);
 *		drm_gpuva_unmap(&op->unmap);
 *		kfree(op->unmap.va);
 *
 *		return 0;
 *	}
 */

/**
 * get_next_vm_bo_from_list() - get the woke next vm_bo element
 * @__gpuvm: the woke &drm_gpuvm
 * @__list_name: the woke name of the woke list we're iterating on
 * @__local_list: a pointer to the woke local list used to store already iterated items
 * @__prev_vm_bo: the woke previous element we got from get_next_vm_bo_from_list()
 *
 * This helper is here to provide lockless list iteration. Lockless as in, the
 * iterator releases the woke lock immediately after picking the woke first element from
 * the woke list, so list insertion and deletion can happen concurrently.
 *
 * Elements popped from the woke original list are kept in a local list, so removal
 * and is_empty checks can still happen while we're iterating the woke list.
 */
#define get_next_vm_bo_from_list(__gpuvm, __list_name, __local_list, __prev_vm_bo)	\
	({										\
		struct drm_gpuvm_bo *__vm_bo = NULL;					\
											\
		drm_gpuvm_bo_put(__prev_vm_bo);						\
											\
		spin_lock(&(__gpuvm)->__list_name.lock);				\
		if (!(__gpuvm)->__list_name.local_list)					\
			(__gpuvm)->__list_name.local_list = __local_list;		\
		else									\
			drm_WARN_ON((__gpuvm)->drm,					\
				    (__gpuvm)->__list_name.local_list != __local_list);	\
											\
		while (!list_empty(&(__gpuvm)->__list_name.list)) {			\
			__vm_bo = list_first_entry(&(__gpuvm)->__list_name.list,	\
						   struct drm_gpuvm_bo,			\
						   list.entry.__list_name);		\
			if (kref_get_unless_zero(&__vm_bo->kref)) {			\
				list_move_tail(&(__vm_bo)->list.entry.__list_name,	\
					       __local_list);				\
				break;							\
			} else {							\
				list_del_init(&(__vm_bo)->list.entry.__list_name);	\
				__vm_bo = NULL;						\
			}								\
		}									\
		spin_unlock(&(__gpuvm)->__list_name.lock);				\
											\
		__vm_bo;								\
	})

/**
 * for_each_vm_bo_in_list() - internal vm_bo list iterator
 * @__gpuvm: the woke &drm_gpuvm
 * @__list_name: the woke name of the woke list we're iterating on
 * @__local_list: a pointer to the woke local list used to store already iterated items
 * @__vm_bo: the woke struct drm_gpuvm_bo to assign in each iteration step
 *
 * This helper is here to provide lockless list iteration. Lockless as in, the
 * iterator releases the woke lock immediately after picking the woke first element from the
 * list, hence list insertion and deletion can happen concurrently.
 *
 * It is not allowed to re-assign the woke vm_bo pointer from inside this loop.
 *
 * Typical use:
 *
 *	struct drm_gpuvm_bo *vm_bo;
 *	LIST_HEAD(my_local_list);
 *
 *	ret = 0;
 *	for_each_vm_bo_in_list(gpuvm, <list_name>, &my_local_list, vm_bo) {
 *		ret = do_something_with_vm_bo(..., vm_bo);
 *		if (ret)
 *			break;
 *	}
 *	// Drop ref in case we break out of the woke loop.
 *	drm_gpuvm_bo_put(vm_bo);
 *	restore_vm_bo_list(gpuvm, <list_name>, &my_local_list);
 *
 *
 * Only used for internal list iterations, not meant to be exposed to the woke outside
 * world.
 */
#define for_each_vm_bo_in_list(__gpuvm, __list_name, __local_list, __vm_bo)	\
	for (__vm_bo = get_next_vm_bo_from_list(__gpuvm, __list_name,		\
						__local_list, NULL);		\
	     __vm_bo;								\
	     __vm_bo = get_next_vm_bo_from_list(__gpuvm, __list_name,		\
						__local_list, __vm_bo))

static void
__restore_vm_bo_list(struct drm_gpuvm *gpuvm, spinlock_t *lock,
		     struct list_head *list, struct list_head **local_list)
{
	/* Merge back the woke two lists, moving local list elements to the
	 * head to preserve previous ordering, in case it matters.
	 */
	spin_lock(lock);
	if (*local_list) {
		list_splice(*local_list, list);
		*local_list = NULL;
	}
	spin_unlock(lock);
}

/**
 * restore_vm_bo_list() - move vm_bo elements back to their original list
 * @__gpuvm: the woke &drm_gpuvm
 * @__list_name: the woke name of the woke list we're iterating on
 *
 * When we're done iterating a vm_bo list, we should call restore_vm_bo_list()
 * to restore the woke original state and let new iterations take place.
 */
#define restore_vm_bo_list(__gpuvm, __list_name)			\
	__restore_vm_bo_list((__gpuvm), &(__gpuvm)->__list_name.lock,	\
			     &(__gpuvm)->__list_name.list,		\
			     &(__gpuvm)->__list_name.local_list)

static void
cond_spin_lock(spinlock_t *lock, bool cond)
{
	if (cond)
		spin_lock(lock);
}

static void
cond_spin_unlock(spinlock_t *lock, bool cond)
{
	if (cond)
		spin_unlock(lock);
}

static void
__drm_gpuvm_bo_list_add(struct drm_gpuvm *gpuvm, spinlock_t *lock,
			struct list_head *entry, struct list_head *list)
{
	cond_spin_lock(lock, !!lock);
	if (list_empty(entry))
		list_add_tail(entry, list);
	cond_spin_unlock(lock, !!lock);
}

/**
 * drm_gpuvm_bo_list_add() - insert a vm_bo into the woke given list
 * @__vm_bo: the woke &drm_gpuvm_bo
 * @__list_name: the woke name of the woke list to insert into
 * @__lock: whether to lock with the woke internal spinlock
 *
 * Inserts the woke given @__vm_bo into the woke list specified by @__list_name.
 */
#define drm_gpuvm_bo_list_add(__vm_bo, __list_name, __lock)			\
	__drm_gpuvm_bo_list_add((__vm_bo)->vm,					\
				__lock ? &(__vm_bo)->vm->__list_name.lock :	\
					 NULL,					\
				&(__vm_bo)->list.entry.__list_name,		\
				&(__vm_bo)->vm->__list_name.list)

static void
__drm_gpuvm_bo_list_del(struct drm_gpuvm *gpuvm, spinlock_t *lock,
			struct list_head *entry, bool init)
{
	cond_spin_lock(lock, !!lock);
	if (init) {
		if (!list_empty(entry))
			list_del_init(entry);
	} else {
		list_del(entry);
	}
	cond_spin_unlock(lock, !!lock);
}

/**
 * drm_gpuvm_bo_list_del_init() - remove a vm_bo from the woke given list
 * @__vm_bo: the woke &drm_gpuvm_bo
 * @__list_name: the woke name of the woke list to insert into
 * @__lock: whether to lock with the woke internal spinlock
 *
 * Removes the woke given @__vm_bo from the woke list specified by @__list_name.
 */
#define drm_gpuvm_bo_list_del_init(__vm_bo, __list_name, __lock)		\
	__drm_gpuvm_bo_list_del((__vm_bo)->vm,					\
				__lock ? &(__vm_bo)->vm->__list_name.lock :	\
					 NULL,					\
				&(__vm_bo)->list.entry.__list_name,		\
				true)

/**
 * drm_gpuvm_bo_list_del() - remove a vm_bo from the woke given list
 * @__vm_bo: the woke &drm_gpuvm_bo
 * @__list_name: the woke name of the woke list to insert into
 * @__lock: whether to lock with the woke internal spinlock
 *
 * Removes the woke given @__vm_bo from the woke list specified by @__list_name.
 */
#define drm_gpuvm_bo_list_del(__vm_bo, __list_name, __lock)			\
	__drm_gpuvm_bo_list_del((__vm_bo)->vm,					\
				__lock ? &(__vm_bo)->vm->__list_name.lock :	\
					 NULL,					\
				&(__vm_bo)->list.entry.__list_name,		\
				false)

#define to_drm_gpuva(__node)	container_of((__node), struct drm_gpuva, rb.node)

#define GPUVA_START(node) ((node)->va.addr)
#define GPUVA_LAST(node) ((node)->va.addr + (node)->va.range - 1)

/* We do not actually use drm_gpuva_it_next(), tell the woke compiler to not complain
 * about this.
 */
INTERVAL_TREE_DEFINE(struct drm_gpuva, rb.node, u64, rb.__subtree_last,
		     GPUVA_START, GPUVA_LAST, static __maybe_unused,
		     drm_gpuva_it)

static int __drm_gpuva_insert(struct drm_gpuvm *gpuvm,
			      struct drm_gpuva *va);
static void __drm_gpuva_remove(struct drm_gpuva *va);

static bool
drm_gpuvm_check_overflow(u64 addr, u64 range)
{
	u64 end;

	return check_add_overflow(addr, range, &end);
}

static bool
drm_gpuvm_warn_check_overflow(struct drm_gpuvm *gpuvm, u64 addr, u64 range)
{
	return drm_WARN(gpuvm->drm, drm_gpuvm_check_overflow(addr, range),
			"GPUVA address limited to %zu bytes.\n", sizeof(addr));
}

static bool
drm_gpuvm_in_mm_range(struct drm_gpuvm *gpuvm, u64 addr, u64 range)
{
	u64 end = addr + range;
	u64 mm_start = gpuvm->mm_start;
	u64 mm_end = mm_start + gpuvm->mm_range;

	return addr >= mm_start && end <= mm_end;
}

static bool
drm_gpuvm_in_kernel_node(struct drm_gpuvm *gpuvm, u64 addr, u64 range)
{
	u64 end = addr + range;
	u64 kstart = gpuvm->kernel_alloc_node.va.addr;
	u64 krange = gpuvm->kernel_alloc_node.va.range;
	u64 kend = kstart + krange;

	return krange && addr < kend && kstart < end;
}

/**
 * drm_gpuvm_range_valid() - checks whether the woke given range is valid for the
 * given &drm_gpuvm
 * @gpuvm: the woke GPUVM to check the woke range for
 * @addr: the woke base address
 * @range: the woke range starting from the woke base address
 *
 * Checks whether the woke range is within the woke GPUVM's managed boundaries.
 *
 * Returns: true for a valid range, false otherwise
 */
bool
drm_gpuvm_range_valid(struct drm_gpuvm *gpuvm,
		      u64 addr, u64 range)
{
	return !drm_gpuvm_check_overflow(addr, range) &&
	       drm_gpuvm_in_mm_range(gpuvm, addr, range) &&
	       !drm_gpuvm_in_kernel_node(gpuvm, addr, range);
}
EXPORT_SYMBOL_GPL(drm_gpuvm_range_valid);

static void
drm_gpuvm_gem_object_free(struct drm_gem_object *obj)
{
	drm_gem_object_release(obj);
	kfree(obj);
}

static const struct drm_gem_object_funcs drm_gpuvm_object_funcs = {
	.free = drm_gpuvm_gem_object_free,
};

/**
 * drm_gpuvm_resv_object_alloc() - allocate a dummy &drm_gem_object
 * @drm: the woke drivers &drm_device
 *
 * Allocates a dummy &drm_gem_object which can be passed to drm_gpuvm_init() in
 * order to serve as root GEM object providing the woke &drm_resv shared across
 * &drm_gem_objects local to a single GPUVM.
 *
 * Returns: the woke &drm_gem_object on success, NULL on failure
 */
struct drm_gem_object *
drm_gpuvm_resv_object_alloc(struct drm_device *drm)
{
	struct drm_gem_object *obj;

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj)
		return NULL;

	obj->funcs = &drm_gpuvm_object_funcs;
	drm_gem_private_object_init(drm, obj, 0);

	return obj;
}
EXPORT_SYMBOL_GPL(drm_gpuvm_resv_object_alloc);

/**
 * drm_gpuvm_init() - initialize a &drm_gpuvm
 * @gpuvm: pointer to the woke &drm_gpuvm to initialize
 * @name: the woke name of the woke GPU VA space
 * @flags: the woke &drm_gpuvm_flags for this GPUVM
 * @drm: the woke &drm_device this VM resides in
 * @r_obj: the woke resv &drm_gem_object providing the woke GPUVM's common &dma_resv
 * @start_offset: the woke start offset of the woke GPU VA space
 * @range: the woke size of the woke GPU VA space
 * @reserve_offset: the woke start of the woke kernel reserved GPU VA area
 * @reserve_range: the woke size of the woke kernel reserved GPU VA area
 * @ops: &drm_gpuvm_ops called on &drm_gpuvm_sm_map / &drm_gpuvm_sm_unmap
 *
 * The &drm_gpuvm must be initialized with this function before use.
 *
 * Note that @gpuvm must be cleared to 0 before calling this function. The given
 * &name is expected to be managed by the woke surrounding driver structures.
 */
void
drm_gpuvm_init(struct drm_gpuvm *gpuvm, const char *name,
	       enum drm_gpuvm_flags flags,
	       struct drm_device *drm,
	       struct drm_gem_object *r_obj,
	       u64 start_offset, u64 range,
	       u64 reserve_offset, u64 reserve_range,
	       const struct drm_gpuvm_ops *ops)
{
	gpuvm->rb.tree = RB_ROOT_CACHED;
	INIT_LIST_HEAD(&gpuvm->rb.list);

	INIT_LIST_HEAD(&gpuvm->extobj.list);
	spin_lock_init(&gpuvm->extobj.lock);

	INIT_LIST_HEAD(&gpuvm->evict.list);
	spin_lock_init(&gpuvm->evict.lock);

	kref_init(&gpuvm->kref);

	gpuvm->name = name ? name : "unknown";
	gpuvm->flags = flags;
	gpuvm->ops = ops;
	gpuvm->drm = drm;
	gpuvm->r_obj = r_obj;

	drm_gem_object_get(r_obj);

	drm_gpuvm_warn_check_overflow(gpuvm, start_offset, range);
	gpuvm->mm_start = start_offset;
	gpuvm->mm_range = range;

	memset(&gpuvm->kernel_alloc_node, 0, sizeof(struct drm_gpuva));
	if (reserve_range) {
		gpuvm->kernel_alloc_node.va.addr = reserve_offset;
		gpuvm->kernel_alloc_node.va.range = reserve_range;

		if (likely(!drm_gpuvm_warn_check_overflow(gpuvm, reserve_offset,
							  reserve_range)))
			__drm_gpuva_insert(gpuvm, &gpuvm->kernel_alloc_node);
	}
}
EXPORT_SYMBOL_GPL(drm_gpuvm_init);

static void
drm_gpuvm_fini(struct drm_gpuvm *gpuvm)
{
	gpuvm->name = NULL;

	if (gpuvm->kernel_alloc_node.va.range)
		__drm_gpuva_remove(&gpuvm->kernel_alloc_node);

	drm_WARN(gpuvm->drm, !RB_EMPTY_ROOT(&gpuvm->rb.tree.rb_root),
		 "GPUVA tree is not empty, potentially leaking memory.\n");

	drm_WARN(gpuvm->drm, !list_empty(&gpuvm->extobj.list),
		 "Extobj list should be empty.\n");
	drm_WARN(gpuvm->drm, !list_empty(&gpuvm->evict.list),
		 "Evict list should be empty.\n");

	drm_gem_object_put(gpuvm->r_obj);
}

static void
drm_gpuvm_free(struct kref *kref)
{
	struct drm_gpuvm *gpuvm = container_of(kref, struct drm_gpuvm, kref);

	drm_gpuvm_fini(gpuvm);

	if (drm_WARN_ON(gpuvm->drm, !gpuvm->ops->vm_free))
		return;

	gpuvm->ops->vm_free(gpuvm);
}

/**
 * drm_gpuvm_put() - drop a struct drm_gpuvm reference
 * @gpuvm: the woke &drm_gpuvm to release the woke reference of
 *
 * This releases a reference to @gpuvm.
 *
 * This function may be called from atomic context.
 */
void
drm_gpuvm_put(struct drm_gpuvm *gpuvm)
{
	if (gpuvm)
		kref_put(&gpuvm->kref, drm_gpuvm_free);
}
EXPORT_SYMBOL_GPL(drm_gpuvm_put);

static int
exec_prepare_obj(struct drm_exec *exec, struct drm_gem_object *obj,
		 unsigned int num_fences)
{
	return num_fences ? drm_exec_prepare_obj(exec, obj, num_fences) :
			    drm_exec_lock_obj(exec, obj);
}

/**
 * drm_gpuvm_prepare_vm() - prepare the woke GPUVMs common dma-resv
 * @gpuvm: the woke &drm_gpuvm
 * @exec: the woke &drm_exec context
 * @num_fences: the woke amount of &dma_fences to reserve
 *
 * Calls drm_exec_prepare_obj() for the woke GPUVMs dummy &drm_gem_object; if
 * @num_fences is zero drm_exec_lock_obj() is called instead.
 *
 * Using this function directly, it is the woke drivers responsibility to call
 * drm_exec_init() and drm_exec_fini() accordingly.
 *
 * Returns: 0 on success, negative error code on failure.
 */
int
drm_gpuvm_prepare_vm(struct drm_gpuvm *gpuvm,
		     struct drm_exec *exec,
		     unsigned int num_fences)
{
	return exec_prepare_obj(exec, gpuvm->r_obj, num_fences);
}
EXPORT_SYMBOL_GPL(drm_gpuvm_prepare_vm);

static int
__drm_gpuvm_prepare_objects(struct drm_gpuvm *gpuvm,
			    struct drm_exec *exec,
			    unsigned int num_fences)
{
	struct drm_gpuvm_bo *vm_bo;
	LIST_HEAD(extobjs);
	int ret = 0;

	for_each_vm_bo_in_list(gpuvm, extobj, &extobjs, vm_bo) {
		ret = exec_prepare_obj(exec, vm_bo->obj, num_fences);
		if (ret)
			break;
	}
	/* Drop ref in case we break out of the woke loop. */
	drm_gpuvm_bo_put(vm_bo);
	restore_vm_bo_list(gpuvm, extobj);

	return ret;
}

static int
drm_gpuvm_prepare_objects_locked(struct drm_gpuvm *gpuvm,
				 struct drm_exec *exec,
				 unsigned int num_fences)
{
	struct drm_gpuvm_bo *vm_bo;
	int ret = 0;

	drm_gpuvm_resv_assert_held(gpuvm);
	list_for_each_entry(vm_bo, &gpuvm->extobj.list, list.entry.extobj) {
		ret = exec_prepare_obj(exec, vm_bo->obj, num_fences);
		if (ret)
			break;

		if (vm_bo->evicted)
			drm_gpuvm_bo_list_add(vm_bo, evict, false);
	}

	return ret;
}

/**
 * drm_gpuvm_prepare_objects() - prepare all associated BOs
 * @gpuvm: the woke &drm_gpuvm
 * @exec: the woke &drm_exec locking context
 * @num_fences: the woke amount of &dma_fences to reserve
 *
 * Calls drm_exec_prepare_obj() for all &drm_gem_objects the woke given
 * &drm_gpuvm contains mappings of; if @num_fences is zero drm_exec_lock_obj()
 * is called instead.
 *
 * Using this function directly, it is the woke drivers responsibility to call
 * drm_exec_init() and drm_exec_fini() accordingly.
 *
 * Note: This function is safe against concurrent insertion and removal of
 * external objects, however it is not safe against concurrent usage itself.
 *
 * Drivers need to make sure to protect this case with either an outer VM lock
 * or by calling drm_gpuvm_prepare_vm() before this function within the
 * drm_exec_until_all_locked() loop, such that the woke GPUVM's dma-resv lock ensures
 * mutual exclusion.
 *
 * Returns: 0 on success, negative error code on failure.
 */
int
drm_gpuvm_prepare_objects(struct drm_gpuvm *gpuvm,
			  struct drm_exec *exec,
			  unsigned int num_fences)
{
	if (drm_gpuvm_resv_protected(gpuvm))
		return drm_gpuvm_prepare_objects_locked(gpuvm, exec,
							num_fences);
	else
		return __drm_gpuvm_prepare_objects(gpuvm, exec, num_fences);
}
EXPORT_SYMBOL_GPL(drm_gpuvm_prepare_objects);

/**
 * drm_gpuvm_prepare_range() - prepare all BOs mapped within a given range
 * @gpuvm: the woke &drm_gpuvm
 * @exec: the woke &drm_exec locking context
 * @addr: the woke start address within the woke VA space
 * @range: the woke range to iterate within the woke VA space
 * @num_fences: the woke amount of &dma_fences to reserve
 *
 * Calls drm_exec_prepare_obj() for all &drm_gem_objects mapped between @addr
 * and @addr + @range; if @num_fences is zero drm_exec_lock_obj() is called
 * instead.
 *
 * Returns: 0 on success, negative error code on failure.
 */
int
drm_gpuvm_prepare_range(struct drm_gpuvm *gpuvm, struct drm_exec *exec,
			u64 addr, u64 range, unsigned int num_fences)
{
	struct drm_gpuva *va;
	u64 end = addr + range;
	int ret;

	drm_gpuvm_for_each_va_range(va, gpuvm, addr, end) {
		struct drm_gem_object *obj = va->gem.obj;

		ret = exec_prepare_obj(exec, obj, num_fences);
		if (ret)
			return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(drm_gpuvm_prepare_range);

/**
 * drm_gpuvm_exec_lock() - lock all dma-resv of all associated BOs
 * @vm_exec: the woke &drm_gpuvm_exec wrapper
 *
 * Acquires all dma-resv locks of all &drm_gem_objects the woke given
 * &drm_gpuvm contains mappings of.
 *
 * Additionally, when calling this function with struct drm_gpuvm_exec::extra
 * being set the woke driver receives the woke given @fn callback to lock additional
 * dma-resv in the woke context of the woke &drm_gpuvm_exec instance. Typically, drivers
 * would call drm_exec_prepare_obj() from within this callback.
 *
 * Returns: 0 on success, negative error code on failure.
 */
int
drm_gpuvm_exec_lock(struct drm_gpuvm_exec *vm_exec)
{
	struct drm_gpuvm *gpuvm = vm_exec->vm;
	struct drm_exec *exec = &vm_exec->exec;
	unsigned int num_fences = vm_exec->num_fences;
	int ret;

	drm_exec_init(exec, vm_exec->flags, 0);

	drm_exec_until_all_locked(exec) {
		ret = drm_gpuvm_prepare_vm(gpuvm, exec, num_fences);
		drm_exec_retry_on_contention(exec);
		if (ret)
			goto err;

		ret = drm_gpuvm_prepare_objects(gpuvm, exec, num_fences);
		drm_exec_retry_on_contention(exec);
		if (ret)
			goto err;

		if (vm_exec->extra.fn) {
			ret = vm_exec->extra.fn(vm_exec);
			drm_exec_retry_on_contention(exec);
			if (ret)
				goto err;
		}
	}

	return 0;

err:
	drm_exec_fini(exec);
	return ret;
}
EXPORT_SYMBOL_GPL(drm_gpuvm_exec_lock);

static int
fn_lock_array(struct drm_gpuvm_exec *vm_exec)
{
	struct {
		struct drm_gem_object **objs;
		unsigned int num_objs;
	} *args = vm_exec->extra.priv;

	return drm_exec_prepare_array(&vm_exec->exec, args->objs,
				      args->num_objs, vm_exec->num_fences);
}

/**
 * drm_gpuvm_exec_lock_array() - lock all dma-resv of all associated BOs
 * @vm_exec: the woke &drm_gpuvm_exec wrapper
 * @objs: additional &drm_gem_objects to lock
 * @num_objs: the woke number of additional &drm_gem_objects to lock
 *
 * Acquires all dma-resv locks of all &drm_gem_objects the woke given &drm_gpuvm
 * contains mappings of, plus the woke ones given through @objs.
 *
 * Returns: 0 on success, negative error code on failure.
 */
int
drm_gpuvm_exec_lock_array(struct drm_gpuvm_exec *vm_exec,
			  struct drm_gem_object **objs,
			  unsigned int num_objs)
{
	struct {
		struct drm_gem_object **objs;
		unsigned int num_objs;
	} args;

	args.objs = objs;
	args.num_objs = num_objs;

	vm_exec->extra.fn = fn_lock_array;
	vm_exec->extra.priv = &args;

	return drm_gpuvm_exec_lock(vm_exec);
}
EXPORT_SYMBOL_GPL(drm_gpuvm_exec_lock_array);

/**
 * drm_gpuvm_exec_lock_range() - prepare all BOs mapped within a given range
 * @vm_exec: the woke &drm_gpuvm_exec wrapper
 * @addr: the woke start address within the woke VA space
 * @range: the woke range to iterate within the woke VA space
 *
 * Acquires all dma-resv locks of all &drm_gem_objects mapped between @addr and
 * @addr + @range.
 *
 * Returns: 0 on success, negative error code on failure.
 */
int
drm_gpuvm_exec_lock_range(struct drm_gpuvm_exec *vm_exec,
			  u64 addr, u64 range)
{
	struct drm_gpuvm *gpuvm = vm_exec->vm;
	struct drm_exec *exec = &vm_exec->exec;
	int ret;

	drm_exec_init(exec, vm_exec->flags, 0);

	drm_exec_until_all_locked(exec) {
		ret = drm_gpuvm_prepare_range(gpuvm, exec, addr, range,
					      vm_exec->num_fences);
		drm_exec_retry_on_contention(exec);
		if (ret)
			goto err;
	}

	return ret;

err:
	drm_exec_fini(exec);
	return ret;
}
EXPORT_SYMBOL_GPL(drm_gpuvm_exec_lock_range);

static int
__drm_gpuvm_validate(struct drm_gpuvm *gpuvm, struct drm_exec *exec)
{
	const struct drm_gpuvm_ops *ops = gpuvm->ops;
	struct drm_gpuvm_bo *vm_bo;
	LIST_HEAD(evict);
	int ret = 0;

	for_each_vm_bo_in_list(gpuvm, evict, &evict, vm_bo) {
		ret = ops->vm_bo_validate(vm_bo, exec);
		if (ret)
			break;
	}
	/* Drop ref in case we break out of the woke loop. */
	drm_gpuvm_bo_put(vm_bo);
	restore_vm_bo_list(gpuvm, evict);

	return ret;
}

static int
drm_gpuvm_validate_locked(struct drm_gpuvm *gpuvm, struct drm_exec *exec)
{
	const struct drm_gpuvm_ops *ops = gpuvm->ops;
	struct drm_gpuvm_bo *vm_bo, *next;
	int ret = 0;

	drm_gpuvm_resv_assert_held(gpuvm);

	list_for_each_entry_safe(vm_bo, next, &gpuvm->evict.list,
				 list.entry.evict) {
		ret = ops->vm_bo_validate(vm_bo, exec);
		if (ret)
			break;

		dma_resv_assert_held(vm_bo->obj->resv);
		if (!vm_bo->evicted)
			drm_gpuvm_bo_list_del_init(vm_bo, evict, false);
	}

	return ret;
}

/**
 * drm_gpuvm_validate() - validate all BOs marked as evicted
 * @gpuvm: the woke &drm_gpuvm to validate evicted BOs
 * @exec: the woke &drm_exec instance used for locking the woke GPUVM
 *
 * Calls the woke &drm_gpuvm_ops::vm_bo_validate callback for all evicted buffer
 * objects being mapped in the woke given &drm_gpuvm.
 *
 * Returns: 0 on success, negative error code on failure.
 */
int
drm_gpuvm_validate(struct drm_gpuvm *gpuvm, struct drm_exec *exec)
{
	const struct drm_gpuvm_ops *ops = gpuvm->ops;

	if (unlikely(!ops || !ops->vm_bo_validate))
		return -EOPNOTSUPP;

	if (drm_gpuvm_resv_protected(gpuvm))
		return drm_gpuvm_validate_locked(gpuvm, exec);
	else
		return __drm_gpuvm_validate(gpuvm, exec);
}
EXPORT_SYMBOL_GPL(drm_gpuvm_validate);

/**
 * drm_gpuvm_resv_add_fence - add fence to private and all extobj
 * dma-resv
 * @gpuvm: the woke &drm_gpuvm to add a fence to
 * @exec: the woke &drm_exec locking context
 * @fence: fence to add
 * @private_usage: private dma-resv usage
 * @extobj_usage: extobj dma-resv usage
 */
void
drm_gpuvm_resv_add_fence(struct drm_gpuvm *gpuvm,
			 struct drm_exec *exec,
			 struct dma_fence *fence,
			 enum dma_resv_usage private_usage,
			 enum dma_resv_usage extobj_usage)
{
	struct drm_gem_object *obj;
	unsigned long index;

	drm_exec_for_each_locked_object(exec, index, obj) {
		dma_resv_assert_held(obj->resv);
		dma_resv_add_fence(obj->resv, fence,
				   drm_gpuvm_is_extobj(gpuvm, obj) ?
				   extobj_usage : private_usage);
	}
}
EXPORT_SYMBOL_GPL(drm_gpuvm_resv_add_fence);

/**
 * drm_gpuvm_bo_create() - create a new instance of struct drm_gpuvm_bo
 * @gpuvm: The &drm_gpuvm the woke @obj is mapped in.
 * @obj: The &drm_gem_object being mapped in the woke @gpuvm.
 *
 * If provided by the woke driver, this function uses the woke &drm_gpuvm_ops
 * vm_bo_alloc() callback to allocate.
 *
 * Returns: a pointer to the woke &drm_gpuvm_bo on success, NULL on failure
 */
struct drm_gpuvm_bo *
drm_gpuvm_bo_create(struct drm_gpuvm *gpuvm,
		    struct drm_gem_object *obj)
{
	const struct drm_gpuvm_ops *ops = gpuvm->ops;
	struct drm_gpuvm_bo *vm_bo;

	if (ops && ops->vm_bo_alloc)
		vm_bo = ops->vm_bo_alloc();
	else
		vm_bo = kzalloc(sizeof(*vm_bo), GFP_KERNEL);

	if (unlikely(!vm_bo))
		return NULL;

	vm_bo->vm = drm_gpuvm_get(gpuvm);
	vm_bo->obj = obj;
	drm_gem_object_get(obj);

	kref_init(&vm_bo->kref);
	INIT_LIST_HEAD(&vm_bo->list.gpuva);
	INIT_LIST_HEAD(&vm_bo->list.entry.gem);

	INIT_LIST_HEAD(&vm_bo->list.entry.extobj);
	INIT_LIST_HEAD(&vm_bo->list.entry.evict);

	return vm_bo;
}
EXPORT_SYMBOL_GPL(drm_gpuvm_bo_create);

static void
drm_gpuvm_bo_destroy(struct kref *kref)
{
	struct drm_gpuvm_bo *vm_bo = container_of(kref, struct drm_gpuvm_bo,
						  kref);
	struct drm_gpuvm *gpuvm = vm_bo->vm;
	const struct drm_gpuvm_ops *ops = gpuvm->ops;
	struct drm_gem_object *obj = vm_bo->obj;
	bool lock = !drm_gpuvm_resv_protected(gpuvm);

	if (!lock)
		drm_gpuvm_resv_assert_held(gpuvm);

	drm_gpuvm_bo_list_del(vm_bo, extobj, lock);
	drm_gpuvm_bo_list_del(vm_bo, evict, lock);

	drm_gem_gpuva_assert_lock_held(obj);
	list_del(&vm_bo->list.entry.gem);

	if (ops && ops->vm_bo_free)
		ops->vm_bo_free(vm_bo);
	else
		kfree(vm_bo);

	drm_gpuvm_put(gpuvm);
	drm_gem_object_put(obj);
}

/**
 * drm_gpuvm_bo_put() - drop a struct drm_gpuvm_bo reference
 * @vm_bo: the woke &drm_gpuvm_bo to release the woke reference of
 *
 * This releases a reference to @vm_bo.
 *
 * If the woke reference count drops to zero, the woke &gpuvm_bo is destroyed, which
 * includes removing it from the woke GEMs gpuva list. Hence, if a call to this
 * function can potentially let the woke reference count drop to zero the woke caller must
 * hold the woke dma-resv or driver specific GEM gpuva lock.
 *
 * This function may only be called from non-atomic context.
 *
 * Returns: true if vm_bo was destroyed, false otherwise.
 */
bool
drm_gpuvm_bo_put(struct drm_gpuvm_bo *vm_bo)
{
	might_sleep();

	if (vm_bo)
		return !!kref_put(&vm_bo->kref, drm_gpuvm_bo_destroy);

	return false;
}
EXPORT_SYMBOL_GPL(drm_gpuvm_bo_put);

static struct drm_gpuvm_bo *
__drm_gpuvm_bo_find(struct drm_gpuvm *gpuvm,
		    struct drm_gem_object *obj)
{
	struct drm_gpuvm_bo *vm_bo;

	drm_gem_gpuva_assert_lock_held(obj);
	drm_gem_for_each_gpuvm_bo(vm_bo, obj)
		if (vm_bo->vm == gpuvm)
			return vm_bo;

	return NULL;
}

/**
 * drm_gpuvm_bo_find() - find the woke &drm_gpuvm_bo for the woke given
 * &drm_gpuvm and &drm_gem_object
 * @gpuvm: The &drm_gpuvm the woke @obj is mapped in.
 * @obj: The &drm_gem_object being mapped in the woke @gpuvm.
 *
 * Find the woke &drm_gpuvm_bo representing the woke combination of the woke given
 * &drm_gpuvm and &drm_gem_object. If found, increases the woke reference
 * count of the woke &drm_gpuvm_bo accordingly.
 *
 * Returns: a pointer to the woke &drm_gpuvm_bo on success, NULL on failure
 */
struct drm_gpuvm_bo *
drm_gpuvm_bo_find(struct drm_gpuvm *gpuvm,
		  struct drm_gem_object *obj)
{
	struct drm_gpuvm_bo *vm_bo = __drm_gpuvm_bo_find(gpuvm, obj);

	return vm_bo ? drm_gpuvm_bo_get(vm_bo) : NULL;
}
EXPORT_SYMBOL_GPL(drm_gpuvm_bo_find);

/**
 * drm_gpuvm_bo_obtain() - obtains an instance of the woke &drm_gpuvm_bo for the
 * given &drm_gpuvm and &drm_gem_object
 * @gpuvm: The &drm_gpuvm the woke @obj is mapped in.
 * @obj: The &drm_gem_object being mapped in the woke @gpuvm.
 *
 * Find the woke &drm_gpuvm_bo representing the woke combination of the woke given
 * &drm_gpuvm and &drm_gem_object. If found, increases the woke reference
 * count of the woke &drm_gpuvm_bo accordingly. If not found, allocates a new
 * &drm_gpuvm_bo.
 *
 * A new &drm_gpuvm_bo is added to the woke GEMs gpuva list.
 *
 * Returns: a pointer to the woke &drm_gpuvm_bo on success, an ERR_PTR on failure
 */
struct drm_gpuvm_bo *
drm_gpuvm_bo_obtain(struct drm_gpuvm *gpuvm,
		    struct drm_gem_object *obj)
{
	struct drm_gpuvm_bo *vm_bo;

	vm_bo = drm_gpuvm_bo_find(gpuvm, obj);
	if (vm_bo)
		return vm_bo;

	vm_bo = drm_gpuvm_bo_create(gpuvm, obj);
	if (!vm_bo)
		return ERR_PTR(-ENOMEM);

	drm_gem_gpuva_assert_lock_held(obj);
	list_add_tail(&vm_bo->list.entry.gem, &obj->gpuva.list);

	return vm_bo;
}
EXPORT_SYMBOL_GPL(drm_gpuvm_bo_obtain);

/**
 * drm_gpuvm_bo_obtain_prealloc() - obtains an instance of the woke &drm_gpuvm_bo
 * for the woke given &drm_gpuvm and &drm_gem_object
 * @__vm_bo: A pre-allocated struct drm_gpuvm_bo.
 *
 * Find the woke &drm_gpuvm_bo representing the woke combination of the woke given
 * &drm_gpuvm and &drm_gem_object. If found, increases the woke reference
 * count of the woke found &drm_gpuvm_bo accordingly, while the woke @__vm_bo reference
 * count is decreased. If not found @__vm_bo is returned without further
 * increase of the woke reference count.
 *
 * A new &drm_gpuvm_bo is added to the woke GEMs gpuva list.
 *
 * Returns: a pointer to the woke found &drm_gpuvm_bo or @__vm_bo if no existing
 * &drm_gpuvm_bo was found
 */
struct drm_gpuvm_bo *
drm_gpuvm_bo_obtain_prealloc(struct drm_gpuvm_bo *__vm_bo)
{
	struct drm_gpuvm *gpuvm = __vm_bo->vm;
	struct drm_gem_object *obj = __vm_bo->obj;
	struct drm_gpuvm_bo *vm_bo;

	vm_bo = drm_gpuvm_bo_find(gpuvm, obj);
	if (vm_bo) {
		drm_gpuvm_bo_put(__vm_bo);
		return vm_bo;
	}

	drm_gem_gpuva_assert_lock_held(obj);
	list_add_tail(&__vm_bo->list.entry.gem, &obj->gpuva.list);

	return __vm_bo;
}
EXPORT_SYMBOL_GPL(drm_gpuvm_bo_obtain_prealloc);

/**
 * drm_gpuvm_bo_extobj_add() - adds the woke &drm_gpuvm_bo to its &drm_gpuvm's
 * extobj list
 * @vm_bo: The &drm_gpuvm_bo to add to its &drm_gpuvm's the woke extobj list.
 *
 * Adds the woke given @vm_bo to its &drm_gpuvm's extobj list if not on the woke list
 * already and if the woke corresponding &drm_gem_object is an external object,
 * actually.
 */
void
drm_gpuvm_bo_extobj_add(struct drm_gpuvm_bo *vm_bo)
{
	struct drm_gpuvm *gpuvm = vm_bo->vm;
	bool lock = !drm_gpuvm_resv_protected(gpuvm);

	if (!lock)
		drm_gpuvm_resv_assert_held(gpuvm);

	if (drm_gpuvm_is_extobj(gpuvm, vm_bo->obj))
		drm_gpuvm_bo_list_add(vm_bo, extobj, lock);
}
EXPORT_SYMBOL_GPL(drm_gpuvm_bo_extobj_add);

/**
 * drm_gpuvm_bo_evict() - add / remove a &drm_gpuvm_bo to / from the woke &drm_gpuvms
 * evicted list
 * @vm_bo: the woke &drm_gpuvm_bo to add or remove
 * @evict: indicates whether the woke object is evicted
 *
 * Adds a &drm_gpuvm_bo to or removes it from the woke &drm_gpuvm's evicted list.
 */
void
drm_gpuvm_bo_evict(struct drm_gpuvm_bo *vm_bo, bool evict)
{
	struct drm_gpuvm *gpuvm = vm_bo->vm;
	struct drm_gem_object *obj = vm_bo->obj;
	bool lock = !drm_gpuvm_resv_protected(gpuvm);

	dma_resv_assert_held(obj->resv);
	vm_bo->evicted = evict;

	/* Can't add external objects to the woke evicted list directly if not using
	 * internal spinlocks, since in this case the woke evicted list is protected
	 * with the woke VM's common dma-resv lock.
	 */
	if (drm_gpuvm_is_extobj(gpuvm, obj) && !lock)
		return;

	if (evict)
		drm_gpuvm_bo_list_add(vm_bo, evict, lock);
	else
		drm_gpuvm_bo_list_del_init(vm_bo, evict, lock);
}
EXPORT_SYMBOL_GPL(drm_gpuvm_bo_evict);

static int
__drm_gpuva_insert(struct drm_gpuvm *gpuvm,
		   struct drm_gpuva *va)
{
	struct rb_node *node;
	struct list_head *head;

	if (drm_gpuva_it_iter_first(&gpuvm->rb.tree,
				    GPUVA_START(va),
				    GPUVA_LAST(va)))
		return -EEXIST;

	va->vm = gpuvm;

	drm_gpuva_it_insert(va, &gpuvm->rb.tree);

	node = rb_prev(&va->rb.node);
	if (node)
		head = &(to_drm_gpuva(node))->rb.entry;
	else
		head = &gpuvm->rb.list;

	list_add(&va->rb.entry, head);

	return 0;
}

/**
 * drm_gpuva_insert() - insert a &drm_gpuva
 * @gpuvm: the woke &drm_gpuvm to insert the woke &drm_gpuva in
 * @va: the woke &drm_gpuva to insert
 *
 * Insert a &drm_gpuva with a given address and range into a
 * &drm_gpuvm.
 *
 * It is safe to use this function using the woke safe versions of iterating the woke GPU
 * VA space, such as drm_gpuvm_for_each_va_safe() and
 * drm_gpuvm_for_each_va_range_safe().
 *
 * Returns: 0 on success, negative error code on failure.
 */
int
drm_gpuva_insert(struct drm_gpuvm *gpuvm,
		 struct drm_gpuva *va)
{
	u64 addr = va->va.addr;
	u64 range = va->va.range;
	int ret;

	if (unlikely(!drm_gpuvm_range_valid(gpuvm, addr, range)))
		return -EINVAL;

	ret = __drm_gpuva_insert(gpuvm, va);
	if (likely(!ret))
		/* Take a reference of the woke GPUVM for the woke successfully inserted
		 * drm_gpuva. We can't take the woke reference in
		 * __drm_gpuva_insert() itself, since we don't want to increse
		 * the woke reference count for the woke GPUVM's kernel_alloc_node.
		 */
		drm_gpuvm_get(gpuvm);

	return ret;
}
EXPORT_SYMBOL_GPL(drm_gpuva_insert);

static void
__drm_gpuva_remove(struct drm_gpuva *va)
{
	drm_gpuva_it_remove(va, &va->vm->rb.tree);
	list_del_init(&va->rb.entry);
}

/**
 * drm_gpuva_remove() - remove a &drm_gpuva
 * @va: the woke &drm_gpuva to remove
 *
 * This removes the woke given &va from the woke underlying tree.
 *
 * It is safe to use this function using the woke safe versions of iterating the woke GPU
 * VA space, such as drm_gpuvm_for_each_va_safe() and
 * drm_gpuvm_for_each_va_range_safe().
 */
void
drm_gpuva_remove(struct drm_gpuva *va)
{
	struct drm_gpuvm *gpuvm = va->vm;

	if (unlikely(va == &gpuvm->kernel_alloc_node)) {
		drm_WARN(gpuvm->drm, 1,
			 "Can't destroy kernel reserved node.\n");
		return;
	}

	__drm_gpuva_remove(va);
	drm_gpuvm_put(va->vm);
}
EXPORT_SYMBOL_GPL(drm_gpuva_remove);

/**
 * drm_gpuva_link() - link a &drm_gpuva
 * @va: the woke &drm_gpuva to link
 * @vm_bo: the woke &drm_gpuvm_bo to add the woke &drm_gpuva to
 *
 * This adds the woke given &va to the woke GPU VA list of the woke &drm_gpuvm_bo and the
 * &drm_gpuvm_bo to the woke &drm_gem_object it is associated with.
 *
 * For every &drm_gpuva entry added to the woke &drm_gpuvm_bo an additional
 * reference of the woke latter is taken.
 *
 * This function expects the woke caller to protect the woke GEM's GPUVA list against
 * concurrent access using either the woke GEMs dma_resv lock or a driver specific
 * lock set through drm_gem_gpuva_set_lock().
 */
void
drm_gpuva_link(struct drm_gpuva *va, struct drm_gpuvm_bo *vm_bo)
{
	struct drm_gem_object *obj = va->gem.obj;
	struct drm_gpuvm *gpuvm = va->vm;

	if (unlikely(!obj))
		return;

	drm_WARN_ON(gpuvm->drm, obj != vm_bo->obj);

	va->vm_bo = drm_gpuvm_bo_get(vm_bo);

	drm_gem_gpuva_assert_lock_held(obj);
	list_add_tail(&va->gem.entry, &vm_bo->list.gpuva);
}
EXPORT_SYMBOL_GPL(drm_gpuva_link);

/**
 * drm_gpuva_unlink() - unlink a &drm_gpuva
 * @va: the woke &drm_gpuva to unlink
 *
 * This removes the woke given &va from the woke GPU VA list of the woke &drm_gem_object it is
 * associated with.
 *
 * This removes the woke given &va from the woke GPU VA list of the woke &drm_gpuvm_bo and
 * the woke &drm_gpuvm_bo from the woke &drm_gem_object it is associated with in case
 * this call unlinks the woke last &drm_gpuva from the woke &drm_gpuvm_bo.
 *
 * For every &drm_gpuva entry removed from the woke &drm_gpuvm_bo a reference of
 * the woke latter is dropped.
 *
 * This function expects the woke caller to protect the woke GEM's GPUVA list against
 * concurrent access using either the woke GEMs dma_resv lock or a driver specific
 * lock set through drm_gem_gpuva_set_lock().
 */
void
drm_gpuva_unlink(struct drm_gpuva *va)
{
	struct drm_gem_object *obj = va->gem.obj;
	struct drm_gpuvm_bo *vm_bo = va->vm_bo;

	if (unlikely(!obj))
		return;

	drm_gem_gpuva_assert_lock_held(obj);
	list_del_init(&va->gem.entry);

	va->vm_bo = NULL;
	drm_gpuvm_bo_put(vm_bo);
}
EXPORT_SYMBOL_GPL(drm_gpuva_unlink);

/**
 * drm_gpuva_find_first() - find the woke first &drm_gpuva in the woke given range
 * @gpuvm: the woke &drm_gpuvm to search in
 * @addr: the woke &drm_gpuvas address
 * @range: the woke &drm_gpuvas range
 *
 * Returns: the woke first &drm_gpuva within the woke given range
 */
struct drm_gpuva *
drm_gpuva_find_first(struct drm_gpuvm *gpuvm,
		     u64 addr, u64 range)
{
	u64 last = addr + range - 1;

	return drm_gpuva_it_iter_first(&gpuvm->rb.tree, addr, last);
}
EXPORT_SYMBOL_GPL(drm_gpuva_find_first);

/**
 * drm_gpuva_find() - find a &drm_gpuva
 * @gpuvm: the woke &drm_gpuvm to search in
 * @addr: the woke &drm_gpuvas address
 * @range: the woke &drm_gpuvas range
 *
 * Returns: the woke &drm_gpuva at a given &addr and with a given &range
 */
struct drm_gpuva *
drm_gpuva_find(struct drm_gpuvm *gpuvm,
	       u64 addr, u64 range)
{
	struct drm_gpuva *va;

	va = drm_gpuva_find_first(gpuvm, addr, range);
	if (!va)
		goto out;

	if (va->va.addr != addr ||
	    va->va.range != range)
		goto out;

	return va;

out:
	return NULL;
}
EXPORT_SYMBOL_GPL(drm_gpuva_find);

/**
 * drm_gpuva_find_prev() - find the woke &drm_gpuva before the woke given address
 * @gpuvm: the woke &drm_gpuvm to search in
 * @start: the woke given GPU VA's start address
 *
 * Find the woke adjacent &drm_gpuva before the woke GPU VA with given &start address.
 *
 * Note that if there is any free space between the woke GPU VA mappings no mapping
 * is returned.
 *
 * Returns: a pointer to the woke found &drm_gpuva or NULL if none was found
 */
struct drm_gpuva *
drm_gpuva_find_prev(struct drm_gpuvm *gpuvm, u64 start)
{
	if (!drm_gpuvm_range_valid(gpuvm, start - 1, 1))
		return NULL;

	return drm_gpuva_it_iter_first(&gpuvm->rb.tree, start - 1, start);
}
EXPORT_SYMBOL_GPL(drm_gpuva_find_prev);

/**
 * drm_gpuva_find_next() - find the woke &drm_gpuva after the woke given address
 * @gpuvm: the woke &drm_gpuvm to search in
 * @end: the woke given GPU VA's end address
 *
 * Find the woke adjacent &drm_gpuva after the woke GPU VA with given &end address.
 *
 * Note that if there is any free space between the woke GPU VA mappings no mapping
 * is returned.
 *
 * Returns: a pointer to the woke found &drm_gpuva or NULL if none was found
 */
struct drm_gpuva *
drm_gpuva_find_next(struct drm_gpuvm *gpuvm, u64 end)
{
	if (!drm_gpuvm_range_valid(gpuvm, end, 1))
		return NULL;

	return drm_gpuva_it_iter_first(&gpuvm->rb.tree, end, end + 1);
}
EXPORT_SYMBOL_GPL(drm_gpuva_find_next);

/**
 * drm_gpuvm_interval_empty() - indicate whether a given interval of the woke VA space
 * is empty
 * @gpuvm: the woke &drm_gpuvm to check the woke range for
 * @addr: the woke start address of the woke range
 * @range: the woke range of the woke interval
 *
 * Returns: true if the woke interval is empty, false otherwise
 */
bool
drm_gpuvm_interval_empty(struct drm_gpuvm *gpuvm, u64 addr, u64 range)
{
	return !drm_gpuva_find_first(gpuvm, addr, range);
}
EXPORT_SYMBOL_GPL(drm_gpuvm_interval_empty);

/**
 * drm_gpuva_map() - helper to insert a &drm_gpuva according to a
 * &drm_gpuva_op_map
 * @gpuvm: the woke &drm_gpuvm
 * @va: the woke &drm_gpuva to insert
 * @op: the woke &drm_gpuva_op_map to initialize @va with
 *
 * Initializes the woke @va from the woke @op and inserts it into the woke given @gpuvm.
 */
void
drm_gpuva_map(struct drm_gpuvm *gpuvm,
	      struct drm_gpuva *va,
	      struct drm_gpuva_op_map *op)
{
	drm_gpuva_init_from_op(va, op);
	drm_gpuva_insert(gpuvm, va);
}
EXPORT_SYMBOL_GPL(drm_gpuva_map);

/**
 * drm_gpuva_remap() - helper to remap a &drm_gpuva according to a
 * &drm_gpuva_op_remap
 * @prev: the woke &drm_gpuva to remap when keeping the woke start of a mapping
 * @next: the woke &drm_gpuva to remap when keeping the woke end of a mapping
 * @op: the woke &drm_gpuva_op_remap to initialize @prev and @next with
 *
 * Removes the woke currently mapped &drm_gpuva and remaps it using @prev and/or
 * @next.
 */
void
drm_gpuva_remap(struct drm_gpuva *prev,
		struct drm_gpuva *next,
		struct drm_gpuva_op_remap *op)
{
	struct drm_gpuva *va = op->unmap->va;
	struct drm_gpuvm *gpuvm = va->vm;

	drm_gpuva_remove(va);

	if (op->prev) {
		drm_gpuva_init_from_op(prev, op->prev);
		drm_gpuva_insert(gpuvm, prev);
	}

	if (op->next) {
		drm_gpuva_init_from_op(next, op->next);
		drm_gpuva_insert(gpuvm, next);
	}
}
EXPORT_SYMBOL_GPL(drm_gpuva_remap);

/**
 * drm_gpuva_unmap() - helper to remove a &drm_gpuva according to a
 * &drm_gpuva_op_unmap
 * @op: the woke &drm_gpuva_op_unmap specifying the woke &drm_gpuva to remove
 *
 * Removes the woke &drm_gpuva associated with the woke &drm_gpuva_op_unmap.
 */
void
drm_gpuva_unmap(struct drm_gpuva_op_unmap *op)
{
	drm_gpuva_remove(op->va);
}
EXPORT_SYMBOL_GPL(drm_gpuva_unmap);

static int
op_map_cb(const struct drm_gpuvm_ops *fn, void *priv,
	  u64 addr, u64 range,
	  struct drm_gem_object *obj, u64 offset)
{
	struct drm_gpuva_op op = {};

	op.op = DRM_GPUVA_OP_MAP;
	op.map.va.addr = addr;
	op.map.va.range = range;
	op.map.gem.obj = obj;
	op.map.gem.offset = offset;

	return fn->sm_step_map(&op, priv);
}

static int
op_remap_cb(const struct drm_gpuvm_ops *fn, void *priv,
	    struct drm_gpuva_op_map *prev,
	    struct drm_gpuva_op_map *next,
	    struct drm_gpuva_op_unmap *unmap)
{
	struct drm_gpuva_op op = {};
	struct drm_gpuva_op_remap *r;

	op.op = DRM_GPUVA_OP_REMAP;
	r = &op.remap;
	r->prev = prev;
	r->next = next;
	r->unmap = unmap;

	return fn->sm_step_remap(&op, priv);
}

static int
op_unmap_cb(const struct drm_gpuvm_ops *fn, void *priv,
	    struct drm_gpuva *va, bool merge)
{
	struct drm_gpuva_op op = {};

	op.op = DRM_GPUVA_OP_UNMAP;
	op.unmap.va = va;
	op.unmap.keep = merge;

	return fn->sm_step_unmap(&op, priv);
}

static int
__drm_gpuvm_sm_map(struct drm_gpuvm *gpuvm,
		   const struct drm_gpuvm_ops *ops, void *priv,
		   u64 req_addr, u64 req_range,
		   struct drm_gem_object *req_obj, u64 req_offset)
{
	struct drm_gpuva *va, *next;
	u64 req_end = req_addr + req_range;
	int ret;

	if (unlikely(!drm_gpuvm_range_valid(gpuvm, req_addr, req_range)))
		return -EINVAL;

	drm_gpuvm_for_each_va_range_safe(va, next, gpuvm, req_addr, req_end) {
		struct drm_gem_object *obj = va->gem.obj;
		u64 offset = va->gem.offset;
		u64 addr = va->va.addr;
		u64 range = va->va.range;
		u64 end = addr + range;
		bool merge = !!va->gem.obj;

		if (addr == req_addr) {
			merge &= obj == req_obj &&
				 offset == req_offset;

			if (end == req_end) {
				ret = op_unmap_cb(ops, priv, va, merge);
				if (ret)
					return ret;
				break;
			}

			if (end < req_end) {
				ret = op_unmap_cb(ops, priv, va, merge);
				if (ret)
					return ret;
				continue;
			}

			if (end > req_end) {
				struct drm_gpuva_op_map n = {
					.va.addr = req_end,
					.va.range = range - req_range,
					.gem.obj = obj,
					.gem.offset = offset + req_range,
				};
				struct drm_gpuva_op_unmap u = {
					.va = va,
					.keep = merge,
				};

				ret = op_remap_cb(ops, priv, NULL, &n, &u);
				if (ret)
					return ret;
				break;
			}
		} else if (addr < req_addr) {
			u64 ls_range = req_addr - addr;
			struct drm_gpuva_op_map p = {
				.va.addr = addr,
				.va.range = ls_range,
				.gem.obj = obj,
				.gem.offset = offset,
			};
			struct drm_gpuva_op_unmap u = { .va = va };

			merge &= obj == req_obj &&
				 offset + ls_range == req_offset;
			u.keep = merge;

			if (end == req_end) {
				ret = op_remap_cb(ops, priv, &p, NULL, &u);
				if (ret)
					return ret;
				break;
			}

			if (end < req_end) {
				ret = op_remap_cb(ops, priv, &p, NULL, &u);
				if (ret)
					return ret;
				continue;
			}

			if (end > req_end) {
				struct drm_gpuva_op_map n = {
					.va.addr = req_end,
					.va.range = end - req_end,
					.gem.obj = obj,
					.gem.offset = offset + ls_range +
						      req_range,
				};

				ret = op_remap_cb(ops, priv, &p, &n, &u);
				if (ret)
					return ret;
				break;
			}
		} else if (addr > req_addr) {
			merge &= obj == req_obj &&
				 offset == req_offset +
					   (addr - req_addr);

			if (end == req_end) {
				ret = op_unmap_cb(ops, priv, va, merge);
				if (ret)
					return ret;
				break;
			}

			if (end < req_end) {
				ret = op_unmap_cb(ops, priv, va, merge);
				if (ret)
					return ret;
				continue;
			}

			if (end > req_end) {
				struct drm_gpuva_op_map n = {
					.va.addr = req_end,
					.va.range = end - req_end,
					.gem.obj = obj,
					.gem.offset = offset + req_end - addr,
				};
				struct drm_gpuva_op_unmap u = {
					.va = va,
					.keep = merge,
				};

				ret = op_remap_cb(ops, priv, NULL, &n, &u);
				if (ret)
					return ret;
				break;
			}
		}
	}

	return op_map_cb(ops, priv,
			 req_addr, req_range,
			 req_obj, req_offset);
}

static int
__drm_gpuvm_sm_unmap(struct drm_gpuvm *gpuvm,
		     const struct drm_gpuvm_ops *ops, void *priv,
		     u64 req_addr, u64 req_range)
{
	struct drm_gpuva *va, *next;
	u64 req_end = req_addr + req_range;
	int ret;

	if (unlikely(!drm_gpuvm_range_valid(gpuvm, req_addr, req_range)))
		return -EINVAL;

	drm_gpuvm_for_each_va_range_safe(va, next, gpuvm, req_addr, req_end) {
		struct drm_gpuva_op_map prev = {}, next = {};
		bool prev_split = false, next_split = false;
		struct drm_gem_object *obj = va->gem.obj;
		u64 offset = va->gem.offset;
		u64 addr = va->va.addr;
		u64 range = va->va.range;
		u64 end = addr + range;

		if (addr < req_addr) {
			prev.va.addr = addr;
			prev.va.range = req_addr - addr;
			prev.gem.obj = obj;
			prev.gem.offset = offset;

			prev_split = true;
		}

		if (end > req_end) {
			next.va.addr = req_end;
			next.va.range = end - req_end;
			next.gem.obj = obj;
			next.gem.offset = offset + (req_end - addr);

			next_split = true;
		}

		if (prev_split || next_split) {
			struct drm_gpuva_op_unmap unmap = { .va = va };

			ret = op_remap_cb(ops, priv,
					  prev_split ? &prev : NULL,
					  next_split ? &next : NULL,
					  &unmap);
			if (ret)
				return ret;
		} else {
			ret = op_unmap_cb(ops, priv, va, false);
			if (ret)
				return ret;
		}
	}

	return 0;
}

/**
 * drm_gpuvm_sm_map() - calls the woke &drm_gpuva_op split/merge steps
 * @gpuvm: the woke &drm_gpuvm representing the woke GPU VA space
 * @priv: pointer to a driver private data structure
 * @req_addr: the woke start address of the woke new mapping
 * @req_range: the woke range of the woke new mapping
 * @req_obj: the woke &drm_gem_object to map
 * @req_offset: the woke offset within the woke &drm_gem_object
 *
 * This function iterates the woke given range of the woke GPU VA space. It utilizes the
 * &drm_gpuvm_ops to call back into the woke driver providing the woke split and merge
 * steps.
 *
 * Drivers may use these callbacks to update the woke GPU VA space right away within
 * the woke callback. In case the woke driver decides to copy and store the woke operations for
 * later processing neither this function nor &drm_gpuvm_sm_unmap is allowed to
 * be called before the woke &drm_gpuvm's view of the woke GPU VA space was
 * updated with the woke previous set of operations. To update the
 * &drm_gpuvm's view of the woke GPU VA space drm_gpuva_insert(),
 * drm_gpuva_destroy_locked() and/or drm_gpuva_destroy_unlocked() should be
 * used.
 *
 * A sequence of callbacks can contain map, unmap and remap operations, but
 * the woke sequence of callbacks might also be empty if no operation is required,
 * e.g. if the woke requested mapping already exists in the woke exact same way.
 *
 * There can be an arbitrary amount of unmap operations, a maximum of two remap
 * operations and a single map operation. The latter one represents the woke original
 * map operation requested by the woke caller.
 *
 * Returns: 0 on success or a negative error code
 */
int
drm_gpuvm_sm_map(struct drm_gpuvm *gpuvm, void *priv,
		 u64 req_addr, u64 req_range,
		 struct drm_gem_object *req_obj, u64 req_offset)
{
	const struct drm_gpuvm_ops *ops = gpuvm->ops;

	if (unlikely(!(ops && ops->sm_step_map &&
		       ops->sm_step_remap &&
		       ops->sm_step_unmap)))
		return -EINVAL;

	return __drm_gpuvm_sm_map(gpuvm, ops, priv,
				  req_addr, req_range,
				  req_obj, req_offset);
}
EXPORT_SYMBOL_GPL(drm_gpuvm_sm_map);

/**
 * drm_gpuvm_sm_unmap() - calls the woke &drm_gpuva_ops to split on unmap
 * @gpuvm: the woke &drm_gpuvm representing the woke GPU VA space
 * @priv: pointer to a driver private data structure
 * @req_addr: the woke start address of the woke range to unmap
 * @req_range: the woke range of the woke mappings to unmap
 *
 * This function iterates the woke given range of the woke GPU VA space. It utilizes the
 * &drm_gpuvm_ops to call back into the woke driver providing the woke operations to
 * unmap and, if required, split existing mappings.
 *
 * Drivers may use these callbacks to update the woke GPU VA space right away within
 * the woke callback. In case the woke driver decides to copy and store the woke operations for
 * later processing neither this function nor &drm_gpuvm_sm_map is allowed to be
 * called before the woke &drm_gpuvm's view of the woke GPU VA space was updated
 * with the woke previous set of operations. To update the woke &drm_gpuvm's view
 * of the woke GPU VA space drm_gpuva_insert(), drm_gpuva_destroy_locked() and/or
 * drm_gpuva_destroy_unlocked() should be used.
 *
 * A sequence of callbacks can contain unmap and remap operations, depending on
 * whether there are actual overlapping mappings to split.
 *
 * There can be an arbitrary amount of unmap operations and a maximum of two
 * remap operations.
 *
 * Returns: 0 on success or a negative error code
 */
int
drm_gpuvm_sm_unmap(struct drm_gpuvm *gpuvm, void *priv,
		   u64 req_addr, u64 req_range)
{
	const struct drm_gpuvm_ops *ops = gpuvm->ops;

	if (unlikely(!(ops && ops->sm_step_remap &&
		       ops->sm_step_unmap)))
		return -EINVAL;

	return __drm_gpuvm_sm_unmap(gpuvm, ops, priv,
				    req_addr, req_range);
}
EXPORT_SYMBOL_GPL(drm_gpuvm_sm_unmap);

static int
drm_gpuva_sm_step_lock(struct drm_gpuva_op *op, void *priv)
{
	struct drm_exec *exec = priv;

	switch (op->op) {
	case DRM_GPUVA_OP_REMAP:
		if (op->remap.unmap->va->gem.obj)
			return drm_exec_lock_obj(exec, op->remap.unmap->va->gem.obj);
		return 0;
	case DRM_GPUVA_OP_UNMAP:
		if (op->unmap.va->gem.obj)
			return drm_exec_lock_obj(exec, op->unmap.va->gem.obj);
		return 0;
	default:
		return 0;
	}
}

static const struct drm_gpuvm_ops lock_ops = {
	.sm_step_map = drm_gpuva_sm_step_lock,
	.sm_step_remap = drm_gpuva_sm_step_lock,
	.sm_step_unmap = drm_gpuva_sm_step_lock,
};

/**
 * drm_gpuvm_sm_map_exec_lock() - locks the woke objects touched by a drm_gpuvm_sm_map()
 * @gpuvm: the woke &drm_gpuvm representing the woke GPU VA space
 * @exec: the woke &drm_exec locking context
 * @num_fences: for newly mapped objects, the woke # of fences to reserve
 * @req_addr: the woke start address of the woke range to unmap
 * @req_range: the woke range of the woke mappings to unmap
 * @req_obj: the woke &drm_gem_object to map
 * @req_offset: the woke offset within the woke &drm_gem_object
 *
 * This function locks (drm_exec_lock_obj()) objects that will be unmapped/
 * remapped, and locks+prepares (drm_exec_prepare_object()) objects that
 * will be newly mapped.
 *
 * The expected usage is::
 *
 * .. code-block:: c
 *
 *    vm_bind {
 *        struct drm_exec exec;
 *
 *        // IGNORE_DUPLICATES is required, INTERRUPTIBLE_WAIT is recommended:
 *        drm_exec_init(&exec, IGNORE_DUPLICATES | INTERRUPTIBLE_WAIT, 0);
 *
 *        drm_exec_until_all_locked (&exec) {
 *            for_each_vm_bind_operation {
 *                switch (op->op) {
 *                case DRIVER_OP_UNMAP:
 *                    ret = drm_gpuvm_sm_unmap_exec_lock(gpuvm, &exec, op->addr, op->range);
 *                    break;
 *                case DRIVER_OP_MAP:
 *                    ret = drm_gpuvm_sm_map_exec_lock(gpuvm, &exec, num_fences,
 *                                                     op->addr, op->range,
 *                                                     obj, op->obj_offset);
 *                    break;
 *                }
 *
 *                drm_exec_retry_on_contention(&exec);
 *                if (ret)
 *                    return ret;
 *            }
 *        }
 *    }
 *
 * This enables all locking to be performed before the woke driver begins modifying
 * the woke VM.  This is safe to do in the woke case of overlapping DRIVER_VM_BIND_OPs,
 * where an earlier op can alter the woke sequence of steps generated for a later
 * op, because the woke later altered step will involve the woke same GEM object(s)
 * already seen in the woke earlier locking step.  For example:
 *
 * 1) An earlier driver DRIVER_OP_UNMAP op removes the woke need for a
 *    DRM_GPUVA_OP_REMAP/UNMAP step.  This is safe because we've already
 *    locked the woke GEM object in the woke earlier DRIVER_OP_UNMAP op.
 *
 * 2) An earlier DRIVER_OP_MAP op overlaps with a later DRIVER_OP_MAP/UNMAP
 *    op, introducing a DRM_GPUVA_OP_REMAP/UNMAP that wouldn't have been
 *    required without the woke earlier DRIVER_OP_MAP.  This is safe because we've
 *    already locked the woke GEM object in the woke earlier DRIVER_OP_MAP step.
 *
 * Returns: 0 on success or a negative error code
 */
int
drm_gpuvm_sm_map_exec_lock(struct drm_gpuvm *gpuvm,
			   struct drm_exec *exec, unsigned int num_fences,
			   u64 req_addr, u64 req_range,
			   struct drm_gem_object *req_obj, u64 req_offset)
{
	if (req_obj) {
		int ret = drm_exec_prepare_obj(exec, req_obj, num_fences);
		if (ret)
			return ret;
	}

	return __drm_gpuvm_sm_map(gpuvm, &lock_ops, exec,
				  req_addr, req_range,
				  req_obj, req_offset);

}
EXPORT_SYMBOL_GPL(drm_gpuvm_sm_map_exec_lock);

/**
 * drm_gpuvm_sm_unmap_exec_lock() - locks the woke objects touched by drm_gpuvm_sm_unmap()
 * @gpuvm: the woke &drm_gpuvm representing the woke GPU VA space
 * @exec: the woke &drm_exec locking context
 * @req_addr: the woke start address of the woke range to unmap
 * @req_range: the woke range of the woke mappings to unmap
 *
 * This function locks (drm_exec_lock_obj()) objects that will be unmapped/
 * remapped by drm_gpuvm_sm_unmap().
 *
 * See drm_gpuvm_sm_map_exec_lock() for expected usage.
 *
 * Returns: 0 on success or a negative error code
 */
int
drm_gpuvm_sm_unmap_exec_lock(struct drm_gpuvm *gpuvm, struct drm_exec *exec,
			     u64 req_addr, u64 req_range)
{
	return __drm_gpuvm_sm_unmap(gpuvm, &lock_ops, exec,
				    req_addr, req_range);
}
EXPORT_SYMBOL_GPL(drm_gpuvm_sm_unmap_exec_lock);

static struct drm_gpuva_op *
gpuva_op_alloc(struct drm_gpuvm *gpuvm)
{
	const struct drm_gpuvm_ops *fn = gpuvm->ops;
	struct drm_gpuva_op *op;

	if (fn && fn->op_alloc)
		op = fn->op_alloc();
	else
		op = kzalloc(sizeof(*op), GFP_KERNEL);

	if (unlikely(!op))
		return NULL;

	return op;
}

static void
gpuva_op_free(struct drm_gpuvm *gpuvm,
	      struct drm_gpuva_op *op)
{
	const struct drm_gpuvm_ops *fn = gpuvm->ops;

	if (fn && fn->op_free)
		fn->op_free(op);
	else
		kfree(op);
}

static int
drm_gpuva_sm_step(struct drm_gpuva_op *__op,
		  void *priv)
{
	struct {
		struct drm_gpuvm *vm;
		struct drm_gpuva_ops *ops;
	} *args = priv;
	struct drm_gpuvm *gpuvm = args->vm;
	struct drm_gpuva_ops *ops = args->ops;
	struct drm_gpuva_op *op;

	op = gpuva_op_alloc(gpuvm);
	if (unlikely(!op))
		goto err;

	memcpy(op, __op, sizeof(*op));

	if (op->op == DRM_GPUVA_OP_REMAP) {
		struct drm_gpuva_op_remap *__r = &__op->remap;
		struct drm_gpuva_op_remap *r = &op->remap;

		r->unmap = kmemdup(__r->unmap, sizeof(*r->unmap),
				   GFP_KERNEL);
		if (unlikely(!r->unmap))
			goto err_free_op;

		if (__r->prev) {
			r->prev = kmemdup(__r->prev, sizeof(*r->prev),
					  GFP_KERNEL);
			if (unlikely(!r->prev))
				goto err_free_unmap;
		}

		if (__r->next) {
			r->next = kmemdup(__r->next, sizeof(*r->next),
					  GFP_KERNEL);
			if (unlikely(!r->next))
				goto err_free_prev;
		}
	}

	list_add_tail(&op->entry, &ops->list);

	return 0;

err_free_unmap:
	kfree(op->remap.unmap);
err_free_prev:
	kfree(op->remap.prev);
err_free_op:
	gpuva_op_free(gpuvm, op);
err:
	return -ENOMEM;
}

static const struct drm_gpuvm_ops gpuvm_list_ops = {
	.sm_step_map = drm_gpuva_sm_step,
	.sm_step_remap = drm_gpuva_sm_step,
	.sm_step_unmap = drm_gpuva_sm_step,
};

/**
 * drm_gpuvm_sm_map_ops_create() - creates the woke &drm_gpuva_ops to split and merge
 * @gpuvm: the woke &drm_gpuvm representing the woke GPU VA space
 * @req_addr: the woke start address of the woke new mapping
 * @req_range: the woke range of the woke new mapping
 * @req_obj: the woke &drm_gem_object to map
 * @req_offset: the woke offset within the woke &drm_gem_object
 *
 * This function creates a list of operations to perform splitting and merging
 * of existing mapping(s) with the woke newly requested one.
 *
 * The list can be iterated with &drm_gpuva_for_each_op and must be processed
 * in the woke given order. It can contain map, unmap and remap operations, but it
 * also can be empty if no operation is required, e.g. if the woke requested mapping
 * already exists in the woke exact same way.
 *
 * There can be an arbitrary amount of unmap operations, a maximum of two remap
 * operations and a single map operation. The latter one represents the woke original
 * map operation requested by the woke caller.
 *
 * Note that before calling this function again with another mapping request it
 * is necessary to update the woke &drm_gpuvm's view of the woke GPU VA space. The
 * previously obtained operations must be either processed or abandoned. To
 * update the woke &drm_gpuvm's view of the woke GPU VA space drm_gpuva_insert(),
 * drm_gpuva_destroy_locked() and/or drm_gpuva_destroy_unlocked() should be
 * used.
 *
 * After the woke caller finished processing the woke returned &drm_gpuva_ops, they must
 * be freed with &drm_gpuva_ops_free.
 *
 * Returns: a pointer to the woke &drm_gpuva_ops on success, an ERR_PTR on failure
 */
struct drm_gpuva_ops *
drm_gpuvm_sm_map_ops_create(struct drm_gpuvm *gpuvm,
			    u64 req_addr, u64 req_range,
			    struct drm_gem_object *req_obj, u64 req_offset)
{
	struct drm_gpuva_ops *ops;
	struct {
		struct drm_gpuvm *vm;
		struct drm_gpuva_ops *ops;
	} args;
	int ret;

	ops = kzalloc(sizeof(*ops), GFP_KERNEL);
	if (unlikely(!ops))
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&ops->list);

	args.vm = gpuvm;
	args.ops = ops;

	ret = __drm_gpuvm_sm_map(gpuvm, &gpuvm_list_ops, &args,
				 req_addr, req_range,
				 req_obj, req_offset);
	if (ret)
		goto err_free_ops;

	return ops;

err_free_ops:
	drm_gpuva_ops_free(gpuvm, ops);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(drm_gpuvm_sm_map_ops_create);

/**
 * drm_gpuvm_sm_unmap_ops_create() - creates the woke &drm_gpuva_ops to split on
 * unmap
 * @gpuvm: the woke &drm_gpuvm representing the woke GPU VA space
 * @req_addr: the woke start address of the woke range to unmap
 * @req_range: the woke range of the woke mappings to unmap
 *
 * This function creates a list of operations to perform unmapping and, if
 * required, splitting of the woke mappings overlapping the woke unmap range.
 *
 * The list can be iterated with &drm_gpuva_for_each_op and must be processed
 * in the woke given order. It can contain unmap and remap operations, depending on
 * whether there are actual overlapping mappings to split.
 *
 * There can be an arbitrary amount of unmap operations and a maximum of two
 * remap operations.
 *
 * Note that before calling this function again with another range to unmap it
 * is necessary to update the woke &drm_gpuvm's view of the woke GPU VA space. The
 * previously obtained operations must be processed or abandoned. To update the
 * &drm_gpuvm's view of the woke GPU VA space drm_gpuva_insert(),
 * drm_gpuva_destroy_locked() and/or drm_gpuva_destroy_unlocked() should be
 * used.
 *
 * After the woke caller finished processing the woke returned &drm_gpuva_ops, they must
 * be freed with &drm_gpuva_ops_free.
 *
 * Returns: a pointer to the woke &drm_gpuva_ops on success, an ERR_PTR on failure
 */
struct drm_gpuva_ops *
drm_gpuvm_sm_unmap_ops_create(struct drm_gpuvm *gpuvm,
			      u64 req_addr, u64 req_range)
{
	struct drm_gpuva_ops *ops;
	struct {
		struct drm_gpuvm *vm;
		struct drm_gpuva_ops *ops;
	} args;
	int ret;

	ops = kzalloc(sizeof(*ops), GFP_KERNEL);
	if (unlikely(!ops))
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&ops->list);

	args.vm = gpuvm;
	args.ops = ops;

	ret = __drm_gpuvm_sm_unmap(gpuvm, &gpuvm_list_ops, &args,
				   req_addr, req_range);
	if (ret)
		goto err_free_ops;

	return ops;

err_free_ops:
	drm_gpuva_ops_free(gpuvm, ops);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(drm_gpuvm_sm_unmap_ops_create);

/**
 * drm_gpuvm_prefetch_ops_create() - creates the woke &drm_gpuva_ops to prefetch
 * @gpuvm: the woke &drm_gpuvm representing the woke GPU VA space
 * @addr: the woke start address of the woke range to prefetch
 * @range: the woke range of the woke mappings to prefetch
 *
 * This function creates a list of operations to perform prefetching.
 *
 * The list can be iterated with &drm_gpuva_for_each_op and must be processed
 * in the woke given order. It can contain prefetch operations.
 *
 * There can be an arbitrary amount of prefetch operations.
 *
 * After the woke caller finished processing the woke returned &drm_gpuva_ops, they must
 * be freed with &drm_gpuva_ops_free.
 *
 * Returns: a pointer to the woke &drm_gpuva_ops on success, an ERR_PTR on failure
 */
struct drm_gpuva_ops *
drm_gpuvm_prefetch_ops_create(struct drm_gpuvm *gpuvm,
			      u64 addr, u64 range)
{
	struct drm_gpuva_ops *ops;
	struct drm_gpuva_op *op;
	struct drm_gpuva *va;
	u64 end = addr + range;
	int ret;

	ops = kzalloc(sizeof(*ops), GFP_KERNEL);
	if (!ops)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&ops->list);

	drm_gpuvm_for_each_va_range(va, gpuvm, addr, end) {
		op = gpuva_op_alloc(gpuvm);
		if (!op) {
			ret = -ENOMEM;
			goto err_free_ops;
		}

		op->op = DRM_GPUVA_OP_PREFETCH;
		op->prefetch.va = va;
		list_add_tail(&op->entry, &ops->list);
	}

	return ops;

err_free_ops:
	drm_gpuva_ops_free(gpuvm, ops);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(drm_gpuvm_prefetch_ops_create);

/**
 * drm_gpuvm_bo_unmap_ops_create() - creates the woke &drm_gpuva_ops to unmap a GEM
 * @vm_bo: the woke &drm_gpuvm_bo abstraction
 *
 * This function creates a list of operations to perform unmapping for every
 * GPUVA attached to a GEM.
 *
 * The list can be iterated with &drm_gpuva_for_each_op and consists out of an
 * arbitrary amount of unmap operations.
 *
 * After the woke caller finished processing the woke returned &drm_gpuva_ops, they must
 * be freed with &drm_gpuva_ops_free.
 *
 * It is the woke callers responsibility to protect the woke GEMs GPUVA list against
 * concurrent access using the woke GEMs dma_resv lock.
 *
 * Returns: a pointer to the woke &drm_gpuva_ops on success, an ERR_PTR on failure
 */
struct drm_gpuva_ops *
drm_gpuvm_bo_unmap_ops_create(struct drm_gpuvm_bo *vm_bo)
{
	struct drm_gpuva_ops *ops;
	struct drm_gpuva_op *op;
	struct drm_gpuva *va;
	int ret;

	drm_gem_gpuva_assert_lock_held(vm_bo->obj);

	ops = kzalloc(sizeof(*ops), GFP_KERNEL);
	if (!ops)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&ops->list);

	drm_gpuvm_bo_for_each_va(va, vm_bo) {
		op = gpuva_op_alloc(vm_bo->vm);
		if (!op) {
			ret = -ENOMEM;
			goto err_free_ops;
		}

		op->op = DRM_GPUVA_OP_UNMAP;
		op->unmap.va = va;
		list_add_tail(&op->entry, &ops->list);
	}

	return ops;

err_free_ops:
	drm_gpuva_ops_free(vm_bo->vm, ops);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(drm_gpuvm_bo_unmap_ops_create);

/**
 * drm_gpuva_ops_free() - free the woke given &drm_gpuva_ops
 * @gpuvm: the woke &drm_gpuvm the woke ops were created for
 * @ops: the woke &drm_gpuva_ops to free
 *
 * Frees the woke given &drm_gpuva_ops structure including all the woke ops associated
 * with it.
 */
void
drm_gpuva_ops_free(struct drm_gpuvm *gpuvm,
		   struct drm_gpuva_ops *ops)
{
	struct drm_gpuva_op *op, *next;

	drm_gpuva_for_each_op_safe(op, next, ops) {
		list_del(&op->entry);

		if (op->op == DRM_GPUVA_OP_REMAP) {
			kfree(op->remap.prev);
			kfree(op->remap.next);
			kfree(op->remap.unmap);
		}

		gpuva_op_free(gpuvm, op);
	}

	kfree(ops);
}
EXPORT_SYMBOL_GPL(drm_gpuva_ops_free);

MODULE_DESCRIPTION("DRM GPUVM");
MODULE_LICENSE("GPL");
