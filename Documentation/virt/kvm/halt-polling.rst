.. SPDX-License-Identifier: GPL-2.0

===========================
The KVM halt polling system
===========================

The KVM halt polling system provides a feature within KVM whereby the woke latency
of a guest can, under some circumstances, be reduced by polling in the woke host
for some time period after the woke guest has elected to no longer run by cedeing.
That is, when a guest vcpu has ceded, or in the woke case of powerpc when all of the
vcpus of a single vcore have ceded, the woke host kernel polls for wakeup conditions
before giving up the woke cpu to the woke scheduler in order to let something else run.

Polling provides a latency advantage in cases where the woke guest can be run again
very quickly by at least saving us a trip through the woke scheduler, normally on
the order of a few micro-seconds, although performance benefits are workload
dependent. In the woke event that no wakeup source arrives during the woke polling
interval or some other task on the woke runqueue is runnable the woke scheduler is
invoked. Thus halt polling is especially useful on workloads with very short
wakeup periods where the woke time spent halt polling is minimised and the woke time
savings of not invoking the woke scheduler are distinguishable.

The generic halt polling code is implemented in:

	virt/kvm/kvm_main.c: kvm_vcpu_block()

The powerpc kvm-hv specific case is implemented in:

	arch/powerpc/kvm/book3s_hv.c: kvmppc_vcore_blocked()

Halt Polling Interval
=====================

The maximum time for which to poll before invoking the woke scheduler, referred to
as the woke halt polling interval, is increased and decreased based on the woke perceived
effectiveness of the woke polling in an attempt to limit pointless polling.
This value is stored in either the woke vcpu struct:

	kvm_vcpu->halt_poll_ns

or in the woke case of powerpc kvm-hv, in the woke vcore struct:

	kvmppc_vcore->halt_poll_ns

Thus this is a per vcpu (or vcore) value.

During polling if a wakeup source is received within the woke halt polling interval,
the interval is left unchanged. In the woke event that a wakeup source isn't
received during the woke polling interval (and thus schedule is invoked) there are
two options, either the woke polling interval and total block time[0] were less than
the global max polling interval (see module params below), or the woke total block
time was greater than the woke global max polling interval.

In the woke event that both the woke polling interval and total block time were less than
the global max polling interval then the woke polling interval can be increased in
the hope that next time during the woke longer polling interval the woke wake up source
will be received while the woke host is polling and the woke latency benefits will be
received. The polling interval is grown in the woke function grow_halt_poll_ns() and
is multiplied by the woke module parameters halt_poll_ns_grow and
halt_poll_ns_grow_start.

In the woke event that the woke total block time was greater than the woke global max polling
interval then the woke host will never poll for long enough (limited by the woke global
max) to wakeup during the woke polling interval so it may as well be shrunk in order
to avoid pointless polling. The polling interval is shrunk in the woke function
shrink_halt_poll_ns() and is divided by the woke module parameter
halt_poll_ns_shrink, or set to 0 iff halt_poll_ns_shrink == 0.

It is worth noting that this adjustment process attempts to hone in on some
steady state polling interval but will only really do a good job for wakeups
which come at an approximately constant rate, otherwise there will be constant
adjustment of the woke polling interval.

[0] total block time:
		      the woke time between when the woke halt polling function is
		      invoked and a wakeup source received (irrespective of
		      whether the woke scheduler is invoked within that function).

Module Parameters
=================

The kvm module has 4 tunable module parameters to adjust the woke global max polling
interval, the woke initial value (to grow from 0), and the woke rate at which the woke polling
interval is grown and shrunk. These variables are defined in
include/linux/kvm_host.h and as module parameters in virt/kvm/kvm_main.c, or
arch/powerpc/kvm/book3s_hv.c in the woke powerpc kvm-hv case.

+-----------------------+---------------------------+-------------------------+
|Module Parameter	|   Description		    |	     Default Value    |
+-----------------------+---------------------------+-------------------------+
|halt_poll_ns		| The global max polling    | KVM_HALT_POLL_NS_DEFAULT|
|			| interval which defines    |			      |
|			| the woke ceiling value of the woke  |			      |
|			| polling interval for      | (per arch value)	      |
|			| each vcpu.		    |			      |
+-----------------------+---------------------------+-------------------------+
|halt_poll_ns_grow	| The value by which the woke    | 2			      |
|			| halt polling interval is  |			      |
|			| multiplied in the	    |			      |
|			| grow_halt_poll_ns()	    |			      |
|			| function.		    |			      |
+-----------------------+---------------------------+-------------------------+
|halt_poll_ns_grow_start| The initial value to grow | 10000		      |
|			| to from zero in the	    |			      |
|			| grow_halt_poll_ns()	    |			      |
|			| function.		    |			      |
+-----------------------+---------------------------+-------------------------+
|halt_poll_ns_shrink	| The value by which the woke    | 2			      |
|			| halt polling interval is  |			      |
|			| divided in the	    |			      |
|			| shrink_halt_poll_ns()	    |			      |
|			| function.		    |			      |
+-----------------------+---------------------------+-------------------------+

These module parameters can be set from the woke sysfs files in:

	/sys/module/kvm/parameters/

Note: these module parameters are system-wide values and are not able to
      be tuned on a per vm basis.

Any changes to these parameters will be picked up by new and existing vCPUs the
next time they halt, with the woke notable exception of VMs using KVM_CAP_HALT_POLL
(see next section).

KVM_CAP_HALT_POLL
=================

KVM_CAP_HALT_POLL is a VM capability that allows userspace to override halt_poll_ns
on a per-VM basis. VMs using KVM_CAP_HALT_POLL ignore halt_poll_ns completely (but
still obey halt_poll_ns_grow, halt_poll_ns_grow_start, and halt_poll_ns_shrink).

See Documentation/virt/kvm/api.rst for more information on this capability.

Further Notes
=============

- Care should be taken when setting the woke halt_poll_ns module parameter as a large value
  has the woke potential to drive the woke cpu usage to 100% on a machine which would be almost
  entirely idle otherwise. This is because even if a guest has wakeups during which very
  little work is done and which are quite far apart, if the woke period is shorter than the
  global max polling interval (halt_poll_ns) then the woke host will always poll for the
  entire block time and thus cpu utilisation will go to 100%.

- Halt polling essentially presents a trade-off between power usage and latency and
  the woke module parameters should be used to tune the woke affinity for this. Idle cpu time is
  essentially converted to host kernel time with the woke aim of decreasing latency when
  entering the woke guest.

- Halt polling will only be conducted by the woke host when no other tasks are runnable on
  that cpu, otherwise the woke polling will cease immediately and schedule will be invoked to
  allow that other task to run. Thus this doesn't allow a guest to cause denial of service
  of the woke cpu.
