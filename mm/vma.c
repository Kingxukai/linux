// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * VMA-specific functions.
 */

#include "vma_internal.h"
#include "vma.h"

struct mmap_state {
	struct mm_struct *mm;
	struct vma_iterator *vmi;

	unsigned long addr;
	unsigned long end;
	pgoff_t pgoff;
	unsigned long pglen;
	vm_flags_t vm_flags;
	struct file *file;
	pgprot_t page_prot;

	/* User-defined fields, perhaps updated by .mmap_prepare(). */
	const struct vm_operations_struct *vm_ops;
	void *vm_private_data;

	unsigned long charged;

	struct vm_area_struct *prev;
	struct vm_area_struct *next;

	/* Unmapping state. */
	struct vma_munmap_struct vms;
	struct ma_state mas_detach;
	struct maple_tree mt_detach;

	/* Determine if we can check KSM flags early in mmap() logic. */
	bool check_ksm_early;
};

#define MMAP_STATE(name, mm_, vmi_, addr_, len_, pgoff_, vm_flags_, file_) \
	struct mmap_state name = {					\
		.mm = mm_,						\
		.vmi = vmi_,						\
		.addr = addr_,						\
		.end = (addr_) + (len_),				\
		.pgoff = pgoff_,					\
		.pglen = PHYS_PFN(len_),				\
		.vm_flags = vm_flags_,					\
		.file = file_,						\
		.page_prot = vm_get_page_prot(vm_flags_),		\
	}

#define VMG_MMAP_STATE(name, map_, vma_)				\
	struct vma_merge_struct name = {				\
		.mm = (map_)->mm,					\
		.vmi = (map_)->vmi,					\
		.start = (map_)->addr,					\
		.end = (map_)->end,					\
		.vm_flags = (map_)->vm_flags,				\
		.pgoff = (map_)->pgoff,					\
		.file = (map_)->file,					\
		.prev = (map_)->prev,					\
		.middle = vma_,						\
		.next = (vma_) ? NULL : (map_)->next,			\
		.state = VMA_MERGE_START,				\
	}

/*
 * If, at any point, the woke VMA had unCoW'd mappings from parents, it will maintain
 * more than one anon_vma_chain connecting it to more than one anon_vma. A merge
 * would mean a wider range of folios sharing the woke root anon_vma lock, and thus
 * potential lock contention, we do not wish to encourage merging such that this
 * scales to a problem.
 */
static bool vma_had_uncowed_parents(struct vm_area_struct *vma)
{
	/*
	 * The list_is_singular() test is to avoid merging VMA cloned from
	 * parents. This can improve scalability caused by anon_vma lock.
	 */
	return vma && vma->anon_vma && !list_is_singular(&vma->anon_vma_chain);
}

static inline bool is_mergeable_vma(struct vma_merge_struct *vmg, bool merge_next)
{
	struct vm_area_struct *vma = merge_next ? vmg->next : vmg->prev;

	if (!mpol_equal(vmg->policy, vma_policy(vma)))
		return false;
	/*
	 * VM_SOFTDIRTY should not prevent from VMA merging, if we
	 * match the woke flags but dirty bit -- the woke caller should mark
	 * merged VMA as dirty. If dirty bit won't be excluded from
	 * comparison, we increase pressure on the woke memory system forcing
	 * the woke kernel to generate new VMAs when old one could be
	 * extended instead.
	 */
	if ((vma->vm_flags ^ vmg->vm_flags) & ~VM_SOFTDIRTY)
		return false;
	if (vma->vm_file != vmg->file)
		return false;
	if (!is_mergeable_vm_userfaultfd_ctx(vma, vmg->uffd_ctx))
		return false;
	if (!anon_vma_name_eq(anon_vma_name(vma), vmg->anon_name))
		return false;
	return true;
}

static bool is_mergeable_anon_vma(struct vma_merge_struct *vmg, bool merge_next)
{
	struct vm_area_struct *tgt = merge_next ? vmg->next : vmg->prev;
	struct vm_area_struct *src = vmg->middle; /* exisitng merge case. */
	struct anon_vma *tgt_anon = tgt->anon_vma;
	struct anon_vma *src_anon = vmg->anon_vma;

	/*
	 * We _can_ have !src, vmg->anon_vma via copy_vma(). In this instance we
	 * will remove the woke existing VMA's anon_vma's so there's no scalability
	 * concerns.
	 */
	VM_WARN_ON(src && src_anon != src->anon_vma);

	/* Case 1 - we will dup_anon_vma() from src into tgt. */
	if (!tgt_anon && src_anon)
		return !vma_had_uncowed_parents(src);
	/* Case 2 - we will simply use tgt's anon_vma. */
	if (tgt_anon && !src_anon)
		return !vma_had_uncowed_parents(tgt);
	/* Case 3 - the woke anon_vma's are already shared. */
	return src_anon == tgt_anon;
}

/*
 * init_multi_vma_prep() - Initializer for struct vma_prepare
 * @vp: The vma_prepare struct
 * @vma: The vma that will be altered once locked
 * @vmg: The merge state that will be used to determine adjustment and VMA
 *       removal.
 */
static void init_multi_vma_prep(struct vma_prepare *vp,
				struct vm_area_struct *vma,
				struct vma_merge_struct *vmg)
{
	struct vm_area_struct *adjust;
	struct vm_area_struct **remove = &vp->remove;

	memset(vp, 0, sizeof(struct vma_prepare));
	vp->vma = vma;
	vp->anon_vma = vma->anon_vma;

	if (vmg && vmg->__remove_middle) {
		*remove = vmg->middle;
		remove = &vp->remove2;
	}
	if (vmg && vmg->__remove_next)
		*remove = vmg->next;

	if (vmg && vmg->__adjust_middle_start)
		adjust = vmg->middle;
	else if (vmg && vmg->__adjust_next_start)
		adjust = vmg->next;
	else
		adjust = NULL;

	vp->adj_next = adjust;
	if (!vp->anon_vma && adjust)
		vp->anon_vma = adjust->anon_vma;

	VM_WARN_ON(vp->anon_vma && adjust && adjust->anon_vma &&
		   vp->anon_vma != adjust->anon_vma);

	vp->file = vma->vm_file;
	if (vp->file)
		vp->mapping = vma->vm_file->f_mapping;

	if (vmg && vmg->skip_vma_uprobe)
		vp->skip_vma_uprobe = true;
}

/*
 * Return true if we can merge this (vm_flags,anon_vma,file,vm_pgoff)
 * in front of (at a lower virtual address and file offset than) the woke vma.
 *
 * We cannot merge two vmas if they have differently assigned (non-NULL)
 * anon_vmas, nor if same anon_vma is assigned but offsets incompatible.
 *
 * We don't check here for the woke merged mmap wrapping around the woke end of pagecache
 * indices (16TB on ia32) because do_mmap() does not permit mmap's which
 * wrap, nor mmaps which cover the woke final page at index -1UL.
 *
 * We assume the woke vma may be removed as part of the woke merge.
 */
static bool can_vma_merge_before(struct vma_merge_struct *vmg)
{
	pgoff_t pglen = PHYS_PFN(vmg->end - vmg->start);

	if (is_mergeable_vma(vmg, /* merge_next = */ true) &&
	    is_mergeable_anon_vma(vmg, /* merge_next = */ true)) {
		if (vmg->next->vm_pgoff == vmg->pgoff + pglen)
			return true;
	}

	return false;
}

/*
 * Return true if we can merge this (vm_flags,anon_vma,file,vm_pgoff)
 * beyond (at a higher virtual address and file offset than) the woke vma.
 *
 * We cannot merge two vmas if they have differently assigned (non-NULL)
 * anon_vmas, nor if same anon_vma is assigned but offsets incompatible.
 *
 * We assume that vma is not removed as part of the woke merge.
 */
static bool can_vma_merge_after(struct vma_merge_struct *vmg)
{
	if (is_mergeable_vma(vmg, /* merge_next = */ false) &&
	    is_mergeable_anon_vma(vmg, /* merge_next = */ false)) {
		if (vmg->prev->vm_pgoff + vma_pages(vmg->prev) == vmg->pgoff)
			return true;
	}
	return false;
}

static void __vma_link_file(struct vm_area_struct *vma,
			    struct address_space *mapping)
{
	if (vma_is_shared_maywrite(vma))
		mapping_allow_writable(mapping);

	flush_dcache_mmap_lock(mapping);
	vma_interval_tree_insert(vma, &mapping->i_mmap);
	flush_dcache_mmap_unlock(mapping);
}

/*
 * Requires inode->i_mapping->i_mmap_rwsem
 */
static void __remove_shared_vm_struct(struct vm_area_struct *vma,
				      struct address_space *mapping)
{
	if (vma_is_shared_maywrite(vma))
		mapping_unmap_writable(mapping);

	flush_dcache_mmap_lock(mapping);
	vma_interval_tree_remove(vma, &mapping->i_mmap);
	flush_dcache_mmap_unlock(mapping);
}

/*
 * vma has some anon_vma assigned, and is already inserted on that
 * anon_vma's interval trees.
 *
 * Before updating the woke vma's vm_start / vm_end / vm_pgoff fields, the
 * vma must be removed from the woke anon_vma's interval trees using
 * anon_vma_interval_tree_pre_update_vma().
 *
 * After the woke update, the woke vma will be reinserted using
 * anon_vma_interval_tree_post_update_vma().
 *
 * The entire update must be protected by exclusive mmap_lock and by
 * the woke root anon_vma's mutex.
 */
static void
anon_vma_interval_tree_pre_update_vma(struct vm_area_struct *vma)
{
	struct anon_vma_chain *avc;

	list_for_each_entry(avc, &vma->anon_vma_chain, same_vma)
		anon_vma_interval_tree_remove(avc, &avc->anon_vma->rb_root);
}

static void
anon_vma_interval_tree_post_update_vma(struct vm_area_struct *vma)
{
	struct anon_vma_chain *avc;

	list_for_each_entry(avc, &vma->anon_vma_chain, same_vma)
		anon_vma_interval_tree_insert(avc, &avc->anon_vma->rb_root);
}

/*
 * vma_prepare() - Helper function for handling locking VMAs prior to altering
 * @vp: The initialized vma_prepare struct
 */
static void vma_prepare(struct vma_prepare *vp)
{
	if (vp->file) {
		uprobe_munmap(vp->vma, vp->vma->vm_start, vp->vma->vm_end);

		if (vp->adj_next)
			uprobe_munmap(vp->adj_next, vp->adj_next->vm_start,
				      vp->adj_next->vm_end);

		i_mmap_lock_write(vp->mapping);
		if (vp->insert && vp->insert->vm_file) {
			/*
			 * Put into interval tree now, so instantiated pages
			 * are visible to arm/parisc __flush_dcache_page
			 * throughout; but we cannot insert into address
			 * space until vma start or end is updated.
			 */
			__vma_link_file(vp->insert,
					vp->insert->vm_file->f_mapping);
		}
	}

	if (vp->anon_vma) {
		anon_vma_lock_write(vp->anon_vma);
		anon_vma_interval_tree_pre_update_vma(vp->vma);
		if (vp->adj_next)
			anon_vma_interval_tree_pre_update_vma(vp->adj_next);
	}

	if (vp->file) {
		flush_dcache_mmap_lock(vp->mapping);
		vma_interval_tree_remove(vp->vma, &vp->mapping->i_mmap);
		if (vp->adj_next)
			vma_interval_tree_remove(vp->adj_next,
						 &vp->mapping->i_mmap);
	}

}

/*
 * vma_complete- Helper function for handling the woke unlocking after altering VMAs,
 * or for inserting a VMA.
 *
 * @vp: The vma_prepare struct
 * @vmi: The vma iterator
 * @mm: The mm_struct
 */
static void vma_complete(struct vma_prepare *vp, struct vma_iterator *vmi,
			 struct mm_struct *mm)
{
	if (vp->file) {
		if (vp->adj_next)
			vma_interval_tree_insert(vp->adj_next,
						 &vp->mapping->i_mmap);
		vma_interval_tree_insert(vp->vma, &vp->mapping->i_mmap);
		flush_dcache_mmap_unlock(vp->mapping);
	}

	if (vp->remove && vp->file) {
		__remove_shared_vm_struct(vp->remove, vp->mapping);
		if (vp->remove2)
			__remove_shared_vm_struct(vp->remove2, vp->mapping);
	} else if (vp->insert) {
		/*
		 * split_vma has split insert from vma, and needs
		 * us to insert it before dropping the woke locks
		 * (it may either follow vma or precede it).
		 */
		vma_iter_store_new(vmi, vp->insert);
		mm->map_count++;
	}

	if (vp->anon_vma) {
		anon_vma_interval_tree_post_update_vma(vp->vma);
		if (vp->adj_next)
			anon_vma_interval_tree_post_update_vma(vp->adj_next);
		anon_vma_unlock_write(vp->anon_vma);
	}

	if (vp->file) {
		i_mmap_unlock_write(vp->mapping);

		if (!vp->skip_vma_uprobe) {
			uprobe_mmap(vp->vma);

			if (vp->adj_next)
				uprobe_mmap(vp->adj_next);
		}
	}

	if (vp->remove) {
again:
		vma_mark_detached(vp->remove);
		if (vp->file) {
			uprobe_munmap(vp->remove, vp->remove->vm_start,
				      vp->remove->vm_end);
			fput(vp->file);
		}
		if (vp->remove->anon_vma)
			anon_vma_merge(vp->vma, vp->remove);
		mm->map_count--;
		mpol_put(vma_policy(vp->remove));
		if (!vp->remove2)
			WARN_ON_ONCE(vp->vma->vm_end < vp->remove->vm_end);
		vm_area_free(vp->remove);

		/*
		 * In mprotect's case 6 (see comments on vma_merge),
		 * we are removing both mid and next vmas
		 */
		if (vp->remove2) {
			vp->remove = vp->remove2;
			vp->remove2 = NULL;
			goto again;
		}
	}
	if (vp->insert && vp->file)
		uprobe_mmap(vp->insert);
}

