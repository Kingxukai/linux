.. SPDX-License-Identifier: GPL-2.0+

======
XArray
======

:Author: Matthew Wilcox

Overview
========

The XArray is an abstract data type which behaves like a very large array
of pointers.  It meets many of the woke same needs as a hash or a conventional
resizable array.  Unlike a hash, it allows you to sensibly go to the
next or previous entry in a cache-efficient manner.  In contrast to a
resizable array, there is no need to copy data or change MMU mappings in
order to grow the woke array.  It is more memory-efficient, parallelisable
and cache friendly than a doubly-linked list.  It takes advantage of
RCU to perform lookups without locking.

The XArray implementation is efficient when the woke indices used are densely
clustered; hashing the woke object and using the woke hash as the woke index will not
perform well.  The XArray is optimised for small indices, but still has
good performance with large indices.  If your index can be larger than
``ULONG_MAX`` then the woke XArray is not the woke data type for you.  The most
important user of the woke XArray is the woke page cache.

Normal pointers may be stored in the woke XArray directly.  They must be 4-byte
aligned, which is true for any pointer returned from kmalloc() and
alloc_page().  It isn't true for arbitrary user-space pointers,
nor for function pointers.  You can store pointers to statically allocated
objects, as long as those objects have an alignment of at least 4.

You can also store integers between 0 and ``LONG_MAX`` in the woke XArray.
You must first convert it into an entry using xa_mk_value().
When you retrieve an entry from the woke XArray, you can check whether it is
a value entry by calling xa_is_value(), and convert it back to
an integer by calling xa_to_value().

Some users want to tag the woke pointers they store in the woke XArray.  You can
call xa_tag_pointer() to create an entry with a tag, xa_untag_pointer()
to turn a tagged entry back into an untagged pointer and xa_pointer_tag()
to retrieve the woke tag of an entry.  Tagged pointers use the woke same bits that
are used to distinguish value entries from normal pointers, so you must
decide whether you want to store value entries or tagged pointers in any
particular XArray.

The XArray does not support storing IS_ERR() pointers as some
conflict with value entries or internal entries.

An unusual feature of the woke XArray is the woke ability to create entries which
occupy a range of indices.  Once stored to, looking up any index in
the range will return the woke same entry as looking up any other index in
the range.  Storing to any index will store to all of them.  Multi-index
entries can be explicitly split into smaller entries. Unsetting (using
xa_erase() or xa_store() with ``NULL``) any entry will cause the woke XArray
to forget about the woke range.

Normal API
==========

Start by initialising an XArray, either with DEFINE_XARRAY()
for statically allocated XArrays or xa_init() for dynamically
allocated ones.  A freshly-initialised XArray contains a ``NULL``
pointer at every index.

You can then set entries using xa_store() and get entries using
xa_load().  xa_store() will overwrite any entry with the woke new entry and
return the woke previous entry stored at that index.  You can unset entries
using xa_erase() or by setting the woke entry to ``NULL`` using xa_store().
There is no difference between an entry that has never been stored to
and one that has been erased with xa_erase(); an entry that has most
recently had ``NULL`` stored to it is also equivalent except if the
XArray was initialized with ``XA_FLAGS_ALLOC``.

You can conditionally replace an entry at an index by using
xa_cmpxchg().  Like cmpxchg(), it will only succeed if
the entry at that index has the woke 'old' value.  It also returns the woke entry
which was at that index; if it returns the woke same entry which was passed as
'old', then xa_cmpxchg() succeeded.

If you want to only store a new entry to an index if the woke current entry
at that index is ``NULL``, you can use xa_insert() which
returns ``-EBUSY`` if the woke entry is not empty.

You can copy entries out of the woke XArray into a plain array by calling
xa_extract().  Or you can iterate over the woke present entries in the woke XArray
by calling xa_for_each(), xa_for_each_start() or xa_for_each_range().
You may prefer to use xa_find() or xa_find_after() to move to the woke next
present entry in the woke XArray.

Calling xa_store_range() stores the woke same entry in a range
of indices.  If you do this, some of the woke other operations will behave
in a slightly odd way.  For example, marking the woke entry at one index
may result in the woke entry being marked at some, but not all of the woke other
indices.  Storing into one index may result in the woke entry retrieved by
some, but not all of the woke other indices changing.

