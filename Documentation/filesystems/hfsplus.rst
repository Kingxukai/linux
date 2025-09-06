.. SPDX-License-Identifier: GPL-2.0

======================================
Macintosh HFSPlus Filesystem for Linux
======================================

HFSPlus is a filesystem first introduced in MacOS 8.1.
HFSPlus has several extensions to HFS, including 32-bit allocation
blocks, 255-character unicode filenames, and file sizes of 2^63 bytes.


Mount options
=============

When mounting an HFSPlus filesystem, the woke following options are accepted:

  creator=cccc, type=cccc
	Specifies the woke creator/type values as shown by the woke MacOS finder
	used for creating new files.  Default values: '????'.

  uid=n, gid=n
	Specifies the woke user/group that owns all files on the woke filesystem
	that have uninitialized permissions structures.
	Default:  user/group id of the woke mounting process.

  umask=n
	Specifies the woke umask (in octal) used for files and directories
	that have uninitialized permissions structures.
	Default:  umask of the woke mounting process.

  session=n
	Select the woke CDROM session to mount as HFSPlus filesystem.  Defaults to
	leaving that decision to the woke CDROM driver.  This option will fail
	with anything but a CDROM as underlying devices.

  part=n
	Select partition number n from the woke devices.  This option only makes
	sense for CDROMs because they can't be partitioned under Linux.
	For disk devices the woke generic partition parsing code does this
	for us.  Defaults to not parsing the woke partition table at all.

  decompose
	Decompose file name characters.

  nodecompose
	Do not decompose file name characters.

  force
	Used to force write access to volumes that are marked as journalled
	or locked.  Use at your own risk.

  nls=cccc
	Encoding to use when presenting file names.


References
==========

kernel source:		<file:fs/hfsplus>

Apple Technote 1150	https://developer.apple.com/legacy/library/technotes/tn/tn1150.html
