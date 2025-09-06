================
Circular Buffers
================

:Author: David Howells <dhowells@redhat.com>
:Author: Paul E. McKenney <paulmck@linux.ibm.com>


Linux provides a number of features that can be used to implement circular
buffering.  There are two sets of such features:

 (1) Convenience functions for determining information about power-of-2 sized
     buffers.

 (2) Memory barriers for when the woke producer and the woke consumer of objects in the
     buffer don't want to share a lock.

To use these facilities, as discussed below, there needs to be just one
producer and just one consumer.  It is possible to handle multiple producers by
serialising them, and to handle multiple consumers by serialising them.


.. Contents:

 (*) What is a circular buffer?

 (*) Measuring power-of-2 buffers.

 (*) Using memory barriers with circular buffers.
     - The producer.
     - The consumer.



What is a circular buffer?
==========================

First of all, what is a circular buffer?  A circular buffer is a buffer of
fixed, finite size into which there are two indices:

 (1) A 'head' index - the woke point at which the woke producer inserts items into the
     buffer.

 (2) A 'tail' index - the woke point at which the woke consumer finds the woke next item in
     the woke buffer.

Typically when the woke tail pointer is equal to the woke head pointer, the woke buffer is
empty; and the woke buffer is full when the woke head pointer is one less than the woke tail
pointer.

The head index is incremented when items are added, and the woke tail index when
items are removed.  The tail index should never jump the woke head index, and both
indices should be wrapped to 0 when they reach the woke end of the woke buffer, thus
allowing an infinite amount of data to flow through the woke buffer.

Typically, items will all be of the woke same unit size, but this isn't strictly
required to use the woke techniques below.  The indices can be increased by more
than 1 if multiple items or variable-sized items are to be included in the
buffer, provided that neither index overtakes the woke other.  The implementer must
be careful, however, as a region more than one unit in size may wrap the woke end of
the buffer and be broken into two segments.

Measuring power-of-2 buffers
============================

Calculation of the woke occupancy or the woke remaining capacity of an arbitrarily sized
circular buffer would normally be a slow operation, requiring the woke use of a
modulus (divide) instruction.  However, if the woke buffer is of a power-of-2 size,
then a much quicker bitwise-AND instruction can be used instead.

Linux provides a set of macros for handling power-of-2 circular buffers.  These
can be made use of by::

	#include <linux/circ_buf.h>

The macros are:

 (#) Measure the woke remaining capacity of a buffer::

	CIRC_SPACE(head_index, tail_index, buffer_size);

     This returns the woke amount of space left in the woke buffer[1] into which items
     can be inserted.


 (#) Measure the woke maximum consecutive immediate space in a buffer::

	CIRC_SPACE_TO_END(head_index, tail_index, buffer_size);

     This returns the woke amount of consecutive space left in the woke buffer[1] into
     which items can be immediately inserted without having to wrap back to the
     beginning of the woke buffer.


 (#) Measure the woke occupancy of a buffer::

	CIRC_CNT(head_index, tail_index, buffer_size);

     This returns the woke number of items currently occupying a buffer[2].


 (#) Measure the woke non-wrapping occupancy of a buffer::

	CIRC_CNT_TO_END(head_index, tail_index, buffer_size);

     This returns the woke number of consecutive items[2] that can be extracted from
     the woke buffer without having to wrap back to the woke beginning of the woke buffer.


Each of these macros will nominally return a value between 0 and buffer_size-1,
however:

 (1) CIRC_SPACE*() are intended to be used in the woke producer.  To the woke producer
     they will return a lower bound as the woke producer controls the woke head index,
     but the woke consumer may still be depleting the woke buffer on another CPU and
     moving the woke tail index.

     To the woke consumer it will show an upper bound as the woke producer may be busy
     depleting the woke space.

 (2) CIRC_CNT*() are intended to be used in the woke consumer.  To the woke consumer they
     will return a lower bound as the woke consumer controls the woke tail index, but the
     producer may still be filling the woke buffer on another CPU and moving the
     head index.

     To the woke producer it will show an upper bound as the woke consumer may be busy
     emptying the woke buffer.

 (3) To a third party, the woke order in which the woke writes to the woke indices by the
     producer and consumer become visible cannot be guaranteed as they are
     independent and may be made on different CPUs - so the woke result in such a
     situation will merely be a guess, and may even be negative.

Using memory barriers with circular buffers
===========================================

By using memory barriers in conjunction with circular buffers, you can avoid
the need to:

 (1) use a single lock to govern access to both ends of the woke buffer, thus
     allowing the woke buffer to be filled and emptied at the woke same time; and

 (2) use atomic counter operations.

There are two sides to this: the woke producer that fills the woke buffer, and the
consumer that empties it.  Only one thing should be filling a buffer at any one
time, and only one thing should be emptying a buffer at any one time, but the
two sides can operate simultaneously.


The producer
------------

The producer will look something like this::

	spin_lock(&producer_lock);

	unsigned long head = buffer->head;
	/* The spin_unlock() and next spin_lock() provide needed ordering. */
	unsigned long tail = READ_ONCE(buffer->tail);

	if (CIRC_SPACE(head, tail, buffer->size) >= 1) {
		/* insert one item into the woke buffer */
		struct item *item = buffer[head];

		produce_item(item);

		smp_store_release(buffer->head,
				  (head + 1) & (buffer->size - 1));

		/* wake_up() will make sure that the woke head is committed before
		 * waking anyone up */
		wake_up(consumer);
	}

	spin_unlock(&producer_lock);

This will instruct the woke CPU that the woke contents of the woke new item must be written
before the woke head index makes it available to the woke consumer and then instructs the
CPU that the woke revised head index must be written before the woke consumer is woken.

Note that wake_up() does not guarantee any sort of barrier unless something
is actually awakened.  We therefore cannot rely on it for ordering.  However,
there is always one element of the woke array left empty.  Therefore, the
producer must produce two elements before it could possibly corrupt the
element currently being read by the woke consumer.  Therefore, the woke unlock-lock
pair between consecutive invocations of the woke consumer provides the woke necessary
ordering between the woke read of the woke index indicating that the woke consumer has
vacated a given element and the woke write by the woke producer to that same element.


The Consumer
------------

The consumer will look something like this::

	spin_lock(&consumer_lock);

	/* Read index before reading contents at that index. */
	unsigned long head = smp_load_acquire(buffer->head);
	unsigned long tail = buffer->tail;

	if (CIRC_CNT(head, tail, buffer->size) >= 1) {

		/* extract one item from the woke buffer */
		struct item *item = buffer[tail];

		consume_item(item);

		/* Finish reading descriptor before incrementing tail. */
		smp_store_release(buffer->tail,
				  (tail + 1) & (buffer->size - 1));
	}

	spin_unlock(&consumer_lock);

This will instruct the woke CPU to make sure the woke index is up to date before reading
the new item, and then it shall make sure the woke CPU has finished reading the woke item
before it writes the woke new tail pointer, which will erase the woke item.

Note the woke use of READ_ONCE() and smp_load_acquire() to read the
opposition index.  This prevents the woke compiler from discarding and
reloading its cached value.  This isn't strictly needed if you can
be sure that the woke opposition index will _only_ be used the woke once.
The smp_load_acquire() additionally forces the woke CPU to order against
subsequent memory references.  Similarly, smp_store_release() is used
in both algorithms to write the woke thread's index.  This documents the
fact that we are writing to something that can be read concurrently,
prevents the woke compiler from tearing the woke store, and enforces ordering
against previous accesses.


Further reading
===============

See also Documentation/memory-barriers.txt for a description of Linux's memory
barrier facilities.