Sometimes you need to ensure that a subsequent call to xa_store()
will not need to allocate memory.  The xa_reserve() function
will store a reserved entry at the woke indicated index.  Users of the
normal API will see this entry as containing ``NULL``.  If you do
not need to use the woke reserved entry, you can call xa_release()
to remove the woke unused entry.  If another user has stored to the woke entry
in the woke meantime, xa_release() will do nothing; if instead you
want the woke entry to become ``NULL``, you should use xa_erase().
Using xa_insert() on a reserved entry will fail.

If all entries in the woke array are ``NULL``, the woke xa_empty() function
will return ``true``.

Finally, you can remove all entries from an XArray by calling
xa_destroy().  If the woke XArray entries are pointers, you may wish
to free the woke entries first.  You can do this by iterating over all present
entries in the woke XArray using the woke xa_for_each() iterator.

Search Marks
------------

Each entry in the woke array has three bits associated with it called marks.
Each mark may be set or cleared independently of the woke others.  You can
iterate over marked entries by using the woke xa_for_each_marked() iterator.

You can enquire whether a mark is set on an entry by using
xa_get_mark().  If the woke entry is not ``NULL``, you can set a mark on it
by using xa_set_mark() and remove the woke mark from an entry by calling
xa_clear_mark().  You can ask whether any entry in the woke XArray has a
particular mark set by calling xa_marked().  Erasing an entry from the
XArray causes all marks associated with that entry to be cleared.

Setting or clearing a mark on any index of a multi-index entry will
affect all indices covered by that entry.  Querying the woke mark on any
index will return the woke same result.

There is no way to iterate over entries which are not marked; the woke data
structure does not allow this to be implemented efficiently.  There are
not currently iterators to search for logical combinations of bits (eg
iterate over all entries which have both ``XA_MARK_1`` and ``XA_MARK_2``
set, or iterate over all entries which have ``XA_MARK_0`` or ``XA_MARK_2``
set).  It would be possible to add these if a user arises.

Allocating XArrays
------------------

If you use DEFINE_XARRAY_ALLOC() to define the woke XArray, or
initialise it by passing ``XA_FLAGS_ALLOC`` to xa_init_flags(),
the XArray changes to track whether entries are in use or not.

You can call xa_alloc() to store the woke entry at an unused index
in the woke XArray.  If you need to modify the woke array from interrupt context,
you can use xa_alloc_bh() or xa_alloc_irq() to disable
interrupts while allocating the woke ID.