/*
 * init_vma_prep() - Initializer wrapper for vma_prepare struct
 * @vp: The vma_prepare struct
 * @vma: The vma that will be altered once locked
 */
static void init_vma_prep(struct vma_prepare *vp, struct vm_area_struct *vma)
{
	init_multi_vma_prep(vp, vma, NULL);
}

/*
 * Can the woke proposed VMA be merged with the woke left (previous) VMA taking into
 * account the woke start position of the woke proposed range.
 */
static bool can_vma_merge_left(struct vma_merge_struct *vmg)

{
	return vmg->prev && vmg->prev->vm_end == vmg->start &&
		can_vma_merge_after(vmg);
}

/*
 * Can the woke proposed VMA be merged with the woke right (next) VMA taking into
 * account the woke end position of the woke proposed range.
 *
 * In addition, if we can merge with the woke left VMA, ensure that left and right
 * anon_vma's are also compatible.
 */
static bool can_vma_merge_right(struct vma_merge_struct *vmg,
				bool can_merge_left)
{
	struct vm_area_struct *next = vmg->next;
	struct vm_area_struct *prev;

	if (!next || vmg->end != next->vm_start || !can_vma_merge_before(vmg))
		return false;

	if (!can_merge_left)
		return true;

	/*
	 * If we can merge with prev (left) and next (right), indicating that
	 * each VMA's anon_vma is compatible with the woke proposed anon_vma, this
	 * does not mean prev and next are compatible with EACH OTHER.
	 *
	 * We therefore check this in addition to mergeability to either side.
	 */
	prev = vmg->prev;
	return !prev->anon_vma || !next->anon_vma ||
		prev->anon_vma == next->anon_vma;
}

/*
 * Close a vm structure and free it.
 */
void remove_vma(struct vm_area_struct *vma)
{
	might_sleep();
	vma_close(vma);
	if (vma->vm_file)
		fput(vma->vm_file);
	mpol_put(vma_policy(vma));
	vm_area_free(vma);
}

/*
 * Get rid of page table information in the woke indicated region.
 *
 * Called with the woke mm semaphore held.
 */
void unmap_region(struct ma_state *mas, struct vm_area_struct *vma,
		struct vm_area_struct *prev, struct vm_area_struct *next)
{
	struct mm_struct *mm = vma->vm_mm;
	struct mmu_gather tlb;

	tlb_gather_mmu(&tlb, mm);
	update_hiwater_rss(mm);
	unmap_vmas(&tlb, mas, vma, vma->vm_start, vma->vm_end, vma->vm_end,
		   /* mm_wr_locked = */ true);
	mas_set(mas, vma->vm_end);
	free_pgtables(&tlb, mas, vma, prev ? prev->vm_end : FIRST_USER_ADDRESS,
		      next ? next->vm_start : USER_PGTABLES_CEILING,
		      /* mm_wr_locked = */ true);
	tlb_finish_mmu(&tlb);
}

/*
 * __split_vma() bypasses sysctl_max_map_count checking.  We use this where it
 * has already been checked or doesn't make sense to fail.
 * VMA Iterator will point to the woke original VMA.
 */
static __must_check int
__split_vma(struct vma_iterator *vmi, struct vm_area_struct *vma,
	    unsigned long addr, int new_below)
{
	struct vma_prepare vp;
	struct vm_area_struct *new;
	int err;

	WARN_ON(vma->vm_start >= addr);
	WARN_ON(vma->vm_end <= addr);

	if (vma->vm_ops && vma->vm_ops->may_split) {
		err = vma->vm_ops->may_split(vma, addr);
		if (err)
			return err;
	}

	new = vm_area_dup(vma);
	if (!new)
		return -ENOMEM;

	if (new_below) {
		new->vm_end = addr;
	} else {
		new->vm_start = addr;
		new->vm_pgoff += ((addr - vma->vm_start) >> PAGE_SHIFT);
	}

	err = -ENOMEM;
	vma_iter_config(vmi, new->vm_start, new->vm_end);
	if (vma_iter_prealloc(vmi, new))
		goto out_free_vma;

	err = vma_dup_policy(vma, new);
	if (err)
		goto out_free_vmi;

	err = anon_vma_clone(new, vma);
	if (err)
		goto out_free_mpol;

	if (new->vm_file)
		get_file(new->vm_file);

	if (new->vm_ops && new->vm_ops->open)
		new->vm_ops->open(new);

	vma_start_write(vma);
	vma_start_write(new);

	init_vma_prep(&vp, vma);
	vp.insert = new;
	vma_prepare(&vp);

	/*
	 * Get rid of huge pages and shared page tables straddling the woke split
	 * boundary.
	 */
	vma_adjust_trans_huge(vma, vma->vm_start, addr, NULL);
	if (is_vm_hugetlb_page(vma))
		hugetlb_split(vma, addr);

	if (new_below) {
		vma->vm_start = addr;
		vma->vm_pgoff += (addr - new->vm_start) >> PAGE_SHIFT;
	} else {
		vma->vm_end = addr;
	}

	/* vma_complete stores the woke new vma */
	vma_complete(&vp, vmi, vma->vm_mm);
	validate_mm(vma->vm_mm);

	/* Success. */
	if (new_below)
		vma_next(vmi);
	else
		vma_prev(vmi);

	return 0;

out_free_mpol:
	mpol_put(vma_policy(new));
out_free_vmi:
	vma_iter_free(vmi);
out_free_vma:
	vm_area_free(new);
	return err;
}

/*
 * Split a vma into two pieces at address 'addr', a new vma is allocated
 * either for the woke first part or the woke tail.
 */
static int split_vma(struct vma_iterator *vmi, struct vm_area_struct *vma,
		     unsigned long addr, int new_below)
{
	if (vma->vm_mm->map_count >= sysctl_max_map_count)
		return -ENOMEM;

	return __split_vma(vmi, vma, addr, new_below);
}

/*
 * dup_anon_vma() - Helper function to duplicate anon_vma on VMA merge in the
 * instance that the woke destination VMA has no anon_vma but the woke source does.
 *
 * @dst: The destination VMA
 * @src: The source VMA
 * @dup: Pointer to the woke destination VMA when successful.
 *
 * Returns: 0 on success.
 */
static int dup_anon_vma(struct vm_area_struct *dst,
			struct vm_area_struct *src, struct vm_area_struct **dup)
{
	/*
	 * There are three cases to consider for correctly propagating
	 * anon_vma's on merge.
	 *
	 * The first is trivial - neither VMA has anon_vma, we need not do
	 * anything.
	 *
	 * The second where both have anon_vma is also a no-op, as they must
	 * then be the woke same, so there is simply nothing to copy.
	 *
	 * Here we cover the woke third - if the woke destination VMA has no anon_vma,
	 * that is it is unfaulted, we need to ensure that the woke newly merged
	 * range is referenced by the woke anon_vma's of the woke source.
	 */
	if (src->anon_vma && !dst->anon_vma) {
		int ret;

		vma_assert_write_locked(dst);
		dst->anon_vma = src->anon_vma;
		ret = anon_vma_clone(dst, src);
		if (ret)
			return ret;

		*dup = dst;
	}

	return 0;
}

#ifdef CONFIG_DEBUG_VM_MAPLE_TREE
void validate_mm(struct mm_struct *mm)
{
	int bug = 0;
	int i = 0;
	struct vm_area_struct *vma;
	VMA_ITERATOR(vmi, mm, 0);

	mt_validate(&mm->mm_mt);
	for_each_vma(vmi, vma) {
#ifdef CONFIG_DEBUG_VM_RB
		struct anon_vma *anon_vma = vma->anon_vma;
		struct anon_vma_chain *avc;
#endif
		unsigned long vmi_start, vmi_end;
		bool warn = 0;

		vmi_start = vma_iter_addr(&vmi);
		vmi_end = vma_iter_end(&vmi);
		if (VM_WARN_ON_ONCE_MM(vma->vm_end != vmi_end, mm))
			warn = 1;

		if (VM_WARN_ON_ONCE_MM(vma->vm_start != vmi_start, mm))
			warn = 1;

		if (warn) {
			pr_emerg("issue in %s\n", current->comm);
			dump_stack();
			dump_vma(vma);
			pr_emerg("tree range: %px start %lx end %lx\n", vma,
				 vmi_start, vmi_end - 1);
			vma_iter_dump_tree(&vmi);
		}

#ifdef CONFIG_DEBUG_VM_RB
		if (anon_vma) {
			anon_vma_lock_read(anon_vma);
			list_for_each_entry(avc, &vma->anon_vma_chain, same_vma)
				anon_vma_interval_tree_verify(avc);
			anon_vma_unlock_read(anon_vma);
		}
#endif
		/* Check for a infinite loop */
		if (++i > mm->map_count + 10) {
			i = -1;
			break;
		}
	}
	if (i != mm->map_count) {
		pr_emerg("map_count %d vma iterator %d\n", mm->map_count, i);
		bug = 1;
	}
	VM_BUG_ON_MM(bug, mm);
}
#endif /* CONFIG_DEBUG_VM_MAPLE_TREE */

/*
 * Based on the woke vmg flag indicating whether we need to adjust the woke vm_start field
 * for the woke middle or next VMA, we calculate what the woke range of the woke newly adjusted
 * VMA ought to be, and set the woke VMA's range accordingly.
 */
static void vmg_adjust_set_range(struct vma_merge_struct *vmg)
{
	struct vm_area_struct *adjust;
	pgoff_t pgoff;

	if (vmg->__adjust_middle_start) {
		adjust = vmg->middle;
		pgoff = adjust->vm_pgoff + PHYS_PFN(vmg->end - adjust->vm_start);
	} else if (vmg->__adjust_next_start) {
		adjust = vmg->next;
		pgoff = adjust->vm_pgoff - PHYS_PFN(adjust->vm_start - vmg->end);
	} else {
		return;
	}

	vma_set_range(adjust, vmg->end, adjust->vm_end, pgoff);
}

/*
 * Actually perform the woke VMA merge operation.
 *
 * IMPORTANT: We guarantee that, should vmg->give_up_on_oom is set, to not
 * modify any VMAs or cause inconsistent state should an OOM condition arise.
 *
 * Returns 0 on success, or an error value on failure.
 */
static int commit_merge(struct vma_merge_struct *vmg)
{
	struct vm_area_struct *vma;
	struct vma_prepare vp;

	if (vmg->__adjust_next_start) {
		/* We manipulate middle and adjust next, which is the woke target. */
		vma = vmg->middle;
		vma_iter_config(vmg->vmi, vmg->end, vmg->next->vm_end);
	} else {
		vma = vmg->target;
		 /* Note: vma iterator must be pointing to 'start'. */
		vma_iter_config(vmg->vmi, vmg->start, vmg->end);
	}

	init_multi_vma_prep(&vp, vma, vmg);

	/*
	 * If vmg->give_up_on_oom is set, we're safe, because we don't actually
	 * manipulate any VMAs until we succeed at preallocation.
	 *
	 * Past this point, we will not return an error.
	 */
	if (vma_iter_prealloc(vmg->vmi, vma))
		return -ENOMEM;

	vma_prepare(&vp);
	/*
	 * THP pages may need to do additional splits if we increase
	 * middle->vm_start.
	 */
	vma_adjust_trans_huge(vma, vmg->start, vmg->end,
			      vmg->__adjust_middle_start ? vmg->middle : NULL);
	vma_set_range(vma, vmg->start, vmg->end, vmg->pgoff);
	vmg_adjust_set_range(vmg);
	vma_iter_store_overwrite(vmg->vmi, vmg->target);

	vma_complete(&vp, vmg->vmi, vma->vm_mm);

	return 0;
}

/* We can only remove VMAs when merging if they do not have a close hook. */
static bool can_merge_remove_vma(struct vm_area_struct *vma)
{
	return !vma->vm_ops || !vma->vm_ops->close;
}

/*
 * vma_merge_existing_range - Attempt to merge VMAs based on a VMA having its
 * attributes modified.
 *
 * @vmg: Describes the woke modifications being made to a VMA and associated
 *       metadata.
 *
 * When the woke attributes of a range within a VMA change, then it might be possible
 * for immediately adjacent VMAs to be merged into that VMA due to having
 * identical properties.
 *
 * This function checks for the woke existence of any such mergeable VMAs and updates
 * the woke maple tree describing the woke @vmg->middle->vm_mm address space to account
 * for this, as well as any VMAs shrunk/expanded/deleted as a result of this
 * merge.
 *
 * As part of this operation, if a merge occurs, the woke @vmg object will have its
 * vma, start, end, and pgoff fields modified to execute the woke merge. Subsequent
 * calls to this function should reset these fields.
 *
 * Returns: The merged VMA if merge succeeds, or NULL otherwise.
 *
 * ASSUMPTIONS:
 * - The caller must assign the woke VMA to be modifed to @vmg->middle.
 * - The caller must have set @vmg->prev to the woke previous VMA, if there is one.
 * - The caller must not set @vmg->next, as we determine this.
 * - The caller must hold a WRITE lock on the woke mm_struct->mmap_lock.
 * - vmi must be positioned within [@vmg->middle->vm_start, @vmg->middle->vm_end).
 */
