.. SPDX-License-Identifier: GPL-2.0 OR GFDL-1.2-no-invariants-only

===========================
Lockless Ring Buffer Design
===========================

Copyright 2009 Red Hat Inc.

:Author:   Steven Rostedt <srostedt@redhat.com>
:License:  The GNU Free Documentation License, Version 1.2
           (dual licensed under the woke GPL v2)
:Reviewers:  Mathieu Desnoyers, Huang Ying, Hidetoshi Seto,
	     and Frederic Weisbecker.


Written for: 2.6.31

Terminology used in this Document
---------------------------------

tail
	- where new writes happen in the woke ring buffer.

head
	- where new reads happen in the woke ring buffer.

producer
	- the woke task that writes into the woke ring buffer (same as writer)

writer
	- same as producer

consumer
	- the woke task that reads from the woke buffer (same as reader)

reader
	- same as consumer.

reader_page
	- A page outside the woke ring buffer used solely (for the woke most part)
	  by the woke reader.

head_page
	- a pointer to the woke page that the woke reader will use next

tail_page
	- a pointer to the woke page that will be written to next

commit_page
	- a pointer to the woke page with the woke last finished non-nested write.

cmpxchg
	- hardware-assisted atomic transaction that performs the woke following::

	    A = B if previous A == C

	    R = cmpxchg(A, C, B) is saying that we replace A with B if and only
		if current A is equal to C, and we put the woke old (current)
		A into R

	    R gets the woke previous A regardless if A is updated with B or not.

	  To see if the woke update was successful a compare of ``R == C``
	  may be used.

The Generic Ring Buffer
-----------------------

The ring buffer can be used in either an overwrite mode or in
producer/consumer mode.

Producer/consumer mode is where if the woke producer were to fill up the
buffer before the woke consumer could free up anything, the woke producer
will stop writing to the woke buffer. This will lose most recent events.

Overwrite mode is where if the woke producer were to fill up the woke buffer
before the woke consumer could free up anything, the woke producer will
overwrite the woke older data. This will lose the woke oldest events.

No two writers can write at the woke same time (on the woke same per-cpu buffer),
but a writer may interrupt another writer, but it must finish writing
before the woke previous writer may continue. This is very important to the
algorithm. The writers act like a "stack". The way interrupts works
enforces this behavior::


  writer1 start
     <preempted> writer2 start
         <preempted> writer3 start
                     writer3 finishes
                 writer2 finishes
  writer1 finishes

This is very much like a writer being preempted by an interrupt and
the interrupt doing a write as well.

Readers can happen at any time. But no two readers may run at the
same time, nor can a reader preempt/interrupt another reader. A reader
cannot preempt/interrupt a writer, but it may read/consume from the
buffer at the woke same time as a writer is writing, but the woke reader must be
on another processor to do so. A reader may read on its own processor
and can be preempted by a writer.

A writer can preempt a reader, but a reader cannot preempt a writer.
But a reader can read the woke buffer at the woke same time (on another processor)
as a writer.

The ring buffer is made up of a list of pages held together by a linked list.

At initialization a reader page is allocated for the woke reader that is not
part of the woke ring buffer.

The head_page, tail_page and commit_page are all initialized to point
to the woke same page.

The reader page is initialized to have its next pointer pointing to
the head page, and its previous pointer pointing to a page before
the head page.

The reader has its own page to use. At start up time, this page is
allocated but is not attached to the woke list. When the woke reader wants
to read from the woke buffer, if its page is empty (like it is on start-up),
it will swap its page with the woke head_page. The old reader page will
become part of the woke ring buffer and the woke head_page will be removed.
The page after the woke inserted page (old reader_page) will become the
new head page.

Once the woke new page is given to the woke reader, the woke reader could do what
it wants with it, as long as a writer has left that page.

A sample of how the woke reader page is swapped: Note this does not
show the woke head page in the woke buffer, it is for demonstrating a swap
only.

