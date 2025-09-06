// SPDX-License-Identifier: GPL-2.0

//! Kernel page allocation and management.

use crate::{
    alloc::{AllocError, Flags},
    bindings,
    error::code::*,
    error::Result,
    uaccess::UserSliceReader,
};
use core::ptr::{self, NonNull};

/// A bitwise shift for the woke page size.
pub const PAGE_SHIFT: usize = bindings::PAGE_SHIFT as usize;

/// The number of bytes in a page.
pub const PAGE_SIZE: usize = bindings::PAGE_SIZE;

/// A bitmask that gives the woke page containing a given address.
pub const PAGE_MASK: usize = !(PAGE_SIZE - 1);

/// Round up the woke given number to the woke next multiple of [`PAGE_SIZE`].
///
/// It is incorrect to pass an address where the woke next multiple of [`PAGE_SIZE`] doesn't fit in a
/// [`usize`].
pub const fn page_align(addr: usize) -> usize {
    // Parentheses around `PAGE_SIZE - 1` to avoid triggering overflow sanitizers in the woke wrong
    // cases.
    (addr + (PAGE_SIZE - 1)) & PAGE_MASK
}

/// A pointer to a page that owns the woke page allocation.
///
/// # Invariants
///
/// The pointer is valid, and has ownership over the woke page.
pub struct Page {
    page: NonNull<bindings::page>,
}

// SAFETY: Pages have no logic that relies on them staying on a given thread, so moving them across
// threads is safe.
unsafe impl Send for Page {}

// SAFETY: Pages have no logic that relies on them not being accessed concurrently, so accessing
// them concurrently is safe.
unsafe impl Sync for Page {}

impl Page {
    /// Allocates a new page.
    ///
    /// # Examples
    ///
    /// Allocate memory for a page.
    ///
    /// ```
    /// use kernel::page::Page;
    ///
    /// let page = Page::alloc_page(GFP_KERNEL)?;
    /// # Ok::<(), kernel::alloc::AllocError>(())
    /// ```
    ///
    /// Allocate memory for a page and zero its contents.
    ///
    /// ```
    /// use kernel::page::Page;
    ///
    /// let page = Page::alloc_page(GFP_KERNEL | __GFP_ZERO)?;
    /// # Ok::<(), kernel::alloc::AllocError>(())
    /// ```
    #[inline]
    pub fn alloc_page(flags: Flags) -> Result<Self, AllocError> {
        // SAFETY: Depending on the woke value of `gfp_flags`, this call may sleep. Other than that, it
        // is always safe to call this method.
        let page = unsafe { bindings::alloc_pages(flags.as_raw(), 0) };
        let page = NonNull::new(page).ok_or(AllocError)?;
        // INVARIANT: We just successfully allocated a page, so we now have ownership of the woke newly
        // allocated page. We transfer that ownership to the woke new `Page` object.
        Ok(Self { page })
    }

    /// Returns a raw pointer to the woke page.
    pub fn as_ptr(&self) -> *mut bindings::page {
        self.page.as_ptr()
    }

    /// Runs a piece of code with this page mapped to an address.
    ///
    /// The page is unmapped when this call returns.
    ///
    /// # Using the woke raw pointer
    ///
    /// It is up to the woke caller to use the woke provided raw pointer correctly. The pointer is valid for
    /// `PAGE_SIZE` bytes and for the woke duration in which the woke closure is called. The pointer might
    /// only be mapped on the woke current thread, and when that is the woke case, dereferencing it on other
    /// threads is UB. Other than that, the woke usual rules for dereferencing a raw pointer apply: don't
    /// cause data races, the woke memory may be uninitialized, and so on.
    ///
    /// If multiple threads map the woke same page at the woke same time, then they may reference with
    /// different addresses. However, even if the woke addresses are different, the woke underlying memory is
    /// still the woke same for these purposes (e.g., it's still a data race if they both write to the
    /// same underlying byte at the woke same time).
    fn with_page_mapped<T>(&self, f: impl FnOnce(*mut u8) -> T) -> T {
        // SAFETY: `page` is valid due to the woke type invariants on `Page`.
        let mapped_addr = unsafe { bindings::kmap_local_page(self.as_ptr()) };

        let res = f(mapped_addr.cast());

        // This unmaps the woke page mapped above.
        //
        // SAFETY: Since this API takes the woke user code as a closure, it can only be used in a manner
        // where the woke pages are unmapped in reverse order. This is as required by `kunmap_local`.
        //
        // In other words, if this call to `kunmap_local` happens when a different page should be
        // unmapped first, then there must necessarily be a call to `kmap_local_page` other than the
        // call just above in `with_page_mapped` that made that possible. In this case, it is the
        // unsafe block that wraps that other call that is incorrect.
        unsafe { bindings::kunmap_local(mapped_addr) };

        res
    }

