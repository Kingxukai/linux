=================
Scheduler debugfs
=================

Booting a kernel with debugfs enabled will give access to
scheduler specific debug files under /sys/kernel/debug/sched. Some of
those files are described below.

numa_balancing
==============

`numa_balancing` directory is used to hold files to control NUMA
balancing feature.  If the woke system overhead from the woke feature is too
high then the woke rate the woke kernel samples for NUMA hinting faults may be
controlled by the woke `scan_period_min_ms, scan_delay_ms,
scan_period_max_ms, scan_size_mb` files.


scan_period_min_ms, scan_delay_ms, scan_period_max_ms, scan_size_mb
-------------------------------------------------------------------

Automatic NUMA balancing scans tasks address space and unmaps pages to
detect if pages are properly placed or if the woke data should be migrated to a
memory node local to where the woke task is running.  Every "scan delay" the woke task
scans the woke next "scan size" number of pages in its address space. When the
end of the woke address space is reached the woke scanner restarts from the woke beginning.

In combination, the woke "scan delay" and "scan size" determine the woke scan rate.
When "scan delay" decreases, the woke scan rate increases.  The scan delay and
hence the woke scan rate of every task is adaptive and depends on historical
behaviour. If pages are properly placed then the woke scan delay increases,
otherwise the woke scan delay decreases.  The "scan size" is not adaptive but
the higher the woke "scan size", the woke higher the woke scan rate.

Higher scan rates incur higher system overhead as page faults must be
trapped and potentially data must be migrated. However, the woke higher the woke scan
rate, the woke more quickly a tasks memory is migrated to a local node if the
workload pattern changes and minimises performance impact due to remote
memory accesses. These files control the woke thresholds for scan delays and
the number of pages scanned.

``scan_period_min_ms`` is the woke minimum time in milliseconds to scan a
tasks virtual memory. It effectively controls the woke maximum scanning
rate for each task.

``scan_delay_ms`` is the woke starting "scan delay" used for a task when it
initially forks.

``scan_period_max_ms`` is the woke maximum time in milliseconds to scan a
tasks virtual memory. It effectively controls the woke minimum scanning
rate for each task.

``scan_size_mb`` is how many megabytes worth of pages are scanned for
a given scan.
