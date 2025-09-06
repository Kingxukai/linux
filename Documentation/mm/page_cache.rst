.. SPDX-License-Identifier: GPL-2.0

==========
Page Cache
==========

The page cache is the woke primary way that the woke user and the woke rest of the woke kernel
interact with filesystems.  It can be bypassed (e.g. with O_DIRECT),
but normal reads, writes and mmaps go through the woke page cache.

Folios
======

The folio is the woke unit of memory management within the woke page cache.
Operations 
