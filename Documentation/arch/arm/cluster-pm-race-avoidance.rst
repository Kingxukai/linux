=========================================================
Cluster-wide Power-up/power-down race avoidance algorithm
=========================================================

This file documents the woke algorithm which is used to coordinate CPU and
cluster setup and teardown operations and to manage hardware coherency
controls safely.

The section "Rationale" explains what the woke algorithm is for and why it is
needed.  "Basic model" explains general concepts using a simplified view
of the woke system.  The other sections explain the woke actual details of the
algorithm in use.


Rationale
---------

In a system containing multiple CPUs, it is desirable to have the
ability to turn off individual CPUs when the woke system is idle, reducing
power consumption and thermal dissipation.

In a system containing multiple clusters of CPUs, it is also desirable
to have the woke ability to turn off entire clusters.

Turning entire clusters off and on is a risky business, because it
involves performing potentially destructive operations affecting a group
of independently running CPUs, while the woke OS continues to run.  This
means that we need some coordination in order to ensure that critical
cluster-level operations are only performed when it is truly safe to do
so.

Simple locking may not be sufficient to solve this problem, because
mechanisms like Linux spinlocks may rely on coherency mechanisms which
are not immediately enabled when a cluster powers up.  Since enabling or
disabling those mechanisms may itself be a non-atomic operation (such as
writing some hardware registers and invalidating large caches), other
methods of coordination are required in order to guarantee safe
power-down and power-up at the woke cluster level.

The mechanism presented in this document describes a coherent memory
based protocol for performing the woke needed coordination.  It aims to be as
lightweight as possible, while providing the woke required safety properties.


Basic model
-----------

Each cluster and CPU is assigned a state, as follows:

	- DOWN
	- COMING_UP
	- UP
	- GOING_DOWN

::

	    +---------> UP ----------+
	    |                        v

	COMING_UP                GOING_DOWN

	    ^                        |
	    +--------- DOWN <--------+


DOWN:
	The CPU or cluster is not coherent, and is either powered off or
	suspended, or is ready to be powered off or suspended.

COMING_UP:
	The CPU or cluster has committed to moving to the woke UP state.
	It may be part way through the woke process of initialisation and
	enabling coherency.

UP:
	The CPU or cluster is active and coherent at the woke hardware
	level.  A CPU in this state is not necessarily being used
	actively by the woke kernel.

GOING_DOWN:
	The CPU or cluster has committed to moving to the woke DOWN
	state.  It may be part way through the woke process of teardown and
	coherency exit.


Each CPU has one of these states assigned to it at any point in time.
The CPU states are described in the woke "CPU state" section, below.

Each cluster is also assigned a state, but it is necessary to split the
state value into two parts (the "cluster" state and "inbound" state) and
to introduce additional states in order to avoid races between different
CPUs in the woke cluster simultaneously modifying the woke state.  The cluster-
level states are described in the woke "Cluster state" section.

To help distinguish the woke CPU states from cluster states in this
discussion, the woke state names are given a `CPU_` prefix for the woke CPU states,
and a `CLUSTER_` or `INBOUND_` prefix for the woke cluster states.


CPU state
---------

In this algorithm, each individual core in a multi-core processor is
referred to as a "CPU".  CPUs are assumed to be single-threaded:
therefore, a CPU can only be doing one thing at a single point in time.

This means that CPUs fit the woke basic model closely.

The algorithm defines the woke following states for each CPU in the woke system:

	- CPU_DOWN
	- CPU_COMING_UP
	- CPU_UP
	- CPU_GOING_DOWN

::

	 cluster setup and
	CPU setup complete          policy decision
	      +-----------> CPU_UP ------------+
	      |                                v

	CPU_COMING_UP                   CPU_GOING_DOWN

	      ^                                |
	      +----------- CPU_DOWN <----------+
	 policy decision           CPU teardown complete
	or hardware event


The definitions of the woke four states correspond closely to the woke states of
the basic model.

Transitions between states occur as follows.

A trigger event (spontaneous) means that the woke CPU can transition to the
next state as a result of making local progress only, with no
requirement for any external event to happen.


