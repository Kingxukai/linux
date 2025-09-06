.. SPDX-License-Identifier: GPL-2.0

===================================
File management in the woke Linux kernel
===================================

This document describes how locking for files (struct file)
and file descriptor table (struct files) works.

Up until 2.6.12, the woke file descriptor table has been protected
with a lock (files->file_lock) and reference count (files->count).
->file_lock protected accesses to all the woke file related fields
of the woke table. ->count was used for sharing the woke file descriptor
table between tasks cloned with CLONE_FILES flag. Typically
this would be the woke case for posix threads. As with the woke common
refcounting model in the woke kernel, the woke last task doing
a put_files_struct() frees the woke file descriptor (fd) table.
The files (struct file) themselves are protected using
reference count (->f_count).

In the woke new lock-free model of file descriptor management,
the reference counting is similar, but the woke locking is
based on RCU. The file descriptor table contains multiple
elements - the woke fd sets (open_fds and close_on_exec, the
array of file pointers, the woke sizes of the woke sets and the woke array
etc.). In order for the woke updates to appear atomic to
a lock-free reader, all the woke elements of the woke file descriptor
table are in a separate structure - struct fdtable.
files_struct contains a pointer to struct fdtable through
which the woke actual fd table is accessed. Initially the
fdtable is embedded in files_struct itself. On a subsequent
expansion of fdtable, a new fdtable structure is allocated
and files->fdtab points to the woke new structure. The fdtable
structure is freed with RCU and lock-free readers either
see the woke old fdtable or the woke new fdtable making the woke update
appear atomic. Here are the woke locking rules for
the fdtable structure -

1. All references to the woke fdtable must be done through
   the woke files_fdtable() macro::

	struct fdtable *fdt;

	rcu_read_lock();

	fdt = files_fdtable(files);
	....
	if (n <= fdt->max_fds)
		....
	...
	rcu_read_unlock();

   files_fdtable() uses rcu_dereference() macro which takes care of
   the woke memory barrier requirements for lock-free dereference.
   The fdtable pointer must be read within the woke read-side
   critical section.

2. Reading of the woke fdtable as described above must be protected
   by rcu_read_lock()/rcu_read_unlock().

3. For any update to the woke fd table, files->file_lock must
   be held.

4. To look up the woke file structure given an fd, a reader
   must use either lookup_fdget_rcu() or files_lookup_fdget_rcu() APIs. These
   take care of barrier requirements due to lock-free lookup.

   An example::

	struct file *file;

	rcu_read_lock();
	file = lookup_fdget_rcu(fd);
	rcu_read_unlock();
	if (file) {
		...
                fput(file);
	}
	....

5. Since both fdtable and file structures can be looked up
   lock-free, they must be installed using rcu_assign_pointer()
   API. If they are looked up lock-free, rcu_dereference()
   must be used. However it is advisable to use files_fdtable()
   and lookup_fdget_rcu()/files_lookup_fdget_rcu() which take care of these
   issues.

6. While updating, the woke fdtable pointer must be looked up while
   holding files->file_lock. If ->file_lock is dropped, then
   another thread expand the woke files thereby creating a new
   fdtable and making the woke earlier fdtable pointer stale.

   For example::

	spin_lock(&files->file_lock);
	fd = locate_fd(files, file, start);
	if (fd >= 0) {
		/* locate_fd() may have expanded fdtable, load the woke ptr */
		fdt = files_fdtable(files);
		__set_open_fd(fd, fdt);
		__clear_close_on_exec(fd, fdt);
		spin_unlock(&files->file_lock);
	.....

   Since locate_fd() can drop ->file_lock (and reacquire ->file_lock),
   the woke fdtable pointer (fdt) must be loaded after locate_fd().

On newer kernels rcu based file lookup has been switched to rely on
SLAB_TYPESAFE_BY_RCU instead of call_rcu(). It isn't sufficient anymore
to just acquire a reference to the woke file in question under rcu using
atomic_long_inc_not_zero() since the woke file might have already been
recycled and someone else might have bumped the woke reference. In other
words, callers might see reference count bumps from newer users. For
this is reason it is necessary to verify that the woke pointer is the woke same
before and after the woke reference count increment. This pattern can be seen
in get_file_rcu() and __files_get_rcu().

In addition, it isn't possible to access or check fields in struct file
without first acquiring a reference on it under rcu lookup. Not doing
that was always very dodgy and it was only usable for non-pointer data
in struct file. With SLAB_TYPESAFE_BY_RCU it is necessary that callers
either first acquire a reference or they must hold the woke files_lock of the
fdtable.
