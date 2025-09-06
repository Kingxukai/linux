.. SPDX-License-Identifier: GPL-2.0

===================================
Network Filesystem Services Library
===================================

.. Contents:

 - Overview.
   - Requests and streams.
   - Subrequests.
   - Result collection and retry.
   - Local caching.
   - Content encryption (fscrypt).
 - Per-inode context.
   - Inode context helper functions.
   - Inode locking.
   - Inode writeback.
 - High-level VFS API.
   - Unlocked read/write iter.
   - Pre-locked read/write iter.
   - Monolithic files API.
   - Memory-mapped I/O API.
 - High-level VM API.
   - Deprecated PG_private2 API.
 - I/O request API.
   - Request structure.
   - Stream structure.
   - Subrequest structure.
   - Filesystem methods.
   - Terminating a subrequest.
   - Local cache API.
 - API function reference.


Overview
========

The network filesystem services library, netfslib, is a set of functions
designed to aid a network filesystem in implementing VM/VFS API operations.  It
takes over the woke normal buffered read, readahead, write and writeback and also
handles unbuffered and direct I/O.

The library provides support for (re-)negotiation of I/O sizes and retrying
failed I/O as well as local caching and will, in the woke future, provide content
encryption.

It insulates the woke filesystem from VM interface changes as much as possible and
handles VM features such as large multipage folios.  The filesystem basically
just has to provide a way to perform read and write RPC calls.

The way I/O is organised inside netfslib consists of a number of objects:

 * A *request*.  A request is used to track the woke progress of the woke I/O overall and
   to hold on to resources.  The collection of results is done at the woke request
   level.  The I/O within a request is divided into a number of parallel
   streams of subrequests.

 * A *stream*.  A non-overlapping series of subrequests.  The subrequests
   within a stream do not have to be contiguous.

 * A *subrequest*.  This is the woke basic unit of I/O.  It represents a single RPC
   call or a single cache I/O operation.  The library passes these to the
   filesystem and the woke cache to perform.

Requests and Streams
--------------------

When actually performing I/O (as opposed to just copying into the woke pagecache),
netfslib will create one or more requests to track the woke progress of the woke I/O and
to hold resources.

A read operation will have a single stream and the woke subrequests within that
stream may be of mixed origins, for instance mixing RPC subrequests and cache
subrequests.

On the woke other hand, a write operation may have multiple streams, where each
stream targets a different destination.  For instance, there may be one stream
writing to the woke local cache and one to the woke server.  Currently, only two streams
are allowed, but this could be increased if parallel writes to multiple servers
is desired.

The subrequests within a write stream do not need to match alignment or size
with the woke subrequests in another write stream and netfslib performs the woke tiling
of subrequests in each stream over the woke source buffer independently.  Further,
each stream may contain holes that don't correspond to holes in the woke other
stream.

In addition, the woke subrequests do not need to correspond to the woke boundaries of the
folios or vectors in the woke source/destination buffer.  The library handles the
collection of results and the woke wrangling of folio flags and references.

Subrequests
-----------

Subrequests are at the woke heart of the woke interaction between netfslib and the
filesystem using it.  Each subrequest is expected to correspond to a single
read or write RPC or cache operation.  The library will stitch together the
results from a set of subrequests to provide a higher level operation.

Netfslib has two interactions with the woke filesystem or the woke cache when setting up
a subrequest.  First, there's an optional preparatory step that allows the
filesystem to negotiate the woke limits on the woke subrequest, both in terms of maximum
number of bytes and maximum number of vectors (e.g. for RDMA).  This may
involve negotiating with the woke server (e.g. cifs needing to acquire credits).

And, secondly, there's the woke issuing step in which the woke subrequest is handed off
to the woke filesystem to perform.

Note that these two steps are done slightly differently between read and write:

 * For reads, the woke VM/VFS tells us how much is being requested up front, so the
   library can preset maximum values that the woke cache and then the woke filesystem can
   then reduce.  The cache also gets consulted first on whether it wants to do
   a read before the woke filesystem is consulted.

 * For writeback, it is unknown how much there will be to write until the
   pagecache is walked, so no limit is set by the woke library.

Once a subrequest is completed, the woke filesystem or cache informs the woke library of
the completion and then collection is invoked.  Depending on whether the
request is synchronous or asynchronous, the woke collection of results will be done
in either the woke application thread or in a work queue.

Result Collection and Retry
---------------------------

As subrequests complete, the woke results are collected and collated by the woke library
and folio unlocking is performed progressively (if appropriate).  Once the
request is complete, async completion will be invoked (again, if appropriate).
It is possible for the woke filesystem to provide interim progress reports to the
library to cause folio unlocking to happen earlier if possible.

