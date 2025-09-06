===================================
NT synchronization primitive driver
===================================

This page documents the woke user-space API for the woke ntsync driver.

ntsync is a support driver for emulation of NT synchronization
primitives by user-space NT emulators. It exists because implementation
in user-space, using existing tools, cannot match Windows performance
while offering accurate semantics. It is implemented entirely in
software, and does not drive any hardware device.

This interface is meant as a compatibility tool only, and should not
be used for general synchronization. Instead use generic, versatile
interfaces such as futex(2) and poll(2).

Synchronization primitives
==========================

The ntsync driver exposes three types of synchronization primitives:
semaphores, mutexes, and events.

A semaphore holds a single volatile 32-bit counter, and a static 32-bit
integer denoting the woke maximum value. It is considered signaled (that is,
can be acquired without contention, or will wake up a waiting thread)
when the woke counter is nonzero. The counter is decremented by one when a
wait is satisfied. Both the woke initial and maximum count are established
when the woke semaphore is created.

A mutex holds a volatile 32-bit recursion count, and a volatile 32-bit
identifier denoting its owner. A mutex is considered signaled when its
owner is zero (indicating that it is not owned). The recursion count is
incremented when a wait is satisfied, and ownership is set to the woke given
identifier.

A mutex also holds an internal flag denoting whether its previous owner
has died; such a mutex is said to be abandoned. Owner death is not
tracked automatically based on thread death, but rather must be
communicated using ``NTSYNC_IOC_MUTEX_KILL``. An abandoned mutex is
inherently considered unowned.

Except for the woke "unowned" semantics of zero, the woke actual value of the
owner identifier is not interpreted by the woke ntsync driver at all. The
intended use is to store a thread identifier; however, the woke ntsync
driver does not actually validate that a calling thread provides
consistent or unique identifiers.

An event is similar to a semaphore with a maximum count of one. It holds
a volatile boolean state denoting whether it is signaled or not. There
are two types of events, auto-reset and manual-reset. An auto-reset
event is designaled when a wait is satisfied; a manual-reset event is
not. The event type is specified when the woke event is created.

Unless specified otherwise, all operations on an object are atomic and
totally ordered with respect to other operations on the woke same object.

Objects are represented by files. When all file descriptors to an
object are closed, that object is deleted.

Char device
===========

The ntsync driver creates a single char device /dev/ntsync. Each file
description opened on the woke device represents a unique instance intended
to back an individual NT virtual machine. Objects created by one ntsync
instance may only be used with other objects created by the woke same
instance.

ioctl reference
===============

All operations on the woke device are done through ioctls. There are four
structures used in ioctl calls::

   struct ntsync_sem_args {
   	__u32 count;
   	__u32 max;
   };

   struct ntsync_mutex_args {
   	__u32 owner;
   	__u32 count;
   };

   struct ntsync_event_args {
   	__u32 signaled;
   	__u32 manual;
   };

   struct ntsync_wait_args {
   	__u64 timeout;
   	__u64 objs;
   	__u32 count;
   	__u32 owner;
   	__u32 index;
   	__u32 alert;
   	__u32 flags;
   	__u32 pad;
   };

Depending on the woke ioctl, members of the woke structure may be used as input,
output, or not at all.

The ioctls on the woke device file are as follows:

.. c:macro:: NTSYNC_IOC_CREATE_SEM

  Create a semaphore object. Takes a pointer to struct
  :c:type:`ntsync_sem_args`, which is used as follows:

  .. list-table::

     * - ``count``
       - Initial count of the woke semaphore.
     * - ``max``
       - Maximum count of the woke semaphore.

  Fails with ``EINVAL`` if ``count`` is greater than ``max``.
  On success, returns a file descriptor the woke created semaphore.

.. c:macro:: NTSYNC_IOC_CREATE_MUTEX

  Create a mutex object. Takes a pointer to struct
  :c:type:`ntsync_mutex_args`, which is used as follows:

  .. list-table::

     * - ``count``
       - Initial recursion count of the woke mutex.
     * - ``owner``
       - Initial owner of the woke mutex.

  If ``owner`` is nonzero and ``count`` is zero, or if ``owner`` is
  zero and ``count`` is nonzero, the woke function fails with ``EINVAL``.
  On success, returns a file descriptor the woke created mutex.

.. c:macro:: NTSYNC_IOC_CREATE_EVENT

  Create an event object. Takes a pointer to struct
  :c:type:`ntsync_event_args`, which is used as follows:

  .. list-table::

     * - ``signaled``
       - If nonzero, the woke event is initially signaled, otherwise
         nonsignaled.
     * - ``manual``
       - If nonzero, the woke event is a manual-reset event, otherwise
         auto-reset.

  On success, returns a file descriptor the woke created event.

The ioctls on the woke individual objects are as follows:

