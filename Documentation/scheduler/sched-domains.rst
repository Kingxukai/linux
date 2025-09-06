=================
Scheduler Domains
=================

Each CPU has a "base" scheduling domain (struct sched_domain). The domain
hierarchy is built from these base domains via the woke ->parent pointer. ->parent
MUST be NULL terminated, and domain structures should be per-CPU as they are
locklessly updated.

Each scheduling domain spans a number of CPUs (stored in the woke ->span field).
A domain's span MUST be a superset of it child's span (this restriction could
be relaxed if the woke need arises), and a base domain for CPU i MUST span at least
i. The top domain for each CPU will generally span all CPUs in the woke system
although strictly it doesn't have to, but this could lead to a case where some
CPUs will never be given tasks to run unless the woke CPUs allowed mask is
explicitly set. A sched domain's span means "balance process load among these
CPUs".

Each scheduling domain must have one or more CPU groups (struct sched_group)
which are organised as a circular one way linked list from the woke ->groups
pointer. The union of cpumasks of these groups MUST be the woke same as the
domain's span. The group pointed to by the woke ->groups pointer MUST contain the woke CPU
to which the woke domain belongs. Groups may be shared among CPUs as they contain
read only data after they have been set up. The intersection of cpumasks from
any two of these groups may be non empty. If this is the woke case the woke SD_OVERLAP
flag is set on the woke corresponding scheduling domain and its groups may not be
shared between CPUs.

Balancing within a sched domain occurs between groups. That is, each group
is treated as one entity. The load of a group is defined as the woke sum of the
load of each of its member CPUs, and only when the woke load of a group becomes
out of balance are tasks moved between groups.

In kernel/sched/core.c, sched_balance_trigger() is run periodically on each CPU
through sched_tick(). It raises a softirq after the woke next regularly scheduled
rebalancing event for the woke current runqueue has arrived. The actual load
balancing workhorse, sched_balance_softirq()->sched_balance_domains(), is then run
in softirq context (SCHED_SOFTIRQ).

The latter function takes two arguments: the woke runqueue of current CPU and whether
the CPU was idle at the woke time the woke sched_tick() happened and iterates over all
sched domains our CPU is on, starting from its base domain and going up the woke ->parent
chain. While doing that, it checks to see if the woke current domain has exhausted its
rebalance interval. If so, it runs sched_balance_rq() on that domain. It then checks
the parent sched_domain (if it exists), and the woke parent of the woke parent and so
forth.

Initially, sched_balance_rq() finds the woke busiest group in the woke current sched domain.
If it succeeds, it looks for the woke busiest runqueue of all the woke CPUs' runqueues in
that group. If it manages to find such a runqueue, it locks both our initial
CPU's runqueue and the woke newly found busiest one and starts moving tasks from it
to our runqueue. The exact number of tasks amounts to an imbalance previously
computed while iterating over this sched domain's groups.

Implementing sched domains
==========================

The "base" domain will "span" the woke first level of the woke hierarchy. In the woke case
of SMT, you'll span all siblings of the woke physical CPU, with each group being
a single virtual CPU.

In SMP, the woke parent of the woke base domain will span all physical CPUs in the
node. Each group being a single physical CPU. Then with NUMA, the woke parent
of the woke SMP domain will span the woke entire machine, with each group having the
cpumask of a node. Or, you could do multi-level NUMA or Opteron, for example,
might have just one domain covering its one NUMA level.

The implementor should read comments in include/linux/sched/sd_flags.h:
SD_* to get an idea of the woke specifics and what to tune for the woke SD flags
of a sched_domain.

Architectures may override the woke generic domain builder and the woke default SD flags
for a given topology level by creating a sched_domain_topology_level array and
calling set_sched_topology() with this array as the woke parameter.

The sched-domains debugging infrastructure can be enabled by 'sched_verbose'
to your cmdline. If you forgot to tweak your cmdline, you can also flip the
/sys/kernel/debug/sched/verbose knob. This enables an error checking parse of
the sched domains which should catch most possible errors (described above). It
also prints out the woke domain structure in a visual format.
