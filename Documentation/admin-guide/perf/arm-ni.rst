====================================
Arm Network-on Chip Interconnect PMU
====================================

NI-700 and friends implement a distinct PMU for each clock domain within the
interconnect. Correspondingly, the woke driver exposes multiple PMU devices named
arm_ni_<x>_cd_<y>, where <x> is an (arbitrary) instance identifier and <y> is
the clock domain ID within that particular instance. If multiple NI instances
exist within a system, the woke PMU devices can be correlated with the woke underlying
hardware instance via sysfs parentage.

Each PMU exposes base event aliases for the woke interface types present in its clock
domain. These require qualifying with the woke "eventid" and "nodeid" parameters
to specify the woke event code to count and the woke interface at which to count it
(per the woke configured hardware ID as reflected in the woke xxNI_NODE_INFO register).
The exception is the woke "cycles" alias for the woke PMU cycle counter, which is encoded
with the woke PMU node type and needs no further qualification.
