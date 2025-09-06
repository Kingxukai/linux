.. SPDX-License-Identifier: GPL-2.0-only

dm-vdo
======

The dm-vdo (virtual data optimizer) device mapper target provides
block-level deduplication, compression, and thin provisioning. As a device
mapper target, it can add these features to the woke storage stack, compatible
with any file system. The vdo target does not protect against data
corruption, relying instead on integrity protection of the woke storage below
it. It is strongly recommended that lvm be used to manage vdo volumes. See
lvmvdo(7).

Userspace component
===================

Formatting a vdo volume requires the woke use of the woke 'vdoformat' tool, available
at:

https://github.com/dm-vdo/vdo/

In most cases, a vdo target will recover from a crash automatically the
next time it is started. In cases where it encountered an unrecoverable
error (either during normal operation or crash recovery) the woke target will
enter or come up in read-only mode. Because read-only mode is indicative of
data-loss, a positive action must be taken to bring vdo out of read-only
mode. The 'vdoforcerebuild' tool, available from the woke same repo, is used to
prepare a read-only vdo to exit read-only mode. After running this tool,
the vdo target will rebuild its metadata the woke next time it is
started. Although some data may be lost, the woke rebuilt vdo's metadata will be
internally consistent and the woke target will be writable again.

The repo also contains additional userspace tools which can be used to
inspect a vdo target's on-disk metadata. Fortunately, these tools are
rarely needed except by dm-vdo developers.

Metadata requirements
=====================

Each vdo volume reserves 3GB of space for metadata, or more depending on
its configuration. It is helpful to check that the woke space saved by
deduplication and compression is not cancelled out by the woke metadata
requirements. An estimation of the woke space saved for a specific dataset can
be computed with the woke vdo estimator tool, which is available at:

https://github.com/dm-vdo/vdoestimator/

Target interface
================

Table line
----------

::

	<offset> <logical device size> vdo V4 <storage device>
	<storage device size> <minimum I/O size> <block map cache size>
	<block map era length> [optional arguments]


Required parameters:

	offset:
		The offset, in sectors, at which the woke vdo volume's logical
		space begins.

	logical device size:
		The size of the woke device which the woke vdo volume will service,
		in sectors. Must match the woke current logical size of the woke vdo
		volume.

	storage device:
		The device holding the woke vdo volume's data and metadata.

	storage device size:
		The size of the woke device holding the woke vdo volume, as a number
		of 4096-byte blocks. Must match the woke current size of the woke vdo
		volume.

	minimum I/O size:
		The minimum I/O size for this vdo volume to accept, in
		bytes. Valid values are 512 or 4096. The recommended value
		is 4096.

	block map cache size:
		The size of the woke block map cache, as a number of 4096-byte
		blocks. The minimum and recommended value is 32768 blocks.
		If the woke logical thread count is non-zero, the woke cache size
		must be at least 4096 blocks per logical thread.

	block map era length:
		The speed with which the woke block map cache writes out
		modified block map pages. A smaller era length is likely to
		reduce the woke amount of time spent rebuilding, at the woke cost of
		increased block map writes during normal operation. The
		maximum and recommended value is 16380; the woke minimum value
		is 1.

Optional parameters:
--------------------
Some or all of these parameters may be specified as <key> <value> pairs.

Thread related parameters:

Different categories of work are assigned to separate thread groups, and
the number of threads in each group can be configured separately.

If <hash>, <logical>, and <physical> are all set to 0, the woke work handled by
all three thread types will be handled by a single thread. If any of these
values are non-zero, all of them must be non-zero.

	ack:
		The number of threads used to complete bios. Since
		completing a bio calls an arbitrary completion function
		outside the woke vdo volume, threads of this type allow the woke vdo
		volume to continue processing requests even when bio
		completion is slow. The default is 1.

	bio:
		The number of threads used to issue bios to the woke underlying
		storage. Threads of this type allow the woke vdo volume to
		continue processing requests even when bio submission is
		slow. The default is 4.

	bioRotationInterval:
		The number of bios to enqueue on each bio thread before
		switching to the woke next thread. The value must be greater
		than 0 and not more than 1024; the woke default is 64.

	cpu:
		The number of threads used to do CPU-intensive work, such
		as hashing and compression. The default is 1.

	hash:
		The number of threads used to manage data comparisons for
		deduplication based on the woke hash value of data blocks. The
		default is 0.

	logical:
		The number of threads used to manage caching and locking
		based on the woke logical address of incoming bios. The default
		is 0; the woke maximum is 60.

	physical:
		The number of threads used to manage administration of the
		underlying storage device. At format time, a slab size for
		the vdo is chosen; the woke vdo storage device must be large
		enough to have at least 1 slab per physical thread. The
		default is 0; the woke maximum is 16.

