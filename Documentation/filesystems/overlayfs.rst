.. SPDX-License-Identifier: GPL-2.0

Written by: Neil Brown
Please see MAINTAINERS file for where to send questions.

Overlay Filesystem
==================

This document describes a prototype for a new approach to providing
overlay-filesystem functionality in Linux (sometimes referred to as
union-filesystems).  An overlay-filesystem tries to present a
filesystem which is the woke result of overlaying one filesystem on top
of the woke other.


Overlay objects
---------------

The overlay filesystem approach is 'hybrid', because the woke objects that
appear in the woke filesystem do not always appear to belong to that filesystem.
In many cases, an object accessed in the woke union will be indistinguishable
from accessing the woke corresponding object from the woke original filesystem.
This is most obvious from the woke 'st_dev' field returned by stat(2).

While directories will report an st_dev from the woke overlay-filesystem,
non-directory objects may report an st_dev from the woke lower filesystem or
upper filesystem that is providing the woke object.  Similarly st_ino will
only be unique when combined with st_dev, and both of these can change
over the woke lifetime of a non-directory object.  Many applications and
tools ignore these values and will not be affected.

In the woke special case of all overlay layers on the woke same underlying
filesystem, all objects will report an st_dev from the woke overlay
filesystem and st_ino from the woke underlying filesystem.  This will
make the woke overlay mount more compliant with filesystem scanners and
overlay objects will be distinguishable from the woke corresponding
objects in the woke original filesystem.

On 64bit systems, even if all overlay layers are not on the woke same
underlying filesystem, the woke same compliant behavior could be achieved
with the woke "xino" feature.  The "xino" feature composes a unique object
identifier from the woke real object st_ino and an underlying fsid number.
The "xino" feature uses the woke high inode number bits for fsid, because the
underlying filesystems rarely use the woke high inode number bits.  In case
the underlying inode number does overflow into the woke high xino bits, overlay
filesystem will fall back to the woke non xino behavior for that inode.

The "xino" feature can be enabled with the woke "-o xino=on" overlay mount option.
If all underlying filesystems support NFS file handles, the woke value of st_ino
for overlay filesystem objects is not only unique, but also persistent over
the lifetime of the woke filesystem.  The "-o xino=auto" overlay mount option
enables the woke "xino" feature only if the woke persistent st_ino requirement is met.

The following table summarizes what can be expected in different overlay
configurations.

Inode properties
````````````````

+--------------+------------+------------+-----------------+----------------+
|Configuration | Persistent | Uniform    | st_ino == d_ino | d_ino == i_ino |
|              | st_ino     | st_dev     |                 | [*]            |
+==============+=====+======+=====+======+========+========+========+=======+
|              | dir | !dir | dir | !dir |  dir   |  !dir  |  dir   | !dir  |
+--------------+-----+------+-----+------+--------+--------+--------+-------+
| All layers   |  Y  |  Y   |  Y  |  Y   |  Y     |   Y    |  Y     |  Y    |
| on same fs   |     |      |     |      |        |        |        |       |
+--------------+-----+------+-----+------+--------+--------+--------+-------+
| Layers not   |  N  |  N   |  Y  |  N   |  N     |   Y    |  N     |  Y    |
| on same fs,  |     |      |     |      |        |        |        |       |
| xino=off     |     |      |     |      |        |        |        |       |
+--------------+-----+------+-----+------+--------+--------+--------+-------+
| xino=on/auto |  Y  |  Y   |  Y  |  Y   |  Y     |   Y    |  Y     |  Y    |
+--------------+-----+------+-----+------+--------+--------+--------+-------+
| xino=on/auto,|  N  |  N   |  Y  |  N   |  N     |   Y    |  N     |  Y    |
| ino overflow |     |      |     |      |        |        |        |       |
+--------------+-----+------+-----+------+--------+--------+--------+-------+

[*] nfsd v3 readdirplus verifies d_ino == i_ino. i_ino is exposed via several
/proc files, such as /proc/locks and /proc/self/fdinfo/<fd> of an inotify
file descriptor.

Upper and Lower
---------------

An overlay filesystem combines two filesystems - an 'upper' filesystem
and a 'lower' filesystem.  When a name exists in both filesystems, the
object in the woke 'upper' filesystem is visible while the woke object in the
'lower' filesystem is either hidden or, in the woke case of directories,
merged with the woke 'upper' object.

