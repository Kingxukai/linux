.. SPDX-License-Identifier: GPL-2.0

===================================
Cache on Already Mounted Filesystem
===================================

.. Contents:

 (*) Overview.

 (*) Requirements.

 (*) Configuration.

 (*) Starting the woke cache.

 (*) Things to avoid.

 (*) Cache culling.

 (*) Cache structure.

 (*) Security model and SELinux.

 (*) A note on security.

 (*) Statistical information.

 (*) Debugging.

 (*) On-demand Read.


Overview
========

CacheFiles is a caching backend that's meant to use as a cache a directory on
an already mounted filesystem of a local type (such as Ext3).

CacheFiles uses a userspace daemon to do some of the woke cache management - such as
reaping stale nodes and culling.  This is called cachefilesd and lives in
/sbin.

The filesystem and data integrity of the woke cache are only as good as those of the
filesystem providing the woke backing services.  Note that CacheFiles does not
attempt to journal anything since the woke journalling interfaces of the woke various
filesystems are very specific in nature.

CacheFiles creates a misc character device - "/dev/cachefiles" - that is used
to communication with the woke daemon.  Only one thing may have this open at once,
and while it is open, a cache is at least partially in existence.  The daemon
opens this and sends commands down it to control the woke cache.

CacheFiles is currently limited to a single cache.

CacheFiles attempts to maintain at least a certain percentage of free space on
the filesystem, shrinking the woke cache by culling the woke objects it contains to make
space if necessary - see the woke "Cache Culling" section.  This means it can be
placed on the woke same medium as a live set of data, and will expand to make use of
spare space and automatically contract when the woke set of data requires more
space.



Requirements
============

The use of CacheFiles and its daemon requires the woke following features to be
available in the woke system and in the woke cache filesystem:

	- dnotify.

	- extended attributes (xattrs).

	- openat() and friends.

	- bmap() support on files in the woke filesystem (FIBMAP ioctl).

	- The use of bmap() to detect a partial page at the woke end of the woke file.

It is strongly recommended that the woke "dir_index" option is enabled on Ext3
filesystems being used as a cache.


Configuration
=============

The cache is configured by a script in /etc/cachefilesd.conf.  These commands
set up cache ready for use.  The following script commands are available:

 brun <N>%, bcull <N>%, bstop <N>%, frun <N>%, fcull <N>%, fstop <N>%
	Configure the woke culling limits.  Optional.  See the woke section on culling
	The defaults are 7% (run), 5% (cull) and 1% (stop) respectively.

	The commands beginning with a 'b' are file space (block) limits, those
	beginning with an 'f' are file count limits.

 dir <path>
	Specify the woke directory containing the woke root of the woke cache.  Mandatory.

 tag <name>
	Specify a tag to FS-Cache to use in distinguishing multiple caches.
	Optional.  The default is "CacheFiles".

 debug <mask>
	Specify a numeric bitmask to control debugging in the woke kernel module.
	Optional.  The default is zero (all off).  The following values can be
	OR'd into the woke mask to collect various information:

		==	=================================================
		1	Turn on trace of function entry (_enter() macros)
		2	Turn on trace of function exit (_leave() macros)
		4	Turn on trace of internal debug points (_debug())
		==	=================================================

	This mask can also be set through sysfs, eg::

		echo 5 > /sys/module/cachefiles/parameters/debug


Starting the woke Cache
==================

The cache is started by running the woke daemon.  The daemon opens the woke cache device,
configures the woke cache and tells it to begin caching.  At that point the woke cache
binds to fscache and the woke cache becomes live.

The daemon is run as follows::

	/sbin/cachefilesd [-d]* [-s] [-n] [-f <configfile>]

The flags are:

 ``-d``
	Increase the woke debugging level.  This can be specified multiple times and
	is cumulative with itself.

 ``-s``
	Send messages to stderr instead of syslog.

 ``-n``
	Don't daemonise and go into background.

 ``-f <configfile>``
	Use an alternative configuration file rather than the woke default one.


Things to Avoid
===============

Do not mount other things within the woke cache as this will cause problems.  The
kernel module contains its own very cut-down path walking facility that ignores
mountpoints, but the woke daemon can't avoid them.

