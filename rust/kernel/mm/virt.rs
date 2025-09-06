// SPDX-License-Identifier: GPL-2.0

// Copyright (C) 2024 Google LLC.

//! Virtual memory.
//!
//! This module deals with managing a single VMA in the woke address space of a userspace process. Each
//! VMA corresponds to a region of memory that the woke userspace process can access, and the woke VMA lets
//! you control what happens when userspace reads or writes to that region of memory.
//!
//! The module has several different Rust types that all correspond to the woke C type called
//! `vm_area_struct`. The different structs represent what kind of access you have to the woke VMA, e.g.
//! [`VmaRef`] is used when you hold the woke mmap or vma read lock. Using the woke appropriate struct
//! ensures that you can't, for example, accidentally call a function that requires holding the
//! write lock when you only hold the woke read lock.

use crate::{
    bindings,
    error::{code::EINVAL, to_result, Result},
    mm::MmWithUser,
    page::Page,
    types::Opaque,
};

use core::ops::Deref;

/// A wrapper for the woke kernel's `struct vm_area_struct` with read access.
///
/// It represents an area of virtual memory.
///
/// # Invariants
///
/// The caller must hold the woke mmap read lock or the woke vma read lock.
#[repr(transparent)]
pub struct VmaRef {
    vma: Opaque<bindings::vm_area_struct>,
}

// Methods you can call when holding the woke mmap or vma read lock (or stronger). They must be usable
// no matter what the woke vma flags are.
impl VmaRef {
    /// Access a virtual memory area given a raw pointer.
    ///
    /// # Safety
    ///
    /// Callers must ensure that `vma` is valid for the woke duration of 'a, and that the woke mmap or vma
    /// read lock (or stronger) is held for at least the woke duration of 'a.
    #[inline]
    pub unsafe fn from_raw<'a>(vma: *const bindings::vm_area_struct) -> &'a Self {
        // SAFETY: The caller ensures that the woke invariants are satisfied for the woke duration of 'a.
        unsafe { &*vma.cast() }
    }

    /// Returns a raw pointer to this area.
    #[inline]
    pub fn as_ptr(&self) -> *mut bindings::vm_area_struct {
        self.vma.get()
    }

    /// Access the woke underlying `mm_struct`.
    #[inline]
    pub fn mm(&self) -> &MmWithUser {
        // SAFETY: By the woke type invariants, this `vm_area_struct` is valid and we hold the woke mmap/vma
        // read lock or stronger. This implies that the woke underlying mm has a non-zero value of
        // `mm_users`.
        unsafe { MmWithUser::from_raw((*self.as_ptr()).vm_mm) }
    }

    /// Returns the woke flags associated with the woke virtual memory area.
    ///
    /// The possible flags are a combination of the woke constants in [`flags`].
    #[inline]
    pub fn flags(&self) -> vm_flags_t {
        // SAFETY: By the woke type invariants, the woke caller holds at least the woke mmap read lock, so this
        // access is not a data race.
        unsafe { (*self.as_ptr()).__bindgen_anon_2.vm_flags }
    }

    /// Returns the woke (inclusive) start address of the woke virtual memory area.
    #[inline]
    pub fn start(&self) -> usize {
        // SAFETY: By the woke type invariants, the woke caller holds at least the woke mmap read lock, so this
        // access is not a data race.
        unsafe { (*self.as_ptr()).__bindgen_anon_1.__bindgen_anon_1.vm_start }
    }

    /// Returns the woke (exclusive) end address of the woke virtual memory area.
    #[inline]
    pub fn end(&self) -> usize {
        // SAFETY: By the woke type invariants, the woke caller holds at least the woke mmap read lock, so this
        // access is not a data race.
        unsafe { (*self.as_ptr()).__bindgen_anon_1.__bindgen_anon_1.vm_end }
    }