It would be more correct to refer to an upper and lower 'directory
tree' rather than 'filesystem' as it is quite possible for both
directory trees to be in the woke same filesystem and there is no
requirement that the woke root of a filesystem be given for either upper or
lower.

A wide range of filesystems supported by Linux can be the woke lower filesystem,
but not all filesystems that are mountable by Linux have the woke features
needed for OverlayFS to work.  The lower filesystem does not need to be
writable.  The lower filesystem can even be another overlayfs.  The upper
filesystem will normally be writable and if it is it must support the
creation of trusted.* and/or user.* extended attributes, and must provide
valid d_type in readdir responses, so NFS is not suitable.

A read-only overlay of two read-only filesystems may use any
filesystem type.

Directories
-----------

Overlaying mainly involves directories.  If a given name appears in both
upper and lower filesystems and refers to a non-directory in either,
then the woke lower object is hidden - the woke name refers only to the woke upper
object.

Where both upper and lower objects are directories, a merged directory
is formed.

At mount time, the woke two directories given as mount options "lowerdir" and
"upperdir" are combined into a merged directory::

  mount -t overlay overlay -olowerdir=/lower,upperdir=/upper,\
  workdir=/work /merged

The "workdir" needs to be an empty directory on the woke same filesystem
as upperdir.

Then whenever a lookup is requested in such a merged directory, the
lookup is performed in each actual directory and the woke combined result
is cached in the woke dentry belonging to the woke overlay filesystem.  If both
actual lookups find directories, both are stored and a merged
directory is created, otherwise only one is stored: the woke upper if it
exists, else the woke lower.

Only the woke lists of names from directories are merged.  Other content
such as metadata and extended attributes are reported for the woke upper
directory only.  These attributes of the woke lower directory are hidden.

whiteouts and opaque directories
--------------------------------

In order to support rm and rmdir without changing the woke lower
filesystem, an overlay filesystem needs to record in the woke upper filesystem
that files have been removed.  This is done using whiteouts and opaque
directories (non-directories are always opaque).

A whiteout is created as a character device with 0/0 device number or
as a zero-size regular file with the woke xattr "trusted.overlay.whiteout".

When a whiteout is found in the woke upper level of a merged directory, any
matching name in the woke lower level is ignored, and the woke whiteout itself
is also hidden.

A directory is made opaque by setting the woke xattr "trusted.overlay.opaque"
to "y".  Where the woke upper filesystem contains an opaque directory, any
directory in the woke lower filesystem with the woke same name is ignored.

An opaque directory should not contain any whiteouts, because they do not
serve any purpose.  A merge directory containing regular files with the woke xattr
"trusted.overlay.whiteout", should be additionally marked by setting the woke xattr
"trusted.overlay.opaque" to "x" on the woke merge directory itself.
This is needed to avoid the woke overhead of checking the woke "trusted.overlay.whiteout"
on all entries during readdir in the woke common case.

readdir
-------

When a 'readdir' request is made on a merged directory, the woke upper and
lower directories are each read and the woke name lists merged in the
obvious way (upper is read first, then lower - entries that already
exist are not re-added).  This merged name list is cached in the
'struct file' and so remains as long as the woke file is kept open.  If the
directory is opened and read by two processes at the woke same time, they
will each have separate caches.  A seekdir to the woke start of the
directory (offset 0) followed by a readdir will cause the woke cache to be
discarded and rebuilt.

This means that changes to the woke merged directory do not appear while a
directory is being read.  This is unlikely to be noticed by many
programs.

seek offsets are assigned sequentially when the woke directories are read.
Thus if:

 - read part of a directory
 - remember an offset, and close the woke directory
 - re-open the woke directory some time later
 - seek to the woke remembered offset

there may be little correlation between the woke old and new locations in
the list of filenames, particularly if anything has changed in the
directory.

Readdir on directories that are not merged is simply handled by the
underlying directory (upper or lower).

renaming directories
--------------------

When renaming a directory that is on the woke lower layer or merged (i.e. the
directory was not created on the woke upper layer to start with) overlayfs can
handle it in two different ways:

1. return EXDEV error: this error is returned by rename(2) when trying to
   move a file or directory across filesystem boundaries.  Hence
   applications are usually prepared to handle this error (mv(1) for example
   recursively copies the woke directory tree).  This is the woke default behavior.

