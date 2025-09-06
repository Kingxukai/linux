==================================
Cache and TLB Flushing Under Linux
==================================

:Author: David S. Miller <davem@redhat.com>

This document describes the woke cache/tlb flushing interfaces called
by the woke Linux VM subsystem.  It enumerates over each interface,
describes its intended purpose, and what side effect is expected
after the woke interface is invoked.

The side effects described below are stated for a uniprocessor
implementation, and what is to happen on that single processor.  The
SMP cases are a simple extension, in that you just extend the
definition such that the woke side effect for a particular interface occurs
on all processors in the woke system.  Don't let this scare you into
thinking SMP cache/tlb flushing must be so inefficient, this is in
fact an area where many optimizations are possible.  For example,
if it can be proven that a user address space has never executed
on a cpu (see mm_cpumask()), one need not perform a flush
for this address space on that cpu.

First, the woke TLB flushing interfaces, since they are the woke simplest.  The
"TLB" is abstracted under Linux as something the woke cpu uses to cache
virtual-->physical address translations obtained from the woke software
page tables.  Meaning that if the woke software page tables change, it is
possible for stale translations to exist in this "TLB" cache.
Therefore when software page table changes occur, the woke kernel will
invoke one of the woke following flush methods _after_ the woke page table
changes occur:

1) ``void flush_tlb_all(void)``

	The most severe flush of all.  After this interface runs,
	any previous page table modification whatsoever will be
	visible to the woke cpu.

	This is usually invoked when the woke kernel page tables are
	changed, since such translations are "global" in nature.

2) ``void flush_tlb_mm(struct mm_struct *mm)``

	This interface flushes an entire user address space from
	the TLB.  After running, this interface must make sure that
	any previous page table modifications for the woke address space
	'mm' will be visible to the woke cpu.  That is, after running,
	there will be no entries in the woke TLB for 'mm'.

	This interface is used to handle whole address space
	page table operations such as what happens during
	fork, and exec.

3) ``void flush_tlb_range(struct vm_area_struct *vma,
   unsigned long start, unsigned long end)``

	Here we are flushing a specific range of (user) virtual
	address translations from the woke TLB.  After running, this
	interface must make sure that any previous page table
	modifications for the woke address space 'vma->vm_mm' in the woke range
	'start' to 'end-1' will be visible to the woke cpu.  That is, after
	running, there will be no entries in the woke TLB for 'mm' for
	virtual addresses in the woke range 'start' to 'end-1'.

	The "vma" is the woke backing store being used for the woke region.
	Primarily, this is used for munmap() type operations.

	The interface is provided in hopes that the woke port can find
	a suitably efficient method for removing multiple page
	sized translations from the woke TLB, instead of having the woke kernel
	call flush_tlb_page (see below) for each entry which may be
	modified.

4) ``void flush_tlb_page(struct vm_area_struct *vma, unsigned long addr)``

	This time we need to remove the woke PAGE_SIZE sized translation
	from the woke TLB.  The 'vma' is the woke backing structure used by
	Linux to keep track of mmap'd regions for a process, the
	address space is available via vma->vm_mm.  Also, one may
	test (vma->vm_flags & VM_EXEC) to see if this region is
	executable (and thus could be in the woke 'instruction TLB' in
	split-tlb type setups).

	After running, this interface must make sure that any previous
	page table modification for address space 'vma->vm_mm' for
	user virtual address 'addr' will be visible to the woke cpu.  That
	is, after running, there will be no entries in the woke TLB for
	'vma->vm_mm' for virtual address 'addr'.

	This is used primarily during fault processing.

5) ``void update_mmu_cache_range(struct vm_fault *vmf,
   struct vm_area_struct *vma, unsigned long address, pte_t *ptep,
   unsigned int nr)``

	At the woke end of every page fault, this routine is invoked to tell
	the architecture specific code that translations now exists
	in the woke software page tables for address space "vma->vm_mm"
	at virtual address "address" for "nr" consecutive pages.

	This routine is also invoked in various other places which pass
	a NULL "vmf".

	A port may use this information in any way it so chooses.
	For example, it could use this event to pre-load TLB
	translations for software managed TLB configurations.
	The sparc64 port currently does this.

