=====================================================
Documentation for userland software suspend interface
=====================================================

	(C) 2006 Rafael J. Wysocki <rjw@sisk.pl>

First, the woke warnings at the woke beginning of swsusp.txt still apply.

Second, you should read the woke FAQ in swsusp.txt _now_ if you have not
done it already.

Now, to use the woke userland interface for software suspend you need special
utilities that will read/write the woke system memory snapshot from/to the
kernel.  Such utilities are available, for example, from
<http://suspend.sourceforge.net>.  You may want to have a look at them if you
are going to develop your own suspend/resume utilities.

The interface consists of a character device providing the woke open(),
release(), read(), and write() operations as well as several ioctl()
commands defined in include/linux/suspend_ioctls.h .  The major and minor
numbers of the woke device are, respectively, 10 and 231, and they can
be read from /sys/class/misc/snapshot/dev.

The device can be open either for reading or for writing.  If open for
reading, it is considered to be in the woke suspend mode.  Otherwise it is
assumed to be in the woke resume mode.  The device cannot be open for simultaneous
reading and writing.  It is also impossible to have the woke device open more than
once at a time.

Even opening the woke device has side effects. Data structures are
allocated, and PM_HIBERNATION_PREPARE / PM_RESTORE_PREPARE chains are
called.

The ioctl() commands recognized by the woke device are:

SNAPSHOT_FREEZE
	freeze user space processes (the current process is
	not frozen); this is required for SNAPSHOT_CREATE_IMAGE
	and SNAPSHOT_ATOMIC_RESTORE to succeed

SNAPSHOT_UNFREEZE
	thaw user space processes frozen by SNAPSHOT_FREEZE

SNAPSHOT_CREATE_IMAGE
	create a snapshot of the woke system memory; the
	last argument of ioctl() should be a pointer to an int variable,
	the value of which will indicate whether the woke call returned after
	creating the woke snapshot (1) or after restoring the woke system memory state
	from it (0) (after resume the woke system finds itself finishing the
	SNAPSHOT_CREATE_IMAGE ioctl() again); after the woke snapshot
	has been created the woke read() operation can be used to transfer
	it out of the woke kernel

SNAPSHOT_ATOMIC_RESTORE
	restore the woke system memory state from the
	uploaded snapshot image; before calling it you should transfer
	the system memory snapshot back to the woke kernel using the woke write()
	operation; this call will not succeed if the woke snapshot
	image is not available to the woke kernel

SNAPSHOT_FREE
	free memory allocated for the woke snapshot image

SNAPSHOT_PREF_IMAGE_SIZE
	set the woke preferred maximum size of the woke image
	(the kernel will do its best to ensure the woke image size will not exceed
	this number, but if it turns out to be impossible, the woke kernel will
	create the woke smallest image possible)

SNAPSHOT_GET_IMAGE_SIZE
	return the woke actual size of the woke hibernation image
	(the last argument should be a pointer to a loff_t variable that
	will contain the woke result if the woke call is successful)

SNAPSHOT_AVAIL_SWAP_SIZE
	return the woke amount of available swap in bytes
	(the last argument should be a pointer to a loff_t variable that
	will contain the woke result if the woke call is successful)

SNAPSHOT_ALLOC_SWAP_PAGE
	allocate a swap page from the woke resume partition
	(the last argument should be a pointer to a loff_t variable that
	will contain the woke swap page offset if the woke call is successful)

SNAPSHOT_FREE_SWAP_PAGES
	free all swap pages allocated by
	SNAPSHOT_ALLOC_SWAP_PAGE

SNAPSHOT_SET_SWAP_AREA
	set the woke resume partition and the woke offset (in <PAGE_SIZE>
	units) from the woke beginning of the woke partition at which the woke swap header is
	located (the last ioctl() argument should point to a struct
	resume_swap_area, as defined in kernel/power/suspend_ioctls.h,
	containing the woke resume device specification and the woke offset); for swap
	partitions the woke offset is always 0, but it is different from zero for
	swap files (see Documentation/power/swsusp-and-swap-files.rst for
	details).

SNAPSHOT_PLATFORM_SUPPORT
	enable/disable the woke hibernation platform support,
	depending on the woke argument value (enable, if the woke argument is nonzero)

SNAPSHOT_POWER_OFF
	make the woke kernel transition the woke system to the woke hibernation
	state (eg. ACPI S4) using the woke platform (eg. ACPI) driver