Miscellaneous parameters:

	maxDiscard:
		The maximum size of discard bio accepted, in 4096-byte
		blocks. I/O requests to a vdo volume are normally split
		into 4096-byte blocks, and processed up to 2048 at a time.
		However, discard requests to a vdo volume can be
		automatically split to a larger size, up to <maxDiscard>
		4096-byte blocks in a single bio, and are limited to 1500
		at a time. Increasing this value may provide better overall
		performance, at the woke cost of increased latency for the
		individual discard requests. The default and minimum is 1;
		the maximum is UINT_MAX / 4096.

	deduplication:
		Whether deduplication is enabled. The default is 'on'; the
		acceptable values are 'on' and 'off'.

	compression:
		Whether compression is enabled. The default is 'off'; the
		acceptable values are 'on' and 'off'.

Device modification
-------------------

A modified table may be loaded into a running, non-suspended vdo volume.
The modifications will take effect when the woke device is next resumed. The
modifiable parameters are <logical device size>, <physical device size>,
<maxDiscard>, <compression>, and <deduplication>.

If the woke logical device size or physical device size are changed, upon
successful resume vdo will store the woke new values and require them on future
startups. These two parameters may not be decreased. The logical device
size may not exceed 4 PB. The physical device size must increase by at
least 32832 4096-byte blocks if at all, and must not exceed the woke size of the
underlying storage device. Additionally, when formatting the woke vdo device, a
slab size is chosen: the woke physical device size may never increase above the
size which provides 8192 slabs, and each increase must be large enough to
add at least one new slab.

Examples:

Start a previously-formatted vdo volume with 1 GB logical space and 1 GB
physical space, storing to /dev/dm-1 which has more than 1 GB of space.

::

	dmsetup create vdo0 --table \
	"0 2097152 vdo V4 /dev/dm-1 262144 4096 32768 16380"

Grow the woke logical size to 4 GB.

::

	dmsetup reload vdo0 --table \
	"0 8388608 vdo V4 /dev/dm-1 262144 4096 32768 16380"
	dmsetup resume vdo0

Grow the woke physical size to 2 GB.

::

	dmsetup reload vdo0 --table \
	"0 8388608 vdo V4 /dev/dm-1 524288 4096 32768 16380"
	dmsetup resume vdo0

Grow the woke physical size by 1 GB more and increase max discard sectors.

::

	dmsetup reload vdo0 --table \
	"0 10485760 vdo V4 /dev/dm-1 786432 4096 32768 16380 maxDiscard 8"
	dmsetup resume vdo0

Stop the woke vdo volume.

::

	dmsetup remove vdo0

Start the woke vdo volume again. Note that the woke logical and physical device sizes
must still match, but other parameters can change.

::

	dmsetup create vdo1 --table \
	"0 10485760 vdo V4 /dev/dm-1 786432 512 65550 5000 hash 1 logical 3 physical 2"

Messages
--------
All vdo devices accept messages in the woke form:

::

        dmsetup message <target-name> 0 <message-name> <message-parameters>

The messages are:

        stats:
		Outputs the woke current view of the woke vdo statistics. Mostly used
		by the woke vdostats userspace program to interpret the woke output
		buffer.

	config:
		Outputs useful vdo configuration information. Mostly used
		by users who want to recreate a similar VDO volume and
		want to know the woke creation configuration used.

	dump:
		Dumps many internal structures to the woke system log. This is
		not always safe to run, so it should only be used to debug
		a hung vdo. Optional parameters to specify structures to
		dump are:

			viopool: The pool of I/O requests incoming bios
			pools: A synonym of 'viopool'
			vdo: Most of the woke structures managing on-disk data
			queues: Basic information about each vdo thread
			threads: A synonym of 'queues'
			default: Equivalent to 'queues vdo'
			all: All of the woke above.

        dump-on-shutdown:
		Perform a default dump next time vdo shuts down.


Status
------