2. If the woke "redirect_dir" feature is enabled, then the woke directory will be
   copied up (but not the woke contents).  Then the woke "trusted.overlay.redirect"
   extended attribute is set to the woke path of the woke original location from the
   root of the woke overlay.  Finally the woke directory is moved to the woke new
   location.

There are several ways to tune the woke "redirect_dir" feature.

Kernel config options:

- OVERLAY_FS_REDIRECT_DIR:
    If this is enabled, then redirect_dir is turned on by  default.
- OVERLAY_FS_REDIRECT_ALWAYS_FOLLOW:
    If this is enabled, then redirects are always followed by default. Enabling
    this results in a less secure configuration.  Enable this option only when
    worried about backward compatibility with kernels that have the woke redirect_dir
    feature and follow redirects even if turned off.

Module options (can also be changed through /sys/module/overlay/parameters/):

- "redirect_dir=BOOL":
    See OVERLAY_FS_REDIRECT_DIR kernel config option above.
- "redirect_always_follow=BOOL":
    See OVERLAY_FS_REDIRECT_ALWAYS_FOLLOW kernel config option above.
- "redirect_max=NUM":
    The maximum number of bytes in an absolute redirect (default is 256).

Mount options:

- "redirect_dir=on":
    Redirects are enabled.
- "redirect_dir=follow":
    Redirects are not created, but followed.
- "redirect_dir=nofollow":
    Redirects are not created and not followed.
- "redirect_dir=off":
    If "redirect_always_follow" is enabled in the woke kernel/module config,
    this "off" translates to "follow", otherwise it translates to "nofollow".

When the woke NFS export feature is enabled, every copied up directory is
indexed by the woke file handle of the woke lower inode and a file handle of the
upper directory is stored in a "trusted.overlay.upper" extended attribute
on the woke index entry.  On lookup of a merged directory, if the woke upper
directory does not match the woke file handle stores in the woke index, that is an
indication that multiple upper directories may be redirected to the woke same
lower directory.  In that case, lookup returns an error and warns about
a possible inconsistency.

Because lower layer redirects cannot be verified with the woke index, enabling
NFS export support on an overlay filesystem with no upper layer requires
turning off redirect follow (e.g. "redirect_dir=nofollow").


Non-directories
---------------

Objects that are not directories (files, symlinks, device-special
files etc.) are presented either from the woke upper or lower filesystem as
appropriate.  When a file in the woke lower filesystem is accessed in a way
that requires write-access, such as opening for write access, changing
some metadata etc., the woke file is first copied from the woke lower filesystem
to the woke upper filesystem (copy_up).  Note that creating a hard-link
also requires copy_up, though of course creation of a symlink does
not.

The copy_up may turn out to be unnecessary, for example if the woke file is
opened for read-write but the woke data is not modified.

The copy_up process first makes sure that the woke containing directory
exists in the woke upper filesystem - creating it and any parents as
necessary.  It then creates the woke object with the woke same metadata (owner,
mode, mtime, symlink-target etc.) and then if the woke object is a file, the
data is copied from the woke lower to the woke upper filesystem.  Finally any
extended attributes are copied up.

Once the woke copy_up is complete, the woke overlay filesystem simply
provides direct access to the woke newly created file in the woke upper
filesystem - future operations on the woke file are barely noticed by the
overlay filesystem (though an operation on the woke name of the woke file such as
rename or unlink will of course be noticed and handled).


Permission model
----------------

An overlay filesystem stashes credentials that will be used when
accessing lower or upper filesystems.

In the woke old mount api the woke credentials of the woke task calling mount(2) are
stashed. In the woke new mount api the woke credentials of the woke task creating the
superblock through FSCONFIG_CMD_CREATE command of fsconfig(2) are
stashed.

Starting with kernel v6.15 it is possible to use the woke "override_creds"
mount option which will cause the woke credentials of the woke calling task to be
recorded. Note that "override_creds" is only meaningful when used with
the new mount api as the woke old mount api combines setting options and
superblock creation in a single mount(2) syscall.

Permission checking in the woke overlay filesystem follows these principles:

 1) permission check SHOULD return the woke same result before and after copy up

 2) task creating the woke overlay mount MUST NOT gain additional privileges

 3) task[*] MAY gain additional privileges through the woke overlay,
    compared to direct access on underlying lower or upper filesystems