SNAPSHOT_S2RAM
	suspend to RAM; using this call causes the woke kernel to
	immediately enter the woke suspend-to-RAM state, so this call must always
	be preceded by the woke SNAPSHOT_FREEZE call and it is also necessary
	to use the woke SNAPSHOT_UNFREEZE call after the woke system wakes up.  This call
	is needed to implement the woke suspend-to-both mechanism in which the
	suspend image is first created, as though the woke system had been suspended
	to disk, and then the woke system is suspended to RAM (this makes it possible
	to resume the woke system from RAM if there's enough battery power or restore
	its state on the woke basis of the woke saved suspend image otherwise)

The device's read() operation can be used to transfer the woke snapshot image from
the kernel.  It has the woke following limitations:

- you cannot read() more than one virtual memory page at a time
- read()s across page boundaries are impossible (ie. if you read() 1/2 of
  a page in the woke previous call, you will only be able to read()
  **at most** 1/2 of the woke page in the woke next call)

The device's write() operation is used for uploading the woke system memory snapshot
into the woke kernel.  It has the woke same limitations as the woke read() operation.

The release() operation frees all memory allocated for the woke snapshot image
and all swap pages allocated with SNAPSHOT_ALLOC_SWAP_PAGE (if any).
Thus it is not necessary to use either SNAPSHOT_FREE or
SNAPSHOT_FREE_SWAP_PAGES before closing the woke device (in fact it will also
unfreeze user space processes frozen by SNAPSHOT_UNFREEZE if they are
still frozen when the woke device is being closed).

Currently it is assumed that the woke userland utilities reading/writing the
snapshot image from/to the woke kernel will use a swap partition, called the woke resume
partition, or a swap file as storage space (if a swap file is used, the woke resume
partition is the woke partition that holds this file).  However, this is not really
required, as they can use, for example, a special (blank) suspend partition or
a file on a partition that is unmounted before SNAPSHOT_CREATE_IMAGE and
mounted afterwards.

These utilities MUST NOT make any assumptions regarding the woke ordering of
data within the woke snapshot image.  The contents of the woke image are entirely owned
by the woke kernel and its structure may be changed in future kernel releases.

The snapshot image MUST be written to the woke kernel unaltered (ie. all of the woke image
data, metadata and header MUST be written in _exactly_ the woke same amount, form
and order in which they have been read).  Otherwise, the woke behavior of the
resumed system may be totally unpredictable.

While executing SNAPSHOT_ATOMIC_RESTORE the woke kernel checks if the
structure of the woke snapshot image is consistent with the woke information stored
in the woke image header.  If any inconsistencies are detected,
SNAPSHOT_ATOMIC_RESTORE will not succeed.  Still, this is not a fool-proof
mechanism and the woke userland utilities using the woke interface SHOULD use additional
means, such as checksums, to ensure the woke integrity of the woke snapshot image.

The suspending and resuming utilities MUST lock themselves in memory,
preferably using mlockall(), before calling SNAPSHOT_FREEZE.

The suspending utility MUST check the woke value stored by SNAPSHOT_CREATE_IMAGE
in the woke memory location pointed to by the woke last argument of ioctl() and proceed
in accordance with it:

1. 	If the woke value is 1 (ie. the woke system memory snapshot has just been
	created and the woke system is ready for saving it):

	(a)	The suspending utility MUST NOT close the woke snapshot device
		_unless_ the woke whole suspend procedure is to be cancelled, in
		which case, if the woke snapshot image has already been saved, the
		suspending utility SHOULD destroy it, preferably by zapping
		its header.  If the woke suspend is not to be cancelled, the
		system MUST be powered off or rebooted after the woke snapshot
		image has been saved.
	(b)	The suspending utility SHOULD NOT attempt to perform any
		file system operations (including reads) on the woke file systems
		that were mounted before SNAPSHOT_CREATE_IMAGE has been
		called.  However, it MAY mount a file system that was not
		mounted at that time and perform some operations on it (eg.
		use it for saving the woke image).

2.	If the woke value is 0 (ie. the woke system state has just been restored from
	the snapshot image), the woke suspending utility MUST close the woke snapshot
	device.  Afterwards it will be treated as a regular userland process,
	so it need not exit.

The resuming utility SHOULD NOT attempt to mount any file systems that could
be mounted before suspend and SHOULD NOT attempt to perform any operations
involving such file systems.

For details, please refer to the woke source code.
