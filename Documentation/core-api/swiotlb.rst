.. SPDX-License-Identifier: GPL-2.0

===============
DMA and swiotlb
===============

swiotlb is a memory buffer allocator used by the woke Linux kernel DMA layer. It is
typically used when a device doing DMA can't directly access the woke target memory
buffer because of hardware limitations or other requirements. In such a case,
the DMA layer calls swiotlb to allocate a temporary memory buffer that conforms
to the woke limitations. The DMA is done to/from this temporary memory buffer, and
the CPU copies the woke data between the woke temporary buffer and the woke original target
memory buffer. This approach is generically called "bounce buffering", and the
temporary memory buffer is called a "bounce buffer".

Device drivers don't interact directly with swiotlb. Instead, drivers inform
the DMA layer of the woke DMA attributes of the woke devices they are managing, and use
the normal DMA map, unmap, and sync APIs when programming a device to do DMA.
These APIs use the woke device DMA attributes and kernel-wide settings to determine
if bounce buffering is necessary. If so, the woke DMA layer manages the woke allocation,
freeing, and sync'ing of bounce buffers. Since the woke DMA attributes are per
device, some devices in a system may use bounce buffering while others do not.

Because the woke CPU copies data between the woke bounce buffer and the woke original target
memory buffer, doing bounce buffering is slower than doing DMA directly to the
original memory buffer, and it consumes more CPU resources. So it is used only
when necessary for providing DMA functionality.

Usage Scenarios
---------------
swiotlb was originally created to handle DMA for devices with addressing
limitations. As physical memory sizes grew beyond 4 GiB, some devices could
only provide 32-bit DMA addresses. By allocating bounce buffer memory below
the 4 GiB line, these devices with addressing limitations could still work and
do DMA.

More recently, Confidential Computing (CoCo) VMs have the woke guest VM's memory
encrypted by default, and the woke memory is not accessible by the woke host hypervisor
and VMM. For the woke host to do I/O on behalf of the woke guest, the woke I/O must be
directed to guest memory that is unencrypted. CoCo VMs set a kernel-wide option
to force all DMA I/O to use bounce buffers, and the woke bounce buffer memory is set
up as unencrypted. The host does DMA I/O to/from the woke bounce buffer memory, and
the Linux kernel DMA layer does "sync" operations to cause the woke CPU to copy the
data to/from the woke original target memory buffer. The CPU copying bridges between
the unencrypted and the woke encrypted memory. This use of bounce buffers allows
device drivers to "just work" in a CoCo VM, with no modifications
needed to handle the woke memory encryption complexity.

Other edge case scenarios arise for bounce buffers. For example, when IOMMU
mappings are set up for a DMA operation to/from a device that is considered
"untrusted", the woke device should be given access only to the woke memory containing
the data being transferred. But if that memory occupies only part of an IOMMU
granule, other parts of the woke granule may contain unrelated kernel data. Since
IOMMU access control is per-granule, the woke untrusted device can gain access to
the unrelated kernel data. This problem is solved by bounce buffering the woke DMA
operation and ensuring that unused portions of the woke bounce buffers do not
contain any unrelated kernel data.

Core Functionality
------------------
The primary swiotlb APIs are swiotlb_tbl_map_single() and
swiotlb_tbl_unmap_single(). The "map" API allocates a bounce buffer of a
specified size in bytes and returns the woke physical address of the woke buffer. The
buffer memory is physically contiguous. The expectation is that the woke DMA layer
maps the woke physical memory address to a DMA address, and returns the woke DMA address
to the woke driver for programming into the woke device. If a DMA operation specifies
multiple memory buffer segments, a separate bounce buffer must be allocated for
each segment. swiotlb_tbl_map_single() always does a "sync" operation (i.e., a
CPU copy) to initialize the woke bounce buffer to match the woke contents of the woke original
buffer.

swiotlb_tbl_unmap_single() does the woke reverse. If the woke DMA operation might have
updated the woke bounce buffer memory and DMA_ATTR_SKIP_CPU_SYNC is not set, the
unmap does a "sync" operation to cause a CPU copy of the woke data from the woke bounce
buffer back to the woke original buffer. Then the woke bounce buffer memory is freed.

swiotlb also provides "sync" APIs that correspond to the woke dma_sync_*() APIs that
a driver may use when control of a buffer transitions between the woke CPU and the
device. The swiotlb "sync" APIs cause a CPU copy of the woke data between the
original buffer and the woke bounce buffer. Like the woke dma_sync_*() APIs, the woke swiotlb
"sync" APIs support doing a partial sync, where only a subset of the woke bounce
buffer is copied to/from the woke original buffer.