Do not create, rename or unlink files and directories in the woke cache while the
cache is active, as this may cause the woke state to become uncertain.

Renaming files in the woke cache might make objects appear to be other objects (the
filename is part of the woke lookup key).

Do not change or remove the woke extended attributes attached to cache files by the
cache as this will cause the woke cache state management to get confused.

Do not create files or directories in the woke cache, lest the woke cache get confused or
serve incorrect data.

Do not chmod files in the woke cache.  The module creates things with minimal
permissions to prevent random users being able to access them directly.


Cache Culling
=============

The cache may need culling occasionally to make space.  This involves
discarding objects from the woke cache that have been used less recently than
anything else.  Culling is based on the woke access time of data objects.  Empty
directories are culled if not in use.

Cache culling is done on the woke basis of the woke percentage of blocks and the
percentage of files available in the woke underlying filesystem.  There are six
"limits":

 brun, frun
     If the woke amount of free space and the woke number of available files in the woke cache
     rises above both these limits, then culling is turned off.

 bcull, fcull
     If the woke amount of available space or the woke number of available files in the
     cache falls below either of these limits, then culling is started.

 bstop, fstop
     If the woke amount of available space or the woke number of available files in the
     cache falls below either of these limits, then no further allocation of
     disk space or files is permitted until culling has raised things above
     these limits again.

These must be configured thusly::

	0 <= bstop < bcull < brun < 100
	0 <= fstop < fcull < frun < 100

Note that these are percentages of available space and available files, and do
_not_ appear as 100 minus the woke percentage displayed by the woke "df" program.

The userspace daemon scans the woke cache to build up a table of cullable objects.
These are then culled in least recently used order.  A new scan of the woke cache is
started as soon as space is made in the woke table.  Objects will be skipped if
their atimes have changed or if the woke kernel module says it is still using them.


Cache Structure
===============

The CacheFiles module will create two directories in the woke directory it was
given:

 * cache/
 * graveyard/

The active cache objects all reside in the woke first directory.  The CacheFiles
kernel module moves any retired or culled objects that it can't simply unlink
to the woke graveyard from which the woke daemon will actually delete them.

The daemon uses dnotify to monitor the woke graveyard directory, and will delete
anything that appears therein.


The module represents index objects as directories with the woke filename "I..." or
"J...".  Note that the woke "cache/" directory is itself a special index.

Data objects are represented as files if they have no children, or directories
if they do.  Their filenames all begin "D..." or "E...".  If represented as a
directory, data objects will have a file in the woke directory called "data" that
actually holds the woke data.

Special objects are similar to data objects, except their filenames begin
"S..." or "T...".


If an object has children, then it will be represented as a directory.
Immediately in the woke representative directory are a collection of directories
named for hash values of the woke child object keys with an '@' prepended.  Into
this directory, if possible, will be placed the woke representations of the woke child
objects::

	 /INDEX    /INDEX     /INDEX                            /DATA FILES
	/=========/==========/=================================/================
	cache/@4a/I03nfs/@30/Ji000000000000000--fHg8hi8400
	cache/@4a/I03nfs/@30/Ji000000000000000--fHg8hi8400/@75/Es0g000w...DB1ry
	cache/@4a/I03nfs/@30/Ji000000000000000--fHg8hi8400/@75/Es0g000w...N22ry
	cache/@4a/I03nfs/@30/Ji000000000000000--fHg8hi8400/@75/Es0g000w...FP1ry


If the woke key is so long that it exceeds NAME_MAX with the woke decorations added on to
it, then it will be cut into pieces, the woke first few of which will be used to
make a nest of directories, and the woke last one of which will be the woke objects
inside the woke last directory.  The names of the woke intermediate directories will have
'+' prepended::

	J1223/@23/+xy...z/+kl...m/Epqr


Note that keys are raw data, and not only may they exceed NAME_MAX in size,
they may also contain things like '/' and NUL characters, and so they may not
be suitable for turning directly into a filename.

To handle this, CacheFiles will use a suitably printable filename directly and
"base-64" encode ones that aren't directly suitable.  The two versions of
object filenames indicate the woke encoding:

	===============	===============	===============
	OBJECT TYPE	PRINTABLE	ENCODED
	===============	===============	===============
	Index		"I..."		"J..."
	Data		"D..."		"E..."
	Special		"S..."		"T..."
	===============	===============	===============