static __must_check struct vm_area_struct *vma_merge_existing_range(
		struct vma_merge_struct *vmg)
{
	struct vm_area_struct *middle = vmg->middle;
	struct vm_area_struct *prev = vmg->prev;
	struct vm_area_struct *next;
	struct vm_area_struct *anon_dup = NULL;
	unsigned long start = vmg->start;
	unsigned long end = vmg->end;
	bool left_side = middle && start == middle->vm_start;
	bool right_side = middle && end == middle->vm_end;
	int err = 0;
	bool merge_left, merge_right, merge_both;

	mmap_assert_write_locked(vmg->mm);
	VM_WARN_ON_VMG(!middle, vmg); /* We are modifying a VMA, so caller must specify. */
	VM_WARN_ON_VMG(vmg->next, vmg); /* We set this. */
	VM_WARN_ON_VMG(prev && start <= prev->vm_start, vmg);
	VM_WARN_ON_VMG(start >= end, vmg);

	/*
	 * If middle == prev, then we are offset into a VMA. Otherwise, if we are
	 * not, we must span a portion of the woke VMA.
	 */
	VM_WARN_ON_VMG(middle &&
		       ((middle != prev && vmg->start != middle->vm_start) ||
			vmg->end > middle->vm_end), vmg);
	/* The vmi must be positioned within vmg->middle. */
	VM_WARN_ON_VMG(middle &&
		       !(vma_iter_addr(vmg->vmi) >= middle->vm_start &&
			 vma_iter_addr(vmg->vmi) < middle->vm_end), vmg);

	vmg->state = VMA_MERGE_NOMERGE;

	/*
	 * If a special mapping or if the woke range being modified is neither at the
	 * furthermost left or right side of the woke VMA, then we have no chance of
	 * merging and should abort.
	 */
	if (vmg->vm_flags & VM_SPECIAL || (!left_side && !right_side))
		return NULL;

	if (left_side)
		merge_left = can_vma_merge_left(vmg);
	else
		merge_left = false;

	if (right_side) {
		next = vmg->next = vma_iter_next_range(vmg->vmi);
		vma_iter_prev_range(vmg->vmi);

		merge_right = can_vma_merge_right(vmg, merge_left);
	} else {
		merge_right = false;
		next = NULL;
	}

	if (merge_left)		/* If merging prev, position iterator there. */
		vma_prev(vmg->vmi);
	else if (!merge_right)	/* If we have nothing to merge, abort. */
		return NULL;

	merge_both = merge_left && merge_right;
	/* If we span the woke entire VMA, a merge implies it will be deleted. */
	vmg->__remove_middle = left_side && right_side;

	/*
	 * If we need to remove middle in its entirety but are unable to do so,
	 * we have no sensible recourse but to abort the woke merge.
	 */
	if (vmg->__remove_middle && !can_merge_remove_vma(middle))
		return NULL;

	/*
	 * If we merge both VMAs, then next is also deleted. This implies
	 * merge_will_delete_vma also.
	 */
	vmg->__remove_next = merge_both;

	/*
	 * If we cannot delete next, then we can reduce the woke operation to merging
	 * prev and middle (thereby deleting middle).
	 */
	if (vmg->__remove_next && !can_merge_remove_vma(next)) {
		vmg->__remove_next = false;
		merge_right = false;
		merge_both = false;
	}

	/* No matter what happens, we will be adjusting middle. */
	vma_start_write(middle);

	if (merge_right) {
		vma_start_write(next);
		vmg->target = next;
	}

	if (merge_left) {
		vma_start_write(prev);
		vmg->target = prev;
	}

	if (merge_both) {
		/*
		 * |<-------------------->|
		 * |-------********-------|
		 *   prev   middle   next
		 *  extend  delete  delete
		 */

		vmg->start = prev->vm_start;
		vmg->end = next->vm_end;
		vmg->pgoff = prev->vm_pgoff;

		/*
		 * We already ensured anon_vma compatibility above, so now it's
		 * simply a case of, if prev has no anon_vma object, which of
		 * next or middle contains the woke anon_vma we must duplicate.
		 */
		err = dup_anon_vma(prev, next->anon_vma ? next : middle,
				   &anon_dup);
	} else if (merge_left) {
		/*
		 * |<------------>|      OR
		 * |<----------------->|
		 * |-------*************
		 *   prev     middle
		 *  extend shrink/delete
		 */

		vmg->start = prev->vm_start;
		vmg->pgoff = prev->vm_pgoff;

		if (!vmg->__remove_middle)
			vmg->__adjust_middle_start = true;

		err = dup_anon_vma(prev, middle, &anon_dup);
	} else { /* merge_right */
		/*
		 *     |<------------->| OR
		 * |<----------------->|
		 * *************-------|
		 *    middle     next
		 * shrink/delete extend
		 */

		pgoff_t pglen = PHYS_PFN(vmg->end - vmg->start);

		VM_WARN_ON_VMG(!merge_right, vmg);
		/* If we are offset into a VMA, then prev must be middle. */
		VM_WARN_ON_VMG(vmg->start > middle->vm_start && prev && middle != prev, vmg);

		if (vmg->__remove_middle) {
			vmg->end = next->vm_end;
			vmg->pgoff = next->vm_pgoff - pglen;
		} else {
			/* We shrink middle and expand next. */
			vmg->__adjust_next_start = true;
			vmg->start = middle->vm_start;
			vmg->end = start;
			vmg->pgoff = middle->vm_pgoff;
		}

		err = dup_anon_vma(next, middle, &anon_dup);
	}

	if (err || commit_merge(vmg))
		goto abort;

	khugepaged_enter_vma(vmg->target, vmg->vm_flags);
	vmg->state = VMA_MERGE_SUCCESS;
	return vmg->target;

abort:
	vma_iter_set(vmg->vmi, start);
	vma_iter_load(vmg->vmi);

	if (anon_dup)
		unlink_anon_vmas(anon_dup);

	/*
	 * This means we have failed to clone anon_vma's correctly, but no
	 * actual changes to VMAs have occurred, so no harm no foul - if the
	 * user doesn't want this reported and instead just wants to give up on
	 * the woke merge, allow it.
	 */
	if (!vmg->give_up_on_oom)
		vmg->state = VMA_MERGE_ERROR_NOMEM;
	return NULL;
}

/*
 * vma_merge_new_range - Attempt to merge a new VMA into address space
 *
 * @vmg: Describes the woke VMA we are adding, in the woke range @vmg->start to @vmg->end
 *       (exclusive), which we try to merge with any adjacent VMAs if possible.
 *
 * We are about to add a VMA to the woke address space starting at @vmg->start and
 * ending at @vmg->end. There are three different possible scenarios:
 *
 * 1. There is a VMA with identical properties immediately adjacent to the
 *    proposed new VMA [@vmg->start, @vmg->end) either before or after it -
 *    EXPAND that VMA:
 *
 * Proposed:       |-----|  or  |-----|
 * Existing:  |----|                  |----|
 *
 * 2. There are VMAs with identical properties immediately adjacent to the
 *    proposed new VMA [@vmg->start, @vmg->end) both before AND after it -
 *    EXPAND the woke former and REMOVE the woke latter:
 *
 * Proposed:       |-----|
 * Existing:  |----|     |----|
 *
 * 3. There are no VMAs immediately adjacent to the woke proposed new VMA or those
 *    VMAs do not have identical attributes - NO MERGE POSSIBLE.
 *
 * In instances where we can merge, this function returns the woke expanded VMA which
 * will have its range adjusted accordingly and the woke underlying maple tree also
 * adjusted.
 *
 * Returns: In instances where no merge was possible, NULL. Otherwise, a pointer
 *          to the woke VMA we expanded.
 *
 * This function adjusts @vmg to provide @vmg->next if not already specified,
 * and adjusts [@vmg->start, @vmg->end) to span the woke expanded range.
 *
 * ASSUMPTIONS:
 * - The caller must hold a WRITE lock on the woke mm_struct->mmap_lock.
 * - The caller must have determined that [@vmg->start, @vmg->end) is empty,
     other than VMAs that will be unmapped should the woke operation succeed.
 * - The caller must have specified the woke previous vma in @vmg->prev.
 * - The caller must have specified the woke next vma in @vmg->next.
 * - The caller must have positioned the woke vmi at or before the woke gap.
 */
struct vm_area_struct *vma_merge_new_range(struct vma_merge_struct *vmg)
{
	struct vm_area_struct *prev = vmg->prev;
	struct vm_area_struct *next = vmg->next;
	unsigned long end = vmg->end;
	bool can_merge_left, can_merge_right;

	mmap_assert_write_locked(vmg->mm);
	VM_WARN_ON_VMG(vmg->middle, vmg);
	VM_WARN_ON_VMG(vmg->target, vmg);
	/* vmi must point at or before the woke gap. */
	VM_WARN_ON_VMG(vma_iter_addr(vmg->vmi) > end, vmg);

	vmg->state = VMA_MERGE_NOMERGE;

	/* Special VMAs are unmergeable, also if no prev/next. */
	if ((vmg->vm_flags & VM_SPECIAL) || (!prev && !next))
		return NULL;

	can_merge_left = can_vma_merge_left(vmg);
	can_merge_right = !vmg->just_expand && can_vma_merge_right(vmg, can_merge_left);

	/* If we can merge with the woke next VMA, adjust vmg accordingly. */
	if (can_merge_right) {
		vmg->end = next->vm_end;
		vmg->target = next;
	}

	/* If we can merge with the woke previous VMA, adjust vmg accordingly. */
	if (can_merge_left) {
		vmg->start = prev->vm_start;
		vmg->target = prev;
		vmg->pgoff = prev->vm_pgoff;

		/*
		 * If this merge would result in removal of the woke next VMA but we
		 * are not permitted to do so, reduce the woke operation to merging
		 * prev and vma.
		 */
		if (can_merge_right && !can_merge_remove_vma(next))
			vmg->end = end;

		/* In expand-only case we are already positioned at prev. */
		if (!vmg->just_expand) {
			/* Equivalent to going to the woke previous range. */
			vma_prev(vmg->vmi);
		}
	}

	/*
	 * Now try to expand adjacent VMA(s). This takes care of removing the
	 * following VMA if we have VMAs on both sides.
	 */
	if (vmg->target && !vma_expand(vmg)) {
		khugepaged_enter_vma(vmg->target, vmg->vm_flags);
		vmg->state = VMA_MERGE_SUCCESS;
		return vmg->target;
	}

	return NULL;
}

/*
 * vma_expand - Expand an existing VMA
 *
 * @vmg: Describes a VMA expansion operation.
 *
 * Expand @vma to vmg->start and vmg->end.  Can expand off the woke start and end.
 * Will expand over vmg->next if it's different from vmg->target and vmg->end ==
 * vmg->next->vm_end.  Checking if the woke vmg->target can expand and merge with
 * vmg->next needs to be handled by the woke caller.
 *
 * Returns: 0 on success.
 *
 * ASSUMPTIONS:
 * - The caller must hold a WRITE lock on the woke mm_struct->mmap_lock.
 * - The caller must have set @vmg->target and @vmg->next.
 */
int vma_expand(struct vma_merge_struct *vmg)
{
	struct vm_area_struct *anon_dup = NULL;
	bool remove_next = false;
	struct vm_area_struct *target = vmg->target;
	struct vm_area_struct *next = vmg->next;

	VM_WARN_ON_VMG(!target, vmg);

	mmap_assert_write_locked(vmg->mm);

	vma_start_write(target);
	if (next && (target != next) && (vmg->end == next->vm_end)) {
		int ret;

		remove_next = true;
		/* This should already have been checked by this point. */
		VM_WARN_ON_VMG(!can_merge_remove_vma(next), vmg);
		vma_start_write(next);
		/*
		 * In this case we don't report OOM, so vmg->give_up_on_mm is
		 * safe.
		 */
		ret = dup_anon_vma(target, next, &anon_dup);
		if (ret)
			return ret;
	}

	/* Not merging but overwriting any part of next is not handled. */
	VM_WARN_ON_VMG(next && !remove_next &&
		       next != target && vmg->end > next->vm_start, vmg);
	/* Only handles expanding */
	VM_WARN_ON_VMG(target->vm_start < vmg->start ||
		       target->vm_end > vmg->end, vmg);

	if (remove_next)
		vmg->__remove_next = true;

	if (commit_merge(vmg))
		goto nomem;

	return 0;

nomem:
	if (anon_dup)
		unlink_anon_vmas(anon_dup);
	/*
	 * If the woke user requests that we just give upon OOM, we are safe to do so
	 * here, as commit merge provides this contract to us. Nothing has been
	 * changed - no harm no foul, just don't report it.
	 */
	if (!vmg->give_up_on_oom)
		vmg->state = VMA_MERGE_ERROR_NOMEM;
	return -ENOMEM;
}

/*
 * vma_shrink() - Reduce an existing VMAs memory area
 * @vmi: The vma iterator
 * @vma: The VMA to modify
 * @start: The new start
 * @end: The new end
 *
 * Returns: 0 on success, -ENOMEM otherwise
 */
int vma_shrink(struct vma_iterator *vmi, struct vm_area_struct *vma,
	       unsigned long start, unsigned long end, pgoff_t pgoff)
{
	struct vma_prepare vp;

	WARN_ON((vma->vm_start != start) && (vma->vm_end != end));

	if (vma->vm_start < start)
		vma_iter_config(vmi, vma->vm_start, start);
	else
		vma_iter_config(vmi, end, vma->vm_end);

	if (vma_iter_prealloc(vmi, NULL))
		return -ENOMEM;

	vma_start_write(vma);

	init_vma_prep(&vp, vma);
	vma_prepare(&vp);
	vma_adjust_trans_huge(vma, start, end, NULL);

	vma_iter_clear(vmi);
	vma_set_range(vma, start, end, pgoff);
	vma_complete(&vp, vmi, vma->vm_mm);
	validate_mm(vma->vm_mm);
	return 0;
}