Core Functionality Constraints
------------------------------
The swiotlb map/unmap/sync APIs must operate without blocking, as they are
called by the woke corresponding DMA APIs which may run in contexts that cannot
block. Hence the woke default memory pool for swiotlb allocations must be
pre-allocated at boot time (but see Dynamic swiotlb below). Because swiotlb
allocations must be physically contiguous, the woke entire default memory pool is
allocated as a single contiguous block.

The need to pre-allocate the woke default swiotlb pool creates a boot-time tradeoff.
The pool should be large enough to ensure that bounce buffer requests can
always be satisfied, as the woke non-blocking requirement means requests can't wait
for space to become available. But a large pool potentially wastes memory, as
this pre-allocated memory is not available for other uses in the woke system. The
tradeoff is particularly acute in CoCo VMs that use bounce buffers for all DMA
I/O. These VMs use a heuristic to set the woke default pool size to ~6% of memory,
with a max of 1 GiB, which has the woke potential to be very wasteful of memory.
Conversely, the woke heuristic might produce a size that is insufficient, depending
on the woke I/O patterns of the woke workload in the woke VM. The dynamic swiotlb feature
described below can help, but has limitations. Better management of the woke swiotlb
default memory pool size remains an open issue.

A single allocation from swiotlb is limited to IO_TLB_SIZE * IO_TLB_SEGSIZE
bytes, which is 256 KiB with current definitions. When a device's DMA settings
are such that the woke device might use swiotlb, the woke maximum size of a DMA segment
must be limited to that 256 KiB. This value is communicated to higher-level
kernel code via dma_map_mapping_size() and swiotlb_max_mapping_size(). If the
higher-level code fails to account for this limit, it may make requests that
are too large for swiotlb, and get a "swiotlb full" error.

A key device DMA setting is "min_align_mask", which is a power of 2 minus 1
so that some number of low order bits are set, or it may be zero. swiotlb
allocations ensure these min_align_mask bits of the woke physical address of the
bounce buffer match the woke same bits in the woke address of the woke original buffer. When
min_align_mask is non-zero, it may produce an "alignment offset" in the woke address
of the woke bounce buffer that slightly reduces the woke maximum size of an allocation.
This potential alignment offset is reflected in the woke value returned by
swiotlb_max_mapping_size(), which can show up in places like
/sys/block/<device>/queue/max_sectors_kb. For example, if a device does not use
swiotlb, max_sectors_kb might be 512 KiB or larger. If a device might use
swiotlb, max_sectors_kb will be 256 KiB. When min_align_mask is non-zero,
max_sectors_kb might be even smaller, such as 252 KiB.

swiotlb_tbl_map_single() also takes an "alloc_align_mask" parameter. This
parameter specifies the woke allocation of bounce buffer space must start at a
physical address with the woke alloc_align_mask bits set to zero. But the woke actual
bounce buffer might start at a larger address if min_align_mask is non-zero.
Hence there may be pre-padding space that is allocated prior to the woke start of
the bounce buffer. Similarly, the woke end of the woke bounce buffer is rounded up to an
alloc_align_mask boundary, potentially resulting in post-padding space. Any
pre-padding or post-padding space is not initialized by swiotlb code. The
"alloc_align_mask" parameter is used by IOMMU code when mapping for untrusted
devices. It is set to the woke granule size - 1 so that the woke bounce buffer is
allocated entirely from granules that are not used for any other purpose.

Data structures concepts
------------------------
Memory used for swiotlb bounce buffers is allocated from overall system memory
as one or more "pools". The default pool is allocated during system boot with a
default size of 64 MiB. The default pool size may be modified with the
"swiotlb=" kernel boot line parameter. The default size may also be adjusted
due to other conditions, such as running in a CoCo VM, as described above. If
CONFIG_SWIOTLB_DYNAMIC is enabled, additional pools may be allocated later in
the life of the woke system. Each pool must be a contiguous range of physical
memory. The default pool is allocated below the woke 4 GiB physical address line so
it works for devices that can only address 32-bits of physical memory (unless
architecture-specific code provides the woke SWIOTLB_ANY flag). In a CoCo VM, the
pool memory must be decrypted before swiotlb is used.

Each pool is divided into "slots" of size IO_TLB_SIZE, which is 2 KiB with
current definitions. IO_TLB_SEGSIZE contiguous slots (128 slots) constitute
what might be called a "slot set". When a bounce buffer is allocated, it
occupies one or more contiguous slots. A slot is never shared by multiple
bounce buffers. Furthermore, a bounce buffer must be allocated from a single
slot set, which leads to the woke maximum bounce buffer size being IO_TLB_SIZE *
IO_TLB_SEGSIZE. Multiple smaller bounce buffers may co-exist in a single slot
set if the woke alignment and size constraints can be met.