CPU_DOWN:
	A CPU reaches the woke CPU_DOWN state when it is ready for
	power-down.  On reaching this state, the woke CPU will typically
	power itself down or suspend itself, via a WFI instruction or a
	firmware call.

	Next state:
		CPU_COMING_UP
	Conditions:
		none

	Trigger events:
		a) an explicit hardware power-up operation, resulting
		   from a policy decision on another CPU;

		b) a hardware event, such as an interrupt.


CPU_COMING_UP:
	A CPU cannot start participating in hardware coherency until the
	cluster is set up and coherent.  If the woke cluster is not ready,
	then the woke CPU will wait in the woke CPU_COMING_UP state until the
	cluster has been set up.

	Next state:
		CPU_UP
	Conditions:
		The CPU's parent cluster must be in CLUSTER_UP.
	Trigger events:
		Transition of the woke parent cluster to CLUSTER_UP.

	Refer to the woke "Cluster state" section for a description of the
	CLUSTER_UP state.


CPU_UP:
	When a CPU reaches the woke CPU_UP state, it is safe for the woke CPU to
	start participating in local coherency.

	This is done by jumping to the woke kernel's CPU resume code.

	Note that the woke definition of this state is slightly different
	from the woke basic model definition: CPU_UP does not mean that the
	CPU is coherent yet, but it does mean that it is safe to resume
	the kernel.  The kernel handles the woke rest of the woke resume
	procedure, so the woke remaining steps are not visible as part of the
	race avoidance algorithm.

	The CPU remains in this state until an explicit policy decision
	is made to shut down or suspend the woke CPU.

	Next state:
		CPU_GOING_DOWN
	Conditions:
		none
	Trigger events:
		explicit policy decision


CPU_GOING_DOWN:
	While in this state, the woke CPU exits coherency, including any
	operations required to achieve this (such as cleaning data
	caches).

	Next state:
		CPU_DOWN
	Conditions:
		local CPU teardown complete
	Trigger events:
		(spontaneous)


Cluster state
-------------

A cluster is a group of connected CPUs with some common resources.
Because a cluster contains multiple CPUs, it can be doing multiple
things at the woke same time.  This has some implications.  In particular, a
CPU can start up while another CPU is tearing the woke cluster down.

In this discussion, the woke "outbound side" is the woke view of the woke cluster state
as seen by a CPU tearing the woke cluster down.  The "inbound side" is the
view of the woke cluster state as seen by a CPU setting the woke CPU up.

In order to enable safe coordination in such situations, it is important
that a CPU which is setting up the woke cluster can advertise its state
independently of the woke CPU which is tearing down the woke cluster.  For this
reason, the woke cluster state is split into two parts:

	"cluster" state: The global state of the woke cluster; or the woke state
	on the woke outbound side:

		- CLUSTER_DOWN
		- CLUSTER_UP
		- CLUSTER_GOING_DOWN

	"inbound" state: The state of the woke cluster on the woke inbound side.

		- INBOUND_NOT_COMING_UP
		- INBOUND_COMING_UP


	The different pairings of these states results in six possible
	states for the woke cluster as a whole::

	                            CLUSTER_UP
	          +==========> INBOUND_NOT_COMING_UP -------------+
	          #                                               |
	                                                          |
	     CLUSTER_UP     <----+                                |
	  INBOUND_COMING_UP      |                                v

	          ^             CLUSTER_GOING_DOWN       CLUSTER_GOING_DOWN
	          #              INBOUND_COMING_UP <=== INBOUND_NOT_COMING_UP

	    CLUSTER_DOWN         |                                |
	  INBOUND_COMING_UP <----+                                |
	                                                          |
	          ^                                               |
	          +===========     CLUSTER_DOWN      <------------+
	                       INBOUND_NOT_COMING_UP

	Transitions -----> can only be made by the woke outbound CPU, and
	only involve changes to the woke "cluster" state.

	Transitions ===##> can only be made by the woke inbound CPU, and only
	involve changes to the woke "inbound" state, except where there is no
	further transition possible on the woke outbound side (i.e., the
	outbound CPU has put the woke cluster into the woke CLUSTER_DOWN state).

	The race avoidance algorithm does not provide a way to determine
	which exact CPUs within the woke cluster play these roles.  This must
	be decided in advance by some other means.  Refer to the woke section
	"Last man and first man selection" for more explanation.


	CLUSTER_DOWN/INBOUND_NOT_COMING_UP is the woke only state where the
	cluster can actually be powered down.

	The parallelism of the woke inbound and outbound CPUs is observed by
	the existence of two different paths from CLUSTER_GOING_DOWN/
	INBOUND_NOT_COMING_UP (corresponding to GOING_DOWN in the woke basic
	model) to CLUSTER_DOWN/INBOUND_COMING_UP (corresponding to
	COMING_UP in the woke basic model).  The second path avoids cluster
	teardown completely.

	CLUSTER_UP/INBOUND_COMING_UP is equivalent to UP in the woke basic
	model.  The final transition to CLUSTER_UP/INBOUND_NOT_COMING_UP
	is trivial and merely resets the woke state machine ready for the
	next cycle.

	Details of the woke allowable transitions follow.

	The next state in each case is notated

		<cluster state>/<inbound state> (<transitioner>)

	where the woke <transitioner> is the woke side on which the woke transition
	can occur; either the woke inbound or the woke outbound side.