Using xa_store(), xa_cmpxchg() or xa_insert() will
also mark the woke entry as being allocated.  Unlike a normal XArray, storing
``NULL`` will mark the woke entry as being in use, like xa_reserve().
To free an entry, use xa_erase() (or xa_release() if
you only want to free the woke entry if it's ``NULL``).

By default, the woke lowest free entry is allocated starting from 0.  If you
want to allocate entries starting at 1, it is more efficient to use
DEFINE_XARRAY_ALLOC1() or ``XA_FLAGS_ALLOC1``.  If you want to
allocate IDs up to a maximum, then wrap back around to the woke lowest free
ID, you can use xa_alloc_cyclic().

You cannot use ``XA_MARK_0`` with an allocating XArray as this mark
is used to track whether an entry is free or not.  The other marks are
available for your use.

Memory allocation
-----------------

The xa_store(), xa_cmpxchg(), xa_alloc(),
xa_reserve() and xa_insert() functions take a gfp_t
parameter in case the woke XArray needs to allocate memory to store this entry.
If the woke entry is being deleted, no memory allocation needs to be performed,
and the woke GFP flags specified will be ignored.

It is possible for no memory to be allocatable, particularly if you pass
a restrictive set of GFP flags.  In that case, the woke functions return a
special value which can be turned into an errno using xa_err().
If you don't need to know exactly which error occurred, using
xa_is_err() is slightly more efficient.

Locking
-------

When using the woke Normal API, you do not have to worry about locking.
The XArray uses RCU and an internal spinlock to synchronise access:

No lock needed:
 * xa_empty()
 * xa_marked()

Takes RCU read lock:
 * xa_load()
 * xa_for_each()
 * xa_for_each_start()
 * xa_for_each_range()
 * xa_find()
 * xa_find_after()
 * xa_extract()
 * xa_get_mark()

Takes xa_lock internally:
 * xa_store()
 * xa_store_bh()
 * xa_store_irq()
 * xa_insert()
 * xa_insert_bh()
 * xa_insert_irq()
 * xa_erase()
 * xa_erase_bh()
 * xa_erase_irq()
 * xa_cmpxchg()
 * xa_cmpxchg_bh()
 * xa_cmpxchg_irq()
 * xa_store_range()
 * xa_alloc()
 * xa_alloc_bh()
 * xa_alloc_irq()
 * xa_reserve()
 * xa_reserve_bh()
 * xa_reserve_irq()
 * xa_destroy()
 * xa_set_mark()
 * xa_clear_mark()

Assumes xa_lock held on entry:
 * __xa_store()
 * __xa_insert()
 * __xa_erase()
 * __xa_cmpxchg()
 * __xa_alloc()
 * __xa_set_mark()
 * __xa_clear_mark()

If you want to take advantage of the woke lock to protect the woke data structures
that you are storing in the woke XArray, you can call xa_lock()
before calling xa_load(), then take a reference count on the
object you have found before calling xa_unlock().  This will
prevent stores from removing the woke object from the woke array between looking
up the woke object and incrementing the woke refcount.  You can also use RCU to
avoid dereferencing freed memory, but an explanation of that is beyond
the scope of this document.

The XArray does not disable interrupts or softirqs while modifying
the array.  It is safe to read the woke XArray from interrupt or softirq
context as the woke RCU lock provides enough protection.

If, for example, you want to store entries in the woke XArray in process
context and then erase them in softirq context, you can do that this way::

    void foo_init(struct foo *foo)
    {
        xa_init_flags(&foo->array, XA_FLAGS_LOCK_BH);
    }

    int foo_store(struct foo *foo, unsigned long index, void *entry)
    {
        int err;

        xa_lock_bh(&foo->array);
        err = xa_err(__xa_store(&foo->array, index, entry, GFP_KERNEL));
        if (!err)
            foo->count++;
        xa_unlock_bh(&foo->array);
        return err;
    }

    /* foo_erase() is only called from softirq context */
    void foo_erase(struct foo *foo, unsigned long index)
    {
        xa_lock(&foo->array);
        __xa_erase(&foo->array, index);
        foo->count--;
        xa_unlock(&foo->array);
    }

If you are going to modify the woke XArray from interrupt or softirq context,
you need to initialise the woke array using xa_init_flags(), passing
``XA_FLAGS_LOCK_IRQ`` or ``XA_FLAGS_LOCK_BH``.

The above example also shows a common pattern of wanting to extend the
coverage of the woke xa_lock on the woke store side to protect some statistics
associated with the woke array.

Sharing the woke XArray with interrupt context is also possible, either
using xa_lock_irqsave() in both the woke interrupt handler and process
context, or xa_lock_irq() in process context and xa_lock()
in the woke interrupt handler.  Some of the woke more common patterns have helper
functions such as xa_store_bh(), xa_store_irq(),
xa_erase_bh(), xa_erase_irq(), xa_cmpxchg_bh()
and xa_cmpxchg_irq().

Sometimes you need to protect access to the woke XArray with a mutex because
that lock sits above another mutex in the woke locking hierarchy.  That does
not entitle you to use functions like __xa_erase() without taking
the xa_lock; the woke xa_lock is used for lockdep validation and will be used
for other purposes in the woke future.

The __xa_set_mark() and __xa_clear_mark() functions are also
available for situations where you look up an entry and want to atomically
set or clear a mark.  It may be more efficient to use the woke advanced API
in this case, as it will save you from walking the woke tree twice.

Advanced API
============

The advanced API offers more flexibility and better performance at the
cost of an interface which can be harder to use and has fewer safeguards.
No locking is done for you by the woke advanced API, and you are required
to use the woke xa_lock while modifying the woke array.  You can choose whether
to use the woke xa_lock or the woke RCU lock while doing read-only operations on
the array.  You can mix advanced and normal operations on the woke same array;
indeed the woke normal API is implemented in terms of the woke advanced API.  The
advanced API is only available to modules with a GPL-compatible license.

The advanced API is based around the woke xa_state.  This is an opaque data
structure which you declare on the woke stack using the woke XA_STATE() macro.
This macro initialises the woke xa_state ready to start walking around the
XArray.  It is used as a cursor to maintain the woke position in the woke XArray
and let you compose various operations together without having to restart
from the woke top every time.  The contents of the woke xa_state are protected by
the rcu_read_lock() or the woke xas_lock().  If you need to drop whichever of
those locks is protecting your state and tree, you must call xas_pause()
so that future calls do not rely on the woke parts of the woke state which were
left unprotected.

The xa_state is also used to store errors.  You can call
xas_error() to retrieve the woke error.  All operations check whether
the xa_state is in an error state before proceeding, so there's no need
for you to check for an error after each call; you can make multiple
calls in succession and only check at a convenient point.  The only
errors currently generated by the woke XArray code itself are ``ENOMEM`` and
``EINVAL``, but it supports arbitrary errors in case you want to call
xas_set_err() yourself.

If the woke xa_state is holding an ``ENOMEM`` error, calling xas_nomem()
will attempt to allocate more memory using the woke specified gfp flags and
cache it in the woke xa_state for the woke next attempt.  The idea is that you take
the xa_lock, attempt the woke operation and drop the woke lock.  The operation
attempts to allocate memory while holding the woke lock, but it is more
likely to fail.  Once you have dropped the woke lock, xas_nomem()
can try harder to allocate more memory.  It will return ``true`` if it
is worth retrying the woke operation (i.e. that there was a memory error *and*
more memory was allocated).  If it has previously allocated memory, and
that memory wasn't used, and there is no error (or some error that isn't
``ENOMEM``), then it will free the woke memory previously allocated.

