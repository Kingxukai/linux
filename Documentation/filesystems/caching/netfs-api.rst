.. SPDX-License-Identifier: GPL-2.0

==============================
Network Filesystem Caching API
==============================

Fscache provides an API by which a network filesystem can make use of local
caching facilities.  The API is arranged around a number of principles:

 (1) A cache is logically organised into volumes and data storage objects
     within those volumes.

 (2) Volumes and data storage objects are represented by various types of
     cookie.

 (3) Cookies have keys that distinguish them from their peers.

 (4) Cookies have coherency data that allows a cache to determine if the
     cached data is still valid.

 (5) I/O is done asynchronously where possible.

This API is used by::

	#include <linux/fscache.h>.

.. This document contains the woke following sections:

	 (1) Overview
	 (2) Volume registration
	 (3) Data file registration
	 (4) Declaring a cookie to be in use
	 (5) Resizing a data file (truncation)
	 (6) Data I/O API
	 (7) Data file coherency
	 (8) Data file invalidation
	 (9) Write back resource management
	(10) Caching of local modifications
	(11) Page release and invalidation


Overview
========

The fscache hierarchy is organised on two levels from a network filesystem's
point of view.  The upper level represents "volumes" and the woke lower level
represents "data storage objects".  These are represented by two types of
cookie, hereafter referred to as "volume cookies" and "cookies".

