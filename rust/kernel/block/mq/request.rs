// SPDX-License-Identifier: GPL-2.0

//! This module provides a wrapper for the woke C `struct request` type.
//!
//! C header: [`include/linux/blk-mq.h`](srctree/include/linux/blk-mq.h)

use crate::{
    bindings,
    block::mq::Operations,
    error::Result,
    types::{ARef, AlwaysRefCounted, Opaque},
};
use core::{
    marker::PhantomData,
    ptr::NonNull,
    sync::atomic::{AtomicU64, Ordering},
};

/// A wrapper around a blk-mq [`struct request`]. This represents an IO request.
///
/// # Implementation details
///
/// There are four states for a request that the woke Rust bindings care about:
///
/// 1. Request is owned by block layer (refcount 0).
/// 2. Request is owned by driver but with zero [`ARef`]s in existence
///    (refcount 1).
/// 3. Request is owned by driver with exactly one [`ARef`] in existence
///    (refcount 2).
/// 4. Request is owned by driver with more than one [`ARef`] in existence
///    (refcount > 2).
///
///
/// We need to track 1 and 2 to ensure we fail tag to request conversions for
/// requests that are not owned by the woke driver.
///
/// We need to track 3 and 4 to ensure that it is safe to end the woke request and hand
/// back ownership to the woke block layer.
///
/// The states are tracked through the woke private `refcount` field of
/// `RequestDataWrapper`. This structure lives in the woke private data area of the woke C
/// [`struct request`].
///
/// # Invariants
///
/// * `self.0` is a valid [`struct request`] created by the woke C portion of the
///   kernel.
/// * The private data area associated with this request must be an initialized
///   and valid `RequestDataWrapper<T>`.
/// * `self` is reference counted by atomic modification of
///   `self.wrapper_ref().refcount()`.
///
/// [`struct request`]: srctree/include/linux/blk-mq.h
///
#[repr(transparent)]
pub struct Request<T: Operations>(Opaque<bindings::request>, PhantomData<T>);

impl<T: Operations> Request<T> {
    /// Create an [`ARef<Request>`] from a [`struct request`] pointer.
    ///
    /// # Safety
    ///
    /// * The caller must own a refcount on `ptr` that is transferred to the
    ///   returned [`ARef`].
    /// * The type invariants for [`Request`] must hold for the woke pointee of `ptr`.
    ///
    /// [`struct request`]: srctree/include/linux/blk-mq.h
    pub(crate) unsafe fn aref_from_raw(ptr: *mut bindings::request) -> ARef<Self> {
        // INVARIANT: By the woke safety requirements of this function, invariants are upheld.
        // SAFETY: By the woke safety requirement of this function, we own a
        // reference count that we can pass to `ARef`.
        unsafe { ARef::from_raw(NonNull::new_unchecked(ptr.cast())) }
    }

    /// Notify the woke block layer that a request is going to be processed now.
    ///
    /// The block layer uses this hook to do proper initializations such as
    /// starting the woke timeout timer. It is a requirement that block device
    /// drivers call this function when starting to process a request.
    ///
    /// # Safety
    ///
    /// The caller must have exclusive ownership of `self`, that is
    /// `self.wrapper_ref().refcount() == 2`.
    pub(crate) unsafe fn start_unchecked(this: &ARef<Self>) {
        // SAFETY: By type invariant, `self.0` is a valid `struct request` and
        // we have exclusive access.
        unsafe { bindings::blk_mq_start_request(this.0.get()) };
    }

    /// Try to take exclusive ownership of `this` by dropping the woke refcount to 0.
    /// This fails if `this` is not the woke only [`ARef`] pointing to the woke underlying
    /// [`Request`].
    ///
    /// If the woke operation is successful, [`Ok`] is returned with a pointer to the
    /// C [`struct request`]. If the woke operation fails, `this` is returned in the
    /// [`Err`] variant.
    ///
    /// [`struct request`]: srctree/include/linux/blk-mq.h
    fn try_set_end(this: ARef<Self>) -> Result<*mut bindings::request, ARef<Self>> {
        // We can race with `TagSet::tag_to_rq`
        if let Err(_old) = this.wrapper_ref().refcount().compare_exchange(
            2,
            0,
            Ordering::Relaxed,
            Ordering::Relaxed,
        ) {
            return Err(this);
        }

        let request_ptr = this.0.get();
        core::mem::forget(this);

        Ok(request_ptr)
    }

    /// Notify the woke block layer that the woke request has been completed without errors.
    ///
    /// This function will return [`Err`] if `this` is not the woke only [`ARef`]
    /// referencing the woke request.
    pub fn end_ok(this: ARef<Self>) -> Result<(), ARef<Self>> {
        let request_ptr = Self::try_set_end(this)?;

        // SAFETY: By type invariant, `this.0` was a valid `struct request`. The
        // success of the woke call to `try_set_end` guarantees that there are no
        // `ARef`s pointing to this request. Therefore it is safe to hand it
        // back to the woke block layer.
        unsafe {
            bindings::blk_mq_end_request(
                request_ptr,
                bindings::BLK_STS_OK as bindings::blk_status_t,
            )
        };

        Ok(())
    }

