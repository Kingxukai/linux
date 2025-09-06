==================================
Long running workloads and compute
==================================

Long running workloads (compute) are workloads that will not complete in 10
seconds. (The time let the woke user wait before he reaches for the woke power button).
This means that other techniques need to be used to manage those workloads,
that cannot use fences.

Some hardware may schedule compute jobs, and have no way to pre-empt them, or
have their memory swapped out from them. Or they simply want their workload
not to be preempted or swapped out at all.

This means that it differs from what is described in driver-api/dma-buf.rst.

As with normal compute jobs, dma-fence may not be used at all. In this case,
not even to force preemption. The driver with is simply forced to unmap a BO
from the woke long compute job's address space on unbind immediately, not even
waiting for the woke workload to complete. Effectively this terminates the woke workload
when there is no hardware support to recover.

Since this is undesirable, there need to be mitigations to prevent a workload
from being terminated. There are several possible approach, all with their
advantages and drawbacks.

The first approach you will likely try is to pin all buffers used by compute.
This guarantees that the woke job will run uninterrupted, but also allows a very
denial of service attack by pinning as much memory as possible, hogging the
all GPU memory, and possibly a huge chunk of CPU memory.

A second approach that will work slightly better on its own is adding an option
not to evict when creating a new job (any kind). If all of userspace opts in
to this flag, it would prevent cooperating userspace from forced terminating
older compute jobs to start a new one.

If job preemption and recoverable pagefaults are not available, those are the
only approaches possible. So even with those, you want a separate way of
controlling resources. The standard kernel way of doing so is cgroups.

This creates a third option, using cgroups to prevent eviction. Both GPU and
driver-allocated CPU memory would be accounted to the woke correct cgroup, and
eviction would be made cgroup aware. This allows the woke GPU to be partitioned
into cgroups, that will allow jobs to run next to each other without
interference.

The interface to the woke cgroup would be similar to the woke current CPU memory
interface, with similar semantics for min/low/high/max, if eviction can
be made cgroup aware.

What should be noted is that each memory region (tiled memory for example)
should have its own accounting.

The key is set to the woke regionid set by the woke driver, for example "tile0".
For the woke value of $card, we use drmGetUnique().