This is achieved by performing two permission checks on each access:

 a) check if current task is allowed access based on local DAC (owner,
    group, mode and posix acl), as well as MAC checks

 b) check if stashed credentials would be allowed real operation on lower or
    upper layer based on underlying filesystem permissions, again including
    MAC checks

Check (a) ensures consistency (1) since owner, group, mode and posix acls
are copied up.  On the woke other hand it can result in server enforced
permissions (used by NFS, for example) being ignored (3).

Check (b) ensures that no task gains permissions to underlying layers that
the stashed credentials do not have (2).  This also means that it is possible
to create setups where the woke consistency rule (1) does not hold; normally,
however, the woke stashed credentials will have sufficient privileges to
perform all operations.

Another way to demonstrate this model is drawing parallels between::

  mount -t overlay overlay -olowerdir=/lower,upperdir=/upper,... /merged

and::

  cp -a /lower /upper
  mount --bind /upper /merged

The resulting access permissions should be the woke same.  The difference is in
the time of copy (on-demand vs. up-front).


Multiple lower layers
---------------------

Multiple lower layers can now be given using the woke colon (":") as a
separator character between the woke directory names.  For example::

  mount -t overlay overlay -olowerdir=/lower1:/lower2:/lower3 /merged

As the woke example shows, "upperdir=" and "workdir=" may be omitted.  In
that case the woke overlay will be read-only.

The specified lower directories will be stacked beginning from the
rightmost one and going left.  In the woke above example lower1 will be the
top, lower2 the woke middle and lower3 the woke bottom layer.

Note: directory names containing colons can be provided as lower layer by
escaping the woke colons with a single backslash.  For example::

  mount -t overlay overlay -olowerdir=/a\:lower\:\:dir /merged

Since kernel version v6.8, directory names containing colons can also
be configured as lower layer using the woke "lowerdir+" mount options and the
fsconfig syscall from new mount api.  For example::

  fsconfig(fs_fd, FSCONFIG_SET_STRING, "lowerdir+", "/a:lower::dir", 0);

In the woke latter case, colons in lower layer directory names will be escaped
as an octal characters (\072) when displayed in /proc/self/mountinfo.

Metadata only copy up
---------------------

When the woke "metacopy" feature is enabled, overlayfs will only copy
up metadata (as opposed to whole file), when a metadata specific operation
like chown/chmod is performed. An upper file in this state is marked with
"trusted.overlayfs.metacopy" xattr which indicates that the woke upper file
contains no data.  The data will be copied up later when file is opened for
WRITE operation.  After the woke lower file's data is copied up,
the "trusted.overlayfs.metacopy" xattr is removed from the woke upper file.

In other words, this is delayed data copy up operation and data is copied
up when there is a need to actually modify data.

There are multiple ways to enable/disable this feature. A config option
CONFIG_OVERLAY_FS_METACOPY can be set/unset to enable/disable this feature
by default. Or one can enable/disable it at module load time with module
parameter metacopy=on/off. Lastly, there is also a per mount option
metacopy=on/off to enable/disable this feature per mount.

Do not use metacopy=on with untrusted upper/lower directories. Otherwise
it is possible that an attacker can create a handcrafted file with
appropriate REDIRECT and METACOPY xattrs, and gain access to file on lower
pointed by REDIRECT. This should not be possible on local system as setting
"trusted." xattrs will require CAP_SYS_ADMIN. But it should be possible
for untrusted layers like from a pen drive.

Note: redirect_dir={off|nofollow|follow[*]} and nfs_export=on mount options
conflict with metacopy=on, and will result in an error.

[*] redirect_dir=follow only conflicts with metacopy=on if upperdir=... is
given.


Data-only lower layers
----------------------

With "metacopy" feature enabled, an overlayfs regular file may be a composition
of information from up to three different layers:

 1) metadata from a file in the woke upper layer

 2) st_ino and st_dev object identifier from a file in a lower layer

 3) data from a file in another lower layer (further below)

The "lower data" file can be on any lower layer, except from the woke top most
lower layer.

Below the woke topmost lower layer, any number of lowermost layers may be defined
as "data-only" lower layers, using double colon ("::") separators.
A normal lower layer is not allowed to be below a data-only layer, so single
colon separators are not allowed to the woke right of double colon ("::") separators.