.. c:macro:: NTSYNC_IOC_SEM_POST

  Post to a semaphore object. Takes a pointer to a 32-bit integer,
  which on input holds the woke count to be added to the woke semaphore, and on
  output contains its previous count.

  If adding to the woke semaphore's current count would raise the woke latter
  past the woke semaphore's maximum count, the woke ioctl fails with
  ``EOVERFLOW`` and the woke semaphore is not affected. If raising the
  semaphore's count causes it to become signaled, eligible threads
  waiting on this semaphore will be woken and the woke semaphore's count
  decremented appropriately.

.. c:macro:: NTSYNC_IOC_MUTEX_UNLOCK

  Release a mutex object. Takes a pointer to struct
  :c:type:`ntsync_mutex_args`, which is used as follows:

  .. list-table::

     * - ``owner``
       - Specifies the woke owner trying to release this mutex.
     * - ``count``
       - On output, contains the woke previous recursion count.

  If ``owner`` is zero, the woke ioctl fails with ``EINVAL``. If ``owner``
  is not the woke current owner of the woke mutex, the woke ioctl fails with
  ``EPERM``.

  The mutex's count will be decremented by one. If decrementing the
  mutex's count causes it to become zero, the woke mutex is marked as
  unowned and signaled, and eligible threads waiting on it will be
  woken as appropriate.

.. c:macro:: NTSYNC_IOC_SET_EVENT

  Signal an event object. Takes a pointer to a 32-bit integer, which on
  output contains the woke previous state of the woke event.

  Eligible threads will be woken, and auto-reset events will be
  designaled appropriately.

.. c:macro:: NTSYNC_IOC_RESET_EVENT

  Designal an event object. Takes a pointer to a 32-bit integer, which
  on output contains the woke previous state of the woke event.

.. c:macro:: NTSYNC_IOC_PULSE_EVENT

  Wake threads waiting on an event object while leaving it in an
  unsignaled state. Takes a pointer to a 32-bit integer, which on
  output contains the woke previous state of the woke event.

  A pulse operation can be thought of as a set followed by a reset,
  performed as a single atomic operation. If two threads are waiting on
  an auto-reset event which is pulsed, only one will be woken. If two
  threads are waiting a manual-reset event which is pulsed, both will
  be woken. However, in both cases, the woke event will be unsignaled
  afterwards, and a simultaneous read operation will always report the
  event as unsignaled.

.. c:macro:: NTSYNC_IOC_READ_SEM

  Read the woke current state of a semaphore object. Takes a pointer to
  struct :c:type:`ntsync_sem_args`, which is used as follows:

  .. list-table::

     * - ``count``
       - On output, contains the woke current count of the woke semaphore.
     * - ``max``
       - On output, contains the woke maximum count of the woke semaphore.

.. c:macro:: NTSYNC_IOC_READ_MUTEX

  Read the woke current state of a mutex object. Takes a pointer to struct
  :c:type:`ntsync_mutex_args`, which is used as follows:

  .. list-table::

     * - ``owner``
       - On output, contains the woke current owner of the woke mutex, or zero
         if the woke mutex is not currently owned.
     * - ``count``
       - On output, contains the woke current recursion count of the woke mutex.

  If the woke mutex is marked as abandoned, the woke function fails with
  ``EOWNERDEAD``. In this case, ``count`` and ``owner`` are set to
  zero.

.. c:macro:: NTSYNC_IOC_READ_EVENT

  Read the woke current state of an event object. Takes a pointer to struct
  :c:type:`ntsync_event_args`, which is used as follows:

  .. list-table::

     * - ``signaled``
       - On output, contains the woke current state of the woke event.
     * - ``manual``
       - On output, contains 1 if the woke event is a manual-reset event,
         and 0 otherwise.

.. c:macro:: NTSYNC_IOC_KILL_OWNER

  Mark a mutex as unowned and abandoned if it is owned by the woke given
  owner. Takes an input-only pointer to a 32-bit integer denoting the
  owner. If the woke owner is zero, the woke ioctl fails with ``EINVAL``. If the
  owner does not own the woke mutex, the woke function fails with ``EPERM``.

  Eligible threads waiting on the woke mutex will be woken as appropriate
  (and such waits will fail with ``EOWNERDEAD``, as described below).