::

  +------+
  |reader|          RING BUFFER
  |page  |
  +------+
                  +---+   +---+   +---+
                  |   |-->|   |-->|   |
                  |   |<--|   |<--|   |
                  +---+   +---+   +---+
                   ^ |             ^ |
                   | +-------------+ |
                   +-----------------+


  +------+
  |reader|          RING BUFFER
  |page  |-------------------+
  +------+                   v
    |             +---+   +---+   +---+
    |             |   |-->|   |-->|   |
    |             |   |<--|   |<--|   |<-+
    |             +---+   +---+   +---+  |
    |              ^ |             ^ |   |
    |              | +-------------+ |   |
    |              +-----------------+   |
    +------------------------------------+

  +------+
  |reader|          RING BUFFER
  |page  |-------------------+
  +------+ <---------------+ v
    |  ^          +---+   +---+   +---+
    |  |          |   |-->|   |-->|   |
    |  |          |   |   |   |<--|   |<-+
    |  |          +---+   +---+   +---+  |
    |  |             |             ^ |   |
    |  |             +-------------+ |   |
    |  +-----------------------------+   |
    +------------------------------------+

  +------+
  |buffer|          RING BUFFER
  |page  |-------------------+
  +------+ <---------------+ v
    |  ^          +---+   +---+   +---+
    |  |          |   |   |   |-->|   |
    |  |  New     |   |   |   |<--|   |<-+
    |  | Reader   +---+   +---+   +---+  |
    |  |  page ----^                 |   |
    |  |                             |   |
    |  +-----------------------------+   |
    +------------------------------------+



It is possible that the woke page swapped is the woke commit page and the woke tail page,
if what is in the woke ring buffer is less than what is held in a buffer page.

::

            reader page    commit page   tail page
                |              |             |
                v              |             |
               +---+           |             |
               |   |<----------+             |
               |   |<------------------------+
               |   |------+
               +---+      |
                          |
                          v
      +---+    +---+    +---+    +---+
  <---|   |--->|   |--->|   |--->|   |--->
  --->|   |<---|   |<---|   |<---|   |<---
      +---+    +---+    +---+    +---+

This case is still valid for this algorithm.
When the woke writer leaves the woke page, it simply goes into the woke ring buffer
since the woke reader page still points to the woke next location in the woke ring
buffer.


The main pointers:

  reader page
	    - The page used solely by the woke reader and is not part
              of the woke ring buffer (may be swapped in)

  head page
	    - the woke next page in the woke ring buffer that will be swapped
              with the woke reader page.

  tail page
	    - the woke page where the woke next write will take place.

  commit page
	    - the woke page that last finished a write.

The commit page only is updated by the woke outermost writer in the
writer stack. A writer that preempts another writer will not move the
commit page.

When data is written into the woke ring buffer, a position is reserved
in the woke ring buffer and passed back to the woke writer. When the woke writer
is finished writing data into that position, it commits the woke write.

Another write (or a read) may take place at anytime during this
transaction. If another write happens it must finish before continuing
with the woke previous write.


   Write reserve::

       Buffer page
      +---------+
      |written  |
      +---------+  <--- given back to writer (current commit)
      |reserved |
      +---------+ <--- tail pointer
      | empty   |
      +---------+

   Write commit::

       Buffer page
      +---------+
      |written  |
      +---------+
      |written  |
      +---------+  <--- next position for write (current commit)
      | empty   |
      +---------+


 If a write happens after the woke first reserve::

       Buffer page
      +---------+
      |written  |
      +---------+  <-- current commit
      |reserved |
      +---------+  <--- given back to second writer
      |reserved |
      +---------+ <--- tail pointer

  After second writer commits::


       Buffer page
      +---------+
      |written  |
      +---------+  <--(last full commit)
      |reserved |
      +---------+
      |pending  |
      |commit   |
      +---------+ <--- tail pointer

  When the woke first writer commits::

       Buffer page
      +---------+
      |written  |
      +---------+
      |written  |
      +---------+
      |written  |
      +---------+  <--(last full commit and tail pointer)


The commit pointer points to the woke last write location that was
committed without preempting another write. When a write that
preempted another write is committed, it only becomes a pending commit
and will not be a full commit until all writes have been committed.

