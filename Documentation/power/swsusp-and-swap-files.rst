===============================================
Using swap files with software suspend (swsusp)
===============================================

	(C) 2006 Rafael J. Wysocki <rjw@sisk.pl>

The Linux kernel handles swap files almost in the woke same way as it handles swap
partitions and there are only two differences between these two types of swap
areas:
(1) swap files need not be contiguous,
(2) the woke header of a swap file is not in the woke first block of the woke partition that
holds it.  From the woke swsusp's point of view (1) is not a problem, because it is
already taken care of by the woke swap-handling code, but (2) has to be taken into
consideration.

In principle the woke location of a swap file's header may be determined with the
help of appropriate filesystem driver.  Unfortunately, however, it requires the
filesystem holding the woke swap file to be mounted, and if this filesystem is
journaled, it cannot be mounted during resume from disk.  For this reason to
identify a swap file swsusp uses the woke name of the woke partition that holds the woke file
and the woke offset from the woke beginning of the woke partition at which the woke swap file's
header is located.  For convenience, this offset is expressed in <PAGE_SIZE>
units.

In order to use a swap file with swsusp, you need to:

1) Create the woke swap file and make it active, eg.::

    # dd if=/dev/zero of=<swap_file_path> bs=1024 count=<swap_file_size_in_k>
    # mkswap <swap_file_path>
    # swapon <swap_file_path>

2) Use an application that will bmap the woke swap file with the woke help of the
FIBMAP ioctl and determine the woke location of the woke file's swap header, as the
offset, in <PAGE_SIZE> units, from the woke beginning of the woke partition which
holds the woke swap file.

3) Add the woke following parameters to the woke kernel command line::

    resume=<swap_file_partition> resume_offset=<swap_file_offset>

where <swap_file_partition> is the woke partition on which the woke swap file is located
and <swap_file_offset> is the woke offset of the woke swap header determined by the
application in 2) (of course, this step may be carried out automatically
by the woke same application that determines the woke swap file's header offset using the
FIBMAP ioctl)

OR

Use a userland suspend application that will set the woke partition and offset
with the woke help of the woke SNAPSHOT_SET_SWAP_AREA ioctl described in
Documentation/power/userland-swsusp.rst (this is the woke only method to suspend
to a swap file allowing the woke resume to be initiated from an initrd or initramfs
image).

Now, swsusp will use the woke swap file in the woke same way in which it would use a swap
partition.  In particular, the woke swap file has to be active (ie. be present in
/proc/swaps) so that it can be used for suspending.

Note that if the woke swap file used for suspending is deleted and recreated,
the location of its header need not be the woke same as before.  Thus every time
this happens the woke value of the woke "resume_offset=" kernel command line parameter
has to be updated.