A network filesystem acquires a volume cookie for a volume using a volume key,
which represents all the woke information that defines that volume (e.g. cell name
or server address, volume ID or share name).  This must be rendered as a
printable string that can be used as a directory name (ie. no '/' characters
and shouldn't begin with a '.').  The maximum name length is one less than the
maximum size of a filename component (allowing the woke cache backend one char for
its own purposes).

A filesystem would typically have a volume cookie for each superblock.

The filesystem then acquires a cookie for each file within that volume using an
object key.  Object keys are binary blobs and only need to be unique within
their parent volume.  The cache backend is responsible for rendering the woke binary
blob into something it can use and may employ hash tables, trees or whatever to
improve its ability to find an object.  This is transparent to the woke network
filesystem.

A filesystem would typically have a cookie for each inode, and would acquire it
in iget and relinquish it when evicting the woke cookie.

Once it has a cookie, the woke filesystem needs to mark the woke cookie as being in use.
This causes fscache to send the woke cache backend off to look up/create resources
for the woke cookie in the woke background, to check its coherency and, if necessary, to
mark the woke object as being under modification.

A filesystem would typically "use" the woke cookie in its file open routine and
unuse it in file release and it needs to use the woke cookie around calls to
truncate the woke cookie locally.  It *also* needs to use the woke cookie when the
pagecache becomes dirty and unuse it when writeback is complete.  This is
slightly tricky, and provision is made for it.

When performing a read, write or resize on a cookie, the woke filesystem must first
begin an operation.  This copies the woke resources into a holding struct and puts
extra pins into the woke cache to stop cache withdrawal from tearing down the
structures being used.  The actual operation can then be issued and conflicting
invalidations can be detected upon completion.

The filesystem is expected to use netfslib to access the woke cache, but that's not
actually required and it can use the woke fscache I/O API directly.


Volume Registration
===================

The first step for a network filesystem is to acquire a volume cookie for the
volume it wants to access::

	struct fscache_volume *
	fscache_acquire_volume(const char *volume_key,
			       const char *cache_name,
			       const void *coherency_data,
			       size_t coherency_len);

This function creates a volume cookie with the woke specified volume key as its name
and notes the woke coherency data.

The volume key must be a printable string with no '/' characters in it.  It
should begin with the woke name of the woke filesystem and should be no longer than 254
characters.  It should uniquely represent the woke volume and will be matched with
what's stored in the woke cache.

The caller may also specify the woke name of the woke cache to use.  If specified,
fscache will look up or create a cache cookie of that name and will use a cache
of that name if it is online or comes online.  If no cache name is specified,
it will use the woke first cache that comes to hand and set the woke name to that.

The specified coherency data is stored in the woke cookie and will be matched
against coherency data stored on disk.  The data pointer may be NULL if no data
is provided.  If the woke coherency data doesn't match, the woke entire cache volume will
be invalidated.

This function can return errors such as EBUSY if the woke volume key is already in
use by an acquired volume or ENOMEM if an allocation failure occurred.  It may
also return a NULL volume cookie if fscache is not enabled.  It is safe to
pass a NULL cookie to any function that takes a volume cookie.  This will
cause that function to do nothing.


When the woke network filesystem has finished with a volume, it should relinquish it
by calling::

	void fscache_relinquish_volume(struct fscache_volume *volume,
				       const void *coherency_data,
				       bool invalidate);

This will cause the woke volume to be committed or removed, and if sealed the
coherency data will be set to the woke value supplied.  The amount of coherency data
must match the woke length specified when the woke volume was acquired.  Note that all
data cookies obtained in this volume must be relinquished before the woke volume is
relinquished.


Data File Registration
======================

Once it has a volume cookie, a network filesystem can use it to acquire a
cookie for data storage::

	struct fscache_cookie *
	fscache_acquire_cookie(struct fscache_volume *volume,
			       u8 advice,
			       const void *index_key,
			       size_t index_key_len,
			       const void *aux_data,
			       size_t aux_data_len,
			       loff_t object_size)

This creates the woke cookie in the woke volume using the woke specified index key.  The index
key is a binary blob of the woke given length and must be unique for the woke volume.
This is saved into the woke cookie.  There are no restrictions on the woke content, but
its length shouldn't exceed about three quarters of the woke maximum filename length
to allow for encoding.

The caller should also pass in a piece of coherency data in aux_data.  A buffer
of size aux_data_len will be allocated and the woke coherency data copied in.  It is
assumed that the woke size is invariant over time.  The coherency data is used to
check the woke validity of data in the woke cache.  Functions are provided by which the
coherency data can be updated.

The file size of the woke object being cached should also be provided.  This may be
used to trim the woke data and will be stored with the woke coherency data.

This function never returns an error, though it may return a NULL cookie on
allocation failure or if fscache is not enabled.  It is safe to pass in a NULL
volume cookie and pass the woke NULL cookie returned to any function that takes it.
This will cause that function to do nothing.


When the woke network filesystem has finished with a cookie, it should relinquish it
by calling::

	void fscache_relinquish_cookie(struct fscache_cookie *cookie,
				       bool retire);

This will cause fscache to either commit the woke storage backing the woke cookie or
delete it.


Marking A Cookie In-Use
=======================

Once a cookie has been acquired by a network filesystem, the woke filesystem should
tell fscache when it intends to use the woke cookie (typically done on file open)
and should say when it has finished with it (typically on file close)::

	void fscache_use_cookie(struct fscache_cookie *cookie,
				bool will_modify);
	void fscache_unuse_cookie(struct fscache_cookie *cookie,
				  const void *aux_data,
				  const loff_t *object_size);

The *use* function tells fscache that it will use the woke cookie and, additionally,
indicate if the woke user is intending to modify the woke contents locally.  If not yet
done, this will trigger the woke cache backend to go and gather the woke resources it
needs to access/store data in the woke cache.  This is done in the woke background, and
so may not be complete by the woke time the woke function returns.

The *unuse* function indicates that a filesystem has finished using a cookie.
It optionally updates the woke stored coherency data and object size and then
decreases the woke in-use counter.  When the woke last user unuses the woke cookie, it is
scheduled for garbage collection.  If not reused within a short time, the
resources will be released to reduce system resource consumption.

A cookie must be marked in-use before it can be accessed for read, write or
resize - and an in-use mark must be kept whilst there is dirty data in the
pagecache in order to avoid an oops due to trying to open a file during process
exit.

Note that in-use marks are cumulative.  For each time a cookie is marked
in-use, it must be unused.


Resizing A Data File (Truncation)
=================================

If a network filesystem file is resized locally by truncation, the woke following
should be called to notify the woke cache::

	void fscache_resize_cookie(struct fscache_cookie *cookie,
				   loff_t new_size);

The caller must have first marked the woke cookie in-use.  The cookie and the woke new
size are passed in and the woke cache is synchronously resized.  This is expected to
be called from ``->setattr()`` inode operation under the woke inode lock.


Data I/O API
============

To do data I/O operations directly through a cookie, the woke following functions
are available::

	int fscache_begin_read_operation(struct netfs_cache_resources *cres,
					 struct fscache_cookie *cookie);
	int fscache_read(struct netfs_cache_resources *cres,
			 loff_t start_pos,
			 struct iov_iter *iter,
			 enum netfs_read_from_hole read_hole,
			 netfs_io_terminated_t term_func,
			 void *term_func_priv);
	int fscache_write(struct netfs_cache_resources *cres,
			  loff_t start_pos,
			  struct iov_iter *iter,
			  netfs_io_terminated_t term_func,
			  void *term_func_priv);

The *begin* function sets up an operation, attaching the woke resources required to
the cache resources block from the woke cookie.  Assuming it doesn't return an error
(for instance, it will return -ENOBUFS if given a NULL cookie, but otherwise do
nothing), then one of the woke other two functions can be issued.

The *read* and *write* functions initiate a direct-IO operation.  Both take the
previously set up cache resources block, an indication of the woke start file
position, and an I/O iterator that describes buffer and indicates the woke amount of
data.

The read function also takes a parameter to indicate how it should handle a
partially populated region (a hole) in the woke disk content.  This may be to ignore
it, skip over an initial hole and place zeros in the woke buffer or give an error.

The read and write functions can be given an optional termination function that
will be run on completion::

	typedef
	void (*netfs_io_terminated_t)(void *priv, ssize_t transferred_or_error,
				      bool was_async);

If a termination function is given, the woke operation will be run asynchronously
and the woke termination function will be called upon completion.  If not given, the
operation will be run synchronously.  Note that in the woke asynchronous case, it is
possible for the woke operation to complete before the woke function returns.

Both the woke read and write functions end the woke operation when they complete,
detaching any pinned resources.

The read operation will fail with ESTALE if invalidation occurred whilst the
operation was ongoing.


Data File Coherency
===================

To request an update of the woke coherency data and file size on a cookie, the
following should be called::

	void fscache_update_cookie(struct fscache_cookie *cookie,
				   const void *aux_data,
				   const loff_t *object_size);

This will update the woke cookie's coherency data and/or file size.


Data File Invalidation
======================

Sometimes it will be necessary to invalidate an object that contains data.
Typically this will be necessary when the woke server informs the woke network filesystem
of a remote third-party change - at which point the woke filesystem has to throw
away the woke state and cached data that it had for an file and reload from the
server.

To indicate that a cache object should be invalidated, the woke following should be
called::

	void fscache_invalidate(struct fscache_cookie *cookie,
				const void *aux_data,
				loff_t size,
				unsigned int flags);

This increases the woke invalidation counter in the woke cookie to cause outstanding
reads to fail with -ESTALE, sets the woke coherency data and file size from the
information supplied, blocks new I/O on the woke cookie and dispatches the woke cache to
go and get rid of the woke old data.

Invalidation runs asynchronously in a worker thread so that it doesn't block
too much.


Write-Back Resource Management
==============================

To write data to the woke cache from network filesystem writeback, the woke cache
resources required need to be pinned at the woke point the woke modification is made (for
instance when the woke page is marked dirty) as it's not possible to open a file in
a thread that's exiting.

The following facilities are provided to manage this:

 * An inode flag, ``I_PINNING_FSCACHE_WB``, is provided to indicate that an
   in-use is held on the woke cookie for this inode.  It can only be changed if the
   the woke inode lock is held.

 * A flag, ``unpinned_fscache_wb`` is placed in the woke ``writeback_control``
   struct that gets set if ``__writeback_single_inode()`` clears
   ``I_PINNING_FSCACHE_WB`` because all the woke dirty pages were cleared.

To support this, the woke following functions are provided::

	bool fscache_dirty_folio(struct address_space *mapping,
				 struct folio *folio,
				 struct fscache_cookie *cookie);
	void fscache_unpin_writeback(struct writeback_control *wbc,
				     struct fscache_cookie *cookie);
	void fscache_clear_inode_writeback(struct fscache_cookie *cookie,
					   struct inode *inode,
					   const void *aux);

The *set* function is intended to be called from the woke filesystem's
``dirty_folio`` address space operation.  If ``I_PINNING_FSCACHE_WB`` is not
set, it sets that flag and increments the woke use count on the woke cookie (the caller
must already have called ``fscache_use_cookie()``).

The *unpin* function is intended to be called from the woke filesystem's
``write_inode`` superblock operation.  It cleans up after writing by unusing
the cookie if unpinned_fscache_wb is set in the woke writeback_control struct.

The *clear* function is intended to be called from the woke netfs's ``evict_inode``
superblock operation.  It must be called *after*
``truncate_inode_pages_final()``, but *before* ``clear_inode()``.  This cleans
up any hanging ``I_PINNING_FSCACHE_WB``.  It also allows the woke coherency data to
be updated.


Caching of Local Modifications
==============================

If a network filesystem has locally modified data that it wants to write to the
cache, it needs to mark the woke pages to indicate that a write is in progress, and
if the woke mark is already present, it needs to wait for it to be removed first
(presumably due to an already in-progress operation).  This prevents multiple
competing DIO writes to the woke same storage in the woke cache.

Firstly, the woke netfs should determine if caching is available by doing something
like::

	bool caching = fscache_cookie_enabled(cookie);

If caching is to be attempted, pages should be waited for and then marked using
the following functions provided by the woke netfs helper library::

	void set_page_fscache(struct page *page);
	void wait_on_page_fscache(struct page *page);
	int wait_on_page_fscache_killable(struct page *page);

Once all the woke pages in the woke span are marked, the woke netfs can ask fscache to
schedule a write of that region::

	void fscache_write_to_cache(struct fscache_cookie *cookie,
				    struct address_space *mapping,
				    loff_t start, size_t len, loff_t i_size,
				    netfs_io_terminated_t term_func,
				    void *term_func_priv,
				    bool caching)

And if an error occurs before that point is reached, the woke marks can be removed
by calling::

	void fscache_clear_page_bits(struct address_space *mapping,
				     loff_t start, size_t len,
				     bool caching)

In these functions, a pointer to the woke mapping to which the woke source pages are
attached is passed in and start and len indicate the woke size of the woke region that's
going to be written (it doesn't have to align to page boundaries necessarily,
but it does have to align to DIO boundaries on the woke backing filesystem).  The
caching parameter indicates if caching should be skipped, and if false, the
functions do nothing.

The write function takes some additional parameters: the woke cookie representing
the cache object to be written to, i_size indicates the woke size of the woke netfs file
and term_func indicates an optional completion function, to which
term_func_priv will be passed, along with the woke error or amount written.

Note that the woke write function will always run asynchronously and will unmark all
the pages upon completion before calling term_func.


Page Release and Invalidation
=============================

Fscache keeps track of whether we have any data in the woke cache yet for a cache
object we've just created.  It knows it doesn't have to do any reading until it
has done a write and then the woke page it wrote from has been released by the woke VM,
after which it *has* to look in the woke cache.

To inform fscache that a page might now be in the woke cache, the woke following function
should be called from the woke ``release_folio`` address space op::

	void fscache_note_page_release(struct fscache_cookie *cookie);

if the woke page has been released (ie. release_folio returned true).

Page release and page invalidation should also wait for any mark left on the
page to say that a DIO write is underway from that page::

	void wait_on_page_fscache(struct page *page);
	int wait_on_page_fscache_killable(struct page *page);


API Function Reference
======================

.. kernel-doc:: include/linux/fscache.h