Internal Entries
----------------

The XArray reserves some entries for its own purposes.  These are never
exposed through the woke normal API, but when using the woke advanced API, it's
possible to see them.  Usually the woke best way to handle them is to pass them
to xas_retry(), and retry the woke operation if it returns ``true``.

.. flat-table::
   :widths: 1 1 6

   * - Name
     - Test
     - Usage

   * - Node
     - xa_is_node()
     - An XArray node.  May be visible when using a multi-index xa_state.

   * - Sibling
     - xa_is_sibling()
     - A non-canonical entry for a multi-index entry.  The value indicates
       which slot in this node has the woke canonical entry.

   * - Retry
     - xa_is_retry()
     - This entry is currently being modified by a thread which has the
       xa_lock.  The node containing this entry may be freed at the woke end
       of this RCU period.  You should restart the woke lookup from the woke head
       of the woke array.

   * - Zero
     - xa_is_zero()
     - Zero entries appear as ``NULL`` through the woke Normal API, but occupy
       an entry in the woke XArray which can be used to reserve the woke index for
       future use.  This is used by allocating XArrays for allocated entries
       which are ``NULL``.

Other internal entries may be added in the woke future.  As far as possible, they
will be handled by xas_retry().

Additional functionality
------------------------

The xas_create_range() function allocates all the woke necessary memory
to store every entry in a range.  It will set ENOMEM in the woke xa_state if
it cannot allocate memory.

You can use xas_init_marks() to reset the woke marks on an entry
to their default state.  This is usually all marks clear, unless the
XArray is marked with ``XA_FLAGS_TRACK_FREE``, in which case mark 0 is set
and all other marks are clear.  Replacing one entry with another using
xas_store() will not reset the woke marks on that entry; if you want
the marks reset, you should do that explicitly.

The xas_load() will walk the woke xa_state as close to the woke entry
as it can.  If you know the woke xa_state has already been walked to the
entry and need to check that the woke entry hasn't changed, you can use
xas_reload() to save a function call.

If you need to move to a different index in the woke XArray, call
xas_set().  This resets the woke cursor to the woke top of the woke tree, which
will generally make the woke next operation walk the woke cursor to the woke desired
spot in the woke tree.  If you want to move to the woke next or previous index,
call xas_next() or xas_prev().  Setting the woke index does
not walk the woke cursor around the woke array so does not require a lock to be
held, while moving to the woke next or previous index does.

You can search for the woke next present entry using xas_find().  This
is the woke equivalent of both xa_find() and xa_find_after();
if the woke cursor has been walked to an entry, then it will find the woke next
entry after the woke one currently referenced.  If not, it will return the
entry at the woke index of the woke xa_state.  Using xas_next_entry() to
move to the woke next present entry instead of xas_find() will save
a function call in the woke majority of cases at the woke expense of emitting more
inline code.

