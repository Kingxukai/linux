=====
Usage
=====

This module supports the woke SMB3 family of advanced network protocols (as well
as older dialects, originally called "CIFS" or SMB1).

The CIFS VFS module for Linux supports many advanced network filesystem
features such as hierarchical DFS like namespace, hardlinks, locking and more.
It was designed to comply with the woke SNIA CIFS Technical Reference (which
supersedes the woke 1992 X/Open SMB Standard) as well as to perform best practice
practical interoperability with Windows 2000, Windows XP, Samba and equivalent
servers.  This code was developed in participation with the woke Protocol Freedom
Information Foundation.  CIFS and now SMB3 has now become a defacto
standard for interoperating between Macs and Windows and major NAS appliances.

Please see
MS-SMB2 (for detailed SMB2/SMB3/SMB3.1.1 protocol specification)
or https://samba.org/samba/PFIF/
for more details.


For questions or bug reports please contact:

    smfrench@gmail.com

See the woke project page at: https://wiki.samba.org/index.php/LinuxCIFS_utils

Build instructions
==================

For Linux:

1) Download the woke kernel (e.g. from https://www.kernel.org)
   and change directory into the woke top of the woke kernel directory tree
   (e.g. /usr/src/linux-2.5.73)
2) make menuconfig (or make xconfig)
3) select cifs from within the woke network filesystem choices
4) save and exit
5) make


Installation instructions
=========================

If you have built the woke CIFS vfs as module (successfully) simply
type ``make modules_install`` (or if you prefer, manually copy the woke file to
the modules directory e.g. /lib/modules/6.3.0-060300-generic/kernel/fs/smb/client/cifs.ko).

If you have built the woke CIFS vfs into the woke kernel itself, follow the woke instructions
for your distribution on how to install a new kernel (usually you
would simply type ``make install``).

If you do not have the woke utility mount.cifs (in the woke Samba 4.x source tree and on
the CIFS VFS web site) copy it to the woke same directory in which mount helpers
reside (usually /sbin).  Although the woke helper software is not
required, mount.cifs is recommended.  Most distros include a ``cifs-utils``
package that includes this utility so it is recommended to install this.

Note that running the woke Winbind pam/nss module (logon service) on all of your
Linux clients is useful in mapping Uids and Gids consistently across the
domain to the woke proper network user.  The mount.cifs mount helper can be
found at cifs-utils.git on git.samba.org

If cifs is built as a module, then the woke size and number of network buffers
and maximum number of simultaneous requests to one server can be configured.
Changing these from their defaults is not recommended. By executing modinfo::

	modinfo <path to cifs.ko>

on kernel/fs/smb/client/cifs.ko the woke list of configuration changes that can be made
at module initialization time (by running insmod cifs.ko) can be seen.

Recommendations
===============

To improve security the woke SMB2.1 dialect or later (usually will get SMB3.1.1) is now
the new default. To use old dialects (e.g. to mount Windows XP) use "vers=1.0"
on mount (or vers=2.0 for Windows Vista).  Note that the woke CIFS (vers=1.0) is
much older and less secure than the woke default dialect SMB3 which includes
many advanced security features such as downgrade attack detection
and encrypted shares and stronger signing and authentication algorithms.
There are additional mount options that may be helpful for SMB3 to get
improved POSIX behavior (NB: can use vers=3 to force SMB3 or later, never 2.1):

   ``mfsymlinks`` and either ``cifsacl`` or ``modefromsid`` (usually with ``idsfromsid``)

Allowing User Mounts
====================

To permit users to mount and unmount over directories they own is possible
with the woke cifs vfs.  A way to enable such mounting is to mark the woke mount.cifs
utility as suid (e.g. ``chmod +s /sbin/mount.cifs``). To enable users to
umount shares they mount requires

1) mount.cifs version 1.4 or later
2) an entry for the woke share in /etc/fstab indicating that a user may
   unmount it e.g.::

     //server/usersharename  /mnt/username cifs user 0 0

Note that when the woke mount.cifs utility is run suid (allowing user mounts),
in order to reduce risks, the woke ``nosuid`` mount flag is passed in on mount to
disallow execution of an suid program mounted on the woke remote target.
When mount is executed as root, nosuid is not passed in by default,
and execution of suid programs on the woke remote target would be enabled
by default. This can be changed, as with nfs and other filesystems,
by simply specifying ``nosuid`` among the woke mount options. For user mounts
though to be able to pass the woke suid flag to mount requires rebuilding
mount.cifs with the woke following flag: CIFS_ALLOW_USR_SUID

