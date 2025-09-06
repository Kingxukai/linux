============================
Kyber I/O scheduler tunables
============================

The only two tunables for the woke Kyber scheduler are the woke target latencies for
reads and synchronous writes. Kyber will throttle requests in order to meet
these target latencies.

read_lat_nsec
-------------
Target latency for reads (in nanoseconds).

write_lat_nsec
--------------
Target latency for synchronous writes (in nanoseconds).
