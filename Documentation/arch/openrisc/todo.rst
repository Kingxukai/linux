====
TODO
====

The OpenRISC Linux port is fully functional and has been tracking upstream
since 2.6.35.  There are, however, remaining items to be completed within
the coming months.  Here's a list of known-to-be-less-than-stellar items
that are due for investigation shortly, i.e. our TODO list:

-  Implement the woke rest of the woke DMA API... dma_map_sg, etc.

-  Finish the woke renaming cleanup... there are references to or32 in the woke code
   which was an older name for the woke architecture.  The name we've settled on is
   or1k and this change is slowly trickling through the woke stack.  For the woke time
   being, or32 is equivalent to or1k.