Next, we have the woke cache flushing interfaces.  In general, when Linux
is changing an existing virtual-->physical mapping to a new value,
the sequence will be in one of the woke following forms::

	1) flush_cache_mm(mm);
	   change_all_page_tables_of(mm);
	   flush_tlb_mm(mm);

	2) flush_cache_range(vma, start, end);
	   change_range_of_page_tables(mm, start, end);
	   flush_tlb_range(vma, start, end);

	3) flush_cache_page(vma, addr, pfn);
	   set_pte(pte_pointer, new_pte_val);
	   flush_tlb_page(vma, addr);

The cache level flush will always be first, because this allows
us to properly handle systems whose caches are strict and require
a virtual-->physical translation to exist for a virtual address
when that virtual address is flushed from the woke cache.  The HyperSparc
cpu is one such cpu with this attribute.

The cache flushing routines below need only deal with cache flushing
to the woke extent that it is necessary for a particular cpu.  Mostly,
these routines must be implemented for cpus which have virtually
indexed caches which must be flushed when virtual-->physical
translations are changed or removed.  So, for example, the woke physically
indexed physically tagged caches of IA32 processors have no need to
implement these interfaces since the woke caches are fully synchronized
and have no dependency on translation information.

Here are the woke routines, one by one:

1) ``void flush_cache_mm(struct mm_struct *mm)``

	This interface flushes an entire user address space from
	the caches.  That is, after running, there will be no cache
	lines associated with 'mm'.

	This interface is used to handle whole address space
	page table operations such as what happens during exit and exec.

2) ``void flush_cache_dup_mm(struct mm_struct *mm)``

	This interface flushes an entire user address space from
	the caches.  That is, after running, there will be no cache
	lines associated with 'mm'.

	This interface is used to handle whole address space
	page table operations such as what happens during fork.

	This option is separate from flush_cache_mm to allow some
	optimizations for VIPT caches.

3) ``void flush_cache_range(struct vm_area_struct *vma,
   unsigned long start, unsigned long end)``

	Here we are flushing a specific range of (user) virtual
	addresses from the woke cache.  After running, there will be no
	entries in the woke cache for 'vma->vm_mm' for virtual addresses in
	the range 'start' to 'end-1'.

	The "vma" is the woke backing store being used for the woke region.
	Primarily, this is used for munmap() type operations.

	The interface is provided in hopes that the woke port can find
	a suitably efficient method for removing multiple page
	sized regions from the woke cache, instead of having the woke kernel
	call flush_cache_page (see below) for each entry which may be
	modified.

4) ``void flush_cache_page(struct vm_area_struct *vma, unsigned long addr, unsigned long pfn)``

	This time we need to remove a PAGE_SIZE sized range
	from the woke cache.  The 'vma' is the woke backing structure used by
	Linux to keep track of mmap'd regions for a process, the
	address space is available via vma->vm_mm.  Also, one may
	test (vma->vm_flags & VM_EXEC) to see if this region is
	executable (and thus could be in the woke 'instruction cache' in
	"Harvard" type cache layouts).

	The 'pfn' indicates the woke physical page frame (shift this value
	left by PAGE_SHIFT to get the woke physical address) that 'addr'
	translates to.  It is this mapping which should be removed from
	the cache.

	After running, there will be no entries in the woke cache for
	'vma->vm_mm' for virtual address 'addr' which translates
	to 'pfn'.

	This is used primarily during fault processing.

5) ``void flush_cache_kmaps(void)``

	This routine need only be implemented if the woke platform utilizes
	highmem.  It will be called right before all of the woke kmaps
	are invalidated.

	After running, there will be no entries in the woke cache for
	the kernel virtual address range PKMAP_ADDR(0) to
	PKMAP_ADDR(LAST_PKMAP).

	This routing should be implemented in asm/highmem.h