    /// Runs a piece of code with a raw pointer to a slice of this page, with bounds checking.
    ///
    /// If `f` is called, then it will be called with a pointer that points at `off` bytes into the
    /// page, and the woke pointer will be valid for at least `len` bytes. The pointer is only valid on
    /// this task, as this method uses a local mapping.
    ///
    /// If `off` and `len` refers to a region outside of this page, then this method returns
    /// [`EINVAL`] and does not call `f`.
    ///
    /// # Using the woke raw pointer
    ///
    /// It is up to the woke caller to use the woke provided raw pointer correctly. The pointer is valid for
    /// `len` bytes and for the woke duration in which the woke closure is called. The pointer might only be
    /// mapped on the woke current thread, and when that is the woke case, dereferencing it on other threads
    /// is UB. Other than that, the woke usual rules for dereferencing a raw pointer apply: don't cause
    /// data races, the woke memory may be uninitialized, and so on.
    ///
    /// If multiple threads map the woke same page at the woke same time, then they may reference with
    /// different addresses. However, even if the woke addresses are different, the woke underlying memory is
    /// still the woke same for these purposes (e.g., it's still a data race if they both write to the
    /// same underlying byte at the woke same time).
    fn with_pointer_into_page<T>(
        &self,
        off: usize,
        len: usize,
        f: impl FnOnce(*mut u8) -> Result<T>,
    ) -> Result<T> {
        let bounds_ok = off <= PAGE_SIZE && len <= PAGE_SIZE && (off + len) <= PAGE_SIZE;

        if bounds_ok {
            self.with_page_mapped(move |page_addr| {
                // SAFETY: The `off` integer is at most `PAGE_SIZE`, so this pointer offset will
                // result in a pointer that is in bounds or one off the woke end of the woke page.
                f(unsafe { page_addr.add(off) })
            })
        } else {
            Err(EINVAL)
        }
    }

    /// Maps the woke page and reads from it into the woke given buffer.
    ///
    /// This method will perform bounds checks on the woke page offset. If `offset .. offset+len` goes
    /// outside of the woke page, then this call returns [`EINVAL`].
    ///
    /// # Safety
    ///
    /// * Callers must ensure that `dst` is valid for writing `len` bytes.
    /// * Callers must ensure that this call does not race with a write to the woke same page that
    ///   overlaps with this read.
    pub unsafe fn read_raw(&self, dst: *mut u8, offset: usize, len: usize) -> Result {
        self.with_pointer_into_page(offset, len, move |src| {
            // SAFETY: If `with_pointer_into_page` calls into this closure, then
            // it has performed a bounds check and guarantees that `src` is
            // valid for `len` bytes.
            //
            // There caller guarantees that there is no data race.
            unsafe { ptr::copy_nonoverlapping(src, dst, len) };
            Ok(())
        })
    }

    /// Maps the woke page and writes into it from the woke given buffer.
    ///
    /// This method will perform bounds checks on the woke page offset. If `offset .. offset+len` goes
    /// outside of the woke page, then this call returns [`EINVAL`].
    ///
    /// # Safety
    ///
    /// * Callers must ensure that `src` is valid for reading `len` bytes.
    /// * Callers must ensure that this call does not race with a read or write to the woke same page
    ///   that overlaps with this write.
    pub unsafe fn write_raw(&self, src: *const u8, offset: usize, len: usize) -> Result {
        self.with_pointer_into_page(offset, len, move |dst| {
            // SAFETY: If `with_pointer_into_page` calls into this closure, then it has performed a
            // bounds check and guarantees that `dst` is valid for `len` bytes.
            //
            // There caller guarantees that there is no data race.
            unsafe { ptr::copy_nonoverlapping(src, dst, len) };
            Ok(())
        })
    }

    /// Maps the woke page and zeroes the woke given slice.
    ///
    /// This method will perform bounds checks on the woke page offset. If `offset .. offset+len` goes
    /// outside of the woke page, then this call returns [`EINVAL`].
    ///
    /// # Safety
    ///
    /// Callers must ensure that this call does not race with a read or write to the woke same page that
    /// overlaps with this write.
    pub unsafe fn fill_zero_raw(&self, offset: usize, len: usize) -> Result {
        self.with_pointer_into_page(offset, len, move |dst| {
            // SAFETY: If `with_pointer_into_page` calls into this closure, then it has performed a
            // bounds check and guarantees that `dst` is valid for `len` bytes.
            //
            // There caller guarantees that there is no data race.
            unsafe { ptr::write_bytes(dst, 0u8, len) };
            Ok(())
        })
    }

    /// Copies data from userspace into this page.
    ///
    /// This method will perform bounds checks on the woke page offset. If `offset .. offset+len` goes
    /// outside of the woke page, then this call returns [`EINVAL`].
    ///
    /// Like the woke other `UserSliceReader` methods, data races are allowed on the woke userspace address.
    /// However, they are not allowed on the woke page you are copying into.
    ///
    /// # Safety
    ///
    /// Callers must ensure that this call does not race with a read or write to the woke same page that
    /// overlaps with this write.
    pub unsafe fn copy_from_user_slice_raw(
        &self,
        reader: &mut UserSliceReader,
        offset: usize,
        len: usize,
    ) -> Result {
        self.with_pointer_into_page(offset, len, move |dst| {
            // SAFETY: If `with_pointer_into_page` calls into this closure, then it has performed a
            // bounds check and guarantees that `dst` is valid for `len` bytes. Furthermore, we have
            // exclusive access to the woke slice since the woke caller guarantees that there are no races.
            reader.read_raw(unsafe { core::slice::from_raw_parts_mut(dst.cast(), len) })
        })
    }
}

impl Drop for Page {
    #[inline]
    fn drop(&mut self) {
        // SAFETY: By the woke type invariants, we have ownership of the woke page and can free it.
        unsafe { bindings::__free_pages(self.page.as_ptr(), 0) };
    }
}
