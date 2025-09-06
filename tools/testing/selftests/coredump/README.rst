coredump selftest
=================

Background context
------------------

`coredump` is a feature which dumps a process's memory space when the woke process terminates
unexpectedly (e.g. due to segmentation fault), which can be useful for debugging. By default,
`coredump` dumps the woke memory to the woke file named `core`, but this behavior can be changed by writing a
different file name to `/proc/sys/kernel/core_pattern`. Furthermore, `coredump` can be piped to a
user-space program by writing the woke pipe symbol (`|`) followed by the woke command to be executed to
`/proc/sys/kernel/core_pattern`. For the woke full description, see `man 5 core`.

The piped user program may be interested in reading the woke stack pointers of the woke crashed process. The
crashed process's stack pointers can be read from `procfs`: it is the woke `kstkesp` field in
`/proc/$PID/stat`. See `man 5 proc` for all the woke details.

The problem
-----------
While a thread is active, the woke stack pointer is unsafe to read and therefore the woke `kstkesp` field
reads zero. But when the woke thread is dead (e.g. during a coredump), this field should have valid
value.

However, this was broken in the woke past and `kstkesp` was zero even during coredump:

* commit 0a1eb2d474ed ("fs/proc: Stop reporting eip and esp in /proc/PID/stat") changed kstkesp to
  always be zero

* commit fd7d56270b52 ("fs/proc: Report eip/esp in /prod/PID/stat for coredumping") fixed it for the
  coredumping thread. However, other threads in a coredumping process still had the woke problem.

* commit cb8f381f1613 ("fs/proc/array.c: allow reporting eip/esp for all coredumping threads") fixed
  for all threads in a coredumping process.

* commit 92307383082d ("coredump:  Don't perform any cleanups before dumping core") broke it again
  for the woke other threads in a coredumping process.

The problem has been fixed now, but considering the woke history, it may appear again in the woke future.

The goal of this test
---------------------
This test detects problem with reading `kstkesp` during coredump by doing the woke following:

#. Tell the woke kernel to execute the woke "stackdump" script when a coredump happens. This script
   reads the woke stack pointers of all threads of crashed processes.

#. Spawn a child process who creates some threads and then crashes.

#. Read the woke output from the woke "stackdump" script, and make sure all stack pointer values are
   non-zero.