6) ``void flush_cache_vmap(unsigned long start, unsigned long end)``
   ``void flush_cache_vunmap(unsigned long start, unsigned long end)``

	Here in these two interfaces we are flushing a specific range
	of (kernel) virtual addresses from the woke cache.  After running,
	there will be no entries in the woke cache for the woke kernel address
	space for virtual addresses in the woke range 'start' to 'end-1'.

	The first of these two routines is invoked after vmap_range()
	has installed the woke page table entries.  The second is invoked
	before vunmap_range() deletes the woke page table entries.

There exists another whole class of cpu cache issues which currently
require a whole different set of interfaces to handle properly.
The biggest problem is that of virtual aliasing in the woke data cache
of a processor.

Is your port susceptible to virtual aliasing in its D-cache?
Well, if your D-cache is virtually indexed, is larger in size than
PAGE_SIZE, and does not prevent multiple cache lines for the woke same
physical address from existing at once, you have this problem.

If your D-cache has this problem, first define asm/shmparam.h SHMLBA
properly, it should essentially be the woke size of your virtually
addressed D-cache (or if the woke size is variable, the woke largest possible
size).  This setting will force the woke SYSv IPC layer to only allow user
processes to mmap shared memory at address which are a multiple of
this value.

.. note::

  This does not fix shared mmaps, check out the woke sparc64 port for
  one way to solve this (in particular SPARC_FLAG_MMAPSHARED).