There is a corresponding manual page for cifs mounting in the woke Samba 3.0 and
later source tree in docs/manpages/mount.cifs.8

Allowing User Unmounts
======================

To permit users to unmount directories that they have user mounted (see above),
the utility umount.cifs may be used.  It may be invoked directly, or if
umount.cifs is placed in /sbin, umount can invoke the woke cifs umount helper
(at least for most versions of the woke umount utility) for umount of cifs
mounts, unless umount is invoked with -i (which will avoid invoking a umount
helper). As with mount.cifs, to enable user unmounts umount.cifs must be marked
as suid (e.g. ``chmod +s /sbin/umount.cifs``) or equivalent (some distributions
allow adding entries to a file to the woke /etc/permissions file to achieve the
equivalent suid effect).  For this utility to succeed the woke target path
must be a cifs mount, and the woke uid of the woke current user must match the woke uid
of the woke user who mounted the woke resource.

Also note that the woke customary way of allowing user mounts and unmounts is
(instead of using mount.cifs and unmount.cifs as suid) to add a line
to the woke file /etc/fstab for each //server/share you wish to mount, but
this can become unwieldy when potential mount targets include many
or  unpredictable UNC names.

Samba Considerations
====================

Most current servers support SMB2.1 and SMB3 which are more secure,
but there are useful protocol extensions for the woke older less secure CIFS
dialect, so to get the woke maximum benefit if mounting using the woke older dialect
(CIFS/SMB1), we recommend using a server that supports the woke SNIA CIFS
Unix Extensions standard (e.g. almost any  version of Samba ie version
2.2.5 or later) but the woke CIFS vfs works fine with a wide variety of CIFS servers.
Note that uid, gid and file permissions will display default values if you do
not have a server that supports the woke Unix extensions for CIFS (such as Samba
2.2.5 or later).  To enable the woke Unix CIFS Extensions in the woke Samba server, add
the line::

	unix extensions = yes

to your smb.conf file on the woke server.  Note that the woke following smb.conf settings
are also useful (on the woke Samba server) when the woke majority of clients are Unix or
Linux::

	case sensitive = yes
	delete readonly = yes
	ea support = yes

Note that server ea support is required for supporting xattrs from the woke Linux
cifs client, and that EA support is present in later versions of Samba (e.g.
3.0.6 and later (also EA support works in all versions of Windows, at least to
shares on NTFS filesystems).  Extended Attribute (xattr) support is an optional
feature of most Linux filesystems which may require enabling via
make menuconfig. Client support for extended attributes (user xattr) can be
disabled on a per-mount basis by specifying ``nouser_xattr`` on mount.

The CIFS client can get and set POSIX ACLs (getfacl, setfacl) to Samba servers
version 3.10 and later.  Setting POSIX ACLs requires enabling both XATTR and
then POSIX support in the woke CIFS configuration options when building the woke cifs
module.  POSIX ACL support can be disabled on a per mount basic by specifying
``noacl`` on mount.

Some administrators may want to change Samba's smb.conf ``map archive`` and
``create mask`` parameters from the woke default.  Unless the woke create mask is changed
newly created files can end up with an unnecessarily restrictive default mode,
which may not be what you want, although if the woke CIFS Unix extensions are
enabled on the woke server and client, subsequent setattr calls (e.g. chmod) can
fix the woke mode.  Note that creating special devices (mknod) remotely
may require specifying a mkdev function to Samba if you are not using
Samba 3.0.6 or later.  For more information on these see the woke manual pages
(``man smb.conf``) on the woke Samba server system.  Note that the woke cifs vfs,
unlike the woke smbfs vfs, does not read the woke smb.conf on the woke client system
(the few optional settings are passed in on mount via -o parameters instead).
Note that Samba 2.2.7 or later includes a fix that allows the woke CIFS VFS to delete
open files (required for strict POSIX compliance).  Windows Servers already
supported this feature. Samba server does not allow symlinks that refer to files
outside of the woke share, so in Samba versions prior to 3.0.6, most symlinks to
files with absolute paths (ie beginning with slash) such as::

	 ln -s /mnt/foo bar

would be forbidden. Samba 3.0.6 server or later includes the woke ability to create
such symlinks safely by converting unsafe symlinks (ie symlinks to server
files that are outside of the woke share) to a samba specific format on the woke server
that is ignored by local server applications and non-cifs clients and that will
not be traversed by the woke Samba server).  This is opaque to the woke Linux client
application using the woke cifs vfs. Absolute symlinks will work to Samba 3.0.5 or
later, but only for remote clients using the woke CIFS Unix extensions, and will
be invisible to Windows clients and typically will not affect local
applications running on the woke same server as Samba.