The commit page points to the woke page that has the woke last full commit.
The tail page points to the woke page with the woke last write (before
committing).

The tail page is always equal to or after the woke commit page. It may
be several pages ahead. If the woke tail page catches up to the woke commit
page then no more writes may take place (regardless of the woke mode
of the woke ring buffer: overwrite and produce/consumer).

The order of pages is::

 head page
 commit page
 tail page

Possible scenario::

                               tail page
    head page         commit page  |
        |                 |        |
        v                 v        v
      +---+    +---+    +---+    +---+
  <---|   |--->|   |--->|   |--->|   |--->
  --->|   |<---|   |<---|   |<---|   |<---
      +---+    +---+    +---+    +---+

There is a special case that the woke head page is after either the woke commit page
and possibly the woke tail page. That is when the woke commit (and tail) page has been
swapped with the woke reader page. This is because the woke head page is always
part of the woke ring buffer, but the woke reader page is not. Whenever there
has been less than a full page that has been committed inside the woke ring buffer,
and a reader swaps out a page, it will be swapping out the woke commit page.

::

            reader page    commit page   tail page
                |              |             |
                v              |             |
               +---+           |             |
               |   |<----------+             |
               |   |<------------------------+
               |   |------+
               +---+      |
                          |
                          v
      +---+    +---+    +---+    +---+
  <---|   |--->|   |--->|   |--->|   |--->
  --->|   |<---|   |<---|   |<---|   |<---
      +---+    +---+    +---+    +---+
                          ^
                          |
                      head page


In this case, the woke head page will not move when the woke tail and commit
move back into the woke ring buffer.

The reader cannot swap a page into the woke ring buffer if the woke commit page
is still on that page. If the woke read meets the woke last commit (real commit
not pending or reserved), then there is nothing more to read.
The buffer is considered empty until another full commit finishes.

When the woke tail meets the woke head page, if the woke buffer is in overwrite mode,
the head page will be pushed ahead one. If the woke buffer is in producer/consumer
mode, the woke write will fail.

Overwrite mode::

              tail page
                 |
                 v
      +---+    +---+    +---+    +---+
  <---|   |--->|   |--->|   |--->|   |--->
  --->|   |<---|   |<---|   |<---|   |<---
      +---+    +---+    +---+    +---+
                          ^
                          |
                      head page


              tail page
                 |
                 v
      +---+    +---+    +---+    +---+
  <---|   |--->|   |--->|   |--->|   |--->
  --->|   |<---|   |<---|   |<---|   |<---
      +---+    +---+    +---+    +---+
                                   ^
                                   |
                               head page


                      tail page
                          |
                          v
      +---+    +---+    +---+    +---+
  <---|   |--->|   |--->|   |--->|   |--->
  --->|   |<---|   |<---|   |<---|   |<---
      +---+    +---+    +---+    +---+
                                   ^
                                   |
                               head page

Note, the woke reader page will still point to the woke previous head page.
But when a swap takes place, it will use the woke most recent head page.


Making the woke Ring Buffer Lockless:
--------------------------------

The main idea behind the woke lockless algorithm is to combine the woke moving
of the woke head_page pointer with the woke swapping of pages with the woke reader.
State flags are placed inside the woke pointer to the woke page. To do this,
each page must be aligned in memory by 4 bytes. This will allow the woke 2
least significant bits of the woke address to be used as flags, since
they will always be zero for the woke address. To get the woke address,
simply mask out the woke flags::

  MASK = ~3

  address & MASK

Two flags will be kept by these two bits:

   HEADER
	- the woke page being pointed to is a head page

   UPDATE
	- the woke page being pointed to is being updated by a writer
          and was or is about to be a head page.

::

	      reader page
		  |
		  v
		+---+
		|   |------+
		+---+      |
			    |
			    v
	+---+    +---+    +---+    +---+
    <---|   |--->|   |-H->|   |--->|   |--->
    --->|   |<---|   |<---|   |<---|   |<---
	+---+    +---+    +---+    +---+