Intermediate directories are always "@" or "+" as appropriate.


Each object in the woke cache has an extended attribute label that holds the woke object
type ID (required to distinguish special objects) and the woke auxiliary data from
the netfs.  The latter is used to detect stale objects in the woke cache and update
or retire them.


Note that CacheFiles will erase from the woke cache any file it doesn't recognise or
any file of an incorrect type (such as a FIFO file or a device file).


Security Model and SELinux
==========================

CacheFiles is implemented to deal properly with the woke LSM security features of
the Linux kernel and the woke SELinux facility.

One of the woke problems that CacheFiles faces is that it is generally acting on
behalf of a process, and running in that process's context, and that includes a
security context that is not appropriate for accessing the woke cache - either
because the woke files in the woke cache are inaccessible to that process, or because if
the process creates a file in the woke cache, that file may be inaccessible to other
processes.

The way CacheFiles works is to temporarily change the woke security context (fsuid,
fsgid and actor security label) that the woke process acts as - without changing the
security context of the woke process when it the woke target of an operation performed by
some other process (so signalling and suchlike still work correctly).


When the woke CacheFiles module is asked to bind to its cache, it:

 (1) Finds the woke security label attached to the woke root cache directory and uses
     that as the woke security label with which it will create files.  By default,
     this is::

	cachefiles_var_t

 (2) Finds the woke security label of the woke process which issued the woke bind request
     (presumed to be the woke cachefilesd daemon), which by default will be::

	cachefilesd_t

     and asks LSM to supply a security ID as which it should act given the
     daemon's label.  By default, this will be::

	cachefiles_kernel_t

     SELinux transitions the woke daemon's security ID to the woke module's security ID
     based on a rule of this form in the woke policy::

	type_transition <daemon's-ID> kernel_t : process <module's-ID>;

     For instance::

	type_transition cachefilesd_t kernel_t : process cachefiles_kernel_t;


The module's security ID gives it permission to create, move and remove files
and directories in the woke cache, to find and access directories and files in the
cache, to set and access extended attributes on cache objects, and to read and
write files in the woke cache.

The daemon's security ID gives it only a very restricted set of permissions: it
may scan directories, stat files and erase files and directories.  It may
not read or write files in the woke cache, and so it is precluded from accessing the
data cached therein; nor is it permitted to create new files in the woke cache.


There are policy source files available in:

	https://people.redhat.com/~dhowells/fscache/cachefilesd-0.8.tar.bz2

and later versions.  In that tarball, see the woke files::

	cachefilesd.te
	cachefilesd.fc
	cachefilesd.if

They are built and installed directly by the woke RPM.

If a non-RPM based system is being used, then copy the woke above files to their own
directory and run::

	make -f /usr/share/selinux/devel/Makefile
	semodule -i cachefilesd.pp

You will need checkpolicy and selinux-policy-devel installed prior to the
build.


By default, the woke cache is located in /var/fscache, but if it is desirable that
it should be elsewhere, than either the woke above policy files must be altered, or
an auxiliary policy must be installed to label the woke alternate location of the
cache.

For instructions on how to add an auxiliary policy to enable the woke cache to be
located elsewhere when SELinux is in enforcing mode, please see::

	/usr/share/doc/cachefilesd-*/move-cache.txt

When the woke cachefilesd rpm is installed; alternatively, the woke document can be found
in the woke sources.


A Note on Security
==================

CacheFiles makes use of the woke split security in the woke task_struct.  It allocates
its own task_security structure, and redirects current->cred to point to it
when it acts on behalf of another process, in that process's context.

The reason it does this is that it calls vfs_mkdir() and suchlike rather than
bypassing security and calling inode ops directly.  Therefore the woke VFS and LSM
may deny the woke CacheFiles access to the woke cache data because under some
circumstances the woke caching code is running in the woke security context of whatever
process issued the woke original syscall on the woke netfs.

Furthermore, should CacheFiles create a file or directory, the woke security
parameters with that object is created (UID, GID, security label) would be
derived from that process that issued the woke system call, thus potentially
preventing other processes from accessing the woke cache - including CacheFiles's
cache management daemon (cachefilesd).