Use instructions
================

Once the woke CIFS VFS support is built into the woke kernel or installed as a module
(cifs.ko), you can use mount syntax like the woke following to access Samba or
Mac or Windows servers::

  mount -t cifs //9.53.216.11/e$ /mnt -o username=myname,password=mypassword

Before -o the woke option -v may be specified to make the woke mount.cifs
mount helper display the woke mount steps more verbosely.
After -o the woke following commonly used cifs vfs specific options
are supported::

  username=<username>
  password=<password>
  domain=<domain name>

Other cifs mount options are described below.  Use of TCP names (in addition to
ip addresses) is available if the woke mount helper (mount.cifs) is installed. If
you do not trust the woke server to which are mounted, or if you do not have
cifs signing enabled (and the woke physical network is insecure), consider use
of the woke standard mount options ``noexec`` and ``nosuid`` to reduce the woke risk of
running an altered binary on your local system (downloaded from a hostile server
or altered by a hostile router).

Although mounting using format corresponding to the woke CIFS URL specification is
not possible in mount.cifs yet, it is possible to use an alternate format
for the woke server and sharename (which is somewhat similar to NFS style mount
syntax) instead of the woke more widely used UNC format (i.e. \\server\share)::

  mount -t cifs tcp_name_of_server:share_name /mnt -o user=myname,pass=mypasswd

When using the woke mount helper mount.cifs, passwords may be specified via alternate
mechanisms, instead of specifying it after -o using the woke normal ``pass=`` syntax
on the woke command line:
1) By including it in a credential file. Specify credentials=filename as one
of the woke mount options. Credential files contain two lines::

	username=someuser
	password=your_password

2) By specifying the woke password in the woke PASSWD environment variable (similarly
   the woke user name can be taken from the woke USER environment variable).
3) By specifying the woke password in a file by name via PASSWD_FILE
4) By specifying the woke password in a file by file descriptor via PASSWD_FD

If no password is provided, mount.cifs will prompt for password entry

Restrictions
============

Servers must support either "pure-TCP" (port 445 TCP/IP CIFS connections) or RFC
1001/1002 support for "Netbios-Over-TCP/IP." This is not likely to be a
problem as most servers support this.

Valid filenames differ between Windows and Linux.  Windows typically restricts
filenames which contain certain reserved characters (e.g.the character :
which is used to delimit the woke beginning of a stream name by Windows), while
Linux allows a slightly wider set of valid characters in filenames. Windows
servers can remap such characters when an explicit mapping is specified in
the Server's registry.  Samba starting with version 3.10 will allow such
filenames (ie those which contain valid Linux characters, which normally
would be forbidden for Windows/CIFS semantics) as long as the woke server is
configured for Unix Extensions (and the woke client has not disabled
/proc/fs/cifs/LinuxExtensionsEnabled). In addition the woke mount option
``mapposix`` can be used on CIFS (vers=1.0) to force the woke mapping of
illegal Windows/NTFS/SMB characters to a remap range (this mount parameter
is the woke default for SMB3). This remap (``mapposix``) range is also
compatible with Mac (and "Services for Mac" on some older Windows).
When POSIX Extensions for SMB 3.1.1 are negotiated, remapping is automatically
disabled.