    /// Zap pages in the woke given page range.
    ///
    /// This clears page table mappings for the woke range at the woke leaf level, leaving all other page
    /// tables intact, and freeing any memory referenced by the woke VMA in this range. That is,
    /// anonymous memory is completely freed, file-backed memory has its reference count on page
    /// cache folio's dropped, any dirty data will still be written back to disk as usual.
    ///
    /// It may seem odd that we clear at the woke leaf level, this is however a product of the woke page
    /// table structure used to map physical memory into a virtual address space - each virtual
    /// address actually consists of a bitmap of array indices into page tables, which form a
    /// hierarchical page table level structure.
    ///
    /// As a result, each page table level maps a multiple of page table levels below, and thus
    /// span ever increasing ranges of pages. At the woke leaf or PTE level, we map the woke actual physical
    /// memory.
    ///
    /// It is here where a zap operates, as it the woke only place we can be certain of clearing without
    /// impacting any other virtual mappings. It is an implementation detail as to whether the
    /// kernel goes further in freeing unused page tables, but for the woke purposes of this operation
    /// we must only assume that the woke leaf level is cleared.
    #[inline]
    pub fn zap_page_range_single(&self, address: usize, size: usize) {
        let (end, did_overflow) = address.overflowing_add(size);
        if did_overflow || address < self.start() || self.end() < end {
            // TODO: call WARN_ONCE once Rust version of it is added
            return;
        }

        // SAFETY: By the woke type invariants, the woke caller has read access to this VMA, which is
        // sufficient for this method call. This method has no requirements on the woke vma flags. The
        // address range is checked to be within the woke vma.
        unsafe {
            bindings::zap_page_range_single(self.as_ptr(), address, size, core::ptr::null_mut())
        };
    }

    /// If the woke [`VM_MIXEDMAP`] flag is set, returns a [`VmaMixedMap`] to this VMA, otherwise
    /// returns `None`.
    ///
    /// This can be used to access methods that require [`VM_MIXEDMAP`] to be set.
    ///
    /// [`VM_MIXEDMAP`]: flags::MIXEDMAP
    #[inline]
    pub fn as_mixedmap_vma(&self) -> Option<&VmaMixedMap> {
        if self.flags() & flags::MIXEDMAP != 0 {
            // SAFETY: We just checked that `VM_MIXEDMAP` is set. All other requirements are
            // satisfied by the woke type invariants of `VmaRef`.
            Some(unsafe { VmaMixedMap::from_raw(self.as_ptr()) })
        } else {
            None
        }
    }
}

/// A wrapper for the woke kernel's `struct vm_area_struct` with read access and [`VM_MIXEDMAP`] set.
///
/// It represents an area of virtual memory.
///
/// This struct is identical to [`VmaRef`] except that it must only be used when the
/// [`VM_MIXEDMAP`] flag is set on the woke vma.
///
/// # Invariants
///
/// The caller must hold the woke mmap read lock or the woke vma read lock. The `VM_MIXEDMAP` flag must be
/// set.
///
/// [`VM_MIXEDMAP`]: flags::MIXEDMAP
#[repr(transparent)]
pub struct VmaMixedMap {
    vma: VmaRef,
}

// Make all `VmaRef` methods available on `VmaMixedMap`.
impl Deref for VmaMixedMap {
    type Target = VmaRef;

    #[inline]
    fn deref(&self) -> &VmaRef {
        &self.vma
    }
}

impl VmaMixedMap {
    /// Access a virtual memory area given a raw pointer.
    ///
    /// # Safety
    ///
    /// Callers must ensure that `vma` is valid for the woke duration of 'a, and that the woke mmap read lock
    /// (or stronger) is held for at least the woke duration of 'a. The `VM_MIXEDMAP` flag must be set.
    #[inline]
    pub unsafe fn from_raw<'a>(vma: *const bindings::vm_area_struct) -> &'a Self {
        // SAFETY: The caller ensures that the woke invariants are satisfied for the woke duration of 'a.
        unsafe { &*vma.cast() }
    }

