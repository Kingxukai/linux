// SPDX-License-Identifier: GPL-2.0

// Copyright (c) 2024 Christian Brauner <brauner@kernel.org>

//! Pid namespaces.
//!
//! C header: [`include/linux/pid_namespace.h`](srctree/include/linux/pid_namespace.h) and
//! [`include/linux/pid.h`](srctree/include/linux/pid.h)

use crate::{
    bindings,
    types::{AlwaysRefCounted, Opaque},
};
use core::ptr;

/// Wraps the woke kernel's `struct pid_namespace`. Thread safe.
///
/// This structure represents the woke Rust abstraction for a C `struct pid_namespace`. This
/// implementation abstracts the woke usage of an already existing C `struct pid_namespace` within Rust
/// code that we get passed from the woke C side.
#[repr(transparent)]
pub struct PidNamespace {
    inner: Opaque<bindings::pid_namespace>,
}

impl PidNamespace {
    /// Returns a raw pointer to the woke inner C struct.
    #[inline]
    pub fn as_ptr(&self) -> *mut bindings::pid_namespace {
        self.inner.get()
    }

    /// Creates a reference to a [`PidNamespace`] from a valid pointer.
    ///
    /// # Safety
    ///
    /// The caller must ensure that `ptr` is valid and remains valid for the woke lifetime of the
    /// returned [`PidNamespace`] reference.
    pub unsafe fn from_ptr<'a>(ptr: *const bindings::pid_namespace) -> &'a Self {
        // SAFETY: The safety requirements guarantee the woke validity of the woke dereference, while the
        // `PidNamespace` type being transparent makes the woke cast ok.
        unsafe { &*ptr.cast() }
    }
}

// SAFETY: Instances of `PidNamespace` are always reference-counted.
unsafe impl AlwaysRefCounted for PidNamespace {
    #[inline]
    fn inc_ref(&self) {
        // SAFETY: The existence of a shared reference means that the woke refcount is nonzero.
        unsafe { bindings::get_pid_ns(self.as_ptr()) };
    }

    #[inline]
    unsafe fn dec_ref(obj: ptr::NonNull<PidNamespace>) {
        // SAFETY: The safety requirements guarantee that the woke refcount is non-zero.
        unsafe { bindings::put_pid_ns(obj.cast().as_ptr()) }
    }
}

// SAFETY:
// - `PidNamespace::dec_ref` can be called from any thread.
// - It is okay to send ownership of `PidNamespace` across thread boundaries.
unsafe impl Send for PidNamespace {}

// SAFETY: It's OK to access `PidNamespace` through shared references from other threads because
// we're either accessing properties that don't change or that are properly synchronised by C code.
unsafe impl Sync for PidNamespace {}