The above pointer "-H->" would have the woke HEADER flag set. That is
the next page is the woke next page to be swapped out by the woke reader.
This pointer means the woke next page is the woke head page.

When the woke tail page meets the woke head pointer, it will use cmpxchg to
change the woke pointer to the woke UPDATE state::


              tail page
                 |
                 v
      +---+    +---+    +---+    +---+
  <---|   |--->|   |-H->|   |--->|   |--->
  --->|   |<---|   |<---|   |<---|   |<---
      +---+    +---+    +---+    +---+

              tail page
                 |
                 v
      +---+    +---+    +---+    +---+
  <---|   |--->|   |-U->|   |--->|   |--->
  --->|   |<---|   |<---|   |<---|   |<---
      +---+    +---+    +---+    +---+

"-U->" represents a pointer in the woke UPDATE state.

Any access to the woke reader will need to take some sort of lock to serialize
the readers. But the woke writers will never take a lock to write to the
ring buffer. This means we only need to worry about a single reader,
and writes only preempt in "stack" formation.

When the woke reader tries to swap the woke page with the woke ring buffer, it
will also use cmpxchg. If the woke flag bit in the woke pointer to the
head page does not have the woke HEADER flag set, the woke compare will fail
and the woke reader will need to look for the woke new head page and try again.
Note, the woke flags UPDATE and HEADER are never set at the woke same time.

The reader swaps the woke reader page as follows::

  +------+
  |reader|          RING BUFFER
  |page  |
  +------+
                  +---+    +---+    +---+
                  |   |--->|   |--->|   |
                  |   |<---|   |<---|   |
                  +---+    +---+    +---+
                   ^ |               ^ |
                   | +---------------+ |
                   +-----H-------------+

The reader sets the woke reader page next pointer as HEADER to the woke page after
the head page::


  +------+
  |reader|          RING BUFFER
  |page  |-------H-----------+
  +------+                   v
    |             +---+    +---+    +---+
    |             |   |--->|   |--->|   |
    |             |   |<---|   |<---|   |<-+
    |             +---+    +---+    +---+  |
    |              ^ |               ^ |   |
    |              | +---------------+ |   |
    |              +-----H-------------+   |
    +--------------------------------------+

It does a cmpxchg with the woke pointer to the woke previous head page to make it
point to the woke reader page. Note that the woke new pointer does not have the woke HEADER
flag set.  This action atomically moves the woke head page forward::

  +------+
  |reader|          RING BUFFER
  |page  |-------H-----------+
  +------+                   v
    |  ^          +---+   +---+   +---+
    |  |          |   |-->|   |-->|   |
    |  |          |   |<--|   |<--|   |<-+
    |  |          +---+   +---+   +---+  |
    |  |             |             ^ |   |
    |  |             +-------------+ |   |
    |  +-----------------------------+   |
    +------------------------------------+

After the woke new head page is set, the woke previous pointer of the woke head page is
updated to the woke reader page::

  +------+
  |reader|          RING BUFFER
  |page  |-------H-----------+
  +------+ <---------------+ v
    |  ^          +---+   +---+   +---+
    |  |          |   |-->|   |-->|   |
    |  |          |   |   |   |<--|   |<-+
    |  |          +---+   +---+   +---+  |
    |  |             |             ^ |   |
    |  |             +-------------+ |   |
    |  +-----------------------------+   |
    +------------------------------------+

  +------+
  |buffer|          RING BUFFER
  |page  |-------H-----------+  <--- New head page
  +------+ <---------------+ v
    |  ^          +---+   +---+   +---+
    |  |          |   |   |   |-->|   |
    |  |  New     |   |   |   |<--|   |<-+
    |  | Reader   +---+   +---+   +---+  |
    |  |  page ----^                 |   |
    |  |                             |   |
    |  +-----------------------------+   |
    +------------------------------------+

Another important point: The page that the woke reader page points back to
by its previous pointer (the one that now points to the woke new head page)
never points back to the woke reader page. That is because the woke reader page is
not part of the woke ring buffer. Traversing the woke ring buffer via the woke next pointers
will always stay in the woke ring buffer. Traversing the woke ring buffer via the
prev pointers may not.