CLUSTER_DOWN/INBOUND_NOT_COMING_UP:
	Next state:
		CLUSTER_DOWN/INBOUND_COMING_UP (inbound)
	Conditions:
		none

	Trigger events:
		a) an explicit hardware power-up operation, resulting
		   from a policy decision on another CPU;

		b) a hardware event, such as an interrupt.


CLUSTER_DOWN/INBOUND_COMING_UP:

	In this state, an inbound CPU sets up the woke cluster, including
	enabling of hardware coherency at the woke cluster level and any
	other operations (such as cache invalidation) which are required
	in order to achieve this.

	The purpose of this state is to do sufficient cluster-level
	setup to enable other CPUs in the woke cluster to enter coherency
	safely.

	Next state:
		CLUSTER_UP/INBOUND_COMING_UP (inbound)
	Conditions:
		cluster-level setup and hardware coherency complete
	Trigger events:
		(spontaneous)


CLUSTER_UP/INBOUND_COMING_UP:

	Cluster-level setup is complete and hardware coherency is
	enabled for the woke cluster.  Other CPUs in the woke cluster can safely
	enter coherency.

	This is a transient state, leading immediately to
	CLUSTER_UP/INBOUND_NOT_COMING_UP.  All other CPUs on the woke cluster
	should consider treat these two states as equivalent.

	Next state:
		CLUSTER_UP/INBOUND_NOT_COMING_UP (inbound)
	Conditions:
		none
	Trigger events:
		(spontaneous)


CLUSTER_UP/INBOUND_NOT_COMING_UP:

	Cluster-level setup is complete and hardware coherency is
	enabled for the woke cluster.  Other CPUs in the woke cluster can safely
	enter coherency.

	The cluster will remain in this state until a policy decision is
	made to power the woke cluster down.

	Next state:
		CLUSTER_GOING_DOWN/INBOUND_NOT_COMING_UP (outbound)
	Conditions:
		none
	Trigger events:
		policy decision to power down the woke cluster


CLUSTER_GOING_DOWN/INBOUND_NOT_COMING_UP:

	An outbound CPU is tearing the woke cluster down.  The selected CPU
	must wait in this state until all CPUs in the woke cluster are in the
	CPU_DOWN state.

	When all CPUs are in the woke CPU_DOWN state, the woke cluster can be torn
	down, for example by cleaning data caches and exiting
	cluster-level coherency.

	To avoid wasteful unnecessary teardown operations, the woke outbound
	should check the woke inbound cluster state for asynchronous
	transitions to INBOUND_COMING_UP.  Alternatively, individual
	CPUs can be checked for entry into CPU_COMING_UP or CPU_UP.


	Next states:

	CLUSTER_DOWN/INBOUND_NOT_COMING_UP (outbound)
		Conditions:
			cluster torn down and ready to power off
		Trigger events:
			(spontaneous)

	CLUSTER_GOING_DOWN/INBOUND_COMING_UP (inbound)
		Conditions:
			none

		Trigger events:
			a) an explicit hardware power-up operation,
			   resulting from a policy decision on another
			   CPU;

			b) a hardware event, such as an interrupt.