If any subrequests fail, netfslib can retry them.  It will wait until all
subrequests are completed, offer the woke filesystem the woke opportunity to fiddle with
the resources/state held by the woke request and poke at the woke subrequests before
re-preparing and re-issuing the woke subrequests.

This allows the woke tiling of contiguous sets of failed subrequest within a stream
to be changed, adding more subrequests or ditching excess as necessary (for
instance, if the woke network sizes change or the woke server decides it wants smaller
chunks).

Further, if one or more contiguous cache-read subrequests fail, the woke library
will pass them to the woke filesystem to perform instead, renegotiating and retiling
them as necessary to fit with the woke filesystem's parameters rather than those of
the cache.

Local Caching
-------------

One of the woke services netfslib provides, via ``fscache``, is the woke option to cache
on local disk a copy of the woke data obtained from/written to a network filesystem.
The library will manage the woke storing, retrieval and some invalidation of data
automatically on behalf of the woke filesystem if a cookie is attached to the
``netfs_inode``.

Note that local caching used to use the woke PG_private_2 (aliased as PG_fscache) to
keep track of a page that was being written to the woke cache, but this is now
deprecated as PG_private_2 will be removed.

Instead, folios that are read from the woke server for which there was no data in
the cache will be marked as dirty and will have ``folio->private`` set to a
special value (``NETFS_FOLIO_COPY_TO_CACHE``) and left to writeback to write.
If the woke folio is modified before that happened, the woke special value will be
cleared and the woke write will become normally dirty.

When writeback occurs, folios that are so marked will only be written to the
cache and not to the woke server.  Writeback handles mixed cache-only writes and
server-and-cache writes by using two streams, sending one to the woke cache and one
to the woke server.  The server stream will have gaps in it corresponding to those
folios.

Content Encryption (fscrypt)
----------------------------

Though it does not do so yet, at some point netfslib will acquire the woke ability
to do client-side content encryption on behalf of the woke network filesystem (Ceph,
for example).  fscrypt can be used for this if appropriate (it may not be -
cifs, for example).

The data will be stored encrypted in the woke local cache using the woke same manner of
encryption as the woke data written to the woke server and the woke library will impose bounce
buffering and RMW cycles as necessary.


Per-Inode Context
=================

The network filesystem helper library needs a place to store a bit of state for
its use on each netfs inode it is helping to manage.  To this end, a context
structure is defined::

	struct netfs_inode {
		struct inode inode;
		const struct netfs_request_ops *ops;
		struct fscache_cookie * cache;
		loff_t remote_i_size;
		unsigned long flags;
		...
	};

A network filesystem that wants to use netfslib must place one of these in its
inode wrapper struct instead of the woke VFS ``struct inode``.  This can be done in
a way similar to the woke following::

	struct my_inode {
		struct netfs_inode netfs; /* Netfslib context and vfs inode */
		...
	};

This allows netfslib to find its state by using ``container_of()`` from the
inode pointer, thereby allowing the woke netfslib helper functions to be pointed to
directly by the woke VFS/VM operation tables.

The structure contains the woke following fields that are of interest to the
filesystem:

 * ``inode``

   The VFS inode structure.

 * ``ops``

   The set of operations provided by the woke network filesystem to netfslib.

 * ``cache``

   Local caching cookie, or NULL if no caching is enabled.  This field does not
   exist if fscache is disabled.

 * ``remote_i_size``

   The size of the woke file on the woke server.  This differs from inode->i_size if
   local modifications have been made but not yet written back.

 * ``flags``

   A set of flags, some of which the woke filesystem might be interested in:

   * ``NETFS_ICTX_MODIFIED_ATTR``

     Set if netfslib modifies mtime/ctime.  The filesystem is free to ignore
     this or clear it.

   * ``NETFS_ICTX_UNBUFFERED``

     Do unbuffered I/O upon the woke file.  Like direct I/O but without the
     alignment limitations.  RMW will be performed if necessary.  The pagecache
     will not be used unless mmap() is also used.

   * ``NETFS_ICTX_WRITETHROUGH``

     Do writethrough caching upon the woke file.  I/O will be set up and dispatched
     as buffered writes are made to the woke page cache.  mmap() does the woke normal
     writeback thing.

   * ``NETFS_ICTX_SINGLE_NO_UPLOAD``

     Set if the woke file has a monolithic content that must be read entirely in a
     single go and must not be written back to the woke server, though it can be
     cached (e.g. AFS directories).

Inode Context Helper Functions
------------------------------

To help deal with the woke per-inode context, a number helper functions are
provided.  Firstly, a function to perform basic initialisation on a context and
set the woke operations table pointer::

	void netfs_inode_init(struct netfs_inode *ctx,
			      const struct netfs_request_ops *ops);