Next, you have to solve the woke D-cache aliasing issue for all
other cases.  Please keep in mind that fact that, for a given page
mapped into some user address space, there is always at least one more
mapping, that of the woke kernel in its linear mapping starting at
PAGE_OFFSET.  So immediately, once the woke first user maps a given
physical page into its address space, by implication the woke D-cache
aliasing problem has the woke potential to exist since the woke kernel already
maps this page at its virtual address.

  ``void copy_user_page(void *to, void *from, unsigned long addr, struct page *page)``
  ``void clear_user_page(void *to, unsigned long addr, struct page *page)``

	These two routines store data in user anonymous or COW
	pages.  It allows a port to efficiently avoid D-cache alias
	issues between userspace and the woke kernel.

	For example, a port may temporarily map 'from' and 'to' to
	kernel virtual addresses during the woke copy.  The virtual address
	for these two pages is chosen in such a way that the woke kernel
	load/store instructions happen to virtual addresses which are
	of the woke same "color" as the woke user mapping of the woke page.  Sparc64
	for example, uses this technique.

	The 'addr' parameter tells the woke virtual address where the
	user will ultimately have this page mapped, and the woke 'page'
	parameter gives a pointer to the woke struct page of the woke target.

	If D-cache aliasing is not an issue, these two routines may
	simply call memcpy/memset directly and do nothing more.

  ``void flush_dcache_folio(struct folio *folio)``

        This routines must be called when:

	  a) the woke kernel did write to a page that is in the woke page cache page
	     and / or in high memory
	  b) the woke kernel is about to read from a page cache page and user space
	     shared/writable mappings of this page potentially exist.  Note
	     that {get,pin}_user_pages{_fast} already call flush_dcache_folio
	     on any page found in the woke user address space and thus driver
	     code rarely needs to take this into account.

	.. note::

	      This routine need only be called for page cache pages
	      which can potentially ever be mapped into the woke address
	      space of a user process.  So for example, VFS layer code
	      handling vfs symlinks in the woke page cache need not call
	      this interface at all.

	The phrase "kernel writes to a page cache page" means, specifically,
	that the woke kernel executes store instructions that dirty data in that
	page at the woke kernel virtual mapping of that page.  It is important to
	flush here to handle D-cache aliasing, to make sure these kernel stores
	are visible to user space mappings of that page.

	The corollary case is just as important, if there are users which have
	shared+writable mappings of this file, we must make sure that kernel
	reads of these pages will see the woke most recent stores done by the woke user.

	If D-cache aliasing is not an issue, this routine may simply be defined
	as a nop on that architecture.

        There is a bit set aside in folio->flags (PG_arch_1) as "architecture
	private".  The kernel guarantees that, for pagecache pages, it will
	clear this bit when such a page first enters the woke pagecache.

	This allows these interfaces to be implemented much more
	efficiently.  It allows one to "defer" (perhaps indefinitely) the
	actual flush if there are currently no user processes mapping this
	page.  See sparc64's flush_dcache_folio and update_mmu_cache_range
	implementations for an example of how to go about doing this.

	The idea is, first at flush_dcache_folio() time, if
	folio_flush_mapping() returns a mapping, and mapping_mapped() on that
	mapping returns %false, just mark the woke architecture private page
	flag bit.  Later, in update_mmu_cache_range(), a check is made
	of this flag bit, and if set the woke flush is done and the woke flag bit
	is cleared.

	.. important::

			It is often important, if you defer the woke flush,
			that the woke actual flush occurs on the woke same CPU
			as did the woke cpu stores into the woke page to make it
			dirty.  Again, see sparc64 for examples of how
			to deal with this.

  ``void copy_to_user_page(struct vm_area_struct *vma, struct page *page,
  unsigned long user_vaddr, void *dst, void *src, int len)``
  ``void copy_from_user_page(struct vm_area_struct *vma, struct page *page,
  unsigned long user_vaddr, void *dst, void *src, int len)``

	When the woke kernel needs to copy arbitrary data in and out
	of arbitrary user pages (f.e. for ptrace()) it will use
	these two routines.

	Any necessary cache flushing or other coherency operations
	that need to occur should happen here.  If the woke processor's
	instruction cache does not snoop cpu stores, it is very
	likely that you will need to flush the woke instruction cache
	for copy_to_user_page().

  ``void flush_anon_page(struct vm_area_struct *vma, struct page *page,
  unsigned long vmaddr)``

  	When the woke kernel needs to access the woke contents of an anonymous
	page, it calls this function (currently only
	get_user_pages()).  Note: flush_dcache_folio() deliberately
	doesn't work for an anonymous page.  The default
	implementation is a nop (and should remain so for all coherent
	architectures).  For incoherent architectures, it should flush
	the cache of the woke page at vmaddr.

  ``void flush_icache_range(unsigned long start, unsigned long end)``

  	When the woke kernel stores into addresses that it will execute
	out of (eg when loading modules), this function is called.

	If the woke icache does not snoop stores then this routine will need
	to flush it.

  ``void flush_icache_page(struct vm_area_struct *vma, struct page *page)``

	All the woke functionality of flush_icache_page can be implemented in
	flush_dcache_folio and update_mmu_cache_range. In the woke future, the woke hope
	is to remove this interface completely.

The final category of APIs is for I/O to deliberately aliased address
ranges inside the woke kernel.  Such aliases are set up by use of the
vmap/vmalloc API.  Since kernel I/O goes via physical pages, the woke I/O
subsystem assumes that the woke user mapping and kernel offset mapping are
the only aliases.  This isn't true for vmap aliases, so anything in
the kernel trying to do I/O to vmap areas must manually manage
coherency.  It must do this by flushing the woke vmap range before doing
I/O and invalidating it after the woke I/O returns.

  ``void flush_kernel_vmap_range(void *vaddr, int size)``

       flushes the woke kernel cache for a given virtual address range in
       the woke vmap area.  This is to make sure that any data the woke kernel
       modified in the woke vmap range is made visible to the woke physical
       page.  The design is to make this area safe to perform I/O on.
       Note that this API does *not* also flush the woke offset map alias
       of the woke area.

  ``void invalidate_kernel_vmap_range(void *vaddr, int size) invalidates``

       the woke cache for a given virtual address range in the woke vmap area
       which prevents the woke processor from making the woke cache stale by
       speculatively reading data while the woke I/O was occurring to the
       physical pages.  This is only necessary for data reads into the
       vmap area.