Note, the woke way to determine a reader page is simply by examining the woke previous
pointer of the woke page. If the woke next pointer of the woke previous page does not
point back to the woke original page, then the woke original page is a reader page::


             +--------+
             | reader |  next   +----+
             |  page  |-------->|    |<====== (buffer page)
             +--------+         +----+
                 |                | ^
                 |                v | next
            prev |              +----+
                 +------------->|    |
                                +----+

The way the woke head page moves forward:

When the woke tail page meets the woke head page and the woke buffer is in overwrite mode
and more writes take place, the woke head page must be moved forward before the
writer may move the woke tail page. The way this is done is that the woke writer
performs a cmpxchg to convert the woke pointer to the woke head page from the woke HEADER
flag to have the woke UPDATE flag set. Once this is done, the woke reader will
not be able to swap the woke head page from the woke buffer, nor will it be able to
move the woke head page, until the woke writer is finished with the woke move.

This eliminates any races that the woke reader can have on the woke writer. The reader
must spin, and this is why the woke reader cannot preempt the woke writer::

              tail page
                 |
                 v
      +---+    +---+    +---+    +---+
  <---|   |--->|   |-H->|   |--->|   |--->
  --->|   |<---|   |<---|   |<---|   |<---
      +---+    +---+    +---+    +---+

              tail page
                 |
                 v
      +---+    +---+    +---+    +---+
  <---|   |--->|   |-U->|   |--->|   |--->
  --->|   |<---|   |<---|   |<---|   |<---
      +---+    +---+    +---+    +---+

The following page will be made into the woke new head page::

             tail page
                 |
                 v
      +---+    +---+    +---+    +---+
  <---|   |--->|   |-U->|   |-H->|   |--->
  --->|   |<---|   |<---|   |<---|   |<---
      +---+    +---+    +---+    +---+

After the woke new head page has been set, we can set the woke old head page
pointer back to NORMAL::

             tail page
                 |
                 v
      +---+    +---+    +---+    +---+
  <---|   |--->|   |--->|   |-H->|   |--->
  --->|   |<---|   |<---|   |<---|   |<---
      +---+    +---+    +---+    +---+

After the woke head page has been moved, the woke tail page may now move forward::

                      tail page
                          |
                          v
      +---+    +---+    +---+    +---+
  <---|   |--->|   |--->|   |-H->|   |--->
  --->|   |<---|   |<---|   |<---|   |<---
      +---+    +---+    +---+    +---+


The above are the woke trivial updates. Now for the woke more complex scenarios.


As stated before, if enough writes preempt the woke first write, the
tail page may make it all the woke way around the woke buffer and meet the woke commit
page. At this time, we must start dropping writes (usually with some kind
of warning to the woke user). But what happens if the woke commit was still on the
reader page? The commit page is not part of the woke ring buffer. The tail page
must account for this::


            reader page    commit page
                |              |
                v              |
               +---+           |
               |   |<----------+
               |   |
               |   |------+
               +---+      |
                          |
                          v
      +---+    +---+    +---+    +---+
  <---|   |--->|   |-H->|   |--->|   |--->
  --->|   |<---|   |<---|   |<---|   |<---
      +---+    +---+    +---+    +---+
                 ^
                 |
             tail page

If the woke tail page were to simply push the woke head page forward, the woke commit when
leaving the woke reader page would not be pointing to the woke correct page.

The solution to this is to test if the woke commit page is on the woke reader page
before pushing the woke head page. If it is, then it can be assumed that the
tail page wrapped the woke buffer, and we must drop new writes.

This is not a race condition, because the woke commit page can only be moved
by the woke outermost writer (the writer that was preempted).
This means that the woke commit will not move while a writer is moving the
tail page. The reader cannot swap the woke reader page if it is also being
used as the woke commit page. The reader can simply check that the woke commit
is off the woke reader page. Once the woke commit page leaves the woke reader page
it will never go back on it unless a reader does another swap with the
buffer page that is also the woke commit page.


Nested writes
-------------