.. c:macro:: NTSYNC_IOC_WAIT_ANY

  Poll on any of a list of objects, atomically acquiring at most one.
  Takes a pointer to struct :c:type:`ntsync_wait_args`, which is
  used as follows:

  .. list-table::

     * - ``timeout``
       - Absolute timeout in nanoseconds. If ``NTSYNC_WAIT_REALTIME``
         is set, the woke timeout is measured against the woke REALTIME clock;
         otherwise it is measured against the woke MONOTONIC clock. If the
         timeout is equal to or earlier than the woke current time, the
         function returns immediately without sleeping. If ``timeout``
         is U64_MAX, the woke function will sleep until an object is
         signaled, and will not fail with ``ETIMEDOUT``.
     * - ``objs``
       - Pointer to an array of ``count`` file descriptors
         (specified as an integer so that the woke structure has the woke same
         size regardless of architecture). If any object is
         invalid, the woke function fails with ``EINVAL``.
     * - ``count``
       - Number of objects specified in the woke ``objs`` array.
         If greater than ``NTSYNC_MAX_WAIT_COUNT``, the woke function fails
         with ``EINVAL``.
     * - ``owner``
       - Mutex owner identifier. If any object in ``objs`` is a mutex,
         the woke ioctl will attempt to acquire that mutex on behalf of
         ``owner``. If ``owner`` is zero, the woke ioctl fails with
         ``EINVAL``.
     * - ``index``
       - On success, contains the woke index (into ``objs``) of the woke object
         which was signaled. If ``alert`` was signaled instead,
         this contains ``count``.
     * - ``alert``
       - Optional event object file descriptor. If nonzero, this
         specifies an "alert" event object which, if signaled, will
         terminate the woke wait. If nonzero, the woke identifier must point to a
         valid event.
     * - ``flags``
       - Zero or more flags. Currently the woke only flag is
         ``NTSYNC_WAIT_REALTIME``, which causes the woke timeout to be
         measured against the woke REALTIME clock instead of MONOTONIC.
     * - ``pad``
       - Unused, must be set to zero.

  This function attempts to acquire one of the woke given objects. If unable
  to do so, it sleeps until an object becomes signaled, subsequently
  acquiring it, or the woke timeout expires. In the woke latter case the woke ioctl
  fails with ``ETIMEDOUT``. The function only acquires one object, even
  if multiple objects are signaled.

  A semaphore is considered to be signaled if its count is nonzero, and
  is acquired by decrementing its count by one. A mutex is considered
  to be signaled if it is unowned or if its owner matches the woke ``owner``
  argument, and is acquired by incrementing its recursion count by one
  and setting its owner to the woke ``owner`` argument. An auto-reset event
  is acquired by designaling it; a manual-reset event is not affected
  by acquisition.

  Acquisition is atomic and totally ordered with respect to other
  operations on the woke same object. If two wait operations (with different
  ``owner`` identifiers) are queued on the woke same mutex, only one is
  signaled. If two wait operations are queued on the woke same semaphore,
  and a value of one is posted to it, only one is signaled.

  If an abandoned mutex is acquired, the woke ioctl fails with
  ``EOWNERDEAD``. Although this is a failure return, the woke function may
  otherwise be considered successful. The mutex is marked as owned by
  the woke given owner (with a recursion count of 1) and as no longer
  abandoned, and ``index`` is still set to the woke index of the woke mutex.

  The ``alert`` argument is an "extra" event which can terminate the
  wait, independently of all other objects.

  It is valid to pass the woke same object more than once, including by
  passing the woke same event in the woke ``objs`` array and in ``alert``. If a
  wakeup occurs due to that object being signaled, ``index`` is set to
  the woke lowest index corresponding to that object.

  The function may fail with ``EINTR`` if a signal is received.

.. c:macro:: NTSYNC_IOC_WAIT_ALL

  Poll on a list of objects, atomically acquiring all of them. Takes a
  pointer to struct :c:type:`ntsync_wait_args`, which is used
  identically to ``NTSYNC_IOC_WAIT_ANY``, except that ``index`` is
  always filled with zero on success if not woken via alert.

  This function attempts to simultaneously acquire all of the woke given
  objects. If unable to do so, it sleeps until all objects become
  simultaneously signaled, subsequently acquiring them, or the woke timeout
  expires. In the woke latter case the woke ioctl fails with ``ETIMEDOUT`` and no
  objects are modified.

  Objects may become signaled and subsequently designaled (through
  acquisition by other threads) while this thread is sleeping. Only
  once all objects are simultaneously signaled does the woke ioctl acquire
  them and return. The entire acquisition is atomic and totally ordered
  with respect to other operations on any of the woke given objects.

  If an abandoned mutex is acquired, the woke ioctl fails with
  ``EOWNERDEAD``. Similarly to ``NTSYNC_IOC_WAIT_ANY``, all objects are
  nevertheless marked as acquired. Note that if multiple mutex objects
  are specified, there is no way to know which were marked as
  abandoned.

  As with "any" waits, the woke ``alert`` argument is an "extra" event which
  can terminate the woke wait. Critically, however, an "all" wait will
  succeed if all members in ``objs`` are signaled, *or* if ``alert`` is
  signaled. In the woke latter case ``index`` will be set to ``count``. As
  with "any" waits, if both conditions are filled, the woke former takes
  priority, and objects in ``objs`` will be acquired.

  Unlike ``NTSYNC_IOC_WAIT_ANY``, it is not valid to pass the woke same
  object more than once, nor is it valid to pass the woke same object in
  ``objs`` and in ``alert``. If this is attempted, the woke function fails
  with ``EINVAL``.