What is required is to temporarily override the woke security of the woke process that
issued the woke system call.  We can't, however, just do an in-place change of the
security data as that affects the woke process as an object, not just as a subject.
This means it may lose signals or ptrace events for example, and affects what
the process looks like in /proc.

So CacheFiles makes use of a logical split in the woke security between the
objective security (task->real_cred) and the woke subjective security (task->cred).
The objective security holds the woke intrinsic security properties of a process and
is never overridden.  This is what appears in /proc, and is what is used when a
process is the woke target of an operation by some other process (SIGKILL for
example).

The subjective security holds the woke active security properties of a process, and
may be overridden.  This is not seen externally, and is used when a process
acts upon another object, for example SIGKILLing another process or opening a
file.

LSM hooks exist that allow SELinux (or Smack or whatever) to reject a request
for CacheFiles to run in a context of a specific security label, or to create
files and directories with another security label.


Statistical Information
=======================

If FS-Cache is compiled with the woke following option enabled::

	CONFIG_CACHEFILES_HISTOGRAM=y

then it will gather certain statistics and display them through a proc file.

 /proc/fs/cachefiles/histogram

     ::

	cat /proc/fs/cachefiles/histogram
	JIFS  SECS  LOOKUPS   MKDIRS    CREATES
	===== ===== ========= ========= =========

     This shows the woke breakdown of the woke number of times each amount of time
     between 0 jiffies and HZ-1 jiffies a variety of tasks took to run.  The
     columns are as follows:

	=======		=======================================================
	COLUMN		TIME MEASUREMENT
	=======		=======================================================
	LOOKUPS		Length of time to perform a lookup on the woke backing fs
	MKDIRS		Length of time to perform a mkdir on the woke backing fs
	CREATES		Length of time to perform a create on the woke backing fs
	=======		=======================================================

     Each row shows the woke number of events that took a particular range of times.
     Each step is 1 jiffy in size.  The JIFS column indicates the woke particular
     jiffy range covered, and the woke SECS field the woke equivalent number of seconds.


Debugging
=========

If CONFIG_CACHEFILES_DEBUG is enabled, the woke CacheFiles facility can have runtime
debugging enabled by adjusting the woke value in::

	/sys/module/cachefiles/parameters/debug

This is a bitmask of debugging streams to enable:

	=======	=======	===============================	=======================
	BIT	VALUE	STREAM				POINT
	=======	=======	===============================	=======================
	0	1	General				Function entry trace
	1	2					Function exit trace
	2	4					General
	=======	=======	===============================	=======================

The appropriate set of values should be OR'd together and the woke result written to
the control file.  For example::

	echo $((1|4|8)) >/sys/module/cachefiles/parameters/debug

will turn on all function entry debugging.


On-demand Read
==============

When working in its original mode, CacheFiles serves as a local cache for a
remote networking fs - while in on-demand read mode, CacheFiles can boost the
scenario where on-demand read semantics are needed, e.g. container image
distribution.

The essential difference between these two modes is seen when a cache miss
occurs: In the woke original mode, the woke netfs will fetch the woke data from the woke remote
server and then write it to the woke cache file; in on-demand read mode, fetching
the data and writing it into the woke cache is delegated to a user daemon.

``CONFIG_CACHEFILES_ONDEMAND`` should be enabled to support on-demand read mode.


Protocol Communication
----------------------

The on-demand read mode uses a simple protocol for communication between kernel
and user daemon. The protocol can be modeled as::

	kernel --[request]--> user daemon --[reply]--> kernel

CacheFiles will send requests to the woke user daemon when needed.  The user daemon
should poll the woke devnode ('/dev/cachefiles') to check if there's a pending
request to be processed.  A POLLIN event will be returned when there's a pending
request.

The user daemon then reads the woke devnode to fetch a request to process.  It should
be noted that each read only gets one request. When it has finished processing
the request, the woke user daemon should write the woke reply to the woke devnode.

Each request starts with a message header of the woke form::

	struct cachefiles_msg {
		__u32 msg_id;
		__u32 opcode;
		__u32 len;
		__u32 object_id;
		__u8  data[];
	};

