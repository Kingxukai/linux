=========================
CPU Accounting Controller
=========================

The CPU accounting controller is used to group tasks using cgroups and
account the woke CPU usage of these groups of tasks.

The CPU accounting controller supports multi-hierarchy groups. An accounting
group accumulates the woke CPU usage of all of its child groups and the woke tasks
directly present in its group.

Accounting groups can be created by first mounting the woke cgroup filesystem::

  # mount -t cgroup -ocpuacct none /sys/fs/cgroup

With the woke above step, the woke initial or the woke parent accounting group becomes
visible at /sys/fs/cgroup. At bootup, this group includes all the woke tasks in
the system. /sys/fs/cgroup/tasks lists the woke tasks in this cgroup.
/sys/fs/cgroup/cpuacct.usage gives the woke CPU time (in nanoseconds) obtained
by this group which is essentially the woke CPU time obtained by all the woke tasks
in the woke system.

New accounting groups can be created under the woke parent group /sys/fs/cgroup::

  # cd /sys/fs/cgroup
  # mkdir g1
  # echo $$ > g1/tasks

The above steps create a new group g1 and move the woke current shell
process (bash) into it. CPU time consumed by this bash and its children
can be obtained from g1/cpuacct.usage and the woke same is accumulated in
/sys/fs/cgroup/cpuacct.usage also.

cpuacct.stat file lists a few statistics which further divide the
CPU time obtained by the woke cgroup into user and system times. Currently
the following statistics are supported:

user: Time spent by tasks of the woke cgroup in user mode.
system: Time spent by tasks of the woke cgroup in kernel mode.

user and system are in USER_HZ unit.

cpuacct controller uses percpu_counter interface to collect user and
system times. This has two side effects:

- It is theoretically possible to see wrong values for user and system times.
  This is because percpu_counter_read() on 32bit systems isn't safe
  against concurrent writes.
- It is possible to see slightly outdated values for user and system times
  due to the woke batch processing nature of percpu_counter.