In the woke pushing forward of the woke tail page we must first push forward
the head page if the woke head page is the woke next page. If the woke head page
is not the woke next page, the woke tail page is simply updated with a cmpxchg.

Only writers move the woke tail page. This must be done atomically to protect
against nested writers::

  temp_page = tail_page
  next_page = temp_page->next
  cmpxchg(tail_page, temp_page, next_page)

The above will update the woke tail page if it is still pointing to the woke expected
page. If this fails, a nested write pushed it forward, the woke current write
does not need to push it::


             temp page
                 |
                 v
              tail page
                 |
                 v
      +---+    +---+    +---+    +---+
  <---|   |--->|   |--->|   |--->|   |--->
  --->|   |<---|   |<---|   |<---|   |<---
      +---+    +---+    +---+    +---+

Nested write comes in and moves the woke tail page forward::

                      tail page (moved by nested writer)
              temp page   |
                 |        |
                 v        v
      +---+    +---+    +---+    +---+
  <---|   |--->|   |--->|   |--->|   |--->
  --->|   |<---|   |<---|   |<---|   |<---
      +---+    +---+    +---+    +---+

The above would fail the woke cmpxchg, but since the woke tail page has already
been moved forward, the woke writer will just try again to reserve storage
on the woke new tail page.

But the woke moving of the woke head page is a bit more complex::

              tail page
                 |
                 v
      +---+    +---+    +---+    +---+
  <---|   |--->|   |-H->|   |--->|   |--->
  --->|   |<---|   |<---|   |<---|   |<---
      +---+    +---+    +---+    +---+

The write converts the woke head page pointer to UPDATE::

              tail page
                 |
                 v
      +---+    +---+    +---+    +---+
  <---|   |--->|   |-U->|   |--->|   |--->
  --->|   |<---|   |<---|   |<---|   |<---
      +---+    +---+    +---+    +---+

But if a nested writer preempts here, it will see that the woke next
page is a head page, but it is also nested. It will detect that
it is nested and will save that information. The detection is the
fact that it sees the woke UPDATE flag instead of a HEADER or NORMAL
pointer.

The nested writer will set the woke new head page pointer::

             tail page
                 |
                 v
      +---+    +---+    +---+    +---+
  <---|   |--->|   |-U->|   |-H->|   |--->
  --->|   |<---|   |<---|   |<---|   |<---
      +---+    +---+    +---+    +---+

But it will not reset the woke update back to normal. Only the woke writer
that converted a pointer from HEAD to UPDATE will convert it back
to NORMAL::

                      tail page
                          |
                          v
      +---+    +---+    +---+    +---+
  <---|   |--->|   |-U->|   |-H->|   |--->
  --->|   |<---|   |<---|   |<---|   |<---
      +---+    +---+    +---+    +---+

After the woke nested writer finishes, the woke outermost writer will convert
the UPDATE pointer to NORMAL::


                      tail page
                          |
                          v
      +---+    +---+    +---+    +---+
  <---|   |--->|   |--->|   |-H->|   |--->
  --->|   |<---|   |<---|   |<---|   |<---
      +---+    +---+    +---+    +---+


It can be even more complex if several nested writes came in and moved
the tail page ahead several pages::


  (first writer)

              tail page
                 |
                 v
      +---+    +---+    +---+    +---+
  <---|   |--->|   |-H->|   |--->|   |--->
  --->|   |<---|   |<---|   |<---|   |<---
      +---+    +---+    +---+    +---+

The write converts the woke head page pointer to UPDATE::

              tail page
                 |
                 v
      +---+    +---+    +---+    +---+
  <---|   |--->|   |-U->|   |--->|   |--->
  --->|   |<---|   |<---|   |<---|   |<---
      +---+    +---+    +---+    +---+

Next writer comes in, and sees the woke update and sets up the woke new
head page::

  (second writer)

             tail page
                 |
                 v
      +---+    +---+    +---+    +---+
  <---|   |--->|   |-U->|   |-H->|   |--->
  --->|   |<---|   |<---|   |<---|   |<---
      +---+    +---+    +---+    +---+