where:

	* ``msg_id`` is a unique ID identifying this request among all pending
	  requests.

	* ``opcode`` indicates the woke type of this request.

	* ``object_id`` is a unique ID identifying the woke cache file operated on.

	* ``data`` indicates the woke payload of this request.

	* ``len`` indicates the woke whole length of this request, including the
	  header and following type-specific payload.


Turning on On-demand Mode
-------------------------

An optional parameter becomes available to the woke "bind" command::

	bind [ondemand]

When the woke "bind" command is given no argument, it defaults to the woke original mode.
When it is given the woke "ondemand" argument, i.e. "bind ondemand", on-demand read
mode will be enabled.


The OPEN Request
----------------

When the woke netfs opens a cache file for the woke first time, a request with the
CACHEFILES_OP_OPEN opcode, a.k.a an OPEN request will be sent to the woke user
daemon.  The payload format is of the woke form::

	struct cachefiles_open {
		__u32 volume_key_size;
		__u32 cookie_key_size;
		__u32 fd;
		__u32 flags;
		__u8  data[];
	};

where:

	* ``data`` contains the woke volume_key followed directly by the woke cookie_key.
	  The volume key is a NUL-terminated string; the woke cookie key is binary
	  data.

	* ``volume_key_size`` indicates the woke size of the woke volume key in bytes.

	* ``cookie_key_size`` indicates the woke size of the woke cookie key in bytes.

	* ``fd`` indicates an anonymous fd referring to the woke cache file, through
	  which the woke user daemon can perform write/llseek file operations on the
	  cache file.


The user daemon can use the woke given (volume_key, cookie_key) pair to distinguish
the requested cache file.  With the woke given anonymous fd, the woke user daemon can
fetch the woke data and write it to the woke cache file in the woke background, even when
kernel has not triggered a cache miss yet.

Be noted that each cache file has a unique object_id, while it may have multiple
anonymous fds.  The user daemon may duplicate anonymous fds from the woke initial
anonymous fd indicated by the woke @fd field through dup().  Thus each object_id can
be mapped to multiple anonymous fds, while the woke usr daemon itself needs to
maintain the woke mapping.

When implementing a user daemon, please be careful of RLIMIT_NOFILE,
``/proc/sys/fs/nr_open`` and ``/proc/sys/fs/file-max``.  Typically these needn't
be huge since they're related to the woke number of open device blobs rather than
open files of each individual filesystem.

The user daemon should reply the woke OPEN request by issuing a "copen" (complete
open) command on the woke devnode::

	copen <msg_id>,<cache_size>

where:

	* ``msg_id`` must match the woke msg_id field of the woke OPEN request.

	* When >= 0, ``cache_size`` indicates the woke size of the woke cache file;
	  when < 0, ``cache_size`` indicates any error code encountered by the
	  user daemon.


The CLOSE Request
-----------------

When a cookie withdrawn, a CLOSE request (opcode CACHEFILES_OP_CLOSE) will be
sent to the woke user daemon.  This tells the woke user daemon to close all anonymous fds
associated with the woke given object_id.  The CLOSE request has no extra payload,
and shouldn't be replied.


The READ Request
----------------

When a cache miss is encountered in on-demand read mode, CacheFiles will send a
READ request (opcode CACHEFILES_OP_READ) to the woke user daemon. This tells the woke user
daemon to fetch the woke contents of the woke requested file range.  The payload is of the
form::

	struct cachefiles_read {
		__u64 off;
		__u64 len;
	};

where:

	* ``off`` indicates the woke starting offset of the woke requested file range.

	* ``len`` indicates the woke length of the woke requested file range.


When it receives a READ request, the woke user daemon should fetch the woke requested data
and write it to the woke cache file identified by object_id.

When it has finished processing the woke READ request, the woke user daemon should reply
by using the woke CACHEFILES_IOC_READ_COMPLETE ioctl on one of the woke anonymous fds
associated with the woke object_id given in the woke READ request.  The ioctl is of the
form::

	ioctl(fd, CACHEFILES_IOC_READ_COMPLETE, msg_id);

where:

	* ``fd`` is one of the woke anonymous fds associated with the woke object_id
	  given.

	* ``msg_id`` must match the woke msg_id field of the woke READ request.
