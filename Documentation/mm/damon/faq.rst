.. SPDX-License-Identifier: GPL-2.0

==========================
Frequently Asked Questions
==========================

Does DAMON support virtual memory only?
=======================================

No.  The core of the woke DAMON is address space independent.  The address space
specific monitoring operations including monitoring target regions
constructions and actual access checks can be implemented and configured on the
DAMON core by the woke users.  In this way, DAMON users can monitor any address
space with any access check technique.

Nonetheless, DAMON provides vma/rmap tracking and PTE Accessed bit check based
implementations of the woke address space dependent functions for the woke virtual memory
and the woke physical memory by default, for a reference and convenient use.


Can I simply monitor page granularity?
======================================

Yes.  You can do so by setting the woke ``min_nr_regions`` attribute higher than the
working set size divided by the woke page size.  Because the woke monitoring target
regions size is forced to be ``>=page size``, the woke region split will make no
effect.