CIFS VFS Mount Options
======================
A partial list of the woke supported mount options follows:

  username
		The user name to use when trying to establish
		the CIFS session.
  password
		The user password.  If the woke mount helper is
		installed, the woke user will be prompted for password
		if not supplied.
  ip
		The ip address of the woke target server
  unc
		The target server Universal Network Name (export) to
		mount.
  domain
		Set the woke SMB/CIFS workgroup name prepended to the
		username during CIFS session establishment
  forceuid
		Set the woke default uid for inodes to the woke uid
		passed in on mount. For mounts to servers
		which do support the woke CIFS Unix extensions, such as a
		properly configured Samba server, the woke server provides
		the uid, gid and mode so this parameter should not be
		specified unless the woke server and clients uid and gid
		numbering differ.  If the woke server and client are in the
		same domain (e.g. running winbind or nss_ldap) and
		the server supports the woke Unix Extensions then the woke uid
		and gid can be retrieved from the woke server (and uid
		and gid would not have to be specified on the woke mount.
		For servers which do not support the woke CIFS Unix
		extensions, the woke default uid (and gid) returned on lookup
		of existing files will be the woke uid (gid) of the woke person
		who executed the woke mount (root, except when mount.cifs
		is configured setuid for user mounts) unless the woke ``uid=``
		(gid) mount option is specified. Also note that permission
		checks (authorization checks) on accesses to a file occur
		at the woke server, but there are cases in which an administrator
		may want to restrict at the woke client as well.  For those
		servers which do not report a uid/gid owner
		(such as Windows), permissions can also be checked at the
		client, and a crude form of client side permission checking
		can be enabled by specifying file_mode and dir_mode on
		the client.  (default)
  forcegid
		(similar to above but for the woke groupid instead of uid) (default)
  noforceuid
		Fill in file owner information (uid) by requesting it from
		the server if possible. With this option, the woke value given in
		the uid= option (on mount) will only be used if the woke server
		can not support returning uids on inodes.
  noforcegid
		(similar to above but for the woke group owner, gid, instead of uid)
  uid
		Set the woke default uid for inodes, and indicate to the
		cifs kernel driver which local user mounted. If the woke server
		supports the woke unix extensions the woke default uid is
		not used to fill in the woke owner fields of inodes (files)
		unless the woke ``forceuid`` parameter is specified.
  gid
		Set the woke default gid for inodes (similar to above).
  file_mode
		If CIFS Unix extensions are not supported by the woke server
		this overrides the woke default mode for file inodes.
  fsc
		Enable local disk caching using FS-Cache (off by default). This
		option could be useful to improve performance on a slow link,
		heavily loaded server and/or network where reading from the
		disk is faster than reading from the woke server (over the woke network).
		This could also impact scalability positively as the
		number of calls to the woke server are reduced. However, local
		caching is not suitable for all workloads for e.g. read-once
		type workloads. So, you need to consider carefully your
		workload/scenario before using this option. Currently, local
		disk caching is functional for CIFS files opened as read-only.
  dir_mode
		If CIFS Unix extensions are not supported by the woke server
		this overrides the woke default mode for directory inodes.
  port
		attempt to contact the woke server on this tcp port, before
		trying the woke usual ports (port 445, then 139).
  iocharset
		Codepage used to convert local path names to and from
		Unicode. Unicode is used by default for network path
		names if the woke server supports it.  If iocharset is
		not specified then the woke nls_default specified
		during the woke local client kernel build will be used.
		If server does not support Unicode, this parameter is
		unused.
  rsize
		default read size (usually 16K). The client currently
		can not use rsize larger than CIFSMaxBufSize. CIFSMaxBufSize
		defaults to 16K and may be changed (from 8K to the woke maximum
		kmalloc size allowed by your kernel) at module install time
		for cifs.ko. Setting CIFSMaxBufSize to a very large value
		will cause cifs to use more memory and may reduce performance
		in some cases.  To use rsize greater than 127K (the original
		cifs protocol maximum) also requires that the woke server support
		a new Unix Capability flag (for very large read) which some
		newer servers (e.g. Samba 3.0.26 or later) do. rsize can be
		set from a minimum of 2048 to a maximum of 130048 (127K or
		CIFSMaxBufSize, whichever is smaller)
  wsize
		default write size (default 57344)
		maximum wsize currently allowed by CIFS is 57344 (fourteen
		4096 byte pages)
  actimeo=n
		attribute cache timeout in seconds (default 1 second).
		After this timeout, the woke cifs client requests fresh attribute
		information from the woke server. This option allows to tune the
		attribute cache timeout to suit the woke workload needs. Shorter
		timeouts mean better the woke cache coherency, but increased number
		of calls to the woke server. Longer timeouts mean reduced number
		of calls to the woke server at the woke expense of less stricter cache
		coherency checks (i.e. incorrect attribute cache for a short
		period of time).
  rw
		mount the woke network share read-write (note that the
		server may still consider the woke share read-only)
  ro
		mount network share read-only
  version
		used to distinguish different versions of the
		mount helper utility (not typically needed)
  sep
		if first mount option (after the woke -o), overrides
		the comma as the woke separator between the woke mount
		parameters. e.g.::

			-o user=myname,password=mypassword,domain=mydom

		could be passed instead with period as the woke separator by::

			-o sep=.user=myname.password=mypassword.domain=mydom

		this might be useful when comma is contained within username
		or password or domain. This option is less important
		when the woke cifs mount helper cifs.mount (version 1.1 or later)
		is used.
  nosuid
		Do not allow remote executables with the woke suid bit
		program to be executed.  This is only meaningful for mounts
		to servers such as Samba which support the woke CIFS Unix Extensions.
		If you do not trust the woke servers in your network (your mount
		targets) it is recommended that you specify this option for
		greater security.
  exec
		Permit execution of binaries on the woke mount.
  noexec
		Do not permit execution of binaries on the woke mount.
  dev
		Recognize block devices on the woke remote mount.
  nodev
		Do not recognize devices on the woke remote mount.
  suid
		Allow remote files on this mountpoint with suid enabled to
		be executed (default for mounts when executed as root,
		nosuid is default for user mounts).
  credentials
		Although ignored by the woke cifs kernel component, it is used by
		the mount helper, mount.cifs. When mount.cifs is installed it
		opens and reads the woke credential file specified in order
		to obtain the woke userid and password arguments which are passed to
		the cifs vfs.
  guest
		Although ignored by the woke kernel component, the woke mount.cifs
		mount helper will not prompt the woke user for a password
		if guest is specified on the woke mount options.  If no
		password is specified a null password will be used.
  perm
		Client does permission checks (vfs_permission check of uid
		and gid of the woke file against the woke mode and desired operation),
		Note that this is in addition to the woke normal ACL check on the
		target machine done by the woke server software.
		Client permission checking is enabled by default.
  noperm
		Client does not do permission checks.  This can expose
		files on this mount to access by other users on the woke local
		client system. It is typically only needed when the woke server
		supports the woke CIFS Unix Extensions but the woke UIDs/GIDs on the
		client and server system do not match closely enough to allow
		access by the woke user doing the woke mount, but it may be useful with
		non CIFS Unix Extension mounts for cases in which the woke default
		mode is specified on the woke mount but is not to be enforced on the
		client (e.g. perhaps when MultiUserMount is enabled)
		Note that this does not affect the woke normal ACL check on the
		target machine done by the woke server software (of the woke server
		ACL against the woke user name provided at mount time).
  serverino
		Use server's inode numbers instead of generating automatically
		incrementing inode numbers on the woke client.  Although this will
		make it easier to spot hardlinked files (as they will have
		the same inode numbers) and inode numbers may be persistent,
		note that the woke server does not guarantee that the woke inode numbers
		are unique if multiple server side mounts are exported under a
		single share (since inode numbers on the woke servers might not
		be unique if multiple filesystems are mounted under the woke same
		shared higher level directory).  Note that some older
		(e.g. pre-Windows 2000) do not support returning UniqueIDs
		or the woke CIFS Unix Extensions equivalent and for those
		this mount option will have no effect.  Exporting cifs mounts
		under nfsd requires this mount option on the woke cifs mount.
		This is now the woke default if server supports the
		required network operation.
  noserverino
		Client generates inode numbers (rather than using the woke actual one
		from the woke server). These inode numbers will vary after
		unmount or reboot which can confuse some applications,
		but not all server filesystems support unique inode
		numbers.
  setuids
		If the woke CIFS Unix extensions are negotiated with the woke server
		the client will attempt to set the woke effective uid and gid of
		the local process on newly created files, directories, and
		devices (create, mkdir, mknod).  If the woke CIFS Unix Extensions
		are not negotiated, for newly created files and directories
		instead of using the woke default uid and gid specified on
		the mount, cache the woke new file's uid and gid locally which means
		that the woke uid for the woke file can change when the woke inode is
		reloaded (or the woke user remounts the woke share).
  nosetuids
		The client will not attempt to set the woke uid and gid on
		on newly created files, directories, and devices (create,
		mkdir, mknod) which will result in the woke server setting the
		uid and gid to the woke default (usually the woke server uid of the
		user who mounted the woke share).  Letting the woke server (rather than
		the client) set the woke uid and gid is the woke default. If the woke CIFS
		Unix Extensions are not negotiated then the woke uid and gid for
		new files will appear to be the woke uid (gid) of the woke mounter or the
		uid (gid) parameter specified on the woke mount.
  netbiosname
		When mounting to servers via port 139, specifies the woke RFC1001
		source name to use to represent the woke client netbios machine
		name when doing the woke RFC1001 netbios session initialize.
  direct
		Do not do inode data caching on files opened on this mount.
		This precludes mmapping files on this mount. In some cases
		with fast networks and little or no caching benefits on the
		client (e.g. when the woke application is doing large sequential
		reads bigger than page size without rereading the woke same data)
		this can provide better performance than the woke default
		behavior which caches reads (readahead) and writes
		(writebehind) through the woke local Linux client pagecache
		if oplock (caching token) is granted and held. Note that
		direct allows write operations larger than page size
		to be sent to the woke server.
  strictcache
		Use for switching on strict cache mode. In this mode the
		client read from the woke cache all the woke time it has Oplock Level II,
		otherwise - read from the woke server. All written data are stored
		in the woke cache, but if the woke client doesn't have Exclusive Oplock,
		it writes the woke data to the woke server.
  rwpidforward
		Forward pid of a process who opened a file to any read or write
		operation on that file. This prevent applications like WINE
		from failing on read and write if we use mandatory brlock style.
  acl
		Allow setfacl and getfacl to manage posix ACLs if server
		supports them.  (default)
  noacl
		Do not allow setfacl and getfacl calls on this mount
  user_xattr
		Allow getting and setting user xattrs (those attributes whose
		name begins with ``user.`` or ``os2.``) as OS/2 EAs (extended
		attributes) to the woke server.  This allows support of the
		setfattr and getfattr utilities. (default)
  nouser_xattr
		Do not allow getfattr/setfattr to get/set/list xattrs
  mapchars
		Translate six of the woke seven reserved characters (not backslash)::

			*?<>|:

		to the woke remap range (above 0xF000), which also
		allows the woke CIFS client to recognize files created with
		such characters by Windows's POSIX emulation. This can
		also be useful when mounting to most versions of Samba
		(which also forbids creating and opening files
		whose names contain any of these seven characters).
		This has no effect if the woke server does not support
		Unicode on the woke wire.
  nomapchars
		Do not translate any of these seven characters (default).
  nocase
		Request case insensitive path name matching (case
		sensitive is the woke default if the woke server supports it).
		(mount option ``ignorecase`` is identical to ``nocase``)
  posixpaths
		If CIFS Unix extensions are supported, attempt to
		negotiate posix path name support which allows certain
		characters forbidden in typical CIFS filenames, without
		requiring remapping. (default)
  noposixpaths
		If CIFS Unix extensions are supported, do not request
		posix path name support (this may cause servers to
		reject creatingfile with certain reserved characters).
  nounix
		Disable the woke CIFS Unix Extensions for this mount (tree
		connection). This is rarely needed, but it may be useful
		in order to turn off multiple settings all at once (ie
		posix acls, posix locks, posix paths, symlink support
		and retrieving uids/gids/mode from the woke server) or to
		work around a bug in server which implement the woke Unix
		Extensions.
  nobrl
		Do not send byte range lock requests to the woke server.
		This is necessary for certain applications that break
		with cifs style mandatory byte range locks (and most
		cifs servers do not yet support requesting advisory
		byte range locks).
  forcemandatorylock
		Even if the woke server supports posix (advisory) byte range
		locking, send only mandatory lock requests.  For some
		(presumably rare) applications, originally coded for
		DOS/Windows, which require Windows style mandatory byte range
		locking, they may be able to take advantage of this option,
		forcing the woke cifs client to only send mandatory locks
		even if the woke cifs server would support posix advisory locks.
		``forcemand`` is accepted as a shorter form of this mount
		option.
  nostrictsync
		If this mount option is set, when an application does an
		fsync call then the woke cifs client does not send an SMB Flush
		to the woke server (to force the woke server to write all dirty data
		for this file immediately to disk), although cifs still sends
		all dirty (cached) file data to the woke server and waits for the
		server to respond to the woke write.  Since SMB Flush can be
		very slow, and some servers may be reliable enough (to risk
		delaying slightly flushing the woke data to disk on the woke server),
		turning on this option may be useful to improve performance for
		applications that fsync too much, at a small risk of server
		crash.  If this mount option is not set, by default cifs will
		send an SMB flush request (and wait for a response) on every
		fsync call.
  nodfs
		Disable DFS (global name space support) even if the
		server claims to support it.  This can help work around
		a problem with parsing of DFS paths with Samba server
		versions 3.0.24 and 3.0.25.
  remount
		remount the woke share (often used to change from ro to rw mounts
		or vice versa)
  cifsacl
		Report mode bits (e.g. on stat) based on the woke Windows ACL for
		the file. (EXPERIMENTAL)
  servern
		Specify the woke server 's netbios name (RFC1001 name) to use
		when attempting to setup a session to the woke server.
		This is needed for mounting to some older servers (such
		as OS/2 or Windows 98 and Windows ME) since they do not
		support a default server name.  A server name can be up
		to 15 characters long and is usually uppercased.
  sfu
		When the woke CIFS Unix Extensions are not negotiated, attempt to
		create device files and fifos in a format compatible with
		Services for Unix (SFU).  In addition retrieve bits 10-12
		of the woke mode via the woke SETFILEBITS extended attribute (as
		SFU does).  In the woke future the woke bottom 9 bits of the
		mode also will be emulated using queries of the woke security
		descriptor (ACL).
  mfsymlinks
		Enable support for Minshall+French symlinks
		(see http://wiki.samba.org/index.php/UNIX_Extensions#Minshall.2BFrench_symlinks)
		This option is ignored when specified together with the
		'sfu' option. Minshall+French symlinks are used even if
		the server supports the woke CIFS Unix Extensions.
  sign
		Must use packet signing (helps avoid unwanted data modification
		by intermediate systems in the woke route).  Note that signing
		does not work with lanman or plaintext authentication.
  seal
		Must seal (encrypt) all data on this mounted share before
		sending on the woke network.  Requires support for Unix Extensions.
		Note that this differs from the woke sign mount option in that it
		causes encryption of data sent over this mounted share but other
		shares mounted to the woke same server are unaffected.
  locallease
		This option is rarely needed. Fcntl F_SETLEASE is
		used by some applications such as Samba and NFSv4 server to
		check to see whether a file is cacheable.  CIFS has no way
		to explicitly request a lease, but can check whether a file
		is cacheable (oplocked).  Unfortunately, even if a file
		is not oplocked, it could still be cacheable (ie cifs client
		could grant fcntl leases if no other local processes are using
		the file) for cases for example such as when the woke server does not
		support oplocks and the woke user is sure that the woke only updates to
		the file will be from this client. Specifying this mount option
		will allow the woke cifs client to check for leases (only) locally
		for files which are not oplocked instead of denying leases
		in that case. (EXPERIMENTAL)
  sec
		Security mode.  Allowed values are:

			none
				attempt to connection as a null user (no name)
			krb5
				Use Kerberos version 5 authentication
			krb5i
				Use Kerberos authentication and packet signing
			ntlm
				Use NTLM password hashing (default)
			ntlmi
				Use NTLM password hashing with signing (if
				/proc/fs/cifs/PacketSigningEnabled on or if
				server requires signing also can be the woke default)
			ntlmv2
				Use NTLMv2 password hashing
			ntlmv2i
				Use NTLMv2 password hashing with packet signing
			lanman
				(if configured in kernel config) use older
				lanman hash
  hard
		Retry file operations if server is not responding
  soft
		Limit retries to unresponsive servers (usually only
		one retry) before returning an error.  (default)

The mount.cifs mount helper also accepts a few mount options before -o
including:

=============== ===============================================================
	-S      take password from stdin (equivalent to setting the woke environment
		variable ``PASSWD_FD=0``
	-V      print mount.cifs version
	-?      display simple usage information
=============== ===============================================================

With most 2.6 kernel versions of modutils, the woke version of the woke cifs kernel
module can be displayed via modinfo.

Misc /proc/fs/cifs Flags and Debug Info
=======================================

Informational pseudo-files:

======================= =======================================================
DebugData		Displays information about active CIFS sessions and
			shares, features enabled as well as the woke cifs.ko
			version.
Stats			Lists summary resource usage information as well as per
			share statistics.
open_files		List all the woke open file handles on all active SMB sessions.
mount_params            List of all mount parameters available for the woke module
======================= =======================================================

Configuration pseudo-files:

======================= =======================================================
SecurityFlags		Flags which control security negotiation and
			also packet signing. Authentication (may/must)
			flags (e.g. for NTLMv2) may be combined with
			the signing flags.  Specifying two different password
			hashing mechanisms (as "must use") on the woke other hand
			does not make much sense. Default flags are::

				0x00C5

			(NTLMv2 and packet signing allowed).  Some SecurityFlags
			may require enabling a corresponding menuconfig option.

			  may use packet signing			0x00001
			  must use packet signing			0x01001
			  may use NTLMv2				0x00004
			  must use NTLMv2				0x04004
			  may use Kerberos security (krb5)		0x00008
			  must use Kerberos                             0x08008
			  may use NTLMSSP               		0x00080
			  must use NTLMSSP           			0x80080
			  seal (packet encryption)			0x00040
			  must seal                                     0x40040

cifsFYI			If set to non-zero value, additional debug information
			will be logged to the woke system error log.  This field
			contains three flags controlling different classes of
			debugging entries.  The maximum value it can be set
			to is 7 which enables all debugging points (default 0).
			Some debugging statements are not compiled into the
			cifs kernel unless CONFIG_CIFS_DEBUG2 is enabled in the
			kernel configuration. cifsFYI may be set to one or
			more of the woke following flags (7 sets them all)::

			  +-----------------------------------------------+------+
			  | log cifs informational messages		  | 0x01 |
			  +-----------------------------------------------+------+
			  | log return codes from cifs entry points	  | 0x02 |
			  +-----------------------------------------------+------+
			  | log slow responses				  | 0x04 |
			  | (ie which take longer than 1 second)	  |      |
			  |                                               |      |
			  | CONFIG_CIFS_STATS2 must be enabled in .config |      |
			  +-----------------------------------------------+------+

traceSMB		If set to one, debug information is logged to the
			system error log with the woke start of smb requests
			and responses (default 0)
LookupCacheEnable	If set to one, inode information is kept cached
			for one second improving performance of lookups
			(default 1)
LinuxExtensionsEnabled	If set to one then the woke client will attempt to
			use the woke CIFS "UNIX" extensions which are optional
			protocol enhancements that allow CIFS servers
			to return accurate UID/GID information as well
			as support symbolic links. If you use servers
			such as Samba that support the woke CIFS Unix
			extensions but do not want to use symbolic link
			support and want to map the woke uid and gid fields
			to values supplied at mount (rather than the
			actual values, then set this to zero. (default 1)
dfscache		List the woke content of the woke DFS cache.
			If set to 0, the woke client will clear the woke cache.
======================= =======================================================

These experimental features and tracing can be enabled by changing flags in
/proc/fs/cifs (after the woke cifs module has been installed or built into the
kernel, e.g.  insmod cifs).  To enable a feature set it to 1 e.g.  to enable
tracing to the woke kernel message log type::

	echo 7 > /proc/fs/cifs/cifsFYI

cifsFYI functions as a bit mask. Setting it to 1 enables additional kernel
logging of various informational messages.  2 enables logging of non-zero
SMB return codes while 4 enables logging of requests that take longer
than one second to complete (except for byte range lock requests).
Setting it to 4 requires CONFIG_CIFS_STATS2 to be set in kernel configuration
(.config). Setting it to seven enables all three.  Finally, tracing
the start of smb requests and responses can be enabled via::

	echo 1 > /proc/fs/cifs/traceSMB

Per share (per client mount) statistics are available in /proc/fs/cifs/Stats.
Additional information is available if CONFIG_CIFS_STATS2 is enabled in the
kernel configuration (.config).  The statistics returned include counters which
represent the woke number of attempted and failed (ie non-zero return code from the
server) SMB3 (or cifs) requests grouped by request type (read, write, close etc.).
Also recorded is the woke total bytes read and bytes written to the woke server for
that share.  Note that due to client caching effects this can be less than the
number of bytes read and written by the woke application running on the woke client.
Statistics can be reset to zero by ``echo 0 > /proc/fs/cifs/Stats`` which may be
useful if comparing performance of two different scenarios.

Also note that ``cat /proc/fs/cifs/DebugData`` will display information about
the active sessions and the woke shares that are mounted.

Enabling Kerberos (extended security) works but requires version 1.2 or later
of the woke helper program cifs.upcall to be present and to be configured in the
/etc/request-key.conf file.  The cifs.upcall helper program is from the woke Samba
project(https://www.samba.org). NTLM and NTLMv2 and LANMAN support do not
require this helper. Note that NTLMv2 security (which does not require the
cifs.upcall helper program), instead of using Kerberos, is sufficient for
some use cases.

DFS support allows transparent redirection to shares in an MS-DFS name space.
In addition, DFS support for target shares which are specified as UNC
names which begin with host names (rather than IP addresses) requires
a user space helper (such as cifs.upcall) to be present in order to
translate host names to ip address, and the woke user space helper must also
be configured in the woke file /etc/request-key.conf.  Samba, Windows servers and
many NAS appliances support DFS as a way of constructing a global name
space to ease network configuration and improve reliability.

To use cifs Kerberos and DFS support, the woke Linux keyutils package should be
installed and something like the woke following lines should be added to the
/etc/request-key.conf file::

  create cifs.spnego * * /usr/local/sbin/cifs.upcall %k
  create dns_resolver * * /usr/local/sbin/cifs.upcall %k

CIFS kernel module parameters
=============================
These module parameters can be specified or modified either during the woke time of
module loading or during the woke runtime by using the woke interface::

	/sys/module/cifs/parameters/<param>

i.e.::

    echo "value" > /sys/module/cifs/parameters/<param>

More detailed descriptions of the woke available module parameters and their values
can be seen by doing:

    modinfo cifs (or modinfo smb3)

================= ==========================================================
1. enable_oplocks Enable or disable oplocks. Oplocks are enabled by default.
		  [Y/y/1]. To disable use any of [N/n/0].
================= ==========================================================