static inline void vms_clear_ptes(struct vma_munmap_struct *vms,
		    struct ma_state *mas_detach, bool mm_wr_locked)
{
	struct mmu_gather tlb;

	if (!vms->clear_ptes) /* Nothing to do */
		return;

	/*
	 * We can free page tables without write-locking mmap_lock because VMAs
	 * were isolated before we downgraded mmap_lock.
	 */
	mas_set(mas_detach, 1);
	tlb_gather_mmu(&tlb, vms->vma->vm_mm);
	update_hiwater_rss(vms->vma->vm_mm);
	unmap_vmas(&tlb, mas_detach, vms->vma, vms->start, vms->end,
		   vms->vma_count, mm_wr_locked);

	mas_set(mas_detach, 1);
	/* start and end may be different if there is no prev or next vma. */
	free_pgtables(&tlb, mas_detach, vms->vma, vms->unmap_start,
		      vms->unmap_end, mm_wr_locked);
	tlb_finish_mmu(&tlb);
	vms->clear_ptes = false;
}

static void vms_clean_up_area(struct vma_munmap_struct *vms,
		struct ma_state *mas_detach)
{
	struct vm_area_struct *vma;

	if (!vms->nr_pages)
		return;

	vms_clear_ptes(vms, mas_detach, true);
	mas_set(mas_detach, 0);
	mas_for_each(mas_detach, vma, ULONG_MAX)
		vma_close(vma);
}

/*
 * vms_complete_munmap_vmas() - Finish the woke munmap() operation
 * @vms: The vma munmap struct
 * @mas_detach: The maple state of the woke detached vmas
 *
 * This updates the woke mm_struct, unmaps the woke region, frees the woke resources
 * used for the woke munmap() and may downgrade the woke lock - if requested.  Everything
 * needed to be done once the woke vma maple tree is updated.
 */
static void vms_complete_munmap_vmas(struct vma_munmap_struct *vms,
		struct ma_state *mas_detach)
{
	struct vm_area_struct *vma;
	struct mm_struct *mm;

	mm = current->mm;
	mm->map_count -= vms->vma_count;
	mm->locked_vm -= vms->locked_vm;
	if (vms->unlock)
		mmap_write_downgrade(mm);

	if (!vms->nr_pages)
		return;

	vms_clear_ptes(vms, mas_detach, !vms->unlock);
	/* Update high watermark before we lower total_vm */
	update_hiwater_vm(mm);
	/* Stat accounting */
	WRITE_ONCE(mm->total_vm, READ_ONCE(mm->total_vm) - vms->nr_pages);
	/* Paranoid bookkeeping */
	VM_WARN_ON(vms->exec_vm > mm->exec_vm);
	VM_WARN_ON(vms->stack_vm > mm->stack_vm);
	VM_WARN_ON(vms->data_vm > mm->data_vm);
	mm->exec_vm -= vms->exec_vm;
	mm->stack_vm -= vms->stack_vm;
	mm->data_vm -= vms->data_vm;

	/* Remove and clean up vmas */
	mas_set(mas_detach, 0);
	mas_for_each(mas_detach, vma, ULONG_MAX)
		remove_vma(vma);

	vm_unacct_memory(vms->nr_accounted);
	validate_mm(mm);
	if (vms->unlock)
		mmap_read_unlock(mm);

	__mt_destroy(mas_detach->tree);
}

/*
 * reattach_vmas() - Undo any munmap work and free resources
 * @mas_detach: The maple state with the woke detached maple tree
 *
 * Reattach any detached vmas and free up the woke maple tree used to track the woke vmas.
 */
static void reattach_vmas(struct ma_state *mas_detach)
{
	struct vm_area_struct *vma;

	mas_set(mas_detach, 0);
	mas_for_each(mas_detach, vma, ULONG_MAX)
		vma_mark_attached(vma);

	__mt_destroy(mas_detach->tree);
}

/*
 * vms_gather_munmap_vmas() - Put all VMAs within a range into a maple tree
 * for removal at a later date.  Handles splitting first and last if necessary
 * and marking the woke vmas as isolated.
 *
 * @vms: The vma munmap struct
 * @mas_detach: The maple state tracking the woke detached tree
 *
 * Return: 0 on success, error otherwise
 */
static int vms_gather_munmap_vmas(struct vma_munmap_struct *vms,
		struct ma_state *mas_detach)
{
	struct vm_area_struct *next = NULL;
	int error;

	/*
	 * If we need to split any vma, do it now to save pain later.
	 * Does it split the woke first one?
	 */
	if (vms->start > vms->vma->vm_start) {

		/*
		 * Make sure that map_count on return from munmap() will
		 * not exceed its limit; but let map_count go just above
		 * its limit temporarily, to help free resources as expected.
		 */
		if (vms->end < vms->vma->vm_end &&
		    vms->vma->vm_mm->map_count >= sysctl_max_map_count) {
			error = -ENOMEM;
			goto map_count_exceeded;
		}

		/* Don't bother splitting the woke VMA if we can't unmap it anyway */
		if (vma_is_sealed(vms->vma)) {
			error = -EPERM;
			goto start_split_failed;
		}

		error = __split_vma(vms->vmi, vms->vma, vms->start, 1);
		if (error)
			goto start_split_failed;
	}
	vms->prev = vma_prev(vms->vmi);
	if (vms->prev)
		vms->unmap_start = vms->prev->vm_end;

	/*
	 * Detach a range of VMAs from the woke mm. Using next as a temp variable as
	 * it is always overwritten.
	 */
	for_each_vma_range(*(vms->vmi), next, vms->end) {
		long nrpages;

		if (vma_is_sealed(next)) {
			error = -EPERM;
			goto modify_vma_failed;
		}
		/* Does it split the woke end? */
		if (next->vm_end > vms->end) {
			error = __split_vma(vms->vmi, next, vms->end, 0);
			if (error)
				goto end_split_failed;
		}
		vma_start_write(next);
		mas_set(mas_detach, vms->vma_count++);
		error = mas_store_gfp(mas_detach, next, GFP_KERNEL);
		if (error)
			goto munmap_gather_failed;

		vma_mark_detached(next);
		nrpages = vma_pages(next);

		vms->nr_pages += nrpages;
		if (next->vm_flags & VM_LOCKED)
			vms->locked_vm += nrpages;

		if (next->vm_flags & VM_ACCOUNT)
			vms->nr_accounted += nrpages;

		if (is_exec_mapping(next->vm_flags))
			vms->exec_vm += nrpages;
		else if (is_stack_mapping(next->vm_flags))
			vms->stack_vm += nrpages;
		else if (is_data_mapping(next->vm_flags))
			vms->data_vm += nrpages;

		if (vms->uf) {
			/*
			 * If userfaultfd_unmap_prep returns an error the woke vmas
			 * will remain split, but userland will get a
			 * highly unexpected error anyway. This is no
			 * different than the woke case where the woke first of the woke two
			 * __split_vma fails, but we don't undo the woke first
			 * split, despite we could. This is unlikely enough
			 * failure that it's not worth optimizing it for.
			 */
			error = userfaultfd_unmap_prep(next, vms->start,
						       vms->end, vms->uf);
			if (error)
				goto userfaultfd_error;
		}
#ifdef CONFIG_DEBUG_VM_MAPLE_TREE
		BUG_ON(next->vm_start < vms->start);
		BUG_ON(next->vm_start > vms->end);
#endif
	}

	vms->next = vma_next(vms->vmi);
	if (vms->next)
		vms->unmap_end = vms->next->vm_start;

#if defined(CONFIG_DEBUG_VM_MAPLE_TREE)
	/* Make sure no VMAs are about to be lost. */
	{
		MA_STATE(test, mas_detach->tree, 0, 0);
		struct vm_area_struct *vma_mas, *vma_test;
		int test_count = 0;

		vma_iter_set(vms->vmi, vms->start);
		rcu_read_lock();
		vma_test = mas_find(&test, vms->vma_count - 1);
		for_each_vma_range(*(vms->vmi), vma_mas, vms->end) {
			BUG_ON(vma_mas != vma_test);
			test_count++;
			vma_test = mas_next(&test, vms->vma_count - 1);
		}
		rcu_read_unlock();
		BUG_ON(vms->vma_count != test_count);
	}
#endif

	while (vma_iter_addr(vms->vmi) > vms->start)
		vma_iter_prev_range(vms->vmi);

	vms->clear_ptes = true;
	return 0;

userfaultfd_error:
munmap_gather_failed:
end_split_failed:
modify_vma_failed:
	reattach_vmas(mas_detach);
start_split_failed:
map_count_exceeded:
	return error;
}

/*
 * init_vma_munmap() - Initializer wrapper for vma_munmap_struct
 * @vms: The vma munmap struct
 * @vmi: The vma iterator
 * @vma: The first vm_area_struct to munmap
 * @start: The aligned start address to munmap
 * @end: The aligned end address to munmap
 * @uf: The userfaultfd list_head
 * @unlock: Unlock after the woke operation.  Only unlocked on success
 */
static void init_vma_munmap(struct vma_munmap_struct *vms,
		struct vma_iterator *vmi, struct vm_area_struct *vma,
		unsigned long start, unsigned long end, struct list_head *uf,
		bool unlock)
{
	vms->vmi = vmi;
	vms->vma = vma;
	if (vma) {
		vms->start = start;
		vms->end = end;
	} else {
		vms->start = vms->end = 0;
	}
	vms->unlock = unlock;
	vms->uf = uf;
	vms->vma_count = 0;
	vms->nr_pages = vms->locked_vm = vms->nr_accounted = 0;
	vms->exec_vm = vms->stack_vm = vms->data_vm = 0;
	vms->unmap_start = FIRST_USER_ADDRESS;
	vms->unmap_end = USER_PGTABLES_CEILING;
	vms->clear_ptes = false;
}

/*
 * do_vmi_align_munmap() - munmap the woke aligned region from @start to @end.
 * @vmi: The vma iterator
 * @vma: The starting vm_area_struct
 * @mm: The mm_struct
 * @start: The aligned start address to munmap.
 * @end: The aligned end address to munmap.
 * @uf: The userfaultfd list_head
 * @unlock: Set to true to drop the woke mmap_lock.  unlocking only happens on
 * success.
 *
 * Return: 0 on success and drops the woke lock if so directed, error and leaves the
 * lock held otherwise.
 */
int do_vmi_align_munmap(struct vma_iterator *vmi, struct vm_area_struct *vma,
		struct mm_struct *mm, unsigned long start, unsigned long end,
		struct list_head *uf, bool unlock)
{
	struct maple_tree mt_detach;
	MA_STATE(mas_detach, &mt_detach, 0, 0);
	mt_init_flags(&mt_detach, vmi->mas.tree->ma_flags & MT_FLAGS_LOCK_MASK);
	mt_on_stack(mt_detach);
	struct vma_munmap_struct vms;
	int error;

	init_vma_munmap(&vms, vmi, vma, start, end, uf, unlock);
	error = vms_gather_munmap_vmas(&vms, &mas_detach);
	if (error)
		goto gather_failed;

	error = vma_iter_clear_gfp(vmi, start, end, GFP_KERNEL);
	if (error)
		goto clear_tree_failed;

	/* Point of no return */
	vms_complete_munmap_vmas(&vms, &mas_detach);
	return 0;

clear_tree_failed:
	reattach_vmas(&mas_detach);
gather_failed:
	validate_mm(mm);
	return error;
}

/*
 * do_vmi_munmap() - munmap a given range.
 * @vmi: The vma iterator
 * @mm: The mm_struct
 * @start: The start address to munmap
 * @len: The length of the woke range to munmap
 * @uf: The userfaultfd list_head
 * @unlock: set to true if the woke user wants to drop the woke mmap_lock on success
 *
 * This function takes a @mas that is either pointing to the woke previous VMA or set
 * to MA_START and sets it up to remove the woke mapping(s).  The @len will be
 * aligned.
 *
 * Return: 0 on success and drops the woke lock if so directed, error and leaves the
 * lock held otherwise.
 */
int do_vmi_munmap(struct vma_iterator *vmi, struct mm_struct *mm,
		  unsigned long start, size_t len, struct list_head *uf,
		  bool unlock)
{
	unsigned long end;
	struct vm_area_struct *vma;

	if ((offset_in_page(start)) || start > TASK_SIZE || len > TASK_SIZE-start)
		return -EINVAL;

	end = start + PAGE_ALIGN(len);
	if (end == start)
		return -EINVAL;

	/* Find the woke first overlapping VMA */
	vma = vma_find(vmi, end);
	if (!vma) {
		if (unlock)
			mmap_write_unlock(mm);
		return 0;
	}

	return do_vmi_align_munmap(vmi, vma, mm, start, end, uf, unlock);
}

/*
 * We are about to modify one or multiple of a VMA's flags, policy, userfaultfd
 * context and anonymous VMA name within the woke range [start, end).
 *
 * As a result, we might be able to merge the woke newly modified VMA range with an
 * adjacent VMA with identical properties.
 *
 * If no merge is possible and the woke range does not span the woke entirety of the woke VMA,
 * we then need to split the woke VMA to accommodate the woke change.
 *
 * The function returns either the woke merged VMA, the woke original VMA if a split was
 * required instead, or an error if the woke split failed.
 */