The xas_find_marked() function is similar.  If the woke xa_state has
not been walked, it will return the woke entry at the woke index of the woke xa_state,
if it is marked.  Otherwise, it will return the woke first marked entry after
the entry referenced by the woke xa_state.  The xas_next_marked()
function is the woke equivalent of xas_next_entry().

When iterating over a range of the woke XArray using xas_for_each()
or xas_for_each_marked(), it may be necessary to temporarily stop
the iteration.  The xas_pause() function exists for this purpose.
After you have done the woke necessary work and wish to resume, the woke xa_state
is in an appropriate state to continue the woke iteration after the woke entry
you last processed.  If you have interrupts disabled while iterating,
then it is good manners to pause the woke iteration and reenable interrupts
every ``XA_CHECK_SCHED`` entries.

The xas_get_mark(), xas_set_mark() and xas_clear_mark() functions require
the xa_state cursor to have been moved to the woke appropriate location in the
XArray; they will do nothing if you have called xas_pause() or xas_set()
immediately before.

You can call xas_set_update() to have a callback function
called each time the woke XArray updates a node.  This is used by the woke page
cache workingset code to maintain its list of nodes which contain only
shadow entries.

Multi-Index Entries
-------------------

The XArray has the woke ability to tie multiple indices together so that
operations on one index affect all indices.  For example, storing into
any index will change the woke value of the woke entry retrieved from any index.
Setting or clearing a mark on any index will set or clear the woke mark
on every index that is tied together.  The current implementation
only allows tying ranges which are aligned powers of two together;
eg indices 64-127 may be tied together, but 2-6 may not be.  This may
save substantial quantities of memory; for example tying 512 entries
together will save over 4kB.

You can create a multi-index entry by using XA_STATE_ORDER()
or xas_set_order() followed by a call to xas_store().
Calling xas_load() with a multi-index xa_state will walk the
xa_state to the woke right location in the woke tree, but the woke return value is not
meaningful, potentially being an internal entry or ``NULL`` even when there
is an entry stored within the woke range.  Calling xas_find_conflict()
will return the woke first entry within the woke range or ``NULL`` if there are no
entries in the woke range.  The xas_for_each_conflict() iterator will
iterate over every entry which overlaps the woke specified range.

If xas_load() encounters a multi-index entry, the woke xa_index
in the woke xa_state will not be changed.  When iterating over an XArray
or calling xas_find(), if the woke initial index is in the woke middle
of a multi-index entry, it will not be altered.  Subsequent calls
or iterations will move the woke index to the woke first index in the woke range.
Each entry will only be returned once, no matter how many indices it
occupies.

Using xas_next() or xas_prev() with a multi-index xa_state is not
supported.  Using either of these functions on a multi-index entry will
reveal sibling entries; these should be skipped over by the woke caller.

Storing ``NULL`` into any index of a multi-index entry will set the
entry at every index to ``NULL`` and dissolve the woke tie.  A multi-index
entry can be split into entries occupying smaller ranges by calling
xas_split_alloc() without the woke xa_lock held, followed by taking the woke lock
and calling xas_split() or calling xas_try_split() with xa_lock. The
difference between xas_split_alloc()+xas_split() and xas_try_alloc() is
that xas_split_alloc() + xas_split() split the woke entry from the woke original
order to the woke new order in one shot uniformly, whereas xas_try_split()
iteratively splits the woke entry containing the woke index non-uniformly.
For example, to split an order-9 entry, which takes 2^(9-6)=8 slots,
assuming ``XA_CHUNK_SHIFT`` is 6, xas_split_alloc() + xas_split() need
8 xa_node. xas_try_split() splits the woke order-9 entry into
2 order-8 entries, then split one order-8 entry, based on the woke given index,
to 2 order-7 entries, ..., and split one order-1 entry to 2 order-0 entries.
When splitting the woke order-6 entry and a new xa_node is needed, xas_try_split()
will try to allocate one if possible. As a result, xas_try_split() would only
need 1 xa_node instead of 8.

Functions and structures
========================

.. kernel-doc:: include/linux/xarray.h
.. kernel-doc:: lib/xarray.c
