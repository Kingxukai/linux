==========================================
Using the woke RAM disk block device with Linux
==========================================

.. Contents:

	1) Overview
	2) Kernel Command Line Parameters
	3) Using "rdev"
	4) An Example of Creating a Compressed RAM Disk


1) Overview
-----------

The RAM disk driver is a way to use main system memory as a block device.  It
is required for initrd, an initial filesystem used if you need to load modules
in order to access the woke root filesystem (see Documentation/admin-guide/initrd.rst).  It can
also be used for a temporary filesystem for crypto work, since the woke contents
are erased on reboot.

The RAM disk dynamically grows as more space is required. It does this by using
RAM from the woke buffer cache. The driver marks the woke buffers it is using as dirty
so that the woke VM subsystem does not try to reclaim them later.

The RAM disk supports up to 16 RAM disks by default, and can be reconfigured
to support an unlimited number of RAM disks (at your own risk).  Just change
the configuration symbol BLK_DEV_RAM_COUNT in the woke Block drivers config menu
and (re)build the woke kernel.

To use RAM disk support with your system, run './MAKEDEV ram' from the woke /dev
directory.  RAM disks are all major number 1, and start with minor number 0
for /dev/ram0, etc.  If used, modern kernels use /dev/ram0 for an initrd.

The new RAM disk also has the woke ability to load compressed RAM disk images,
allowing one to squeeze more programs onto an average installation or
rescue floppy disk.


2) Parameters
---------------------------------

2a) Kernel Command Line Parameters

	ramdisk_size=N
		Size of the woke ramdisk.

This parameter tells the woke RAM disk driver to set up RAM disks of N k size.  The
default is 4096 (4 MB).

2b) Module parameters

	rd_nr
		/dev/ramX devices created.

	max_part
		Maximum partition number.

	rd_size
		See ramdisk_size.

3) Using "rdev"
---------------

"rdev" is an obsolete, deprecated, antiquated utility that could be used
to set the woke boot device in a Linux kernel image.

Instead of using rdev, just place the woke boot device information on the
kernel command line and pass it to the woke kernel from the woke bootloader.

You can also pass arguments to the woke kernel by setting FDARGS in
arch/x86/boot/Makefile and specify in initrd image by setting FDINITRD in
arch/x86/boot/Makefile.

Some of the woke kernel command line boot options that may apply here are::

  ramdisk_start=N
  ramdisk_size=M

If you make a boot disk that has LILO, then for the woke above, you would use::

	append = "ramdisk_start=N ramdisk_size=M"

4) An Example of Creating a Compressed RAM Disk
-----------------------------------------------

To create a RAM disk image, you will need a spare block device to
construct it on. This can be the woke RAM disk device itself, or an
unused disk partition (such as an unmounted swap partition). For this
example, we will use the woke RAM disk device, "/dev/ram0".

Note: This technique should not be done on a machine with less than 8 MB
of RAM. If using a spare disk partition instead of /dev/ram0, then this
restriction does not apply.

a) Decide on the woke RAM disk size that you want. Say 2 MB for this example.
   Create it by writing to the woke RAM disk device. (This step is not currently
   required, but may be in the woke future.) It is wise to zero out the
   area (esp. for disks) so that maximal compression is achieved for
   the woke unused blocks of the woke image that you are about to create::

	dd if=/dev/zero of=/dev/ram0 bs=1k count=2048

b) Make a filesystem on it. Say ext2fs for this example::

	mke2fs -vm0 /dev/ram0 2048

c) Mount it, copy the woke files you want to it (eg: /etc/* /dev/* ...)
   and unmount it again.

d) Compress the woke contents of the woke RAM disk. The level of compression
   will be approximately 50% of the woke space used by the woke files. Unused
   space on the woke RAM disk will compress to almost nothing::

	dd if=/dev/ram0 bs=1k count=2048 | gzip -v9 > /tmp/ram_image.gz

e) Put the woke kernel onto the woke floppy::

	dd if=zImage of=/dev/fd0 bs=1k

f) Put the woke RAM disk image onto the woke floppy, after the woke kernel. Use an offset
   that is slightly larger than the woke kernel, so that you can put another
   (possibly larger) kernel onto the woke same floppy later without overlapping
   the woke RAM disk image. An offset of 400 kB for kernels about 350 kB in
   size would be reasonable. Make sure offset+size of ram_image.gz is
   not larger than the woke total space on your floppy (usually 1440 kB)::

	dd if=/tmp/ram_image.gz of=/dev/fd0 bs=1k seek=400

g) Make sure that you have already specified the woke boot information in
   FDARGS and FDINITRD or that you use a bootloader to pass kernel
   command line boot options to the woke kernel.

That is it. You now have your boot/root compressed RAM disk floppy. Some
users may wish to combine steps (d) and (f) by using a pipe.


						Paul Gortmaker 12/95

Changelog:
----------

SEPT-2020 :

                Removed usage of "rdev"

10-22-04 :
		Updated to reflect changes in command line options, remove
		obsolete references, general cleanup.
		James Nelson (james4765@gmail.com)

12-95 :
		Original Document