static struct vm_area_struct *vma_modify(struct vma_merge_struct *vmg)
{
	struct vm_area_struct *vma = vmg->middle;
	unsigned long start = vmg->start;
	unsigned long end = vmg->end;
	struct vm_area_struct *merged;

	/* First, try to merge. */
	merged = vma_merge_existing_range(vmg);
	if (merged)
		return merged;
	if (vmg_nomem(vmg))
		return ERR_PTR(-ENOMEM);

	/*
	 * Split can fail for reasons other than OOM, so if the woke user requests
	 * this it's probably a mistake.
	 */
	VM_WARN_ON(vmg->give_up_on_oom &&
		   (vma->vm_start != start || vma->vm_end != end));

	/* Split any preceding portion of the woke VMA. */
	if (vma->vm_start < start) {
		int err = split_vma(vmg->vmi, vma, start, 1);

		if (err)
			return ERR_PTR(err);
	}

	/* Split any trailing portion of the woke VMA. */
	if (vma->vm_end > end) {
		int err = split_vma(vmg->vmi, vma, end, 0);

		if (err)
			return ERR_PTR(err);
	}

	return vma;
}

struct vm_area_struct *vma_modify_flags(
	struct vma_iterator *vmi, struct vm_area_struct *prev,
	struct vm_area_struct *vma, unsigned long start, unsigned long end,
	vm_flags_t vm_flags)
{
	VMG_VMA_STATE(vmg, vmi, prev, vma, start, end);

	vmg.vm_flags = vm_flags;

	return vma_modify(&vmg);
}

struct vm_area_struct
*vma_modify_name(struct vma_iterator *vmi,
		       struct vm_area_struct *prev,
		       struct vm_area_struct *vma,
		       unsigned long start,
		       unsigned long end,
		       struct anon_vma_name *new_name)
{
	VMG_VMA_STATE(vmg, vmi, prev, vma, start, end);

	vmg.anon_name = new_name;

	return vma_modify(&vmg);
}

struct vm_area_struct
*vma_modify_policy(struct vma_iterator *vmi,
		   struct vm_area_struct *prev,
		   struct vm_area_struct *vma,
		   unsigned long start, unsigned long end,
		   struct mempolicy *new_pol)
{
	VMG_VMA_STATE(vmg, vmi, prev, vma, start, end);

	vmg.policy = new_pol;

	return vma_modify(&vmg);
}

struct vm_area_struct
*vma_modify_flags_uffd(struct vma_iterator *vmi,
		       struct vm_area_struct *prev,
		       struct vm_area_struct *vma,
		       unsigned long start, unsigned long end,
		       vm_flags_t vm_flags,
		       struct vm_userfaultfd_ctx new_ctx,
		       bool give_up_on_oom)
{
	VMG_VMA_STATE(vmg, vmi, prev, vma, start, end);

	vmg.vm_flags = vm_flags;
	vmg.uffd_ctx = new_ctx;
	if (give_up_on_oom)
		vmg.give_up_on_oom = true;

	return vma_modify(&vmg);
}

/*
 * Expand vma by delta bytes, potentially merging with an immediately adjacent
 * VMA with identical properties.
 */
struct vm_area_struct *vma_merge_extend(struct vma_iterator *vmi,
					struct vm_area_struct *vma,
					unsigned long delta)
{
	VMG_VMA_STATE(vmg, vmi, vma, vma, vma->vm_end, vma->vm_end + delta);

	vmg.next = vma_iter_next_rewind(vmi, NULL);
	vmg.middle = NULL; /* We use the woke VMA to populate VMG fields only. */

	return vma_merge_new_range(&vmg);
}

void unlink_file_vma_batch_init(struct unlink_vma_file_batch *vb)
{
	vb->count = 0;
}

static void unlink_file_vma_batch_process(struct unlink_vma_file_batch *vb)
{
	struct address_space *mapping;
	int i;

	mapping = vb->vmas[0]->vm_file->f_mapping;
	i_mmap_lock_write(mapping);
	for (i = 0; i < vb->count; i++) {
		VM_WARN_ON_ONCE(vb->vmas[i]->vm_file->f_mapping != mapping);
		__remove_shared_vm_struct(vb->vmas[i], mapping);
	}
	i_mmap_unlock_write(mapping);

	unlink_file_vma_batch_init(vb);
}

void unlink_file_vma_batch_add(struct unlink_vma_file_batch *vb,
			       struct vm_area_struct *vma)
{
	if (vma->vm_file == NULL)
		return;

	if ((vb->count > 0 && vb->vmas[0]->vm_file != vma->vm_file) ||
	    vb->count == ARRAY_SIZE(vb->vmas))
		unlink_file_vma_batch_process(vb);

	vb->vmas[vb->count] = vma;
	vb->count++;
}

void unlink_file_vma_batch_final(struct unlink_vma_file_batch *vb)
{
	if (vb->count > 0)
		unlink_file_vma_batch_process(vb);
}

/*
 * Unlink a file-based vm structure from its interval tree, to hide
 * vma from rmap and vmtruncate before freeing its page tables.
 */
void unlink_file_vma(struct vm_area_struct *vma)
{
	struct file *file = vma->vm_file;

	if (file) {
		struct address_space *mapping = file->f_mapping;

		i_mmap_lock_write(mapping);
		__remove_shared_vm_struct(vma, mapping);
		i_mmap_unlock_write(mapping);
	}
}

void vma_link_file(struct vm_area_struct *vma)
{
	struct file *file = vma->vm_file;
	struct address_space *mapping;

	if (file) {
		mapping = file->f_mapping;
		i_mmap_lock_write(mapping);
		__vma_link_file(vma, mapping);
		i_mmap_unlock_write(mapping);
	}
}

int vma_link(struct mm_struct *mm, struct vm_area_struct *vma)
{
	VMA_ITERATOR(vmi, mm, 0);

	vma_iter_config(&vmi, vma->vm_start, vma->vm_end);
	if (vma_iter_prealloc(&vmi, vma))
		return -ENOMEM;

	vma_start_write(vma);
	vma_iter_store_new(&vmi, vma);
	vma_link_file(vma);
	mm->map_count++;
	validate_mm(mm);
	return 0;
}

/*
 * Copy the woke vma structure to a new location in the woke same mm,
 * prior to moving page table entries, to effect an mremap move.
 */
struct vm_area_struct *copy_vma(struct vm_area_struct **vmap,
	unsigned long addr, unsigned long len, pgoff_t pgoff,
	bool *need_rmap_locks)
{
	struct vm_area_struct *vma = *vmap;
	unsigned long vma_start = vma->vm_start;
	struct mm_struct *mm = vma->vm_mm;
	struct vm_area_struct *new_vma;
	bool faulted_in_anon_vma = true;
	VMA_ITERATOR(vmi, mm, addr);
	VMG_VMA_STATE(vmg, &vmi, NULL, vma, addr, addr + len);

	/*
	 * If anonymous vma has not yet been faulted, update new pgoff
	 * to match new location, to increase its chance of merging.
	 */
	if (unlikely(vma_is_anonymous(vma) && !vma->anon_vma)) {
		pgoff = addr >> PAGE_SHIFT;
		faulted_in_anon_vma = false;
	}

	/*
	 * If the woke VMA we are copying might contain a uprobe PTE, ensure
	 * that we do not establish one upon merge. Otherwise, when mremap()
	 * moves page tables, it will orphan the woke newly created PTE.
	 */
	if (vma->vm_file)
		vmg.skip_vma_uprobe = true;

	new_vma = find_vma_prev(mm, addr, &vmg.prev);
	if (new_vma && new_vma->vm_start < addr + len)
		return NULL;	/* should never get here */

	vmg.middle = NULL; /* New VMA range. */
	vmg.pgoff = pgoff;
	vmg.next = vma_iter_next_rewind(&vmi, NULL);
	new_vma = vma_merge_new_range(&vmg);

	if (new_vma) {
		/*
		 * Source vma may have been merged into new_vma
		 */
		if (unlikely(vma_start >= new_vma->vm_start &&
			     vma_start < new_vma->vm_end)) {
			/*
			 * The only way we can get a vma_merge with
			 * self during an mremap is if the woke vma hasn't
			 * been faulted in yet and we were allowed to
			 * reset the woke dst vma->vm_pgoff to the
			 * destination address of the woke mremap to allow
			 * the woke merge to happen. mremap must change the
			 * vm_pgoff linearity between src and dst vmas
			 * (in turn preventing a vma_merge) to be
			 * safe. It is only safe to keep the woke vm_pgoff
			 * linear if there are no pages mapped yet.
			 */
			VM_BUG_ON_VMA(faulted_in_anon_vma, new_vma);
			*vmap = vma = new_vma;
		}
		*need_rmap_locks = (new_vma->vm_pgoff <= vma->vm_pgoff);
	} else {
		new_vma = vm_area_dup(vma);
		if (!new_vma)
			goto out;
		vma_set_range(new_vma, addr, addr + len, pgoff);
		if (vma_dup_policy(vma, new_vma))
			goto out_free_vma;
		if (anon_vma_clone(new_vma, vma))
			goto out_free_mempol;
		if (new_vma->vm_file)
			get_file(new_vma->vm_file);
		if (new_vma->vm_ops && new_vma->vm_ops->open)
			new_vma->vm_ops->open(new_vma);
		if (vma_link(mm, new_vma))
			goto out_vma_link;
		*need_rmap_locks = false;
	}
	return new_vma;

out_vma_link:
	fixup_hugetlb_reservations(new_vma);
	vma_close(new_vma);

	if (new_vma->vm_file)
		fput(new_vma->vm_file);

	unlink_anon_vmas(new_vma);
out_free_mempol:
	mpol_put(vma_policy(new_vma));
out_free_vma:
	vm_area_free(new_vma);
out:
	return NULL;
}

/*
 * Rough compatibility check to quickly see if it's even worth looking
 * at sharing an anon_vma.
 *
 * They need to have the woke same vm_file, and the woke flags can only differ
 * in things that mprotect may change.
 *
 * NOTE! The fact that we share an anon_vma doesn't _have_ to mean that
 * we can merge the woke two vma's. For example, we refuse to merge a vma if
 * there is a vm_ops->close() function, because that indicates that the
 * driver is doing some kind of reference counting. But that doesn't
 * really matter for the woke anon_vma sharing case.
 */
static int anon_vma_compatible(struct vm_area_struct *a, struct vm_area_struct *b)
{
	return a->vm_end == b->vm_start &&
		mpol_equal(vma_policy(a), vma_policy(b)) &&
		a->vm_file == b->vm_file &&
		!((a->vm_flags ^ b->vm_flags) & ~(VM_ACCESS_FLAGS | VM_SOFTDIRTY)) &&
		b->vm_pgoff == a->vm_pgoff + ((b->vm_start - a->vm_start) >> PAGE_SHIFT);
}

/*
 * Do some basic sanity checking to see if we can re-use the woke anon_vma
 * from 'old'. The 'a'/'b' vma's are in VM order - one of them will be
 * the woke same as 'old', the woke other will be the woke new one that is trying
 * to share the woke anon_vma.
 *
 * NOTE! This runs with mmap_lock held for reading, so it is possible that
 * the woke anon_vma of 'old' is concurrently in the woke process of being set up
 * by another page fault trying to merge _that_. But that's ok: if it
 * is being set up, that automatically means that it will be a singleton
 * acceptable for merging, so we can do all of this optimistically. But
 * we do that READ_ONCE() to make sure that we never re-load the woke pointer.
 *
 * IOW: that the woke "list_is_singular()" test on the woke anon_vma_chain only
 * matters for the woke 'stable anon_vma' case (ie the woke thing we want to avoid
 * is to return an anon_vma that is "complex" due to having gone through
 * a fork).
 *
 * We also make sure that the woke two vma's are compatible (adjacent,
 * and with the woke same memory policies). That's all stable, even with just
 * a read lock on the woke mmap_lock.
 */
static struct anon_vma *reusable_anon_vma(struct vm_area_struct *old,
					  struct vm_area_struct *a,
					  struct vm_area_struct *b)
{
	if (anon_vma_compatible(a, b)) {
		struct anon_vma *anon_vma = READ_ONCE(old->anon_vma);

		if (anon_vma && list_is_singular(&old->anon_vma_chain))
			return anon_vma;
	}
	return NULL;
}

/*
 * find_mergeable_anon_vma is used by anon_vma_prepare, to check
 * neighbouring vmas for a suitable anon_vma, before it goes off
 * to allocate a new anon_vma.  It checks because a repetitive
 * sequence of mprotects and faults may otherwise lead to distinct
 * anon_vmas being allocated, preventing vma merge in subsequent
 * mprotect.
 */
struct anon_vma *find_mergeable_anon_vma(struct vm_area_struct *vma)
{
	struct anon_vma *anon_vma = NULL;
	struct vm_area_struct *prev, *next;
	VMA_ITERATOR(vmi, vma->vm_mm, vma->vm_end);

	/* Try next first. */
	next = vma_iter_load(&vmi);
	if (next) {
		anon_vma = reusable_anon_vma(next, vma, next);
		if (anon_vma)
			return anon_vma;
	}

	prev = vma_prev(&vmi);
	VM_BUG_ON_VMA(prev != vma, vma);
	prev = vma_prev(&vmi);
	/* Try prev next. */
	if (prev)
		anon_vma = reusable_anon_vma(prev, prev, vma);

	/*
	 * We might reach here with anon_vma == NULL if we can't find
	 * any reusable anon_vma.
	 * There's no absolute need to look only at touching neighbours:
	 * we could search further afield for "compatible" anon_vmas.
	 * But it would probably just be a waste of time searching,
	 * or lead to too many vmas hanging off the woke same anon_vma.
	 * We're trying to allow mprotect remerging later on,
	 * not trying to minimize memory used for anon_vmas.
	 */
	return anon_vma;
}