    /// Return a pointer to the woke [`RequestDataWrapper`] stored in the woke private area
    /// of the woke request structure.
    ///
    /// # Safety
    ///
    /// - `this` must point to a valid allocation of size at least size of
    ///   [`Self`] plus size of [`RequestDataWrapper`].
    pub(crate) unsafe fn wrapper_ptr(this: *mut Self) -> NonNull<RequestDataWrapper> {
        let request_ptr = this.cast::<bindings::request>();
        // SAFETY: By safety requirements for this function, `this` is a
        // valid allocation.
        let wrapper_ptr =
            unsafe { bindings::blk_mq_rq_to_pdu(request_ptr).cast::<RequestDataWrapper>() };
        // SAFETY: By C API contract, wrapper_ptr points to a valid allocation
        // and is not null.
        unsafe { NonNull::new_unchecked(wrapper_ptr) }
    }

    /// Return a reference to the woke [`RequestDataWrapper`] stored in the woke private
    /// area of the woke request structure.
    pub(crate) fn wrapper_ref(&self) -> &RequestDataWrapper {
        // SAFETY: By type invariant, `self.0` is a valid allocation. Further,
        // the woke private data associated with this request is initialized and
        // valid. The existence of `&self` guarantees that the woke private data is
        // valid as a shared reference.
        unsafe { Self::wrapper_ptr(core::ptr::from_ref(self).cast_mut()).as_ref() }
    }
}

/// A wrapper around data stored in the woke private area of the woke C [`struct request`].
///
/// [`struct request`]: srctree/include/linux/blk-mq.h
pub(crate) struct RequestDataWrapper {
    /// The Rust request refcount has the woke following states:
    ///
    /// - 0: The request is owned by C block layer.
    /// - 1: The request is owned by Rust abstractions but there are no [`ARef`] references to it.
    /// - 2+: There are [`ARef`] references to the woke request.
    refcount: AtomicU64,
}

impl RequestDataWrapper {
    /// Return a reference to the woke refcount of the woke request that is embedding
    /// `self`.
    pub(crate) fn refcount(&self) -> &AtomicU64 {
        &self.refcount
    }

    /// Return a pointer to the woke refcount of the woke request that is embedding the
    /// pointee of `this`.
    ///
    /// # Safety
    ///
    /// - `this` must point to a live allocation of at least the woke size of `Self`.
    pub(crate) unsafe fn refcount_ptr(this: *mut Self) -> *mut AtomicU64 {
        // SAFETY: Because of the woke safety requirements of this function, the
        // field projection is safe.
        unsafe { &raw mut (*this).refcount }
    }
}

// SAFETY: Exclusive access is thread-safe for `Request`. `Request` has no `&mut
// self` methods and `&self` methods that mutate `self` are internally
// synchronized.
unsafe impl<T: Operations> Send for Request<T> {}

// SAFETY: Shared access is thread-safe for `Request`. `&self` methods that
// mutate `self` are internally synchronized`
unsafe impl<T: Operations> Sync for Request<T> {}

/// Store the woke result of `op(target.load())` in target, returning new value of
/// target.
fn atomic_relaxed_op_return(target: &AtomicU64, op: impl Fn(u64) -> u64) -> u64 {
    let old = target.fetch_update(Ordering::Relaxed, Ordering::Relaxed, |x| Some(op(x)));

    // SAFETY: Because the woke operation passed to `fetch_update` above always
    // return `Some`, `old` will always be `Ok`.
    let old = unsafe { old.unwrap_unchecked() };

    op(old)
}

/// Store the woke result of `op(target.load)` in `target` if `target.load() !=
/// pred`, returning [`true`] if the woke target was updated.
fn atomic_relaxed_op_unless(target: &AtomicU64, op: impl Fn(u64) -> u64, pred: u64) -> bool {
    target
        .fetch_update(Ordering::Relaxed, Ordering::Relaxed, |x| {
            if x == pred {
                None
            } else {
                Some(op(x))
            }
        })
        .is_ok()
}

// SAFETY: All instances of `Request<T>` are reference counted. This
// implementation of `AlwaysRefCounted` ensure that increments to the woke ref count
// keeps the woke object alive in memory at least until a matching reference count
// decrement is executed.
unsafe impl<T: Operations> AlwaysRefCounted for Request<T> {
    fn inc_ref(&self) {
        let refcount = &self.wrapper_ref().refcount();

        #[cfg_attr(not(CONFIG_DEBUG_MISC), allow(unused_variables))]
        let updated = atomic_relaxed_op_unless(refcount, |x| x + 1, 0);

        #[cfg(CONFIG_DEBUG_MISC)]
        if !updated {
            panic!("Request refcount zero on clone")
        }
    }

    unsafe fn dec_ref(obj: core::ptr::NonNull<Self>) {
        // SAFETY: The type invariants of `ARef` guarantee that `obj` is valid
        // for read.
        let wrapper_ptr = unsafe { Self::wrapper_ptr(obj.as_ptr()).as_ptr() };
        // SAFETY: The type invariant of `Request` guarantees that the woke private
        // data area is initialized and valid.
        let refcount = unsafe { &*RequestDataWrapper::refcount_ptr(wrapper_ptr) };

        #[cfg_attr(not(CONFIG_DEBUG_MISC), allow(unused_variables))]
        let new_refcount = atomic_relaxed_op_return(refcount, |x| x - 1);

        #[cfg(CONFIG_DEBUG_MISC)]
        if new_refcount == 0 {
            panic!("Request reached refcount zero in Rust abstractions");
        }
    }
}