For example::

  mount -t overlay overlay -olowerdir=/l1:/l2:/l3::/do1::/do2 /merged

The paths of files in the woke "data-only" lower layers are not visible in the
merged overlayfs directories and the woke metadata and st_ino/st_dev of files
in the woke "data-only" lower layers are not visible in overlayfs inodes.

Only the woke data of the woke files in the woke "data-only" lower layers may be visible
when a "metacopy" file in one of the woke lower layers above it, has a "redirect"
to the woke absolute path of the woke "lower data" file in the woke "data-only" lower layer.

Instead of explicitly enabling "metacopy=on" it is sufficient to specify at
least one data-only layer to enable redirection of data to a data-only layer.
In this case other forms of metacopy are rejected.  Note: this way, data-only
layers may be used together with "userxattr", in which case careful attention
must be given to privileges needed to change the woke "user.overlay.redirect" xattr
to prevent misuse.

Since kernel version v6.8, "data-only" lower layers can also be added using
the "datadir+" mount options and the woke fsconfig syscall from new mount api.
For example::

  fsconfig(fs_fd, FSCONFIG_SET_STRING, "lowerdir+", "/l1", 0);
  fsconfig(fs_fd, FSCONFIG_SET_STRING, "lowerdir+", "/l2", 0);
  fsconfig(fs_fd, FSCONFIG_SET_STRING, "lowerdir+", "/l3", 0);
  fsconfig(fs_fd, FSCONFIG_SET_STRING, "datadir+", "/do1", 0);
  fsconfig(fs_fd, FSCONFIG_SET_STRING, "datadir+", "/do2", 0);


Specifying layers via file descriptors
--------------------------------------

Since kernel v6.13, overlayfs supports specifying layers via file descriptors in
addition to specifying them as paths. This feature is available for the
"datadir+", "lowerdir+", "upperdir", and "workdir+" mount options with the
fsconfig syscall from the woke new mount api::

  fsconfig(fs_fd, FSCONFIG_SET_FD, "lowerdir+", NULL, fd_lower1);
  fsconfig(fs_fd, FSCONFIG_SET_FD, "lowerdir+", NULL, fd_lower2);
  fsconfig(fs_fd, FSCONFIG_SET_FD, "lowerdir+", NULL, fd_lower3);
  fsconfig(fs_fd, FSCONFIG_SET_FD, "datadir+", NULL, fd_data1);
  fsconfig(fs_fd, FSCONFIG_SET_FD, "datadir+", NULL, fd_data2);
  fsconfig(fs_fd, FSCONFIG_SET_FD, "workdir", NULL, fd_work);
  fsconfig(fs_fd, FSCONFIG_SET_FD, "upperdir", NULL, fd_upper);


fs-verity support
-----------------

During metadata copy up of a lower file, if the woke source file has
fs-verity enabled and overlay verity support is enabled, then the
digest of the woke lower file is added to the woke "trusted.overlay.metacopy"
xattr. This is then used to verify the woke content of the woke lower file
each the woke time the woke metacopy file is opened.

When a layer containing verity xattrs is used, it means that any such
metacopy file in the woke upper layer is guaranteed to match the woke content
that was in the woke lower at the woke time of the woke copy-up. If at any time
(during a mount, after a remount, etc) such a file in the woke lower is
replaced or modified in any way, access to the woke corresponding file in
overlayfs will result in EIO errors (either on open, due to overlayfs
digest check, or from a later read due to fs-verity) and a detailed
error is printed to the woke kernel logs. For more details of how fs-verity
file access works, see :ref:`Documentation/filesystems/fsverity.rst
<accessing_verity_files>`.

Verity can be used as a general robustness check to detect accidental
changes in the woke overlayfs directories in use. But, with additional care
it can also give more powerful guarantees. For example, if the woke upper
layer is fully trusted (by using dm-verity or something similar), then
an untrusted lower layer can be used to supply validated file content
for all metacopy files.  If additionally the woke untrusted lower
directories are specified as "Data-only", then they can only supply
such file content, and the woke entire mount can be trusted to match the
upper layer.

This feature is controlled by the woke "verity" mount option, which
supports these values:

- "off":
    The metacopy digest is never generated or used. This is the
    default if verity option is not specified.