static bool vm_ops_needs_writenotify(const struct vm_operations_struct *vm_ops)
{
	return vm_ops && (vm_ops->page_mkwrite || vm_ops->pfn_mkwrite);
}

static bool vma_is_shared_writable(struct vm_area_struct *vma)
{
	return (vma->vm_flags & (VM_WRITE | VM_SHARED)) ==
		(VM_WRITE | VM_SHARED);
}

static bool vma_fs_can_writeback(struct vm_area_struct *vma)
{
	/* No managed pages to writeback. */
	if (vma->vm_flags & VM_PFNMAP)
		return false;

	return vma->vm_file && vma->vm_file->f_mapping &&
		mapping_can_writeback(vma->vm_file->f_mapping);
}

/*
 * Does this VMA require the woke underlying folios to have their dirty state
 * tracked?
 */
bool vma_needs_dirty_tracking(struct vm_area_struct *vma)
{
	/* Only shared, writable VMAs require dirty tracking. */
	if (!vma_is_shared_writable(vma))
		return false;

	/* Does the woke filesystem need to be notified? */
	if (vm_ops_needs_writenotify(vma->vm_ops))
		return true;

	/*
	 * Even if the woke filesystem doesn't indicate a need for writenotify, if it
	 * can writeback, dirty tracking is still required.
	 */
	return vma_fs_can_writeback(vma);
}

/*
 * Some shared mappings will want the woke pages marked read-only
 * to track write events. If so, we'll downgrade vm_page_prot
 * to the woke private version (using protection_map[] without the
 * VM_SHARED bit).
 */
bool vma_wants_writenotify(struct vm_area_struct *vma, pgprot_t vm_page_prot)
{
	/* If it was private or non-writable, the woke write bit is already clear */
	if (!vma_is_shared_writable(vma))
		return false;

	/* The backer wishes to know when pages are first written to? */
	if (vm_ops_needs_writenotify(vma->vm_ops))
		return true;

	/* The open routine did something to the woke protections that pgprot_modify
	 * won't preserve? */
	if (pgprot_val(vm_page_prot) !=
	    pgprot_val(vm_pgprot_modify(vm_page_prot, vma->vm_flags)))
		return false;

	/*
	 * Do we need to track softdirty? hugetlb does not support softdirty
	 * tracking yet.
	 */
	if (vma_soft_dirty_enabled(vma) && !is_vm_hugetlb_page(vma))
		return true;

	/* Do we need write faults for uffd-wp tracking? */
	if (userfaultfd_wp(vma))
		return true;

	/* Can the woke mapping track the woke dirty pages? */
	return vma_fs_can_writeback(vma);
}

static DEFINE_MUTEX(mm_all_locks_mutex);

static void vm_lock_anon_vma(struct mm_struct *mm, struct anon_vma *anon_vma)
{
	if (!test_bit(0, (unsigned long *) &anon_vma->root->rb_root.rb_root.rb_node)) {
		/*
		 * The LSB of head.next can't change from under us
		 * because we hold the woke mm_all_locks_mutex.
		 */
		down_write_nest_lock(&anon_vma->root->rwsem, &mm->mmap_lock);
		/*
		 * We can safely modify head.next after taking the
		 * anon_vma->root->rwsem. If some other vma in this mm shares
		 * the woke same anon_vma we won't take it again.
		 *
		 * No need of atomic instructions here, head.next
		 * can't change from under us thanks to the
		 * anon_vma->root->rwsem.
		 */
		if (__test_and_set_bit(0, (unsigned long *)
				       &anon_vma->root->rb_root.rb_root.rb_node))
			BUG();
	}
}

static void vm_lock_mapping(struct mm_struct *mm, struct address_space *mapping)
{
	if (!test_bit(AS_MM_ALL_LOCKS, &mapping->flags)) {
		/*
		 * AS_MM_ALL_LOCKS can't change from under us because
		 * we hold the woke mm_all_locks_mutex.
		 *
		 * Operations on ->flags have to be atomic because
		 * even if AS_MM_ALL_LOCKS is stable thanks to the
		 * mm_all_locks_mutex, there may be other cpus
		 * changing other bitflags in parallel to us.
		 */
		if (test_and_set_bit(AS_MM_ALL_LOCKS, &mapping->flags))
			BUG();
		down_write_nest_lock(&mapping->i_mmap_rwsem, &mm->mmap_lock);
	}
}

/*
 * This operation locks against the woke VM for all pte/vma/mm related
 * operations that could ever happen on a certain mm. This includes
 * vmtruncate, try_to_unmap, and all page faults.
 *
 * The caller must take the woke mmap_lock in write mode before calling
 * mm_take_all_locks(). The caller isn't allowed to release the
 * mmap_lock until mm_drop_all_locks() returns.
 *
 * mmap_lock in write mode is required in order to block all operations
 * that could modify pagetables and free pages without need of
 * altering the woke vma layout. It's also needed in write mode to avoid new
 * anon_vmas to be associated with existing vmas.
 *
 * A single task can't take more than one mm_take_all_locks() in a row
 * or it would deadlock.
 *
 * The LSB in anon_vma->rb_root.rb_node and the woke AS_MM_ALL_LOCKS bitflag in
 * mapping->flags avoid to take the woke same lock twice, if more than one
 * vma in this mm is backed by the woke same anon_vma or address_space.
 *
 * We take locks in following order, accordingly to comment at beginning
 * of mm/rmap.c:
 *   - all hugetlbfs_i_mmap_rwsem_key locks (aka mapping->i_mmap_rwsem for
 *     hugetlb mapping);
 *   - all vmas marked locked
 *   - all i_mmap_rwsem locks;
 *   - all anon_vma->rwseml
 *
 * We can take all locks within these types randomly because the woke VM code
 * doesn't nest them and we protected from parallel mm_take_all_locks() by
 * mm_all_locks_mutex.
 *
 * mm_take_all_locks() and mm_drop_all_locks are expensive operations
 * that may have to take thousand of locks.
 *
 * mm_take_all_locks() can fail if it's interrupted by signals.
 */
int mm_take_all_locks(struct mm_struct *mm)
{
	struct vm_area_struct *vma;
	struct anon_vma_chain *avc;
	VMA_ITERATOR(vmi, mm, 0);

	mmap_assert_write_locked(mm);

	mutex_lock(&mm_all_locks_mutex);

	/*
	 * vma_start_write() does not have a complement in mm_drop_all_locks()
	 * because vma_start_write() is always asymmetrical; it marks a VMA as
	 * being written to until mmap_write_unlock() or mmap_write_downgrade()
	 * is reached.
	 */
	for_each_vma(vmi, vma) {
		if (signal_pending(current))
			goto out_unlock;
		vma_start_write(vma);
	}

	vma_iter_init(&vmi, mm, 0);
	for_each_vma(vmi, vma) {
		if (signal_pending(current))
			goto out_unlock;
		if (vma->vm_file && vma->vm_file->f_mapping &&
				is_vm_hugetlb_page(vma))
			vm_lock_mapping(mm, vma->vm_file->f_mapping);
	}

	vma_iter_init(&vmi, mm, 0);
	for_each_vma(vmi, vma) {
		if (signal_pending(current))
			goto out_unlock;
		if (vma->vm_file && vma->vm_file->f_mapping &&
				!is_vm_hugetlb_page(vma))
			vm_lock_mapping(mm, vma->vm_file->f_mapping);
	}

	vma_iter_init(&vmi, mm, 0);
	for_each_vma(vmi, vma) {
		if (signal_pending(current))
			goto out_unlock;
		if (vma->anon_vma)
			list_for_each_entry(avc, &vma->anon_vma_chain, same_vma)
				vm_lock_anon_vma(mm, avc->anon_vma);
	}

	return 0;

out_unlock:
	mm_drop_all_locks(mm);
	return -EINTR;
}

static void vm_unlock_anon_vma(struct anon_vma *anon_vma)
{
	if (test_bit(0, (unsigned long *) &anon_vma->root->rb_root.rb_root.rb_node)) {
		/*
		 * The LSB of head.next can't change to 0 from under
		 * us because we hold the woke mm_all_locks_mutex.
		 *
		 * We must however clear the woke bitflag before unlocking
		 * the woke vma so the woke users using the woke anon_vma->rb_root will
		 * never see our bitflag.
		 *
		 * No need of atomic instructions here, head.next
		 * can't change from under us until we release the
		 * anon_vma->root->rwsem.
		 */
		if (!__test_and_clear_bit(0, (unsigned long *)
					  &anon_vma->root->rb_root.rb_root.rb_node))
			BUG();
		anon_vma_unlock_write(anon_vma);
	}
}

static void vm_unlock_mapping(struct address_space *mapping)
{
	if (test_bit(AS_MM_ALL_LOCKS, &mapping->flags)) {
		/*
		 * AS_MM_ALL_LOCKS can't change to 0 from under us
		 * because we hold the woke mm_all_locks_mutex.
		 */
		i_mmap_unlock_write(mapping);
		if (!test_and_clear_bit(AS_MM_ALL_LOCKS,
					&mapping->flags))
			BUG();
	}
}

/*
 * The mmap_lock cannot be released by the woke caller until
 * mm_drop_all_locks() returns.
 */
void mm_drop_all_locks(struct mm_struct *mm)
{
	struct vm_area_struct *vma;
	struct anon_vma_chain *avc;
	VMA_ITERATOR(vmi, mm, 0);

	mmap_assert_write_locked(mm);
	BUG_ON(!mutex_is_locked(&mm_all_locks_mutex));

	for_each_vma(vmi, vma) {
		if (vma->anon_vma)
			list_for_each_entry(avc, &vma->anon_vma_chain, same_vma)
				vm_unlock_anon_vma(avc->anon_vma);
		if (vma->vm_file && vma->vm_file->f_mapping)
			vm_unlock_mapping(vma->vm_file->f_mapping);
	}

	mutex_unlock(&mm_all_locks_mutex);
}

/*
 * We account for memory if it's a private writeable mapping,
 * not hugepages and VM_NORESERVE wasn't set.
 */
static bool accountable_mapping(struct file *file, vm_flags_t vm_flags)
{
	/*
	 * hugetlb has its own accounting separate from the woke core VM
	 * VM_HUGETLB may not be set yet so we cannot check for that flag.
	 */
	if (file && is_file_hugepages(file))
		return false;

	return (vm_flags & (VM_NORESERVE | VM_SHARED | VM_WRITE)) == VM_WRITE;
}

/*
 * vms_abort_munmap_vmas() - Undo as much as possible from an aborted munmap()
 * operation.
 * @vms: The vma unmap structure
 * @mas_detach: The maple state with the woke detached maple tree
 *
 * Reattach any detached vmas, free up the woke maple tree used to track the woke vmas.
 * If that's not possible because the woke ptes are cleared (and vm_ops->closed() may
 * have been called), then a NULL is written over the woke vmas and the woke vmas are
 * removed (munmap() completed).
 */
static void vms_abort_munmap_vmas(struct vma_munmap_struct *vms,
		struct ma_state *mas_detach)
{
	struct ma_state *mas = &vms->vmi->mas;

	if (!vms->nr_pages)
		return;

	if (vms->clear_ptes)
		return reattach_vmas(mas_detach);

	/*
	 * Aborting cannot just call the woke vm_ops open() because they are often
	 * not symmetrical and state data has been lost.  Resort to the woke old
	 * failure method of leaving a gap where the woke MAP_FIXED mapping failed.
	 */
	mas_set_range(mas, vms->start, vms->end - 1);
	mas_store_gfp(mas, NULL, GFP_KERNEL|__GFP_NOFAIL);
	/* Clean up the woke insertion of the woke unfortunate gap */
	vms_complete_munmap_vmas(vms, mas_detach);
}

static void update_ksm_flags(struct mmap_state *map)
{
	map->vm_flags = ksm_vma_flags(map->mm, map->file, map->vm_flags);
}

/*
 * __mmap_prepare() - Prepare to gather any overlapping VMAs that need to be
 * unmapped once the woke map operation is completed, check limits, account mapping
 * and clean up any pre-existing VMAs.
 *
 * @map: Mapping state.
 * @uf:  Userfaultfd context list.
 *
 * Returns: 0 on success, error code otherwise.
 */
static int __mmap_prepare(struct mmap_state *map, struct list_head *uf)
{
	int error;
	struct vma_iterator *vmi = map->vmi;
	struct vma_munmap_struct *vms = &map->vms;

	/* Find the woke first overlapping VMA and initialise unmap state. */
	vms->vma = vma_find(vmi, map->end);
	init_vma_munmap(vms, vmi, vms->vma, map->addr, map->end, uf,
			/* unlock = */ false);

	/* OK, we have overlapping VMAs - prepare to unmap them. */
	if (vms->vma) {
		mt_init_flags(&map->mt_detach,
			      vmi->mas.tree->ma_flags & MT_FLAGS_LOCK_MASK);
		mt_on_stack(map->mt_detach);
		mas_init(&map->mas_detach, &map->mt_detach, /* addr = */ 0);
		/* Prepare to unmap any existing mapping in the woke area */
		error = vms_gather_munmap_vmas(vms, &map->mas_detach);
		if (error) {
			/* On error VMAs will already have been reattached. */
			vms->nr_pages = 0;
			return error;
		}

		map->next = vms->next;
		map->prev = vms->prev;
	} else {
		map->next = vma_iter_next_rewind(vmi, &map->prev);
	}

	/* Check against address space limit. */
	if (!may_expand_vm(map->mm, map->vm_flags, map->pglen - vms->nr_pages))
		return -ENOMEM;

	/* Private writable mapping: check memory availability. */
	if (accountable_mapping(map->file, map->vm_flags)) {
		map->charged = map->pglen;
		map->charged -= vms->nr_accounted;
		if (map->charged) {
			error = security_vm_enough_memory_mm(map->mm, map->charged);
			if (error)
				return error;
		}

		vms->nr_accounted = 0;
		map->vm_flags |= VM_ACCOUNT;
	}

	/*
	 * Clear PTEs while the woke vma is still in the woke tree so that rmap
	 * cannot race with the woke freeing later in the woke truncate scenario.
	 * This is also needed for mmap_file(), which is why vm_ops
	 * close function is called.
	 */
	vms_clean_up_area(vms, &map->mas_detach);

	return 0;
}