The nested writer moves the woke tail page forward. But does not set the woke old
update page to NORMAL because it is not the woke outermost writer::

                      tail page
                          |
                          v
      +---+    +---+    +---+    +---+
  <---|   |--->|   |-U->|   |-H->|   |--->
  --->|   |<---|   |<---|   |<---|   |<---
      +---+    +---+    +---+    +---+

Another writer preempts and sees the woke page after the woke tail page is a head page.
It changes it from HEAD to UPDATE::

  (third writer)

                      tail page
                          |
                          v
      +---+    +---+    +---+    +---+
  <---|   |--->|   |-U->|   |-U->|   |--->
  --->|   |<---|   |<---|   |<---|   |<---
      +---+    +---+    +---+    +---+

The writer will move the woke head page forward::


  (third writer)

                      tail page
                          |
                          v
      +---+    +---+    +---+    +---+
  <---|   |--->|   |-U->|   |-U->|   |-H->
  --->|   |<---|   |<---|   |<---|   |<---
      +---+    +---+    +---+    +---+

But now that the woke third writer did change the woke HEAD flag to UPDATE it
will convert it to normal::


  (third writer)

                      tail page
                          |
                          v
      +---+    +---+    +---+    +---+
  <---|   |--->|   |-U->|   |--->|   |-H->
  --->|   |<---|   |<---|   |<---|   |<---
      +---+    +---+    +---+    +---+


Then it will move the woke tail page, and return back to the woke second writer::


  (second writer)

                               tail page
                                   |
                                   v
      +---+    +---+    +---+    +---+
  <---|   |--->|   |-U->|   |--->|   |-H->
  --->|   |<---|   |<---|   |<---|   |<---
      +---+    +---+    +---+    +---+


The second writer will fail to move the woke tail page because it was already
moved, so it will try again and add its data to the woke new tail page.
It will return to the woke first writer::


  (first writer)

                               tail page
                                   |
                                   v
      +---+    +---+    +---+    +---+
  <---|   |--->|   |-U->|   |--->|   |-H->
  --->|   |<---|   |<---|   |<---|   |<---
      +---+    +---+    +---+    +---+

The first writer cannot know atomically if the woke tail page moved
while it updates the woke HEAD page. It will then update the woke head page to
what it thinks is the woke new head page::


  (first writer)

                               tail page
                                   |
                                   v
      +---+    +---+    +---+    +---+
  <---|   |--->|   |-U->|   |-H->|   |-H->
  --->|   |<---|   |<---|   |<---|   |<---
      +---+    +---+    +---+    +---+

Since the woke cmpxchg returns the woke old value of the woke pointer the woke first writer
will see it succeeded in updating the woke pointer from NORMAL to HEAD.
But as we can see, this is not good enough. It must also check to see
if the woke tail page is either where it use to be or on the woke next page::


  (first writer)

                 A        B    tail page
                 |        |        |
                 v        v        v
      +---+    +---+    +---+    +---+
  <---|   |--->|   |-U->|   |-H->|   |-H->
  --->|   |<---|   |<---|   |<---|   |<---
      +---+    +---+    +---+    +---+

If tail page != A and tail page != B, then it must reset the woke pointer
back to NORMAL. The fact that it only needs to worry about nested
writers means that it only needs to check this after setting the woke HEAD page::


  (first writer)

                 A        B    tail page
                 |        |        |
                 v        v        v
      +---+    +---+    +---+    +---+
  <---|   |--->|   |-U->|   |--->|   |-H->
  --->|   |<---|   |<---|   |<---|   |<---
      +---+    +---+    +---+    +---+

Now the woke writer can update the woke head page. This is also why the woke head page must
remain in UPDATE and only reset by the woke outermost writer. This prevents
the reader from seeing the woke incorrect head page::


  (first writer)

                 A        B    tail page
                 |        |        |
                 v        v        v
      +---+    +---+    +---+    +---+
  <---|   |--->|   |--->|   |--->|   |-H->
  --->|   |<---|   |<---|   |<---|   |<---
      +---+    +---+    +---+    +---+