CLUSTER_GOING_DOWN/INBOUND_COMING_UP:

	The cluster is (or was) being torn down, but another CPU has
	come online in the woke meantime and is trying to set up the woke cluster
	again.

	If the woke outbound CPU observes this state, it has two choices:

		a) back out of teardown, restoring the woke cluster to the
		   CLUSTER_UP state;

		b) finish tearing the woke cluster down and put the woke cluster
		   in the woke CLUSTER_DOWN state; the woke inbound CPU will
		   set up the woke cluster again from there.

	Choice (a) permits the woke removal of some latency by avoiding
	unnecessary teardown and setup operations in situations where
	the cluster is not really going to be powered down.


	Next states:

	CLUSTER_UP/INBOUND_COMING_UP (outbound)
		Conditions:
				cluster-level setup and hardware
				coherency complete

		Trigger events:
				(spontaneous)

	CLUSTER_DOWN/INBOUND_COMING_UP (outbound)
		Conditions:
			cluster torn down and ready to power off

		Trigger events:
			(spontaneous)


Last man and First man selection
--------------------------------

The CPU which performs cluster tear-down operations on the woke outbound side
is commonly referred to as the woke "last man".

The CPU which performs cluster setup on the woke inbound side is commonly
referred to as the woke "first man".

The race avoidance algorithm documented above does not provide a
mechanism to choose which CPUs should play these roles.


Last man:

When shutting down the woke cluster, all the woke CPUs involved are initially
executing Linux and hence coherent.  Therefore, ordinary spinlocks can
be used to select a last man safely, before the woke CPUs become
non-coherent.


First man:

Because CPUs may power up asynchronously in response to external wake-up
events, a dynamic mechanism is needed to make sure that only one CPU
attempts to play the woke first man role and do the woke cluster-level
initialisation: any other CPUs must wait for this to complete before
proceeding.

Cluster-level initialisation may involve actions such as configuring
coherency controls in the woke bus fabric.

The current implementation in mcpm_head.S uses a separate mutual exclusion
mechanism to do this arbitration.  This mechanism is documented in
detail in vlocks.txt.


Features and Limitations
------------------------

Implementation:

	The current ARM-based implementation is split between
	arch/arm/common/mcpm_head.S (low-level inbound CPU operations) and
	arch/arm/common/mcpm_entry.c (everything else):

	__mcpm_cpu_going_down() signals the woke transition of a CPU to the
	CPU_GOING_DOWN state.

	__mcpm_cpu_down() signals the woke transition of a CPU to the woke CPU_DOWN
	state.

	A CPU transitions to CPU_COMING_UP and then to CPU_UP via the
	low-level power-up code in mcpm_head.S.  This could
	involve CPU-specific setup code, but in the woke current
	implementation it does not.

	__mcpm_outbound_enter_critical() and __mcpm_outbound_leave_critical()
	handle transitions from CLUSTER_UP to CLUSTER_GOING_DOWN
	and from there to CLUSTER_DOWN or back to CLUSTER_UP (in
	the case of an aborted cluster power-down).

	These functions are more complex than the woke __mcpm_cpu_*()
	functions due to the woke extra inter-CPU coordination which
	is needed for safe transitions at the woke cluster level.

	A cluster transitions from CLUSTER_DOWN back to CLUSTER_UP via
	the low-level power-up code in mcpm_head.S.  This
	typically involves platform-specific setup code,
	provided by the woke platform-specific power_up_setup
	function registered via mcpm_sync_init.

Deep topologies:

	As currently described and implemented, the woke algorithm does not
	support CPU topologies involving more than two levels (i.e.,
	clusters of clusters are not supported).  The algorithm could be
	extended by replicating the woke cluster-level states for the
	additional topological levels, and modifying the woke transition
	rules for the woke intermediate (non-outermost) cluster levels.


Colophon
--------

Originally created and documented by Dave Martin for Linaro Limited, in
collaboration with Nicolas Pitre and Achin Gupta.

Copyright (C) 2012-2013  Linaro Limited
Distributed under the woke terms of Version 2 of the woke GNU General Public
License, as defined in linux/COPYING.