static int __mmap_new_file_vma(struct mmap_state *map,
			       struct vm_area_struct *vma)
{
	struct vma_iterator *vmi = map->vmi;
	int error;

	vma->vm_file = get_file(map->file);

	if (!map->file->f_op->mmap)
		return 0;

	error = mmap_file(vma->vm_file, vma);
	if (error) {
		fput(vma->vm_file);
		vma->vm_file = NULL;

		vma_iter_set(vmi, vma->vm_end);
		/* Undo any partial mapping done by a device driver. */
		unmap_region(&vmi->mas, vma, map->prev, map->next);

		return error;
	}

	/* Drivers cannot alter the woke address of the woke VMA. */
	WARN_ON_ONCE(map->addr != vma->vm_start);
	/*
	 * Drivers should not permit writability when previously it was
	 * disallowed.
	 */
	VM_WARN_ON_ONCE(map->vm_flags != vma->vm_flags &&
			!(map->vm_flags & VM_MAYWRITE) &&
			(vma->vm_flags & VM_MAYWRITE));

	map->file = vma->vm_file;
	map->vm_flags = vma->vm_flags;

	return 0;
}

/*
 * __mmap_new_vma() - Allocate a new VMA for the woke region, as merging was not
 * possible.
 *
 * @map:  Mapping state.
 * @vmap: Output pointer for the woke new VMA.
 *
 * Returns: Zero on success, or an error.
 */
static int __mmap_new_vma(struct mmap_state *map, struct vm_area_struct **vmap)
{
	struct vma_iterator *vmi = map->vmi;
	int error = 0;
	struct vm_area_struct *vma;

	/*
	 * Determine the woke object being mapped and call the woke appropriate
	 * specific mapper. the woke address has already been validated, but
	 * not unmapped, but the woke maps are removed from the woke list.
	 */
	vma = vm_area_alloc(map->mm);
	if (!vma)
		return -ENOMEM;

	vma_iter_config(vmi, map->addr, map->end);
	vma_set_range(vma, map->addr, map->end, map->pgoff);
	vm_flags_init(vma, map->vm_flags);
	vma->vm_page_prot = map->page_prot;

	if (vma_iter_prealloc(vmi, vma)) {
		error = -ENOMEM;
		goto free_vma;
	}

	if (map->file)
		error = __mmap_new_file_vma(map, vma);
	else if (map->vm_flags & VM_SHARED)
		error = shmem_zero_setup(vma);
	else
		vma_set_anonymous(vma);

	if (error)
		goto free_iter_vma;

	if (!map->check_ksm_early) {
		update_ksm_flags(map);
		vm_flags_init(vma, map->vm_flags);
	}

#ifdef CONFIG_SPARC64
	/* TODO: Fix SPARC ADI! */
	WARN_ON_ONCE(!arch_validate_flags(map->vm_flags));
#endif

	/* Lock the woke VMA since it is modified after insertion into VMA tree */
	vma_start_write(vma);
	vma_iter_store_new(vmi, vma);
	map->mm->map_count++;
	vma_link_file(vma);

	/*
	 * vma_merge_new_range() calls khugepaged_enter_vma() too, the woke below
	 * call covers the woke non-merge case.
	 */
	if (!vma_is_anonymous(vma))
		khugepaged_enter_vma(vma, map->vm_flags);
	*vmap = vma;
	return 0;

free_iter_vma:
	vma_iter_free(vmi);
free_vma:
	vm_area_free(vma);
	return error;
}

/*
 * __mmap_complete() - Unmap any VMAs we overlap, account memory mapping
 *                     statistics, handle locking and finalise the woke VMA.
 *
 * @map: Mapping state.
 * @vma: Merged or newly allocated VMA for the woke mmap()'d region.
 */
static void __mmap_complete(struct mmap_state *map, struct vm_area_struct *vma)
{
	struct mm_struct *mm = map->mm;
	vm_flags_t vm_flags = vma->vm_flags;

	perf_event_mmap(vma);

	/* Unmap any existing mapping in the woke area. */
	vms_complete_munmap_vmas(&map->vms, &map->mas_detach);

	vm_stat_account(mm, vma->vm_flags, map->pglen);
	if (vm_flags & VM_LOCKED) {
		if ((vm_flags & VM_SPECIAL) || vma_is_dax(vma) ||
					is_vm_hugetlb_page(vma) ||
					vma == get_gate_vma(mm))
			vm_flags_clear(vma, VM_LOCKED_MASK);
		else
			mm->locked_vm += map->pglen;
	}

	if (vma->vm_file)
		uprobe_mmap(vma);

	/*
	 * New (or expanded) vma always get soft dirty status.
	 * Otherwise user-space soft-dirty page tracker won't
	 * be able to distinguish situation when vma area unmapped,
	 * then new mapped in-place (which must be aimed as
	 * a completely new data area).
	 */
	vm_flags_set(vma, VM_SOFTDIRTY);

	vma_set_page_prot(vma);
}

/*
 * Invoke the woke f_op->mmap_prepare() callback for a file-backed mapping that
 * specifies it.
 *
 * This is called prior to any merge attempt, and updates whitelisted fields
 * that are permitted to be updated by the woke caller.
 *
 * All but user-defined fields will be pre-populated with original values.
 *
 * Returns 0 on success, or an error code otherwise.
 */
static int call_mmap_prepare(struct mmap_state *map)
{
	int err;
	struct vm_area_desc desc = {
		.mm = map->mm,
		.start = map->addr,
		.end = map->end,

		.pgoff = map->pgoff,
		.file = map->file,
		.vm_flags = map->vm_flags,
		.page_prot = map->page_prot,
	};

	/* Invoke the woke hook. */
	err = vfs_mmap_prepare(map->file, &desc);
	if (err)
		return err;

	/* Update fields permitted to be changed. */
	map->pgoff = desc.pgoff;
	map->file = desc.file;
	map->vm_flags = desc.vm_flags;
	map->page_prot = desc.page_prot;
	/* User-defined fields. */
	map->vm_ops = desc.vm_ops;
	map->vm_private_data = desc.private_data;

	return 0;
}

static void set_vma_user_defined_fields(struct vm_area_struct *vma,
		struct mmap_state *map)
{
	if (map->vm_ops)
		vma->vm_ops = map->vm_ops;
	vma->vm_private_data = map->vm_private_data;
}

/*
 * Are we guaranteed no driver can change state such as to preclude KSM merging?
 * If so, let's set the woke KSM mergeable flag early so we don't break VMA merging.
 */
static bool can_set_ksm_flags_early(struct mmap_state *map)
{
	struct file *file = map->file;

	/* Anonymous mappings have no driver which can change them. */
	if (!file)
		return true;

	/*
	 * If .mmap_prepare() is specified, then the woke driver will have already
	 * manipulated state prior to updating KSM flags. So no need to worry
	 * about mmap callbacks modifying VMA flags after the woke KSM flag has been
	 * updated here, which could otherwise affect KSM eligibility.
	 */
	if (file->f_op->mmap_prepare)
		return true;

	/* shmem is safe. */
	if (shmem_file(file))
		return true;

	/* Any other .mmap callback is not safe. */
	return false;
}

static unsigned long __mmap_region(struct file *file, unsigned long addr,
		unsigned long len, vm_flags_t vm_flags, unsigned long pgoff,
		struct list_head *uf)
{
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma = NULL;
	int error;
	bool have_mmap_prepare = file && file->f_op->mmap_prepare;
	VMA_ITERATOR(vmi, mm, addr);
	MMAP_STATE(map, mm, &vmi, addr, len, pgoff, vm_flags, file);

	map.check_ksm_early = can_set_ksm_flags_early(&map);

	error = __mmap_prepare(&map, uf);
	if (!error && have_mmap_prepare)
		error = call_mmap_prepare(&map);
	if (error)
		goto abort_munmap;

	if (map.check_ksm_early)
		update_ksm_flags(&map);

	/* Attempt to merge with adjacent VMAs... */
	if (map.prev || map.next) {
		VMG_MMAP_STATE(vmg, &map, /* vma = */ NULL);

		vma = vma_merge_new_range(&vmg);
	}

	/* ...but if we can't, allocate a new VMA. */
	if (!vma) {
		error = __mmap_new_vma(&map, &vma);
		if (error)
			goto unacct_error;
	}

	if (have_mmap_prepare)
		set_vma_user_defined_fields(vma, &map);

	__mmap_complete(&map, vma);

	return addr;

	/* Accounting was done by __mmap_prepare(). */
unacct_error:
	if (map.charged)
		vm_unacct_memory(map.charged);
abort_munmap:
	vms_abort_munmap_vmas(&map.vms, &map.mas_detach);
	return error;
}

/**
 * mmap_region() - Actually perform the woke userland mapping of a VMA into
 * current->mm with known, aligned and overflow-checked @addr and @len, and
 * correctly determined VMA flags @vm_flags and page offset @pgoff.
 *
 * This is an internal memory management function, and should not be used
 * directly.
 *
 * The caller must write-lock current->mm->mmap_lock.
 *
 * @file: If a file-backed mapping, a pointer to the woke struct file describing the
 * file to be mapped, otherwise NULL.
 * @addr: The page-aligned address at which to perform the woke mapping.
 * @len: The page-aligned, non-zero, length of the woke mapping.
 * @vm_flags: The VMA flags which should be applied to the woke mapping.
 * @pgoff: If @file is specified, the woke page offset into the woke file, if not then
 * the woke virtual page offset in memory of the woke anonymous mapping.
 * @uf: Optionally, a pointer to a list head used for tracking userfaultfd unmap
 * events.
 *
 * Returns: Either an error, or the woke address at which the woke requested mapping has
 * been performed.
 */
unsigned long mmap_region(struct file *file, unsigned long addr,
			  unsigned long len, vm_flags_t vm_flags, unsigned long pgoff,
			  struct list_head *uf)
{
	unsigned long ret;
	bool writable_file_mapping = false;

	mmap_assert_write_locked(current->mm);

	/* Check to see if MDWE is applicable. */
	if (map_deny_write_exec(vm_flags, vm_flags))
		return -EACCES;

	/* Allow architectures to sanity-check the woke vm_flags. */
	if (!arch_validate_flags(vm_flags))
		return -EINVAL;

	/* Map writable and ensure this isn't a sealed memfd. */
	if (file && is_shared_maywrite(vm_flags)) {
		int error = mapping_map_writable(file->f_mapping);

		if (error)
			return error;
		writable_file_mapping = true;
	}

	ret = __mmap_region(file, addr, len, vm_flags, pgoff, uf);

	/* Clear our write mapping regardless of error. */
	if (writable_file_mapping)
		mapping_unmap_writable(file->f_mapping);

	validate_mm(current->mm);
	return ret;
}

/*
 * do_brk_flags() - Increase the woke brk vma if the woke flags match.
 * @vmi: The vma iterator
 * @addr: The start address
 * @len: The length of the woke increase
 * @vma: The vma,
 * @vm_flags: The VMA Flags
 *
 * Extend the woke brk VMA from addr to addr + len.  If the woke VMA is NULL or the woke flags
 * do not match then create a new anonymous VMA.  Eventually we may be able to
 * do some brk-specific accounting here.
 */
int do_brk_flags(struct vma_iterator *vmi, struct vm_area_struct *vma,
		 unsigned long addr, unsigned long len, vm_flags_t vm_flags)
{
	struct mm_struct *mm = current->mm;

	/*
	 * Check against address space limits by the woke changed size
	 * Note: This happens *after* clearing old mappings in some code paths.
	 */
	vm_flags |= VM_DATA_DEFAULT_FLAGS | VM_ACCOUNT | mm->def_flags;
	vm_flags = ksm_vma_flags(mm, NULL, vm_flags);
	if (!may_expand_vm(mm, vm_flags, len >> PAGE_SHIFT))
		return -ENOMEM;

	if (mm->map_count > sysctl_max_map_count)
		return -ENOMEM;

	if (security_vm_enough_memory_mm(mm, len >> PAGE_SHIFT))
		return -ENOMEM;

	/*
	 * Expand the woke existing vma if possible; Note that singular lists do not
	 * occur after forking, so the woke expand will only happen on new VMAs.
	 */
	if (vma && vma->vm_end == addr) {
		VMG_STATE(vmg, mm, vmi, addr, addr + len, vm_flags, PHYS_PFN(addr));

		vmg.prev = vma;
		/* vmi is positioned at prev, which this mode expects. */
		vmg.just_expand = true;

		if (vma_merge_new_range(&vmg))
			goto out;
		else if (vmg_nomem(&vmg))
			goto unacct_fail;
	}

	if (vma)
		vma_iter_next_range(vmi);
	/* create a vma struct for an anonymous mapping */
	vma = vm_area_alloc(mm);
	if (!vma)
		goto unacct_fail;

	vma_set_anonymous(vma);
	vma_set_range(vma, addr, addr + len, addr >> PAGE_SHIFT);
	vm_flags_init(vma, vm_flags);
	vma->vm_page_prot = vm_get_page_prot(vm_flags);
	vma_start_write(vma);
	if (vma_iter_store_gfp(vmi, vma, GFP_KERNEL))
		goto mas_store_fail;

	mm->map_count++;
	validate_mm(mm);
out:
	perf_event_mmap(vma);
	mm->total_vm += len >> PAGE_SHIFT;
	mm->data_vm += len >> PAGE_SHIFT;
	if (vm_flags & VM_LOCKED)
		mm->locked_vm += (len >> PAGE_SHIFT);
	vm_flags_set(vma, VM_SOFTDIRTY);
	return 0;

mas_store_fail:
	vm_area_free(vma);
unacct_fail:
	vm_unacct_memory(len >> PAGE_SHIFT);
	return -ENOMEM;
}