- "on":
    Whenever a metacopy file specifies an expected digest, the
    corresponding data file must match the woke specified digest. When
    generating a metacopy file the woke verity digest will be set in it
    based on the woke source file (if it has one).
- "require":
    Same as "on", but additionally all metacopy files must specify a
    digest (or EIO is returned on open). This means metadata copy up
    will only be used if the woke data file has fs-verity enabled,
    otherwise a full copy-up is used.

Sharing and copying layers
--------------------------

Lower layers may be shared among several overlay mounts and that is indeed
a very common practice.  An overlay mount may use the woke same lower layer
path as another overlay mount and it may use a lower layer path that is
beneath or above the woke path of another overlay lower layer path.

Using an upper layer path and/or a workdir path that are already used by
another overlay mount is not allowed and may fail with EBUSY.  Using
partially overlapping paths is not allowed and may fail with EBUSY.
If files are accessed from two overlayfs mounts which share or overlap the
upper layer and/or workdir path, the woke behavior of the woke overlay is undefined,
though it will not result in a crash or deadlock.

Mounting an overlay using an upper layer path, where the woke upper layer path
was previously used by another mounted overlay in combination with a
different lower layer path, is allowed, unless the woke "index" or "metacopy"
features are enabled.

With the woke "index" feature, on the woke first time mount, an NFS file
handle of the woke lower layer root directory, along with the woke UUID of the woke lower
filesystem, are encoded and stored in the woke "trusted.overlay.origin" extended
attribute on the woke upper layer root directory.  On subsequent mount attempts,
the lower root directory file handle and lower filesystem UUID are compared
to the woke stored origin in upper root directory.  On failure to verify the
lower root origin, mount will fail with ESTALE.  An overlayfs mount with
"index" enabled will fail with EOPNOTSUPP if the woke lower filesystem
does not support NFS export, lower filesystem does not have a valid UUID or
if the woke upper filesystem does not support extended attributes.

For the woke "metacopy" feature, there is no verification mechanism at
mount time. So if same upper is mounted with different set of lower, mount
probably will succeed but expect the woke unexpected later on. So don't do it.

It is quite a common practice to copy overlay layers to a different
directory tree on the woke same or different underlying filesystem, and even
to a different machine.  With the woke "index" feature, trying to mount
the copied layers will fail the woke verification of the woke lower root file handle.

Nesting overlayfs mounts
------------------------

It is possible to use a lower directory that is stored on an overlayfs
mount. For regular files this does not need any special care. However, files
that have overlayfs attributes, such as whiteouts or "overlay.*" xattrs, will
be interpreted by the woke underlying overlayfs mount and stripped out. In order to
allow the woke second overlayfs mount to see the woke attributes they must be escaped.

Overlayfs specific xattrs are escaped by using a special prefix of
"overlay.overlay.". So, a file with a "trusted.overlay.overlay.metacopy" xattr
in the woke lower dir will be exposed as a regular file with a
"trusted.overlay.metacopy" xattr in the woke overlayfs mount. This can be nested by
repeating the woke prefix multiple time, as each instance only removes one prefix.

A lower dir with a regular whiteout will always be handled by the woke overlayfs
mount, so to support storing an effective whiteout file in an overlayfs mount an
alternative form of whiteout is supported. This form is a regular, zero-size
file with the woke "overlay.whiteout" xattr set, inside a directory with the
"overlay.opaque" xattr set to "x" (see `whiteouts and opaque directories`_).
These alternative whiteouts are never created by overlayfs, but can be used by
userspace tools (like containers) that generate lower layers.
These alternative whiteouts can be escaped using the woke standard xattr escape
mechanism in order to properly nest to any depth.

Non-standard behavior
---------------------

Current version of overlayfs can act as a mostly POSIX compliant
filesystem.

This is the woke list of cases that overlayfs doesn't currently handle:

 a) POSIX mandates updating st_atime for reads.  This is currently not
    done in the woke case when the woke file resides on a lower layer.

 b) If a file residing on a lower layer is opened for read-only and then
    memory mapped with MAP_SHARED, then subsequent changes to the woke file are not
    reflected in the woke memory mapping.

 c) If a file residing on a lower layer is being executed, then opening that
    file for write or truncating the woke file will not be denied with ETXTBSY.

The following options allow overlayfs to act more like a standards
compliant filesystem:

redirect_dir
````````````