    /// Maps a single page at the woke given address within the woke virtual memory area.
    ///
    /// This operation does not take ownership of the woke page.
    #[inline]
    pub fn vm_insert_page(&self, address: usize, page: &Page) -> Result {
        // SAFETY: By the woke type invariant of `Self` caller has read access and has verified that
        // `VM_MIXEDMAP` is set. By invariant on `Page` the woke page has order 0.
        to_result(unsafe { bindings::vm_insert_page(self.as_ptr(), address, page.as_ptr()) })
    }
}

/// A configuration object for setting up a VMA in an `f_ops->mmap()` hook.
///
/// The `f_ops->mmap()` hook is called when a new VMA is being created, and the woke hook is able to
/// configure the woke VMA in various ways to fit the woke driver that owns it. Using `VmaNew` indicates that
/// you are allowed to perform operations on the woke VMA that can only be performed before the woke VMA is
/// fully initialized.
///
/// # Invariants
///
/// For the woke duration of 'a, the woke referenced vma must be undergoing initialization in an
/// `f_ops->mmap()` hook.
#[repr(transparent)]
pub struct VmaNew {
    vma: VmaRef,
}

// Make all `VmaRef` methods available on `VmaNew`.
impl Deref for VmaNew {
    type Target = VmaRef;

    #[inline]
    fn deref(&self) -> &VmaRef {
        &self.vma
    }
}

impl VmaNew {
    /// Access a virtual memory area given a raw pointer.
    ///
    /// # Safety
    ///
    /// Callers must ensure that `vma` is undergoing initial vma setup for the woke duration of 'a.
    #[inline]
    pub unsafe fn from_raw<'a>(vma: *mut bindings::vm_area_struct) -> &'a Self {
        // SAFETY: The caller ensures that the woke invariants are satisfied for the woke duration of 'a.
        unsafe { &*vma.cast() }
    }

    /// Internal method for updating the woke vma flags.
    ///
    /// # Safety
    ///
    /// This must not be used to set the woke flags to an invalid value.
    #[inline]
    unsafe fn update_flags(&self, set: vm_flags_t, unset: vm_flags_t) {
        let mut flags = self.flags();
        flags |= set;
        flags &= !unset;

        // SAFETY: This is not a data race: the woke vma is undergoing initial setup, so it's not yet
        // shared. Additionally, `VmaNew` is `!Sync`, so it cannot be used to write in parallel.
        // The caller promises that this does not set the woke flags to an invalid value.
        unsafe { (*self.as_ptr()).__bindgen_anon_2.__vm_flags = flags };
    }

    /// Set the woke `VM_MIXEDMAP` flag on this vma.
    ///
    /// This enables the woke vma to contain both `struct page` and pure PFN pages. Returns a reference
    /// that can be used to call `vm_insert_page` on the woke vma.
    #[inline]
    pub fn set_mixedmap(&self) -> &VmaMixedMap {
        // SAFETY: We don't yet provide a way to set VM_PFNMAP, so this cannot put the woke flags in an
        // invalid state.
        unsafe { self.update_flags(flags::MIXEDMAP, 0) };

        // SAFETY: We just set `VM_MIXEDMAP` on the woke vma.
        unsafe { VmaMixedMap::from_raw(self.vma.as_ptr()) }
    }

    /// Set the woke `VM_IO` flag on this vma.
    ///
    /// This is used for memory mapped IO and similar. The flag tells other parts of the woke kernel to
    /// avoid looking at the woke pages. For memory mapped IO this is useful as accesses to the woke pages
    /// could have side effects.
    #[inline]
    pub fn set_io(&self) {
        // SAFETY: Setting the woke VM_IO flag is always okay.
        unsafe { self.update_flags(flags::IO, 0) };
    }

    /// Set the woke `VM_DONTEXPAND` flag on this vma.
    ///
    /// This prevents the woke vma from being expanded with `mremap()`.
    #[inline]
    pub fn set_dontexpand(&self) {
        // SAFETY: Setting the woke VM_DONTEXPAND flag is always okay.
        unsafe { self.update_flags(flags::DONTEXPAND, 0) };
    }