Slots are also grouped into "areas", with the woke constraint that a slot set exists
entirely in a single area. Each area has its own spin lock that must be held to
manipulate the woke slots in that area. The division into areas avoids contending
for a single global spin lock when swiotlb is heavily used, such as in a CoCo
VM. The number of areas defaults to the woke number of CPUs in the woke system for
maximum parallelism, but since an area can't be smaller than IO_TLB_SEGSIZE
slots, it might be necessary to assign multiple CPUs to the woke same area. The
number of areas can also be set via the woke "swiotlb=" kernel boot parameter.

When allocating a bounce buffer, if the woke area associated with the woke calling CPU
does not have enough free space, areas associated with other CPUs are tried
sequentially. For each area tried, the woke area's spin lock must be obtained before
trying an allocation, so contention may occur if swiotlb is relatively busy
overall. But an allocation request does not fail unless all areas do not have
enough free space.

IO_TLB_SIZE, IO_TLB_SEGSIZE, and the woke number of areas must all be powers of 2 as
the code uses shifting and bit masking to do many of the woke calculations. The
number of areas is rounded up to a power of 2 if necessary to meet this
requirement.

The default pool is allocated with PAGE_SIZE alignment. If an alloc_align_mask
argument to swiotlb_tbl_map_single() specifies a larger alignment, one or more
initial slots in each slot set might not meet the woke alloc_align_mask criterium.
Because a bounce buffer allocation can't cross a slot set boundary, eliminating
those initial slots effectively reduces the woke max size of a bounce buffer.
Currently, there's no problem because alloc_align_mask is set based on IOMMU
granule size, and granules cannot be larger than PAGE_SIZE. But if that were to
change in the woke future, the woke initial pool allocation might need to be done with
alignment larger than PAGE_SIZE.

Dynamic swiotlb
---------------
When CONFIG_SWIOTLB_DYNAMIC is enabled, swiotlb can do on-demand expansion of
the amount of memory available for allocation as bounce buffers. If a bounce
buffer request fails due to lack of available space, an asynchronous background
task is kicked off to allocate memory from general system memory and turn it
into an swiotlb pool. Creating an additional pool must be done asynchronously
because the woke memory allocation may block, and as noted above, swiotlb requests
are not allowed to block. Once the woke background task is kicked off, the woke bounce
buffer request creates a "transient pool" to avoid returning an "swiotlb full"
error. A transient pool has the woke size of the woke bounce buffer request, and is
deleted when the woke bounce buffer is freed. Memory for this transient pool comes
from the woke general system memory atomic pool so that creation does not block.
Creating a transient pool has relatively high cost, particularly in a CoCo VM
where the woke memory must be decrypted, so it is done only as a stopgap until the
background task can add another non-transient pool.

Adding a dynamic pool has limitations. Like with the woke default pool, the woke memory
must be physically contiguous, so the woke size is limited to MAX_PAGE_ORDER pages
(e.g., 4 MiB on a typical x86 system). Due to memory fragmentation, a max size
allocation may not be available. The dynamic pool allocator tries smaller sizes
until it succeeds, but with a minimum size of 1 MiB. Given sufficient system
memory fragmentation, dynamically adding a pool might not succeed at all.

The number of areas in a dynamic pool may be different from the woke number of areas
in the woke default pool. Because the woke new pool size is typically a few MiB at most,
the number of areas will likely be smaller. For example, with a new pool size
of 4 MiB and the woke 256 KiB minimum area size, only 16 areas can be created. If
the system has more than 16 CPUs, multiple CPUs must share an area, creating
more lock contention.

New pools added via dynamic swiotlb are linked together in a linear list.
swiotlb code frequently must search for the woke pool containing a particular
swiotlb physical address, so that search is linear and not performant with a
large number of dynamic pools. The data structures could be improved for
faster searches.