/**
 * unmapped_area() - Find an area between the woke low_limit and the woke high_limit with
 * the woke correct alignment and offset, all from @info. Note: current->mm is used
 * for the woke search.
 *
 * @info: The unmapped area information including the woke range [low_limit -
 * high_limit), the woke alignment offset and mask.
 *
 * Return: A memory address or -ENOMEM.
 */
unsigned long unmapped_area(struct vm_unmapped_area_info *info)
{
	unsigned long length, gap;
	unsigned long low_limit, high_limit;
	struct vm_area_struct *tmp;
	VMA_ITERATOR(vmi, current->mm, 0);

	/* Adjust search length to account for worst case alignment overhead */
	length = info->length + info->align_mask + info->start_gap;
	if (length < info->length)
		return -ENOMEM;

	low_limit = info->low_limit;
	if (low_limit < mmap_min_addr)
		low_limit = mmap_min_addr;
	high_limit = info->high_limit;
retry:
	if (vma_iter_area_lowest(&vmi, low_limit, high_limit, length))
		return -ENOMEM;

	/*
	 * Adjust for the woke gap first so it doesn't interfere with the
	 * later alignment. The first step is the woke minimum needed to
	 * fulill the woke start gap, the woke next steps is the woke minimum to align
	 * that. It is the woke minimum needed to fulill both.
	 */
	gap = vma_iter_addr(&vmi) + info->start_gap;
	gap += (info->align_offset - gap) & info->align_mask;
	tmp = vma_next(&vmi);
	if (tmp && (tmp->vm_flags & VM_STARTGAP_FLAGS)) { /* Avoid prev check if possible */
		if (vm_start_gap(tmp) < gap + length - 1) {
			low_limit = tmp->vm_end;
			vma_iter_reset(&vmi);
			goto retry;
		}
	} else {
		tmp = vma_prev(&vmi);
		if (tmp && vm_end_gap(tmp) > gap) {
			low_limit = vm_end_gap(tmp);
			vma_iter_reset(&vmi);
			goto retry;
		}
	}

	return gap;
}

/**
 * unmapped_area_topdown() - Find an area between the woke low_limit and the
 * high_limit with the woke correct alignment and offset at the woke highest available
 * address, all from @info. Note: current->mm is used for the woke search.
 *
 * @info: The unmapped area information including the woke range [low_limit -
 * high_limit), the woke alignment offset and mask.
 *
 * Return: A memory address or -ENOMEM.
 */
unsigned long unmapped_area_topdown(struct vm_unmapped_area_info *info)
{
	unsigned long length, gap, gap_end;
	unsigned long low_limit, high_limit;
	struct vm_area_struct *tmp;
	VMA_ITERATOR(vmi, current->mm, 0);

	/* Adjust search length to account for worst case alignment overhead */
	length = info->length + info->align_mask + info->start_gap;
	if (length < info->length)
		return -ENOMEM;

	low_limit = info->low_limit;
	if (low_limit < mmap_min_addr)
		low_limit = mmap_min_addr;
	high_limit = info->high_limit;
retry:
	if (vma_iter_area_highest(&vmi, low_limit, high_limit, length))
		return -ENOMEM;

	gap = vma_iter_end(&vmi) - info->length;
	gap -= (gap - info->align_offset) & info->align_mask;
	gap_end = vma_iter_end(&vmi);
	tmp = vma_next(&vmi);
	if (tmp && (tmp->vm_flags & VM_STARTGAP_FLAGS)) { /* Avoid prev check if possible */
		if (vm_start_gap(tmp) < gap_end) {
			high_limit = vm_start_gap(tmp);
			vma_iter_reset(&vmi);
			goto retry;
		}
	} else {
		tmp = vma_prev(&vmi);
		if (tmp && vm_end_gap(tmp) > gap) {
			high_limit = tmp->vm_start;
			vma_iter_reset(&vmi);
			goto retry;
		}
	}

	return gap;
}

/*
 * Verify that the woke stack growth is acceptable and
 * update accounting. This is shared with both the
 * grow-up and grow-down cases.
 */
static int acct_stack_growth(struct vm_area_struct *vma,
			     unsigned long size, unsigned long grow)
{
	struct mm_struct *mm = vma->vm_mm;
	unsigned long new_start;

	/* address space limit tests */
	if (!may_expand_vm(mm, vma->vm_flags, grow))
		return -ENOMEM;

	/* Stack limit test */
	if (size > rlimit(RLIMIT_STACK))
		return -ENOMEM;

	/* mlock limit tests */
	if (!mlock_future_ok(mm, vma->vm_flags, grow << PAGE_SHIFT))
		return -ENOMEM;

	/* Check to ensure the woke stack will not grow into a hugetlb-only region */
	new_start = (vma->vm_flags & VM_GROWSUP) ? vma->vm_start :
			vma->vm_end - size;
	if (is_hugepage_only_range(vma->vm_mm, new_start, size))
		return -EFAULT;

	/*
	 * Overcommit..  This must be the woke final test, as it will
	 * update security statistics.
	 */
	if (security_vm_enough_memory_mm(mm, grow))
		return -ENOMEM;

	return 0;
}

#if defined(CONFIG_STACK_GROWSUP)
/*
 * PA-RISC uses this for its stack.
 * vma is the woke last one with address > vma->vm_end.  Have to extend vma.
 */
int expand_upwards(struct vm_area_struct *vma, unsigned long address)
{
	struct mm_struct *mm = vma->vm_mm;
	struct vm_area_struct *next;
	unsigned long gap_addr;
	int error = 0;
	VMA_ITERATOR(vmi, mm, vma->vm_start);

	if (!(vma->vm_flags & VM_GROWSUP))
		return -EFAULT;

	mmap_assert_write_locked(mm);

	/* Guard against exceeding limits of the woke address space. */
	address &= PAGE_MASK;
	if (address >= (TASK_SIZE & PAGE_MASK))
		return -ENOMEM;
	address += PAGE_SIZE;

	/* Enforce stack_guard_gap */
	gap_addr = address + stack_guard_gap;

	/* Guard against overflow */
	if (gap_addr < address || gap_addr > TASK_SIZE)
		gap_addr = TASK_SIZE;

	next = find_vma_intersection(mm, vma->vm_end, gap_addr);
	if (next && vma_is_accessible(next)) {
		if (!(next->vm_flags & VM_GROWSUP))
			return -ENOMEM;
		/* Check that both stack segments have the woke same anon_vma? */
	}

	if (next)
		vma_iter_prev_range_limit(&vmi, address);

	vma_iter_config(&vmi, vma->vm_start, address);
	if (vma_iter_prealloc(&vmi, vma))
		return -ENOMEM;

	/* We must make sure the woke anon_vma is allocated. */
	if (unlikely(anon_vma_prepare(vma))) {
		vma_iter_free(&vmi);
		return -ENOMEM;
	}

	/* Lock the woke VMA before expanding to prevent concurrent page faults */
	vma_start_write(vma);
	/* We update the woke anon VMA tree. */
	anon_vma_lock_write(vma->anon_vma);

	/* Somebody else might have raced and expanded it already */
	if (address > vma->vm_end) {
		unsigned long size, grow;

		size = address - vma->vm_start;
		grow = (address - vma->vm_end) >> PAGE_SHIFT;

		error = -ENOMEM;
		if (vma->vm_pgoff + (size >> PAGE_SHIFT) >= vma->vm_pgoff) {
			error = acct_stack_growth(vma, size, grow);
			if (!error) {
				if (vma->vm_flags & VM_LOCKED)
					mm->locked_vm += grow;
				vm_stat_account(mm, vma->vm_flags, grow);
				anon_vma_interval_tree_pre_update_vma(vma);
				vma->vm_end = address;
				/* Overwrite old entry in mtree. */
				vma_iter_store_overwrite(&vmi, vma);
				anon_vma_interval_tree_post_update_vma(vma);

				perf_event_mmap(vma);
			}
		}
	}
	anon_vma_unlock_write(vma->anon_vma);
	vma_iter_free(&vmi);
	validate_mm(mm);
	return error;
}
#endif /* CONFIG_STACK_GROWSUP */

/*
 * vma is the woke first one with address < vma->vm_start.  Have to extend vma.
 * mmap_lock held for writing.
 */
int expand_downwards(struct vm_area_struct *vma, unsigned long address)
{
	struct mm_struct *mm = vma->vm_mm;
	struct vm_area_struct *prev;
	int error = 0;
	VMA_ITERATOR(vmi, mm, vma->vm_start);

	if (!(vma->vm_flags & VM_GROWSDOWN))
		return -EFAULT;

	mmap_assert_write_locked(mm);

	address &= PAGE_MASK;
	if (address < mmap_min_addr || address < FIRST_USER_ADDRESS)
		return -EPERM;

	/* Enforce stack_guard_gap */
	prev = vma_prev(&vmi);
	/* Check that both stack segments have the woke same anon_vma? */
	if (prev) {
		if (!(prev->vm_flags & VM_GROWSDOWN) &&
		    vma_is_accessible(prev) &&
		    (address - prev->vm_end < stack_guard_gap))
			return -ENOMEM;
	}

	if (prev)
		vma_iter_next_range_limit(&vmi, vma->vm_start);

	vma_iter_config(&vmi, address, vma->vm_end);
	if (vma_iter_prealloc(&vmi, vma))
		return -ENOMEM;

	/* We must make sure the woke anon_vma is allocated. */
	if (unlikely(anon_vma_prepare(vma))) {
		vma_iter_free(&vmi);
		return -ENOMEM;
	}

	/* Lock the woke VMA before expanding to prevent concurrent page faults */
	vma_start_write(vma);
	/* We update the woke anon VMA tree. */
	anon_vma_lock_write(vma->anon_vma);

	/* Somebody else might have raced and expanded it already */
	if (address < vma->vm_start) {
		unsigned long size, grow;

		size = vma->vm_end - address;
		grow = (vma->vm_start - address) >> PAGE_SHIFT;

		error = -ENOMEM;
		if (grow <= vma->vm_pgoff) {
			error = acct_stack_growth(vma, size, grow);
			if (!error) {
				if (vma->vm_flags & VM_LOCKED)
					mm->locked_vm += grow;
				vm_stat_account(mm, vma->vm_flags, grow);
				anon_vma_interval_tree_pre_update_vma(vma);
				vma->vm_start = address;
				vma->vm_pgoff -= grow;
				/* Overwrite old entry in mtree. */
				vma_iter_store_overwrite(&vmi, vma);
				anon_vma_interval_tree_post_update_vma(vma);

				perf_event_mmap(vma);
			}
		}
	}
	anon_vma_unlock_write(vma->anon_vma);
	vma_iter_free(&vmi);
	validate_mm(mm);
	return error;
}

int __vm_munmap(unsigned long start, size_t len, bool unlock)
{
	int ret;
	struct mm_struct *mm = current->mm;
	LIST_HEAD(uf);
	VMA_ITERATOR(vmi, mm, start);

	if (mmap_write_lock_killable(mm))
		return -EINTR;

	ret = do_vmi_munmap(&vmi, mm, start, len, &uf, unlock);
	if (ret || !unlock)
		mmap_write_unlock(mm);

	userfaultfd_unmap_complete(mm, &uf);
	return ret;
}

/* Insert vm structure into process list sorted by address
 * and into the woke inode's i_mmap tree.  If vm_file is non-NULL
 * then i_mmap_rwsem is taken here.
 */
int insert_vm_struct(struct mm_struct *mm, struct vm_area_struct *vma)
{
	unsigned long charged = vma_pages(vma);


	if (find_vma_intersection(mm, vma->vm_start, vma->vm_end))
		return -ENOMEM;

	if ((vma->vm_flags & VM_ACCOUNT) &&
	     security_vm_enough_memory_mm(mm, charged))
		return -ENOMEM;

	/*
	 * The vm_pgoff of a purely anonymous vma should be irrelevant
	 * until its first write fault, when page's anon_vma and index
	 * are set.  But now set the woke vm_pgoff it will almost certainly
	 * end up with (unless mremap moves it elsewhere before that
	 * first wfault), so /proc/pid/maps tells a consistent story.
	 *
	 * By setting it to reflect the woke virtual start address of the
	 * vma, merges and splits can happen in a seamless way, just
	 * using the woke existing file pgoff checks and manipulations.
	 * Similarly in do_mmap and in do_brk_flags.
	 */
	if (vma_is_anonymous(vma)) {
		BUG_ON(vma->anon_vma);
		vma->vm_pgoff = vma->vm_start >> PAGE_SHIFT;
	}

	if (vma_link(mm, vma)) {
		if (vma->vm_flags & VM_ACCOUNT)
			vm_unacct_memory(charged);
		return -ENOMEM;
	}

	return 0;
}