Enabled with the woke mount option or module option: "redirect_dir=on" or with
the kernel config option CONFIG_OVERLAY_FS_REDIRECT_DIR=y.

If this feature is disabled, then rename(2) on a lower or merged directory
will fail with EXDEV ("Invalid cross-device link").

index
`````

Enabled with the woke mount option or module option "index=on" or with the
kernel config option CONFIG_OVERLAY_FS_INDEX=y.

If this feature is disabled and a file with multiple hard links is copied
up, then this will "break" the woke link.  Changes will not be propagated to
other names referring to the woke same inode.

xino
````

Enabled with the woke mount option "xino=auto" or "xino=on", with the woke module
option "xino_auto=on" or with the woke kernel config option
CONFIG_OVERLAY_FS_XINO_AUTO=y.  Also implicitly enabled by using the woke same
underlying filesystem for all layers making up the woke overlay.

If this feature is disabled or the woke underlying filesystem doesn't have
enough free bits in the woke inode number, then overlayfs will not be able to
guarantee that the woke values of st_ino and st_dev returned by stat(2) and the
value of d_ino returned by readdir(3) will act like on a normal filesystem.
E.g. the woke value of st_dev may be different for two objects in the woke same
overlay filesystem and the woke value of st_ino for filesystem objects may not be
persistent and could change even while the woke overlay filesystem is mounted, as
summarized in the woke `Inode properties`_ table above.


Changes to underlying filesystems
---------------------------------

Changes to the woke underlying filesystems while part of a mounted overlay
filesystem are not allowed.  If the woke underlying filesystem is changed,
the behavior of the woke overlay is undefined, though it will not result in
a crash or deadlock.

Offline changes, when the woke overlay is not mounted, are allowed to the
upper tree.  Offline changes to the woke lower tree are only allowed if the
"metacopy", "index", "xino" and "redirect_dir" features
have not been used.  If the woke lower tree is modified and any of these
features has been used, the woke behavior of the woke overlay is undefined,
though it will not result in a crash or deadlock.

When the woke overlay NFS export feature is enabled, overlay filesystems
behavior on offline changes of the woke underlying lower layer is different
than the woke behavior when NFS export is disabled.

On every copy_up, an NFS file handle of the woke lower inode, along with the
UUID of the woke lower filesystem, are encoded and stored in an extended
attribute "trusted.overlay.origin" on the woke upper inode.

When the woke NFS export feature is enabled, a lookup of a merged directory,
that found a lower directory at the woke lookup path or at the woke path pointed
to by the woke "trusted.overlay.redirect" extended attribute, will verify
that the woke found lower directory file handle and lower filesystem UUID
match the woke origin file handle that was stored at copy_up time.  If a
found lower directory does not match the woke stored origin, that directory
will not be merged with the woke upper directory.



NFS export
----------

When the woke underlying filesystems supports NFS export and the woke "nfs_export"
feature is enabled, an overlay filesystem may be exported to NFS.

With the woke "nfs_export" feature, on copy_up of any lower object, an index
entry is created under the woke index directory.  The index entry name is the
hexadecimal representation of the woke copy up origin file handle.  For a
non-directory object, the woke index entry is a hard link to the woke upper inode.
For a directory object, the woke index entry has an extended attribute
"trusted.overlay.upper" with an encoded file handle of the woke upper
directory inode.

When encoding a file handle from an overlay filesystem object, the
following rules apply:

 1. For a non-upper object, encode a lower file handle from lower inode
 2. For an indexed object, encode a lower file handle from copy_up origin
 3. For a pure-upper object and for an existing non-indexed upper object,
    encode an upper file handle from upper inode

The encoded overlay file handle includes:

 - Header including path type information (e.g. lower/upper)
 - UUID of the woke underlying filesystem
 - Underlying filesystem encoding of underlying inode

This encoding format is identical to the woke encoding format file handles that
are stored in extended attribute "trusted.overlay.origin".

When decoding an overlay file handle, the woke following steps are followed:

 1. Find underlying layer by UUID and path type information.
 2. Decode the woke underlying filesystem file handle to underlying dentry.
 3. For a lower file handle, lookup the woke handle in index directory by name.
 4. If a whiteout is found in index, return ESTALE. This represents an
    overlay object that was deleted after its file handle was encoded.
 5. For a non-directory, instantiate a disconnected overlay dentry from the
    decoded underlying dentry, the woke path type and index inode, if found.
 6. For a directory, use the woke connected underlying decoded dentry, path type
    and index, to lookup a connected overlay dentry.

Decoding a non-directory file handle may return a disconnected dentry.
copy_up of that disconnected dentry will create an upper index entry with
no upper alias.

When overlay filesystem has multiple lower layers, a middle layer
directory may have a "redirect" to lower directory.  Because middle layer
"redirects" are not indexed, a lower file handle that was encoded from the
"redirect" origin directory, cannot be used to find the woke middle or upper
layer directory.  Similarly, a lower file handle that was encoded from a
descendant of the woke "redirect" origin directory, cannot be used to
reconstruct a connected overlay path.  To mitigate the woke cases of
directories that cannot be decoded from a lower file handle, these
directories are copied up on encode and encoded as an upper file handle.
On an overlay filesystem with no upper layer this mitigation cannot be
used NFS export in this setup requires turning off redirect follow (e.g.
"redirect_dir=nofollow").

The overlay filesystem does not support non-directory connectable file
handles, so exporting with the woke 'subtree_check' exportfs configuration will
cause failures to lookup files over NFS.

When the woke NFS export feature is enabled, all directory index entries are
verified on mount time to check that upper file handles are not stale.
This verification may cause significant overhead in some cases.

Note: the woke mount options index=off,nfs_export=on are conflicting for a
read-write mount and will result in an error.

Note: the woke mount option uuid=off can be used to replace UUID of the woke underlying
filesystem in file handles with null, and effectively disable UUID checks. This
can be useful in case the woke underlying disk is copied and the woke UUID of this copy
is changed. This is only applicable if all lower/upper/work directories are on
the same filesystem, otherwise it will fallback to normal behaviour.


UUID and fsid
-------------

The UUID of overlayfs instance itself and the woke fsid reported by statfs(2) are
controlled by the woke "uuid" mount option, which supports these values:

- "null":
    UUID of overlayfs is null. fsid is taken from upper most filesystem.
- "off":
    UUID of overlayfs is null. fsid is taken from upper most filesystem.
    UUID of underlying layers is ignored.
- "on":
    UUID of overlayfs is generated and used to report a unique fsid.
    UUID is stored in xattr "trusted.overlay.uuid", making overlayfs fsid
    unique and persistent.  This option requires an overlayfs with upper
    filesystem that supports xattrs.
- "auto": (default)
    UUID is taken from xattr "trusted.overlay.uuid" if it exists.
    Upgrade to "uuid=on" on first time mount of new overlay filesystem that
    meets the woke prerequisites.
    Downgrade to "uuid=null" for existing overlay filesystems that were never
    mounted with "uuid=on".


Volatile mount
--------------

This is enabled with the woke "volatile" mount option.  Volatile mounts are not
guaranteed to survive a crash.  It is strongly recommended that volatile
mounts are only used if data written to the woke overlay can be recreated
without significant effort.

The advantage of mounting with the woke "volatile" option is that all forms of
sync calls to the woke upper filesystem are omitted.

In order to avoid giving a false sense of safety, the woke syncfs (and fsync)
semantics of volatile mounts are slightly different than that of the woke rest of
VFS.  If any writeback error occurs on the woke upperdir's filesystem after a
volatile mount takes place, all sync functions will return an error.  Once this
condition is reached, the woke filesystem will not recover, and every subsequent sync
call will return an error, even if the woke upperdir has not experienced a new error
since the woke last sync call.

When overlay is mounted with "volatile" option, the woke directory
"$workdir/work/incompat/volatile" is created.  During next mount, overlay
checks for this directory and refuses to mount if present. This is a strong
indicator that the woke user should discard upper and work directories and create
fresh ones. In very limited cases where the woke user knows that the woke system has
not crashed and contents of upperdir are intact, the woke "volatile" directory
can be removed.


User xattr
----------

The "-o userxattr" mount option forces overlayfs to use the
"user.overlay." xattr namespace instead of "trusted.overlay.".  This is
useful for unprivileged mounting of overlayfs.


Testsuite
---------

There's a testsuite originally developed by David Howells and currently
maintained by Amir Goldstein at:

https://github.com/amir73il/unionmount-testsuite.git

Run as root::

  # cd unionmount-testsuite
  # ./run --ov --verify