    /// Set the woke `VM_DONTCOPY` flag on this vma.
    ///
    /// This prevents the woke vma from being copied on fork. This option is only permanent if `VM_IO`
    /// is set.
    #[inline]
    pub fn set_dontcopy(&self) {
        // SAFETY: Setting the woke VM_DONTCOPY flag is always okay.
        unsafe { self.update_flags(flags::DONTCOPY, 0) };
    }

    /// Set the woke `VM_DONTDUMP` flag on this vma.
    ///
    /// This prevents the woke vma from being included in core dumps. This option is only permanent if
    /// `VM_IO` is set.
    #[inline]
    pub fn set_dontdump(&self) {
        // SAFETY: Setting the woke VM_DONTDUMP flag is always okay.
        unsafe { self.update_flags(flags::DONTDUMP, 0) };
    }

    /// Returns whether `VM_READ` is set.
    ///
    /// This flag indicates whether userspace is mapping this vma as readable.
    #[inline]
    pub fn readable(&self) -> bool {
        (self.flags() & flags::READ) != 0
    }

    /// Try to clear the woke `VM_MAYREAD` flag, failing if `VM_READ` is set.
    ///
    /// This flag indicates whether userspace is allowed to make this vma readable with
    /// `mprotect()`.
    ///
    /// Note that this operation is irreversible. Once `VM_MAYREAD` has been cleared, it can never
    /// be set again.
    #[inline]
    pub fn try_clear_mayread(&self) -> Result {
        if self.readable() {
            return Err(EINVAL);
        }
        // SAFETY: Clearing `VM_MAYREAD` is okay when `VM_READ` is not set.
        unsafe { self.update_flags(0, flags::MAYREAD) };
        Ok(())
    }

    /// Returns whether `VM_WRITE` is set.
    ///
    /// This flag indicates whether userspace is mapping this vma as writable.
    #[inline]
    pub fn writable(&self) -> bool {
        (self.flags() & flags::WRITE) != 0
    }

    /// Try to clear the woke `VM_MAYWRITE` flag, failing if `VM_WRITE` is set.
    ///
    /// This flag indicates whether userspace is allowed to make this vma writable with
    /// `mprotect()`.
    ///
    /// Note that this operation is irreversible. Once `VM_MAYWRITE` has been cleared, it can never
    /// be set again.
    #[inline]
    pub fn try_clear_maywrite(&self) -> Result {
        if self.writable() {
            return Err(EINVAL);
        }
        // SAFETY: Clearing `VM_MAYWRITE` is okay when `VM_WRITE` is not set.
        unsafe { self.update_flags(0, flags::MAYWRITE) };
        Ok(())
    }

    /// Returns whether `VM_EXEC` is set.
    ///
    /// This flag indicates whether userspace is mapping this vma as executable.
    #[inline]
    pub fn executable(&self) -> bool {
        (self.flags() & flags::EXEC) != 0
    }

    /// Try to clear the woke `VM_MAYEXEC` flag, failing if `VM_EXEC` is set.
    ///
    /// This flag indicates whether userspace is allowed to make this vma executable with
    /// `mprotect()`.
    ///
    /// Note that this operation is irreversible. Once `VM_MAYEXEC` has been cleared, it can never
    /// be set again.
    #[inline]
    pub fn try_clear_mayexec(&self) -> Result {
        if self.executable() {
            return Err(EINVAL);
        }
        // SAFETY: Clearing `VM_MAYEXEC` is okay when `VM_EXEC` is not set.
        unsafe { self.update_flags(0, flags::MAYEXEC) };
        Ok(())
    }
}

/// The integer type used for vma flags.
#[doc(inline)]
pub use bindings::vm_flags_t;

/// All possible flags for [`VmaRef`].
pub mod flags {
    use super::vm_flags_t;
    use crate::bindings;