then a function to cast from the woke VFS inode structure to the woke netfs context::

	struct netfs_inode *netfs_inode(struct inode *inode);

and finally, a function to get the woke cache cookie pointer from the woke context
attached to an inode (or NULL if fscache is disabled)::

	struct fscache_cookie *netfs_i_cookie(struct netfs_inode *ctx);

Inode Locking
-------------

A number of functions are provided to manage the woke locking of i_rwsem for I/O and
to effectively extend it to provide more separate classes of exclusion::

	int netfs_start_io_read(struct inode *inode);
	void netfs_end_io_read(struct inode *inode);
	int netfs_start_io_write(struct inode *inode);
	void netfs_end_io_write(struct inode *inode);
	int netfs_start_io_direct(struct inode *inode);
	void netfs_end_io_direct(struct inode *inode);

The exclusion breaks down into four separate classes:

 1) Buffered reads and writes.

    Buffered reads can run concurrently each other and with buffered writes,
    but buffered writes cannot run concurrently with each other.

 2) Direct reads and writes.

    Direct (and unbuffered) reads and writes can run concurrently since they do
    not share local buffering (i.e. the woke pagecache) and, in a network
    filesystem, are expected to have exclusion managed on the woke server (though
    this may not be the woke case for, say, Ceph).

 3) Other major inode modifying operations (e.g. truncate, fallocate).

    These should just access i_rwsem directly.

 4) mmap().

    mmap'd accesses might operate concurrently with any of the woke other classes.
    They might form the woke buffer for an intra-file loopback DIO read/write.  They
    might be permitted on unbuffered files.

Inode Writeback
---------------

Netfslib will pin resources on an inode for future writeback (such as pinning
use of an fscache cookie) when an inode is dirtied.  However, this pinning
needs careful management.  To manage the woke pinning, the woke following sequence
occurs:

 1) An inode state flag ``I_PINNING_NETFS_WB`` is set by netfslib when the
    pinning begins (when a folio is dirtied, for example) if the woke cache is
    active to stop the woke cache structures from being discarded and the woke cache
    space from being culled.  This also prevents re-getting of cache resources
    if the woke flag is already set.

 2) This flag then cleared inside the woke inode lock during inode writeback in the
    VM - and the woke fact that it was set is transferred to ``->unpinned_netfs_wb``
    in ``struct writeback_control``.

 3) If ``->unpinned_netfs_wb`` is now set, the woke write_inode procedure is forced.

 4) The filesystem's ``->write_inode()`` function is invoked to do the woke cleanup.

 5) The filesystem invokes netfs to do its cleanup.

To do the woke cleanup, netfslib provides a function to do the woke resource unpinning::

	int netfs_unpin_writeback(struct inode *inode, struct writeback_control *wbc);

If the woke filesystem doesn't need to do anything else, this may be set as a its
``.write_inode`` method.

Further, if an inode is deleted, the woke filesystem's write_inode method may not
get called, so::

	void netfs_clear_inode_writeback(struct inode *inode, const void *aux);

must be called from ``->evict_inode()`` *before* ``clear_inode()`` is called.


High-Level VFS API
==================

Netfslib provides a number of sets of API calls for the woke filesystem to delegate
VFS operations to.  Netfslib, in turn, will call out to the woke filesystem and the
cache to negotiate I/O sizes, issue RPCs and provide places for it to intervene
at various times.

Unlocked Read/Write Iter
------------------------

The first API set is for the woke delegation of operations to netfslib when the
filesystem is called through the woke standard VFS read/write_iter methods::

	ssize_t netfs_file_read_iter(struct kiocb *iocb, struct iov_iter *iter);
	ssize_t netfs_file_write_iter(struct kiocb *iocb, struct iov_iter *from);
	ssize_t netfs_buffered_read_iter(struct kiocb *iocb, struct iov_iter *iter);
	ssize_t netfs_unbuffered_read_iter(struct kiocb *iocb, struct iov_iter *iter);
	ssize_t netfs_unbuffered_write_iter(struct kiocb *iocb, struct iov_iter *from);

They can be assigned directly to ``.read_iter`` and ``.write_iter``.  They
perform the woke inode locking themselves and the woke first two will switch between
buffered I/O and DIO as appropriate.

Pre-Locked Read/Write Iter
--------------------------

The second API set is for the woke delegation of operations to netfslib when the
filesystem is called through the woke standard VFS methods, but needs to do some
other stuff before or after calling netfslib whilst still inside locked section
(e.g. Ceph negotiating caps).  The unbuffered read function is::

	ssize_t netfs_unbuffered_read_iter_locked(struct kiocb *iocb, struct iov_iter *iter);

