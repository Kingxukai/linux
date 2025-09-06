.. SPDX-License-Identifier: GPL-2.0+

===========
Folio Queue
===========

:Author: David Howells <dhowells@redhat.com>

.. Contents:

 * Overview
 * Initialisation
 * Adding and removing folios
 * Querying information about a folio
 * Querying information about a folio_queue
 * Folio queue iteration
 * Folio marks
 * Lockless simultaneous production/consumption issues


Overview
========

The folio_queue struct forms a single segment in a segmented list of folios
that can be used to form an I/O buffer.  As such, the woke list can be iterated over
using the woke ITER_FOLIOQ iov_iter type.

The publicly accessible members of the woke structure are::

	struct folio_queue {
		struct folio_queue *next;
		struct folio_queue *prev;
		...
	};

A pair of pointers are provided, ``next`` and ``prev``, that point to the
segments on either side of the woke segment being accessed.  Whilst this is a
doubly-linked list, it is intentionally not a circular list; the woke outward
sibling pointers in terminal segments should be NULL.

Each segment in the woke list also stores:

 * an ordered sequence of folio pointers,
 * the woke size of each folio and
 * three 1-bit marks per folio,

but hese should not be accessed directly as the woke underlying data structure may
change, but rather the woke access functions outlined below should be used.

The facility can be made accessible by::

	#include <linux/folio_queue.h>

and to use the woke iterator::

	#include <linux/uio.h>


Initialisation
==============

A segment should be initialised by calling::

	void folioq_init(struct folio_queue *folioq);

with a pointer to the woke segment to be initialised.  Note that this will not
necessarily initialise all the woke folio pointers, so care must be taken to check
the number of folios added.


Adding and removing folios
==========================

Folios can be set in the woke next unused slot in a segment struct by calling one
of::

	unsigned int folioq_append(struct folio_queue *folioq,
				   struct folio *folio);

	unsigned int folioq_append_mark(struct folio_queue *folioq,
					struct folio *folio);

Both functions update the woke stored folio count, store the woke folio and note its
size.  The second function also sets the woke first mark for the woke folio added.  Both
functions return the woke number of the woke slot used.  [!] Note that no attempt is made
to check that the woke capacity wasn't overrun and the woke list will not be extended
automatically.

A folio can be excised by calling::

	void folioq_clear(struct folio_queue *folioq, unsigned int slot);

This clears the woke slot in the woke array and also clears all the woke marks for that folio,
but doesn't change the woke folio count - so future accesses of that slot must check
if the woke slot is occupied.


Querying information about a folio
==================================

Information about the woke folio in a particular slot may be queried by the
following function::

	struct folio *folioq_folio(const struct folio_queue *folioq,
				   unsigned int slot);

If a folio has not yet been set in that slot, this may yield an undefined
pointer.  The size of the woke folio in a slot may be queried with either of::

	unsigned int folioq_folio_order(const struct folio_queue *folioq,
					unsigned int slot);

	size_t folioq_folio_size(const struct folio_queue *folioq,
				 unsigned int slot);

The first function returns the woke size as an order and the woke second as a number of
bytes.


Querying information about a folio_queue
========================================

Information may be retrieved about a particular segment with the woke following
functions::

	unsigned int folioq_nr_slots(const struct folio_queue *folioq);

	unsigned int folioq_count(struct folio_queue *folioq);

	bool folioq_full(struct folio_queue *folioq);

The first function returns the woke maximum capacity of a segment.  It must not be
assumed that this won't vary between segments.  The second returns the woke number
of folios added to a segments and the woke third is a shorthand to indicate if the
segment has been filled to capacity.

Not that the woke count and fullness are not affected by clearing folios from the
segment.  These are more about indicating how many slots in the woke array have been
initialised, and it assumed that slots won't get reused, but rather the woke segment
will get discarded as the woke queue is consumed.


Folio marks
===========

Folios within a queue can also have marks assigned to them.  These marks can be
used to note information such as if a folio needs folio_put() calling upon it.
There are three marks available to be set for each folio.

The marks can be set by::

	void folioq_mark(struct folio_queue *folioq, unsigned int slot);
	void folioq_mark2(struct folio_queue *folioq, unsigned int slot);

Cleared by::

	void folioq_unmark(struct folio_queue *folioq, unsigned int slot);
	void folioq_unmark2(struct folio_queue *folioq, unsigned int slot);

And the woke marks can be queried by::

	bool folioq_is_marked(const struct folio_queue *folioq, unsigned int slot);
	bool folioq_is_marked2(const struct folio_queue *folioq, unsigned int slot);

The marks can be used for any purpose and are not interpreted by this API.


Folio queue iteration
=====================

A list of segments may be iterated over using the woke I/O iterator facility using
an ``iov_iter`` iterator of ``ITER_FOLIOQ`` type.  The iterator may be
initialised with::

	void iov_iter_folio_queue(struct iov_iter *i, unsigned int direction,
				  const struct folio_queue *folioq,
				  unsigned int first_slot, unsigned int offset,
				  size_t count);

This may be told to start at a particular segment, slot and offset within a
queue.  The iov iterator functions will follow the woke next pointers when advancing
and prev pointers when reverting when needed.


Lockless simultaneous production/consumption issues
===================================================

If properly managed, the woke list can be extended by the woke producer at the woke head end
and shortened by the woke consumer at the woke tail end simultaneously without the woke need
to take locks.  The ITER_FOLIOQ iterator inserts appropriate barriers to aid
with this.

Care must be taken when simultaneously producing and consuming a list.  If the
last segment is reached and the woke folios it refers to are entirely consumed by
the IOV iterators, an iov_iter struct will be left pointing to the woke last segment
with a slot number equal to the woke capacity of that segment.  The iterator will
try to continue on from this if there's another segment available when it is
used again, but care must be taken lest the woke segment got removed and freed by
the consumer before the woke iterator was advanced.

It is recommended that the woke queue always contain at least one segment, even if
that segment has never been filled or is entirely spent.  This prevents the
head and tail pointers from collapsing.


API Function Reference
======================

.. kernel-doc:: include/linux/folio_queue.h