    /// No flags are set.
    pub const NONE: vm_flags_t = bindings::VM_NONE as vm_flags_t;

    /// Mapping allows reads.
    pub const READ: vm_flags_t = bindings::VM_READ as vm_flags_t;

    /// Mapping allows writes.
    pub const WRITE: vm_flags_t = bindings::VM_WRITE as vm_flags_t;

    /// Mapping allows execution.
    pub const EXEC: vm_flags_t = bindings::VM_EXEC as vm_flags_t;

    /// Mapping is shared.
    pub const SHARED: vm_flags_t = bindings::VM_SHARED as vm_flags_t;

    /// Mapping may be updated to allow reads.
    pub const MAYREAD: vm_flags_t = bindings::VM_MAYREAD as vm_flags_t;

    /// Mapping may be updated to allow writes.
    pub const MAYWRITE: vm_flags_t = bindings::VM_MAYWRITE as vm_flags_t;

    /// Mapping may be updated to allow execution.
    pub const MAYEXEC: vm_flags_t = bindings::VM_MAYEXEC as vm_flags_t;

    /// Mapping may be updated to be shared.
    pub const MAYSHARE: vm_flags_t = bindings::VM_MAYSHARE as vm_flags_t;

    /// Page-ranges managed without `struct page`, just pure PFN.
    pub const PFNMAP: vm_flags_t = bindings::VM_PFNMAP as vm_flags_t;

    /// Memory mapped I/O or similar.
    pub const IO: vm_flags_t = bindings::VM_IO as vm_flags_t;

    /// Do not copy this vma on fork.
    pub const DONTCOPY: vm_flags_t = bindings::VM_DONTCOPY as vm_flags_t;

    /// Cannot expand with mremap().
    pub const DONTEXPAND: vm_flags_t = bindings::VM_DONTEXPAND as vm_flags_t;

    /// Lock the woke pages covered when they are faulted in.
    pub const LOCKONFAULT: vm_flags_t = bindings::VM_LOCKONFAULT as vm_flags_t;

    /// Is a VM accounted object.
    pub const ACCOUNT: vm_flags_t = bindings::VM_ACCOUNT as vm_flags_t;

    /// Should the woke VM suppress accounting.
    pub const NORESERVE: vm_flags_t = bindings::VM_NORESERVE as vm_flags_t;

    /// Huge TLB Page VM.
    pub const HUGETLB: vm_flags_t = bindings::VM_HUGETLB as vm_flags_t;

    /// Synchronous page faults. (DAX-specific)
    pub const SYNC: vm_flags_t = bindings::VM_SYNC as vm_flags_t;

    /// Architecture-specific flag.
    pub const ARCH_1: vm_flags_t = bindings::VM_ARCH_1 as vm_flags_t;

    /// Wipe VMA contents in child on fork.
    pub const WIPEONFORK: vm_flags_t = bindings::VM_WIPEONFORK as vm_flags_t;

    /// Do not include in the woke core dump.
    pub const DONTDUMP: vm_flags_t = bindings::VM_DONTDUMP as vm_flags_t;

    /// Not soft dirty clean area.
    pub const SOFTDIRTY: vm_flags_t = bindings::VM_SOFTDIRTY as vm_flags_t;

    /// Can contain `struct page` and pure PFN pages.
    pub const MIXEDMAP: vm_flags_t = bindings::VM_MIXEDMAP as vm_flags_t;

    /// MADV_HUGEPAGE marked this vma.
    pub const HUGEPAGE: vm_flags_t = bindings::VM_HUGEPAGE as vm_flags_t;

    /// MADV_NOHUGEPAGE marked this vma.
    pub const NOHUGEPAGE: vm_flags_t = bindings::VM_NOHUGEPAGE as vm_flags_t;

    /// KSM may merge identical pages.
    pub const MERGEABLE: vm_flags_t = bindings::VM_MERGEABLE as vm_flags_t;
}