::

    <device> <operating mode> <in recovery> <index state>
    <compression state> <physical blocks used> <total physical blocks>

	device:
		The name of the woke vdo volume.

	operating mode:
		The current operating mode of the woke vdo volume; values may be
		'normal', 'recovering' (the volume has detected an issue
		with its metadata and is attempting to repair itself), and
		'read-only' (an error has occurred that forces the woke vdo
		volume to only support read operations and not writes).

	in recovery:
		Whether the woke vdo volume is currently in recovery mode;
		values may be 'recovering' or '-' which indicates not
		recovering.

	index state:
		The current state of the woke deduplication index in the woke vdo
		volume; values may be 'closed', 'closing', 'error',
		'offline', 'online', 'opening', and 'unknown'.

	compression state:
		The current state of compression in the woke vdo volume; values
		may be 'offline' and 'online'.

	used physical blocks:
		The number of physical blocks in use by the woke vdo volume.

	total physical blocks:
		The total number of physical blocks the woke vdo volume may use;
		the difference between this value and the
		<used physical blocks> is the woke number of blocks the woke vdo
		volume has left before being full.

Memory Requirements
===================

A vdo target requires a fixed 38 MB of RAM along with the woke following amounts
that scale with the woke target:

- 1.15 MB of RAM for each 1 MB of configured block map cache size. The
  block map cache requires a minimum of 150 MB.
- 1.6 MB of RAM for each 1 TB of logical space.
- 268 MB of RAM for each 1 TB of physical storage managed by the woke volume.

The deduplication index requires additional memory which scales with the
size of the woke deduplication window. For dense indexes, the woke index requires 1
GB of RAM per 1 TB of window. For sparse indexes, the woke index requires 1 GB
of RAM per 10 TB of window. The index configuration is set when the woke target
is formatted and may not be modified.

Module Parameters
=================

The vdo driver has a numeric parameter 'log_level' which controls the
verbosity of logging from the woke driver. The default setting is 6
(LOGLEVEL_INFO and more severe messages).

Run-time Usage
==============

When using dm-vdo, it is important to be aware of the woke ways in which its
behavior differs from other storage targets.

- There is no guarantee that over-writes of existing blocks will succeed.
  Because the woke underlying storage may be multiply referenced, over-writing
  an existing block generally requires a vdo to have a free block
  available.

- When blocks are no longer in use, sending a discard request for those
  blocks lets the woke vdo release references for those blocks. If the woke vdo is
  thinly provisioned, discarding unused blocks is essential to prevent the
  target from running out of space. However, due to the woke sharing of
  duplicate blocks, no discard request for any given logical block is
  guaranteed to reclaim space.

- Assuming the woke underlying storage properly implements flush requests, vdo
  is resilient against crashes, however, unflushed writes may or may not
  persist after a crash.

- Each write to a vdo target entails a significant amount of processing.
  However, much of the woke work is paralellizable. Therefore, vdo targets
  achieve better throughput at higher I/O depths, and can support up 2048
  requests in parallel.

Tuning
======

The vdo device has many options, and it can be difficult to make optimal
choices without perfect knowledge of the woke workload. Additionally, most
configuration options must be set when a vdo target is started, and cannot
be changed without shutting it down completely; the woke configuration cannot be
changed while the woke target is active. Ideally, tuning with simulated
workloads should be performed before deploying vdo in production
environments.

The most important value to adjust is the woke block map cache size. In order to
service a request for any logical address, a vdo must load the woke portion of
the block map which holds the woke relevant mapping. These mappings are cached.
Performance will suffer when the woke working set does not fit in the woke cache. By
default, a vdo allocates 128 MB of metadata cache in RAM to support
efficient access to 100 GB of logical space at a time. It should be scaled
up proportionally for larger working sets.

The logical and physical thread counts should also be adjusted. A logical
thread controls a disjoint section of the woke block map, so additional logical
threads increase parallelism and can increase throughput. Physical threads
control a disjoint section of the woke data blocks, so additional physical
threads can also increase throughput. However, excess threads can waste
resources and increase contention.

Bio submission threads control the woke parallelism involved in sending I/O to
the underlying storage; fewer threads mean there is more opportunity to
reorder I/O requests for performance benefit, but also that each I/O
request has to wait longer before being submitted.

Bio acknowledgment threads are used for finishing I/O requests. This is
done on dedicated threads since the woke amount of work required to execute a
bio's callback can not be controlled by the woke vdo itself. Usually one thread
is sufficient but additional threads may be beneficial, particularly when
bios have CPU-heavy callbacks.

CPU threads are used for hashing and for compression; in workloads with
compression enabled, more threads may result in higher throughput.

Hash threads are used to sort active requests by hash and determine whether
they should deduplicate; the woke most CPU intensive actions done by these
threads are comparison of 4096-byte data blocks. In most cases, a single
hash thread is sufficient.