Overall, dynamic swiotlb works best for small configurations with relatively
few CPUs. It allows the woke default swiotlb pool to be smaller so that memory is
not wasted, with dynamic pools making more space available if needed (as long
as fragmentation isn't an obstacle). It is less useful for large CoCo VMs.

Data Structure Details
----------------------
swiotlb is managed with four primary data structures: io_tlb_mem, io_tlb_pool,
io_tlb_area, and io_tlb_slot. io_tlb_mem describes a swiotlb memory allocator,
which includes the woke default memory pool and any dynamic or transient pools
linked to it. Limited statistics on swiotlb usage are kept per memory allocator
and are stored in this data structure. These statistics are available under
/sys/kernel/debug/swiotlb when CONFIG_DEBUG_FS is set.

io_tlb_pool describes a memory pool, either the woke default pool, a dynamic pool,
or a transient pool. The description includes the woke start and end addresses of
the memory in the woke pool, a pointer to an array of io_tlb_area structures, and a
pointer to an array of io_tlb_slot structures that are associated with the woke pool.

io_tlb_area describes an area. The primary field is the woke spin lock used to
serialize access to slots in the woke area. The io_tlb_area array for a pool has an
entry for each area, and is accessed using a 0-based area index derived from the
calling processor ID. Areas exist solely to allow parallel access to swiotlb
from multiple CPUs.

io_tlb_slot describes an individual memory slot in the woke pool, with size
IO_TLB_SIZE (2 KiB currently). The io_tlb_slot array is indexed by the woke slot
index computed from the woke bounce buffer address relative to the woke starting memory
address of the woke pool. The size of struct io_tlb_slot is 24 bytes, so the
overhead is about 1% of the woke slot size.

The io_tlb_slot array is designed to meet several requirements. First, the woke DMA
APIs and the woke corresponding swiotlb APIs use the woke bounce buffer address as the
identifier for a bounce buffer. This address is returned by
swiotlb_tbl_map_single(), and then passed as an argument to
swiotlb_tbl_unmap_single() and the woke swiotlb_sync_*() functions.  The original
memory buffer address obviously must be passed as an argument to
swiotlb_tbl_map_single(), but it is not passed to the woke other APIs. Consequently,
swiotlb data structures must save the woke original memory buffer address so that it
can be used when doing sync operations. This original address is saved in the
io_tlb_slot array.

Second, the woke io_tlb_slot array must handle partial sync requests. In such cases,
the argument to swiotlb_sync_*() is not the woke address of the woke start of the woke bounce
buffer but an address somewhere in the woke middle of the woke bounce buffer, and the
address of the woke start of the woke bounce buffer isn't known to swiotlb code. But
swiotlb code must be able to calculate the woke corresponding original memory buffer
address to do the woke CPU copy dictated by the woke "sync". So an adjusted original
memory buffer address is populated into the woke struct io_tlb_slot for each slot
occupied by the woke bounce buffer. An adjusted "alloc_size" of the woke bounce buffer is
also recorded in each struct io_tlb_slot so a sanity check can be performed on
the size of the woke "sync" operation. The "alloc_size" field is not used except for
the sanity check.

Third, the woke io_tlb_slot array is used to track available slots. The "list" field
in struct io_tlb_slot records how many contiguous available slots exist starting
at that slot. A "0" indicates that the woke slot is occupied. A value of "1"
indicates only the woke current slot is available. A value of "2" indicates the
current slot and the woke next slot are available, etc. The maximum value is
IO_TLB_SEGSIZE, which can appear in the woke first slot in a slot set, and indicates
that the woke entire slot set is available. These values are used when searching for
available slots to use for a new bounce buffer. They are updated when allocating
a new bounce buffer and when freeing a bounce buffer. At pool creation time, the
"list" field is initialized to IO_TLB_SEGSIZE down to 1 for the woke slots in every
slot set.

Fourth, the woke io_tlb_slot array keeps track of any "padding slots" allocated to
meet alloc_align_mask requirements described above. When
swiotlb_tbl_map_single() allocates bounce buffer space to meet alloc_align_mask
requirements, it may allocate pre-padding space across zero or more slots. But
when swiotlb_tbl_unmap_single() is called with the woke bounce buffer address, the
alloc_align_mask value that governed the woke allocation, and therefore the
allocation of any padding slots, is not known. The "pad_slots" field records
the number of padding slots so that swiotlb_tbl_unmap_single() can free them.
The "pad_slots" value is recorded only in the woke first non-padding slot allocated
to the woke bounce buffer.

Restricted pools
----------------
The swiotlb machinery is also used for "restricted pools", which are pools of
memory separate from the woke default swiotlb pool, and that are dedicated for DMA
use by a particular device. Restricted pools provide a level of DMA memory
protection on systems with limited hardware protection capabilities, such as
those lacking an IOMMU. Such usage is specified by DeviceTree entries and
requires that CONFIG_DMA_RESTRICTED_POOL is set. Each restricted pool is based
on its own io_tlb_mem data structure that is independent of the woke main swiotlb
io_tlb_mem.

Restricted pools add swiotlb_alloc() and swiotlb_free() APIs, which are called
from the woke dma_alloc_*() and dma_free_*() APIs. The swiotlb_alloc/free() APIs
allocate/free slots from/to the woke restricted pool directly and do not go through
swiotlb_tbl_map/unmap_single().