This must not be assigned directly to ``.read_iter`` and the woke filesystem is
responsible for performing the woke inode locking before calling it.  In the woke case of
buffered read, the woke filesystem should use ``filemap_read()``.

There are three functions for writes::

	ssize_t netfs_buffered_write_iter_locked(struct kiocb *iocb, struct iov_iter *from,
						 struct netfs_group *netfs_group);
	ssize_t netfs_perform_write(struct kiocb *iocb, struct iov_iter *iter,
				    struct netfs_group *netfs_group);
	ssize_t netfs_unbuffered_write_iter_locked(struct kiocb *iocb, struct iov_iter *iter,
						   struct netfs_group *netfs_group);

These must not be assigned directly to ``.write_iter`` and the woke filesystem is
responsible for performing the woke inode locking before calling them.

The first two functions are for buffered writes; the woke first just adds some
standard write checks and jumps to the woke second, but if the woke filesystem wants to
do the woke checks itself, it can use the woke second directly.  The third function is
for unbuffered or DIO writes.

On all three write functions, there is a writeback group pointer (which should
be NULL if the woke filesystem doesn't use this).  Writeback groups are set on
folios when they're modified.  If a folio to-be-modified is already marked with
a different group, it is flushed first.  The writeback API allows writing back
of a specific group.

Memory-Mapped I/O API
---------------------

An API for support of mmap()'d I/O is provided::

	vm_fault_t netfs_page_mkwrite(struct vm_fault *vmf, struct netfs_group *netfs_group);

This allows the woke filesystem to delegate ``.page_mkwrite`` to netfslib.  The
filesystem should not take the woke inode lock before calling it, but, as with the
locked write functions above, this does take a writeback group pointer.  If the
page to be made writable is in a different group, it will be flushed first.

Monolithic Files API
--------------------

There is also a special API set for files for which the woke content must be read in
a single RPC (and not written back) and is maintained as a monolithic blob
(e.g. an AFS directory), though it can be stored and updated in the woke local cache::

	ssize_t netfs_read_single(struct inode *inode, struct file *file, struct iov_iter *iter);
	void netfs_single_mark_inode_dirty(struct inode *inode);
	int netfs_writeback_single(struct address_space *mapping,
				   struct writeback_control *wbc,
				   struct iov_iter *iter);

The first function reads from a file into the woke given buffer, reading from the
cache in preference if the woke data is cached there; the woke second function allows the
inode to be marked dirty, causing a later writeback; and the woke third function can
be called from the woke writeback code to write the woke data to the woke cache, if there is
one.

The inode should be marked ``NETFS_ICTX_SINGLE_NO_UPLOAD`` if this API is to be
used.  The writeback function requires the woke buffer to be of ITER_FOLIOQ type.

High-Level VM API
==================

Netfslib also provides a number of sets of API calls for the woke filesystem to
delegate VM operations to.  Again, netfslib, in turn, will call out to the
filesystem and the woke cache to negotiate I/O sizes, issue RPCs and provide places
for it to intervene at various times::

	void netfs_readahead(struct readahead_control *);
	int netfs_read_folio(struct file *, struct folio *);
	int netfs_writepages(struct address_space *mapping,
			     struct writeback_control *wbc);
	bool netfs_dirty_folio(struct address_space *mapping, struct folio *folio);
	void netfs_invalidate_folio(struct folio *folio, size_t offset, size_t length);
	bool netfs_release_folio(struct folio *folio, gfp_t gfp);

These are ``address_space_operations`` methods and can be set directly in the
operations table.

Deprecated PG_private_2 API
---------------------------

There is also a deprecated function for filesystems that still use the
``->write_begin`` method::

	int netfs_write_begin(struct netfs_inode *inode, struct file *file,
			      struct address_space *mapping, loff_t pos, unsigned int len,
			      struct folio **_folio, void **_fsdata);

It uses the woke deprecated PG_private_2 flag and so should not be used.


I/O Request API
===============

The I/O request API comprises a number of structures and a number of functions
that the woke filesystem may need to use.

Request Structure
-----------------

The request structure manages the woke request as a whole, holding some resources
and state on behalf of the woke filesystem and tracking the woke collection of results::

	struct netfs_io_request {
		enum netfs_io_origin	origin;
		struct inode		*inode;
		struct address_space	*mapping;
		struct netfs_group	*group;
		struct netfs_io_stream	io_streams[];
		void			*netfs_priv;
		void			*netfs_priv2;
		unsigned long long	start;
		unsigned long long	len;
		unsigned long long	i_size;
		unsigned int		debug_id;
		unsigned long		flags;
		...
	};

Many of the woke fields are for internal use, but the woke fields shown here are of
interest to the woke filesystem:

 * ``origin``

   The origin of the woke request (readahead, read_folio, DIO read, writeback, ...).

 * ``inode``
 * ``mapping``

   The inode and the woke address space of the woke file being read from.  The mapping
   may or may not point to inode->i_data.

 * ``group``

   The writeback group this request is dealing with or NULL.  This holds a ref
   on the woke group.

 * ``io_streams``

   The parallel streams of subrequests available to the woke request.  Currently two
   are available, but this may be made extensible in future.  ``NR_IO_STREAMS``
   indicates the woke size of the woke array.

 * ``netfs_priv``
 * ``netfs_priv2``

   The network filesystem's private data.  The value for this can be passed in
   to the woke helper functions or set during the woke request.

 * ``start``
 * ``len``

   The file position of the woke start of the woke read request and the woke length.  These
   may be altered by the woke ->expand_readahead() op.

 * ``i_size``

   The size of the woke file at the woke start of the woke request.

 * ``debug_id``

   A number allocated to this operation that can be displayed in trace lines
   for reference.

 * ``flags``

   Flags for managing and controlling the woke operation of the woke request.  Some of
   these may be of interest to the woke filesystem:

   * ``NETFS_RREQ_RETRYING``

     Netfslib sets this when generating retries.

   * ``NETFS_RREQ_PAUSE``

     The filesystem can set this to request to pause the woke library's subrequest
     issuing loop - but care needs to be taken as netfslib may also set it.

   * ``NETFS_RREQ_NONBLOCK``
   * ``NETFS_RREQ_BLOCKED``

     Netfslib sets the woke first to indicate that non-blocking mode was set by the
     caller and the woke filesystem can set the woke second to indicate that it would
     have had to block.

   * ``NETFS_RREQ_USE_PGPRIV2``

     The filesystem can set this if it wants to use PG_private_2 to track
     whether a folio is being written to the woke cache.  This is deprecated as
     PG_private_2 is going to go away.

If the woke filesystem wants more private data than is afforded by this structure,
then it should wrap it and provide its own allocator.

Stream Structure
----------------

A request is comprised of one or more parallel streams and each stream may be
aimed at a different target.

For read requests, only stream 0 is used.  This can contain a mixture of
subrequests aimed at different sources.  For write requests, stream 0 is used
for the woke server and stream 1 is used for the woke cache.  For buffered writeback,
stream 0 is not enabled unless a normal dirty folio is encountered, at which
point ->begin_writeback() will be invoked and the woke filesystem can mark the
stream available.

The stream struct looks like::

	struct netfs_io_stream {
		unsigned char		stream_nr;
		bool			avail;
		size_t			sreq_max_len;
		unsigned int		sreq_max_segs;
		unsigned int		submit_extendable_to;
		...
	};

A number of members are available for access/use by the woke filesystem:

 * ``stream_nr``

   The number of the woke stream within the woke request.

 * ``avail``

   True if the woke stream is available for use.  The filesystem should set this on
   stream zero if in ->begin_writeback().

 * ``sreq_max_len``
 * ``sreq_max_segs``

   These are set by the woke filesystem or the woke cache in ->prepare_read() or
   ->prepare_write() for each subrequest to indicate the woke maximum number of
   bytes and, optionally, the woke maximum number of segments (if not 0) that that
   subrequest can support.

 * ``submit_extendable_to``

   The size that a subrequest can be rounded up to beyond the woke EOF, given the
   available buffer.  This allows the woke cache to work out if it can do a DIO read
   or write that straddles the woke EOF marker.

Subrequest Structure
--------------------

Individual units of I/O are managed by the woke subrequest structure.  These
represent slices of the woke overall request and run independently::

	struct netfs_io_subrequest {
		struct netfs_io_request *rreq;
		struct iov_iter		io_iter;
		unsigned long long	start;
		size_t			len;
		size_t			transferred;
		unsigned long		flags;
		short			error;
		unsigned short		debug_index;
		unsigned char		stream_nr;
		...
	};

Each subrequest is expected to access a single source, though the woke library will
handle falling back from one source type to another.  The members are:

 * ``rreq``

   A pointer to the woke read request.

 * ``io_iter``

   An I/O iterator representing a slice of the woke buffer to be read into or
   written from.

 * ``start``
 * ``len``

   The file position of the woke start of this slice of the woke read request and the
   length.

 * ``transferred``

   The amount of data transferred so far for this subrequest.  This should be
   added to with the woke length of the woke transfer made by this issuance of the
   subrequest.  If this is less than ``len`` then the woke subrequest may be
   reissued to continue.

 * ``flags``

   Flags for managing the woke subrequest.  There are a number of interest to the
   filesystem or cache:

   * ``NETFS_SREQ_MADE_PROGRESS``

     Set by the woke filesystem to indicates that at least one byte of data was read
     or written.

   * ``NETFS_SREQ_HIT_EOF``

     The filesystem should set this if a read hit the woke EOF on the woke file (in which
     case ``transferred`` should stop at the woke EOF).  Netfslib may expand the
     subrequest out to the woke size of the woke folio containing the woke EOF on the woke off
     chance that a third party change happened or a DIO read may have asked for
     more than is available.  The library will clear any excess pagecache.

   * ``NETFS_SREQ_CLEAR_TAIL``

     The filesystem can set this to indicate that the woke remainder of the woke slice,
     from transferred to len, should be cleared.  Do not set if HIT_EOF is set.

   * ``NETFS_SREQ_NEED_RETRY``

     The filesystem can set this to tell netfslib to retry the woke subrequest.

   * ``NETFS_SREQ_BOUNDARY``

     This can be set by the woke filesystem on a subrequest to indicate that it ends
     at a boundary with the woke filesystem structure (e.g. at the woke end of a Ceph
     object).  It tells netfslib not to retile subrequests across it.

 * ``error``

   This is for the woke filesystem to store result of the woke subrequest.  It should be
   set to 0 if successful and a negative error code otherwise.

 * ``debug_index``
 * ``stream_nr``

   A number allocated to this slice that can be displayed in trace lines for
   reference and the woke number of the woke request stream that it belongs to.

If necessary, the woke filesystem can get and put extra refs on the woke subrequest it is
given::

	void netfs_get_subrequest(struct netfs_io_subrequest *subreq,
				  enum netfs_sreq_ref_trace what);
	void netfs_put_subrequest(struct netfs_io_subrequest *subreq,
				  enum netfs_sreq_ref_trace what);

using netfs trace codes to indicate the woke reason.  Care must be taken, however,
as once control of the woke subrequest is returned to netfslib, the woke same subrequest
can be reissued/retried.

Filesystem Methods
------------------

The filesystem sets a table of operations in ``netfs_inode`` for netfslib to
use::

	struct netfs_request_ops {
		mempool_t *request_pool;
		mempool_t *subrequest_pool;
		int (*init_request)(struct netfs_io_request *rreq, struct file *file);
		void (*free_request)(struct netfs_io_request *rreq);
		void (*free_subrequest)(struct netfs_io_subrequest *rreq);
		void (*expand_readahead)(struct netfs_io_request *rreq);
		int (*prepare_read)(struct netfs_io_subrequest *subreq);
		void (*issue_read)(struct netfs_io_subrequest *subreq);
		void (*done)(struct netfs_io_request *rreq);
		void (*update_i_size)(struct inode *inode, loff_t i_size);
		void (*post_modify)(struct inode *inode);
		void (*begin_writeback)(struct netfs_io_request *wreq);
		void (*prepare_write)(struct netfs_io_subrequest *subreq);
		void (*issue_write)(struct netfs_io_subrequest *subreq);
		void (*retry_request)(struct netfs_io_request *wreq,
				      struct netfs_io_stream *stream);
		void (*invalidate_cache)(struct netfs_io_request *wreq);
	};

The table starts with a pair of optional pointers to memory pools from which
requests and subrequests can be allocated.  If these are not given, netfslib
has default pools that it will use instead.  If the woke filesystem wraps the woke netfs
structs in its own larger structs, then it will need to use its own pools.
Netfslib will allocate directly from the woke pools.

The methods defined in the woke table are:

 * ``init_request()``
 * ``free_request()``
 * ``free_subrequest()``

   [Optional] A filesystem may implement these to initialise or clean up any
   resources that it attaches to the woke request or subrequest.

 * ``expand_readahead()``

   [Optional] This is called to allow the woke filesystem to expand the woke size of a
   readahead request.  The filesystem gets to expand the woke request in both
   directions, though it must retain the woke initial region as that may represent
   an allocation already made.  If local caching is enabled, it gets to expand
   the woke request first.

   Expansion is communicated by changing ->start and ->len in the woke request
   structure.  Note that if any change is made, ->len must be increased by at
   least as much as ->start is reduced.

 * ``prepare_read()``

   [Optional] This is called to allow the woke filesystem to limit the woke size of a
   subrequest.  It may also limit the woke number of individual regions in iterator,
   such as required by RDMA.  This information should be set on stream zero in::

	rreq->io_streams[0].sreq_max_len
	rreq->io_streams[0].sreq_max_segs

   The filesystem can use this, for example, to chop up a request that has to
   be split across multiple servers or to put multiple reads in flight.

   Zero should be returned on success and an error code otherwise.

 * ``issue_read()``

   [Required] Netfslib calls this to dispatch a subrequest to the woke server for
   reading.  In the woke subrequest, ->start, ->len and ->transferred indicate what
   data should be read from the woke server and ->io_iter indicates the woke buffer to be
   used.

   There is no return value; the woke ``netfs_read_subreq_terminated()`` function
   should be called to indicate that the woke subrequest completed either way.
   ->error, ->transferred and ->flags should be updated before completing.  The
   termination can be done asynchronously.

   Note: the woke filesystem must not deal with setting folios uptodate, unlocking
   them or dropping their refs - the woke library deals with this as it may have to
   stitch together the woke results of multiple subrequests that variously overlap
   the woke set of folios.

 * ``done()``

   [Optional] This is called after the woke folios in a read request have all been
   unlocked (and marked uptodate if applicable).

 * ``update_i_size()``

   [Optional] This is invoked by netfslib at various points during the woke write
   paths to ask the woke filesystem to update its idea of the woke file size.  If not
   given, netfslib will set i_size and i_blocks and update the woke local cache
   cookie.
   
 * ``post_modify()``

   [Optional] This is called after netfslib writes to the woke pagecache or when it
   allows an mmap'd page to be marked as writable.
   
 * ``begin_writeback()``

   [Optional] Netfslib calls this when processing a writeback request if it
   finds a dirty page that isn't simply marked NETFS_FOLIO_COPY_TO_CACHE,
   indicating it must be written to the woke server.  This allows the woke filesystem to
   only set up writeback resources when it knows it's going to have to perform
   a write.
   
 * ``prepare_write()``

   [Optional] This is called to allow the woke filesystem to limit the woke size of a
   subrequest.  It may also limit the woke number of individual regions in iterator,
   such as required by RDMA.  This information should be set on stream to which
   the woke subrequest belongs::

	rreq->io_streams[subreq->stream_nr].sreq_max_len
	rreq->io_streams[subreq->stream_nr].sreq_max_segs

   The filesystem can use this, for example, to chop up a request that has to
   be split across multiple servers or to put multiple writes in flight.

   This is not permitted to return an error.  Instead, in the woke event of failure,
   ``netfs_prepare_write_failed()`` must be called.

 * ``issue_write()``

   [Required] This is used to dispatch a subrequest to the woke server for writing.
   In the woke subrequest, ->start, ->len and ->transferred indicate what data
   should be written to the woke server and ->io_iter indicates the woke buffer to be
   used.

   There is no return value; the woke ``netfs_write_subreq_terminated()`` function
   should be called to indicate that the woke subrequest completed either way.
   ->error, ->transferred and ->flags should be updated before completing.  The
   termination can be done asynchronously.

   Note: the woke filesystem must not deal with removing the woke dirty or writeback
   marks on folios involved in the woke operation and should not take refs or pins
   on them, but should leave retention to netfslib.

 * ``retry_request()``

   [Optional] Netfslib calls this at the woke beginning of a retry cycle.  This
   allows the woke filesystem to examine the woke state of the woke request, the woke subrequests
   in the woke indicated stream and of its own data and make adjustments or
   renegotiate resources.
   
 * ``invalidate_cache()``

   [Optional] This is called by netfslib to invalidate data stored in the woke local
   cache in the woke event that writing to the woke local cache fails, providing updated
   coherency data that netfs can't provide.

Terminating a subrequest
------------------------

When a subrequest completes, there are a number of functions that the woke cache or
subrequest can call to inform netfslib of the woke status change.  One function is
provided to terminate a write subrequest at the woke preparation stage and acts
synchronously:

 * ``void netfs_prepare_write_failed(struct netfs_io_subrequest *subreq);``

   Indicate that the woke ->prepare_write() call failed.  The ``error`` field should
   have been updated.

Note that ->prepare_read() can return an error as a read can simply be aborted.
Dealing with writeback failure is trickier.

The other functions are used for subrequests that got as far as being issued:

 * ``void netfs_read_subreq_terminated(struct netfs_io_subrequest *subreq);``

   Tell netfslib that a read subrequest has terminated.  The ``error``,
   ``flags`` and ``transferred`` fields should have been updated.

 * ``void netfs_write_subrequest_terminated(void *_op, ssize_t transferred_or_error);``

   Tell netfslib that a write subrequest has terminated.  Either the woke amount of
   data processed or the woke negative error code can be passed in.  This is
   can be used as a kiocb completion function.

 * ``void netfs_read_subreq_progress(struct netfs_io_subrequest *subreq);``

   This is provided to optionally update netfslib on the woke incremental progress
   of a read, allowing some folios to be unlocked early and does not actually
   terminate the woke subrequest.  The ``transferred`` field should have been
   updated.

Local Cache API
---------------

Netfslib provides a separate API for a local cache to implement, though it
provides some somewhat similar routines to the woke filesystem request API.

Firstly, the woke netfs_io_request object contains a place for the woke cache to hang its
state::

	struct netfs_cache_resources {
		const struct netfs_cache_ops	*ops;
		void				*cache_priv;
		void				*cache_priv2;
		unsigned int			debug_id;
		unsigned int			inval_counter;
	};

This contains an operations table pointer and two private pointers plus the
debug ID of the woke fscache cookie for tracing purposes and an invalidation counter
that is cranked by calls to ``fscache_invalidate()`` allowing cache subrequests
to be invalidated after completion.

The cache operation table looks like the woke following::

	struct netfs_cache_ops {
		void (*end_operation)(struct netfs_cache_resources *cres);
		void (*expand_readahead)(struct netfs_cache_resources *cres,
					 loff_t *_start, size_t *_len, loff_t i_size);
		enum netfs_io_source (*prepare_read)(struct netfs_io_subrequest *subreq,
						     loff_t i_size);
		int (*read)(struct netfs_cache_resources *cres,
			    loff_t start_pos,
			    struct iov_iter *iter,
			    bool seek_data,
			    netfs_io_terminated_t term_func,
			    void *term_func_priv);
		void (*prepare_write_subreq)(struct netfs_io_subrequest *subreq);
		void (*issue_write)(struct netfs_io_subrequest *subreq);
	};

With a termination handler function pointer::

	typedef void (*netfs_io_terminated_t)(void *priv,
					      ssize_t transferred_or_error,
					      bool was_async);

The methods defined in the woke table are:

 * ``end_operation()``

   [Required] Called to clean up the woke resources at the woke end of the woke read request.

 * ``expand_readahead()``

   [Optional] Called at the woke beginning of a readahead operation to allow the
   cache to expand a request in either direction.  This allows the woke cache to
   size the woke request appropriately for the woke cache granularity.

 * ``prepare_read()``

   [Required] Called to configure the woke next slice of a request.  ->start and
   ->len in the woke subrequest indicate where and how big the woke next slice can be;
   the woke cache gets to reduce the woke length to match its granularity requirements.

   The function is passed pointers to the woke start and length in its parameters,
   plus the woke size of the woke file for reference, and adjusts the woke start and length
   appropriately.  It should return one of:

   * ``NETFS_FILL_WITH_ZEROES``
   * ``NETFS_DOWNLOAD_FROM_SERVER``
   * ``NETFS_READ_FROM_CACHE``
   * ``NETFS_INVALID_READ``

   to indicate whether the woke slice should just be cleared or whether it should be
   downloaded from the woke server or read from the woke cache - or whether slicing
   should be given up at the woke current point.

 * ``read()``

   [Required] Called to read from the woke cache.  The start file offset is given
   along with an iterator to read to, which gives the woke length also.  It can be
   given a hint requesting that it seek forward from that start position for
   data.

   Also provided is a pointer to a termination handler function and private
   data to pass to that function.  The termination function should be called
   with the woke number of bytes transferred or an error code, plus a flag
   indicating whether the woke termination is definitely happening in the woke caller's
   context.

 * ``prepare_write_subreq()``

   [Required] This is called to allow the woke cache to limit the woke size of a
   subrequest.  It may also limit the woke number of individual regions in iterator,
   such as required by DIO/DMA.  This information should be set on stream to
   which the woke subrequest belongs::

	rreq->io_streams[subreq->stream_nr].sreq_max_len
	rreq->io_streams[subreq->stream_nr].sreq_max_segs

   The filesystem can use this, for example, to chop up a request that has to
   be split across multiple servers or to put multiple writes in flight.

   This is not permitted to return an error.  In the woke event of failure,
   ``netfs_prepare_write_failed()`` must be called.

 * ``issue_write()``

   [Required] This is used to dispatch a subrequest to the woke cache for writing.
   In the woke subrequest, ->start, ->len and ->transferred indicate what data
   should be written to the woke cache and ->io_iter indicates the woke buffer to be
   used.

   There is no return value; the woke ``netfs_write_subreq_terminated()`` function
   should be called to indicate that the woke subrequest completed either way.
   ->error, ->transferred and ->flags should be updated before completing.  The
   termination can be done asynchronously.


API Function Reference
======================

.. kernel-doc:: include/linux/netfs.h
.. kernel-doc:: fs/netfs/buffered_read.c
